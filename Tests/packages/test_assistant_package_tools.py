"""Typed project package tools + real source admission and attachment owner.
No native Editor, model, download, provider build or renderer is launched.
"""
from pathlib import Path
import json
import os
import subprocess
import time
import unittest
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]
HOST=r'''

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
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
import ai.mcp;
import package.registry;
import package.source_install;
import project.planetary;
#include "GOAL_HEADER"
namespace ai=epochengine::ai;namespace package_registry=epochengine::package_registry;
namespace package_source=epochengine::package_source;namespace project_planetary=epochengine::project_planetary;
namespace fs=std::filesystem;namespace core {struct Context{};}
enum class EditorMainSurface {Project,Scene};
enum class EditorProjectKind {Game,Tool};
enum class EditorProjectInputProvision {project_default};
struct CreationResult {bool succeeded{true};std::string project_id{"Created01"},root_path,manifest_path,summary;};
CreationResult created;unsigned createCalls{};EditorProjectKind createdKind{};
CreationResult editor_create_project_shell(EditorProjectKind kind,EditorProjectInputProvision){++createCalls;createdKind=kind;return created;}
CreationResult editor_admit_project_manifest(const std::string&){return created;}
bool opened=true;
bool editor_open_project(const std::shared_ptr<core::Context>&,const std::string&){return opened;}
std::optional<project_planetary::Settings> persisted;
bool record_save_settings(const fs::path&,const project_planetary::Settings& settings){persisted=settings;return true;}
std::optional<project_planetary::Settings> record_load_settings(const fs::path&){return persisted;}
struct Ticket {std::shared_ptr<std::stop_source> source;bool valid()const{return bool(source);}};
struct Scheduler {bool cancel(const Ticket& ticket){if(ticket.source)ticket.source->request_stop();return true;}};
struct Lifecycle {struct Evidence {struct Generations {struct Selected{std::uint64_t generation{7};}selected_project;}generations;bool runtime_active{};}value;
 const Evidence& evidence()const{return value;}};
struct SourceTask {std::stop_source cancellation;};
struct EditorState {
 FIELD
 std::optional<AiPackageRequest> aiPackageRequest;
 epochengine::editor::ai_chat_goal::ToolSequence aiGoalTools{};
 std::optional<bool> aiGoalPackageReceipt{};
 std::string projectId{"GameProject"},projectRoot,projectStatus,aiToolStatus,packageInstallStatus;
 Lifecycle projectLifecycle;bool projectBuildPending{},scriptBuildPending{},projectCreationOpen{},sourceBusy{};
 std::optional<EditorMainSurface> requestedMainSurface;
 std::unique_ptr<SourceTask> packageSourceTask=std::make_unique<SourceTask>();Scheduler scheduler;Scheduler* taskScheduler{&scheduler};
 std::string planetaryCheckedRoot,planetaryOperationRoot,planetaryStatus,planetarySeedDraft,selectedPackageId,packageSourceStatus;
 std::uint64_t planetaryOperationProjectKey{},planetaryOperationProjectGeneration{};
 bool planetaryAttachmentIsInstall{},planetaryAttachmentIsUpdate{},showPackageManagerModal{},packageCatalogModelsOnly{};
 package_source::ProjectAttachment planetaryAttachment;
 std::optional<package_source::Inspection> packageSourceReview;
 bool packageSourceReviewCurrent{};
 package_source::StageResult packageSourceStaged;
 project_planetary::Settings planetarySettings;
 std::optional<std::future<package_source::ProjectAttachment>> planetaryAttachmentPending;
 std::optional<int> planetaryGeneratePending,planetaryStreamPending;
 std::optional<int> planetaryFeatureReviewPending,planetaryFeatureAdmissionPending,waterAttachmentPending,planetaryFeatureRequest;
 Ticket planetaryCancellation;double packageInstallProgress{};
 fs::path cache;unsigned generationCalls{},sourceReviewCalls{},builtinCalls{};
};
std::uint64_t planetary_project_key(const EditorState& editor)noexcept{return editor.projectId=="GameProject"?11u:22u;}
bool package_source_task_busy(const EditorState& editor){return editor.sourceBusy;}
void push_editor_log(EditorState&,const std::string&){}
fs::path resolve_editor_path(const fs::path& path){return path;}
bool builtin_project_package_ready(std::string_view id){return id==package_registry::kEngineForestFactoryPackageId||id==package_registry::kEngineArcadePackageId;}
void add_builtin_project_package(EditorState& editor,const std::shared_ptr<core::Context>&,const package_registry::PackageDescriptor&){
 ++editor.builtinCalls;editor.packageInstallProgress=1;editor.projectStatus="Builtin added by recorded existing owner";}
void open_package_source_review(EditorState& editor,std::string id){
 ++editor.sourceReviewCalls;const auto cached=package_source::inspect_cached_source(editor.cache,id);
 editor.packageSourceReview=cached.inspection;editor.packageSourceStaged=cached.staged;editor.packageSourceReviewCurrent=cached.review_ready();}
void start_planetary_project_generation(EditorState& editor){
 // Existing production generator is a recorded queue boundary here. This
 // contract proves attachment/approval/fencing, not a CMake/provider build.
 ++editor.generationCalls;editor.planetaryGeneratePending=1;
}
template<class F>auto schedule_editor_cancellable_task(EditorState&,std::string_view,F callback){
 struct Submission {std::future<std::invoke_result_t<F,std::stop_token>> completion;Ticket cancellation;};
 auto source=std::make_shared<std::stop_source>();
 return Submission{std::async(std::launch::deferred,[callback=std::move(callback),source]()mutable{return callback(source->get_token());}),{source}};
}
BUSY
HELPERS
ATTACH
CANCEL
#define CHECK(value) do{if(!(value)){std::fprintf(stderr,"Assistant package contract line %d\n",__LINE__);return __LINE__;}}while(false)
void write(const fs::path& file,std::string_view value){fs::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary);out<<value;}
ai::McpToolCall call(std::string_view text){return ai::parse_tool_plan("EPOCH_TOOL_PLAN_V1\nTITLE Project package\nSUMMARY One host operation.\nCALL "+std::string{text}+"\nEND\n").plan.call;}
void complete_attachment(EditorState& editor){auto value=editor.planetaryAttachmentPending->get();editor.planetaryAttachmentPending.reset();editor.planetaryAttachment=std::move(value);
 std::ofstream out(fs::path{editor.projectRoot}/"attachment-observation.txt");out<<editor.planetaryAttachment.message<<"\n"<<editor.planetaryAttachment.content_hash<<"\n"<<editor.aiPackageRequest->source_hash<<"\n"<<editor.aiPackageRequest->generate<<"\n"<<ai_project_package_request_current(editor)<<"\n";
 continue_ai_project_package_after_attachment(editor);out<<editor.aiToolStatus<<"\n"<<editor.generationCalls;}

int main(int argc,char**argv){
 CHECK(argc==2||argc==3);const fs::path base=fs::weakly_canonical(argv[1]);
 const auto repository=base/"fixture";const auto manifest=repository/"packages/planetary_terrain/package.epoch-package.json";
 write(repository/"LICENSE","Source-only test license evidence; not legal verification.");
 write(repository/"packages/_api/epoch_package_script_api.hpp","// Test API, never compiled.");
 write(manifest.parent_path()/"CMakeLists.txt","# Never configured in this admission contract.");
 write(manifest.parent_path()/"src/unit.cpp","// Test source, never executed.");
 write(manifest.parent_path()/"scripts/unit.ascript.cpp","// Never run.");
 write(manifest,R"json({"id":"planetary_terrain","displayName":"Planet source","summary":"Source admission contract","manifestLicense":"LicenseRef-MIT-NoSell","kind":"code-extension","activation":"package-manager-opt-in","security":{"serverOrListenerAllowed":false,"requiresExplicitNetworkApproval":false,"autoRunAllowed":false,"includeByDefault":false},"source":{"repo":"https://github.com/Autodidac/EpochEngineExtensions","path":"packages/planetary_terrain"},"script":{"path":"scripts/unit.ascript.cpp","language":"C++23","api":"epoch_package_script_api","humanGated":true,"autoRunAllowed":false},"build":{"humanGated":true,"sourceFiles":["CMakeLists.txt","src/unit.cpp"]}})json");
 const auto reviewed=package_source::inspect_local_source(manifest);CHECK(reviewed.review_ready());
 const auto cache=base/"cache";CHECK(package_source::stage_local_source(reviewed,"unapproved",cache).code==package_source::Code::approval_required);
 CHECK(package_source::stage_local_source(reviewed,reviewed.content_hash,cache).source_staged());
 const auto context=std::make_shared<core::Context>();
 // The completion owner reports a typed receipt, never success inferred
 // from status text. Use its real ToolSequence rather than a goal-state mock.
 for(const bool success:{false,true}){
  EditorState goal;goal.projectRoot="goal-root";
  auto& sequence=goal.aiGoalTools;sequence.enabled=true;sequence.waiting=true;
  sequence.project_id=goal.projectId;sequence.project_root=goal.projectRoot;
  sequence.project_generation=7;
  sequence.bind_request("planet and forest",goal.projectId,goal.projectRoot,7);
  goal.aiPackageRequest=EditorState::AiPackageRequest{};
  finish_ai_project_package_request(goal,"completed text is not evidence",success);
  CHECK(goal.aiGoalPackageReceipt==success&&!goal.aiPackageRequest);
  CHECK(sequence.receipt("project.create",*goal.aiGoalPackageReceipt,
   goal.projectId,goal.projectRoot,7)==success);
  CHECK(sequence.stage==(success?1u:0u)&&sequence.stopped==!success);
  if(!success){
   CHECK(sequence.resume("planet and forest",goal.projectId,goal.projectRoot,7));
   CHECK(sequence.stage==0&&!sequence.stopped);
   CHECK(!sequence.resume("changed objective",goal.projectId,goal.projectRoot,7));
  }
 }
 {EditorState goal;goal.aiGoalTools.enabled=true;goal.aiGoalTools.waiting=true;
  finish_ai_project_package_request(goal,"completed");
  CHECK(goal.aiGoalPackageReceipt.has_value()&&!*goal.aiGoalPackageReceipt);}
 for(const unsigned mismatch:{0u,1u,2u}){
  EditorState goal;goal.projectRoot="goal-root";
  auto& sequence=goal.aiGoalTools;sequence.enabled=true;sequence.waiting=true;
  sequence.project_id=goal.projectId;sequence.project_root=goal.projectRoot;sequence.project_generation=7;
  finish_ai_project_package_request(goal,"success delivered to a changed project",true);
  CHECK(goal.aiGoalPackageReceipt==true);
  CHECK(!sequence.receipt("project.create",*goal.aiGoalPackageReceipt,
   mismatch==0u?"other":goal.projectId,mismatch==1u?"other-root":goal.projectRoot,mismatch==2u?8u:7u));
  CHECK(sequence.stage==0&&sequence.stopped);
 }
 {EditorState goal;goal.aiGoalTools.enabled=true;goal.aiGoalTools.waiting=true;goal.aiGoalTools.stage=1;
  finish_ai_project_package_request(goal,"unrelated package completion",true);
  CHECK(!goal.aiGoalPackageReceipt);}
 EditorState fresh;fresh.projectRoot=(base/"new-planet").generic_string();fresh.cache=cache;
 created.root_path=fresh.projectRoot;created.manifest_path=(fs::path{fresh.projectRoot}/"project.epoch.json").generic_string();
 CHECK(create_ai_project_from_tool(fresh,call("project.create kind=game coverage=planet seed=42 radius_m=1737400 detail=fast ocean=on"),context));
 CHECK(createCalls==1&&createdKind==EditorProjectKind::Game&&persisted);
 CHECK(persisted->seed==42&&persisted->reference_radius_meters==1737400&&persisted->maximum_level==17&&persisted->ocean_surface);
 CHECK(fresh.aiPackageRequest&&fresh.aiPackageRequest->settings==*persisted&&fresh.generationCalls==0);
 CHECK(fresh.showPackageManagerModal&&fresh.aiToolStatus.find("Created01")!=std::string::npos);
 CHECK(fresh.requestedMainSurface==EditorMainSurface::Project);
 EditorState empty;CHECK(create_ai_project_from_tool(empty,call("project.create kind=tool coverage=none"),context));
 CHECK(createCalls==2&&createdKind==EditorProjectKind::Tool&&!empty.aiPackageRequest&&empty.generationCalls==0);
 CHECK(empty.aiToolStatus.find("nothing was built or run")!=std::string::npos);
 opened=false;CHECK(!create_ai_project_from_tool(empty,call("project.create kind=game coverage=none"),context));opened=true;
 CHECK(createCalls==3&&empty.aiToolStatus.find("could not open")!=std::string::npos);
 CHECK(!create_ai_project_from_tool(empty,call("project.create kind=game coverage=planet seed=3 radius_m=nan detail=fast ocean=on"),context));
 CHECK(createCalls==3);
 CHECK(!create_ai_project_from_tool(empty,call("project.inspect"),context)&&createCalls==3);
 if(argc==3){CHECK(std::string_view{argv[2]}=="--creation-only");return 0;}
 for(const std::string mode:{"classic","tiled","hemisphere","planet","universe"}){
  EditorState editor;editor.projectRoot=(base/("project-"+mode)).generic_string();editor.cache=cache;write(fs::path{editor.projectRoot}/"project.epoch.json","{}");
  CHECK(dispatch_ai_project_package_tool(editor,call("terrain.generate coverage="+mode+" seed=92"),context));
  CHECK(editor.aiPackageRequest&&editor.aiPackageRequest->phase==EditorState::AiPackageRequest::Phase::review&&editor.generationCalls==0);
  CHECK(editor.packageSourceReview->content_hash==reviewed.content_hash&&editor.showPackageManagerModal);
  CHECK(!fs::exists(fs::path{editor.projectRoot}/"Packages"));
  CHECK(ai_package_generation_awaiting_attachment(editor));
  // Explicit human Add & Generate action uses the actual existing attachment owner.
  start_planetary_project_attachment(editor);CHECK(editor.planetaryAttachmentPending&&editor.generationCalls==0);
  CHECK(editor.aiPackageRequest->source_hash==reviewed.content_hash);
  complete_attachment(editor);
  CHECK(editor.planetaryAttachment.source_attached()&&editor.generationCalls==1);
  CHECK(editor.planetarySettings.mode==mode&&editor.planetarySettings.seed==92&&ai_project_package_generation_current(editor));
  CHECK(package_source::load_project_attachment(editor.projectRoot,"research_planetary_terrain").source_attached());
  ++editor.projectLifecycle.value.generations.selected_project.generation;CHECK(!ai_project_package_generation_current(editor));
  --editor.projectLifecycle.value.generations.selected_project.generation;editor.aiPackageRequest->cancelled=true;CHECK(!ai_project_package_generation_current(editor));
  editor.aiPackageRequest->cancelled=false;editor.planetarySettings.seed++;CHECK(!ai_project_package_generation_current(editor));
  editor.planetarySettings.seed--;editor.planetaryAttachment.content_hash+="changed";CHECK(!ai_project_package_generation_current(editor));
 }
 EditorState missing;missing.projectRoot=(base/"missing").generic_string();missing.cache=base/"missing-cache";write(fs::path{missing.projectRoot}/"project.epoch.json","{}");
 CHECK(dispatch_ai_project_package_tool(missing,call("terrain.generate coverage=planet seed=1"),context));
 CHECK(!missing.packageSourceStaged.source_staged());start_planetary_project_attachment(missing);CHECK(!missing.planetaryAttachmentPending&&missing.generationCalls==0);
 cancel_package_source_task(missing);CHECK(!missing.aiPackageRequest&&missing.generationCalls==0);
 EditorState changed;changed.projectRoot=(base/"changed").generic_string();changed.cache=cache;write(fs::path{changed.projectRoot}/"project.epoch.json","{}");
 CHECK(dispatch_ai_project_package_tool(changed,call("terrain.generate coverage=hemisphere seed=1"),context));
 start_planetary_project_attachment(changed);++changed.projectLifecycle.value.generations.selected_project.generation;
 complete_attachment(changed);CHECK(changed.generationCalls==0&&!changed.aiPackageRequest);
 EditorState cancelled;cancelled.projectRoot=(base/"cancelled").generic_string();cancelled.cache=cache;write(fs::path{cancelled.projectRoot}/"project.epoch.json","{}");
 CHECK(dispatch_ai_project_package_tool(cancelled,call("terrain.generate coverage=planet seed=1"),context));
 start_planetary_project_attachment(cancelled);cancel_package_source_task(cancelled);complete_attachment(cancelled);
 CHECK(!cancelled.planetaryAttachment.source_attached()&&cancelled.generationCalls==0&&!cancelled.aiPackageRequest);
 EditorState install;install.projectRoot=(base/"install").generic_string();install.cache=cache;write(fs::path{install.projectRoot}/"project.epoch.json","{}");
 CHECK(dispatch_ai_project_package_tool(install,call("package.install package=research_planetary_terrain"),context));
 start_planetary_project_attachment(install);complete_attachment(install);CHECK(install.planetaryAttachment.source_attached()&&install.generationCalls==0&&!install.aiPackageRequest);
 CHECK(dispatch_ai_project_package_tool(install,call("terrain.generate coverage=planet seed=42"),context));
 CHECK(install.generationCalls==1&&install.sourceReviewCalls==1);
 EditorState denied;denied.projectRoot=install.projectRoot;denied.cache=cache;
 for(const auto& package:package_registry::known_packages())if(package_registry::is_network_sensitive(package)){
  CHECK(!dispatch_ai_project_package_tool(denied,call("package.install package="+std::string{package.id}),context));}
 CHECK(!dispatch_ai_project_package_tool(denied,call("package.install package=research_fft_ocean"),context));
 CHECK(!dispatch_ai_project_package_tool(denied,call("package.install package=unknown_package"),context));
 CHECK(denied.sourceReviewCalls==0&&denied.generationCalls==0);
 CHECK(dispatch_ai_project_package_tool(denied,call("package.catalog"),context));CHECK(denied.aiToolStatus.find("REGISTERED_")==std::string::npos);
 CHECK(denied.aiToolStatus.find(install.projectRoot)==std::string::npos&&denied.aiToolStatus.find("Reference only")!=std::string::npos);
 const auto evidence=ai_project_package_catalog(install,true);CHECK(evidence.find("research_planetary_terrain")!=std::string::npos&&evidence.find(install.projectRoot)==std::string::npos);
 denied.projectRoot.clear();CHECK(!dispatch_ai_project_package_tool(denied,call("package.install package=research_planetary_terrain"),context)&&denied.projectCreationOpen);
 // Explicit native review update uses the same existing owner, without AI
 // generation or the ordinary Add action silently upgrading installed source.
 write(manifest.parent_path()/"CMakeLists.txt","# explicitly reviewed updated source\n");
 const auto replacement=package_source::inspect_local_source(manifest);CHECK(replacement.review_ready()&&replacement.content_hash!=reviewed.content_hash);
 const auto replacementStage=package_source::stage_local_source(replacement,replacement.content_hash,base/"update-cache");
 if(!replacementStage.source_staged())std::fprintf(stderr,"Replacement stage: %s\n",replacementStage.message.c_str());
 CHECK(replacementStage.source_staged());
 EditorState updating;updating.projectRoot=install.projectRoot;updating.planetaryAttachment=install.planetaryAttachment;
 updating.packageSourceReview=replacement;updating.packageSourceStaged=replacementStage;updating.packageSourceReviewCurrent=true;
 write(fs::path{updating.projectRoot}/"Assets/Worlds/Planet.epoch_planet","retained recipe bytes\n");
 start_planetary_project_attachment(updating);CHECK(updating.planetaryAttachmentPending);
 CHECK(!updating.planetaryAttachmentPending->get().source_attached());updating.planetaryAttachmentPending.reset();
 CHECK(package_source::load_project_attachment(updating.projectRoot,"planetary_terrain").content_hash==reviewed.content_hash);
 updating.projectBuildPending=true;start_planetary_project_attachment(updating,true);CHECK(!updating.planetaryAttachmentPending);
 updating.projectBuildPending=false;start_planetary_project_attachment(updating,true);
 CHECK(updating.planetaryAttachmentPending&&updating.planetaryAttachmentIsUpdate&&!updating.planetaryAttachmentIsInstall);
 const auto updated=updating.planetaryAttachmentPending->get();CHECK(updated.source_attached()&&!updated.retained_previous_source.empty());
 CHECK(package_source::load_project_attachment(updating.projectRoot,"planetary_terrain").content_hash==replacement.content_hash&&updating.generationCalls==0);
 {std::ifstream input(fs::path{updating.projectRoot}/"Assets/Worlds/Planet.epoch_planet");std::string bytes{std::istreambuf_iterator<char>{input},{}};CHECK(bytes=="retained recipe bytes\n");}
 return 0;
}

'''
PROTOCOL=r'''
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
import ai.mcp;
namespace ai=epochengine::ai;
#define CHECK(value) do{if(!(value))return __LINE__;}while(false)
ai::ToolPlanParseResult plan(std::string_view call){return ai::parse_tool_plan(
    "EPOCH_TOOL_PLAN_V1\nTITLE Package task\nSUMMARY One approved host operation.\nCALL "+std::string{call}+"\nEND\n");}
int main(){
 CHECK(ai::run_mcp_contract());
 auto catalog=plan("package.catalog");CHECK(catalog&&catalog.plan.call.arguments.empty());
 auto install=plan("package.install package=research_planetary_terrain");CHECK(install&&install.plan.call.arguments.size()==1);
 for(const std::string_view mode:{"classic","tiled","hemisphere","planet","universe"}){
  auto terrain=plan("terrain.generate coverage="+std::string{mode}+" seed=4294967295");
  CHECK(terrain&&terrain.plan.call.arguments.size()==2&&ai::validate_project_tool_arguments(terrain.plan.call));
 }
 for(const std::string_view bad:{"package.install", "package.install package=../private", "package.install package=https://x",
  "package.install package=research_planetary_terrain path=foreign", "package.install package=one package=two",
  "package.catalog url=remote", "terrain.generate coverage=planet", "terrain.generate coverage=planet seed=-1",
  "terrain.generate coverage=planet seed=4294967296", "terrain.generate coverage=planet seed=1 path=foreign",
  "terrain.generate coverage=planet seed=nan", "terrain.generate coverage=planet seed=1.5",
  "terrain.generate coverage=foreign seed=1", "terrain.generate coverage=planet coverage=hemisphere",
  "project.run executable=foreign", "project.inspect root=private"})CHECK(!plan(bad));
 const auto registry=ai::make_epoch_project_tool_registry();
 ai::McpSessionAuthority authority{.session_id="editor-tool-preview",.granted_capabilities=ai::McpToolCapability::inspect|ai::McpToolCapability::author|ai::McpToolCapability::build};
 CHECK(registry.validate(catalog.plan.call,authority));
 CHECK(registry.validate(install.plan.call,authority).error==ai::McpErrorCode::approval_required);
 authority.approved_call_ids={install.plan.call.call_id};CHECK(registry.validate(install.plan.call,authority));
 auto terrain=plan("terrain.generate coverage=hemisphere seed=12");CHECK(terrain);
 authority.granted_capabilities=ai::McpToolCapability::author;
 CHECK(registry.validate(terrain.plan.call,authority).error==ai::McpErrorCode::capability_denied);
 return 0;
}

'''

def fixture():
    app=ROOT/"Engine/src/editor/application"
    packages=(app/"editor.application_packages_creation.inl").read_text(encoding="utf-8")
    state=(app/"editor.application_state.inl").read_text(encoding="utf-8")
    field=definition(state,"            struct AiPackageRequest final",True)
    markers=(
        "        [[nodiscard]] bool ai_project_package_request_current(",
        "        void finish_ai_project_package_request(",
        "        [[nodiscard]] bool ai_package_generation_awaiting_attachment(",
        "        [[nodiscard]] std::string ai_project_package_catalog(",
        "        [[nodiscard]] bool dispatch_ai_project_package_tool(",
        "        [[nodiscard]] bool create_ai_project_from_tool(",
        "        [[nodiscard]] std::string ai_project_package_tool_description(",
        "        void continue_ai_project_package_after_attachment(",
        "        [[nodiscard]] bool ai_project_package_generation_current(")
    completion_declarations=[line.strip() for line in packages.splitlines()
        if line.strip().startswith("void finish_ai_project_package_request(")
        and line.strip().endswith(";")]
    assert len(completion_declarations)==1, "Expected the production completion declaration"
    helpers=completion_declarations[0]+"\n"+"\n".join(
        definition(packages[packages.rfind(marker):],marker) for marker in markers)
    helpers=helpers.replace('project_planetary::save_settings(root,*terrain)','record_save_settings(root,*terrain)').replace(
        'project_planetary::load_settings(root) != terrain','record_load_settings(root) != terrain')
    attach=definition(packages,"        void start_planetary_project_attachment(")
    cancel=definition(packages,"        void cancel_package_source_task(")
    busy=definition(packages,"        [[nodiscard]] bool planetary_project_busy(")
    return HOST.replace("GOAL_HEADER",(ROOT/"Engine/src/editor/ai/editor.ai_chat_goal.hpp").as_posix()).replace("FIELD",field).replace("BUSY",busy).replace("HELPERS",helpers).replace("ATTACH",attach).replace("CANCEL",cancel)

class AssistantPackageTools(unittest.TestCase):
    def test_existing_approval_and_publication_paths_are_connected(self):
        app=ROOT/"Engine/src/editor/application"
        packages=(app/"editor.application_packages_creation.inl").read_text(encoding="utf-8")
        poll=definition(packages,"        void poll_planetary_project_task(")
        self.assertIn("continue_ai_project_package_after_attachment(editor)",poll)
        self.assertLess(poll.index("ai_project_package_generation_current(editor)"),poll.index("import_project_mesh(editor"))
        runtime=(app/"editor.frame_project_runtime.inl").read_text(encoding="utf-8")
        self.assertIn("validate_project_tool_arguments(call)",runtime)
        self.assertIn("dispatch_ai_project_package_tool(editor, call, ctx)",runtime)
        view=(app/"editor.frame_packages_about.inl").read_text(encoding="utf-8")
        self.assertIn('"Add & Generate"',view)
        self.assertIn("ai_package_generation_awaiting_attachment(editor)",view)
        reset=definition((app/"editor.application_project_documents.inl").read_text(encoding="utf-8"),"        void reset_project_bound_editor_state(")
        self.assertIn("state.aiPackageRequest.reset()",reset)

    def test_actual_typed_protocol_and_source_attachment(self):
        proof=Path(os.environ.get("EPOCH_ASSISTANT_PACKAGE_REPORT",ROOT/"Artifacts/Reports/assistant-packages-update-oct28"))
        proof.mkdir(parents=True,exist_ok=True)
        (proof/"host_contract.cpp").write_text(fixture(),encoding="utf-8")
        (proof/"protocol_contract.cpp").write_text(PROTOCOL,encoding="utf-8")
        names=("ai.mcp.ixx","core.sha256.ixx","package.registry.ixx","package.catalog.ixx","platform.filesystem.ixx","package.source_install.ixx",
            "project.planetary.ixx","asset.cloud_volume.ixx","project.planetary_snapshot.ixx","water.wave.ixx","asset.mesh_artifact.ixx","asset.texture_artifact.ixx")
        modules=" ".join('"'+(ROOT/"Engine/modules"/name).as_posix()+'"' for name in names)
        token=str(time.time_ns())
        (proof/"CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(AssistantPackageTools LANGUAGES CXX)
add_executable(protocol_contract protocol_contract.cpp)
target_compile_features(protocol_contract PRIVATE cxx_std_23)
target_sources(protocol_contract PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/"Engine/modules").as_posix()}" FILES "{(ROOT/"Engine/modules/ai.mcp.ixx").as_posix()}")
add_executable(host_contract host_contract.cpp
 "{(ROOT/"Engine/src/packages/package.catalog.cpp").as_posix()}"
 "{(ROOT/"Engine/src/packages/package.source_install.cpp").as_posix()}")
target_compile_features(host_contract PRIVATE cxx_std_23)
target_sources(host_contract PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/"Engine/modules").as_posix()}" FILES {modules})
if(MSVC)
 target_compile_options(protocol_contract PRIVATE /utf-8 /EHsc /permissive-)
 target_compile_options(host_contract PRIVATE /utf-8 /EHsc /permissive-)
 target_sources(host_contract PRIVATE "{(ROOT/"Engine/resource/epoch.manifest").as_posix()}")
 target_link_libraries(host_contract PRIVATE winhttp)
endif()
enable_testing()
add_test(NAME typed_package_protocol COMMAND protocol_contract)
add_test(NAME typed_project_creation COMMAND host_contract "{(ROOT/'Artifacts/apkg').as_posix()}/create-$<CONFIG>-{token}" --creation-only)
add_test(NAME approved_attachment_continuation COMMAND host_contract "{(ROOT/'Artifacts/apkg').as_posix()}/$<CONFIG>-{token}")
set_tests_properties(typed_package_protocol typed_project_creation approved_attachment_continuation PROPERTIES TIMEOUT 30)
''',encoding="utf-8")
        configure=["cmake","-S",str(proof),"-B",str(proof/"cmake")]
        configure += ["-G","Visual Studio 17 2022","-A","x64"] if os.name=="nt" else ["-G","Ninja","-DCMAKE_BUILD_TYPE=Release"]
        commands=[("configure",configure)]
        for configuration in (("Debug","Release") if os.name=="nt" else ("Release",)):
            build=["cmake","--build",str(proof/"cmake"),"--config",configuration,"--parallel","1"]
            if os.name=="nt":build += ["--","/nr:false","/p:UseMultiToolTask=false","/p:CL_MPCount=1"]
            commands += [(configuration+"-build",build),(configuration+"-contract",["ctest","--test-dir",str(proof/"cmake"),"-C",configuration,"--output-on-failure"])]
        receipt={"scope":"Actual typed protocol/registry/source installer and continuation/attachment functions. Native UI, scheduler and provider generation are explicit recorded ports; no provider/native/Engine acceptance claim.","actual_modules":len(names),"phases":[]}
        for label,command in commands:
            result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,env=dict(os.environ,MSBUILDDISABLENODEREUSE="1"),timeout=180)
            (proof/(label+".log")).write_text(result.stdout+result.stderr,encoding="utf-8")
            receipt["phases"].append({"phase":label,"exit":result.returncode,"command":command})
            (proof/"result.json").write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
            self.assertEqual(result.returncode,0,(result.stdout+result.stderr)[-12000:])

if __name__=="__main__":unittest.main()
