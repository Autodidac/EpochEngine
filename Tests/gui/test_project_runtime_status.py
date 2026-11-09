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
        assistant = definition(source, '            else if (call.tool == "project.run")').replace(
            "else if", "if", 1)
        settlement = "\n".join(definition(runtime, marker) for marker in (
            "        [[nodiscard]] bool settle_project_build_lifecycle(",
            "        void abandon_project_build_lifecycle(",
            "        [[nodiscard]] bool project_runtime_identity_current("))
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
 struct {project_lifecycle::ProjectStamp project;} projectRuntimeLaunch;
 std::shared_ptr<platform::child_process::ProcessSnapshot> projectRuntimeSnapshot;
};
bool selected=true,output_exists=true,launch_allowed=true,inputs_current=true;
unsigned launches{},focuses{},builds{},tests{},transcripts{},watches{};
std::vector<std::string> logs,notes;
namespace epochengine {template<class...Args>std::string format_text(std::string_view text,Args&&...){return std::string{text};}}
void push_editor_log(EditorState&,std::string text){logs.push_back(std::move(text));}
void append_project_note(EditorState&,std::string_view,std::string_view summary,std::string_view detail){notes.emplace_back(std::string{summary}+" | "+std::string{detail});}
bool project_runtime_embedding_supported(){return true;}
void clear_completed_editor_builds(EditorState&,std::string_view){}
std::string script_id_from_source_path(const std::filesystem::path&){return "script";}
bool project_gui_document_layout(EditorState&){return true;}
bool save_project_gui_document(EditorState&,bool){return true;}
bool save_editor_scene_snapshot(EditorState&){return true;}
void repair_active_project_evidence(EditorState&){}
namespace project_planetary {struct Result{bool ok{true};std::string message;};
 Result prepare_runtime_dependency(const std::filesystem::path&){return {};}}
std::filesystem::path resolve_editor_path(const std::filesystem::path& path){return path;}
Freshness project_child_build_freshness(const EditorState&){return {.rebuildRequired=!inputs_current};}
std::filesystem::path project_existing_output_exe_path(std::string_view,std::string_view){return "child.exe";}
bool path_exists(const std::filesystem::path&){return output_exists;}
std::optional<Digest> project_output_digest(const std::filesystem::path&){return output_exists?std::optional{Digest{7}}:std::nullopt;}
bool project_build_targets_selected_project(const EditorState&){return selected;}
std::filesystem::path resolved_project_build_output(const EditorState&,const EditorProjectBuildResult& build){return build.output_path;}
bool project_runtime_launch_identity_matches(const EditorState&,const platform::child_process::ProcessSnapshot& snapshot){return snapshot.identity;}
SETTLEMENT
void reconcile_project_runtime(EditorState&){}
void start_project_build(EditorState&,bool,std::string,bool=false){++builds;}
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
void direct(EditorState& editor,bool inEditor=true){
 LAUNCH
 launch_active_project_context(inEditor);
}
void poll(EditorState& editor){Chat chat;
 POLL
 poll_project_build();
}
bool assistant(EditorState& editor,bool queueBuild=false){
 struct {std::string tool{"project.run"};}call;
 auto launch_active_project_context=[&](bool inEditor){if(queueBuild){
  std::promise<EditorProjectBuildResult> promise;editor.projectBuildPending.emplace(promise.get_future());
  editor.projectBuildStatus="Building Release...";
 }else if(!editor.projectRuntimeSnapshot)direct(editor,inEditor);};
 auto watch_execution=[&](std::string_view operation){if(operation=="project.run")++watches;};
 ASSISTANT
 return true;
}
void initialize(EditorState& editor){
 selected=output_exists=launch_allowed=inputs_current=true;
 launches=focuses=builds=tests=transcripts=watches=0;logs.clear();notes.clear();
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
 {EditorState editor;initialize(editor);direct(editor);
  CHECK(launches==1&&builds==0&&editor.projectBuildStatus=="Release build accepted");
  CHECK(editor.projectRuntimeStatus.starts_with("Starting project"));
  direct(editor);CHECK(focuses==1&&launches==1&&editor.projectBuildStatus=="Release build accepted");
  editor.projectRuntimeSnapshot.reset();editor.projectLifecycle.record_runtime_stopped();
  editor.projectRuntimeStatus="Exited with code 0 after 462s.";
  CHECK(editor.projectBuildStatus=="Release build accepted"&&editor.projectRuntimeStatus.starts_with("Exited"));
  launch_allowed=false;direct(editor);
  CHECK(editor.projectBuildStatus=="Release build accepted"&&editor.projectRuntimeStatus.starts_with("Launch refused"));
  CHECK(!logs.empty()&&logs.back().find(editor.projectRuntimeStatus)!=std::string::npos);}
 {EditorState editor;queued(editor);poll(editor);
  CHECK(!editor.projectBuildPending&&transcripts==1&&launches==1);
  CHECK(editor.projectBuildStatus=="Release build accepted"&&editor.projectRuntimeStatus.starts_with("Starting project"));
  CHECK(editor.projectLifecycle.evidence().output_exists&&editor.projectLifecycle.evidence().runtime_active);
  CHECK(!notes.empty()&&notes.back().starts_with("Release build accepted"));}
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
  CHECK(watches==1&&editor.aiToolStatus.find("runtime=Starting project")!=std::string::npos);
  CHECK(editor.aiToolStatus.find("build=Release build accepted")!=std::string::npos);}
 {EditorState editor;initialize(editor);launch_allowed=false;CHECK(!assistant(editor));
  CHECK(watches==0&&editor.aiToolStatus.find("runtime=Launch refused")!=std::string::npos);
  CHECK(editor.aiToolStatus.find("build=Release build accepted")!=std::string::npos);}
 {EditorState editor;initialize(editor);CHECK(assistant(editor,true));
  CHECK(watches==1&&editor.aiToolStatus.find("build=Building Release")!=std::string::npos);}
 {EditorState editor;initialize(editor);editor.projectRuntimeSnapshot=std::make_shared<platform::child_process::ProcessSnapshot>();
  editor.projectRuntimeSnapshot->running=true;editor.projectRuntimeLaunch.project={999,999};
  CHECK(!assistant(editor)&&watches==0);}
}
'''
        for key, fragment in {"LIFECYCLE": lifecycle, "SETTLEMENT": settlement,
                              "LAUNCH": launch, "POLL": poll, "ASSISTANT": assistant}.items():
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
