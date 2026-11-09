"""Actual build owner, filesystem and child supervisor; no compiler or renderer launch.

Consumes the accepted Release module/library closure. The generated fixture's
PowerShell script emits diagnostic text or waits for owned cancellation only.
"""
from pathlib import Path
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import msvc_2022_toolsets, windows_sdk_includes

ROOT = Path(__file__).resolve().parents[2]


class ProjectBuildTranscripts(unittest.TestCase):
    def compile_and_run(self, name, code):
        products = ROOT / "Artifacts/Runtime/windows/development/x64/Release"
        if os.name != "nt" or not (products / "EpochEngine.lib").is_file():
            self.skipTest("Accepted Windows Release owner products required")
        out = ROOT / "Artifacts/Reports/project-build-progress-oct27" / name
        out.mkdir(parents=True, exist_ok=True)
        source = out / "contract.cpp"
        source.write_text(code, encoding="utf-8")
        tool = msvc_2022_toolsets()[-1]
        sdk = Path("C:/Program Files (x86)/Windows Kits/10")
        kit = windows_sdk_includes(sdk)[-1]
        references = {}
        for file in (ROOT / "Artifacts/Intermediate/msvc/development/x64/Release").rglob("*.ixx.ifc"):
            module = file.name.removesuffix(".ixx.ifc")
            if module not in references or file.parent.name == "EpochEngine":
                references[module] = file
        args = ["/nologo", "/std:c++23preview", "/EHsc", "/MD", "/DNDEBUG", "/permissive-",
                "/fp:precise", "/D_GUARDOVERFLOW_CRT_ALLOCATORS=1"]
        args += ["/I" + str(path) for path in (tool / "include", kit / "ucrt", kit / "um", kit / "shared")]
        for module, file in references.items():
            args += ["/reference", module + "=" + str(file)]
        args += ["/Fo" + str(out / "contract.obj"), "/Fe" + str(out / "contract.exe"), str(source), "/link"]
        args += ["/LIBPATH:" + str(path) for path in (tool / "lib/x64", sdk / "Lib" / kit.name / "um/x64", sdk / "Lib" / kit.name / "ucrt/x64")]
        args += [str(products / "EpochEngine.lib"), str(products / "EpochPlatformEngine.lib")]
        args += [name + ".lib" for name in ("user32", "advapi32", "kernel32", "shell32", "shlwapi", "ole32", "uuid", "dbghelp", "ws2_32", "winmm", "psapi", "bcrypt", "ntdll", "userenv", "winhttp")]
        response = out / "compile.rsp"
        response.write_text(" ".join('"' + arg + '"' if arg not in ("/reference", "/link") else arg for arg in args), encoding="utf-8")
        environment = dict(os.environ)
        environment["PATH"] = str(tool / "bin/Hostx64/x64") + os.pathsep + environment.get("PATH", "")
        environment["TMP"] = environment["TEMP"] = str(out)
        runs = []
        for label, command in (("compile", [str(tool / "bin/Hostx64/x64/cl.exe"), "@" + str(response)]),
                               ("run", [str(out / "contract.exe")])):
            result = subprocess.run(command, cwd=out, env=environment, capture_output=True, text=True, timeout=30)
            (out / (label + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
            runs.append({"stage": label, "exit_code": result.returncode})
            (out / "runs.json").write_text(json.dumps(runs, indent=2), encoding="utf-8")
            self.assertEqual(result.returncode, 0, result.stdout[-7000:] + result.stderr)

    def test_unique_attempts_real_merged_stderr_failure_cancel_and_owner(self):
        if os.name != "nt": self.skipTest("Windows child supervisor and PowerShell transcript consumer")
        registry = (ROOT / "Engine/src/editor/implementation/editor.project_registry.inl").read_text(encoding="utf-8")
        scene = (ROOT / "Engine/src/editor/editor.scene.cpp").read_text(encoding="utf-8")
        runtime = (ROOT / "Engine/src/editor/application/editor.application_project_runtime.inl").read_text(encoding="utf-8")
        paths = (ROOT / "Engine/src/editor/application/editor.application_runtime_source.inl").read_text(encoding="utf-8")
        core = (ROOT / "Engine/modules/editor.core.ixx").read_text(encoding="utf-8")
        session = (ROOT / "Engine/src/editor/application/editor.application_context_session.inl").read_text(encoding="utf-8")
        lifecycle = (ROOT / "Engine/src/editor/application/editor.application_context_lifecycle.inl").read_text(encoding="utf-8")
        common_stop = definition(session, "        void request_editor_task_session_stop(")
        cancel_build = definition(runtime, "        [[nodiscard]] bool cancel_project_build(")
        code_workspaces = (ROOT / "Engine/src/editor/application/editor.application_code_workspaces.inl").read_text(encoding="utf-8")
        continuous_build = definition(code_workspaces, "        void start_ai_continuous_project_build(")
        # These are the actual two production call sites, including Public;
        # the helper must run before each private source scheduler retirement.
        for function, argument in (("    void cleanup_chat_context(", "retiredState.mapped()"),
                                   ("    void shutdown_chat_system(", "state")):
            body = definition(lifecycle, function)
            self.assertIn("request_editor_task_session_stop(" + argument + ");", body)
            private_marker = "EPOCH_PRIVATE_" + "SOURCE_BEGIN:"
            if private_marker in body:  # Physically absent from the Public edition.
                self.assertLess(body.index("request_editor_task_session_stop("), body.index(private_marker))
        helpers = "\n".join(definition(registry, marker) for marker in (
            "    [[nodiscard]] static fs::path generated_project_windows_build_script_path(",
            "    [[nodiscard]] static fs::path generated_project_linux_build_script_path(",
            "    [[nodiscard]] static fs::path generated_project_build_log_path(",
            "    [[nodiscard]] static fs::path reserve_generated_project_build_transcript("))
        result = definition(core, "    export struct EditorProjectBuildResult", True).replace("export ", "", 1)
        supported = definition(core, "    export [[nodiscard]] constexpr bool editor_project_build_configuration_supported(").replace("export ", "", 1)
        worker = definition(scene, "    EditorProjectBuildResult editor_build_project(\n")
        display = definition(paths, "        [[nodiscard]] std::filesystem::path project_build_transcript_for_display(") + "\n" + "\n".join(definition(runtime, marker) for marker in (
            "        [[nodiscard]] bool project_build_targets_selected_project(",
            "        void retain_project_build_transcript("))
        code = r'''
#include <cassert>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <mutex>
#include <optional>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>
import platform.child_process;
import project.lifecycle;
import editor.task_scheduler;
#undef NDEBUG
#include <cassert>
namespace fs=std::filesystem;
namespace platform=epochengine::platform;
namespace project_lifecycle=epochengine::project_lifecycle;
namespace editor_tasks=epochengine::editor_tasks;
namespace taskgraph=epochengine::taskgraph;
namespace epochengine { SUPPORTED }
using epochengine::editor_project_build_configuration_supported;
RESULT
fs::path resolve_project_root_path(const fs::path& path){return fs::absolute(path).lexically_normal();}
std::optional<std::string> editor_project_build_configuration(std::string_view){return "Debug";}
struct Profile{std::string id;};const Profile* find_project_profile_by_root(std::string_view){return nullptr;}
EditorProjectBuildResult editor_ensure_project_shell(std::string_view){return {};}
bool repair_generated_windows_child_project_build_files(const fs::path&){return true;}
fs::path generated_project_output_path(const fs::path& root,std::string_view){return root/"product.exe";}
fs::path generated_project_existing_output_path(const fs::path& root,std::string_view config){return generated_project_output_path(root,config);}
HELPERS
WORKER
struct EditorState{project_lifecycle::EvidenceLedger projectLifecycle;std::uint64_t projectBuildProjectKey{},projectBuildLifecycleGeneration{};std::string projectBuildProjectRoot,projectRoot,projectBuildRequestedConfiguration{"Debug"},projectBuildConfiguration{"Debug"};struct Transcript{project_lifecycle::ProjectStamp project;std::string configuration,path;}projectBuildTranscript;
std::unique_ptr<editor_tasks::Scheduler>taskScheduler;editor_tasks::CancellationTicket projectBuildCancellation{},scriptBuildCancellation{},planetaryCancellation{},planetaryPreviewCancellation{},assetGenerationCancellation{},aiDirectRuntimeCancellation{},aiAuthoringApplyCancellation{},aiToolTestCancellation{},aiContinuousBuildCancellation{};std::optional<std::future<EditorProjectBuildResult>>projectBuildPending,aiContinuousBuildPending;bool projectBuildRunAfterBuild{true},aiProjectScriptRunAfterBuild{true},aiProjectScriptRepairQueued{true},aiToolTestAfterBuild{true};std::string projectBuildStatus,aiContinuousBuildStatus,aiContinuousBuildFingerprint;};
void push_editor_log(EditorState&,const std::string&){}
std::string ai_continuous_build_fingerprint(EditorState&,const std::string&,const fs::path&){return "explicit unchanged fixture identity";}
template<class Op>auto schedule_editor_cancellable_task(EditorState&e,std::string name,Op&&op){return e.taskScheduler->submit_cancellable(std::move(name),std::forward<Op>(op));}
CANCEL_BUILD
COMMON_STOP
CONTINUOUS_BUILD
fs::path project_build_log_path(std::string_view root,std::string_view config){return generated_project_build_log_path(fs::path{root},config);}
DISPLAY
std::string read(const fs::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>{f},{}};}
void write(const fs::path& path,std::string_view text){fs::create_directories(path.parent_path());std::ofstream f(path,std::ios::binary);f<<text;assert(f);}
int main(){
 const auto root=fs::current_path()/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());fs::create_directory(root);
 const auto script=root/"build_project.ps1";write(root/"product.exe","Fixture output only; never executed.");
 write(script,"param($Configuration,$Platform)\nWrite-Output 'source validator stdout'\n[Console]::Error.WriteLine('quoted \"source validation stderr\"')\nexit 0\n");
 const auto first=editor_build_project(root.string(),{},"Debug");assert(first.succeeded&&!first.log_path.empty());const auto original=read(first.log_path);assert(original.find("source validator stdout")!=std::string::npos&&original.find("quoted \"source validation stderr\"")!=std::string::npos);assert(original.find("elapsed_seconds=")!=std::string::npos);
 write(script,"param($Configuration,$Platform)\n[Console]::Error.WriteLine('fixture.cpp(10): error C1234: compiler diagnostic')\nexit 7\n");
 const auto failed=editor_build_project(root.string(),{},"Debug");assert(!failed.succeeded&&!failed.cancelled&&failed.log_path!=first.log_path);assert(read(first.log_path)==original);assert(read(failed.log_path).find("error C1234")!=std::string::npos&&failed.summary.find("exit code 7")!=std::string::npos);
 write(script,"param($Configuration,$Platform)\nWrite-Output 'waiting for owned cancellation'\nStart-Sleep -Seconds 10\n");std::stop_source stop;auto pending=std::async(std::launch::async,[&]{return editor_build_project(root.string(),stop.get_token(),"Debug");});
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds{5};while(!platform::child_process::active_in_group("epoch.project.build")&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds{2});assert(platform::child_process::active_in_group("epoch.project.build"));
 const auto count=std::distance(fs::directory_iterator(root/"Artifacts/Reports"),fs::directory_iterator{});const auto duplicate=editor_build_project(root.string(),{},"Debug");assert(!duplicate.succeeded&&duplicate.log_path.empty());assert(count==std::distance(fs::directory_iterator(root/"Artifacts/Reports"),fs::directory_iterator{}));stop.request_stop();const auto cancelled=pending.get();assert(cancelled.cancelled&&cancelled.log_path!=failed.log_path);assert(!platform::child_process::active_in_group("epoch.project.build"));assert(read(cancelled.log_path).find("cancelled=1")!=std::string::npos&&read(first.log_path)==original);
 EditorState noWork;request_editor_task_session_stop(noWork);assert(!noWork.projectBuildRunAfterBuild&&!noWork.taskScheduler);
 EditorState closing,unrelated;unrelated.projectBuildRunAfterBuild=true;closing.taskScheduler=std::make_unique<editor_tasks::Scheduler>(editor_tasks::SchedulerLimits{.worker_count=4,.retained_finished_nodes=32});
 std::promise<void>aiEntered;auto aiReady=aiEntered.get_future();std::stop_token aiToken;
 auto aiTask=closing.taskScheduler->submit_cancellable("AI source task",[&](std::stop_token token){aiToken=token;aiEntered.set_value();while(!token.stop_requested())std::this_thread::sleep_for(std::chrono::milliseconds{1});return 11;});
 assert(aiReady.wait_for(std::chrono::seconds{3})==std::future_status::ready);
 auto buildTask=closing.taskScheduler->submit_cancellable("Project Build",[&](std::stop_token token){return editor_build_project(root.string(),token,"Debug");});closing.projectBuildCancellation=buildTask.cancellation;closing.projectBuildPending.emplace(std::move(buildTask.completion));
 const auto started=std::chrono::steady_clock::now()+std::chrono::seconds{5};while(!platform::child_process::active_in_group("epoch.project.build")&&std::chrono::steady_clock::now()<started)std::this_thread::sleep_for(std::chrono::milliseconds{2});assert(platform::child_process::active_in_group("epoch.project.build"));
 std::vector<std::future<int>>ownedTasks;
 for(auto*ticket:std::array{&closing.scriptBuildCancellation,&closing.planetaryCancellation,&closing.planetaryPreviewCancellation,&closing.assetGenerationCancellation,&closing.aiDirectRuntimeCancellation,&closing.aiAuthoringApplyCancellation,&closing.aiToolTestCancellation,&closing.aiContinuousBuildCancellation}){
  auto task=closing.taskScheduler->submit_cancellable("Owned common task",[](std::stop_token token){while(!token.stop_requested())std::this_thread::sleep_for(std::chrono::milliseconds{1});return 1;});*ticket=task.cancellation;ownedTasks.push_back(std::move(task.completion));}
 const auto closed=std::chrono::steady_clock::now();request_editor_task_session_stop(closing);assert(!closing.projectBuildRunAfterBuild&&!closing.aiProjectScriptRunAfterBuild&&!closing.aiProjectScriptRepairQueued&&!closing.aiToolTestAfterBuild&&closing.projectBuildPending&&closing.taskScheduler&&unrelated.projectBuildRunAfterBuild&&!aiToken.stop_requested());
 assert(closing.projectBuildPending->wait_for(std::chrono::seconds{7})==std::future_status::ready);const auto closeResult=closing.projectBuildPending->get();assert(closeResult.cancelled&&!closeResult.succeeded&&!closeResult.log_path.empty()&&read(closeResult.log_path).find("cancelled=1")!=std::string::npos);assert(!platform::child_process::active_in_group("epoch.project.build")&&read(first.log_path)==original);
 for(auto&task:ownedTasks){assert(task.wait_for(std::chrono::seconds{2})==std::future_status::ready);try{assert(task.get()==1);}catch(const editor_tasks::TaskCancelled&){}}
 (void)closing.taskScheduler->cancel(aiTask.cancellation);closing.taskScheduler.reset();assert(aiTask.completion.get()==11&&aiToken.stop_requested());assert(std::chrono::steady_clock::now()-closed<std::chrono::seconds{8});
 EditorState evidence;evidence.projectRoot=root.string();evidence.projectBuildConfiguration="Release";evidence.taskScheduler=std::make_unique<editor_tasks::Scheduler>(editor_tasks::SchedulerLimits{.worker_count=1,.retained_finished_nodes=32}); // Legacy owner reads the admitted manifest configuration, not stale view state.
 start_ai_continuous_project_build(evidence,"fixture",root/"paths.txt","close regression",true);assert(evidence.aiContinuousBuildPending&&evidence.aiContinuousBuildCancellation.valid());
 const auto evidenceStarted=std::chrono::steady_clock::now()+std::chrono::seconds{5};while(!platform::child_process::active_in_group("epoch.project.build")&&std::chrono::steady_clock::now()<evidenceStarted)std::this_thread::sleep_for(std::chrono::milliseconds{2});assert(platform::child_process::active_in_group("epoch.project.build"));
 request_editor_task_session_stop(evidence);evidence.taskScheduler.reset();assert(evidence.aiContinuousBuildPending->wait_for(std::chrono::seconds{0})==std::future_status::ready);const auto evidenceResult=evidence.aiContinuousBuildPending->get();assert(evidenceResult.cancelled&&!evidenceResult.log_path.empty()&&!platform::child_process::active_in_group("epoch.project.build")&&read(first.log_path)==original);
 EditorState editor;editor.projectRoot=editor.projectBuildProjectRoot=root.string();assert(editor.projectLifecycle.select_project(11));assert(editor.projectLifecycle.record_scene_commit(1));assert(editor.projectLifecycle.record_shell_materialized());assert(editor.projectLifecycle.observe_build_inputs(123));editor.projectBuildLifecycleGeneration=editor.projectLifecycle.begin_build();editor.projectBuildProjectKey=11;retain_project_build_transcript(editor,failed);assert(project_build_transcript_for_display(editor)==failed.log_path);editor.projectBuildConfiguration="Release";assert(project_build_transcript_for_display(editor)!=failed.log_path);editor.projectBuildConfiguration="Debug";assert(editor.projectLifecycle.select_project(12));assert(project_build_transcript_for_display(editor)!=failed.log_path);const auto saved=editor.projectBuildTranscript.path;retain_project_build_transcript(editor,cancelled);assert(editor.projectBuildTranscript.path==saved);
 std::ofstream receipt("result.json");receipt<<"{\"real_child_supervisor\":true,\"unique_attempts\":true,\"quoted_preflight_stderr\":true,\"failure_exit7\":true,\"owned_cancel\":true,\"duplicate_no_reservation\":true,\"project_configuration_binding\":true,\"common_close_build_cancellation\":true,\"future_and_transcript_retained\":true,\"all_owned_common_tickets_cancelled\":true,\"continuous_build_actual_child_cancelled\":true,\"other_context_and_private_AI_ticket_untouched\":true}";
}
'''.replace("SUPPORTED", supported).replace("RESULT", result).replace("HELPERS", helpers).replace("WORKER", worker).replace("DISPLAY", display).replace("CANCEL_BUILD", cancel_build).replace("COMMON_STOP", common_stop).replace("CONTINUOUS_BUILD",continuous_build)
        self.compile_and_run("immutable-transcripts", code)

    def test_actual_scheduler_elapsed_advances_without_clock_change(self):
        runtime = (ROOT / "Engine/src/editor/application/editor.application_project_runtime.inl").read_text(encoding="utf-8")
        functions = "\n".join(definition(runtime, marker) for marker in (
            "        [[nodiscard]] std::optional<std::string> project_build_task_progress(",
            "        void refresh_project_build_progress("))
        code = r'''
#include <chrono>
#include <cstdint>
#include <fstream>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
import editor.task_scheduler;
#undef NDEBUG
#include <cassert>
namespace taskgraph=epochengine::taskgraph;
namespace editor_tasks=epochengine::editor_tasks;
struct EditorState {std::unique_ptr<editor_tasks::Scheduler> taskScheduler{std::make_unique<editor_tasks::Scheduler>(editor_tasks::SchedulerLimits{.worker_count=1,.retained_finished_nodes=32})};editor_tasks::CancellationTicket projectBuildCancellation{};std::chrono::steady_clock::time_point projectBuildProgressSample{};std::string projectBuildStatus;bool selected{true};};
bool project_build_targets_selected_project(const EditorState&e){return e.selected;}
FUNCTIONS
int main(){
 EditorState e;std::promise<void> started;auto entered=started.get_future();
 auto submitted=e.taskScheduler->submit_cancellable("Project Build",[&](std::stop_token stop){started.set_value();while(!stop.stop_requested())std::this_thread::sleep_for(std::chrono::milliseconds{1});return 1;});e.projectBuildCancellation=submitted.cancellation;
 assert(entered.wait_for(std::chrono::seconds{3})==std::future_status::ready);refresh_project_build_progress(e);const auto first=e.projectBuildStatus;
 bool valid{};for(const auto&n:e.taskScheduler->snapshot().nodes)if(n.id==e.projectBuildCancellation.node.id)valid=n.wasStarted&&n.startedNanoseconds>0;assert(valid);assert(first.find("0s elapsed")!=std::string::npos);
 std::this_thread::sleep_for(std::chrono::milliseconds{1200});refresh_project_build_progress(e);const auto second=e.projectBuildStatus;assert(second.find("1s elapsed")!=std::string::npos);
 (void)e.taskScheduler->cancel(e.projectBuildCancellation);submitted.completion.wait();e.projectBuildProgressSample={};refresh_project_build_progress(e);assert(e.projectBuildStatus.starts_with("Cancelling"));
 std::ofstream f("result.json");f<<"{\"actual_scheduler\":true,\"started_timestamp_valid\":true,\"first\":\""<<first<<"\",\"second\":\""<<second<<"\",\"cancel\":\""<<e.projectBuildStatus<<"\",\"observed_stuck_zero_reproduced\":false}";
}
'''.replace("FUNCTIONS", functions)
        self.compile_and_run("actual-scheduler-elapsed", code)


if __name__ == "__main__":
    unittest.main()
