"""Execute real palette/font/control selectors with recording CPU draw services."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import msvc_2022_toolsets, windows_sdk_includes

ROOT = Path(__file__).resolve().parents[2]
GUI = ROOT/'Engine/src/epochgui'


class PaletteFontPolicy(unittest.TestCase):
    def test_registered_light_metadata_drives_fonts_controls_and_native_policy(self):
        source = (GUI/'gui.engine.cpp').read_text(encoding='utf-8')
        text = (GUI/'controls/gui.text_controls.inl').read_text(encoding='utf-8')
        buttons = (GUI/'controls/gui.buttons_images.inl').read_text(encoding='utf-8')
        nodes = (GUI/'controls/gui.node_surface_controls.inl').read_text(encoding='utf-8')
        variant = definition((ROOT/'Engine/modules/gui/gui.engine.ixx').read_text(encoding='utf-8'),
            '    export enum class ThemeVariant', True).replace('export enum class', 'enum class', 1)
        palette = definition(source, '            struct PaletteSprites', True)
        semantic = definition(source, '            struct SemanticSpritePair', True)
        # The lexer contains quoted braces; retain its complete owner region.
        lexer_start = source.index('        enum class CodeInk')
        lexer_end = source.index('        static float draw_wrapped_text(', lexer_start)
        functions = source[lexer_start:lexer_end] + '\n' + '\n'.join(definition(source, marker) for marker in (
            '        [[nodiscard]] static bool complete_palette(',
            '        [[nodiscard]] static const GuiResources::PaletteSprites& active_palette(',
            '        [[nodiscard]] static bool active_palette_is_light(',
            '        [[nodiscard]] static const GuiResources::SemanticSpritePair& active_semantic_pair(',
            '        [[nodiscard]] static const GuiFontCache& active_font_cache(',
            '        [[nodiscard]] static const font::Glyph* lookup_shadow_glyph(',
            '        [[nodiscard]] static StatusKeyword status_keyword_at(',
            '        [[nodiscard]] static float content_available_width(',
            '        [[nodiscard]] static float measure_wrapped_text_height(',
            '        static float draw_wrapped_text(',
            '    void publish_native_window_theme(',
            '    NativeWindowChromePalette native_window_chrome_palette(',
            '    bool native_window_prefers_dark('))
        functions += '\n'+definition(source, '    bool rounded_rectangles_enabled(')
        functions += '\n'+definition(source, '        static void draw_caption_text(')
        functions += '\n'+definition(text, '    [[nodiscard]] static const GuiFontCache* semantic_text_font(')
        functions += '\n'+definition(text, '    void status_label(')
        functions += '\n'+definition(text, '    float measure_section_heading_height(')
        functions += '\n'+definition(text, '    void section_heading(')
        functions += '\n'+definition(buttons, '    static bool menu_item_control(')
        functions += '\n'+definition(buttons, '    bool menu_item(')
        functions += '\n'+definition(nodes, '    SliderResult slider(')
        functions = definition(source, '        [[nodiscard]] static std::size_t widget_press_key(') + '\n' + functions
        registration = definition(source, '                const auto registerPalette = [&atlas](')
        calls_start = source.index('                registerPalette(g_resources.defaultDark,')
        calls_end = source.index('\n\n', calls_start)
        registration_calls = source[calls_start:calls_end].replace('registerPalette(', 'register_test_palette(')
        # Execute the actual placeholder branch without constructing an entire
        # edit-box/controller; font metrics and draw services are recording fakes.
        placeholder = definition(text, '            if (view.empty() && !placeholder.empty())')
        prefix = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "ACTUAL_SPEC_HEADER"
// Contract failures return their line instead of opening a Windows CRT dialog.
inline void contract_require(bool value,int line) noexcept {if(!value)std::exit(line);}
#undef assert
#define assert(...) contract_require(static_cast<bool>((__VA_ARGS__)),__LINE__)
struct Vec2 { float x{},y{}; };
struct SpriteHandle { int id{}; bool is_valid() const noexcept {return id!=0;} };
struct WidgetBounds { Vec2 position{},size{}; };
namespace font {struct Glyph;}
struct GuiFontCache { int id{}; bool asset{true}; std::string fontName{}; const font::Glyph* fallbackGlyph{}; };
THEME_VARIANT
enum class ThemePreference {FollowSystemDark,Light,Dark,ThemeCycle,ClassicLauncher};
enum class SemanticTone {neutral,muted,info,success,warning,error,assistant,engine};
namespace theme_detail {
 using Rgb=epochengine::gui::theme_detail::Rgb;
 using Palette=epochengine::gui::theme_detail::Palette;
 using epochengine::gui::theme_detail::section_heading_color;
 inline std::array<Palette,std::size(epochengine::gui::theme_detail::palettes)> fixtures{};
 const Palette& palette_at(std::size_t index) {return fixtures[index<fixtures.size()?index:0];}
 std::uint32_t packed_rgb(Rgb v) {return (std::uint32_t{v.r}<<16)|(std::uint32_t{v.g}<<8)|v.b;}
}
struct GuiResources {
PALETTE
SEMANTIC
 PaletteSprites defaultDark{},defaultLight{},classicLauncher{},midnightBlue{},emberForge{},forestTerminal{},auroraSteel{},codexBlack{},graphiteCopper{},slateOrchard{},indigoQuartz{},signalPlum{},forgedSteel{};
 SpriteHandle contrastBlack{14},contrastWhite{15};
 std::array<SemanticSpritePair,8> semanticDark{},semanticLight{};
 GuiFontCache font{1},darkTextFont{2},halfTextFont{3},halfTextLightFont{4},textShadowFont{13};
 GuiFontCache statusInfoFont{5},statusInfoLightFont{6},statusSuccessFont{7},statusSuccessLightFont{8};
 GuiFontCache statusWarningFont{9},statusWarningLightFont{10},statusErrorFont{11},statusErrorLightFont{12};
} g_resources;
struct Frame {std::string windowKey{"test-window"};ThemeVariant activeTheme{ThemeVariant::DefaultDark};struct RoundedStyle {bool enabled{true};} roundedRectStyle;void*ctx{reinterpret_cast<void*>(1)};bool insideWindow{true},mouseDown{},justReleased{};Vec2 cursor{},mousePos{},contentMin{},contentMax{1000,1000};std::optional<WidgetBounds> lastButtonBounds;} g_frame;
const GuiFontCache* g_temporaryTextFont{};
static const GuiFontCache& active_font_cache() noexcept;
struct StatusKeyword {std::string_view word{};const GuiFontCache*font{};};
std::atomic<ThemePreference> g_nativeWindowTheme{ThemePreference::Dark};
std::atomic<ThemeVariant> g_publishedWindowVariant{ThemeVariant::DefaultDark};
std::atomic<bool> g_cycleWindowDark{true};
bool system_prefers_dark_palette() {return true;}
struct NativeWindowChromePalette {std::uint8_t paletteId{};bool isLight{};std::uint32_t backgroundRgb{},captionRgb{},borderRgb{},textRgb{};};
NativeWindowChromePalette native_window_chrome_palette() noexcept;
namespace gui_lib {
 inline constexpr std::size_t semantic_tone_count=8;
 namespace rounded_rect {struct RoundedRectStyle {bool enabled{};float control_radius{};};}
 struct SliderLayoutOptions {WidgetBounds bounds;float minimum{},maximum{},value{},step{},horizontal_padding{},track_height{},thumb_width{},thumb_height{};};
 struct SliderLayout {bool valid{true};WidgetBounds track{{0,2},{100,7}},fill{{0,2},{50,7}},thumb{{50,0},{16,21}};};
 SliderLayout make_slider_layout(const SliderLayoutOptions&) {return {};}
 float slider_value_from_position(const SliderLayoutOptions& o,float x) {return std::clamp(x,o.minimum,o.maximum);}
}
struct TextureAtlas {};
int nextSprite=100;
int make_solid_pixels(std::uint8_t,std::uint8_t,std::uint8_t,std::uint8_t,int,int) {return 0;}
SpriteHandle add_sprite(TextureAtlas&,const std::string&,int,int,int) {return {++nextSprite};}
void register_rounded_control(TextureAtlas&,SpriteHandle,const std::string&,theme_detail::Rgb) {}
constexpr float kFontScale=1,kTitleScale=1,kContentPadding=4;
void ensure_resources() {}
bool has_content_clip() {return false;}
float space_advance(float) {return 1;}
float line_advance_amount(float) {return 1;}
float baseline_offset(float) {return 0;}
float base_line_height(float) {return 1;}
struct DecodedUtf8Codepoint {char32_t codepoint{};std::size_t byte_count{1};};
DecodedUtf8Codepoint decode_utf8_codepoint(std::string_view text,std::size_t at) {return {static_cast<unsigned char>(text[at]),1};}
char32_t safe_draw_codepoint(char32_t c) {return c;}
bool is_wrap_space(char32_t c) {return c==' '||c=='\t';}
char32_t next_drawable_char(std::string_view,std::size_t) {return U'x';}
float glyph_advance_with_kerning(char32_t,char32_t,float) {return 1;}
float measure_word_advance(std::string_view,std::size_t,float) {return 1;}
namespace font {struct Glyph {Vec2 size_px{1,1},offset_px{};SpriteHandle handle;};}
const font::Glyph* lookup_glyph_from_cache(const GuiFontCache& f,char32_t) {static std::array<font::Glyph,128> glyphs;auto& g=glyphs.at(f.id);g.handle={f.id};return &g;}
const font::Glyph* lookup_glyph(char32_t c) {return lookup_glyph_from_cache(active_font_cache(),c);}
[[nodiscard]] static const font::Glyph* lookup_shadow_glyph(char32_t) noexcept;
bool glyph_rect_is_reasonable(float,float,float) {return true;}
float align_leading_glyph_to_clip(float pen,float,float,float) {return pen;}
bool glyph_intersects_clip(float,float,float,float,float,float,float,float) {return true;}
std::vector<int> sprites,textFonts;std::vector<std::array<float,4>>spriteBounds;
void draw_sprite(SpriteHandle s,float x,float y,float w,float h) {sprites.push_back(s.id);spriteBounds.push_back({x,y,w,h});}
void draw_rounded_sprite(SpriteHandle s,float,float,float,float,gui_lib::rounded_rect::RoundedRectStyle) {sprites.push_back(s.id);}
void draw_text_line(std::string_view,float,float,float) {textFonts.push_back(active_font_cache().id);}
struct ContentClipScope {ContentClipScope(Vec2,Vec2) {}};
float content_right() {return g_frame.contentMax.x;}
float menu_item_row_height() {return 30;}
float measure_text_width(std::string_view s,float) {return static_cast<float>(s.size());}
std::string fit_text_to_width(std::string_view s,float,float) {return std::string{s};}
bool testHover{},testPress{};
bool point_in_rect(Vec2,float,float,float,float) {return testHover;}
bool point_in_active_clip(Vec2) {return true;}

std::unordered_map<void*,std::size_t> g_contextPressedButtonKeys;
bool left_press_available() {return testPress;}
std::optional<Vec2> mouse_press_position() {return {};}
bool left_release_available() {return false;}
void consume_left_press() {testPress=false;}
void consume_left_release() {}
void advance_cursor(Vec2 delta) {g_frame.cursor.x+=delta.x;g_frame.cursor.y+=delta.y;}
enum class MenuItemIndicator {None,Radio,Check,Disclosure};
struct MenuItemOptions {std::string_view label{},shortcut{};Vec2 size{};bool enabled{true};MenuItemIndicator indicator{MenuItemIndicator::None};bool checked{},separator_before{};SemanticTone tone{SemanticTone::neutral};bool selected{};float indentation{};};
struct MenuLayout {bool valid{true};WidgetBounds row{{0,0},{300,30}},label{{0,0},{180,30}},shortcut{{180,0},{80,30}},indicator{{270,0},{20,20}};};
MenuLayout make_menu_item_layout(Vec2,Vec2,float,float,float shortcutWidth,float =0) {MenuLayout value;value.shortcut.size.x=shortcutWidth;return value;}
struct SliderOptions {std::string_view id{},label{};Vec2 size{200,40};float minimum{},maximum{100},value{50},step{1};bool enabled{true};};
struct SliderResult {float value{};bool changed{},hovered{};WidgetBounds bounds{};};
'''.replace('THEME_VARIANT', variant).replace('PALETTE', palette).replace('SEMANTIC', semantic)
        main = r'''
void register_test_palette(GuiResources::PaletteSprites& target,const theme_detail::Palette& spec) {
 TextureAtlas atlas;
 REGISTRATION;
 registerPalette(target,spec);
}
void placeholder_branch() {
 const std::string_view view{},placeholder="hint";
 const float textX{},textY{};
 PLACEHOLDER
}
void check(bool light) {
 assert(active_palette_is_light()==light);
 assert(active_font_cache().id==(light?2:1));
 assert((lookup_shadow_glyph(U'A')==nullptr)==light);
 assert(&active_semantic_pair(SemanticTone::info)==&(light?g_resources.semanticLight[2]:g_resources.semanticDark[2]));
 assert(status_keyword_at("passed",0).font->id==(light?8:7));
 assert(status_keyword_at("failed",0).font->id==(light?12:11));
 assert(status_keyword_at("queued",0).font->id==(light?10:9));
 assert(status_keyword_at("candidate",0).font->id==(light?6:5));
 textFonts.clear();status_label("status",SemanticTone::warning,{});
 assert(textFonts==std::vector<int>{light?10:9});assert(!g_temporaryTextFont);
 textFonts.clear();menu_item({.label="Disabled",.shortcut="Ctrl+X",.enabled=false});
 assert(textFonts==std::vector<int>({light?4:3,light?4:3}));assert(!g_temporaryTextFont);
 textFonts.clear();menu_item({.label="Available extension",.tone=SemanticTone::info});
 assert(textFonts==std::vector<int>{light?6:5});assert(!g_temporaryTextFont);
 textFonts.clear();menu_item({.label="Selected extension",.tone=SemanticTone::info,.selected=true});
 assert(textFonts==std::vector<int>{light?2:1});assert(!g_temporaryTextFont);
 const auto savedCursor=g_frame.cursor;const auto savedContentMax=g_frame.contentMax;
 const std::string heading="A long extension heading remains readable";
 const float wide=measure_section_heading_height(heading,300);
 for(float width:{8.0f,28.0f,300.0f}){
  g_frame.cursor={0,0};g_frame.contentMax={width,1000};sprites.clear();
  const float measured=measure_section_heading_height(heading,width);
  section_heading(heading,SemanticTone::info);
  assert(std::isfinite(measured)&&std::abs(g_frame.cursor.y-measured)<0.001f);
  assert(width>=300||measured>wide);
  assert(std::ranges::find(sprites,light?6:5)!=sprites.end());
  assert(!g_temporaryTextFont);
 }
 g_frame.cursor=savedCursor;g_frame.contentMax=savedContentMax;
 textFonts.clear();placeholder_branch();
 assert(textFonts==std::vector<int>{light?4:3});assert(!g_temporaryTextFont);
 sprites.clear();draw_wrapped_text("const x = {\"key\": true, \"value\": 7}",0,0,300,1,false,true);
 assert(std::ranges::find(sprites,light?6:5)!=sprites.end());
 assert(std::ranges::find(sprites,light?8:7)!=sprites.end());
 assert(std::ranges::find(sprites,light?5:6)==sprites.end());
 assert(std::ranges::find(sprites,light?7:8)==sprites.end());
 sprites.clear();slider({.id="slider"});
 const auto& p=active_palette();
 assert(sprites.size()==5u && sprites[1]==(light?p.buttonActive:p.titleBar).id);
 assert(sprites[2]==(light?p.modalScrim:p.buttonActive).id);
 assert(sprites[3]==(light?p.consoleBackground:p.textField).id);
}
void check_slider_reflow() {
 g_frame.cursor={0,0};g_frame.mousePos={25,10};g_frame.mouseDown=true;
 testHover=true;testPress=true;
 auto first=slider({.id="reflow",.value=0});assert(first.changed&&first.value==25);
 // A wrapped value label or a resize can move the control during a held drag.
 // The same explicit ID must retain capture, even outside its new hit area.
 g_frame.cursor={0,40};g_frame.mousePos={75,10};testHover=false;
 auto moved=slider({.id="reflow",.size={160,40},.value=25});
 assert(moved.changed&&moved.value==75);
 g_frame.cursor={0,40};
 auto other=slider({.id="other",.value=0});assert(!other.changed);
 g_frame.mouseDown=false;g_frame.justReleased=true;
 g_frame.cursor={0,40};slider({.id="reflow",.value=75});
 assert(g_contextPressedButtonKeys[g_frame.ctx]==0);
 g_frame.justReleased=false;g_frame.mouseDown=true;g_frame.cursor={0,40};
 assert(!slider({.id="reflow",.value=75}).changed);
 g_frame.mouseDown=false;testHover=false;
}
int main() {
 auto& specs=theme_detail::fixtures;
 for(std::size_t i=0;i<specs.size();++i)specs[i]=epochengine::gui::theme_detail::palette_at(i);
 assert(specs[9].label=="Slate Orchard"&&!specs[9].isLight);
 OWNED_CALLS
 const std::array variants{ThemeVariant::DefaultDark,ThemeVariant::DefaultLight,ThemeVariant::ClassicLauncher,ThemeVariant::MidnightBlue,ThemeVariant::EmberForge,ThemeVariant::ForestTerminal,ThemeVariant::AuroraSteel,ThemeVariant::CodexBlack,ThemeVariant::GraphiteCopper,ThemeVariant::SlateOrchard,ThemeVariant::IndigoQuartz,ThemeVariant::SignalPlum,ThemeVariant::ForgedSteel};
 const auto luminance=[](theme_detail::Rgb c){
  const auto linear=[](std::uint8_t byte){const double x=byte/255.;return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};
  return .2126*linear(c.r)+.7152*linear(c.g)+.0722*linear(c.b);
 };
 const auto contrast=[&](theme_detail::Rgb a,theme_detail::Rgb b){const double x=luminance(a),y=luminance(b);return ((std::max)(x,y)+.05)/((std::min)(x,y)+.05);};
 for(std::size_t i=0;i<variants.size();++i){
 assert(variants.size()==specs.size());
 const auto& spec=specs[i];g_frame.activeTheme=variants[i];check(spec.isLight);
  const auto& headingPalette=active_palette();
  assert(contrast(headingPalette.headingColor,spec.panel)>=4.5);
  assert(theme_detail::packed_rgb(headingPalette.headingColor)==theme_detail::packed_rgb(theme_detail::section_heading_color(spec)));
  assert(headingPalette.headingFont.fontName==std::string{spec.atlasPrefix}+"/heading_font");
  // Every default section uses its palette font and accent; explicitly marked
  // status headings retain their semantic color and restore the outer font.
  headingPalette.headingFont.id=static_cast<int>(20+i);
  sprites.clear();section_heading("Objects",SemanticTone::neutral);
  assert(std::ranges::find(sprites,headingPalette.headingFont.id)!=sprites.end());
  assert(std::ranges::find(sprites,headingPalette.headingAccent.id)!=sprites.end());
  assert(!g_temporaryTextFont);
  for(bool expanded:{false,true}){
   testHover=false;testPress=false;g_frame.mouseDown=false;g_frame.justReleased=false;
   sprites.clear();spriteBounds.clear();textFonts.clear();
   (void)menu_item_control({.label="Scene",.size={300,30},.indicator=MenuItemIndicator::Disclosure,
       .checked=expanded,.tone=SemanticTone::neutral},false);
   assert(sprites==std::vector<int>(10,spec.isLight?g_resources.contrastBlack.id:g_resources.contrastWhite.id));
   assert(textFonts.front()==(spec.isLight?2:1)&&!g_temporaryTextFont);
   for(const auto& box:spriteBounds)assert(box[0]>=270&&box[1]>=0&&box[0]+box[2]<=282&&box[1]+box[3]<=12);
   assert(spriteBounds.front()[0]==(expanded?271:273)&&spriteBounds.front()[1]==(expanded?3:1));
  }
  g_publishedWindowVariant.store(variants[i]);const auto chrome=native_window_chrome_palette();
  assert(chrome.paletteId==i&&chrome.isLight==spec.isLight&&chrome.captionRgb==theme_detail::packed_rgb(spec.title));
  const theme_detail::Rgb ink=spec.isLight?DARK_INK:LIGHT_INK;
  for(auto surface:{spec.window,spec.panel,spec.button,spec.hover,spec.active,spec.input,spec.inputActive,spec.console})
   assert(contrast(ink,surface)>=4.5);
  if(!spec.captionUsesLightText)assert(contrast(ink,spec.title)>=4.5);
  const theme_detail::Rgb caption{static_cast<std::uint8_t>(chrome.textRgb>>16),static_cast<std::uint8_t>(chrome.textRgb>>8),static_cast<std::uint8_t>(chrome.textRgb)};
  assert(contrast(caption,spec.title)>=4.5);
  textFonts.clear();draw_caption_text("Caption",0,0,1);
  assert(textFonts==std::vector<int>{spec.captionUsesLightText?1:spec.isLight?2:1}&&!g_temporaryTextFont);
 }
 assert(specs[2].label=="Classic Theme"&&specs[2].isLight&&specs[2].window.r>=0xC0);
 assert(specs[7].label=="Codex Black"&&!specs[7].isLight);
 for(std::size_t i:{0u,3u,4u,5u,6u,7u}){
  const auto& p=specs[i];const auto [lo,hi]=std::minmax({p.active.r,p.active.g,p.active.b});
  assert(hi-lo>=35&&theme_detail::packed_rgb(p.button)!=theme_detail::packed_rgb(p.active));
 }
 g_frame.activeTheme=ThemeVariant::ClassicLauncher;assert(!rounded_rectangles_enabled()&&g_frame.roundedRectStyle.enabled);
 assert(active_palette().beveledControls&&active_palette().captionUsesLightText);
 assert(theme_detail::packed_rgb(specs[2].title)==0x10246Au);
 GuiFontCache captionTemporary{77};g_temporaryTextFont=&captionTemporary;
 textFonts.clear();draw_caption_text("Caption",0,0,1);
 assert(textFonts==std::vector<int>{1}&&g_temporaryTextFont==&captionTemporary);g_temporaryTextFont=nullptr;
 g_frame.activeTheme=ThemeVariant::DefaultDark;assert(rounded_rectangles_enabled());
 g_frame.roundedRectStyle.enabled=false;assert(!rounded_rectangles_enabled());g_frame.roundedRectStyle.enabled=true;
 g_frame.activeTheme=ThemeVariant::SlateOrchard;check(false);
 g_publishedWindowVariant.store(ThemeVariant::SlateOrchard);
 assert(native_window_chrome_palette().paletteId==9 && !native_window_chrome_palette().isLight);
 assert(native_window_chrome_palette().backgroundRgb==epochengine::gui::theme_detail::packed_rgb(specs[9].window));
 g_frame.activeTheme=ThemeVariant::GraphiteCopper;check(false);
 g_publishedWindowVariant.store(ThemeVariant::GraphiteCopper);
 assert(native_window_chrome_palette().paletteId==8 && !native_window_chrome_palette().isLight);
 g_frame.activeTheme=ThemeVariant::DefaultDark;check(false);
 g_frame.activeTheme=ThemeVariant::DefaultLight;check(true);
 g_frame.activeTheme=ThemeVariant::ClassicLauncher;check(true);
 publish_native_window_theme(ThemePreference::ClassicLauncher);
 assert(native_window_chrome_palette().isLight && !native_window_prefers_dark());
 publish_native_window_theme(ThemePreference::ThemeCycle);
 assert(!g_cycleWindowDark.load() && !native_window_prefers_dark());
 GuiFontCache temporary{77};g_temporaryTextFont=&temporary;
 assert(active_font_cache().id==77);g_temporaryTextFont=nullptr;
 g_frame.activeTheme=ThemeVariant::StudioNavy;check(false);
 g_temporaryTextFont=&temporary;assert(!lookup_shadow_glyph(U'A'));
 g_temporaryTextFont=nullptr;
 // Missing sprites fall back to the complete Dark palette, including its font.
 g_frame.activeTheme=ThemeVariant::ClassicLauncher;
 g_resources.classicLauncher.buttonNormal={};check(false);
 check_slider_reflow();
}
'''.replace('REGISTRATION', registration).replace('PLACEHOLDER', placeholder).replace('OWNED_CALLS', registration_calls)
        # Read the actual normal font atlas colors rather than inventing a
        # second palette/foreground policy in the fixture.
        for token, cache in (('LIGHT_INK','font'),('DARK_INK','darkTextFont')):
            color = re.search(r'loadCache\(\s*g_resources\.'+cache+r',\s*font::FontColor\{([^}]+)\}', source).group(1)
            main = main.replace(token, 'theme_detail::Rgb{'+color+'}')
        prefix = prefix.replace('ACTUAL_SPEC_HEADER', (GUI/'gui.theme_palette_spec.hpp').as_posix())
        parent = ROOT/'Artifacts/Reports/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='gui-palette-font-', dir=parent))
        try:
            cpp = scratch/'contract.cpp'
            cpp.write_text(prefix+functions+main, encoding='utf-8')
            if os.name == 'nt':
                tools = msvc_2022_toolsets()
                sdk = Path('C:/Program Files (x86)/Windows Kits/10')
                kits = windows_sdk_includes(sdk)
                if not tools or not kits:
                    self.skipTest('Installed MSVC/SDK required')
                tool, kit = tools[-1], kits[-1]
                binary = scratch/'contract.exe'
                args = [str(tool/'bin/Hostx64/x64/cl.exe'), '/nologo', '/utf-8', '/std:c++latest', '/EHsc', '/W4', '/WX',
                        '/I'+str(tool/'include'), '/I'+str(kit/'ucrt'),
                        '/Fo'+str(scratch/'contract.obj'), '/Fe'+str(binary), str(cpp), '/link',
                        '/LIBPATH:'+str(tool/'lib/x64'), '/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),
                        '/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
            else:
                compiler = shutil.which('clang++') or shutil.which('g++')
                if not compiler:
                    self.skipTest('C++23 compiler required')
                binary = scratch/'contract'
                args = [compiler, '-std=c++23', str(cpp), '-o', str(binary)]
            modes = [('Debug',['/Od','/MDd','/D_DEBUG']),('Release',['/O2','/MD'])] if os.name=='nt' else [('Debug',['-O0']),('Release',['-O2'])]
            for mode, flags in modes:
                with self.subTest(configuration=mode):
                    built = subprocess.run(args[:1]+flags+args[1:], cwd=scratch, capture_output=True, text=True, timeout=60)
                    self.assertEqual(built.returncode, 0, built.stdout[-7000:]+built.stderr)
                    result = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode, 0, 'Actual palette contract failed at generated C++ line '+str(result.returncode)+': '+result.stdout+result.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)


if __name__ == '__main__':
    unittest.main()
