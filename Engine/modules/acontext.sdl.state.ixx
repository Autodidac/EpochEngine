module;

//#include "aengine.hpp" // DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT

export module acontext.sdl.state;

import aengine.platform;

import <array>;
import <bitset>;
import <functional>;
import <SDL3/SDL.h>;

import aengine.core.time;
import aengine.context.window;

inline constexpr int DEFAULT_WINDOW_WIDTH = 1280;
inline constexpr int DEFAULT_WINDOW_HEIGHT = 720;

// Win32 forward decls MUST be here (module purview), not in the global module fragment.
#if defined(_WIN32)
namespace almondnamespace::win32
{
    struct HWND__;
    struct HDC__;
    struct HGLRC__;
    using HWND = HWND__*;
    using HDC = HDC__*;
    using HGLRC = HGLRC__*;

    using LRESULT = long long;
    using WPARAM = unsigned long long;
    using LPARAM = long long;

    using WNDPROC = LRESULT(__stdcall*)(HWND, unsigned int, WPARAM, LPARAM);
}
#endif

export namespace almondnamespace::sdlcontext::state
{
    struct SDL3State
    {
        SDL3State()
        {
            window.width = DEFAULT_WINDOW_WIDTH;
            window.height = DEFAULT_WINDOW_HEIGHT;
            window.should_close = false;
            screenWidth = window.width;
            screenHeight = window.height;
        }

        almondnamespace::contextwindow::WindowData window{};

        SDL_Event sdl_event{};

        bool shouldClose{ false };
        int screenWidth{ DEFAULT_WINDOW_WIDTH };
        int screenHeight{ DEFAULT_WINDOW_HEIGHT };
        bool running{ false };

        std::function<void(int, int)> onResize{};

        struct MouseState
        {
            std::array<bool, 5> down{};
            std::array<bool, 5> pressed{};
            std::array<bool, 5> prevDown{};
            int lastX = 0;
            int lastY = 0;
        } mouse{};

        struct KeyboardState
        {
            std::bitset<SDL_SCANCODE_COUNT> down;
            std::bitset<SDL_SCANCODE_COUNT> pressed;
            std::bitset<SDL_SCANCODE_COUNT> prevDown;
        } keyboard{};

        almondnamespace::timing::Timer pollTimer = almondnamespace::timing::createTimer(1.0);
        almondnamespace::timing::Timer fpsTimer = almondnamespace::timing::createTimer(1.0);
        int frameCount = 0;

#if defined(_WIN32)
    private:
        almondnamespace::win32::WNDPROC oldWndProc_ = nullptr;
        almondnamespace::win32::HWND    parent_ = nullptr;

    public:
        auto getOldWndProc() const noexcept { return oldWndProc_; }
        void setOldWndProc(almondnamespace::win32::WNDPROC proc) noexcept { oldWndProc_ = proc; }

        void setParent(almondnamespace::win32::HWND parent) noexcept { parent_ = parent; }
        auto getParent() const noexcept { return parent_; }

        // If you want these accessors, WindowData must expose real HWND/HDC/HGLRC types
        // (meaning a Win32 header/module somewhere). Otherwise remove these.
        // almondnamespace::win32::HWND  hwnd() const noexcept { return (almondnamespace::win32::HWND)window.hwnd; }
#endif

        void mark_should_close(bool value) noexcept
        {
            shouldClose = value;
            window.should_close = value;
        }

        void set_dimensions(int w, int h) noexcept
        {
            screenWidth = w;
            screenHeight = h;
            window.set_size(w, h);
        }
    };

    SDL3State& get_sdl_state() noexcept;
}

namespace almondnamespace::sdlcontext::state
{
    inline SDL3State& get_sdl_state() noexcept
    {
        static SDL3State state{};
        return state;
    }
}
