"""Actual shared model metadata/classification and automatic-choice policy; no model loads."""
from pathlib import Path
import json
import os
import unittest

from Tests.support.cpp import compile_and_run, dependency_include
from Tests.platform.test_child_close_router import definition

ROOT = Path(__file__).resolve().parents[2]


class ModelBehavior(unittest.TestCase):
    def test_actual_registry_and_ambient_selection(self):
        native=ROOT/'Engine/src/ai/native'
        owner=(native/'ai.backend_model_multiplex_provider.cpp').read_text()
        functions='\n'.join(definition(owner, marker) for marker in (
            'bool MultiplexProvider::requires_explicit_selection(',
            'std::expected<std::string, core::Error> MultiplexProvider::resolve_request_model(',
            'std::string MultiplexProvider::resolve_model_hint('))
        # All inference/preparation entry points must return the admission error
        # before reaching any selected provider, not silently choose another one.
        for signature in ('std::expected<ModelResponse, core::Error> MultiplexProvider::complete(',
                          'std::expected<ModelResponse, core::Error> MultiplexProvider::complete_stream(',
                          'std::expected<json, core::Error> MultiplexProvider::prepare('):
            actual=definition(owner,signature)
            resolution=actual.index('const auto resolution = resolve_request_model(')
            rejection=actual.index('if (!resolution) return std::unexpected(resolution.error());')
            resolved=actual.index('const auto& resolved_model = *resolution;')
            self.assertLess(resolution,rejection);self.assertLess(rejection,resolved)
        runtime=(native/'ai.backend_runtime_runtime_host.cpp').read_text()
        self.assertIn('impl_->model.bind_model_selections(impl_->models);',
                      definition(runtime,'RuntimeHost::RuntimeHost('))
        code=r'''#include <chrono>
#include <expected>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "PROFILE_CPP"
#include "REGISTRY_CPP"
#include "ERROR_CPP"
#include "PROVIDER_HPP"
namespace epoch_ai::model {
struct NativePort {
 std::optional<std::string> ambient;
 std::vector<RuntimeModelProfile> profiles;
 json running=json::object();
 std::optional<std::string> active_model_uri()const{return ambient;}
 std::optional<RuntimeModelProfile> find(std::string_view id)const{
  for(const auto& p:profiles)if(p.id==id||runtime_model_uri(p)==id)return p;
  return std::nullopt;
 }
 json model_runtime_status(std::string_view)const{return running;}
};
struct MultiplexProvider: IModelProvider {
 NativePort& native_models_;const models::ModelRegistry* selection_registry_{};
 explicit MultiplexProvider(NativePort& p):native_models_(p){}
 bool requires_explicit_selection(std::string_view)const override;
 std::expected<std::string,core::Error> resolve_request_model(std::string_view)const;
 std::string resolve_model_hint(std::string_view)const;
 std::expected<ModelResponse,core::Error> complete(const ModelRequest&)override{return ModelResponse{};}
 ProviderCapabilities capabilities()const noexcept override{return {};}
 std::expected<json,core::Error> discover()override{return json::object();}
};
ACTUAL_FUNCTIONS
}
using namespace epoch_ai;
void write(const std::filesystem::path& p,const nlohmann::json& j){
 std::filesystem::create_directories(p.parent_path());std::ofstream out(p);out<<j.dump();
 if(!out)throw std::runtime_error("fixture write failed");
}
void component(const std::filesystem::path& state,std::string_view kind,std::string_view id,
               std::string_view uri,nlohmann::json extra){
 const auto p=state/"models"/kind;
 auto j=nlohmann::json{{"format","engcoder.model-component.v2"},{"id",id},
  {"display_name","Neutral label"},{"provider_model",uri},{"enabled",true},{"inference_ready",true}};
 j.update(extra);write(p/"active.json",{{"generation","g"}});write(p/"generations/g/manifest.json",j);
}
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){try{
 const auto root=std::filesystem::current_path()/("registry-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 core::Config config;config.model="auto";config.controller_model="";
 component(root,"foundation","neutral","engcoder://neutral",{{"behavior_labels",{"abliterated"}}});
 component(root,"controller","control","engcoder://control",{{"tags",{"derestricted"}}});
 models::ModelRegistry registry(config,root);
 CHECK(registry.find("neutral")->behavior.flagged());CHECK(registry.find("control")->behavior.flagged());
 CHECK(!registry.active(models::ComponentKind::foundation));CHECK(!registry.active(models::ComponentKind::controller));
 CHECK(registry.foundation_model()=="auto");CHECK(registry.controller_model().empty());
 CHECK(registry.explicit_model_selections().empty());
 CHECK(registry.set_foundation_model("engcoder://neutral"));
 CHECK(registry.set_role_model("controller","engcoder://control"));registry.refresh();
 CHECK(registry.foundation_model()=="engcoder://neutral");CHECK(registry.controller_model()=="engcoder://control");
 CHECK(registry.explicit_model_selections().size()==2);
 core::Config flagged;flagged.model="publisher/uncensored";flagged.controller_model="publisher/abliterated";
 models::ModelRegistry configured(flagged,root/"configured");
 CHECK(configured.foundation_model()=="auto");CHECK(configured.controller_model().empty());
 CHECK(configured.set_foundation_model(flagged.model));CHECK(configured.set_role_model("controller",flagged.controller_model));
 CHECK(configured.foundation_model()==flagged.model);CHECK(configured.controller_model()==flagged.controller_model);
 component(root/"normal","foundation","plain","engcoder://plain",nlohmann::json::object());
 component(root/"normal","controller","plain-control","engcoder://plain-control",nlohmann::json::object());
 models::ModelRegistry ordinary(config,root/"normal");
 CHECK(ordinary.foundation_model()=="engcoder://plain");CHECK(ordinary.controller_model()=="engcoder://plain-control");
 model::RuntimeModelProfile p;p.id="neutral-native";p.display_name="Neutral";p.behavior_labels={"uncensored"};
 model::NativePort port;port.profiles={p};port.ambient=model::runtime_model_uri(p);
 model::MultiplexProvider provider(port);
 CHECK(!provider.resolve_request_model(""));CHECK(!provider.resolve_request_model("auto"));
 CHECK(provider.resolve_model_hint("auto").empty());
 CHECK(provider.resolve_request_model(*port.ambient)==*port.ambient); // explicit request preserved
 provider.selection_registry_=&ordinary;CHECK(!provider.resolve_request_model("auto"));
 CHECK(ordinary.set_foundation_model("different-provider"));CHECK(!provider.resolve_request_model("auto"));
 CHECK(ordinary.set_foundation_model(p.id));CHECK(provider.resolve_request_model("auto")==*port.ambient); // exact alias canonicalized
 // A saved role is sufficient only when it names this exact running model.
 const auto roleRoot=root/"role";
 write(roleRoot/"models/runtimes/neutral-native/manifest.json",{{"format","engcoder.runtime-model.v1"},
  {"runtime","qwen-flash"},{"id",p.id},{"behavior_labels",{"uncensored"}}});
 models::ModelRegistry roles(config,roleRoot);provider.selection_registry_=&roles;
 CHECK(!provider.resolve_request_model("auto"));
 CHECK(roles.set_role_model("reviewer",*port.ambient));
 CHECK(provider.resolve_request_model("auto")==*port.ambient);
 CHECK(roles.set_role_model("reviewer",""));CHECK(!provider.resolve_request_model("auto"));
 // Actual running profile remains restrictive if a saved neutral profile changes.
 port.profiles.front().behavior_labels.clear();
 port.running={{"running",true},{"effective_profile",{{"id",p.id},{"behavior_labels",{"abliterated"}}}}};
 CHECK(!provider.resolve_request_model("auto"));
 port.running=nlohmann::json::object();CHECK(provider.resolve_request_model("auto")==*port.ambient);
 port.ambient.reset();CHECK(provider.resolve_request_model("")=="auto");CHECK(provider.resolve_request_model("auto")=="auto");
}catch(const std::exception& error){std::ofstream out("contract-error.txt");out<<error.what();return 99;}}
'''
        for key,name in {'PROFILE_CPP':'ai.backend_model_runtime_model.cpp',
                         'REGISTRY_CPP':'ai.backend_models_model_registry.cpp',
                         'ERROR_CPP':'ai.backend_core_error.cpp',
                         'PROVIDER_HPP':'ai.backend_model_model_provider.hpp'}.items():
            code=code.replace(key,(native/name).as_posix())
        code=code.replace('ACTUAL_FUNCTIONS',functions)
        report=ROOT/'Artifacts/Reports/model-behavior-oct29/registry'
        report.mkdir(parents=True,exist_ok=True)
        source=report/'registry.cpp';source.write_text(code)
        result=compile_and_run(self,source,report,includes=[dependency_include(ROOT)])
        (report/'result.json').write_text(json.dumps(result,indent=2)+'\n')

    def test_actual_native_weight_store_admission(self):
        owner=(ROOT/'Engine/src/ai/ai.native_runtime.cpp').read_text()
        actual=definition(owner,'std::filesystem::path model_install_cache_root() noexcept')
        code=r'''
#include <cstdlib>
#include <filesystem>
#include <string>
#include "STORAGE"
namespace epochengine::ai::native_runtime {
ACTUAL
}
void environment(const char* key,const std::string& value){
#ifdef _WIN32
 _putenv_s(key,value.c_str());
#else
 if(value.empty())unsetenv(key);else setenv(key,value.c_str(),1);
#endif
}
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
 using namespace epochengine::ai;
 const auto root=std::filesystem::current_path()/"model-store-fixture";
 for(const auto* key:{native_shared_inputs::cache_variable,native_shared_inputs::disabled_variable,native_shared_inputs::directories_variable})environment(key,"");
#ifdef _WIN32
 environment("LOCALAPPDATA",root.string());
#else
 environment("XDG_DATA_HOME",root.string());
#endif
 CHECK(native_runtime::model_install_cache_root()==root/"EpochEngine/AI/model-cache");
 CHECK(!std::filesystem::exists(root/"EpochEngine"));
 const auto shared=root/"borrowed";std::filesystem::create_directories(shared);
 environment(native_shared_inputs::cache_variable,shared.string());
 CHECK(native_runtime::model_install_cache_root().empty());
 environment(native_shared_inputs::disabled_variable,"1");CHECK(native_runtime::model_install_cache_root().empty());
 environment(native_shared_inputs::cache_variable,"");CHECK(native_runtime::model_install_cache_root().empty());
 environment(native_shared_inputs::disabled_variable,"");environment(native_shared_inputs::cache_variable,"relative-cache");
 CHECK(native_runtime::model_install_cache_root().empty());
 environment(native_shared_inputs::cache_variable,"");environment(native_shared_inputs::directories_variable,"bad");
 CHECK(native_runtime::model_install_cache_root().empty());
 CHECK(!std::filesystem::exists(root/"EpochEngine"));
}
'''.replace('STORAGE',(ROOT/'Engine/src/ai/ai.epoch_native_storage.hpp').as_posix()).replace('ACTUAL',actual)
        report=ROOT/'Artifacts/Reports/model-behavior-oct29/model-store'
        report.mkdir(parents=True,exist_ok=True)
        source=report/'store.cpp';source.write_text(code)
        result=compile_and_run(self,source,report)
        (report/'result.json').write_text(json.dumps(result,indent=2)+'\n')

    def test_actual_catalog_action_gates(self):
        owner=(ROOT/'Engine/src/editor/application/editor.frame_model_manager.inl').read_text()
        start=owner.index('                // Offers are metadata only;')
        body=owner[start:owner.index('            else if (editor.aiModelManagerTab == 0)',start)]
        body=body[:body.rfind('            }')]
        self.assertLess(body.index('Download pinned weights'),body.index('if (editor.aiModelCatalogDetailsOpen)'))
        code=r'''
#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "BEHAVIOR"
#include "CATALOG"
namespace ai=epochengine::ai;
#include "FORMAT_HEADER"
struct EditorState {std::string aiModelCatalogSelection,aiModelCatalogApproval,packageInstallStatus;float packageInstallProgress{};bool packageModelInstall{},aiModelCatalogDetailsOpen{};};
struct NativeState {struct Model{std::string id;};std::vector<Model> models;bool busy{};};
unsigned downloads{},registrations{},cancellations{},warnings{},technicalLines{};bool ack{};std::optional<std::size_t> fixtureChoice;
std::string click,captured;
bool writableStore=true;
namespace native {std::filesystem::path model_install_cache_root(){return writableStore?std::filesystem::current_path()/"shared-model-store":std::filesystem::path{};}}
namespace gui {
struct Vec2{float x{},y{};};enum class SemanticTone{warning,info};enum class InlineButtonAlignment{Center};
struct SelectBoxOptions {std::string_view id,placeholder,selected;std::span<const std::string_view> options;Vec2 size;bool fit_content{};InlineButtonAlignment alignment{};unsigned max_visible_options{};};
struct Result{bool changed{};std::optional<std::size_t> selected_index;};
struct InlineButtonSpec{std::string_view label;bool enabled{true};};
struct ProgressBarOptions{std::string_view label,status;float value{};Vec2 size;bool show_percent{},activity{};};
void section_heading(std::string_view,SemanticTone=SemanticTone::warning){} void wrapped_label(std::string_view,float){}
void semantic_block(std::string_view,SemanticTone,float){++warnings;}
void progress_bar(ProgressBarOptions){}
Result select_box(SelectBoxOptions){const auto choice=fixtureChoice;fixtureChoice.reset();return {choice.has_value(),choice};}
bool toggle_switch(std::string_view,bool& value,Vec2){if(!ack)return false;ack=false;value=!value;return true;}
template<std::size_t N>std::optional<std::size_t> inline_button_row(const std::array<InlineButtonSpec,N>& actions,float,float,int){
 for(std::size_t i{};i<N;++i)if(actions[i].enabled&&actions[i].label==click){click.clear();return i;}return {};
}
}
bool start_model_offer_install(EditorState&,std::string_view id){++downloads;captured=id;return true;}
bool start_model_offer_registration(EditorState&,std::string_view id){++registrations;captured=id;return true;}
bool cancel_model_offer_operation(EditorState&){++cancellations;return true;}
void draw(EditorState& editor,const NativeState& nativeState,bool modelRequestActive=false){
 constexpr float managerWidth=640;const int managerActionLayout=0;
 const auto modelInfo=[](std::string_view,std::string_view){++technicalLines;};
ACTUAL
}
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
 using namespace epoch_ai::model;EditorState editor;NativeState state;
 click="Download pinned weights";draw(editor,state);CHECK(downloads==0&&registrations==0);
 fixtureChoice=0;click="Download pinned weights";draw(editor,state);CHECK(warnings&&downloads==0);
 ack=true;click="Download pinned weights";draw(editor,state);CHECK(downloads==1&&captured==catalog::offers()[0].id);
 fixtureChoice=1;click="Download pinned weights";draw(editor,state);CHECK(downloads==1&&editor.aiModelCatalogApproval.empty());
 ack=true;state.busy=true;click="Download pinned weights";draw(editor,state);CHECK(downloads==1);
 state.busy=false;click="Download pinned weights";draw(editor,state,true);CHECK(downloads==1);
 writableStore=false;click="Download pinned weights";draw(editor,state);CHECK(downloads==1);writableStore=true;
 CHECK(technicalLines==0);click="Source & verification >";draw(editor,state);CHECK(technicalLines>0);
 editor.packageModelInstall=true;click="Download pinned weights";draw(editor,state);CHECK(downloads==1);
 editor.packageModelInstall=false;state.models.push_back({std::string{catalog::offers()[1].id}});
 click="Verify & Register";draw(editor,state);CHECK(registrations==0);
 state.models.clear();click="Verify & Register";draw(editor,state);CHECK(registrations==1&&captured==catalog::offers()[1].id);
 fixtureChoice=4;ack=true;click="Download pinned weights";draw(editor,state);CHECK(downloads==1&&registrations==1);
 CHECK(editor.aiModelCatalogApproval.empty());
}
'''.replace('BEHAVIOR',(ROOT/'Engine/src/ai/ai.model_behavior.hpp').as_posix()).replace(
            'CATALOG',(ROOT/'Engine/src/ai/native/ai.backend_models_candidate_catalog.hpp').as_posix()).replace('ACTUAL',body).replace('FORMAT_HEADER',(ROOT/'Engine/include/core.format_text.hpp').as_posix())
        report=ROOT/'Artifacts/Reports/model-behavior-oct29/catalog-ui'
        report.mkdir(parents=True,exist_ok=True)
        source=report/'catalog_ui.cpp';source.write_text(code)
        result=compile_and_run(self,source,report)
        (report/'result.json').write_text(json.dumps(result,indent=2)+'\n')

    def test_actual_manager_navigation_progress_and_footer(self):
        owner=(ROOT/'Engine/src/editor/application/editor.frame_model_manager.inl').read_text()
        nav=owner[owner.index('            // One fitted navigation surface.'):owner.index('            const auto nativeState = native::snapshot();')]
        actions=owner[owner.index('            std::vector<gui::InlineButtonSpec> visibleManagerFooterActions;'):owner.index('            const gui::InlineButtonRowOptions managerFooterLayout{')]
        footer=owner[owner.index('            // Fixed live progress:'):owner.index('            gui::end_floating_window();')]
        label=definition(owner,'                const auto registeredModelLabel = [](',semicolon=True)
        self.assertLess(owner.index('gui::end_scroll_area();'),owner.index('// Fixed live progress:'))
        code=r'''#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "CATALOG"
#include "FORMAT_HEADER"
struct Install{struct Artifact{std::uint64_t bytes{};};struct Plan{std::vector<Artifact> artifacts;};std::uint64_t totalBytes{};std::optional<bool> publishPending;bool cancelled{};Plan plan;};
struct EditorState{int aiModelManagerSection{},aiModelManagerTab{};std::optional<Install> packageModelInstall;float packageInstallProgress{};std::string aiModelManagerFeedback;bool showAiModelManagerModal{true};};
struct NativeState{bool busy{};};unsigned cancelDownloads{},cancelLoads{},rows{},overflow{};float availableWidth{},bar{};
std::string clicked,summary;bool clickView{};
namespace native{void cancel_setup(){++cancelLoads;}}
bool cancel_model_offer_operation(EditorState&){++cancelDownloads;return true;}
namespace gui{
struct Vec2{float x{},y{};};Vec2 cursor;Vec2 cursor_position(){return cursor;}void set_cursor(Vec2 v){cursor=v;}
struct SegmentedButtonSpec{std::string_view label;float width{};bool active{};};
struct InlineButtonSpec{std::string_view label;float width{};bool enabled{true};};
struct ProgressBarOptions{std::string_view label,status;float value{};Vec2 size;bool show_percent{},activity{};};
float segmented_button_row_width(std::span<const SegmentedButtonSpec> items,float gap){float width{};for(const auto& item:items)width+=static_cast<float>(item.label.size()*9+24);return width+gap*(items.size()-1);}
std::optional<std::size_t> segmented_button_row(std::span<const SegmentedButtonSpec> items,float,float gap){
 ++rows;if(segmented_button_row_width(items,gap)>availableWidth)++overflow;cursor.y+=35;
 for(std::size_t i{};i<items.size();++i)if(clickView&&items[i].label==clicked){clickView=false;return i;}return {};
}
void wrapped_label(std::string_view s,float){summary=s;}void progress_bar(ProgressBarOptions o){bar=o.value;}
std::optional<std::size_t> inline_button_row(std::span<const InlineButtonSpec> actions,float,float,int){
 for(std::size_t i{};i<actions.size();++i)if(actions[i].enabled&&actions[i].label==clicked){clicked.clear();return i;}return {};
}
}
void nav(EditorState& editor,float managerWidth){
 const float managerContentX=10;availableWidth=managerWidth;
 constexpr std::string_view managerSections[]{"Models","Roles","Routing","Training","Native Tools"};
 const auto managerSectionIndex=static_cast<std::size_t>(std::clamp(editor.aiModelManagerSection,0,4));
NAV
}
void footer(EditorState& editor,const NativeState& nativeState){
 constexpr float managerWidth=640,managerFooterY=480;const gui::Vec2 managerBodyOrigin{10,90};const int managerFooterLayout=0;
ACTIONS
FOOTER
}
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
 EditorState editor;NativeState state;clicked="Browse";clickView=true;nav(editor,350);
 CHECK(rows>1&&overflow==0&&editor.aiModelManagerTab==3&&editor.aiModelManagerSection==0);
 clicked="Local API";clickView=true;nav(editor,900);CHECK(editor.aiModelManagerTab==1);
 clicked="Roles";clickView=true;nav(editor,900);CHECK(editor.aiModelManagerSection==1);
 struct Entry{std::string id,label;};
LABEL
 const auto& offer=epoch_ai::model::catalog::offers().front();
 CHECK(registeredModelLabel(Entry{std::string{offer.id},std::string{offer.id}})==offer.label);
 CHECK(registeredModelLabel(Entry{std::string{offer.id},"My custom label"})=="My custom label");
 CHECK(registeredModelLabel(Entry{"ordinary-id","ordinary-name"})=="ordinary-name");
 editor.packageModelInstall=Install{8ull*1024*1024*1024};editor.packageInstallProgress=.5f;
 clicked="Cancel download / verification";footer(editor,state);
 CHECK(cancelDownloads==1&&summary.find("50%")!=std::string::npos&&summary.find("4.00 / 8.00 GiB")!=std::string::npos&&bar==.5f);
 CHECK(summary.find("gguf")==std::string::npos&&editor.showAiModelManagerModal);
 editor.packageModelInstall->publishPending=true;clicked="";footer(editor,state);
 CHECK(summary.find("Checking hashes")!=std::string::npos&&summary.find("Progress unavailable")!=std::string::npos);
 editor.packageModelInstall->totalBytes=0;editor.packageModelInstall->plan.artifacts={{8ull*1024*1024*1024}};footer(editor,state);
 CHECK(summary.find("8.00 GiB")!=std::string::npos&&summary.find('%')==std::string::npos);
 editor.packageModelInstall->cancelled=true;footer(editor,state);CHECK(summary.find("Stopping")!=std::string::npos);
 state.busy=true;clicked="Cancel Loading";footer(editor,state);CHECK(cancelLoads==1&&cancelDownloads==1);
 clicked="Close Model Manager";footer(editor,state);CHECK(!editor.showAiModelManagerModal&&cancelLoads==1&&cancelDownloads==1);
}
'''
        for token,value in {'CATALOG':(ROOT/'Engine/src/ai/native/ai.backend_models_candidate_catalog.hpp').as_posix(),
                            'FORMAT_HEADER':(ROOT/'Engine/include/core.format_text.hpp').as_posix(),'NAV':nav,'ACTIONS':actions,'FOOTER':footer,'LABEL':label}.items():
            code=code.replace(token,value)
        report=ROOT/'Artifacts/Reports/model-manager-layout-oct29/cpu'
        report.mkdir(parents=True,exist_ok=True)
        source=report/'layout.cpp';source.write_text(code)
        result=compile_and_run(self,source,report)
        (report/'result.json').write_text(json.dumps(result,indent=2)+'\n')

    def test_actual_candidate_registration_defaults(self):
        owner=(ROOT/'Engine/src/ai/ai.native_runtime.cpp').read_text()
        actual=definition(owner,'bool register_model(')
        code=r'''
#include <expected>
#include <stop_token>
#include <string>
#include <nlohmann/json.hpp>
namespace epoch_ai::runtime {using json=nlohmann::json;}
nlohmann::json captured;unsigned requests{};struct Error{std::string message;};
struct Host{std::expected<nlohmann::json,Error> register_native_model(const nlohmann::json& value){captured=value;++requests;return value;}};
void phase(const std::string&){}
template<class F>bool queue(const std::string&,F function){Host host;function(host,std::stop_token{});return true;}
ACTUAL
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
 CHECK(register_model("candidate","existing.gguf",true));
 CHECK(requests==1&&captured["only_if_absent"]==true&&captured["context_tokens"]==8192u);
 CHECK(captured["parallel_slots"]==1u&&captured["context_reserve_tokens"]==1024u&&captured["speculative_enabled"]==false);
 CHECK(captured["runtime_policy"]=="recommended"&&captured["gpu_layers"]=="auto");
 CHECK(register_model("ordinary","existing.gguf",false));
 CHECK(requests==2&&captured["only_if_absent"]==false&&!captured.contains("context_tokens")&&!captured.contains("gpu_layers"));
 CHECK(!register_model("","existing.gguf",true)&&requests==2);
}
'''.replace('ACTUAL',actual)
        report=ROOT/'Artifacts/Reports/model-behavior-oct29/registration-defaults'
        report.mkdir(parents=True,exist_ok=True)
        source=report/'registration.cpp';source.write_text(code)
        result=compile_and_run(self,source,report,includes=[dependency_include(ROOT)])
        (report/'result.json').write_text(json.dumps(result,indent=2)+'\n')

    def test_actual_no_replace_profile_publication(self):
        native = ROOT/'Engine/src/ai/native'
        owner=(native/'ai.backend_model_native_model_manager.cpp').read_text()
        save=definition(owner, 'void NativeModelManager::save_profile(')
        code=r'''
#include <atomic>
#include <fstream>
#include <iterator>
#include <latch>
#include <thread>
#include "PROFILE"
#include "IDS"
#include "REPLACE_WINDOWS"
#include "REPLACE_POSIX"
namespace epoch_ai::model {
class NativeModelManager {
 std::filesystem::path root;
public:
 explicit NativeModelManager(std::filesystem::path value):root(std::move(value)){}
 std::filesystem::path manifest_path(std::string_view id)const{return root/std::string{id}/"manifest.json";}
 void save_profile(const RuntimeModelProfile&,bool)const;
};
ACTUAL_SAVE
}
#define CHECK(value) do{if(!(value))return __LINE__;}while(false)
std::string read(const std::filesystem::path& path){std::ifstream in{path};return {std::istreambuf_iterator<char>{in},{}};}
int main(){try{
 using namespace epoch_ai::model;
 const auto root=std::filesystem::current_path()/"profiles";
 NativeModelManager manager{root};RuntimeModelProfile p;p.id="race";p.repo="synthetic/race";
 std::latch start{1};std::atomic<unsigned> success{},refused{};
 const auto create=[&](std::string name){auto value=p;value.display_name=name;start.wait();
  try{manager.save_profile(value,true);++success;}catch(const std::exception& error){std::ofstream out{name+"-error.txt"};out<<error.what();++refused;}};
 std::jthread first{create,"first"},second{create,"second"};start.count_down();first.join();second.join();
 CHECK(success==1&&refused==1);
 const auto path=manager.manifest_path(p.id);const auto original=read(path);
 const auto decoded=nlohmann::json::parse(original);CHECK(decoded["display_name"]=="first"||decoded["display_name"]=="second");
 bool existingRefused{};try{manager.save_profile(p,true);}catch(const std::exception&){existingRefused=true;}
 CHECK(existingRefused&&read(path)==original);
 p.display_name="explicitly updated";manager.save_profile(p,false);
 CHECK(nlohmann::json::parse(read(path))["display_name"]=="explicitly updated");
 unsigned files{};for(const auto& entry:std::filesystem::directory_iterator(path.parent_path())){CHECK(entry.path()==path);++files;}CHECK(files==1);
 p.id="directory-conflict";std::filesystem::create_directories(manager.manifest_path(p.id));
 bool directoryRefused{};try{manager.save_profile(p,true);}catch(const std::exception&){directoryRefused=true;}CHECK(directoryRefused);
}catch(const std::exception& error){std::ofstream out{"contract-error.txt"};out<<error.what();return 99;}}
'''.replace('PROFILE', (native/'ai.backend_model_runtime_model.cpp').as_posix())
        for key, path in {'IDS':native/'ai.backend_core_ids.cpp',
                          'REPLACE_WINDOWS':native/'ai.backend_core_file_replace_windows.cpp',
                          'REPLACE_POSIX':native/'ai.backend_core_file_replace_posix.cpp'}.items():
            code=code.replace(key,path.as_posix())
        code=code.replace('ACTUAL_SAVE', save)
        report=ROOT/'Artifacts/Reports/model-behavior-oct29/no-replace'
        report.mkdir(parents=True,exist_ok=True)
        # Each configuration exercises a fresh absent destination, preserving all
        # prior case evidence rather than deleting a tested directory.
        code=code.replace('std::filesystem::current_path()/"profiles"',
                          'std::filesystem::current_path()/("profiles-"+epoch_ai::core::make_id("case"))')
        source=report/'no_replace.cpp';source.write_text(code)
        includes=[dependency_include(ROOT)]
        if os.name == 'nt':
            kits=sorted(Path('C:/Program Files (x86)/Windows Kits/10/Include').glob('*'))
            if not kits:
                self.skipTest('Installed Windows SDK required for actual file publication owner')
            includes += [kits[-1]/'um', kits[-1]/'shared']
        result=compile_and_run(self,source,report,includes=includes)
        (report/'result.json').write_text(json.dumps(result,indent=2)+'\n')

    def test_actual_profile_metadata_and_explicit_selection(self):
        report = ROOT / 'Artifacts/Reports/model-behavior-oct29/profile'
        result = compile_and_run(self, ROOT/'Tests/models/ai.model_behavior_contract.cpp', report,
                                 includes=[dependency_include(ROOT)])
        (report/'result.json').write_text(json.dumps(result, indent=2)+'\n')

    def test_actual_api_suggestion_excludes_labelled_variants(self):
        owner = (ROOT/'Engine/src/editor/application/editor.application_models_attachments.inl').read_text()
        functions = '\n'.join(definition(owner, marker) for marker in (
            '        [[nodiscard]] std::string ascii_lower_model_name(',
            '        [[nodiscard]] int ai_model_suggestion_score(',
            '        [[nodiscard]] std::string suggest_discovered_ai_model('))
        code = r'''
#include <algorithm>
#include <cassert>
#include <string>
#include <string_view>
#include <vector>
#include "HEADER"
namespace ai=epochengine::ai;
FUNCTIONS
int main(){
 assert(suggest_discovered_ai_model({"publisher/Qwen-coder-uncensored","ordinary-model"})=="ordinary-model");
 assert(suggest_discovered_ai_model({"Qwen-de-restricted","Qwen-ab_lit_erated"}).empty());
 assert(suggest_discovered_ai_model({"embedding","unsloth/Qwen3.8-Flash-Next-UD-Q2_K_XL"})=="unsloth/Qwen3.8-Flash-Next-UD-Q2_K_XL");
 assert(suggest_discovered_ai_model({}).empty());
}
'''.replace('HEADER', (ROOT/'Engine/src/ai/ai.model_behavior.hpp').as_posix()).replace('FUNCTIONS', functions)
        report = ROOT/'Artifacts/Reports/model-behavior-oct29/suggestion'
        report.mkdir(parents=True, exist_ok=True)
        source=report/'suggestion.cpp'; source.write_text(code)
        result=compile_and_run(self, source, report)
        (report/'result.json').write_text(json.dumps(result, indent=2)+'\n')


if __name__ == '__main__':
    unittest.main()
