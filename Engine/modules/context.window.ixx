/************************************************
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•—  â–ˆâ–ˆâ•—   *
 *  â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ•”â•â•â•  â–ˆâ–ˆâ•”â•â•â•â• â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘     â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â•šâ•â•â•â•â•â•â•â•šâ•â•      â•šâ•â•â•â•â•â•  â•šâ•â•â•â•â•â•â•šâ•â•  â•šâ•â•   *
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

#include <atomic>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>
#include <functional>
#include <memory>

// Must be before anything that might include <windows.h>
#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#endif

#include "../include/engine.hpp"                 // DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT
#include "../include/engine.config.hpp"          // EPOCH_USING_* macros (fix path; do not use <include/...>)

#if defined(_WIN32)
#   ifdef EPOCH_USING_WINMAIN
#       include "framework.hpp"
#   endif
#endif

#include <glad/glad.h>

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
#   include <SDL3/SDL.h>
#   include <SDL3/SDL_version.h>
#endif

#if defined(EPOCH_USING_SFML)
#define SFML_STATIC
#include <SFML/Graphics.hpp>
#endif

// If any include above dragged in <windows.h>, defuse min/max anyway.
#if defined(_WIN32)
#   ifdef min
#       undef min
#   endif
#   ifdef max
#       undef max
#   endif
#endif

export module context.window;

//import aframework;                    // if this imports SFML headers, NOMINMAX is already set
import context.type;
import context.commandqueue;

namespace epochnamespace::core
{
    export using OpaqueContextHandle = std::shared_ptr<void>;
    export using ThreadInitializeCallback = std::function<bool(const OpaqueContextHandle&)>;

    export struct WindowData final
    {
#if defined(_WIN32)
#   if !defined(EPOCH_MAIN_HEADLESS)
        HWND  hwnd = nullptr;
        HWND  hwndChild = nullptr;
        HWND  host_hwnd = nullptr;
        HDC   hdc = nullptr;

        HGLRC glrc = nullptr;
        HGLRC& glContext = glrc; // legacy alias
#   endif
#else
        void* hwnd = nullptr;
        void* hdc = nullptr;
        void* glContext = nullptr;
#endif

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        SDL_Window* sdl_window = nullptr;
        SDL_GLContext sdl_glrc = nullptr;
#endif

#if defined(EPOCH_USING_SFML)
        sf::RenderWindow* sfml_window = nullptr;
        sf::Context       sfml_context{};
#endif

        OpaqueContextHandle          context{};
        core::CommandQueue          commandQueue{};
        ThreadInitializeCallback    threadInitialize{};

        bool running = false;
        bool usesSharedContext = false;

        core::ContextType type = core::ContextType::Custom;

        std::wstring titleWide{};
        std::string  titleNarrow{};

        int  width = DEFAULT_WINDOW_WIDTH;
        int  height = DEFAULT_WINDOW_HEIGHT;
        bool should_close = false;
        bool isFloating = false;
        std::atomic_bool firstPresentComplete = false;

        std::function<void(int, int)> onResize{};

        using RenderCommand = std::function<void()>;

        void EnqueueCommand(RenderCommand cmd)
        {
            commandQueue.enqueue(std::move(cmd));
        }

        void EnqueueCommand(RenderCommand cmd, core::RenderPath path)
        {
            commandQueue.enqueue(std::move(cmd), path);
        }

        inline static WindowData* s_instance = nullptr;

        WindowData() = default;

#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)
        WindowData(HWND inHwnd, HDC inHdc, HGLRC inGlrc, bool inUsesShared, core::ContextType inType)
            : hwnd(inHwnd)
            , hdc(inHdc)
            , glrc(inGlrc)
            , usesSharedContext(inUsesShared)
            , type(inType)
        {
        }
#else
        WindowData(void* inHwnd, void* inHdc, void* inGlrc, bool inUsesShared, core::ContextType inType)
            : hwnd(inHwnd)
            , hdc(inHdc)
            , glContext(inGlrc)
            , usesSharedContext(inUsesShared)
            , type(inType)
        {
        }
#endif

        static void set_global_instance(WindowData* instance) noexcept { s_instance = instance; }
        static WindowData* get_global_instance() noexcept { return s_instance; }

#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)
        void setParentHandle(HWND v) noexcept { hwnd = v; }
        void setChildHandle(HWND v) noexcept { hwndChild = v; }

        HWND getParentHandle() const noexcept { return hwnd; }
        HWND getChildHandle()  const noexcept { return hwndChild; }
#endif

#if defined(_WIN32)
        static HWND getWindowHandle() noexcept
        {
#   if defined(EPOCH_MAIN_HEADLESS)
            return nullptr;
#   else
            return s_instance ? s_instance->hwnd : nullptr;
#   endif
        }

        static HWND getChildHandleStatic() noexcept
        {
#   if defined(EPOCH_MAIN_HEADLESS)
            return nullptr;
#   else
            return s_instance ? s_instance->hwndChild : nullptr;
#   endif
        }
#endif

        void set_size(int w, int h) noexcept { width = w; height = h; }
        [[nodiscard]] int  get_width()  const noexcept { return width; }
        [[nodiscard]] int  get_height() const noexcept { return height; }

        void set_should_close(bool v) noexcept { should_close = v; }
        [[nodiscard]] bool get_should_close() const noexcept { return should_close; }

        void set_window_title(std::string_view title) noexcept
        {
            titleNarrow.assign(title.begin(), title.end());
#if defined(_WIN32)
            titleWide.assign(titleNarrow.begin(), titleNarrow.end());
#endif
        }

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        static SDL_Window* getSDLWindow() noexcept { return s_instance ? s_instance->sdl_window : nullptr; }
#endif

#if defined(EPOCH_USING_SFML)
        sf::RenderWindow* getSFMLWindow()
        {
            if (!s_instance) throw std::runtime_error("WindowData is null!");
            return s_instance->sfml_window;
        }
#endif
    };
}

namespace epochnamespace::contextwindow
{
    export using WindowData = epochnamespace::core::WindowData;
}
