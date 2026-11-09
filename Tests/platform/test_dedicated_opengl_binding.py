"""Actual native context guard and dedicated-thread lifetime route; no GPU or window."""
from Tools.source.owned_source_text import read_owned_source
from pathlib import Path
import os
import json
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def definition(source, beginning):
    start = source.index(beginning)
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class DedicatedOpenGLBinding(unittest.TestCase):
    def test_actual_guard_and_host_admission_for_both_editions(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        toolsets = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kitroot = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((kitroot / 'Include').glob('*'))
        if not toolsets or not kits:
            self.skipTest('MSVC/SDK unavailable')
        platform = read_owned_source(ROOT / 'Engine/modules/opengl.platform.ixx', encoding='utf-8')
        api = platform[platform.index('    struct PlatformGLContext'):platform.index('    [[nodiscard]] inline bool swap_buffers')]
        guard = definition(platform, '    class ScopedContext') + ';'
        host = read_owned_source(ROOT / 'Engine/src/renderers/host/host.context_win.cpp', encoding='utf-8')
        host = host.split('void MultiContextManager::RenderLoop(WindowData& win)', 1)[1]
        start = host.index('        epochengine::openglcontext::PlatformGL::ScopedContext dedicatedOpenGLContext;')
        admission = host[start:host.index('\n#endif', start)]
        release = host[host.index('        dedicatedOpenGLContext.release();'):]
        release = release[:release.index('\n#endif')]
        retirement_start = host.index('        win.ownerThreadCommandQueue.clear();')
        retirement = host[retirement_start:host.index('\n#if defined(EPOCH_USING_OPENGL)', retirement_start)]
        fixture = r'''
#include "epoch.edition.hpp"
#define EPOCH_USING_OPENGL 1
#include <cstdint>
#include <source_location>
#include <initializer_list>
using HDC=void*; using HGLRC=void*;
constexpr int TRUE=1;
namespace mock {HDC dc{};HGLRC rc{};unsigned binds{},clears{};bool fail{};}
HDC wglGetCurrentDC() noexcept {return mock::dc;}
HGLRC wglGetCurrentContext() noexcept {return mock::rc;}
int wglMakeCurrent(HDC dc,HGLRC rc) noexcept {
    if(dc || rc) {++mock::binds;if(mock::fail)return 0;} else ++mock::clears;
    mock::dc=dc;mock::rc=rc;return TRUE;
}
namespace epochengine::openglcontext::PlatformGL {
'''
        fixture += api + guard + r'''
}
namespace epochengine::logger {
enum class LogLevel {Error};
struct Log {template<typename... Args> void log(Args&&...) {}};
inline Log& get(const char*) {static Log log;return log;}
}
enum class ContextType {OpenGL,RayLib,Vulkan};
enum class BackendLifecycleState {initializing,failed};
unsigned drains{}, waits{},queueClears{};bool drain_ok{},wait_ok{},clear_ok=true;
HDC target_dc{};HGLRC target_rc{};
struct Queue {
    void clear() {++queueClears;clear_ok &= mock::dc==target_dc && mock::rc==target_rc;}
    void drain() {++drains;drain_ok=mock::dc==target_dc && mock::rc==target_rc;}
};
struct Window {
    Queue ownerThreadCommandQueue,commandQueue;
    bool get_should_close() const {return false;}
    bool running=true;BackendLifecycleState lifecycle=BackendLifecycleState::initializing;
    void set_backend_lifecycle(BackendLifecycleState state) {lifecycle=state;}
};
struct Context {
    ContextType type=ContextType::OpenGL;
    void* native_drawable{};void* native_gl_context{};
    bool cleanup=true;unsigned cleaned{};bool cleanup_current_ok{};
    HDC expected_dc{};HGLRC expected_rc{};
    void cleanup_safe() {++cleaned;cleanup_current_ok=mock::dc==expected_dc && mock::rc==expected_rc;}
};
constexpr const char* kLogSys="host";
unsigned callbacks{};bool callback_ok{},published{},throw_callback{},break_frame{},cancel_frame{},fail_drain{};unsigned turnLimit=3;
void frames(Context* ctx) {
    ++callbacks;callback_ok=true;
    if(ctx->type==ContextType::OpenGL) {
        using namespace epochengine::openglcontext::PlatformGL;
        const PlatformGLContext target{ctx->native_drawable,ctx->native_gl_context};
        const auto initial_binds=mock::binds,initial_clears=mock::clears;
        for(unsigned i=0;i<1000;++i) {
            // Owner commands, pacing configuration and frame work see the context;
            // actual per-frame/nested resource guards need no native switch.
            callback_ok &= get_current()==target;
            ScopedContext nested;
            callback_ok &= nested.set(target) && !nested.switched();
        }
        callback_ok &= initial_binds==mock::binds && initial_clears==mock::clears;
    }
    if(throw_callback) throw 7;
}
void route(Context* ctx,Window& win) {
    const auto publishLifecycle=[](){published=true;};
    target_dc=ctx->type==ContextType::OpenGL?ctx->native_drawable:mock::dc;
    target_rc=ctx->type==ContextType::OpenGL?ctx->native_gl_context:mock::rc;
'''
        fixture += admission + r'''
    for(unsigned turn=0;turn<turnLimit;++turn) {
    frames(ctx);
    if(cancel_frame) {win.running=false;break;}
    if(break_frame) break;
        ++waits;wait_ok &= mock::dc==target_dc && mock::rc==target_rc;
        if(fail_drain && turn==2) mock::fail=true;
    }
''' + retirement + release + r'''
    if(ctx->cleanup) ctx->cleanup_safe();
}
void reset() {
    mock::dc=nullptr;mock::rc=nullptr;mock::binds=mock::clears=0;mock::fail=false;
    callbacks=drains=waits=queueClears=0;turnLimit=3;callback_ok=published=throw_callback=break_frame=cancel_frame=fail_drain=drain_ok=false;wait_ok=clear_ok=true;
}
#define CHECK(...) do { if(!(__VA_ARGS__)) return __LINE__; } while(false)
int main() {
    int dcA{},rcA{},dcB{},rcB{};
    for(bool previous : {false,true}) {
        reset();
        if(previous) {mock::dc=&dcB;mock::rc=&rcB;}
        Context ctx;Window win;
        ctx.native_drawable=&dcA;ctx.native_gl_context=&rcA;
        ctx.expected_dc=mock::dc;ctx.expected_rc=mock::rc;
        route(&ctx,win);
        CHECK(callbacks==3 && callback_ok && ctx.cleaned==1 && ctx.cleanup_current_ok);
        CHECK(waits==3 && wait_ok && drains==1 && drain_ok);
        CHECK(mock::binds==(previous?2u:1u) && mock::clears==(previous?0u:1u) && queueClears==1 && clear_ok);
        CHECK(win.running && !published);
        CHECK(mock::dc==ctx.expected_dc && mock::rc==ctx.expected_rc);
    }
    for(ContextType type : {ContextType::RayLib,ContextType::Vulkan}) {
        reset();Context ctx;Window win;ctx.type=type;
        route(&ctx,win);
        CHECK(callbacks==3 && ctx.cleaned==1 && ctx.cleanup_current_ok);
        CHECK(waits==3 && wait_ok && drains==1 && drain_ok);
        CHECK(mock::binds==0 && mock::clears==0 && queueClears==1 && clear_ok);
    }
    for(bool missing : {false,true}) {
        reset();Context ctx;Window win;
        ctx.native_drawable=&dcA;ctx.native_gl_context=missing?nullptr:&rcA;
        mock::fail=!missing;
        route(&ctx,win);
        CHECK(callbacks==0 && ctx.cleaned==1 && ctx.cleanup_current_ok);
        CHECK(!win.running && published && win.lifecycle==BackendLifecycleState::failed);
        CHECK(mock::binds==(missing?0u:1u) && mock::clears==0);
    }
    reset();
    {Context ctx;Window win;ctx.native_drawable=&dcA;ctx.native_gl_context=&rcA;
     break_frame=true;route(&ctx,win);
     CHECK(callbacks==1 && waits==0 && drains==1 && drain_ok);
     CHECK(mock::dc==nullptr && mock::rc==nullptr && mock::binds==1 && mock::clears==1 && queueClears==1 && clear_ok);}
    reset();
    {Context ctx;Window win;ctx.native_drawable=&dcA;ctx.native_gl_context=&rcA;
     cancel_frame=true;route(&ctx,win);
     CHECK(callbacks==1 && waits==0 && drains==0 && ctx.cleaned==1);
     CHECK(mock::dc==nullptr && mock::rc==nullptr && mock::binds==1 && mock::clears==1 && queueClears==2 && clear_ok);}
    reset();
    {Context ctx;Window win;ctx.native_drawable=&dcA;ctx.native_gl_context=&rcA;
     fail_drain=true;route(&ctx,win);
     CHECK(waits==3 && wait_ok && drains==1 && drain_ok && ctx.cleaned==1 && ctx.cleanup_current_ok);
     CHECK(win.running && !published);
     CHECK(mock::dc==nullptr && mock::rc==nullptr && mock::binds==1 && mock::clears==1 && queueClears==1 && clear_ok);}
    // A stop immediately after admission still retires queued work while bound.
    reset();
    {Context ctx;Window win;ctx.native_drawable=&dcA;ctx.native_gl_context=&rcA;
     turnLimit=0;route(&ctx,win);
     CHECK(callbacks==0 && waits==0 && drains==1 && drain_ok && clear_ok && queueClears==1);
     CHECK(mock::binds==1 && mock::clears==1 && ctx.cleaned==1 && ctx.cleanup_current_ok);}
    reset();mock::dc=&dcB;mock::rc=&rcB;
    Context ctx;Window win;ctx.native_drawable=&dcA;ctx.native_gl_context=&rcA;
    throw_callback=true;
    try {route(&ctx,win);CHECK(false);} catch(int error) {CHECK(error==7);}
    CHECK(mock::dc==&dcB && mock::rc==&rcB && mock::binds==2 && mock::clears==0);
    CHECK(ctx.cleaned==0); // Existing exception cleanup policy is not expanded.
    return 0;
}
'''
        parent = (ROOT / 'Artifacts/Reports/opengl-lifetime-binding-restore-oct30/cpu').resolve()
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='dedicated-gl-binding-', dir=parent))
        records = []
        try:
            toolset, kit = toolsets[-1], kits[-1]
            for edition, value in (('development', 0), ('public', 1)):
                for configuration, flags in (('Debug', ('/Od', '/MDd', '/D_DEBUG')), ('Release', ('/O2', '/MD'))):
                    lane = scratch / edition / configuration
                    lane.mkdir(parents=True)
                    marker = read_owned_source(ROOT / 'Engine/include/epoch.edition.hpp', encoding='utf-8')
                    (lane / 'epoch.edition.hpp').write_text(marker.replace(
                        '#define EPOCH_SOURCE_EDITION_PUBLIC 0', '#define EPOCH_SOURCE_EDITION_PUBLIC ' + str(value)), encoding='utf-8')
                    source = lane / 'contract.cpp'
                    source.write_text(fixture, encoding='utf-8')
                    binary = lane / 'contract.exe'
                    args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/WX', '/UNDEBUG', *flags,
                        '/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'), '/I' + str(kit / 'shared'), '/I' + str(kit / 'um'),
                        '/Fo' + str(lane / 'contract.obj'), '/Fe' + str(binary), str(source), '/link',
                        '/LIBPATH:' + str(toolset / 'lib/x64'), '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'um/x64'),
                        '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'ucrt/x64')]
                    result = subprocess.run(args, cwd=lane, capture_output=True, text=True)
                    (parent / (edition + '-' + configuration + '-compile.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
                    compile_exit = result.returncode
                    self.assertEqual(result.returncode, 0, result.stdout[-3500:] + result.stderr)
                    result = subprocess.run([str(binary)], cwd=lane, capture_output=True, text=True)
                    lines = fixture.splitlines()
                    failure = lines[result.returncode - 1] if 0 < result.returncode <= len(lines) else ''
                    records.append({'edition': edition, 'configuration': configuration, 'compile_exit': compile_exit, 'run_exit': result.returncode, 'command': args})
                    self.assertEqual(result.returncode, 0, edition + '/' + configuration + ': ' + failure)
        finally:
            (parent / 'result.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
            self.assertTrue(scratch.resolve().is_relative_to(parent))
            shutil.rmtree(scratch)

    def test_host_scope_keeps_order_and_linux_existing_binding(self):
        source = read_owned_source(ROOT / 'Engine/src/renderers/host/host.context_win.cpp', encoding='utf-8')
        loop = source.split('void MultiContextManager::RenderLoop(WindowData& win)', 1)[1].split('void MultiContextManager::HandleDropFiles', 1)[0]
        admission = loop.index('ScopedContext dedicatedOpenGLContext;')
        self.assertLess(loop.index('if (ctx->init_failed)'), admission)
        self.assertLess(loop.index('if (!ctx->process)'), admission)
        self.assertLess(admission, loop.index('while (running.load'))
        self.assertLess(admission, loop.index('win.ownerThreadCommandQueue.drain();'))
        self.assertLess(admission, loop.index('configureFramePacing();'))
        self.assertLess(admission, loop.index('ctx->process_safe('))
        release = loop.index('dedicatedOpenGLContext.release();')
        self.assertLess(loop.index('ctx->process_safe('), release)
        self.assertLess(loop.index('coreFrameLimiter.wait_for_next_frame();'), release)
        self.assertNotIn('ScopedContext drainOpenGLContext;', loop)
        self.assertNotIn('ScopedContext frameOpenGLContext;', loop)
        self.assertEqual(loop.count('ScopedContext dedicatedOpenGLContext;'), 1)
        self.assertLess(loop.index('win.ownerThreadCommandQueue.clear();'), loop.rindex('win.commandQueue.drain();'))
        self.assertLess(loop.rindex('win.commandQueue.drain();'), release)
        self.assertLess(loop.rindex('win.commandQueue.clear();'), release)
        self.assertLess(release, loop.rindex('ctx->cleanup_safe();'))
        self.assertNotIn('backend.glState', loop[admission:loop.index('win.ownerThreadCommandQueue.drain();', admission)])
        linux = read_owned_source(ROOT / 'Engine/src/renderers/host/host.context_linux.cpp', encoding='utf-8')
        loop = linux.split('void MultiContextManager::RenderLoop(WindowData& win)', 1)[1].split('namespace epochengine::platform', 1)[0]
        self.assertLess(loop.index('glXMakeCurrent(localDisplay, xwin, glxCtx);'), loop.index('ctx->initialize_safe();'))
        self.assertIn('glXMakeCurrent(display, 0, nullptr);', loop)
        direct = read_owned_source(ROOT / 'Engine/src/renderers/opengl/opengl.context_process_impl.cpp', encoding='utf-8')
        self.assertIn('PlatformGL::ScopedContext guard;', direct)
        self.assertIn('guard.set(desired)', direct)


if __name__ == '__main__':
    unittest.main()
