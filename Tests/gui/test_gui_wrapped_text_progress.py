"""Execute production UTF-8 wrapping with recording CPU font/draw services."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import msvc_2022_toolsets

ROOT = Path(__file__).resolve().parents[2]


class WrappedTextProgress(unittest.TestCase):
    def test_sanitized_controls_advance_in_measure_caret_and_draw(self):
        source = (ROOT / 'Engine/src/epochgui/gui.engine.cpp').read_text(encoding='utf-8')
        decoder = definition(source, '        [[nodiscard]] static DecodedUtf8Codepoint decode_utf8_codepoint(')
        decoder = decoder.replace('decode_utf8_codepoint(', 'decode_utf8_unchecked(', 1)
        prefix = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
struct Vec2 { float x{}, y{}; };
namespace font { struct Glyph; }
struct GuiFontCache { bool asset{true}; const font::Glyph* fallbackGlyph{}; };
struct Frame { void* ctx{reinterpret_cast<void*>(1)}; Vec2 contentMin{}, contentMax{1000,1000}; } g_frame;
struct Resources { GuiFontCache statusInfoLightFont{},statusInfoFont{},statusSuccessLightFont{},statusSuccessFont{}; } g_resources;
struct StatusKeyword { std::string_view word{}; const GuiFontCache* font{}; };
StatusKeyword status_keyword_at(std::string_view,std::size_t) { return {}; }
bool active_palette_is_light() { return false; }
const GuiFontCache& active_font_cache() { static GuiFontCache font; return font; }
bool clipping{};
float metric{1};
void ensure_resources() {}
bool has_content_clip() { return clipping; }
float space_advance(float) { return metric; }
float line_advance_amount(float) { return metric; }
float baseline_offset(float) { return 0; }
float base_line_height(float) { return metric; }
float glyph_advance_with_kerning(char32_t,std::optional<char32_t>,float) { return metric; }
struct Sprite { bool is_valid() const { return true; } };
namespace font { struct Glyph { Vec2 size_px{1,1},offset_px{}; Sprite handle{}; }; }
const font::Glyph* lookup_glyph(char32_t) { static font::Glyph value; return &value; }
const font::Glyph* lookup_glyph_from_cache(const GuiFontCache&,char32_t c) { return lookup_glyph(c); }
const font::Glyph* lookup_shadow_glyph(char32_t) { return nullptr; }
bool glyph_rect_is_reasonable(float,float,float) { return true; }
float align_leading_glyph_to_clip(float pen,float,float,float) { return pen; }
bool glyph_intersects_clip(float,float,float,float,float,float,float,float) { return true; }
std::size_t sprites{};
void draw_sprite(Sprite,float,float,float,float) { ++sprites; }
std::size_t decodeCalls{}, decodeBudget{};
'''
        functions = '\n'.join(definition(source, marker, semi) for marker, semi in (
            ('        [[nodiscard]] static char32_t safe_draw_codepoint(', False),
            ('        [[nodiscard]] static bool is_utf8_continuation_byte(', False),
            ('        struct DecodedUtf8Codepoint', True)))
        functions += '\n' + decoder + r'''
static DecodedUtf8Codepoint decode_utf8_codepoint(std::string_view text,std::size_t at) noexcept {
 // Deterministic instrumentation catches a non-progress loop without a timed
 // GUI/runtime probe. Decoding itself remains the production implementation.
 if (++decodeCalls > decodeBudget) std::_Exit(61);
 return decode_utf8_unchecked(text,at);
}
'''
        functions += '\n'.join(definition(source, marker) for marker in (
            '        [[nodiscard]] static std::optional<char32_t> next_drawable_char(',
            '        [[nodiscard]] static bool is_wrap_space(',
            '        [[nodiscard]] static float measure_word_advance(',
            '        [[nodiscard]] static float measure_wrapped_text_height(',
            '        [[nodiscard]] static Vec2 compute_caret_position('))
        lexer = source.index('        enum class CodeInk')
        draw = source.index('        static float draw_wrapped_text(', lexer)
        functions += '\n' + source[lexer:draw] + definition(source, '        static float draw_wrapped_text(')
        main = r'''
void reset(std::string_view text) { decodeCalls=0;decodeBudget=4096+text.size()*128;sprites=0; }
void check(std::string_view text,float width,bool verifyGeometry=true) {
 const std::string saved{text};
 reset(text);const float height=measure_wrapped_text_height(text,width,1);
 reset(text);const auto caret=compute_caret_position(text,0,0,width,1);
 reset(text);const float drawn=draw_wrapped_text(text,0,0,width,1);
 assert(text==saved);
 if (verifyGeometry) {
  assert(std::isfinite(height)&&std::isfinite(drawn)&&std::isfinite(caret.x)&&std::isfinite(caret.y));
  if(!clipping) assert(std::abs(height-drawn)<0.001f);
 }
}
int main() {
 // CR in a binary Windows log was normalized to a space by the outer loop,
 // then rejected by the old raw-byte whitespace scan: runEnd never advanced.
 for (const std::string text : {"\rCancelled", "Build\r\nCancel requested\rProgress 10%\rDone",
       "\t \r\r\nnext", "singleword", "\n\n", "", "a\tb  c"})
  for(float width : {0.0f,0.1f,1.0f,4.0f,80.0f}) check(text,width);
 for(unsigned control=0;control<32;++control) {
  const std::string text=std::string("before")+char(control)+"after";
  check(text,2);check(text,200);
 }
 for(unsigned control=0x7f;control<0xa0;++control) {
  std::string text="before";
  if(control<0x80) text+=char(control);
  else {text+="\xc2";text+=char(control);}
  text+="after";check(text,2);check(text,200);
 }
 check("\xce\xa9\xe6\xbc\xa2\xf0\x9f\x94\xa5\r\nUnicode",2);
 check(std::string(512,'W'),0.1f);
 check(std::string("bad\xff\xc0\x80",6),3);
 clipping=true;g_frame.contentMax={2,3};check("top\r\nnext\r\nclipped",2);clipping=false;
 for(float width : {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) check("a\rb",width);
 metric=0;check("a\rb\r\nc",0);
 // Broken font metrics must not prevent consuming bytes. This test does not
 // assert a drawable/finite layout from nonfinite font input.
 for(float bad : {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
  metric=bad;check("a\rb\r\nc",1,false);
 }
 metric=1;
}
'''
        parent = ROOT / 'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='wrapped-progress-', dir=parent))
        try:
            cpp = scratch / 'contract.cpp'
            cpp.write_text(prefix + functions + main, encoding='utf-8')
            if os.name == 'nt':
                tools = msvc_2022_toolsets()
                sdk = Path('C:/Program Files (x86)/Windows Kits/10')
                kits = sorted((sdk / 'Include').glob('*'))
                if not tools or not kits:
                    self.skipTest('Installed MSVC/SDK required')
                tool, kit = tools[-1], kits[-1]
                binary = scratch / 'contract.exe'
                command = [str(tool / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/utf-8', '/std:c++latest', '/EHsc', '/W4', '/WX',
                           '/I' + str(tool / 'include'), '/I' + str(kit / 'ucrt'),
                           '/Fo' + str(scratch / 'contract.obj'), '/Fe' + str(binary), str(cpp), '/link',
                           '/LIBPATH:' + str(tool / 'lib/x64'), '/LIBPATH:' + str(sdk / 'Lib' / kit.name / 'um/x64'),
                           '/LIBPATH:' + str(sdk / 'Lib' / kit.name / 'ucrt/x64')]
            else:
                compiler = shutil.which('clang++') or shutil.which('g++')
                if not compiler:
                    self.skipTest('C++23 compiler required')
                binary = scratch / 'contract'
                command = [compiler, '-std=c++23', str(cpp), '-o', str(binary)]
            built = subprocess.run(command, cwd=scratch, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout[-7000:] + built.stderr)
            run = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=10)
            self.assertEqual(run.returncode, 0, f'Actual wrapping exit={run.returncode}; 61 means byte progress budget exceeded.\n' + run.stdout + run.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)


if __name__ == '__main__':
    unittest.main()
