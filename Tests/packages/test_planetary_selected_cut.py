"""Actual C++23 snapshot owner and extracted worker publication; CPU only."""
from pathlib import Path
import json
import os
import subprocess
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / 'Engine/dep/EpochEngineExtensions/packages/planetary_terrain'

CODE = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>
import planet.terrain;
import asset.mesh_artifact;
import project.planetary_snapshot;
import project.planetary;
namespace p=epochengine::project_planetary_snapshot;
namespace mesh=epochengine::asset::mesh;
namespace core=epoch::extensions::planetary_terrain::core;
namespace project_planetary_snapshot=p;
struct Result {std::shared_ptr<const mesh::Artifact> snapshot;std::shared_ptr<const p::Snapshot> patch_snapshot;};
Result actual_worker(const mesh::Result& decoded,std::optional<p::Metadata> metadata,p::Limits patchLimits,std::stop_token cancellation={}) {
 Result result;struct {std::string message;} operation;
 ACTUAL_WORKER
 return result;
}
std::shared_ptr<p::Snapshot> fixture99(unsigned copies=1) {
 auto s=std::make_shared<p::Snapshot>();
 s->metadata.identity.source_hash="sha256:"+std::string(64,'a');
 s->metadata.identity.recipe_key=std::string(64,'b');s->metadata.identity.request_id=1;
 std::vector<mesh::Vertex> vertices;std::vector<std::uint32_t> indices;
 auto add=[&](core::TileKey key,bool target) {
  p::Patch patch;patch.key={key.x,key.y,key.body,key.face,key.level};patch.target=target;
  unsigned n{};for(auto k:core::neighbors(key))patch.neighbors[n++]={k.x,k.y,k.body,k.face,k.level};
  patch.first_vertex=static_cast<std::uint32_t>(vertices.size());patch.vertex_count=4*copies;
  patch.first_index=static_cast<std::uint32_t>(indices.size());patch.index_count=6*copies;
  float side=float(1u<<key.level);
  for(unsigned copy=0;copy<copies;++copy) {
   for(unsigned i=0;i<4;++i) {mesh::Vertex v;v.position={(key.x+i%2)/side,(key.y+i/2)/side,float(key.face*2)+key.level*.1f};vertices.push_back(v);s->metadata.parent_delta.push_back({0,0,key.level?-.1f:0.f});}
   for(auto i:std::array<unsigned,6>{0,1,2,1,3,2})indices.push_back(patch.first_vertex+copy*4+i);
  }
  s->metadata.patches.push_back(patch);
 };
 for(std::uint8_t face=0;face<6;++face) {
  core::TileKey root{0,0,0,face,0};add(root,false);
  for(auto first:core::children(root)) {add(first,false);
   for(auto second:core::children(first)) {bool split=face==0&&second.x==0&&second.y==0;add(second,!split);
    if(split)for(auto third:core::children(second))add(third,true);
   }
  }
 }
 s->artifact=mesh::build_artifact(vertices,indices,{1000000,6000000,64ull*1024*1024}).artifact;
 if(s->artifact)s->metadata.identity.artifact_key=s->artifact->key();return s;
}
bool same_vertex(const mesh::Vertex& a,const mesh::Vertex& b) {return a.position==b.position&&a.normal==b.normal&&a.uv==b.uv&&a.color==b.color;}
int check_cut(std::shared_ptr<const p::Snapshot> s,p::Limits limits,unsigned minimum) {
 if(!s||!s->artifact||!p::valid_metadata(s->metadata,*s->artifact,limits))return 1;
 unsigned targets{};for(auto& patch:s->metadata.patches)targets+=patch.target;
 if(targets<minimum)return 2;
 p::Transition legacy{limits};if(!legacy.admit(s)||legacy.draw().size()!=6)return 3;
 p::Transition exact{limits};if(!exact.admit(s,{},p::InitialCut::selected_patches)||!exact.settled()||exact.draw().size()!=targets)return 4;
 auto output=exact.materialize();if(!output)return 5;
 std::size_t nv{},ni{};
 for(auto& draw:exact.draw()) {
  if(draw.morph!=0||draw.snapshot!=s)return 6;
  auto& patch=s->metadata.patches[draw.patch];if(!patch.target)return 7;
  for(unsigned i=0;i<patch.vertex_count;++i)if(!same_vertex(output->vertices()[nv+i],s->artifact->vertices()[patch.first_vertex+i]))return 8;
  for(unsigned i=0;i<patch.index_count;++i)if(output->indices()[ni+i]!=nv+s->artifact->indices()[patch.first_index+i]-patch.first_vertex)return 9;
  nv+=patch.vertex_count;ni+=patch.index_count;
 }
 if(output->vertices().size()!=nv||output->indices().size()!=ni)return 10;
 mesh::Result decoded{mesh::Code::ready,s->artifact};
 auto worker=actual_worker(decoded,s->metadata,limits);
 if(!worker.snapshot||worker.snapshot->key()!=output->key()||worker.patch_snapshot->artifact!=s->artifact)return 11;
 std::stop_source stop;stop.request_stop();
 if(actual_worker(decoded,s->metadata,limits,stop.get_token()).snapshot)return 12;
 auto tiny=limits;tiny.maximum_retained_bytes=1;
 if(actual_worker(decoded,s->metadata,tiny).snapshot)return 13;
 auto invalid=std::make_shared<p::Snapshot>(*s);invalid->metadata.patches[0].target=true;
 if(actual_worker(decoded,invalid->metadata,limits).snapshot)return 14;
 return 0;
}
int main(int argc,char** argv) {
 p::Limits limits;limits.artifact={1000000,6000000,64ull*1024*1024};limits.maximum_retained_bytes=128ull*1024*1024;limits.maximum_transient_bytes=128ull*1024*1024;
 auto fixture=fixture99();if(!fixture||std::count_if(fixture->metadata.patches.begin(),fixture->metadata.patches.end(),[](auto& v){return v.target;})!=99)return 20;
 if(auto fail=check_cut(fixture,limits,99))return 30+fail;
 namespace planet=epochengine::project_planetary;
 auto settings=planet::with_detail_preset(planet::Settings{},planet::DetailPreset::detailed);
 if(!settings)return 100;
 auto admittedLimits=planet::snapshot_transition_limits(*settings);
 auto dense=fixture99(700);if(!dense||!dense->artifact)return 101;
 auto rendererLimits=admittedLimits;rendererLimits.maximum_retained_bytes=planet::scene_mesh_budget(*settings).cache_bytes;
 p::Transition oldRendererBudget{rendererLimits};
 if(oldRendererBudget.admit(dense,{},p::InitialCut::selected_patches))return 102;
 if(auto fail=check_cut(dense,admittedLimits,99))return 110+fail;
 const auto memory=planet::preview_memory_footprint(*settings);
 if(memory.replacement_upper_bytes<admittedLimits.maximum_retained_bytes+admittedLimits.maximum_transient_bytes)return 130;
 if(argc==3) {
  auto decoded=mesh::read_artifact_file(std::filesystem::path(argv[1])/"terrain.epoch_mesh",limits.artifact);if(!decoded)return 60;
  std::ifstream in(std::filesystem::path(argv[1])/"terrain.epoch_planet_patch",std::ios::binary);std::string raw{std::istreambuf_iterator<char>(in),{}};
  p::Identity identity{decoded.artifact->key(),"sha256:"+std::string(64,'a'),std::string(64,'b'),std::strtoull(argv[2],nullptr,10)};
  auto metadata=p::decode(std::as_bytes(std::span(raw.data(),raw.size())),*decoded.artifact,identity,limits);if(!metadata)return 61;
  auto s=std::make_shared<p::Snapshot>(p::Snapshot{decoded.artifact,std::move(*metadata)});
  if(auto fail=check_cut(s,limits,7))return 70+fail;
 }
 return 0;
}
'''

class SelectedCut(unittest.TestCase):
    @unittest.skipUnless(os.name=='nt','This lane uses the installed Windows/MSVC production module toolchain')
    def test_actual_worker_selected_cut_and_legacy_morph(self):
        owner=(ROOT/'Engine/src/project/project.planetary.cpp').read_text(encoding='utf-8')
        first=owner.index('                    auto admitted=std::make_shared<const project_planetary_snapshot::Snapshot>(')
        last=owner.index('                    result.patch_snapshot=std::move(admitted);',first)+len('                    result.patch_snapshot=std::move(admitted);')
        code=CODE.replace('ACTUAL_WORKER',owner[first:last])
        proof=ROOT/'Artifacts/Reports/planetary-selected-cut-oct31'/str(time.time_ns());proof.mkdir(parents=True)
        # MSVC appends nested scheduler target, module and file-tracker paths.
        build=ROOT/'Artifacts/Tests/psc'/proof.name
        (proof/'consumer.cpp').write_text(code,encoding='utf-8')
        (proof/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(SelectedCut LANGUAGES CXX)
set(EPOCH_ENGINE_SOURCE_ROOT "{ROOT.as_posix()}" CACHE PATH "" FORCE)
set(EPOCH_PLANET_BUILD_MESH_EXPORT ON CACHE BOOL "" FORCE)
set(EPOCH_PLANET_BUILD_WATER ON CACHE BOOL "" FORCE)
set(EPOCH_PLANET_WATER_SOURCE_ROOT "{(PACKAGE.parent/'fft_ocean').as_posix()}" CACHE PATH "" FORCE)
set(BUILD_TESTING ON CACHE BOOL "" FORCE)
enable_testing()
add_subdirectory("{PACKAGE.as_posix()}" planet)
add_executable(selected_cut consumer.cpp)
target_compile_features(selected_cut PRIVATE cxx_std_23)
target_compile_options(selected_cut PRIVATE /utf-8 /EHsc)
target_link_libraries(selected_cut PRIVATE EpochEngineExtensions::PlanetaryCore epoch_planet_mesh_format)
add_library(selected_cut_settings STATIC)
target_compile_features(selected_cut_settings PUBLIC cxx_std_23)
target_sources(selected_cut_settings PUBLIC FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES "{(ROOT/'Engine/modules/project/project.planetary.ixx').as_posix()}")
target_link_libraries(selected_cut_settings PUBLIC epoch_planet_mesh_format)
target_link_libraries(selected_cut PRIVATE selected_cut_settings)
add_executable(settings_contract "{(ROOT/'Tests/packages/project.planetary_settings_contract.cpp').as_posix()}")
target_link_libraries(settings_contract PRIVATE selected_cut_settings)
''',encoding='utf-8')
        records=[]
        def run(command,timeout=180):
            started=time.monotonic();r=subprocess.run(list(map(str,command)),capture_output=True,text=True,cwd=proof,timeout=timeout)
            log=proof/f'lane-{len(records)}.log';log.write_text(r.stdout+r.stderr,encoding='utf-8')
            records.append(dict(command=list(map(str,command)),exit=r.returncode,seconds=time.monotonic()-started,log=str(log)))
            (proof/'result.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
            self.assertEqual(r.returncode,0,r.stdout[-7000:]+r.stderr);return r
        run(['cmake','-S',proof,'-B',build,'-G','Visual Studio 17 2022','-A','x64'])
        for config in ('Debug','Release'):
            run(['cmake','--build',build,'--config',config,'--target','selected_cut','settings_contract','epoch_planet_snapshot_contract','epoch_planet_mesh_export','--parallel','1','--','/nr:false','/p:UseMultiToolTask=false','/p:CL_MPCount=1'])
            run([build/config/'selected_cut.exe'])
            run([build/config/'settings_contract.exe'])
            run([build/'planet'/config/'epoch_planet_snapshot_contract.exe'])
        producer=build/'planet/Release/epoch_planet_mesh_export.exe'
        identity='sha256:'+'a'*64+':'+'b'*64
        pending=1
        for request in range(1,9):
            output=proof/f'provider-{request}'
            run([producer,'--stream-step-preview',output,proof/'cache',1337,16,0,0,0,25,1024,1.0471975511965976,18,384,134217728,32,request,1,'--mesh-bytes',33554432,'--patch-transport-v1',identity])
            receipt=dict(line.split('=',1) for line in (output/'terrain.stream').read_text().splitlines() if '=' in line)
            pending=int(receipt['pending'])
            if pending==0:break
        self.assertEqual(pending,0,'Actual provider did not settle within eight bounded steps')
        for config in ('Debug','Release'):run([build/config/'selected_cut.exe',output,request])

if __name__=='__main__':unittest.main()
