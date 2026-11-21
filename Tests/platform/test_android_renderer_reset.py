"""Exercise the actual Android renderer reset owner with mocked SDL handles."""

from hashlib import sha256
from pathlib import Path
import unittest

from Tests.platform.test_child_close_router import definition
from Tests.support.cpp import compile_and_run


ROOT = Path(__file__).resolve().parents[2]


class AndroidRendererReset(unittest.TestCase):
    def test_reset_recreates_the_owned_renderer_and_retires_stale_resources(self):
        adapter = (ROOT / "Engine/src/renderers/sdl/core.context_sdl.cpp").read_text(encoding="utf-8")
        markers = (
            "    [[nodiscard]] bool admit_android_renderer_context(",
            "    void release_android_render_targets(",
            "    [[nodiscard]] bool recreate_android_renderer(",
            "    [[nodiscard]] bool handle_android_render_event(",
        )
        owners = "\n".join(definition(adapter, marker) for marker in markers)
        fixture = r'''
#include <atomic>
#include <source_location>
#include <string_view>
#include <utility>
#include <vector>

#define CHECK(value) do { if (!(value)) return __LINE__; } while (false)
struct SDL_Window {} window, other;
struct SDL_Renderer {} original, fresh;
using SDL_GLContext = void*;
int lostContext, provisionalContext, freshContext;
constexpr int GL_VERSION = 1;
enum {SDL_EVENT_OTHER, SDL_EVENT_RENDER_DEVICE_LOST, SDL_EVENT_RENDER_DEVICE_RESET, SDL_EVENT_RENDER_TARGETS_RESET};
struct SDL_Event { int type; struct {unsigned windowID;} render; };
constexpr unsigned s_windowId = 11;
SDL_Window* s_window = &window;
SDL_Renderer* s_renderer = &original;
SDL_GLContext s_androidRendererContext = &lostContext;
SDL_GLContext activeGlContext = &provisionalContext;
SDL_Window* currentWindow = &window;
bool s_ownsRenderer = true;
int s_logicalPresentationWidth = 480, s_logicalPresentationHeight = 720;
bool allowUnbind = true, canCreate = true, canBind = true, canRestoreVSync = true, clouds = true;
SDL_Window* createdRendererWindow = &window;
const char* admittedGlVersion = "OpenGL ES 3.2";
enum Step {unbind, read_vsync, touch, scene, canvas, arcade, unregister_atlas, clear_atlas,
           destroy_old, destroy_provisional, create_renderer, write_vsync, initialize, refresh_gui, register_atlas};
std::vector<Step> steps;
bool admittedHandles = true;
int failures{}, warnings{};

auto SDL_GL_GetCurrentContext() { return activeGlContext; }
auto SDL_GL_GetCurrentWindow() { return currentWindow; }
bool SDL_GL_MakeCurrent(SDL_Window* requestedWindow, SDL_GLContext requestedContext) {
    steps.push_back(unbind);
    admittedHandles &= requestedWindow == nullptr && requestedContext == nullptr;
    if (!allowUnbind) return false;
    activeGlContext = requestedContext;
    currentWindow = requestedWindow;
    return true;
}
bool SDL_GetRenderVSync(SDL_Renderer* renderer, int* vsync) {
    steps.push_back(read_vsync);
    admittedHandles &= renderer == &original;
    *vsync = 1;
    return true;
}
bool SDL_SetRenderVSync(SDL_Renderer* renderer, int vsync) {
    steps.push_back(write_vsync);
    admittedHandles &= renderer == &fresh && vsync == 1;
    return canRestoreVSync;
}
void SDL_DestroyRenderer(SDL_Renderer* renderer) {
    steps.push_back(destroy_old);
    admittedHandles &= renderer == &original && activeGlContext != &provisionalContext;
    activeGlContext = nullptr;
}
bool SDL_GL_DestroyContext(SDL_GLContext context) {
    steps.push_back(destroy_provisional);
    admittedHandles &= context == &provisionalContext && activeGlContext == nullptr;
    return true;
}
SDL_Renderer* SDL_CreateRenderer(SDL_Window* target, std::string_view name) {
    steps.push_back(create_renderer);
    admittedHandles &= target == &window && name == "opengles2";
    if (!canCreate) return nullptr;
    activeGlContext = canBind ? &freshContext : nullptr;
    currentWindow = createdRendererWindow;
    return &fresh;
}
const char* glGetString(int) { return admittedGlVersion; }
const char* SDL_GetCurrentVideoDriver() { return "android"; }
const char* SDL_GetRendererName(SDL_Renderer*) { return "opengles2"; }
int SDL_GetGlobalProperties() { return 1; }
bool SDL_GetBooleanProperty(int, std::string_view name, bool fallback) {
    admittedHandles &= name == "epoch.android.density_clouds" && !fallback;
    return clouds;
}

namespace epochengine {
namespace core {
enum class ContextType { SDL };
enum class BackendLifecycleState { ready, initializing, failed };
struct Window {
    std::atomic_bool firstPresentComplete{true};
    BackendLifecycleState lifecycle{BackendLifecycleState::ready};
    void set_backend_lifecycle(BackendLifecycleState state) { lifecycle = state; }
};
struct Context { Window* windowData; std::atomic_bool preview_density_clouds{}; };
}
namespace atlasmanager {
void unregister_backend_uploader(core::ContextType) { steps.push_back(unregister_atlas); }
}
namespace sdltextures {
SDL_Renderer* sdl_renderer = &original;
void clear_gpu_atlases() { steps.push_back(clear_atlas); }
}
namespace sdlcontext {
struct { SDL_Renderer* renderer = &original; } sdl_renderer;
void init_renderer(SDL_Renderer* renderer) {
    steps.push_back(initialize);
    admittedHandles &= renderer == &fresh;
    sdl_renderer.renderer = renderer;
}
namespace state {
struct State {
    bool renderFaulted{};
    struct { unsigned resets{}; void reset() { ++resets; } } surfaceLifecycle;
};
State shared;
State& get_sdl_state() { return shared; }
}
}
namespace gui {
void refresh_context_resources(core::Context*) {
    admittedHandles &= sdltextures::sdl_renderer == &fresh && sdlcontext::sdl_renderer.renderer == &fresh;
    steps.push_back(refresh_gui);
}
}
namespace logger {
void error(const char*, const char*) { ++failures; }
void warn(const char*, const char*) { ++warnings; }
struct Log { template<class... Args> void logf(Args&&...) {} };
Log get(const char*) { return {}; }
enum class LogLevel { INFO };
}
}

void cancel_android_touch(epochengine::core::Context*) { steps.push_back(touch); }
bool keyboardHeld = true;
void reset_android_keyboard() { keyboardHeld = false; }
namespace android_scene { void release() { steps.push_back(scene); } }
void release_canvas2d_presenter() { steps.push_back(canvas); }
void destroy_arcade_screen_preview_target() { steps.push_back(arcade); }
void register_sdl_atlas_uploader() { steps.push_back(register_atlas); }

ACTUAL_OWNERS

int main() {
    using namespace epochengine;
    core::Window windowData;
    core::Context ctx{&windowData};
    auto& state = sdlcontext::state::get_sdl_state();
    const auto send = [&](int event, unsigned id = s_windowId) { return handle_android_render_event({event, {id}}, ctx); };
    const auto restartFixture = [&] {
        s_renderer = sdltextures::sdl_renderer = sdlcontext::sdl_renderer.renderer = &original;
        s_ownsRenderer = true;
        s_androidRendererContext = &lostContext;
        activeGlContext = &provisionalContext;
        currentWindow = &window;
        windowData.firstPresentComplete = true;
        windowData.lifecycle = core::BackendLifecycleState::ready;
        s_logicalPresentationWidth = 480;
        s_logicalPresentationHeight = 720;
        state = {};
        keyboardHeld = true;
        steps.clear();
        failures = 0;
        warnings = 0;
    };

    CHECK(send(SDL_EVENT_RENDER_DEVICE_RESET, 99) && send(SDL_EVENT_OTHER));
    CHECK(steps.empty() && keyboardHeld && windowData.firstPresentComplete && state.surfaceLifecycle.resets == 0);
    CHECK(windowData.lifecycle == core::BackendLifecycleState::ready);
    CHECK(send(SDL_EVENT_RENDER_TARGETS_RESET));
    CHECK((steps == std::vector<Step>{touch, scene, canvas, arcade}));
    CHECK(s_renderer == &original && !keyboardHeld && !windowData.firstPresentComplete && state.surfaceLifecycle.resets == 1);
    CHECK(windowData.lifecycle == core::BackendLifecycleState::initializing);

    restartFixture();
    CHECK(send(SDL_EVENT_RENDER_DEVICE_RESET));
    CHECK((steps == std::vector<Step>{unbind, read_vsync, touch, scene, canvas, arcade, unregister_atlas, clear_atlas,
                                    destroy_old, destroy_provisional, create_renderer, write_vsync, initialize, refresh_gui, register_atlas}));
    CHECK(admittedHandles && !keyboardHeld && !state.renderFaulted && failures == 0);
    CHECK(s_renderer == &fresh && s_ownsRenderer && s_androidRendererContext == &freshContext);
    CHECK(!windowData.firstPresentComplete && state.surfaceLifecycle.resets == 1 && ctx.preview_density_clouds);
    CHECK(windowData.lifecycle == core::BackendLifecycleState::initializing);
    CHECK(s_logicalPresentationWidth == 0 && s_logicalPresentationHeight == 0);

    restartFixture();
    activeGlContext = &lostContext;
    admittedGlVersion = "OpenGL ES 2.0";
    CHECK(send(SDL_EVENT_RENDER_DEVICE_RESET));
    CHECK(steps.size() == 13 && steps.front() == read_vsync && steps[8] == create_renderer);
    CHECK(!ctx.preview_density_clouds && admittedHandles);

    restartFixture();
    CHECK(!send(SDL_EVENT_RENDER_DEVICE_LOST));
    CHECK(steps.empty() && failures == 1 && state.renderFaulted && !windowData.firstPresentComplete);
    CHECK(windowData.lifecycle == core::BackendLifecycleState::failed);

    restartFixture();
    currentWindow = &other;
    CHECK(!send(SDL_EVENT_RENDER_DEVICE_RESET));
    CHECK(steps.empty() && s_renderer == &original && state.renderFaulted && failures == 1);

    restartFixture();
    allowUnbind = false;
    CHECK(!send(SDL_EVENT_RENDER_DEVICE_RESET) && steps.size() == 1 && steps.front() == unbind);
    CHECK(s_renderer == &original && state.renderFaulted && failures == 1);

    restartFixture();
    allowUnbind = true;
    canCreate = false;
    CHECK(!send(SDL_EVENT_RENDER_DEVICE_RESET));
    CHECK(!s_renderer && !s_ownsRenderer && !s_androidRendererContext && !windowData.firstPresentComplete);
    CHECK(!sdltextures::sdl_renderer && !sdlcontext::sdl_renderer.renderer);
    CHECK(admittedHandles && steps.back() == create_renderer && state.renderFaulted && failures == 1);

    restartFixture();
    canCreate = true;
    canBind = false;
    CHECK(!send(SDL_EVENT_RENDER_DEVICE_RESET));
    CHECK(s_renderer == &fresh && s_ownsRenderer && !s_androidRendererContext);
    CHECK(!sdltextures::sdl_renderer && !sdlcontext::sdl_renderer.renderer);
    CHECK(steps.back() == create_renderer && windowData.lifecycle == core::BackendLifecycleState::failed);

    restartFixture();
    canBind = true;
    createdRendererWindow = &other;
    CHECK(!send(SDL_EVENT_RENDER_DEVICE_RESET));
    CHECK(steps.back() == create_renderer && !s_androidRendererContext && !ctx.preview_density_clouds);

    restartFixture();
    createdRendererWindow = &window;
    canRestoreVSync = false;
    CHECK(send(SDL_EVENT_RENDER_DEVICE_RESET));
    CHECK(warnings == 1 && !windowData.firstPresentComplete);
    return 0;
}
'''.replace("ACTUAL_OWNERS", owners)
        report = ROOT / "Artifacts/Reports/android-renderer-reset" / sha256(fixture.encode()).hexdigest()[:16]
        report.mkdir(parents=True, exist_ok=True)
        source = report / "android_reset_consumer.cpp"
        source.write_text(fixture, encoding="utf-8")
        compile_and_run(self, source, report, windows_flags=("/W4", "/WX"), linux_flags=("-Wall", "-Wextra", "-Werror"))
