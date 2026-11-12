"""Compile the production pacing policy/deadline with a deterministic CPU clock.

No renderer, native window, model, GPU stress or real-time pacing test is run.
"""
from Tools.source.owned_source_text import read_owned_source
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
import hashlib,json,time
from Tests.support.cpp import compile_and_run, msvc_2022_toolsets, windows_sdk_includes

ROOT = Path(__file__).resolve().parents[2]


class FrameSubmissionPolicy(unittest.TestCase):
    def write_edition_marker(self, directory, value):
        marker = read_owned_source(ROOT / 'Engine/include/epoch.edition.hpp', encoding='utf-8')
        marker, replaced = re.subn(r'(?m)^#define EPOCH_SOURCE_EDITION_PUBLIC [01]$',
            f'#define EPOCH_SOURCE_EDITION_PUBLIC {value}', marker)
        self.assertEqual(replaced, 1)
        (directory / 'epoch.edition.hpp').write_text(marker, encoding='utf-8')

    def test_hosts_use_selected_limiter_without_extra_diagnostic_pause(self):
        # Integration guard: the compiled policy test below proves unlimited has
        # no deadline; host/producer callers must not append a second timer.
        for host in ('win', 'linux'):
            source = read_owned_source(ROOT / ('Engine/src/renderers/host/host.context_' + host + '.cpp'), encoding='utf-8')
            self.assertIn('coreFrameLimiter.wait_for_next_frame();', source)
            self.assertIn('startup, win.editorFramePacingOverride', source)
            self.assertNotIn('epochengine::platform::cooperative_pause();', source)
        source = read_owned_source(ROOT / 'Engine/src/epoch/epoch.engine_legacy.cpp', encoding='utf-8')
        wait = source.split('void wait_for_session_frame(', 1)[1].split('template <typename PumpFunc>', 1)[0]
        self.assertIn('limiter.wait_for_next_frame();', wait)
        self.assertIn('with_frame_pacing_override(policy, window->editorFramePacingOverride)', wait)
        self.assertNotIn('cooperative_pause', wait)

    def test_actual_policy_and_deadline_for_both_editions(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        parent = (ROOT / 'Artifacts/Reports/frame-pacing-fairness-oct30').resolve()
        parent.mkdir(parents=True, exist_ok=True)
        self.assertTrue(parent.is_relative_to(ROOT.resolve()))
        scratch = parent / str(time.time_ns());scratch.mkdir()
        records=[]
        try:
            module = read_owned_source(ROOT / 'Engine/dep/EpochPlatformEngine/modules/perf.tier.ixx', encoding='utf-8')
            start = module.index('export namespace epochengine::perf')
            end = module.index('\nnamespace epochengine::perf', start)
            api = module[start:end].replace('export namespace', 'namespace', 1)
            wait = module[module.index('    void frame_limiter::wait_for_next_frame()'):].replace('std::this_thread::yield()', 'pacing_clock::yield()')
            raylib = read_owned_source(
                ROOT / 'Engine/src/renderers/raylib/core.context_raylib.cpp', encoding='utf-8')
            callback_start = raylib.index('ctx->apply_frame_pacing = [](')
            callback_end = raylib.index('\n        ctx->draw_sprite', callback_start)
            raylib_callback = raylib[callback_start:callback_end].replace(
                'ctx->apply_frame_pacing', 'auto apply_raylib_native_pacing', 1)
            raylib_mock = '''namespace epochengine::raylib_api {
inline int target=-1; inline unsigned configurations{};
void set_target_fps(int value) { target=value; ++configurations; }
}
'''
            diagnostics = read_owned_source(
                ROOT / 'Engine/src/editor/application/editor.application_runtime_diagnostics.inl')
            finite = diagnostics.split('        [[nodiscard]] static double finite_or(', 1)[1].split(
                '        [[nodiscard]] static float snapshot_layout_split', 1)[0]
            finite = '        [[nodiscard]] static double finite_or(' + finite
            sanitize = diagnostics.split('        [[nodiscard]] static double snapshot_frame_limit(', 1)[1].split(
                '        [[nodiscard]] static canvas2d::ProjectSettings', 1)[0]
            sanitize = '        [[nodiscard]] static double snapshot_frame_limit(' + sanitize
            tier_start = diagnostics.index('        [[nodiscard]] static perf::tier recommended_editor_performance_tier(')
            tier_end = diagnostics.index('        [[nodiscard]] static std::string recommended_support_tier(', tier_start)
            recommend_tier = diagnostics[tier_start:tier_end]
            fps_for_tier = module[module.index('    double target_fps_for(tier t) noexcept\n'):
                module.index('    double target_fps_for(frame_limit_preset preset) noexcept\n')]
            session = read_owned_source(ROOT / 'Engine/src/editor/application/editor.application_context_session.inl')
            startup_selection = session[session.index('                if (core::cli::frame_limit_explicit)'):
                session.index('                it->second.automationCommand =')].replace(
                    'std::thread::hardware_concurrency()', 'hardwareThreadCount')
            startup_publish = session[session.index('                if (ctx && ctx->windowData)'):
                session.index('                if (const auto requestedWorkspace')]
            snapshot_source = read_owned_source(
                ROOT / 'Engine/src/editor/application/editor.application_context_snapshot.inl')
            capture = snapshot_source.split('        snapshot.editor_frame_limit_fps =', 1)[1].split(
                '        snapshot.', 2)
            capture = '        snapshot.editor_frame_limit_fps =' + capture[0] + '        snapshot.' + capture[1]
            restore = snapshot_source.split('        editor.editorFrameLimitFps =', 1)[1].split(
                '        editor.selectedProjectFile', 1)[0]
            restore = '        editor.editorFrameLimitFps =' + restore
            settings = read_owned_source(
                ROOT / 'Engine/src/editor/application/editor.frame_consent_settings.inl')
            manual = settings.split('                editor.editorFrameVsyncRequested = false;', 1)[1].split(
                '                push_editor_log(', 1)[0]
            manual = '                editor.editorFrameVsyncRequested = false;' + manual
            persistence = r'''
namespace pacing_fixture {
namespace perf=epochengine::perf;
SANITIZE
struct Window {perf::frame_pacing_override editorFramePacingOverride;};
namespace core {
enum class ContextType {OpenGL,Vulkan,RayLib,SFML,SDL,Software};
struct Context {std::shared_ptr<Window> windowData=std::make_shared<Window>();ContextType type{ContextType::OpenGL};};
namespace cli {bool frame_limit_explicit{},frame_vsync_requested{};double frame_limit_fps{};}
}
using core::Context;
RECOMMEND_TIER
struct Editor {double editorFrameLimitFps=120.0;bool editorFrameVsyncRequested=false;};
Editor start(const std::shared_ptr<Context>& ctx,unsigned hardwareThreadCount){
struct Slot {Editor second;};Slot slot;auto* it=&slot;
STARTUP_SELECTION
STARTUP_PUBLISH
return slot.second;
}
struct Snapshot {double editor_frame_limit_fps=120.0;bool editor_frame_vsync_requested=false;};
Snapshot capture(const Editor& editor) {Snapshot snapshot;
CAPTURE
return snapshot;}
void restore(Editor& editor,Context* ctx,const Snapshot& snapshot){
RESTORE
}
void select(Editor& editor,Context* ctx,std::size_t index){
struct Choice {double fps;};const std::array settingsLimitChoices{Choice{60.0},Choice{120.0},Choice{0.0}};
struct Selection {std::optional<std::size_t> selected_index;};
const Selection settingsEditorLimitSelect{index};
// UI boundary supplies the selected value; execute actual settings publication below.
editor.editorFrameLimitFps = settingsLimitChoices.at(index).fps;
MANUAL
}
}
'''.replace('SANITIZE', finite + sanitize).replace('CAPTURE', capture).replace(
                'RESTORE', restore).replace('MANUAL', manual).replace('RECOMMEND_TIER', recommend_tier).replace(
                'STARTUP_SELECTION', startup_selection).replace('STARTUP_PUBLISH', startup_publish)
            persistence = 'namespace epochengine::perf {\n' + fps_for_tier + '}\n' + persistence
            clock = '''namespace epochengine::core::time {
inline double now{},readStep=0.00005; inline unsigned reads{}, sleeps{};inline bool zeroSleep{};
// The independent monotonic clock advances on observation, even when the OS
// does not switch threads. Yield count must never drive deadline progress.
double now_seconds() { ++reads;const double observed=now;now+=readStep;return observed; }
void sleep_ms(std::uint32_t value) { ++sleeps;zeroSleep=zeroSleep||value==0; now+=static_cast<double>(value)/1000.0; }
}
namespace pacing_clock {
inline unsigned yields{};
// Deterministic OS scheduling port; production still calls this_thread::yield.
void yield(){++yields;}
}
'''
            main = r'''
int main() {
    using namespace epochengine::perf;
    constexpr auto foreground=frame_activity::foreground;
    constexpr auto uncapped=frame_pacing_mode::uncapped;
    const auto unrestrictedPolicy=select_frame_pacing_policy(uncapped,0.0,true,false);
    assert(unrestrictedPolicy.mode==uncapped && unrestrictedPolicy.target_hz==0.0);
    for(unsigned backend=0; backend!=7; ++backend) {
        const auto capabilities=frame_pacing_capabilities_for(static_cast<frame_pacing_backend>(backend));
        const auto desired=resolve_frame_pacing_with_capabilities(unrestrictedPolicy,foreground,capabilities);
        assert(desired.requested_mode==uncapped && desired.configured_hz==0.0);
        assert(desired.effective_mode==uncapped && desired.effective_hz==0.0);
        assert(!desired.cpu_deadline_wait && !desired.native_pacing_requested);
        const auto unchanged=finalize_native_frame_pacing(desired,{true,false,uncapped,0.0});
        assert(!unchanged.cpu_deadline_wait && unchanged.effective_hz==0.0);
        frame_limiter limiter; limiter.set_plan(unchanged);
        assert(limiter.target_dt==0.0);
        epochengine::core::time::reads=0; epochengine::core::time::sleeps=0;
        const auto previousYields=pacing_clock::yields;
        limiter.wait_for_next_frame();
        assert(pacing_clock::yields==previousYields+1);
        assert(epochengine::core::time::reads==0 && epochengine::core::time::sleeps==0);
        const auto pressure=with_frame_rate_ceiling(unchanged,10.0);
        assert(pressure.cpu_deadline_wait && pressure.effective_hz==10.0);
        const auto background=resolve_frame_pacing_with_capabilities(unrestrictedPolicy,frame_activity::background,capabilities);
        const auto minimized=resolve_frame_pacing_with_capabilities(unrestrictedPolicy,frame_activity::minimized,capabilities);
        assert(background.effective_hz==30.0 && minimized.effective_hz==10.0);
        // Every backend shares this CPU fallback if native admission is absent.
        // Exercise the real capability/policy/finalization chain, not a second
        // pacing implementation; neither fixed nor late turns may burst yields.
        for(double hz : {60.0,120.0,1500.0,5000.0}) {
            const auto desiredFixed=resolve_frame_pacing_with_capabilities(target_hz_policy(hz),foreground,capabilities);
            const auto fallbackFixed=finalize_native_frame_pacing(desiredFixed,{});
            assert(fallbackFixed.cpu_deadline_wait&&fallbackFixed.effective_hz==hz);
            frame_limiter fixed;fixed.set_plan(fallbackFixed);
            epochengine::core::time::now=10;epochengine::core::time::reads=0;
            epochengine::core::time::sleeps=0;pacing_clock::yields=0;
            fixed.wait_for_next_frame();
            assert(pacing_clock::yields==1&&epochengine::core::time::reads>2);
            assert(std::abs(fixed.next_time-(10+1/hz))<1e-12);
            epochengine::core::time::now=20;epochengine::core::time::sleeps=0;pacing_clock::yields=0;
            fixed.wait_for_next_frame();
            assert(fixed.next_time==20&&pacing_clock::yields==1&&epochengine::core::time::sleeps==0);
        }
    }
    const auto standalone=resolve_frame_pacing(select_frame_pacing_policy(uncapped,0.0,true,true),foreground,false);
    assert(standalone.effective_mode==uncapped && standalone.effective_hz==0.0);
    assert(!standalone.cpu_deadline_wait);
    auto editorContext=std::make_shared<pacing_fixture::Context>();
    assert(pacing_fixture::start(editorContext,12u).editorFrameLimitFps==120.0);
    const auto autoPolicy=with_frame_pacing_override(target_hz_policy(30),editorContext->windowData->editorFramePacingOverride);
    assert(resolve_frame_pacing(autoPolicy,foreground,false).effective_hz==120.0);
    assert(resolve_frame_pacing(autoPolicy,frame_activity::background,false).effective_hz==30.0);
    assert(pacing_fixture::start(editorContext,6u).editorFrameLimitFps==60.0);
    assert(pacing_fixture::start(editorContext,0u).editorFrameLimitFps==30.0);
    assert(pacing_fixture::start({},16u).editorFrameLimitFps==30.0);
    editorContext->type=pacing_fixture::core::ContextType::Software;
    assert(pacing_fixture::start(editorContext,16u).editorFrameLimitFps==30.0);
    pacing_fixture::core::cli::frame_limit_explicit=true;
    pacing_fixture::core::cli::frame_limit_fps=75.0;
    assert(pacing_fixture::start(editorContext,0u).editorFrameLimitFps==75.0);
    // Compile Raylib's actual native-admission callback against the actual
    // policy and deadline. Epoch's custom frame-control path never waits in
    // SetTargetFPS, so fixed/background/minimized rates must reach the host.
    const auto raylibCapabilities=frame_pacing_capabilities_for(frame_pacing_backend::raylib);
    assert(!raylibCapabilities.vsync && !raylibCapabilities.target_hz);
    for(auto mode : {frame_pacing_mode::target_hz, frame_pacing_mode::vsync, uncapped}) {
        for(auto activity : {foreground, frame_activity::background, frame_activity::minimized}) {
            for(double selectedHz : {59.94, 120.0, 240.0, 999.0, 1500.0, 5000.0}) {
                const auto selected=select_frame_pacing_policy(mode,selectedHz,true,false);
                const auto desired=resolve_frame_pacing_with_capabilities(selected,activity,raylibCapabilities);
                const auto native=apply_raylib_native_pacing(desired.effective_mode,desired.effective_hz);
                assert(epochengine::raylib_api::target==0);
                assert(native.configured && !native.pacing_active);
                const auto applied=finalize_native_frame_pacing(desired,native);
                assert(applied.native_pacing_configured && !applied.native_pacing_active);
                assert(!applied.native_pacing_requested && !applied.native_vsync_requested);
                const double expectedHz=activity==frame_activity::minimized ? 10.0
                    : activity==frame_activity::background ? 30.0 : mode==uncapped ? 0.0 : selectedHz;
                assert(std::abs(applied.effective_hz-expectedHz)<1e-12);
                assert(applied.cpu_deadline_wait==(expectedHz>0.0));
                frame_limiter actualHostLimiter; actualHostLimiter.set_plan(applied);
                epochengine::core::time::reads=0; epochengine::core::time::sleeps=0;
                pacing_clock::yields=0;
                actualHostLimiter.wait_for_next_frame();
                assert(pacing_clock::yields==1);
                assert((epochengine::core::time::reads>0)==(expectedHz>0.0));
                if(expectedHz==0.0) assert(epochengine::core::time::sleeps==0);
            }
        }
    }
    assert(epochengine::raylib_api::configurations==54);
    // Actual context-local selection and Editor capture/restore paths cannot
    // mutate another context or turn persisted Uncapped into a 15 FPS target.
    const auto startupVsync=select_frame_pacing_policy(frame_pacing_mode::vsync,60.0,true,false);
    pacing_fixture::Context contextA,contextB,restoredContext;
    pacing_fixture::Editor editorA,restoredEditor;
    assert(!contextA.windowData->editorFramePacingOverride.snapshot().explicit_selection);
    assert(with_frame_pacing_override(startupVsync,contextA.windowData->editorFramePacingOverride).mode==frame_pacing_mode::vsync);
    editorA.editorFrameVsyncRequested=true;
    pacing_fixture::select(editorA,&contextA,0);
    const auto selectedA=with_frame_pacing_override(startupVsync,contextA.windowData->editorFramePacingOverride);
    const auto untouchedB=with_frame_pacing_override(startupVsync,contextB.windowData->editorFramePacingOverride);
    assert(selectedA.mode==frame_pacing_mode::target_hz&&selectedA.target_hz==60.0);
    assert(!editorA.editorFrameVsyncRequested&&untouchedB.mode==frame_pacing_mode::vsync);
    pacing_fixture::select(editorA,&contextA,2);
    const auto saved=pacing_fixture::capture(editorA);
    assert(saved.editor_frame_limit_fps==0.0&&!saved.editor_frame_vsync_requested);
    pacing_fixture::restore(restoredEditor,&restoredContext,saved);
    const auto restoredPlan=with_frame_pacing_override(startupVsync,restoredContext.windowData->editorFramePacingOverride);
    assert(restoredEditor.editorFrameLimitFps==0.0&&restoredPlan.mode==uncapped);
    assert(!resolve_frame_pacing(restoredPlan,foreground,false).cpu_deadline_wait);
    pacing_fixture::restore(restoredEditor,&restoredContext,{60.0,true});
    assert(with_frame_pacing_override(target_hz_policy(120),restoredContext.windowData->editorFramePacingOverride).mode==frame_pacing_mode::vsync);
    contextA.windowData->editorFramePacingOverride.clear();
    assert(with_frame_pacing_override(target_hz_policy(120),contextA.windowData->editorFramePacingOverride).target_hz==120.0);
    std::atomic_bool publicationDone=false,invalidSnapshot=false;
    std::thread producer([&] {
        for(unsigned i=0;i<10000;++i){
            contextA.windowData->editorFramePacingOverride.publish(frame_pacing_mode::target_hz,240.0);
            contextA.windowData->editorFramePacingOverride.publish(frame_pacing_mode::vsync,60.0);
            contextA.windowData->editorFramePacingOverride.publish(uncapped,0.0);
        }
        publicationDone.store(true,std::memory_order_release);
    });
    do {
        const auto selection=contextA.windowData->editorFramePacingOverride.snapshot();
        if(selection.explicit_selection&&!(
            (selection.mode==frame_pacing_mode::target_hz&&selection.target_hz==240.0)
            ||(selection.mode==frame_pacing_mode::vsync&&selection.target_hz==60.0)
            ||(selection.mode==uncapped&&selection.target_hz==0.0))) invalidSnapshot=true;
    }while(!publicationDone.load(std::memory_order_acquire));
    producer.join();assert(!invalidSnapshot.load());
    frame_limiter unrestricted; unrestricted.set_plan(standalone);
    epochengine::core::time::reads=0; epochengine::core::time::sleeps=0;
    unrestricted.wait_for_next_frame();
    assert(epochengine::core::time::reads==0 && epochengine::core::time::sleeps==0);
    for(double hz : {60.0,120.0,1500.0,5000.0}) {
        const auto fixed=resolve_frame_pacing(select_frame_pacing_policy(frame_pacing_mode::target_hz,hz,true,false),foreground,false);
        assert(fixed.effective_hz==hz && fixed.cpu_deadline_wait);
    }
    for(double hz : {1500.0,5000.0}) {
        double parsed{};assert(parse_frame_limit_value(std::to_string(hz),parsed)&&parsed==hz);
        editorA.editorFrameLimitFps=parsed;editorA.editorFrameVsyncRequested=false;
        contextA.windowData->editorFramePacingOverride.publish(frame_pacing_mode::target_hz,parsed);
        const auto highSaved=pacing_fixture::capture(editorA);
        pacing_fixture::restore(restoredEditor,&restoredContext,highSaved);
        const auto policy=with_frame_pacing_override(startupVsync,restoredContext.windowData->editorFramePacingOverride);
        const auto high=resolve_frame_pacing_with_capabilities(policy,foreground,{true,false});
        assert(restoredEditor.editorFrameLimitFps==hz&&policy.target_hz==hz);
        assert(high.configured_hz==hz&&high.effective_hz==hz&&high.cpu_deadline_wait);
    }
    // Fine waits cooperate without inserting an integer-ms minimum or changing deadlines.
    for(double hz : {1500.0,5000.0}) {
        frame_limiter fine;fine.set_plan(resolve_frame_pacing(target_hz_policy(hz),foreground,false));
        epochengine::core::time::readStep=1/(hz*64);
        epochengine::core::time::now=10;epochengine::core::time::reads=0;epochengine::core::time::sleeps=0;pacing_clock::yields=0;
        fine.wait_for_next_frame();
        assert(pacing_clock::yields==1&&epochengine::core::time::sleeps==0&&epochengine::core::time::reads>32);
        assert(std::abs(fine.next_time-(10+1/hz))<1e-12&&fine.target_dt==1/hz);
        fine.next_time=1;epochengine::core::time::now=10;pacing_clock::yields=0;
        fine.wait_for_next_frame();
        assert(fine.next_time==10&&pacing_clock::yields==1&&epochengine::core::time::sleeps==0);
    }
    epochengine::core::time::readStep=0.00005;
    frame_limiter coarse;coarse.set_plan(resolve_frame_pacing(target_hz_policy(120),foreground,false));
    epochengine::core::time::now=10;epochengine::core::time::sleeps=0;pacing_clock::yields=0;
    coarse.wait_for_next_frame();
    assert(epochengine::core::time::sleeps>0&&pacing_clock::yields==1&&!epochengine::core::time::zeroSleep);
    assert(std::abs(coarse.next_time-(10+1/120.0))<1e-12);
    assert(sanitize_frame_hz((std::numeric_limits<double>::max)(),120)==(std::numeric_limits<double>::max)());
    const auto nativeVsync=finalize_native_frame_pacing(resolve_frame_pacing(
        select_frame_pacing_policy(frame_pacing_mode::vsync,60.0,true,false),foreground,true),
        {true,true,frame_pacing_mode::vsync,0.0});
    assert(nativeVsync.effective_mode==frame_pacing_mode::vsync && nativeVsync.effective_hz==0.0);
    assert(!nativeVsync.cpu_deadline_wait);
    for(bool standaloneProject : {false,true}) {
        const auto startup=resolve_frame_pacing(
            select_frame_pacing_policy(uncapped,0.0,false,standaloneProject),foreground,false);
        assert(startup.effective_mode==frame_pacing_mode::target_hz);
        assert(startup.effective_hz==30.0 && startup.cpu_deadline_wait);
    }
    for(double invalid : {-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        const auto safe=resolve_frame_pacing(select_frame_pacing_policy(frame_pacing_mode::target_hz,invalid,true,false),foreground,false);
        assert(safe.effective_hz==30.0 && safe.cpu_deadline_wait);
    }
    const double dt=1.0/120.0;
    assert(std::abs(next_frame_deadline(0,100,dt,false)-(100+dt))<1e-12);
    assert(next_frame_deadline(0,100,dt,true)==100); // no catch-up burst
    return 0;
}
'''
            for edition, value in (('development', 0), ('public', 1)):
                include = scratch / edition
                include.mkdir()
                self.write_edition_marker(include, value)
                source = include / 'contract.cpp'
                source.write_text('#include "epoch.edition.hpp"\n#include <cstdint>\n#include <cmath>\n'
                    '#include <charconv>\n#include <system_error>\n#include <cassert>\n#include <initializer_list>\n#include <limits>\n'
                    '#include <algorithm>\n#include <atomic>\n#include <array>\n#include <memory>\n#include <optional>\n#include <thread>\n'
                    '#include <string>\n#include <string_view>\n' + clock + api + '\nnamespace epochengine::perf {\n' + wait
                    + raylib_mock + raylib_callback + persistence
                    + '\n#define CHECK(...) do { if(!(__VA_ARGS__)) return __LINE__; } while(false)\n'
                    + main.replace('assert(', 'CHECK('),
                    encoding='utf-8')
                modes=compile_and_run(self,source,include,windows_flags=('/WX',),compile_timeout=90,run_timeout=10)
                records.append({'edition':edition,'configurations':modes})
            (scratch/'result.json').write_text(json.dumps({'scope':'Actual policy/override/snapshot/deadline with deterministic clock/sleep/yield ports; both physical edition markers; no window/GPU',
                'owner_sha256':hashlib.sha256((ROOT/'Engine/dep/EpochPlatformEngine/modules/perf.tier.ixx').read_bytes()).hexdigest(),'results':records},indent=2),encoding='utf-8')
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent))
            # Retain source, complete compiler logs and results under this report.
            pass

    def test_actual_gl_admission_with_mocked_driver_for_both_editions(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        toolsets = msvc_2022_toolsets()
        kitroot = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = windows_sdk_includes(kitroot)
        if not toolsets or not kits:
            self.skipTest('MSVC/SDK unavailable')
        parent = (ROOT / 'Artifacts/Intermediate/tests/frame-submission').resolve()
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='gpu-admission-', dir=parent))
        try:
            owner = read_owned_source(ROOT / 'Engine/src/renderers/opengl/opengl.context_process_impl.cpp', encoding='utf-8')
            admission_start = owner.index('const auto admissionStart = FrameClock::now();')
            admission_body = owner[admission_start:owner.index('const auto admissionEnd = FrameClock::now();', admission_start)]
            submission_start = owner.index('if (uncappedSubmission && !frame_admission::submitted(')
            submission_body = owner[submission_start:owner.index('const double admissionMs', submission_start)]
            contract = r'''#include "epoch.edition.hpp"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#define Status int
#include "opengl.frame_admission.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#define CHECK(...) do { if(!(__VA_ARGS__)) return __LINE__; } while(false)

namespace mock
{
    struct Fence { int owner{}; bool complete{}, flushed{}; };
    inline std::unordered_map<GLsync, Fence> live;
    inline std::uintptr_t id{};
    inline int owner = 1;
    inline unsigned maximum{}, waits{}, polls{}, flushes{}, deletes{}, wrong_owner{};
    inline unsigned timeouts{}, flush_requests{}, wrong_flags{}, missing_flush{};
    inline unsigned owner_swaps{};
    inline bool failed{}, null_fence{}, complete_new{};

    GLsync APIENTRY create(GLenum condition, GLbitfield flags)
    {
        if (condition != GL_SYNC_GPU_COMMANDS_COMPLETE || flags || null_fence)
            return nullptr;
        const auto fence = reinterpret_cast<GLsync>(++id);
        live.emplace(fence, Fence{owner, complete_new});
        maximum = (std::max)(maximum, static_cast<unsigned>(live.size()));
        return fence;
    }

    GLenum APIENTRY wait(GLsync fence, GLbitfield flags, GLuint64 timeout)
    {
        const auto it = live.find(fence);
        if (it == live.end() || it->second.owner != owner)
        {
            ++wrong_owner;
            return GL_WAIT_FAILED;
        }
        // The initial nonblocking poll must request a same-context flush;
        // subsequent finite waits must not repeatedly flush newer work.
        if (flags != (timeout ? 0u : GL_SYNC_FLUSH_COMMANDS_BIT))
        {
            ++wrong_flags;
            return GL_WAIT_FAILED;
        }
        if (flags & GL_SYNC_FLUSH_COMMANDS_BIT)
        {
            ++flush_requests;
            it->second.flushed = true;
        }
        timeout ? ++waits : ++polls;
        if (failed)
            return GL_WAIT_FAILED;
        if (timeouts)
        {
            --timeouts;
            return GL_TIMEOUT_EXPIRED;
        }
        if (!it->second.complete && timeout)
        {
            if (!it->second.flushed)
            {
                ++missing_flush;
                return GL_WAIT_FAILED;
            }
            it->second.complete = true;
            return GL_CONDITION_SATISFIED;
        }
        return it->second.complete ? GL_ALREADY_SIGNALED : GL_TIMEOUT_EXPIRED;
    }

    void APIENTRY erase(GLsync fence)
    {
        const auto it = live.find(fence);
        if (it == live.end() || it->second.owner != owner)
            ++wrong_owner;
        else
            live.erase(it);
        ++deletes;
    }
    void APIENTRY flush() { ++flushes; }
}

extern "C" {
PFNGLFENCESYNCPROC glad_glFenceSync = mock::create;
PFNGLCLIENTWAITSYNCPROC glad_glClientWaitSync = mock::wait;
PFNGLDELETESYNCPROC glad_glDeleteSync = mock::erase;
PFNGLFLUSHPROC glad_glFlush = mock::flush;
}

// Extract the real process owner's admission/submission gates. The existing
// mock driver measures actual GL calls; no second implementation of policy.
__OWNER_GATED_FRAME__

int main()
{
    using namespace epochengine::openglcontext::frame_admission;
    State state{};
    auto running = [] { return true; };
    CHECK(available() && frames_in_flight == 2u);
    CHECK(admit(state, running).status == AdmissionStatus::ready);
    CHECK(submitted(state) && submitted(state));
    CHECK(mock::live.size() == 2u && mock::maximum == 2u);
    CHECK(!submitted(state)); // cannot overwrite unfinished work

    unsigned checks{};
    mock::timeouts = 10u;
    auto cancelled = admit(state, [&] { return ++checks < 5u; });
    CHECK(cancelled.status == AdmissionStatus::cancelled && cancelled.blocking_waits > 0u);
    CHECK(mock::live.size() == 2u && !submitted(state));
    CHECK(mock::waits > 0u && wait_slice_ns == 1'000'000u);

    mock::timeouts = 0u;
    mock::failed = true;
    CHECK(admit(state, running).status == AdmissionStatus::failed);
    CHECK(mock::live.size() == 2u && !submitted(state));
    mock::failed = false;
    mock::timeouts = 100u;
    const auto bounded = admit(state, running, 3u);
    CHECK(bounded.status == AdmissionStatus::failed && bounded.blocking_waits == 3u);
    CHECK(mock::live.size() == 2u && !submitted(state));
    mock::timeouts = 0u;
    for (auto& [_, fence] : mock::live) fence.complete = true;
    const auto before_waits = mock::waits;
    CHECK(admit(state, running).status == AdmissionStatus::ready);
    CHECK(mock::waits == before_waits && mock::live.size() == 1u);
    mock::complete_new = true;
    for (unsigned frame = 0; frame < 20000u; ++frame)
    {
        CHECK(submitted(state));
        CHECK(admit(state, running).status == AdmissionStatus::ready);
    }
    CHECK(mock::maximum == 2u && mock::waits == before_waits);
    CHECK(release(state) && mock::live.empty());
    CHECK(mock::flushes == 0u && !mock::wrong_flags && !mock::missing_flush);

    // An old unsignaled fence must reach the server even with no further frame
    // submission or explicit glFlush function. One flush poll then one finite
    // wait provides forward progress, without assuming SwapBuffers flushes it.
    glad_glFlush = nullptr;
    CHECK(available());
    mock::complete_new = false;
    State pending{};
    CHECK(submitted(pending) && submitted(pending));
    CHECK(!mock::live.at(pending.fences[pending.next]).flushed);
    const auto initial_polls = mock::polls;
    const auto initial_waits = mock::waits;
    const auto initial_flush_requests = mock::flush_requests;
    const auto progressed = admit(pending, running, 1u);
    CHECK(progressed.status == AdmissionStatus::ready && progressed.blocking_waits == 1u);
    CHECK(mock::polls == initial_polls + 1u && mock::waits == initial_waits + 1u);
    CHECK(mock::flush_requests == initial_flush_requests + 1u && mock::live.size() == 1u);
    CHECK(release(pending) && mock::live.empty());
    mock::complete_new = true;

    // Each native context has its own ring. Retiring or closing one must not
    // inspect/delete another context's fences.
    State first{}, second{};
    mock::owner = 1;
    CHECK(submitted(first) && submitted(first));
    mock::owner = 2;
    CHECK(submitted(second) && submitted(second));
    CHECK(admit(second, running).status == AdmissionStatus::ready);
    CHECK(release(second) && mock::live.size() == 2u);
    mock::owner = 1;
    CHECK(release(first) && mock::live.empty() && !mock::wrong_owner);

    State switched{};
    OwnerWindow window{};
    OwnerContext context{&window};
    const auto capped_id = mock::id;
    const auto capped_polls = mock::polls;
    const auto capped_waits = mock::waits;
    const auto capped_deletes = mock::deletes;
    const auto capped_swaps = mock::owner_swaps;
    for (unsigned frame=0; frame<20000u; ++frame)
        CHECK(owner_frame(false, switched, &context));
    CHECK(mock::owner_swaps == capped_swaps+20000u);
    CHECK(mock::id == capped_id && mock::polls == capped_polls);
    CHECK(mock::waits == capped_waits && mock::deletes == capped_deletes && mock::live.empty());
    CHECK(owner_frame(true, switched, &context) && owner_frame(true, switched, &context));
    CHECK(mock::live.size()==2u);
    const auto transition_deletes = mock::deletes;
    const auto transition_id = mock::id;
    CHECK(owner_frame(false, switched, &context));
    CHECK(mock::live.empty() && mock::deletes==transition_deletes+2u && mock::id==transition_id);
    CHECK(owner_frame(false, switched, &context));
    CHECK(mock::deletes==transition_deletes+2u);
    CHECK(owner_frame(true, switched, &context) && owner_frame(true, switched, &context));
    CHECK(owner_frame(true, switched, &context) && mock::live.size()==2u);
    const auto before_cancel_swaps = mock::owner_swaps;
    window.running=false;
    CHECK(!owner_frame(true, switched, &context) && mock::owner_swaps==before_cancel_swaps);
    window.running=true;
    CHECK(release(switched) && mock::live.empty());

    mock::null_fence = true;
    CHECK(!submitted(first) && mock::live.empty());
    mock::null_fence = false;
    glad_glClientWaitSync = nullptr;
    CHECK(admit(first, running).status == AdmissionStatus::failed);
    CHECK(!submitted(first) && mock::live.empty());
    glad_glClientWaitSync = mock::wait;
    CHECK(admit(first, [] { return false; }).status == AdmissionStatus::cancelled);
    CHECK(!mock::wrong_owner && !mock::wrong_flags && !mock::missing_flush);
    CHECK(mock::flushes == 0u && mock::flush_requests > 20000u && mock::deletes > 20000u);
    return 0;
}
'''
            contract = contract.replace('__OWNER_GATED_FRAME__', r'''
namespace frame_admission = epochengine::openglcontext::frame_admission;
struct OwnerWindow {
    std::atomic_bool running{true};
    bool get_should_close() const noexcept { return false; }
};
struct OwnerContext { OwnerWindow* windowData{}; };
bool owner_frame(bool uncappedSubmission, frame_admission::State& state, OwnerContext* ctx) {
    using FrameClock = std::chrono::steady_clock;
    struct NativeState { frame_admission::State& frameAdmission; } nativeState{state};
''' + admission_body + '\n++mock::owner_swaps;\n' + submission_body + '\nreturn true;\n}\n')
            toolset, kit = toolsets[-1], kits[-1]
            for edition, value in (('development', 0), ('public', 1)):
                lane = scratch / edition
                lane.mkdir()
                self.write_edition_marker(lane, value)
                source = lane / 'contract.cpp'
                source.write_text(contract, encoding='utf-8')
                binary = lane / 'contract.exe'
                args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/WX',
                    '/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'), '/I' + str(kit / 'shared'),
                    '/I' + str(kit / 'um'), '/I' + str(ROOT / 'Engine/src/renderers/opengl'),
                    '/I' + str(ROOT / 'Engine/third_party/glad/include'),
                    '/Fo' + str(lane / 'contract.obj'), '/Fe' + str(binary), str(source),
                    '/link', '/LIBPATH:' + str(toolset / 'lib/x64'),
                    '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'um/x64'),
                    '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'ucrt/x64')]
                result = subprocess.run(args, cwd=lane, capture_output=True, text=True, timeout=90)
                self.assertEqual(result.returncode, 0, result.stdout[-3500:] + result.stderr)
                result = subprocess.run([str(binary)], cwd=lane, capture_output=True, text=True, timeout=15)
                lines = contract.splitlines()
                failure = lines[result.returncode - 1] if 0 < result.returncode <= len(lines) else ''
                self.assertEqual(result.returncode, 0, edition + ' GL admission: ' + failure)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent))
            shutil.rmtree(scratch)

    def test_actual_host_mode_publication_and_render_insertion(self):
        for relative in ('Engine/src/renderers/host/host.context_win.cpp',
                         'Engine/src/renderers/host/host.context_linux.cpp'):
            source = read_owned_source(ROOT / relative, encoding='utf-8')
            self.assertIn('select_frame_pacing_policy(', source)
            self.assertIn('coreFrameLimiter.set_plan(framePlan);', source)
            publication = source.split('win.uncappedFrameSubmission.store(', 1)[1].split(';', 1)[0]
            for condition in ('framePlan.requested_mode', 'framePlan.effective_mode',
                              'framePlan.activity', '!framePlan.native_pacing_active'):
                self.assertIn(condition, publication)
            self.assertNotIn('desktop_safety', source)
        source = read_owned_source(ROOT / 'Engine/src/renderers/opengl/opengl.context_process_impl.cpp', encoding='utf-8')
        # The earlier state lookup only selects CPU handles/dimensions.
        # GPU admission must use the state selected after the native guard.
        native_state = source.index('auto& nativeState = opengltextures::state_for_platform_context(')
        admission = source.index('frame_admission::admit(')
        self.assertLess(source.index('guard.set(desired)'), native_state)
        self.assertLess(native_state, admission)
        self.assertIn('backend, guard.target(), ctx.get()', source[native_state:admission])
        self.assertLess(source.index('frame_admission::admit('), source.index('glViewport('))
        self.assertLess(source.index('glViewport('), source.index('opengl_clear();'))
        self.assertLess(source.index('opengl_clear();'), source.index('opengl_render_active_frame('))
        self.assertLess(source.index('opengl_render_active_frame('), source.index('PlatformGL::swap_buffers('))
        self.assertLess(source.index('PlatformGL::swap_buffers('), source.index('frame_admission::submitted('))
        # Restore the retained capped policy: no new GL synchronization at a
        # finite/VSync target, and retire the previous uncapped ring on transition.
        # Actual gates and GL call counts are exercised by the compiled test above.
        self.assertIn('if (uncappedSubmission)', source)
        self.assertIn('uncappedSubmission && !frame_admission::submitted', source)
        self.assertIn('else if (!frame_admission::release(nativeState.frameAdmission))', source)
        self.assertIn('ctx->windowData->running.load(', source)
        self.assertIn('!ctx->windowData->get_should_close()', source)
        self.assertIn('!core::cli::frame_vsync_requested', source)
        self.assertIn('core::cli::frame_limit_explicit', source)
        self.assertIn(": 2'000u", source)
        self.assertIn('phaseEvidence.uncapped != uncappedSubmission', source)
        cleanup = read_owned_source(ROOT / 'Engine/src/renderers/opengl/opengl.context_cleanup.cpp', encoding='utf-8')
        self.assertLess(cleanup.index('#include <windows.h>'), cleanup.index('#include "opengl.frame_admission.hpp"'))
        self.assertIn('context_gl_states.find(', cleanup)
        self.assertIn('platform_context_key(native)', cleanup)
        self.assertIn('if (nativeCurrent)', cleanup)
        self.assertIn('admission = {};', cleanup)
        app = read_owned_source(ROOT / 'Engine/src/editor/editor.application.cpp', encoding='utf-8')
        self.assertNotIn('desktop safety', app)
        self.assertNotIn('120 FPS safety', app)
        self.assertIn('import gui.frame_limit_control;', app)
        control = read_owned_source(ROOT / 'Engine/modules/gui/gui.frame_limit_control.ixx', encoding='utf-8')
        self.assertIn('.selected = edit.manual ? "Manual" : vsync ? "VSync"', control)
        settings = read_owned_source(ROOT / 'Engine/src/editor/application/editor.frame_consent_settings.inl')
        self.assertNotIn('core::cli::frame_limit_explicit =', settings)
        self.assertNotIn('core::cli::frame_limit_fps =', settings)
        self.assertIn('editorFramePacingOverride.publish(', settings)
        self.assertIn('frame_limit_control("settings-project-frame-limit-select", editor.projectRunFrameLimitFps,', app)


if __name__ == '__main__':
    unittest.main()

