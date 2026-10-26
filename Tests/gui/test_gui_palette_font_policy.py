"""Execute real palette/font/control selectors with recording CPU draw services."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
GUI = ROOT/'Engine/src/epochgui'


class PaletteFontPolicy(unittest.TestCase):
    def test_registered_light_metadata_drives_fonts_controls_and_native_policy(self):
        source = (GUI/'gui.engine.cpp').read_text(encoding='utf-8')
        text = (GUI/'controls/gui.text_controls.inl').read_text(encoding='utf-8')
        buttons = (GUI/'controls/gui.buttons_images.inl').read_text(encoding='utf-8')
        nodes = (GUI/'controls/gui.node_surface_controls.inl').read_text(encoding='utf-8')
        variant = definition((ROOT/'Engine/modules/gui.engine.ixx').read_text(encoding='utf-8'),
            '    export enum class ThemeVariant', True).replace('export enum class', 'enum class', 1)
        palette = definition(source, '            struct PaletteSprites', True)
        semantic = definition(source, '            struct SemanticSpritePair', True)
        functions = '\n'.join(definition(source, marker) for marker in (
            '        [[nodiscard]] static bool complete_palette(',
            '        [[nodiscard]] static const GuiResources::PaletteSprites& active_palette(',
            '        [[nodiscard]] static bool active_palette_is_light(',
            '        [[nodiscard]] static const GuiResources::SemanticSpritePair& active_semantic_pair(',
            '        [[nodiscard]] static const GuiFontCache& active_font_cache(',
            '        [[nodiscard]] static const font::Glyph* lookup_shadow_glyph(',
            '        [[nodiscard]] static StatusKeyword status_keyword_at(',
            '        static float draw_wrapped_text(',
            '    void publish_native_window_theme(',
            '    NativeWindowChromePalette native_window_chrome_palette(',
            '    bool native_window_prefers_dark('))
        functions += '\n'+definition(text, '    void status_label(')
        functions += '\n'+definition(buttons, '    bool menu_item(')
        functions += '\n'+definition(nodes, '    SliderResult slider(')
        registration = definition(source, '                const auto registerPalette = [&atlas](')
        # Execute the actual placeholder branch without constructing an entire
        # edit-box/controller; font metrics and draw services are recording fakes.
        placeholder = definition(text, '            if (view.empty() && !placeholder.empty())')
        prefix = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
struct Vec2 { float x{},y{}; };
struct SpriteHandle { int id{}; bool is_valid() const noexcept {return id!=0;} };
struct WidgetBounds { Vec2 position{},size{}; };
struct GuiFontCache { int id{}; bool asset{true}; };
THEME_VARIANT
enum class ThemePreference {FollowSystemDark,Light,Dark,ThemeCycle,ClassicLauncher};
enum class SemanticTone {neutral,muted,info,success,warning,error,assistant,engine};
namespace theme_detail {
 struct Rgb {std::uint8_t r{},g{},b{},a{255};};
 struct Palette {std::string_view atlasPrefix{},roundedPrefix{};Rgb window{},button{},hover{},active{},input{},inputActive{},panel{},console{},title{},scrim{};bool isLight{};};
 inline std::array<Palette,8> fixtures{};
 const Palette& palette_at(std::size_t index) {return fixtures[index<8?index:0];}
 std::uint32_t packed_rgb(Rgb v) {return (std::uint32_t{v.r}<<16)|(std::uint32_t{v.g}<<8)|v.b;}
}
struct GuiResources {
PALETTE
SEMANTIC
 PaletteSprites defaultDark{},defaultLight{},classicLauncher{},midnightBlue{},emberForge{},forestTerminal{},auroraSteel{},codexBlack{};
 std::array<SemanticSpritePair,8> semanticDark{},semanticLight{};
 GuiFontCache font{1},darkTextFont{2},halfTextFont{3},halfTextLightFont{4},textShadowFont{13};
 GuiFontCache statusInfoFont{5},statusInfoLightFont{6},statusSuccessFont{7},statusSuccessLightFont{8};
 GuiFontCache statusWarningFont{9},statusWarningLightFont{10},statusErrorFont{11},statusErrorLightFont{12};
} g_resources;
struct Frame {ThemeVariant activeTheme{ThemeVariant::DefaultDark};void*ctx{reinterpret_cast<void*>(1)};bool insideWindow{true},mouseDown{},justReleased{};Vec2 cursor{},mousePos{},contentMin{},contentMax{1000,1000};std::optional<WidgetBounds> lastButtonBounds;} g_frame;
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
 float slider_value_from_position(const SliderLayoutOptions& o,float) {return o.value;}
}
struct TextureAtlas {};
int nextSprite=100;
int make_solid_pixels(std::uint8_t,std::uint8_t,std::uint8_t,std::uint8_t,int,int) {return 0;}
SpriteHandle add_sprite(TextureAtlas&,const std::string&,int,int,int) {return {++nextSprite};}
void register_rounded_control(TextureAtlas&,SpriteHandle,const std::string&,theme_detail::Rgb) {}
constexpr float kFontScale=1,kContentPadding=4;
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
std::vector<int> sprites,textFonts;
void draw_sprite(SpriteHandle s,float,float,float,float) {sprites.push_back(s.id);}
void draw_rounded_sprite(SpriteHandle s,float,float,float,float,gui_lib::rounded_rect::RoundedRectStyle) {sprites.push_back(s.id);}
void draw_text_line(std::string_view,float,float,float) {textFonts.push_back(active_font_cache().id);}
struct ContentClipScope {ContentClipScope(Vec2,Vec2) {}};
float content_right() {return 300;}
float menu_item_row_height() {return 30;}
float measure_text_width(std::string_view s,float) {return static_cast<float>(s.size());}
std::string fit_text_to_width(std::string_view s,float,float) {return std::string{s};}
bool point_in_rect(Vec2,float,float,float,float) {return false;}
bool point_in_active_clip(Vec2) {return true;}
std::size_t widget_press_key(std::string_view,Vec2,Vec2) {return 1;}
std::unordered_map<void*,std::size_t> g_contextPressedButtonKeys;
bool left_press_available() {return false;}
bool left_release_available() {return false;}
void consume_left_press() {}
void consume_left_release() {}
void advance_cursor(Vec2) {}
enum class MenuItemIndicator {None,Radio,Check};
struct MenuItemOptions {std::string_view label{},shortcut{};Vec2 size{};bool enabled{true},separator_before{},checked{};MenuItemIndicator indicator{MenuItemIndicator::None};};
struct MenuLayout {bool valid{true};WidgetBounds row{{0,0},{300,30}},label{{0,0},{180,30}},shortcut{{180,0},{80,30}},indicator{{270,0},{20,20}};};
MenuLayout make_menu_item_layout(Vec2,Vec2,float,float,float) {return {};}
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
int main() {
 auto& specs=theme_detail::fixtures;
 specs[0].isLight=false;specs[1].isLight=true;
 // A second independently authored light palette, with an existing registration
 // slot. It must behave by metadata, without adding another enum-name branch.
 specs[2].isLight=true;
 register_test_palette(g_resources.defaultDark,specs[0]);
 register_test_palette(g_resources.defaultLight,specs[1]);
 register_test_palette(g_resources.classicLauncher,specs[2]);
 register_test_palette(g_resources.midnightBlue,specs[3]);
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
}
'''.replace('REGISTRATION', registration).replace('PLACEHOLDER', placeholder)
        parent = ROOT/'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='gui-palette-font-', dir=parent))
        try:
            cpp = scratch/'contract.cpp'
            cpp.write_text(prefix+functions+main, encoding='utf-8')
            if os.name == 'nt':
                tools = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
                sdk = Path('C:/Program Files (x86)/Windows Kits/10')
                kits = sorted((sdk/'Include').glob('*'))
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
            built = subprocess.run(args, cwd=scratch, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout[-7000:]+built.stderr)
            result = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)


if __name__ == '__main__':
    unittest.main()
