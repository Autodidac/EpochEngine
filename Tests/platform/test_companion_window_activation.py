"""Execute the real existing-window request route without native windows/GPU."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


class CompanionWindowActivation(unittest.TestCase):
    def test_route_uses_live_window_and_its_owner_without_creating_a_context(self):
        module = (ROOT/'Engine/modules/context.multiplexer.ixx').read_text(encoding='utf-8')
        request = definition(module, '    export struct DetachedContextWindowRequest', True).replace('export ', '', 1)
        linux = definition(module, '        bool OpenDetachedContextWindow(const DetachedContextWindowRequest&)')
        host = (ROOT/'Engine/src/renderers/host/host.context_win.cpp').read_text(encoding='utf-8')
        dispatch = definition(host, '    bool MultiContextManager::OpenDetachedContextWindow(')
        branch = definition(host, '        if (request.activate_existing_only)')
        runtime = (ROOT/'Engine/src/epoch/implementation/epoch.session_runtime.inl').read_text(encoding='utf-8')
        legacy = (ROOT/'Engine/src/epoch/implementation/epoch.session_editor_launcher.inl').read_text(encoding='utf-8')
        forwarding = [definition(source, '                            for (const auto& route : editor_frame.companion_pane_activations)')
                      for source in (runtime, legacy)]
        core = (ROOT/'Engine/modules/editor.core.ixx').read_text(encoding='utf-8')
        frame_types = '\n'.join(definition(core, marker, True).replace('export ', '', 1)
                                for marker in ('    export enum class EditorCommand', '    export struct EditorFrameResult'))
        source = r'''
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
using HWND=std::uintptr_t;using DWORD=std::uint32_t;using LPARAM=std::intptr_t;
constexpr int FALSE=0,GA_ROOT=2,SW_RESTORE=9,WM_EPOCH_OPEN_DETACHED_CONTEXT=100;
enum class ContextType{None,OpenGL};
REQUEST
namespace core{using ::ContextType;}
namespace gui{struct WidgetBounds{};}
namespace epochengine::core{using ::DetachedContextWindowRequest;}
FRAME_TYPES
struct Context{};
DWORD currentThread=12,foregroundPidFixture=9;
HWND foreground=300,invalidWindow=0;bool iconic{};
int restored{},raised{},foregrounded{},focuses{},apiCalls{};
int registryLockDepth{},activationCallbacks{};
std::function<void()> activationCallback;
std::function<void()> registryUnlockCallback;
struct RegistryMutex{void lock(){++registryLockDepth;}void unlock(){--registryLockDepth;if(registryUnlockCallback)registryUnlockCallback();}};
void native_activation(){assert(registryLockDepth==0);++activationCallbacks;if(activationCallback)activationCallback();}
std::vector<std::pair<HWND,DWORD>> focused;
std::vector<std::unique_ptr<DetachedContextWindowRequest>> postedRequests;
DWORD GetCurrentThreadId(){return currentThread;}DWORD GetCurrentProcessId(){return 9;}
HWND GetForegroundWindow(){return foreground;}
DWORD GetWindowThreadProcessId(HWND h,DWORD*pid){
 if(pid)*pid=h==foreground?foregroundPidFixture:h==777?88:9;
 return h==0?0:h==200||h==777?34:12;
}
int IsWindow(HWND h){return h!=0&&h!=invalidWindow;}
HWND GetAncestor(HWND h,int){return h?300:0;}
int IsIconic(HWND){return iconic;}
void ShowWindow(HWND,int command){native_activation();assert(command==SW_RESTORE);++restored;}
void BringWindowToTop(HWND){native_activation();++raised;}
void SetForegroundWindow(HWND){native_activation();++foregrounded;}
int PostMessageW(HWND,int message,int,LPARAM p){
 assert(message==WM_EPOCH_OPEN_DETACHED_CONTEXT);
 postedRequests.emplace_back(reinterpret_cast<DetachedContextWindowRequest*>(p));return 1;
}
void focus_gui_mouse_target(HWND h){native_activation();++focuses;focused.emplace_back(h,currentThread);}
struct Queue{std::vector<std::function<void()>> entries;
 void enqueue(std::function<void()> f){entries.push_back(std::move(f));}
 void drain(){auto commands=std::exchange(entries,{});for(auto& f:commands)f();}
};
struct Window{std::string guiRoute;HWND native{100};
 std::atomic<bool>running{true},retirementQueued{false};bool closing{};
 Queue ownerThreadCommandQueue;
 bool get_should_close()const{return closing;}
};
HWND owner_thread_dock_handle(Window*w){return w&&IsWindow(w->native)?w->native:0;}
HWND primary_window_handle(Window*w){return owner_thread_dock_handle(w);}
struct MultiContextManager;MultiContextManager*g_activeManager{};
struct MultiContextManager{
 std::atomic<bool>running{true};DWORD uiThreadId{12};HWND parent{300};
 RegistryMutex windowsMutex;std::vector<std::unique_ptr<Window>> windows;
 inline static MultiContextManager*s_activeInstance{};int creates{};
 bool OpenDetachedContextWindow(const DetachedContextWindowRequest&request);
 bool CreateDetachedContextWindowOnOwnerThread(const DetachedContextWindowRequest&request){
  assert(currentThread==uiThreadId);
  BRANCH
  ++creates;return true;
 }
};
DISPATCH
struct LinuxManager{LINUX};
namespace logger{enum class LogLevel{WARN};
 struct Log{template<class...Args>void logf(LogLevel,std::source_location,const char*,Args&&...){}};
 Log get(const char*){return {};}
}
constexpr const char*kEditorLog="editor";
int main(){
 MultiContextManager mgr;auto window=std::make_unique<Window>();
 window->guiRoute="pane.gui_hierarchy";mgr.windows.push_back(std::move(window));
 activationCallback=[&]{std::scoped_lock reentrant(mgr.windowsMutex);assert(registryLockDepth==1&&mgr.windows.size()==1);};
 const DetachedContextWindowRequest request{.gui_route="pane.gui_hierarchy",.activate_existing_only=true};
 mgr.uiThreadId=0;assert(!mgr.OpenDetachedContextWindow(request)&&mgr.creates==0&&focuses==0);mgr.uiThreadId=12;
 assert(mgr.OpenDetachedContextWindow(request)&&mgr.creates==0&&mgr.windows.size()==1&&focuses==1);
 assert((focused.back()==std::pair<HWND,DWORD>{100,12}));
 // Native activation preserves maximized/normal placement; only minimized restores.
 assert(restored==0);iconic=true;assert(mgr.OpenDetachedContextWindow(request));
 assert(restored==1);iconic=false;
 const auto beforeFocus=focuses;
 auto missing=request;missing.gui_route="pane.properties";
 assert(!mgr.OpenDetachedContextWindow(missing)&&mgr.creates==0&&focuses==beforeFocus);
 auto empty=request;empty.gui_route.clear();assert(!mgr.OpenDetachedContextWindow(empty));
 mgr.windows[0]->closing=true;assert(!mgr.OpenDetachedContextWindow(request));mgr.windows[0]->closing=false;
 mgr.windows[0]->retirementQueued=true;assert(!mgr.OpenDetachedContextWindow(request));mgr.windows[0]->retirementQueued=false;
 mgr.windows[0]->running=false;assert(!mgr.OpenDetachedContextWindow(request));mgr.windows[0]->running=true;
 invalidWindow=100;assert(!mgr.OpenDetachedContextWindow(request));invalidWindow=0;
 foregroundPidFixture=88;assert(!mgr.OpenDetachedContextWindow(request));foregroundPidFixture=9;
 registryUnlockCallback=[] {foregroundPidFixture=88;};
 assert(!mgr.OpenDetachedContextWindow(request));registryUnlockCallback={};foregroundPidFixture=9;
 mgr.windows[0]->native=777;assert(!mgr.OpenDetachedContextWindow(request));mgr.windows[0]->native=100;
 assert(focuses==beforeFocus&&mgr.creates==0);
 // Render-thread callers use the existing owner message, then the backend queue.
 currentThread=34;mgr.windows[0]->native=200;
 assert(mgr.OpenDetachedContextWindow(request)&&postedRequests.size()==1&&focuses==beforeFocus);
 currentThread=12;assert(mgr.CreateDetachedContextWindowOnOwnerThread(*postedRequests.back()));postedRequests.clear();
 assert(mgr.windows[0]->ownerThreadCommandQueue.entries.size()==1&&focuses==beforeFocus);
 currentThread=34;mgr.windows[0]->ownerThreadCommandQueue.drain();
 assert((focused.back()==std::pair<HWND,DWORD>{200,34}));
 // A close between posting and owner execution cannot produce a ghost window/focus.
 currentThread=12;assert(mgr.OpenDetachedContextWindow(request));
 const auto liveFocus=focuses;mgr.windows[0]->closing=true;currentThread=34;
 mgr.windows[0]->ownerThreadCommandQueue.drain();assert(focuses==liveFocus);
 currentThread=12;mgr.windows[0]->closing=false;
 assert(mgr.OpenDetachedContextWindow(request));foregroundPidFixture=88;currentThread=34;
 mgr.windows[0]->ownerThreadCommandQueue.drain();assert(focuses==liveFocus);
 foregroundPidFixture=9;currentThread=12;
 auto forward=[&](const auto&editor_frame){RUNTIME_FORWARD};
 auto legacyForward=[&](const auto&editor_frame){LEGACY_FORWARD};
 EditorFrameResult editor_frame;
 for(int frame=0;frame<100;++frame){forward(editor_frame);legacyForward(editor_frame);}
 assert(mgr.windows[0]->ownerThreadCommandQueue.entries.empty());
 editor_frame.command=EditorCommand::Exit;editor_frame.command_argument="keep";
 editor_frame.companion_pane_activations[0]="pane.gui_hierarchy";
 forward(editor_frame);assert(editor_frame.command==EditorCommand::Exit&&editor_frame.command_argument=="keep");
 assert(mgr.windows[0]->ownerThreadCommandQueue.entries.size()==1);
 mgr.windows[0]->ownerThreadCommandQueue.entries.clear();
 // Existing Linux detached hosts are explicitly unsupported, not fake parity.
 LinuxManager linux;assert(!linux.OpenDetachedContextWindow(request));
 // Ordinary explicit Open Window behavior still uses its existing creation path.
 DetachedContextWindowRequest normal;assert(mgr.OpenDetachedContextWindow(normal)&&mgr.creates==1);
 assert(activationCallbacks>0&&registryLockDepth==0);
}
'''
        for key, value in [('REQUEST', request), ('FRAME_TYPES', frame_types), ('BRANCH', branch),
                           ('DISPATCH', dispatch), ('LINUX', linux),
                           ('RUNTIME_FORWARD', forwarding[0]), ('LEGACY_FORWARD', forwarding[1])]:
            source = source.replace(key, value)
        parent = ROOT/'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='companion-native-activation-', dir=parent))
        try:
            cpp = scratch/'contract.cpp'
            binary = scratch/'contract'
            cpp.write_text(source, encoding='utf-8')
            if os.name == 'nt':
                versions = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
                sdk = Path('C:/Program Files (x86)/Windows Kits/10')
                kits = sorted((sdk/'Include').glob('*'))
                if not versions or not kits:
                    self.skipTest('MSVC/SDK required')
                tool, kit = versions[-1], kits[-1]
                binary = scratch/'contract.exe'
                args = [str(tool/'bin/Hostx64/x64/cl.exe'), '/nologo', '/utf-8', '/std:c++latest', '/EHsc', '/W4', '/WX',
                        '/I'+str(tool/'include'), '/I'+str(kit/'ucrt'), '/Fo'+str(scratch/'contract.obj'),
                        '/Fe'+str(binary), str(cpp), '/link', '/LIBPATH:'+str(tool/'lib/x64'),
                        '/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'), '/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
            else:
                compiler = shutil.which('clang++') or shutil.which('g++')
                if not compiler:
                    self.skipTest('C++23 compiler required')
                args = [compiler, '-std=c++23', str(cpp), '-o', str(binary)]
            built = subprocess.run(args, cwd=scratch, capture_output=True, text=True, timeout=45)
            self.assertEqual(built.returncode, 0, built.stdout[-6000:]+built.stderr)
            ran = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=10)
            self.assertEqual(ran.returncode, 0, ran.stdout+ran.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)


if __name__ == '__main__':
    unittest.main()
