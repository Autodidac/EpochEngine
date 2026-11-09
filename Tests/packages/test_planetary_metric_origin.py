"""Actual provider/codec regression for a far-orbit metric-frame overflow."""
from pathlib import Path
import hashlib
import json
import math
import os
import struct
import subprocess
import time
import unittest


ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / "Engine/dep/EpochEngineExtensions/packages/planetary_terrain"
OWNER = PACKAGE / "tools/planet.stream_export.cpp"

CONSUMER = r'''
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>
import asset.mesh_artifact;
import project.planetary_snapshot;
namespace mesh=epochengine::asset::mesh;
namespace patches=epochengine::project_planetary_snapshot;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return __LINE__;}}while(false)
int main(int argc,char** argv){
 const double nan=std::numeric_limits<double>::quiet_NaN();
 CHECK(!patches::metric_source_origin({nan,0,0},true));
 CHECK(!patches::metric_source_origin({0,1.00001e12,0},true));
 const std::array<double,3> near{1.25,-2.5,6371001.0};
 CHECK(patches::metric_source_origin(near,true)==patches::metric_source_origin(near,false));
 CHECK(*patches::metric_source_origin(near,true)==near);
 const std::array<double,3> far{-832702918.6151904,333123084.7457615,-1048498067.653896};
 CHECK(*patches::metric_source_origin(far,false)==far);
 const auto expected=patches::metric_source_origin(far,true);CHECK(expected);
 CHECK((*expected==std::array<double,3>{-500000000.0,333123084.7457615,-500000000.0}));
 CHECK(argc==2);const std::filesystem::path output{argv[1]};
 const mesh::Limits limits{1048576u,6291456u,32u*1024u*1024u};
 const auto artifact=mesh::read_artifact_file(output/"terrain.epoch_mesh",limits);CHECK(artifact);
 std::ifstream input{output/"terrain.epoch_planet_patch",std::ios::binary};
 const std::string bytes{std::istreambuf_iterator<char>{input},{}};CHECK(input&&!bytes.empty());
 const patches::Identity identity{artifact.artifact->key(),"sha256:"+std::string(64,'a'),std::string(64,'b'),17};
 patches::Limits policy;policy.artifact=limits;
 const auto metadata=patches::decode(std::as_bytes(std::span{bytes.data(),bytes.size()}),*artifact.artifact,identity,policy);
 CHECK(metadata&&metadata->water_radius_meters==6371000.0&&metadata->water_meters_to_artifact==1.0);
 CHECK(metadata->water_origin_meters==*expected&&metadata->cloud&&metadata->cloud->density);
 CHECK(metadata->cloud->body==0&&metadata->cloud->radius_meters==6371000.0);
 for(const auto& vertex:artifact.artifact->vertices()){
  double radius_squared{};
  for(unsigned axis=0;axis<3;++axis){
   CHECK(std::isfinite(vertex.position[axis])&&std::abs(vertex.position[axis])<=1e9f);
   const double body_position=double(vertex.position[axis])+(*expected)[axis];
   radius_squared+=body_position*body_position;
  }
  // Coarse root skirts intentionally extend well below the spherical surface.
  const auto radius=std::sqrt(radius_squared);CHECK(radius>0.9*6371000.0&&radius<6500000.0);
 }
 const auto encoded=mesh::encode_artifact(*artifact.artifact);
 const auto roundtrip=mesh::decode_artifact(encoded,limits);CHECK(roundtrip&&roundtrip.artifact->key()==artifact.artifact->key());
 CHECK(mesh::encode_artifact(*roundtrip.artifact)==encoded);
 CHECK(patches::encode(*metadata,*artifact.artifact,policy)==std::vector<std::byte>(
  reinterpret_cast<const std::byte*>(bytes.data()),reinterpret_cast<const std::byte*>(bytes.data()+bytes.size())));
 // Clamping origins must not loosen the independent mesh geometry validator.
 std::vector<mesh::Vertex> invalid(artifact.artifact->vertices().begin(),artifact.artifact->vertices().end());
 invalid[0].position[0]=1.1e9f;
 CHECK(mesh::build_artifact(invalid,artifact.artifact->indices(),limits).code==mesh::Code::invalid_geometry);
 return 0;
}
'''


class PlanetaryMetricOrigin(unittest.TestCase):
    def test_actual_wet_provider_far_origin_and_legacy_near_bytes(self):
        source = OWNER.read_text(encoding="utf-8-sig")
        needle = "{camera_origin.x,camera_origin.y,camera_origin.z},true);"
        self.assertEqual(source.count(needle), 1)
        legacy = source.replace(needle, "{camera_origin.x,camera_origin.y,camera_origin.z},false);")
        identity = hashlib.sha256((source + CONSUMER).encode()).hexdigest()[:12]
        proof = ROOT / "Artifacts/Reports/planet-metric-origin87-nov07" / identity
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "consumer.cpp").write_text(CONSUMER, encoding="utf-8")
        (proof / "planet.legacy_stream_export.cpp").write_text(legacy, encoding="utf-8")
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(PlanetMetricOrigin LANGUAGES CXX)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(EPOCH_PLANET_BUILD_MESH_EXPORT ON CACHE BOOL "" FORCE)
set(EPOCH_PLANET_BUILD_WATER ON CACHE BOOL "" FORCE)
set(EPOCH_ENGINE_SOURCE_ROOT "{ROOT.as_posix()}" CACHE PATH "" FORCE)
set(EPOCH_PLANET_WATER_SOURCE_ROOT "{(PACKAGE.parent/'fft_ocean').as_posix()}" CACHE PATH "" FORCE)
add_subdirectory("{PACKAGE.as_posix()}" provider EXCLUDE_FROM_ALL)
add_executable(metric_origin consumer.cpp)
target_link_libraries(metric_origin PRIVATE epoch_planet_mesh_format)
target_compile_features(metric_origin PRIVATE cxx_std_23)
add_executable(legacy_origin "{(PACKAGE/'tools/planet.mesh_export.cpp').as_posix()}" planet.legacy_stream_export.cpp)
target_include_directories(legacy_origin PRIVATE "{(PACKAGE/'tools').as_posix()}")
target_link_libraries(legacy_origin PRIVATE EpochEngineExtensions::PlanetaryCore epoch_extension_planetary_clouds epoch_planet_mesh_format epoch_extension_planetary_water)
target_compile_features(legacy_origin PRIVATE cxx_std_23)
target_compile_definitions(legacy_origin PRIVATE EPOCH_PLANET_HAS_WATER_EXTENSION=1)
include("{(ROOT/'Engine/cmake/epoch_task_scheduler.cmake').as_posix()}")
epoch_admit_task_scheduler(legacy_origin "{ROOT.as_posix()}")
add_custom_target(metric_origin_proof DEPENDS epoch_planet_mesh_export legacy_origin metric_origin)
''', encoding="utf-8")
        # MSVC appends the nested platform target and module scanner paths.
        # Keep those outputs and provider scratch data below its path limit.
        fixture = ROOT / "Artifacts/Tests/pmo" / identity
        temporary = fixture / "tmp"
        temporary.mkdir(parents=True, exist_ok=True)
        env = dict(os.environ, TEMP=str(temporary), TMP=str(temporary), TMPDIR=str(temporary),
                   MSBUILDDISABLENODEREUSE="1")
        build = fixture / "cmake"
        runs = []

        def run(phase, command, expected=0, timeout=180):
            completed = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, timeout=timeout)
            (proof / (phase + ".log")).write_text(completed.stdout + completed.stderr, encoding="utf-8")
            runs.append(dict(phase=phase, exit=completed.returncode, expected=expected, command=command))
            (proof / "result.json").write_text(json.dumps(dict(
                passed=False, runs=runs, owner_sha256=hashlib.sha256(OWNER.read_bytes()).hexdigest(),
                scope="Actual CPU provider, Water, cloud metadata and mesh/hierarchy codecs; no native acceptance"
            ), indent=2) + "\n", encoding="utf-8")
            self.assertEqual(completed.returncode, expected, (phase, (completed.stdout + completed.stderr)[-10000:]))

        configure = ["cmake", "-S", str(proof), "-B", str(build)]
        configure += (["-G", "Visual Studio 17 2022", "-A", "x64",
                       "-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false"]
                      if os.name == "nt" else ["-G", "Ninja Multi-Config"])
        run("configure", configure)
        metrics = []
        data_root = fixture / str(time.time_ns())
        for config in ("Debug", "Release"):
            command = ["cmake", "--build", str(build), "--config", config, "--parallel", "1", "--target",
                       "metric_origin_proof"]
            if os.name == "nt":
                command += ["--", "/nr:false", "/p:UseMultiToolTask=false", "/p:CL_MPCount=1"]
            run(config + "-build", command, timeout=300)
            suffix = ".exe" if os.name == "nt" else ""
            producer = build / "provider" / config / ("epoch_planet_mesh_export" + suffix)
            old = build / config / ("legacy_origin" + suffix)
            consumer = build / config / ("metric_origin" + suffix)
            data = data_root / config
            data.mkdir(parents=True, exist_ok=True)

            def args(binary, label, far=True):
                output = data / label
                camera = ["-1307.020748101068", "522.87409314983756", "-1645.7354695556364"] if far else ["-15", "6", "-19"]
                command = [str(binary), "--stream-step-preview", str(output), str(data / (label + "-cache")),
                           "1337", "16", "0", *camera, "761", "1.0499999523162842", "20", "384",
                           "134217728", "128", "17", "1", "--mesh-bytes", "33554432", "--seconds", "16.300000000000356",
                           "--patch-transport-v1", "sha256:" + "a" * 64 + ":" + "b" * 64,
                           "--metric-frame-v1", "--view-footprint-v1",
                           "0.60351672565136361,-0.24143707059294234,0.75991822112813923,1.6609724047306176,0.15015337598378095,0.97041647808737186,0.18906565721244756",
                           "--cloud-density-v1"]
                return output, command

            red, command = args(old, "red-far")
            run(config + "-red-far", command, expected=5)
            self.assertIn("stage=mesh validation\n", (red / "terrain.failure").read_text())
            self.assertIn("detail=1\n", (red / "terrain.failure").read_text())
            output, command = args(producer, "green-far")
            run(config + "-green-far", command)
            run(config + "-consumer", [str(consumer), str(output)])
            # Reconstruct the ground from the admitted artifact and compare it
            # with the actual provider cache, including its lowered root skirts.
            # The C++ consumer above has already decoded/validated both artifacts.
            encoded = (output / "terrain.epoch_mesh").read_bytes()
            metadata = (output / "terrain.epoch_planet_patch").read_bytes()
            self.assertEqual(struct.unpack_from("<II", metadata), (0x544C5050, 5))
            # Cloud v5 carries the underlying wet/local format at byte8.
            self.assertEqual(struct.unpack_from("<I", metadata, 8)[0], 3)
            count = struct.unpack_from("<I", metadata, 187)[0]
            origin = (-500000000.0, 333123084.7457615, -500000000.0)
            maximum_restoration_error = 0.0
            for patch in range(count):
                offset = 243 + patch * 92
                x, y, body, face, level = struct.unpack_from("<IIBBB", metadata, offset)
                first_vertex = struct.unpack_from("<I", metadata, offset + 55)[0]
                cached = (data / "green-far-cache" / f"tile-{body}-{face}-{level}-{x}-{y}.epoch_patch").read_bytes()
                anchor = struct.unpack_from("<3d", cached, 27)
                for vertex in range(17 * 17 + 4 * 16):
                    local = struct.unpack_from("<3f", cached, 115 + vertex * 52)
                    restored = struct.unpack_from("<3f", encoded, 48 + (first_vertex + vertex) * 44)
                    error = math.dist([anchor[axis] + local[axis] for axis in range(3)],
                                      [restored[axis] + origin[axis] for axis in range(3)])
                    maximum_restoration_error = max(maximum_restoration_error, error)
            self.assertLessEqual(maximum_restoration_error, math.sqrt(3) * 16.0)
            receipt = dict(line.split("=", 1) for line in (output / "terrain.stream").read_text().splitlines()[1:])
            self.assertEqual(receipt["snapshot_ready"], "1")
            self.assertEqual(receipt["pending"], "0")
            self.assertEqual(receipt["meters_to_preview_units"], "1")
            self.assertEqual(receipt["ocean_surface"], "1")
            old_near, command = args(old, "legacy-near", far=False)
            run(config + "-legacy-near", command)
            new_near, command = args(producer, "bounded-near", far=False)
            run(config + "-bounded-near", command)
            for name in ("terrain.epoch_mesh", "terrain.epoch_planet_patch", "terrain.stream", "terrain.preview.json", "terrain.frame"):
                if (old_near / name).exists():
                    self.assertEqual((old_near / name).read_bytes(), (new_near / name).read_bytes(), name)
            metrics.append(dict(configuration=config, exact_failure_reproduced=True, far_snapshot_admitted=True,
                                metric_units_unchanged=True, near_geometry_metadata_bytes_unchanged=True,
                                independent_geometry_range_refusal=True,
                                ground_restoration_maximum_error_meters=maximum_restoration_error, receipt=receipt))
        result = json.loads((proof / "result.json").read_text())
        result.update(passed=True, measurements=metrics, data_root=str(data_root))
        (proof / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
