"""Actual camera module: offscreen eye crossings must not collapse depth precision."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]


def fixture(baseline=False):
    aspect = "" if baseline else ", aspect"
    return r'''
#include <array>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <limits>
#include <vector>
import render.preview_grid;
import asset.mesh_artifact;
namespace p=epochengine::previewgrid;
namespace m=epochengine::asset::mesh;
#define CHECK(...) do{if(!(__VA_ARGS__)){std::fprintf(stderr,"depth frustum line %d\n",__LINE__);return 91;}}while(false)
auto mesh(std::initializer_list<std::array<float,3>> points){
 std::vector<m::Vertex> vertices;std::vector<std::uint32_t> indices;
 for(const auto point:points){m::Vertex v;v.position=point;vertices.push_back(v);indices.push_back(static_cast<std::uint32_t>(indices.size()));}
 return m::build_artifact(vertices,indices).artifact;
}
int main(){
 p::Camera view{.eye={},.target={0,0,1},.up={0,1,0},.fovRadians=1.0f,.nearPlane=900,.farPlane=42813184};
 p::ObjectMarker marker{.primitive=p::ObjectPreviewPrimitive::IndexedMesh};
 const auto fit=[&](double aspect=1.5){return p::fit_visible_depth_range(view,std::span{&marker,1u}ASPECT);};
 // A distant visible surface plus an offscreen triangle crossing the eye
 // plane reproduces the live 0.05 / 42 Mm interval. All four side directions
 // are checked, including their boundary intersections after clipping.
 for(unsigned axis=0;axis<2;++axis)for(float sign:{-1.0f,1.0f}){
  std::array<float,3>a{0,0,-100},b{0,0,100},c{0,10,100};
  a[axis]=sign*10000;b[axis]=sign*10010;c[axis]=sign*10000;
  if(axis==1)c[0]=10;
  marker.mesh=mesh({{-100,-100,9000},{100,-100,9000},{0,100,9000},a,b,c,
    {-100,-100,40000000},{100,-100,40000000},{0,100,40000000}});CHECK(marker.mesh);
  const auto value=fit();
  CHECK(value.nearPlane==2250&&value.farPlane>=40000000);
 }
 // With no aspect evidence, old callers remain conservative. Invalid
 // viewport/FOV input cannot make a more aggressive near plane.
 const auto legacy=p::fit_visible_depth_range(view,std::span{&marker,1u});CHECK(legacy.nearPlane==.05f);
 CHECK(fit(0).nearPlane==.05f&&fit(-1).nearPlane==.05f);
 CHECK(fit(std::numeric_limits<double>::quiet_NaN()).nearPlane==.05f);
 CHECK(fit(std::numeric_limits<double>::infinity()).nearPlane==.05f);
 const auto projection=view.fovRadians;view.fovRadians=std::numeric_limits<float>::quiet_NaN();
 CHECK(fit().nearPlane==.05f);view.fovRadians=projection;
 // A real surface through the eye remains conservative. Side clipping must
 // retain a triangle intersecting the viewport even with all vertices outside.
 marker.mesh=mesh({{-1,0,-1},{1,0,-1},{0,0,1},{-10,-10,9000},{10,-10,9000},{0,10,9000}});CHECK(marker.mesh);
 CHECK(fit().nearPlane==.05f);
 marker.mesh=mesh({{-100,-100,-1},{100,-100,1000},{0,100,1000}});CHECK(marker.mesh);
 const auto intersection=fit();CHECK(intersection.nearPlane>.05f&&intersection.nearPlane<250);
 // Merely touching a side plane or the eye must not overflow fixed clipping
 // storage or discard a visible surface.
 marker.mesh=mesh({{0,0,0},{10,-10,1000},{-10,10,1000}});CHECK(marker.mesh);
 CHECK(fit().nearPlane==.05f);
 // A crossing just outside a narrow view becomes relevant when resized wide.
 marker.mesh=mesh({{100,-10,-100},{100,10,1000},{110,0,1000},
   {-10,-10,9000},{10,-10,9000},{0,10,9000}});CHECK(marker.mesh);
 const auto narrow=fit(.5),wide=fit(4);CHECK(narrow.nearPlane>wide.nearPlane&&wide.nearPlane>.05f);
 // Nonuniform scale and a precise translated/rotated marker use the same
 // rendered coordinates. Large origin values must cancel before projection.
 marker.mesh=mesh({{-50,-50,4500},{50,-50,4500},{0,50,4500},
   {10000,0,-100},{10010,0,100},{10000,10,100}});CHECK(marker.mesh);
 marker.scale={2,3,2};CHECK(fit().nearPlane==2250);
 marker.preciseTransform=p::make_precise_mesh_transform({90000000.25,-40000000.5,123456789.25},{2,3,2},{0,90,0});CHECK(marker.preciseTransform);
 view.navigationValid=true;view.navigation.eye=marker.preciseTransform->position;view.navigation.forward={1,0,0};
 view.eye={};view.target={1,0,0};CHECK(std::abs(fit().nearPlane-2250)<.01);
 auto invalid=*marker.preciseTransform;invalid.position[0]=std::numeric_limits<double>::quiet_NaN();
 marker.preciseTransform=invalid;CHECK(fit().nearPlane==.05f);
 // Solid terrain and wire water both constrain the same depth fit; neither
 // surface can erase a nearer in-view surface owned by the other.
 view.navigationValid=false;view.eye={};view.target={0,0,1};marker.preciseTransform.reset();marker.scale={1,1,1};
 marker.mesh=mesh({{-1,-1,1000},{1,-1,1000},{0,1,1000}});CHECK(marker.mesh);
 auto water=marker;water.mesh=mesh({{-1,-1,1100},{1,-1,1100},{0,1,1100}});water.wireframe=true;CHECK(water.mesh);
 const std::array wet{marker,water};CHECK(p::fit_visible_depth_range(view,wet,1.5).nearPlane==250);
 // Time only the actual depth owner, outside mesh admission/encoding. This is
 // advisory CPU evidence for a large mixed planet packet, never native FPS.
 const auto basis=mesh({{-100,-100,9000},{100,-100,9000},{0,100,9000},
  {10000,0,-100},{10010,0,100},{10000,10,100},
  {-100,-100,40000000},{100,-100,40000000},{0,100,40000000}});CHECK(basis);
 std::vector<std::uint32_t> many;many.reserve(262144u*3u);
 for(unsigned i=0;i<262144u;++i){const unsigned base=i%257u==0u?3u:(i%2u?0u:6u);many.insert(many.end(),{base,base+1u,base+2u});}
 const auto large=m::build_artifact(basis->vertices(),many,{1048576u,6291456u,64ull*1024*1024});CHECK(large);marker.mesh=large.artifact;
 const auto started=std::chrono::steady_clock::now();double nearTotal{};
 for(unsigned sample=0;sample<3u;++sample)nearTotal+=fit().nearPlane;
 CHECK(nearTotal==6750);
 std::printf("262144_triangles_mean_fit_ms=%.6f\n",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()/3.0);
 p::SceneFrameRequest request;request.depth_fit_aspect=1.5;auto resized=request;
 CHECK(p::same_scene_frame_request(request,resized));resized.depth_fit_aspect=2;
 CHECK(!p::same_scene_frame_request(request,resized));
 std::puts("offscreen_floor_reproduced; four_sides; intersections; true_crossings; transforms; water_wire; resize_identity");
 return 0;
}
'''.replace("ASPECT", aspect)


class PreviewDepthFrustum(unittest.TestCase):
    def test_actual_camera_module_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC module consumer lane")
        baseline = os.environ.get("EPOCH_DEPTH_FRUSTUM_BASELINE") == "1"
        report = ROOT / "Artifacts/Reports/planet-depth-frustum82-nov07" / ("red" if baseline else "green")
        report.mkdir(parents=True, exist_ok=True)
        source = report / "depth.frustum_contract.cpp"
        code = fixture(baseline)
        # The baseline has no new request field; its first behavioral check is
        # the regression, before request identity checks are reachable.
        if baseline:
            code = code[:code.index(" p::SceneFrameRequest request;")] + " return 0;\n}\n"
            code = code.replace("p::fit_visible_depth_range(view,wet,1.5)", "p::fit_visible_depth_range(view,wet)")
        source.write_text(code, encoding="utf-8")
        module_map = {}
        for owner in (ROOT / "Engine/modules", ROOT / "Engine/dep/EpochPlatformEngine/modules"):
            for path in owner.glob("*.ixx"):
                match = re.search(r"^export module\s+([\w.:]+);", path.read_text(encoding="utf-8-sig"), re.M)
                if match:
                    self.assertNotIn(match.group(1), module_map)
                    module_map[match.group(1)] = path
        camera_source = ROOT / "Engine/src/render/render.preview_camera.cpp"
        if baseline:
            module_map["render.preview_grid"] = report.parent / "baseline/render.preview_grid.ixx"
            camera_source = report.parent / "baseline/render.preview_camera.cpp"
        pending, closure = ["render.preview_grid", "authoring.texture_detail"], set()
        while pending:
            name = pending.pop()
            if name in closure:
                continue
            self.assertIn(name, module_map)
            closure.add(name)
            pending.extend(re.findall(r"^(?:export\s+)?import\s+([\w.:]+);", module_map[name].read_text(encoding="utf-8-sig"), re.M))
        modules = " ".join('"' + module_map[name].as_posix() + '"' for name in sorted(closure))
        roots = " ".join('"' + path.as_posix() + '"' for path in (ROOT / "Engine/modules", ROOT / "Engine/dep/EpochPlatformEngine/modules", report.parent / "baseline"))
        lighting_source = '' if baseline else f'"{(ROOT / "Engine/src/render/render.preview_lighting.cpp").as_posix()}"'
        (report / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(PreviewDepthFrustum LANGUAGES CXX)
add_executable(depth_contract "{source.as_posix()}" "{camera_source.as_posix()}" {lighting_source} "{(ROOT / 'Engine/src/assets/texture/asset.texture_artifact.cpp').as_posix()}" "{(ROOT / 'Engine/src/authoring/texture/authoring.texture_document.cpp').as_posix()}")
target_compile_features(depth_contract PRIVATE cxx_std_23)
target_sources(depth_contract PRIVATE FILE_SET CXX_MODULES BASE_DIRS {roots} FILES {modules})
target_include_directories(depth_contract PRIVATE "{(ROOT / 'Engine').as_posix()}" "{(ROOT / 'Engine/include').as_posix()}" "{(ROOT / 'Engine/src').as_posix()}")
if(MSVC)
 target_compile_options(depth_contract PRIVATE /utf-8 /EHsc)
endif()
''', encoding="utf-8")
        build = report / "cmake"
        commands = [("configure", ["cmake", "-S", str(report), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"])]
        for configuration in ("Debug", "Release"):
            commands.extend([(configuration + "-build", ["cmake", "--build", str(build), "--config", configuration, "--parallel", "1", "--", "/nr:false", "/p:UseMultiToolTask=false", "/p:CL_MPCount=1"]),
                             (configuration + "-contract", [str(build / configuration / "depth_contract.exe")])])
        completed = []
        temporary = report.parent / "compiler-temp"
        temporary.mkdir(exist_ok=True)
        environment = dict(os.environ, MSBUILDDISABLENODEREUSE="1", TEMP=str(temporary), TMP=str(temporary), TMPDIR=str(temporary))
        for phase, command in commands:
            started = time.monotonic()
            result = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True, text=True, timeout=180)
            (report / (phase + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
            completed.append(dict(phase=phase, exit=result.returncode, wall_seconds=time.monotonic() - started, command=command))
            (report / "result.json").write_text(json.dumps(dict(baseline=baseline, source_sha256=hashlib.sha256(camera_source.read_bytes()).hexdigest(), module_count=len(closure), completed=completed, native=False), indent=2) + "\n", encoding="utf-8")
            if baseline and phase.endswith("-contract"):
                self.assertEqual(result.returncode, 91, result.stdout + result.stderr)
            else:
                self.assertEqual(result.returncode, 0, (result.stdout + result.stderr)[-12000:])


if __name__ == "__main__":
    unittest.main()
