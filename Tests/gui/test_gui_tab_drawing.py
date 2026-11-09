"""Actual UTF8 glyph drawing, caret and hit tests: Tab advances without fallback ink."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]
OWNER=ROOT/"Engine/src/epochgui/gui.engine.cpp"

CODE=r"""
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
import epoch.gui;
namespace gui_lib=epochengine::gui_lib;
struct Vec2{float x{},y{};};
struct Sprite{unsigned id{};bool is_valid()const{return id!=0;}};
namespace font{struct Glyph{Vec2 size_px{7,12},offset_px{};Sprite handle;float advance=7;};}
struct FontAsset{std::map<char32_t,font::Glyph>glyphs;float get_kerning(char32_t,char32_t)const{return 0;}};
struct GuiFontCache{FontAsset* asset{};struct{float ascent=10,descent=2,lineHeight=14,lineGap{},spaceAdvance=6,averageAdvance=7;}metrics;
 std::array<const font::Glyph*,256>glyphLookup{};const font::Glyph* fallbackGlyph{};};
GuiFontCache cache;FontAsset asset;
struct{void*ctx=reinterpret_cast<void*>(1);Vec2 contentMin{},contentMax{2000,2000};}g_frame;
const GuiFontCache* g_temporaryTextFont{};bool clipped{};
void ensure_resources(){}bool has_content_clip(){return clipped;}
const GuiFontCache& active_font_cache(){return cache;}
const font::Glyph*lookup_shadow_glyph(char32_t){return nullptr;}
struct Ink{unsigned id;float x,y,w,h;};std::vector<Ink>ink;
void draw_sprite(Sprite glyph,float x,float y,float w,float h){ink.push_back({glyph.id,x,y,w,h});}
void check(bool good,int line){if(!good){std::fprintf(stderr,"Whitespace draw failed line %d\n",line);std::exit(83);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),__LINE__)
CONSTANTS
FUNCTIONS
int main(){
 for(char32_t c:{U'?',U'A',U'M',U'R',U'X',U'\u00e9'}){font::Glyph glyph;glyph.handle.id=static_cast<unsigned>(c);asset.glyphs.emplace(c,glyph);}
 font::Glyph space;space.handle.id=0;space.advance=6;asset.glyphs.emplace(U' ',space);
 cache.asset=&asset;for(auto&[c,glyph]:asset.glyphs)if(c<cache.glyphLookup.size())cache.glyphLookup[c]=&glyph;
 cache.fallbackGlyph=&asset.glyphs.at(U'?');
 // An actual missing-tab cache returns '?'; drawing must not ask it to paint
 // that fallback. Its width comes from the existing four-space metric.
 CHECK(lookup_glyph(U'\t')==cache.fallbackGlyph);
 for(float scale:{0.5f,1.0f,2.0f}){
  ink.clear();draw_text_line("\tR",10,0,scale);CHECK(ink.size()==1&&ink[0].id=='R');
  const auto advance=space_advance(scale)*float(kTabSpaces);CHECK(ink[0].x==10+advance);
  CHECK(edit_box_x_for_index("\tR",0,1,scale)==advance);
  CHECK(edit_box_index_from_point("\tR",10+advance+0.01f,0,10,0,{},line_advance_amount(scale),scale,true,1000)==1);
  ink.clear();draw_text_line("\t\tR",10,0,scale);CHECK(ink.size()==1&&ink[0].x==10+2*advance);
  ink.clear();draw_text_line(" \tR",10,0,scale);CHECK(ink.size()==1&&ink[0].x==10+space_advance(scale)+advance);
  const std::string utf8="\xc3\xa9\tR";const auto original=utf8;
  ink.clear();draw_text_line(utf8,10,0,scale);CHECK(ink.size()==2&&ink[0].id==233&&ink[1].id=='R');
  const auto afterTab=edit_box_x_for_index(utf8,0,3,scale);CHECK(ink[1].x==10+afterTab&&utf8==original);
  CHECK(edit_box_index_from_point(utf8,10+afterTab+0.01f,0,10,0,{},line_advance_amount(scale),scale,true,1000)==3);
  ink.clear();draw_text_line("\t\n \t",10,0,scale);CHECK(ink.empty());
  ink.clear();draw_text_line("?\tR",10,0,scale);CHECK(ink.size()==2&&ink[0].id=='?'&&ink[1].id=='R');
  ink.clear();draw_text_line("\xe2\x82\xac\tR",10,0,scale);CHECK(ink.size()==2&&ink[0].id=='?'&&ink[1].id=='R');
  clipped=true;ink.clear();draw_text_line("R\n\tR",10,0,scale);CHECK(ink.size()==2&&ink[1].x==10+advance);
  CHECK(std::abs(ink[1].y-ink[0].y-line_advance_amount(scale))<0.0001f);
  clipped=false;
 }
 clipped=true;g_frame.contentMin={0,0};g_frame.contentMax={30,100};
 ink.clear();draw_text_line("\tR",10,0,1);CHECK(ink.empty());
 g_frame.contentMin={20,0};g_frame.contentMax={60,100};
 ink.clear();draw_text_line("\tR",10,0,1);CHECK(ink.size()==1&&ink[0].id=='R'&&ink[0].x==34);
 return 0;
}
"""

class GuiTabDrawing(unittest.TestCase):
    def test_actual_whitespace_ink_metrics_and_utf8_hit_test_debug_release(self):
        if os.name!="nt":self.skipTest("Installed MSVC CPU module consumer")
        source=OWNER.read_text(encoding="utf-8")
        text=(ROOT/"Engine/src/epochgui/controls/gui.text_controls.inl").read_text(encoding="utf-8")
        import re
        constants="\n".join(re.search(r"^\s*constexpr[^\n]*\b"+name+r"\b[^\n]*",source,re.M)[0] for name in
            ("kLineSpacingFactor","kLetterSpacingFactor","kGlyphRasterPadding","kTabSpaces"))
        functions="\n".join(definition(source,marker,marker.endswith("Codepoint")) for marker in (
            "        [[nodiscard]] static char32_t safe_draw_codepoint(","        struct DecodedUtf8Codepoint",
            "        [[nodiscard]] static bool is_utf8_continuation_byte(",
            "        [[nodiscard]] static DecodedUtf8Codepoint decode_utf8_codepoint(",
            "        [[nodiscard]] static std::optional<char32_t> next_drawable_char(",
            "        [[nodiscard]] static const font::Glyph* lookup_glyph_from_cache(",
            "        [[nodiscard]] static const font::Glyph* lookup_glyph(",
            "        [[nodiscard]] static float base_line_height(","        [[nodiscard]] static float line_advance_amount(",
            "        [[nodiscard]] static float baseline_offset(","        [[nodiscard]] static float space_advance(",
            "        [[nodiscard]] static float letter_spacing(","        [[nodiscard]] static float kerning_adjust(",
            "        [[nodiscard]] static float glyph_advance(","        [[nodiscard]] static float glyph_advance_with_kerning(",
            "        [[nodiscard]] static bool glyph_rect_is_reasonable(","        [[nodiscard]] static bool glyph_intersects_clip(",
            "        [[nodiscard]] static constexpr float align_leading_glyph_to_clip(",
            "        [[nodiscard]] static float measure_text_width(","        [[nodiscard]] static bool is_wrap_space(",
            "        static void draw_text_line("))
        functions+="\n"+"\n".join(definition(text,marker) for marker in (
            "    [[nodiscard]] static float edit_box_x_for_index(","    [[nodiscard]] static gui_lib::TextGlyphMetrics edit_box_glyph_metrics(",
            "    [[nodiscard]] static std::vector<gui_lib::TextVisualLine> edit_box_visual_lines(",
            "    [[nodiscard]] static std::size_t edit_box_index_from_point("))
        code=CODE.replace("CONSTANTS",constants).replace("FUNCTIONS",functions)
        identity=hashlib.sha256(code.encode()).hexdigest()[:12]
        lane=ROOT/"Artifacts/Reports/gui-tab-drawing-oct28"/identity;lane.mkdir(parents=True,exist_ok=True)
        (lane/"consumer.cpp").write_text(code,encoding="utf-8")
        portable=ROOT/"Engine/dep/EpochGui"
        (lane/"CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.30)\nproject(GuiTabDrawing LANGUAGES CXX)\nadd_executable(whitespace consumer.cpp "'+(portable/'src/epochgui/gui.text_layout.cpp').as_posix()+'")\ntarget_compile_features(whitespace PRIVATE cxx_std_23)\ntarget_compile_options(whitespace PRIVATE /W3 /WX /utf-8 /EHsc)\ntarget_sources(whitespace PRIVATE FILE_SET CXX_MODULES BASE_DIRS "'+portable.as_posix()+'" FILES "'+(portable/'modules/epoch.gui.ixx').as_posix()+'")\nenable_testing()\nadd_test(NAME actual_tab_drawing COMMAND whitespace)\n',encoding="utf-8")
        build=ROOT/"Artifacts/Intermediate/cmake/gui-tab-drawing"/identity
        commands=[["cmake","-S",str(lane),"-B",str(build),"-G","Visual Studio 17 2022","-A","x64"]]
        for config in ("Debug","Release"):commands += [["cmake","--build",str(build),"--config",config,"--parallel","1"],["ctest","--test-dir",str(build),"-C",config,"--output-on-failure"]]
        runs=[]
        for index,command in enumerate(commands):
            r=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=90)
            (lane/f"lane-{index}.log").write_text(r.stdout+r.stderr,encoding="utf-8");runs.append({"command":command,"exit":r.returncode})
            (lane/"result.json").write_text(json.dumps({"passed":r.returncode==0 and index==len(commands)-1,"commands":runs,"owner_sha256":hashlib.sha256(OWNER.read_bytes()).hexdigest(),"scope":"Actual shared UTF8 draw/advance/caret/hit-test functions with missing-tab fallback font and recording draw port; real portable text layout. No GPU/native/model call."},indent=2),encoding="utf-8")
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)

if __name__=="__main__":unittest.main()
