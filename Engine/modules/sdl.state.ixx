/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include <array>
#include <bitset>
#include <functional>
#include <mutex>
#include <SDL3/SDL.h>

#include <include/engine.config.hpp> // for EPOCH_USING_RAYLIB

//#include "engine.hpp" // DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT

export module sdl.state;
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)

import engine.platform;

import core.timer;
import context.window;

inline constexpr int DEFAULT_WINDOW_WIDTH = 1280;
inline constexpr int DEFAULT_WINDOW_HEIGHT = 720;

// Win32 forward decls MUST be here (module purview), not in the global module fragment.
#if defined(_WIN32)
namespace epochengine::win32
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

export namespace epochengine::sdlcontext::state
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

        epochengine::contextwindow::WindowData window{};

        SDL_Event sdl_event{};

        bool shouldClose{ false };
        int screenWidth{ DEFAULT_WINDOW_WIDTH };
        int screenHeight{ DEFAULT_WINDOW_HEIGHT };
        bool running{ false };
        bool renderFaulted{ false };

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

        epochengine::timing::Timer pollTimer = epochengine::timing::createTimer(1.0);
        epochengine::timing::Timer fpsTimer = epochengine::timing::createTimer(1.0);
        int frameCount = 0;

#if defined(_WIN32)
    private:
        epochengine::win32::WNDPROC oldWndProc_ = nullptr;
        epochengine::win32::HWND    parent_ = nullptr;

    public:
        auto getOldWndProc() const noexcept { return oldWndProc_; }
        void setOldWndProc(epochengine::win32::WNDPROC proc) noexcept { oldWndProc_ = proc; }

        void setParent(epochengine::win32::HWND parent) noexcept { parent_ = parent; }
        auto getParent() const noexcept { return parent_; }

        // If you want these accessors, WindowData must expose real HWND/HDC/HGLRC types
        // (meaning a Win32 header/module somewhere). Otherwise remove these.
        // epochengine::win32::HWND  hwnd() const noexcept { return (epochengine::win32::HWND)window.hwnd; }
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

namespace epochengine::sdlcontext::state
{
    inline SDL3State& get_sdl_state() noexcept
    {
        static SDL3State state{};
        return state;
    }
}

#endif
