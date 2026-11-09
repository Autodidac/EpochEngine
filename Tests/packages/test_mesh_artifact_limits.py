"""Actual immutable mesh codec: explicit bounded large assets, unchanged defaults."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/modules/asset.mesh_artifact.ixx"
DEFAULT_V1_SHA256 = "d61f0361b51174e9d6069a87f9997c3098f22327ad95c719eccde1ea79337b74"

CODE = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <span>
#include <vector>
import asset.mesh_artifact;
import asset.texture_artifact;
namespace mesh=epochengine::asset::mesh;namespace fs=std::filesystem;
bool denyAll{},denyLarge{};std::size_t blocked{};
void* operator new(std::size_t count){if(denyAll||(denyLarge&&count>65536)){++blocked;throw std::bad_alloc{};}if(void* p=std::malloc(count?count:1))return p;throw std::bad_alloc{};}
void* operator new[](std::size_t count){return ::operator new(count);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
void check(bool value,int line){if(!value){std::fprintf(stderr,"Mesh limits contract failed line %d\n",line);std::exit(89);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),__LINE__)
template<class Action>auto noAllocation(Action action){blocked=0;denyAll=true;auto result=action();denyAll=false;CHECK(blocked==0);return result;}
void put32(std::vector<std::byte>& bytes,std::size_t at,std::uint32_t value){for(unsigned shift=0;shift<32;shift+=8)bytes[at++]=std::byte((value>>shift)&255);}
void write(const fs::path& path,std::span<const std::byte> bytes){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));CHECK(out.good());}
struct Geometry{std::vector<mesh::Vertex>vertices;std::vector<std::uint32_t>indices;};
Geometry sphere(unsigned cells){
 Geometry result;result.vertices.reserve(6*(cells+1)*(cells+1));result.indices.reserve(36*cells*cells);
 for(unsigned face=0;face<6;++face){const auto base=std::uint32_t(result.vertices.size());
  for(unsigned y=0;y<=cells;++y)for(unsigned x=0;x<=cells;++x){const float s=2.0f*float(x)/float(cells)-1,t=2.0f*float(y)/float(cells)-1;std::array<float,3> p;
   switch(face){case 0:p={1,s,t};break;case 1:p={-1,s,t};break;case 2:p={s,1,t};break;case 3:p={s,-1,t};break;case 4:p={s,t,1};break;default:p={s,t,-1};break;}
   const float inverse=1/std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);mesh::Vertex vertex;
   for(unsigned axis=0;axis<3;++axis){vertex.normal[axis]=p[axis]*inverse;vertex.position[axis]=10*vertex.normal[axis];}
   vertex.uv={float(x)/float(cells),float(y)/float(cells)};vertex.color={0.1f,0.4f,0.65f};result.vertices.push_back(vertex);
  }
  for(unsigned y=0;y<cells;++y)for(unsigned x=0;x<cells;++x){const auto a=base+y*(cells+1)+x,b=a+1,c=a+(cells+1),d=c+1;for(auto index:{a,b,d,a,d,c})result.indices.push_back(index);}
 }return result;
}
int main(int argc,char**argv){
 CHECK(argc==2);const fs::path root=argv[1];fs::create_directories(root);
 std::array<mesh::Vertex,4> small;small[0].position={-1,0,-1};small[1].position={1,0,-1};small[2].position={1,0,1};small[3].position={-1,0,1};
 const std::array<std::uint32_t,6> triangles{0,2,1,0,3,2};const auto first=mesh::build_artifact(small,triangles);CHECK(first);
 const auto golden=mesh::encode_artifact(*first.artifact);CHECK(golden.size()==248);write(root/"default-v1.bin",golden);
 // Legacy input may retain signed zero while its content key and re-encoding
 // use canonical +0. Every zero-valued channel must preserve that contract.
 for(std::size_t channel=0;channel<11;++channel){const auto at=48+channel*4;
  const auto bits=std::bit_cast<std::uint32_t>(channel<3?small[0].position[channel]:channel<6?small[0].normal[channel-3]:channel<8?small[0].uv[channel-6]:small[0].color[channel-8]);
  if(bits!=0u)continue;
  auto legacy=golden;put32(legacy,at,0x80000000u);const auto original=legacy;
  const auto decoded=mesh::decode_artifact(legacy);CHECK(decoded&&decoded.artifact->key()==first.artifact->key());
  CHECK(mesh::encode_artifact(*decoded.artifact)==golden&&legacy==original);
  const auto raw=epochengine::asset::texture::deterministic_content_hash("epoch.mesh.artifact.v1",std::span<const std::byte>{legacy}.subspan(40));
  CHECK(raw!=first.artifact->key());for(unsigned word=0;word<4;++word){put32(legacy,8+word*8,std::uint32_t(raw.words[word]));put32(legacy,12+word*8,std::uint32_t(raw.words[word]>>32));}
  CHECK(mesh::decode_artifact(legacy).code==mesh::Code::content_mismatch);
 }
 for(const auto tiny:{std::numeric_limits<float>::denorm_min(),-std::numeric_limits<float>::denorm_min()}){
  auto denormal=small;denormal[0].uv[0]=tiny;const auto built=mesh::build_artifact(denormal,triangles);CHECK(built);
  auto bytes=mesh::encode_artifact(*built.artifact);const auto original=bytes;const auto decoded=mesh::decode_artifact(bytes);
  CHECK(decoded&&decoded.artifact->key()==built.artifact->key()&&mesh::encode_artifact(*decoded.artifact)==original&&bytes==original);
 }
 CHECK(mesh::Limits{}.maximum_vertices==65536&&mesh::Limits{}.maximum_indices==196608&&mesh::Limits{}.maximum_bytes==4ull*1024*1024);
 const mesh::Limits large{300000,1600000,32ull*1024*1024};
 for(unsigned cells:{150u,210u}){auto geometry=sphere(cells);CHECK(geometry.vertices.size()>65536&&geometry.indices.size()>196608);
  CHECK(!noAllocation([&]{return mesh::build_artifact(geometry.vertices,geometry.indices);}));
  for(unsigned cycle=0;cycle<2;++cycle){const auto built=mesh::build_artifact(geometry.vertices,geometry.indices,large);CHECK(built);
   const auto bytes=mesh::encode_artifact(*built.artifact);CHECK(bytes.size()==48+geometry.vertices.size()*44+geometry.indices.size()*4&&bytes.size()<large.maximum_bytes);
   CHECK(!noAllocation([&]{return mesh::decode_artifact(bytes);}));const auto decoded=mesh::decode_artifact(bytes,large);CHECK(decoded&&decoded.artifact->key()==built.artifact->key()&&mesh::encode_artifact(*decoded.artifact)==bytes);
   CHECK(decoded.artifact->vertices().size()==geometry.vertices.size()&&decoded.artifact->indices().size()==geometry.indices.size());
   if(cycle==1){const auto path=root/"large.epoch_mesh";write(path,bytes);CHECK(mesh::read_artifact_file(path,large));CHECK(!mesh::read_artifact_file(path));
    // Valid caps do not justify allocating a file buffer whose header counts
    // cannot describe the file length. Stream/filebuf internals may allocate.
    auto invalid=bytes;put32(invalid,40,3);write(path,invalid);blocked=0;denyLarge=true;const auto rejected=mesh::read_artifact_file(path,large);denyLarge=false;CHECK(!rejected&&blocked==0);
   }
  }
  std::fprintf(stderr,"cells=%u vertices=%zu indices=%zu bytes=%zu\n",cells,geometry.vertices.size(),geometry.indices.size(),48+geometry.vertices.size()*44+geometry.indices.size()*4);
 }
 const fs::path tinyPath=root/"tiny.epoch_mesh";write(tinyPath,golden);
 const std::array invalidLimits{mesh::Limits{0,0,0},mesh::Limits{2,6,1024},mesh::Limits{4,2,1024},mesh::Limits{1048577,3,1024},mesh::Limits{3,6291457,1024},mesh::Limits{3,3,64ull*1024*1024+1},mesh::Limits{0xffffffffu,0xffffffffu,0xffffffffffffffffull}};
 for(unsigned cycle=0;cycle<3;++cycle)for(auto limits:invalidLimits){
  CHECK(noAllocation([&]{return mesh::build_artifact(small,triangles,limits);}).code==mesh::Code::limit_exceeded);
  CHECK(noAllocation([&]{return mesh::decode_artifact(golden,limits);}).code==mesh::Code::limit_exceeded);
  CHECK(noAllocation([&]{return mesh::read_artifact_file(tinyPath,limits);}).code==mesh::Code::limit_exceeded);
 }
 CHECK(!noAllocation([&]{return mesh::build_artifact(small,triangles,{4,6,247});}));
 for(unsigned cycle=0;cycle<3;++cycle){
  auto invalid=golden;invalid.push_back(std::byte{});CHECK(!noAllocation([&]{return mesh::decode_artifact(invalid,large);}));
  invalid=golden;put32(invalid,40,0xffffffffu);put32(invalid,44,0xffffffffu);CHECK(!noAllocation([&]{return mesh::decode_artifact(invalid,large);}));
  invalid=golden;put32(invalid,48+4*44,4);CHECK(!noAllocation([&]{return mesh::decode_artifact(invalid,large);}));
  invalid=golden;put32(invalid,48,std::bit_cast<std::uint32_t>(std::numeric_limits<float>::quiet_NaN()));CHECK(!noAllocation([&]{return mesh::decode_artifact(invalid,large);}));
  invalid=golden;put32(invalid,48+12,std::bit_cast<std::uint32_t>(0.0f));put32(invalid,48+16,std::bit_cast<std::uint32_t>(0.0f));put32(invalid,48+20,std::bit_cast<std::uint32_t>(0.0f));CHECK(!noAllocation([&]{return mesh::decode_artifact(invalid,large);}));
  invalid=golden;invalid[8]^=std::byte{1};CHECK(mesh::decode_artifact(invalid,large).code==mesh::Code::content_mismatch);
 }
 for(std::size_t size=0;size<golden.size();++size)
  CHECK(!noAllocation([&]{return mesh::decode_artifact(std::span<const std::byte>{golden}.first(size));}));
 const auto nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
 const std::array badChannels{std::pair{0u,1.0e10f},std::pair{0u,inf},std::pair{3u,nan},std::pair{3u,1.0e30f},
  std::pair{6u,nan},std::pair{7u,inf},std::pair{8u,-0.01f},std::pair{9u,1.01f},std::pair{10u,nan}};
 for(const auto [channel,value]:badChannels){auto invalid=golden;put32(invalid,48+channel*4,std::bit_cast<std::uint32_t>(value));const auto original=invalid;
  CHECK(noAllocation([&]{return mesh::decode_artifact(invalid);}).code==mesh::Code::invalid_geometry);CHECK(invalid==original);}
 {auto invalid=golden;put32(invalid,48+4*44+4,0u);CHECK(noAllocation([&]{return mesh::decode_artifact(invalid);}).code==mesh::Code::invalid_geometry);}
 const mesh::Limits absolute{1048576,6291456,64ull*1024*1024};CHECK(mesh::build_artifact(small,triangles,absolute));
 CHECK(mesh::decode_artifact(golden,absolute)&&mesh::read_artifact_file(tinyPath,absolute));
}
'''


class MeshArtifactLimits(unittest.TestCase):
    def test_actual_bounded_large_codec_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC C++23 CPU module consumer")
        owner_before = hashlib.sha256(OWNER.read_bytes()).hexdigest()
        proof = ROOT / "build/validation/mesh-artifact-limits-oct27"
        proof.mkdir(parents=True, exist_ok=True)
        fixture = proof / "fixture" / str(time.time_ns())
        build = proof / "msvc" / hashlib.sha256(OWNER.read_bytes()).hexdigest()[:12]
        (proof / "mesh.limits_contract.cpp").write_text(CODE, encoding="utf-8")
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(MeshArtifactLimits LANGUAGES CXX)
add_executable(mesh_limits mesh.limits_contract.cpp "{(ROOT/'Engine/src/assets/texture/asset.texture_artifact.cpp').as_posix()}")
target_compile_features(mesh_limits PRIVATE cxx_std_23)
target_compile_options(mesh_limits PRIVATE /W3 /WX /utf-8)
target_sources(mesh_limits PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES "{OWNER.as_posix()}" "{(ROOT/'Engine/modules/asset.texture_artifact.ixx').as_posix()}")
enable_testing()
add_test(NAME actual_bounded_mesh_codec COMMAND mesh_limits "{fixture.as_posix()}/$<CONFIG>")
set_tests_properties(actual_bounded_mesh_codec PROPERTIES TIMEOUT 120)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=150)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-7000:] + completed.stderr)
        for config in ("Debug", "Release"):
            self.assertEqual(hashlib.sha256((fixture / config / "default-v1.bin").read_bytes()).hexdigest(), DEFAULT_V1_SHA256)
        self.assertEqual(hashlib.sha256(OWNER.read_bytes()).hexdigest(), owner_before, "Codec source changed during the proof")
        (proof / "result.json").write_text(json.dumps({"passed": True,
            "scope": "Actual mesh/texture modules and hash implementation; no renderer, project or Engine runtime",
            "ownerSha256": hashlib.sha256(OWNER.read_bytes()).hexdigest(), "defaultV1Sha256": DEFAULT_V1_SHA256,
            "fixture": str(fixture), "commands": results,
            "cases": ["unchanged defaults/v1 golden", "136806/810000 and267126/1587600 cube-sphere roundtrips under explicit32MiB", "default rejection", "three repeated invalid caps/counts/length/index/nonfinite cycles without decoded allocation", "file header/length preflight before file buffer", "absolute bounded caps accepted"],
            "limits": "Only this codec supports explicit larger caps. Project/render/Editor residency budgets remain separate.",
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
