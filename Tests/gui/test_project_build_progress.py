"""Actual task-backed project status; CPU only, no file log or Engine build."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import msvc_2022_toolsets, windows_sdk_includes

ROOT = Path(__file__).resolve().parents[2]


class ProjectBuildProgress(unittest.TestCase):
    def test_ticket_identity_cancel_elapsed_throttle_and_selected_owner(self):
        graph = (ROOT / 'Engine/modules/taskgraph/taskgraph.dotsystem.ixx').read_text(encoding='utf-8')
        source = (ROOT / 'Engine/src/editor/application/editor.application_project_runtime.inl').read_text(encoding='utf-8')
        types = '\n'.join(definition(graph, marker, True) for marker in (
            '    enum class NodeState', '    struct NodeHandle final', '    struct NodeDiagnostic'))
        functions = '\n'.join(definition(source, marker) for marker in (
            '        [[nodiscard]] std::optional<std::string> project_build_task_progress(',
            '        void refresh_project_build_progress('))
        cpp = r'''
#include <cassert>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
namespace taskgraph {
TYPES
struct GraphSnapshot { std::vector<NodeDiagnostic> nodes; };
}
struct Ticket { std::uint64_t scheduler_id{1};taskgraph::NodeHandle node{7};
 bool valid()const{return scheduler_id!=0&&node.valid();} };
struct Scheduler {taskgraph::GraphSnapshot graph;unsigned samples{};
 taskgraph::GraphSnapshot snapshot(){++samples;return graph;} };
struct EditorState {std::unique_ptr<Scheduler> taskScheduler{std::make_unique<Scheduler>()};
 Ticket projectBuildCancellation{};std::chrono::steady_clock::time_point projectBuildProgressSample{};
 bool selected{true},projectBuildReusesReviewedArtifact{};std::string projectBuildStatus{"queued"};};
bool project_build_targets_selected_project(const EditorState& e){return e.selected;}
FUNCTIONS
int main(){
 taskgraph::GraphSnapshot g;
 taskgraph::NodeDiagnostic other;other.id=8;other.label="Other work";other.state=taskgraph::NodeState::running;
 taskgraph::NodeDiagnostic build;build.id=7;build.label="Project Build";build.state=taskgraph::NodeState::queued;
 g.nodes={other,build};
 assert(!project_build_task_progress(g,{},0));assert(!project_build_task_progress(g,{99},0));
 assert(project_build_task_progress(g,{7},0)->find("queued")!=std::string::npos);
 g.nodes[1].state=taskgraph::NodeState::running;g.nodes[1].wasStarted=true;g.nodes[1].startedNanoseconds=2'000'000'000ull;
 assert(project_build_task_progress(g,{7},9'000'000'000ull)->find("7s elapsed")!=std::string::npos);
 assert(project_build_task_progress(g,{7},1)->find("0s elapsed")!=std::string::npos);
 g.nodes[1].cancellationRequested=true;
 assert(project_build_task_progress(g,{7},9'000'000'000ull)->starts_with("Cancelling"));
 g.nodes[1].state=taskgraph::NodeState::completed;
 assert(project_build_task_progress(g,{7},9'000'000'000ull)->starts_with("Cancelling"));
 g.nodes[1].cancellationRequested=false;
 assert(project_build_task_progress(g,{7},9'000'000'000ull)->starts_with("Finishing"));
 g.nodes[1].label="Another build";assert(!project_build_task_progress(g,{7},0));g.nodes[1].label="Project Build";
 EditorState e;e.taskScheduler->graph=g;
 refresh_project_build_progress(e);assert(e.taskScheduler->samples==1&&e.projectBuildStatus.starts_with("Finishing"));
 refresh_project_build_progress(e);assert(e.taskScheduler->samples==1);
 e.projectBuildProgressSample=std::chrono::steady_clock::now()-std::chrono::seconds{1};
 e.taskScheduler->graph.nodes[1].cancellationRequested=true;refresh_project_build_progress(e);
 assert(e.taskScheduler->samples==2&&e.projectBuildStatus.starts_with("Cancelling"));
 e.projectBuildProgressSample={};e.selected=false;e.projectBuildStatus="keep cancellation";
 refresh_project_build_progress(e);assert(e.taskScheduler->samples==2&&e.projectBuildStatus=="keep cancellation");
 e.selected=true;e.projectBuildCancellation.scheduler_id=0;refresh_project_build_progress(e);assert(e.taskScheduler->samples==2);
}
'''.replace('TYPES', types).replace('FUNCTIONS', functions)
        parent = ROOT / 'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='project-build-progress-', dir=parent))
        try:
            path = scratch / 'contract.cpp'
            path.write_text(cpp, encoding='utf-8')
            if os.name == 'nt':
                tools = msvc_2022_toolsets()
                sdk = Path('C:/Program Files (x86)/Windows Kits/10')
                kits = windows_sdk_includes(sdk)
                if not tools or not kits:
                    self.skipTest('Installed MSVC/SDK required')
                tool, kit = tools[-1], kits[-1]
                binary = scratch / 'contract.exe'
                command = [str(tool / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/W4', '/WX',
                           '/I' + str(tool / 'include'), '/I' + str(kit / 'ucrt'),
                           '/Fo' + str(scratch / 'contract.obj'), '/Fe' + str(binary), str(path), '/link',
                           '/LIBPATH:' + str(tool / 'lib/x64'), '/LIBPATH:' + str(sdk / 'Lib' / kit.name / 'um/x64'),
                           '/LIBPATH:' + str(sdk / 'Lib' / kit.name / 'ucrt/x64')]
            else:
                compiler = shutil.which('clang++') or shutil.which('g++')
                if not compiler:
                    self.skipTest('C++23 compiler required')
                binary = scratch / 'contract'
                command = [compiler, '-std=c++23', str(path), '-o', str(binary)]
            built = subprocess.run(command, cwd=scratch, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=10)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)


if __name__ == '__main__':
    unittest.main()


class ProjectBuildCancellation(unittest.TestCase):
    def test_actual_owned_cancel_without_process_launch(self):
        from Tests.support.cpp import compile_and_run
        owner = (ROOT / 'Engine/src/editor/application/editor.application_project_runtime.inl').read_text(encoding='utf-8')
        function = definition(owner, '        [[nodiscard]] bool cancel_project_build(')
        code = r"""
#include <memory>
#include <string>
namespace taskgraph {enum class CancelCode {cancelled,requested,already_terminal,stale};}
struct Ticket { bool active{true}; bool valid()const{return active;} };
struct Scheduler {unsigned calls{}; taskgraph::CancelCode result{taskgraph::CancelCode::requested};
 taskgraph::CancelCode cancel(const Ticket&){++calls;return result;} };
struct EditorState {bool projectBuildPending{true},projectBuildRunAfterBuild{true},projectBuildReusesReviewedArtifact{};
 Ticket projectBuildCancellation;std::shared_ptr<Scheduler>taskScheduler{std::make_shared<Scheduler>()};std::string projectBuildStatus;unsigned logs{};};
void push_editor_log(EditorState&e,const std::string&){++e.logs;}
""" + function + r"""
int main(){
 for(auto result:{taskgraph::CancelCode::cancelled,taskgraph::CancelCode::requested}){
  EditorState e;e.taskScheduler->result=result;
  if(!cancel_project_build(e)||e.projectBuildRunAfterBuild||e.taskScheduler->calls!=1||e.logs!=1)return 1;
 }
 for(auto result:{taskgraph::CancelCode::already_terminal,taskgraph::CancelCode::stale}){
  EditorState e;e.taskScheduler->result=result;
  if(cancel_project_build(e)||!e.projectBuildRunAfterBuild||e.taskScheduler->calls!=1)return 2;
 }
 EditorState e;e.projectBuildPending=false;
 if(cancel_project_build(e)||e.taskScheduler->calls)return 3;
 e.projectBuildPending=true;e.projectBuildCancellation.active=false;
 if(cancel_project_build(e)||e.taskScheduler->calls)return 4;
 e.projectBuildCancellation.active=true;e.taskScheduler.reset();if(cancel_project_build(e))return 5;
 return 0;
}
"""
        report=ROOT/'Artifacts/Reports/project-build-cancel-cpu'
        report.mkdir(parents=True,exist_ok=True)
        source=report/'cancel.cpp';source.write_text(code,encoding='utf-8')
        compile_and_run(self,source,report)
