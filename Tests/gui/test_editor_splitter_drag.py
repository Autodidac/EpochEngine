"""Actual Editor divider capture with real queued GUI/portable pointer owner."""
from pathlib import Path
from Tests.support.cpp import configure_arguments, executable
import hashlib
import json
import subprocess
import unittest
from Tests.gui.test_gui_queued_drag import fixture as queue_fixture
from Tests.build.test_opengl_sprite_stream import definition
ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT/"Engine/src/editor/application/editor.frame_toolbar_dock_targets.inl"

def fixture():
    source=OWNER.read_text(encoding="utf-8")
    first=source.index("        const gui::Vec2 mouse = toolbarMouse;")
    last=source.index("        if (modal_visible_now())",first)
    block=source[first:last]
    frame=(ROOT/"Engine/src/epochgui/controls/gui.frame_lifecycle.inl").read_text(encoding="utf-8")
    base=queue_fixture();base=base[:base.rindex("int main(){")]
    base=base.replace("import epoch.gui.input;","import epoch.gui.input;\nimport epoch.gui;")
    base+="\nnamespace gui {\n"+"\n".join(definition(frame,m) for m in ("    Vec2 mouse_position(","    bool is_mouse_down(","    bool was_mouse_pressed("))+"\n}\n"
    routing=(ROOT/"Engine/src/editor/application/editor.application_pane_routing.inl").read_text(encoding="utf-8")
    base+=definition(routing,"        [[nodiscard]] static bool editor_point_in_rect(")
    widget=(ROOT/"Engine/src/epochgui/controls/gui.window_controls.inl").read_text(encoding="utf-8")
    widget=definition(widget,"    void splitter_bar(")
    bottom=(ROOT/"Engine/src/editor/application/editor.frame_bottom_docks.inl").read_text(encoding="utf-8")
    bottom=bottom[:bottom.index('        auto render_console_tool_pane')]
    viewport=(ROOT/"Engine/src/editor/application/editor.frame_viewport.inl").read_text(encoding="utf-8")
    hover=viewport[viewport.rindex('        const bool outlinerSplitHovered'):]
    semantic=definition((ROOT/'Engine/src/epochgui/gui.engine.cpp').read_text(encoding='utf-8'),
                        '        [[nodiscard]] static const GuiResources::SemanticSpritePair& active_semantic_pair(')
    base+=r'''
namespace gui {
struct SpriteHandle {unsigned id{};friend bool operator==(const SpriteHandle&,const SpriteHandle&)=default;};
using SemanticTone=gui_lib::SemanticTone;
struct Palette {SpriteHandle windowBackground{1},buttonActive{2},textFieldActive{3},titleBar{4},panelBackground{5};}palette;
const Palette& active_palette(){return palette;}
bool lightPalette{};bool active_palette_is_light(){return lightPalette;}
struct GuiResources {struct SemanticSpritePair {SpriteHandle background{},accent{};};};
struct {std::array<GuiResources::SemanticSpritePair,gui_lib::semantic_tone_count>semanticLight{},semanticDark{};}g_resources;
struct Draw {SpriteHandle sprite;float x,y,w,h;};std::vector<Draw>draws;
void ensure_resources(){}
void draw_sprite(SpriteHandle sprite,float x,float y,float w,float h){draws.push_back({sprite,x,y,w,h});}
void draw_sprite_raw(SpriteHandle sprite,float x,float y,float w,float h){draw_sprite(sprite,x,y,w,h);}
SEMANTIC
WIDGET
}
'''.replace('SEMANTIC',semantic).replace('WIDGET',widget)
    return base+r'''
namespace logger=epochengine::logger;
enum class EditorLayoutDrag{None,Outliner,Inspector,Dock,BottomColumns};
enum class TopMenu{None,File};
struct EditorState{
 EditorLayoutDrag layoutDrag{};TopMenu openMenu{};
 gui::DragSurfaceState layoutDividerDrag{};
 float bottomGridDragOffset{},layoutDividerDragOffset{};
 float outlinerSplit=.2f,inspectorSplit=.2f,dockSplit=.25f,bottomGridSplit=.5f;
 int surfaceSettleFrames{};
};
bool floating{},modal{},automationInputReserved{};bool floating_gui_visible(){return floating;}bool modal_visible_now(){return modal;}
struct Result{bool scene_input_captured{};};
Result update(EditorState& editor,float width=1000,float height=800){
 const auto ctx=g_frame.ctxShared;Result result;const float w=width,h=height,toolbar_h=84,splitter_w=10,splitter_h=10;
 const bool layout_outliner_visible=true,layout_inspector_visible=true,bottom_visible=true,splitBottomColumns=true;
 const auto bottom=gui_lib::make_bottom_dock_height_layout({.viewport_height=h,.toolbar_height=toolbar_h,.splitter_height=splitter_h,.requested_fraction=editor.dockSplit,.minimum_bottom_height=190,.minimum_center_height=220,.maximum_bottom_fraction=.72f,.visible=true});
 const float bottom_split_h=bottom.splitter_height;
 const gui::Vec2 bottom_pos{0,h-bottom.bottom_height};
 const auto side=gui_lib::make_side_dock_layout({.viewport_width=w,.requested_left_fraction=editor.outlinerSplit,.requested_right_fraction=editor.inspectorSplit,.splitter_width=splitter_w,.minimum_center_width=180,.left_visible=true,.right_visible=true});
 const float right_split_w=side.right_splitter_width;
 const gui::Vec2 outliner_split_pos{side.left_width,toolbar_h},outliner_split_size{side.left_splitter_width,bottom.center_height};
 const gui::Vec2 inspector_split_pos{w-side.right_width-right_split_w,toolbar_h},inspector_split_size{right_split_w,bottom.center_height};
 const gui::Vec2 bottom_split_pos{0,h-bottom.bottom_height-bottom_split_h},bottom_split_size{w,bottom_split_h};
 const gui_lib::BottomDockColumnOptions bottomColumnOptions{.viewport_width=w,.splitter_width=splitter_w,.requested_fraction=editor.bottomGridSplit,.left_visible=true,.right_visible=true};
 const auto columns=gui_lib::make_bottom_dock_column_layout(bottomColumnOptions);
 const gui::Vec2 bottomColumnsSplitPosition{columns.left_width,bottom_pos.y},bottomColumnsSplitSize{columns.splitter_width,bottom.bottom_height};
 const gui::Vec2 toolbarMouse=gui::mouse_position();
 BLOCK
 HOVER
 gui::draws.clear();
 DRAW
 return result;
}
void input(const std::shared_ptr<core::Context>& ctx,std::initializer_list<InputEvent> events,bool down=false,Vec2 mouse={}){
 tick(ctx,events,down,mouse);g_frame.insideWindow=false;
}
void divider_contract(){
 for(std::size_t index=0;index<gui_lib::semantic_tone_count;++index){
  const auto tone=static_cast<gui_lib::SemanticTone>(index);
  const auto rgb=[](const auto&color){return 0xff000000u|(unsigned(color.r)<<16)|(unsigned(color.g)<<8)|color.b;};
  g_resources.semanticLight[index].accent={rgb(gui_lib::semantic_colors(tone,true).accent)};
  g_resources.semanticDark[index].accent={rgb(gui_lib::semantic_colors(tone,false).accent)};
 }
 const auto grip_colors=[](bool enabled){
  CHECK(draws.size()==28);const auto expected=enabled?palette.buttonActive:active_semantic_pair(SemanticTone::muted).accent;
  for(std::size_t divider=0;divider<4;++divider)for(std::size_t grip=4;grip<7;++grip)
   CHECK(draws[divider*7+grip].sprite==expected);
 };
 auto ctx=std::make_shared<core::Context>();fresh(ctx);EditorState e;
 // The actual shared draw routine honors disabled state in both orientations,
 // even if a stale caller still reports hover/active, with each theme palette.
 for(const bool light:{false,true}){
  lightPalette=light;palette.buttonActive={light?12u:2u};palette.textFieldActive={light?13u:3u};
  for(const bool enabled:{false,true})for(const bool hovered:{false,true})for(const bool active:{false,true})
   for(const Vec2 size:std::array<Vec2,2>{{{10,200},{200,10}}}){
    draws.clear();splitter_bar({10,20},size,hovered,active,enabled);CHECK(draws.size()==7);
    const auto muted=active_semantic_pair(SemanticTone::muted).accent;
    CHECK(draws[2].sprite==(!enabled?muted:active?palette.buttonActive:palette.textFieldActive));
    for(std::size_t i=4;i<7;++i)CHECK(draws[i].sprite==(enabled?palette.buttonActive:muted));
   }
  fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,205,120)},true);
  CHECK(update(e).scene_input_captured);grip_colors(true);const auto heldFraction=e.outlinerSplit;
  automationInputReserved=true;input(ctx,{event(EventType::MouseMove,405,120)},true);
  CHECK(!update(e).scene_input_captured&&e.layoutDrag==EditorLayoutDrag::None&&!e.layoutDividerDrag.pending);
  CHECK(e.outlinerSplit==heldFraction);grip_colors(false);
  automationInputReserved=false;input(ctx,{event(EventType::MouseMove,505,120)},true);
  CHECK(!update(e).scene_input_captured&&e.layoutDrag==EditorLayoutDrag::None&&e.outlinerSplit==heldFraction);grip_colors(true);
  input(ctx,{event(EventType::MouseUp,505,120)});(void)update(e);
  input(ctx,{event(EventType::MouseDown,205,120),event(EventType::MouseMove,305,120)},true);
  CHECK(update(e).scene_input_captured&&std::abs(e.outlinerSplit-.3f)<.00001);grip_colors(true);
  for(unsigned gate=0;gate<3;++gate){
   fresh(ctx);e={};floating=gate==0;modal=gate==1;if(gate==2)e.openMenu=TopMenu::File;
   input(ctx,{event(EventType::MouseDown,205,120)},true);CHECK(!update(e).scene_input_captured);
   CHECK(e.layoutDrag==EditorLayoutDrag::None);grip_colors(false);floating=modal=false;
  }
 }
 lightPalette=false;palette.buttonActive={2};palette.textFieldActive={3};fresh(ctx);e={};
 // Down is on the 10px divider; final sampled pointer is already outside it.
 input(ctx,{event(EventType::MouseDown,205,120),event(EventType::MouseMove,305,120)},true);
 CHECK(update(e).scene_input_captured);CHECK(e.layoutDrag==EditorLayoutDrag::Outliner);
 CHECK(std::abs(e.outlinerSplit-.3f)<.00001);
 input(ctx,{event(EventType::MouseMove,405,120)},true);CHECK(update(e).scene_input_captured);
 CHECK(std::abs(e.outlinerSplit-.4f)<.00001); // reflow moved the bar, gesture identity survives
 input(ctx,{event(EventType::MouseUp,405,120)});CHECK(update(e).scene_input_captured);
 CHECK(e.layoutDrag==EditorLayoutDrag::None&&!e.layoutDividerDrag.pending);
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,205,120),event(EventType::MouseMove,305,120),event(EventType::MouseUp,305,120)});
 CHECK(update(e).scene_input_captured&&std::abs(e.outlinerSplit-.3f)<.00001&&e.layoutDrag==EditorLayoutDrag::None);
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,205,120),event(EventType::MouseMove,305,120),event(EventType::MouseUp,305,120),event(EventType::MouseMove,705,120)});
 CHECK(update(e).scene_input_captured&&std::abs(e.outlinerSplit-.3f)<.00001); // later hover cannot move the released divider
 // No late hover acquisition when the actual press began in panel content.
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,150,120),event(EventType::MouseMove,205,120)},true);
 CHECK(!update(e).scene_input_captured&&e.layoutDrag==EditorLayoutDrag::None&&e.outlinerSplit==.2f);
 // Other directions share the same queue/capture owner and preserve grab offsets.
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,795,120),event(EventType::MouseMove,695,120),event(EventType::MouseUp,695,120)});
 CHECK(update(e).scene_input_captured&&std::abs(e.inspectorSplit-.3f)<.00001);
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,100,605),event(EventType::MouseMove,100,505),event(EventType::MouseUp,100,505)});
 CHECK(update(e).scene_input_captured&&e.dockSplit>.41f&&e.dockSplit<.42f);
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,500,700),event(EventType::MouseMove,600,700),event(EventType::MouseUp,600,700)});
 CHECK(update(e).scene_input_captured&&e.bottomGridSplit>.59f&&e.bottomGridSplit<.61f);
 // Modal/floating ownership and focus loss cancel instead of dragging behind UI.
 fresh(ctx);e={};modal=true;input(ctx,{event(EventType::MouseDown,205,120)},true);CHECK(!update(e).scene_input_captured&&e.layoutDrag==EditorLayoutDrag::None);modal=false;
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,205,120)},true);CHECK(update(e).scene_input_captured);
 ctx->focus=false;input(ctx,{{.type=EventType::FocusLost},event(EventType::MouseUp,405,120)});CHECK(!update(e).scene_input_captured&&e.layoutDrag==EditorLayoutDrag::None);
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,205,120)},true);CHECK(update(e).scene_input_captured);
 floating=true;input(ctx,{},true,{405,120});CHECK(!update(e).scene_input_captured&&e.layoutDrag==EditorLayoutDrag::None&&!e.layoutDividerDrag.pending);floating=false;
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,205,120)},true);CHECK(update(e).scene_input_captured);
 auto other=std::make_shared<core::Context>();fresh(other);EditorState second;input(other,{});
 CHECK(!update(second).scene_input_captured&&second.layoutDrag==EditorLayoutDrag::None);
 input(ctx,{event(EventType::MouseMove,305,120),event(EventType::MouseUp,305,120)});
 CHECK(update(e).scene_input_captured&&std::abs(e.outlinerSplit-.3f)<.00001&&second.outlinerSplit==.2f);
 // Near-edge acquisition uses the same 16px grip for all four dividers.
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,198,120),event(EventType::MouseMove,298,120),event(EventType::MouseUp,298,120)});
 CHECK(update(e).scene_input_captured&&std::abs(e.outlinerSplit-.3f)<.00001);
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,802,120),event(EventType::MouseMove,702,120),event(EventType::MouseUp,702,120)});
 CHECK(update(e).scene_input_captured&&std::abs(e.inspectorSplit-.3f)<.00001);
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,100,598),event(EventType::MouseMove,100,498),event(EventType::MouseUp,100,498)});
 CHECK(update(e).scene_input_captured&&e.dockSplit>.41f&&e.dockSplit<.42f);
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,493,700),event(EventType::MouseMove,593,700),event(EventType::MouseUp,593,700)});
 CHECK(update(e).scene_input_captured&&e.bottomGridSplit>.59f&&e.bottomGridSplit<.61f);
 // Both outside edges of every 16px grip preserve all original fractions.
 for(const Vec2 outside:std::array<Vec2,8>{{{196,120},{214,120},{786,120},{804,120},{100,596},{100,614},{491,700},{509,700}}}){
  fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,outside.x,outside.y),event(EventType::MouseMove,outside.x+20,outside.y+20),event(EventType::MouseUp,outside.x+20,outside.y+20)});
  CHECK(!update(e).scene_input_captured&&e.layoutDrag==EditorLayoutDrag::None);
  CHECK(e.outlinerSplit==.2f&&e.inspectorSplit==.2f&&e.dockSplit==.25f&&e.bottomGridSplit==.5f);
 }
 // The small grip cannot acquire a press beyond its edge or behind other owners.
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,196,120),event(EventType::MouseMove,296,120),event(EventType::MouseUp,296,120)});
 CHECK(!update(e).scene_input_captured&&e.layoutDrag==EditorLayoutDrag::None&&e.outlinerSplit==.2f);
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,198,120),event(EventType::MouseMove,298,120)},true);g_frame.mousePressConsumed=true;
 CHECK(!update(e).scene_input_captured&&e.layoutDrag==EditorLayoutDrag::None);
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,198,120),event(EventType::MouseMove,298,120)},true);
 g_frame.insideWindow=true;g_frame.contentMin={200,84};g_frame.contentMax={1000,800};
 CHECK(!update(e).scene_input_captured&&e.layoutDrag==EditorLayoutDrag::None);
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,198,120),event(EventType::MouseMove,298,120)},true);
 g_frame.modalInput.begin({{400,100},{200,200}});
 CHECK(!update(e).scene_input_captured&&e.layoutDrag==EditorLayoutDrag::None);
 fresh(ctx);e={};e.openMenu=TopMenu::File;input(ctx,{event(EventType::MouseDown,198,120),event(EventType::MouseMove,298,120)},true);
 CHECK(!update(e).scene_input_captured&&e.layoutDrag==EditorLayoutDrag::None);
 // A 100ms build-throttled frame may receive many adjacent moves. The real
 // queue coalesces motion, preserves both edges, and the divider consumes once.
 fresh(ctx);e={};push_input_for_context(ctx.get(),event(EventType::MouseDown,198,120));
 for(int step=1;step<=100;++step)push_input_for_context(ctx.get(),event(EventType::MouseMove,198.0f+step,120));
 push_input_for_context(ctx.get(),event(EventType::MouseUp,298,120));
 CHECK(g_contextPendingEvents[ctx.get()].size()==3);
 begin_frame(ctx,.1f,{},false);g_frame.insideWindow=false;
 CHECK(update(e).scene_input_captured&&std::abs(e.outlinerSplit-.3f)<.00001&&e.layoutDrag==EditorLayoutDrag::None);
 CHECK(!update(e).scene_input_captured); // consumed burst cannot update twice
 // Trace is off by default, bounded when enabled, and diagnostic failures
 // cannot prevent the real divider state transition.
 CHECK(traceRecords.empty());core::cli::trace_menu_button0_rect=true;traceThrows=true;
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,205,120),event(EventType::MouseMove,305,120)},true);
 CHECK(update(e).scene_input_captured&&std::abs(e.outlinerSplit-.3f)<.00001);
 traceThrows=false;modal=true;input(ctx,{event(EventType::MouseMove,315,120)},true);(void)update(e);modal=false;
 CHECK(std::ranges::any_of(traceRecords,[](const auto& row){return row.tag=="PointerTrace.Divider"&&row.message.find("blocked_modal_menu_floating=1,1,")!=std::string::npos;}));
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,205,120)},true);(void)update(e);
 for(unsigned frame=0;frame<130;++frame){input(ctx,{event(EventType::MouseMove,205.0f+frame*.1f,120)},true);(void)update(e);}
 CHECK(std::ranges::count_if(traceRecords,[](const auto& row){return row.tag=="PointerTrace.Divider";})<=120);
 core::cli::trace_menu_button0_rect=false;traceRecords.clear();g_pointerFrameTrace={};fresh(ctx);
 // Existing workspace/host-resize cancellation clears the shared gesture too.
 fresh(ctx);e={};input(ctx,{event(EventType::MouseDown,205,120)},true);CHECK(update(e).scene_input_captured);
 e.layoutDrag=EditorLayoutDrag::None;input(ctx,{},true,{405,120});CHECK(!update(e).scene_input_captured&&!e.layoutDividerDrag.pending);
}
int main(){divider_contract();}
'''.replace("BLOCK",block).replace('HOVER',hover).replace('DRAW',bottom)

class EditorSplitterDrag(unittest.TestCase):
 def test_actual_queued_divider_capture(self):
  code=fixture();key=hashlib.sha256(code.encode()).hexdigest()[:12]
  report=ROOT/"Artifacts/Reports/editor-divider-capture-oct28"/key;report.mkdir(parents=True,exist_ok=True)
  build=ROOT/"Artifacts/Intermediate/cmake/editor-divider-capture"/key
  (report/"consumer.cpp").write_text(code,encoding="utf-8")
  portable=ROOT/"Engine/dep/EpochGui"
  (report/"CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(EditorDivider LANGUAGES CXX)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
set(BUILD_TESTING OFF CACHE BOOL "Only the divider CPU contract" FORCE)
add_subdirectory("{portable.as_posix()}" gui-dependency)
add_executable(divider consumer.cpp)
target_link_libraries(divider PRIVATE EpochGui)
target_compile_features(divider PRIVATE cxx_std_23)
if(MSVC)
 target_compile_options(EpochGui PRIVATE /utf-8 /UNDEBUG)
 target_compile_options(divider PRIVATE /W3 /WX /utf-8 /UNDEBUG)
else()
 target_compile_options(EpochGui PRIVATE -UNDEBUG)
 target_compile_options(divider PRIVATE -UNDEBUG)
endif()
''',encoding="utf-8")
  commands=[configure_arguments(report,build)]
  for mode in ("Debug","Release"):
   commands.extend([["cmake","--build",str(build),"--config",mode,"--parallel","1"],[str(executable(build,mode,"divider"))]])
  runs=[]
  for i,cmd in enumerate(commands):
   r=subprocess.run(cmd,cwd=report,capture_output=True,text=True,timeout=100)
   (report/f"lane-{i}.log").write_text(r.stdout+r.stderr,encoding="utf-8");runs.append({"command":cmd,"exit":r.returncode})
   (report/"result.json").write_text(json.dumps({"passed":r.returncode==0 and i==len(commands)-1,"runs":runs,"owner_sha256":hashlib.sha256(OWNER.read_bytes()).hexdigest()},indent=2),encoding="utf-8")
   self.assertEqual(r.returncode,0,r.stdout[-6000:]+r.stderr[-2000:])
if __name__=="__main__":unittest.main()
