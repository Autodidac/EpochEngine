"""Run the real project HUD actions and shared GUI scroll/layout owners on CPU.

Font/glyph, project/package and native-window services are recording ports. No
Editor, generated project, model, renderer, or network service is launched.
"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.gui.test_edit_box_scrolling import fixture as gui_fixture

ROOT = Path(__file__).resolve().parents[2]
PLAY = ROOT / "Engine/src/epoch/implementation/epoch.project_play_scene.inl"
GUI = ROOT / "Engine/src/epochgui"


def hud_prefix(source):
    start = source.index("                if (!m_runtimeControlsVisible)\n")
    end = source.index("                        if (!m_inputProfilePath.empty())", start)
    # The real common/planetary action and diagnostics prefix. The unrelated
    # gameplay/cost inspector is outside this regression's admitted CPU scope.
    return source[start:end] + "\n                        gui::end_scroll_area();\n                    }\n                    gui::end_window();\n                }\n"


def before_exploration_order(source):
    """Restore the exact pre-repair exploration method in the CPU fixture only."""
    current = definition(source, "            void draw_planetary_exploration_controls(")
    previous = current.replace("                if(!m_explorationController)return;", """                if(!m_explorationAdvanceStatus.empty())gui::wrapped_label(m_explorationAdvanceStatus,contentWidth);
                if(!m_explorationController)
                {if(!m_explorationStatus.empty())gui::wrapped_label(m_explorationStatus,contentWidth);return;}""")
    previous = previous.replace("                std::vector<gui::InlineButtonSpec> bodies;", """                std::string anchorName,targetName;
                for(std::uint32_t i=0;i<m_explorationFrame.body_count;++i)
                {const auto& body=m_explorationFrame.bodies[i];if(body.id==m_explorationFrame.anchor)anchorName=body.name;if(body.id==m_explorationFrame.target)targetName=body.name;}
                const auto mode=m_explorationFrame.mode==project_planetary_runtime::TravelMode::walk?"Walk":
                    m_explorationFrame.mode==project_planetary_runtime::TravelMode::cruise?"Cruise":"Flight";
                if(!m_explorationStatus.empty())gui::wrapped_label(m_explorationStatus,contentWidth);
                std::vector<gui::InlineButtonSpec> bodies;""")
    previous = previous.replace("                const std::array essential{", """                gui::wrapped_label("Anchor: "+anchorName+" | Target: "+targetName+" | "+mode,contentWidth);
                gui::wrapped_label(epochengine::format_text("Altitude {:.1f} m  |  Speed {:.1f} m/s",
                    m_explorationFrame.altitude,m_explorationFrame.speed),contentWidth);
                gui::wrapped_label(epochengine::format_text("Time {:.1f} s  /  {:g}x",
                    m_explorationFrame.seconds,m_explorationFrame.time_scale),contentWidth);
                const std::array essential{""")
    previous = previous.replace("                const std::array speed{", """                gui::wrapped_label("Flight: WASD | Roll: Q/E | Up/down: Space/Ctrl | Shift: boost. Walk keeps ground alignment.",contentWidth);
                const std::array speed{""")
    return source.replace(current, previous)


def fixture(source):
    engine = (GUI / "gui.engine.cpp").read_text(encoding="utf-8")
    api = (ROOT / "Engine/modules/gui/gui.engine.ixx").read_text(encoding="utf-8")
    windows = (GUI / "controls/gui.window_controls.inl").read_text(encoding="utf-8")
    navigation = (GUI / "controls/gui.navigation_chrome.inl").read_text(encoding="utf-8")
    text = (GUI / "controls/gui.text_controls.inl").read_text(encoding="utf-8")
    base = gui_fixture().split("\nInputEvent text(", 1)[0]
    window_state = definition(engine, "        struct WindowFrameState", True)
    base = base.replace("std::vector<int> g_windowFrameStack;", window_state + "\nstd::vector<WindowFrameState> g_windowFrameStack;")
    base = base.replace("constexpr float kFontScale=1,kBoxInnerPadding=4,kContentPadding=4,kButtonTextClipInset=0;",
                        "constexpr float kFontScale=1,kBoxInnerPadding=4,kContentPadding=6,kButtonTextClipInset=0,kTitleScale=1.07f,kTitleBarPadding=6;")
    base = base.replace("struct SpriteHandle {int value{};};", "struct SpriteHandle {int value{};bool is_valid()const{return value!=0;}};")
    base = base.replace("struct GuiFontCache {bool asset{};};", "struct Glyph{SpriteHandle handle{1};Vec2 size_px{6,12},offset_px{0,-10};};\nstruct GuiFontCache {bool asset{true};const Glyph* fallbackGlyph{};};")
    base = base.replace("struct {GuiFontCache halfTextLightFont,halfTextFont;}g_resources;",
                        "struct {GuiFontCache halfTextLightFont,halfTextFont,statusInfoFont,statusInfoLightFont,statusSuccessFont,statusSuccessLightFont;SpriteHandle windowShadow;}g_resources;")
    base = base.replace("consoleBackground{5},buttonNormal{6},buttonHover{7},panelBackground{8};",
                        "consoleBackground{5},buttonNormal{6},buttonHover{7},panelBackground{8},titleBar{9},headingAccent{10};GuiFontCache headingFont;")
    base = base.replace("    ScrollAreaResult begin_scroll_area(",
                        "struct ScrollCall{Vec2 position,size;};std::vector<ScrollCall>scrollCalls;\n    ScrollAreaResult begin_scroll_area(")
    scroll_start = base.index("    ScrollAreaResult begin_scroll_area(")
    base = base[:scroll_start] + base[scroll_start:].replace("        const Vec2 pos = g_frame.cursor;",
                        "        const Vec2 pos = g_frame.cursor;scrollCalls.push_back({pos,options.size});", 1)
    extra_api = "\n".join(definition(api, marker, True).replace("export ", "") for marker in (
        "    export struct InlineButtonSpec", "    export enum class InlineButtonSizing",
        "    export enum class InlineButtonAlignment", "    export struct InlineButtonRowOptions"))
    font_owners = "\n".join(definition(engine, marker) for marker in (
        "        [[nodiscard]] static float measure_word_advance(",
        "        [[nodiscard]] static float measure_wrapped_text_height(",
        "        static float draw_wrapped_text("))
    text_owners = "\n".join(definition(text, marker) for marker in (
        "    Vec2 cursor_position()", "    float titled_window_total_height(",
        "    float measure_section_heading_height(", "    void section_heading(",
        "    void label(", "    void wrapped_label(", "    float measure_wrapped_label_height("))
    # Record only the font call boundary. The cursor advance/wrapping code above
    # is unchanged production source, including below-clip early termination.
    text_owners = text_owners.replace("        const float availableWidth = content_available_width(g_frame.cursor.x);\n        const float wrapWidth",
        "        labelCalls.push_back({std::string(text),g_frame.cursor,g_frame.contentMin,g_frame.contentMax});\n        const float availableWidth = content_available_width(g_frame.cursor.x);\n        const float wrapWidth")
    nav_owners = "\n".join(definition(navigation, marker, marker.endswith("Layout")) for marker in (
        "    [[nodiscard]] static std::vector<float> resolve_inline_button_widths(",
        "    struct InlineButtonRowLayout", "    [[nodiscard]] static InlineButtonRowLayout make_inline_button_layout(",
        "    [[nodiscard]] static InlineButtonRowLayout resolve_inline_button_layout(",
        "    float inline_button_row_height(", "    std::optional<std::size_t> inline_button_row("))
    window_owners = "\n".join(definition(windows, marker) for marker in (
        "    void begin_window(std::string_view title, Vec2 position, Vec2 size, bool draw_background, bool show_title_bar)",
        "    void end_window()"))
    runtime_owners = "\n".join(definition(source, marker) for marker in (
        "            [[nodiscard]] std::string_view runtime_terrain_activity()",
        "            [[nodiscard]] float runtime_panel_height()",
        "            [[nodiscard]] bool runtime_panel_captures_pointer(",
        "            [[nodiscard]] gui::WidgetBounds runtime_panel_bounds()",
        "            void draw_runtime_controls_restore()", "            void draw_runtime_controls_hide(",
        "            [[nodiscard]] bool runtime_water_visibility_pending()",
        "            void draw_runtime_environment_controls(", "            void draw_planetary_exploration_controls("))
    return base + r'''
namespace gui {
EXTRA_API
struct LabelCall{std::string text;Vec2 position,clipMin,clipMax;};std::vector<LabelCall>labelCalls;
struct Action{std::string label;WidgetBounds bounds;};std::vector<Action>actions;
std::vector<int>g_floatingWindowTopLayerStack;
const GuiFontCache&active_font_cache(){static GuiFontCache f;return f;}
float baseline_offset(float){return 10;}
struct Keyword{std::string_view word;const GuiFontCache*font{};};Keyword status_keyword_at(std::string_view,std::size_t){return{};}
enum class CodeInk{plain,keyword,value};struct Lexeme{std::size_t end{};CodeInk ink{};};Lexeme code_lexeme_at(std::string_view,std::size_t){return{};}
const Glyph*lookup_glyph(char32_t c){static Glyph glyph;glyph.handle.value=static_cast<int>(c)+100;return &glyph;}
const Glyph*lookup_glyph_from_cache(const GuiFontCache&,char32_t c){return lookup_glyph(c);}
const Glyph*lookup_shadow_glyph(char32_t){return nullptr;}
bool glyph_rect_is_reasonable(float w,float h,float){return w>0&&h>0;}
float align_leading_glyph_to_clip(float x,float,float,float){return x;}
bool glyph_intersects_clip(float x,float y,float w,float h,float l,float t,float r,float b){return x<r&&x+w>l&&y<b&&y+h>t;}
const GuiFontCache*semantic_text_font(SemanticTone){return nullptr;}
void draw_caption_text(std::string_view s,float x,float y,float scale){draw_text_line(s,x,y,scale);}
void draw_sprite_raw(SpriteHandle s,float x,float y,float w,float h){draw_sprite(s,x,y,w,h);}
FONT_OWNERS
void section_heading(std::string_view,SemanticTone=SemanticTone::neutral)noexcept;
TEXT_OWNERS
NAV_OWNERS
void begin_window(std::string_view,Vec2,Vec2,bool,bool)noexcept;
void begin_window(std::string_view title,Vec2 p,Vec2 s)noexcept{begin_window(title,p,s,true,true);}
WINDOW_OWNERS
}
#include "core.format_text.hpp"
namespace project_planetary_runtime {
enum class TravelMode{walk,cruise,flight};
enum class Command{orbit,land,toggle_cruise,toggle_walk,toggle_gravity,toggle_level_assist,toggle_time_pause,cycle_time_rate};
}
struct Settings{bool ocean_surface{true};std::string mode{"planet"};};
struct ExplorationController{std::string encode_session(){return{};}};
struct ExplorationSession{bool ok{};std::string message;};
namespace project_planetary {ExplorationSession save_exploration_session(const std::string&,int,std::string,ExplorationSession){return{};}}
struct Scheduler{bool cancel(int){return true;}};
struct Gameplay{bool enabled{};bool active()const{return enabled;}};
struct Scene {
 float m_runtimeViewportWidth{1280},m_runtimeViewportHeight{720};bool m_runtimeControlsVisible{true};
 std::optional<Gameplay>m_gameplayRuntime;std::string m_tileMapPath;
 std::optional<Settings>m_planetarySettings{Settings{}},m_meshPlanetarySettings{Settings{}};
 std::optional<ExplorationController>m_explorationController{ExplorationController{}};
 std::optional<Scheduler>m_planetaryScheduler;std::optional<int>m_planetaryPending;
 bool m_planetaryFailed{},m_planetaryPaused{},m_planetaryContinue{},m_runtimeInspectionLight{},m_runtimeCloudsVisible{true},m_runtimeWaterVisible{true},m_hasPlanetRecipe{true},m_planetaryPublishedOcean{true};
 int m_planetaryEntity{1},m_planetaryCancellation{},m_planetaryNextTick{},m_screenshotAfterFrames{};
 std::string m_projectName{"Runtime"},m_projectRoot,m_runtimeViewStatus,m_terrainAttachmentStatus,m_screenshotStatus,m_planetaryStatus,m_planetaryWaterStatus,m_explorationStatus,m_explorationAdvanceStatus;
 std::optional<int>m_meshPlanetarySource;ExplorationSession m_explorationSession;
 struct Body{unsigned id;std::string name;};
 struct Frame{unsigned body_count{2},anchor{1},target{2};project_planetary_runtime::TravelMode mode{};double altitude{1.8},speed{0},seconds{0},time_scale{1};bool gravity_enabled{true},level_assist{true};std::array<Body,2>bodies{{{1,"VERDANT"},{2,"MOON"}}};}m_explorationFrame;
 struct Input{unsigned selected_body{};project_planetary_runtime::Command command{};double speed_multiplier{};}m_explorationInput;
 struct Entity{std::string type;bool visible()const{return true;}};std::array<Entity,1>m_entities{{{"Water"}}};
 struct Water{std::string diagnostic;};std::map<core::Context*,Water>m_waterCaches;
 void set_runtime_water_visible(bool value){m_runtimeWaterVisible=value;}
 // Frame-limit widget admission/publishing has its independent actual-module
 // Debug/Release CPU consumer in test_live_frame_limit. This geometry port
 // reserves the fixed header's real label and selector row before scrolling.
 void draw_runtime_frame_limit(const std::shared_ptr<core::Context>&,float){
  gui::label("Foreground FPS (this run)");
  const auto start=gui::cursor_position();gui::set_cursor({start.x,start.y+32.0f});
 }
 RUNTIME_OWNERS
 void draw(const std::shared_ptr<core::Context>&ctx){HUD_PREFIX}
};
CASES
'''.replace("EXTRA_API", extra_api).replace("FONT_OWNERS", font_owners).replace("TEXT_OWNERS", text_owners).replace(
        "NAV_OWNERS", nav_owners).replace("WINDOW_OWNERS", window_owners).replace("RUNTIME_OWNERS", runtime_owners).replace(
        "HUD_PREFIX", hud_prefix(source)).replace("CASES", CASES)


CASES = r'''
using namespace gui;
bool near(float a,float b){return std::abs(a-b)<.01f;}
std::map<std::string,WidgetBounds>recorded;
void draw(Scene&s,const std::shared_ptr<core::Context>&context,int wheel=0,Vec2 pointer={100,300}){
 if(wheel)push_input_for_context(context.get(),{.type=EventType::MouseWheel,.mouse_pos=pointer,.wheel_delta=wheel});
 begin_frame(context,.016f,pointer,false);paints.clear();actions.clear();labelCalls.clear();scrollCalls.clear();
 s.draw(context);CHECK(g_scrollAreaStack.empty()&&g_windowFrameStack.empty());end_frame();
}
std::map<std::string,WidgetBounds>buttons(){
 std::map<std::string,WidgetBounds>result;
 for(const auto&p:paints)if(!p.label.empty())result[p.label]={{p.position.x-6,p.position.y-1},p.size};
 return result;
}
const std::array targets{"Hide Controls","Orbit","Land","Cruise","Walk / Fly","Slower","Faster","Inspection Light: Off","Copy Screenshot [F12]","Retry Terrain LOD"};
void stable(const std::map<std::string,WidgetBounds>&before,const std::map<std::string,WidgetBounds>&after){
 for(const auto target:targets){if(!before.contains(target)||!after.contains(target))std::fprintf(stderr,"Missing action: %s\n",target);
  CHECK(before.contains(target)&&after.contains(target));
  if(!near(before.at(target).position.x,after.at(target).position.x)||!near(before.at(target).position.y,after.at(target).position.y))std::fprintf(stderr,"Moved action: %s %.1f,%.1f to %.1f,%.1f\n",target,before.at(target).position.x,before.at(target).position.y,after.at(target).position.x,after.at(target).position.y);
  CHECK(near(before.at(target).position.x,after.at(target).position.x)&&near(before.at(target).position.y,after.at(target).position.y));}
}
void statuses(Scene&s,bool longStatus){
 std::string status=longStatus?std::string(1000,'x'):"ready";
 s.m_explorationStatus=status;s.m_explorationAdvanceStatus=status;s.m_planetaryStatus=status;s.m_runtimeViewStatus=status;
 s.m_terrainAttachmentStatus=status;s.m_screenshotStatus=status;s.m_planetaryWaterStatus="Water: "+status+"~";
 s.m_explorationFrame.altitude=longStatus?999999999999.8:1.8;s.m_explorationFrame.speed=longStatus?999999999999.0:0;
 s.m_explorationFrame.seconds=longStatus?999999999999.9:0;s.m_explorationFrame.time_scale=longStatus?999999999.0:1;
}
void scenarios(const std::shared_ptr<core::Context>&context){
 for(const auto viewport:{Vec2{1280,900},Vec2{900,720},Vec2{460,480},Vec2{320,560}}){
  g_scrollAreaStates.clear();Scene s;s.m_runtimeViewportWidth=viewport.x;s.m_runtimeViewportHeight=viewport.y;
  statuses(s,false);draw(s,context);const auto before=buttons();const auto bounds=s.runtime_panel_bounds();
  CHECK(bounds.position.x>=0&&bounds.position.y>=0&&bounds.position.x+bounds.size.x<=viewport.x&&bounds.position.y+bounds.size.y<=viewport.y);
  for(int frame=0;frame<12;++frame){statuses(s,(frame%2)==0);s.m_planetaryFailed=(frame%3)==0;s.m_planetaryPaused=(frame%4)==0;
   s.m_planetaryContinue=(frame%5)==0;s.m_planetaryPending=(frame%2)?std::optional<int>{1}:std::nullopt;
   draw(s,context);stable(before,buttons());const auto after=s.runtime_panel_bounds();
   CHECK(near(bounds.size.x,after.size.x)&&near(bounds.size.y,after.size.y));
  }
  statuses(s,true);draw(s,context);
  const auto&area=g_scrollAreaStates.begin()->second;
  CHECK(area.contentHeight>area.viewportHeight&&area.scrollY==0);
  CHECK(scrollCalls.size()==1&&scrollCalls[0].position.y+scrollCalls[0].size.y<=bounds.position.y+bounds.size.y);
  float fullLabelsHeight{};for(const auto&label:labelCalls){fullLabelsHeight+=measure_wrapped_label_height(label.text,bounds.size.x-52);
   CHECK(label.clipMin.x>=bounds.position.x&&label.clipMin.y>=bounds.position.y&&label.clipMax.x<=bounds.position.x+bounds.size.x&&label.clipMax.y<=bounds.position.y+bounds.size.y);}
  CHECK(area.contentHeight>=fullLabelsHeight);
  const auto longExtent=area.contentHeight;
  draw(s,context);CHECK(near(g_scrollAreaStates.begin()->second.contentHeight,longExtent));
  const auto topHide=buttons().at("Hide Controls");
  const auto pointer=Vec2{bounds.position.x+50,scrollCalls[0].position.y+20};
  draw(s,context,-120,pointer);const auto scrolled=g_scrollAreaStates.begin()->second.scrollY;CHECK(scrolled>0);
  CHECK(near(topHide.position.y,buttons().at("Hide Controls").position.y));
  draw(s,context);CHECK(near(g_scrollAreaStates.begin()->second.scrollY,scrolled));
  draw(s,context,-1200000,pointer);auto&bottom=g_scrollAreaStates.begin()->second;
  CHECK(near(bottom.scrollY,bottom.contentHeight-bottom.viewportHeight));
  draw(s,context);CHECK(near(bottom.scrollY,bottom.contentHeight-bottom.viewportHeight));
  bool lastStatusVisible=false;for(const auto&label:labelCalls)if(label.text==s.m_planetaryWaterStatus&&label.position.y<label.clipMax.y&&label.position.y+measure_wrapped_label_height(label.text,268)>label.clipMin.y)lastStatusVisible=true;
  CHECK(lastStatusVisible);CHECK(near(topHide.position.y,buttons().at("Hide Controls").position.y));
  // The unique final glyph was submitted within the actual scroll clip: the
  // exceptional diagnostic's tail is reachable, rather than just measured.
  bool tailVisible=false;for(const auto&paint:paints)if(paint.sprite=='~'+100
   &&paint.position.y>=scrollCalls[0].position.y&&paint.position.y+paint.size.y<=scrollCalls[0].position.y+scrollCalls[0].size.y+.01f)tailVisible=true;
  CHECK(tailVisible);
  // Resize reuses the same shared scroll identity and clamps to its new extent.
  s.m_runtimeViewportHeight=360;draw(s,context);CHECK(g_scrollAreaStates.size()==1);
  CHECK(bottom.scrollY>=0&&bottom.scrollY<=bottom.contentHeight-bottom.viewportHeight+.01f);
  s.m_runtimeControlsVisible=false;const auto hidden=s.runtime_panel_bounds();CHECK(hidden.size.y<=32&&hidden.size.x<=160);
  CHECK(!s.runtime_panel_captures_pointer({1,1})&&s.runtime_panel_captures_pointer({hidden.position.x+1,hidden.position.y+1}));
  const Vec2 hidePointer{hidden.position.x+5,hidden.position.y+5};
  push_input_for_context(context.get(),event(EventType::MouseDown,hidePointer.x,hidePointer.y));
  push_input_for_context(context.get(),event(EventType::MouseUp,hidePointer.x,hidePointer.y));
  draw(s,context,0,hidePointer);CHECK(s.m_runtimeControlsVisible);
 }
 // Invalid/minimal viewport input stays finite and physically bounded.
 for(const auto viewport:{Vec2{1,1},Vec2{100,80},Vec2{NAN,INFINITY}}){Scene s;s.m_runtimeViewportWidth=viewport.x;s.m_runtimeViewportHeight=viewport.y;const auto b=s.runtime_panel_bounds();CHECK(std::isfinite(b.size.x)&&std::isfinite(b.size.y)&&b.size.x>0&&b.size.y>0);}
}
void buffered_diagnostics(const std::shared_ptr<core::Context>&context){
 for(const auto width:{1280.0f,460.0f,320.0f}){
  g_scrollAreaStates.clear();Scene s;s.m_runtimeViewportWidth=width;s.m_runtimeViewportHeight=480;
  const auto bounds=s.runtime_panel_bounds();draw(s,context);
  const Vec2 pointer{bounds.position.x+30,scrollCalls[0].position.y+20};
  draw(s,context,-1200000,pointer);draw(s,context);
  const auto extent=g_scrollAreaStates.begin()->second.contentHeight;
  const auto anchor=g_scrollAreaStates.begin()->second.scrollY;
  CHECK(anchor>0&&near(anchor,extent-g_scrollAreaStates.begin()->second.viewportHeight));
  for(int frame=0;frame<12;++frame){
   const bool lengthy=(frame%3)==0,empty=(frame%3)==1;
   s.m_explorationFrame.altitude=lengthy?-999999999999.9:1.8;
   s.m_explorationFrame.speed=lengthy?999999999999.9:0;
   s.m_explorationFrame.seconds=lengthy?999999999999.9:0;
   s.m_explorationFrame.time_scale=lengthy?999999999999.0:1;
   s.m_explorationAdvanceStatus=empty?"":lengthy?"Planet exploration provider is unavailable; the last camera is retained.":"Ready";
   s.m_explorationStatus=empty?"":lengthy?"Planet exploration needs object Scale 1, 1, 1. Use World Size / Radius to resize it while preserving walking speed and distances.":"Ready";
   s.m_runtimeViewStatus=empty?"":lengthy?"Scene geometry was not admitted. The previous mesh display is retained; check the terrain and asset diagnostics.":"Ready";
   s.m_terrainAttachmentStatus=empty?"":lengthy?"Forest area no longer matches terrain; regenerate/repaint to attach it. Authored poses are retained.":"Ready";
   s.m_screenshotStatus=empty?"":lengthy?"The screenshot could not be copied. The clipboard is busy; try Copy Screenshot again.":"Copied";
   s.m_waterCaches[context.get()].diagnostic=empty?"":lengthy?"Water could not update. The previous surface is retained; check the terrain and asset diagnostics.":"Ready";
   s.m_planetaryStatus=empty?"":lengthy?"Planet LOD 0–24 | 999999 visible patches | 999999 loading | patch cache 999999.9 / 999999 MiB\nLast LOD build: 4/4 workers, 3600000 ms provider":"Planet LOD 0–1 | 1 visible patches | 0 loading | patch cache 0.0 / 128 MiB\nLast LOD build: 1/4 workers, 4 ms provider";
   s.m_planetaryWaterStatus=empty?"":lengthy?"Ocean: waiting for updated terrain. Review the Planetary source update, then Build & Generate.":"Ocean: ready";
   s.m_runtimeInspectionLight=(frame%2)==0;s.m_planetaryFailed=(frame%3)==0;
   s.m_planetaryPaused=(frame%4)==0;s.m_planetaryPending=(frame%2)?std::optional<int>{1}:std::nullopt;
   draw(s,context);const auto&area=g_scrollAreaStates.begin()->second;
   if(!near(area.contentHeight,extent)||!near(area.scrollY,anchor))
    std::fprintf(stderr,"Runtime diagnostic buffer moved: width %.0f frame %d extent %.1f -> %.1f anchor %.1f -> %.1f\n",width,frame,extent,area.contentHeight,anchor,area.scrollY);
   CHECK(near(area.contentHeight,extent)&&near(area.scrollY,anchor));
   CHECK(near(area.scrollY,area.contentHeight-area.viewportHeight));
  }
  // Reflow the same retained area, then keep its resized extent stable as the
  // measured numeric values change again. Width is not a second scroll owner.
  s.m_runtimeViewportWidth=width==320.0f?1280.0f:320.0f;draw(s,context);draw(s,context);
  CHECK(g_scrollAreaStates.size()==1);const auto resizedExtent=g_scrollAreaStates.begin()->second.contentHeight;
  CHECK(near(g_scrollAreaStates.begin()->second.scrollY,resizedExtent-g_scrollAreaStates.begin()->second.viewportHeight));
  s.m_explorationFrame.altitude=-999999999999.9;s.m_explorationFrame.speed=999999999999.9;
  s.m_explorationFrame.seconds=999999999999.9;s.m_explorationFrame.time_scale=999999999999.0;
  draw(s,context);CHECK(near(g_scrollAreaStates.begin()->second.contentHeight,resizedExtent));
  CHECK(near(g_scrollAreaStates.begin()->second.scrollY,resizedExtent-g_scrollAreaStates.begin()->second.viewportHeight));
 }
}
int main(){auto context=std::make_shared<core::Context>();fresh(context);scenarios(context);buffered_diagnostics(context);return 0;}
'''


class RuntimeHud(unittest.TestCase):
    def test_actual_actions_wrapping_scroll_and_viewport(self):
        self.run_source(PLAY.read_text(encoding="utf-8"), "green")

    def test_before_exploration_order_reproduces_moving_controls(self):
        self.run_source(before_exploration_order(PLAY.read_text(encoding="utf-8")), "red-order", "Moved action:")

    def test_before_clipped_cursor_advance_loses_scroll_extent(self):
        source = PLAY.read_text(encoding="utf-8")
        marker = "                            gui::set_cursor({start.x,start.y+(std::max)(textHeight,bufferHeight)});"
        self.assertEqual(source.count(marker), 1)
        source = source.replace(marker, "                            (void)start;(void)textHeight;(void)bufferHeight;")
        self.run_source(source, "red-scroll", "Queued drag contract failed line")

    def test_without_line_buffers_reproduces_bottom_scroll_movement(self):
        source = PLAY.read_text(encoding="utf-8")
        marker = "                            const float bufferHeight=lineBuffer.empty()?0.0f:gui::measure_wrapped_label_height(lineBuffer,width);"
        self.assertEqual(source.count(marker), 1)
        source = source.replace(marker, "                            const float bufferHeight=0.0f;(void)lineBuffer;")
        self.run_source(source, "red-line-buffers", "Runtime diagnostic buffer moved:")

    def run_source(self, source, lane, expected_failure=None):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU module consumer")
        code = fixture(source)
        identity = hashlib.sha256(code.encode()).hexdigest()[:12]
        report = ROOT / "Artifacts/Reports/project-runtime-hud86-nov07" / lane / identity
        report.mkdir(parents=True, exist_ok=True)
        temporary = report / "tmp"
        temporary.mkdir(exist_ok=True)
        env = dict(os.environ, TEMP=str(temporary), TMP=str(temporary), TMPDIR=str(temporary))
        (report / "consumer.cpp").write_text(code, encoding="utf-8")
        portable = ROOT / "Engine/dep/EpochGui"
        modules = [portable / "modules" / name for name in ("epoch.gui.ixx", "epoch.gui.input.ixx", "epoch.gui.rounded_rect.ixx")]
        sources = [portable / "src/epochgui" / name for name in ("text_control.cpp", "gui.text_layout.cpp", "input.cpp", "gui.scrollbar_layout.cpp")]
        (report / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.30)\nproject(RuntimeHud LANGUAGES CXX)\n'
            + 'add_executable(runtime_hud consumer.cpp ' + " ".join('"' + p.as_posix() + '"' for p in sources) + ')\n'
            + 'target_compile_features(runtime_hud PRIVATE cxx_std_23)\n'
            + 'set_property(TARGET runtime_hud PROPERTY MSVC_DEBUG_INFORMATION_FORMAT Embedded)\n'
            + 'target_compile_options(runtime_hud PRIVATE /W3 /WX /utf-8 /EHsc)\n'
            + 'target_sources(runtime_hud PRIVATE FILE_SET CXX_MODULES BASE_DIRS "' + ROOT.as_posix() + '" FILES '
            + " ".join('"' + p.as_posix() + '"' for p in modules) + ')\n'
            + 'target_include_directories(runtime_hud PRIVATE "' + ROOT.as_posix() + '/Engine/include")\n', encoding="utf-8")
        build = ROOT / "Artifacts/Intermediate/cmake/project-runtime-hud86" / lane / identity
        commands = [["cmake", "-S", str(report), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"], [str(build / config / "runtime_hud.exe")]]
        runs = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=report, env=env, capture_output=True, text=True, timeout=120)
            log = report / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            runs.append({"command": command, "exit": completed.returncode, "log": str(log)})
            expected_exit = 81 if expected_failure and index in (2, 4) else 0
            (report / "result.json").write_text(json.dumps({
                "passed": completed.returncode == expected_exit and index == len(commands) - 1,
                "negative_control": expected_failure,
                "temporary_root": str(temporary),
                "scope": "Actual runtime HUD common/planetary actions and diagnostics, stable measured line buffers across empty/short/lengthy status and numeric changes, shared scroll range/bottom anchoring, long diagnostic tail glyph, live width reflow and bounds/Hide/restore; actual core number formatting, GUI window/button/wrapping/measurement/scroll owners with compiled portable scroll geometry. Recording font/glyph/project/package services; excludes gameplay inspector/native window/renderer/model.",
                "owner_sha256": hashlib.sha256(source.encode()).hexdigest(),
                "runs": runs}, indent=2) + "\n", encoding="utf-8")
            self.assertEqual(completed.returncode, expected_exit, completed.stdout[-7000:] + completed.stderr[-1500:])
            if expected_exit:
                self.assertIn(expected_failure, completed.stdout + completed.stderr)


if __name__ == "__main__":
    unittest.main()
