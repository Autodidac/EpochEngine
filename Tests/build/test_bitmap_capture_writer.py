"""Actual shared BMP writer: existing RGBA callers and native BGRA capture."""
from pathlib import Path
import hashlib
import json
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class BitmapCaptureWriter(unittest.TestCase):
    def test_shared_writer_preserves_pixels_orientation_and_failure(self):
        owner = ROOT / 'Engine/modules/image/image.writer.ixx'
        source = owner.read_text(encoding='utf-8')
        body = definition(source, '    inline bool a_writeBMP(')
        channels = definition(source, '    enum class BitmapSourceChannels', True)
        code = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>
CHANNELS
BODY
std::vector<unsigned char> read(const std::filesystem::path& p){
 std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>{f},{}};}
unsigned u32(const std::vector<unsigned char>& b,unsigned p){
 return unsigned(b[p])|(unsigned(b[p+1])<<8)|(unsigned(b[p+2])<<16)|(unsigned(b[p+3])<<24);}
int main(){
 const std::vector<unsigned char> rgba{255,0,0,11,0,255,0,22,0,0,255,33,255,255,255,44};
 const std::vector<unsigned char> bgra{0,0,255,11,0,255,0,22,255,0,0,33,255,255,255,44};
 assert(a_writeBMP("rgba.bmp",rgba,2,2,false));
 assert(a_writeBMP("bgra.bmp",bgra,2,2,false,BitmapSourceChannels::bgra));
 const auto b=read("rgba.bmp");assert(b==read("bgra.bmp"));
 assert(b.size()==70&&b[0]=='B'&&b[1]=='M'&&u32(b,2)==70&&u32(b,10)==54);
 assert(u32(b,18)==2&&u32(b,22)==2&&b[28]==24);
 const std::vector<unsigned char> expected{255,0,0,255,255,255,0,0,0,0,255,0,255,0,0,0};
 assert(std::vector<unsigned char>(b.begin()+54,b.end())==expected);
 assert(a_writeBMP("flipped.bmp",rgba,2,2,true));const auto flipped=read("flipped.bmp");
 assert(flipped[54]==0&&flipped[56]==255&&flipped[62]==255&&flipped[64]==0);
 assert(a_writeBMP("padded.bmp",std::vector<unsigned char>(12,127),3,1,false));
 const auto padded=read("padded.bmp");assert(padded.size()==66&&padded[63]==0&&padded[65]==0);
 assert(!a_writeBMP("bad.bmp",{},1,1,false)&&!std::filesystem::exists("bad.bmp"));
 assert(!a_writeBMP("bad.bmp",rgba,2147483647,2147483647,false));
 assert(!a_writeBMP("bad.bmp",rgba,2,2,false,static_cast<BitmapSourceChannels>(255)));
 assert(!a_writeBMP("missing-folder/out.bmp",rgba,2,2,false));
 bool threw=false;try{(void)a_writeBMP("bad.bmp",rgba,0,2,false);}catch(const std::runtime_error&){threw=true;}assert(threw);
}
'''.replace('CHANNELS', channels).replace('BODY', body)
        report = ROOT / 'Artifacts/Reports/bitmap-capture-consolidation-nov02' / hashlib.sha256(code.encode()).hexdigest()[:12]
        report.mkdir(parents=True, exist_ok=True)
        fixture = report / 'contract.cpp'
        fixture.write_text(code, encoding='utf-8')
        phases = compile_and_run(self, fixture, report)
        (report / 'result.json').write_text(json.dumps(dict(
            source_sha256=hashlib.sha256(owner.read_bytes()).hexdigest(),
            phases=phases, graphical_launch=False), indent=2), encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
