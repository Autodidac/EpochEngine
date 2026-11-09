"""Canonical Run keeps planetary provider admission after removing Explore UI."""
from pathlib import Path
import hashlib
import json
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
APP = ROOT / 'Engine/src/editor/application'


class PlanetExploreBridge(unittest.TestCase):
    def test_actual_admission_and_shared_play_route(self):
        paths = [APP / 'editor.frame_inspector.inl', APP / 'editor.frame_project_runtime.inl']
        inspector, runtime = [path.read_text(encoding='utf-8') for path in paths]
        self.assertNotIn('Explore Planet', inspector)
        self.assertNotIn('planet_exploration_ready', inspector)
        play = definition(runtime, '        auto play_active_context =', True)
        guard = definition(play, '            if(!requirement.ok)')
        required = definition(play, '            if(requirement.requires_project_runtime)')
        code = r'''
#include <filesystem>
#include <string>
#include <vector>
namespace project_planetary {
struct Requirement {bool ok{true},requires_project_runtime{true};std::string message{"admitted compiled provider"};};
Requirement required;std::filesystem::path observedRoot;
Requirement runtime_requirement(const std::filesystem::path& root){observedRoot=root;return required;}
}
struct EditorState {std::string projectRoot{"project"},projectStatus,projectBuildStatus;
 bool sceneRunFullscreenInEditor{},sceneRunMaximizedInEditor{};};
unsigned launches{},fallbacks{};bool embeddedLaunch{};std::vector<std::string> logs;
void push_editor_log(EditorState&,std::string message){logs.push_back(message);}
void invoke(EditorState& editor){
 auto launch_active_project_context=[&](bool inEditor=false){++launches;embeddedLaunch=inEditor;};
 const auto requirement=project_planetary::runtime_requirement(std::filesystem::path{editor.projectRoot});
GUARD
REQUIRED
 ++fallbacks;
}
void reset(EditorState& editor){editor={};launches=fallbacks=0;embeddedLaunch=false;logs.clear();project_planetary::required={};}
#define CHECK(condition) do {if(!(condition))return __LINE__;}while(false)
int main(){
 EditorState editor;reset(editor);invoke(editor);
 CHECK(launches==1&&embeddedLaunch&&fallbacks==0);
 CHECK(editor.projectStatus==project_planetary::required.message&&project_planetary::observedRoot=="project");
 CHECK(logs.size()==1);
 // A failed provider admission cannot fall through to the ordinary preview.
 reset(editor);project_planetary::required.ok=false;invoke(editor);
 CHECK(launches==0&&fallbacks==0&&editor.projectBuildStatus==project_planetary::required.message);
 CHECK(editor.projectStatus==editor.projectBuildStatus&&logs.size()==1);
 // Non-planetary projects keep their ordinary Run path.
 reset(editor);project_planetary::required.requires_project_runtime=false;invoke(editor);
 CHECK(launches==0&&fallbacks==1&&logs.empty());
 reset(editor);editor.sceneRunFullscreenInEditor=true;editor.sceneRunMaximizedInEditor=true;invoke(editor);
 CHECK(embeddedLaunch&&editor.sceneRunFullscreenInEditor&&editor.sceneRunMaximizedInEditor);
}
'''.replace('GUARD', guard).replace('REQUIRED', required)
        identity = hashlib.sha256(code.encode()).hexdigest()[:12]
        report = ROOT / 'Artifacts/Reports/planet-explore-bridge-nov01' / identity
        report.mkdir(parents=True, exist_ok=True)
        cpp = report / 'consumer.cpp'
        cpp.write_text(code, encoding='utf-8')
        runs = compile_and_run(self, cpp, report)
        (report / 'result.json').write_text(json.dumps({
            'passed': True, 'runs': runs,
            'sources': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths},
            'scope': 'Actual canonical Run admission and compiled-provider branch; failure cannot launch or fall through. Inspector Explore action absent. Provider and launch are recorded ports; no native acceptance.'
        }, indent=2) + '\n', encoding='utf-8')

    def test_assistant_run_uses_selected_gui_target(self):
        owner = APP / 'editor.frame_project_runtime.inl'
        protocol_owner = ROOT / 'Engine/modules/ai.mcp.ixx'
        source = owner.read_text(encoding='utf-8')
        branch = definition(source, '            else if (call.tool == "project.run")')
        branch = branch.replace('else if', 'if', 1)
        protocol = definition(protocol_owner.read_text(encoding='utf-8'),
                              '    [[nodiscard]] inline std::string tool_plan_protocol_prompt()')
        code = r'''
#include <memory>
#include <string>
PROTOCOL
struct Snapshot {bool running{},identity{true};bool active()const{return running;}};
struct EditorState {bool sceneRunFullscreenInEditor{true},projectBuildPending{};
 std::string aiToolStatus,projectBuildStatus{"Build accepted"},projectRuntimeStatus{"No runtime"};std::shared_ptr<Snapshot> projectRuntimeSnapshot;};
bool project_runtime_identity_current(const EditorState&,const Snapshot& snapshot){return snapshot.identity;}
bool allowLaunch=true,requestedEmbedding{};unsigned launches{},watches{};
bool invoke(EditorState& editor){
 struct {std::string tool{"project.run"};}call;
 auto launch_active_project_context=[&](bool inEditor=false){++launches;requestedEmbedding=inEditor;editor.projectBuildPending=allowLaunch;};
 auto watch_execution=[&](const char*){++watches;};
BRANCH
 return true;
}
#define CHECK(condition) do {if(!(condition))return __LINE__;}while(false)
int main(){
 const auto instructions=tool_plan_protocol_prompt();
 CHECK(instructions.find("current Run destination: In Editor or Separate Window")!=std::string::npos);
 CHECK(instructions.find("project.run takes no arguments.")!=std::string::npos);
 CHECK(instructions.find("compiled standalone runtime")==std::string::npos);
 for(bool target:{true,false}){
  EditorState editor;editor.sceneRunFullscreenInEditor=target;launches=watches=0;allowLaunch=true;
  CHECK(invoke(editor)&&launches==1&&watches==1&&requestedEmbedding==target);
  CHECK(editor.sceneRunFullscreenInEditor==target);
  CHECK(editor.aiToolStatus.find(target?"in-editor":"standalone")!=std::string::npos);
 }
 EditorState refused;launches=watches=0;allowLaunch=false;
 CHECK(!invoke(refused)&&launches==1&&watches==0&&!refused.projectBuildPending);
}
'''.replace('BRANCH', branch).replace('PROTOCOL', protocol)
        identity = hashlib.sha256(code.encode()).hexdigest()[:12]
        report = ROOT / 'Artifacts/Reports/assistant-run-target-nov06' / identity
        report.mkdir(parents=True, exist_ok=True)
        cpp = report / 'consumer.cpp'
        cpp.write_text(code, encoding='utf-8')
        runs = compile_and_run(self, cpp, report)
        (report / 'result.json').write_text(json.dumps({
            'passed': True, 'runs': runs,
            'owner_sha256': hashlib.sha256(owner.read_bytes()).hexdigest(),
            'protocol_sha256': hashlib.sha256(protocol_owner.read_bytes()).hexdigest(),
            'scope': 'Actual tool protocol and Assistant project.run branch agree on the current GUI Run destination, with no model-selected destination argument; accepted work retains completion watching and refused launches do not wait. Recorded launch ports; no native runtime proof.'
        }, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
