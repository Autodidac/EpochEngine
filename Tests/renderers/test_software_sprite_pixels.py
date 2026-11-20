"""Actual Software sprite rasterization with recording atlas and framebuffer inputs."""
import hashlib
from pathlib import Path
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run


ROOT = Path(__file__).resolve().parents[2]


class SoftwareSpritePixels(unittest.TestCase):
    def test_gui_pixel_extents_preserve_clip_alpha_and_legacy_normalized_draws(self):
        source = (ROOT / 'Engine/src/renderers/software/software.context_draw.cpp').read_text(encoding='utf-8')
        detail = definition(source, '    namespace detail')
        draw = '\n'.join(definition(source, marker) for marker in (
            '    static void draw_sprite_impl(', '    void draw_sprite(', '    void draw_sprite_pixels('))
        fixture = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>
struct SpriteHandle {int atlasIndex{},localIndex{};bool is_valid()const{return true;}};
struct AtlasRegion {std::uint32_t x{},y{},width{},height{};};
struct TextureAtlas {
 std::uint32_t width{},height{};std::vector<std::uint8_t> pixel_data;
 bool try_get_entry_info(int,AtlasRegion& region)const{region={0,0,width,height};return true;}
 void rebuild_pixels(){}
};
struct {int width{8},height{6};std::vector<std::uint32_t> framebuffer=std::vector<std::uint32_t>(48);} s_softrendererstate;
DETAIL
DRAW
#define CHECK(value) do {if(!(value)){std::fprintf(stderr,"Software sprite pixels failed line %d\n",__LINE__);return 1;}}while(false)
int main(){
 auto& pixels=s_softrendererstate.framebuffer;
 const auto reset=[&]{std::ranges::fill(pixels,0xFF000000u);};
 const auto at=[&](std::size_t x,std::size_t y){return pixels.at(y*8+x);};
 TextureAtlas uniform{1,1,{18,52,86,255}};
 const std::array<const TextureAtlas*,1> solid{&uniform};
 reset();draw_sprite_pixels({},solid,0,0,1,1);
 CHECK(at(0,0)==0xFF123456u&&at(1,0)==0xFF000000u&&at(0,1)==0xFF000000u);
 reset();draw_sprite({},solid,0,0,1,1);
 CHECK(std::ranges::all_of(pixels,[](auto value){return value==0xFF123456u;}));
 reset();draw_sprite_pixels({},solid,2,2,4,1);
 CHECK(at(2,2)==0xFF123456u&&at(5,2)==0xFF123456u&&at(6,2)==0xFF000000u&&at(2,3)==0xFF000000u);
 TextureAtlas varied{3,1,{18,52,86,255,255,0,0,0,0,255,0,128}};
 const std::array<const TextureAtlas*,1> mixed{&varied};
 reset();draw_sprite_pixels({},mixed,2,3,3,1);
 CHECK(at(2,3)==0xFF123456u&&at(3,3)==0xFF000000u&&at(4,3)==0xFF008000u);
 reset();draw_sprite_pixels({},mixed,-1,3,3,1);
 CHECK(at(0,3)==0xFF000000u&&at(1,3)==0xFF008000u&&at(2,3)==0xFF000000u&&at(1,4)==0xFF000000u);
}
'''.replace('DETAIL', detail).replace('DRAW', draw)
        report = ROOT / 'Artifacts/Reports/software-sprite-pixels' / hashlib.sha256(fixture.encode()).hexdigest()[:12]
        report.mkdir(parents=True, exist_ok=True)
        consumer = report / 'consumer.cpp'
        consumer.write_text(fixture, encoding='utf-8')
        compile_and_run(self, consumer, report)


if __name__ == '__main__':
    unittest.main()
