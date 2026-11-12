"""Actual startup/discovery owners and console-free supervised console child."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest
from Tests.platform.test_child_close_router import definition

ROOT = Path(__file__).resolve().parents[2]


class DeferredProjectWindow(unittest.TestCase):
    def test_actual_startup_discovery_focus_and_no_console(self):
        if os.name != "nt":
            self.skipTest("Windows supervisor owner")
        owner = ROOT / "Engine/src/platform/platform.child_process.cpp"
        source = owner.read_text(encoding="utf-8")
        cli = (ROOT / "Engine/modules/core/core.commandline.ixx").read_text(encoding="utf-8")
        host = (ROOT / "Engine/src/renderers/host/host.context_win.cpp").read_text(encoding="utf-8")
        editor = (ROOT / "Engine/src/editor/application/editor.application_project_runtime.inl").read_text(encoding="utf-8")
        selected = definition(source, "        struct WindowSearch final", True)
        selected += definition(source, "        [[nodiscard]] bool matches_window_class(")
        selected += definition(source, "        void record_window_diagnostic(")
        selected += definition(source, "        BOOL CALLBACK find_process_window(")
        selected += definition(source, "        [[nodiscard]] std::uint64_t process_window_id(")
        selected += definition(source, "        [[nodiscard]] FocusCode focus_slot(")
        stop_begin = source.index('        if (!slot->expected_window_class.empty())',source.index('    StopCode stop_impl('))
        stop_branch=definition(source[stop_begin:], '        if (!slot->expected_window_class.empty())')
        flags_begin = source.index("            DWORD flags = CREATE_SUSPENDED;")
        flags_end = source.index("            const std::wstring working", flags_begin)
        flags = source[flags_begin:flags_end]
        style_begin = host.index("            const DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN")
        style = host[style_begin:host.index(";", style_begin)+1]
        standalone_begin = host.index("                        (parent\n", host.index("auto make_backend_windows"))
        standalone = host[standalone_begin:host.index("initialX, initialY", standalone_begin)].strip().rstrip(",")
        cli_branch = definition(cli, '            else if (key == "--defer-window-show"sv)').replace("else if", "if", 1)
        launch = definition(editor, "        [[nodiscard]] bool launch_project_runtime(")
        presentation_begin = launch.index("            request.window_mode =")
        presentation_end = launch.index("            if(inEditor&&projectProfile", presentation_begin)
        presentation = launch[presentation_begin:presentation_end]
        self.assertIn("request.suppress_console = true;", launch)
        self.assertIn("slot.suppress_console != request.suppress_console", source)
        self.assertIn("slot->suppress_console = request.suppress_console;", source)
        self.assertIn("        defer_window_show = false;", cli)
        self.assertIn("Stop requested; no separate-window fallback", editor)
        code = r'''
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#define CHECK(c) do{if(!(c))return __LINE__;}while(false)
import platform.child_process;
namespace process=epochengine::platform::child_process;
namespace platform=epochengine::platform;
using WindowMode=process::WindowMode;using FocusCode=process::FocusCode;using LaunchRequest=process::LaunchRequest;
using StopCode=process::StopCode;
using DWORD=std::uint32_t;using BOOL=int;using LPARAM=std::intptr_t;using HWND=void*;
constexpr BOOL TRUE=1,FALSE=0;constexpr DWORD WAIT_TIMEOUT=258,GW_OWNER=4,GWL_STYLE=0,GWL_EXSTYLE=1,WS_CHILD=0x40000000,WS_EX_TOOLWINDOW=0x80;
constexpr DWORD SW_SHOWNORMAL=1,CREATE_SUSPENDED=4,CREATE_UNICODE_ENVIRONMENT=1024,CREATE_NO_WINDOW=0x08000000,EXTENDED_STARTUPINFO_PRESENT=0x00080000;
constexpr DWORD WS_OVERLAPPEDWINDOW=0x00cf0000,WS_CLIPCHILDREN=0x02000000,WS_CLIPSIBLINGS=0x04000000,WS_VISIBLE=0x10000000;
constexpr DWORD WM_CLOSE=0x10,ERROR_SUCCESS=0;
#define CALLBACK
struct RECT{long left{},top{},right{},bottom{};};
struct NativeWindow {DWORD pid{42},thread{8};bool visible{},valid{true},owned{},child{},tool{},enabled{true};std::wstring name{L"EpochChild"};long width{1280},height{800};};
NativeWindow first,helper;std::vector<NativeWindow*> nativeWindows{&first};unsigned shows{},focusCalls{},enumerations{},posts{};bool signaled{},postSucceeds{true};HWND postedWindow{};
NativeWindow& native(HWND h){return *static_cast<NativeWindow*>(h);}
BOOL IsWindowVisible(HWND h){return native(h).visible;}
HWND GetWindow(HWND h,DWORD){return native(h).owned?reinterpret_cast<HWND>(1):nullptr;}
DWORD GetWindowThreadProcessId(HWND h,DWORD* pid){if(!h)return 0;*pid=native(h).pid;return native(h).thread;}
BOOL IsWindow(HWND h){return h&&native(h).valid;}
DWORD WaitForSingleObject(void*,DWORD){return signaled?0:WAIT_TIMEOUT;}
DWORD GetProcessId(void*){return 42;}
BOOL EnumWindows(BOOL(*callback)(HWND,LPARAM),LPARAM argument){++enumerations;for(auto*window:nativeWindows)if(!window->child&&callback(window,argument)==FALSE)break;return TRUE;}
int GetClassNameW(HWND h,wchar_t*output,int capacity){if(!h||!native(h).valid)return 0;const auto count=(std::min)(native(h).name.size(),static_cast<std::size_t>(capacity-1));std::copy_n(native(h).name.data(),count,output);output[count]=0;return static_cast<int>(count);}
BOOL IsWindowEnabled(HWND h){return native(h).enabled;}
BOOL GetClientRect(HWND h,RECT*r){if(!h||!native(h).valid)return FALSE;*r={0,0,native(h).width,native(h).height};return TRUE;}
BOOL PostMessageW(HWND h,DWORD message,std::uintptr_t,std::intptr_t){if(message!=WM_CLOSE)return FALSE;++posts;postedWindow=h;return postSucceeds;}
DWORD GetLastError(){return 5;}
std::intptr_t GetWindowLongPtrW(HWND h,DWORD index){return index==GWL_EXSTYLE?(native(h).tool?WS_EX_TOOLWINDOW:0):(native(h).child?WS_CHILD:0);}
BOOL ShowWindow(HWND h,DWORD){++shows;native(h).visible=true;return TRUE;}
BOOL BringWindowToTop(HWND){return TRUE;}BOOL SetForegroundWindow(HWND){++focusCalls;return TRUE;}HWND SetActiveWindow(HWND h){return h;}
struct ProcessSlot {bool live{true};WindowMode window_mode{WindowMode::normal};void* process{reinterpret_cast<void*>(1)};
 DWORD process_id{42};mutable HWND observed_window{};mutable DWORD observed_window_thread{};
 std::string expected_window_class{},message{};mutable std::string window_diagnostic{};mutable unsigned window_diagnostic_records{};
 bool focus_pending{};std::uint64_t focus_deadline_ns{};int state{};};
bool active(int state){return state==0;}std::uint64_t now_ns(){return 1;}
SELECTED
void revise(int&){} StopCode explicit_stop(ProcessSlot*slot){int value{};STOP_BRANCH return StopCode::requested;}
namespace cli {bool defer_window_show{};}
DWORD parent_style(){@@STYLE@@ return style;}
DWORD child_style(bool parent){return STANDALONE;}
void parse(std::string_view key){using namespace std::literals;using cli::defer_window_show;CLI_BRANCH}
DWORD flags_for(const LaunchRequest& request,bool extended=false){struct{bool attributes_initialized;}captured{extended};FLAGS return flags;}
LaunchRequest presentation(bool inEditor){LaunchRequest request;PRESENTATION return request;}
int main(int argc,char**argv){
 assert(argc==3);const std::filesystem::path root=argv[2];std::filesystem::create_directories(root);
 // Default GUI starts visible. Explicit owned-embedding startup is not visible.
 assert((parent_style()&WS_VISIBLE)!=0 && (child_style(false)&WS_VISIBLE)!=0);
 parse("other");assert(!cli::defer_window_show);parse("--defer-window-show");
 assert(cli::defer_window_show && !(parent_style()&WS_VISIBLE) && !(child_style(false)&WS_VISIBLE));
 assert(child_style(true)==(WS_CHILD|WS_CLIPSIBLINGS|WS_CLIPCHILDREN));
 assert(presentation(true).window_mode==WindowMode::deferred);
 assert(presentation(true).expected_window_class=="EpochChild"&&presentation(false).expected_window_class.empty());
 assert(presentation(true).arguments==std::vector<std::string>{"--defer-window-show"});
 assert(presentation(false).window_mode==WindowMode::normal && presentation(false).arguments.empty());
 LaunchRequest request;assert(!(flags_for(request)&CREATE_NO_WINDOW));request.suppress_console=true;
 assert(flags_for(request)&CREATE_NO_WINDOW);assert(flags_for(request)&CREATE_SUSPENDED);
 assert(flags_for(request,true)&EXTENDED_STARTUPINFO_PRESENT);request.environment.emplace();assert(flags_for(request)&CREATE_UNICODE_ENVIRONMENT);
 ProcessSlot slot;assert(process_window_id(slot)==0);slot.window_mode=WindowMode::deferred;
 assert(process_window_id(slot)==reinterpret_cast<std::uintptr_t>(&first));
 assert(focus_slot(slot)==FocusCode::unsupported && shows==0 && focusCalls==0);
 // Visibility is never authorization to adopt a foreign window or dialog.
 for(unsigned variant=0;variant<5;++variant){slot.observed_window=nullptr;first={};
  if(variant==0)first.pid=99;else if(variant==1)first.owned=true;else if(variant==2)first.thread=0;else if(variant==3)slot.process_id=0;else first.tool=true;
  assert(process_window_id(slot)==0);slot.process_id=42;}
 first={};slot.observed_window=nullptr;assert(process_window_id(slot)!=0);
 first.pid=99;assert(process_window_id(slot)==0);first={};slot.observed_window=nullptr;
 slot.window_mode=WindowMode::hidden;assert(process_window_id(slot)==0 && focus_slot(slot)==FocusCode::unsupported);
 slot.window_mode=WindowMode::normal;first.visible=true;assert(focus_slot(slot)==FocusCode::focused && shows==1);
 first.child=true;assert(focus_slot(slot)==FocusCode::unsupported && shows==1);first={};
 // The actual deferred selector skips a hidden helper encountered first,
 // including an enabled helper with a nonzero client area. Generic callers
 // retain the original first-window behavior.
 helper={};helper.name=L"MSCTFIME UI";nativeWindows={&helper,&first};slot={};slot.window_mode=WindowMode::deferred;
 assert(process_window_id(slot)==reinterpret_cast<std::uintptr_t>(&helper));
 slot={};slot.window_mode=WindowMode::deferred;slot.expected_window_class="EpochChild";
 assert(process_window_id(slot)==reinterpret_cast<std::uintptr_t>(&first)&&!first.visible);
 assert(slot.window_diagnostic.find("class=EpochChild expected=EpochChild")!=std::string::npos);
 // Cached render identity survives reparenting without top-level enumeration.
 first.child=true;const auto beforeEnumeration=enumerations;
 assert(process_window_id(slot)==reinterpret_cast<std::uintptr_t>(&first)&&enumerations==beforeEnumeration);
 assert(explicit_stop(&slot)==StopCode::requested&&postedWindow==&first&&posts==1);
 postSucceeds=false;assert(explicit_stop(&slot)==StopCode::failed&&posts==2&&slot.message.find("Win32 5")!=std::string::npos);postSucceeds=true;
 first.name=L"MSCTFIME UI";assert(process_window_id(slot)==0);first={};slot.observed_window=nullptr;
 assert(process_window_id(slot)==reinterpret_cast<std::uintptr_t>(&first));first.child=true;first.thread=9;
 assert(process_window_id(slot)==0);first={};slot.observed_window=nullptr;
 assert(process_window_id(slot)==reinterpret_cast<std::uintptr_t>(&first));first.child=true;first.pid=99;
 assert(process_window_id(slot)==0);first={};slot.observed_window=nullptr;
 for(unsigned variant=0;variant<3;++variant){first={};slot.observed_window=nullptr;
  if(variant==0)first.enabled=false;else if(variant==1)first.width=0;else first.height=0;
  assert(process_window_id(slot)==0);}
 first={};nativeWindows={&first};slot.observed_window=nullptr;signaled=true;
 assert(process_window_id(slot)==0);signaled=false;
 // Real supervisor + real console-subsystem child: no window/renderer is created.
 request={};request.executable=std::filesystem::absolute(argv[1]);request.working_directory=root;
 request.arguments={(root/"console-state.txt").string()};request.merged_output_path=root/"captured.log";
 request.correlation_key="deferred-window-console-contract";request.suppress_console=true;request.disconnect_standard_input=true;
 request.expected_window_class=std::string(129,'X');assert(process::launch_or_focus(request).code==process::LaunchCode::invalid_request);
 request.expected_window_class="EpochChild\n";assert(process::launch_or_focus(request).code==process::LaunchCode::invalid_request);
 request.expected_window_class.clear();
 const auto launched=process::launch_or_focus(request);assert(launched && launched.owns_new_process());
 const auto completed=process::wait(launched.handle,{},10'000'000'000ull);
 assert(completed && completed.process && completed.process->exit_code_valid && completed.process->exit_code==0);
 assert(process::release(launched.handle));std::ifstream result(root/"console-state.txt");std::string value;result>>value;assert(value=="no-console");
}
'''
        code = (code.replace("SELECTED", selected).replace("@@STYLE@@", style)
                .replace("STANDALONE", standalone).replace("CLI_BRANCH", cli_branch)
                .replace("FLAGS", flags).replace("PRESENTATION", presentation).replace("STOP_BRANCH",stop_branch).replace("assert(","CHECK("))
        proof = ROOT / "Artifacts/Reports/lantern-visible-oct28/deferred-window"
        build = ROOT / "Artifacts/Intermediate/tests/deferred-project-window"
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "deferred.contract.cpp").write_text(code, encoding="utf-8")
        (proof / "console.probe.cpp").write_text(r'''
#include <Windows.h>
#include <fstream>
int main(int argc,char**argv){if(argc!=2)return 1;std::ofstream output(argv[1]);
 const bool absent=GetConsoleWindow()==nullptr;output<<(absent?"no-console":"console");return absent?0:2;}
''', encoding="utf-8")
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(DeferredProjectWindow LANGUAGES CXX)
add_executable(deferred deferred.contract.cpp "{(ROOT/'Engine/src/platform/platform.child_process.cpp').as_posix()}")
target_compile_features(deferred PRIVATE cxx_std_23)
target_compile_options(deferred PRIVATE /UNDEBUG /utf-8 /EHsc /Z7)
target_sources(deferred PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES
 "{(ROOT/'Engine/modules/platform/platform.child_process.ixx').as_posix()}" "{(ROOT/'Engine/modules/core/core.sha256.ixx').as_posix()}")
target_link_libraries(deferred PRIVATE user32 advapi32 bcrypt userenv)
add_executable(console_probe console.probe.cpp)
enable_testing()
add_test(NAME deferred COMMAND deferred "$<TARGET_FILE:console_probe>" "{(proof/'fixture').as_posix()}/$<CONFIG>")
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64",
                     "-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                         ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120,
                                 env=dict(os.environ, MSBUILDDISABLENODEREUSE="1"))
            (proof / f"lane-{index}.log").write_text(run.stdout + run.stderr, encoding="utf-8")
            results.append({"command": command, "exit": run.returncode})
            self.assertEqual(run.returncode, 0, (run.stdout + run.stderr)[-7000:])
        (proof / "result.json").write_text(json.dumps({"passed": True, "commands": results,
            "ownerSha256": hashlib.sha256(owner.read_bytes()).hexdigest(),
            "scope": "Actual supervisor and CLI/host/discovery/focus owner bodies. Recorded HWND ports; real captured console-subsystem child proves CREATE_NO_WINDOW. No renderer, GUI window, model or GPU.",
            "nativeEmbeddingAcceptance": False}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
