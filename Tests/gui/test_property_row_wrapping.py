"""Execute production property-row geometry with recording font/draw ports."""
from pathlib import Path
import hashlib
import json
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class PropertyRowWrapping(unittest.TestCase):
    def test_opt_in_labels_reserve_height_in_both_layouts(self):
        source = (ROOT / 'Engine/src/epochgui/controls/gui.text_controls.inl').read_text(encoding='utf-8')
        owner = definition(source, '    void property_row(')
        code = r'''
#include <algorithm>
#include <cstdlib>
#define CHECK(...) do { if (!(__VA_ARGS__)) std::exit(EXIT_FAILURE); } while(false)
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
struct Vec2 { float x{}, y{}; };
struct Frame { bool insideWindow{true}; void* ctx{this}; Vec2 cursor{}; } g_frame;
constexpr float kFontScale=1;
float available=440;
float content_available_width(float) { return available; }
float space_advance(float) { return 8; }
float line_advance_amount(float) { return 18; }
struct Draw { std::string text; float x,y,width,height; bool wrapped; };
std::vector<Draw> draws;
float draw_wrapped_text(std::string_view text,float x,float y,float width,float) {
 const float height=18*std::max(1.0f,std::ceil(float(text.size())*8/std::max(8.0f,width)));
 draws.push_back({std::string(text),x,y,width,height,true}); return height;
}
std::string fit_text_to_width(std::string_view text,float width,float) {
 return std::string(text.substr(0,std::size_t(std::max(0.0f,width)/8)));
}
void draw_text_line(std::string_view text,float x,float y,float) {
 draws.push_back({std::string(text),x,y,0,18,false});
}
void advance_cursor(Vec2 delta) { g_frame.cursor.y+=delta.y; }
OWNER
int main() {
 const std::string label="1  Draft & review source change";
 for(float width : {800.0f,440.0f,320.0f,160.0f,80.0f}) {
  available=width;g_frame.cursor={};draws.clear();
  property_row(label,"Awaiting proposal review",width*.55f,true);
  if(width==800.0f) CHECK(draws[0].height==18);
  if(width==320.0f) CHECK(draws[0].height>18);
  CHECK(draws.size()==2 && draws[0].text==label && draws[0].wrapped);
  const float end=std::max(draws[0].y+draws[0].height,draws[1].y+draws[1].height);
  CHECK(g_frame.cursor.y>=end);
  if(draws[1].x==0) CHECK(draws[1].y>=draws[0].height);
  else CHECK(draws[0].x+draws[0].width<=draws[1].x);
  property_row("2  Debug build","Pending",200,true);
  CHECK(draws[2].y>=end && draws[3].y>=end);
  g_frame.cursor={};draws.clear();property_row(label,"",200,true);
  CHECK(draws.size()==1 && g_frame.cursor.y>=draws[0].height);
  g_frame.cursor={};draws.clear();property_row(label,"Pending",200,false);
  CHECK(!draws[0].wrapped && draws[0].height==18);
 }
}
'''.replace('OWNER', owner)
        report = ROOT / 'Artifacts/Reports/property-row-wrapping-nov16' / hashlib.sha256(code.encode()).hexdigest()[:16]
        report.mkdir(parents=True, exist_ok=True)
        cpp = report / 'row.cpp'
        cpp.write_text(code, encoding='utf-8')
        results = compile_and_run(self, cpp, report)
        (report / 'result.json').write_text(json.dumps({'passed': True, 'results': results,
            'scope': 'Actual property_row layout, recording fixed-width font ports; no native window/model.'}, indent=2), encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
