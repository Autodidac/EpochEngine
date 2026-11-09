"""Actual GUI queue/drag adapter and compiled portable scrollbar geometry."""
from pathlib import Path
import hashlib,json,os,subprocess,time,unittest
from Tests.gui.test_gui_queued_drag import fixture as queue_fixture
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]
SCROLL=ROOT/'Engine/src/epochgui/controls/gui.scroll_controls.inl'
GUI=ROOT/'Engine/src/epochgui/gui.engine.cpp'

def fixture(helper=None,state=None):
    base=queue_fixture();base=base[:base.index('void fast_sequences(')]
    source=SCROLL.read_text(encoding='utf-8')
    if helper is None:helper=definition(source,'    [[nodiscard]] static bool update_scrollbar_drag(')
    if state is None:state=definition(GUI.read_text(encoding='utf-8'),'        struct ScrollAreaState',True)
    return base+'\nnamespace gui {\n'+state+'\n'+helper+r'''
bool scroll(ScrollAreaState& state){const auto layout=gui_lib::make_scrollbar_layout({.track={{100,10},{10,100}},.content_extent=1000,.scroll_offset=state.scrollY});return update_scrollbar_drag(state.scrollbarDrag,state.dragGrabOffset,layout,state.scrollY);}
}
int main(){
 try {
  auto ctx=std::make_shared<core::Context>();fresh(ctx);ScrollAreaState state;
  tick(ctx,{event(EventType::MouseDown,105,15),event(EventType::MouseMove,105,85),event(EventType::MouseUp,105,85)});
  CHECK(scroll(state)&&state.scrollY>700&&!state.scrollbarDrag.pending);
  CHECK(g_frame.mousePressConsumed&&g_frame.mouseReleaseConsumed);
  fresh(ctx);state={};tick(ctx,{event(EventType::MouseDown,105,15)},false);CHECK(scroll(state)&&state.scrollbarDrag.pending);
  tick(ctx,{event(EventType::MouseMove,105,50)},false);CHECK(scroll(state)&&state.scrollY>350&&state.scrollbarDrag.pending);
  tick(ctx,{event(EventType::MouseMove,120,180)},false);CHECK(scroll(state)&&state.scrollY==900&&state.scrollbarDrag.pending);
  tick(ctx,{event(EventType::MouseUp,120,180)},true);CHECK(scroll(state)&&!state.scrollbarDrag.pending);
  const auto final=state.scrollY;tick(ctx,{},true);CHECK(!scroll(state)&&state.scrollY==final);
  // A track click keeps center-on-click behavior, not a thumb-top jump.
  fresh(ctx);state={};tick(ctx,{event(EventType::MouseDown,105,70),event(EventType::MouseUp,105,70)});
  CHECK(scroll(state)&&state.scrollY>500&&state.scrollY<600);
  // Starting outside never acquires the scrollbar, even if release lands in it.
  fresh(ctx);state={};tick(ctx,{event(EventType::MouseDown,50,15),event(EventType::MouseMove,105,85),event(EventType::MouseUp,105,85)});
  CHECK(!scroll(state)&&state.scrollY==0&&!g_frame.mousePressConsumed);
  // Focus cancellation retires the held gesture; no stale sampler restarts it.
  fresh(ctx);state={};tick(ctx,{event(EventType::MouseDown,105,15)});CHECK(scroll(state));
  tick(ctx,{event(EventType::FocusLost,105,15)});CHECK(!scroll(state)&&!state.scrollbarDrag.pending);
  tick(ctx,{},true);CHECK(!scroll(state));
  // Contexts and independent scrollbar state cannot share a captured origin.
  auto other=std::make_shared<core::Context>();ScrollAreaState second;
  fresh(ctx);state={};tick(ctx,{event(EventType::MouseDown,105,15)});CHECK(scroll(state));
  fresh(other);tick(other,{event(EventType::MouseDown,105,20)});CHECK(scroll(second));
  tick(ctx,{event(EventType::MouseMove,105,60)});CHECK(scroll(state)&&state.scrollY>450&&second.scrollY==0);
  tick(other,{event(EventType::MouseUp,105,40)});CHECK(scroll(second)&&second.scrollY>200&&second.scrollY<250);
  CHECK(state.scrollbarDrag.pending&&!second.scrollbarDrag.pending);
  return 0;
 } catch (...) {return 89;}
}
'''

def prove(code,negative=False):
    tag=('before-' if negative else 'after-')+hashlib.sha256(code.encode()).hexdigest()[:12]
    proof=ROOT/'Artifacts/Reports/gui-scrollbar-drag-oct29'/tag;proof.mkdir(parents=True,exist_ok=True)
    cpp=proof/'gui.scrollbar_contract.cpp';cpp.write_text(code,encoding='utf-8')
    build=ROOT/'Artifacts/Intermediate/tests/gui-scrollbar-drag'/tag;portable=ROOT/'Engine/dep/EpochGui'
    (proof/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
project(ScrollbarDrag LANGUAGES CXX)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
add_executable(scrollbar "{cpp.as_posix()}" "{(portable/'src/epochgui/input.cpp').as_posix()}" "{(portable/'src/epochgui/gui.scrollbar_layout.cpp').as_posix()}")
target_compile_features(scrollbar PRIVATE cxx_std_23)
target_compile_options(scrollbar PRIVATE /utf-8 /EHsc)
target_sources(scrollbar PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{portable.as_posix()}" FILES "{(portable/'modules/epoch.gui.ixx').as_posix()}" "{(portable/'modules/epoch.gui.input.ixx').as_posix()}")
''',encoding='utf-8')
    commands=[['cmake','-S',str(proof),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64','-T','host=x64']]
    for config in (('Debug',) if negative else ('Debug','Release')):commands += [['cmake','--build',str(build),'--config',config,'--parallel','1'],[str(build/config/'scrollbar.exe')]]
    results=[]
    for i,command in enumerate(commands):
        run=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=100)
        (proof/f'lane-{i}.log').write_text(run.stdout+run.stderr,encoding='utf-8');results.append({'command':command,'exit':run.returncode})
        expected=81 if negative and i==len(commands)-1 else 0
        if run.returncode!=expected:raise AssertionError(f'{proof}: expected {expected}, got {run.returncode}\n'+run.stdout[-6000:]+run.stderr)
    (proof/'result.json').write_text(json.dumps({'passed':True,'before_failure_reproduced':negative,'commands':results,'scope':'Actual queue/filter/frame/drag and portable scrollbar geometry, recorded native input only; no window/GPU. Normal process return, no assert/abort.'},indent=2),encoding='utf-8')
    return proof

class ScrollbarDrag(unittest.TestCase):
    def test_real_ordered_scrollbar_drag_debug_release(self):
        if os.name!='nt':self.skipTest('Installed MSVC CPU lane')
        prove(fixture())
        source=SCROLL.read_text(encoding='utf-8')
        self.assertEqual(source.count('update_scrollbar_drag(state.scrollbarDrag, state.dragGrabOffset, scrollbar, state.scrollY)'),2)
        self.assertNotIn('state.draggingScrollbar',source)

if __name__=='__main__':unittest.main()
