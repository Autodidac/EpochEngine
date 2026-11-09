"""Real provider samples and actual host query orchestration; CPU only."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import time
import unittest
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]
PACKAGE=ROOT/'Engine/dep/EpochEngineExtensions/packages/planetary_terrain'
CODE = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
import asset.mesh_artifact;
import project.planetary;
import planet.terrain;
namespace core=epoch::extensions::planetary_terrain::core;
namespace fs=std::filesystem;
void check(bool value,int line){if(!value){std::cerr<<"Surface contract line "<<line<<'\n';std::exit(71);}}
#define CHECK(...) check(bool((__VA_ARGS__)),__LINE__)
PROVIDER_QUERY
namespace probe {
using namespace epochengine::project_planetary;
using Clock=std::chrono::steady_clock;std::atomic_uint64_t sequence{};
unsigned executions{},providerAdmissions{};bool sourceCurrent=true,meshCurrent=true,bodyCurrent=true,providerReady=true,receiptBound=true,mutateAfter=false;
std::string recipe="recipe-a",mutation;std::vector<GeneratedBody> bodies;
std::stop_source cancellation;
namespace process {
struct ExecutableIdentity {int id=1;};struct EnvironmentVariable{};
struct Active {bool active()const{return true;}};bool busy{};
std::optional<Active> active_in_group(std::string_view){return busy?std::optional{Active{}}:std::nullopt;}
std::optional<std::vector<EnvironmentVariable>> prepare_workspace_environment(const fs::path&){return std::vector<EnvironmentVariable>{};}
}
namespace package_source {
struct Attachment {std::string content_hash="source-a";fs::path relative_source_root="provider";bool source_attached()const{return true;}};
using ProjectAttachment=Attachment;
Attachment load_project_attachment(const fs::path&,std::string_view,std::stop_token){return {};}
}
inline auto source_generation_is_current=[](const fs::path&,const SourceGeneration&,std::stop_token){return sourceCurrent;};
inline auto mesh_generation_is_current=[](const fs::path&,const SourceGeneration&,const Settings&,const MeshGenerationIdentity&){return meshCurrent;};
inline auto mesh_generation_bodies=[](const fs::path&,const SourceGeneration&,const MeshGenerationIdentity&,const epochengine::asset::mesh::Artifact&){return bodyCurrent?bodies:std::vector<GeneratedBody>{};};
std::optional<std::string> recipe_guard(const fs::path&){return recipe;}
std::optional<fs::path> project_directory(const fs::path& root,const fs::path& relative,bool create){auto path=root/relative;if(create)fs::create_directories(path);return path;}
struct AdmittedProvider {fs::path executable="provider.exe",working=".";std::optional<process::ExecutableIdentity> identity{process::ExecutableIdentity{}};};
std::optional<AdmittedProvider> admitted_provider(const fs::path&,const std::string&,Result&,bool require_receipt=false,std::string_view={}){++providerAdmissions;CHECK(require_receipt);if(!providerReady)return {};AdmittedProvider value;if(!receiptBound)value.identity.reset();return value;}
bool execute(const fs::path&,std::vector<std::string> args,const fs::path&,const fs::path&,std::stop_token,
 const std::vector<process::EnvironmentVariable>&,std::uint64_t timeout,Result&,std::optional<process::ExecutableIdentity> identity){
 ++executions;CHECK(timeout==2'000'000'000ull&&identity);args.insert(args.begin(),"provider");std::vector<char*> raw;for(auto& a:args)raw.push_back(a.data());
 const auto code=planet_surface_query_main(int(raw.size()),raw.data());if(code)return false;
 if(!mutation.empty()&&mutation!="cancel"){std::ifstream in(args[2],std::ios::binary);std::string data{std::istreambuf_iterator<char>{in},{}};in.close();
  if(mutation=="trailing")data+="extra=1\n";
  else if(mutation=="truncated")data.resize(data.size()-16u);
  else if(mutation=="oversized")data.append(300000u,'x');
  else {const auto pos=data.find(mutation+'=');CHECK(pos!=std::string::npos);const auto end=data.find('\n',pos);data.replace(pos,end-pos,mutation+"=999999");}
  std::ofstream out(args[2],std::ios::binary|std::ios::trunc);out<<data;
 }
 if(mutateAfter)recipe="recipe-b";
 if(mutation=="cancel")cancellation.request_stop();
 return true;
}
HOST_FUNCTIONS
}
int main(int argc,char** argv){
 CHECK(argc==3);const fs::path root=argv[1],providerSource=argv[2];fs::create_directories(root/"provider/tools");
 fs::copy_file(providerSource,root/"provider/tools/planet.mesh_export.cpp",fs::copy_options::overwrite_existing);
 namespace mesh=epochengine::asset::mesh;using namespace epochengine::project_planetary;
 const mesh::Vertex vertices[]={{{0,0,0},{0,1,0}},{{1,0,0},{0,1,0}},{{0,0,1},{0,1,0}}};
 const std::uint32_t indices[]={0,1,2};const auto artifact=mesh::build_artifact(vertices,indices);CHECK(artifact);
 SurfaceQuery q;q.source={root,"manifest","source-a"};q.artifact=artifact.artifact;q.request_id=1;q.direction={1,0,0};
 q.mesh={q.artifact->key().words,3,3,{}};q.body.id=0;q.body.radius_meters=6371000;probe::bodies={q.body};
 unsigned ready{};
 for(auto radius:{250.0,6371000.0,100000000.0})for(auto seed:{1u,1337u})for(auto body:{0u,1u,5u})for(const auto d:std::array<std::array<double,3>,3>{{{1,0,0},{0,1,0},{0,-1,0}}}){
  q.settings.mode="universe";q.settings.seed=seed;q.settings.reference_radius_meters=radius;q.body.id=body;q.direction=d;
  const core::Universe world{seed,{0,radius}};const auto& b=world.body(body);q.body.radius_meters=b.radius;probe::bodies={q.body};
  const auto actual=probe::query_surface(root,q,{});CHECK(actual.code==SurfaceCode::ready&&actual.operation.ok&&actual.request_id==1);
  const core::DVec3 direction{d[0],d[1],d[2]};const auto expected=core::sample_surface(b,direction);
  CHECK(std::abs(actual.height_meters-expected.height)<1e-8);for(unsigned i=0;i<3;++i)CHECK(std::abs(actual.point_meters[i]-d[i]*(b.radius+expected.height))<1e-7);++ready;
 }
 CHECK(ready==54);q.body.id=4;probe::bodies={q.body};CHECK(probe::query_surface(root,q,{}).code==SurfaceCode::unsupported);
 q.body.id=0;q.settings={};q.body.radius_meters=6371000;q.direction={1,0,0};probe::bodies={q.body};
 const auto no_execution=[&](SurfaceCode code){const auto before=probe::executions;CHECK(probe::query_surface(root,q,{}).code==code);CHECK(probe::executions==before);};
 q.direction={0,0,0};no_execution(SurfaceCode::invalid);q.direction={1,0,0};q.mesh.vertex_count=4;no_execution(SurfaceCode::invalid);q.mesh.vertex_count=3;
 probe::sourceCurrent=false;no_execution(SurfaceCode::stale);probe::sourceCurrent=true;
 probe::meshCurrent=false;no_execution(SurfaceCode::stale);probe::meshCurrent=true;
 probe::bodyCurrent=false;no_execution(SurfaceCode::stale);probe::bodyCurrent=true;
 probe::process::busy=true;no_execution(SurfaceCode::busy);probe::process::busy=false;
 probe::receiptBound=false;no_execution(SurfaceCode::provider_failure);probe::receiptBound=true;
 {std::ofstream old(root/"provider/tools/planet.mesh_export.cpp",std::ios::trunc);old<<"immutable old provider";}
 no_execution(SurfaceCode::unsupported);fs::copy_file(providerSource,root/"provider/tools/planet.mesh_export.cpp",fs::copy_options::overwrite_existing);
 probe::mutateAfter=true;auto stale=probe::query_surface(root,q,{});CHECK(stale.code==SurfaceCode::stale&&!stale.operation.ok);probe::mutateAfter=false;probe::recipe="recipe-a";
 for(const auto field:{"request_id","seed","body","reference_radius_meters","body_radius_meters","up_x","normal_y","trailing"}){
  probe::mutation=field;CHECK(probe::query_surface(root,q,{}).code==SurfaceCode::provider_failure);
 }probe::mutation.clear();
 probe::cancellation.request_stop();const auto before=probe::executions;CHECK(probe::query_surface(root,q,probe::cancellation.get_token()).code==SurfaceCode::cancelled&&probe::executions==before);
 CHECK(probe::query_surface(root,q,{}).code==SurfaceCode::ready);
 probe::cancellation=std::stop_source{};probe::mutation="cancel";CHECK(probe::query_surface(root,q,probe::cancellation.get_token()).code==SurfaceCode::cancelled);
 probe::mutation.clear();probe::cancellation=std::stop_source{};
 SurfaceBatchQuery batch{q,{}};for(unsigned i=0;i<64;++i){const double angle=i*0.07;batch.directions.push_back({std::cos(angle),std::sin(angle),0});}
 const auto prior=probe::executions;const auto sampled=probe::query_surfaces(root,batch,{});
 CHECK(sampled.code==SurfaceCode::ready&&sampled.operation.ok&&sampled.samples.size()==64&&probe::executions==prior+1);
 const core::Universe world{q.settings.seed,{q.settings.size_reference_body,q.settings.reference_radius_meters}};
 for(unsigned i=0;i<64;++i){const auto& d=batch.directions[i];const auto expected=core::sample_surface(world.body(q.body.id),{d[0],d[1],d[2]});
  CHECK(std::abs(sampled.samples[i].height_meters-expected.height)<1e-8&&sampled.samples[i].request_id==q.request_id);}
 const auto no_batch_execution=[&](SurfaceCode code){const auto before=probe::executions;const auto value=probe::query_surfaces(root,batch,{});CHECK(value.code==code&&value.samples.empty()&&probe::executions==before);};
 auto kept=batch.directions;batch.directions.clear();no_batch_execution(SurfaceCode::invalid);batch.directions=kept;
 batch.directions.push_back({1,0,0});no_batch_execution(SurfaceCode::invalid);batch.directions.pop_back();
 batch.directions[63]={0,0,0};no_batch_execution(SurfaceCode::invalid);batch.directions[63]={1,0,0};
 probe::sourceCurrent=false;no_batch_execution(SurfaceCode::stale);probe::sourceCurrent=true;
 probe::meshCurrent=false;no_batch_execution(SurfaceCode::stale);probe::meshCurrent=true;
 probe::process::busy=true;no_batch_execution(SurfaceCode::busy);probe::process::busy=false;
 probe::receiptBound=false;no_batch_execution(SurfaceCode::provider_failure);probe::receiptBound=true;
 batch.query.body.id=4;probe::bodies={batch.query.body};const auto gas=probe::query_surfaces(root,batch,{});CHECK(gas.code==SurfaceCode::unsupported&&gas.samples.empty());batch.query=q;probe::bodies={q.body};
 for(const auto field:{"request_id","samples","sample_bytes","body","up_x","normal_y","trailing","truncated","oversized"}){
  probe::mutation=field;const auto value=probe::query_surfaces(root,batch,{});CHECK(value.code==SurfaceCode::provider_failure&&value.samples.empty());}probe::mutation.clear();
 probe::mutateAfter=true;const auto changed=probe::query_surfaces(root,batch,{});CHECK(changed.code==SurfaceCode::stale&&changed.samples.empty());probe::mutateAfter=false;probe::recipe="recipe-a";
 probe::cancellation.request_stop();
 const auto cancelled=probe::query_surfaces(root,batch,probe::cancellation.get_token());CHECK(cancelled.code==SurfaceCode::cancelled&&cancelled.samples.empty());
 probe::cancellation=std::stop_source{};probe::mutation="cancel";const auto stopped=probe::query_surfaces(root,batch,probe::cancellation.get_token());CHECK(stopped.code==SurfaceCode::cancelled&&stopped.samples.empty());
 std::cout<<"ready_samples="<<ready<<" batch_samples=64 single_batch_execution=1 stale_cancel_packet_bounds=pass\n";
}
'''

class PlanetarySurfaceQuery(unittest.TestCase):
    def test_admitted_query_and_real_provider(self):
        if os.name!='nt': self.skipTest('Installed MSVC CPU module lane')
        host=ROOT/'Engine/src/project/project.planetary.cpp'
        provider=PACKAGE/'tools/planet.mesh_export.cpp'
        source=host.read_text(encoding='utf-8')
        provider_text=provider.read_text(encoding='utf-8')
        actual_provider='\n'.join(definition(provider_text,marker) for marker in (
            'std::string planet_surface_packet(', 'int planet_surface_query_main('))
        functions='\n'.join(definition(source,marker) for marker in (
            '        std::string read_small(', '        std::string real_text(',
            '        bool source_supports_surface_query(', '        bool valid_surface_query(',
            '        bool source_supports_surface_batch(',
            '        bool surface_generation_is_current(', '        bool parse_surface_receipt(',
            '        bool parse_surface_batch_receipt(', '    SurfaceBatchResult query_surface_samples(',
            '    SurfaceResult query_surface(', '    SurfaceBatchResult query_surfaces('))
        code=CODE.replace('PROVIDER_QUERY',actual_provider).replace('HOST_FUNCTIONS',functions)
        identity=hashlib.sha256(code.encode()+Path(__file__).read_bytes()).hexdigest()[:12]
        proof=ROOT/'Artifacts/Reports/planet-surface-query-oct31'/identity
        proof.mkdir(parents=True,exist_ok=True)
        (proof/'consumer.cpp').write_text(code,encoding='utf-8')
        modules=[PACKAGE/'modules'/name for name in ('planet.terrain.ixx','planet.coverage.ixx','planet.streaming.ixx','planet.clouds.ixx')]
        modules += [ROOT/'Engine/modules'/name for name in ('asset.mesh_artifact.ixx','asset.texture_artifact.ixx','asset.cloud_volume.ixx','project.planetary.ixx','project.planetary_snapshot.ixx','water.wave.ixx')]
        owners=[PACKAGE/'upstream/EpochPlanet-0.2.3/src/core/world.cpp',PACKAGE/'upstream/EpochPlanet-0.2.3/src/core/terrain.cpp',ROOT/'Engine/src/assets/texture/asset.texture_artifact.cpp',ROOT/'Engine/src/project/project.planetary_snapshot.cpp']
        quoted=lambda paths:'\n'.join('"'+p.as_posix()+'"' for p in paths)
        (proof/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(PlanetSurfaceQuery LANGUAGES CXX)
add_library(owners STATIC {quoted(owners)})
target_compile_features(owners PUBLIC cxx_std_23)
target_compile_options(owners PUBLIC /sdl /utf-8 /permissive- /Zc:__cplusplus /EHsc)
target_include_directories(owners PUBLIC "{(PACKAGE/'upstream/EpochPlanet-0.2.3/include').as_posix()}")
target_sources(owners PUBLIC FILE_SET CXX_MODULES BASE_DIRS "{(PACKAGE/'modules').as_posix()}" "{(ROOT/'Engine/modules').as_posix()}" FILES {quoted(modules)})
add_executable(surface_consumer consumer.cpp)
add_executable(surface_provider "{provider.as_posix()}" "{(PACKAGE/'tools/planet.stream_export.cpp').as_posix()}")
target_link_libraries(surface_consumer PRIVATE owners)
target_link_libraries(surface_provider PRIVATE owners)
include("{(ROOT/'Engine/cmake/epoch_task_scheduler.cmake').as_posix()}")
epoch_admit_task_scheduler(surface_provider "{ROOT.as_posix()}")
''',encoding='utf-8')
        build=ROOT/'Artifacts/Intermediate/cmake/planet-surface-query'/identity
        source_paths=owners+modules+[host,provider,PACKAGE/'tools/planet.stream_export.cpp',Path(__file__)]
        pins={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in source_paths}
        runs=[]
        def run(command,expected=0):
            start=time.monotonic();result=subprocess.run(command,cwd=proof,capture_output=True,text=True,timeout=180)
            log=proof/f'lane-{len(runs)}.log';log.write_text(result.stdout+result.stderr,encoding='utf-8')
            runs.append({'command':command,'exit':result.returncode,'seconds':time.monotonic()-start,'log':str(log)})
            (proof/'result.json').write_text(json.dumps({'runs':runs,'source_before':pins,'scope':'Actual complete query/provider function bodies with recording source/process ports; real world sampler linked. Procedural contact is not displayed triangle contact.'},indent=2),encoding='utf-8')
            self.assertEqual(result.returncode,expected,result.stdout[-5000:]+result.stderr[-2000:])
            return result
        run(['cmake','-S',str(proof),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64'])
        for config in ('Debug','Release'):
            run(['cmake','--build',str(build),'--config',config,'--parallel','1'])
            data=proof/config;data.mkdir(exist_ok=True)
            run([str(build/config/'surface_consumer.exe'),str(data),str(provider)])
            binary=build/config/'surface_provider.exe'
            receipt=data/'cli.surface'
            run([str(binary),'--surface-query',str(receipt),'1337','0','0','6371000','1','0','0','42'])
            packet=receipt.read_text();self.assertIn('request_id=42\nstatus=ready\n',packet)
            run([str(binary),'--surface-query',str(data/'invalid.surface'),'1337','0','0','6371000','0','0','0','43'],2)
            self.assertFalse((data/'invalid.surface').exists())
            batch_receipt=data/'cli.batch'
            run([str(binary),'--surface-batch',str(batch_receipt),'1337','0','0','6371000','44','2','1','0','0','0','1','0'])
            self.assertEqual(batch_receipt.read_text().count('epoch.planet.surface.v1\n'),2)
            run([str(binary),'--surface-batch',str(data/'invalid.batch'),'1337','0','0','6371000','45','65'],2)
            self.assertFalse((data/'invalid.batch').exists())
        after={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in source_paths}
        self.assertEqual(pins,after)
        (proof/'source-freeze.json').write_text(json.dumps({'passed':True,'source_before':pins,'source_after':after,'runs':runs,'canonical_provider_source':str(provider)},indent=2),encoding='utf-8')
if __name__=='__main__':unittest.main()
