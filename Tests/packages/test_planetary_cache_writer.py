"""Actual private cache writer: byte compatibility, bounded writes and refusal."""
from pathlib import Path
import hashlib
import json
import os
import time
import unittest
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / "Engine/dep/EpochEngineExtensions/packages/planetary_terrain"
CODE = r'''
#include "epoch/planet/terrain.hpp"
#include "planet.stream_cache.hpp"
#ifdef EPOCH_CONTROL
#define planet_stream_cache control_cache
#include "planet.stream_cache_control.hpp"
#undef planet_stream_cache
#endif
#include <chrono>
#include <iterator>
#include <limits>
#include <sstream>
namespace fs=std::filesystem;
namespace cache=planet_stream_cache;
#define CHECK(...) do{if(!(__VA_ARGS__))return __LINE__;}while(false)
struct Sink:std::streambuf{
 std::string bytes;std::size_t remaining=64,blocks{},singles{};
 std::streamsize xsputn(const char* data,std::streamsize n)override{
  ++blocks;const auto size=(std::min)(remaining,std::size_t(n));bytes.append(data,size);remaining-=size;return std::streamsize(size);}
 int_type overflow(int_type c)override{++singles;if(!remaining)return traits_type::eof();--remaining;bytes.push_back(traits_type::to_char_type(c));return c;}
};
template<class T>std::string expected(T value){
 if constexpr(std::is_floating_point_v<T>){
  if constexpr(sizeof(T)==4)return expected(std::bit_cast<std::uint32_t>(value));
  else return expected(std::bit_cast<std::uint64_t>(value));
 }else{std::string bytes;for(std::size_t i=0;i<sizeof(T);++i)bytes.push_back(char((value>>(8*i))&255u));return bytes;}
}
template<class T>int scalar(T value,bool bounded){
 const auto bytes=expected(value);Sink sink;std::ostream output{&sink};
 CHECK(cache::write_scalar(output,value)&&sink.bytes==bytes);
 if(bounded)CHECK(sink.blocks==1&&sink.singles==0);
 std::istringstream input{sink.bytes};T restored{};CHECK(cache::read_scalar(input,restored)&&expected(restored)==bytes);
 std::span<const std::byte> payload{reinterpret_cast<const std::byte*>(bytes.data()),bytes.size()};
 CHECK(cache::read_scalar(payload,restored)&&expected(restored)==bytes&&payload.empty());
 for(std::size_t count=0;count<sizeof(T);++count){
  Sink short_sink;short_sink.remaining=count;std::ostream short_output{&short_sink};
  CHECK(!cache::write_scalar(short_output,value)&&short_output.bad()&&short_sink.bytes==bytes.substr(0,count));
 }
 for(const auto state:{std::ios::badbit,std::ios::failbit,std::ios::eofbit}){
  Sink failed;std::ostream out{&failed};out.setstate(state);CHECK(!cache::write_scalar(out,value)&&failed.bytes.empty());
 }
 Sink failing;failing.remaining=0;std::ostream throws{&failing};throws.exceptions(std::ios::badbit);
 bool caught=false;try{cache::write_scalar(throws,value);}catch(const std::ios_base::failure&){caught=true;}
 CHECK(caught);return 0;
}
std::string read(const fs::path&p){std::ifstream input{p,std::ios::binary};return {std::istreambuf_iterator<char>{input},{}};}
epoch::planet::Mesh mesh(unsigned layers){
 epoch::planet::Mesh result;result.anchor={1.25,-2.5,6371000.125};result.bound_radius=123.5;
 result.corners={epoch::planet::FVec3{1,2,3},{4,5,6},{7,8,9},{10,11,12}};
 result.vertices.resize(layers*(17u*17u+4u*16u));result.indices.resize(layers*(6u*16u*16u+24u*16u));
 for(std::size_t i=0;i<result.vertices.size();++i){auto&v=result.vertices[i];
  v.position={float(i)*.25f,float(i)*-.125f,float(i%17)};v.normal={0,1,0};
  v.color={.125f,.25f,.5f,float(i%2)};v.parent_delta={.01f,0,-.02f};}
 for(std::size_t i=0;i<result.indices.size();++i)result.indices[i]=std::uint32_t(i%result.vertices.size());
 return result;
}
int main(int argc,char**argv){
 CHECK(argc==3);const bool bounded=std::string_view(argv[1])=="bounded";const fs::path root{argv[2]};fs::create_directories(root);
 for(auto v:{std::uint8_t{0},std::uint8_t{1},std::uint8_t{255}})CHECK(!scalar(v,bounded));
 for(auto v:{std::uint16_t{0},std::uint16_t{0x1234},std::uint16_t{65535}})CHECK(!scalar(v,bounded));
 for(std::uint32_t v:{0u,1u,0x12345678u,0xffffffffu})CHECK(!scalar(v,bounded));
 for(std::uint64_t v:{0ull,1ull,0x123456789abcdef0ull,0xffffffffffffffffull})CHECK(!scalar(v,bounded));
 for(std::uint32_t bits:{0u,0x80000000u,1u,0x007fffffu,0x7f7fffffu,0x7f800000u,0xff800000u,0x7fc01234u,0xffffffffu})CHECK(!scalar(std::bit_cast<float>(bits),bounded));
 for(std::uint64_t bits:{0ull,0x8000000000000000ull,1ull,0xfffffffffffffull,0x7fefffffffffffffull,0x7ff0000000000000ull,0xfff0000000000000ull,0x7ff8000012345678ull,0xffffffffffffffffull})CHECK(!scalar(std::bit_cast<double>(bits),bounded));
 std::ofstream metrics{root/"metrics.json"};metrics<<"{\"patches\":108,\"cells\":16,\"ocean_layers\":2";
 for(unsigned layers:{1u,2u}){
  const auto original=mesh(layers);epoch::planet::TileKey key{1,2,0,3,4};const auto path=root/(std::to_string(layers)+".epoch_patch");
  CHECK(cache::write_patch(path,key,original));CHECK(!cache::write_patch(path,key,original));
  epoch::planet::Mesh restored;CHECK(cache::read_patch(path,key,16,restored,layers==2));
  const auto roundtrip=root/(std::to_string(layers)+"-roundtrip.epoch_patch");CHECK(cache::write_patch(roundtrip,key,restored)&&read(path)==read(roundtrip));
  auto wrong=key;++wrong.x;CHECK(!cache::read_patch(path,wrong,16,restored,layers==2));
  CHECK(!cache::read_patch(path,key,1,restored,true)&&!cache::read_patch(path,key,65,restored,true));
  if(layers==2)CHECK(!cache::read_patch(path,key,16,restored,false));
#ifdef EPOCH_CONTROL
  const auto control=root/(std::to_string(layers)+"-control.epoch_patch");CHECK(control_cache::write_patch(control,key,original)&&read(control)==read(path));
#endif
  const auto bytes=read(path);for(const auto size:{std::size_t{0},std::size_t{6},std::size_t{114},bytes.size()-1}){
   const auto broken=root/"short.epoch_patch";{std::ofstream out{broken,std::ios::binary|std::ios::trunc};out.write(bytes.data(),size);}CHECK(!cache::read_patch(broken,key,16,restored,true));}
  for(const auto&corrupt:{bytes+"x",std::string{"X"}+bytes.substr(1),bytes.substr(0,107)+std::string(4,'\0')+bytes.substr(111)}){
   const auto broken=root/"corrupt.epoch_patch";{std::ofstream out{broken,std::ios::binary|std::ios::trunc};out.write(corrupt.data(),corrupt.size());}CHECK(!cache::read_patch(broken,key,16,restored,true));}
 }
 const auto source=mesh(2);
 auto timed=[&](bool control){const auto start=std::chrono::steady_clock::now();std::uint64_t bytes{};
  for(unsigned i=0;i<108;++i){epoch::planet::TileKey key{i%16,i/16,0,0,4};const auto path=root/(std::string(control?"old-":"new-")+std::to_string(i)+".epoch_patch");
#ifdef EPOCH_CONTROL
   if(control){if(!control_cache::write_patch(path,key,source))return -1.;}
   else
#endif
   if(!cache::write_patch(path,key,source))return -1.;
   bytes+=fs::file_size(path);
  }
  metrics<<",\""<<(control?"control":"current")<<"_bytes\":"<<bytes;
  return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};
#ifdef EPOCH_CONTROL
 const auto old=timed(true);CHECK(old>=0);metrics<<",\"control_ms\":"<<old;
#endif
 const auto current=timed(false);CHECK(current>=0);metrics<<",\"current_ms\":"<<current<<"}\n";
#ifdef EPOCH_CONTROL
 for(unsigned i=0;i<108;++i)CHECK(read(root/("old-"+std::to_string(i)+".epoch_patch"))==read(root/("new-"+std::to_string(i)+".epoch_patch")));
#endif
 return 0;
}
'''


class PlanetaryCacheWriter(unittest.TestCase):
    def test_actual_writer_bounded_and_compatible(self):
        identity=hashlib.sha256((PACKAGE/'tools/planet.stream_cache.hpp').read_bytes()+CODE.encode()).hexdigest()[:12]
        report=ROOT/'Artifacts/Reports/planet-cache-writer80-nov07'/identity
        report.mkdir(parents=True,exist_ok=True)
        source=report/'planet.cache_writer_consumer.cpp';source.write_text(CODE,encoding='utf-8')
        control=os.environ.get('EPOCH_CACHE_WRITER_CONTROL')
        includes=[PACKAGE/'tools',PACKAGE/'upstream/EpochPlanet-0.2.3/include']
        if control:
            import shutil
            shutil.copyfile(control,report/'planet.stream_cache_control.hpp')
        mode=os.environ.get('EPOCH_CACHE_WRITER_MODE','bounded')
        data=report/str(time.time_ns())
        runs=compile_and_run(self,source,report,
            args=lambda config:[mode,data/config],includes=includes,
            windows_flags=['/DEPOCH_CONTROL'] if control else [],
            linux_flags=['-DEPOCH_CONTROL'] if control else [],run_timeout=30)
        metrics={run['configuration']:json.loads((data/run['configuration']/'metrics.json').read_text()) for run in runs}
        (report/'result.json').write_text(json.dumps({'pass':True,'runs':runs,'metrics':metrics,
            'scope':'CPU actual-header 108 serial 16-cell ocean patch writes; no provider build, scheduling, GPU or Editor runtime proof.'},indent=2)+'\n')


if __name__=='__main__':unittest.main()
