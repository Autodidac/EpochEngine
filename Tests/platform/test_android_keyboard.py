"""Exercise Android SDL's authored game key state without opening a device."""

from pathlib import Path
import re
import time
import unittest

from Tests.platform.test_child_close_router import definition
from Tests.support.cpp import compile_and_run


ROOT = Path(__file__).resolve().parents[2]


class AndroidKeyboard(unittest.TestCase):
    def test_key_edges_release_and_window_ownership(self):
        # The actual Android SDL scancodes are portable. No Android binary or
        # SDL library is loaded by this host fixture.
        includes = (ROOT / 'Artifacts/Dependencies/android-tools/vcpkg-installed/'
                    'arm64-android-epoch/include')
        if not (includes / 'SDL3/SDL_events.h').is_file():
            self.skipTest('The checkout-local Android SDL headers are required')
        owner = (ROOT / 'Engine/src/renderers/sdl/core.context_sdl.cpp').read_text(encoding='utf-8')
        input_source = (ROOT / 'Engine/modules/input/input.engine.ixx').read_text(encoding='utf-8')
        sdl = (ROOT / 'Engine/modules/sdl/sdl.state.ixx').read_text(encoding='utf-8')
        key = re.search(r'enum Key : std::uint16_t\s*\{[^}]+\};', input_source).group()
        keyboard = re.search(r'struct KeyboardState\s*\{[^}]+\} keyboard\{\};', sdl).group()
        functions = '\n'.join(definition(owner, marker) for marker in (
            '    [[nodiscard]] constexpr SDL_Scancode android_key_scancode(',
            '    void reset_android_keyboard(',
            '    void observe_android_keyboard(',
            '    [[nodiscard]] bool android_key_state(',
        ))
        report = ROOT / 'Artifacts/Reports/android-keyboard' / str(time.time_ns())
        report.mkdir(parents=True)
        test = report / 'keyboard.cpp'
        test.write_text(r'''
#include <SDL3/SDL_events.h>
#include <array>
#include <bitset>
#include <cstdint>
#include <mutex>
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)
namespace epochengine::input {
''' + key + r'''
}
namespace epochengine::sdlcontext::state {
struct State {
''' + keyboard + r'''
} engine_state;
auto& get_sdl_state() { return engine_state; }
std::recursive_mutex& runtime_api_mutex() { static std::recursive_mutex mutex; return mutex; }
}
constexpr SDL_WindowID s_windowId = 9;
''' + functions + r'''
int main() {
    using epochengine::input::Key;
    using epochengine::sdlcontext::state::get_sdl_state;
    static_assert(android_key_scancode(Key::W) == SDL_SCANCODE_W);
    static_assert(android_key_scancode(Key::LeftShift) == SDL_SCANCODE_LSHIFT);
    static_assert(android_key_scancode(Key::KPEnter) == SDL_SCANCODE_KP_ENTER);
    static_assert(android_key_scancode(Key::Enter) == SDL_SCANCODE_RETURN);
    static_assert(android_key_scancode(Key::Count) == SDL_SCANCODE_UNKNOWN);
    SDL_KeyboardEvent key{};
    key.windowID = s_windowId;
    key.scancode = SDL_SCANCODE_W;
    std::scoped_lock lock{epochengine::sdlcontext::state::runtime_api_mutex()};
    observe_android_keyboard(key, true);
    CHECK(android_key_state(Key::W, false) && android_key_state(Key::W, true));
    CHECK(!android_key_state(Key::A, false) && !android_key_state(Key::Unknown, true));
    get_sdl_state().keyboard.pressed.reset();
    key.repeat = true;
    observe_android_keyboard(key, true);
    CHECK(android_key_state(Key::W, false) && !android_key_state(Key::W, true));
    key.windowID = s_windowId + 1;
    observe_android_keyboard(key, false);
    CHECK(android_key_state(Key::W, false));
    key.windowID = s_windowId;
    observe_android_keyboard(key, false);
    CHECK(!android_key_state(Key::W, false));
    observe_android_keyboard(key, true);
    CHECK(!android_key_state(Key::W, false));
    key.repeat = false;
    observe_android_keyboard(key, true);
    reset_android_keyboard();
    CHECK(!android_key_state(Key::W, false) && !android_key_state(Key::W, true));
    key.scancode = SDL_SCANCODE_UNKNOWN;
    observe_android_keyboard(key, true);
    key.scancode = SDL_SCANCODE_COUNT;
    observe_android_keyboard(key, true);
    CHECK(get_sdl_state().keyboard.down.none());
    return 0;
}
''', encoding='utf-8')
        compile_and_run(self, test, report, includes=(includes,))
