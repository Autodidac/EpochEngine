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

//#include "epoch.engine.hpp" // DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT

export module sdl.state;
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)

import platform.engine;

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
    struct PresentationDimensions
    {
        int logicalWidth{};
        int logicalHeight{};
        int framebufferWidth{};
        int framebufferHeight{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return logicalWidth > 0
                && logicalHeight > 0
                && framebufferWidth > 0
                && framebufferHeight > 0;
        }

        [[nodiscard]] constexpr float pixel_scale_x() const noexcept
        {
            return valid()
                ? static_cast<float>(framebufferWidth)
                    / static_cast<float>(logicalWidth)
                : 0.0f;
        }

        [[nodiscard]] constexpr float pixel_scale_y() const noexcept
        {
            return valid()
                ? static_cast<float>(framebufferHeight)
                    / static_cast<float>(logicalHeight)
                : 0.0f;
        }
    };

    [[nodiscard]] constexpr PresentationDimensions make_presentation_dimensions(
        int logicalWidth,
        int logicalHeight,
        int framebufferWidth,
        int framebufferHeight) noexcept
    {
        return {
            logicalWidth,
            logicalHeight,
            framebufferWidth,
            framebufferHeight};
    }

    [[nodiscard]] constexpr PresentationDimensions
        make_display_scaled_presentation_dimensions(
            int framebufferWidth,
            int framebufferHeight,
            float displayScale) noexcept
    {
        if (framebufferWidth <= 0
            || framebufferHeight <= 0
            || displayScale <= 0.0f
            || displayScale > 16.0f)
        {
            return {};
        }

        return make_presentation_dimensions(
            static_cast<int>(
                static_cast<float>(framebufferWidth) / displayScale + 0.5f),
            static_cast<int>(
                static_cast<float>(framebufferHeight) / displayScale + 0.5f),
            framebufferWidth,
            framebufferHeight);
    }

    [[nodiscard]] constexpr int normalize_presented_coordinate(
        int coordinate,
        int logicalExtent,
        int framebufferExtent) noexcept
    {
        if (logicalExtent <= 0
            || framebufferExtent <= 0
            || logicalExtent == framebufferExtent)
        {
            return coordinate;
        }

        const long long scaled =
            static_cast<long long>(coordinate)
            * static_cast<long long>(logicalExtent);
        const long long half = framebufferExtent / 2;
        return static_cast<int>(
            scaled >= 0
                ? (scaled + half) / framebufferExtent
                : (scaled - half) / framebufferExtent);
    }

    enum class SurfaceLifecycleEvent : unsigned char
    {
        minimized,
        restored,
        occluded,
        exposed,
        resized
    };

    struct SurfaceLifecycle
    {
        bool minimized{};
        bool occluded{};
        bool zeroDrawable{true};
        bool dimensionsDirty{true};
        bool firstPresentRequired{true};
        unsigned long long transitionGeneration{};

        constexpr void reset() noexcept
        {
            minimized = false;
            occluded = false;
            zeroDrawable = true;
            dimensionsDirty = true;
            firstPresentRequired = true;
            ++transitionGeneration;
        }

        constexpr void observe(SurfaceLifecycleEvent event) noexcept
        {
            switch (event)
            {
            case SurfaceLifecycleEvent::minimized:
                if (!minimized)
                    ++transitionGeneration;
                minimized = true;
                firstPresentRequired = true;
                break;
            case SurfaceLifecycleEvent::restored:
                if (minimized || !dimensionsDirty)
                    ++transitionGeneration;
                minimized = false;
                dimensionsDirty = true;
                firstPresentRequired = true;
                break;
            case SurfaceLifecycleEvent::occluded:
                occluded = true;
                break;
            case SurfaceLifecycleEvent::exposed:
                occluded = false;
                dimensionsDirty = true;
                firstPresentRequired = true;
                ++transitionGeneration;
                break;
            case SurfaceLifecycleEvent::resized:
                dimensionsDirty = true;
                firstPresentRequired = true;
                ++transitionGeneration;
                break;
            }
        }

        constexpr void observe_window_flags(
            bool isMinimized,
            bool isOccluded) noexcept
        {
            if (isMinimized != minimized)
            {
                observe(
                    isMinimized
                        ? SurfaceLifecycleEvent::minimized
                        : SurfaceLifecycleEvent::restored);
            }
            occluded = isOccluded;
        }

        constexpr void observe_drawable_extent(int width, int height) noexcept
        {
            const bool nextZeroDrawable = width <= 0 || height <= 0;
            if (nextZeroDrawable != zeroDrawable)
            {
                ++transitionGeneration;
                firstPresentRequired = true;
            }
            zeroDrawable = nextZeroDrawable;
            dimensionsDirty = false;
        }

        [[nodiscard]] constexpr bool rendering_allowed() const noexcept
        {
            return !minimized && !zeroDrawable && !dimensionsDirty;
        }

        [[nodiscard]] constexpr bool capture_allowed() const noexcept
        {
            return rendering_allowed();
        }

        [[nodiscard]] constexpr bool retain_queued_work() const noexcept
        {
            return !rendering_allowed();
        }

        constexpr void acknowledge_present() noexcept
        {
            if (rendering_allowed())
                firstPresentRequired = false;
        }
    };

    inline void observe_window_event(
        SurfaceLifecycle& lifecycle,
        Uint32 eventType) noexcept
    {
        switch (eventType)
        {
        case SDL_EVENT_WINDOW_MINIMIZED:
            lifecycle.observe(SurfaceLifecycleEvent::minimized);
            break;
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_MAXIMIZED:
            lifecycle.observe(SurfaceLifecycleEvent::restored);
            break;
        case SDL_EVENT_WINDOW_OCCLUDED:
            lifecycle.observe(SurfaceLifecycleEvent::occluded);
            break;
        case SDL_EVENT_WINDOW_SHOWN:
        case SDL_EVENT_WINDOW_EXPOSED:
            lifecycle.observe(SurfaceLifecycleEvent::exposed);
            break;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            lifecycle.observe(SurfaceLifecycleEvent::resized);
            break;
        default:
            break;
        }
    }

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
        SurfaceLifecycle surfaceLifecycle{};

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

        // SDL's process-wide subsystem state is serialized by runtime_api_mutex().

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
    std::recursive_mutex& runtime_api_mutex() noexcept;
}

namespace epochengine::sdlcontext::state
{
    inline SDL3State& get_sdl_state() noexcept
    {
        static SDL3State state{};
        return state;
    }

    inline std::recursive_mutex& runtime_api_mutex() noexcept
    {
        static std::recursive_mutex mutex{};
        return mutex;
    }
}

#endif
