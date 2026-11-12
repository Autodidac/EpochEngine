"""Real queued input and portable floating chrome; no native window or renderer."""
from pathlib import Path
import hashlib
import json
import subprocess
import time
import unittest
from Tests.support.cpp import configure_arguments
from Tests.gui.test_gui_queued_drag import fixture as queued_fixture
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]

def fixture():
    windows=(ROOT/'Engine/src/epochgui/controls/gui.window_controls.inl').read_text(encoding='utf-8')
    gui=(ROOT/'Engine/src/epochgui/gui.engine.cpp').read_text(encoding='utf-8')
    api=(ROOT/'Engine/modules/gui/gui.engine.ixx').read_text(encoding='utf-8')
    adapter=(ROOT/'Engine/src/epochgui/gui.docking_adapter.cpp').read_text(encoding='utf-8')
    base=queued_fixture().rsplit('int main(){',1)[0].replace('import epoch.gui.input;','import epoch.gui.input;\nimport epoch.gui;')
    base=base.replace('Vec2 contentMin{},contentMax{};bool insideWindow{};', 'Vec2 contentMin{},contentMax{},origin{},windowSize{};std::string windowKey;bool insideWindow{};')
    types='\n'.join(definition(api,marker,True).replace('export ','') for marker in (
        '    export struct WidgetBounds','    export struct FloatingWindowState',
        '    export struct FloatingWindowOptions','    export struct FloatingWindowResult'))
    conv=definition(adapter,'    [[nodiscard]] gui_lib::Vec2 to_lib(Vec2')
    conv+='\n'+'\n'.join(definition(adapter,m) for m in ('    [[nodiscard]] Vec2 from_lib(gui_lib::Vec2','    [[nodiscard]] WidgetBounds from_lib(gui_lib::Rect','    [[nodiscard]] gui_lib::FloatingWindowState to_lib(','    void from_lib(const gui_lib::FloatingWindowState&'))
    replay=windows[windows.index('    struct FloatingWindowPointerReplay final'):windows.index('    static void paint_floating_window_chrome')]
    paint=definition(windows,'    static void paint_floating_window_chrome(')
    facade=definition(windows,'    [[nodiscard]] static gui_lib::FloatingWindowOptions floating_window_core_options(')
    begin=definition(windows,'    FloatingWindowResult begin_floating_window(')
    clip=definition(gui,'        struct ContentClipClearScope',True)
    glyph=definition(gui,'        static void draw_close_mark(')
    return base+'\nnamespace gui {\n'+types+'\n'+conv+r"""
namespace floating_input {bool chrome_owns_press(bool a,bool b,bool c,bool d,bool e){return gui_lib::input::floating_chrome_owns_press(a,b,c,d,e);}}
struct SpriteHandle {int id{};bool is_valid()const{return id!=0;}};
struct Glyph {SpriteHandle handle;Vec2 size_px{10,14};};
struct FontCache {bool asset=true;Glyph glyph;};
struct Resources {FontCache font{true,{{1}}},darkTextFont{true,{{2}}},statusErrorFont{true,{{3}}},statusErrorLightFont{true,{{4}}};}g_resources;
struct Palette {bool isLight{},captionUsesLightText=true;SpriteHandle textFieldActive{6},buttonHover{7},modalScrim{8},headingAccent{9};}palette;
const Palette& active_palette(){return palette;}void ensure_resources(){}
const Glyph* lookup_glyph_from_cache(const FontCache& font,char32_t){return font.asset?&font.glyph:nullptr;}
const Glyph* lookup_shadow_glyph(char32_t){return nullptr;}
constexpr float kFontScale=1;
bool glyph_rect_is_reasonable(float w,float h,float){return std::isfinite(w)&&std::isfinite(h)&&w>0&&h>0;}
int layerDepth{};struct Draw {SpriteHandle sprite;Vec2 position,size;bool in_clip;int layer;};std::vector<Draw> draws;
void draw_sprite(SpriteHandle sprite,float x,float y,float w,float h){draws.push_back({sprite,{x,y},{w,h},x>=g_frame.contentMin.x&&y>=g_frame.contentMin.y&&x+w<=g_frame.contentMax.x&&y+h<=g_frame.contentMax.y,layerDepth});}
"""+clip+'\n'+glyph+'\n'+replay+'\n'+paint+r"""
constexpr float kTitleScale=1.07f,kTitleBarPadding=6,kContentPadding=6;
float line_advance_amount(float){return 20;}
void begin_modal_input_capture(Vec2 pos,Vec2 size){g_frame.modalInput.begin({{pos.x,pos.y},{size.x,size.y}});}
void begin_top_layer(){++layerDepth;}
std::vector<bool> g_floatingWindowTopLayerStack,g_floatingWindowPointerCoverStack;
void begin_window(std::string_view,Vec2 pos,Vec2 size,bool){draw_sprite({99},pos.x,pos.y,size.x,size.y);g_frame.origin=pos;g_frame.windowSize=size;g_frame.contentMin={pos.x+6,pos.y+38};g_frame.contentMax={pos.x+size.x-6,pos.y+size.y-6};}
void set_cursor(Vec2){}
"""+facade+'\n'+begin+r"""
}
const gui_lib::FloatingWindowOptions floatingOptions{.default_position={100,80},.default_size={300,220},.min_size={100,80},.viewport_size={1000,800},.title_bar_height=32,.content_padding=6};
gui_lib::FloatingWindowState window(){gui_lib::FloatingWindowState state;gui_lib::normalize_floating_window(state,floatingOptions);return state;}
void input(const std::shared_ptr<core::Context>& c,std::initializer_list<InputEvent> events,bool down=false,Vec2 final={}){tick(c,events,down,final);g_frame.insideWindow=false;}
void floating_tests(const std::shared_ptr<core::Context>& c){
 auto state=window();fresh(c);
 input(c,{event(EventType::MouseDown,120,90),event(EventType::MouseMove,180,130),event(EventType::MouseUp,180,130)});
 CHECK(!g_frame.justPressed&&!g_frame.justReleased); // Existing stationary-button policy is retained.
 auto result=replay_floating_window_pointer(state,floatingOptions);
 CHECK(result.chrome_press&&result.chrome_release&&result.layout.moved&&state.position.x==160&&state.position.y==120&&!state.dragging);
 // No separate Move event is required for final release motion.
 fresh(c);state=window();input(c,{event(EventType::MouseDown,120,90),event(EventType::MouseUp,180,130)});
 result=replay_floating_window_pointer(state,floatingOptions);CHECK(result.layout.moved&&state.position.x==160&&state.position.y==120);
 // Sustained forty-frame held drag uses the same actual queued producer owner.
 fresh(c);state=window();input(c,{event(EventType::MouseDown,120,90)},false);result=replay_floating_window_pointer(state,floatingOptions);CHECK(state.dragging&&result.chrome_press);
 for(int i=1;i<=40;++i){input(c,{event(EventType::MouseMove,120+2.0f*i,90+1.0f*i)},false);result=replay_floating_window_pointer(state,floatingOptions);CHECK(state.dragging&&state.position.x==100+2.0f*i&&state.position.y==80+1.0f*i);}
 input(c,{event(EventType::MouseUp,200,130)});result=replay_floating_window_pointer(state,floatingOptions);CHECK(result.chrome_release&&!state.dragging);
 // Body controls keep their own press/release. Crossing onto close cannot turn a body press into Close.
 fresh(c);state=window();input(c,{event(EventType::MouseDown,150,150),event(EventType::MouseUp,390,90)});
 result=replay_floating_window_pointer(state,floatingOptions);CHECK(!result.chrome_press&&!result.chrome_release&&state.open&&!result.layout.close_requested&&!g_frame.mousePressConsumed);
 fresh(c);state=window();input(c,{event(EventType::MouseDown,20,20),event(EventType::MouseUp,390,90)});
 result=replay_floating_window_pointer(state,floatingOptions);CHECK(state.open&&!result.chrome_press&&!result.layout.close_requested);
 // Upper-right close uses the real portable rectangle, with unchanged 24x22 hit extent.
 fresh(c);state=window();input(c,{event(EventType::MouseDown,390,81),event(EventType::MouseUp,390,81)});
 result=replay_floating_window_pointer(state,floatingOptions);CHECK(!state.open&&result.layout.close_requested&&result.chrome_press&&result.chrome_release);
 CHECK(result.layout.close_button.position.x==376&&result.layout.close_button.position.y==80&&result.layout.close_button.size.x==24&&result.layout.close_button.size.y==22);
 // Resize in one queued batch, and release beyond the window.
 fresh(c);state=window();input(c,{event(EventType::MouseDown,397,297),event(EventType::MouseUp,447,337)});
 result=replay_floating_window_pointer(state,floatingOptions);CHECK(result.layout.resized&&state.size.x==350&&state.size.y==260&&!state.resizing);
 for(int guard=0;guard<5;++guard){fresh(c);state=window();input(c,{event(EventType::MouseDown,120,90),event(EventType::MouseUp,180,130)});
  if(guard==0)g_frame.mousePressConsumed=true;
  if(guard==1)g_frame.pointerInputAllowed=false;
  if(guard==2){g_frame.modalInput.begin({{0,0},{1000,800}});g_frame.modalInput.require_owner_scope();}
  if(guard==3)g_frame.menuPopupBlocksBackground=true;
  if(guard==4)g_frame.selectPopupCapturedThisFrame=true;
  result=replay_floating_window_pointer(state,floatingOptions);CHECK(!result.chrome_press&&!result.layout.moved&&state.position.x==100&&!state.dragging);
 }
 g_frame.menuPopupBlocksBackground=g_frame.selectPopupCapturedThisFrame=false;
 fresh(c);state=window();input(c,{event(EventType::MouseDown,120,90)},true);(void)replay_floating_window_pointer(state,floatingOptions);
 input(c,{event(EventType::MouseUp,180,130)});g_frame.mouseReleaseConsumed=true;result=replay_floating_window_pointer(state,floatingOptions);CHECK(!state.dragging&&!result.chrome_release&&state.position.x==100);
 // Focus loss cancels a captured gesture, and a foreign held pointer cannot acquire it.
 fresh(c);state=window();input(c,{event(EventType::MouseDown,120,90)},true);(void)replay_floating_window_pointer(state,floatingOptions);
 c->focus=false;input(c,{{.type=EventType::FocusLost}});result=replay_floating_window_pointer(state,floatingOptions);CHECK(!state.dragging&&!result.layout.moved);c->focus=true;
 fresh(c);state=window();input(c,{event(EventType::MouseDown,20,20)},true);(void)replay_floating_window_pointer(state,floatingOptions);
 input(c,{event(EventType::MouseMove,140,90)},true);result=replay_floating_window_pointer(state,floatingOptions);CHECK(!state.dragging&&!result.chrome_press&&state.position.x==100);
 // Sampler-only contexts retain the existing fallback path.
 fresh(c);state=window();input(c,{},true,{120,90});(void)replay_floating_window_pointer(state,floatingOptions);CHECK(state.dragging);
 input(c,{},true,{150,120});(void)replay_floating_window_pointer(state,floatingOptions);CHECK(state.position.x==130&&state.position.y==110);
 input(c,{},false,{150,120});result=replay_floating_window_pointer(state,floatingOptions);CHECK(!state.dragging&&result.chrome_release);
}
void facade_tests(const std::shared_ptr<core::Context>& c){
 fresh(c);FloatingWindowState state;FloatingWindowOptions options;
 options.default_position={100,80};options.default_size={300,220};options.min_size={100,80};options.capture_input=false;
 input(c,{event(EventType::MouseDown,120,90),event(EventType::MouseMove,180,130),event(EventType::MouseUp,180,130)});
 auto result=begin_floating_window(state,options);CHECK(result.begun&&result.moved&&state.position.x==160&&state.position.y==120);
 CHECK(g_frame.mousePressConsumed&&g_frame.mouseReleaseConsumed); // Fast replay owns both edges even though generic stationary flags are false.
 fresh(c);state={};input(c,{event(EventType::MouseDown,150,150),event(EventType::MouseUp,150,150)});
 result=begin_floating_window(state,options);CHECK(result.begun&&!g_frame.mousePressConsumed&&!g_frame.mouseReleaseConsumed&&mouse_press_position()); // Child gets first refusal.
}
void dimmed_window_tests(const std::shared_ptr<core::Context>& c){
 for(bool light:{false,true})for(bool dimmed:{false,true}){
  fresh(c);palette.isLight=light;palette.captionUsesLightText=!light;layerDepth=0;
  FloatingWindowState state;FloatingWindowOptions options;
  CHECK(!options.dim_background);
  options.default_position={100,80};options.default_size={300,220};options.min_size={100,80};
  options.viewport_size={1000,800};options.capture_input=false;options.dim_background=dimmed;
  // A narrow background owner clip must not clip the viewport scrim.
  g_frame.insideWindow=true;g_frame.origin={20,30};g_frame.windowSize={80,60};g_frame.contentMin={26,36};g_frame.contentMax={94,84};
  draws.clear();const auto result=begin_floating_window(state,options);CHECK(result.begun);
  const std::size_t frameStart=dimmed?2u:1u;
  CHECK(draws.size()==(dimmed?9u:4u));
  CHECK(draws[dimmed?1u:0u].sprite.id==99); // Window paints after its optional scrim.
  if(dimmed){
   const auto& scrim=draws[0];CHECK(scrim.sprite.id==8&&scrim.position.x==0&&scrim.position.y==0&&scrim.size.x==1000&&scrim.size.y==800&&scrim.in_clip&&scrim.layer==1);
   for(std::size_t edge=0;edge<4;++edge){const auto& draw=draws[frameStart+edge];CHECK(draw.sprite.id==9&&draw.in_clip&&draw.layer==1);CHECK(draw.size.x==1||draw.size.y==1);}
   CHECK(draws[frameStart].position.x==state.position.x&&draws[frameStart].position.y==state.position.y&&draws[frameStart].size.x==state.size.x);
   CHECK(draws[frameStart+4].sprite.id==(light?2:1)); // Existing recognizable corner close mark.
  }
  CHECK(g_frame.contentMin.x==106&&g_frame.contentMin.y==118&&g_frame.contentMax.x==394&&g_frame.contentMax.y==294);
 }
 palette.isLight=false;palette.captionUsesLightText=true;
}
void chrome_tests(const std::shared_ptr<core::Context>& c){
 fresh(c);g_frame.origin={100,80};g_frame.windowSize={300,220};g_frame.insideWindow=true;
 g_frame.contentMin={106,118};g_frame.contentMax={394,294};
 FloatingWindowState state;FloatingWindowOptions options;FloatingWindowResult result;
 result.close_button={{376,80},{24,22}};result.resize_handle={{384,284},{16,16}};
 draws.clear();paint_floating_window_chrome(state,options,result);
 CHECK(draws.size()==3&&draws[0].sprite.id==1&&draws[0].in_clip&&draws[1].in_clip&&draws[2].in_clip);
 CHECK(g_frame.contentMin.x==106&&g_frame.contentMin.y==118&&g_frame.contentMax.x==394&&g_frame.contentMax.y==294);
 CHECK(draws[0].position.y<102&&draws[0].size.x<24&&draws[0].size.y<22); // Glyph only: no rectangular close background.
 result.close_hovered=true;draws.clear();paint_floating_window_chrome(state,options,result);CHECK(draws[0].sprite.id==3&&draws.size()==3);
 palette.captionUsesLightText=false;draws.clear();paint_floating_window_chrome(state,options,result);CHECK(draws[0].sprite.id==4);
 palette.isLight=true;draws.clear();{ContentClipClearScope clip;draw_close_mark({376,80},{24,22});}CHECK(draws.size()==1&&draws[0].sprite.id==2);
 palette.isLight=false;draws.clear();{ContentClipClearScope clip;draw_close_mark({376,80},{24,22},true);}CHECK(draws.size()==1&&draws[0].sprite.id==3);
}
int main(){auto context=std::make_shared<core::Context>();floating_tests(context);facade_tests(context);dimmed_window_tests(context);chrome_tests(context);return 0;}
"""

class FloatingChrome(unittest.TestCase):
    def test_actual_queue_portable_gesture_and_chrome_clip(self):
        report=ROOT/'Artifacts/Reports/gui-floating-chrome-oct29'/str(time.time_ns());report.mkdir(parents=True)
        source=report/'consumer.cpp';source.write_text(fixture(),encoding='utf-8')
        (report/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(FloatingChrome LANGUAGES CXX)
if(MSVC)
 set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG:NONE /INCREMENTAL:NO")
endif()
set(BUILD_TESTING OFF CACHE BOOL "Only floating chrome owner proof" FORCE)
add_subdirectory("{(ROOT/'Engine/dep/EpochGui').as_posix()}" gui-dependency)
add_executable(floating_chrome "{source.as_posix()}")
target_link_libraries(floating_chrome PRIVATE EpochGui)
target_compile_features(floating_chrome PRIVATE cxx_std_23)
add_executable(portable_geometry "{(ROOT/'Engine/dep/EpochGui/tests/text_control_tests.cpp').as_posix()}")
target_link_libraries(portable_geometry PRIVATE EpochGui)
target_compile_features(portable_geometry PRIVATE cxx_std_23)
if(MSVC)
 target_compile_options(EpochGui PRIVATE /utf-8 /UNDEBUG)
 target_compile_options(portable_geometry PRIVATE /utf-8 /UNDEBUG)
 target_compile_options(floating_chrome PRIVATE /W3 /WX /utf-8 /UNDEBUG)
endif()
enable_testing()
add_test(NAME real_floating_chrome COMMAND floating_chrome)
add_test(NAME portable_control_geometry COMMAND portable_geometry)
set_tests_properties(real_floating_chrome PROPERTIES TIMEOUT 10)
''',encoding='utf-8')
        commands=[configure_arguments(report,report/'build')]
        for config in ('Debug','Release'):
            commands += [['cmake','--build',str(report/'build'),'--config',config,'--parallel','1'],['ctest','--test-dir',str(report/'build'),'-C',config,'--output-on-failure']]
        records=[]
        for i,command in enumerate(commands):
            run=subprocess.run(command,cwd=report,capture_output=True,text=True,timeout=180)
            (report/f'lane-{i}.log').write_text(run.stdout+run.stderr,encoding='utf-8')
            records.append(dict(command=command,exit=run.returncode))
            (report/'result.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
            self.assertEqual(run.returncode,0,run.stdout[-6000:]+run.stderr[-2000:])

if __name__=='__main__':unittest.main()
