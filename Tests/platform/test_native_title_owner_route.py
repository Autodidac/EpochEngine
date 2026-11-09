"""Execute actual title-cache/owner routing with controlled Win32 receipts.

No native window, process, renderer, model or message pump is started.
"""
from Tools.source.owned_source_text import read_owned_source
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from Tests.support.cpp import msvc_2022_toolsets
from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
HOST = read_owned_source(ROOT/'Engine/src/renderers/host/host.context_win.cpp', encoding='utf-8')


class NativeTitleOwnerRoute(unittest.TestCase):
    def test_actual_latest_cache_owner_and_retirement(self):
        helpers = HOST.split('    struct NativeTitleUpdate\n', 1)[1].split(
            '    [[nodiscard]] std::wstring make_native_fps_title', 1)[0]
        helpers = '    struct NativeTitleUpdate\n' + helpers
        retirement = HOST.split('    void forget_native_title_frame_source(', 1)[1].split(
            '\n}\n\nnamespace epochengine::core', 1)[0]
        retirement = '    void forget_native_title_frame_source(' + retirement
        # Compile only the two actual portable title functions. Adjacent native
        # diagnostics are not part of this controlled title-routing consumer.
        backend_titles = 'namespace backend {\n' + definition(
            HOST, '        std::wstring_view BackendDisplayName(') + '\n' + definition(
            HOST, '        std::wstring BuildChildWindowTitle(') + '\n}\n'
        detached = HOST.split('    bool MultiContextManager::CreateDetachedContextWindowOnOwnerThread(', 1)[1]
        selection = detached.split('        std::shared_ptr<Context> ctx;\n', 1)[1].split(
            '        ctx->type = request.type;', 1)[0]
        selection = '        std::shared_ptr<Context> ctx;\n' + selection
        fps_title = HOST.split('    [[nodiscard]] std::wstring make_native_fps_title(', 1)[1].split(
            '    void record_native_title_frame(', 1)[0]
        fps_title = '    [[nodiscard]] std::wstring make_native_fps_title(' + fps_title
        code = r'''
#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "FRAME_RATE_HEADER"
using HWND=std::uintptr_t;
using DWORD=unsigned long;
using UINT=unsigned;
using WPARAM=std::uintptr_t;
using LPARAM=std::intptr_t;
using LONG_PTR=std::intptr_t;
using WNDPROC=LONG_PTR(*)(HWND,UINT,WPARAM,LPARAM);
constexpr int GCLP_WNDPROC=-24;
constexpr UINT WM_APP=0x8000;
namespace epochengine::core {
 struct WindowData {};
 struct MultiContextManager {
  static LONG_PTR ParentProc(HWND,UINT,WPARAM,LPARAM){return 0;}
  static LONG_PTR ChildProc(HWND,UINT,WPARAM,LPARAM){return 0;}
 };
}
struct NativeWindow {DWORD thread{},process{};WNDPROC procedure{};};
std::unordered_map<HWND,NativeWindow> windows;
struct Notification {HWND hwnd{};UINT message{};WPARAM ticket{};LPARAM data{};};
std::vector<Notification> messages;
std::vector<std::wstring> applied;
DWORD currentThread=1;
bool postingSucceeds=true,reentrantUpdate=false;
unsigned wrongOwnerWrites=0;
DWORD GetCurrentProcessId(){return 10;}
DWORD GetCurrentThreadId(){return currentThread;}
DWORD GetWindowThreadProcessId(HWND hwnd,DWORD*process) {
 auto found=windows.find(hwnd);
 if(found==windows.end()){if(process)*process=0;return 0;}
 if(process)*process=found->second.process;
 return found->second.thread;
}
LONG_PTR GetClassLongPtrW(HWND hwnd,int) {
 auto found=windows.find(hwnd);
 return found==windows.end()?0:reinterpret_cast<LONG_PTR>(found->second.procedure);
}
bool PostMessageW(HWND hwnd,UINT message,WPARAM ticket,LPARAM data) {
 if(!postingSucceeds)return false;
 messages.push_back({hwnd,message,ticket,data});return true;
}
bool SetWindowTextW(HWND hwnd,const wchar_t*text);
HELPERS
std::mutex g_nativeTitleFpsMutex;
std::unordered_map<const epochengine::core::WindowData*,int>g_nativeTitleFps;
RETIREMENT
bool SetWindowTextW(HWND hwnd,const wchar_t*text) {
 if(native_title_owner_thread(hwnd)!=currentThread)++wrongOwnerWrites;
 applied.emplace_back(text);
 if(reentrantUpdate){reentrantUpdate=false;queue_native_title_if_owned(hwnd,L"during owner apply");}
 return true;
}
enum class ContextType {None,OpenGL,SDL,SFML,RayLib,Software,Vulkan,DirectX,Noop,Custom};
namespace epochengine::core::cli { bool updater_shell_requested=false; }
BACKEND_TITLES
FPS_TITLE_HELPER_BLOCK
struct Context {void* windowData{};};
struct BackendSlot {std::shared_ptr<Context>master;std::vector<std::shared_ptr<Context>>duplicates;};
std::unordered_map<ContextType,BackendSlot>g_backends;
std::shared_mutex g_backendsMutex;
std::shared_ptr<Context>CloneContext(const Context&){return std::make_shared<Context>();}
unsigned released{},destroyed{};
void ReleaseDC(HWND,int){++released;}
void DestroyWindow(HWND){++destroyed;}
struct Request {ContextType type;};
std::shared_ptr<Context>selectedContext;
std::wstring selectedTitle;
bool select_detached_slot(Request request) {
 HWND hwnd=2;int hdc=0;std::wstring title=L"Epoch GUI";
 ACTUAL_SELECTION
 selectedContext=ctx;selectedTitle=title;return true;
}
#define TITLE_CHECK(...) do {if(!(__VA_ARGS__))return __LINE__;}while(false)
int title_contract() {
 currentThread=30;
 auto&slot=g_backends[ContextType::OpenGL];slot.master=std::make_shared<Context>();
 TITLE_CHECK(select_detached_slot({ContextType::OpenGL}));
 TITLE_CHECK(selectedContext==slot.master && selectedTitle==L"OpenGL 1");
 slot.master->windowData=reinterpret_cast<void*>(1);
 TITLE_CHECK(select_detached_slot({ContextType::OpenGL}));
 TITLE_CHECK(selectedContext==slot.duplicates[0] && selectedTitle==L"OpenGL 2");
 slot.duplicates[0]->windowData=reinterpret_cast<void*>(1);
 TITLE_CHECK(select_detached_slot({ContextType::OpenGL}));
 TITLE_CHECK(selectedContext==slot.duplicates[1] && selectedTitle==L"OpenGL 3");
 slot.duplicates[1]->windowData=reinterpret_cast<void*>(1);
 slot.duplicates[0]->windowData=nullptr; // reuse does not renumber another live slot
 TITLE_CHECK(select_detached_slot({ContextType::OpenGL}));
 TITLE_CHECK(selectedContext==slot.duplicates[0] && selectedTitle==L"OpenGL 2");
 TITLE_CHECK(slot.duplicates.size()==2 && slot.duplicates[1]->windowData!=nullptr);
 for(auto type:{ContextType::SDL,ContextType::SFML,ContextType::RayLib,ContextType::Software,
                 ContextType::Vulkan,ContextType::DirectX,ContextType::Noop,ContextType::Custom}) {
  auto&other=g_backends[type];other.master=std::make_shared<Context>();
  TITLE_CHECK(select_detached_slot({type}));
  TITLE_CHECK(selectedTitle==std::wstring(backend::BackendDisplayName(type))+L" 1");
  other.master->windowData=reinterpret_cast<void*>(1);
  TITLE_CHECK(select_detached_slot({type}));
  TITLE_CHECK(selectedTitle==std::wstring(backend::BackendDisplayName(type))+L" 2");
 }
 TITLE_CHECK(!select_detached_slot({ContextType::None}) && released==1 && destroyed==1);
 epochengine::renderhost::frame_rate::Sample measured;
 TITLE_CHECK(!measured.observe(0,0));TITLE_CHECK(measured.observe(100,1));
 TITLE_CHECK(measured.observe(400,2));
 TITLE_CHECK(measured.live_rate()==300 && measured.average_rate()==200);
 TITLE_CHECK(make_native_fps_title(L"OpenGL 2",measured.average_rate())==L"OpenGL 2 | 200 FPS avg");
 epochengine::core::cli::updater_shell_requested=true;
 TITLE_CHECK(backend::BuildChildWindowTitle(ContextType::OpenGL,0)==L"Epoch Updater Shell");
 TITLE_CHECK(backend::BuildChildWindowTitle(ContextType::OpenGL,1)==L"OpenGL 2");
 return 0;
}
LONG_PTR BackendProc(HWND,UINT,WPARAM,LPARAM){return 0;}
#define CHECK(...) do {if(!(__VA_ARGS__))return __LINE__;}while(false)
int main() {
 using epochengine::core::MultiContextManager;
 epochengine::core::WindowData first,second;
 windows.emplace(1,NativeWindow{20,10,MultiContextManager::ParentProc});
 windows.emplace(2,NativeWindow{30,10,MultiContextManager::ChildProc});
 windows.emplace(3,NativeWindow{40,10,BackendProc}); // e.g. SFML/SDL/GLFW child
 windows.emplace(4,NativeWindow{50,99,MultiContextManager::ChildProc}); // foreign process
 windows.emplace(5,NativeWindow{60,10,MultiContextManager::ChildProc});
 queue_native_title_if_owned(0,L"invalid");queue_native_title_if_owned(3,L"third party");
 queue_native_title_if_owned(4,L"foreign");
 CHECK(messages.empty() && g_nativeFpsTitles.empty() && applied.empty());
 for(unsigned i=0;i<1000;++i)queue_native_title_if_owned(1,L"latest "+std::to_wstring(i));
 CHECK(messages.size()==1 && messages[0].data==0 && applied.empty());
 const auto firstMessage=messages.back();
 apply_pending_native_title(1,firstMessage.ticket); // renderer is not owner
 CHECK(applied.empty() && g_nativeFpsTitles.at(1).pending);
 currentThread=20;apply_pending_native_title(1,firstMessage.ticket);
 CHECK(applied.size()==1 && applied.back()==L"latest 999");
 apply_pending_native_title(1,firstMessage.ticket);CHECK(applied.size()==1);
 currentThread=1;queue_native_title_if_owned(2,L"child latest",&first);
 CHECK(messages.size()==2 && applied.size()==1);
 currentThread=30;apply_pending_native_title(2,messages.back().ticket);
 CHECK(applied.size()==2 && applied.back()==L"child latest");
 // The callback releases the cache lock before native SetWindowText dispatch.
 currentThread=1;queue_native_title_if_owned(1,L"before reentrant update");
 auto message=messages.back();currentThread=20;reentrantUpdate=true;
 apply_pending_native_title(1,message.ticket);
 CHECK(applied.back()==L"before reentrant update" && g_nativeFpsTitles.at(1).pending);
 apply_pending_native_title(1,messages.back().ticket);CHECK(applied.back()==L"during owner apply");
 // A failed post never blocks or falls back to synchronous title mutation.
 currentThread=1;postingSucceeds=false;const auto priorWrites=applied.size();
 queue_native_title_if_owned(2,L"post failed",&first);
 CHECK(!g_nativeFpsTitles.at(2).pending && applied.size()==priorWrites);
 postingSucceeds=true;queue_native_title_if_owned(2,L"retry next sample",&first);
 auto old=messages.back();forget_native_title_window(2);
 queue_native_title_if_owned(2,L"new HWND lifetime",&second);auto fresh=messages.back();
 CHECK(old.ticket!=fresh.ticket);
 currentThread=30;apply_pending_native_title(2,old.ticket);CHECK(applied.size()==priorWrites);
 apply_pending_native_title(2,fresh.ticket);CHECK(applied.back()==L"new HWND lifetime");
 // Source retirement cannot erase a reused handle belonging to another source.
 currentThread=1;queue_native_title_if_owned(5,L"retiring source",&first);
 g_nativeTitleFps[&first]=1;g_nativeTitleFps[&second]=2;
 forget_native_title_frame_source(&first);
 CHECK(g_nativeFpsTitles.contains(1) && g_nativeFpsTitles.at(2).source==&second);
 CHECK(!g_nativeFpsTitles.contains(5));
 CHECK(!g_nativeTitleFps.contains(&first) && g_nativeTitleFps.contains(&second));
 forget_native_title_frame_source(&second);CHECK(!g_nativeFpsTitles.contains(2));
 forget_native_title_window(1);CHECK(g_nativeFpsTitles.empty());
 const auto titleResult=title_contract();if(titleResult)return titleResult;
 CHECK(wrongOwnerWrites==0);
 return 0;
}
'''.replace('HELPERS', helpers).replace('RETIREMENT', retirement).replace(
            'BACKEND_TITLES', backend_titles).replace('FPS_TITLE_HELPER_BLOCK', fps_title).replace(
            'ACTUAL_SELECTION', selection).replace('FRAME_RATE_HEADER',
            (ROOT/'Engine/src/renderers/host/host.frame_rate_sample.hpp').as_posix())
        parent = (ROOT/'build/contract-tests').resolve(); parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='native-title-owner-', dir=parent))
        passed = False
        try:
            source = scratch/'contract.cpp'; source.write_text(code, encoding='utf-8')
            if os.name == 'nt':
                toolsets = msvc_2022_toolsets()
                kitroot = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)'))/'Windows Kits/10'
                kits = sorted((kitroot/'Include').glob('*'))
                if not toolsets or not kits:self.skipTest('Installed MSVC/SDK required')
                toolset,kit=toolsets[-1],kits[-1];binary=scratch/'contract.exe'
                args=[str(toolset/'bin/Hostx64/x64/cl.exe'),'/nologo','/std:c++latest','/EHsc',
                    '/I'+str(toolset/'include'),'/I'+str(kit/'ucrt'),'/I'+str(kit/'shared'),'/I'+str(kit/'um'),
                    '/Fo'+str(scratch/'contract.obj'),'/Fe'+str(binary),str(source),'/link',
                    '/LIBPATH:'+str(toolset/'lib/x64'),'/LIBPATH:'+str(kitroot/'Lib'/kit.name/'um/x64'),
                    '/LIBPATH:'+str(kitroot/'Lib'/kit.name/'ucrt/x64')]
            else:
                compiler=shutil.which('clang++') or shutil.which('g++')
                if not compiler:self.skipTest('C++23 compiler required')
                binary=scratch/'contract';args=[compiler,'-std=c++23',str(source),'-o',str(binary)]
            result=subprocess.run(args,cwd=scratch,capture_output=True,text=True,timeout=90)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(binary)],cwd=scratch,capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,0,'Actual owner-route CPU line '+str(result.returncode))
            passed = True
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent))
            if passed:
                shutil.rmtree(scratch)

    def test_both_epoch_procedures_consume_and_clean_the_cache(self):
        self.assertEqual(HOST.count('apply_pending_native_title(hwnd, static_cast<std::uintptr_t>(wParam));'),2)
        parent=HOST.split('LRESULT CALLBACK MultiContextManager::ParentProc(',1)[1].split(
            'LRESULT CALLBACK MultiContextManager::ChildProc(',1)[0]
        child=HOST.split('LRESULT CALLBACK MultiContextManager::ChildProc(',1)[1]
        nonclient_destroy=parent.split('if (msg == WM_NCDESTROY)',1)[1].split('if (msg == WM_NCCREATE)',1)[0]
        self.assertIn('forget_native_title_window(hwnd);',nonclient_destroy)
        self.assertIn('forget_native_title_window(hwnd)',parent.split('case WM_DESTROY:',1)[1])
        self.assertIn('forget_native_title_window(hwnd)',child.split('case WM_NCDESTROY:',1)[1])
        self.assertIn('forget_native_title_window(hwnd)',child.split('case WM_DESTROY:',1)[1])
        sampler=HOST.split('void record_native_title_frame(',1)[1].split('void forget_native_title_frame_source',1)[0]
        self.assertNotIn('SetWindowText',sampler)
        self.assertEqual(sampler.count('queue_native_title_if_owned('),4)


if __name__=='__main__':unittest.main()
