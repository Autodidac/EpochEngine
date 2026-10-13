#include "ai.native_runtime.hpp"
#include "ai.epoch_native_storage.hpp"
#include "ai.backend_runtime_runtime_host.hpp"
#include "ai.backend_host_request.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace epochengine::ai::native_runtime {
namespace {
struct Service;
void update_metadata(Service&);
struct Service final {
    std::mutex state_mutex{};
    std::mutex host_mutex{};
    std::unique_ptr<epoch_ai::runtime::RuntimeHost> host{};
    std::jthread work{};
    std::atomic_bool busy{false};
    std::atomic_bool loaded{false};
    std::atomic_bool enabled{false};
    std::string model{};
    std::string phase{"Ready. Select a native model."};
    std::string events{};
    std::deque<EventBlock> blocks{};
    std::uint64_t sequence{};
    std::deque<ActivityBlock> activities{};
    std::uint64_t activity_sequence{};
    Snapshot metadata{};
    std::atomic<epoch_ai::runtime::RuntimeHost*> published_host{nullptr};
    std::jthread monitor{};
    Service();
    ~Service() {
        enabled.store(false);
        monitor.request_stop();
        if (monitor.joinable()) monitor.join();
        if (auto* value = published_host.load(std::memory_order_acquire)) value->cancel_training();
        work.request_stop();
        // Join while the phase, journal, atomics and their mutex still exist.
        // Member destruction would otherwise destroy them before the jthread.
        if (work.joinable()) work.join();
    }
};
Service& service() { static Service instance; return instance; }
std::string string_field(const epoch_ai::runtime::json& value, const char* key) {
    const auto found=value.find(key);
    return found!=value.end() && found->is_string()?found->get<std::string>():std::string{};
}
TrainingState training_state(const epoch_ai::runtime::json& value) {
    const auto& progress=value.contains("progress")&&value.at("progress").is_object()?value.at("progress"):value;
    return {.running=value.value("running",false),.enabled=value.value("enabled",false),.progress_known=progress.contains("step"),
        .state=string_field(value,"state"),.scope=string_field(value,"scope"),.error=string_field(value,"error"),
        .step=progress.value("step",std::uint64_t{}),.maximum_steps=progress.value("maximum_steps",value.value("max_steps",std::uint64_t{}))};
}
void update_metadata(Service& s) {
    const auto* published=s.published_host.load(std::memory_order_acquire);
    if (!published) return;
    // Training status owns its own synchronization and immutable configuration.
    // It remains observable while the inference mutex is held by a long call.
    const auto training=training_state(published->training_status());
    {std::scoped_lock lock{s.state_mutex};s.metadata.training=training;}
    std::unique_lock host_lock{s.host_mutex,std::try_to_lock};
    if (!host_lock.owns_lock()) return;
    const auto library=s.host->native_model_library();
    const auto architecture=s.host->architecture();
    Snapshot next{};next.training=training;next.routing=s.host->decision_mode();
    for (const auto& item:library.value("models",epoch_ai::runtime::json::array())) {
        const auto cache=item.value("cache",epoch_ai::runtime::json::object());
        next.models.push_back({.id=string_field(item,"id"),.label=string_field(item,"display_name"),
            .uri=string_field(item,"uri"),.runtime=string_field(item,"runtime"),.role="foundation",
            .installed=cache.value("installed",false),.active=item.value("active",false),
            .cached_bytes=cache.value("cached_bytes",std::uint64_t{}),.expected_bytes=cache.value("expected_bytes",std::uint64_t{})});
    }
    const auto registry=architecture.value("model_registry",epoch_ai::runtime::json::object());
    for(const auto& item:registry.value("components",epoch_ai::runtime::json::array()))
        next.roles.push_back({.id=string_field(item,"id"),.label=string_field(item,"display_name"),
            .uri=string_field(item,"provider_model"),.runtime=string_field(item,"runtime_type"),
            .role=string_field(item,"kind"),.installed=item.value("active",false),
            .active=item.value("active",false),.trainable=item.value("trainable",false)});
    const auto role_models=architecture.value("role_models",epoch_ai::runtime::json::object());
    for(const auto& [role,model]:role_models.items())if(model.is_string())
        next.roles.push_back({.id=role,.label=role,.uri=model.get<std::string>(),.role="assignment"});
    for(const auto& entry:s.host->tool_catalog()) {
        const auto& item=entry.contains("function")?entry.at("function"):entry;
        next.tools.push_back({string_field(item,"name"),string_field(item,"description")});
    }
    std::scoped_lock lock{s.state_mutex};s.metadata=std::move(next);
}
Service::Service() : monitor([this](std::stop_token stop){
    while(!stop.stop_requested()) {
        try {update_metadata(*this);} catch(const std::exception& error) {
            std::scoped_lock lock{state_mutex};metadata.inventory_error=error.what();
        } catch(...) {
            std::scoped_lock lock{state_mutex};metadata.inventory_error="Model inventory refresh failed";
        }
        for(unsigned n=0;n<10u&&!stop.stop_requested();++n)std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}) {}
void phase(std::string message) {
    auto& s=service(); std::scoped_lock lock{s.state_mutex};
    if(s.phase!=message) {
        s.activities.push_back({++s.activity_sequence,"Runtime",message});
        if(s.activities.size()>500u)s.activities.pop_front();
    }
    s.phase=std::move(message);
}
void activity(std::string group,std::string message) {
    if(message.empty())return;
    auto& s=service();std::scoped_lock lock{s.state_mutex};
    if(!s.activities.empty()&&s.activities.back().group==group&&s.activities.back().text==message)return;
    s.activities.push_back({++s.activity_sequence,std::move(group),std::move(message)});
    if(s.activities.size()>500u)s.activities.pop_front();
}
void append_event(std::string text) {
    auto& s=service(); std::scoped_lock lock{s.state_mutex};
    if(text.size()>2048) text.resize(2048);
    s.events.append(text).append("\n");
    if(s.events.size()>24576) s.events.erase(0,s.events.size()-16384);
}
void append_output(OutputKind kind, std::string text = {}) {
    auto& s=service(); std::scoped_lock lock{s.state_mutex};
    s.blocks.push_back(EventBlock{++s.sequence,std::move(text),kind});
    if(s.blocks.size()>4096u) s.blocks.pop_front();
}
epoch_ai::runtime::RuntimeHost& ensure_host(Service& s) {
    if(!s.host) {
        // Keep the embedded runtime entirely under Epoch's application data.
        // Preserve inference/GPU tuning from the environment, but not Epoch AI
        // file-system roots. Existing shared GGUF caches are discovered by the
        // Qwen loader without copying weights into the new Epoch cache.
        auto config = epoch_ai::core::Config::from_environment();
        // Epoch runs selected models; source iteration is not weight training.
        config.training_enabled = false;
        const auto paths = native_storage::defaults();
        native_storage::initialize(paths); // RuntimeHost::Workspace requires it.
        config.data_root = paths.data_root;
        config.workspace = paths.workspace; // Isolated from live engine sources.
        config.model_cache_root = paths.model_cache;
        config.model_root = paths.model_root;
        config.learning_root = paths.learning_root;
        config.web_root = paths.web_root;
        // Preserve explicit Epoch AI training backend configuration, but use
        // the bundled worker when Epoch AI's original source directory moved.
        const auto repository = std::filesystem::path{__FILE__}
            .parent_path().parent_path().parent_path().parent_path();
        auto bundled_worker=std::filesystem::current_path()/"assets/ai/native/training/model_worker.py";
        std::error_code worker_error;
        if(!std::filesystem::is_regular_file(bundled_worker,worker_error))
            bundled_worker=repository/"Engine/assets/ai/native/training/model_worker.py";
        if (std::filesystem::is_regular_file(bundled_worker, worker_error)) {
            const auto default_worker = config.workspace / "projects/native/resources/training/model_worker.py";
            if (config.training_backend.empty() || config.training_backend == default_worker
                || !std::filesystem::is_regular_file(config.training_backend, worker_error))
                config.training_backend = bundled_worker;
        }
        s.host = std::make_unique<epoch_ai::runtime::RuntimeHost>(std::move(config));
        s.published_host.store(s.host.get(),std::memory_order_release);
    }
    return *s.host;
}
template<typename F> bool queue(std::string label,F action) {
    auto& s=service();
    if(s.busy.exchange(true,std::memory_order_acq_rel)) return false;
    phase(std::move(label));
    // Only replacing the prior completed jthread; no UI-frame blocking.
    s.work=std::jthread([fn=std::move(action)](std::stop_token stop) mutable {
        auto& current=service();
        try { if(!stop.stop_requested()) {
            std::scoped_lock host_lock{current.host_mutex};
            auto& host=ensure_host(current);
            fn(host, stop);
        }} catch(const std::exception& e) { phase(std::string{"Native backend error: "}+e.what()); }
        catch(...) { phase("Native backend: unknown error"); }
        current.busy.store(false,std::memory_order_release);
    });
    return true;
}
} // namespace
Snapshot snapshot(bool include_inventory) {
    auto& s=service(); std::scoped_lock lock{s.state_mutex};
    auto value=include_inventory?s.metadata:Snapshot{};value.busy=s.busy.load();value.loaded=s.loaded.load();value.selected=s.enabled.load();
    value.model=s.model;value.phase=s.phase;value.recent_events=s.events;
    value.output_sequence=s.sequence;
    value.storage_root=native_storage::defaults().data_root.string();return value;
}
std::vector<EventBlock> events_since(std::uint64_t last_seen) {
    auto& s=service(); std::scoped_lock lock{s.state_mutex};
    std::vector<EventBlock> result;
    for(const auto& e:s.blocks) if(e.sequence>last_seen) result.push_back(e);
    return result;
}
std::vector<ActivityBlock> activities_since(std::uint64_t last_seen) {
    auto& s=service();std::scoped_lock lock{s.state_mutex};
    std::vector<ActivityBlock> result;
    for(const auto& event:s.activities)if(event.sequence>last_seen)result.push_back(event);
    return result;
}
bool selected() noexcept { return service().enabled.load(std::memory_order_acquire); }
bool select_for_epoch_requests(bool enabled) {
    auto& s=service();
    if(enabled && !s.loaded.load()) return false;
    s.enabled.store(enabled,std::memory_order_release);
    phase(enabled?"Epoch chat and self-iteration use native Epoch AI agents":"Native model available; Epoch uses its previous inference selection");
    return true;
}
bool refresh() {return queue("Discovering Epoch AI models and tools",[](auto&,std::stop_token){});}
bool register_model(std::string id,std::string path) {
    if(id.empty()||path.empty())return false;
    return queue("Registering native model",[id=std::move(id),path=std::move(path)](auto& host,std::stop_token){
        const auto result=host.register_native_model({{"id",id},{"display_name",id},{"source","local-gguf"},{"local_path",path}});
        phase(result?"Model registered: "+id:"Model registration failed: "+result.error().message);
    });
}
bool assign_role(std::string role,std::string uri) {
    return queue("Assigning model role",[role=std::move(role),uri=std::move(uri)](auto& host,std::stop_token){
        const auto assigned=host.assign_model_role(role,uri);
        phase(assigned?"Model role saved: "+role:"Role assignment failed: "+assigned.error().message);
    });
}
bool prepare_training(std::string profile) {
    return queue("Preparing internal training models",[profile=std::move(profile)](auto& host,std::stop_token stop){
        const auto result=host.prepare_training(profile,stop);
        phase(result?"Training models initialized; select a scope and start training":"Training setup failed: "+result.error().message);
    });
}
bool start_training(std::string scope,std::uint32_t steps,bool require_cuda) {
    return queue("Starting explicit internal-model training",[scope=std::move(scope),steps,require_cuda](auto& host,std::stop_token){
        const auto result=host.start_training(scope,steps,require_cuda);
        phase(result?"Internal training job started; live progress is in Training":"Training failed: "+result.error().message);
    });
}
bool cancel_training() {if(auto* host=service().published_host.load(std::memory_order_acquire)){host->cancel_training();return true;}return false;}
bool pause_training(bool paused) {if(auto* host=service().published_host.load(std::memory_order_acquire)){host->pause_training(paused);return true;}return false;}
bool prepare(std::string id) {
    return queue("Preparing native Epoch AI",[id=std::move(id)](auto& host,std::stop_token stop) mutable {
        auto library=host.native_model_library();
        const auto models=library.value("models",epoch_ai::runtime::json::array());
        if(id.empty()) {
            const auto preferred=string_field(library,"preferred_model");
            for(const auto& item:models)if(string_field(item,"uri")==preferred){id=string_field(item,"id");break;}
        }
        if(id.empty()) {
            for(const auto& item:models)if(item.value("cache",epoch_ai::runtime::json::object()).value("installed",false)){id=string_field(item,"id");break;}
            if(id.empty()&&!models.empty())id=string_field(models.front(),"id");
        }
        if(id.empty()){phase("No compatible native model is registered");return;}
        const auto runtime=library.value("qwen_flash",epoch_ai::runtime::json::object());
        if(string_field(runtime,"server").empty()) {
            phase("Preparing the Qwen runtime before model installation");
            const auto started=host.provision_native_runtime();
            if(!started){phase("Native runtime setup failed: "+started.error().message);return;}
            while(!stop.stop_requested()) {
                const auto install=host.native_model_library().value("runtime_install",epoch_ai::runtime::json::object());
                const auto state=string_field(install,"state");
                if(state=="installed")break;
                if(state!="installing"){phase("Runtime setup failed: "+string_field(install,"error"));return;}
                phase("Preparing Qwen runtime: "+string_field(install,"phase"));
                for(unsigned n=0;n<10u&&!stop.stop_requested();++n)std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if(stop.stop_requested()){phase("Native setup cancelled");return;}
        }
        // Setup owns the model transition. A request must not capture the old
        // identity and then silently execute on a newly loaded model.
        service().enabled.store(false,std::memory_order_release);
        service().loaded.store(false,std::memory_order_release);
        bool installed{};for(const auto& item:models)if(string_field(item,"id")==id)installed=item.value("cache",epoch_ai::runtime::json::object()).value("installed",false);
        if(!installed) {
            const auto started=host.install_native_model(id);
            if(!started){phase("Model install failed: "+started.error().message);return;}
            while(!stop.stop_requested()) {
                const auto current=host.native_model_library().value("install",epoch_ai::runtime::json::object());
                const auto state=string_field(current,"state");
                if(state=="installed")break;
                if(state!="installing"){phase("Model install failed: "+string_field(current,"error"));return;}
                phase("Downloading "+id+" into Epoch's model cache");
                for(unsigned n=0;n<10u&&!stop.stop_requested();++n)std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        if(stop.stop_requested()){phase("Native setup cancelled");return;}
        phase("Loading native model "+id);
        auto loaded=host.load_native_model(id,{},stop);
        if(!loaded){phase("Native setup failed: "+loaded.error().message);return;}
        if(stop.stop_requested()){host.unload_native_model();phase("Native setup cancelled");return;}
        const auto uri=std::string{"qwenflash://"}+id;
        const auto selected=host.set_foundation_model(uri);
        if(!selected){host.unload_native_model();phase("Model selection failed: "+selected.error().message);return;}
        auto& current=service();{std::scoped_lock lock{current.state_mutex};current.model=id;}
        current.loaded.store(true,std::memory_order_release);current.enabled.store(true,std::memory_order_release);
        phase("Ready: native Epoch AI uses "+id);
    });
}
void cancel_setup() {
    auto& current=service();current.work.request_stop();
    if(auto* host=current.published_host.load(std::memory_order_acquire))host->cancel_native_installation();
    phase("Cancelling native setup; completed cache bytes are retained");
}
bool provision() {
    return queue("Checking Epoch native Qwen runtime",[](auto& host, std::stop_token stop){
        auto result=host.provision_native_runtime();
        if(!result) { phase("Runtime provisioning failed: "+result.error().message); return; }
        const auto state=result->value("state",std::string{});
        if(state=="installed") { phase("Native Qwen runtime available in Epoch AI"); return; }
        if(state!="installing") { phase("Runtime provisioning state: "+state); return; }
        while(!stop.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            const auto library=host.native_model_library();
            if(!library.contains("runtime_install") || !library.at("runtime_install").is_object()) continue;
            const auto& status=library.at("runtime_install");
            const auto current=status.value("state",std::string{});
            if(current=="installed") { phase("Native Qwen runtime installed. Install or load a model."); return; }
            if(current=="failed") { phase("Runtime provisioning failed: "+status.value("error",std::string{"Unknown installer error"})); return; }
            if(current=="cancelled") { phase("Runtime provisioning cancelled"); return; }
            if(current!="installing") { phase("Runtime provisioning stopped: "+current); return; }
            auto progress=status.value("message",std::string{"Provisioning native Qwen runtime"});
            if(status.contains("percent") && status.at("percent").is_number_integer())
                progress+=" ("+std::to_string(status.at("percent").template get<int>())+"%)";
            phase(std::move(progress));
        }
    });
}
bool install(std::string id) {
    if(id.empty())return false;
    return queue("Installing model "+id,[id=std::move(id)](auto& host, std::stop_token stop){
        auto result=host.install_native_model(id);
        if(!result) { phase("Install failed: "+result.error().message); return; }
        const auto initial=result->value("state", std::string{});
        if(initial=="installed") { phase("Model already installed: "+id); return; }
        if(initial!="installing") { phase("Model install state: "+initial); return; }
        phase("Downloading model: "+id+" (Epoch AI model cache)");
        // NativeModelManager launches a separate download worker. Keep this
        // editor operation active until it terminates, rather than telling the
        // user a queued download has completed. Never block the render thread.
        while(!stop.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            const auto library=host.native_model_library();
            if(!library.contains("install") || !library.at("install").is_object()) continue;
            const auto& install=library.at("install");
            const auto state=install.value("state", std::string{});
            if(state=="installed") { phase("Model installed: "+id+". Select Load Model."); return; }
            if(state=="failed") {
                phase("Install failed: "+install.value("error",std::string{"Unknown download error"}));
                return;
            }
            if(state=="cancelled") { phase("Model download cancelled: "+id); return; }
            if(state!="installing") { phase("Model download stopped: "+state); return; }
            std::string message="Downloading model: "+id;
            if(install.contains("cache") && install.at("cache").is_object()) {
                const auto& cache=install.at("cache");
                const auto current=cache.value("cached_bytes",std::uint64_t{});
                const auto expected=cache.value("expected_bytes",std::uint64_t{});
                constexpr std::uint64_t mib=1024ULL*1024ULL;
                if(expected>0) message+=" ("+std::to_string(current/mib)+" / "+std::to_string(expected/mib)+" MiB cached)";
                else if(current>0) message+=" ("+std::to_string(current/mib)+" MiB cached)";
            }
            phase(std::move(message));
        }
        phase("Epoch closed the install monitor; download state will be checked on next launch.");
    });
}
bool load(std::string id) {
    if(id.empty())return false;
    auto& s=service();
    return queue("Loading native model "+id,[id=std::move(id),&s](auto& host, std::stop_token stop){
        s.loaded.store(false);s.enabled.store(false);
        auto result=host.load_native_model(id,{},stop);
        if(result) {
            const auto native_uri=std::string{"qwenflash://"}+id;
            auto foundation=host.set_foundation_model(native_uri);
            if(!foundation) {
                host.unload_native_model();
                phase("Native loader succeeded but foundation selection failed: "+foundation.error().message);
                return;
            }
            {std::scoped_lock lock{s.state_mutex};s.model=id;}
            s.loaded.store(true);
            phase("Native model loaded: "+id+". Select Use for Epoch requests.");
        } else phase("Load failed: "+result.error().message);
    });
}
bool unload() {
    auto& s=service(); s.enabled.store(false);
    return queue("Unloading native model",[&s](auto& host, std::stop_token){
        host.unload_native_model();s.loaded.store(false);
        phase("Model unloaded; weights and saved settings preserved");
    });
}
Result run_agent(std::string_view message,std::stop_token cancellation) {
    auto& s=service();
    if(!s.enabled.load()||!s.loaded.load())return {false,"Native model is not loaded and selected"};
    if(message.empty())return {false,"Empty request"};
    // The editor already dispatches requests on its async worker. Do not hold
    // an Epoch UI lock; serialize only Epoch AI's mutable agent/model runtime.
    std::unique_lock lock{s.host_mutex,std::defer_lock};
    while(!lock.try_lock()) {
        if(cancellation.stop_requested())return {false,"Native request cancelled"};
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if(cancellation.stop_requested())return {false,"Native request cancelled"};
    try {
        auto& host=ensure_host(s);
        phase("Architect routing task to native model / tools");
        bool streamedText{};
        // Epoch supplies conversation history in the request. A fresh native
        // session prevents cross-project history from the formerly shared ID.
        const auto session="epoch-editor-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        auto result=host.run_task("epoch-engine",session,message,cancellation,
            [&streamedText](const epoch_ai::core::Event& event){
                // The conversation displays decoded token text, never event
                // names, JSON escapes or reasoning-function wrappers.
                if(event.type=="model.request") {
                    streamedText=false;
                    append_output(OutputKind::boundary);
                } else if(event.type=="model.text_delta") {
                    streamedText=true;
                    append_output(OutputKind::text,event.data.value("delta",std::string{}));
                } else if(event.type=="model.reasoning_delta") {
                    append_output(OutputKind::reasoning,event.data.value("delta",std::string{}));
                } else if(event.type=="model.tool_argument_delta") {
                    append_output(OutputKind::tool_arguments,event.data.value("delta",std::string{}));
                } else if(event.type=="model.response") {
                    if(!streamedText)
                        append_output(OutputKind::text,event.data.value("text",std::string{}));
                    append_output(OutputKind::boundary);
                } else {
                    append_event(event.type+" "+event.data.dump());
                    if(event.type.starts_with("controller.")||event.type.starts_with("tool.")) {
                        std::string detail;
                        for(const auto* field:{"reason","summary","message","tool","name","model","decision","task_class"}) {
                            const auto value=string_field(event.data,field);
                            if(!value.empty()){if(!detail.empty())detail+=" · ";detail+=value;}
                        }
                        auto label=event.type.substr(event.type.find('.')+1u);
                        std::replace(label.begin(),label.end(),'_',' ');
                        activity(event.type.starts_with("controller.")?"System 1":"Tools",label+(detail.empty()?"":": "+detail));
                    }
                }
            });
        if(!result) {phase("Native agent failed: "+result.error().message);return {false,result.error().message};}
        using Terminal=epoch_ai::runtime::RunTerminal;
        if(result->terminal!=Terminal::completed) {
            const auto reason=result->terminal==Terminal::awaiting_approval ? "Task awaiting approval" :
                result->terminal==Terminal::cancelled?"Task cancelled":"Task failed";
            phase(reason);return {false,reason+std::string{": "}+result->final_text};
        }
        if(result->final_text.empty()) {
            phase("Native agent returned no final assistant text");
            return {false,"No final assistant text was produced"};
        }
        phase("Native agent completed task");
        return {true,std::move(result->final_text)};
    }catch(const std::exception& e){phase(std::string{"Native agent exception: "}+e.what());return {false,e.what()};}
}
Result complete_host_request(std::string_view request_json,
    std::uint32_t timeout_seconds, std::stop_token cancellation, const StreamObserver& observer) {
    auto& s=service();
    if(!s.enabled.load()||!s.loaded.load())return {false,"Native model is not loaded and selected"};
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds{timeout_seconds};
    std::unique_lock lock{s.host_mutex,std::defer_lock};
    while(!lock.try_lock()) {
        if(cancellation.stop_requested())return {false,"Native request cancelled"};
        if(std::chrono::steady_clock::now()>=deadline)return {false,"Native request deadline expired while waiting for the model"};
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if(cancellation.stop_requested())return {false,"Native request cancelled"};
    try {
        auto& host=ensure_host(s);
        append_output(OutputKind::boundary);
        phase("Epoch host model request; approvals and tools remain with Epoch");
        auto response=host.complete_host_request(epoch_ai::model::json::parse(request_json),
            cancellation,deadline,
            [&](const epoch_ai::model::StreamEvent& event) {
                using Kind=epoch_ai::model::StreamEventKind;
                const auto kind=event.kind==Kind::reasoning_delta?OutputKind::reasoning
                    :event.kind==Kind::tool_argument_delta?OutputKind::tool_arguments
                    :event.kind==Kind::text_delta?OutputKind::text:OutputKind::boundary;
                append_output(kind,event.delta);
                if(observer)observer(kind,event.delta);
            });
        append_output(OutputKind::boundary);
        if(!response){phase("Native request failed: "+response.error().message);return {false,response.error().message};}
        phase("Model response completed; Epoch validates the result");
        return {true,epoch_ai::model::host_response(*response).dump()};
    }catch(const std::exception& e){phase(std::string{"Native request failed: "}+e.what());return {false,e.what()};}
}
} // namespace epochengine::ai::native_runtime
