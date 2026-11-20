"""Actual finalized-plan publication and toolbar text, with CPU-only data ports."""
from pathlib import Path
import hashlib
import json
import re
import unittest

from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
OWNERS = (
    "Engine/modules/context/context.window.ixx",
    "Engine/src/renderers/host/implementation/host.render_loop.inl",
    "Engine/src/renderers/host/host.context_linux.cpp",
    "Engine/src/editor/application/editor.frame_bootstrap.inl",
)


def publication(source):
    start = source.index("            win.editorFramePacingHz.store(framePlan.effective_hz,")
    end = source.index("            if (!ctx->apply_frame_pacing || nativeResult.configured)", start)
    return source[start:end]


class NativeVsyncStatus(unittest.TestCase):
    def test_actual_native_publishers_and_toolbar_format(self):
        window, windows, linux, toolbar = [(ROOT / path).read_text(encoding="utf-8") for path in OWNERS]
        names = ("editorThrottleHz", "editorFramePacingHz", "editorNativeVsyncActive",
                 "uncappedFrameSubmission", "editorForegroundWorkActive")
        fields = "\n".join(re.search(r"^.*std::atomic[^\n]*\b" + name + r"\{[^\n]*$",
                                    window, re.MULTILINE)[0] for name in names)
        begin = toolbar.index("        const double chromeThrottle =")
        label = toolbar[begin:toolbar.index("        const bool epochUsage =", begin)]
        source = r'''
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <string>
#include "core.format_text.hpp"
#define CHECK(value) do { if (!(value)) { std::fprintf(stderr,"CHECK line %d: %s\n",__LINE__,#value); std::exit(1); } } while(false)
namespace epochengine::perf {
enum class frame_pacing_mode {capped,uncapped,vsync};
enum class frame_activity {foreground,background,minimized};
}
namespace epochengine::editor::build_pressure {inline bool compiling{};bool active(){return compiling;}}
using Mode=epochengine::perf::frame_pacing_mode;
using Activity=epochengine::perf::frame_activity;
// These ports carry finalized data only. The production plan resolver and rate
// sampler are unchanged and are not replaced or simulated by this fixture.
struct Plan {
 Mode requested_mode{Mode::capped},effective_mode{Mode::capped};
 double effective_hz{};bool native_pacing_active{};
 Activity activity{Activity::foreground};
};
struct Window {
''' + fields + r'''
};
struct Context {Window* windowData{};Activity activity{Activity::foreground};
 Activity frame_pacing_activity()const{return activity;}};
void publish_windows(Window& win,const Plan& framePlan){
''' + publication(windows) + r'''
}
void publish_linux(Window& win,const Plan& framePlan){
''' + publication(linux) + r'''
}
enum class Badge {rate, full, compact};
std::string toolbar_label(Context* ctx,double fps,bool measured,bool building=false,bool child=false,Badge badge=Badge::rate){
 struct Process{bool live;bool valid()const{return live;}};
 struct {double chromeFpsSample;bool chromeFpsMeasured;bool projectBuildPending;Process projectRuntimeProcess;} editor{fps,measured,building,{child}};
 namespace perf=epochengine::perf;
''' + label + r'''
 return badge==Badge::compact ? toolbarFpsCompact : badge==Badge::full ? toolbarFpsFull : toolbarFps;
}
int main(){
 Window defaults;
 CHECK(!defaults.editorNativeVsyncActive.load()&&defaults.editorFramePacingHz.load()==0);
 for(auto publish:{publish_windows,publish_linux}){
  Window window;Context ctx{&window};
  const Plan vsync{Mode::vsync,Mode::vsync,0,true};
  publish(window,vsync);
  CHECK(window.editorNativeVsyncActive.load(std::memory_order_acquire));
  CHECK(window.editorFramePacingHz.load()==0&&!window.uncappedFrameSubmission.load());
  CHECK(toolbar_label(&ctx,61,true)=="61 FPS / VSync");
  CHECK(toolbar_label(&ctx,61,true,false,false,Badge::compact)=="61 FPS / VSync");
  // CPU/build admission has priority over a still-active native VSync plan.
  window.editorThrottleHz=20;CHECK(toolbar_label(&ctx,61,true)=="61 FPS / 20 CPU cap");
  CHECK(toolbar_label(&ctx,61,true,false,false,Badge::compact)=="61 FPS / 20 CPU cap");
  epochengine::editor::build_pressure::compiling=true;
  window.editorThrottleHz=10;CHECK(toolbar_label(&ctx,61,true)=="61 FPS / 10 build cap");
  CHECK(toolbar_label(&ctx,61,true,false,false,Badge::full)=="Build 61 FPS / 10 build cap");
  epochengine::editor::build_pressure::compiling=false;
  CHECK(toolbar_label(&ctx,61,true)=="61 FPS / 10 CPU cap");
  window.editorThrottleHz=0;
  CHECK(toolbar_label(&ctx,61,false)=="-- FPS");
  CHECK(toolbar_label(&ctx,std::numeric_limits<double>::quiet_NaN(),true)=="-- FPS");
  CHECK(toolbar_label(&ctx,std::numeric_limits<double>::infinity(),true)=="-- FPS");
  // Requested VSync is not evidence of active native VSync.
  publish(window,{Mode::vsync,Mode::capped,30,false});
  CHECK(!window.editorNativeVsyncActive.load()&&toolbar_label(&ctx,30,true)=="30 FPS / 30 pacing cap");
  publish(window,{Mode::vsync,Mode::vsync,0,false});
  CHECK(!window.editorNativeVsyncActive.load()&&toolbar_label(&ctx,61,true)=="61 FPS / uncapped");
  // Both required predicates must hold; native pacing in another mode is not VSync.
  publish(window,{Mode::capped,Mode::capped,30,true});
  CHECK(!window.editorNativeVsyncActive.load()&&toolbar_label(&ctx,30,true)=="30 FPS / 30 pacing cap");
  publish(window,vsync);publish(window,{Mode::capped,Mode::capped,30,false});
  CHECK(!window.editorNativeVsyncActive.load()&&toolbar_label(&ctx,30,true)=="30 FPS / 30 pacing cap");
  publish(window,vsync);publish(window,{Mode::uncapped,Mode::uncapped,0,false});
  CHECK(!window.editorNativeVsyncActive.load()&&window.uncappedFrameSubmission.load());
  CHECK(toolbar_label(&ctx,120,true)=="120 FPS / uncapped");
  publish(window,vsync);publish(window,{Mode::uncapped,Mode::capped,30,false,Activity::background});
  CHECK(!window.editorNativeVsyncActive.load()&&!window.uncappedFrameSubmission.load());
  CHECK(toolbar_label(&ctx,30,true)=="30 FPS / 30 pacing cap");
  CHECK(toolbar_label(&ctx,30,true,true,false,Badge::full)=="Build 30 FPS / 30 pacing cap");
  CHECK(toolbar_label(&ctx,30,true,false,true,Badge::full)=="Project host 30 FPS / 30 pacing cap");
  CHECK(toolbar_label(&ctx,30,true,false,false,Badge::full)=="30 FPS / 30 pacing cap");
  CHECK(toolbar_label(&ctx,30,true,false,false,Badge::compact)=="30 FPS / 30 pacing cap");
  CHECK(toolbar_label(&ctx,0,false,false,false,Badge::compact)=="-- FPS");
  ctx.activity=Activity::background;
  CHECK(toolbar_label(&ctx,30,true)=="30 FPS unfocused");
  CHECK(toolbar_label(&ctx,30,true,false,false,Badge::full)=="30 FPS unfocused");
  CHECK(toolbar_label(&ctx,30,true,false,false,Badge::compact)=="30 FPS unfocused");
  CHECK(toolbar_label(&ctx,27,true,false,false,Badge::compact)=="27/30 FPS unfocused");
  CHECK(toolbar_label(&ctx,0,false,false,false,Badge::full)=="Unfocused -- FPS");
  CHECK(toolbar_label(&ctx,30,true,false,true,Badge::full)=="Project host 30 FPS unfocused");
  CHECK(toolbar_label(&ctx,30,true,true,true,Badge::full)=="Build 30 FPS unfocused");
  window.editorThrottleHz=20;
  CHECK(toolbar_label(&ctx,20,true,false,false,Badge::full)=="Unfocused 20 FPS / 20 CPU cap");
  window.editorThrottleHz=40; // The looser CPU ceiling must not replace the effective background cap.
  CHECK(toolbar_label(&ctx,30,true,false,false,Badge::full)=="30 FPS unfocused");
  publish(window,{Mode::capped,Mode::capped,5,false,Activity::minimized});ctx.activity=Activity::minimized;
  window.editorThrottleHz=10;epochengine::editor::build_pressure::compiling=true;
  CHECK(toolbar_label(&ctx,5,true,true,false,Badge::full)=="Build 5 FPS / 5 minimized cap");
  epochengine::editor::build_pressure::compiling=false;
  window.editorThrottleHz=0;ctx.activity=Activity::foreground;
 }
 Context no_window;
 CHECK(toolbar_label(&no_window,60,false)=="-- FPS");
 CHECK(toolbar_label(&no_window,60,true)=="60 FPS / uncapped");
 return 0;
}
'''
        report = ROOT / "Artifacts/Reports/opengl-native-production-oct30/vsync-label/cpu"
        report.mkdir(parents=True, exist_ok=True)
        fixture = report / "native_vsync_status_contract.cpp"
        fixture.write_text(source, encoding="utf-8")
        results = compile_and_run(self, fixture, report, includes=(ROOT / "Engine/include",), windows_flags=("/WX",))
        (report / "result.json").write_text(json.dumps(dict(
            configurations=results,
            production={path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in OWNERS},
            scope="Actual WindowData fields, Windows/Linux publisher snippets and toolbar format; finalized-plan/rate-sample data ports only. No native API."
        ), indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
