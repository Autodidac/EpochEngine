"""Actual Windows host observation/cache logic through deterministic native ports.

No HWND, compositor, GPU, model or application is started. Native query ports
record lock ownership; the production cache and WindowState resolver are used.
"""
from pathlib import Path
import json
import time
import unittest

from Tests.support.cpp import compile_and_run
from Tools.source.owned_source_text import read_owned_source


ROOT = Path(__file__).resolve().parents[2]


class NativeWindowObservation(unittest.TestCase):
    def test_actual_cache_cadence_invalidation_and_coherent_geometry(self):
        render = read_owned_source(ROOT / "Engine/src/renderers/host/implementation/host.render_loop.inl")
        cache = render.split("        epochengine::rendercontext::WindowObservation cachedNativeObservation{};", 1)[1]
        cache = "epochengine::rendercontext::WindowObservation cachedNativeObservation{};" + cache.split(
            "        const auto publishFrameWindowState", 1)[0]
        cache = cache.replace("        const auto resolveFrameWindowState = [&]() noexcept", "        auto observe() noexcept")
        self.assertTrue(cache.rstrip().endswith("};"))
        cache = cache.rstrip()[:-2] + "}"
        frame = read_owned_source(ROOT / "Engine/modules/render/render.context_frame.ixx")
        frame = frame.split("export namespace epochengine::rendercontext", 1)[1]
        frame = "namespace epochengine::rendercontext" + frame
        interaction = read_owned_source(ROOT / "Engine/src/renderers/host/implementation/host.window_interaction.inl")
        invalidation = interaction.split("    [[nodiscard]] inline bool invalidates_native_window_observation", 1)[1]
        invalidation = "[[nodiscard]] inline bool invalidates_native_window_observation" + invalidation.split(
            "    inline void remember_gui_input_owner", 1)[0]
        manager = read_owned_source(ROOT / "Engine/modules/context/context.multiplexer.ixx")
        invalidate = manager.split("        void invalidate_native_window_state() noexcept", 1)[1].split("\n\n", 1)[0]
        invalidate = "void invalidate_native_window_state() noexcept" + invalidate
        dispatch = read_owned_source(ROOT / "Engine/src/renderers/host/implementation/host.native_dispatch.inl")
        for name in ("ParentProc", "ChildProc"):
            body = dispatch.split("LRESULT CALLBACK MultiContextManager::" + name, 1)[1]
            self.assertLess(body.index("const NativeWindowObservationInvalidation nativeObservation"), body.index("switch (msg)"))
        for name in ("backend_input_callback", "DockableProc"):
            body = interaction.split(name + "(", 1)[1]
            self.assertLess(body.index("const NativeWindowObservationInvalidation nativeObservation"), body.index("switch ("))
        code = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
using HWND=void*;
using UINT=unsigned;
using DWORD=std::uint32_t;
using HRESULT=long;
constexpr int FALSE=0,GA_ROOT=2,DWMWA_CLOAKED=14;
constexpr HRESULT E_FAIL=-1;
constexpr bool SUCCEEDED(HRESULT result){return result>=0;}
enum : UINT { WM_ACTIVATE=1,WM_SETFOCUS,WM_KILLFOCUS,WM_SIZE,WM_SHOWWINDOW,
    WM_WINDOWPOSCHANGED,WM_DPICHANGED,WM_DISPLAYCHANGE,WM_DWMCOMPOSITIONCHANGED,
    WM_DESTROY,WM_NCDESTROY,WM_MOUSEMOVE,WM_PAINT,WM_TIMER };
HWND handle(std::uintptr_t value){return reinterpret_cast<HWND>(value);}
struct TrackingMutex {
    std::recursive_mutex native;
    static inline thread_local unsigned depth{};
    void lock(){native.lock();++depth;}
    void unlock(){--depth;native.unlock();}
};
struct ClockPort {
    using time_point=std::chrono::steady_clock::time_point;
    static inline std::atomic<std::int64_t> nanoseconds{};
    static time_point now() noexcept {return time_point{std::chrono::nanoseconds{nanoseconds.load()}};}
};
namespace ports {
struct Native {
    HWND root=handle(1),foreground=handle(1);
    bool valid=true,visible=true,minimized=false,child=false;
    DWORD cloaked{};
    UINT dpi=144;
    HRESULT cloakResult{};
    unsigned ancestor{},validity{},foregroundQueries{},cloak{},dpiQueries{},visibility{},children{},iconic{},queriesUnderLock{};
    std::atomic<std::uint64_t>* invalidateDuringQuery{};
} native;
void query() noexcept {if(TrackingMutex::depth)++native.queriesUnderLock;}
}
HWND GetAncestor(HWND,int){ports::query();++ports::native.ancestor;return ports::native.root;}
int IsWindow(HWND){ports::query();++ports::native.validity;return ports::native.valid;}
HWND GetForegroundWindow(){ports::query();++ports::native.foregroundQueries;return ports::native.foreground;}
HRESULT DwmGetWindowAttribute(HWND,int,DWORD* value,unsigned){
    ports::query();++ports::native.cloak;*value=ports::native.cloaked;
    if(auto* epoch=ports::native.invalidateDuringQuery){ports::native.invalidateDuringQuery=nullptr;++*epoch;}
    return ports::native.cloakResult;
}
UINT GetDpiForWindow(HWND){ports::query();++ports::native.dpiQueries;return ports::native.dpi;}
int IsWindowVisible(HWND){ports::query();++ports::native.visibility;return ports::native.visible;}
int IsChild(HWND,HWND){ports::query();++ports::native.children;return ports::native.child;}
int IsIconic(HWND){ports::query();++ports::native.iconic;return ports::native.minimized;}
FRAME
namespace epochengine::core {
struct MultiContextManager {
    std::atomic_uint64_t nativeWindowStateGeneration{};
    INVALIDATE
};
inline MultiContextManager* active{};
MultiContextManager* GetActiveMultiContextManager() noexcept {return active;}
}
INVALIDATION
struct WindowDataPort {
    HWND host_hwnd=handle(1),hwnd=handle(2);
    int width=800,height=600;
    std::atomic_uint64_t resizeGeneration{};
};
struct ContextPort {int framebufferWidth=1200,framebufferHeight=900;};
struct ObservationConsumer {
    using HostPhaseClock=ClockPort;
    enum {hostQuery,hostDwmCloak,hostDpi,hostPhaseCount};
    WindowDataPort& win;
    TrackingMutex& windowsMutex;
    std::atomic_uint64_t& nativeWindowStateGeneration;
    ContextPort* ctx;
    bool measureHostPhases=false;
    std::array<double,hostPhaseCount> hostFrame{};
    static double hostMicros(HostPhaseClock::time_point begin,HostPhaseClock::time_point end) noexcept {
        return std::chrono::duration<double,std::micro>(end-begin).count();
    }
    CACHE
};
#define CHECK(...) do {if(!(__VA_ARGS__))return __LINE__;} while(false)
int main(){
    using namespace epochengine::rendercontext;
    epochengine::core::MultiContextManager manager,other;
    epochengine::core::active=&manager;
    WindowDataPort win;
    ContextPort ctx;
    TrackingMutex mutex;
    ObservationConsumer observer{win,mutex,manager.nativeWindowStateGeneration,&ctx};
    auto state=observer.observe();
    CHECK(state.logical_width==800 && state.logical_height==600 && state.framebuffer_width==1200);
    CHECK(state.visibility_known && state.focus_known && state.minimized_known && state.occlusion_known && state.dpi_known);
    CHECK(state.activity==WindowActivity::foreground && state.dpi_milli==1500 && ports::native.cloak==1 && ports::native.dpiQueries==1);
    // At 100k producer frames/second a native sample remains bounded to 10Hz.
    for(unsigned frame=1;frame<10000;++frame){
        ClockPort::nanoseconds=static_cast<std::int64_t>(frame)*10000;
        state=observer.observe();
        CHECK(state.logical_width==800 && state.logical_height==600);
    }
    CHECK(ports::native.cloak==1 && ports::native.dpiQueries==1 && ports::native.ancestor==1 && ports::native.foregroundQueries==1);
    ClockPort::nanoseconds=100000000;
    state=observer.observe();
    CHECK(ports::native.cloak==2 && ports::native.dpiQueries==2);
    const auto previous=manager.nativeWindowStateGeneration.load();
    for(UINT ignored:{WM_MOUSEMOVE,WM_PAINT,WM_TIMER}){
        NativeWindowObservationInvalidation guard{&manager,ignored};
    }
    CHECK(manager.nativeWindowStateGeneration==previous);
    for(UINT message:{WM_ACTIVATE,WM_SETFOCUS,WM_KILLFOCUS,WM_SIZE,WM_SHOWWINDOW,
        WM_WINDOWPOSCHANGED,WM_DPICHANGED,WM_DISPLAYCHANGE,WM_DWMCOMPOSITIONCHANGED,WM_DESTROY,WM_NCDESTROY}){
        const auto queries=ports::native.cloak;
        {NativeWindowObservationInvalidation guard{&manager,message};}
        state=observer.observe();
        CHECK(ports::native.cloak==queries+1);
    }
    // Old-manager handlers must not invalidate the replacement manager.
    {NativeWindowObservationInvalidation guard{&manager,WM_ACTIVATE};epochengine::core::active=&other;}
    CHECK(other.nativeWindowStateGeneration==0);
    epochengine::core::active=&manager;
    // Refused focus is background, never false foreground evidence.
    ports::native.foreground=handle(77);
    manager.invalidate_native_window_state();state=observer.observe();
    CHECK(state.focus_known && !state.focused && state.activity==WindowActivity::background);
    ports::native.foreground=nullptr;
    manager.invalidate_native_window_state();state=observer.observe();
    CHECK(!state.focus_known);
    ports::native.foreground=handle(1);
    ports::native.minimized=true;
    manager.invalidate_native_window_state();state=observer.observe();
    CHECK(state.minimized_known && state.minimized && !state.presentable && state.logical_width==0 && state.framebuffer_width==0);
    ports::native.minimized=false;ports::native.visible=false;
    manager.invalidate_native_window_state();state=observer.observe();
    CHECK(state.visibility_known && !state.visible && !state.presentable);
    ports::native.visible=true;ports::native.cloaked=1;
    manager.invalidate_native_window_state();state=observer.observe();
    CHECK(state.occlusion_known && state.occluded && !state.presentable);
    ports::native.cloaked=0;ports::native.cloakResult=E_FAIL;ports::native.dpi=0;
    manager.invalidate_native_window_state();state=observer.observe();
    CHECK(!state.occlusion_known && !state.dpi_known && state.dpi_milli==1000);
    ports::native.cloakResult=0;ports::native.dpi=96;
    // Invalid/destroyed native surfaces must retain unavailable knowledge flags.
    ports::native.valid=false;manager.invalidate_native_window_state();
    const auto beforeInvalid=ports::native.cloak;
    state=observer.observe();
    CHECK(!state.visibility_known && !state.focus_known && !state.minimized_known && !state.occlusion_known && !state.dpi_known);
    CHECK(ports::native.cloak==beforeInvalid);
    ports::native.valid=true;
    {std::scoped_lock lock(mutex);win.width=901;win.height=1802;++win.resizeGeneration;}
    state=observer.observe();
    CHECK(state.logical_width==901 && state.logical_height==1802 && state.resize_generation==1);
    const auto beforeHwnd=ports::native.cloak;
    {std::scoped_lock lock(mutex);win.host_hwnd=handle(9);}
    state=observer.observe();CHECK(ports::native.cloak==beforeHwnd+1);
    // Native changes arriving during a query cannot disappear behind its cache.
    ports::native.invalidateDuringQuery=&manager.nativeWindowStateGeneration;
    manager.invalidate_native_window_state();state=observer.observe();
    const auto interrupted=ports::native.cloak;
    state=observer.observe();CHECK(ports::native.cloak==interrupted+1);
    // Every context owns a separate sample; the shared epoch invalidates both.
    ObservationConsumer second{win,mutex,manager.nativeWindowStateGeneration,&ctx};
    second.observe();const auto perContext=ports::native.cloak;
    manager.invalidate_native_window_state();observer.observe();second.observe();
    CHECK(ports::native.cloak==perContext+2);
    ClockPort::nanoseconds=1;const auto beforeClock=ports::native.cloak;
    observer.observe();CHECK(ports::native.cloak==beforeClock+1);
    observer.measureHostPhases=true;observer.observe();
    CHECK(observer.hostFrame[ObservationConsumer::hostDwmCloak]==0 && observer.hostFrame[ObservationConsumer::hostDpi]==0);
    // Actual lock/snapshot logic under concurrent pair publication: no torn pair.
    {std::scoped_lock lock(mutex);win.width=1;win.height=2;++win.resizeGeneration;}
    std::atomic_bool done{},coherent=true;
    std::thread writer([&]{for(int n=2;n<=20000;++n){std::scoped_lock lock(mutex);win.width=n;win.height=n*2;++win.resizeGeneration;}done=true;});
    do {state=observer.observe();if(state.logical_height!=state.logical_width*2)coherent=false;} while(!done.load());
    writer.join();CHECK(coherent);
    CHECK(ports::native.queriesUnderLock==0);
    return 0;
}
'''
        code = code.replace("FRAME", frame).replace("INVALIDATE", invalidate).replace("INVALIDATION", invalidation).replace("CACHE", cache)
        report = ROOT / "Artifacts/Reports/native-window-observation-oct30" / str(time.time_ns())
        report.mkdir(parents=True)
        source = report / "consumer.cpp"
        source.write_text(code, encoding="utf-8")
        records = compile_and_run(self, source, report)
        (report / "result.json").write_text(json.dumps({"tests": 1, "records": records,
            "scope": "Actual observation cache/resolver/invalidation; native query and clock ports; no native window/GPU"}, indent=2), encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
