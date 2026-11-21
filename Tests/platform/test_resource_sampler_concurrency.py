"""Actual sampler bodies with CPU-only clock/resource/driver boundary ports."""
from pathlib import Path
import hashlib
import json
import os
import sys
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run, windows_sdk_includes

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/platform/platform.work_admission.cpp"
MODULE = ROOT / "Engine/modules/platform/platform.work_admission.ixx"

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
#include <optional>
#include <thread>
using namespace std::chrono_literals;
enum class GpuTelemetrySource : std::uint8_t { unavailable, nvidia_nvml, windows_pdh_process };
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
std::atomic_uint32_t epoch_queries{};
struct EpochCpuFrame final {std::uint64_t sampled_at_ms{};};
struct ObservedProcesses final {};
struct GpuProcessFamily final {};
GpuProcessFamily identify_gpu_processes(const ObservedProcesses&,bool) noexcept { return {}; }
struct NativeEpochGpu final {
 GpuResourceSample sample(const GpuProcessFamily&,std::uint64_t) {return {};}
};
EpochResourceSample read_epoch_resources(EpochCpuFrame& previous,std::uint64_t began,ObservedProcesses&) noexcept {
 ++epoch_queries;
 EpochResourceSample sample;
 sample.process_count=2;sample.resident_memory_bytes=3ull<<30;sample.total_memory_bytes=32ull<<30;
 sample.memory_valid=true;sample.coverage_complete=true;
 if(previous.sampled_at_ms && began>=previous.sampled_at_ms+1000){sample.cpu_valid=true;sample.cpu_busy_fraction=.25;}
 previous.sampled_at_ms=began;
 return sample;
}
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
 assert(start_gpu_telemetry());
 // Work admission and pacing reads of CPU/RAM leave GPU diagnostics dormant.
 for(int i=0;i<11;++i){(void)sample_host_resources();std::this_thread::sleep_for(100ms);}
 assert(driver_queries==0&&driver_loads==0&&epoch_queries==0);
 assert(!sample_gpu_telemetry().busy_valid);const bool entered=await_driver();
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
 assert(start_gpu_telemetry());
 assert(!sample_gpu_telemetry().busy_valid);assert(await_driver());
 assert(!sample_host_resources().gpu.busy_valid);
 fake_now=6000;release_fake_driver();
 auto deadline=std::chrono::steady_clock::now()+2s;ResourceSample published;
 do {published=sample_host_resources();std::this_thread::yield();}
 while(!published.gpu.busy_valid&&std::chrono::steady_clock::now()<deadline);
 assert(published.gpu.busy_valid&&published.gpu.captured_at_ms==6000);
 assert(published.gpu.sampling_started_at_ms==4000&&driver_queries==2);
 // Completion-based cadence prevents immediate catch-up after a two-second query.
 std::this_thread::sleep_for(1100ms);assert(driver_queries==2);
 fake_now=11001;assert(!sample_host_resources().gpu.busy_valid);
 assert(stop_gpu_telemetry(100)&&driver_unloads==2);
 {std::lock_guard lock{barrier_mutex};driver_entered=false;release_driver=false;}
 fake_now=12000;assert(start_gpu_telemetry());
 assert(!sample_gpu_telemetry().busy_valid);assert(await_driver());
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
 assert(start_gpu_telemetry());
 assert(!sample_gpu_telemetry().busy_valid&&epoch_queries==0);
 assert(driver_loads==0&&driver_queries==0&&!sample_host_resources().gpu.busy_valid);
 assert(!sample_epoch_resources().memory_valid);
 auto deadline=std::chrono::steady_clock::now()+3s;
 EpochResourceSample family;
 do {family=sample_epoch_resources();std::this_thread::yield();}
 while(!family.memory_valid&&std::chrono::steady_clock::now()<deadline);
 assert(family.memory_valid&&!family.cpu_valid&&family.process_count==2&&family.coverage_complete);
 assert(family.resident_memory_bytes==(3ull<<30)&&!family.gpu.busy_valid&&!family.gpu.memory_valid);
 fake_now=3000;deadline=std::chrono::steady_clock::now()+3s;
 do {family=sample_epoch_resources();std::this_thread::yield();}
 while(!family.cpu_valid&&std::chrono::steady_clock::now()<deadline);
 assert(family.cpu_valid&&family.cpu_busy_fraction==.25&&driver_loads==0&&driver_queries==0);
 assert(stop_gpu_telemetry(100)&&!sample_epoch_resources().memory_valid);
#endif
}
'''

NATIVE_PROCESS_MAIN = r'''
#include <cstdio>
#include <stdexcept>

using namespace epochengine::platform::work_admission;
namespace observation=epochengine::platform::process_observation;

#define CHECK(condition) do {if(!(condition)) throw std::runtime_error(\
    std::string(#condition)+" at line "+std::to_string(__LINE__)+\
    " (last Windows error "+std::to_string(::GetLastError())+")");}while(false)

struct Job final {
 HANDLE handle{::CreateJobObjectW(nullptr,nullptr)};
 ~Job(){if(handle)(void)::CloseHandle(handle);}
 void close(){CHECK(::CloseHandle(handle));handle=nullptr;}
};

struct Worker final {
 PROCESS_INFORMATION process{};
 Worker() {
  wchar_t executable[1024]{};
  const auto size=::GetModuleFileNameW(nullptr,executable,1024);
  CHECK(size>0&&size<1024);
  std::wstring command=L"\"";command+=executable;command+=L"\" --fixture-child";
  STARTUPINFOW startup{};startup.cb=sizeof(startup);
  CHECK(::CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,
      CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&startup,&process));
 }
 ~Worker(){
  if(::WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT)(void)::TerminateProcess(process.hProcess,1);
  (void)::WaitForSingleObject(process.hProcess,1000);
  (void)::CloseHandle(process.hThread);(void)::CloseHandle(process.hProcess);
 }
 void resume(){CHECK(::ResumeThread(process.hThread)!=static_cast<DWORD>(-1));}
};

void verify_gpu_rollup(){
 GpuInstance instance;
 CHECK(parse_gpu_instance(L"pid_44_luid_0xFFFFFFFF_0x0001A12B_phys_0_eng_3_engtype_Cuda#1",instance,true));
 CHECK(instance.pid==44&&instance.luid==0xFFFFFFFF0001A12Bull&&instance.physical==0&&instance.engine==3);
 CHECK(!parse_gpu_instance(L"pid_4294967296_luid_0x00000000_0x0001A12B_phys_0",instance,false));
 CHECK(!parse_gpu_instance(L"pid_44_luid_0x00000000_0x0001A12B_phys_0_unknown",instance,false));
 CHECK(parse_gpu_instance(L"pid_44_luid_0x00000000_0x0001A12B_phys_0#2",instance,false));
 GpuResourceSample gpu;
 std::array<GpuAdapter,32> adapters{};
 adapters[0]={1,16ull<<30};adapters[1]={2,24ull<<30};adapters[2]={3,8ull<<30,true};
 std::vector<GpuCounterRow> engines{
  {{1,0,3,44},10,0},{{1,0,3,45},25,0},{{1,0,3,45},25,0},{{1,0,7,44},21,0},
  {{2,0,3,44},42,1},{{2,0,3,45},80,1},{{2,0,3,45},90,1},{{3,1,3,44},40,2}};
 std::vector<GpuCounterRow> memory{
  {{1,0,0,44},double(1ull<<30),0},{{1,0,0,44},double(1ull<<30),0},
  {{1,0,0,45},double(2ull<<30),0},{{2,0,0,44},double(4ull<<30),1},{{3,1,0,44},double(5ull<<30),2}};
 summarize_gpu_rows(gpu,engines,memory,adapters);
 CHECK(gpu.source==GpuTelemetrySource::windows_pdh_process&&gpu.busy_valid&&gpu.memory_valid);
 CHECK(std::abs(gpu.busy_fraction-.42)<1e-12&&gpu.activity_partial&&gpu.memory_partial&&gpu.adapter_count==3);
 CHECK(gpu.dedicated_used_bytes==(7ull<<30)&&gpu.dedicated_total_bytes==(40ull<<30));
}

int main(int argc,char**) try {
 (void)::SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 if(argc>1){
  const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds{150};
  while(std::chrono::steady_clock::now()<end) {}
  return 0;
 }
 verify_gpu_rollup();
 Worker owned,sibling,other;
 Job job;CHECK(job.handle);
 JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
 limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
 CHECK(::SetInformationJobObject(job.handle,JobObjectExtendedLimitInformation,&limits,sizeof(limits)));
 CHECK(::AssignProcessToJobObject(job.handle,owned.process.hProcess));
 CHECK(::AssignProcessToJobObject(job.handle,sibling.process.hProcess));
 CHECK(!sample_epoch_resources().memory_valid);
 EpochCpuFrame frames{};
 std::vector<DWORD> live_ids;
 const auto read=[&](std::uint64_t clock){
     ObservedProcesses processes;
     auto sample=read_epoch_resources(frames,clock,processes);
     live_ids.clear();
     for (std::size_t i=0;i<processes.count;++i)
         if (i==0||::WaitForSingleObject(processes.members[i].handle,0)==WAIT_TIMEOUT)
             live_ids.push_back(processes.members[i].id);
     return sample;
 };
 const auto observed=[&](DWORD pid){return std::ranges::find(live_ids,pid)!=live_ids.end();};
 auto startup=read(1000);
 CHECK(startup.process_count==1&&startup.memory_valid&&!startup.cpu_valid&&startup.coverage_complete);
 CHECK(observed(::GetCurrentProcessId())&&!observed(owned.process.dwProcessId)&&!observed(other.process.dwProcessId));
 observation::Lease job_registration;
 job_registration.attach(job.handle,observation::TargetKind::job);
 auto initial=read(2000);
 CHECK(initial.process_count>=3&&initial.memory_valid&&!initial.coverage_complete);
 CHECK(observed(owned.process.dwProcessId)&&observed(sibling.process.dwProcessId)&&!observed(other.process.dwProcessId));
 auto owned_frame=frames;
 CHECK(owned_frame.counters[1].generation!=0u&&owned_frame.counters[1].valid);
 owned.resume();
 CHECK(::WaitForSingleObject(owned.process.hProcess,3000)==WAIT_OBJECT_0);
 DWORD result{};
 CHECK(::GetExitCodeProcess(owned.process.hProcess,&result)&&result==0u);
 auto finished=read(3000);
 CHECK(finished.process_count>=2&&finished.cpu_valid&&finished.memory_valid);
 CHECK(!observed(owned.process.dwProcessId)&&observed(sibling.process.dwProcessId)&&!observed(other.process.dwProcessId));
 CHECK(frames.counters[1].generation==owned_frame.counters[1].generation);
 CHECK(frames.counters[1].ticks>owned_frame.counters[1].ticks&&finished.cpu_busy_fraction>0);
 CHECK(!finished.gpu.busy_valid&&!finished.gpu.memory_valid);
 job_registration.reset();job.close();
 CHECK(::WaitForSingleObject(sibling.process.hProcess,3000)==WAIT_OBJECT_0);

 observation::Lease direct_registration;
 direct_registration.attach(other.process.hProcess,observation::TargetKind::process);
 auto direct=read(4000);
 CHECK(direct.process_count==2&&direct.memory_valid&&!direct.coverage_complete);
 CHECK(observed(other.process.dwProcessId)&&!observed(sibling.process.dwProcessId));
 CHECK(frames.counters[1].generation!=owned_frame.counters[1].generation);
 direct_registration.reset();
 auto retired=read(5000);
 CHECK(retired.process_count==1&&retired.memory_valid&&!retired.coverage_complete);
 auto settled=read(6000);
 CHECK(settled.process_count==1&&settled.memory_valid&&settled.coverage_complete);
 return 0;
} catch(const std::exception& error) {
 std::fprintf(stderr,"Epoch process observation: %s\n",error.what());
 return 1;
}
'''


def types():
    module = MODULE.read_text(encoding="utf-8")
    return "\n".join(definition(module, "    struct " + name + " final", True)
                     for name in ("GpuResourceSample", "ResourceSample", "EpochResourceSample"))


class ResourceSamplerConcurrency(unittest.TestCase):
    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux procfs native telemetry")
    def test_linux_epoch_cpu_rss_and_owned_child(self):
        report = ROOT / "Artifacts/Reports/linux-epoch-process-sample" / str(time.time_ns())
        report.mkdir(parents=True)
        declarations = ("namespace epochengine::platform::work_admission {\n"
            + "enum class GpuTelemetrySource : std::uint8_t { unavailable, nvidia_nvml, windows_pdh_process };\n"
            + types() + "\nstd::uint64_t now_milliseconds() noexcept;\n}\n")
        source = OWNER.read_text(encoding="utf-8")
        body = source.replace("module;", "", 1).replace("module platform.work_admission;", declarations, 1)
        fixture = report / "consumer.cpp"
        fixture.write_text(body + r'''
#include <cassert>
#include <csignal>
#include <sys/prctl.h>
#include <sys/wait.h>
int main() {
    using namespace epochengine::platform::work_admission;
    using namespace std::chrono_literals;
    const auto first = sample_epoch_resources();
    assert(first.process_count == 1 && first.memory_valid && first.resident_memory_bytes > 0);
    assert(!first.cpu_valid && !first.coverage_complete && !first.gpu.busy_valid && !first.gpu.memory_valid);
    assert(!read_linux_process(getppid(), getpid()));
    int ready[2]; assert(pipe(ready) == 0);
    const auto editor = getpid();
    pid_t child = -1;
    std::atomic_bool quit{};
    // Keep the launching task alive while procfs is enumerated.
    std::thread launcher([&] {
        child = fork(); assert(child >= 0);
        if (!child) {
            if (prctl(PR_SET_PDEATHSIG, SIGKILL) || getppid() != editor) _exit(2);
            static volatile char resident[16 * 1024 * 1024];
            for (std::size_t i = 0; i < sizeof resident; i += 4096) resident[i] = 1;
            if (write(ready[1], "y", 1) != 1) _exit(3);
            for (;;) (void)getpid();
        }
        while (!quit.load()) std::this_thread::sleep_for(10ms);
    });
    char started{}; assert(read(ready[0], &started, 1) == 1);
    const auto cached = sample_epoch_resources();
    assert(cached.captured_at_ms == first.captured_at_ms && cached.process_count == 1);
    std::this_thread::sleep_for(1100ms);
    const auto added = sample_epoch_resources();
    assert(added.process_count == 2 && !added.cpu_valid && added.memory_valid);
    assert(added.resident_memory_bytes >= first.resident_memory_bytes + 12 * 1024 * 1024);
    std::this_thread::sleep_for(1100ms);
    const auto busy = sample_epoch_resources();
    assert(busy.cpu_valid && busy.cpu_busy_fraction > 0 && busy.cpu_busy_fraction <= 1);
    assert(!busy.gpu.busy_valid && !busy.gpu.memory_valid && !busy.coverage_complete);
    quit = true; launcher.join(); assert(kill(child, SIGKILL) == 0); assert(waitpid(child, nullptr, 0) == child);
    close(ready[0]); close(ready[1]);
    std::this_thread::sleep_for(1100ms);
    const auto retired = sample_epoch_resources();
    assert(retired.process_count == 1 && !retired.cpu_valid && retired.memory_valid);
    std::this_thread::sleep_for(1100ms);
    assert(sample_epoch_resources().cpu_valid);
}
''', encoding="utf-8")
        records = compile_and_run(self, fixture, report, includes=(OWNER.parent,))
        (report / "result.json").write_text(json.dumps({"owner_sha256": hashlib.sha256(OWNER.read_bytes()).hexdigest(),
            "invocations": records, "observed": "Editor and direct child CPU and RSS, churn warmup, unavailable GPU"},
            indent=2) + "\n", encoding="utf-8")

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
            "    GpuResourceSample sample_gpu_telemetry()", "    EpochResourceSample sample_epoch_resources()",
            "    ResourceSample sample_host_resources()"))
        code = sampler.split("        void run_gpu_telemetry(", 1)
        sampler = code[0] + state + "        void run_gpu_telemetry(" + code[1]
        kit = windows_sdk_includes(Path("C:/Program Files (x86)/Windows Kits/10"))[-1]
        before = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in (OWNER, MODULE, Path(__file__))}
        records = []
        for enabled in (1, 0):
            lane = report / str(enabled)
            lane.mkdir()
            fixture = lane / "consumer.cpp"
            fixture.write_text("#define NOMINMAX\n#include <Windows.h>\n#define EPOCH_HAS_NVML_TELEMETRY "
                + str(enabled) + "\n" + PREFIX + types() + PORTS + WORKER_PORTS
                + sampler + "\n" + functions + REPAIRED_MAIN,
                encoding="utf-8")
            records.extend(compile_and_run(self, fixture, lane, includes=(kit / "um", kit / "shared"),
                                           windows_flags=("/W4", "/WX")))
        self.assertEqual(before, {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                for path in (OWNER, MODULE, Path(__file__))})
        (report / "result.json").write_text(json.dumps({"passed": True, "source_sha256": before,
            "driver_calls": False, "actual_owner": "GPU demand, worker, native thread lifecycle, publication and host sampler bodies",
            "proved": ["initial status never starts GPU", "CPU/RAM read finishes before blocked GPU release",
                "passive CPU/RAM reads keep driver unloaded, explicit demand resumes and no query repeats without demand",
                "zero-budget stop, pending-restart refusal, cancelled late publication",
                "completion cadence, GPU age expiry and overlong-query rejection", "worker DLL cleanup precedes join",
                "Windows without NVML still samples requested process-family CPU/RAM; GPU stays unavailable"],
            "invocations": records}, indent=2) + "\n", encoding="utf-8")

    def test_native_epoch_process_family(self):
        if os.name != "nt":
            self.skipTest("Windows owned process and Job accounting")
        source = OWNER.read_bytes()
        report = ROOT / "Artifacts/Reports/epoch-process-observation" / str(time.time_ns())
        report.mkdir(parents=True)
        fixture = report / "consumer.cpp"
        declarations = ("namespace epochengine::platform::work_admission {\n"
            + "enum class GpuTelemetrySource : std::uint8_t { unavailable, nvidia_nvml, windows_pdh_process };\n"
            + types() + "\nstd::uint64_t now_milliseconds() noexcept;\n}\n")
        body = source.decode("utf-8").replace("module;", "", 1).replace(
            "module platform.work_admission;", declarations, 1)
        fixture.write_text(body + NATIVE_PROCESS_MAIN, encoding="utf-8")
        kit = windows_sdk_includes(Path("C:/Program Files (x86)/Windows Kits/10"))[-1]
        records = compile_and_run(self, fixture, report, includes=(OWNER.parent, kit / "um", kit / "shared"),
            windows_flags=("/W4", "/WX", "/DEPOCH_ENABLE_NVIDIA_TELEMETRY=0"))
        self.assertEqual(OWNER.read_bytes(), source)
        (report / "result.json").write_text(json.dumps({"passed": True,
            "owner_sha256": hashlib.sha256(source).hexdigest(),
            "driver_calls": False,
            "proved": ["foreign process excluded", "exact owned job child identities captured",
                "exited member CPU persists while another owned member is alive",
                "closing the original fixture Job retires its surviving member", "lease reuse changes generation",
                "jobless process root has partial coverage", "owned family settles after retirement",
                "GPU LUID and physical engine grouped independently, identical aliases collapsed",
                "conflicting duplicates and ambiguous linked GPU capacity are partial"],
            "invocations": records}, indent=2) + "\n", encoding="utf-8")


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
