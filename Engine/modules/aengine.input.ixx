module;

#include <wtypes.h>

// ainput.ixx
export module aengine.input;

// ------------------------------------------------------------
// Engine / framework modules (no includes)
// ------------------------------------------------------------
//import aengine.platform;

import aengine.context.window;
//import aengine.context.state;

// Platform framework lives behind aplatform / aframework
#if defined(_WIN32)
import aframework;
#elif defined(__APPLE__)
import <ApplicationServices/ApplicationServices.h>;
#elif defined(__linux__)
import <X11/Xlib.h>;
import <X11/Xutil.h>;
import <X11/keysym.h>;
import <X11/extensions/XInput2.h>;
#endif

// ------------------------------------------------------------
// Standard library imports
// ------------------------------------------------------------
import <atomic>;
import <array>;
import <bitset>;
import <cstdint>;
import <iostream>;
import <mutex>;
import <shared_mutex>;
import <thread>;

// ============================================================
// Input core
// ============================================================
export namespace almondnamespace::input
{
    // --------------------------------------------------------
    // Key / Mouse enums
    // --------------------------------------------------------
    enum Key : std::uint16_t
    {
        Unknown = 0,
        A, B, C, D, E, F, G, H, I, J,
        K, L, M, N, O, P, Q, R, S, T,
        U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        Space, Apostrophe, Comma, Minus, Period, Slash,
        Semicolon, Equal, LeftBracket, Backslash, RightBracket, GraveAccent,
        Escape, Enter, Tab, Backspace, Insert, Delete,
        Right, Left, Down, Up,
        PageUp, PageDown, Home, End,
        CapsLock, ScrollLock, NumLock,
        PrintScreen, Pause,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10,
        F11, F12, F13, F14, F15, F16, F17, F18, F19, F20,
        F21, F22, F23, F24,
        LeftShift, RightShift,
        LeftControl, RightControl,
        LeftAlt, RightAlt,
        LeftSuper, RightSuper,
        Menu,
        KP0, KP1, KP2, KP3, KP4, KP5, KP6, KP7, KP8, KP9,
        KPDecimal, KPDivide, KPMultiply,
        KPSubtract, KPAdd, KPEnter, KPEqual,
        Count
    };

    enum MouseButton : std::uint8_t
    {
        MouseLeft = 0,
        MouseRight,
        MouseMiddle,
        MouseButton4,
        MouseButton5,
        MouseButton6,
        MouseButton7,
        MouseButton8,
        MouseCount
    };

    // --------------------------------------------------------
    // State storage (header-only → module globals)
    // --------------------------------------------------------
    inline std::thread::id        g_pollingThread{};
    inline std::atomic<bool>     g_pollingThreadLocked{ false };
    inline std::shared_mutex     g_inputMutex{};

    inline std::bitset<Key::Count>               keyDown{};
    inline std::bitset<Key::Count>               keyPressed{};
    inline std::bitset<MouseButton::MouseCount>  mouseDown{};
    inline std::bitset<MouseButton::MouseCount>  mousePressed{};

    inline std::atomic<int>   mouseX{ 0 };
    inline std::atomic<int>   mouseY{ 0 };
    inline std::atomic<int>   mouseWheel{ 0 };
    inline std::atomic<bool>  mouseCoordsAreGlobal{ true };

    // --------------------------------------------------------
    // Thread ownership helpers
    // --------------------------------------------------------
    export inline void set_mouse_coords_are_global(bool v)
    {
        mouseCoordsAreGlobal.store(v, std::memory_order_release);
    }

    export inline bool are_mouse_coords_global()
    {
        return mouseCoordsAreGlobal.load(std::memory_order_acquire);
    }

    export inline void designate_polling_thread(std::thread::id id)
    {
        g_pollingThread = id;
        g_pollingThreadLocked.store(true, std::memory_order_release);
    }

    export inline void designate_polling_thread_to_current()
    {
        designate_polling_thread(std::this_thread::get_id());
    }

    export inline void clear_polling_thread_designation()
    {
        g_pollingThread = {};
        g_pollingThreadLocked.store(false, std::memory_order_release);
    }

    export inline bool can_poll_on_this_thread()
    {
        if (!g_pollingThreadLocked.load(std::memory_order_acquire))
            return true;
        return std::this_thread::get_id() == g_pollingThread;
    }

    // ========================================================
    // Win32 implementation
    // ========================================================
#if defined(_WIN32)

    export inline constexpr int map_key_to_vk(Key k)
    {
        switch (k)
        {
        case Key::A: return 'A'; case Key::B: return 'B'; case Key::C: return 'C'; case Key::D: return 'D';
        case Key::E: return 'E'; case Key::F: return 'F'; case Key::G: return 'G'; case Key::H: return 'H';
        case Key::I: return 'I'; case Key::J: return 'J'; case Key::K: return 'K'; case Key::L: return 'L';
        case Key::M: return 'M'; case Key::N: return 'N'; case Key::O: return 'O'; case Key::P: return 'P';
        case Key::Q: return 'Q'; case Key::R: return 'R'; case Key::S: return 'S'; case Key::T: return 'T';
        case Key::U: return 'U'; case Key::V: return 'V'; case Key::W: return 'W'; case Key::X: return 'X';
        case Key::Y: return 'Y'; case Key::Z: return 'Z';

        case Key::Num0: return '0'; case Key::Num1: return '1'; case Key::Num2: return '2';
        case Key::Num3: return '3'; case Key::Num4: return '4'; case Key::Num5: return '5';
        case Key::Num6: return '6'; case Key::Num7: return '7'; case Key::Num8: return '8'; case Key::Num9: return '9';

        case Key::Escape: return VK_ESCAPE;
        case Key::Enter: return VK_RETURN;
        case Key::Tab: return VK_TAB;
        case Key::Backspace: return VK_BACK;
        case Key::Space: return VK_SPACE;
        case Key::Left: return VK_LEFT;
        case Key::Right: return VK_RIGHT;
        case Key::Up: return VK_UP;
        case Key::Down: return VK_DOWN;

        case Key::LeftShift: return VK_LSHIFT;
        case Key::RightShift: return VK_RSHIFT;
        case Key::LeftControl: return VK_LCONTROL;
        case Key::RightControl: return VK_RCONTROL;
        case Key::LeftAlt: return VK_LMENU;
        case Key::RightAlt: return VK_RMENU;
        case Key::LeftSuper: return VK_LWIN;
        case Key::RightSuper: return VK_RWIN;

        default: return 0;
        }
    }

    export inline constexpr int map_mouse_to_vk(MouseButton b)
    {
        switch (b)
        {
        case MouseButton::MouseLeft:   return VK_LBUTTON;
        case MouseButton::MouseRight:  return VK_RBUTTON;
        case MouseButton::MouseMiddle: return VK_MBUTTON;
        case MouseButton::MouseButton4:return XBUTTON1;
        case MouseButton::MouseButton5:return XBUTTON2;
        default: return 0;
        }
    }

    // --------------------------------------------------------
    // Per-frame polling
    // --------------------------------------------------------
    export inline void poll_input()
    {
        if (!can_poll_on_this_thread())
            return;

        std::unique_lock lock(g_inputMutex);

        keyPressed.reset();
        mousePressed.reset();
        mouseWheel.store(0, std::memory_order_relaxed);

        for (std::uint16_t k = 0; k < Key::Count; ++k)
        {
            int vk = map_key_to_vk(static_cast<Key>(k));
            if (!vk) continue;

            bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
            if (down && !keyDown.test(k))
                keyPressed.set(k);

            keyDown.set(k, down);
        }

        for (std::uint8_t b = 0; b < MouseButton::MouseCount; ++b)
        {
            int vk = map_mouse_to_vk(static_cast<MouseButton>(b));
            if (!vk) continue;

            bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
            if (down && !mouseDown.test(b))
                mousePressed.set(b);

            mouseDown.set(b, down);
        }

        POINT p{};
        if (GetCursorPos(&p))
        {
            mouseX.store(p.x, std::memory_order_relaxed);
            mouseY.store(p.y, std::memory_order_relaxed);
            set_mouse_coords_are_global(true);
        }
    }

#endif // _WIN32

    // ========================================================
    // Query helpers (platform independent)
    // ========================================================
    export inline bool is_key_held(Key k)
    {
        std::shared_lock lock(g_inputMutex);
        return keyDown.test(static_cast<size_t>(k));
    }

    export inline bool is_key_down(Key k)
    {
        std::shared_lock lock(g_inputMutex);
        return keyPressed.test(static_cast<size_t>(k));
    }

    export inline bool is_mouse_button_held(MouseButton b)
    {
        std::shared_lock lock(g_inputMutex);
        return mouseDown.test(static_cast<size_t>(b));
    }

    export inline bool is_mouse_button_down(MouseButton b)
    {
        std::shared_lock lock(g_inputMutex);
        return mousePressed.test(static_cast<size_t>(b));
    }
} // namespace almondnamespace::input

// ============================================================
// Win32 WndProc hook (module-visible, header-only → module)
// ============================================================
#if defined(_WIN32) && !defined(ALMOND_MAIN_HEADLESS)

export inline LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
    using namespace almondnamespace::input;

    switch (msg)
    {
    case WM_LBUTTONDOWN: mouseDown.set(MouseLeft); break;
    case WM_LBUTTONUP:   mouseDown.reset(MouseLeft); break;
    case WM_RBUTTONDOWN: mouseDown.set(MouseRight); break;
    case WM_RBUTTONUP:   mouseDown.reset(MouseRight); break;
    case WM_MOUSEWHEEL:
        mouseWheel.fetch_add(GET_WHEEL_DELTA_WPARAM(wParam), std::memory_order_relaxed);
        break;
    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

#endif
