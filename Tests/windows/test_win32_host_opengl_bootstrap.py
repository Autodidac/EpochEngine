"""Exercise the Windows host's shared-context startup with recording WGL calls."""
from pathlib import Path
import json
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run


ROOT = Path(__file__).resolve().parents[2]
HOST = ROOT / "Engine/src/renderers/host/host.context_win.cpp"


class WindowsHostOpenGLBootstrap(unittest.TestCase):
    def test_only_opengl_creates_and_reuses_the_share_root(self):
        host = HOST.read_text(encoding="utf-8-sig")
        ensure = definition(host, "    bool MultiContextManager::EnsureSharedGLContext(")
        startup = definition(host, "        if (OpenGLWinCount > 0 && !EnsureSharedGLContext(hInst))")
        begin = host.index("        if (request.type == ContextType::OpenGL && !EnsureSharedGLContext(")
        detached = host[begin:host.index("#endif", begin)]
        begin = host.index("        const bool ownsDC = !hdc;")
        add = host[begin:host.index("        if (parentWnd", begin)]
        replacement = definition(host, "    bool MultiContextManager::OpenReplacementContextWindow(")
        self.assertIn("CreateDetachedContextWindowOnOwnerThread(dockedRequest, createdContext)", replacement)
        shared = definition(host, "        HGLRC MultiContextManager::CreateSharedGLContext(")
        self.assertIn("createAttribs(hdc, sharedContext, attribs46)", shared)
        self.assertIn("wglShareLists(sharedContext, finalCtx)", shared)
        self.assertEqual(host.count('L"Dummy", WS_POPUP'), 1)

        source = r'''
#include <cstdlib>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#define EPOCH_USING_OPENGL 1
#define EPOCH_USING_VULKAN 1
#define CHECK(...) do { if (!(__VA_ARGS__)) return __LINE__; } while (false)
using HWND = void*;
using HDC = void*;
using HGLRC = void*;
using HINSTANCE = void*;
constexpr int WS_EX_TOOLWINDOW = 1;
constexpr int WS_POPUP = 2;
enum class ContextType { Software, Vulkan, DirectX, SDL, SFML, RayLib, OpenGL };
enum class Failure { none, window, dc, create, activate, load };
Failure failure{};
unsigned windows{}, destroyed{}, acquired{}, released{}, created{}, deleted{}, loaded{};
int windowToken{}, dcToken{}, instanceToken{}, contextTokens[16];
HWND CreateWindowExW(int, const wchar_t*, const wchar_t*, int, int, int, int, int,
    HWND, void*, HINSTANCE, void*) { ++windows; return failure == Failure::window ? nullptr : &windowToken; }
HINSTANCE GetModuleHandleW(const wchar_t*) { return &instanceToken; }
HDC GetDC(HWND) { ++acquired; return failure == Failure::dc ? nullptr : &dcToken; }
int ReleaseDC(HWND, HDC) { return ++released; }
int DestroyWindow(HWND) { return ++destroyed; }
int wglMakeCurrent(HDC, HGLRC context) { return !context || failure != Failure::activate; }
int wglDeleteContext(HGLRC) { return ++deleted; }
int gladLoadGL() { ++loaded; return failure != Failure::load; }
constexpr const char* kLogSys = "fixture";
namespace logger {
enum class LogLevel { Error };
struct Sink { void log(LogLevel, const char*, std::source_location) {} };
Sink get(const char*) { return {}; }
}
namespace epochengine { namespace logger = ::logger; }

struct MultiContextManager {
    HGLRC sharedContext{}, sharedWith{};
    bool EnsureSharedGLContext(HINSTANCE) noexcept;
    HGLRC CreateSharedGLContext(HDC) {
        if (failure == Failure::create) throw std::runtime_error("creation failed");
        sharedWith = sharedContext;
        return &contextTokens[created++];
    }
    bool startup(int OpenGLWinCount) {
        const auto hInst = GetModuleHandleW(nullptr);
        STARTUP
        return true;
    }
    bool detach(ContextType type) {
        const struct { ContextType type; } request{type};
        DETACHED
        if (type == ContextType::OpenGL) (void)CreateSharedGLContext(&dcToken);
        return true;
    }
    void add(ContextType type, HDC hdc = nullptr, HGLRC glContext = nullptr) {
        HWND hwnd = &windowToken;
        bool usesSharedContext{};
        ADD_WINDOW
        if (type == ContextType::OpenGL && !glContext) std::abort();
        (void)usesSharedContext;
    }
};
ENSURE

int main() {
    MultiContextManager software;
    CHECK(software.startup(0));
    for (const auto type : {ContextType::Software, ContextType::Vulkan, ContextType::DirectX,
                          ContextType::SDL, ContextType::SFML, ContextType::RayLib}) {
        CHECK(software.detach(type));
        software.add(type, &dcToken);
    }
    CHECK(windows == 0 && created == 0 && loaded == 0 && !software.sharedContext);

    failure = Failure::window;
    CHECK(!software.detach(ContextType::OpenGL) && destroyed == 0 && acquired == 0);
    failure = Failure::dc;
    CHECK(!software.detach(ContextType::OpenGL) && destroyed == 1 && released == 0);
    failure = Failure::create;
    CHECK(!software.detach(ContextType::OpenGL) && destroyed == 2 && released == 1);
    failure = Failure::activate;
    CHECK(!software.detach(ContextType::OpenGL) && destroyed == 3 && released == 2 && deleted == 1);
    failure = Failure::load;
    CHECK(!software.detach(ContextType::OpenGL) && destroyed == 4 && released == 3 && deleted == 2);
    CHECK(!software.sharedContext);

    failure = Failure::none;
    CHECK(software.detach(ContextType::OpenGL));
    const auto shareRoot = software.sharedContext;
    CHECK(shareRoot && software.sharedWith == shareRoot);
    CHECK(software.detach(ContextType::OpenGL));
    software.add(ContextType::OpenGL, &dcToken);
    CHECK(software.sharedContext == shareRoot && software.sharedWith == shareRoot);
    CHECK(windows == 6 && destroyed == 5 && released == 4 && loaded == 2);

    MultiContextManager opengl;
    CHECK(opengl.startup(2) && opengl.sharedContext && opengl.sharedContext != shareRoot);
    CHECK(opengl.detach(ContextType::OpenGL) && opengl.sharedWith == opengl.sharedContext);
    CHECK(windows == 7 && destroyed == 6 && loaded == 2);
}
'''.replace("STARTUP", startup).replace("DETACHED", detached).replace("ADD_WINDOW", add).replace("ENSURE", ensure)
        report = ROOT / "Artifacts/Reports/windows-host-opengl-bootstrap" / str(time.time_ns())
        report.mkdir(parents=True)
        consumer = report / "consumer.cpp"
        consumer.write_text(source, encoding="utf-8")
        runs = compile_and_run(self, consumer, report)
        (report / "result.json").write_text(json.dumps({"scope": "Production Windows host branches with recording WGL calls", "runs": runs}, indent=2), encoding="utf-8")
