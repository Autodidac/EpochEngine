"""Actual Engine control and perf modules; recording widget ports, no native runtime."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
CONTROL = ROOT / "Engine/modules/gui/gui.frame_limit_control.ixx"


def gui_port():
    api = (ROOT / "Engine/modules/gui/gui.engine.ixx").read_text(encoding="utf-8")
    types = "\n".join(definition(api, marker, True).replace("export ", "") for marker in (
        "    export struct Vec2", "    export struct EditBoxResult",
        "    export enum class TextInputKeyboardPolicy", "    export enum class InlineButtonAlignment",
        "    export struct SelectBoxOptions", "    export struct SelectBoxResult"))
    return r'''
module;
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
export module gui.engine;
export namespace epochengine::gui {
TYPES
struct RecordingPort {
 Vec2 cursor{};std::optional<std::size_t> choice{};bool submit{},apply{},enabled{},suppressed{};
 std::string selected,label;std::size_t calls{};
};
inline RecordingPort port{};
inline Vec2 cursor_position(){return port.cursor;}
inline void set_cursor(Vec2 p){port.cursor=p;}
inline void label(std::string_view text){port.label=text;port.cursor.y+=20;}
struct ScopedInputSuppression {
 bool previous{};explicit ScopedInputSuppression(bool disabled=true):previous(port.suppressed){port.suppressed=previous||disabled;}
 ~ScopedInputSuppression(){port.suppressed=previous;}
};
inline SelectBoxResult select_box(const SelectBoxOptions& options){
 ++port.calls;port.selected=options.selected;port.enabled=options.enabled;
 const auto choice=port.choice;port.choice.reset();return {false,choice.has_value(),choice};
}
inline EditBoxResult edit_box(std::string&,Vec2,std::size_t,bool,TextInputKeyboardPolicy,bool,std::string_view){return {false,false,port.submit&&!port.suppressed};}
inline bool button(std::string_view,Vec2){return port.apply&&!port.suppressed;}
}
'''.replace("TYPES", types)


CONSUMER = r'''
#include <array>
#include <atomic>
#include <string>
#include <thread>
import gui.engine;
import gui.frame_limit_control;
import perf.tier;
#define CHECK(x) do { if(!(x)) return __LINE__; } while(false)
int main(){
 namespace gui=epochengine::gui;namespace perf=epochengine::perf;
 const auto startup=perf::target_hz_policy(30.0);
 perf::frame_pacing_override first,other;other.publish(perf::frame_pacing_mode::target_hz,60.0);
 gui::FrameLimitEditState edit;
 const auto drawTarget=[&](perf::frame_pacing_override* target){
  gui::port.cursor={};return gui::live_frame_limit_control("live",startup,target,edit,378.0f);};
 const auto draw=[&]{return drawTarget(&first);};
 CHECK(!draw()&&!first.snapshot().explicit_selection&&gui::port.selected=="30 FPS");
 CHECK(edit.text=="1500"&&!edit.manual);
 gui::port.choice=2;CHECK(draw());
 auto policy=perf::with_frame_pacing_override(startup,first);
 CHECK(policy.target_hz==120&&policy.background_hz==30&&policy.minimized_hz==10);
 for(const auto capabilities:{perf::frame_pacing_capabilities{},perf::frame_pacing_capabilities{true,false},perf::frame_pacing_capabilities{false,true}}){
  CHECK(perf::resolve_frame_pacing_with_capabilities(policy,perf::frame_activity::foreground,capabilities).effective_hz==120);
  CHECK(perf::resolve_frame_pacing_with_capabilities(policy,perf::frame_activity::background,capabilities).effective_hz==30);
  CHECK(perf::resolve_frame_pacing_with_capabilities(policy,perf::frame_activity::minimized,capabilities).effective_hz==10);
 }
 CHECK(other.snapshot().target_hz==60);
 // Manual is a deliberate numeric selection; its initial text never implies uncapped.
 gui::port.choice=4;CHECK(draw()&&first.snapshot().target_hz==1500);
 for(const auto invalid:{"", "-1", "nan", "inf", "1fps", "1e999"}){
  edit.text=invalid;gui::port.submit=true;
  CHECK(!draw()&&first.snapshot().target_hz==1500&&!edit.error.empty());
 }
 gui::port.submit=false;gui::port.apply=true;edit.text="59.94";
 CHECK(draw()&&first.snapshot().target_hz==59.94&&edit.error.empty());
 gui::port.apply=false;edit.text="0";gui::port.submit=true;
 CHECK(draw()&&first.snapshot().mode==perf::frame_pacing_mode::uncapped);
 gui::port.submit=false;gui::port.choice=2;CHECK(draw()&&first.snapshot().target_hz==120);
 edit.text="-50";gui::port.choice=4;CHECK(!draw()&&first.snapshot().target_hz==120);
 // Explicit unlimited remains available without launching an uncapped runtime.
 gui::port.choice=3;CHECK(draw()&&first.snapshot().mode==perf::frame_pacing_mode::uncapped);
 gui::port.choice=2;CHECK(draw());
 // A refused context cannot consume an apparent widget activation or publish elsewhere.
 gui::port.choice=0;CHECK(!drawTarget(nullptr)&&!gui::port.enabled);
 CHECK(first.snapshot().target_hz==120&&other.snapshot().target_hz==60);
 CHECK(gui::port.label=="Frame limit unavailable: no runtime window.");
 gui::FrameLimitEditState vsyncEdit;perf::frame_pacing_override vsyncOverride;
 const auto vsync=perf::select_frame_pacing_policy(perf::frame_pacing_mode::vsync,60,true,false);
 CHECK(!gui::live_frame_limit_control("vsync",vsync,&vsyncOverride,vsyncEdit,378));
 CHECK(gui::port.selected=="VSync"&&!vsyncOverride.snapshot().explicit_selection);
 gui::FrameLimitEditState highEdit;double high=144.5;
 CHECK(!gui::frame_limit_control("high",high,highEdit,180)&&highEdit.manual&&highEdit.text=="144.5");
 CHECK(gui::port.cursor.y>=66);
 // The actual atomic publication is consumed concurrently while the UI changes rapidly.
 std::atomic<bool> running{true},invalid{false};
 std::thread reader([&]{while(running.load()){
  const auto p=perf::with_frame_pacing_override(startup,first);
  if((p.target_hz!=30&&p.target_hz!=60&&p.target_hz!=120)||p.background_hz!=30||p.minimized_hz!=10)invalid=true;
 }});
 for(unsigned i=0;i<3000;++i){gui::port.choice=i%3;CHECK(draw());}
 running=false;reader.join();CHECK(!invalid&&other.snapshot().target_hz==60);
 CHECK(first.snapshot().target_hz==120);
 return 0;
}
'''


class LiveFrameLimit(unittest.TestCase):
    def test_runtime_wiring_is_pinned_and_window_local(self):
        owner = (ROOT / "Engine/src/epoch/implementation/epoch.project_play_scene.inl").read_text(encoding="utf-8")
        header = owner[owner.index("                        draw_runtime_controls_hide(controlsContentWidth);"):]
        self.assertLess(header.index("draw_runtime_frame_limit(ctx, controlsContentWidth)"), header.index("gui::begin_scroll_area"))
        adapter = definition(owner, "            void draw_runtime_frame_limit(")
        self.assertIn("ctx && ctx->windowData ? &ctx->windowData->editorFramePacingOverride : nullptr", adapter)
        self.assertIn("m_runtimeFrameLimitEdits[ctx.get()]", adapter)
        self.assertNotIn("persist_", adapter)
        self.assertNotIn("editorThrottleHz", adapter)
        for flag in ("frame_limit_fps", "frame_limit_explicit", "frame_vsync_requested"):
            self.assertNotIn("cli::" + flag + " =", adapter)
        for path in (ROOT / "Engine/src/epoch/epoch.engine_legacy.cpp",
                     ROOT / "Engine/src/renderers/host/implementation/host.render_loop.inl"):
            self.assertIn("with_frame_pacing_override(", path.read_text(encoding="utf-8"))

    def test_actual_control_module_and_pacing_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC C++23 module consumer")
        port = gui_port()
        identity = hashlib.sha256((CONTROL.read_text(encoding="utf-8") + port + CONSUMER).encode()).hexdigest()[:12]
        report = ROOT / "Artifacts/Reports/live-frame-limit86-nov07" / identity
        build = ROOT / "Artifacts/Intermediate/cmake/live-frame-limit86" / identity
        report.mkdir(parents=True, exist_ok=True)
        temporary = report / "tmp"
        temporary.mkdir(exist_ok=True)
        env = dict(os.environ, TEMP=str(temporary), TMP=str(temporary), TMPDIR=str(temporary))
        (report / "gui.engine.ixx").write_text(port, encoding="utf-8")
        (report / "consumer.cpp").write_text(CONSUMER, encoding="utf-8")
        platform = ROOT / "Engine/dep/EpochPlatformEngine"
        modules = [report / "gui.engine.ixx", CONTROL, *(platform / "modules" / name for name in (
            "core.env.ixx", "core.time.ixx", "perf.tier.ixx"))]
        sources = [platform / "src/core" / name for name in ("core.env.cpp", "core.time.cpp")]
        (report / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.30)\nproject(LiveFrameLimit LANGUAGES CXX)\n'
            + 'add_executable(live_frame_limit consumer.cpp ' + " ".join('"' + p.as_posix() + '"' for p in sources) + ')\n'
            + 'target_compile_features(live_frame_limit PRIVATE cxx_std_23)\n'
            + 'set_property(TARGET live_frame_limit PROPERTY MSVC_DEBUG_INFORMATION_FORMAT Embedded)\n'
            + 'target_compile_options(live_frame_limit PRIVATE /W3 /WX /utf-8 /EHsc)\n'
            + 'target_sources(live_frame_limit PRIVATE FILE_SET CXX_MODULES BASE_DIRS "' + ROOT.as_posix() + '" FILES '
            + " ".join('"' + p.as_posix() + '"' for p in modules) + ')\n', encoding="utf-8")
        commands = [["cmake", "-S", str(report), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"], [str(build / config / "live_frame_limit.exe")]]
        runs = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=report, env=env, capture_output=True, text=True, timeout=120)
            log = report / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            runs.append({"command": command, "exit": completed.returncode, "log": str(log)})
            (report / "result.json").write_text(json.dumps({
                "passed": completed.returncode == 0 and index == len(commands) - 1,
                "scope": "Independent actual gui.frame_limit_control module consumer plus actual perf.tier/core.env/core.time; widget activation/edit ports only. Foreground120/background30/minimized10, invalid manual refusal, Manual1500, explicit zero, VSync display, missing-window refusal, per-context isolation and rapid concurrent publication. CPU only; no native pacing/runtime eye-test.",
                "owner_sha256": hashlib.sha256(CONTROL.read_bytes()).hexdigest(), "runs": runs}, indent=2) + "\n", encoding="utf-8")
            self.assertEqual(completed.returncode, 0, completed.stdout[-8000:] + completed.stderr[-1500:])


if __name__ == "__main__":
    unittest.main()
