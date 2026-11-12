"""Actual Run/build dispatch keeps build receipts separate from runtime status."""
from pathlib import Path
import hashlib
import json
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
APPLICATION = ROOT / "Engine/src/editor/application"


class ProjectRuntimeStatus(unittest.TestCase):
    def test_actual_launch_settlement_and_assistant_status_debug_release(self):
        owner = APPLICATION / "editor.frame_project_runtime.inl"
        runtime_owner = APPLICATION / "editor.application_project_runtime.inl"
        lifecycle_owner = ROOT / "Engine/modules/project.lifecycle.ixx"
        source = owner.read_text(encoding="utf-8")
        runtime = runtime_owner.read_text(encoding="utf-8")
        lifecycle = lifecycle_owner.read_text(encoding="utf-8")
        lifecycle = lifecycle.replace("module;", "").replace(
            "export module project.lifecycle;", "").replace("export namespace", "namespace")
        launch = definition(source, "        auto launch_active_project_context =", True)
        poll = definition(source, "        auto poll_project_build =", True)
        deferred = definition(source, "        if (editor.projectRunAfterTerrainPreview && !planetary_project_busy(editor))")
        assistant = definition(source, '            else if (call.tool == "project.run")').replace(
            "else if", "if", 1)
        settlement = "\n".join(definition(runtime, marker) for marker in (
            "        [[nodiscard]] bool settle_project_build_lifecycle(",
            "        void abandon_project_build_lifecycle(",
            "        [[nodiscard]] bool project_runtime_identity_current(",
            "        [[nodiscard]] bool resolve_project_runtime_embedding("))
        code = r'''
#include <chrono>
#include <exception>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
LIFECYCLE
using namespace std::chrono_literals;
namespace project_lifecycle=epochengine::project_lifecycle;
namespace platform::child_process {struct ProcessSnapshot {
 bool running{};bool identity{true};bool active()const{return running;}
};}
namespace editor_tasks {struct TaskCancelled:std::exception {};}
enum class EditorWorkspaceTab{Project,Scripts,Assets};
struct EditorProjectBuildResult {bool succeeded{true},cancelled{};
 std::string summary{"Release build accepted"},output_path{"child.exe"},log_path{"build.log"};};
struct Freshness {std::uint64_t inputFingerprint{7};bool rebuildRequired{};std::string reason{"current"};};
using ProjectBuildFreshness=Freshness;
struct Digest {int value{};bool valid()const{return value!=0;}explicit operator bool()const{return valid();}
 friend bool operator==(const Digest&,const Digest&)=default;};
struct EditorState {
 std::optional<std::future<EditorProjectBuildResult>> projectBuildPending;
 std::optional<bool> projectRunAfterTerrainPreview;
 bool planetaryStreamPending{};
 bool planetaryAttachmentPending{},planetaryAttachmentIsInstall{},planetaryAttachmentIsUpdate{};
 bool aiContinuousBuildPending{},projectBuildRunAfterBuild{},projectBuildRunInEditor{},aiToolTestAfterBuild{};
 bool projectRuntimeInEditor{},sceneRunMaximizedInEditor{},projectRuntimeViewportMaximized{},sceneRunFullscreenInEditor{};
 EditorWorkspaceTab workspaceTab{EditorWorkspaceTab::Project};
 std::string selectedAssetPath,activeScript,scriptBuildStatus,projectRoot{"project"},projectBuildConfiguration{"Release"};
 std::string projectBuildStatus{"Release build accepted"},projectRuntimeStatus{"Exited with code 0 after 462s."};
 std::string projectBuildRunScene,projectBuildRunBackend,projectRunBackend,projectBuildDiagnosticTail,aiToolStatus;
 double projectBuildRunFrameLimitFps{30},projectRunFrameLimitFps{30};
 int projectBuildRunCameraMode{},projectBuildRunInputProfile{},projectCameraMode{},inputProfilePreset{};
 Digest projectBuildAcceptedOutputDigest{7};std::uint64_t projectBuildLifecycleGeneration{};
 project_lifecycle::EvidenceLedger projectLifecycle;
 bool aiGoalActive{},aiGoalRunning{};
 struct {bool build_and_run{};std::uint64_t accepted_build_generation{};} aiGoalTools;
 struct {project_lifecycle::ProjectStamp project;} projectRuntimeLaunch;
 std::shared_ptr<platform::child_process::ProcessSnapshot> projectRuntimeSnapshot;
};
bool selected=true,output_exists=true,launch_allowed=true,inputs_current=true;
int output_digest=7;
unsigned launches{},focuses{},builds{},tests{},transcripts{},watches{},scene_saves{},dependency_prepares{};
std::vector<std::string> logs,notes;
namespace epochengine {template<class...Args>std::string format_text(std::string_view text,Args&&...){return std::string{text};}}
void push_editor_log(EditorState&,std::string text){logs.push_back(std::move(text));}
void append_project_note(EditorState&,std::string_view,std::string_view summary,std::string_view detail){notes.emplace_back(std::string{summary}+" | "+std::string{detail});}
namespace core {enum class ContextType{OpenGL,Vulkan};struct Context{ContextType type{ContextType::OpenGL};};}
auto ctx=std::make_shared<core::Context>();
bool embedding_supported=true;
bool project_runtime_embedding_supported(){return embedding_supported;}
void clear_completed_editor_builds(EditorState&,std::string_view){}
std::string script_id_from_source_path(const std::filesystem::path&){return "script";}
bool project_gui_document_layout(EditorState&){return true;}
bool save_project_gui_document(EditorState&,bool){return true;}
bool save_editor_scene_snapshot(EditorState&){++scene_saves;return true;}
void repair_active_project_evidence(EditorState&){}
namespace project_planetary {struct Result{bool ok{true};std::string message;};
 Result prepare_runtime_dependency(const std::filesystem::path&){++dependency_prepares;return {};}
 Result runtime_requirement(const std::filesystem::path&){return {};}}
std::filesystem::path resolve_editor_path(const std::filesystem::path& path){return path;}
Freshness project_child_build_freshness(const EditorState&){return {.rebuildRequired=!inputs_current};}
std::filesystem::path project_existing_output_exe_path(std::string_view,std::string_view){return "child.exe";}
bool path_exists(const std::filesystem::path&){return output_exists;}
std::optional<Digest> project_output_digest(const std::filesystem::path&){return output_exists?std::optional{Digest{output_digest}}:std::nullopt;}
bool project_build_targets_selected_project(const EditorState&){return selected;}
std::filesystem::path resolved_project_build_output(const EditorState&,const EditorProjectBuildResult& build){return build.output_path;}
bool project_runtime_launch_identity_matches(const EditorState&,const platform::child_process::ProcessSnapshot& snapshot){return snapshot.identity;}
SETTLEMENT
void reconcile_project_runtime(EditorState&){}
void start_project_build(EditorState& editor,bool run,std::string,bool inEditor=false){
 ++builds;
 editor.projectBuildLifecycleGeneration=editor.projectLifecycle.begin_build();
 editor.projectBuildRunAfterBuild=run;editor.projectBuildRunInEditor=inEditor;
 editor.projectBuildRunBackend=editor.projectRunBackend;
 editor.projectBuildStatus="Building Release...";
 std::promise<EditorProjectBuildResult> promise;
 editor.projectBuildPending.emplace(promise.get_future());promise.set_value({});
}
bool focus_project_runtime(EditorState& editor){++focuses;editor.projectRuntimeStatus="Owned runtime focused";return true;}
std::string project_runtime_scene_id(const EditorState&){return "scene";}
std::string_view project_run_backend_label(std::string_view backend){return backend;}
bool launch_project_runtime(EditorState& editor,const std::filesystem::path&,std::string_view,std::string_view,double,int,int,bool inEditor){
 ++launches;
 if(!launch_allowed){editor.projectRuntimeStatus="Launch refused: owned child unavailable";return false;}
 editor.projectRuntimeInEditor=inEditor;
 editor.projectRuntimeLaunch.project=editor.projectLifecycle.evidence().generations.selected_project;
 editor.projectRuntimeSnapshot=std::make_shared<platform::child_process::ProcessSnapshot>();
 editor.projectRuntimeSnapshot->running=true;
 (void)editor.projectLifecycle.record_runtime_started();
 editor.projectRuntimeStatus="Starting project; waiting for its window to appear inside the Editor.";
 return true;
}
void refresh_project_build_progress(EditorState&){}
void retain_project_build_transcript(EditorState&,const EditorProjectBuildResult&){++transcripts;}
std::string read_text_tail(const std::filesystem::path&,std::size_t){return "compiler diagnostic";}
void reset_project_build_request(EditorState& editor){editor.projectBuildRunAfterBuild=editor.projectBuildRunInEditor=false;}
bool start_ai_project_test(const std::filesystem::path&){++tests;return true;}
struct Chat{void append_status(std::string_view){}};
bool planetary_project_busy(const EditorState& editor){return editor.planetaryStreamPending||editor.planetaryAttachmentPending;}
void direct(EditorState& editor,bool inEditor=true,std::uint64_t requiredBuild=0,bool allowTerrainWait=false){
 LAUNCH
 launch_active_project_context(inEditor,requiredBuild,allowTerrainWait);
}
void finish_terrain_preview(EditorState& editor){
 LAUNCH
 DEFERRED
}
void poll(EditorState& editor){Chat chat;
 POLL
 poll_project_build();
}
bool assistant(EditorState& editor,bool queueBuild=false){
 struct {std::string tool{"project.run"};}call;
 auto launch_active_project_context=[&](bool inEditor,std::uint64_t requiredBuild){if(queueBuild){
  std::promise<EditorProjectBuildResult> promise;editor.projectBuildPending.emplace(promise.get_future());
  editor.projectBuildStatus="Building Release...";
 }else if(!editor.projectRuntimeSnapshot)direct(editor,inEditor,requiredBuild);};
 auto watch_execution=[&](std::string_view operation){if(operation=="project.run")++watches;};
 ASSISTANT
 return true;
}
void initialize(EditorState& editor){
 selected=output_exists=launch_allowed=inputs_current=true;
 embedding_supported=true;ctx->type=core::ContextType::OpenGL;
 output_digest=7;
 launches=focuses=builds=tests=transcripts=watches=scene_saves=dependency_prepares=0;logs.clear();notes.clear();
 (void)editor.projectLifecycle.select_project(1);
 (void)editor.projectLifecycle.record_scene_commit(1);
 (void)editor.projectLifecycle.record_shell_materialized();
 (void)editor.projectLifecycle.observe_build_inputs(7);
 auto generation=editor.projectLifecycle.begin_build();
 (void)editor.projectLifecycle.complete_build(generation,true);
}
void queued(EditorState& editor,EditorProjectBuildResult build={}){
 initialize(editor);
 editor.projectBuildLifecycleGeneration=editor.projectLifecycle.begin_build();
 editor.projectBuildRunAfterBuild=true;editor.projectBuildRunInEditor=true;
 editor.projectBuildStatus="Finishing Release...";
 std::promise<EditorProjectBuildResult> promise;
 editor.projectBuildPending.emplace(promise.get_future());promise.set_value(std::move(build));
}
#define CHECK(condition) do{if(!(condition))return __LINE__;}while(false)
int main(){
 {EditorState editor;initialize(editor);editor.planetaryStreamPending=true;
  direct(editor,false,0,true);
  CHECK(editor.projectRunAfterTerrainPreview==false&&scene_saves==0&&launches==0);
  direct(editor,true,0,true);finish_terrain_preview(editor);
  CHECK(editor.projectRunAfterTerrainPreview==false&&launches==0);
  editor.planetaryStreamPending=false;finish_terrain_preview(editor);finish_terrain_preview(editor);
  CHECK(builds==1&&launches==0);poll(editor);
  CHECK(!editor.projectRunAfterTerrainPreview&&launches==1&&!editor.projectRuntimeInEditor);}
 {EditorState editor;initialize(editor);editor.planetaryAttachmentPending=true;
  direct(editor,true,0,true);finish_terrain_preview(editor);
  CHECK(editor.projectRunAfterTerrainPreview==true&&scene_saves==0&&launches==0);
  editor.planetaryAttachmentPending=false;finish_terrain_preview(editor);
  CHECK(builds==1&&launches==0);poll(editor);
  CHECK(!editor.projectRunAfterTerrainPreview&&launches==1&&editor.projectRuntimeInEditor);}
 {EditorState editor;initialize(editor);output_exists=false;editor.planetaryStreamPending=true;
  direct(editor,false,0,true);CHECK(builds==0&&editor.projectRunAfterTerrainPreview==false);
  editor.planetaryStreamPending=false;finish_terrain_preview(editor);finish_terrain_preview(editor);
  CHECK(builds==1&&launches==0&&!editor.projectRunAfterTerrainPreview);}
 {EditorState editor;initialize(editor);direct(editor);
  CHECK(launches==0&&builds==1&&editor.projectBuildPending);poll(editor);
  CHECK(launches==1&&builds==1&&editor.projectBuildStatus=="Release build accepted");
  CHECK(editor.projectRuntimeStatus.starts_with("Starting project"));
  direct(editor);CHECK(focuses==1&&launches==1&&builds==1&&editor.projectBuildStatus=="Release build accepted");
  editor.projectRuntimeSnapshot.reset();editor.projectLifecycle.record_runtime_stopped();
  editor.projectRuntimeStatus="Exited with code 0 after 462s.";
  CHECK(editor.projectBuildStatus=="Release build accepted"&&editor.projectRuntimeStatus.starts_with("Exited"));
  launch_allowed=false;direct(editor);CHECK(builds==2&&launches==1);poll(editor);
  CHECK(editor.projectBuildStatus=="Release build accepted"&&editor.projectRuntimeStatus.starts_with("Launch refused"));
  CHECK(!logs.empty()&&logs.back().find(editor.projectRuntimeStatus)!=std::string::npos);}
 {EditorState editor;queued(editor);poll(editor);
  CHECK(!editor.projectBuildPending&&transcripts==1&&launches==1);
  CHECK(editor.projectBuildStatus=="Release build accepted"&&editor.projectRuntimeStatus.starts_with("Starting project"));
  CHECK(editor.projectLifecycle.evidence().output_exists&&editor.projectLifecycle.evidence().runtime_active);
  CHECK(!notes.empty()&&notes.back().starts_with("Release build accepted"));}
 {EditorState editor;initialize(editor);editor.projectRunBackend="vulkan";direct(editor);poll(editor);
  CHECK(launches==1&&!editor.projectRuntimeInEditor&&editor.projectRunBackend=="vulkan");
  CHECK(logs.front().find("Using Separate Window")!=std::string::npos);}
 {EditorState editor;initialize(editor);editor.projectRunBackend="vulkan";
  ctx->type=core::ContextType::Vulkan;direct(editor);poll(editor);
  CHECK(launches==1&&editor.projectRuntimeInEditor);}
 {EditorState editor;queued(editor);editor.projectBuildRunBackend="vulkan";poll(editor);
  CHECK(launches==1&&!editor.projectRuntimeInEditor&&editor.projectBuildRunBackend=="vulkan");}
 {EditorState editor;initialize(editor);embedding_supported=false;direct(editor);poll(editor);
  CHECK(launches==1&&!editor.projectRuntimeInEditor);}
 {EditorState editor;queued(editor);launch_allowed=false;poll(editor);
  CHECK(launches==1&&editor.projectBuildStatus=="Release build accepted");
  CHECK(editor.projectRuntimeStatus.starts_with("Launch refused")&&logs.back().find("Runtime launch refused")!=std::string::npos);}
 {EditorState editor;queued(editor);inputs_current=false;poll(editor);
  CHECK(launches==0&&editor.projectBuildStatus.find("Artifact evidence was rejected")!=std::string::npos);
  CHECK(!editor.projectLifecycle.evidence().output_exists&&!editor.projectBuildAcceptedOutputDigest);}
 {EditorState editor;EditorProjectBuildResult build;build.succeeded=false;build.summary="compiler failed";
  queued(editor,build);poll(editor);CHECK(launches==0&&editor.projectBuildStatus=="compiler failed");
  CHECK(editor.projectBuildDiagnosticTail=="compiler diagnostic");}
 {EditorState editor;queued(editor);selected=false;poll(editor);
  CHECK(launches==0&&editor.projectBuildStatus.find("Artifact evidence was rejected")!=std::string::npos);}
 {EditorState editor;queued(editor);editor.aiToolTestAfterBuild=true;poll(editor);
  CHECK(tests==1&&launches==0&&editor.projectBuildStatus=="Release build accepted");}
 {EditorState editor;initialize(editor);CHECK(assistant(editor));
  CHECK(watches==1&&builds==1&&launches==0&&editor.aiToolStatus.find("build=Building Release")!=std::string::npos);
  poll(editor);CHECK(launches==1&&editor.projectRuntimeStatus.starts_with("Starting project"));}
 {EditorState editor;initialize(editor);launch_allowed=false;CHECK(assistant(editor));
  CHECK(watches==1&&builds==1&&launches==0);poll(editor);
  CHECK(editor.projectRuntimeStatus.starts_with("Launch refused")&&editor.projectBuildStatus=="Release build accepted");}
 {EditorState editor;initialize(editor);CHECK(assistant(editor,true));
  CHECK(watches==1&&editor.aiToolStatus.find("build=Building Release")!=std::string::npos);}
 {EditorState editor;initialize(editor);editor.projectRuntimeSnapshot=std::make_shared<platform::child_process::ProcessSnapshot>();
  editor.projectRuntimeSnapshot->running=true;editor.projectRuntimeLaunch.project={999,999};
  CHECK(!assistant(editor)&&watches==0);}
 for(const unsigned stale:{0u,1u,2u,3u,4u}){EditorState editor;initialize(editor);
  editor.aiGoalActive=editor.aiGoalRunning=editor.aiGoalTools.build_and_run=true;
  editor.aiGoalTools.accepted_build_generation=editor.projectLifecycle.evidence().generations.output_artifact.build_generation;
  CHECK(editor.aiGoalTools.accepted_build_generation!=0);
  if(stale==1u)output_exists=false;
  if(stale==2u)inputs_current=false;
  if(stale==3u)output_digest=8;
  if(stale==4u)++editor.aiGoalTools.accepted_build_generation;
  CHECK(assistant(editor)==(stale==0u));
  CHECK(builds==0&&scene_saves==0&&dependency_prepares==0);
  CHECK(launches==(stale==0u?1u:0u)&&watches==launches);
  if(stale!=0u)CHECK(editor.aiToolStatus.find("accepted build is no longer current")!=std::string::npos);
 }
}
'''
        for key, fragment in {"LIFECYCLE": lifecycle, "SETTLEMENT": settlement,
                              "LAUNCH": launch, "POLL": poll, "ASSISTANT": assistant,
                              "DEFERRED": deferred}.items():
            code = code.replace(key, fragment)
        identity = hashlib.sha256(code.encode()).hexdigest()[:12]
        report = ROOT / "Artifacts/Reports/project-runtime-status84-nov07" / identity
        report.mkdir(parents=True, exist_ok=True)
        cpp = report / "consumer.cpp"
        cpp.write_text(code, encoding="utf-8")
        runs = compile_and_run(self, cpp, report)
        (report / "result.json").write_text(json.dumps({
            "passed": True, "runs": runs,
            "sources": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                        for path in (owner, runtime_owner, lifecycle_owner, Path(__file__))},
            "scope": "Actual frame launch, full build settlement dispatch, Assistant run branch and production lifecycle/settlement/identity code. Compiler, digest, process and GUI operations are recorded CPU ports. No renderer, model or native runtime acceptance."
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
