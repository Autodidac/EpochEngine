"""Actual session publication, Context accessors and project Water/marker owners."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import time
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.packages.test_planetary_mesh_admission import owner_function

ROOT = Path(__file__).resolve().parents[2]


def consumer():
    context = (ROOT/'Engine/modules/core.context.ixx').read_text(encoding='utf-8')
    host = (ROOT/'Engine/src/epoch/implementation/epoch.session_runtime.inl').read_text(encoding='utf-8')
    legacy = (ROOT/'Engine/src/epoch/epoch.engine_legacy.cpp').read_text(encoding='utf-8')
    accessors = '\n'.join(definition(context, marker) for marker in (
        '        [[nodiscard]] bool publish_simulation_sample_time(',
        '        [[nodiscard]] std::optional<double> simulation_sample_time('))
    field = re.search(r'        std::atomic<double> simulationSampleSeconds[^;]+;',context).group()
    publication = re.search(r'\(void\)ctx->publish_simulation_sample_time\(stats\.simulated_seconds\);',host).group()
    helpers = '\n'.join(definition(legacy, marker) for marker in (
        '        [[nodiscard]] epochengine::previewgrid::Vec3 runtime_marker_color_for_entity(',
        '        [[nodiscard]] float runtime_marker_radius_for_entity(',
        '        [[nodiscard]] epochengine::previewgrid::ObjectPreviewPrimitive runtime_preview_primitive_for_entity(',
        '        [[nodiscard]] bool runtime_entity_is_forest_preview(',
        '        [[nodiscard]] bool runtime_forest_projection_admitted(',
        '        [[nodiscard]] std::optional<epochengine::previewgrid::Vec3> project_play_position_for_origin(',
        '        [[nodiscard]] bool project_play_entity_has_geometry(',
        '        [[nodiscard]] std::optional<epochengine::previewgrid::ObjectMarker> project_play_marker_for_entity('))
    owner = legacy[legacy.index('        struct ProjectPlayWaterCache final'):legacy.index('        [[nodiscard]] std::filesystem::path project_runtime_root(')]
    play=(ROOT/'Engine/src/epoch/implementation/epoch.project_play_scene.inl').read_text(encoding='utf-8')
    owner += '\n' + owner_function(play, '            bool publish_project_play_markers(')
    sampling_start=play.index('                    auto& waterCache=m_waterCaches[ctx.get()];')
    sampling=play[sampling_start:play.index('                    const bool published=',sampling_start)]
    cache_field=re.search(r'            std::unordered_map<const void\*,ProjectPlayWaterCache> m_waterCaches\{\};',play).group()
    return r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
import water.system;
import water.surface;
import asset.mesh_artifact;
import asset.texture_artifact;
import scene.snapshot;
import scene.serializer;
import scene.runtime;
import render.preview_grid;
import render.lighting;
import context.type;
import core.time;
#define CHECK(...) do {if(!(__VA_ARGS__)) return __LINE__;} while(false)
namespace epochengine::core {
// The actual Context accessors/member are compiled verbatim. Native hooks are
// absent; the only type field is the existing publisher's backend admission port.
class Context { public: ContextType type{ContextType::OpenGL}; ACCESSORS
private: FIELD
}; }
namespace logger {
enum class LogLevel {WARN}; inline unsigned warnings{};
struct Port {void log(LogLevel,std::string_view,std::source_location){++warnings;}};
inline Port& get(const char*) {static Port port;return port;}
}
namespace epochengine::engine {
using ProjectRuntimeEntity=scene_runtime::RuntimeEntity;
HELPERS
constexpr int m_runtimeViewportWidth=1280,m_runtimeViewportHeight=720;
// This contract uses nonplanetary Water with no explicit frame camera.
bool pump_runtime_planetary_frame(const void*,previewgrid::SceneFrameRequest,bool){std::abort();}
OWNER
struct ProjectWaterPort {
 bool m_runtimeWaterVisible{true};
 std::vector<ProjectRuntimeEntity> m_entities; CACHE_FIELD
 void sample(core::Context* raw) { const std::shared_ptr<core::Context> ctx{raw,[](auto*){}}; SAMPLING }
};
}
int main(int argc,char** argv) {
 try {
  CHECK(argc==2); using namespace epochengine; using namespace epochengine::engine;
  std::ofstream proof{argv[1]}; CHECK(proof.good());
  core::Context a,b; auto* ctx=&a;
  CHECK(!a.simulation_sample_time()&&!b.simulation_sample_time());
  CHECK(!a.publish_simulation_sample_time(std::numeric_limits<double>::quiet_NaN()));
  CHECK(!a.publish_simulation_sample_time(std::numeric_limits<double>::infinity()));
  CHECK(!a.simulation_sample_time());
  core::time::simulation_clock clock;clock.start();clock.tick(0.25);clock.consume_steps(clock.step_budget());
  const auto stats=clock.stats(); PUBLICATION
  CHECK(a.simulation_sample_time()==stats.simulated_seconds&&!b.simulation_sample_time());
  const auto before=*a.simulation_sample_time();clock.set_paused(true);clock.tick(0.25);clock.consume_steps(clock.step_budget());
  CHECK(clock.stats().simulated_seconds==before);
  CHECK(a.publish_simulation_sample_time(0.0));CHECK(b.publish_simulation_sample_time(9.0));
  CHECK(!a.publish_simulation_sample_time(-std::numeric_limits<double>::infinity()));
  CHECK(*a.simulation_sample_time()==0.0&&*b.simulation_sample_time()==9.0);
  scene::SceneSnapshot source;source.scene_id="water-scene";source.project_id="water-project";source.world_name="Water";
  source.objects.push_back({.id=1,.name="Water",.type="Water",.category="World",.position={2,3,4},.rotation={0,20,0},.scale={2,1,3}});
  source.objects.push_back({.id=2,.name="Cube",.type="Cube",.category="Geometry"});
  source.objects.push_back({.id=10,.name="Camera",.type="Camera",.category="Camera"});
  source.objects.push_back({.id=11,.name="Spawn",.type="Spawn",.category="Gameplay"});
  source.primary_camera=10;source.primary_spawn=11;
  const auto saved=scene::serialize_snapshot_text(source);CHECK(!saved.empty());
  const auto reopened=scene::parse_snapshot_text(saved);CHECK(reopened&&reopened.snapshot.objects[0].type=="Water");
  CHECK(!reopened.snapshot.objects[0].mesh_asset&&reopened.snapshot.objects[0].position==source.objects[0].position);
  scene_runtime::SceneRuntime runtime;CHECK(runtime.replace(reopened.snapshot).committed());
  const auto projection=runtime.projection();std::vector<ProjectRuntimeEntity> entities(projection->entities().begin(),projection->entities().end());
  ProjectWaterPort shared;shared.m_entities=entities;shared.sample(&a);const auto perA=shared.m_waterCaches[&a].mesh;
  shared.sample(&b);CHECK(shared.m_waterCaches.size()==2&&shared.m_waterCaches[&a].mesh==perA&&shared.m_waterCaches[&b].mesh!=perA);
  shared.sample(&a);CHECK(shared.m_waterCaches[&a].mesh==perA&&shared.m_waterCaches[&a].sampled_seconds==0.0&&shared.m_waterCaches[&b].sampled_seconds==9.0);
  ProjectPlayWaterCache cache;CHECK(sample_project_play_water(entities,a.simulation_sample_time(),cache));
  CHECK(cache.mesh&&cache.mesh->vertices().size()==1089&&cache.mesh->indices().size()==6144);
  const auto first=cache.mesh;const auto bytes=asset::mesh::encode_artifact(*first);
  CHECK(sample_project_play_water(entities,a.simulation_sample_time(),cache)&&cache.mesh==first);
  CHECK(a.publish_simulation_sample_time(2.0));CHECK(sample_project_play_water(entities,a.simulation_sample_time(),cache));
  CHECK(cache.mesh!=first&&cache.mesh->key()!=first->key());
  CHECK(a.publish_simulation_sample_time(0.0));CHECK(sample_project_play_water(entities,a.simulation_sample_time(),cache));
  CHECK(asset::mesh::encode_artifact(*cache.mesh)==bytes);
  CHECK(publish_project_play_markers(&a,entities,nullptr,{}, {},0,false,{},nullptr,cache.mesh));
  const auto frame=previewgrid::object_solid_frame_for(&a);CHECK(frame&&frame->markers.size()==2);
  const auto& water=frame->markers[0].marker;const auto& cube=frame->markers[1].marker;
  CHECK(water.primitive==previewgrid::ObjectPreviewPrimitive::IndexedMesh&&water.mesh==cache.mesh);
  CHECK(water.position.x==2&&water.position.y==3&&water.position.z==4&&water.scale.x==2&&water.scale.z==3&&water.rotationDegrees.y==20);
  CHECK(cube.primitive==previewgrid::ObjectPreviewPrimitive::Cube&&!cube.mesh);
  CHECK(!publish_project_play_markers(&a,entities,nullptr,{.maximum_indices=6143},{},0,false,{},nullptr,cache.mesh));
  CHECK(previewgrid::object_solid_frame_for(&a)==frame);
  const auto warn=logger::warnings;CHECK(!sample_project_play_water(entities,{},cache)&&!cache.mesh&&!cache.diagnostic.empty());
  CHECK(logger::warnings==warn+1);CHECK(!sample_project_play_water(entities,{},cache)&&logger::warnings==warn+1);
  CHECK(publish_project_play_markers(&a,entities,nullptr,{}, {},0,false,{},nullptr,cache.mesh));
  CHECK(previewgrid::object_solid_frame_for(&a)->markers.size()==1);
  CHECK(!sample_project_play_water(entities,std::numeric_limits<double>::quiet_NaN(),cache));
  CHECK(sample_project_play_water(entities,b.simulation_sample_time(),cache)&&cache.diagnostic.empty());
  auto second=entities[0];second.source_id=3;second.transform.position={10,0,0};entities.push_back(second);
  CHECK(publish_project_play_markers(&a,entities,nullptr,{}, {},0,false,{},nullptr,cache.mesh));
  const auto multiple=previewgrid::object_solid_frame_for(&a);CHECK(multiple->markers.size()==3&&multiple->markers[0].marker.mesh==multiple->markers[2].marker.mesh);
  entities.erase(entities.begin());entities.pop_back();CHECK(sample_project_play_water(entities,{},cache)&&!cache.mesh&&cache.diagnostic.empty());
  CHECK(publish_project_play_markers(&a,entities)&&previewgrid::object_solid_frame_for(&a)->markers.size()==1);
  CHECK(scene::serialize_snapshot_text(reopened.snapshot)==saved);
  previewgrid::clear_object_markers(&a);previewgrid::clear_object_markers(&b);
  proof<<"PASS: per-context central time, pause cache, Water mesh publication, budget refusal, saved typed scene, forward sampling and explicit-time replay\n";
  return proof.good()?0:94;
 } catch (...) {return 95;}
}
'''.replace('ACCESSORS',accessors).replace('CACHE_FIELD',cache_field).replace('FIELD',field).replace('PUBLICATION',publication).replace('HELPERS',helpers).replace('OWNER',owner).replace('SAMPLING',sampling)


class ProjectWaterRuntime(unittest.TestCase):
    def test_actual_water_runtime_owners_debug_release(self):
        if os.name!='nt':self.skipTest('Installed MSVC CPU lane')
        report=ROOT/'Artifacts/Reports/project-water-runtime-oct29'/str(time.time_ns());report.mkdir(parents=True)
        cpp=report/'water.runtime_contract.cpp';cpp.write_text(consumer(),encoding='utf-8')
        roots=[ROOT/'Engine/modules',ROOT/'Engine/dep/EpochPlatformEngine/modules'];modules={}
        for folder in roots:
            for path in folder.glob('*.ixx'):
                match=re.search(r'^export module\s+([\w.:]+);',path.read_text(encoding='utf-8-sig'),re.M)
                if match:modules[match[1]]=path
        selected={};pending=['water.surface','scene.runtime','scene.serializer','render.preview_grid','context.type','core.time','authoring.texture_detail']
        while pending:
            name=pending.pop()
            if name in selected:continue
            selected[name]=modules[name];pending+=re.findall(r'^(?:export )?import\s+([\w.]+);',modules[name].read_text(encoding='utf-8-sig'),re.M)
        implementations=[ROOT/'Engine/src/render/render.preview_camera.cpp',ROOT/'Engine/src/water/water.system.cpp',ROOT/'Engine/src/water/water.surface.cpp',ROOT/'Engine/src/assets/texture/asset.texture_artifact.cpp',ROOT/'Engine/dep/EpochPlatformEngine/src/core/core.time.cpp',ROOT/'Engine/src/authoring/texture/authoring.texture_document.cpp',ROOT/'Engine/src/authoring/texture/authoring.texture_residency.cpp']
        paths=' '.join('"'+p.as_posix()+'"' for p in implementations)
        (report/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
project(ProjectWaterRuntime LANGUAGES CXX)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
add_library(owners STATIC {paths})
target_compile_features(owners PUBLIC cxx_std_23)
target_compile_options(owners PUBLIC /utf-8 /EHsc /permissive-)
target_include_directories(owners PRIVATE "{(ROOT/'Engine/include').as_posix()}" "{(ROOT/'Engine').as_posix()}")
target_sources(owners PUBLIC FILE_SET CXX_MODULES BASE_DIRS {' '.join('"'+p.as_posix()+'"' for p in roots)} FILES {' '.join('"'+p.as_posix()+'"' for p in selected.values())})
add_executable(water_runtime "{cpp.as_posix()}")
target_link_libraries(water_runtime PRIVATE owners)
''',encoding='utf-8')
        build=ROOT/'Artifacts/Intermediate/tests/project-water-runtime'/report.name
        commands=[['cmake','-S',str(report),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64','-T','host=x64']]
        for mode in ('Debug','Release'):
            commands += [['cmake','--build',str(build),'--config',mode,'--parallel','1'],[str(build/mode/'water_runtime.exe'),str(report/(mode+'-proof.txt'))]]
        results=[]
        for index,command in enumerate(commands):
            run=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=180)
            (report/f'lane-{index}.log').write_text(run.stdout+run.stderr,encoding='utf-8');results.append({'command':command,'exit':run.returncode})
            self.assertEqual(run.returncode,0,run.stdout[-9000:]+run.stderr)
        self.assertEqual((report/'Debug-proof.txt').read_bytes(),(report/'Release-proof.txt').read_bytes())
        owners=[ROOT/'Engine/modules/core.context.ixx',ROOT/'Engine/src/epoch/epoch.engine_legacy.cpp',ROOT/'Engine/src/epoch/implementation/epoch.project_play_scene.inl',ROOT/'Engine/src/epoch/implementation/epoch.session_runtime.inl',*implementations,*selected.values(),Path(__file__)]
        (report/'result.json').write_text(json.dumps({'passed':True,'commands':results,'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in owners},'scope':'Actual Context accessor/member and central publication statement extracted verbatim, real project Water cache/marker helpers, real mesh/preview/simulation/scene modules. Native hooks and logging are recording ports. No window, GPU, model, global allocator fault, assertion or abort. Legacy launcher time exposure and physical rewind are not claimed.'},indent=2),encoding='utf-8')


if __name__=='__main__':unittest.main()
