"""Wrong destination paints before a real source pane during mixed held input."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.gui.test_gui_pane_drag import fixture as pane_fixture

ROOT = Path(__file__).resolve().parents[2]
BUTTONS = ROOT / "Engine/src/epochgui/controls/gui.buttons_images.inl"
TABS = ROOT / "Engine/src/epochgui/controls/gui.navigation_chrome.inl"


def fixture(before_buttons=None, before_tabs=None):
    base = pane_fixture()
    api = (ROOT / "Engine/modules/gui/gui.engine.ixx").read_text(encoding="utf-8")
    bounds = definition(api, "    export struct WidgetBounds", True).replace("export ", "")
    base = base.replace("struct Frame{", bounds + "\nstruct Frame{std::optional<WidgetBounds> lastButtonBounds;")
    base = base.replace("textFieldActive=5;", "textFieldActive=5,textField=6;")
    button_source=before_buttons or BUTTONS.read_text(encoding="utf-8")
    button_start=button_source.index("    static bool button_with_state(")
    button_body=button_source.index("\n    {",button_start)+5
    button=button_source[button_start:button_body]+definition(button_source[button_body:],"{")
    icon=definition(api,"    export enum class ButtonIcon",True).replace("export ","",1)
    ports = r'''
using SemanticTone=gui_lib::SemanticTone;
struct SemanticPair{int accent=7,background=8;};SemanticPair active_semantic_pair(SemanticTone){return {};}
float space_advance(float scale){return 4*scale;}constexpr float kButtonTextClipInset=2;
// Neutral painting ports; pointer ownership is the tested production contract.
void draw_button_icon(ButtonIcon,Vec2,Vec2){}
void draw_button_tooltip(std::string_view,Vec2){}
'''
    base = base.replace("}\nusing namespace gui;", icon + ports + button + "\n}\nusing namespace gui;", 1)
    if before_tabs:
        current = definition(TABS.read_text(encoding="utf-8"), "    TabBarResult tab_bar_buttons(")
        base = base.replace(current, definition(before_tabs, "    TabBarResult tab_bar_buttons("), 1)
    base = base[:base.index("int main(){pane_contract();")]
    return base + r'''
void origin_contract(){
 auto ctx=std::make_shared<core::Context>();fresh(ctx);
 const std::array source{TabButtonSpec{.id="pane.gui_hierarchy",.label="GUI Hierarchy",.width=180,.active=true,.closable=true}};
 const std::array wrong{TabButtonSpec{.id="systems",.label="Systems",.width=180}};
 const Vec2 origin{40,25},destination{450,100};DragSurfaceState drag;
 const auto drawSource=[&](std::string_view id){g_frame.cursor={10,10};return tab_bar_buttons(source,30,1,TabBarPresentation::Document,{},TabDragOptions{&drag,id,12});};
 const auto frame=[&](std::initializer_list<InputEvent> events,bool held=false){tick(ctx,events,held);g_frame.contentMin={0,0};g_frame.contentMax={800,600};};
 // The wrong main-workspace tab paints first while final hover is over it.
 frame({event(EventType::MouseDown,origin.x,origin.y),event(EventType::MouseMove,destination.x,destination.y)},true);
 CHECK(g_frame.justPressed&&g_frame.mouseDown);g_frame.cursor={360,85};
 auto destinationTab=tab_bar_buttons(wrong,30,1,TabBarPresentation::Workbench,{},{});
 CHECK(!destinationTab.pressed_index&&!destinationTab.selected_index&&!g_frame.mousePressConsumed);
 auto sourceTab=drawSource({});CHECK(sourceTab.drag.pressed&&sourceTab.drag.dragging&&sourceTab.drag_origin.x==origin.x);
 frame({event(EventType::MouseUp,destination.x,destination.y)});g_frame.cursor={360,85};
 destinationTab=tab_bar_buttons(wrong,30,1,TabBarPresentation::Workbench,{},{});
 CHECK(!destinationTab.selected_index);sourceTab=drawSource(source[0].id);CHECK(sourceTab.drag.released);
 // The shared button owner, including a circular button, must also leave the
 // press for the source pane when only its final hover is over the button.
 for(bool circular:{false,true}){
  fresh(ctx);drag={};frame({event(EventType::MouseDown,origin.x,origin.y),event(EventType::MouseMove,destination.x,destination.y)},true);
  g_frame.cursor={400,80};CHECK(!button_with_state("Wrong",{100,40},false,true,SemanticTone::neutral,circular));
  CHECK(!g_frame.mousePressConsumed);sourceTab=drawSource({});CHECK(sourceTab.drag.pressed&&sourceTab.drag.dragging);
  frame({event(EventType::MouseUp,destination.x,destination.y)});g_frame.cursor={400,80};
  CHECK(!button_with_state("Wrong",{100,40},false,true,SemanticTone::neutral,circular));
  CHECK(drawSource(source[0].id).drag.released);
 }
 // Circle membership is checked at the origin, not its enclosing rectangle.
 fresh(ctx);frame({event(EventType::MouseDown,401,81),event(EventType::MouseMove,450,100)},true);g_frame.cursor={400,80};
 CHECK(!button_with_state("Circle",{100,40},false,true,SemanticTone::neutral,true)&&!g_frame.mousePressConsumed);
 frame({event(EventType::MouseUp,450,100)});g_frame.cursor={400,80};
 CHECK(!button_with_state("Circle",{100,40},false,true,SemanticTone::neutral,true));
 // Valid stationary clicks retain their old complete-in-one-frame behavior.
 for(bool circular:{false,true}){
  fresh(ctx);frame({event(EventType::MouseDown,450,100),event(EventType::MouseUp,450,100)});g_frame.cursor={400,80};
  CHECK(button_with_state("Valid",{100,40},false,true,SemanticTone::neutral,circular));
 }
 // A press outside the active clip or modal cannot be laundered by final hover.
 for(bool modal:{false,true}){
  fresh(ctx);frame({event(EventType::MouseDown,405,100),event(EventType::MouseMove,450,100)},true);
  if(modal)g_frame.modalInput.begin({{430,80},{100,100}});else g_frame.contentMin={430,80};
  g_frame.cursor={400,80};CHECK(!button_with_state("Clipped",{100,40},false,true,SemanticTone::neutral,false));
  CHECK(!g_frame.mousePressConsumed);
 }
 fresh(ctx);frame({event(EventType::MouseDown,450,100)},true);g_frame.mousePressConsumed=true;g_frame.cursor={400,80};
 g_contextPressedButtonKeys[ctx.get()]=999;CHECK(!button_with_state("Consumed",{100,40},false,true,SemanticTone::neutral,false));
 CHECK(g_contextPressedButtonKeys[ctx.get()]==999);
 // A close press moved onto the tab body must neither become a body press nor
 // start a pane drag. Existing release-at-close semantics stay unchanged.
 fresh(ctx);g_frame.cursor={10,10};const auto geometry=tab_bar_buttons(source,30,1,TabBarPresentation::Document,{},{});
 (void)geometry;const Vec2 close{closePosition.x+closeSize.x/2,closePosition.y+closeSize.y/2};
 frame({event(EventType::MouseDown,close.x,close.y),event(EventType::MouseMove,origin.x,origin.y)},true);
 sourceTab=drawSource({});CHECK(!sourceTab.dragged_index&&!sourceTab.pressed_index);
 frame({event(EventType::MouseUp,origin.x,origin.y)});sourceTab=drawSource({});
 CHECK(!sourceTab.selected_index&&!sourceTab.closed_index&&!sourceTab.dragged_index);
}
int main(){origin_contract();}
'''


class GuiPressOrigin(unittest.TestCase):
    def test_actual_mixed_held_origin_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC C++23 module consumer")
        proof = ROOT / "build/validation/gui-press-origin-oct27"
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "gui.press_origin_contract.cpp"
        source.write_text(fixture(), encoding="utf-8")
        portable = ROOT / "Engine/dep/EpochGui"
        modules = [portable / "modules/epoch.gui.ixx", portable / "modules/epoch.gui.input.ixx"]
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(GuiPressOrigin LANGUAGES CXX)
add_executable(press_origin "{source.as_posix()}" "{(portable/'src/epochgui/input.cpp').as_posix()}")
target_compile_features(press_origin PRIVATE cxx_std_23)
target_compile_options(press_origin PRIVATE /W3 /WX /utf-8)
target_sources(press_origin PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{portable.as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in modules)})
enable_testing()
add_test(NAME actual_press_origin COMMAND press_origin)
set_tests_properties(actual_press_origin PROPERTIES TIMEOUT 15)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(proof / "msvc"), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(proof / "msvc"), "--config", config, "--parallel", "1"],
                ["ctest", "--test-dir", str(proof / "msvc"), "-C", config, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=150)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-9000:] + completed.stderr)
        (proof / "result.json").write_text(json.dumps({
            "scope": "Actual queue/frame, default tab/button owners before source pane; mixed held press/move; real portable layout",
            "passed": True, "nativeAcceptance": False,
            "owners": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in [BUTTONS, TABS]},
            "commands": results,
            "cases": ["wrong Systems tab paints before source pane", "wrong rectangular/circular button paints before source pane",
                "source receives real origin and held/release gesture", "circle origin outside shape", "stationary one-frame valid clicks",
                "origin clip/modal/consumed ownership", "close-origin movement does not select or detach body"],
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
