"""Measure real Roboto21 sheets in the real four-page allocator, with CPU ports."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import time
import unittest
from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
# Real full-coverage pixel hashes captured before the coverage API change.
FULL_PIXEL_GOLDEN = (
    6549989033582135816, 18303729823005917667, 3835450728349986417,
    3135017900414105442, 7432004138350159806, 14760319885485566471,
    7418206647559535793, 3409090031279308077, 11954600213728813320,
    13209894093635576723, 9887164175293350757, 17260262330269219196,
    9127493290183503647,
)


class HeadingFontCapacity(unittest.TestCase):
    def test_real_roboto_ink_and_heading_capacity(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU evidence lane")
        engine = ROOT / "Engine"
        renderer_path = engine / "src/epochgui/font.renderer.cpp"
        renderer = renderer_path.read_text(encoding="utf-8")
        font_module = (engine / "modules/font.renderer.ixx").read_text(encoding="utf-8")
        atlas_module = (engine / "modules/atlas.texture.ixx").read_text(encoding="utf-8")
        texture_module = (engine / "modules/texture.core.ixx").read_text(encoding="utf-8")
        handle_module = (engine / "modules/sprite.handle.ixx").read_text(encoding="utf-8")
        gui = (engine / "src/epochgui/gui.engine.cpp").read_text(encoding="utf-8")
        inks = re.findall(r"loadCache\(\s*g_resources\.([\w]+),\s*font::FontColor\{([^}]+)\}", gui)
        self.assertEqual(len(inks), 13)
        ink_rows = ",\n".join(f'{{"{name}",{{{color}}}}}' for name, color in inks)
        lookup_functions = "\n".join(definition(gui, marker) for marker in (
            "        static void populate_font_lookup(",
            "        [[nodiscard]] static const font::Glyph* lookup_glyph_from_cache(",
            "        [[nodiscard]] static const GuiFontCache& font_cache_for_codepoint(",
            "        [[nodiscard]] static const font::Glyph* lookup_glyph(",
            "        [[nodiscard]] static float kerning_adjust("))

        def namespace_body(text, marker):
            return re.sub(r"\bexport\s+", "", text[text.index(marker):])

        code = r'''
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif
#define EPOCH_ENABLE_TEXTURE_DIAGNOSTICS 0
#define STB_TRUETYPE_IMPLEMENTATION
#include "STB_HEADER"
#include "PALETTE"
namespace epochengine {
template<class...Args> std::string format_text(std::string_view text,Args&&...){return std::string{text};}
namespace logger {
struct Logger {};
std::vector<std::string> errors;
void error(std::string_view,std::string_view text){errors.emplace_back(text);}
template<class...Args> void warnf_loc(Args&&...){}
template<class...Args> void infof_loc(Args&&...){}
}}
TEXTURE
HANDLE
ATLAS
// CPU-only registrar/upload recording boundary. TextureAtlas and FontRenderer
// implementations below are the actual owners; no renderer is constructed.
namespace epochengine::atlasmanager {
struct Registrar {std::unique_ptr<TextureAtlas> owner;TextureAtlas& atlas;
 explicit Registrar(const AtlasConfig& config):owner(TextureAtlas::create(config)),atlas(*owner){}};
std::unordered_map<std::string,std::unique_ptr<Registrar>> registrars;
Registrar* get_registrar(const std::string& name){const auto it=registrars.find(name);return it==registrars.end()?nullptr:it->second.get();}
bool create_atlas(AtlasConfig config){config.index=int(registrars.size());return registrars.emplace(config.name,std::make_unique<Registrar>(config)).second;}
void ensure_uploaded(const TextureAtlas&){}
}
FONT_DECLARATIONS
FONT_IMPLEMENTATION
#define CHECK(...) do {if(!(__VA_ARGS__))return __LINE__;}while(false)
namespace epochengine::gui::capacity_probe {
constexpr const char* kDefaultFontName="capacity";
constexpr float kDefaultFontSizePt=21.0f;
GUI_CACHE
struct Resources {GuiFontCache font,darkTextFont;}g_resources;
const GuiFontCache* selected{};
bool light{};
const GuiFontCache& active_font_cache(){return *selected;}
bool active_palette_is_light(){return light;}
LOOKUP_FUNCTIONS
int verify(const font::FontRenderer& renderer){
 g_resources.font.asset=renderer.get_font("font");
 g_resources.darkTextFont.asset=renderer.get_font("darkTextFont");
 populate_font_lookup(g_resources.font);populate_font_lookup(g_resources.darkTextFont);
 for(const auto& palette:theme_detail::palettes){
  GuiFontCache heading;
  heading.asset=renderer.get_font(std::string{palette.atlasPrefix}+"/heading_font");
  CHECK(heading.asset&&heading.asset->coverage==font::GlyphCoverage::basic_latin);
  populate_font_lookup(heading);selected=&heading;
  for(const bool isLight:{false,true}){
   light=isLight;
   const auto& normal=isLight?g_resources.darkTextFont:g_resources.font;
   CHECK(lookup_glyph(U'A')==&heading.asset->glyphs.at(U'A'));
   for(const auto codepoint:{U'\u03A9',U'\u0416',U'\u00E9'}){
    CHECK(&font_cache_for_codepoint(codepoint)==&normal);
    const auto* expected=&normal.asset->glyphs.at(codepoint);
    CHECK(lookup_glyph(codepoint)==expected&&lookup_glyph(codepoint)!=heading.fallbackGlyph);
    CHECK(expected->handle.is_valid());
   }
   CHECK(kerning_adjust(U'A',U'V',1.0f)==heading.asset->get_kerning(U'A',U'V'));
   unsigned leftFallback{},rightFallback{};
   for(const auto& [key,kern]:normal.asset->kerning_pairs){
    const auto left=char32_t(key>>32),right=char32_t(key&0xffffffffu);
    if(left>U'~'&&right<=U'~'&&kern!=0){CHECK(kerning_adjust(left,right,1.0f)==kern);++leftFallback;}
    if(right>U'~'&&left<=U'~'&&kern!=0){CHECK(kerning_adjust(left,right,1.0f)==kern);++rightFallback;}
   }
   CHECK(leftFallback&&rightFallback);
  }
 }
 GuiFontCache semantic;semantic.asset=renderer.get_font("statusInfoFont");populate_font_lookup(semantic);selected=&semantic;
 CHECK(lookup_glyph(U'\u03A9')==&semantic.asset->glyphs.at(U'\u03A9'));
 CHECK(font_cache_for_codepoint(U'\u03A9').asset==semantic.asset);
 return 0;
}
}
int main(int argc,char**argv){
#if defined(_MSC_VER)
 _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
 _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
 CHECK(argc==3);
 using namespace epochengine;
 struct Ink {std::string_view name; font::FontColor color;};
 const std::array<Ink,13> inks{{INKS}};
 font::FontRenderer renderer;
 std::ofstream rows{argv[2]};CHECK(rows.is_open());
 std::uint32_t sheetWidth{},sheetHeight{};
 std::size_t loaded{},refused{};
 const auto load=[&](std::string name,font::FontColor color,std::string_view kind,
                     font::GlyphCoverage coverage=font::GlyphCoverage::full){
  const bool ready=coverage==font::GlyphCoverage::full
   ?renderer.load_font(name,argv[1],21.0f,color)
   :renderer.load_font(name,argv[1],21.0f,color,coverage);
  const auto* asset=renderer.get_font(name);
  rows<<kind<<'\t'<<name<<'\t'<<ready;
  if(asset){
   if(asset->coverage!=coverage)std::exit(89);
   if(coverage==font::GlyphCoverage::basic_latin){
    if(asset->glyphs.size()!=95||!asset->glyphs.contains(U'A')||asset->glyphs.contains(U'\u03A9')
       ||asset->glyphs.contains(U'\u0416')||asset->glyphs.contains(U'\u00E9'))std::exit(90);
   }else if(!asset->glyphs.contains(U'\u03A9')||!asset->glyphs.contains(U'\u0416')||!asset->glyphs.contains(U'\u00E9'))std::exit(91);
   ++loaded;
   const TextureAtlas* atlas{};
   for(const auto& [_,registration]:atlasmanager::registrars)
    if(registration->atlas.index==asset->atlas_index)atlas=&registration->atlas;
   if(!atlas)std::exit(86);
   const auto entry=std::find_if(atlas->entries.begin(),atlas->entries.end(),[&](const auto& e){return e.name==name;});
   if(entry==atlas->entries.end())std::exit(87);
   sheetWidth=entry->texWidth;sheetHeight=entry->texHeight;
   std::uint64_t pixelHash=1469598103934665603ull,coverage{};
   for(std::size_t pixel=0;pixel<entry->pixels.size();++pixel){
    pixelHash^=entry->pixels[pixel];pixelHash*=1099511628211ull;
    if(pixel%4==3&&entry->pixels[pixel])++coverage;
   }
   if(!coverage||asset->glyphs.empty()||!(asset->metrics.lineHeight>0))std::exit(88);
   rows<<'\t'<<entry->texWidth<<'\t'<<entry->texHeight<<'\t'<<asset->atlas_index
       <<'\t'<<entry->region.x<<'\t'<<entry->region.y<<'\t'<<asset->glyphs.size()
       <<'\t'<<asset->metrics.lineHeight<<'\t'<<asset->metrics.spaceAdvance
       <<'\t'<<pixelHash<<'\t'<<coverage;
  }else{++refused;rows<<"\t0\t0\t-1\t0\t0\t0\t0\t0\t0\t0";}
  rows<<'\n';rows.flush();return ready;
 };
 for(const auto& ink:inks)CHECK(load(std::string{ink.name},ink.color,"base"));
 for(const auto& palette:gui::theme_detail::palettes){
  const auto color=gui::theme_detail::section_heading_color(palette);
  (void)load(std::string{palette.atlasPrefix}+"/heading_font",{color.r,color.g,color.b,255},"heading",font::GlyphCoverage::basic_latin);
 }
 CHECK(atlasmanager::registrars.size()<=font::kFontAtlasMaximumPages);
 CHECK(loaded+refused==26&&sheetWidth>0&&sheetHeight>0);
 CHECK(!renderer.load_font("invalid-coverage",argv[1],21.0f,{},static_cast<font::GlyphCoverage>(255)));
 CHECK(!renderer.get_font("invalid-coverage"));
 CHECK(!renderer.load_font("font",argv[1],21.0f,{},font::GlyphCoverage::basic_latin));
 CHECK(renderer.get_font("font")->coverage==font::GlyphCoverage::full&&renderer.get_font("font")->glyphs.size()==1151);
 for(const auto& [_,registration]:atlasmanager::registrars)
  CHECK(registration->atlas.width==font::kFontAtlasPageExtent&&registration->atlas.height==font::kFontAtlasPageExtent);
 const int lookup=gui::capacity_probe::verify(renderer);if(lookup)return lookup;
 rows.close();CHECK(rows.good());return 0;
}
'''
        replacements = {
            "STB_HEADER": (engine / "src/stb/stb_truetype.h").as_posix(),
            "PALETTE": (engine / "src/epochgui/gui.theme_palette_spec.hpp").as_posix(),
            "TEXTURE": namespace_body(texture_module, "export namespace epochengine"),
            "HANDLE": namespace_body(handle_module, "export namespace epochengine"),
            "ATLAS": namespace_body(atlas_module, "export namespace epochengine"),
            "FONT_DECLARATIONS": namespace_body(font_module, "namespace epochengine::font"),
            "FONT_IMPLEMENTATION": renderer[renderer.index("namespace epochengine::font"):],
            "GUI_CACHE": definition(gui, "        struct GuiFontCache", True),
            "LOOKUP_FUNCTIONS": lookup_functions,
            "INKS": ink_rows,
        }
        code = re.sub(r"\b(" + "|".join(map(re.escape, replacements)) + r")\b",
                      lambda match: replacements[match.group(1)], code)
        report = ROOT / "Artifacts/Reports/heading-font-capacity-oct29" / str(time.time_ns())
        report.mkdir(parents=True)
        cpp = report / "actual_owners.cpp"
        cpp.write_text(code, encoding="utf-8")
        tool = sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*"))[-1]
        sdk = Path("C:/Program Files (x86)/Windows Kits/10")
        kit = sorted((sdk / "Include").glob("*"))[-1]
        binary = report / "capacity.exe"
        command = [str(tool / "bin/Hostx64/x64/cl.exe"), "/nologo", "/std:c++latest", "/O2", "/MD", "/EHsc", "/utf-8",
                   "/I" + str(tool / "include"), "/I" + str(kit / "ucrt"), "/Fo" + str(report / "capacity.obj"),
                   "/Fe" + str(binary), str(cpp), "/link", "/LIBPATH:" + str(tool / "lib/x64"),
                   "/LIBPATH:" + str(sdk / "Lib" / kit.name / "ucrt/x64"), "/LIBPATH:" + str(sdk / "Lib" / kit.name / "um/x64")]
        started = time.monotonic()
        font = engine / "assets/fonts/Roboto-Regular.ttf"
        configurations = []
        for configuration in ("Debug", "Release"):
            current_binary = report / (configuration + ".exe")
            current_command = [arg.replace(str(binary), str(current_binary))
                               .replace(str(report / "capacity.obj"), str(report / (configuration + ".obj")))
                               for arg in command]
            if configuration == "Debug":
                current_command = [{"/O2": "/Od", "/MD": "/MDd"}.get(arg, arg) for arg in current_command]
                current_command.insert(1, "/D_DEBUG")
            build = subprocess.run(current_command, cwd=report, capture_output=True, text=True, timeout=90)
            (report / (configuration + "-build.log")).write_text(build.stdout + build.stderr, encoding="utf-8")
            self.assertEqual(build.returncode, 0, build.stdout[-6000:] + build.stderr)
            run = subprocess.run([str(current_binary), str(font), str(report / (configuration + "-sheets.tsv"))], cwd=report,
                                 capture_output=True, text=True, timeout=60)
            (report / (configuration + "-run.log")).write_text(run.stdout + run.stderr, encoding="utf-8")
            self.assertEqual(run.returncode, 0, f"capacity owner exit{run.returncode}: {report}\n{run.stdout}{run.stderr}")
            configurations.append({"configuration": configuration, "build_exit": build.returncode, "run_exit": run.returncode})
        self.assertEqual((report / "Debug-sheets.tsv").read_bytes(), (report / "Release-sheets.tsv").read_bytes())
        rows = []
        for line in (report / "Release-sheets.tsv").read_text(encoding="utf-8").splitlines():
            fields = line.split("\t")
            rows.append(dict(zip(("kind", "name", "ready", "width", "height", "page", "x", "y", "glyphs", "line_height", "space_advance", "pixel_fnv64", "alpha_coverage"), fields)))
        self.assertEqual(len(rows), 26)
        paths = [renderer_path, engine / "modules/font.renderer.ixx", engine / "modules/atlas.texture.ixx",
                 engine / "modules/texture.core.ixx", engine / "modules/sprite.handle.ixx",
                 engine / "src/stb/stb_truetype.h", engine / "src/epochgui/gui.theme_palette_spec.hpp",
                 engine / "src/epochgui/gui.engine.cpp", font, Path(__file__)]
        verdict = {"schema": "epoch.real-heading-font-capacity.v1", "build_exit": build.returncode, "run_exit": run.returncode,
                   "configurations": configurations, "wall_seconds": time.monotonic() - started, "page_cap": 4,
                   "page_extent": 2048, "gpu_or_window": False, "sheets": rows,
                   "all_26_fit": all(row["ready"] == "1" for row in rows),
                   "pages_used": len({row["page"] for row in rows if row["ready"] == "1"}),
                   "ports": "Only logging, registrar storage and upload recording; actual FontRenderer bodies, STB, pixels/metrics and TextureAtlas allocator.",
                   "actual_gui_lookup_and_kerning": "All13 headings: ASCII scoped glyph; Ω/Ж/é exact normal glyph in light/dark, mixed nonASCII left/right kerning from full font; semantic ink retains full coverage.",
                   "source_sha256": {str(path.relative_to(ROOT)).replace("\\", "/"): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}}
        (report / "result.json").write_text(json.dumps(verdict, indent=2) + "\n", encoding="utf-8")
        self.assertEqual(tuple(int(row["pixel_fnv64"]) for row in rows if row["kind"] == "base"), FULL_PIXEL_GOLDEN)
        self.assertTrue(all(row["width"] == "1021" and row["height"] == "637" and row["glyphs"] == "1151"
                            for row in rows if row["kind"] == "base"))
        self.assertTrue(verdict["all_26_fit"], f"Bounded real heading fonts refused: {report}")
        print(f"Real font capacity: all26fit={verdict['all_26_fit']} pages={verdict['pages_used']} receipt={report / 'result.json'}")


if __name__ == "__main__":
    unittest.main()
