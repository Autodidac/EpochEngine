"""Private fixed-command supervisor proof; CPU console children, no GUI/GPU."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import time
import unittest
from Tests.platform.test_child_close_router import definition

ROOT=Path(__file__).resolve().parents[2]

class RuntimeMeshControl(unittest.TestCase):
    def test_actual_entrypoints_admit_private_ports_before_cli_parse(self):
        source=(ROOT/'Engine/src/epoch/epoch.engine_legacy.cpp').read_text(encoding='utf-8')
        for marker in ('        (void)cli::parse(argc, argv);',
                       '        const auto cli_result = epochengine::core::cli::parse(argc, argv);'):
            starts=[index for index in range(len(source)) if source.startswith(marker,index)]
            self.assertTrue(starts)
            for index in starts:
                prior=source[:index].splitlines()[-1]
                self.assertIn('initialize_inherited_runtime_control(argc,argv)',prior)
                self.assertIn('return',prior)
        self.assertIn('else if (key == "--owned-runtime-control"sv)',
                      (ROOT/'Engine/modules/core.commandline.ixx').read_text(encoding='utf-8'))

    def test_actual_supervisor_private_command_and_ack(self):
        if os.name!='nt':self.skipTest('Windows exact executable launch admission')
        proof=ROOT/'Artifacts/Reports/project-play-flicker-oct31/control'
        build=ROOT/'Artifacts/Intermediate/tests/runtime-mesh-control'
        proof.mkdir(parents=True,exist_ok=True)
        owner=(ROOT/'Engine/src/platform/platform.child_process.cpp').read_text(encoding='utf-8')
        endpoint_owner='using ControlPort=std::uintptr_t;\n'+ '\n'.join(definition(owner,marker,semicolon) for marker,semicolon in (
            ('        void close_control_port(',False),
            ('        struct OwnedRuntimeControl final',True),
            ('        bool make_control_pipe(',False)))
        (proof/'control.cpp').write_text(r"""
#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <limits>
#include <optional>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <Windows.h>
import platform.child_process;
namespace process=epochengine::platform::child_process;
namespace recorded {using namespace process;RECORDED_ENDPOINT_OWNER}
using namespace std::chrono_literals;
bool endpoint_closed(std::uintptr_t port) noexcept {
 // The managed test host traps invalid-handle queries as0xC0000008. That
 // exact exception is expected only for our just-closed, known pipe handles.
 __try {DWORD flags{};return !GetHandleInformation(reinterpret_cast<HANDLE>(port),&flags);}
 __except(GetExceptionCode()==0xc0000008u?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
}
#define CHECK(...) do{if(!(__VA_ARGS__))return __LINE__;}while(false)
int main(int argc,char**argv){
 CHECK(process::initialize_inherited_runtime_control(argc,argv));
 if(argc>1&&std::string_view(argv[1])=="warm"){std::this_thread::sleep_for(100ms);return 0;}
 if(argc>1&&std::string_view(argv[1])=="timeline"){
  CHECK(process::claim_inherited_runtime_control("project","scene"));const std::filesystem::path root=argv[2];
  for(unsigned turn=0;turn<3000;++turn){
   std::uint64_t sequence{};bool presentation{};
   if(auto command=process::poll_runtime_mesh_command()){
    sequence=command->sequence;{std::ofstream out(root/("mesh-"+std::to_string(sequence)));out<<static_cast<unsigned>(command->mode);}
    for(unsigned n=0;n<1000&&!std::filesystem::exists(root/("release-"+std::to_string(sequence)));++n)std::this_thread::sleep_for(2ms);
    process::acknowledge_runtime_mesh_command(*command,true,command->mode);
   }
   if(auto command=process::poll_runtime_presentation_command()){
    sequence=command->sequence;presentation=true;
    {std::ofstream out(root/("time-"+std::to_string(sequence)));out<<command->sample.seconds<<" "<<command->sample.reverse<<" "<<command->sample.playing;}
    auto stale=*command;stale.sample.seconds+=.125;process::acknowledge_runtime_presentation_command(stale,true);
    for(unsigned n=0;n<1000&&!std::filesystem::exists(root/("release-"+std::to_string(sequence)));++n)std::this_thread::sleep_for(2ms);
    process::acknowledge_runtime_presentation_command(*command,true);
   }
   if(presentation&&sequence==4){std::this_thread::sleep_for(100ms);return 0;}
   std::this_thread::sleep_for(2ms);
  }return 91;
 }
 if(argc>1&&std::string_view(argv[1])=="child"){
  CHECK(!process::claim_inherited_runtime_control("foreign","scene"));
  CHECK(process::claim_inherited_runtime_control("project","scene"));
  const std::filesystem::path root=argv[2];
  bool actualWire{};
  for(unsigned turn=0;turn<3000;++turn){
   if(auto command=process::poll_runtime_mesh_command()){
    auto stale=*command;++stale.sequence;
    process::acknowledge_runtime_mesh_command(stale,true,stale.mode); // Must not acknowledge this generation.
    {std::ofstream received(root/("received-"+std::to_string(command->sequence)));received<<static_cast<unsigned>(command->mode);}
    for(unsigned n=0;n<1000&&!std::filesystem::exists(root/("release-"+std::to_string(command->sequence)));++n)std::this_thread::sleep_for(2ms);
    const bool publish=command->sequence==1; // Second requested frame is refused.
    if(publish)actualWire=command->mode==process::MeshDrawMode::wireframe;
    process::acknowledge_runtime_mesh_command(*command,publish,actualWire?process::MeshDrawMode::wireframe:process::MeshDrawMode::solid);
    if(command->sequence==2){std::this_thread::sleep_for(100ms);return 0;}
   }
   std::this_thread::sleep_for(2ms);
  }
  return 90;
 }
 CHECK(argc==2);
 // Actual production endpoint constructor/close/destructor, known pipe handles
 // only. No global handle-table query or guessed process-wide baseline.
 for(unsigned explicitClose=0;explicitClose<2;++explicitClose){std::array<std::uintptr_t,4> ports{};
  {recorded::OwnedRuntimeControl control;
   CHECK(recorded::make_control_pipe(control.command_read,control.command_write));
   CHECK(recorded::make_control_pipe(control.ack_read,control.ack_write));
   ports={control.command_read,control.command_write,control.ack_read,control.ack_write};
   if(explicitClose){control.close();CHECK(!control.command_read&&!control.command_write&&!control.ack_read&&!control.ack_write);}}
  for(auto port:ports)CHECK(endpoint_closed(port));}
 const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
 wchar_t self[32768]{};CHECK(GetModuleFileNameW(nullptr,self,32768));
 auto image=process::inspect_executable(self);CHECK(image);
 process::LaunchRequest request;request.executable=self;request.working_directory=root;
 request.arguments={"child",root.string()};request.correlation_key="mesh-control-test";
 request.exclusive_group="mesh-control-test";request.merged_output_path=root/"child.log";
 request.expected_executable=image;request.disconnect_standard_input=true;request.suppress_console=true;
 request.runtime_control=process::RuntimeControlLease{17,"project","scene"};
 auto invalid=request;invalid.expected_executable.reset();CHECK(!process::launch_or_focus(invalid));
 // Warm the unchanged discovery/GUI-library supervisor path before measuring
 // whether the opt-in command pipes leave any additional handles behind.
 auto warmRequest=request;warmRequest.runtime_control.reset();warmRequest.arguments={"warm"};warmRequest.correlation_key="warm-control";
 auto warm=process::launch_or_focus(warmRequest);CHECK(warm&&process::wait(warm.handle,{},5'000'000'000ull));CHECK(process::release(warm.handle));
 DWORD handlesBefore{};CHECK(GetProcessHandleCount(GetCurrentProcess(),&handlesBefore));
 auto measure=[&](const char*name){DWORD count{};GetProcessHandleCount(GetCurrentProcess(),&count);std::ofstream evidence(root/"handle-stages.txt",std::ios::app);evidence<<name<<" "<<count<<"\n";};measure("before");
 const auto launched=process::launch_or_focus(request);CHECK(launched.owns_new_process()&&launched);
 const auto handle=launched.handle;auto snapshot=process::snapshot(handle);CHECK(snapshot&&snapshot->standard_input_disconnected);
 auto lease=*request.runtime_control;auto wrongLease=lease;++wrongLease.project_generation;
 auto wrongImage=*image;++wrongImage.size_bytes;
 CHECK(!process::request_runtime_mesh_mode(handle,wrongLease,*image,request.correlation_key,process::MeshDrawMode::wireframe));
 CHECK(!process::request_runtime_mesh_mode(handle,lease,wrongImage,request.correlation_key,process::MeshDrawMode::wireframe));
 CHECK(!process::request_runtime_mesh_mode(handle,lease,*image,"foreign",process::MeshDrawMode::wireframe));
 CHECK(!process::request_runtime_mesh_mode(handle,lease,*image,request.correlation_key,static_cast<process::MeshDrawMode>(9)));
 auto changed=request;changed.runtime_control->scene_id="foreign";CHECK(!process::launch_or_focus(changed));
 for(unsigned sequence=1;sequence<=2;++sequence){
  const auto mode=sequence==1?process::MeshDrawMode::wireframe:process::MeshDrawMode::solid;
  CHECK(process::request_runtime_mesh_mode(handle,lease,*image,request.correlation_key,mode));
  CHECK(!process::request_runtime_mesh_mode(handle,lease,*image,request.correlation_key,mode));
  for(unsigned n=0;n<1000&&!std::filesystem::exists(root/("received-"+std::to_string(sequence)));++n)std::this_thread::sleep_for(2ms);
  CHECK(std::filesystem::exists(root/("received-"+std::to_string(sequence))));
  snapshot=process::snapshot(handle);CHECK(snapshot&&snapshot->runtime_control.status==process::RuntimeControlStatus::pending);
  CHECK(snapshot->runtime_control.acknowledged_sequence==sequence-1);
  {std::ofstream release(root/("release-"+std::to_string(sequence)));release<<"publish";}
  for(unsigned n=0;n<1000;++n){snapshot=process::snapshot(handle);
   if(snapshot&&snapshot->runtime_control.acknowledged_sequence==sequence)break;std::this_thread::sleep_for(2ms);}
  CHECK(snapshot&&snapshot->runtime_control.acknowledged_sequence==sequence);
  CHECK(snapshot->runtime_control.acknowledged_mode==process::MeshDrawMode::wireframe);
  CHECK(snapshot->runtime_control.status==(sequence==1?process::RuntimeControlStatus::accepted:process::RuntimeControlStatus::rejected));
 }
 auto retired=process::wait(handle,{},5'000'000'000ull);CHECK(retired&&retired.process->exit_code==0);CHECK(process::release(handle));measure("after-first");
 CHECK(!process::request_runtime_mesh_mode(handle,lease,*image,request.correlation_key,process::MeshDrawMode::solid));
 // Reused generation cannot adopt an old send or configuration; Stop closes
 // command ports even while the child is waiting to publish its first frame.
 request.correlation_key="mesh-control-stop";request.arguments={"child",(root/"stopped").string()};
 std::filesystem::create_directories(root/"stopped");
 const auto second=process::launch_or_focus(request);CHECK(second&&second.handle!=handle);
 CHECK(!process::request_runtime_mesh_mode(handle,lease,*image,request.correlation_key,process::MeshDrawMode::solid));
 CHECK(process::request_runtime_mesh_mode(second.handle,lease,*image,request.correlation_key,process::MeshDrawMode::wireframe));
 CHECK(process::stop(second.handle,process::StopMode::force)==process::StopCode::forced);
 CHECK(!process::request_runtime_mesh_mode(second.handle,lease,*image,request.correlation_key,process::MeshDrawMode::solid));
 CHECK(process::wait(second.handle,{},5'000'000'000ull));CHECK(process::release(second.handle));measure("after-stop");
 // A single typed pipe keeps mesh ACKs distinct, coalesces explicit seeks and
 // dispatches a waiting mesh before the next timeline cursor.
 const auto timelineRoot=root/"timeline";std::filesystem::create_directories(timelineRoot);
 request.correlation_key="timeline-control";request.arguments={"timeline",timelineRoot.string()};
 const auto timed=process::launch_or_focus(request);CHECK(timed);
 process::RuntimePresentation timeSample{1,120,1,true,true};
 CHECK(!process::request_runtime_presentation(timed.handle,wrongLease,*image,request.correlation_key,timeSample));
 CHECK(!process::request_runtime_presentation(timed.handle,lease,wrongImage,request.correlation_key,timeSample));
 CHECK(!process::request_runtime_presentation(timed.handle,lease,*image,"foreign",timeSample));
 for(double invalid:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
  auto bad=timeSample;bad.seconds=invalid;CHECK(!process::request_runtime_presentation(timed.handle,lease,*image,request.correlation_key,bad));}
 auto bad=timeSample;bad.duration_seconds=.5;CHECK(!process::request_runtime_presentation(timed.handle,lease,*image,request.correlation_key,bad));
 bad=timeSample;bad.rate=9;CHECK(!process::request_runtime_presentation(timed.handle,lease,*image,request.correlation_key,bad));
 CHECK(process::request_runtime_mesh_mode(timed.handle,lease,*image,request.correlation_key,process::MeshDrawMode::wireframe));
 CHECK(process::request_runtime_presentation(timed.handle,lease,*image,request.correlation_key,timeSample));
 timeSample.seconds=3.25;CHECK(process::request_runtime_presentation(timed.handle,lease,*image,request.correlation_key,timeSample));
 auto waitFile=[&](std::string name){for(unsigned n=0;n<1000&&!std::filesystem::exists(timelineRoot/name);++n){(void)process::snapshot(timed.handle);std::this_thread::sleep_for(2ms);}return std::filesystem::exists(timelineRoot/name);};
 CHECK(waitFile("mesh-1"));CHECK(!std::filesystem::exists(timelineRoot/"time-2"));
 {std::ofstream release(timelineRoot/"release-1");release<<"publish";}
 CHECK(waitFile("time-2"));double received{};{std::ifstream in(timelineRoot/"time-2");in>>received;}CHECK(received==3.25);
 snapshot=process::snapshot(timed.handle);CHECK(snapshot&&snapshot->runtime_control.acknowledged_sequence==1&&snapshot->runtime_control.presentation_pending);
 CHECK(snapshot->runtime_control.presentation_acknowledged_sequence==0); // Wrong-payload ACK was ignored.
 timeSample.seconds=8;timeSample.playing=false;
 CHECK(process::request_runtime_presentation(timed.handle,lease,*image,request.correlation_key,timeSample));
 CHECK(process::request_runtime_mesh_mode(timed.handle,lease,*image,request.correlation_key,process::MeshDrawMode::solid));
 {std::ofstream release(timelineRoot/"release-2");release<<"publish";}
 CHECK(waitFile("mesh-3"));CHECK(!std::filesystem::exists(timelineRoot/"time-4"));
 {std::ofstream release(timelineRoot/"release-3");release<<"publish";}
 CHECK(waitFile("time-4"));{std::ifstream in(timelineRoot/"time-4");in>>received;}CHECK(received==8);
 {std::ofstream release(timelineRoot/"release-4");release<<"publish";}
 for(unsigned n=0;n<1000;++n){snapshot=process::snapshot(timed.handle);if(snapshot&&snapshot->runtime_control.presentation_acknowledged_sequence==4)break;std::this_thread::sleep_for(2ms);}
 CHECK(snapshot&&snapshot->runtime_control.presentation_accepted&&!snapshot->runtime_control.presentation_pending&&snapshot->runtime_control.acknowledged_sequence==3);
 CHECK(process::wait(timed.handle,{},5'000'000'000ull));CHECK(process::release(timed.handle));
 CHECK(!process::request_runtime_presentation(timed.handle,lease,*image,request.correlation_key,timeSample));
 request.correlation_key="timeline-stop";request.arguments={"timeline",(root/"timeline-stop").string()};std::filesystem::create_directories(root/"timeline-stop");
 const auto stoppedTime=process::launch_or_focus(request);CHECK(stoppedTime);
 CHECK(process::request_runtime_presentation(stoppedTime.handle,lease,*image,request.correlation_key,timeSample));
 CHECK(process::stop(stoppedTime.handle,process::StopMode::force)==process::StopCode::forced);
 CHECK(!process::request_runtime_presentation(stoppedTime.handle,lease,*image,request.correlation_key,timeSample));
 CHECK(process::wait(stoppedTime.handle,{},5'000'000'000ull));CHECK(process::release(stoppedTime.handle));
 // A missing working directory fails after pipe preparation and owns cleanup.
 auto failedRequest=request;failedRequest.working_directory=root/"missing-directory";failedRequest.correlation_key="missing-control";
 const auto failed=process::launch_or_focus(failedRequest);CHECK(!failed);
 if(failed.owns_new_process())CHECK(process::release(failed.handle));measure("after-failure");

 DWORD handlesAfter{};CHECK(GetProcessHandleCount(GetCurrentProcess(),&handlesAfter));{std::ofstream evidence(root/"handle-counts.txt");evidence<<handlesBefore<<" "<<handlesAfter;}// Overall first-use library handles are diagnostic, not endpoint ownership evidence.
}
""".replace("RECORDED_ENDPOINT_OWNER",endpoint_owner),encoding='utf-8')
        (proof/'CMakeLists.txt').write_text(f"""cmake_minimum_required(VERSION 3.30)
project(RuntimeMeshControl LANGUAGES CXX)
add_executable(control control.cpp "{(ROOT/'Engine/src/platform/platform.child_process.cpp').as_posix()}")
target_compile_features(control PRIVATE cxx_std_23)
target_compile_options(control PRIVATE /utf-8 /EHsc /Z7)
target_link_libraries(control PRIVATE bcrypt userenv advapi32)
target_sources(control PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES
 "{(ROOT/'Engine/modules/platform.child_process.ixx').as_posix()}" "{(ROOT/'Engine/modules/core.sha256.ixx').as_posix()}")
enable_testing()
add_test(NAME control COMMAND control "${{CMAKE_CURRENT_BINARY_DIR}}/case-$<CONFIG>-{time.time_ns()}")
""",encoding='utf-8')
        commands=[['cmake','-S',str(proof),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64','-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false']]
        for config in ('Debug','Release'):
            commands += [['cmake','--build',str(build),'--config',config,'--parallel','1'],['ctest','--test-dir',str(build),'-C',config,'--output-on-failure']]
        results=[]
        for number,command in enumerate(commands):
            run=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=90,env=dict(os.environ,MSBUILDDISABLENODEREUSE='1'))
            (proof/f'lane-{number}.log').write_text(run.stdout+run.stderr,encoding='utf-8')
            results.append(dict(command=command,exit=run.returncode))
            self.assertEqual(run.returncode,0,(run.stdout+run.stderr)[-8000:])
        (proof/'result.json').write_text(json.dumps(dict(native=False,commands=results,sources={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'Engine/modules/platform.child_process.ixx',ROOT/'Engine/src/platform/platform.child_process.cpp')}),indent=2),encoding='utf-8')
