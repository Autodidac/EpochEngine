"""Actual GUI frame/keyboard owners release hidden focus without losing document state."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.gui.test_project_gui_keyboard import fixture as keyboard_fixture

ROOT = Path(__file__).resolve().parents[2]
GUI = ROOT / "Engine/src/epochgui"


def consumer():
    source = (GUI / "controls/gui.frame_lifecycle.inl").read_text(encoding="utf-8")
    frame_end = definition(source, "    void end_frame(")
    keyboard_capture = definition(source, "    bool keyboard_input_captured(")
    base = keyboard_fixture()
    base = base[:base.index("int main(){")]
    base = base.replace("const void* visitedActiveWidget{};", """const void* visitedActiveWidget{};
 std::vector<InputEvent> modalHeldEvents;int modalHeldWheel{};
 bool modalInputSuspended{},modalOwnerDrawn{},modalOwnerDrawing{};""")
    base = base.replace("bool visited_this_frame{};", "bool open{},alignSelectedOnOpen{},visited_this_frame{};")
    base += "\nnamespace gui{void render_pending_select_popups()noexcept{}void flush_queued_draws()noexcept{}\n" + \
        frame_end + ("" if "bool keyboard_input_captured()" in base else keyboard_capture) + "\n}\n"
    before_end = frame_end[:frame_end.index("        // A text/document owner")] + \
        frame_end[frame_end.index("        flush_queued_draws();"):]
    return base, frame_end, before_end


MAIN = r'''
int main(){
 auto a=std::make_shared<core::Context>(),b=std::make_shared<core::Context>();int hidden{},other{},generic{};
 g_contextActiveWidgets[a.get()]=&hidden;g_contextActiveWidgets[b.get()]=&other;
 begin_frame(a,0.01f,{},false);CHECK(keyboard_input_captured());end_frame();
 begin_frame(a,0.01f,{},false);CHECK(!keyboard_input_captured());CHECK(g_contextActiveWidgets[b.get()]==&other);end_frame();
 // A visible document/generic owner is visited through the real dispatcher,
 // including when a modal/popup covers it and it receives no key events.
 auto target=[&]()->KeyboardInputTarget{return {.identity=&generic};};
 auto command=[](const InputEvent&){return false;};
 KeyboardInputOptions options{.owner=&generic,.target=target,.command=command};
 begin_frame(a,0.01f,{},false);CHECK(dispatch_keyboard_input(options));CHECK(keyboard_input_captured());end_frame();
 CHECK(g_contextActiveWidgets[a.get()]==&generic);
 begin_frame(a,0.01f,{},false);testPopup=true;CHECK(!dispatch_keyboard_input(options));end_frame();testPopup=false;
 CHECK(g_contextActiveWidgets[a.get()]==&generic);
 begin_frame(a,0.01f,{},false);g_frame.modalInput.begin({{0,0},{800,600}});g_frame.modalInput.require_owner_scope();
 CHECK(!dispatch_keyboard_input(options));end_frame();CHECK(g_contextActiveWidgets[a.get()]==&generic);
 // The owner itself disappears on the next frame: only its input claim ends.
 begin_frame(a,0.01f,{},false);end_frame();CHECK(g_contextActiveWidgets[a.get()]==nullptr);
 CHECK(g_contextActiveWidgets[b.get()]==&other);
 std::string value="original";auto textTarget=[&]()->KeyboardInputTarget{return {.identity=&value,.text=&value,.size={300,40},.maximum_bytes=1000};};
 KeyboardInputOptions textOptions{.owner=&value,.target=textTarget,.command=command};
 begin_frame(a,0.01f,{},false);g_frame.events={text("typed")};CHECK(dispatch_keyboard_input(textOptions));end_frame();
 CHECK(value!="original");const auto beforeText=value;const auto beforeCaret=g_editBoxTextStates[&value].control.caret;
 begin_frame(a,0.01f,{},false);end_frame();CHECK(g_contextActiveWidgets[a.get()]==nullptr);
 CHECK(value==beforeText&&g_editBoxTextStates[&value].control.caret==beforeCaret);
 // Refocusing reuses the real text journal; hidden-control retirement does
 // not erase Undo, selection/caret or another context's control state.
 begin_frame(a,0.01f,{},false);g_frame.events={key(90,false,true)};CHECK(dispatch_keyboard_input(textOptions));end_frame();
 CHECK(value=="original");CHECK(g_contextActiveWidgets[b.get()]==&other);
 a->focus=false;begin_frame(a,0.01f,{},false);CHECK(!dispatch_keyboard_input(textOptions));end_frame();
 CHECK(g_contextActiveWidgets[a.get()]==nullptr);a->focus=true;
 return 0;
}
'''

BEFORE_MAIN = r'''
int main(){auto a=std::make_shared<core::Context>();int hidden{};
 g_contextActiveWidgets[a.get()]=&hidden;begin_frame(a,0.01f,{},false);end_frame();
 begin_frame(a,0.01f,{},false);CHECK(!keyboard_input_captured());return 0;}
'''


class HiddenKeyboardFocus(unittest.TestCase):
    def test_actual_lifecycle_dispatch_and_retained_text_history(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC module consumer")
        base, frame_end, before_end = consumer()
        # Both visible text implementations must visit the same existing owner.
        text_owner = (GUI / "controls/gui.text_controls.inl").read_text(encoding="utf-8")
        self.assertIn("if (active) g_frame.visitedActiveWidget = id;", definition(text_owner, "    EditBoxResult edit_box("))
        self.assertIn("if (active) g_frame.visitedActiveWidget = textId;", definition(text_owner, "    SourceEditorResult source_editor("))
        proof = ROOT / "Artifacts/Reports/gui-hidden-focus-oct28"
        build = ROOT / "Artifacts/Intermediate/tests/gui-hidden-focus"
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "consumer.cpp").write_text(base+MAIN, encoding="utf-8")
        (proof / "before.cpp").write_text(base.replace(frame_end, before_end)+BEFORE_MAIN, encoding="utf-8")
        portable = ROOT / "Engine/dep/EpochGui"
        modules = [ROOT / "Engine/modules" / name for name in ("core.sha256.ixx", "asset.gui_artifact.ixx", "project.gui_runtime.ixx")]
        modules += [portable / "modules" / name for name in ("epoch.gui.ixx", "epoch.gui.input.ixx")]
        sources = [ROOT / "Engine/src/assets/gui/asset.gui_artifact.cpp", ROOT / "Engine/src/project/project.gui_runtime.cpp",
                   ROOT / "Engine/src/project/project.gui_runtime_contract.cpp", portable / "src/epochgui/text_control.cpp", portable / "src/epochgui/input.cpp"]
        cmake = "cmake_minimum_required(VERSION 3.30)\nproject(HiddenKeyboardFocus LANGUAGES CXX)\nenable_testing()\n"
        for name, file in (("hidden_focus", "consumer.cpp"), ("before_focus", "before.cpp")):
            cmake += f'''add_executable({name} {file} {" ".join('"'+p.as_posix()+'"' for p in sources)})
target_compile_features({name} PRIVATE cxx_std_23)
target_compile_options({name} PRIVATE /W3 /utf-8 /EHsc)
target_include_directories({name} PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_sources({name} PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{ROOT.as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in modules)})
'''
        cmake += "add_test(NAME actual_hidden_focus COMMAND hidden_focus)\nadd_test(NAME observed_before_hidden_focus COMMAND before_focus)\nset_tests_properties(observed_before_hidden_focus PROPERTIES WILL_FAIL TRUE)\n"
        (proof / "CMakeLists.txt").write_text(cmake, encoding="utf-8")
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
        (proof / "result.json").write_text(json.dumps({
            "passed": True, "nativeAcceptance": False, "commands": results,
            "beforeEndFrameSHA256": hashlib.sha256(before_end.encode()).hexdigest(),
            "cases": ["old hidden owner reproduces failure", "unvisited owner released in same context", "other context untouched",
                      "visible generic owner retained through modal/popup cover", "text history/caret retained", "refocus Undo uses existing text history", "native-focus loss releases ownership"],
            "scope": "Actual begin/end frame, keyboard capture/dispatch, shared text event owner and real text/runtime modules; neutral drawing and context ports. Edit-box/source-editor visitation wiring checked from actual owner bodies.",
        }, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
