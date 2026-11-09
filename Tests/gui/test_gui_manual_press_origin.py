"""Actual manual controls admit the queued press origin before a source pane."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.gui.test_gui_press_origin import fixture as origin_fixture
from Tests.gui.test_command_menu_submenu_adapter import child_view_definition

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/epochgui/controls/gui.buttons_images.inl"


def fixture(owner_text=None):
    source = owner_text or OWNER.read_text(encoding="utf-8")
    api = (ROOT / "Engine/modules/gui.engine.ixx").read_text(encoding="utf-8")
    base = origin_fixture()
    base = base.replace('namespace gui {','namespace epochengine::gui_lib{'+child_view_definition()+'}\nnamespace gui {',1)
    base = base[:base.index("int main(){origin_contract();}")]
    base = base.replace("using SpriteHandle=int;", r'''struct SpriteHandle{
 int id{};SpriteHandle()=default;SpriteHandle(int value):id(value){}
 operator int()const{return id;}bool is_valid()const{return id!=0;}};''')
    base = base.replace("float control_radius{};", "float control_radius{};unsigned segments_per_corner{};")
    base = base.replace("struct Palette{int ", "struct Palette{SpriteHandle ")
    base = base.replace("struct SemanticPair{int ", "struct SemanticPair{SpriteHandle ")
    base = base.replace("struct Palette{", "struct Palette{bool isLight{};")
    base = base.replace("textFieldActive=5,textField=6;", "textFieldActive=5,textField=6,titleBar=7,windowBackground=8;")
    types = "\n".join(definition(api, marker, True).replace("export ", "") for marker in (
        "    export enum class MenuItemIndicator", "    export struct MenuItemOptions",
        "    export enum class ImageFit", "    export struct ImageBoxOptions",
        "    export struct ImageBoxResult"))
    ports = r'''
struct FontPort{bool asset{};};struct{FontPort halfTextLightFont,halfTextFont;SpriteHandle contrastBlack{9},contrastWhite{10};}g_resources;
const FontPort* g_temporaryTextFont{};bool active_palette_is_light(){return false;}
// This contract records pointer ownership, with a neutral font port. The
// palette/font-policy suite executes the real semantic font selection.
const FontPort* semantic_text_font(SemanticTone){return nullptr;}
struct ContentClipScope{ContentClipScope(Vec2,Vec2){}};
void ensure_resources(){}float content_right(){return g_frame.contentMax.x;}
'''
    owners = "\n".join(definition(source, marker, marker.endswith("Layout")) for marker in (
        "    struct MenuItemLayout", "    [[nodiscard]] static MenuItemLayout make_menu_item_layout(",
        "    float menu_item_row_height()", "    static bool menu_item_control(",
        "    bool menu_item(", "    bool toggle_switch(",
        "    bool text_link(", "    ImageBoxResult image_box("))
    base = base.replace("}\nusing namespace gui;", types + ports + owners + "\n}\nusing namespace gui;", 1)
    return base + r'''
int main(int argc,char** argv){
 CHECK(argc==2);const std::string_view owner=argv[1];bool value=false,enabled=true,interactive=true;
 const auto draw=[&](){g_frame.cursor={400,80};
  if(owner=="menu")return menu_item({.label="Destination",.size={100,40},.enabled=enabled});
  if(owner=="toggle")return toggle_switch("Destination",value,{100,40});
  if(owner=="link")return text_link("Destination",{100,40},false);
  if(owner=="image")return image_box({.id="Destination",.sprite=1,.size={100,40},.interactive=interactive,.enabled=enabled}).clicked;
  CHECK(false);return false;};
 auto ctx=std::make_shared<core::Context>();fresh(ctx);DragSurfaceState drag;
 const std::array source{TabButtonSpec{.id="pane.gui_hierarchy",.label="GUI Hierarchy",.width=180,.active=true}};
 const auto sourcePane=[&](std::string_view active){g_frame.cursor={10,10};return tab_bar_buttons(source,30,1,TabBarPresentation::Document,{},TabDragOptions{&drag,active,12});};
 const auto frame=[&](std::initializer_list<InputEvent> events,bool held=false){tick(ctx,events,held);g_frame.contentMin={0,0};g_frame.contentMax={800,600};};
 // The destination paints before the real owning source tab.
 frame({event(EventType::MouseDown,40,25),event(EventType::MouseMove,450,100)},true);
 CHECK(!draw()&&!value&&!g_frame.mousePressConsumed);
 auto result=sourcePane({});CHECK(result.drag.pressed&&result.drag.dragging);
 frame({event(EventType::MouseUp,450,100)});CHECK(!draw()&&!value);
 // Menu rows retain their existing popup release absorption, even without a
 // matching press. This repair changes initial admission only.
 if(owner=="menu")CHECK(g_frame.mouseReleaseConsumed&&!sourcePane(source[0].id).drag.released);
 else CHECK(sourcePane(source[0].id).drag.released);
 // A valid stationary click still activates; a later release outside does not.
 fresh(ctx);frame({event(EventType::MouseDown,450,100),event(EventType::MouseUp,450,100)});
 CHECK(draw());CHECK(value==(owner=="toggle"));
 fresh(ctx);value=false;frame({event(EventType::MouseDown,450,100)},true);CHECK(!draw());
 frame({event(EventType::MouseUp,650,100)});CHECK(!draw()&&!value);
 // Queued origin must also satisfy the existing content clip and modal gate.
 for(bool modal:{false,true}){
  fresh(ctx);frame({event(EventType::MouseDown,405,100),event(EventType::MouseMove,450,100)},true);
  if(modal)g_frame.modalInput.begin({{430,80},{100,100}});else g_frame.contentMin={430,80};
  CHECK(!draw()&&!g_frame.mousePressConsumed);
 }
 fresh(ctx);frame({event(EventType::MouseDown,450,100)},true);g_frame.mousePressConsumed=true;
 g_contextPressedButtonKeys[ctx.get()]=999;CHECK(!draw()&&g_contextPressedButtonKeys[ctx.get()]==999);
 // Disabled menu rows retain their old consumption policy; disabled/noninteractive
 // image previews remain unable to claim the press.
 if(owner=="menu"||owner=="image"){
  fresh(ctx);enabled=false;frame({event(EventType::MouseDown,450,100),event(EventType::MouseUp,450,100)});
  CHECK(!draw());CHECK(g_frame.mousePressConsumed==(owner=="menu"));
 }
 if(owner=="image"){
  fresh(ctx);enabled=true;interactive=false;frame({event(EventType::MouseDown,450,100)},true);
  CHECK(!draw()&&!g_frame.mousePressConsumed);
 }
}
'''


class GuiManualPressOrigin(unittest.TestCase):
    def test_actual_manual_controls_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC C++23 module consumer")
        proof = ROOT / "Artifacts/Reports/workspace-submenus-oct29/manual-press"
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "gui.manual_press_contract.cpp"
        source.write_text(fixture(), encoding="utf-8")
        portable = ROOT / "Engine/dep/EpochGui"
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(GuiManualPressOrigin LANGUAGES CXX)
add_executable(manual_press "{source.as_posix()}" "{(portable/'src/epochgui/input.cpp').as_posix()}")
target_compile_features(manual_press PRIVATE cxx_std_23)
target_compile_options(manual_press PRIVATE /W3 /WX /utf-8)
target_sources(manual_press PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{portable.as_posix()}" FILES "{(portable/'modules/epoch.gui.ixx').as_posix()}" "{(portable/'modules/epoch.gui.input.ixx').as_posix()}")
enable_testing()
foreach(owner menu toggle link image)
  add_test(NAME actual_${{owner}}_origin COMMAND manual_press ${{owner}})
  set_tests_properties(actual_${{owner}}_origin PROPERTIES TIMEOUT 10)
endforeach()
''', encoding="utf-8")
        build = ROOT / "Artifacts/Intermediate/tests/gui-manual-press-origin"
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=150)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-9000:] + completed.stderr)
        (proof / "result.json").write_text(json.dumps({
            "scope": "Four complete actual control owners; actual queue/begin_frame/source pane and real portable layout; recording paint/font ports",
            "passed": True, "nativeAcceptance": False,
            "owner": {str(OWNER.relative_to(ROOT)): hashlib.sha256(OWNER.read_bytes()).hexdigest()},
            "commands": results,
            "cases": ["destination rendered before source pane", "origin + held move + later release", "stationary click",
                "origin clip/modal/consumed guards", "release outside", "disabled menu/image and noninteractive image policies"],
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
