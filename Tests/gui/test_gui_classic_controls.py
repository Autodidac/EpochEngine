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
        text = (ROOT/'Engine/src/epochgui/controls/gui.text_controls.inl').read_text(encoding='utf-8')
        windows = (ROOT/'Engine/src/epochgui/controls/gui.window_controls.inl').read_text(encoding='utf-8')
        chrome = (ROOT/'Engine/src/epochgui/controls/gui.navigation_chrome.inl').read_text(encoding='utf-8')
        self.assertEqual(windows.count('draw_caption_text('), 2)
        self.assertIn('draw_caption_text(display, itemBounds.position.x + inset, textY, kFontScale)', chrome)
        fields = definition(text, '        if (!integrated_composer)')
        begin = text.index('        const auto editorField = active ?')
        editor_field = text[begin:text.index('\n\n', begin)]
        paint = '\n'.join(definition(source, marker) for marker in (
            '        static void draw_sprite_raw(',
            '        [[nodiscard]] static bool draw_beveled_control(',
            '        [[nodiscard]] static bool draw_high_contrast_control(',
            '        static void draw_caption_text(',
            '        static void draw_contrast_text('))
        button_start=buttons.index('    static bool button_with_state(')
        button = buttons[button_start:buttons.index('\n    bool button(',button_start)]
        gui_api=(ROOT/'Engine/modules/gui/gui.engine.ixx').read_text(encoding='utf-8')
        icon_api=definition(gui_api,'    export enum class ButtonIcon').replace('export ','',1)+';'
        icon_paint=definition(buttons,'    static void draw_button_icon(')+definition(buttons,'    static void draw_button_tooltip(')
        palette = definition(source, '            struct PaletteSprites', True)
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
struct SpriteHandle{int id{};bool is_valid()const{return id!=0;}operator int()const{return id;}};
struct WidgetBounds{Vec2 position{},size{};};
struct QueuedSpriteDraw{SpriteHandle handle;float x{},y{},w{},h{};};
struct TextureAtlas{};
std::vector<QueuedSpriteDraw> draws;
std::unordered_map<int,theme_detail::Rgb> colors;
namespace epochengine::atlasmanager {std::vector<const TextureAtlas*> get_atlas_vector_snapshot(){return {};}}
struct Context{
 bool windowData{};
 void draw_sprite_safe(SpriteHandle h,std::span<const TextureAtlas* const>,float x,float y,float w,float height){draws.push_back({h,x,y,w,height});}
};
struct Frame{
 Context*ctx{};std::shared_ptr<Context>ctxShared;bool insideWindow{true},mouseDown{},justReleased{};
 Vec2 cursor{10,20},mousePos{},contentMin{0,0},contentMax{200,100};
 Vec2 origin{},windowSize{200,100};bool pointerInputAllowed{};
 int topLayerDepth{};std::vector<QueuedSpriteDraw>topLayerDraws,queuedDraws;
 std::optional<WidgetBounds>lastButtonBounds;
}g_frame;
struct GuiFontCache{int id{};bool asset{true};std::string fontName{};};
struct GuiResources{
 PALETTE
 PaletteSprites current{};GuiFontCache font{1},darkTextFont{2};
 SpriteHandle contrastBlack{},contrastWhite{};std::unordered_map<int,SpriteHandle>circleControls;
 struct Pair{SpriteHandle background{},accent{};}semantic;
}g_resources;
const GuiResources::PaletteSprites& active_palette(){return g_resources.current;}
const GuiFontCache*g_temporaryTextFont{};
struct DrawText{std::string text;float x{},y{};int font{};};
std::vector<DrawText>textDraws;
void draw_text_line(std::string_view s,float x,float y,float){textDraws.push_back({std::string{s},x,y,g_temporaryTextFont?g_temporaryTextFont->id:g_resources.current.isLight?2:1});}
constexpr float kMinQueuedSpriteExtent=.001f,kMaxQueuedSpriteExtent=1e8f;
bool clipped=true;
bool has_content_clip(){return clipped;}
int nextSprite{};
int make_solid_pixels(std::uint8_t r,std::uint8_t g,std::uint8_t b,std::uint8_t a,int,int){int id=++nextSprite;colors[id]={r,g,b,a};return id;}
SpriteHandle add_sprite(TextureAtlas&,const std::string&,int id,int,int){return {id};}
void register_rounded_control(TextureAtlas&,SpriteHandle h,const std::string&,theme_detail::Rgb c){const SpriteHandle circle{++nextSprite};colors[circle.id]=c;g_resources.circleControls[h.id]=circle;}
namespace gui_lib::rounded_rect{struct RoundedRectStyle{bool enabled{};float control_radius{};};}
constexpr float kFontScale=1,kContentPadding=4,kBoxInnerPadding=4,kButtonTextClipInset=1;
float base_line_height(float){return 10;}
float space_advance(float){return 5;}
float measure_text_width(std::string_view s,float){return static_cast<float>(s.size())*5;}
std::string fit_text_to_width(std::string_view s,float,float){return std::string{s};}
bool point_in_rect(Vec2 p,float x,float y,float w,float h){return p.x>=x&&p.y>=y&&p.x<x+w&&p.y<y+h;}
bool point_in_active_clip(Vec2){return true;}
bool point_in_modal_input_capture(Vec2){return true;}
void begin_top_layer(){++g_frame.topLayerDepth;}void end_top_layer(){--g_frame.topLayerDepth;}
std::string lastPressLabel;std::size_t widget_press_key(std::string_view label,Vec2,Vec2){lastPressLabel=label;return 42;}
std::unordered_map<void*,std::size_t>g_contextPressedButtonKeys;
bool left_press_available(){return false;}
bool releaseAvailable{};bool left_release_available(){return releaseAvailable;}
std::optional<Vec2>mouse_press_position(){return {};}
void consume_left_press(){}void consume_left_release(){}
void advance_cursor(Vec2 v){g_frame.cursor.x+=v.x;g_frame.cursor.y+=v.y;}
enum class SemanticTone{neutral,info};
const auto&active_semantic_pair(SemanticTone){return g_resources.semantic;}
PAINT
void draw_sprite(SpriteHandle h,float x,float y,float w,float height){draw_sprite_raw(h,x,y,w,height);}
void draw_rounded_sprite(SpriteHandle h,float x,float y,float w,float height,gui_lib::rounded_rect::RoundedRectStyle){draw_sprite_raw(h,x,y,w,height);}
ICON_PAINT
BUTTON
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
 reset();assert(!button_with_state("Semantic Play",{24,24},false,true,SemanticTone::neutral,false,false,ButtonIcon::play,"Play forward"));
 assert(textDraws.empty()&&lastPressLabel=="Semantic Play"&&g_frame.lastButtonBounds->size.x==24&&g_frame.lastButtonBounds->size.y==24);
 reset();g_frame.pointerInputAllowed=true;g_frame.mousePos={22,32};
 assert(!button_with_state("Semantic Play",{24,24},false,true,SemanticTone::neutral,false,false,ButtonIcon::play,"Play forward"));
 assert(textDraws.size()==1&&textDraws.back().text=="Play forward"&&g_frame.topLayerDepth==0);
 register_palette(9);reset();assert(!draw_beveled_control(active_palette(),active_palette().buttonNormal,10,20,120,30,false)&&draws.empty());
 assert(!button_with_state("Apply",{120,30},false)&&draws.size()==1);assert(textDraws.back().font==1&&textDraws.back().x==normalText.x);
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
}
'''.replace('SPEC', (ROOT/'Engine/src/epochgui/gui.theme_palette_spec.hpp').as_posix()).replace('PALETTE', palette).replace('ICON_PAINT',icon_paint).replace('ICONS_API',icon_api).replace('PAINT', paint).replace('BUTTON', button).replace('CONTRAST_REGISTRATION', contrast_registration).replace('REGISTRATION', registration).replace('FIELDS', fields).replace('EDITOR_FIELD', editor_field)
        # Every failure exits with its generated line number; no CRT assertion
        # or abort dialog is permitted in this recording consumer.
        code = code.replace('assert(', 'CHECK(')
        report = ROOT/'Artifacts/Reports/classic-controls-oct28'/hashlib.sha256(code.encode()).hexdigest()[:16]
        report.mkdir(parents=True, exist_ok=True)
        cpp=report/'actual_owners.cpp';cpp.write_text(code,encoding='utf-8')
        results=compile_and_run(self,cpp,report)
        (report/'result.json').write_text(json.dumps({'scope':'Actual button/field branches, bevel paint, clipping/queued layers and caption font with recording GPU services','results':results,'GPU_or_model_launch':False},indent=2),encoding='utf-8')


if __name__=='__main__':
    unittest.main()
