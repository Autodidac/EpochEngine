"""Actual platform GPU reader/loader and toolbar presentation, CPU ports only."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import msvc_2022_root

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/platform/platform.work_admission.cpp"
MODULE = ROOT / "Engine/modules/platform/platform.work_admission.ixx"
BOOTSTRAP = ROOT / "Engine/src/editor/application/editor.frame_bootstrap.inl"


def consumer():
    source = OWNER.read_text(encoding="utf-8")
    gui = (ROOT / "Engine/modules/gui/gui.engine.ixx").read_text(encoding="utf-8")
    status_type = definition(gui, "    export struct StatusStripItem", True).replace("export ", "")
    tone = definition(gui, "    export enum class SemanticTone", True).replace("export ", "")
    backend = "\n".join(definition(source, marker, marker.endswith(" final")) for marker in (
        "        struct NvmlFunctions final",
        "        [[nodiscard]] GpuResourceSample read_nvml_resources(",
        "        struct NativeNvml final"))
    bootstrap = BOOTSTRAP.read_text(encoding="utf-8")
    start = bootstrap.index("        const bool toolbarCpuValid =")
    end = bootstrap.index("        // Keep status on the PRIMARY chrome row.", start)
    presentation = bootstrap[start:end]
    toolbar = (ROOT / "Engine/src/editor/application/editor.frame_toolbar_dock_targets.inl").read_text(encoding="utf-8")
    start = toolbar.index("        const std::span<const gui::StatusStripItem> resourceMinimumItems")
    minimum = toolbar[start:toolbar.index("        const float minimumMenuReserve", start)]
    panel = (ROOT / "Engine/src/editor/systems/editor.systems_panel.cpp").read_text(encoding="utf-8")
    gpu_panel = definition(panel, "        void render_gpu_resources(")
    return PREFIX + "\nnamespace gui {\n" + tone + "\n" + status_type + "\n" + WIDTH_PORT + PANEL_PORTS + "\n}\n" + PORTS + backend + gpu_panel + "\n" + PRESENTATION_PREFIX + presentation + minimum + PRESENTATION_CHECKS + MAIN


PREFIX = r'''
#include <Windows.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
// MSVC's template-instantiation C4702 in the existing formatter's terminal
// throw is unrelated to this fixture; retain W4/WX for every owner below.
#pragma warning(push)
#pragma warning(disable:4702)
#include "core.format_text.hpp"
#pragma warning(pop)
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#if __has_include(<nvml.h>)
#include <nvml.h>
#else
// Test-only ABI-shaped ports when the SDK is absent. Production then compiles
// unavailable telemetry; this fallback never acts as an installed SDK.
using nvmlDevice_t=struct nvmlDevice_st*;
enum nvmlReturn_t {NVML_SUCCESS=0,NVML_ERROR_UNINITIALIZED=1,NVML_ERROR_NOT_SUPPORTED=3};
struct nvmlUtilization_t {unsigned gpu,memory;};
struct nvmlMemory_t {unsigned long long total,free,used;};
extern "C" {
nvmlReturn_t nvmlInit_v2(); nvmlReturn_t nvmlShutdown();
nvmlReturn_t nvmlDeviceGetCount_v2(unsigned*);
nvmlReturn_t nvmlDeviceGetHandleByIndex_v2(unsigned,nvmlDevice_t*);
nvmlReturn_t nvmlDeviceGetUtilizationRates(nvmlDevice_t,nvmlUtilization_t*);
nvmlReturn_t nvmlDeviceGetMemoryInfo(nvmlDevice_t,nvmlMemory_t*);
nvmlReturn_t nvmlDeviceGetName(nvmlDevice_t,char*,unsigned);
nvmlReturn_t nvmlDeviceGetEncoderUtilization(nvmlDevice_t,unsigned*,unsigned*);
nvmlReturn_t nvmlDeviceGetDecoderUtilization(nvmlDevice_t,unsigned*,unsigned*);
}
#endif
import platform.work_admission;
using epochengine::platform::work_admission::GpuResourceSample;
using epochengine::platform::work_admission::GpuTelemetrySource;
using epochengine::platform::work_admission::ResourceSample;
namespace platform=epochengine::platform;
namespace epochengine {
std::string GetEngineVersionString(){return "test";}
}
#define CHECK(condition) do {if(!(condition)){std::fprintf(stderr,"GPU telemetry fixture line %d: %s\n",__LINE__,#condition);return __LINE__;}}while(false)
'''

WIDTH_PORT = r'''
struct WidthOptions {std::span<const StatusStripItem> items;};
// Width measurement port records the actual caller's immutable references.
// Production responsive layout/font measurement is a separate existing owner.
float status_strip_minimum_width(WidthOptions options) {
 float result=0;for(const auto& item:options.items)result+=float(item.minimal_width_reference.size());return result;
}
'''

PANEL_PORTS = r'''
std::vector<std::pair<std::string,std::string>> rows;
std::vector<std::string> messages;
void section_heading(std::string_view text){messages.emplace_back(text);}
void property_row(std::string_view name,std::string_view value,float){rows.emplace_back(name,value);}
void wrapped_label(std::string_view text,float){messages.emplace_back(text);}
'''

PORTS = r'''
namespace port {
bool dll=true,initialize_ok=true,device_ok=true,utilization_ok=true,memory_ok=true,encoder_ok=true,decoder_ok=true;
std::string missing;
unsigned count=1,percent=42,load_calls{},free_calls{},init_calls{},shutdown_calls{},query_calls{};
unsigned memory_percent=13,encoder_percent=7,decoder_percent=9,encoder_period=166000,decoder_period=200000;
bool safe_load=true;
std::uint64_t total=16ull<<30,used=4ull<<30,free_bytes=12ull<<30;
nvmlReturn_t initialize(){++init_calls;return initialize_ok?NVML_SUCCESS:NVML_ERROR_NOT_SUPPORTED;}
nvmlReturn_t shutdown(){++shutdown_calls;return NVML_SUCCESS;}
nvmlReturn_t counts(unsigned* out){++query_calls;*out=count;return NVML_SUCCESS;}
nvmlReturn_t device(unsigned index,nvmlDevice_t* out){++query_calls;*out=device_ok&&index==0
    ?reinterpret_cast<nvmlDevice_t>(std::uintptr_t{0x1000}):nullptr;return NVML_SUCCESS;}
nvmlReturn_t utilization(nvmlDevice_t,nvmlUtilization_t* out)
{++query_calls;out->gpu=percent;out->memory=memory_percent;return utilization_ok?NVML_SUCCESS:NVML_ERROR_NOT_SUPPORTED;}
nvmlReturn_t encoder(nvmlDevice_t,unsigned* out,unsigned* period)
{++query_calls;*out=encoder_percent;*period=encoder_period;return encoder_ok?NVML_SUCCESS:NVML_ERROR_NOT_SUPPORTED;}
nvmlReturn_t decoder(nvmlDevice_t,unsigned* out,unsigned* period)
{++query_calls;*out=decoder_percent;*period=decoder_period;return decoder_ok?NVML_SUCCESS:NVML_ERROR_NOT_SUPPORTED;}
nvmlReturn_t memory(nvmlDevice_t,nvmlMemory_t* out)
{++query_calls;*out={total,free_bytes,used};return memory_ok?NVML_SUCCESS:NVML_ERROR_NOT_SUPPORTED;}
nvmlReturn_t name(nvmlDevice_t,char* out,unsigned size)
{const char text[]="NVIDIA test\nboard";if(size<sizeof(text))return NVML_ERROR_NOT_SUPPORTED;
 std::memcpy(out,text,sizeof(text));return NVML_SUCCESS;}
template<class Fn> FARPROC procedure(Fn value){return std::bit_cast<FARPROC>(value);}
}
HMODULE WINAPI port_load(LPCWSTR name,HANDLE file,DWORD flags)
{++port::load_calls;port::safe_load&=std::wstring_view{name}==L"nvml.dll"&&file==nullptr&&flags==LOAD_LIBRARY_SEARCH_SYSTEM32;
 return port::dll?reinterpret_cast<HMODULE>(std::uintptr_t{0x2000}):nullptr;}
FARPROC WINAPI port_proc(HMODULE,LPCSTR name)
{
 if(port::missing==name)return nullptr;
 const std::string_view key{name};
 if(key=="nvmlInit_v2")return port::procedure(&port::initialize);
 if(key=="nvmlShutdown")return port::procedure(&port::shutdown);
 if(key=="nvmlDeviceGetCount_v2")return port::procedure(&port::counts);
 if(key=="nvmlDeviceGetHandleByIndex_v2")return port::procedure(&port::device);
 if(key=="nvmlDeviceGetUtilizationRates")return port::procedure(&port::utilization);
 if(key=="nvmlDeviceGetMemoryInfo")return port::procedure(&port::memory);
 if(key=="nvmlDeviceGetName")return port::procedure(&port::name);
 if(key=="nvmlDeviceGetEncoderUtilization")return port::procedure(&port::encoder);
 if(key=="nvmlDeviceGetDecoderUtilization")return port::procedure(&port::decoder);
 return nullptr;
}
BOOL WINAPI port_free(HMODULE){++port::free_calls;return TRUE;}
#define LoadLibraryExW port_load
#define GetProcAddress port_proc
#define FreeLibrary port_free
'''

PRESENTATION_PREFIX = r'''
namespace previewgrid {float camera_distance_for(const void*){return 13.5f;}}
std::string renderer_name(const void*){return "OpenGL";}
std::string build_configuration_label(){return "fixture";}
int check_toolbar(ResourceSample toolbarResources,std::string_view gpu,std::string_view vram,
    gui::SemanticTone gpuTone,gui::SemanticTone memoryTone)
{
 struct {double chromeFpsSample=60;bool chromeFpsMeasured=true;} editor;
 const std::string_view toolbarArch="x64",toolbarCompiler="MSVC",toolbarOs="Windows";
 const std::string toolbarFps="60 FPS";const unsigned toolbarRunning=0,toolbarQueued=0;
 struct {unsigned idle_workers=4;} toolbarActivity;
 struct {const void* get() const{return nullptr;} operator const void*() const{return nullptr;}} ctx;
 const double chromeThrottle=0;
'''
PRESENTATION_CHECKS = r'''
 CHECK(toolbarStatusItems.size()==10);
 CHECK(resourceMinimumItems.size()==4&&resourceMinimumItems.data()==toolbarStatusItems.data());
 CHECK(minimumResourceWidth==24.0f);
 CHECK(toolbarStatusItems[2].label==gpu&&toolbarStatusItems[3].label==vram);
 CHECK(toolbarStatusItems[2].tone==gpuTone&&toolbarStatusItems[3].tone==memoryTone);
 CHECK(toolbarStatusItems[0].width_reference=="CPU 100%"&&toolbarStatusItems[1].width_reference=="RAM 100%");
 CHECK(toolbarStatusItems[2].width_reference=="GPU 100%");
 CHECK(toolbarStatusItems[3].width_reference==toolbarVramWidthReference);
 CHECK(toolbarStatusItems[2].compact_width_reference=="G 100%"&&toolbarStatusItems[3].minimal_width_reference=="V 100%");
 CHECK(toolbarStatusItems[2].compact_label=="G "+toolbarGpuPercent&&toolbarStatusItems[2].minimal_label==toolbarStatusItems[2].compact_label);
 CHECK(toolbarStatusItems[3].compact_label=="V "+toolbarVramPercent&&toolbarStatusItems[3].minimal_label==toolbarStatusItems[3].compact_label);
 CHECK(toolbarStatusItems[6].label=="Editor 60 FPS"&&toolbarStatusItems[6].compact_label=="60 FPS");
 CHECK(toolbarStatusItems[7].label.find("Queued")!=std::string_view::npos);
 CHECK(toolbarStatusItems[8].label=="OpenGL");
 CHECK(toolbarStatusItems[2].tooltip.find("process")!=std::string_view::npos);
 CHECK(toolbarStatusItems[3].tooltip.find("process")!=std::string_view::npos||!toolbarVramValid);
 if(toolbarGpu.source==GpuTelemetrySource::nvidia_nvml)
  CHECK(toolbarStatusItems[2].tooltip.find("Whole-adapter load")!=std::string_view::npos);
 if(toolbarVramValid)CHECK(toolbarStatusItems[3].tooltip.find("Whole adapter")!=std::string_view::npos);
 CHECK(toolbarStatusItems[0].label=="CPU 20%"&&toolbarStatusItems[1].label=="RAM 50%");
 return 0;
}
'''
MAIN = r'''
int main(){
 try {
  NvmlFunctions absent;
  CHECK(!read_nvml_resources(absent).busy_valid);
  port::dll=false;{NativeNvml native;CHECK(!native.api.initialized);}CHECK(port::init_calls==0&&port::free_calls==0);
  port::dll=true;port::missing="nvmlShutdown";
  {NativeNvml native;CHECK(!native.api.initialized);}CHECK(port::init_calls==0&&port::free_calls==1);
  port::missing.clear();port::initialize_ok=false;
  {NativeNvml native;CHECK(!native.api.initialized);}CHECK(port::init_calls==1&&port::shutdown_calls==0);
  port::initialize_ok=true;
  {NativeNvml native;CHECK(native.api.initialized);
   auto sample=read_nvml_resources(native.api);
   CHECK(sample.source==GpuTelemetrySource::nvidia_nvml&&sample.busy_valid&&sample.busy_fraction==.42);
   CHECK(sample.memory_valid&&sample.dedicated_total_bytes==(16ull<<30)&&sample.dedicated_used_bytes==(4ull<<30));
   CHECK(sample.memory_activity_valid&&sample.memory_activity_fraction==.13);
   CHECK(sample.encoder_valid&&sample.encoder_busy_fraction==.07&&sample.encoder_sample_period_us==166000);
   CHECK(sample.decoder_valid&&sample.decoder_busy_fraction==.09&&sample.decoder_sample_period_us==200000);
   gui::rows.clear();gui::messages.clear();render_gpu_resources(sample,320);
   CHECK(gui::rows.size()==7&&gui::rows[0].second=="42%"&&gui::rows[1].second=="13%");
   CHECK(gui::rows[2].second.find("4.00 / 16.00 GiB used (4294967296 / 17179869184 bytes)")!=std::string::npos);
   CHECK(gui::rows[3].second=="7% / 166.0 ms driver sample"&&gui::rows[4].second=="9% / 200.0 ms driver sample");
   CHECK(gui::rows[5].second.find("unavailable")!=std::string::npos&&gui::rows[6].second.find("not attributed")!=std::string::npos);
   CHECK(sample.adapter_index==0&&sample.adapter_count==1&&std::string_view{sample.adapter_name.data()}=="NVIDIA test board");
   port::count=2;CHECK(read_nvml_resources(native.api).adapter_count==2);
   port::count=129;CHECK(read_nvml_resources(native.api).source==GpuTelemetrySource::unavailable);port::count=1;
   port::device_ok=false;CHECK(!read_nvml_resources(native.api).memory_valid);port::device_ok=true;
   port::percent=101;sample=read_nvml_resources(native.api);CHECK(!sample.busy_valid&&sample.memory_valid);
   CHECK(sample.memory_activity_valid&&sample.memory_activity_fraction==.13);
   port::memory_percent=101;CHECK(!read_nvml_resources(native.api).memory_activity_valid);port::memory_percent=0;
   sample=read_nvml_resources(native.api);CHECK(sample.memory_activity_valid&&sample.memory_activity_fraction==0);
   port::encoder_ok=false;sample=read_nvml_resources(native.api);CHECK(!sample.encoder_valid&&sample.decoder_valid&&sample.memory_valid);
   port::encoder_ok=true;port::decoder_percent=101;CHECK(!read_nvml_resources(native.api).decoder_valid);port::decoder_percent=9;
   port::encoder_percent=0;port::encoder_period=0;sample=read_nvml_resources(native.api);
   CHECK(sample.encoder_valid&&sample.encoder_busy_fraction==0&&sample.encoder_sample_period_us==0);
   native.api.encoder=nullptr;sample=read_nvml_resources(native.api);CHECK(!sample.encoder_valid&&sample.decoder_valid);
   port::percent=0;sample=read_nvml_resources(native.api);CHECK(sample.busy_valid&&sample.busy_fraction==0);
   port::utilization_ok=false;sample=read_nvml_resources(native.api);CHECK(!sample.busy_valid&&sample.busy_fraction==0&&sample.memory_valid);
   port::utilization_ok=true;port::used=port::total+1;CHECK(!read_nvml_resources(native.api).memory_valid);
   port::used=0;port::free_bytes=port::total;CHECK(read_nvml_resources(native.api).memory_valid);
   port::total=0;CHECK(!read_nvml_resources(native.api).memory_valid);port::total=16ull<<30;
   port::memory_ok=false;sample=read_nvml_resources(native.api);CHECK(!sample.memory_valid&&sample.dedicated_total_bytes==0&&sample.busy_valid);
   port::memory_ok=true;port::used=4ull<<30;port::free_bytes=12ull<<30;
   native.api.utilization=nullptr;sample=read_nvml_resources(native.api);CHECK(!sample.busy_valid&&sample.memory_valid);
  }
  CHECK(port::shutdown_calls==1&&port::free_calls==3&&port::safe_load);
  ResourceSample observation{1,32ull<<30,16ull<<30,.2,true,true};
  using Tone=gui::SemanticTone;
  CHECK(check_toolbar(observation,"GPU --","VRAM --",Tone::neutral,Tone::neutral)==0);
  observation.gpu.source=GpuTelemetrySource::nvidia_nvml;observation.gpu.busy_valid=true;
  observation.gpu.memory_valid=true;observation.gpu.adapter_count=1;observation.gpu.dedicated_total_bytes=16ull<<30;
  CHECK(check_toolbar(observation,"GPU 0%","VRAM 0.0/16.0 GiB",Tone::success,Tone::success)==0);
  observation.gpu.busy_fraction=.8;observation.gpu.dedicated_used_bytes=12ull<<30;
  CHECK(check_toolbar(observation,"GPU 80%","VRAM 12.0/16.0 GiB",Tone::warning,Tone::warning)==0);
  observation.gpu.busy_fraction=.95;observation.gpu.dedicated_used_bytes=15ull<<30;
  CHECK(check_toolbar(observation,"GPU 95%","VRAM 15.0/16.0 GiB",Tone::error,Tone::error)==0);
  observation.gpu.busy_fraction=(std::numeric_limits<double>::quiet_NaN)();observation.gpu.dedicated_total_bytes=0;
  CHECK(check_toolbar(observation,"GPU --","VRAM --",Tone::neutral,Tone::neutral)==0);
  gui::rows.clear();gui::messages.clear();render_gpu_resources({},320);CHECK(gui::rows.empty()&&gui::messages.size()==2);
  observation.gpu.source=GpuTelemetrySource::nvidia_nvml;observation.gpu.busy_valid=true;
  observation.gpu.encoder_valid=true;observation.gpu.encoder_busy_fraction=(std::numeric_limits<double>::quiet_NaN)();
  gui::rows.clear();render_gpu_resources(observation.gpu,180);
  CHECK(gui::rows[0].second=="Unavailable"&&gui::rows[2].second=="Unavailable"&&gui::rows[3].second=="Unavailable");
  CHECK(std::ranges::any_of(gui::messages,[](const auto& text){return text.find("All applications and driver use")!=std::string::npos;}));
#if !EPOCH_ENABLE_NVIDIA_TELEMETRY
  CHECK(!platform::work_admission::start_gpu_telemetry());
  CHECK(platform::work_admission::stop_gpu_telemetry(0));
  const auto unavailable=platform::work_admission::sample_host_resources();
  CHECK(unavailable.gpu.source==GpuTelemetrySource::unavailable&&!unavailable.gpu.busy_valid&&!unavailable.gpu.memory_valid);
#endif
  return 0;
 }catch(...){return 250;}
}
'''


class GpuTelemetry(unittest.TestCase):
    def test_actual_reader_toolbar_and_optional_build(self):
        if os.name != "nt":
            self.skipTest("Installed Windows SDK/MSVC CPU lane; other platforms remain unavailable")
        installation = msvc_2022_root()
        if installation is None:
            self.skipTest("Installed MSVC compiler unavailable")
        msbuild = installation / "MSBuild/Current/Bin/amd64/MSBuild.exe"
        self.assertTrue(msbuild.is_file(), "Installed MSVC is missing x64 MSBuild")
        report = ROOT / "Artifacts/Reports/gpu-telemetry-oct29" / str(time.time_ns())
        report.mkdir(parents=True)
        (report / "consumer.cpp").write_text(consumer(), encoding="utf-8")
        cmake = (ROOT / "Engine/CMakeLists.txt").read_text(encoding="utf-8")
        option = next(line for line in cmake.splitlines() if line.startswith("option(EPOCH_ENABLE_NVIDIA_TELEMETRY "))
        start = cmake.index("if(EPOCH_ENABLE_NVIDIA_TELEMETRY AND WIN32)")
        sdk = cmake[start:cmake.index("endif()", start)+7]
        start = cmake.index("if(EPOCH_ENABLE_NVIDIA_TELEMETRY AND EPOCH_NVML_INCLUDE_DIR)", cmake.index("target_compile_definitions(epoch PRIVATE")-250)
        include = cmake[start:cmake.index("endif()", start)+7].replace("target_include_directories(epoch ", "target_include_directories(gpu_ports ")
        flag = next(line.strip().removesuffix(")") for line in cmake.splitlines() if line.strip().startswith("EPOCH_ENABLE_NVIDIA_TELEMETRY=$"))
        (report / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(GpuTelemetryPorts LANGUAGES CXX)
{option}
{sdk}
add_executable(gpu_ports consumer.cpp "{OWNER.as_posix()}")
target_sources(gpu_ports PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES "{MODULE.as_posix()}")
target_compile_features(gpu_ports PRIVATE cxx_std_23)
target_compile_options(gpu_ports PRIVATE /utf-8 /EHsc /permissive- /W4 /WX)
target_include_directories(gpu_ports PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_compile_definitions(gpu_ports PRIVATE {flag})
{include}
''', encoding="utf-8")
        tmp = report / "tmp"
        tmp.mkdir()
        env = os.environ.copy()
        for key in ("TEMP", "TMP", "TMPDIR"):
            env[key] = str(tmp)
        invocations = []
        owners = [OWNER, MODULE, BOOTSTRAP, ROOT/"Engine/include/engine.config.hpp", ROOT/"Engine/CMakeLists.txt",
                  ROOT/"Engine/src/editor/systems/editor.systems_panel.cpp", ROOT/"Engine/modules/editor/editor.systems_panel.ixx",
                  ROOT/"Tools/MSVC/EpochDiagnostics.props", Path(__file__)]
        hashes = lambda: {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in owners}
        before = hashes()
        def checked(label, command, expected=0):
            started = time.monotonic()
            result = subprocess.run(command, cwd=report, env=env, text=True, capture_output=True, timeout=120)
            (report/(label+".log")).write_text(result.stdout+result.stderr, encoding="utf-8")
            invocations.append({"label":label,"command":command,"exit":result.returncode,"seconds":time.monotonic()-started})
            self.assertEqual(result.returncode, expected, f"{label}: {report}\n{result.stdout[-5000:]}{result.stderr}")
        for enabled in ("ON", "OFF"):
            build = report / enabled
            checked(enabled+"-configure", ["cmake","-S",str(report),"-B",str(build),"-G","Visual Studio 17 2022","-A","x64","-T","host=x64",
                                        "-DCMAKE_TOOLCHAIN_FILE=","-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false", "-DEPOCH_ENABLE_NVIDIA_TELEMETRY="+enabled])
            for config in ("Debug","Release"):
                checked(enabled+"-"+config+"-build", ["cmake","--build",str(build),"--config",config,"--parallel","1"])
                checked(enabled+"-"+config+"-run", [str(build/config/"gpu_ports.exe")])
        project=report/"flags.proj"
        project.write_text(f'''<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<Import Project="{ROOT/'Tools/MSVC/EpochDiagnostics.props'}"/>
<Target Name="Print" DependsOnTargets="EpochValidateNvidiaTelemetry"><Message Importance="High" Text="gpu=$(EpochNvidiaTelemetryDefine)"/></Target>
</Project>''',encoding="utf-8")
        for value, expected in (("true","1"),("false","0"),("1","1"),("0","0")):
            checked("flags-"+value, [str(msbuild),str(project),"/nologo","/t:Print","/p:EpochNvidiaTelemetry="+value])
            self.assertIn("gpu="+expected,(report/("flags-"+value+".log")).read_text(encoding="utf-8"))
        checked("flags-invalid", [str(msbuild),str(project),"/nologo","/t:Print","/p:EpochNvidiaTelemetry=2"], expected=1)
        self.assertEqual(hashes(),before,"Source changed during actual-owner proof")
        (report/"result.json").write_text(json.dumps({"passed":True,"source_sha256":before,"invocations":invocations,
            "native_nvml_calls":False,"mock_ports":"Only system library resolution and NVML callbacks; actual reader/loader and toolbar bodies, actual platform module+CPP compiled.",
            "scope":"Windows SDK/NVML header compile when installed; GPU/driver/model/render measurements remain untested."},indent=2)+"\n",encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
