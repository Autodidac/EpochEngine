"""Actual Editor prerequisite continuations with the production immutable installer.

The scheduler, save and generation boundaries are recorded CPU ports. No native
Editor, provider compiler, model, renderer or operator project is used.
"""
from pathlib import Path
from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


def consumer():
    owner = (ROOT / "Engine/src/editor/application/editor.application_packages_creation.inl").read_text(encoding="utf-8")
    application = (ROOT / "Engine/src/editor/editor.application.cpp").read_text(encoding="utf-8")
    state = (ROOT / "Engine/src/editor/application/editor.application_state.inl").read_text(encoding="utf-8")
    settings = (ROOT / "Engine/modules/project/project.planetary.ixx").read_text(encoding="utf-8")
    functions = "\n".join(definition(owner, marker) for marker in (
        "        [[nodiscard]] bool planetary_project_busy(",
        "        [[nodiscard]] std::uint64_t planetary_project_key(",
        "        void save_admitted_planetary_settings(",
        "        [[nodiscard]] bool planetary_feature_request_current(",
        "        void approve_planetary_water_source(",
        "        void request_planetary_feature_action(",
        "        void poll_planetary_feature_task(",
        "        void start_reviewed_water_attachment(",
        "        void cancel_planetary_project_task(",
        "        void cancel_package_source_task("))
    # Exercise the actual next-frame hidden-modal cleanup too. The feature
    # functions alone cannot expose a Project-started review cancelled before
    # its future is polled by editor_run.
    frame_cleanup = application.split("// Poll owned local source work even after a modal/context transition.", 1)[1].split(
        "        poll_package_source_task(editor);", 1)[0]
    return PREFIX.replace("SETTINGS", definition(settings, "    struct Settings final", True)).replace(
        "REQUEST", definition(state, "            struct PlanetaryFeatureRequest final", True)) + functions + (
            "\nvoid frame_package_cleanup(EditorState& editor){\n" + frame_cleanup + "\n}\n") + MAIN


def durable_consumer():
    fixture = MAIN[MAIN.index("void make_source("):MAIN.index("int main(){")]
    owner = (ROOT / "Engine/src/editor/application/editor.application_packages_creation.inl").read_text(encoding="utf-8")
    defaults = definition(owner, "                            if (!savedSettings && editor.planetaryAttachmentIsInstall)")
    default_port = """
namespace project_planetary=epochengine::project_planetary;
struct DefaultEditorPort {planet::Settings planetarySettings;bool planetaryAttachmentIsInstall{true};};
void apply_install_defaults(DefaultEditorPort& editor,const std::optional<planet::Settings>& savedSettings){
 editor.planetarySettings=savedSettings.value_or(planet::Settings{});
""" + defaults + "\n}\n"
    return DURABLE_PREFIX + fixture + default_port + DURABLE_MAIN


PREFIX = r'''
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
import package.source_install;
namespace package_source=epochengine::package_source;
namespace fs=std::filesystem;
using namespace std::chrono_literals;
inline fs::path testRepository,testData;
namespace core::path {
fs::path candidate_data_root(){return testData;}
fs::path executable_dir(){return testData;}
fs::path find_epoch_repo_root(const fs::path&){return testRepository;}
}
namespace project_planetary {
SETTINGS
inline unsigned saves{};
inline Settings saved;
inline bool saveOk=true,readbackOk=true;
bool save_settings(const fs::path&,const Settings& value){++saves;if(saveOk)saved=value;return saveOk;}
std::optional<Settings> load_settings(const fs::path&){return readbackOk?std::optional{saved}:std::nullopt;}
}
struct Ticket {std::shared_ptr<std::stop_source> source;bool valid()const{return bool(source);}};
struct Scheduler {bool cancel(const Ticket& ticket){if(ticket.source)ticket.source->request_stop();return true;}};
struct EditorState {
 REQUEST
 std::optional<PlanetaryFeatureRequest> planetaryFeatureRequest;
 std::optional<std::future<package_source::ProjectFeatureReview>> planetaryFeatureReviewPending;
 std::optional<std::future<package_source::ProjectFeatureAdmission>> planetaryFeatureAdmissionPending;
 std::optional<std::future<package_source::ProjectAttachment>> waterAttachmentPending,planetaryAttachmentPending;
 std::optional<int> planetaryGeneratePending,planetaryStreamPending;
 struct Lifecycle {struct Evidence {struct Generations {struct Selected {std::uint64_t generation{1},project_key{2};} selected_project;} generations;} value;
 const auto& evidence()const{return value;}} projectLifecycle;
 project_planetary::Settings planetarySettings;
 package_source::ProjectAttachment planetaryAttachment;
 std::string projectRoot,planetaryStatus,packageSourceStatus,packageSourceExpectedId,planetaryOperationRoot;
 std::uint64_t planetaryOperationProjectKey{},planetaryOperationProjectGeneration{};
 bool projectBuildPending{},scriptBuildPending{},planetaryStreamContinue{},planetaryStreamFailed{};
 bool packageSourceReviewCurrent{},packageSourceView{},showPackageManagerModal{};
 std::optional<package_source::Inspection> packageSourceReview;
 package_source::StageResult packageSourceStaged;
 std::vector<std::string> packageSourceFileLines;
 struct AiPackageRequest {enum class Phase{review,attaching,generating};Phase phase{Phase::review};bool cancelled{};};
 std::optional<AiPackageRequest> aiPackageRequest;
 std::string aiToolStatus;
 struct SourceTask{std::stop_source cancellation;};std::unique_ptr<SourceTask> packageSourceTask;
 Scheduler scheduler;Scheduler* taskScheduler{&scheduler};Ticket planetaryCancellation;
 unsigned generations{},refreshes{};
};
inline bool packageBusy{};
bool package_source_task_busy(const EditorState&){return packageBusy;}
void finish_ai_project_package_request(EditorState& editor,const std::string& message){editor.aiToolStatus=message;editor.aiPackageRequest.reset();}
void refresh_editor_planetary_mesh_policy(EditorState& editor){++editor.refreshes;}
void start_admitted_planetary_generation(EditorState& editor){++editor.generations;}
fs::path resolve_editor_path(const fs::path& path){return path;}
template<class F>auto schedule_editor_cancellable_task(EditorState&,std::string_view,F callback){
 struct Submission {std::future<std::invoke_result_t<F,std::stop_token>> completion;Ticket cancellation;};
 auto stop=std::make_shared<std::stop_source>();
 // Deterministic worker boundary: tests decide when the real job executes and
 // can revoke its real stop token before the immutable installer runs.
 return Submission{std::async(std::launch::deferred,[callback=std::move(callback),stop]()mutable{return callback(stop->get_token());}),{stop}};
}
'''

MAIN = r'''
void write(const fs::path& path,std::string_view value){fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out.write(value.data(),value.size());if(!out)std::exit(98);}
std::string read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>{in},{}};}
#define CHECK(x) do{if(!(x)){write(testRepository/"editor-water-failure.txt",std::to_string(__LINE__));return __LINE__;}}while(false)
void complete(EditorState& editor){
 if(editor.planetaryFeatureReviewPending)editor.planetaryFeatureReviewPending->wait();
 poll_planetary_feature_task(editor);
 if(editor.planetaryFeatureAdmissionPending)editor.planetaryFeatureAdmissionPending->wait();
 poll_planetary_feature_task(editor);
}
void make_source(const fs::path& repository,const std::string& id){
 const auto folder=repository/"packages"/id;
 write(repository/"LICENSE","Inert source-admission fixture; not algorithm or legal evidence.\n");
 write(repository/"packages/_api/epoch_package_script_api.hpp","// inert\n");
 write(folder/"src/unit.cpp","// never execute\n");write(folder/"scripts/unit.ascript.cpp","// never execute\n");
 write(folder/"package.epoch-package.json",R"({"id":")"+id+R"(","displayName":"Source fixture","summary":"CPU admission fixture","manifestLicense":"LicenseRef-MIT-NoSell","kind":"code-extension","activation":"package-manager-opt-in","security":{"serverOrListenerAllowed":false,"requiresExplicitNetworkApproval":false,"autoRunAllowed":false,"includeByDefault":false},"source":{"repo":"https://github.com/Autodidac/EpochEngineExtensions","path":"packages/)"+id+R"("},"script":{"path":"scripts/unit.ascript.cpp","language":"C++23","api":"epoch_package_script_api","humanGated":true,"autoRunAllowed":false},"build":{"humanGated":true,"sourceFiles":["src/unit.cpp"]}})");
}
int main(){
 testRepository=fs::current_path()/("water-editor-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));testData=testRepository/"data";
 const auto extensions=testRepository/"Engine/dep/EpochEngineExtensions";
 make_source(extensions,"planetary_terrain");
 const auto planet=package_source::inspect_available_source(testData/"cache/packages",extensions,"research_planetary_terrain");CHECK(planet.review_ready());
 EditorState editor;editor.projectRoot=(testRepository/"project").string();
 write(fs::path(editor.projectRoot)/"project.epoch.json","{}\n");
 write(fs::path(editor.projectRoot)/"worlds/main.epoch","retained scene\n");
 editor.planetaryAttachment=package_source::attach_project_source(planet.inspection,planet.staged,planet.inspection.content_hash,editor.projectRoot);CHECK(editor.planetaryAttachment.source_attached());
 const auto parentBytes=read(fs::path(editor.projectRoot)/editor.planetaryAttachment.relative_source_root/"src/unit.cpp");
 auto saves=project_planetary::saves;
 // Explicit OceanOff does not inspect optional source or show a modal.
 request_planetary_feature_action(editor,false);CHECK(project_planetary::saves==saves+1&&!editor.planetaryFeatureReviewPending&&!editor.showPackageManagerModal);
 CHECK(editor.planetaryStatus.find("settings saved")!=std::string::npos);
 const auto unchanged=editor.planetarySettings;
 packageBusy=true;request_planetary_feature_action(editor,false);packageBusy=false;
 CHECK(project_planetary::saves==saves+1&&editor.planetarySettings==unchanged&&editor.planetaryStatus.find("not been saved")!=std::string::npos);
 project_planetary::readbackOk=false;save_admitted_planetary_settings(editor);
 CHECK(editor.planetaryStatus.find("Could not save")!=std::string::npos);project_planetary::readbackOk=true;
 project_planetary::saveOk=false;save_admitted_planetary_settings(editor);
 CHECK(editor.planetaryStatus.find("Could not save")!=std::string::npos);project_planetary::saveOk=true;
 saves+=2;
 editor.planetarySettings.ocean_surface=true;
 request_planetary_feature_action(editor,true);CHECK(editor.planetaryFeatureReviewPending&&editor.generations==0);complete(editor);
 CHECK(!editor.planetaryFeatureRequest&&editor.generations==0&&editor.planetaryStatus.find("Water is required")!=std::string::npos);
 make_source(extensions,"fft_ocean");
 // Fresh Planet installation hands its default OceanOn settings to Project,
 // closes the source modal, then asynchronously reviews Water. The next
 // ordinary Editor frame must preserve that owned request and stop token.
 request_planetary_feature_action(editor,false);CHECK(editor.planetaryFeatureReviewPending);
 editor.showPackageManagerModal=false;
 frame_package_cleanup(editor);
 CHECK(editor.planetaryFeatureRequest&&!editor.planetaryCancellation.source->stop_requested());
 complete(editor);
 CHECK(editor.planetaryFeatureRequest&&editor.showPackageManagerModal&&editor.packageSourceReviewCurrent);
 cancel_package_source_task(editor);
 // Closing a displayed, idle approval still revokes it; hiding a window is
 // not implicit approval and does not leave a reusable source decision.
 request_planetary_feature_action(editor,false);complete(editor);
 editor.showPackageManagerModal=false;frame_package_cleanup(editor);
 CHECK(!editor.planetaryFeatureRequest&&!editor.packageSourceReviewCurrent);
 // A project-generation change discards a late review before project mutation.
 request_planetary_feature_action(editor,true);++editor.projectLifecycle.value.generations.selected_project.generation;complete(editor);
 CHECK(!editor.planetaryFeatureRequest&&!fs::exists(fs::path(editor.projectRoot)/"Packages/fft_ocean")&&editor.generations==0);
 request_planetary_feature_action(editor,false);complete(editor);
 CHECK(editor.planetaryFeatureRequest&&editor.showPackageManagerModal&&editor.packageSourceReviewCurrent);
 CHECK(project_planetary::saves==saves+1&&!fs::exists(fs::path(editor.projectRoot)/"Packages/fft_ocean"));
 // Declining never mutates project source or executes the continuation.
 cancel_package_source_task(editor);CHECK(!editor.planetaryFeatureRequest&&project_planetary::saves==saves+1);
 editor.aiPackageRequest=EditorState::AiPackageRequest{.phase=EditorState::AiPackageRequest::Phase::generating};
 request_planetary_feature_action(editor,true);complete(editor);cancel_package_source_task(editor);
 CHECK(!editor.aiPackageRequest&&editor.generations==0);
 request_planetary_feature_action(editor,true);complete(editor);CHECK(editor.planetaryFeatureRequest);
 start_reviewed_water_attachment(editor);CHECK(editor.planetaryFeatureAdmissionPending);cancel_package_source_task(editor);complete(editor);
 CHECK(editor.generations==0&&!fs::exists(fs::path(editor.projectRoot)/"Packages/fft_ocean"));
 // Changed settings invalidate the exact continuation before approval.
 request_planetary_feature_action(editor,true);complete(editor);++editor.planetarySettings.seed;
 start_reviewed_water_attachment(editor);CHECK(!editor.planetaryFeatureAdmissionPending&&editor.generations==0);
 cancel_package_source_task(editor);
 // A displayed source hash different from the feature review cannot approve.
 request_planetary_feature_action(editor,true);complete(editor);
 const auto reviewedHash=editor.packageSourceReview->content_hash;editor.packageSourceReview->content_hash="sha256:changed";
 start_reviewed_water_attachment(editor);CHECK(!editor.planetaryFeatureAdmissionPending);
 editor.packageSourceReview->content_hash=reviewedHash;
 // Exact approval attaches the real source once and resumes the requested save.
 cancel_package_source_task(editor);request_planetary_feature_action(editor,false);complete(editor);
 start_reviewed_water_attachment(editor);
 editor.showPackageManagerModal=false;frame_package_cleanup(editor);
 CHECK(editor.planetaryFeatureAdmissionPending&&editor.planetaryFeatureRequest
   &&!editor.planetaryCancellation.source->stop_requested());
 complete(editor);
 CHECK(project_planetary::saves==saves+2&&!editor.planetaryFeatureRequest&&!editor.showPackageManagerModal);
 const auto installed=package_source::load_project_attachment(editor.projectRoot,"fft_ocean");CHECK(installed.source_attached());
 CHECK(read(fs::path(editor.projectRoot)/"worlds/main.epoch")=="retained scene\n");
 CHECK(read(fs::path(editor.projectRoot)/editor.planetaryAttachment.relative_source_root/"src/unit.cpp")==parentBytes);
 // Reopen/existing-source path requires no external repository or new approval.
 testRepository/= "source-is-unavailable";testData/="fresh-cache";
 request_planetary_feature_action(editor,true);complete(editor);CHECK(editor.generations==1&&!editor.planetaryFeatureRequest);
 // An edited existing Water provider is preserved and refuses activation.
 write(fs::path(editor.projectRoot)/installed.relative_source_root/"src/unit.cpp","// user edited\n");
 request_planetary_feature_action(editor,true);complete(editor);CHECK(editor.generations==1&&!editor.planetaryFeatureRequest);
 CHECK(read(fs::path(editor.projectRoot)/installed.relative_source_root/"src/unit.cpp")=="// user edited\n");
 return 0;
}
'''

DURABLE_PREFIX = r'''
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
import package.source_install;
import project.planetary;
import platform.child_process;
namespace fs=std::filesystem;
namespace source=epochengine::package_source;
namespace planet=epochengine::project_planetary;
void write(const fs::path& path,std::string_view value){fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out.write(value.data(),value.size());}
std::string read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>{in},{}};}
#define CHECK(x) do{if(!(x)){write(base/"durable-water-failure.txt",std::to_string(__LINE__));return __LINE__;}}while(false)
'''

DURABLE_MAIN = r'''
int main(){
 const auto base=fs::current_path()/("durable-water-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 const auto project=base/"project",extensions=base/"extensions",cache=base/"cache";
 DefaultEditorPort newPlanet;apply_install_defaults(newPlanet,{});
 CHECK(newPlanet.planetarySettings.ocean_surface&&planet::valid_settings(newPlanet.planetarySettings));
 const auto oldDry=planet::Settings{};apply_install_defaults(newPlanet,oldDry);
 CHECK(newPlanet.planetarySettings==oldDry&&!newPlanet.planetarySettings.ocean_surface);
 write(project/"project.epoch.json","{}\n");write(project/"worlds/main.epoch","retained scene\n");
 make_source(extensions,"planetary_terrain");
 const auto parent=source::inspect_available_source(cache,extensions,"research_planetary_terrain");CHECK(parent.review_ready());
 const auto attached=source::attach_project_source(parent.inspection,parent.staged,parent.inspection.content_hash,project);CHECK(attached.source_attached());
 planet::Settings dry;dry.mode="classic";
 CHECK(planet::save_settings(project,dry)&&planet::runtime_requirement(project).ok&&!fs::exists(project/"Packages/fft_ocean"));
 const auto dryBytes=read(project/planet::recipe_relative_path);
 auto wet=planet::Settings{};wet.ocean_surface=true;
 CHECK(!planet::save_settings(project,wet)&&read(project/planet::recipe_relative_path)==dryBytes);
 // Retained OceanOn projects still load recipe/source evidence. Future writes,
 // generation and Run must stop before a compiler/provider is started.
 const auto legacyBytes=planet::encode_settings(wet);write(project/planet::recipe_relative_path,legacyBytes);
 CHECK(planet::load_settings(project)==wet);
 const auto identity=planet::capture_source_generation(project,attached.content_hash);
 CHECK(identity&&planet::source_generation_is_current(project,*identity));
 const auto requirement=planet::runtime_requirement(project);
 CHECK(!requirement.ok&&requirement.message.find("Water (FFT Ocean)")!=std::string::npos);
 const auto generated=planet::generate(project,project/attached.relative_source_root,base/"unusable-sdk",wet);
 CHECK(!generated.ok&&generated.message.find("Water (FFT Ocean)")!=std::string::npos&&!fs::exists(project/"build"));
 CHECK(planet::load_settings(project)==wet&&read(project/planet::recipe_relative_path)==legacyBytes);
 CHECK(!epochengine::platform::child_process::active_in_group("epoch.project.planet"));
 make_source(extensions,"fft_ocean");
 const auto review=source::review_project_feature(project,"planetary_terrain",source::SourceFeature::planetary_ocean,true,cache,extensions);
 CHECK(review.needs_approval());
 CHECK(source::admit_project_feature(review,review.dependency.inspection.content_hash).sources_admitted());
 CHECK(planet::save_settings(project,wet)&&planet::load_settings(project)==wet);
 CHECK(read(project/"worlds/main.epoch")=="retained scene\n");
 CHECK(planet::save_settings(project,dry)&&planet::runtime_requirement(project).ok);
 return 0;
}
'''
