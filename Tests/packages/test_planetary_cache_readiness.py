"""Actual cache owner, codec, streaming state and scheduler; no runtime or GPU."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import time
import unittest

from Tests.platform.test_child_close_router import definition

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / "Engine/dep/EpochEngineExtensions/packages/planetary_terrain"
OWNER = PACKAGE / "tools/planet.stream_export.cpp"

CODE = r'''
#include "planet.stream_cache.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <future>
#include <iterator>
#include <mutex>
#include <optional>
#include <stop_token>
#include <stdexcept>
#include <thread>
import editor.task_scheduler;
import planet.terrain;
import planet.streaming;
namespace fs=std::filesystem;
namespace core=epoch::extensions::planetary_terrain::core;
namespace stream=epoch::extensions::planetary_terrain::streaming;
namespace tasks=epochengine::editor_tasks;
using namespace std::chrono_literals;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return __LINE__;}}while(false)
struct ReadGate {
 std::mutex mutex;std::condition_variable changed;
 core::TileKey first{},second{};bool enabled{},failure{},released{},first_entered{},later_entered{};
 unsigned started{},active{},peak{};
} gate;
// Only the read boundary is recorded to hold one actual codec call. All path,
// content, planner, residency and worker admission below use production owners.
namespace cache {
using planet_stream_cache::plain_path;
using planet_stream_cache::parse_filename;
using planet_stream_cache::encode_options;
using planet_stream_cache::Options;
template<class Key,class Mesh>bool read_patch(const fs::path& path,Key key,unsigned cells,Mesh& mesh,bool ocean){
 bool fail{};
 {
  std::unique_lock lock(gate.mutex);++gate.started;++gate.active;gate.peak=(std::max)(gate.peak,gate.active);
  if(gate.enabled){
   if(key==gate.first){gate.first_entered=true;gate.changed.notify_all();gate.changed.wait(lock,[]{return gate.released;});}
   else if(gate.failure&&key==gate.second){
    gate.changed.wait(lock,[]{return gate.first_entered;});fail=true;
   }else if(gate.failure)gate.changed.wait(lock,[]{return gate.released;});
   else if(key!=gate.second){gate.later_entered=true;gate.changed.notify_all();}
  }
 }
 const bool valid=!fail&&planet_stream_cache::read_patch(path,key,cells,mesh,ocean);
 {std::lock_guard lock(gate.mutex);--gate.active;gate.changed.notify_all();}
 return valid;
}
}
ACTUAL_ACTIVITY
ACTUAL_OWNER
std::string read(const fs::path& path){std::ifstream input{path,std::ios::binary};return {std::istreambuf_iterator<char>{input},{}};}
void write(const fs::path& path,const std::string& text){std::ofstream output{path,std::ios::binary|std::ios::trunc};output<<text;}
stream::State state(){
 stream::State result;stream::Request request;
 request.camera={0,0,result.world.body(0).radius*8};request.lod={0,6,210,.15};
 request.cells_per_tile=2;request.maximum_in_flight=4;
 if(stream::update(result,request)!=stream::Status::ready)throw std::runtime_error("state setup");
 std::ranges::reverse(result.plan.needed);return result;
}
std::vector<core::TileKey> files(const fs::path& root,const stream::State& source){
 fs::create_directories(root);
 write(root/"cache.epoch_planet_stream",cache::encode_options({1337,source.request.cells_per_tile,
  source.request.body_index,false,source.world.config().reference_body,source.world.config().reference_radius_meters}));
 for(const auto key:source.plan.needed){
  const stream::Work work{source.generation,1,key,source.request.cells_per_tile,false};
  if(!planet_stream_cache::write_patch(root/planet_stream_cache::filename(key),key,
    stream::build(source.world.body(key.body),work)))throw std::runtime_error("cache setup");
 }
 std::vector<core::TileKey> order;
 for(const auto& entry:fs::directory_iterator(root)){
  core::TileKey key;if(cache::parse_filename(entry.path().filename().string(),key))order.push_back(key);
 }
 if(order.size()!=6)throw std::runtime_error("six root setup");return order;
}
void reset_gate(const std::vector<core::TileKey>& order,bool failure){
 std::lock_guard lock(gate.mutex);gate.first=order[0];gate.second=order[1];
 gate.enabled=true;gate.failure=failure;gate.released=gate.first_entered=gate.later_entered=false;
 gate.started=gate.active=gate.peak=0;
}
void release(){std::lock_guard lock(gate.mutex);gate.released=true;gate.changed.notify_all();}
int main(int argc,char** argv){
 CHECK(argc==2);const fs::path root{argv[1]};fs::create_directories(root);
 bool refilled{},cancelled_before_release{};unsigned success_peak{},failure_peak{};
 {
  auto source=state();const auto path=root/"success";const auto order=files(path,source);
  // Two slots make the earlier blocked/later ready condition unambiguous.
  source.request.maximum_in_flight=2;reset_gate(order,false);
  // Use the codec's exact filename grammar for an obsolete pending patch.
  const auto pending=path/(planet_stream_cache::filename(core::TileKey{0,0,0,0,1})+".pending");
  write(pending,"retained until every read retires");
  tasks::Scheduler scheduler{{.worker_count=2,.retained_finished_nodes=32}};WorkerActivity activity;std::size_t reused{};
  auto owner=std::async(std::launch::async,[&]{return prepare_cache(path,source,1337,scheduler,activity,reused);});
  {std::unique_lock lock(gate.mutex);refilled=gate.changed.wait_for(lock,3s,[]{return gate.later_entered;});}
  const bool pending_before_retirement=fs::exists(pending);release();CHECK(owner.wait_for(3s)==std::future_status::ready);
  CHECK(owner.get()&&reused==6&&pending_before_retirement&&!fs::exists(pending));
  scheduler.graph().WaitAll();CHECK(scheduler.activity().outstanding_tasks==0);
  CHECK(source.resident.size()==source.plan.needed.size());fs::create_directories(root/"roundtrip");
  for(std::size_t i=0;i<source.resident.size();++i){const auto& resident=source.resident[i];
   CHECK(resident.key==source.plan.needed[i]);const auto name=planet_stream_cache::filename(resident.key);
   CHECK(planet_stream_cache::write_patch(root/"roundtrip"/name,resident.key,resident.mesh));
   CHECK(read(path/name)==read(root/"roundtrip"/name));
  }
  CHECK(stream::stats(source).complete_coverage&&source.flight_reservation==0&&source.in_flight.empty());
  {std::lock_guard lock(gate.mutex);success_peak=gate.peak;CHECK(gate.active==0&&gate.started==6&&success_peak<=2);}
  CHECK(activity.peak<=2);
 }
 {
  auto source=state();const auto path=root/"failure";const auto order=files(path,source);reset_gate(order,true);
  const auto pending=path/(planet_stream_cache::filename(core::TileKey{0,0,0,0,1})+".pending");write(pending,"must survive refusal");
  tasks::Scheduler scheduler{{.worker_count=2,.retained_finished_nodes=32}};WorkerActivity activity;std::size_t reused{};
  auto owner=std::async(std::launch::async,[&]{return prepare_cache(path,source,1337,scheduler,activity,reused);});
  const auto deadline=std::chrono::steady_clock::now()+3s;
  while(std::chrono::steady_clock::now()<deadline){
   if(scheduler.graph().Snapshot(0).cancellationRequestCount){cancelled_before_release=true;break;}
   std::this_thread::sleep_for(1ms);
  }
  const bool retained_while_running=fs::exists(pending)&&owner.wait_for(0ms)!=std::future_status::ready;
  release();CHECK(owner.wait_for(3s)==std::future_status::ready);CHECK(!owner.get());
  scheduler.graph().WaitAll();CHECK(scheduler.activity().outstanding_tasks==0);
  CHECK(retained_while_running&&fs::exists(pending)&&source.resident.empty()&&reused==0);
  CHECK(source.flight_reservation==0&&source.in_flight.empty());
  {std::lock_guard lock(gate.mutex);failure_peak=gate.peak;CHECK(gate.active==0&&failure_peak<=2&&gate.started<=4);}
  CHECK(activity.peak<=2);
 }
 std::ofstream metrics{root/"measurements.json"};
 metrics<<"{\"refilled_before_first_release\":"<<(refilled?"true":"false")
  <<",\"cancel_requested_before_first_release\":"<<(cancelled_before_release?"true":"false")
  <<",\"restored_in_plan_order\":true,\"patch_bytes_unchanged\":true,\"complete_coverage\":true"
  <<",\"workers_retired\":true,\"failed_read_preserves_obsolete_files\":true"
  <<",\"success_peak_reads\":"<<success_peak<<",\"failure_peak_reads\":"<<failure_peak<<"}\n";
 metrics.close();CHECK(metrics);if(!refilled||!cancelled_before_release)return 86;return 0;
}
'''


class PlanetaryCacheReadiness(unittest.TestCase):
    def test_actual_cache_owner_drains_ready_reads_and_retires_failures(self):
        source = OWNER.read_text(encoding="utf-8-sig")
        actual = definition(source, "    bool prepare_cache(")
        activity = definition(source, "    struct WorkerActivity", True)
        code = CODE.replace("ACTUAL_ACTIVITY", activity).replace("ACTUAL_OWNER", actual)
        identity = hashlib.sha256(code.encode()).hexdigest()[:12]
        proof = ROOT / "Artifacts/Reports/planet-cache-readiness84-nov07" / identity
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "consumer.cpp").write_text(code, encoding="utf-8")
        (proof / "owner.cpp").write_text(source, encoding="utf-8")
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(PlanetCacheReadiness LANGUAGES CXX)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("{PACKAGE.as_posix()}" provider EXCLUDE_FROM_ALL)
add_executable(cache_readiness consumer.cpp)
target_link_libraries(cache_readiness PRIVATE EpochEngineExtensions::PlanetaryCore)
target_include_directories(cache_readiness PRIVATE "{(PACKAGE/'tools').as_posix()}")
target_compile_features(cache_readiness PRIVATE cxx_std_23)
include("{(ROOT/'Engine/cmake/epoch_task_scheduler.cmake').as_posix()}")
epoch_admit_task_scheduler(cache_readiness "{ROOT.as_posix()}")
''', encoding="utf-8")
        build = proof / "cmake"
        temporary = proof / "tmp"
        temporary.mkdir(exist_ok=True)
        data = proof / str(time.time_ns())
        env = dict(os.environ, TEMP=str(temporary), TMP=str(temporary), TMPDIR=str(temporary),
                   MSBUILDDISABLENODEREUSE="1")
        configure = ["cmake", "-S", str(proof), "-B", str(build)]
        configure += (["-G", "Visual Studio 17 2022", "-A", "x64",
                       "-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false"]
                      if os.name == "nt" else ["-G", "Ninja Multi-Config"])
        runs = []
        commands = [("configure", configure)]
        for config in ("Debug", "Release"):
            command = ["cmake", "--build", str(build), "--config", config, "--parallel", "1", "--target", "cache_readiness"]
            if os.name == "nt":
                command += ["--", "/nr:false", "/p:UseMultiToolTask=false", "/p:CL_MPCount=1"]
            commands += [(config + "-build", command), (config + "-run", [
                str(build/config/("cache_readiness.exe" if os.name == "nt" else "cache_readiness")),
                str(data/config)])]
        for phase, command in commands:
            completed = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, timeout=180)
            (proof / (phase + ".log")).write_text(completed.stdout + completed.stderr, encoding="utf-8")
            runs.append(dict(phase=phase, exit=completed.returncode, command=command))
            (proof / "result.json").write_text(json.dumps(dict(
                passed=len(runs) == len(commands) and all(run["exit"] == 0 for run in runs), runs=runs,
                measurements_root=str(data),
                owner_sha256=hashlib.sha256(OWNER.read_bytes()).hexdigest(),
                actual_prepare_cache=True, actual_codec=True, actual_stream_state=True, actual_scheduler=True,
                recorded_boundary="Only cache read gating/failure; no replacement scheduler or path/codec admission",
                scope="Deterministic blocked-first/ready-later refill and failed-read owned cancellation/retirement; no native latency, snapshot rendering or GUI acceptance"
            ), indent=2) + "\n", encoding="utf-8")
            if not phase.endswith("-run"):
                self.assertEqual(completed.returncode, 0, (completed.stdout + completed.stderr)[-10000:])
        self.assertTrue(all(run["exit"] == 0 for run in runs), str(proof / "result.json"))


if __name__ == "__main__":
    unittest.main()
