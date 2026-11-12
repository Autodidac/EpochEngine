"""Real tab layout/input + Editor pane handoff/drop decision; no native window."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.gui.test_gui_queued_drag import fixture as queue_fixture

ROOT = Path(__file__).resolve().parents[2]
API = ROOT / "Engine/modules/gui/gui.engine.ixx"
CHROME = ROOT / "Engine/src/epochgui/controls/gui.navigation_chrome.inl"
DOCK = ROOT / "Engine/src/editor/application/editor.frame_toolbar_dock_targets.inl"
VIEW = ROOT / "Engine/src/editor/application/editor.frame_viewport.inl"


def fixture(before_chrome=None):
    api, chrome, dock, view = (p.read_text(encoding="utf-8") for p in (API, CHROME, DOCK, VIEW))
    types = "\n".join(definition(api, marker, True).replace("export ", "") for marker in (
        "    export struct TabButtonSpec", "    export enum class TabBarPresentation",
        "    export struct TabDragOptions", "    export struct TabBarResult", "    export struct ResponsiveTabBarOptions"))
    tab = definition(chrome,"    [[nodiscard]] static float tab_button_width(") + "\n" + definition(before_chrome or chrome, "    TabBarResult tab_bar_buttons(")
    if before_chrome is not None:
        tab = tab.replace("TabBarResult tab_bar_buttons(", "TabBarResult old_tab_bar_buttons(")
        tab += r'''
TabBarResult tab_bar_buttons(std::span<const TabButtonSpec> tabs,float height,float gap,
 TabBarPresentation presentation,Vec2 keys,const TabDragOptions&){
 return old_tab_bar_buttons(tabs,height,gap,presentation,keys);
}
'''
    ports = r'''
using SpriteHandle=int;
struct Palette{int panelBackground=1,buttonActive=2,buttonHover=3,buttonNormal=4,textFieldActive=5;bool beveledControls{};};
Palette active_palette(){return {};}
constexpr float kFontScale=1,kBoxInnerPadding=5,kContentPadding=6;
float measure_text_width(std::string_view text,float){return float(text.size()*8);}
float base_line_height(float){return 12;}
std::string fit_text_to_width(std::string_view text,float,float){return std::string{text};}
gui_lib::Vec2 to_lib(Vec2 p){return {p.x,p.y};}Vec2 from_lib(gui_lib::Vec2 p){return {p.x,p.y};}
void draw_sprite(int,float,float,float,float){}
void draw_rounded_sprite(int,float,float,float,float,const gui_lib::rounded_rect::RoundedRectStyle&){}
void draw_text_line(std::string_view,float,float,float){}
// Painting is neutral in this pointer-owner fixture. The real Classic/contrast
// paint and font owners run in test_gui_classic_controls.
bool draw_beveled_control(const Palette&,int,float,float,float,float,bool){return false;}
bool draw_high_contrast_control(float,float,float,float,bool){return false;}
void draw_contrast_text(std::string_view s,float x,float y,float scale){draw_text_line(s,x,y,scale);}
struct SelectBoxOptions{std::string_view id,placeholder,selected;std::span<const std::string_view> options;Vec2 size;float row_height{};unsigned max_visible_options{};};
struct SelectBoxResult{bool changed{};std::optional<std::size_t> selected_index;};
SelectBoxResult select_box(const SelectBoxOptions&){return {};}
bool button_with_state(std::string_view,Vec2,bool,bool){return false;}
Vec2 cursor_position(){return g_frame.cursor;}
Vec2 closePosition{},closeSize{};
void draw_close_mark(Vec2 p,Vec2 s,bool=false,bool=false,bool=false){closePosition=p;closeSize=s;}
void set_cursor(Vec2 p){g_frame.cursor=p;}void advance_cursor(Vec2 p){g_frame.cursor.x+=p.x;g_frame.cursor.y+=p.y;}
float content_available_width(float x){return g_frame.contentMax.x-x;}
std::optional<std::size_t> navigate_tab_keys(std::span<const TabButtonSpec>,Vec2,Vec2,Vec2,bool){return {};}
'''
    engine_gui = (ROOT / "Engine/src/epochgui/gui.engine.cpp").read_text(encoding="utf-8")
    ports += definition(engine_gui, "        [[nodiscard]] static std::size_t widget_press_key(")
    ports += definition(engine_gui, "        static void consume_left_press()")
    frame = (ROOT / "Engine/src/epochgui/controls/gui.frame_lifecycle.inl").read_text(encoding="utf-8")
    ports += definition(frame, "    bool is_mouse_down()")
    reset = definition(dock, "        auto reset_pane_title_drag = [&]()") + ";"
    capture = definition(dock, "        auto capture_pane_title_drag = [&](") + ";"
    update = definition(dock, "        auto update_pane_dock_drag = [&]()")
    prefix = update[update.index("{\n") + 2:update.index("            const auto sourcePane")]
    terminal = definition(update, "            if (editor.paneTitleDragFrame.released)\n            {\n                const std::string route")
    activation = view[view.index("            const std::optional<std::size_t> activationIndex ="):
        view.index("            if (tabResult.closed_index")]
    guard = view[view.index("            const bool paneDragAllowed ="):
        view.index("            const std::size_t regionIndex =", view.index("            const bool paneDragAllowed ="))]
    direct_strip = view[view.index("            const gui::ResponsiveTabBarOptions tabOptions{"):
        view.index("            toolGroupTabHeights[regionIndex] =")]
    direct_strip += "\nconst auto tabResult=gui::responsive_tab_bar_buttons(tabOptions);\n"
    tab += "\n"+definition(chrome,"    float responsive_tab_bar_height(")+"\n"+definition(chrome,"    TabBarResult responsive_tab_bar_buttons(")
    base = queue_fixture().replace("namespace gui {\n", "namespace gui {\n", 1)
    base = base.replace("struct Frame{", "struct Frame{Vec2 cursor;std::string windowKey;")
    base = base.replace("constexpr float kCaretBlinkPeriod", types + "\nconstexpr float kCaretBlinkPeriod", 1)
    base = base.replace("namespace gui_lib=epochengine::gui_lib;", "namespace gui_lib=epochengine::gui_lib;\nnamespace epochengine::gui_lib::rounded_rect{struct RoundedRectStyle{bool enabled{};float control_radius{};};}")
    base = base.replace("bool focus=true,pointer=true", "void set_gui_overlay_priority(bool){}\n bool focus=true,pointer=true")
    base = base.replace("}\nusing namespace gui;", ports + tab + "\n}\nusing namespace gui;", 1)
    base = base[:base.index("int main(){auto context=")]
    return base + r'''
enum class EditorToolDockRegion{Left,Right,BottomLeft,BottomRight};
enum class TopMenu{None,Window};
DROP_ENUM
struct EditorState{
 bool paneTitleDragActive{};std::string paneTitleDragRoute,paneTitleDragLabel;
 gui::Vec2 paneTitleDragStart{};gui::DragSurfaceState paneTitleDrag;gui::DragSurfaceResult paneTitleDragFrame;
 TopMenu openMenu{};
};
struct EditorFrameResult{bool scene_input_captured{};};
void pane_contract(){
 auto ctx=std::make_shared<core::Context>();fresh(ctx);EditorState editor;EditorFrameResult result;
 unsigned opened{},docked{};std::string openedRoute,activeRoute;
 const auto set_active_pane=[&](EditorToolDockRegion,std::string_view route){activeRoute=route;};
 const auto request_pane_external_window=[&](std::string_view route,std::string_view){++opened;openedRoute=route;};
 const auto request_pane_region=[&](std::string_view,std::string_view,EditorToolDockRegion){++docked;};
 const std::array tabs{TabButtonSpec{.id="pane.gui_hierarchy",.label="GUI Hierarchy",.width=180,.active=true,.closable=true},
  TabButtonSpec{.id="pane.properties",.label="Properties",.width=140,.closable=true}};
 const std::array<std::string_view,2> routes{"pane.gui_hierarchy","pane.properties"};
 const auto region=EditorToolDockRegion::Left;
 RESET
 CAPTURE
 const auto settle=[&](PaneDropTarget hovered){
 PREFIX
 (void)dragMouse;
 TERMINAL
 };
 const auto bar=[&](std::initializer_list<InputEvent> events,bool held=false,bool enabled=true,bool modal=false,bool menu=false,bool floating=false){
  tick(ctx,events,held);g_frame.cursor={10,10};g_frame.contentMin={0,0};g_frame.contentMax={800,600};
  if(modal){g_frame.modalInput.begin({{0,0},{800,600}});g_frame.modalInput.require_owner_scope();}
  editor.openMenu=menu?TopMenu::Window:TopMenu::None;
  const auto modal_visible_now=[&](){return modal;};const auto floating_gui_visible=[&](){return floating;};
  GUARD
  (void)paneDragAllowed;
  editor.paneTitleDragFrame={};
  const auto tabResult=tab_bar_buttons(tabs,30,1,TabBarPresentation::Document,{},enabled?
   TabDragOptions{&editor.paneTitleDrag,editor.paneTitleDragRoute,12}:TabDragOptions{});
  ACTIVATION
  if(tabResult.dragged_index)capture_pane_title_drag(routes[*tabResult.dragged_index],tabs[*tabResult.dragged_index].label,tabResult);
  return tabResult;
 };
 auto r=bar({});const auto bounds=r.items[0];const Vec2 origin{bounds.position.x+50,bounds.position.y+15};
 const Vec2 destination{400,300};
 r=bar({event(EventType::MouseDown,origin.x,origin.y),event(EventType::MouseMove,destination.x,destination.y),event(EventType::MouseUp,destination.x,destination.y)});
 CHECK(r.dragged_index==0&&r.drag.pressed&&r.drag.released&&!r.closed_index);
 settle(PaneDropTarget::None);CHECK(opened==1&&openedRoute==routes[0]&&activeRoute==routes[0]&&!editor.paneTitleDragActive);
 // Default callers have no drag behavior; stationary selection/close stay owned.
 r=bar({event(EventType::MouseDown,origin.x,origin.y),event(EventType::MouseUp,origin.x,origin.y)},false,false);
 CHECK(r.selected_index==0&&!r.dragged_index&&!r.closed_index);
 r=bar({});const Vec2 close{closePosition.x+closeSize.x/2,closePosition.y+closeSize.y/2};
 r=bar({event(EventType::MouseDown,close.x,close.y),event(EventType::MouseUp,close.x,close.y)});
 CHECK(r.closed_index==1&&!r.dragged_index&&!editor.paneTitleDragActive);settle(PaneDropTarget::None);CHECK(opened==1);
 r=bar({event(EventType::MouseDown,origin.x,origin.y),event(EventType::MouseUp,origin.x,origin.y)});
 CHECK(r.selected_index==0&&r.dragged_index==0&&!r.drag.released);settle(PaneDropTarget::None);CHECK(opened==1&&!editor.paneTitleDragActive);
 r=bar({event(EventType::MouseDown,origin.x,origin.y)},true);CHECK(r.drag.pressed&&editor.paneTitleDrag.pending);settle(PaneDropTarget::None);
 r=bar({event(EventType::MouseMove,origin.x+11,origin.y)},true);settle(PaneDropTarget::None);CHECK(opened==1&&!r.drag.dragging);
 r=bar({event(EventType::MouseMove,origin.x+12,origin.y)},true);CHECK(r.drag.dragging);settle(PaneDropTarget::None);CHECK(opened==1);
 r=bar({event(EventType::MouseUp,destination.x,destination.y)});CHECK(r.drag.released);settle(PaneDropTarget::None);CHECK(opened==2);
 // Body-to-close motion is a drag, never an X click.
 r=bar({});const auto firstLayout=gui_lib::make_tab_button_layout({.strip={.position={10,10},.item_widths=std::array<float,2>{180,140},.height=30,.gap=1},.indicator_height=3,.close_extent=18,.label_padding=6},0,true,true);
 const auto firstClose=firstLayout.close_button.position;
 r=bar({event(EventType::MouseDown,origin.x,origin.y),event(EventType::MouseUp,firstClose.x+8,firstClose.y+8)});
 CHECK(r.drag.released&&!r.closed_index);reset_pane_title_drag();
 // A hidden content header cannot claim clicks in the pane body.
 r=bar({event(EventType::MouseDown,50,70),event(EventType::MouseUp,400,300)});settle(PaneDropTarget::None);
 CHECK(!r.dragged_index&&opened==2&&!editor.paneTitleDragActive);
 r=bar({event(EventType::MouseDown,origin.x,origin.y),event(EventType::MouseUp,400,300)},false,true,true);
 settle(PaneDropTarget::None);CHECK(!r.dragged_index&&opened==2);
 r=bar({event(EventType::MouseDown,origin.x,origin.y)},true);CHECK(editor.paneTitleDrag.pending);
 r=bar({},true,true,true);settle(PaneDropTarget::None);CHECK(!editor.paneTitleDragActive&&!editor.paneTitleDrag.pending&&opened==2);
 for(bool menu:{true,false}){
  r=bar({event(EventType::MouseDown,origin.x,origin.y)},true);CHECK(editor.paneTitleDrag.pending);
  r=bar({},true,true,false,menu,!menu);settle(PaneDropTarget::None);
  CHECK(!editor.paneTitleDragActive&&!editor.paneTitleDrag.pending&&opened==2);
 }
 r=bar({event(EventType::MouseDown,origin.x,origin.y)},true);editor.paneTitleDragRoute="missing";
 r=bar({},true);settle(PaneDropTarget::None);CHECK(!editor.paneTitleDragActive&&!editor.paneTitleDrag.pending&&opened==2);
 // Losing the drawing context also retires an otherwise held tab gesture.
 r=bar({event(EventType::MouseDown,origin.x,origin.y)},true);g_frame.ctx=nullptr;
 r=tab_bar_buttons(tabs,30,1,TabBarPresentation::Document,{},TabDragOptions{&editor.paneTitleDrag,editor.paneTitleDragRoute,12});
 CHECK(!editor.paneTitleDrag.pending&&!r.dragged_index);reset_pane_title_drag();
 // Focus loss cancels the old owner rather than turning a trailing release into a drop.
 r=bar({event(EventType::MouseDown,origin.x,origin.y)},true);CHECK(editor.paneTitleDrag.pending);
 ctx->focus=false;r=bar({{.type=EventType::FocusLost},event(EventType::MouseUp,400,300)});settle(PaneDropTarget::None);
 CHECK(opened==2&&!editor.paneTitleDragActive&&!editor.paneTitleDrag.pending);ctx->focus=true;
 // The second context has an independent drag owner despite the same tab ids.
 r=bar({event(EventType::MouseDown,origin.x,origin.y)},true);CHECK(editor.paneTitleDrag.pending);
 auto other=std::make_shared<core::Context>();fresh(other);DragSurfaceState second;
 tick(other,{event(EventType::MouseDown,origin.x,origin.y),event(EventType::MouseUp,400,300)});g_frame.cursor={10,10};g_frame.contentMin={0,0};g_frame.contentMax={800,600};
 r=tab_bar_buttons(tabs,30,1,TabBarPresentation::Document,{},TabDragOptions{&second,{},12});
 CHECK(r.drag.released&&!second.pending&&editor.paneTitleDrag.pending);
 r=bar({event(EventType::MouseUp,400,300)});CHECK(r.drag.released);settle(PaneDropTarget::None);CHECK(opened==3);
}
void compressed_tabs_contract(){
 auto ctx=std::make_shared<core::Context>();fresh(ctx);EditorState editor;
 constexpr float toolGroupTabHeight=38;const bool paneDragAllowed=true,ownsPaneDrag=true;
 for(float width:{160.0f,320.0f,760.0f}){
  std::array tabs{TabButtonSpec{.id="world",.label="World Outliner",.closable=true},
   TabButtonSpec{.id="assets",.label="Asset Browser",.closable=true},
   TabButtonSpec{.id="gui",.label="GUI Hierarchy",.closable=true},
   TabButtonSpec{.id="scripts",.label="Script Browser",.closable=true}};
  const Vec2 size{width,600};
  const auto draw=[&](std::initializer_list<InputEvent> events){
   tick(ctx,events);g_frame.cursor={3,2};g_frame.contentMin={0,0};g_frame.contentMax={width,600};
   DIRECT_STRIP
   return tabResult;
  };
  auto r=draw({});CHECK(r.items.size()==tabs.size());
  for(std::size_t i=0;i<tabs.size();++i){
   const auto b=r.items[i];CHECK(b.size.x>0&&b.position.x>=3&&b.position.x+b.size.x<=width-2.9f);
   const Vec2 from{b.position.x+2,b.position.y+15},to{width+100,200};
   r=draw({event(EventType::MouseDown,from.x,from.y),event(EventType::MouseMove,to.x,to.y),event(EventType::MouseUp,to.x,to.y)});
   CHECK(r.dragged_index==i&&r.drag.released&&!r.closed_index);
   editor.paneTitleDrag={};editor.paneTitleDragRoute.clear();
  }
 }
}
int main(){pane_contract();compressed_tabs_contract();}
'''.replace("DROP_ENUM", definition(dock, "        enum class PaneDropTarget", True)).replace("RESET", reset).replace("CAPTURE", capture).replace("PREFIX", prefix).replace("TERMINAL", terminal).replace("ACTIVATION", activation).replace("GUARD", guard).replace("DIRECT_STRIP", direct_strip)


class GuiPaneDrag(unittest.TestCase):
    def test_actual_tabs_and_editor_drop_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC C++23 module consumer")
        proof = ROOT / "Artifacts/Reports/gui-pane-drag-oct27"
        build = ROOT / "Artifacts/Intermediate/cmake/gui-pane-drag"
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "gui.pane_drag_contract.cpp"
        source.write_text(fixture(), encoding="utf-8")
        portable = ROOT / "Engine/dep/EpochGui"
        modules = [portable / "modules/epoch.gui.ixx", portable / "modules/epoch.gui.input.ixx"]
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(GuiPaneDrag LANGUAGES CXX)
add_executable(pane_drag "{source.as_posix()}" "{(portable/'src/epochgui/input.cpp').as_posix()}")
target_compile_features(pane_drag PRIVATE cxx_std_23)
target_compile_options(pane_drag PRIVATE /W3 /WX /utf-8)
target_sources(pane_drag PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{portable.as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in modules)})
enable_testing()
add_test(NAME actual_pane_drag COMMAND pane_drag)
set_tests_properties(actual_pane_drag PROPERTIES TIMEOUT 15)
''', encoding="utf-8")
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
        for name in ("editor.frame_outliner.inl", "editor.frame_inspector.inl", "editor.frame_bottom_docks.inl", "editor.frame_assistant_chat.inl"):
            self.assertNotIn("handle_pane_title_drag_to_tab", (ROOT / "Engine/src/editor/application" / name).read_text(encoding="utf-8"))
        (proof / "result.json").write_text(json.dumps({
            "scope": "Actual tab-bar geometry/input + Editor capture/activation/threshold/drop decision, real queue/frame and portable module; no native window",
            "passed": True, "nativeAcceptance": False,
            "owners": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in [API, CHROME, DOCK, VIEW]},
            "commands": results,
            "cases": ["fast completed pane drag", "stationary/default tab selection", "close-icon press excluded",
                "drag release over close does not close", "normal held drag threshold11/12px", "phantom content-header negative",
                "modal/focus cancellation", "foreground pane activation", "two independent context gesture states"],
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
