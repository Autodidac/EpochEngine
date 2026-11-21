"""Production tooltip geometry and UTF-8 wrapping with recording font/paint ports."""
from pathlib import Path
import hashlib
import json
import sys
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class TooltipWrapping(unittest.TestCase):
    def test_shipped_font_label_fits_after_dropdown_padding_round_trip(self):
        source = (ROOT / 'Engine/src/epochgui/gui.engine.cpp').read_text(encoding='utf-8')
        declarations = [
            definition(source, '        struct DecodedUtf8Codepoint', True),
            *[definition(source, '        [[nodiscard]] static ' + signature)
              for signature in ('bool is_utf8_continuation_byte(', 'DecodedUtf8Codepoint decode_utf8_codepoint(',
                                'char32_t safe_draw_codepoint(', 'std::optional<char32_t> next_drawable_char(',
                                'float glyph_advance_with_kerning(', 'float measure_text_width(',
                                'std::string fit_text_to_width(')],
        ]
        code = r'''
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

stbtt_fontinfo font{};
float pixelScale{};
float glyph_advance(char32_t ch, float scale) {
    int advance{};
    stbtt_GetCodepointHMetrics(&font, ch, &advance, nullptr);
    return (pixelScale * advance) * scale;
}
float kerning_adjust(char32_t left, char32_t right, float scale) {
    return (pixelScale * stbtt_GetCodepointKernAdvance(&font, left, right)) * scale;
}
float letter_spacing(float) { return 0.0f; }
OWNERS

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(input), {}};
    if (bytes.empty() || !stbtt_InitFont(&font, bytes.data(), 0)) return 2;
    pixelScale = stbtt_ScaleForPixelHeight(&font, 21.0f);
    const std::string label = "Move: Select";
    const float measured = measure_text_width(label, 1.0f);
    // Closed select box reserves the 24px indicator, two 6px insets and 2px.
    const float box = measured + 24.0f + 12.0f + 2.0f;
    const float available = box - 12.0f - 2.0f - 24.0f;
    if (std::nextafter(available, measured) != measured || available == measured) return 3;
    if (fit_text_to_width(label, available, 1.0f) != label) return 4;
    if (fit_text_to_width(label, available - 2.0f, 1.0f) != "Move: Sel...") return 5;
    // A word boundary plus ellipsis gets the same fit tolerance as a full label.
    const float prefix = measure_text_width("Move: ...", 1.0f);
    if (fit_text_to_width(label, std::nextafter(prefix, 0.0f), 1.0f) != "Move: ...") return 6;
    if (fit_text_to_width(label, prefix - 1.0f, 1.0f) != "Move:...") return 7;
    if (!fit_text_to_width(label, 0.0f, 1.0f).empty()) return 8;
}
'''.replace('OWNERS', '\n'.join(declarations))
        report = ROOT / 'Artifacts/Reports/text-fit-roundtrip' / hashlib.sha256(code.encode()).hexdigest()[:16] / sys.platform
        report.mkdir(parents=True, exist_ok=True)
        fixture = report / 'roundtrip.cpp'
        fixture.write_text(code, encoding='utf-8')
        results = compile_and_run(self, fixture, report,
                                  args=(str(ROOT / 'Engine/assets/fonts/Roboto-Regular.ttf'),),
                                  includes=(ROOT / 'Engine/src/stb',))
        (report / 'results.json').write_text(json.dumps(results, indent=2), encoding='utf-8')

    def test_actual_wrapping_bounds_and_noninteractive_overlay(self):
        source = (ROOT / 'Engine/src/epochgui/gui.engine.cpp').read_text(encoding='utf-8')
        buttons = (ROOT / 'Engine/src/epochgui/controls/gui.buttons_images.inl').read_text(encoding='utf-8')
        owners = '\n'.join(definition(source, marker, semi) for marker, semi in (
            ('        struct DecodedUtf8Codepoint', True),
            ('        [[nodiscard]] static bool is_utf8_continuation_byte(', False),
            ('        [[nodiscard]] static DecodedUtf8Codepoint decode_utf8_codepoint(', False),
            ('        [[nodiscard]] static char32_t safe_draw_codepoint(', False),
            ('        [[nodiscard]] static std::optional<char32_t> next_drawable_char(', False),
            ('        [[nodiscard]] static bool glyph_rect_is_reasonable(', False),
            ('        [[nodiscard]] static bool glyph_intersects_clip(', False),
            ('        [[nodiscard]] static constexpr float align_leading_glyph_to_clip(', False),
            ('        [[nodiscard]] static float measure_text_width(', False),
            ('        [[nodiscard]] static std::string fit_text_to_width(', False),
            ('        [[nodiscard]] static bool is_wrap_space(', False),
            ('        [[nodiscard]] static float measure_word_advance(', False),
            ('        [[nodiscard]] static float measure_wrapped_text_height(', False)))
        lexer = source.index('        enum class CodeInk')
        draw = source.index('        static float draw_wrapped_text(', lexer)
        owners += source[lexer:draw] + definition(source, '        static float draw_wrapped_text(')
        owners += definition(source, '    ContentClipScope::ContentClipScope(')
        owners += definition(source, '    ContentClipScope::~ContentClipScope(')
        owners += definition(buttons, '    static void draw_button_tooltip(')
        owners += definition(source, '        struct ContentClipClearScope', True)
        navigation = (ROOT / 'Engine/src/epochgui/controls/gui.navigation_chrome.inl').read_text(encoding='utf-8')
        status = definition(navigation, '    StatusStripResult status_strip(')
        owners += '\nvoid status_tooltip(std::string_view hoverLabel,Vec2 hoverPosition)' + definition(status, '        if (!hoverLabel.empty())').replace('if (!hoverLabel.empty())', '', 1)
        code = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
#define CHECK(c) do{if(!(c))return __LINE__;}while(false)
struct Vec2{float x{},y{};};
struct Sprite{int id{1};bool is_valid()const{return id!=0;}};
namespace font {struct Glyph{Vec2 size_px{6,14},offset_px{};Sprite handle{};};}
struct GuiFontCache{bool asset{true};const font::Glyph*fallbackGlyph{};};
struct Context{int get_width_safe(){return 1898;}int get_height_safe(){return 1000;}}context;
struct Frame{Context*ctx{&context};Vec2 contentMin{},contentMax{800,600},cursor{31,27},origin{},windowSize{1898,84};int depth{};}g_frame;
struct Resources{GuiFontCache statusInfoLightFont,statusInfoFont,statusSuccessLightFont,statusSuccessFont;}g_resources;
struct StatusKeyword{std::string_view word;const GuiFontCache*font{};};
StatusKeyword status_keyword_at(std::string_view,std::size_t){return {};}
struct Palette{Sprite panelBackground{2},headingAccent{3};}palette;
const Palette&active_palette(){return palette;}bool active_palette_is_light(){return false;}
const GuiFontCache&active_font_cache(){static GuiFontCache value;return value;}
void ensure_resources(){}bool has_content_clip(){return true;}
constexpr float kFontScale=1;
float space_advance(float){return 6;}float line_advance_amount(float){return 16;}
float base_line_height(float){return 14;}float baseline_offset(float){return 0;}
float glyph_advance_with_kerning(char32_t,std::optional<char32_t>,float){return 6;}
const font::Glyph*lookup_glyph(char32_t){static font::Glyph glyph;return &glyph;}
const font::Glyph*lookup_glyph_from_cache(const GuiFontCache&,char32_t c){return lookup_glyph(c);}
const font::Glyph*lookup_shadow_glyph(char32_t){return nullptr;}
struct Draw{int sprite;float x,y,w,h;int depth;};std::vector<Draw>draws;
struct Text{std::string text;float x,y;};std::vector<Text>text;
void draw_sprite(Sprite sprite,float x,float y,float w,float h){
 const float right=(std::min)(x+w,g_frame.contentMax.x),bottom=(std::min)(y+h,g_frame.contentMax.y);
 x=(std::max)(x,g_frame.contentMin.x);y=(std::max)(y,g_frame.contentMin.y);
 if(right>x&&bottom>y)draws.push_back({sprite.id,x,y,right-x,bottom-y,g_frame.depth});}
void draw_sprite_raw(Sprite s,float x,float y,float w,float h){draw_sprite(s,x,y,w,h);}
struct Style{bool enabled;float control_radius;};
void draw_rounded_sprite(Sprite s,float x,float y,float w,float h,Style){draw_sprite(s,x,y,w,h);}
void draw_window_shadow(Vec2 p,Vec2 size,bool){draw_sprite({4},p.x,p.y,size.x,size.y);}
void begin_top_layer(){++g_frame.depth;}void end_top_layer(){--g_frame.depth;}
void draw_text_line(std::string_view s,float x,float y,float){text.push_back({std::string{s},x,y});}
struct ContentClipScope{Vec2 previousMin,previousMax;ContentClipScope(Vec2,Vec2)noexcept;~ContentClipScope()noexcept;};
OWNERS
int main(){
 g_frame.contentMin={0,0};g_frame.contentMax={1898,84};
 status_tooltip(std::string(1000,'W'),{1880,38});
 CHECK(g_frame.contentMax.x==1898&&g_frame.contentMax.y==84&&g_frame.depth==0);
 const auto toolbarBody=std::ranges::find_if(draws,[](const Draw&d){return d.sprite==2;});
 CHECK(toolbarBody!=draws.end()&&toolbarBody->w<=340&&toolbarBody->h>84&&toolbarBody->x+toolbarBody->w<=1898);
 CHECK(toolbarBody->y+toolbarBody->h<=1000);
 draws.clear();text.clear();
 const std::string longWords="An unusually long tooltip should wrap naturally into several readable lines while retaining its complete explanation.";
 for(float width:{26.f,30.f,37.f,70.f,120.f,340.f,800.f})for(float height:{60.f,180.f,600.f}){
  for(const std::string message:{longWords,std::string(200,'W'),std::string("Unicode \xce\xa9 \xe6\xbc\xa2\nSecond line")}){
   g_frame.contentMin={11,17};g_frame.contentMax={11+width,17+height};
   draws.clear();text.clear();draw_button_tooltip(message,{10+width,16+height},20);
   CHECK(g_frame.depth==0&&g_frame.cursor.x==31&&g_frame.cursor.y==27);
   CHECK(g_frame.contentMin.x==11&&g_frame.contentMin.y==17&&g_frame.contentMax.x==11+width&&g_frame.contentMax.y==17+height);
   const auto body=std::ranges::find_if(draws,[](const Draw&d){return d.sprite==2;});CHECK(body!=draws.end());
   CHECK(body->w<=340&&body->w<=width&&body->h<=height&&body->h<=136);
   for(const auto&t:text){CHECK(t.x+measure_text_width(t.text,kFontScale)<=body->x+body->w-10);}
   for(const auto&d:draws){CHECK(d.depth==1&&d.x>=11&&d.y>=17&&d.x+d.w<=11+width&&d.y+d.h<=17+height);}
   for(const auto&d:draws)if(d.sprite==1){CHECK(d.x>=body->x+10&&d.x+d.w<=body->x+body->w-10);CHECK(d.y>=body->y+5&&d.y+d.h<=body->y+body->h-5);}
   if(width==800&&height==600&&message==longWords){
    CHECK(body->w==340&&body->h>24&&text.empty());
    const auto count=std::ranges::count_if(draws,[](const Draw&d){return d.sprite==1;});
    CHECK(count==std::ranges::count_if(message,[](char c){return c!=' ';}));
   }
  }
 }
 g_frame.contentMin={0,0};g_frame.contentMax={340,60};draws.clear();text.clear();
 draw_button_tooltip(std::string(400,'W'),{0,0},20);CHECK(text.size()==1&&text.front().text=="...");
 g_frame.contentMax={340,24};draws.clear();text.clear();draw_button_tooltip(longWords,{0,0},20);
 CHECK(text.size()==1&&text.front().text.ends_with("..."));
 g_frame.contentMax={20,24};draws.clear();text.clear();draw_button_tooltip(longWords,{0,0},20);CHECK(draws.empty()&&text.empty());
 g_frame.contentMax={340,23};draw_button_tooltip(longWords,{0,0},20);CHECK(draws.empty()&&text.empty());
}
'''.replace('OWNERS', owners)
        report = ROOT / 'Artifacts/Reports/tooltip-wrapping-nov16' / hashlib.sha256(code.encode()).hexdigest()[:16]
        report.mkdir(parents=True, exist_ok=True)
        cpp = report / 'tooltip.cpp'
        cpp.write_text(code, encoding='utf-8')
        results = compile_and_run(self, cpp, report)
        (report / 'result.json').write_text(json.dumps({'passed': True, 'results': results,
            'scope': 'Actual tooltip, UTF-8 measurement and wrapping draw, edge clamping and clip restoration; recording paint/font ports, no native window or model.'}, indent=2), encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
