"""Native camera-relative spray admission, flight and deterministic timing."""
from pathlib import Path
import os
import re
import subprocess
import unittest

from Tests.support.cpp import configure_arguments, executable


ROOT = Path(__file__).resolve().parents[2]
CONSUMER = r'''
#include <array>
#include <cmath>
#include <fstream>
#include <memory>
import asset.mesh_artifact;
import render.preview_grid;

using namespace epochengine;
using namespace previewgrid;

int main(int argc,char** argv)
{
    if(argc!=2)return 1;
    using MeshVertex=asset::mesh::Vertex;
    const std::array triangle{MeshVertex{{-1,0,-2}},MeshVertex{{1,0,-2}},MeshVertex{{0,0,-4}}};
    const std::array<std::uint32_t,3> indices{0,1,2};
    const auto mesh=asset::mesh::build_artifact(triangle,indices);
    if(!mesh)return 2;
    auto water=std::make_shared<asset::mesh::SphericalWaterAppearance>();
    water->artifact_key=mesh.artifact->key();
    water->ranges.push_back({0,3,1.0f});
    for(unsigned i=0;i<48u;++i)
        water->spray_sites[water->spray_count++]={{float(i%6u)*0.24f-0.6f,0.12f,-3.0f-float(i/6u)*0.10f},
            0x77724f19ull*(i+1u),0.8f};
    auto solids=std::make_shared<SolidDrawFrame>();
    solids->water_detail_ranges.push_back({0,3});
    PublishedSceneFrame scene;
    scene.camera.eye={0,0.8f,1.6f};
    scene.camera.target={0,0.55f,-4.0f};
    scene.camera.fovRadians=1.05f;
    scene.solids=solids;
    ObjectMarker marker;
    marker.primitive=ObjectPreviewPrimitive::IndexedMesh;
    marker.mesh=mesh.artifact;
    marker.sphericalWater=water;
    marker.waterDetail=SphericalWaterView{{0,-6371000,0},6371000,1,0};
    scene.markers.push_back(marker);
    SceneFrameRequest original;
    original.markers.push_back(marker);
    auto updated=original;
    auto changed=std::make_shared<asset::mesh::SphericalWaterAppearance>(*water);
    updated.markers.front().sphericalWater=changed;
    if(!same_scene_frame_request(original,updated))return 16;
    changed->spray_sites.front().strength=0.2f;
    if(same_scene_frame_request(original,updated))return 17;
    changed->spray_sites.front()=water->spray_sites.front();
    --changed->spray_count;
    if(same_scene_frame_request(original,updated))return 18;
    scene.water_detail_seconds=0.25;
    constexpr auto budget=ShorelineSprayBatch::desktop_portable_budget;
    const auto first=shoreline_spray_for(scene,360,budget);
    if(first.size!=budget||shoreline_spray_for(scene,360,0).size)return 3;
    if(shoreline_spray_for(scene,360,3).size!=3u)return 4;
    const auto repeat=shoreline_spray_for(scene,360,budget);
    const auto near=[](float a,float b){return std::abs(a-b)<0.00002f;};
    const auto same=[&](Vec3 a,Vec3 b){return near(a.x,b.x)&&near(a.y,b.y)&&near(a.z,b.z);};
    for(std::size_t i=0;i<first.size;++i)
    {
        const auto p=first.points[i];
        if(p.position.y<=0.12f||p.color.x<1.5f||p.color.x>22.0f||p.color.y<=0||p.color.y>0.48f
            ||!same(p.position,repeat.points[i].position)||!same(p.color,repeat.points[i].color))return 5;
    }
    auto rebased=scene;
    rebased.render_origin={8,0,12};
    const Vec3 shift{8,0,12};
    rebased.camera.eye=subtract(scene.camera.eye,shift);
    rebased.camera.target=subtract(scene.camera.target,shift);
    rebased.markers[0].position=subtract(marker.position,shift);
    const auto shifted=shoreline_spray_for(rebased,360,budget);
    if(shifted.size!=first.size)return 6;
    for(std::size_t i=0;i<first.size;++i)
        if(!same(first.points[i].position,add(shifted.points[i].position,shift))
            ||!same(first.points[i].color,shifted.points[i].color))return 7;
    auto far=scene;
    far.camera.eye={0,0.8f,60};
    if(shoreline_spray_for(far,360,budget).size)return 8;
    auto orbit=scene;
    orbit.camera.eye.y=130000;
    if(shoreline_spray_for(orbit,360,budget).size)return 9;
    auto inspect=scene;
    inspect.markers[0].wireframe=true;
    if(shoreline_spray_for(inspect,360,budget).size)return 10;
    scene.water_detail_seconds=0.65;
    const auto motion=shoreline_spray_for(scene,360,budget);
    if(!motion.size||same(first.points[0].position,motion.points[0].position))return 11;
    scene.water_detail_seconds=0.25;
    const auto seek=shoreline_spray_for(scene,360,budget);
    if(seek.size!=first.size||!same(first.points[0].position,seek.points[0].position))return 12;
    std::ofstream out(argv[1]);
    for(const auto* sample:{&first,&motion})
    {
        out<<sample->size<<'\n';
        for(std::size_t i=0;i<sample->size;++i)
        {
            const auto p=sample->points[i];
            out<<p.position.x<<' '<<p.position.y<<' '<<p.position.z<<' '
                <<p.color.x<<' '<<p.color.y<<' '<<p.color.z<<'\n';
        }
    }
    return out?0:13;
}
'''


class ShorelineSpray(unittest.TestCase):
    def test_native_world_space_spray_camera_and_time(self):
        folder = ROOT / 'Artifacts/Reports/shoreline-spray'
        folder.mkdir(parents=True, exist_ok=True)
        (folder / 'consumer.cpp').write_text(CONSUMER, encoding='utf-8')
        module_roots = [ROOT / 'Engine/modules', ROOT / 'Engine/dep/EpochPlatformEngine/modules']
        owners = {}
        for directory in module_roots:
            for path in directory.rglob('*.ixx'):
                if match := re.search(r'^export module\s+([\w.:]+);', path.read_text(encoding='utf-8-sig'), re.M):
                    self.assertNotIn(match[1], owners)
                    owners[match[1]] = path
        closure = set()
        pending = ['render.preview_grid', 'authoring.texture_detail']
        while pending:
            module = pending.pop()
            if module in closure:
                continue
            closure.add(module)
            pending += re.findall(r'^(?:export\s+)?import\s+([\w.:]+);', owners[module].read_text(encoding='utf-8-sig'), re.M)
        implementations = [ROOT / name for name in (
            'Engine/src/render/render.preview_camera.cpp', 'Engine/src/render/render.preview_lighting.cpp',
            'Engine/src/assets/texture/asset.texture_artifact.cpp',
            'Engine/src/authoring/texture/authoring.texture_document.cpp',
            'Engine/src/authoring/texture/authoring.texture_snapshot_codec.cpp',
            'Engine/src/authoring/texture/authoring.texture_residency.cpp')]
        quoted = lambda paths: ' '.join(f'"{path.as_posix()}"' for path in paths)
        (folder / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(ShorelineSpray LANGUAGES CXX)
add_executable(spray_contract consumer.cpp {quoted(implementations)})
target_compile_features(spray_contract PRIVATE cxx_std_23)
target_include_directories(spray_contract PRIVATE {quoted([ROOT/'Engine', ROOT/'Engine/include', ROOT/'Engine/dep/EpochPlatformEngine/include'])})
target_sources(spray_contract PRIVATE FILE_SET CXX_MODULES BASE_DIRS {quoted(module_roots)}
    FILES {quoted(owners[name] for name in sorted(closure))})
''', encoding='utf-8')
        build = Path(os.environ.get('EPOCH_SPRAY_TEST_BUILD', ROOT / 'Artifacts/Tests/spray'))
        commands = [configure_arguments(folder, build),
                    ['cmake', '--build', str(build), '--config', 'Release', '--parallel', '1'],
                    [str(executable(build, 'Release', 'spray_contract')), str(folder / 'particles.txt')]]
        for index, command in enumerate(commands):
            result = subprocess.run(command, capture_output=True, text=True, timeout=180)
            (folder / f'lane-{index}.log').write_text(result.stdout + result.stderr, encoding='utf-8')
            self.assertEqual(result.returncode, 0, result.stdout[-3500:] + result.stderr[-3500:])
