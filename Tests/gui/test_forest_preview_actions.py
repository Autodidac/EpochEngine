"""Compile Forest Factory's actual action block; no window or renderer starts."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ForestPreviewActions(unittest.TestCase):
    def test_inline_actions_require_project_and_advanced_lab_is_explicit(self):
        viewport = (ROOT/'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
        start = viewport.index('                const std::array<gui::InlineButtonSpec, 5> actions')
        end = viewport.index('                gui::wrapped_label(editor.plantLabStatus', start)
        actual = viewport[start:end]
        source = r'''
#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
enum class EditorMainSurface { Scene,PlantLab };
enum class EditorCommand { None,OpenContextWindow };
struct Context { int type=7; };
struct Result { EditorCommand command{};std::string command_argument;int requested_context_type{}; };
struct Editor { bool project;int plantLabDocument{}; };
bool has_open_project(const Editor& value) { return value.project; }
namespace forest {
inline unsigned undoCalls{},redoCalls{};
bool undo_forest_profile_edit(int&) {++undoCalls;return true;}
bool redo_forest_profile_edit(int&) {++redoCalls;return true;}
}
namespace gui {
struct InlineButtonSpec { std::string_view label;float width;bool enabled=true; };
inline std::size_t requested;
inline std::array<InlineButtonSpec,5> seen;
std::optional<std::size_t> inline_button_row(const std::array<InlineButtonSpec,5>& specs,float,float) {
 seen=specs;return specs[requested].enabled ? std::optional{requested}:std::nullopt;
}
}
void run(bool project,bool ready,std::size_t action) {
 Editor editor{project};auto ctx=std::make_unique<Context>();Result result;
 unsigned placementCalls{},saveCalls{},focusCalls{},surfaceCalls{},refreshCalls{};
 forest::undoCalls=forest::redoCalls=0;
 EditorMainSurface surface=EditorMainSurface::Scene;
 gui::requested=action;
 auto place_forest_factory_asset=[&](Editor&) { ++placementCalls;return std::size_t{1}; };
 auto save_editor_scene_snapshot=[&](Editor&) { ++saveCalls;return true; };
 auto ensure_forest_factory_preview=[&](Editor&) {++refreshCalls;return true;};
 auto apply_editor_surface=[&](EditorMainSurface value,std::string_view) { ++surfaceCalls;surface=value; };
 auto handle_scene_tool=[&](Editor&,Context*,std::string_view command) { assert(command=="focus_selection");++focusCalls; };
ACTUAL
 assert(gui::seen[0].enabled==ready&&gui::seen[1].enabled==ready);
 assert(gui::seen[2].enabled==(project&&ready));
 assert(gui::seen[3].enabled&&gui::seen[4].enabled==ready);
 if(!gui::seen[action].enabled) {
  assert(placementCalls==0&&saveCalls==0&&surfaceCalls==0&&focusCalls==0);return;
 }
 if(action==0)assert(forest::undoCalls==1&&refreshCalls==1&&placementCalls==0&&surfaceCalls==0);
 if(action==1)assert(forest::redoCalls==1&&refreshCalls==1&&placementCalls==0&&surfaceCalls==0);
 if(action==2)assert(placementCalls==1&&saveCalls==1&&surfaceCalls==1&&surface==EditorMainSurface::Scene&&focusCalls==1);
 if(action==3)assert(placementCalls==0&&saveCalls==0&&surfaceCalls==1&&surface==EditorMainSurface::Scene&&focusCalls==0);
 if(action==4)assert(placementCalls==0&&saveCalls==0&&surfaceCalls==0&&focusCalls==0
  &&result.command==EditorCommand::OpenContextWindow&&result.command_argument=="application.plant_lab"&&result.requested_context_type==7);
}
int main() {
 for(bool project:{false,true})for(bool ready:{false,true})
  for(std::size_t action:{0u,1u,2u,3u,4u})run(project,ready,action);
}
'''.replace('ACTUAL', actual)
        parent = ROOT/'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='forest-preview-actions-', dir=parent))
        try:
            cpp, binary = scratch/'contract.cpp', scratch/'contract'
            cpp.write_text(source, encoding='utf-8')
            if os.name == 'nt':
                tools = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
                sdk = Path('C:/Program Files (x86)/Windows Kits/10')
                kits = sorted((sdk/'Include').glob('*'))
                if not tools or not kits:
                    self.skipTest('MSVC/SDK required')
                tool, kit = tools[-1], kits[-1]
                binary = scratch/'contract.exe'
                args = [str(tool/'bin/Hostx64/x64/cl.exe'), '/nologo', '/utf-8', '/std:c++latest', '/EHsc',
                        '/I'+str(tool/'include'), '/I'+str(kit/'ucrt'),
                        '/Fo'+str(scratch/'contract.obj'), '/Fe'+str(binary), str(cpp), '/link',
                        '/LIBPATH:'+str(tool/'lib/x64'), '/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),
                        '/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
            else:
                compiler = shutil.which('clang++') or shutil.which('g++')
                if not compiler:
                    self.skipTest('C++23 compiler required')
                args = [compiler, '-std=c++23', str(cpp), '-o', str(binary)]
            compiled = subprocess.run(args, cwd=scratch, capture_output=True, text=True, timeout=45)
            self.assertEqual(compiled.returncode, 0, compiled.stdout[-6000:]+compiled.stderr)
            result = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)
