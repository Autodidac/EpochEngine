"""Actual sampler bodies with CPU-only clock/resource/driver boundary ports."""
from pathlib import Path
import hashlib
import json
import os
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/platform/platform.work_admission.cpp"
MODULE = ROOT / "Engine/modules/platform.work_admission.ixx"

PREFIX = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
using namespace std::chrono_literals;
enum class GpuTelemetrySource : std::uint8_t { unavailable, nvidia_nvml };
'''

PORTS = r'''
std::atomic_uint64_t fake_now{1000};
std::atomic_uint32_t driver_queries{}, host_queries{}, driver_loads{}, driver_unloads{};
std::mutex barrier_mutex;
std::condition_variable barrier_cv;
bool driver_entered{}, release_driver{};
std::uint64_t now_milliseconds() noexcept {return fake_now.load();}
struct CpuTimes final {std::uint64_t total{}, idle{}; bool valid{};};
CpuTimes read_resources(ResourceSample& sample) {
 ++host_queries; sample.total_memory_bytes=32ull<<30;sample.available_memory_bytes=16ull<<30;
 sample.memory_valid=true;const auto now=fake_now.load();return {now*10,now*8,true};
}
GpuResourceSample fake_gpu_query() noexcept {
 ++driver_queries;
 {std::unique_lock lock{barrier_mutex};driver_entered=true;barrier_cv.notify_all();
  barrier_cv.wait(lock,[]{return release_driver;});}
 GpuResourceSample value;value.source=GpuTelemetrySource::nvidia_nvml;
 value.busy_valid=true;value.busy_fraction=.42;return value;
}
bool await_driver() {std::unique_lock lock{barrier_mutex};
 return barrier_cv.wait_for(lock,2s,[]{return driver_entered;});}
void release_fake_driver() {{std::lock_guard lock{barrier_mutex};release_driver=true;}barrier_cv.notify_all();}
'''

COUNTERFACTUAL = r'''
GpuResourceSample read_gpu_resources() noexcept {return fake_gpu_query();}
'''
COUNTERFACTUAL_MAIN = r'''
int main() {
 auto first=std::async(std::launch::async,[]{return sample_host_resources();});
 const bool entered=await_driver();
 auto second=std::async(std::launch::async,[]{return sample_host_resources();});
 const bool blocked=second.wait_for(50ms)==std::future_status::timeout;
 fake_now=3000;release_fake_driver();
 const auto a=first.get(),b=second.get();
 assert(entered&&blocked&&a.memory_valid&&b.memory_valid);
 // The timestamp precedes the slow query, so the next reader refreshes again.
 assert(driver_queries==2&&a.captured_at_ms==1000&&b.captured_at_ms==3000);
}
'''

WORKER_PORTS = r'''
std::atomic_bool hold_unload{}, unload_entered{}, release_unload{};
struct NativeNvml final {
 int api{};
 NativeNvml() noexcept {++driver_loads;}
 ~NativeNvml() {
  unload_entered=true;
  while(hold_unload&&!release_unload)std::this_thread::yield();
  ++driver_unloads;
 }
};
GpuResourceSample read_nvml_resources(int) noexcept {return fake_gpu_query();}
'''
REPAIRED_MAIN = r'''
int main() {
 auto initial=sample_host_resources();
 assert(initial.memory_valid&&!initial.gpu.busy_valid&&driver_loads==0);
 fake_now=2000;auto secondHost=sample_host_resources();
 assert(secondHost.cpu_valid&&secondHost.memory_valid&&driver_loads==0);
#if EPOCH_HAS_NVML_TELEMETRY
 assert(gpuSampler.load()==nullptr);
 assert(start_gpu_telemetry());const bool entered=await_driver();
 assert(start_gpu_telemetry());
 auto reader=std::async(std::launch::async,[]{return sample_host_resources();});
 const bool finishedBeforeRelease=reader.wait_for(100ms)==std::future_status::ready;
 const auto stopStarted=std::chrono::steady_clock::now();
 const bool stopped=stop_gpu_telemetry(0);
 const auto stopElapsed=std::chrono::steady_clock::now()-stopStarted;
 const bool restarted=start_gpu_telemetry();
 fake_now=4000;release_fake_driver();const auto available=reader.get();
 const bool retired=stop_gpu_telemetry(100);
 assert(entered&&finishedBeforeRelease&&available.memory_valid&&available.cpu_valid);
 assert(!stopped&&!restarted&&stopElapsed<50ms&&retired);
 assert(driver_loads==1&&driver_unloads==1&&!sample_host_resources().gpu.busy_valid);
 // A new generation cannot inherit the cancelled generation's late result.
 {std::lock_guard lock{barrier_mutex};driver_entered=false;release_driver=false;}
 assert(start_gpu_telemetry());assert(await_driver());
 assert(!sample_host_resources().gpu.busy_valid);
 fake_now=6000;release_fake_driver();
 auto deadline=std::chrono::steady_clock::now()+2s;ResourceSample published;
 do {published=sample_host_resources();std::this_thread::yield();}
 while(!published.gpu.busy_valid&&std::chrono::steady_clock::now()<deadline);
 assert(published.gpu.busy_valid&&published.gpu.captured_at_ms==6000);
 assert(published.gpu.sampling_started_at_ms==4000&&driver_queries==2);
 // Completion-based cadence prevents immediate catch-up after a two-second query.
 std::this_thread::sleep_for(30ms);assert(driver_queries==2);
 fake_now=11001;assert(!sample_host_resources().gpu.busy_valid);
 assert(stop_gpu_telemetry(100)&&driver_unloads==2);
 {std::lock_guard lock{barrier_mutex};driver_entered=false;release_driver=false;}
 fake_now=12000;assert(start_gpu_telemetry());assert(await_driver());
 fake_now=19000;release_fake_driver();std::this_thread::sleep_for(30ms);
 assert(!sample_host_resources().gpu.busy_valid&&driver_queries==3);
 // Driver shutdown is also uninterruptible: it must finish before join/restart.
 hold_unload=true;unload_entered=false;assert(!stop_gpu_telemetry(0));
 deadline=std::chrono::steady_clock::now()+2s;
 while(!unload_entered&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
 assert(unload_entered&&!stop_gpu_telemetry(0)&&!start_gpu_telemetry()&&driver_unloads==2);
 release_unload=true;assert(stop_gpu_telemetry(100));
 assert(driver_loads==3&&driver_unloads==3&&!sample_host_resources().gpu.busy_valid);
#else
 assert(!start_gpu_telemetry()&&stop_gpu_telemetry(0));
 assert(driver_loads==0&&driver_queries==0&&!sample_host_resources().gpu.busy_valid);
#endif
}
'''


def types():
    module = MODULE.read_text(encoding="utf-8")
    return "\n".join(definition(module, "    struct " + name + " final", True)
                     for name in ("GpuResourceSample", "ResourceSample"))


class ResourceSamplerConcurrency(unittest.TestCase):
    def test_counterfactual_actual_owner(self):
        source = OWNER.read_text(encoding="utf-8")
        if "bool start_gpu_telemetry(" in source:
            self.skipTest("Pre-repair actual-owner proof is frozen in gpu-sampler-concurrency-oct30/counterfactual")
        report = ROOT / "Artifacts/Reports/gpu-sampler-concurrency-oct30/counterfactual" / str(time.time_ns())
        report.mkdir(parents=True)
        fixture = report / "consumer.cpp"
        fixture.write_text(PREFIX + types() + PORTS + COUNTERFACTUAL
                           + definition(source, "    ResourceSample sample_host_resources()")
                           + COUNTERFACTUAL_MAIN, encoding="utf-8")
        records = compile_and_run(self, fixture, report)
        (report / "result.json").write_text(json.dumps({"passed": True,
            "owner_sha256": hashlib.sha256(OWNER.read_bytes()).hexdigest(),
            "driver_calls": False, "actual_owner": "sample_host_resources body unchanged",
            "proved": ["second reader blocked until fake driver released", "slow query expires newly published cache"],
            "invocations": records}, indent=2) + "\n", encoding="utf-8")

    def test_repaired_actual_owner(self):
        if os.name != "nt":
            self.skipTest("Windows optional NVML worker native thread retirement contract")
        source = OWNER.read_text(encoding="utf-8")
        report = ROOT / "Artifacts/Reports/gpu-sampler-concurrency-oct30/repaired" / str(time.time_ns())
        report.mkdir(parents=True)
        sampler = "\n".join(definition(source, marker, marker.endswith(" final")) for marker in (
            "        struct GpuTelemetrySampler final", "        void run_gpu_telemetry("))
        state = "std::atomic<GpuTelemetrySampler*> gpuSampler{};\nstd::mutex gpuSamplerCreation{};\n"
        # Keep the actual preprocessor branches and actual lifecycle/sampler bodies.
        functions = "\n".join(definition(source, marker) for marker in (
            "        [[nodiscard]] GpuResourceSample cached_gpu_resources()",
            "    bool start_gpu_telemetry()", "    bool stop_gpu_telemetry(",
            "    ResourceSample sample_host_resources()"))
        code = sampler.split("        void run_gpu_telemetry(", 1)
        sampler = code[0] + state + "        void run_gpu_telemetry(" + code[1]
        kit = sorted(Path("C:/Program Files (x86)/Windows Kits/10/Include").glob("*"))[-1]
        before = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in (OWNER, MODULE, Path(__file__))}
        records = []
        for enabled in (1, 0):
            lane = report / str(enabled)
            lane.mkdir()
            fixture = lane / "consumer.cpp"
            fixture.write_text("#define NOMINMAX\n#include <Windows.h>\n#define EPOCH_HAS_NVML_TELEMETRY "
                + str(enabled) + "\n" + PREFIX + types() + PORTS + WORKER_PORTS
                + "#if EPOCH_HAS_NVML_TELEMETRY\n" + sampler + "\n#endif\n" + functions + REPAIRED_MAIN,
                encoding="utf-8")
            records.extend(compile_and_run(self, fixture, lane, includes=(kit / "um", kit / "shared"),
                                           windows_flags=("/W4", "/WX")))
        self.assertEqual(before, {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                for path in (OWNER, MODULE, Path(__file__))})
        (report / "result.json").write_text(json.dumps({"passed": True, "source_sha256": before,
            "driver_calls": False, "actual_owner": "worker, native thread lifecycle, publication and host sampler bodies unchanged",
            "proved": ["initial status never starts GPU", "CPU/RAM read finishes before blocked GPU release",
                "zero-budget stop, pending-restart refusal, cancelled late publication",
                "completion cadence, GPU age expiry and overlong-query rejection", "worker DLL cleanup precedes join",
                "OFF branch never starts worker"], "invocations": records}, indent=2) + "\n", encoding="utf-8")


class SessionTelemetryLifecycle(unittest.TestCase):
    def test_actual_session_scope_and_early_retirement(self):
        source = (ROOT / "Engine/src/epoch/implementation/epoch.session_runtime.inl").read_text(encoding="utf-8")
        scope = definition(source, "            struct SessionGpuTelemetry final", True)
        self.assertLess(source.index("gpuTelemetry.stop();"), source.index("mgr.StopAll();"))
        lane = ROOT / "Artifacts/Intermediate/tests/session-gpu-telemetry"
        lane.mkdir(parents=True, exist_ok=True)
        fixture = lane / "scope.cpp"
        fixture.write_text(r'''
#include <cassert>
#include <stdexcept>
unsigned starts{},stops{},warnings{};bool start_ok=true,stop_ok=true,throw_log=false;
namespace epochengine::platform::work_admission {
bool start_gpu_telemetry() noexcept {++starts;return start_ok;}
bool stop_gpu_telemetry(unsigned budget) noexcept {assert(budget==50);++stops;return stop_ok;}
}
constexpr const char* kEngineLog="test";
namespace logger {void warn(const char*,const char*){++warnings;if(throw_log)throw std::runtime_error("log");}}
''' + scope + r'''
int main(){
 {SessionGpuTelemetry owner;assert(starts==1&&stops==0);owner.stop();assert(stops==1);owner.stop();}
 assert(stops==1&&warnings==0);
 try{SessionGpuTelemetry owner;throw std::runtime_error("exit");}catch(const std::runtime_error&){}
 assert(starts==2&&stops==2);
 start_ok=false;{SessionGpuTelemetry owner;owner.stop();}assert(starts==3&&stops==2);
 start_ok=true;stop_ok=false;throw_log=true;{SessionGpuTelemetry owner;owner.stop();}
 assert(starts==4&&stops==3&&warnings==1);
}
''', encoding="utf-8")
        compile_and_run(self, fixture, lane)


if __name__ == "__main__":
    unittest.main()
