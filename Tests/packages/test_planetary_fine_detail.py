"""Original metric-world LOD planning and actual Engine/provider transport; CPU only."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / "Engine/dep/EpochEngineExtensions/packages/planetary_terrain"

CODE = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
#include "planet.stream_cache.hpp"
import planet.terrain;
import planet.streaming;
import planet.coverage;
import project.planetary;
import project.planetary_snapshot;
import asset.mesh_artifact;
import asset.cloud_volume;
import asset.texture_artifact;
import editor.task_scheduler;
namespace core=epoch::extensions::planetary_terrain::core;
namespace stream=epoch::extensions::planetary_terrain::streaming;
namespace planet=epochengine::project_planetary;
namespace artifact=epochengine::asset::mesh;
namespace patches=epochengine::project_planetary_snapshot;
namespace project_planetary_snapshot=epochengine::project_planetary_snapshot;
namespace coverage=epoch::extensions::planetary_terrain::coverage;
namespace tasks=epochengine::editor_tasks;
namespace mesh=artifact;
namespace fs=std::filesystem;
namespace cache=planet_stream_cache;
void check(bool value,int line){if(!value){std::fprintf(stderr,"Fine-detail contract failed line %d\n",line);std::exit(86);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),__LINE__)
READ_SMALL
SOURCE_CAPABILITY
EXPORT_OPTIONS
WRITE_FRAME
WORKER_ACTIVITY
WRITE_SNAPSHOT
planet::StreamResult parse_receipt(planet::Settings settings,planet::StreamView view,std::string receipt,bool metricTransport=false,bool boundedMetricOrigin=false){
 planet::StreamResult result;auto& operation=result.operation;const bool customLod=planet::requires_lod_controls(settings);
 const bool customSize=planet::requires_size_controls(settings);
 const bool patchTransport=metricTransport;
 const bool viewTransport=metricTransport&&view.forward.has_value();
 const bool footprintTransport=viewTransport&&view.up.has_value();
 PARSE_RECEIPT
 operation.ok=true;return result;
}
int actual_conversion(const stream::State& state,core::TileKey key,std::vector<artifact::Vertex>& vertices,std::vector<std::uint32_t>& indices){
 const std::array cut{key};const double scale=1.0;
 const auto artifact_origin=state.request.camera-state.world.center(state.request.body_index,state.request.seconds);
 const std::vector exported{key};patches::Metadata metadata;const std::string_view patch_identity{};
 const auto limits=planet::artifact_limits(*planet::with_detail_preset({},planet::DetailPreset::fine));
 tasks::Scheduler scheduler{{.worker_count=4u,.retained_finished_nodes=32u}};
 WorkerActivity activity;
 const auto fail=[](std::string_view,std::size_t=0u,std::size_t=0u,int=0){return 5;};
 CONVERSION
 return 0;
}
bool options(std::vector<std::string> args,double& pixels,double& hysteresis,bool custom=true){
 std::vector<char*> argv;for(auto& arg:args)argv.push_back(arg.data());int argc=int(argv.size());
 std::uint64_t bytes=4ull*1024*1024;std::optional<double> seconds;
 const bool ok=planet_export_options(argc,argv.data(),bytes,seconds,custom?&pixels:nullptr,custom?&hysteresis:nullptr);
 return ok&&argc==1;
}
int main(int argc,char**argv){
 CHECK(argc==4);const auto output=fs::path{argv[1]};fs::create_directories(output);
 CHECK(source_supports_lod_controls(argv[2]));
 const auto legacy=output/"legacy";fs::create_directories(legacy/"tools");
 {std::ofstream old(legacy/"tools/planet.stream_export.cpp");old<<"epoch.planet.stream.v3 --stream-step-preview";}
 CHECK(!source_supports_lod_controls(legacy));
 {std::ofstream huge(legacy/"tools/planet.stream_export.cpp");huge<<std::string(65537,'x')<<"epoch.planet.stream.v4 --patch-pixels --lod-hysteresis";}
 CHECK(!source_supports_lod_controls(legacy));
 double pixels=210,hysteresis=.15;
 CHECK(options({"test","--mesh-bytes","33554432","--patch-pixels","170","--seconds","0","--lod-hysteresis","0.18"},pixels,hysteresis));
 CHECK(pixels==170&&hysteresis==.18);
 for(const auto value:{"31","4097","nan","inf","170junk"})
  CHECK(!options({"test","--patch-pixels",value},pixels,hysteresis));
 for(const auto value:{"-0.1","0.46","nan","inf"})
  CHECK(!options({"test","--lod-hysteresis",value},pixels,hysteresis));
 CHECK(!options({"test","--patch-pixels","170","--patch-pixels","210"},pixels,hysteresis));
 CHECK(!options({"test","--patch-pixels","170"},pixels,hysteresis,false));
 CHECK(options({"test"},pixels,hysteresis,false));
 auto fine=*planet::with_detail_preset({},planet::DetailPreset::fine);fine.maximum_level=20;
 const auto balanced=*planet::with_detail_preset({},planet::DetailPreset::detailed);
 const auto fast=*planet::with_detail_preset({},planet::DetailPreset::fast);
 stream::State state{1337};CHECK(state.world.body(0).radius==6371000.0&&state.world.body(5).radius==1737000.0);
 auto direction=core::cube_direction(0,.13,.09);bool land{};
 for(std::uint8_t face=0;face<6&&!land;++face)for(double u:{-.75,-.25,.25,.75})for(double v:{-.75,-.25,.25,.75})
  if(!land){const auto candidate=core::cube_direction(face,u,v);
   if(core::sample_height(state.world.body(0),candidate)>100.0){direction=candidate;land=true;}}
 CHECK(land);
 const auto camera=state.world.center(0,0)+direction*(state.world.ground_radius(0,direction)+1.8);
 auto request_for=[&](const planet::Settings& settings,bool wet=false){
  stream::Request request;request.camera=camera;request.body_index=0;request.viewport_height=1080;
  request.cells_per_tile=settings.cells_per_tile;request.ocean_surface=wet;
  request.lod={static_cast<std::uint8_t>(settings.maximum_level),settings.maximum_leaves,settings.patch_pixels,settings.lod_hysteresis};
  request.maximum_bytes=std::uint64_t{settings.resident_megabytes}*1024u*1024u;
  request.maximum_resident_patches=4096;request.maximum_in_flight=4u;return request;
 };
 auto maximum=[](const auto& plan){std::uint8_t level{};for(const auto key:plan.leaves)level=std::max(level,key.level);return level;};
 CHECK(stream::update(state,request_for(balanced))==stream::Status::ready);
 const auto balancedLevel=maximum(state.plan);CHECK(balancedLevel==balanced.maximum_level);
 CHECK(stream::update(state,request_for(fine))==stream::Status::ready);
 const auto fineLevel=maximum(state.plan);CHECK(fineLevel==20&&state.plan.leaves.size()<=576);
 const auto fineLeaves=state.plan.leaves.size();
 CHECK(state.resident.empty()); // Planning is not a claim of generated/resident terrain.
 auto wet=request_for(fine,true);stream::State wetState{1337};CHECK(stream::update(wetState,wet)==stream::Status::ready);
 const auto wetRequired=stream::detail::overhead+wetState.plan.needed.size()*stream::detail::retained_bound(16,true)
  +wet.maximum_in_flight*stream::detail::scratch_bound(16);
 CHECK(wetRequired<=wet.maximum_bytes);
 auto insufficient=wet;insufficient.maximum_bytes=4ull*1024*1024;
 stream::State refused{1337};CHECK(stream::update(refused,insufficient)==stream::Status::capacity_exceeded&&refused.resident.empty());
 std::stop_source cancelled;cancelled.request_stop();CHECK(stream::update(refused,wet,cancelled.get_token())==stream::Status::cancelled);
 CHECK(stream::update(state,request_for(fast))==stream::Status::ready);CHECK(maximum(state.plan)==17);
 CHECK(stream::update(state,request_for(fine))==stream::Status::ready);
 const auto tile=*std::max_element(state.plan.leaves.begin(),state.plan.leaves.end(),[](auto a,auto b){return a.level<b.level;});
 CHECK(tile.level==20);
 auto mesh=core::build_tile(state.world.body(0),tile,{.cells=16,.vegetation=false,.skirts=true});
 CHECK(!mesh.vertices.empty());state.resident.push_back({tile,std::move(mesh)});
 std::vector<artifact::Vertex> vertices;std::vector<std::uint32_t> indices;
 CHECK(actual_conversion(state,tile,vertices,indices)==0);
 const auto converted=artifact::build_artifact(vertices,indices,planet::artifact_limits(fine));CHECK(converted);
 CHECK(artifact::decode_artifact(artifact::encode_artifact(*converted.artifact),planet::artifact_limits(fine)));
 const auto& native=state.resident.front().mesh;const double scale=1.0;
 const auto artifact_origin=state.request.camera-state.world.center(0,state.request.seconds);
 double maxError{};
 for(std::size_t i=0;i<vertices.size();++i){const auto exact=native.anchor+core::DVec3{native.vertices[i].position};
  const auto& encoded=vertices[i].position;const auto back=artifact_origin+core::DVec3{encoded[0]/scale,encoded[1]/scale,encoded[2]/scale};
  maxError=std::max(maxError,core::length(back-exact));}
 CHECK(std::isfinite(maxError)&&maxError<0.001);
 const auto a=native.anchor+core::DVec3{native.vertices[0].position},b=native.anchor+core::DVec3{native.vertices[1].position};
 const double spacing=core::length(b-a);CHECK(spacing>0&&spacing<1);
 // Actual producer receipt, then the exact Engine receipt-parser block.
 const auto generated=output/"custom";fs::create_directories(generated);
 tasks::Scheduler snapshot_scheduler{{.worker_count=4u,.retained_finished_nodes=32u}};
 WorkerActivity activity;
 CHECK(write_snapshot(generated,state,17,1,0,planet::artifact_limits(fine),{},"sha256:"+std::string(64,'a')+":"+std::string(64,'b'),snapshot_scheduler,activity,true)==0);
 auto receipt=read_small(generated/"terrain.stream",4096);CHECK(receipt.starts_with("epoch.planet.stream.v7\n"));
 planet::StreamView view;view.request_id=17;
 const auto relative=(camera-state.world.center(0,0))*(10.0/state.world.body(0).radius);view.camera={relative.x,relative.y,relative.z};
 CHECK(parse_receipt(fine,view,receipt,true).operation.ok);
 // The exact host parser preserves legacy origins and accepts bounded origins
 // only for an admitted provider declaring the new metric-origin capability.
 auto farView=view;farView.camera={-1307.020748101068,522.87409314983756,-1645.7354695556364};
 const std::array<double,3> farCamera{farView.camera[0]*(6371000.0/10.0),farView.camera[1]*(6371000.0/10.0),farView.camera[2]*(6371000.0/10.0)};
 const auto bounded=*patches::metric_source_origin(farCamera,true);
 const auto replaceOrigin=[](std::string value,const std::array<double,3>& origin){
  for(unsigned axis=0;axis<3;++axis){const auto key=std::string{"source_origin_"}+char('x'+axis)+"_meters=";
   const auto begin=value.find(key)+key.size(),end=value.find('\n',begin);value.replace(begin,end-begin,std::to_string(origin[axis]));}
  return value;
 };
 const auto boundedReceipt=replaceOrigin(receipt,bounded),legacyReceipt=replaceOrigin(receipt,farCamera);
 CHECK(parse_receipt(fine,farView,boundedReceipt,true,true).operation.ok);
 CHECK(!parse_receipt(fine,farView,boundedReceipt,true,false).operation.ok);
 CHECK(parse_receipt(fine,farView,legacyReceipt,true,false).operation.ok);
 CHECK(!parse_receipt(fine,farView,legacyReceipt,true,true).operation.ok);
 auto forged=bounded;forged[0]+=10000.;CHECK(!parse_receipt(fine,farView,replaceOrigin(receipt,forged),true,true).operation.ok);
 auto foreignView=view;foreignView.camera[0]+=1;CHECK(!parse_receipt(fine,foreignView,receipt,true).operation.ok);
 auto wrong=receipt;wrong.replace(wrong.find("patch_pixels=170"),16,"patch_pixels=171");CHECK(!parse_receipt(fine,view,wrong,true).operation.ok);
 CHECK(!parse_receipt(fine,view,receipt+"patch_pixels=170\n",true).operation.ok);
 auto another=fine;another.lod_hysteresis=.18;CHECK(!parse_receipt(another,view,receipt,true).operation.ok);
 CHECK(!parse_receipt({},view,receipt).operation.ok);
 // Actual directional receipt and exact host parser bind the admitted view.
 const auto directed=output/"view";fs::create_directories(directed);
 state.request.view=core::TerrainView{{-direction.x,-direction.y,-direction.z},16.0/9.0};
 view.forward=std::array{-direction.x,-direction.y,-direction.z};
 CHECK(write_snapshot(directed,state,17,1,0,planet::artifact_limits(fine),{},"sha256:"+std::string(64,'a')+":"+std::string(64,'b'),snapshot_scheduler,activity,true)==0);
 const auto directedReceipt=read_small(directed/"terrain.stream",4096);
 CHECK(directedReceipt.starts_with("epoch.planet.stream.v8\n"));
 CHECK(parse_receipt(fine,view,directedReceipt,true).operation.ok);
 auto wrongDirection=view;(*wrongDirection.forward)[0]+=.125;
 CHECK(!parse_receipt(fine,wrongDirection,directedReceipt,true).operation.ok);
 auto wrongAspect=view;wrongAspect.aspect_ratio=1.0;
 CHECK(!parse_receipt(fine,wrongAspect,directedReceipt,true).operation.ok);
 const auto footprint=output/"footprint";fs::create_directories(footprint);
 const auto unscaledUp=core::DVec3{-direction.y,direction.x,0.};
 const auto up=unscaledUp/core::length(unscaledUp);
 state.request.view->up=up;view.up=std::array{up.x,up.y,up.z};
 CHECK(planet::valid_stream_up(view));
 CHECK(write_snapshot(footprint,state,17,1,0,planet::artifact_limits(fine),{},"sha256:"+std::string(64,'a')+":"+std::string(64,'b'),snapshot_scheduler,activity,true)==0);
 const auto footprintReceipt=read_small(footprint/"terrain.stream",4096);
 CHECK(footprintReceipt.starts_with("epoch.planet.stream.v9\n"));
 CHECK(parse_receipt(fine,view,footprintReceipt,true).operation.ok);
 auto wrongUp=view;(*wrongUp.up)[0]+=.125;
 CHECK(!parse_receipt(fine,wrongUp,footprintReceipt,true).operation.ok);
 CHECK(!parse_receipt(fine,view,directedReceipt,true).operation.ok);
 auto invalidUp=view;invalidUp.up=invalidUp.forward;CHECK(!planet::valid_stream_up(invalidUp));
 invalidUp.up=std::array{0.,0.,0.};CHECK(!planet::stream_view_reusable(invalidUp,view));
 invalidUp.up=std::array{std::numeric_limits<double>::infinity(),0.,0.};CHECK(!planet::valid_stream_up(invalidUp));
 invalidUp=view;invalidUp.forward.reset();CHECK(!planet::valid_stream_up(invalidUp));
 view.up.reset();
 view.forward.reset();
 stream::State oldState{1337};oldState.request=request_for({});const auto oldOutput=output/"old";fs::create_directories(oldOutput);
 CHECK(write_snapshot(oldOutput,oldState,17,0,0,planet::artifact_limits({}),{},{},snapshot_scheduler,activity)==0);
 CHECK(parse_receipt({},view,read_small(oldOutput/"terrain.stream",4096)).operation.ok);
 std::ofstream measured(argv[3]);measured<<std::setprecision(17)<<"{\n\"earth_radius_meters\":6371000,\n\"moon_radius_meters\":1737000,\n\"balanced_maximum_level\":"<<unsigned(balancedLevel)
  <<",\n\"fine_maximum_level\":"<<unsigned(fineLevel)<<",\n\"fine_planned_leaves\":"<<fineLeaves<<",\n\"wet_retained_admission_bytes\":"<<wetRequired
  <<",\n\"sample_patch_vertices\":"<<vertices.size()<<",\n\"sample_patch_indices\":"<<indices.size()<<",\n\"sample_cell_spacing_meters\":"<<spacing
  <<",\n\"maximum_preview_float_roundtrip_error_meters\":"<<maxError<<"\n}\n";
 CHECK(measured.good());
}
'''


class PlanetaryFineDetail(unittest.TestCase):
    def test_original_fine_profile_and_verified_transport(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC actual optional module CPU lane")
        if not (PACKAGE / "tools/planet.stream_export.cpp").is_file():
            self.skipTest("Optional planetary source is not staged in this checkout")
        source = (ROOT / "Engine/src/project/project.planetary.cpp").read_text(encoding="utf-8")
        exporter = (PACKAGE / "tools/planet.stream_export.cpp").read_text(encoding="utf-8")
        shared = (PACKAGE / "tools/planet.mesh_export.cpp").read_text(encoding="utf-8")
        parser_start = source.index("            std::string_view remaining{receipt};")
        parser_end = source.index("            if (ready)\n", parser_start)
        code = CODE.replace("READ_SMALL", definition(source, "        std::string read_small("))
        code = code.replace("SOURCE_CAPABILITY", definition(source, "        bool source_supports_lod_controls("))
        code = code.replace("EXPORT_OPTIONS", definition(shared, "bool planet_export_options("))
        code = code.replace("WRITE_FRAME", 'constexpr std::array<std::string_view,5> planet_mode_names{"classic","tiled","hemisphere","planet","universe"};\n'
                            + definition(shared, "bool planet_write_mesh_frame("))
        snapshot_source = exporter.replace('cloud_mesh={}', 'cloud_mesh=std::shared_ptr<const artifact::Artifact>()').replace(
            'cloud_density={}', 'cloud_density=std::optional<epochengine::asset::cloud_volume::Recipe>()')
        code = code.replace("WRITE_SNAPSHOT", definition(snapshot_source, "    int write_snapshot("))
        activity_start = exporter.index("    struct WorkerActivity")
        activity_end = exporter.index("    template<class T>", activity_start)
        code = code.replace("WORKER_ACTIVITY", exporter[activity_start:activity_end])
        code = code.replace("PARSE_RECEIPT", source[parser_start:parser_end])
        assembly_start = exporter.index("            struct PatchGeometry", exporter.index("    int write_snapshot("))
        assembly_end = exporter.index("            assembly_done=", assembly_start)
        code = code.replace("CONVERSION", exporter[assembly_start:assembly_end])
        proof = ROOT / "Artifacts/Reports/planet-fine-detail-oct27"
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "consumer.cpp").write_text(code, encoding="utf-8")
        owners = [PACKAGE / "upstream/EpochPlanet-0.2.3/src/core/world.cpp", PACKAGE / "upstream/EpochPlanet-0.2.3/src/core/terrain.cpp", ROOT / "Engine/src/assets/texture/asset.texture_artifact.cpp"]
        modules = [PACKAGE / "modules/planet.terrain.ixx", PACKAGE / "modules/planet.streaming.ixx", ROOT / "Engine/modules/project.planetary.ixx", ROOT / "Engine/modules/asset.mesh_artifact.ixx", ROOT / "Engine/modules/asset.texture_artifact.ixx"]
        modules.append(PACKAGE / "modules/planet.coverage.ixx")
        modules.append(PACKAGE / "modules/planet.clouds.ixx")
        modules.append(ROOT / "Engine/modules/project.planetary_snapshot.ixx")
        modules.append(ROOT / "Engine/modules/asset.cloud_volume.ixx")
        modules.append(ROOT / "Engine/modules/water.wave.ixx")
        owners.append(ROOT / "Engine/src/project/project.planetary_snapshot.cpp")
        paths = lambda values: "\n".join('"'+p.as_posix()+'"' for p in values)
        fixture = ROOT / "Artifacts/Runtime/TestData" / ("p" + f"{time.time_ns():x}"[-8:])
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(PlanetaryFineDetail LANGUAGES CXX)
add_library(fine_owners STATIC {paths(owners)})
target_compile_features(fine_owners PUBLIC cxx_std_23)
target_compile_options(fine_owners PUBLIC /sdl /utf-8 /permissive- /Zc:__cplusplus /EHsc)
target_include_directories(fine_owners PUBLIC "{(PACKAGE/'upstream/EpochPlanet-0.2.3/include').as_posix()}")
target_include_directories(fine_owners PUBLIC "{(PACKAGE/'tools').as_posix()}")
target_sources(fine_owners PUBLIC FILE_SET CXX_MODULES BASE_DIRS "{(PACKAGE/'modules').as_posix()}" "{(ROOT/'Engine/modules').as_posix()}" FILES {paths(modules)})
add_executable(fine_detail consumer.cpp)
add_executable(settings_contract "{(ROOT/'Tests/packages/project.planetary_settings_contract.cpp').as_posix()}")
add_executable(fine_provider "{(PACKAGE/'tools/planet.mesh_export.cpp').as_posix()}" "{(PACKAGE/'tools/planet.stream_export.cpp').as_posix()}")
foreach(target IN ITEMS fine_detail settings_contract fine_provider)
target_link_libraries(${{target}} PRIVATE fine_owners)
endforeach()
include("{(ROOT/'Engine/cmake/epoch_task_scheduler.cmake').as_posix()}")
epoch_admit_task_scheduler(fine_detail "{ROOT.as_posix()}")
epoch_admit_task_scheduler(fine_provider "{ROOT.as_posix()}")
enable_testing()
add_test(NAME actual_original_fine_detail COMMAND fine_detail "{fixture.as_posix()}/$<CONFIG>" "{PACKAGE.as_posix()}" "{proof.as_posix()}/measurements-$<CONFIG>.json")
add_test(NAME actual_settings_legacy_and_v5 COMMAND settings_contract)
set_tests_properties(actual_original_fine_detail PROPERTIES TIMEOUT 30)
''', encoding="utf-8")
        build = ROOT / "Artifacts/Intermediate/cmake/planet-fine-detail"
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"], ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        runs = []
        for index, command in enumerate(commands):
            run = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=180)
            log = proof / f"lane-{index}.log"
            log.write_text(run.stdout+run.stderr, encoding="utf-8")
            runs.append({"command": command, "exitCode": run.returncode, "log": str(log)})
            self.assertEqual(run.returncode, 0, run.stdout[-9000:]+run.stderr)
        cli = []
        for config in ("Debug", "Release"):
            producer = build / config / "fine_provider.exe"
            for custom in (False, True):
                output = fixture / config / ("cli-custom" if custom else "cli-legacy")
                command = [str(producer), "--stream-step-preview", str(output), str(output.parent / (output.name+"-cache")),
                    "1337", "4", "0", "10.0001", "0", "0", "1080", "1.0471975511965976", "19" if custom else "10",
                    "576" if custom else "48", str((192 if custom else 16)*1024*1024), "1", "17"]
                if custom:
                    command += ["--mesh-bytes", str(32*1024*1024), "--patch-pixels", "170", "--lod-hysteresis", "0.15"]
                completed = subprocess.run(command, capture_output=True, text=True, timeout=30)
                self.assertEqual(completed.returncode, 0, completed.stdout+completed.stderr)
                lines = (output / "terrain.stream").read_text().splitlines()
                self.assertEqual(lines[0], "epoch.planet.stream.v4" if custom else "epoch.planet.stream.v1")
                receipt = dict(line.split("=", 1) for line in lines[1:])
                self.assertLessEqual(int(receipt["built"]), 1)
                if custom:
                    self.assertEqual(float(receipt["patch_pixels"]), 170)
                    self.assertEqual(float(receipt["lod_hysteresis"]), .15)
                cli.append({"configuration": config, "custom": custom, "exitCode": completed.returncode,
                    "receipt": receipt, "producer_sha256": hashlib.sha256(producer.read_bytes()).hexdigest()})
            output = fixture / config / "cli-view"
            command = [str(producer), "--stream-step-preview", str(output), str(output.parent / "cli-view-cache"),
                "1337", "4", "0", "10.0001", "0", "0", "1080", "1.0471975511965976", "20",
                "384", str(128*1024*1024), "1", "19", "--mesh-bytes", str(32*1024*1024),
                "--patch-transport-v1", "sha256:"+"a"*64+":"+"b"*64,
                "--metric-frame-v1", "--view-cone-v1", "-1,0,0,1.7777777777777777"]
            completed = subprocess.run(command, capture_output=True, text=True, timeout=30)
            self.assertEqual(completed.returncode, 0, completed.stdout+completed.stderr)
            lines = (output / "terrain.stream").read_text().splitlines()
            self.assertEqual(lines[0], "epoch.planet.stream.v8")
            receipt = dict(line.split("=", 1) for line in lines[1:])
            self.assertEqual(float(receipt["view_forward_x"]), -1)
            self.assertEqual(float(receipt["view_aspect"]), 16/9)
            for bad in ("0,0,0,1", "nan,0,0,1", "-1,0,0,0", "-1,0,0,1,2"):
                rejected = subprocess.run(command[:-1]+[bad], capture_output=True, text=True, timeout=30)
                self.assertNotEqual(rejected.returncode, 0)
            cli.append({"configuration": config, "view_cone": True, "receipt": receipt})
        (proof / "result.json").write_text(json.dumps({"passed": True, "commands": runs,
            "scope": "Actual original metric body/ground planner; one highest-detail patch through production conversion and artifact validation. Actual CLI options, producer receipt and Engine receipt parser. No GPU, full576patch generation or native terrain edits.",
            "measurements": {config: json.loads((proof/f"measurements-{config}.json").read_text()) for config in ("Debug", "Release")},
            "actual_producer_cli": cli,
            "sourceHashes": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in owners+modules+[PACKAGE/'tools/planet.mesh_export.cpp', PACKAGE/'tools/planet.stream_export.cpp',ROOT/'Engine/src/project/project.planetary.cpp']}}, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
