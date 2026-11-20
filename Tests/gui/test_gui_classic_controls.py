"""Execute real Classic painting, clipping, button and inset-field owners on CPU."""
from pathlib import Path
from Tests.support.cpp import compile_and_run
import hashlib
import json
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


class ClassicControls(unittest.TestCase):
    def test_real_painting_preserves_bounds_and_uses_distinct_caption_font(self):
        source = (ROOT/'Engine/src/epochgui/gui.engine.cpp').read_text(encoding='utf-8')
        buttons = (ROOT/'Engine/src/epochgui/controls/gui.buttons_images.inl').read_text(encoding='utf-8')
        scroll = (ROOT/'Engine/src/epochgui/controls/gui.scroll_controls.inl').read_text(encoding='utf-8')
        text = (ROOT/'Engine/src/epochgui/controls/gui.text_controls.inl').read_text(encoding='utf-8')
        windows = (ROOT/'Engine/src/epochgui/controls/gui.window_controls.inl').read_text(encoding='utf-8')
        chrome = (ROOT/'Engine/src/epochgui/controls/gui.navigation_chrome.inl').read_text(encoding='utf-8')
        self.assertEqual(windows.count('draw_window_caption('), 2)
        self.assertIn('draw_caption_text(display, itemBounds.position.x + inset, textY, kFontScale)', chrome)
        fields = definition(text, '        if (!integrated_composer)')
        begin = text.index('        const auto editorField = active ?')
        editor_field = text[begin:text.index('\n\n', begin)]
        paint = '\n'.join(definition(source, marker) for marker in (
            '        static void draw_sprite_raw(',
            '        static void draw_window_shadow(',
            '        static void draw_rounded_sprite(',
            '        [[nodiscard]] static bool draw_beveled_control(',
            '        [[nodiscard]] static bool draw_high_contrast_control(',
            '        [[nodiscard]] static float centered_text_top(',
            '        [[nodiscard]] static const GuiFontCache* caption_text_font(',
            '        static void draw_caption_text(',
            '        static void draw_window_caption(',
            '        static void draw_contrast_text('))
        shadow_constants = '\n'.join(line for line in source.splitlines()
                                    if 'constexpr std::uint32_t kWindowShadow' in line)
        utf8 = '\n'.join((
            definition(source, '        struct DecodedUtf8Codepoint', True),
            definition(source, '        [[nodiscard]] static bool is_utf8_continuation_byte('),
            definition(source, '        [[nodiscard]] static DecodedUtf8Codepoint decode_utf8_codepoint(')))
        fit_text = definition(source, '        [[nodiscard]] static std::string fit_text_to_width(')
        wrap_measure = '\n'.join(definition(source, marker) for marker in (
            '        [[nodiscard]] static char32_t safe_draw_codepoint(',
            '        [[nodiscard]] static std::optional<char32_t> next_drawable_char(',
            '        [[nodiscard]] static bool is_wrap_space(',
            '        [[nodiscard]] static float measure_word_advance(',
            '        [[nodiscard]] static float measure_wrapped_text_height('))
        button_start=buttons.index('    static bool button_with_state(')
        button = buttons[button_start:buttons.index('\n    bool button(',button_start)]
        gui_api=(ROOT/'Engine/modules/gui/gui.engine.ixx').read_text(encoding='utf-8')
        icon_api=definition(gui_api,'    export enum class ButtonIcon').replace('export ','',1)+';'
        authored_api='\n'.join(definition(gui_api, marker, True).replace('export ', '', 1) for marker in (
            '    export struct Color', '    export struct AuthoredButtonStyle'))
        authored_surface=definition(source,'        struct AuthoredButtonSurface final',True)
        gui_lib = (ROOT/'Engine/dep/EpochGui/modules/epoch.gui.ixx').read_text(encoding='utf-8')
        translucent_surface = '\n'.join(definition(gui_lib, marker, is_type) for marker, is_type in (
            ('    struct Rgba8', True), ('    [[nodiscard]] inline Rgba8 translucent_surface(', False)))
        authored_face=definition(source,'        [[nodiscard]] static bool draw_authored_button_face(')
        icon_paint=definition(buttons,'    static void draw_button_icon(')+definition(buttons,'    static void draw_button_tooltip(')
        latest=definition(scroll, '        if (latestFits && (options.conversation_style || outputLatestRail)')
        geometryStart = scroll.index('        const float scrollbarWidth = options.lines.empty()')
        scrollGeometry = scroll[geometryStart:scroll.index('        const float linePitch', geometryStart)]
        palette = definition(source, '            struct PaletteSprites', True)
        roundedCorners = definition(source, '        struct RoundedCorners', True)
        registration = definition(source, '                const auto registerPalette = [&atlas](')
        start = source.index('                g_resources.contrastBlack = add_sprite(')
        contrast_registration = source[start:source.index('\n\n', start)]
        code = r'''
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "SPEC"
void require(bool value,int line) noexcept {if(!value)std::exit(line);}
#define CHECK(...) require(static_cast<bool>((__VA_ARGS__)),__LINE__)
namespace theme_detail=epochengine::gui::theme_detail;
struct Vec2{float x{},y{};};
ICONS_API
AUTHORED_API
namespace gui_lib { TRANSLUCENT_SURFACE }
struct SpriteHandle{int id{};bool is_valid()const{return id!=0;}operator int()const{return id;}};
struct WidgetBounds{Vec2 position{},size{};};
struct QueuedSpriteDraw{SpriteHandle handle;float x{},y{},w{},h{};};
struct TextureAtlas{};
std::vector<QueuedSpriteDraw> draws;
std::unordered_map<int,theme_detail::Rgb> colors;
namespace epochengine::atlasmanager {std::vector<const TextureAtlas*> get_atlas_vector_snapshot(){return {};}}
struct Context{
 bool windowData{};
 void draw_sprite_safe(SpriteHandle h,std::span<const TextureAtlas* const>,float x,float y,float w,float height,bool pixels){CHECK(pixels);draws.push_back({h,x,y,w,height});}
};
struct Frame{
 Context*ctx{};std::shared_ptr<Context>ctxShared;bool insideWindow{true},mouseDown{},justReleased{};
 Vec2 cursor{10,20},mousePos{},contentMin{0,0},contentMax{200,100};
 Vec2 origin{},windowSize{200,100};bool pointerInputAllowed{};
 int topLayerDepth{};std::vector<QueuedSpriteDraw>topLayerDraws,queuedDraws;
 std::optional<WidgetBounds>lastButtonBounds;
}g_frame;
struct GuiFontCache{
 int id{};bool asset{true};std::string fontName{};
 float height{},ascent{},glyphWidth{5};
};
ROUNDED_CORNERS
struct GuiResources{
 PALETTE
 PaletteSprites current{};GuiFontCache font{1},darkTextFont{2};
 SpriteHandle contrastBlack{},contrastWhite{};std::unordered_map<int,SpriteHandle>circleControls;
 std::unordered_map<int,RoundedCorners>roundedControls;
 std::array<std::array<SpriteHandle,3>,3> windowShadow{{{{-1,-2,-3}},{{-4,0,-5}},{{-6,-7,-8}}}};
 struct Pair{SpriteHandle background{},accent{};}semantic;
}g_resources;
const GuiResources::PaletteSprites& active_palette(){return g_resources.current;}
const GuiFontCache*g_temporaryTextFont{};
struct DrawText{std::string text;float x{},y{};int font{};float scale{};int layer{};};
std::vector<DrawText>textDraws;
void draw_text_line(std::string_view s,float x,float y,float scale){textDraws.push_back({std::string{s},x,y,g_temporaryTextFont?g_temporaryTextFont->id:g_resources.current.isLight?2:1,scale,g_frame.topLayerDepth});}
constexpr float kMinQueuedSpriteExtent=.001f,kMaxQueuedSpriteExtent=1e8f;
bool clipped=true;
bool has_content_clip(){return clipped;}
int nextSprite{};
int make_solid_pixels(std::uint8_t r,std::uint8_t g,std::uint8_t b,std::uint8_t a,int,int){int id=++nextSprite;colors[id]={r,g,b,a};return id;}
SpriteHandle add_sprite(TextureAtlas&,const std::string&,int id,int,int){return {id};}
void register_rounded_control(TextureAtlas&,SpriteHandle h,const std::string&,theme_detail::Rgb c){const SpriteHandle circle{++nextSprite};colors[circle.id]=c;g_resources.circleControls[h.id]=circle;}
namespace gui_lib::rounded_rect{
struct RoundedRectStyle{bool enabled{};float control_radius{};};
RoundedRectStyle normalize_rounded_rect_style(RoundedRectStyle style){return style;}
}
bool rounded_rectangles_enabled(){return false;}
constexpr float kFontScale=1,kContentPadding=4,kBoxInnerPadding=4,kButtonTextClipInset=1;
float fontHeight=10;
float base_line_height(float scale){
 return (g_temporaryTextFont&&g_temporaryTextFont->height>0 ? g_temporaryTextFont->height : fontHeight)*scale;
}
float baseline_offset(float scale){
 return (g_temporaryTextFont&&g_temporaryTextFont->ascent>0 ? g_temporaryTextFont->ascent : fontHeight*.75f)*scale;
}
float space_advance(float){return 5;}
UTF8
float measure_text_width(std::string_view text,float scale){
 std::size_t count{};
 for(std::size_t i{};i<text.size();++count) i+=decode_utf8_codepoint(text,i).byte_count;
 return static_cast<float>(count)*(g_temporaryTextFont?g_temporaryTextFont->glyphWidth:5.0f)*scale;
}
FIT_TEXT
void ensure_resources(){}
float line_advance_amount(float scale){return (fontHeight+2)*scale;}
float glyph_advance_with_kerning(char32_t,std::optional<char32_t>,float scale){return 5*scale;}
WRAP_MEASURE
struct ContentClipScope{
 Vec2 oldMin{g_frame.contentMin},oldMax{g_frame.contentMax};
 ContentClipScope(Vec2 lo,Vec2 hi){g_frame.contentMin=lo;g_frame.contentMax=hi;}
 ~ContentClipScope(){g_frame.contentMin=oldMin;g_frame.contentMax=oldMax;}
};
// This palette fixture covers single visible rows; the wrapping renderer is
// exercised separately by test_tooltip_wrapping with production glyph layout.
void draw_wrapped_text(std::string_view s,float x,float y,float,float scale){draw_text_line(s,x,y,scale);}
bool point_in_rect(Vec2 p,float x,float y,float w,float h){return p.x>=x&&p.y>=y&&p.x<x+w&&p.y<y+h;}
bool point_in_active_clip(Vec2 p){
 return !has_content_clip()||point_in_rect(p,g_frame.contentMin.x,g_frame.contentMin.y,
  g_frame.contentMax.x-g_frame.contentMin.x,g_frame.contentMax.y-g_frame.contentMin.y);
}
bool point_in_modal_input_capture(Vec2){return true;}
bool point_in_tooltip_capture(Vec2 point){return g_frame.pointerInputAllowed&&point_in_modal_input_capture(point);}
void begin_top_layer(){++g_frame.topLayerDepth;}void end_top_layer(){--g_frame.topLayerDepth;}
std::string lastPressLabel;std::size_t widget_press_key(std::string_view label,Vec2,Vec2){lastPressLabel=label;return 42;}
std::unordered_map<void*,std::size_t>g_contextPressedButtonKeys;
bool left_press_available(){return false;}
bool releaseAvailable{};bool left_release_available(){return releaseAvailable;}
std::optional<Vec2>mouse_press_position(){return {};}
void consume_left_press(){}void consume_left_release(){}
void set_cursor(Vec2 position){g_frame.cursor=position;}
void advance_cursor(Vec2 v){g_frame.cursor.x+=v.x;g_frame.cursor.y+=v.y;}
enum class SemanticTone{neutral,info};
const auto&active_semantic_pair(SemanticTone){return g_resources.semantic;}
SHADOW_CONSTANTS
PAINT
AUTHORED_SURFACE
AuthoredButtonSurface authored_button_surface(Color color){
 const SpriteHandle sprite{++nextSprite};colors.emplace(sprite.id,theme_detail::Rgb{color.r,color.g,color.b,color.a});
 return {sprite,{sprite,sprite,sprite,sprite,8.0f}};
}
AUTHORED_FACE
void draw_sprite(SpriteHandle h,float x,float y,float w,float height){draw_sprite_raw(h,x,y,w,height);}
ICON_PAINT
BUTTON
struct ScrollState {bool contextMenuOpen{},followTail{};float scrollY{40.0f};};
struct ScrollOptions {
 bool conversation_style{},show_scroll_to_end{};
 std::vector<std::string> lines{"A retained entry"};
};
struct ScrollViewport {float textRight{},scrollbarLeft{},maximumScrollY{};};
ScrollViewport draw_latest(const ScrollOptions& options,ScrollState& state,
 float width=180,float height=90,float totalTextHeight=370){
 constexpr Vec2 pos{12,8};
 SCROLL_GEOMETRY
 const float maxScrollY=(std::max)(0.0f,totalTextHeight-contentHeight);
 LATEST_CONTROL
 return {contentX+contentWidth,pos.x+width-kBoxInnerPadding-scrollbarWidth,maxScrollY};
}
void register_palette(std::size_t index){TextureAtlas atlas;CONTRAST_REGISTRATION;REGISTRATION;registerPalette(g_resources.current,theme_detail::palette_at(index));}
void ordinary_field(bool integrated_composer,bool keyboardInputEnabled){const auto&palette=active_palette();Vec2 pos{10,20};float width=120,height=30;FIELDS}
void source_field(bool active){const auto&palette=active_palette();Vec2 pos{10,20};float editorWidth=120,height=30;EDITOR_FIELD}
bool rgb(SpriteHandle h,unsigned r,unsigned g,unsigned b){auto c=colors.at(h.id);return c.r==r&&c.g==g&&c.b==b;}
void reset(){draws.clear();textDraws.clear();releaseAvailable=false;g_frame.cursor={10,20};g_frame.mouseDown=false;g_frame.justReleased=false;g_frame.pointerInputAllowed=false;g_contextPressedButtonKeys[g_frame.ctx]=0;}
void check_bounds(){for(auto d:draws){assert(d.x>=10&&d.y>=20&&d.w>0&&d.h>0&&d.x+d.w<=130&&d.y+d.h<=50);}}
int main(){
 Context ctx;g_frame.ctx=&ctx;register_palette(2);const auto classic=active_palette();
 assert(classic.isLight&&classic.captionUsesLightText&&classic.beveledControls);
 assert(rgb(classic.windowBackground,210,210,210)&&rgb(classic.panelBackground,210,210,210));
 assert(rgb(classic.buttonNormal,210,210,210)&&rgb(classic.buttonHover,230,230,230)&&rgb(classic.buttonActive,186,186,186));
 assert(rgb(classic.textField,255,255,255)&&rgb(classic.titleBar,16,36,106));
 const AuthoredButtonStyle authored{{33,84,130,255},{82,150,207,255},1.0f,2.0f};
 reset();
 assert(!button_with_state("Calculate",{120,30},false,true,SemanticTone::neutral,false,false,ButtonIcon::none,{},false,&authored));
 assert(draws.size()==14&&rgb(draws.front().handle,82,150,207)&&rgb(draws.back().handle,33,84,130));
 check_bounds();assert(draws.front().y==20&&draws[7].y==21);
 reset();g_frame.cursor={10.4f,20.4f};
 g_frame.contentMin={10.4f,20.4f};
 g_frame.contentMax={130.8f,35.8f};
 assert(!button_with_state("1",{120.4f,15.4f},false,true,SemanticTone::neutral,false,false,ButtonIcon::none,{},false,&authored));
 assert(g_frame.lastButtonBounds->size.y==15.4f);
 assert(draws.size()==14&&draws.front().y+draws.front().h==35.0f);
 assert(draws[7].y+draws[7].h==34.0f);
 g_frame.contentMin={0,0};g_frame.contentMax={200,100};
 reset();g_frame.mouseDown=true;g_contextPressedButtonKeys[&ctx]=42;
 assert(!button_with_state("Calculate",{120,30},false,true,SemanticTone::neutral,false,false,ButtonIcon::none,{},false,&authored));
 assert(rgb(draws.back().handle,28,73,113));
 reset();assert(!button_with_state("Apply",{120,30},false));assert(draws.size()==9);check_bounds();
 assert(rgb(draws[1].handle,255,255,255)&&rgb(draws[3].handle,64,64,64)&&rgb(draws[7].handle,128,128,128));
 const auto normalText=textDraws.back();assert(normalText.font==2&&g_frame.lastButtonBounds->size.x==120&&g_frame.lastButtonBounds->size.y==30);
 reset();g_frame.mouseDown=true;g_contextPressedButtonKeys[&ctx]=42;
 assert(!button_with_state("Apply",{120,30},false));assert(draws.size()==9);check_bounds();
 assert(rgb(draws[1].handle,128,128,128)&&rgb(draws[3].handle,255,255,255)&&rgb(draws[5].handle,64,64,64));
 assert(textDraws.back().x==normalText.x+1&&textDraws.back().y==normalText.y+1);
 reset();ordinary_field(false,false);assert(draws.size()==9&&rgb(draws[0].handle,255,255,255)&&rgb(draws[1].handle,128,128,128));check_bounds();
 reset();ordinary_field(false,true);assert(draws.size()==9&&rgb(draws[0].handle,255,255,255));
 reset();ordinary_field(true,true);assert(draws.empty());
 reset();source_field(true);assert(draws.size()==9&&rgb(draws[0].handle,255,255,255)&&rgb(draws[1].handle,128,128,128));check_bounds();
 reset();g_frame.contentMin={11,21};g_frame.contentMax={50,40};ordinary_field(false,false);
 assert(!draws.empty()&&draws.size()<9);
 for(auto d:draws)assert(d.x>=11&&d.y>=21&&d.x+d.w<=50&&d.y+d.h<=40);
 reset();g_frame.contentMin={0,0};g_frame.contentMax={200,100};ctx.windowData=true;g_frame.ctxShared=std::make_shared<Context>();g_frame.topLayerDepth=1;
 ordinary_field(false,false);assert(draws.empty()&&g_frame.topLayerDraws.size()==9&&g_frame.queuedDraws.empty());
 g_frame.topLayerDraws.clear();g_frame.topLayerDepth=0;ordinary_field(false,false);assert(g_frame.queuedDraws.size()==9);
 ctx.windowData=false;g_frame.ctxShared.reset();g_frame.queuedDraws.clear();
 reset();draw_caption_text("Classic",0,0,1);assert(textDraws.back().font==1&&!g_temporaryTextFont);
 GuiFontCache temporary{77};g_temporaryTextFont=&temporary;draw_caption_text("Classic",0,0,1);assert(textDraws.back().font==1&&g_temporaryTextFont==&temporary);g_temporaryTextFont=nullptr;
 // Caption ink, fitting and baseline all use the selected font metrics.
 reset();g_temporaryTextFont=&temporary;
 g_resources.font.height=19;g_resources.font.ascent=12.5f;g_resources.font.glyphWidth=8;
 draw_window_caption("Classic",4,20.25f,31.5f,50,1);
 assert(textDraws.size()==1&&textDraws.back().text=="Cla..."&&textDraws.back().font==1);
 assert(textDraws.back().x==4&&textDraws.back().y==26.5f&&g_temporaryTextFont==&temporary);
 g_resources.font.height=0;g_resources.font.ascent=0;g_resources.font.glyphWidth=5;
 g_temporaryTextFont=nullptr;
 reset();assert(!draw_beveled_control(classic,classic.buttonNormal,0,0,3,30,false));assert(draws.empty());
 assert(!draw_beveled_control(classic,classic.buttonNormal,0,0,std::numeric_limits<float>::quiet_NaN(),30,false)&&draws.empty());
 for(auto icon:{ButtonIcon::start,ButtonIcon::step_back,ButtonIcon::play,ButtonIcon::pause,ButtonIcon::step_forward,ButtonIcon::reverse,ButtonIcon::add,ButtonIcon::more}){
  reset();draw_button_icon(icon,{10,20},{24,24});assert(!draws.empty());
  for(const auto&d:draws)assert(d.x>=10&&d.y>=20&&d.x+d.w<=34&&d.y+d.h<=44&&d.w>0&&d.h>0&&rgb(d.handle,0,0,0));
  if(icon==ButtonIcon::step_back)assert(draws.front().x==16&&draws.front().w==2&&draws.size()==13);
  if(icon==ButtonIcon::step_forward)assert(draws.back().x==26&&draws.back().w==2&&draws.size()==13);
  if(icon==ButtonIcon::pause)assert(draws.size()==2&&draws[0].x<draws[1].x);
  if(icon==ButtonIcon::add)assert(draws.size()==2&&draws[0].w==2&&draws[1].h==2);
  if(icon==ButtonIcon::more)assert(draws.size()==3&&draws[0].y<draws[1].y&&draws[1].y<draws[2].y);
 }
 reset();draw_button_icon(ButtonIcon::chevron_left,{10,20},{32,32});
 const auto leftChevron=draws;
 reset();draw_button_icon(ButtonIcon::chevron_right,{10,20},{32,32});
 assert(draws.size()==9&&leftChevron.size()==draws.size());
 for(std::size_t i=0;i<draws.size();++i){
  assert(leftChevron[i].x+draws[i].x+draws[i].w==52.f&&leftChevron[i].y==draws[i].y);
  assert(draws[i].y+draws[draws.size()-1-i].y+draws[i].h==72.f);
 }
 reset();assert(!button_with_state("Semantic Play",{24,24},false,true,SemanticTone::neutral,false,false,ButtonIcon::play,"Play forward"));
 assert(textDraws.empty()&&lastPressLabel=="Semantic Play"&&g_frame.lastButtonBounds->size.x==24&&g_frame.lastButtonBounds->size.y==24);
 reset();g_frame.pointerInputAllowed=true;g_frame.mousePos={22,32};
 assert(!button_with_state("Semantic Play",{24,24},false,true,SemanticTone::neutral,false,false,ButtonIcon::play,"Play forward"));
 assert(textDraws.size()==1&&textDraws.back().text=="Play forward"&&g_frame.topLayerDepth==0);
 assert(textDraws.back().y==52&&textDraws.back().layer==1);
 register_palette(9);reset();assert(!draw_beveled_control(active_palette(),active_palette().buttonNormal,10,20,120,30,false)&&draws.empty());
 assert(!button_with_state("Apply",{120,30},false)&&draws.size()==1);assert(textDraws.back().font==1&&textDraws.back().x==normalText.x);
 // Dropdowns keep their label separate from the indicator, preserving their
 // own hit target and themed button input rather than appending text arrows.
 for(std::size_t index:{2u,9u})for(bool opened:{false,true}){
  register_palette(index);reset();g_frame.mousePos={0,0};
  assert(!button_with_state("Long camera perspective",{120,30},opened,true,SemanticTone::neutral,false,false,ButtonIcon::none,{},true));
  assert(lastPressLabel=="Long camera perspective"&&textDraws.size()==1);
  const auto&caption=textDraws.front();const float pressOffset=active_palette().beveledControls&&opened?1.f:0.f;
  assert(caption.x==15.f+pressOffset&&caption.x+measure_text_width(caption.text,caption.scale)<=106.f);
  assert(g_frame.lastButtonBounds->size.x==120&&g_frame.lastButtonBounds->size.y==30);
  const auto first=draws[draws.size()-10];const auto last=draws.back();
  assert(first.x==114.f&&last.x==118.f&&first.y==(opened?37.f:33.f)&&last.y==(opened?33.f:37.f));
  for(std::size_t d=draws.size()-10;d<draws.size();++d){assert(draws[d].x>=106.f&&draws[d].x+draws[d].w<=130.f);}
 }
 for(std::size_t index:{2u,9u}){
  register_palette(index);reset();assert(!button_with_state("down",{34,34},false,true,SemanticTone::neutral,true,true));
  assert(draws.size()==2&&draws[0].w==34&&draws[1].w==32&&draws[1].x==11);
  const bool light=active_palette().isLight;
  assert(rgb(draws[0].handle,light?0:255,light?0:255,light?0:255));
  assert(rgb(draws[1].handle,light?255:0,light?255:0,light?255:0));
  assert(colors.at(draws[0].handle.id).a==255&&colors.at(draws[1].handle.id).a==255);
  assert(textDraws.back().font==(light?2:1)&&!g_temporaryTextFont);
  // The visible circle retains the original round release hit test.
  for(bool inside:{false,true}){
   reset();releaseAvailable=true;g_frame.justReleased=true;g_contextPressedButtonKeys[&ctx]=42;
   g_frame.mousePos=inside?Vec2{27,37}:Vec2{10,20};
   assert(button_with_state("down",{34,34},false,true,SemanticTone::neutral,true,true)==inside);
  }
 }
 // Composer icon bounds and the themed jump button remain circular even
 // when larger text metrics would grow a plain text button.
 fontHeight=36;
 for(std::size_t index:{0u,2u,4u}){
  register_palette(index);
  const auto& current=active_palette();
  reset();g_frame.mousePos={0,0};
  assert(!button_with_state("Add to chat",{30,30},false,true,SemanticTone::neutral,true,false,ButtonIcon::add));
  assert(g_frame.lastButtonBounds->size.x==30&&g_frame.lastButtonBounds->size.y==30);
  assert(draws.size()==3&&textDraws.empty());
  assert(draws[0].handle.id==g_resources.circleControls.at(current.buttonNormal.id).id&&draws[0].w==30&&draws[0].h==30);
  for(auto glyph:{draws[1],draws[2]}){
   assert(glyph.x+glyph.w/2==25&&glyph.y+glyph.h/2==35);
   assert(glyph.handle.id==(current.isLight?g_resources.contrastBlack:g_resources.contrastWhite).id);
  }
  // The production scroll control owns its top-layer paint, and browsing old
  // messages only resumes following after a click within its circular target.
  ctx.windowData=true;g_frame.ctxShared=std::make_shared<Context>();
  for(bool chat:{false,true}){
   const ScrollOptions options{.conversation_style=chat,.show_scroll_to_end=!chat};
   reset();g_frame.topLayerDraws.clear();g_frame.mousePos={};ScrollState state;
   const auto viewport=draw_latest(options,state);
   assert(!state.followTail&&state.scrollY==40&&g_frame.queuedDraws.empty()&&g_frame.topLayerDepth==0);
   assert(g_frame.cursor.x==10&&g_frame.cursor.y==20);
   assert(g_frame.topLayerDraws.size()==1);
   const auto face=g_frame.topLayerDraws.front();
   assert(face.w==face.h&&textDraws.size()==1&&textDraws[0].scale<1.0f);
   assert(face.handle.id==g_resources.circleControls.at(current.buttonNormal.id).id);
   if(chat) assert(face.x+face.w<viewport.textRight);
   else assert(face.x>=viewport.textRight&&face.x+face.w<=viewport.scrollbarLeft);
   const Vec2 center{face.x+face.w/2,face.y+face.h/2};
   reset();g_frame.topLayerDraws.clear();g_frame.mousePos=center;draw_latest(options,state);
   assert(g_frame.topLayerDraws.front().handle.id==g_resources.circleControls.at(current.buttonHover.id).id);
   // A parent clip still rejects pointer input even when the arrow is visible elsewhere in the pane.
   const Vec2 savedClip=g_frame.contentMax;
   reset();g_frame.topLayerDraws.clear();g_frame.contentMax.y=face.y+1.0f;
   releaseAvailable=true;g_frame.justReleased=true;g_contextPressedButtonKeys[&ctx]=42;
   g_frame.mousePos=center;draw_latest(options,state);
   assert(!state.followTail&&g_frame.topLayerDraws.size()==1&&g_frame.topLayerDraws.front().h==1.0f);
   g_frame.contentMax=savedClip;
   for(bool inside:{false,true}){
    reset();g_frame.topLayerDraws.clear();releaseAvailable=true;g_frame.justReleased=true;
    g_contextPressedButtonKeys[&ctx]=42;g_frame.mousePos=inside?center:Vec2{face.x,face.y};
    draw_latest(options,state);
    assert(state.followTail==inside&&state.scrollY==(inside?viewport.maximumScrollY:40));
   }
   g_frame.topLayerDraws.clear();const auto atBottom=draw_latest(options,state);
   assert(g_frame.topLayerDraws.empty()&&atBottom.textRight==viewport.textRight);
   if(!chat){
    ScrollState menu{.contextMenuOpen=true};
    assert(draw_latest(options,menu).textRight==viewport.textRight&&g_frame.topLayerDraws.empty());
    for(auto size:{Vec2{130,90},Vec2{180,60}}){
     ScrollState compact;
     const auto compactView=draw_latest(options,compact,size.x,size.y);
     assert(compactView.textRight==compactView.scrollbarLeft&&g_frame.topLayerDraws.empty());
    }
    ScrollState noButton;
    const auto disabled=draw_latest({},noButton);
    assert(disabled.textRight==disabled.scrollbarLeft&&g_frame.topLayerDraws.empty());
    const auto empty=draw_latest({.show_scroll_to_end=true,.lines={}},noButton,180,90,0);
    assert(empty.textRight==empty.scrollbarLeft&&g_frame.topLayerDraws.empty());
   }
  }
  ctx.windowData=false;g_frame.ctxShared.reset();
 }

 // Test the real tooltip and shadow placements against a narrow inherited
 // clip, with room above the icon for larger text, in every theme.
 ctx.windowData=true;
 g_frame.ctxShared=std::make_shared<Context>();
 fontHeight=38;
 g_frame.contentMin={18,14};
 g_frame.contentMax={90,96};
 for(std::size_t index=0;index<std::size(theme_detail::palettes);++index){
  register_palette(index);
  const auto& current=active_palette();
  reset();g_frame.topLayerDraws.clear();
  draw_button_tooltip("Play forward",{78,70},20);
  assert(g_frame.queuedDraws.empty()&&g_frame.topLayerDepth==0);
  assert(g_frame.topLayerDraws.size()>2&&textDraws.size()==1);
  const auto& edge=g_frame.topLayerDraws.back();
  const auto& body=g_frame.topLayerDraws[g_frame.topLayerDraws.size()-2];
  const auto& caption=textDraws.front();
  assert(body.handle.id==current.panelBackground.id&&colors.at(body.handle.id).a==255);
  assert(body.x==18&&body.y==19&&body.w==72&&body.h==48);
  assert(edge.handle.id==current.headingAccent.id&&colors.at(edge.handle.id).a==255);
  assert(edge.x==body.x+3&&edge.y==body.y+5&&edge.w==1&&edge.h==fontHeight);
  assert(caption.text=="Play fo..."&&caption.x==body.x+10&&caption.y==edge.y);
  assert(caption.font==(current.isLight?2:1)&&caption.layer==1);
  for(const auto& draw:g_frame.topLayerDraws){
   assert(draw.x>=18&&draw.y>=14&&draw.x+draw.w<=90&&draw.y+draw.h<=96);
  }
  for(std::size_t i=0;i<g_frame.topLayerDraws.size()-2;++i){
   assert(g_frame.topLayerDraws[i].handle.id<0);
  }
  assert(std::ranges::any_of(g_frame.topLayerDraws,[&](const auto& draw){
   return draw.handle.id<0&&draw.y+draw.h>body.y+body.h;
  }));
 }
 reset();g_frame.topLayerDraws.clear();
 g_frame.contentMax={90,61};
 draw_button_tooltip("Too short",{30,20},20);
 assert(g_frame.topLayerDraws.empty()&&textDraws.empty());
 g_frame.contentMax={48,96};
 draw_button_tooltip("Too narrow",{20,20},20);
 assert(g_frame.topLayerDraws.empty()&&textDraws.empty());
 draw_button_tooltip("OK",{20,20},20);
 assert(textDraws.size()==1&&textDraws.front().text=="OK");
}
'''.replace('SPEC', (ROOT/'Engine/src/epochgui/gui.theme_palette_spec.hpp').as_posix()).replace('PALETTE', palette).replace('ROUNDED_CORNERS', roundedCorners).replace('ICON_PAINT',icon_paint).replace('ICONS_API',icon_api).replace('TRANSLUCENT_SURFACE', translucent_surface).replace('PAINT', paint).replace('BUTTON', button).replace('SCROLL_GEOMETRY', scrollGeometry).replace('LATEST_CONTROL', latest).replace('CONTRAST_REGISTRATION', contrast_registration).replace('REGISTRATION', registration).replace('FIELDS', fields).replace('EDITOR_FIELD', editor_field)
        code = (code.replace('SHADOW_CONSTANTS', shadow_constants)
                    .replace('AUTHORED_API',authored_api).replace('AUTHORED_SURFACE',authored_surface)
                    .replace('AUTHORED_FACE',authored_face)
                    .replace('UTF8', utf8).replace('FIT_TEXT', fit_text).replace('WRAP_MEASURE', wrap_measure))
        # Every failure exits with its generated line number; no CRT assertion
        # or abort dialog is permitted in this recording consumer.
        code = code.replace('assert(', 'CHECK(')
        report = ROOT/'Artifacts/Reports/classic-controls-oct28'/hashlib.sha256(code.encode()).hexdigest()[:16]
        report.mkdir(parents=True, exist_ok=True)
        cpp=report/'actual_owners.cpp';cpp.write_text(code,encoding='utf-8')
        results=compile_and_run(self,cpp,report)
        (report/'result.json').write_text(json.dumps({'scope':'Actual control paint, captions and tooltip depth, text fit and clipping across registered themes with a recording renderer','results':results,'GPU_or_model_launch':False},indent=2),encoding='utf-8')


if __name__=='__main__':
    unittest.main()
