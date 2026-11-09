"""Actual cache codec round trips and short reads; no modules/graphics/models."""
from pathlib import Path
import hashlib
import json
import shutil
import time
import unittest
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / "Engine/dep/EpochEngineExtensions/packages/planetary_terrain"
CODE = r'''
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#include "epoch/planet/terrain.hpp"
#include "planet.stream_cache.hpp"
namespace fs=std::filesystem;
namespace cache=planet_stream_cache;
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
std::string read(const fs::path& path){std::ifstream input{path,std::ios::binary};return {std::istreambuf_iterator<char>{input},{}};}
int main(int argc,char**argv){
 CHECK(argc==3);const fs::path root{argv[1]},output{argv[2]};fs::create_directories(output);
 for(std::size_t bytes=0;bytes<8;++bytes){std::istringstream input{std::string(bytes,'x')};std::uint64_t value{};CHECK(!cache::read_scalar(input,value));
  std::vector<std::byte> shortBytes(bytes);std::span<const std::byte> cursor{shortBytes};CHECK(!cache::read_scalar(cursor,value)&&cursor.size()==bytes);}
 for(const std::uint64_t bits:{0ull,1ull,0x7ff8000000000001ull,0xffffffffffffffffull}){
  std::ostringstream encoded;CHECK(cache::write_scalar(encoded,bits));std::istringstream input{encoded.str()};std::uint64_t value{};
  CHECK(cache::read_scalar(input,value)&&value==bits&&input.peek()==std::char_traits<char>::eof());
  std::istringstream floating{encoded.str()};double real{};CHECK(cache::read_scalar(floating,real)&&std::bit_cast<std::uint64_t>(real)==bits);
  const auto raw=encoded.str();std::span<const std::byte> cursor{reinterpret_cast<const std::byte*>(raw.data()),raw.size()};
  CHECK(cache::read_scalar(cursor,value)&&value==bits&&cursor.empty());
  cursor={reinterpret_cast<const std::byte*>(raw.data()),raw.size()};CHECK(cache::read_scalar(cursor,real)&&std::bit_cast<std::uint64_t>(real)==bits&&cursor.empty());
 }
 for(std::uint32_t bits:{0u,1u,0x7fc00001u,0xffffffffu}){
  std::ostringstream encoded;CHECK(cache::write_scalar(encoded,bits));const auto raw=encoded.str();
  std::span<const std::byte> cursor{reinterpret_cast<const std::byte*>(raw.data()),raw.size()};float real{};
  CHECK(cache::read_scalar(cursor,real)&&std::bit_cast<std::uint32_t>(real)==bits&&cursor.empty());
 }
 unsigned sampled{};
 for(const auto& entry:fs::directory_iterator(root)){
  epoch::planet::TileKey key;if(!cache::parse_filename(entry.path().filename().string(),key))continue;
  if(sampled++%64u!=0u)continue;
  epoch::planet::Mesh mesh;CHECK(cache::read_patch(entry.path(),key,16,mesh,true));
  const auto copy=output/entry.path().filename();CHECK(cache::write_patch(copy,key,mesh)&&read(copy)==read(entry.path()));
  epoch::planet::Mesh restored;CHECK(cache::read_patch(copy,key,16,restored,true));
  auto wrong=key;wrong.face=(wrong.face+1u)%6u;CHECK(!cache::read_patch(copy,wrong,16,restored,true));
  CHECK(!cache::read_patch(copy,key,1,restored,true)&&!cache::read_patch(copy,key,65,restored,true));
  const auto bytes=read(copy);
  for(const auto length:{std::size_t{0},std::size_t{6},std::size_t{26},std::size_t{114},bytes.size()-1}){
   {std::ofstream truncated{copy,std::ios::binary|std::ios::trunc};truncated.write(bytes.data(),length);}
   CHECK(!cache::read_patch(copy,key,16,restored,true));}
  for(auto corrupt:std::vector<std::string>{bytes+"x",std::string{"X"}+bytes.substr(1),bytes.substr(0,107)+std::string(4,'\0')+bytes.substr(111)}){
   {std::ofstream changed{copy,std::ios::binary|std::ios::trunc};changed.write(corrupt.data(),corrupt.size());}
   CHECK(!cache::read_patch(copy,key,16,restored,true));}
 }
 CHECK(sampled>128u);
 // The largest admitted cache payload remains bounded, with mesh plus input
 // bytes inside the provider's existing conservative retained-patch allowance.
 for(std::uint64_t cells=2;cells<=64;++cells)for(std::uint64_t layers:{1u,2u}){
  const auto side=cells+1,vertices=side*side+4*cells,indices=6*cells*cells+24*cells;
  const auto decoded=layers*(vertices*52+indices*4),payload=115+decoded;
  const auto retained=layers*(2*(side*side+512)*52+2*(6*cells*cells+1536)*4)+sizeof(epoch::planet::Mesh);
  CHECK(payload<=675035u&&decoded+payload<=retained);
 }
 {epoch::planet::Mesh largest;largest.vertices.resize(2u*(65u*65u+4u*64u));largest.indices.resize(2u*(6u*64u*64u+24u*64u));
  epoch::planet::TileKey key{};const auto path=output/"largest.epoch_patch";
  CHECK(cache::write_patch(path,key,largest)&&fs::file_size(path)==675035u);
  epoch::planet::Mesh restored;CHECK(cache::read_patch(path,key,64,restored,true));
  CHECK(restored.vertices.size()==largest.vertices.size()&&restored.indices.size()==largest.indices.size());}
 return 0;
}
'''


class PlanetaryCacheCodec(unittest.TestCase):
    def test_bounded_snapshot_readback(self):
        code = r'''#include "planet.stream_cache.hpp"
#include <sstream>
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
 for(const std::size_t size:{0u,1u,65535u,65536u,65537u,196621u}){
  std::string bytes(size,'x');for(std::size_t i=0;i<size;++i)bytes[i]=static_cast<char>(i*37u);
  const std::span<const std::byte> expected{reinterpret_cast<const std::byte*>(bytes.data()),bytes.size()};
  std::istringstream exact{bytes};CHECK(planet_stream_cache::matches_bytes(exact,expected));
  std::istringstream extra{bytes+"x"};CHECK(!planet_stream_cache::matches_bytes(extra,expected));
  if(size){
   std::istringstream short_read{bytes.substr(0,size-1)};CHECK(!planet_stream_cache::matches_bytes(short_read,expected));
   for(const auto offset:{std::size_t{0},size/2,size-1}){
    auto changed=bytes;changed[offset]^=1;std::istringstream corrupt{changed};
    CHECK(!planet_stream_cache::matches_bytes(corrupt,expected));
   }
  }
  for(const auto flags:{std::ios::badbit,std::ios::failbit,std::ios::eofbit}){
   std::istringstream failed{bytes};failed.setstate(flags);
   CHECK(!planet_stream_cache::matches_bytes(failed,expected));
  }
 }
 return 0;
}'''
        identity=hashlib.sha256((PACKAGE/'tools/planet.stream_cache.hpp').read_bytes()+code.encode()).hexdigest()[:12]
        report=ROOT/'Artifacts/Reports/planet-throughput-nov02/readback'/identity
        report.mkdir(parents=True,exist_ok=True)
        source=report/'readback.cpp';source.write_text(code,encoding='utf-8')
        runs=compile_and_run(self,source,report,includes=[PACKAGE/'tools'])
        (report/'result.json').write_text(json.dumps({'pass':True,'runs':runs,
            'scope':'Actual producer readback; exact/short/extra/corrupt/failing streams across64KiB boundaries; no GPU/model.'},indent=2)+'\n')

    def test_real_cache_bytes_and_short_reads(self):
        caches = list((ROOT/"Projects/GameProject01/cache/planet").glob("*/**/1337-16-0-ocean"))
        caches = [p for p in caches if len(list(p.glob("*.epoch_patch"))) > 128]
        if not caches:
            self.skipTest("Retained real 16-cell planetary cache required")
        cache = max(caches, key=lambda p: p.stat().st_mtime)
        frozen = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in cache.iterdir() if p.is_file()}
        identity = hashlib.sha256((PACKAGE/"tools/planet.stream_cache.hpp").read_bytes()+CODE.encode()).hexdigest()[:12]
        report = ROOT/"Artifacts/Reports/planet-lod-latency-nov01/cache-codec"/identity
        report.mkdir(parents=True, exist_ok=True)
        source = report/"cache.cpp";source.write_text(CODE, encoding="utf-8")
        # This small header consumer has no application manifest. Preserve the
        # exact retained bytes under a short checkout-local input path.
        data=report/str(time.time_ns());data.mkdir()
        copied=data/"input";shutil.copytree(cache,copied)
        runs = compile_and_run(self, source, report,
            args=lambda config: [copied, data/config],
            includes=[PACKAGE/"tools", PACKAGE/"upstream/EpochPlanet-0.2.3/include"], run_timeout=30)
        unchanged = all(hashlib.sha256((cache/name).read_bytes()).hexdigest() == digest for name, digest in frozen.items())
        self.assertTrue(unchanged)
        (report/"result.json").write_text(json.dumps({"passed": True, "runs": runs,
            "cache_unchanged": unchanged, "scope": "Actual header codec, real retained patch byte roundtrip, wrong identity/truncation/short scalar refusal. No graphics/models."}, indent=2)+"\n")


if __name__ == "__main__":
    unittest.main()
