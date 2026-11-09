"""Real ordered GUI keyboard owner + project adapter + RuntimeSession, CPU only."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.gui.test_gui_queued_drag import fixture as queue_fixture

ROOT = Path(__file__).resolve().parents[2]
GUI = ROOT / "Engine/src/epochgui"
ADAPTER = ROOT / "Engine/src/project/project.gui_epochgui.cpp"


def fixture():
    api = (ROOT / "Engine/modules/gui.engine.ixx").read_text(encoding="utf-8")
    text = (GUI / "controls/gui.text_controls.inl").read_text(encoding="utf-8")
    frame = (GUI / "controls/gui.frame_lifecycle.inl").read_text(encoding="utf-8")
    owner = (GUI / "gui.engine.cpp").read_text(encoding="utf-8")
    adapter = ADAPTER.read_text(encoding="utf-8")
    base = queue_fixture()
    base = base[:base.index("void fast_sequences(")]
    base = base.replace("#include <algorithm>", "#include <functional>\n#include <ranges>\n#include <unordered_map>\n#include <algorithm>")
    base = base.replace("const void* visitedActiveWidget{};", "const void* visitedActiveWidget{};std::string windowKey;")
    base = base.replace("import epoch.gui.input;", "import epoch.gui.input;\nimport epoch.gui;\nimport asset.gui_artifact;\nimport project.gui_runtime;")
    base = base.replace("void reset_frame(){", "bool testPopup{};\nvoid reset_frame(){")
    base = base.replace("bool any_select_box_open(const core::Context*){return false;}", "bool any_select_box_open(const core::Context*){return testPopup;}")
    # The actual scoped suppression swaps events and pointer state, not just a
    # test-only bool; restore the same fields as the production FrameState.
    base = base.replace("bool testPopup{};\nvoid reset_frame(){", "bool testPopup{};\nvoid reset_frame(){")
    base = base.replace("bool any_select_box_open(const core::Context*)", "bool testPopup{};\nbool any_select_box_open(const core::Context*)")
    base = base.replace("bool testPopup{};\nvoid reset_frame(){", "void reset_frame(){")
    declarations = "\n".join(definition(api, marker, True).replace("export ", "") for marker in (
        "    export struct KeyboardInputTarget", "    export struct KeyboardInputOptions",
        "    export class ScopedInputSuppression"))
    states = definition(owner, "        struct EditBoxTextState", True)
    key_helpers = text[text.index("    struct EditBoxEventResult final"):text.index("    EditBoxResult edit_box(")]
    scope = frame[frame.index("    ScopedInputSuppression::ScopedInputSuppression("):frame.index("    Vec2 mouse_position()")]
    ports = r"""
std::map<const void*,const void*> g_contextActiveWidgets;
STATES
std::map<const void*,EditBoxTextState> g_editBoxTextStates;
std::map<const void*,int> g_editBoxMenuStates;
std::map<const void*,bool> g_textFieldSelectAllStates;
constexpr float kFontScale=1,kBoxInnerPadding=4;
float base_line_height(float){return 12;}float line_advance_amount(float){return 14;}
float space_advance(float){return 8;}
std::string clipboard;
bool clipboard_write_text(std::string_view value){clipboard=value;return true;}
std::string clipboard_read_text(){return clipboard;}
gui_lib::TextControlMetrics edit_box_text_metrics(const gui_lib::TextControlState& control,
 float,float,float,float,bool){return {.content_size={float(control.text.size()*8),14},
 .caret_position={float(control.caret*8),0},.caret_size={1,12},.valid=true};}
""".replace("STATES", states)
    capture = definition(frame, "    bool keyboard_input_captured(")
    base = base.replace("}\nusing namespace gui;", declarations+ports+capture+key_helpers+scope+"\n}\nusing namespace gui;", 1)
    # A neutral draw port records the adapter's real per-widget call. All key,
    # text state, queue, admission, mutation and focus behavior above is real.
    base += r"""
namespace gui{
struct SpriteHandle{bool is_valid()const{return false;}};
struct ImageBoxResult{};struct ImageBoxOptions{std::string_view id;SpriteHandle sprite;Vec2 size,source_size;
 std::string_view caption;int fit{};bool interactive{},selected{},enabled{};};
struct SliderResult{bool changed{};float value{};};struct SliderOptions{std::string_view id,label;float minimum{},maximum{},value{},step{};Vec2 size;bool enabled{};};
struct EditBoxResult{bool changed{};};enum class TextInputKeyboardPolicy{focused};
struct ImageFit{static constexpr int Contain=1;};
struct ContentClipScope{Vec2 low=g_frame.contentMin,high=g_frame.contentMax;
 ContentClipScope(Vec2 a,Vec2 b){g_frame.contentMin=a;g_frame.contentMax=b;}
 ~ContentClipScope(){g_frame.contentMin=low;g_frame.contentMax=high;}};
unsigned selectedPaint{},stableFields{};
void begin_window(std::string_view,Vec2 position,Vec2 size,bool){g_frame.insideWindow=true;g_frame.contentMin=position;g_frame.contentMax={position.x+size.x,position.y+size.y};}
void end_window(){}void begin_top_layer(){}void end_top_layer(){}
void panel_rect(Vec2,Vec2){}void wrapped_label(std::string_view,float){}
bool button_selected(std::string_view,Vec2,bool focused){selectedPaint+=focused;return false;}
ImageBoxResult image_box(const ImageBoxOptions&){return {};}
SliderResult slider(const SliderOptions&){return {};}
EditBoxResult edit_box(std::string&,Vec2,std::size_t,bool,TextInputKeyboardPolicy,bool,std::string_view,bool stable){CHECK(stable);++stableFields;return {};}
Vec2 mouse_position(){return g_frame.mousePos;}bool was_mouse_released(){return left_release_available();}
}
namespace epochengine::project_gui{
using SpriteHandle=gui::SpriteHandle;
namespace project_gui_runtime=epochengine::project_gui_runtime;
namespace asset=epochengine::asset;
struct EpochGuiRenderResult{project_gui_runtime::RuntimeCode code{};bool pointer_captured{};unsigned visible_widgets{},resolved_images{};std::vector<project_gui_runtime::RuntimeEvent>events;};
class EpochGuiAdapter{public:
 struct ImageState{SpriteHandle sprite;unsigned width{},height{};};
 std::map<std::uint64_t,std::string>text_state_;std::map<std::string,ImageState>images_;
 bool valid()const{return true;}ImageState& resolve_image(std::string_view){static ImageState image;return image;}
 ~EpochGuiAdapter()noexcept;void clear()noexcept;
 void render_tabs(project_gui_runtime::RuntimeSession&,const project_gui_runtime::RuntimeFrame&){}
 project_gui_runtime::RuntimeCode dispatch_keyboard(project_gui_runtime::RuntimeSession&,const project_gui_runtime::RuntimeFrame&);
 void render_widget(project_gui_runtime::RuntimeSession&,const project_gui_runtime::WidgetView&);
 EpochGuiRenderResult render(project_gui_runtime::RuntimeSession&,project_gui_runtime::Float2)noexcept;
};
ADAPTER_HELPERS
ADAPTER_METHODS
}
namespace fixture{
namespace asset=epochengine::asset;
ARTIFACT_HELPERS
}
namespace rt=epochengine::project_gui_runtime;
using Adapter=epochengine::project_gui::EpochGuiAdapter;
using Kind=epochengine::asset::gui::WidgetKind;
"""
    helpers = "\n".join(definition(adapter, marker) for marker in (
        "        [[nodiscard]] gui::Vec2 point(", "        [[nodiscard]] gui::Vec2 extent(",
        "        [[nodiscard]] std::string widget_id(", "        [[nodiscard]] bool container("))
    methods = "\n".join(definition(adapter, marker) for marker in (
        "    EpochGuiAdapter::~EpochGuiAdapter() noexcept",
        "    void EpochGuiAdapter::clear() noexcept",
        "    EpochGuiRenderResult EpochGuiAdapter::render(",
        "    project_gui_runtime::RuntimeCode EpochGuiAdapter::dispatch_keyboard(",
        "    void EpochGuiAdapter::render_widget("))
    contract = (ROOT/"Engine/src/project/project.gui_runtime_contract.cpp").read_text(encoding="utf-8")
    artifact = "\n".join(definition(contract, marker) for marker in (
        "        [[nodiscard]] asset::gui::CompiledWidget widget(",
        "        [[nodiscard]] asset::gui::CompiledGuiArtifact artifact()"))
    base = base.replace("ADAPTER_HELPERS",helpers).replace("ADAPTER_METHODS",methods).replace("ARTIFACT_HELPERS",artifact)
    return base + CASES


CASES = r"""
InputEvent key(int code,bool shift=false,bool ctrl=false){return {.type=EventType::KeyDown,.key=code,.ctrl_down=ctrl,.shift_down=shift};}
InputEvent text(std::string value){return {.type=EventType::TextInput,.text=std::move(value)};}
auto source(){auto value=fixture::artifact();auto second=value.widgets[3];second.id={9};second.name="SecondField";second.primary_text="B";second.tab_index=2;
 value.widgets[4].tab_index=3;value.widgets[1].children.insert(value.widgets[1].children.begin()+2,8);value.widgets.push_back(second);
 value.identity.key=epochengine::asset::gui::payload_content(value);return value;}
auto view(rt::RuntimeSession& runtime,std::uint64_t id){auto frame=runtime.build_frame({800,600});for(const auto& entry:frame.widgets)if(entry.id.value==id)return entry;CHECK(false);return frame.widgets[0];}
void tick_keys(const std::shared_ptr<core::Context>& context,std::initializer_list<InputEvent> events,bool down=false){tick(context,events,down);g_frame.contentMin={0,0};g_frame.contentMax={800,600};}
void render(Adapter& adapter,rt::RuntimeSession& runtime){auto result=adapter.render(runtime,{800,600});CHECK(result.code==rt::RuntimeCode::ready||result.code==rt::RuntimeCode::no_change);}
int main(){
 auto context=std::make_shared<core::Context>();fresh(context);Adapter adapter;rt::RuntimeSession runtime{source()};CHECK(runtime.valid());
 // No GUI focus: Tab and Space remain available to scene/game input.
 tick_keys(context,{key(9),key(32)});render(adapter,runtime);CHECK(g_frame.events.size()==2&&!runtime.keyboard_captured());
 // Actual pointer release establishes canonical focus; its focused button
 // captures gameplay keys and paints selected through the existing control.
 auto button=view(runtime,3);const Vec2 buttonPoint{button.bounds.x+10,button.bounds.y+10};
 tick_keys(context,{event(EventType::MouseDown,buttonPoint.x,buttonPoint.y),event(EventType::MouseUp,buttonPoint.x,buttonPoint.y)});
 render(adapter,runtime);CHECK(runtime.focused_widget().value==3&&runtime.keyboard_captured()&&selectedPaint);
 tick_keys(context,{key(13),{.type=EventType::KeyUp,.key=13},text("\r")});auto activated=adapter.render(runtime,{800,600});
 CHECK(std::count_if(activated.events.begin(),activated.events.end(),[](const auto& event){return event.kind==rt::EventKind::activated;})==1);
 CHECK(runtime.focus({4})==rt::RuntimeCode::ready);(void)runtime.drain_events();
 tick_keys(context,{text("A"),key(9),text("\t"),text("C")});render(adapter,runtime);
 CHECK(view(runtime,4).primary_text=="EpochA"&&view(runtime,9).primary_text=="BC"&&runtime.focused_widget().value==9);
 // Reverse traversal is independent of paint order and retains UTF8 caret/undo.
 tick_keys(context,{text("\xC3\xA9"),key(9,true),text("D")});render(adapter,runtime);
 CHECK(view(runtime,9).primary_text=="BC\xC3\xA9"&&view(runtime,4).primary_text=="EpochAD");
 tick_keys(context,{key(8),key('Z',false,true)});render(adapter,runtime);CHECK(view(runtime,4).primary_text=="EpochAD");
 tick_keys(context,{key(9,true),key(9,true)});render(adapter,runtime);CHECK(runtime.focused_widget().value==5);
 tick_keys(context,{key(39),key(39)});render(adapter,runtime);CHECK(std::abs(view(runtime,5).value-0.7)<1e-12);
 // CtrlTab remains the containing group's event, never a text edit or action.
 tick_keys(context,{key(9,false,true)});render(adapter,runtime);CHECK(g_frame.events.size()==1&&runtime.focused_widget().value==5);
 // Suppression and modal/pop-up owners keep the whole keyboard channel.
 for(int gate=0;gate<4;++gate){CHECK(runtime.focus({4})==rt::RuntimeCode::ready||runtime.focused_widget().value==4);(void)runtime.drain_events();
  tick_keys(context,{text("X"),key(9)});auto before=std::string(view(runtime,4).primary_text);
  if(gate==0){ScopedInputSuppression suppressed;render(adapter,runtime);}
  if(gate==1){g_frame.modalInput.begin({{0,0},{800,600}});g_frame.modalInput.require_owner_scope();render(adapter,runtime);}
  if(gate==2){testPopup=true;render(adapter,runtime);testPopup=false;}
  if(gate==3){g_frame.menuPopupBlocksBackground=true;render(adapter,runtime);g_frame.menuPopupBlocksBackground=false;}
  CHECK(runtime.focused_widget().value==4&&view(runtime,4).primary_text==before&&g_frame.events.size()==2);}
 // Foreign shared text focus cannot be stolen by the project adapter.
 int foreign{};tick_keys(context,{text("X")});g_contextActiveWidgets[context.get()]=&foreign;render(adapter,runtime);
 CHECK(!runtime.keyboard_captured()&&g_contextActiveWidgets[context.get()]==&foreign&&g_frame.events.size()==1);
 g_contextActiveWidgets[context.get()]=nullptr;
 CHECK(runtime.focus({4})==rt::RuntimeCode::ready);(void)runtime.drain_events();
 tick_keys(context,{text("cancelled"),{.type=EventType::FocusLost}});render(adapter,runtime);CHECK(!runtime.keyboard_captured());
 CHECK(runtime.focus({4})==rt::RuntimeCode::ready);(void)runtime.drain_events();context->focus=false;tick_keys(context,{text("discard")});render(adapter,runtime);CHECK(!runtime.keyboard_captured());context->focus=true;
 // An admitted outside click clears GUI focus; gameplay recovers next frame.
 CHECK(runtime.focus({4})==rt::RuntimeCode::ready);(void)runtime.drain_events();
 tick_keys(context,{event(EventType::MouseDown,799,599)},true);render(adapter,runtime);CHECK(!runtime.keyboard_captured());
 // Context queues and existing text states remain distinct.
 auto other=std::make_shared<core::Context>();fresh(other);Adapter second;rt::RuntimeSession otherRuntime{source()};
 CHECK(otherRuntime.focus({4})==rt::RuntimeCode::ready);(void)otherRuntime.drain_events();
 push_input_for_context(context.get(),text("first"));tick_keys(other,{text("second")});render(second,otherRuntime);
 CHECK(view(otherRuntime,4).primary_text=="Epochsecond");
 CHECK(runtime.focus({4})==rt::RuntimeCode::ready);(void)runtime.drain_events();tick_keys(context,{});render(adapter,runtime);CHECK(view(runtime,4).primary_text=="EpochADfirst");
 // Replacement does not retain a hidden or stale GUI focus.
 CHECK(runtime.replace(source())==rt::RuntimeCode::ready);tick_keys(context,{key(32)});render(adapter,runtime);CHECK(!runtime.keyboard_captured()&&g_frame.events.size()==1&&g_contextActiveWidgets[context.get()]==nullptr);
 // Runtime activation and cancellation preserve budgets before mutation.
 rt::RuntimeLimits limits;limits.maximum_pending_events=1;rt::RuntimeSession bounded{source(),limits};
 CHECK(bounded.focus({3})==rt::RuntimeCode::ready);CHECK(bounded.activate_focused()==rt::RuntimeCode::action_limit_exceeded);
 CHECK(bounded.clear_focus()==rt::RuntimeCode::action_limit_exceeded&&bounded.focused_widget().value==3);
 (void)bounded.drain_events();CHECK(bounded.activate_focused()==rt::RuntimeCode::ready);auto actions=bounded.drain_events();CHECK(actions.size()==1&&actions[0].action=="game.start");
 CHECK(bounded.clear_focus()==rt::RuntimeCode::ready&&!bounded.keyboard_captured());
 auto disabled=source();disabled.widgets[2].enabled=false;disabled.identity.key=epochengine::asset::gui::payload_content(disabled);
 rt::RuntimeSession dis{disabled};CHECK(dis.focus({3})==rt::RuntimeCode::disabled_widget);
 CHECK(runtime.focus({4})==rt::RuntimeCode::ready);CHECK(runtime.activate_focused()==rt::RuntimeCode::wrong_widget_kind);
 // Literal multiline Tab, read-only clipboard and bounded rejected edits use
 // the existing text controller without changing Session state/history.
 for(int mode=0;mode<3;++mode){g_contextActiveWidgets[context.get()]=nullptr;
  auto inputSource=source();inputSource.widgets[3].multiline=true;inputSource.widgets[3].read_only=mode==1;
  inputSource.identity.key=epochengine::asset::gui::payload_content(inputSource);
  rt::RuntimeLimits inputLimits;if(mode==2)inputLimits.maximum_pending_events=1;
  rt::RuntimeSession inputRuntime{inputSource,inputLimits};Adapter inputAdapter;
  CHECK(inputRuntime.focus({4})==rt::RuntimeCode::ready);if(mode!=2)(void)inputRuntime.drain_events();
  if(mode==0){tick_keys(context,{key(9),text("X")});render(inputAdapter,inputRuntime);
   CHECK(inputRuntime.focused_widget().value==4&&view(inputRuntime,4).primary_text=="Epoch\tX");
   tick_keys(context,{key(9),text("\t"),text("Y")});render(inputAdapter,inputRuntime);
   CHECK(view(inputRuntime,4).primary_text=="Epoch\tX\tY");
   tick_keys(context,{key(9)});render(inputAdapter,inputRuntime);
   CHECK(view(inputRuntime,4).primary_text=="Epoch\tX\tY\t");
   tick_keys(context,{text("\t"),text("Z")});render(inputAdapter,inputRuntime);
   CHECK(view(inputRuntime,4).primary_text=="Epoch\tX\tY\tZ");
   tick_keys(context,{{.type=EventType::KeyUp,.key=9},text("\t")});render(inputAdapter,inputRuntime);
   CHECK(view(inputRuntime,4).primary_text=="Epoch\tX\tY\tZ\t");
   const auto beforeModifiers=std::string(view(inputRuntime,4).primary_text);
   tick_keys(context,{key(9,false,true)});render(inputAdapter,inputRuntime);
   CHECK(inputRuntime.focused_widget().value==4&&view(inputRuntime,4).primary_text==beforeModifiers&&g_frame.events.size()==1);
   tick_keys(context,{{.type=EventType::KeyDown,.key=9,.alt_down=true}});render(inputAdapter,inputRuntime);
   CHECK(inputRuntime.focused_widget().value==4&&view(inputRuntime,4).primary_text==beforeModifiers&&g_frame.events.size()==1);
   tick_keys(context,{key(9)});{ScopedInputSuppression suppressed;render(inputAdapter,inputRuntime);}
   CHECK(view(inputRuntime,4).primary_text==beforeModifiers&&g_frame.events.size()==1);
   tick_keys(context,{key(9)});render(inputAdapter,inputRuntime);const auto beforeFocusLoss=std::string(view(inputRuntime,4).primary_text);
   tick_keys(context,{{.type=EventType::FocusLost}});render(inputAdapter,inputRuntime);CHECK(!inputRuntime.keyboard_captured());
   CHECK(inputRuntime.focus({4})==rt::RuntimeCode::ready);(void)inputRuntime.drain_events();
   tick_keys(context,{text("\t")});render(inputAdapter,inputRuntime);CHECK(view(inputRuntime,4).primary_text==beforeFocusLoss+"\t");
   tick_keys(context,{key(9,true)});render(inputAdapter,inputRuntime);CHECK(inputRuntime.focused_widget().value==3);}
  if(mode==1){tick_keys(context,{key(9),text("\t"),key('A',false,true),text("X"),key('C',false,true)});render(inputAdapter,inputRuntime);
   CHECK(view(inputRuntime,4).primary_text=="Epoch"&&clipboard=="Epoch");}
  if(mode==2){tick_keys(context,{key(9)});auto rejected=inputAdapter.render(inputRuntime,{800,600});
   CHECK(rejected.code==rt::RuntimeCode::action_limit_exceeded&&view(inputRuntime,4).primary_text=="Epoch");
   tick_keys(context,{text("\t")});render(inputAdapter,inputRuntime);CHECK(view(inputRuntime,4).primary_text=="Epoch");}
 }
 auto hidden=source();hidden.widgets[3].visible=false;hidden.identity.key=epochengine::asset::gui::payload_content(hidden);
 rt::RuntimeSession hiddenRuntime{hidden};CHECK(hiddenRuntime.focus({4})==rt::RuntimeCode::hidden_widget);
 CHECK(hiddenRuntime.focus_next()==rt::RuntimeCode::ready&&hiddenRuntime.focused_widget().value==3);
 CHECK(hiddenRuntime.focus_next()==rt::RuntimeCode::ready&&hiddenRuntime.focused_widget().value==9);
 // Idle painting performs one layout build; there is no additional identity
 // vector allocation or blind focus/text/tab rebuild on every GUI frame.
 g_contextActiveWidgets[context.get()]=nullptr;Adapter idle;rt::RuntimeSession idleRuntime{source()};
 tick_keys(context,{});auto beforeFrames=idleRuntime.metrics().frames_built;render(idle,idleRuntime);
 CHECK(idleRuntime.metrics().frames_built==beforeFrames+1);
 CHECK(idleRuntime.focus({4})==rt::RuntimeCode::ready);(void)idleRuntime.drain_events();
 tick_keys(context,{});render(idle,idleRuntime);beforeFrames=idleRuntime.metrics().frames_built;
 tick_keys(context,{});render(idle,idleRuntime);CHECK(idleRuntime.metrics().frames_built==beforeFrames+1);
 const auto textIdentity=&idle.text_state_.at(4);g_contextActiveWidgets[other.get()]=textIdentity;
 int unrelated{};g_contextActiveWidgets[reinterpret_cast<void*>(7)]=&unrelated;
 idle.clear();CHECK(g_contextActiveWidgets[context.get()]==nullptr&&g_contextActiveWidgets[other.get()]==nullptr
  &&g_contextActiveWidgets[reinterpret_cast<void*>(7)]==&unrelated&&!g_editBoxTextStates.contains(textIdentity));
 CHECK(rt::run_contract()==rt::ContractFailure::none);
 return 0;
}
"""


class ProjectGuiKeyboard(unittest.TestCase):
    def test_actual_ordered_keyboard_adapter_and_session(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU module consumer")
        code = fixture()
        # Bind the real production edit box to the exact same single-event owner.
        text = (GUI / "controls/gui.text_controls.inl").read_text(encoding="utf-8")
        ordinary = definition(text,"    EditBoxResult edit_box(")
        self.assertIn("dispatch_edit_box_event(event, multiline, submit_on_enter",ordinary)
        runtime = (ROOT/"Engine/src/epoch/implementation/epoch.project_play_scene.inl").read_text(encoding="utf-8")
        self.assertLess(runtime.index("m_projectGuiRuntime->keyboard_captured()"),runtime.index("advance_gameplay("))
        self.assertLess(runtime.index("advance_gameplay("),runtime.index("gui::begin_frame("))
        identity = hashlib.sha256(code.encode()).hexdigest()[:12]
        report = ROOT/"Artifacts/Reports/project-gui-keyboard-oct28"/identity
        report.mkdir(parents=True,exist_ok=True)
        (report/"consumer.cpp").write_text(code,encoding="utf-8")
        current_render = definition(ADAPTER.read_text(encoding="utf-8"),"    EpochGuiRenderResult EpochGuiAdapter::render(")
        # Exercise the regression without depending on a dirty checkout or Git history.
        keyboard_dispatch = """            const auto keyboard = dispatch_keyboard(runtime, frame);
            if (keyboard != project_gui_runtime::RuntimeCode::ready
                && keyboard != project_gui_runtime::RuntimeCode::no_change)
                result.code = keyboard;
"""
        self.assertEqual(current_render.count(keyboard_dispatch), 1)
        self.assertEqual(code.count(current_render), 1)
        before_render = current_render.replace(keyboard_dispatch, "", 1)
        before = code[:code.index("int main(){")].replace(current_render,before_render) + r'''
int main(){auto context=std::make_shared<core::Context>();fresh(context);Adapter adapter;rt::RuntimeSession runtime{source()};
 CHECK(runtime.focus({4})==rt::RuntimeCode::ready);(void)runtime.drain_events();
 tick_keys(context,{key(9)});render(adapter,runtime);CHECK(runtime.focused_widget().value==9);
}
'''
        (report/"before.cpp").write_text(before,encoding="utf-8")
        before_line = before[:before.index("CHECK(runtime.focused_widget().value==9);")].count("\n")+1
        modules = [ROOT/"Engine/modules"/name for name in ("core.sha256.ixx","asset.gui_artifact.ixx","project.gui_runtime.ixx")]
        portable = ROOT/"Engine/dep/EpochGui"
        modules += [portable/"modules"/name for name in ("epoch.gui.ixx","epoch.gui.input.ixx")]
        sources = [ROOT/"Engine/src/assets/gui/asset.gui_artifact.cpp",ROOT/"Engine/src/project/project.gui_runtime.cpp",ROOT/"Engine/src/project/project.gui_runtime_contract.cpp",
                   portable/"src/epochgui/text_control.cpp",portable/"src/epochgui/input.cpp"]
        (report/"CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.30)\nproject(ProjectGuiKeyboard LANGUAGES CXX)\n'
            +'set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)\n'
            +'add_executable(keyboard consumer.cpp '+" ".join('"'+p.as_posix()+'"' for p in sources)+')\n'
            +'target_compile_features(keyboard PRIVATE cxx_std_23)\ntarget_compile_options(keyboard PRIVATE /W3 /WX /utf-8 /EHsc)\n'
            +'target_sources(keyboard PRIVATE FILE_SET CXX_MODULES BASE_DIRS "'+ROOT.as_posix()+'" FILES '+" ".join('"'+p.as_posix()+'"' for p in modules)+')\n'
            +'target_include_directories(keyboard PRIVATE "'+ROOT.as_posix()+'/Engine/include")\n'
            +'add_executable(before before.cpp '+" ".join('"'+p.as_posix()+'"' for p in sources)+')\n'
            +'target_compile_features(before PRIVATE cxx_std_23)\ntarget_compile_options(before PRIVATE /W3 /WX /utf-8 /EHsc)\n'
            +'target_sources(before PRIVATE FILE_SET CXX_MODULES BASE_DIRS "'+ROOT.as_posix()+'" FILES '+" ".join('"'+p.as_posix()+'"' for p in modules)+')\n'
            +'enable_testing()\nadd_test(NAME actual_project_gui_keyboard COMMAND keyboard)\n'
            +'add_test(NAME adapter_without_ordered_dispatch_misses_tab COMMAND before)\nset_tests_properties(adapter_without_ordered_dispatch_misses_tab PROPERTIES WILL_FAIL TRUE)\n',encoding="utf-8")
        build = ROOT/"Artifacts/Intermediate/cmake/project-gui-keyboard"/identity
        commands = [["cmake","-S",str(report),"-B",str(build),"-G","Visual Studio 17 2022","-A","x64"]]
        for config in ("Debug","Release"):
            commands += [["cmake","--build",str(build),"--config",config,"--parallel","1"],
                         ["ctest","--test-dir",str(build),"-C",config,"--output-on-failure"]]
        runs=[]
        for index,command in enumerate(commands):
            completed=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=150)
            log=report/f"lane-{index}.log";log.write_text(completed.stdout+completed.stderr,encoding="utf-8")
            runs.append({"command":command,"exit":completed.returncode,"log":str(log)})
            if command[0] == "ctest" and completed.returncode == 0:
                config = command[command.index("-C")+1]
                negative = subprocess.run([str(build/config/"before.exe")],cwd=report,
                    capture_output=True,text=True,timeout=10)
                (report/f"before-{config}.log").write_text(negative.stdout+negative.stderr,encoding="utf-8")
                self.assertEqual(negative.returncode,81,negative.stdout+negative.stderr)
                self.assertIn(f"failed line {before_line}",negative.stderr)
                runs.append({"before_configuration":config,"actual_exit":negative.returncode,
                    "expected_exit":81,"expected_assertion_line":before_line,
                    "negative_render_sha256":hashlib.sha256(before_render.encode()).hexdigest()})
            (report/"result.json").write_text(json.dumps({"passed":completed.returncode==0 and index==len(commands)-1,
                "scope":"Actual queued input/filter/begin_frame, shared ordered text command/caret owner, actual adapter render/dispatch/widget caller and real artifact+RuntimeSession modules. Neutral drawing/clipboard/metric ports; no native input, renderer or model.",
                "owners":{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ADAPTER,
                    ROOT/'Engine/modules/project.gui_epochgui.ixx',ROOT/'Engine/modules/project.gui_runtime.ixx',
                    ROOT/'Engine/src/project/project.gui_runtime.cpp',ROOT/'Engine/src/project/project.gui_runtime_contract.cpp',
                    ROOT/'Engine/modules/gui.engine.ixx',GUI/'gui.engine.cpp',GUI/'controls/gui.frame_lifecycle.inl',
                    GUI/'controls/gui.text_controls.inl',Path(__file__)]},"commands":runs},indent=2),encoding="utf-8")
            self.assertEqual(completed.returncode,0,completed.stdout[-7500:]+completed.stderr[-2500:])


if __name__=="__main__":
    unittest.main()
