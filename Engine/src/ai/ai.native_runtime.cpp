// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#include "ai.native_runtime.hpp"
#include "../include/ai.token_metrics.hpp"
#include "ai.epoch_native_storage.hpp"
#include "ai.native_resident_selection.hpp"
#include "ai.native_saved_set.hpp"
#include "ai.native_set_memory_budget.hpp"
#include "ai.native_multimodel_gpu_budget.hpp"
#include "ai.backend_runtime_runtime_host.hpp"
#include "ai.backend_host_request.hpp"
#include "native/ai.backend_model_profile_import.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <limits>
#include <iterator>
#include <latch>
#include <fstream>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <thread>
#include <utility>
extern "C" void core_log_write(std::uint32_t, const char*, const char*);
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace epochengine::ai::native_runtime {
namespace {
struct Service;
std::atomic<Service*> existing_service{nullptr};
void update_metadata(Service&);

// Persist the last completed load stage in Epoch's existing rotating logs.
// The in-memory Model Manager status vanishes on a Windows access violation;
// stage breadcrumbs remain available in a diagnostic ZIP after a crash.
void trace_load_stage(std::string message) {
    core_log_write(2u, "Epoch.AI.Native", message.c_str());
}
struct Service final {
    std::mutex state_mutex{};
    std::mutex host_mutex{};
    std::unique_ptr<epoch_ai::runtime::RuntimeHost> host{};
    std::jthread work{};
    std::atomic_bool busy{false};
    std::atomic_bool stopping{false};
    std::atomic_bool loaded{false};
    std::atomic_bool enabled{false};
    std::string model{};
    std::string last_response_model{};
    std::string last_response_role{};
    std::string phase{"Ready. Select a native model."};
    std::string events{};
    std::atomic_bool capture_raw_stream{true}; // retain bounded raw streams in chronological Activity
    std::deque<EventBlock> blocks{};
    std::uint64_t sequence{};
    std::deque<ActivityBlock> activities{};
    std::uint64_t activity_sequence{};
    Snapshot metadata{};
    ModelBenchmark benchmark{};
    std::atomic<epoch_ai::runtime::RuntimeHost*> published_host{nullptr};
    std::jthread monitor{};
    Service();
    void request_shutdown() noexcept {
        stopping.store(true, std::memory_order_release);
        enabled.store(false);
        // A readiness wait belongs to work, not an inference/chat worker.
        // Signal it BEFORE joining the monitor or retiring renderer contexts.
        work.request_stop();
        monitor.request_stop();
    }
    void finish_shutdown() {
        request_shutdown();
        if (monitor.joinable()) monitor.join();
        // Join while the phase, journal, atomics and their mutex still exist.
        // Member destruction would otherwise destroy them before the jthread.
        if (work.joinable()) work.join();
        published_host.store(nullptr, std::memory_order_release);
        // Destroy owned processes/providers while status, journals and mutexes
        // still exist, rather than in reverse-order member destruction.
        host.reset();
        loaded.store(false, std::memory_order_release);
        busy.store(false, std::memory_order_release);
    }
    ~Service() { finish_shutdown(); }
};
Service& service() {
    static Service instance;
    existing_service.store(&instance, std::memory_order_release);
    return instance;
}
std::string string_field(const epoch_ai::runtime::json& value, const char* key) {
    const auto found=value.find(key);
    return found!=value.end() && found->is_string()?found->get<std::string>():std::string{};
}
ModelTuning model_tuning_from_json(const epoch_ai::runtime::json& item) {
    ModelTuning tuning{};
    if (!item.is_object()) return tuning;
    tuning.runtime_policy = item.value("runtime_policy", tuning.runtime_policy);
    tuning.gpu_layers = item.value("gpu_layers", tuning.gpu_layers);
    tuning.gpu_layer_count_hint = item.value("gpu_layer_count_hint", tuning.gpu_layer_count_hint);
    tuning.context_tokens = item.value("context_tokens", tuning.context_tokens);
    tuning.parallel_slots = item.value("parallel_slots", tuning.parallel_slots);
    tuning.context_reserve_tokens = item.value("context_reserve_tokens", tuning.context_reserve_tokens);
    tuning.auto_compact = item.value("auto_compact", tuning.auto_compact);
    tuning.compact_trigger_ratio = item.value("compact_trigger_ratio", tuning.compact_trigger_ratio);
    tuning.compact_target_ratio = item.value("compact_target_ratio", tuning.compact_target_ratio);
    tuning.threads = item.value("threads", tuning.threads);
    tuning.batch_threads = item.value("batch_threads", tuning.batch_threads);
    tuning.batch_size = item.value("batch_size", tuning.batch_size);
    tuning.ubatch_size = item.value("ubatch_size", tuning.ubatch_size);
    tuning.flash_attention = item.value("flash_attention", tuning.flash_attention);
    tuning.kv_type_k = item.value("kv_type_k", tuning.kv_type_k);
    tuning.kv_type_v = item.value("kv_type_v", tuning.kv_type_v);
    tuning.kv_offload = item.value("kv_offload", tuning.kv_offload);
    tuning.kv_cpu_pinned = item.value("kv_cpu_pinned", tuning.kv_cpu_pinned);
    tuning.kv_gpu_layers = item.value("kv_gpu_layers", tuning.kv_gpu_layers);
    tuning.prompt_cache_mib = item.value("prompt_cache_mib", tuning.prompt_cache_mib);
    tuning.expert_cache_mode = item.value("expert_cache_mode", tuning.expert_cache_mode);
    tuning.expert_cache_mib = item.value("expert_cache_mib", tuning.expert_cache_mib);
    tuning.pinned_host_mib = item.value("pinned_host_mib", tuning.pinned_host_mib);
    tuning.speculative_enabled = item.value("speculative_enabled", tuning.speculative_enabled);
    tuning.speculative_model = string_field(item, "speculative_model");
    tuning.speculative_tokens = item.value("speculative_tokens", tuning.speculative_tokens);
    return tuning;
}
epoch_ai::runtime::json model_tuning_json(const ModelTuning& tuning) {
    return {{"runtime_policy", tuning.runtime_policy},
        {"gpu_layers", tuning.gpu_layers},
        {"gpu_layer_count_hint", tuning.gpu_layer_count_hint},
        {"context_tokens", tuning.context_tokens}, {"parallel_slots", tuning.parallel_slots},
        {"context_reserve_tokens", tuning.context_reserve_tokens}, {"auto_compact", tuning.auto_compact},
        {"compact_trigger_ratio", tuning.compact_trigger_ratio}, {"compact_target_ratio", tuning.compact_target_ratio},
        {"threads", tuning.threads}, {"batch_threads", tuning.batch_threads},
        {"batch_size", tuning.batch_size}, {"ubatch_size", tuning.ubatch_size},
        {"flash_attention", tuning.flash_attention}, {"kv_type_k", tuning.kv_type_k},
        {"kv_type_v", tuning.kv_type_v}, {"kv_offload", tuning.kv_offload},
        {"kv_cpu_pinned", tuning.kv_cpu_pinned}, {"kv_gpu_layers", tuning.kv_gpu_layers},
        {"prompt_cache_mib", tuning.prompt_cache_mib},
        {"expert_cache_mode", tuning.expert_cache_mode},
        {"expert_cache_mib", tuning.expert_cache_mib},
        {"pinned_host_mib", tuning.pinned_host_mib},
        {"speculative_enabled", tuning.speculative_enabled},
        {"speculative_model", tuning.speculative_model},
        {"speculative_tokens", tuning.speculative_tokens}};
}
TrainingState training_state(const epoch_ai::runtime::json& value) {
    const auto& progress=value.contains("progress")&&value.at("progress").is_object()?value.at("progress"):value;
    return {.running=value.value("running",false),.enabled=value.value("enabled",false),.progress_known=progress.contains("step"),
        .state=string_field(value,"state"),.scope=string_field(value,"scope"),.error=string_field(value,"error"),
        .step=progress.value("step",std::uint64_t{}),.maximum_steps=progress.value("maximum_steps",value.value("max_steps",std::uint64_t{}))};
}
// A Model Manager rescan is discovery, not an instruction to instantiate the
// entire agent/runtime, adopt backends, start workers or load model weights.
// Read only the small persisted profile manifests. The host performs the
// authoritative cache/runtime checks once the operator explicitly loads a model.
[[nodiscard]] Snapshot discover_offline_native_inventory(
    const std::filesystem::path& data_root, std::stop_token stop) {
    Snapshot result{};
    result.inventory_preview = true;
    const auto catalog = data_root / "models" / "runtimes";
    std::error_code ec;
    constexpr std::uintmax_t max_manifest_bytes = 1024u * 1024u;
    constexpr std::size_t max_profiles = 64u;
    std::size_t invalid = 0;
    if (std::filesystem::is_directory(catalog, ec) && !ec) {
        auto iterator = std::filesystem::directory_iterator(
            catalog, std::filesystem::directory_options::skip_permission_denied, ec);
        const auto end = std::filesystem::directory_iterator{};
        for (; !ec && iterator != end && !stop.stop_requested()
                && result.models.size() < max_profiles; iterator.increment(ec)) {
            const auto& directory = *iterator;
            std::error_code item_error;
            if (directory.is_symlink(item_error) || item_error
                || !directory.is_directory(item_error) || item_error) continue;
            const auto manifest_path = directory.path() / "manifest.json";
            const auto size = std::filesystem::file_size(manifest_path, item_error);
            if (item_error || size == 0 || size > max_manifest_bytes) {
                // A folder with no manifest is not a registered model.
                if (!item_error && size > max_manifest_bytes) ++invalid;
                continue;
            }
            std::ifstream stream(manifest_path, std::ios::binary);
            if (!stream) { ++invalid; continue; }
            try {
                const auto manifest = epoch_ai::runtime::json::parse(
                    stream, nullptr, false);
                if (!manifest.is_object()
                    || !manifest.value("format", std::string{}).starts_with(
                        "engcoder.runtime-model.v")) {
                    ++invalid;
                    continue;
                }
                const auto id = manifest.value("id", std::string{});
                if (id.empty() || id.size() > 160u ||
                    !std::all_of(id.begin(), id.end(), [](unsigned char c) {
                        return std::isalnum(c) != 0 || c == '.' || c == '_' || c == '-';
                    })) {
                    ++invalid;
                    continue;
                }
                const auto runtime = manifest.value("runtime", std::string{"qwen-flash"});
                if (runtime != "qwen-flash") continue;
                auto label = manifest.value("display_name", id);
                if (label.empty()) label = id;
                if (label.size() > 256u) label.resize(256u);
                const auto estimated = manifest.value("estimated_bytes", std::uint64_t{});
                auto gpu_target = manifest.value("gpu_offload_percent", 60u);
                // This view cannot mutate profiles: preview the revision-11
                // migration until the real model manager opens them.
                if (manifest.value("profile_revision", 1u) == 10u && gpu_target != 0u)
                    gpu_target = std::min(100u, gpu_target + 10u);
                if (manifest.value("profile_revision", 1u) == 12u
                    && gpu_target == 90u
                    && manifest.value("runtime_policy", std::string{}) == "recommended"
                    && manifest.value("gpu_layers", std::string{}) == "auto"
                    && manifest.value("gpu_layer_count_hint", 0u) == 48u)
                    gpu_target = 100u; // Read-only preview of the revision-13 migration.
                if (manifest.value("profile_revision", 1u) == 13u
                    && gpu_target == 100u
                    && manifest.value("runtime_policy", std::string{}) == "recommended"
                    && manifest.value("gpu_layers", std::string{}) == "auto"
                    && manifest.value("gpu_layer_count_hint", 0u) == 48u
                    && manifest.value("threads", 0u) == 12u
                    && manifest.value("batch_size", 0u) == 1024u)
                    gpu_target = 60u;
                result.models.push_back(ModelEntry{
                    .id = id, .label = std::move(label),
                    .uri = "qwenflash://" + id, .runtime = runtime,
                    .role = "foundation", .expected_bytes = estimated,
                    .gpu_offload_percent = gpu_target,
                    .tuning = model_tuning_from_json(manifest)});
            } catch (const std::exception&) {
                // A corrupt profile is isolated to that model, never the editor.
                ++invalid;
            }
        }
        if (ec) result.inventory_error = "Native profile scan: " + ec.message();
        if (result.models.size() == max_profiles)
            result.inventory_error = "Native profile inventory is limited to 64 entries.";
    } else if (ec) {
        result.inventory_error = "Native profile folder: " + ec.message();
    }
    // The built-in model is available even before its first full-host startup
    // has saved a manifest. No native runtime or model download is started here.
    constexpr std::string_view builtin = "qwen3.8-flash-next-ud-q2-k-xl";
    if (std::none_of(result.models.begin(), result.models.end(),
            [&](const ModelEntry& item) { return item.id == builtin; })) {
        result.models.push_back(ModelEntry{
            .id = std::string{builtin},
            .label = "Qwen3.8 Flash Next (UD-Q2_K_XL)",
            .uri = "qwenflash://" + std::string{builtin},
            .runtime = "qwen-flash", .role = "foundation"});
    }
    if (invalid) {
        if (!result.inventory_error.empty()) result.inventory_error += " ";
        result.inventory_error += std::to_string(invalid) +
            " invalid/oversized model manifest(s) skipped.";
    }
    return result;
}
// Conservative admission check for additional GGUF processes. File bytes are
// not a measured RAM requirement (offload and KV context affect real usage);
// use them only as a heuristic and never claim to have measured free VRAM.
struct SystemMemory final {
    std::uint64_t total{}, available{};
};
[[nodiscard]] SystemMemory sample_system_memory() noexcept {
    SystemMemory result{};
#if defined(_WIN32)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (::GlobalMemoryStatusEx(&status))
        result = {status.ullTotalPhys, status.ullAvailPhys};
#elif defined(__linux__)
    std::ifstream stream{"/proc/meminfo"};
    std::string key{}, units{};
    std::uint64_t value{};
    bool total_seen{}, available_seen{};
    for (unsigned i=0; i<128u && stream >> key >> value >> units; ++i) {
        if (units != "kB" || value > (std::numeric_limits<std::uint64_t>::max)() / 1024u) continue;
        if (key == "MemTotal:") { result.total = value * 1024u; total_seen = true; }
        else if (key == "MemAvailable:") { result.available = value * 1024u; available_seen = true; }
        if (total_seen && available_seen) break;
    }
    if (!total_seen || !available_seen) result = {};
#endif
    return result;
}
[[nodiscard]] bool safe_to_start_another_model(std::uint64_t expected_bytes,
    std::string& reason) {
    const auto memory = sample_system_memory();
    const resident_budget::Sample ram{memory.total, memory.available};
    if (resident_budget::admit(ram, expected_bytes)) return true;
    const auto minimum = resident_budget::threshold_bytes(ram, expected_bytes);
    reason = "Load set paused before starting another model: "
        + std::to_string(memory.available / (1024u*1024u))
        + " MiB system RAM available; conservative admission threshold "
        + std::to_string(minimum / (1024u*1024u))
        + " MiB (heuristic; VRAM is not measured). Existing models are retained. "
          "Free memory or reduce resident-model/context budgets, then retry.";
    return false;
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
    const auto performance=library.value("performance", epoch_ai::runtime::json::object())
        .value("models", epoch_ai::runtime::json::object());
    for (const auto& item:library.value("models",epoch_ai::runtime::json::array())) {
        const auto cache=item.value("cache",epoch_ai::runtime::json::object());
        ModelEntry entry{.id=string_field(item,"id"),.label=string_field(item,"display_name"),
            .uri=string_field(item,"uri"),.runtime=string_field(item,"runtime"),.role="foundation",
            .installed=cache.value("installed",false),.active=item.value("active",false),
            .cached_bytes=cache.value("cached_bytes",std::uint64_t{}),.expected_bytes=cache.value("expected_bytes",std::uint64_t{}),
            .gpu_offload_percent=item.value("gpu_offload_percent",70u),
            .effective_gpu_layers=string_field(item,"effective_gpu_layers"),
            .tuning=model_tuning_from_json(item)};
        // The runtime may have fallen back from 76K to 64K/32K (or another
        // configured context). The model profile is only an aspiration.
        const auto runtime_state=item.value("runtime_state", epoch_ai::runtime::json::object());
        if (entry.active && runtime_state.is_object() && runtime_state.contains("image_input")
            && runtime_state.at("image_input").is_boolean()) {
            entry.image_input_known=true;
            entry.image_input_supported=runtime_state.at("image_input").get<bool>();
        }
        const auto effective=runtime_state.value("effective_profile", epoch_ai::runtime::json::object());
        if (entry.active && effective.is_object() && !effective.empty()) {
            entry.loaded_tuning=model_tuning_from_json(effective);
            entry.loaded_gpu_layers=string_field(effective,"gpu_layers");
            entry.loaded_tuning_known=true;
        }
        if (entry.active && runtime_state.is_object()
            && runtime_state.contains("active_context_tokens")
            && runtime_state.at("active_context_tokens").is_number_unsigned())
            entry.loaded_context_tokens=runtime_state.at("active_context_tokens").get<std::uint32_t>();
        if (entry.active && runtime_state.is_object()
            && runtime_state.contains("active_parallel_slots")
            && runtime_state.at("active_parallel_slots").is_number_unsigned())
            entry.loaded_parallel_slots=runtime_state.at("active_parallel_slots").get<std::uint32_t>();
        if (performance.contains(entry.uri) && performance.at(entry.uri).is_object()) {
            const auto& by_class=performance.at(entry.uri);
            for (const auto* task_class : {"simple","complex","coding","unknown"}) {
                if (!by_class.contains(task_class) || !by_class.at(task_class).is_object()) continue;
                const auto& stats=by_class.at(task_class);
                entry.requests+=static_cast<std::uint64_t>(stats.value("requests",0.0));
                entry.inference_ok+=static_cast<std::uint64_t>(stats.value("inference_ok",0.0));
                const double first=stats.value("ttft_samples",0.0);
                if (first>0) { entry.avg_first_token_ms+=stats.value("ttft_ms",0.0);
                    entry.first_token_known=true; }
                const double samples=stats.value("token_samples",0.0);
                if (samples>0) { entry.reported_tps+=stats.value("completion_tokens",0.0);
                    entry.tps_known=true; }
            }
            if (entry.requests) {
                double total_ms=0,ttft_samples=0, generation_ms=0;
                for (const auto* task_class : {"simple","complex","coding","unknown"}) {
                    if (!by_class.contains(task_class) || !by_class.at(task_class).is_object()) continue;
                    const auto& stats=by_class.at(task_class);
                    total_ms+=stats.value("total_ms",0.0);
                    ttft_samples+=stats.value("ttft_samples",0.0);
                    generation_ms+=stats.value("generation_ms",0.0);
                }
                entry.avg_request_ms=total_ms/static_cast<double>(entry.requests);
                if (ttft_samples>0) entry.avg_first_token_ms/=ttft_samples;
                if (generation_ms>0) entry.reported_tps/=generation_ms/1000.0;
                else entry.tps_known=false;
            }
            // Compare only measurements with the same classified intent.
            // These are observational averages, NOT controlled prompt pairs.
            const auto foundation_uri=string_field(library,"preferred_model");
            if (entry.uri != foundation_uri && performance.contains(foundation_uri)
                && performance.at(foundation_uri).is_object()) {
                const auto& reference=performance.at(foundation_uri);
                if (reference.contains("simple") && by_class.contains("simple")
                    && reference.at("simple").is_object() && by_class.at("simple").is_object()) {
                    const auto& base=reference.at("simple");
                    const auto& small=by_class.at("simple");
                    const double base_n=base.value("requests",0.0);
                    const double small_n=small.value("requests",0.0);
                    if (base_n>=5.0 && small_n>=5.0) {
                        entry.simple_baseline_available=true;
                        entry.simple_latency_difference_ms=base.value("total_ms",0.0)/base_n
                            - small.value("total_ms",0.0)/small_n;
                    }
                }
            }
            if (by_class.contains("agent_outcomes") && by_class.at("agent_outcomes").is_object()) {
                const auto& outcomes=by_class.at("agent_outcomes");
                entry.agent_completed=static_cast<std::uint64_t>(outcomes.value("completed",0.0));
                entry.agent_failed=static_cast<std::uint64_t>(outcomes.value("failed",0.0));
            }
        }
        next.models.push_back(std::move(entry));
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
    auto& s=service();
    if (!s.capture_raw_stream.load(std::memory_order_acquire)) return;
    std::scoped_lock lock{s.state_mutex};
    if (!s.capture_raw_stream.load(std::memory_order_relaxed)) return;
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
        const auto paths = native_storage::defaults();
        native_storage::initialize(paths); // RuntimeHost::Workspace requires it.
        config.data_root = paths.data_root;
        config.workspace = paths.workspace; // Isolated from live engine sources.
        config.model_cache_root = paths.model_cache;
        config.model_root = paths.model_root;
        config.learning_root = paths.learning_root;
        config.web_root = paths.web_root;
        trace_load_stage("Native RuntimeHost construction started");
        s.host = std::make_unique<epoch_ai::runtime::RuntimeHost>(std::move(config));
        trace_load_stage("Native RuntimeHost constructed");
        s.published_host.store(s.host.get(),std::memory_order_release);
    }
    return *s.host;
}
template<typename F> bool queue(std::string label,F action) {
    auto& s=service();
    if (s.stopping.load(std::memory_order_acquire)) return false;
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
    value.model=s.model;value.last_response_model=s.last_response_model;
    value.last_response_role=s.last_response_role;
    value.phase=s.phase;value.recent_events=s.events;
    value.output_sequence=s.sequence;
    value.benchmark=s.benchmark;
    for (const auto& model : s.metadata.models)
        if (model.active && (model.id == value.model || model.uri == value.model)) {
            value.image_input_known=model.image_input_known;
            value.image_input_supported=model.image_input_supported;
            break;
        }
    value.storage_root=native_storage::defaults().data_root.string();return value;
}
void set_raw_stream_capture(bool enabled) noexcept {
    auto& s=service();
    std::scoped_lock lock{s.state_mutex};
    if (s.capture_raw_stream.load(std::memory_order_relaxed) == enabled) return;
    s.capture_raw_stream.store(enabled, std::memory_order_release);
    // Avoid exposing stale deltas when capturing is switched mid-request.
    s.blocks.clear();
}
bool raw_stream_capture_enabled() noexcept {
    return service().capture_raw_stream.load(std::memory_order_acquire);
}
std::vector<EventBlock> events_since(std::uint64_t last_seen) {
    auto& s=service();
    if (!s.capture_raw_stream.load(std::memory_order_acquire)) return {};
    std::scoped_lock lock{s.state_mutex};
    std::vector<EventBlock> result;
    if (!s.capture_raw_stream.load(std::memory_order_relaxed)) return result;
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
    phase(enabled?"Assistant uses Local Models":"Local model available; the previous provider remains selected");
    return true;
}
bool refresh() {
    auto& s = service();
    if (s.stopping.load(std::memory_order_acquire)) return false;
    if (s.busy.exchange(true, std::memory_order_acq_rel)) return false;
    phase("Scanning registered native model profiles (read-only)");
    s.work = std::jthread([](std::stop_token stop) {
        auto& current = service();
        try {
            if (!stop.stop_requested()) {
                // A live host has authoritative resident/cache data. It was
                // initialized by a prior explicit request; reuse it. Otherwise,
                // inventory discovery never instantiates a RuntimeHost.
                if (current.published_host.load(std::memory_order_acquire)) {
                    update_metadata(current);
                    phase("Native model inventory refreshed; loaded models unchanged.");
                } else {
                    auto scanned = discover_offline_native_inventory(
                        native_storage::defaults().data_root, stop);
                    if (!stop.stop_requested()) {
                        const auto count = scanned.models.size();
                        {
                            std::scoped_lock lock{current.state_mutex};
                            current.metadata = std::move(scanned);
                        }
                        phase("Found " + std::to_string(count) +
                            " registered native models. Cache and residency are checked when loaded.");
                    }
                }
            }
        } catch (const std::exception& error) {
            phase(std::string{"Native model inventory scan failed: "} + error.what());
        } catch (...) {
            phase("Native model inventory scan failed; live runtime unchanged.");
        }
        current.busy.store(false, std::memory_order_release);
    });
    return true;
}
bool set_gpu_offload(std::string id, std::uint32_t percent) {
    if (id.empty() || percent > 100u) return false;
    return queue("Saving GPU offload for " + id,
        [id=std::move(id), percent](auto& host, std::stop_token) {
            const auto saved = host.set_native_gpu_offload(id,percent);
            phase(saved
                ? "Saved " + id + " GPU target " + std::to_string(percent)
                    + "%. Unload/reload that model to apply; shared VRAM cap still applies."
                : "Offload save failed: " + saved.error().message);
        });
}

bool save_model_tuning(std::string id, ModelTuning tuning) {
    if (id.empty()) return false;
    return queue("Saving native model options for " + id,
        [id=std::move(id), tuning=std::move(tuning)](auto& host, std::stop_token) {
            const auto result = host.set_native_model_tuning(id, model_tuning_json(tuning));
            phase(result ? "Saved " + id + " runtime options; unload/reload to apply."
                : "Model options NOT saved: " + result.error().message);
        });
}

bool benchmark_model(std::string id, std::string prompt) {
    if (id.empty() || prompt.empty() || prompt.size() > 512u) return false;
    auto& s = service();
    if (!s.loaded.load(std::memory_order_acquire)) return false;
    const bool accepted = queue("Benchmarking " + id,
        [&s,id=std::move(id),prompt=std::move(prompt)](auto& host,std::stop_token stop) {
            ModelBenchmark measurement{.model_id=id,.status="Running one 96-token test..."};
            {
                std::scoped_lock lock{s.state_mutex}; s.benchmark=measurement;
            }
            const auto started = std::chrono::steady_clock::now();
            std::optional<std::chrono::steady_clock::time_point> first_token;
            const epoch_ai::runtime::json body{
                {"model","qwenflash://" + id},
                {"epoch_host_stage","model_benchmark"},
                {"max_tokens",96},
                {"stream",true},
                {"tools",epoch_ai::runtime::json::array()},
                {"tool_choice","none"},
                {"chat_template_kwargs",{{"enable_thinking",false}}},
                {"messages",epoch_ai::runtime::json::array({
                    {{"role","user"},{"content",prompt}}})}
            };
            auto result = host.complete_host_request(body,stop,started+std::chrono::seconds{150},
                [&](const epoch_ai::model::StreamEvent& event) {
                    if (!first_token && event.kind == epoch_ai::model::StreamEventKind::text_delta
                        && !event.delta.empty()) first_token=std::chrono::steady_clock::now();
                });
            const auto finished = std::chrono::steady_clock::now();
            measurement.elapsed_ms = std::chrono::duration<double,std::milli>(finished-started).count();
            measurement.completed=static_cast<bool>(result);
            if (result) {
                measurement.status="Completed";
                measurement.reply=result->text.substr(0,240);
                if(first_token) {
                    measurement.first_token_known=true;
                    measurement.first_token_ms=
                        std::chrono::duration<double,std::milli>(*first_token-started).count();
                }
                const auto usage=result->raw.is_object()
                    ? result->raw.value("usage",epoch_ai::runtime::json::object())
                    : epoch_ai::runtime::json::object();
                const auto timings=result->raw.is_object()
                    ? result->raw.value("timings",epoch_ai::runtime::json::object())
                    : epoch_ai::runtime::json::object();
                std::optional<std::uint64_t> reportedTokens;
                if (usage.is_object()) {
                    const auto count=usage.find("completion_tokens");
                    if (count!=usage.end() && count->is_number_unsigned())
                        reportedTokens=count->template get<std::uint64_t>();
                }
                double decodeMs{};
                if (timings.is_object()) {
                    const auto count=timings.find("predicted_n");
                    if (!reportedTokens && count!=timings.end() && count->is_number_unsigned())
                        reportedTokens=count->template get<std::uint64_t>();
                    const auto duration=timings.find("predicted_ms");
                    if (duration!=timings.end() && duration->is_number()) {
                        const double candidate=duration->template get<double>();
                        if (std::isfinite(candidate) && candidate > 0.0)
                            decodeMs=candidate;
                    }
                }
                if (decodeMs <= 0.0 && first_token)
                    decodeMs=std::chrono::duration<double,std::milli>(
                        finished-*first_token).count();
                if (reportedTokens && *reportedTokens <= UINT32_MAX) {
                    measurement.output_tokens=static_cast<std::uint32_t>(*reportedTokens);
                    if (const auto rate=metrics::tokens_per_second(*reportedTokens,decodeMs)) {
                        measurement.tps_known=true;
                        measurement.output_tps=*rate;
                    }
                }
            } else {
                measurement.status=stop.stop_requested() ? "Cancelled" : result.error().message;
            }
            {
                std::scoped_lock lock{s.state_mutex};s.benchmark=std::move(measurement);
            }
            phase("Model test finished; see Model Manager for results");
        });
    return accepted;
}

bool register_model(std::string id,std::string path) {
    if(id.empty()||path.empty())return false;
    return queue("Registering native model",[id=std::move(id),path=std::move(path)](auto& host,std::stop_token){
        const auto result=host.register_native_model({{"id",id},{"display_name",id},{"source","local-gguf"},{"local_path",path}});
        phase(result?"Model registered: "+id:"Model registration failed: "+result.error().message);
    });
}
bool import_model_profiles(std::string selected_path) {
    if (selected_path.empty()) return false;
    return queue("Importing model profiles (metadata only)",
        [selected_path=std::move(selected_path)](auto& host, std::stop_token stop) {
            std::set<std::string> existing;
            const auto library = host.native_model_library();
            for (const auto& item : library.value("models", epoch_ai::runtime::json::array()))
                existing.insert(string_field(item, "id"));
            const auto parsed = epoch_ai::model::read_profile_import(selected_path, existing);
            if (!parsed) { phase("Profile import failed: " + parsed.error()); return; }
            std::size_t imported{};
            for (const auto& spec : parsed->registrations) {
                if (stop.stop_requested()) {
                    phase("Profile import cancelled after " + std::to_string(imported) + " registrations.");
                    return;
                }
                const auto saved = host.register_native_model(spec);
                if (!saved) {
                    phase("Profile import stopped after " + std::to_string(imported)
                        + " registrations: " + saved.error().message);
                    return;
                }
                ++imported;
            }
            phase("Imported " + std::to_string(imported) + " model profiles; skipped "
                + std::to_string(parsed->skipped)
                + " existing/built-in profiles. Saved tuning and model files were preserved; no model was loaded.");
        });
}
bool unregister_model(std::string id) {
    if (id.empty() || id == "qwen3.8-flash-next-ud-q2-k-xl") return false;
    auto& s = service();
    if (s.loaded.load(std::memory_order_acquire)) {
        std::scoped_lock lock{s.state_mutex};
        if (s.model == id) return false;
    }
    return queue("Unregistering native model", [id=std::move(id)](auto& host, std::stop_token){
        const auto result = host.remove_native_model(id, false);
        phase(!result ? "Model unregister failed: " + result.error().message
            : result->value("removed", false)
                ? "Model unregistered: " + id + ". Model weight files were not deleted."
                : "This built-in profile cannot be unregistered.");
    });
}
bool assign_role(std::string role,std::string uri) {
    return queue("Assigning model role",[role=std::move(role),uri=std::move(uri)](auto& host,std::stop_token){
        const auto assigned=host.assign_model_role(role,uri);
        phase(assigned?"Model role saved: "+role:"Role assignment failed: "+assigned.error().message);
    });
}
bool prepare(std::string id) {
    return queue("Preparing Local Models",[id=std::move(id)](auto& host,std::stop_token stop) mutable {
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
        // Keep the existing foundation usable if loading this candidate fails.
        // The host_mutex serializes the actual switch with in-flight requests;
        // readiness is published only after model load and role selection succeed.
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
        if(stop.stop_requested()){host.unload_native_model(id);phase("Native setup cancelled");return;}
        const auto uri=std::string{"qwenflash://"}+id;
        const auto selected=host.set_foundation_model(uri);
        if(!selected){host.unload_native_model(id);phase("Model selection failed: "+selected.error().message);return;}
        auto& current=service();{std::scoped_lock lock{current.state_mutex};current.model=id;}
        current.loaded.store(true,std::memory_order_release);current.enabled.store(true,std::memory_order_release);
        phase("Ready to chat: "+id);
    });
}
void cancel_setup() {
    auto& current=service();current.work.request_stop();
    if(auto* host=current.published_host.load(std::memory_order_acquire))host->cancel_native_installation();
    phase("Cancelling native setup; completed cache bytes are retained");
}
void request_shutdown() noexcept {
    if (auto* current = existing_service.load(std::memory_order_acquire)) {
        if (!current->stopping.load(std::memory_order_acquire)
            && current->busy.load(std::memory_order_acquire))
            core_log_write(3u, "Epoch.AI.Native",
                "Closing during model setup interrupts loading; cancelling the owned worker before context teardown.");
        current->request_shutdown();
    }
}
void finish_shutdown() noexcept {
    if (auto* current = existing_service.load(std::memory_order_acquire)) {
        try { current->finish_shutdown(); }
        catch (...) { core_log_write(4u, "Epoch.AI.Native", "Native service shutdown did not complete."); }
    }
}
bool shutdown_contract() noexcept {
    try {
        // Exercise the real lifecycle with a waiting loader and a late status
        // write, but no model, provider, child process, GUI or listener.
        Service local;
        std::latch started{1};
        std::atomic_bool observedStop{false};
        local.busy.store(true);
        local.work = std::jthread([&](std::stop_token stop) {
            started.count_down();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
            while (!stop.stop_requested() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            std::scoped_lock lock{local.state_mutex};
            local.phase = "loader retired";
            observedStop.store(stop.stop_requested());
        });
        started.wait();
        local.request_shutdown();
        local.finish_shutdown();
        local.finish_shutdown(); // Process cleanup/destruction is idempotent.
        return observedStop.load() && local.phase == "loader retired"
            && local.stopping.load() && !local.busy.load() && !local.loaded.load()
            && !local.work.joinable() && !local.monitor.joinable() && !local.host;
    } catch (...) { return false; }
}
bool provision() {
    return queue("Checking Epoch native Qwen runtime",[](auto& host, std::stop_token stop){
        auto result=host.provision_native_runtime();
        if(!result) { phase("Runtime provisioning failed: "+result.error().message); return; }
        const auto state=result->value("state",std::string{});
        if(state=="installed") { phase("Local Qwen runtime available"); return; }
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
        phase("Downloading model: "+id+" (local model cache)");
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
// Model IDs such as qwen3.8-27b and nemotron-9b have trustworthy
// parameter-size hints. Do not guess from GGUF quantization file sizes or
// automatically replace an operator-assigned simple responder.
[[nodiscard]] static std::optional<unsigned> named_parameter_billions(std::string_view id) {
    static const std::regex count{R"((?:^|[^0-9])([0-9]{1,3})[bB](?:[^a-zA-Z0-9]|$))"};
    std::string text{id};
    std::sregex_iterator it{text.begin(), text.end(), count}, end;
    std::optional<unsigned> result;
    for (; it != end; ++it) {
        const auto count = static_cast<unsigned>(std::stoul((*it)[1].str()));
        if (count > 0 && count < 400) result = count;
    }
    return result;
}
bool load(std::string id, bool select_for_requests) {
    if(id.empty())return false;
    auto& s=service();
    return queue("Loading native model "+id,[id=std::move(id),select_for_requests,&s](auto& host, std::stop_token stop){
        // Apply the same RAM admission to a direct load and a saved set.
        // Re-selecting a resident model never triggers a new load restriction.
        const auto library = host.native_model_library();
        for (const auto& model : library.value("models", epoch_ai::runtime::json::array())) {
            if (string_field(model,"id") != id || model.value("active",false)) continue;
            const auto cache=model.value("cache",epoch_ai::runtime::json::object());
            const auto weights=cache.value("expected_bytes",std::uint64_t{})
                ? cache.value("expected_bytes",std::uint64_t{})
                : cache.value("cached_bytes",model.value("estimated_bytes",std::uint64_t{}));
            std::string reason{};
            if (!safe_to_start_another_model(weights,reason)) { phase(std::move(reason)); return; }
        }
        auto result=host.load_native_model(id,{},stop);
        if(result) {
            // The first successful Load Additional needs a foundation: it is
            // otherwise a resident server with no selected chat model, leaving
            // the composer queued forever while the machine is idle. Preserve
            // an existing foundation when loading a second/third model.
            const bool establishFoundation = detail::should_establish_foundation(
                s.loaded.load(std::memory_order_acquire), select_for_requests);
            if (establishFoundation) {
                const auto native_uri=std::string{"qwenflash://"}+id;
                auto foundation=host.set_foundation_model(native_uri);
                if(!foundation) {
                    host.unload_native_model(id);
                    phase("Native loader succeeded but foundation selection failed: "+foundation.error().message);
                    return;
                }
                {std::scoped_lock lock{s.state_mutex};s.model=id;}
                s.loaded.store(true,std::memory_order_release);
                s.enabled.store(true,std::memory_order_release);
            }
            // Each Load Additional must update automatic routing as models
            // become resident. Preserve an explicitly selected foundation and
            // any external operator-assigned roles. Importantly, 27B remains
            // eligible for AI Engine Dev even if Flash was loaded first.
            std::string assigned_role;
            if (!establishFoundation) {
                const auto role_models = host.architecture().value(
                    "role_models", epoch_ai::runtime::json::object());
                const auto params = named_parameter_billions(id);
                const auto role_is_native_or_free = [&](std::string_view role) {
                    const auto it = role_models.find(std::string{role});
                    return it == role_models.end() || !it->is_string() ||
                        it->template get<std::string>().empty() ||
                        it->template get<std::string>().starts_with("qwenflash://");
                };
                const auto current_role_model = [&](std::string_view role) -> std::string {
                    const auto it = role_models.find(std::string{role});
                    return it != role_models.end() && it->is_string()
                        ? it->template get<std::string>() : std::string{};
                };
                if (params && *params <= 12u && role_is_native_or_free("simple")) {
                    const auto prev = current_role_model("simple");
                    const auto previous_params = named_parameter_billions(prev);
                    if (prev.empty() || !previous_params || *params < *previous_params)
                        if (host.assign_model_role("simple", "qwenflash://" + id))
                            assigned_role = "simple";
                } else if (params && *params >= 20u &&
                           role_is_native_or_free("reviewer")) {
                    // 27B isn't forced into the foundation slot. The auto
                    // router can use it for coding even when Flash loaded first.
                    const auto prev = current_role_model("reviewer");
                    const auto previous_params = named_parameter_billions(prev);
                    if (prev.empty() || !previous_params || *params >= *previous_params)
                        if (host.assign_model_role("reviewer", "qwenflash://" + id))
                            assigned_role = "middle";
                }
            }
            phase("Ready to chat: " + id +
                (establishFoundation ? ". Foundation selected and chat dispatch enabled."
                    : !assigned_role.empty() ? ". Automatic " + assigned_role +
                        " responder available without changing the selected foundation."
                    : ". Foundation and manually assigned roles preserved."));
        } else phase("Load failed: "+result.error().message);
    });
}
// All three native models must be loaded within one owned background request;
// issuing three UI requests would race the one-at-a-time native queue.
bool load_set(std::vector<std::string> ids) {
    if (ids.empty() || ids.size() > 3u) return false;
    for (const auto& id : ids) {
        if (id.empty() || id.size() > 160u ||
            !std::all_of(id.begin(), id.end(), [](unsigned char c) {
                return c > 0x20u && c < 0x7fu;
            })) return false;
    }
    auto unique = ids;
    std::sort(unique.begin(), unique.end());
    if (std::unique(unique.begin(), unique.end()) != unique.end()) return false;
    auto& s = service();
    // Preflight before instantiating the heavyweight host. If the editor is
    // already under severe physical RAM pressure, constructing all agent and
    // server infrastructure can fail before the per-model checks run.
    if (!s.loaded.load(std::memory_order_acquire)) {
        std::string reason{};
        if (!safe_to_start_another_model(0u, reason)) {
            phase(std::move(reason));
            return false;
        }
    }
    trace_load_stage("Saved resident set accepted for asynchronous loading");
    return queue("Loading native resident model set", [ids = std::move(ids), &s](
        auto& host, std::stop_token stop) mutable {
        trace_load_stage("Resident set: native host ready, reading model catalog");
        const auto catalog = host.native_model_library();
        trace_load_stage("Resident set: model catalog read");
        const auto entries = catalog.value("models", epoch_ai::runtime::json::array());
        auto find_entry = [&](const std::string& id) -> const epoch_ai::runtime::json* {
            for (const auto& entry : entries)
                if (string_field(entry, "id") == id) return &entry;
            return nullptr;
        };
        for (const auto& id : ids) {
            if (!find_entry(id)) {
                phase("Set stopped: unknown native profile " + id + ". Rescan or register it first.");
                return;
            }
        }
        // Measured weight bytes best predict residency pressure. When absent,
        // use the model name's *parameter* B count (not Q2/Q4 quantization).
        auto rank = [&](const std::string& id) -> std::uint64_t {
            constexpr auto unknown = std::numeric_limits<std::uint64_t>::max();
            const auto* entry = find_entry(id);
            if (!entry) return unknown;
            const auto cache = entry->value("cache", epoch_ai::runtime::json::object());
            const auto expected = cache.value("expected_bytes", std::uint64_t{});
            const auto cached = cache.value("cached_bytes", std::uint64_t{});
            if (expected || cached) return expected ? expected : cached;
            if (const auto params = named_parameter_billions(id))
                return static_cast<std::uint64_t>(*params) * 1'000'000'000ull;
            return unknown;
        };
        std::stable_sort(ids.begin(), ids.end(), [&](const auto& a, const auto& b) {
            return rank(a) < rank(b);
        });
        // Temporary multi-resident budget: never rewrite user single-model
        // profiles. Reused processes keep their current GPU allocations;
        // lowering those would require an explicit unload/reload.
        const auto weight_pool = ids.size() > 1u ? gpu_budget::weight_budget_bytes(
            host.config().vram_budget_gib, ids.size()) : 0u;
        std::size_t completed = 0;
        bool started_new_model = false;
        bool reused_existing_gpu_settings = false;
        for (const auto& id : ids) {
            if (stop.stop_requested()) {
                phase("Load set cancelled; previously loaded models remain resident.");
                return;
            }
            const auto inventory = host.native_model_library();
            bool resident = false;
            for (const auto& model : inventory.value("models", epoch_ai::runtime::json::array()))
                if (string_field(model, "id") == id && model.value("active", false)) {
                    resident = true;
                    break;
                }
            phase("Resident load " + std::to_string(completed + 1) + "/" +
                std::to_string(ids.size()) + (resident ? " reusing " : " starting ") + id);
            trace_load_stage("Resident set stage " + std::to_string(completed + 1)
                + "/" + std::to_string(ids.size()) + ": " + id
                + (resident ? " already resident" : " starting"));
            if (resident && ids.size() > 1u) reused_existing_gpu_settings = true;
            if (!resident) {
                // Short spacing applies only to consecutive process launches.
                // Re-selecting an already resident saved set must not wait 8 seconds.
                if (started_new_model) {
                    phase("Waiting 4 seconds before the next new model process.");
                    for (unsigned step = 0; step < 40u && !stop.stop_requested(); ++step)
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    if (stop.stop_requested()) {
                        phase("Load set cancelled; previous models remain resident.");
                        return;
                    }
                }
                std::string memory_reason{};
                const auto* profile = find_entry(id);
                const auto cache = profile != nullptr
                    ? profile->value("cache", epoch_ai::runtime::json::object())
                    : epoch_ai::runtime::json::object();
                if (!safe_to_start_another_model(
                        cache.value("expected_bytes",
                            cache.value("cached_bytes", std::uint64_t{})),
                        memory_reason)) {
                    phase(std::move(memory_reason));
                    return;
                }
                // Use the stored profile's measured/estimated GGUF bytes, not
                // parameter labels or a standalone percentage. Even a heavily
                // quantized Flash may still contain tens of GiB of weights.
                const auto weights = cache.value("expected_bytes", std::uint64_t{})
                    ? cache.value("expected_bytes", std::uint64_t{})
                    : cache.value("cached_bytes", std::uint64_t{});
                std::uint64_t estimated = weights;
                if (!estimated && profile != nullptr) {
                    estimated = profile->value("estimated_bytes", std::uint64_t{});
                }
                if (!estimated) {
                    if (auto params = named_parameter_billions(id))
                        estimated = static_cast<std::uint64_t>(*params) * 700'000'000ull;
                }
                const auto hint = profile != nullptr
                    ? profile->value("gpu_layer_count_hint", 48U) : 48U;
                const auto gpu_cap = gpu_budget::offload_cap_percent(
                    estimated, weight_pool, ids.size(), completed, hint);
                if (gpu_cap < 100U) {
                    phase("Starting " + id + " with safe GPU offload cap " +
                        std::to_string(gpu_cap) + "% (solo profile unchanged).");
                }
                trace_load_stage("Resident set: " + id + " estimated " +
                    std::to_string(estimated / (1024u*1024u)) +
                    " MiB weights; temporary GPU offload cap " +
                    std::to_string(gpu_cap) + "% (solo settings unchanged)");
                trace_load_stage("Resident set: calling load_native_model for " + id);
                auto result = host.load_native_model(id, {}, stop,
                    weight_pool > 0u ? std::optional<std::uint32_t>{gpu_cap} : std::nullopt);
                if (!result) {
                    trace_load_stage("Resident set: load_native_model failed for " + id
                        + ": " + result.error().message);
                    phase("Native load stopped at " + id + ": " + result.error().message +
                        ". Earlier resident models are retained.");
                    return;
                }
                started_new_model = true;
            }
            trace_load_stage("Resident set: model stage completed for " + id);
            ++completed;
        }
        if (stop.stop_requested()) {
            phase("Native set cancelled; earlier residents retained.");
            return;
        }
        // An explicit resident set establishes capability roles. Do not demote
        // the strongest resident to escalation merely because it uses more RAM;
        // easy work can use smaller responders without making them the coder.
        auto response_order = ids;
        std::stable_sort(response_order.begin(), response_order.end(),
            [&](const auto& a, const auto& b) {
                const auto small = named_parameter_billions(a);
                const auto large = named_parameter_billions(b);
                if (small && large && *small != *large) return *small < *large;
                return rank(a) < rank(b);
            });
        const auto is_flash = [](const std::string& id) {
            return id.find("flash-next") != std::string::npos ||
                id.find("flash_next") != std::string::npos;
        };
        auto normal_foundation = response_order.back();
        const auto flash = std::find_if(response_order.rbegin(), response_order.rend(), is_flash);
        if (flash != response_order.rend()) normal_foundation = *flash;
        const std::string escalation{};
        const auto& chosen = normal_foundation;
        trace_load_stage("Resident set: selecting normal foundation " + chosen +
            (escalation.empty() ? "" : ", escalation=" + escalation));
        const auto selected = host.set_foundation_model("qwenflash://" + chosen);
        if (!selected) {
            phase("All models resident but foundation selection failed: " + selected.error().message);
            return;
        }
        { std::scoped_lock lock{s.state_mutex}; s.model = chosen; }
        s.loaded.store(true, std::memory_order_release);
        s.enabled.store(true, std::memory_order_release);

        // This explicit set selection is authoritative for native responder
        // roles, even if an earlier set left valid-but-wrong native choices.
        // External LM Studio/operator overrides remain untouched.
        const auto roles = host.architecture().value(
            "role_models", epoch_ai::runtime::json::object());
        auto native_role_editable = [&](std::string_view name) {
            const auto it = roles.find(std::string{name});
            return it == roles.end() || !it->is_string() ||
                it->template get<std::string>().empty() ||
                it->template get<std::string>().starts_with("qwenflash://");
        };
        auto synchronize_role = [&](std::string_view name, const std::string& id) {
            if (!native_role_editable(name)) return true;
            const std::string uri = id.empty() ? std::string{} : "qwenflash://" + id;
            const auto result = host.assign_model_role(name, uri);
            if (!result) {
                phase("Resident set ready but " + std::string{name} +
                    " role assignment failed: " + result.error().message);
                trace_load_stage("Resident set: " + std::string{name} +
                    " role error: " + result.error().message);
                return false;
            }
            return true;
        };
        const std::string simple = response_order.size() > 1u &&
            response_order.front() != chosen ? response_order.front() : std::string{};
        const auto middleModel = std::find_if(response_order.rbegin(), response_order.rend(),
            [&](const auto& id) { return id != chosen && id != simple; });
        const std::string middle = middleModel != response_order.rend()
            ? *middleModel : std::string{};
        if (!synchronize_role("simple", simple) ||
            !synchronize_role("reviewer", middle) ||
            !synchronize_role("implementation", chosen) ||
            !synchronize_role("escalation", escalation)) return;
        trace_load_stage("Resident set: foundation=" + chosen +
            ", simple=" + (simple.empty() ? "none" : simple) +
            ", middle=" + (middle.empty() ? "none" : middle) +
            ", escalation=" + (escalation.empty() ? "none" : escalation));
        phase("Ready to chat: " + std::to_string(ids.size()) +
            " models" + (reused_existing_gpu_settings
                ? " (already-resident GPU allocations unchanged; unload and reload to rebalance)"
                : " (shared GPU offload budgets applied to new processes)") +
            ". Simple: " + (simple.empty() ? "foundation fallback" : simple) +
            "; middle: " + (middle.empty() ? "foundation fallback" : middle) +
            "; foundation: " + chosen +
            "; escalation: " + (escalation.empty() ? "none" : escalation) +
            ". No models are auto-loaded. External role overrides are preserved.");
    });
}

// Only explicit operator actions touch this file. First-open UI and passive
// discovery NEVER access it, preserving the prior crash isolation.
static std::filesystem::path saved_model_set_path() {
    return native_storage::defaults().data_root / "resident_model_set.txt";
}
bool save_load_set(const std::vector<std::string>& ids) {
    const auto error = native_saved_set::save(saved_model_set_path(), ids);
    if (!error.empty()) {
        phase("Saved resident set rejected: " + error);
        return false;
    }
    trace_load_stage("Resident set: saved " + std::to_string(ids.size()) + " model IDs to disk");
    return true;
}
bool load_saved_set() {
    const auto saved = native_saved_set::read(saved_model_set_path());
    if (!saved) {
        phase("Saved resident set not started: " + saved.error);
        trace_load_stage("Resident set: saved-file read rejected: " + saved.error);
        return false;
    }
    trace_load_stage("Resident set: saved file parsed, " + std::to_string(saved.ids.size())
        + " model IDs; scheduling load request");
    // Identical host, memory admission, smallest-first ordering and role
    // selection to Save & Load Set. Saved files never invoke a separate loader.
    return load_set(std::move(saved.ids));
}
bool use_loaded(std::string id) {
    if (id.empty()) return false;
    auto& s = service();
    return queue("Selecting resident native foundation " + id,
        [id = std::move(id), &s](auto& host, std::stop_token){
            const auto library = host.native_model_library();
            std::string uri{};
            for (const auto& item : library.value("models", epoch_ai::runtime::json::array())) {
                if (string_field(item, "id") == id && item.value("active", false)) {
                    uri = string_field(item, "uri");
                    break;
                }
            }
            if (uri.empty()) {
                phase("Native model is not resident: " + id + ". Use Load & Use to prepare it.");
                return;
            }
            const auto selected = host.set_foundation_model(uri);
            if (!selected) {
                phase("Could not select resident native foundation: " + selected.error().message);
                return;
            }
            { std::scoped_lock lock{s.state_mutex}; s.model = id; }
            s.loaded.store(true, std::memory_order_release);
            s.enabled.store(true, std::memory_order_release);
            phase("Ready to chat: " + id + " (selected without reloading weights)");
        });
}
bool unload() {
    auto& s=service();
    // Failed admission must not deselect the existing foundation. Clear the
    // selected flag only after the queued unload actually begins.
    return queue("Unloading native model",[&s](auto& host, std::stop_token){
        s.enabled.store(false, std::memory_order_release);
        host.unload_native_model();s.loaded.store(false);
        phase("Model unloaded; weights and saved settings preserved");
    });
}
bool unload(std::string id) {
    if (id.empty()) return false;
    auto& s=service();
    return queue("Unloading native model "+id, [id=std::move(id),&s](auto& host,std::stop_token){
        host.unload_native_model(id);
        bool selected = false;
        { std::scoped_lock lock{s.state_mutex}; selected = s.model == id; }
        if (selected) {
            // Unloading the foundation must not strand a usable resident model.
            // Promote an already-running alternative without loading weights.
            const auto library = host.native_model_library();
            bool replaced = false;
            for (const auto& item : library.value("models", epoch_ai::runtime::json::array())) {
                if (!item.value("active", false) || string_field(item, "id") == id) continue;
                const auto replacement_id = string_field(item, "id");
                const auto replacement_uri = string_field(item, "uri");
                if (replacement_uri.empty() || replacement_id.empty()) continue;
                const auto switched = host.set_foundation_model(replacement_uri);
                if (!switched) continue;
                { std::scoped_lock lock{s.state_mutex}; s.model = replacement_id; }
                s.loaded.store(true);
                replaced = true;
                phase("Model unloaded: " + id + ". Resident foundation is now " + replacement_id + ".");
                break;
            }
            if (!replaced) {
                s.loaded.store(false);
                s.enabled.store(false);
            }
            if (replaced) return;
        }
        phase("Model unloaded: " + id + ". Other resident models were preserved.");
    });
}
Result complete_host_request(std::string_view request_json,
    std::uint32_t timeout_seconds, std::stop_token cancellation, const StreamObserver& observer) {
    auto& s=service();
    if(!s.enabled.load()||!s.loaded.load())return {false,"Native model is not loaded and selected"};
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds{timeout_seconds};
    const auto startedAt = std::chrono::steady_clock::now();
    std::optional<std::chrono::steady_clock::time_point> firstOutputAt{};
    std::unique_lock lock{s.host_mutex,std::defer_lock};
    while(!lock.try_lock()) {
        if(cancellation.stop_requested() || s.stopping.load())return {false,"Native request cancelled"};
        if(std::chrono::steady_clock::now()>=deadline)return {false,"Native request deadline expired while waiting for the model"};
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if(cancellation.stop_requested() || s.stopping.load())return {false,"Native request cancelled"};
    try {
        auto& host=ensure_host(s);
        // Independent of the bounded/normalized chat view. Keep exact provider
        // deltas and the completed host packet locally, never prompts or auth
        // headers. A missing audit must not silently admit a source proposal.
        const auto auditRoot = native_storage::defaults().data_root / "logs" / "host-requests";
        std::filesystem::create_directories(auditRoot);
        const auto auditPath = auditRoot / ("response-" + std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count()) + ".jsonl");
        std::ofstream audit(auditPath, std::ios::binary | std::ios::out | std::ios::noreplace);
        std::size_t auditBytes{};
        const auto retain = [&](const epoch_ai::model::json& record) {
            const auto bytes = record.dump();
            constexpr std::size_t maximumAuditBytes = 64u * 1024u * 1024u;
            if (bytes.size() + 1u > maximumAuditBytes - auditBytes)
                throw std::runtime_error("Native response audit limit reached; response not admitted");
            audit.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            audit.put('\n');
            audit.flush();
            if (!audit) throw std::runtime_error("Cannot retain native response audit; response not admitted");
            auditBytes += bytes.size() + 1u;
        };
        auto request = epoch_ai::model::json::parse(request_json);
        retain({{"kind", "begin"}, {"schema", "epoch.host-response.v1"},
            {"stage", request.value("epoch_host_stage", std::string{})},
            {"max_tokens", request.value("max_tokens", 0)},
            {"tool_count", request.contains("tools") ? request.at("tools").size() : 0u},
            {"tool_required", request.value("tool_choice", epoch_ai::model::json{}) == "required"}});
        trace_load_stage("Exact model output audit: " + auditPath.string());
        activity("Tools", "Raw output audit: " + auditPath.string());
        if (raw_stream_capture_enabled()) append_output(OutputKind::boundary);
        phase("Epoch host model request; approvals and tools remain with Epoch");
        auto response=host.complete_host_request(request,
            cancellation,deadline,
            [&](const epoch_ai::model::StreamEvent& event) {
                using Kind=epoch_ai::model::StreamEventKind;
                retain({{"kind", event.kind == Kind::tool_argument_delta ? "tool_arguments"
                    : event.kind == Kind::reasoning_delta ? "reasoning"
                    : event.kind == Kind::text_delta ? "text" : "tool_started"},
                    {"index", event.index}, {"tool_call_id", event.tool_call_id},
                    {"tool_name", event.tool_name}, {"delta", event.delta},
                    {"elapsed_ms", std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - startedAt).count()}});
                const auto kind=event.kind==Kind::reasoning_delta?OutputKind::reasoning
                    :event.kind==Kind::tool_argument_delta?OutputKind::tool_arguments
                    :event.kind==Kind::text_delta?OutputKind::text:OutputKind::boundary;
                if (!event.delta.empty() && kind != OutputKind::boundary
                    && !firstOutputAt)
                    firstOutputAt = std::chrono::steady_clock::now();
                if (raw_stream_capture_enabled()) append_output(kind,event.delta);
                if(observer)observer(kind,event.delta);
            });
        if (raw_stream_capture_enabled()) append_output(OutputKind::boundary);
        if(!response){retain({{"kind", "failed"}, {"error", response.error().message}});phase("Native request failed: "+response.error().message);return {false,response.error().message};}
        if (!response->model.empty()) {
            std::scoped_lock lock{s.state_mutex};
            s.last_response_model = response->model;
            s.last_response_role = "AI Engine Dev host model";
        }
        phase("AI Engine Dev host completion: " + response->model);
        phase("Model response completed; Epoch validates the result");
        // Preserve provider usage/termination even when an incomplete packet is
        // rejected. A token-limit exception must not leave a misleading audit
        // containing only a begin record and deltas.
        auto finishReasons = epoch_ai::model::json::array();
        if (response->raw.is_object() && response->raw.contains("choices")
            && response->raw.at("choices").is_array())
            for (const auto& choice : response->raw.at("choices"))
                if (choice.is_object() && choice.contains("finish_reason"))
                    finishReasons.push_back(choice.at("finish_reason"));
        retain({{"kind", "provider_complete"}, {"model", response->model},
            {"finish_reasons", std::move(finishReasons)},
            {"usage", response->raw.is_object()
                ? response->raw.value("usage", epoch_ai::model::json::object())
                : epoch_ai::model::json::object()}});
        Result completed{};
        try { completed = Result{true,epoch_ai::model::host_response(*response).dump()}; }
        catch (const std::exception& error) {
            retain({{"kind", "failed"}, {"error", error.what()}});
            throw;
        }
        retain({{"kind", "complete"}, {"host_response", completed.message}});
        // Only the model/provider's reported completion-token count can be
        // used for t/s. Never interpret stream bytes or events as tokens.
        const auto usage = response->raw.is_object()
            ? response->raw.value("usage", epoch_ai::model::json::object())
            : epoch_ai::model::json::object();
        if (usage.is_object() && usage.contains("completion_tokens")
            && usage.at("completion_tokens").is_number_unsigned())
        {
            completed.completion_tokens = usage.at("completion_tokens")
                .get<std::uint32_t>();
            const double generationSeconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now()
                    - firstOutputAt.value_or(startedAt)).count();
            if (completed.completion_tokens > 0u && generationSeconds > 0.0)
            {
                completed.generation_tps =
                    static_cast<double>(completed.completion_tokens) / generationSeconds;
                completed.generation_tps_known = true;
            }
        }
        return completed;
    }catch(const std::exception& e){phase(std::string{"Native request failed: "}+e.what());return {false,e.what()};}
}
} // namespace epochengine::ai::native_runtime
