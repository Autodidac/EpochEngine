"""Run actual backend dimension publication and input-adapter admission on CPU.

Native API receipts are mocked; no window, GPU, model or message pump is used.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from Tools.source.owned_source_text import read_owned_source


ROOT = Path(__file__).resolve().parents[2]


class BackendSurfaceInput(unittest.TestCase):
    def test_actual_surface_dimensions_and_input_adapter_admission(self):
        sdl = read_owned_source(ROOT / 'Engine/src/renderers/sdl/core.context_sdl.cpp')
        refresh = sdl.split('    void refresh_dimensions(', 1)[1].split(
            '    void sync_docked_child_size(', 1)[0]
        refresh = '    void refresh_dimensions(' + refresh
        state = read_owned_source(ROOT / 'Engine/modules/sdl.state.ixx')
        dimensions = state.split('    struct PresentationDimensions', 1)[1].split(
            '    enum class SurfaceLifecycleEvent', 1)[0]
        dimensions = '    struct PresentationDimensions' + dimensions
        host = read_owned_source(ROOT / 'Engine/src/renderers/host/host.context_win.cpp')
        adapter = host.split('    void MultiContextManager::AttachBackendInputAdapter(', 1)[1].split(
            '    namespace backend', 1)[0]
        adapter = '    void MultiContextManager::AttachBackendInputAdapter(' + adapter
        multiplexer = read_owned_source(ROOT / 'Engine/modules/context.multiplexer.ixx')
        declaration = next(line.strip() for line in multiplexer.splitlines()
            if 'void AttachBackendInputAdapter(HWND hwnd) noexcept;' in line)
        self.assertTrue(declaration.startswith('static '))
        platform = read_owned_source(ROOT / 'Engine/dep/EpochPlatformEngine/src/platform/platform.window.cpp')
        admission = platform.split('    bool attach_native_input(', 1)[1].split(
            '    void cooperative_pause(', 1)[0]
        admission = '    bool attach_native_input(' + admission
        sfml = read_owned_source(ROOT / 'Engine/src/renderers/sfml/core.context_sfml.cpp')
        sfml_dimensions = sfml.split('    void apply_view_size()', 1)[1].split(
            '    void sync_docked_child_size(', 1)[0]
        sfml_dimensions = '    void apply_view_size()' + sfml_dimensions
        evidence = 'struct SdlInputEvidence' + sdl.split('struct SdlInputEvidence', 1)[1].split('    SdlInputEvidence s_inputEvidence', 1)[0]
        routing_source = read_owned_source(ROOT / 'Engine/src/renderers/host/implementation/host.window_interaction.inl')
        focus_helper = '    struct MouseFocusEvidence' + routing_source.split('    struct MouseFocusEvidence',1)[1].split('    inline void push_gui_mouse_event',1)[0]
        forward = 'inline void forward_gui_input_message(' + routing_source.split(
            'inline void forward_gui_input_message(', 1)[1].split(
            '    epochengine::platform::window_adapter::NativeInputResult backend_input_callback', 1)[0]
        dock = routing_source.split('LRESULT CALLBACK DockableProc(', 1)[1]
        left_arm = 'case WM_LBUTTONDOWN:' + dock.split('case WM_LBUTTONDOWN:',1)[1].split('case WM_RBUTTONDOWN:',1)[0]
        right_arm = 'case WM_RBUTTONDOWN:' + dock.split('case WM_RBUTTONDOWN:', 1)[1].split('case WM_CHAR:', 1)[0]
        code = r'''
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
using HWND=void*;
HWND handle(std::uintptr_t value){return reinterpret_cast<HWND>(value);}
using HANDLE=void*;
struct RECT {long left{},top{},right{},bottom{};};
constexpr bool FALSE=false;
constexpr int SDL_LOGICAL_PRESENTATION_STRETCH=1;
constexpr const wchar_t* kEpochDockParentProp=L"dock";
constexpr const wchar_t* kEpochBackendInputAdapterProp=L"adapter";
int BackendInputProc(){return 0;}
namespace mock {
int logicalWidth=795,logicalHeight=1009,framebufferWidth=795,framebufferHeight=1009;
int nativeWidth=795,nativeHeight=1009,publishedWidth{},publishedHeight{};
bool logicalAvailable=true,rendererAvailable=true,configurationSucceeds=true;
bool subclassSucceeds=true;
std::unordered_map<HWND,std::uintptr_t> registrations;
unsigned displayScaleQueries{},subclassCalls{};
std::unordered_map<HWND,unsigned> properties;
}
bool native_class_owns_gui_input(HWND hwnd){return hwnd==handle(77);}
bool IsWindow(HWND hwnd){return hwnd!=0 && hwnd!=handle(99);}
bool GetClientRect(HWND,RECT* rect){*rect={0,0,mock::nativeWidth,mock::nativeHeight};return true;}
HANDLE GetPropW(HWND hwnd,const wchar_t* key){
    const unsigned flag=key==kEpochDockParentProp?1u:2u;
    return (mock::properties[hwnd]&flag)?reinterpret_cast<HANDLE>(1):nullptr;
}
bool SetPropW(HWND hwnd,const wchar_t* key,HANDLE){
    mock::properties[hwnd]|=key==kEpochDockParentProp?1u:2u;return true;
}
bool SetWindowSubclass(HWND hwnd,int(*)(),unsigned,std::uintptr_t data){
    ++mock::subclassCalls;
    if(mock::subclassSucceeds)mock::registrations[hwnd]=data;
    return mock::subclassSucceeds;
}
using DWORD_PTR=std::uintptr_t;
bool GetWindowSubclass(HWND hwnd,int(*)(),unsigned,DWORD_PTR* data){
    if(auto it=mock::registrations.find(hwnd);it!=mock::registrations.end()){
        *data=it->second;return true;
    }return false;
}
bool RemoveWindowSubclass(HWND hwnd,int(*)(),unsigned){return mock::registrations.erase(hwnd)!=0;}
HANDLE RemovePropW(HWND hwnd,const wchar_t*){mock::properties[hwnd]=0;return nullptr;}
namespace epochengine::platform::window_adapter {
struct WindowHandle {std::uintptr_t value{};};
struct NativeInputBinding {void* owner{};int(*callback)(){};};
struct NativeInputRegistration {NativeInputBinding binding;};
constexpr const wchar_t* nativeInputProperty=kEpochBackendInputAdapterProp;
constexpr unsigned nativeInputSubclass=2;
int native_input_proc(){return 0;}
bool owns_native_input_thread(HWND hwnd){return IsWindow(hwnd);}
ADMISSION
}
int backend_input_callback(){return 0;}

void* s_window=reinterpret_cast<void*>(1);
void* s_renderer=reinterpret_cast<void*>(2);
HWND s_childWindow=handle(1),s_hostWindow=handle(2);
int s_width=1,s_height=1,s_framebufferWidth=1,s_framebufferHeight=1;
int s_logicalPresentationWidth{},s_logicalPresentationHeight{};
bool SDL_GetWindowSize(void*,int* width,int* height){
    *width=mock::logicalWidth;*height=mock::logicalHeight;return mock::logicalAvailable;
}
bool SDL_GetRenderOutputSize(void*,int* width,int* height){
    *width=mock::framebufferWidth;*height=mock::framebufferHeight;return mock::rendererAvailable;
}
float SDL_GetWindowDisplayScale(void*){++mock::displayScaleQueries;return 1.5f;}
void* SDL_GetRenderTarget(void*){return nullptr;}
bool SDL_SetRenderLogicalPresentation(void*,int width,int height,int){
    mock::publishedWidth=width;mock::publishedHeight=height;return mock::configurationSucceeds;
}
const char* SDL_GetError(){return "controlled configuration failure";}
namespace epochengine::core {
struct WindowData {
    void* sdl_window{};int width{},height{};std::atomic<bool> firstPresentComplete{true};
    void set_size(int w,int h){width=w;height=h;}
};
struct Context {
    int width{},height{},virtualWidth{},virtualHeight{},framebufferWidth{},framebufferHeight{};
    std::shared_ptr<WindowData> windowData=std::make_shared<WindowData>();
};
struct MultiContextManager {ADAPTER_DECLARATION static int ChildProc(HWND,unsigned,std::uintptr_t,std::intptr_t){return 0;}};
MultiContextManager* activeManager{};
MultiContextManager* GetActiveMultiContextManager(){return activeManager;}
ADAPTER
}
namespace epochengine::logger {
inline unsigned failures{};
void error(const char*,const std::string&){++failures;}
}
namespace epochengine::sdlcontext::state {
DIMENSIONS
struct State {
    struct Lifecycle {
        int width{},height{};bool firstPresentRequired=true;
        void observe_drawable_extent(int w,int h){width=w;height=h;}
        bool rendering_allowed()const{return width>0&&height>0;}
    } surfaceLifecycle;
    struct {void* sdl_window{};} window;
    bool renderFaulted{};int width{},height{};
    void set_dimensions(int w,int h){width=w;height=h;}
};
State& get_sdl_state(){static State state;return state;}
}
REFRESH
namespace sf {
struct Vector2u {unsigned x{},y{};Vector2u(unsigned w,unsigned h):x(w),y(h){}};
struct FloatRect {float x{},y{},width{},height{};};
struct View {FloatRect rectangle;explicit View(FloatRect value):rectangle(value){}};
struct RenderWindow {
    Vector2u size{0,0};View view{FloatRect{}};
    void setSize(Vector2u value){size=value;}
    void setView(View value){view=value;}
};
}
namespace epochengine::sfml_compat {
sf::FloatRect float_rect(float x,float y,float w,float h){return {x,y,w,h};}
}
namespace epochengine::sfmlcontext::state {
struct State {int width{},height{};void set_dimensions(int w,int h){width=w;height=h;}};
State s_sfmlstate;
}
namespace sfml_backend {
sf::RenderWindow renderWindow;
sf::RenderWindow* s_window=&renderWindow;
HWND s_childWindow=handle(3),s_hostWindow=handle(4);
int s_width=1,s_height=1;
SFML_DIMENSIONS
}
using UINT=unsigned;using WPARAM=std::uintptr_t;using LPARAM=std::intptr_t;
enum {WM_LBUTTONDOWN,WM_RBUTTONDOWN,WM_MOUSEMOVE,WM_LBUTTONUP,WM_RBUTTONUP,
WM_MOUSEWHEEL,WM_KEYDOWN,WM_SYSKEYDOWN,WM_CHAR,WM_SYSCHAR,WM_KEYUP,WM_SYSKEYUP};
namespace epochengine::core::cli {bool trace_sdl_input=false;bool trace_menu_button0_rect=false;}
INPUT_EVIDENCE
#define GET_WHEEL_DELTA_WPARAM(w) static_cast<short>((w)>>16)
namespace epochengine::gui {enum class EventType {MouseDown,MouseMove,MouseUp,MouseWheel};}
namespace routing_mock {int focus{},owner{},events{},fallback{},button{},wheel{};bool screen{};
epochengine::gui::EventType type{};}
using DWORD=unsigned;
DWORD lastError=77;bool focusFails=false;HWND currentFocus=handle(8);
bool needsAttachment=false,attachedQueue=false,attachSucceeds=true,targetEnabled=true,childRelation=true;
DWORD foregroundPid=100;int attaches=0,detaches=0;
constexpr bool TRUE=true;constexpr DWORD ERROR_INVALID_PARAMETER=87;
DWORD GetCurrentProcessId(){return 100;}
DWORD GetWindowThreadProcessId(HWND hwnd,DWORD* pid){*pid=hwnd==handle(2)?foregroundPid:100;return hwnd==handle(2)?13:42;}
HWND GetForegroundWindow(){return handle(2);}
bool IsChild(HWND,HWND){return childRelation;}
bool AttachThreadInput(DWORD from,DWORD to,bool value){
if(from!=42||to!=13)return false;
if(value){++attaches;attachedQueue=attachSucceeds;return attachSucceeds;}
++detaches;attachedQueue=false;return true;}

DWORD GetLastError(){return lastError;}void SetLastError(DWORD e){lastError=e;}
DWORD GetCurrentThreadId(){return 42;}
HWND GetFocus(){return currentFocus;}
HWND GetParent(HWND){return handle(2);}constexpr int GA_ROOT=2;
HWND GetAncestor(HWND,int){return handle(2);}
bool IsWindowEnabled(HWND){lastError=999;return targetEnabled && !focusFails;}
HWND SetFocus(HWND target){++routing_mock::focus; if(focusFails || (needsAttachment && !attachedQueue)){lastError=87;return nullptr;}
const auto old=currentFocus;currentFocus=target;return old;}
FOCUS_HELPER
void remember_gui_input_owner(HWND){++routing_mock::owner;}
auto resolve_gui_context_for_hwnd(HWND){return std::make_shared<epochengine::core::Context>();}
bool accepts_gui_keyboard_input(HWND){return true;}
void push_gui_mouse_event(epochengine::core::Context*,HWND,epochengine::gui::EventType type,
LPARAM,int wheel=0,bool screen=false,int button=0){++routing_mock::events;routing_mock::type=type;
routing_mock::wheel=wheel;routing_mock::screen=screen;routing_mock::button=button;}
void push_gui_key_event(epochengine::core::Context*,int){}
void push_gui_text_event(epochengine::core::Context*,char32_t){}
inline void forward_gui_input_message(HWND,UINT,WPARAM,LPARAM) noexcept;
namespace epochengine::core {
struct FakeDrag {bool dragging=false;HWND draggedWindow=nullptr;};
FakeDrag& Drag(){static FakeDrag drag;return drag;}
}
bool is_dock_drag_hotspot(HWND,LPARAM){return false;}
int DefSubclassProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){++routing_mock::fallback;
if(native_class_owns_gui_input(hwnd))forward_gui_input_message(hwnd,msg,wp,lp);
return 73;}
FORWARD_ROUTING
int dock_right_arm(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){switch(msg){LEFT_ARM RIGHT_ARM default:return -1;}}
#define CHECK(...) do {if(!(__VA_ARGS__))return __LINE__;}while(false)
int main(){
    for(const auto msg : {WM_LBUTTONDOWN,WM_LBUTTONUP,WM_MOUSEMOVE,WM_RBUTTONDOWN,WM_RBUTTONUP,WM_MOUSEWHEEL}) {
        for(const auto target : {handle(77),handle(3)}) {
            const auto before=routing_mock::events;const auto fallbackBefore=routing_mock::fallback;
            dock_right_arm(target,msg,0,0);
            CHECK(routing_mock::events==before+1);
            CHECK(routing_mock::fallback==fallbackBefore+1);
        }
    }
    routing_mock::events=routing_mock::fallback=routing_mock::owner=routing_mock::focus=0;
    SdlInputEvidence evidence;
    CHECK(!evidence.enabled());
    epochengine::core::cli::trace_sdl_input=true;
    CHECK(evidence.enabled());
    evidence.pump(0xfffffff0u);evidence.pump(4u);
    CHECK(evidence.maxPumpGap==20u);
    evidence.input(999u,0u,5u);
    CHECK(evidence.inputs==0&&!evidence.started);
    evidence.input(WM_RBUTTONDOWN,0xfffffffdu,5u);
    evidence.input(WM_MOUSEMOVE,4u,6u);
    evidence.input(WM_CHAR,5u,7u);
    CHECK(evidence.started&&evidence.began==5u&&evidence.inputs==3);
    CHECK(evidence.buttons==1&&evidence.moves==1&&evidence.keys==1&&evidence.maxAge==8u);
    evidence.reports=60;CHECK(!evidence.enabled());
    using namespace routing_mock;
    CHECK(dock_right_arm(handle(3),WM_RBUTTONDOWN,0,0)==73);
    CHECK(events==1&&focus==1&&owner==1&&fallback==1&&button==1);
    CHECK(type==epochengine::gui::EventType::MouseDown);
    CHECK(dock_right_arm(handle(3),WM_RBUTTONUP,0,0)==73);
    CHECK(events==2&&focus==1&&owner==1&&fallback==2&&button==1);
    CHECK(type==epochengine::gui::EventType::MouseUp);
    CHECK(dock_right_arm(handle(3),WM_MOUSEWHEEL,120u<<16,0)==73);
    CHECK(events==3&&fallback==3&&wheel==120&&screen&&button==0);

    epochengine::core::cli::trace_menu_button0_rect=true;
    lastError=77;currentFocus=handle(8);focusFails=false;
    focus_gui_mouse_target(handle(3));
    CHECK(mouseFocusEvidence.callerThread==42 && mouseFocusEvidence.target==handle(3));
    CHECK(mouseFocusEvidence.before==handle(8) && mouseFocusEvidence.previous==handle(8));
    CHECK(mouseFocusEvidence.after==handle(3) && mouseFocusEvidence.error==0 && lastError==77);
    focusFails=true;lastError=11;
    focus_gui_mouse_target(handle(4));
    CHECK(mouseFocusEvidence.error==87 && lastError==87 && !mouseFocusEvidence.targetEnabled);
    CHECK(mouseFocusEvidence.previous==nullptr && mouseFocusEvidence.after==handle(3));
    focusFails=false;needsAttachment=true;currentFocus=nullptr;lastError=0;
    focus_gui_mouse_target(handle(4));
    CHECK(attaches==1 && detaches==1 && !attachedQueue);
    CHECK(mouseFocusEvidence.initialError==87 && mouseFocusEvidence.error==0);
    CHECK(mouseFocusEvidence.attached && mouseFocusEvidence.detached && mouseFocusEvidence.after==handle(4));
    attachSucceeds=false;currentFocus=nullptr;
    focus_gui_mouse_target(handle(4));
    CHECK(attaches==2 && detaches==1 && mouseFocusEvidence.error==87);
    attachSucceeds=true;foregroundPid=200;
    focus_gui_mouse_target(handle(4));
    CHECK(attaches==2 && detaches==1);
    foregroundPid=100;childRelation=false;
    focus_gui_mouse_target(handle(4));
    CHECK(attaches==2 && detaches==1);
    childRelation=true;targetEnabled=false;
    focus_gui_mouse_target(handle(4));
    CHECK(attaches==2 && detaches==1);
    targetEnabled=true;needsAttachment=false;
    epochengine::core::cli::trace_menu_button0_rect=false;focusFails=false;
    auto ctx=std::make_shared<epochengine::core::Context>();
    // At a 150% monitor setting SDL can already report physical client-space.
    // A second division by display scale shrinks the layout and enlarges UI.
    refresh_dimensions(ctx);
    CHECK(ctx->width==795&&ctx->height==1009);
    CHECK(ctx->framebufferWidth==795&&ctx->framebufferHeight==1009);
    CHECK(ctx->virtualWidth==795&&ctx->virtualHeight==1009);
    CHECK(mock::publishedWidth==795&&mock::publishedHeight==1009);
    CHECK(ctx->windowData->width==795&&!ctx->windowData->firstPresentComplete.load());
    CHECK(mock::displayScaleQueries==0);
    // A genuinely distinct logical/drawable tuple must remain distinct.
    mock::logicalWidth=530;mock::logicalHeight=673;
    refresh_dimensions(ctx);
    CHECK(ctx->width==530&&ctx->height==673);
    CHECK(ctx->framebufferWidth==795&&ctx->framebufferHeight==1009);
    CHECK(mock::publishedWidth==530&&mock::publishedHeight==673);
    CHECK(epochengine::sdlcontext::state::normalize_presented_coordinate(795,530,795)==530);
    // Win32 fallback uses the actual child extent when SDL has none.
    mock::logicalAvailable=false;mock::nativeWidth=640;mock::nativeHeight=480;
    mock::framebufferWidth=640;mock::framebufferHeight=480;
    refresh_dimensions(ctx);CHECK(ctx->width==640&&ctx->height==480);
    // A minimized zero extent records suspension instead of reconfiguring.
    mock::nativeWidth=0;mock::nativeHeight=0;
    refresh_dimensions(ctx);
    CHECK(!epochengine::sdlcontext::state::get_sdl_state().surfaceLifecycle.rendering_allowed());
    CHECK(mock::publishedWidth==640&&mock::publishedHeight==480);
    using epochengine::core::MultiContextManager;
    MultiContextManager manager;
    epochengine::core::activeManager=&manager;
    manager.AttachBackendInputAdapter(handle(0));
    manager.AttachBackendInputAdapter(handle(99));
    CHECK(mock::subclassCalls==0);
    manager.AttachBackendInputAdapter(handle(5));
    manager.AttachBackendInputAdapter(handle(5));
    CHECK(mock::subclassCalls==1&&mock::properties[handle(5)]==2);
    mock::properties[handle(6)]=1;
    manager.AttachBackendInputAdapter(handle(6));
    CHECK(mock::subclassCalls==1&&mock::properties[handle(6)]==1);
    mock::subclassSucceeds=false;
    {const auto before=mock::subclassCalls;manager.AttachBackendInputAdapter(handle(77));CHECK(mock::subclassCalls==before);}
    manager.AttachBackendInputAdapter(handle(7));
    CHECK(mock::subclassCalls==2&&mock::properties[handle(7)]==0);
    mock::subclassSucceeds=true;
    manager.AttachBackendInputAdapter(handle(7));
    CHECK(mock::subclassCalls==3&&mock::properties[handle(7)]==2);
    using namespace epochengine::platform::window_adapter;
    CHECK(!attach_native_input({5},{nullptr,backend_input_callback}));
    detach_native_input({5},{nullptr,backend_input_callback});
    CHECK(mock::registrations.count(handle(5))==1);
    detach_native_input({5},{&manager,backend_input_callback});
    CHECK(mock::registrations.count(handle(5))==0&&mock::properties[handle(5)]==0);
    detach_native_input({5},{&manager,backend_input_callback});
    manager.AttachBackendInputAdapter(handle(5));
    CHECK(mock::registrations.count(handle(5))==1&&mock::subclassCalls==4);
    detach_native_input({5},{&manager,backend_input_callback});
    detach_native_input({7},{&manager,backend_input_callback});
    // SFML's actual client extent, context publication and view agree in
    // pixels. Resizing must not leave the old view shrinking its GUI glyphs.
    mock::nativeWidth=795;mock::nativeHeight=1009;
    sfml_backend::refresh_dimensions(ctx);sfml_backend::apply_view_size();
    CHECK(ctx->width==795&&ctx->framebufferWidth==795);
    CHECK(sfml_backend::renderWindow.size.x==795);
    CHECK(sfml_backend::renderWindow.view.rectangle.width==795.0f);
    CHECK(sfml_backend::renderWindow.view.rectangle.height==1009.0f);
    mock::nativeWidth=320;mock::nativeHeight=240;
    sfml_backend::refresh_dimensions(ctx);sfml_backend::apply_view_size();
    CHECK(ctx->width==320&&ctx->height==240);
    CHECK(sfml_backend::renderWindow.size.x==320);
    CHECK(sfml_backend::renderWindow.view.rectangle.width==320.0f);
    CHECK(sfml_backend::renderWindow.view.rectangle.height==240.0f);
    return 0;
}
'''.replace('FOCUS_HELPER', focus_helper).replace('INPUT_EVIDENCE', evidence).replace('FORWARD_ROUTING', forward).replace('RIGHT_ARM', right_arm).replace('LEFT_ARM', left_arm).replace('ADAPTER_DECLARATION', declaration).replace('ADMISSION', admission).replace('ADAPTER', adapter).replace('SFML_DIMENSIONS', sfml_dimensions).replace(
            'DIMENSIONS', dimensions).replace('REFRESH', refresh)
        if os.name != 'nt':
            # The test executes the Windows adapter's real conditional path.
            self.skipTest('Installed MSVC CPU lane')
        toolsets = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kitroot = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((kitroot / 'Include').glob('*'))
        if not toolsets or not kits:
            self.skipTest('Installed MSVC/SDK required')
        parent = (ROOT / 'build/contract-tests').resolve()
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='backend-surface-input-', dir=parent))
        try:
            source = scratch / 'contract.cpp'
            source.write_text(code, encoding='utf-8')
            toolset, kit = toolsets[-1], kits[-1]
            binary = scratch / 'contract.exe'
            args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/WX',
                '/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'), '/I' + str(kit / 'shared'),
                '/I' + str(kit / 'um'), '/Fo' + str(scratch / 'contract.obj'), '/Fe' + str(binary),
                str(source), '/link', '/LIBPATH:' + str(toolset / 'lib/x64'),
                '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'um/x64'),
                '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'ucrt/x64')]
            compiled = subprocess.run(args, cwd=scratch, capture_output=True, text=True, timeout=90)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            executed = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=10)
            lines = code.splitlines()
            failure = lines[executed.returncode - 1] if 0 < executed.returncode <= len(lines) else ''
            self.assertEqual(executed.returncode, 0, 'Actual surface/input CPU contract: ' + failure)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent))
            shutil.rmtree(scratch)

    def test_sdl_binds_shared_adapter_after_publishing_live_identity(self):
        source = read_owned_source(ROOT / 'Engine/src/renderers/sdl/core.context_sdl.cpp')
        initialization = source.split('    void sdl_initialize_adapter()', 1)[1].split(
            '    void sdl_cleanup_adapter()', 1)[0]
        self.assertEqual(initialization.count('AttachBackendInputAdapter(s_childWindow);'), 1)
        binding = initialization.index('AttachBackendInputAdapter(s_childWindow);')
        self.assertLess(initialization.index('ctx->windowData->hwndChild = s_childWindow;'), binding)
        self.assertLess(initialization.index('ctx->native_window = s_childWindow'), binding)
        self.assertLess(binding, initialization.index('SDL_ShowWindow(s_window);'))

    def test_raylib_binds_shared_adapter_after_publishing_live_identity(self):
        source = read_owned_source(ROOT / 'Engine/modules/raylib.context.ixx')
        adoption = source.split('void adopt_raylib_window(', 1)[1].split(
            '#if !defined(_WIN32)', 1)[0]
        self.assertEqual(adoption.count('AttachBackendInputAdapter(raylibHwnd);'), 1)
        binding = adoption.index('AttachBackendInputAdapter(raylibHwnd);')
        self.assertLess(adoption.index('ctx->windowData->hwnd = raylibHwnd;'), binding)
        self.assertLess(adoption.index('ctx->native_window = raylibHwnd;'), binding)


if __name__ == '__main__':
    unittest.main()
