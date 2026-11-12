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
        names = ("editorThrottleHz", "editorFramePacingHz", "editorNativeVsyncActive", "uncappedFrameSubmission")
        fields = "\n".join(re.search(r"^.*std::atomic[^\n]*\b" + name + r"\{[^\n]*$",
                                    window, re.MULTILINE)[0] for name in names)
        begin = toolbar.index("        const double chromeThrottle =")
        label = toolbar[begin:toolbar.index("        const auto toolbarResources =", begin)]
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
enum class frame_activity {foreground,background};
}
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
struct Context {Window* windowData{};};
void publish_windows(Window& win,const Plan& framePlan){
''' + publication(windows) + r'''
}
void publish_linux(Window& win,const Plan& framePlan){
''' + publication(linux) + r'''
}
std::string toolbar_label(Context* ctx,double fps,bool measured){
 struct {double chromeFpsSample;bool chromeFpsMeasured;} editor{fps,measured};
''' + label + r'''
 return toolbarFps;
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
  // CPU/build admission has priority over a still-active native VSync plan.
  window.editorThrottleHz=20;CHECK(toolbar_label(&ctx,61,true)=="61 FPS / 20 CPU cap");
  window.editorThrottleHz=10;CHECK(toolbar_label(&ctx,61,true)=="61 FPS / 10 build cap");
  window.editorThrottleHz=0;
  CHECK(toolbar_label(&ctx,61,false)=="-- FPS");
  CHECK(toolbar_label(&ctx,std::numeric_limits<double>::quiet_NaN(),true)=="-- FPS");
  CHECK(toolbar_label(&ctx,std::numeric_limits<double>::infinity(),true)=="-- FPS");
  // Requested VSync is not evidence of active native VSync.
  publish(window,{Mode::vsync,Mode::capped,30,false});
  CHECK(!window.editorNativeVsyncActive.load()&&toolbar_label(&ctx,30,true)=="30 FPS / 30 cap");
  publish(window,{Mode::vsync,Mode::vsync,0,false});
  CHECK(!window.editorNativeVsyncActive.load()&&toolbar_label(&ctx,61,true)=="61 FPS / uncapped");
  // Both required predicates must hold; native pacing in another mode is not VSync.
  publish(window,{Mode::capped,Mode::capped,30,true});
  CHECK(!window.editorNativeVsyncActive.load()&&toolbar_label(&ctx,30,true)=="30 FPS / 30 cap");
  publish(window,vsync);publish(window,{Mode::capped,Mode::capped,30,false});
  CHECK(!window.editorNativeVsyncActive.load()&&toolbar_label(&ctx,30,true)=="30 FPS / 30 cap");
  publish(window,vsync);publish(window,{Mode::uncapped,Mode::uncapped,0,false});
  CHECK(!window.editorNativeVsyncActive.load()&&window.uncappedFrameSubmission.load());
  CHECK(toolbar_label(&ctx,120,true)=="120 FPS / uncapped");
  publish(window,vsync);publish(window,{Mode::uncapped,Mode::capped,30,false,Activity::background});
  CHECK(!window.editorNativeVsyncActive.load()&&!window.uncappedFrameSubmission.load());
  CHECK(toolbar_label(&ctx,30,true)=="30 FPS / 30 cap");
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
