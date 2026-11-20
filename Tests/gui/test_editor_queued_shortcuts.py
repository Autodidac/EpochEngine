"""Actual context queue and Editor shortcut block admit fast chords exactly once."""
from pathlib import Path
from Tests.support.cpp import engine_module_source
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.gui.test_gui_queued_drag import fixture as queue_fixture
from Tests.gui.test_project_gui_keyboard import fixture as project_keyboard_fixture

ROOT = Path(__file__).resolve().parents[2]
FRAME = ROOT / "Engine/src/epochgui/controls/gui.frame_lifecycle.inl"
TEXT = ROOT / "Engine/src/epochgui/controls/gui.text_controls.inl"
EDITOR = ROOT / "Engine/src/editor/application/editor.frame_finish.inl"


def editor_shortcut_blocks():
    early = (ROOT / "Engine/src/editor/application/editor.frame_bootstrap.inl").read_text(encoding="utf-8")
    opening = early[early.index("        const bool workspaceShortcutInputAvailable ="):early.index("        const bool overlayPriorityActive =")]
    late = EDITOR.read_text(encoding="utf-8")
    return opening + late[late.index("        const bool exitedProjectFullScreen ="):late.index("        const auto deferredNative=")]


def consumer():
    base = queue_fixture()
    base = base[:base.index("void fast_sequences(")]
    base = base.replace("#include <algorithm>", "#include <functional>\n#include <algorithm>")
    base = base.replace("namespace core {", "namespace input {enum class Key {LeftControl,RightControl,LeftShift,RightShift,Z,Y,Delete,Escape};}\nnamespace core {", 1)
    base = base.replace("bool focus=true,pointer=true,wheel=true;", "bool focus=true,pointer=true,wheel=true;bool is_key_held_safe(input::Key)const{return false;}bool is_key_down_safe(input::Key)const{return false;}")
    base = base.replace("bool any_select_box_open(const core::Context*){return false;}",
                        "bool testSelectOpen{};bool any_select_box_open(const core::Context*){return testSelectOpen;}")
    base = base.replace("void reset_frame(){", "void reset_frame(){g_frame.selectPopupCapturedThisFrame=false;g_frame.menuPopupBlocksBackground=false;g_frame.menuPopupOwnsInput=false;")
    functions = definition(FRAME.read_text(encoding="utf-8"), "    bool keyboard_input_captured(") + \
        definition(TEXT.read_text(encoding="utf-8"), "    bool dispatch_keyboard_shortcuts(")
    base += "\nnamespace gui{std::map<const void*,const void*> g_contextActiveWidgets;\n" + functions + "\n}\n"
    block = editor_shortcut_blocks()
    return base + PORT + "\nvoid shortcuts(EditorState& editor,const std::shared_ptr<core::Context>& ctx,bool modalVisible=false,bool pointerGrabActive=false){Result result;\n" + block + "\ncaptured=result.scene_input_captured;}\n" + MAIN


PORT = r'''
enum class TopMenu{None,Edit};
enum class SurfaceEditAction{undo,redo,delete_selection};
struct EditorState{TopMenu openMenu{TopMenu::None};bool maximized{};struct{struct{struct{bool open{};}popup;}menu;}outlinerContextMenu;};
unsigned full_screen_exits{};
bool project_runtime_viewport_maximized(const EditorState& editor){return editor.maximized;}
bool editor_project_viewport_input_reserved(const EditorState&){return false;}
void exit_project_runtime_full_screen(EditorState& editor){editor.maximized=false;++full_screen_exits;}
struct Result{bool scene_input_captured{};};
std::vector<SurfaceEditAction> actions;
bool testFloating{},testModalNow{},captured{},automationInputReserved{};
bool floating_gui_visible(){return testFloating;}
bool modal_visible_now(){return testModalNow;}
bool execute_surface_edit(EditorState&,core::Context*,SurfaceEditAction action){actions.push_back(action);return false;}
bool editor_has_selection(const EditorState&){return false;}
void clear_editor_selection(EditorState&){}
void push_editor_log(EditorState&,const std::string&){}
InputEvent chord(int key,bool shift=false){return {.type=EventType::KeyDown,.key=key,.ctrl_down=true,.shift_down=shift};}
void key_frame(const std::shared_ptr<core::Context>& context,std::initializer_list<InputEvent> input){for(const auto& e:input)push_input_for_context(context.get(),e);begin_frame(context,0.01f,{},false);}
'''

MAIN = r'''
int main(){
 auto a=std::make_shared<core::Context>(),b=std::make_shared<core::Context>();EditorState editor;
 // The entire press/release occurred before this frame. Both current-sampler
 // methods intentionally return false; the actual queued modifier must win.
 key_frame(a,{chord('Z'),{.type=EventType::KeyUp,.key='Z'},{.type=EventType::KeyUp,.key=17}});
 CHECK(!a->is_key_held_safe(input::Key::LeftControl)&&!a->is_key_down_safe(input::Key::Z));
 shortcuts(editor,a);CHECK(actions==std::vector<SurfaceEditAction>{SurfaceEditAction::undo}&&captured);
 CHECK(g_frame.events.size()==2&&g_frame.visitedActiveWidget==nullptr&&g_contextActiveWidgets.empty());
 shortcuts(editor,a);CHECK(actions.size()==1&&!captured);
 // Multiple independent chords keep their original order and shift state.
 actions.clear();key_frame(a,{chord('Z'),chord('z',true),chord('Y')});shortcuts(editor,a);
 CHECK(actions==std::vector<SurfaceEditAction>({SurfaceEditAction::undo,SurfaceEditAction::redo,SurfaceEditAction::redo}));
 CHECK(g_frame.events.empty());shortcuts(editor,a);CHECK(actions.size()==3);
 actions.clear();key_frame(a,{{.type=EventType::KeyDown,.key='Z'},chord('A'),{.type=EventType::TextInput,.text="z"}});shortcuts(editor,a);
 CHECK(actions.empty()&&g_frame.events.size()==3);
 // A text/source field keeps its events; this consumer never claims focus.
 int owner{};g_contextActiveWidgets[a.get()]=&owner;
 key_frame(a,{chord('Z')});shortcuts(editor,a);CHECK(actions.empty()&&g_frame.events.size()==1&&g_contextActiveWidgets[a.get()]==&owner);
 g_contextActiveWidgets.erase(a.get());
 // Root guards remain authoritative, while the reusable GUI consumer also
 // refuses modal/select/menu ownership independently of Editor state.
 for(int guard=0;guard<7;++guard){
  key_frame(a,{chord('Z')});
  if(guard==0)g_frame.modalInput.begin({{0,0},{800,600}});
  if(guard==1)testSelectOpen=true;
  if(guard==2)g_frame.selectPopupCapturedThisFrame=true;
  if(guard==3)g_frame.menuPopupBlocksBackground=true;
  if(guard==4)g_frame.menuPopupOwnsInput=true;
  if(guard==5)editor.openMenu=TopMenu::Edit;
  if(guard==6)testFloating=true;
  shortcuts(editor,a);CHECK(actions.empty()&&g_frame.events.size()==1);
  testSelectOpen=false;editor.openMenu=TopMenu::None;testFloating=false;
 }
 key_frame(a,{chord('Z')});
 {gui_lib::input::ScopedInputSuppression gate{g_frame.modalInput,true};shortcuts(editor,a);CHECK(actions.empty()&&g_frame.events.size()==1);}
 key_frame(a,{chord('Z')});shortcuts(editor,a,true);CHECK(actions.empty()&&g_frame.events.size()==1);
 key_frame(a,{chord('Z')});shortcuts(editor,a,false,true);CHECK(actions.empty()&&g_frame.events.size()==1);
 key_frame(a,{chord('Z')});testModalNow=true;shortcuts(editor,a);CHECK(actions.empty()&&g_frame.events.size()==1);testModalNow=false;
 automationInputReserved=true;key_frame(a,{chord('Z')});shortcuts(editor,a);CHECK(actions.empty());
 automationInputReserved=false;key_frame(a,{});shortcuts(editor,a);CHECK(actions.empty());
 // FocusLost/native refusal cannot dispatch a command from an unfocused window.
 a->focus=false;key_frame(a,{chord('Z')});shortcuts(editor,a);CHECK(actions.empty());a->focus=true;
 key_frame(a,{{.type=EventType::FocusLost},chord('Z')});shortcuts(editor,a);CHECK(actions.empty());
 // Other contexts retain their own queue; this frame cannot steal their keys.
 push_input_for_context(b.get(),chord('Z',true));key_frame(a,{});shortcuts(editor,a);CHECK(actions.empty());
 begin_frame(b,0.01f,{},false);shortcuts(editor,b);CHECK(actions==std::vector<SurfaceEditAction>{SurfaceEditAction::redo});
 CHECK(g_contextActiveWidgets.empty());
 editor.maximized=true;key_frame(a,{{.type=EventType::KeyDown,.key=27},{.type=EventType::KeyUp,.key=27}});
 shortcuts(editor,a);CHECK(!editor.maximized&&full_screen_exits==1&&captured);
 shortcuts(editor,a);CHECK(full_screen_exits==1&&!captured);
 editor.maximized=true;testModalNow=true;key_frame(a,{{.type=EventType::KeyDown,.key=27}});
 shortcuts(editor,a);CHECK(editor.maximized&&full_screen_exits==1);testModalNow=false;
 key_frame(a,{{.type=EventType::KeyDown,.key='Q'}});shortcuts(editor,a);
 CHECK(editor.maximized&&full_screen_exits==1&&!captured);
 return 0;
}
'''

FOCUS_MAIN = r'''
int main(){
 auto context=std::make_shared<core::Context>();fresh(context);Adapter adapter;rt::RuntimeSession runtime{source()};EditorState editor;
 CHECK(runtime.focus({4})==rt::RuntimeCode::ready);(void)runtime.drain_events();
 tick_keys(context,{text("A")});render(adapter,runtime);CHECK(view(runtime,4).primary_text=="EpochA");
 CHECK(runtime.clear_focus()==rt::RuntimeCode::ready);tick_keys(context,{});render(adapter,runtime);
 CHECK(!runtime.keyboard_captured()&&g_contextActiveWidgets[context.get()]==nullptr);
 const auto field=view(runtime,4);const Vec2 point{field.bounds.x+10,field.bounds.y+10};
 // Current-frame controls run first. Real pointer admission focuses this
 // formerly unfocused field, and its real text owner consumes Ctrl+Z.
 tick_keys(context,{event(EventType::MouseDown,point.x,point.y),event(EventType::MouseUp,point.x,point.y),key('Z',false,true)});
 render(adapter,runtime);shortcuts(editor,context);
 CHECK(runtime.focused_widget().value==4&&view(runtime,4).primary_text=="Epoch");
 CHECK(actions.empty()&&!captured);
 // A click outside the GUI clears its focus through the real adapter. The
 // same-frame chord must remain available for the global document command.
 tick_keys(context,{event(EventType::MouseDown,799,599),event(EventType::MouseUp,799,599),key('Z',false,true)});
 render(adapter,runtime);shortcuts(editor,context);
 CHECK(!runtime.keyboard_captured()&&actions==std::vector<SurfaceEditAction>{SurfaceEditAction::undo}&&captured);
 shortcuts(editor,context);CHECK(actions.size()==1&&!captured);
 return 0;
}
'''


class EditorQueuedShortcuts(unittest.TestCase):
    def test_same_frame_real_text_focus_precedes_document_shortcut(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU module consumer")
        code = project_keyboard_fixture()
        code = code[:code.index("int main(){")]
        code = code.replace("namespace core {", "namespace input {enum class Key {LeftControl,RightControl,LeftShift,RightShift,Z,Y,Delete,Escape};}\nnamespace core {", 1)
        code = code.replace("bool focus=true,pointer=true,wheel=true;", "bool focus=true,pointer=true,wheel=true;bool is_key_held_safe(input::Key)const{return false;}bool is_key_down_safe(input::Key)const{return false;}")
        block = editor_shortcut_blocks()
        code += PORT + "\nvoid shortcuts(EditorState& editor,const std::shared_ptr<core::Context>& ctx,bool modalVisible=false,bool pointerGrabActive=false){Result result;\n" + block + "\ncaptured=result.scene_input_captured;}\n" + FOCUS_MAIN
        proof = ROOT / "Artifacts/Reports/editor-queued-shortcuts-oct28/same-frame-focus"
        build = ROOT / "Artifacts/Intermediate/tests/editor-shortcut-current-focus"
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "consumer.cpp").write_text(code, encoding="utf-8")
        portable = ROOT / "Engine/dep/EpochGui"
        modules = [engine_module_source(ROOT / "Engine/modules", name) for name in ("core.sha256.ixx", "asset.gui_artifact.ixx", "project.gui_runtime.ixx")]
        modules += [portable / "modules" / name for name in ("epoch.gui.ixx", "epoch.gui.input.ixx")]
        sources = [ROOT / "Engine/src/assets/gui/asset.gui_artifact.cpp", ROOT / "Engine/src/project/project.gui_runtime.cpp",
                   ROOT / "Engine/src/project/project.gui_runtime_contract.cpp", portable / "src/epochgui/text_control.cpp", portable / "src/epochgui/input.cpp"]
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(EditorShortcutCurrentFocus LANGUAGES CXX)
add_executable(editor_shortcut_focus consumer.cpp {" ".join('"'+p.as_posix()+'"' for p in sources)})
target_compile_features(editor_shortcut_focus PRIVATE cxx_std_23)
target_compile_options(editor_shortcut_focus PRIVATE /W3 /utf-8 /EHsc)
target_include_directories(editor_shortcut_focus PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_sources(editor_shortcut_focus PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{ROOT.as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in modules)})
enable_testing()
add_test(NAME actual_same_frame_text_focus COMMAND editor_shortcut_focus)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                         ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout+completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-6000:]+completed.stderr)
        (proof / "result.json").write_text(json.dumps({"passed": True, "nativeAcceptance": False, "commands": results,
            "scope": "Actual project GUI pointer/focus/text dispatcher and real RuntimeSession/text history; actual late Editor shortcut block with command hook port.",
            "cases": ["same-frame click focuses text and CtrlZ undoes text only", "outside click plus CtrlZ dispatches global command once"]}, indent=2)+"\n", encoding="utf-8")

    def test_actual_queue_and_editor_commands_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU module consumer")
        early = (ROOT / "Engine/src/editor/application/editor.frame_bootstrap.inl").read_text(encoding="utf-8")
        self.assertEqual(early.count("dispatch_keyboard_shortcuts("), 1)
        self.assertIn("return event.key == 27;", early)
        self.assertNotIn("SurfaceEditAction::undo", early)
        order = (ROOT / "Engine/src/editor/editor.application.cpp").read_text(encoding="utf-8")
        self.assertLess(order.index('"application/editor.frame_packages_about.inl"'), order.index('"application/editor.frame_finish.inl"'))
        late = EDITOR.read_text(encoding="utf-8")
        edition = (ROOT / "Engine/include/epoch.edition.hpp").read_text(encoding="utf-8")
        if "#define EPOCH_SOURCE_EDITION_PUBLIC 1" in edition.splitlines():
            self.assertNotIn("editor_native_agent_approval_modal", late)
        else:
            self.assertIn("#define EPOCH_SOURCE_EDITION_PUBLIC 0", edition.splitlines())
            self.assertLess(late.index("editor_native_agent_approval_modal"), late.index("dispatch_keyboard_shortcuts("))
        proof = ROOT / "Artifacts/Reports/editor-queued-shortcuts-oct28"
        build = ROOT / "Artifacts/Intermediate/tests/editor-queued-shortcuts"
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "editor.queued_shortcuts_contract.cpp"
        source.write_text(consumer(), encoding="utf-8")
        portable = ROOT / "Engine/dep/EpochGui"
        modules = [portable / "modules" / name for name in ("epoch.gui.ixx", "epoch.gui.input.ixx")]
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(EditorQueuedShortcuts LANGUAGES CXX)
add_executable(editor_queued_shortcuts "{source.as_posix()}" "{(portable/'src/epochgui/input.cpp').as_posix()}")
target_compile_features(editor_queued_shortcuts PRIVATE cxx_std_23)
target_compile_options(editor_queued_shortcuts PRIVATE /W3 /WX /utf-8)
target_sources(editor_queued_shortcuts PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{portable.as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in modules)})
enable_testing()
add_test(NAME actual_editor_queued_shortcuts COMMAND editor_queued_shortcuts)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                         ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=100)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout+completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-6000:]+completed.stderr)
        (proof / "result.json").write_text(json.dumps({
            "passed": True, "nativeAcceptance": False, "commands": results,
            "sourceIdentity": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in (FRAME, TEXT, EDITOR)},
            "scope": "Actual GUI context queue/filter/begin_frame, focus-neutral shortcut consumer, and Editor shortcut block; final polled key state and edit command hook are minimal ports. Existing journal execution is verified separately.",
            "cases": ["fully released fast chord", "ordered Undo/Redo modifiers", "once-only consumed event",
                      "no focus claim", "text/source/modal/menu/select/floating/pointer guards", "focus-lost/native refusal", "context-specific queue"],
        }, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
