"""Compile the production pacing policy/deadline with a deterministic CPU clock.

No renderer, native window, model, GPU stress or real-time pacing test is run.
"""
from Tools.source.owned_source_text import read_owned_source
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class FrameSubmissionPolicy(unittest.TestCase):
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
        toolsets = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kitroot = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((kitroot / 'Include').glob('*'))
        if not toolsets or not kits:
            self.skipTest('MSVC/SDK unavailable')
        parent = (ROOT / 'build/contract-tests').resolve()
        parent.mkdir(parents=True, exist_ok=True)
        self.assertTrue(parent.is_relative_to(ROOT.resolve()))
        scratch = Path(tempfile.mkdtemp(prefix='desktop-pacing-', dir=parent))
        try:
            module = read_owned_source(ROOT / 'Engine/dep/EpochPlatformEngine/modules/perf.tier.ixx', encoding='utf-8')
            start = module.index('export namespace epochengine::perf')
            end = module.index('\nnamespace epochengine::perf', start)
            api = module[start:end].replace('export namespace', 'namespace', 1)
            wait = module[module.index('    void frame_limiter::wait_for_next_frame()'):]
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
struct Context {std::shared_ptr<Window> windowData=std::make_shared<Window>();};
struct Editor {double editorFrameLimitFps=120.0;bool editorFrameVsyncRequested=false;};
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
                'RESTORE', restore).replace('MANUAL', manual)
            clock = '''namespace epochengine::core::time {
inline double now{}; inline unsigned reads{}, sleeps{};
double now_seconds() { ++reads; const auto value=now; now+=0.0002; return value; }
void sleep_ms(std::uint32_t value) { ++sleeps; now+=static_cast<double>(value)/1000.0; }
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
        limiter.wait_for_next_frame();
        assert(epochengine::core::time::reads==0 && epochengine::core::time::sleeps==0);
        const auto pressure=with_frame_rate_ceiling(unchanged,10.0);
        assert(pressure.cpu_deadline_wait && pressure.effective_hz==10.0);
        const auto background=resolve_frame_pacing_with_capabilities(unrestrictedPolicy,frame_activity::background,capabilities);
        const auto minimized=resolve_frame_pacing_with_capabilities(unrestrictedPolicy,frame_activity::minimized,capabilities);
        assert(background.effective_hz==30.0 && minimized.effective_hz==10.0);
    }
    const auto standalone=resolve_frame_pacing(select_frame_pacing_policy(uncapped,0.0,true,true),foreground,false);
    assert(standalone.effective_mode==uncapped && standalone.effective_hz==0.0);
    assert(!standalone.cpu_deadline_wait);
    // Compile Raylib's actual native-admission callback against the actual
    // policy and deadline. Epoch's custom frame-control path never waits in
    // SetTargetFPS, so fixed/background/minimized rates must reach the host.
    const auto raylibCapabilities=frame_pacing_capabilities_for(frame_pacing_backend::raylib);
    assert(!raylibCapabilities.vsync && !raylibCapabilities.target_hz);
    for(auto mode : {frame_pacing_mode::target_hz, frame_pacing_mode::vsync, uncapped}) {
        for(auto activity : {foreground, frame_activity::background, frame_activity::minimized}) {
            for(double selectedHz : {59.94, 120.0, 240.0, 999.0}) {
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
                actualHostLimiter.wait_for_next_frame();
                assert((epochengine::core::time::reads>0)==(expectedHz>0.0));
                if(expectedHz==0.0) assert(epochengine::core::time::sleeps==0);
            }
        }
    }
    assert(epochengine::raylib_api::configurations==36);
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
    for(double hz : {60.0,120.0}) {
        const auto fixed=resolve_frame_pacing(select_frame_pacing_policy(frame_pacing_mode::target_hz,hz,true,false),foreground,false);
        assert(fixed.effective_hz==hz && fixed.cpu_deadline_wait);
    }
    const auto nativeVsync=finalize_native_frame_pacing(resolve_frame_pacing(
        select_frame_pacing_policy(frame_pacing_mode::vsync,60.0,true,false),foreground,true),
        {true,true,frame_pacing_mode::vsync,0.0});
    assert(nativeVsync.effective_mode==frame_pacing_mode::vsync && nativeVsync.effective_hz==0.0);
    assert(!nativeVsync.cpu_deadline_wait);
    const auto defaultEditor=resolve_frame_pacing(select_frame_pacing_policy(uncapped,0.0,false,false),foreground,false);
    assert(defaultEditor.effective_hz==120.0 && defaultEditor.cpu_deadline_wait);
    for(double invalid : {-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        const auto safe=resolve_frame_pacing(select_frame_pacing_policy(frame_pacing_mode::target_hz,invalid,true,false),foreground,false);
        assert(safe.effective_hz==120.0 && safe.cpu_deadline_wait);
    }
    const double dt=1.0/120.0;
    assert(std::abs(next_frame_deadline(0,100,dt,false)-(100+dt))<1e-12);
    assert(next_frame_deadline(0,100,dt,true)==100); // no catch-up burst
    return 0;
}
'''
            toolset, kit = toolsets[-1], kits[-1]
            for edition, value in (('development', 0), ('public', 1)):
                include = scratch / edition
                include.mkdir()
                marker = read_owned_source(ROOT / 'Engine/include/epoch.edition.hpp', encoding='utf-8')
                (include / 'epoch.edition.hpp').write_text(marker.replace(
                    '#define EPOCH_SOURCE_EDITION_PUBLIC 0', '#define EPOCH_SOURCE_EDITION_PUBLIC ' + str(value)), encoding='utf-8')
                source = include / 'contract.cpp'
                source.write_text('#include "epoch.edition.hpp"\n#include <cstdint>\n#include <cmath>\n'
                    '#include <charconv>\n#include <system_error>\n#include <cassert>\n#include <initializer_list>\n#include <limits>\n'
                    '#include <algorithm>\n#include <atomic>\n#include <array>\n#include <memory>\n#include <optional>\n#include <thread>\n'
                    '#include <string_view>\n' + clock + api + '\nnamespace epochengine::perf {\n' + wait
                    + raylib_mock + raylib_callback + persistence
                    + '\n#define CHECK(...) do { if(!(__VA_ARGS__)) return __LINE__; } while(false)\n'
                    + main.replace('assert(', 'CHECK('),
                    encoding='utf-8')
                binary = include / 'contract.exe'
                args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/WX',
                    '/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'), '/I' + str(kit / 'shared'),
                    '/I' + str(kit / 'um'), '/Fo' + str(include / 'contract.obj'), '/Fe' + str(binary),
                    str(source), '/link', '/LIBPATH:' + str(toolset / 'lib/x64'),
                    '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'um/x64'),
                    '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'ucrt/x64')]
                result = subprocess.run(args, cwd=include, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout[-3500:] + result.stderr)
                result = subprocess.run([str(binary)], cwd=include, capture_output=True, text=True)
                lines = read_owned_source(source, encoding='utf-8').splitlines()
                failed = lines[result.returncode - 1] if 0 < result.returncode <= len(lines) else ''
                self.assertEqual(result.returncode, 0, edition + ' CPU policy/deadline: ' + failed)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent))
            shutil.rmtree(scratch)

    def test_actual_gl_admission_with_mocked_driver_for_both_editions(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        toolsets = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kitroot = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((kitroot / 'Include').glob('*'))
        if not toolsets or not kits:
            self.skipTest('MSVC/SDK unavailable')
        parent = (ROOT / 'build/contract-tests').resolve()
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='gpu-admission-', dir=parent))
        try:
            contract = r'''#include "epoch.edition.hpp"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#define Status int
#include "opengl.frame_admission.hpp"
#include <algorithm>
#include <cstdint>
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
            toolset, kit = toolsets[-1], kits[-1]
            for edition, value in (('development', 0), ('public', 1)):
                lane = scratch / edition
                lane.mkdir()
                marker = read_owned_source(ROOT / 'Engine/include/epoch.edition.hpp', encoding='utf-8')
                (lane / 'epoch.edition.hpp').write_text(marker.replace(
                    '#define EPOCH_SOURCE_EDITION_PUBLIC 0', '#define EPOCH_SOURCE_EDITION_PUBLIC ' + str(value)), encoding='utf-8')
                source = lane / 'contract.cpp'
                source.write_text(contract, encoding='utf-8')
                binary = lane / 'contract.exe'
                args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/WX',
                    '/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'), '/I' + str(kit / 'shared'),
                    '/I' + str(kit / 'um'), '/I' + str(ROOT / 'Engine/src/renderers/opengl'),
                    '/I' + str(ROOT / 'Engine/vcpkg_installed/x64-windows/include'),
                    '/Fo' + str(lane / 'contract.obj'), '/Fe' + str(binary), str(source),
                    '/link', '/LIBPATH:' + str(toolset / 'lib/x64'),
                    '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'um/x64'),
                    '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'ucrt/x64')]
                result = subprocess.run(args, cwd=lane, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout[-3500:] + result.stderr)
                result = subprocess.run([str(binary)], cwd=lane, capture_output=True, text=True)
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
        self.assertLess(source.index('guard.set(desired)'), source.index('state_for_platform_context('))
        self.assertLess(source.index('frame_admission::admit('), source.index('glViewport('))
        self.assertLess(source.index('glViewport('), source.index('opengl_clear();'))
        self.assertLess(source.index('opengl_clear();'), source.index('opengl_render_active_frame('))
        self.assertLess(source.index('opengl_render_active_frame('), source.index('PlatformGL::swap_buffers('))
        self.assertLess(source.index('PlatformGL::swap_buffers('), source.index('frame_admission::submitted('))
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
        self.assertIn('.selected = edit.manual ? "Manual" : vsync ? "VSync"', app)
        settings = read_owned_source(ROOT / 'Engine/src/editor/application/editor.frame_consent_settings.inl')
        self.assertNotIn('core::cli::frame_limit_explicit =', settings)
        self.assertNotIn('core::cli::frame_limit_fps =', settings)
        self.assertIn('editorFramePacingOverride.publish(', settings)
        self.assertIn('frame_limit_control("settings-project-frame-limit-select", editor.projectRunFrameLimitFps,', app)


if __name__ == '__main__':
    unittest.main()
