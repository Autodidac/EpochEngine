// acontext.sdl.context.ixx
module;

// -----------------------------------------------------------------------------
// Global module fragment: macros + C headers MUST live here.
// -----------------------------------------------------------------------------

#ifndef SDL_WINDOW_FULLSCREEN_DESKTOP
#define SDL_WINDOW_FULLSCREEN_DESKTOP 1
#endif

// SDL wants this defined BEFORE including SDL headers.
#define SDL_MAIN_HANDLED

#include <include/aengine.config.hpp> // for ALMOND_USING Macros

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>   // HWND, RECT, LONG_PTR, SetParent, GetWindowLongPtr, etc.
#endif

#include <chrono> 
#include <thread>

#include <SDL3/SDL.h>    // keep in GMF because it’s a C header with macros

export module acontext.sdl.context;

// Project
import aengine.platform;              // fine, but does NOT replace windows.h for this TU
import aengine.core.context;
import aengine.context.window;
import aengine.context.commandqueue;
import aengine.context.control;
import aatlas.manager;
import acontext.sdl.state;
import acontext.sdl.renderer;
import acontext.sdl.textures;
import aengine.context.multiplexer;   // MakeDockable(...)
import aengine.diagnostics;
import aengine.telemetry;

// Std
import <algorithm>;
//import <chrono>;  // as include for intellisense stability, this can probably be changed in the future
import <cstdint>;
import <functional>;
import <iostream>;
import <memory>;
import <mutex>;
import <stdexcept>;
import <string>;
import <utility>;

export namespace almondnamespace::sdlcontext
{
#if defined(ALMOND_USING_SDL)

    struct SDLState
    {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;

#if defined(_WIN32)
        HWND parent = nullptr;
        HWND hwnd = nullptr;
#endif

        bool running = false;

        int width = 400;
        int height = 300;

        int framebufferWidth = 400;
        int framebufferHeight = 300;

        int virtualWidth = 400;
        int virtualHeight = 300;

        std::function<void(int, int)> onResize;

        bool useFrameLimiter = false;
        std::chrono::steady_clock::time_point lastFrameTime{};
    };

    inline SDLState sdlcontext{};

    inline void refresh_dimensions(const std::shared_ptr<core::Context>& ctx) noexcept
    {
        int logicalW = (std::max)(1, sdlcontext.width);
        int logicalH = (std::max)(1, sdlcontext.height);

        if (sdlcontext.window)
        {
            int windowW = 0;
            int windowH = 0;
            SDL_GetWindowSize(sdlcontext.window, &windowW, &windowH);
            if (windowW > 0 && windowH > 0)
            {
                logicalW = windowW;
                logicalH = windowH;
            }
        }

        sdlcontext.width = logicalW;
        sdlcontext.height = logicalH;
        sdlcontext.virtualWidth = logicalW;
        sdlcontext.virtualHeight = logicalH;

        int fbW = logicalW;
        int fbH = logicalH;

        if (sdlcontext.renderer)
        {
            int renderW = 0;
            int renderH = 0;
            if (SDL_GetCurrentRenderOutputSize(sdlcontext.renderer, &renderW, &renderH) == 0
                && renderW > 0 && renderH > 0)
            {
                fbW = renderW;
                fbH = renderH;
            }
        }

        sdlcontext.framebufferWidth = (std::max)(1, fbW);
        sdlcontext.framebufferHeight = (std::max)(1, fbH);

        if (ctx)
        {
            ctx->width = sdlcontext.width;
            ctx->height = sdlcontext.height;
            ctx->virtualWidth = sdlcontext.virtualWidth;
            ctx->virtualHeight = sdlcontext.virtualHeight;
            ctx->framebufferWidth = sdlcontext.framebufferWidth;
            ctx->framebufferHeight = sdlcontext.framebufferHeight;
            if (ctx->windowData)
            {
                ctx->windowData->sdl_window = sdlcontext.window;
                ctx->windowData->width = sdlcontext.width;
                ctx->windowData->height = sdlcontext.height;
            }
        }

        auto& sharedState = state::get_sdl_state();
        sharedState.window.sdl_window = sdlcontext.window;
        sharedState.set_dimensions(sdlcontext.width, sdlcontext.height);
    }

    inline bool sdl_initialize(std::shared_ptr<core::Context> ctx,
#if defined(_WIN32)
        HWND parentWnd = nullptr,
#else
        void* parentWnd = nullptr,
#endif
        int w = 400,
        int h = 300,
        std::function<void(int, int)> onResize = nullptr,
        std::string windowTitle = {})
    {
        const int clampedWidth = (std::max)(1, w);
        const int clampedHeight = (std::max)(1, h);

        sdlcontext.width = clampedWidth;
        sdlcontext.height = clampedHeight;
        sdlcontext.virtualWidth = clampedWidth;
        sdlcontext.virtualHeight = clampedHeight;
        sdlcontext.framebufferWidth = clampedWidth;
        sdlcontext.framebufferHeight = clampedHeight;

#if defined(_WIN32)
        HWND hostWnd = parentWnd;
        HWND dockParent = hostWnd ? ::GetParent(hostWnd) : nullptr;
        sdlcontext.parent = dockParent;
#endif

        refresh_dimensions(ctx);

        std::weak_ptr<core::Context> weakCtx = ctx;
        auto userResize = std::move(onResize);

        sdlcontext.onResize =
            [weakCtx, userResize = std::move(userResize)](int width, int height) mutable
            {
                sdlcontext.width = (std::max)(1, width);
                sdlcontext.height = (std::max)(1, height);
                sdlcontext.virtualWidth = sdlcontext.width;
                sdlcontext.virtualHeight = sdlcontext.height;

                auto locked = weakCtx.lock();
                refresh_dimensions(locked);

                if (userResize)
                    userResize(sdlcontext.framebufferWidth, sdlcontext.framebufferHeight);
            };

        if (ctx)
            ctx->onResize = sdlcontext.onResize;

        if (static_cast<int>(SDL_Init(SDL_INIT_VIDEO)) < 0)
        {
            SDL_Log("SDL_Init failed: %s", SDL_GetError());
            return false;
        }

        if (windowTitle.empty())
            windowTitle = "SDL3 Window";

        SDL_PropertiesID props = SDL_CreateProperties();
        if (!props)
        {
            std::cerr << "[SDL] SDL_CreateProperties failed: " << SDL_GetError() << "\n";
            SDL_Quit();
            return false;
        }

        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, windowTitle.c_str());
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, sdlcontext.width);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, sdlcontext.height);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);

        sdlcontext.window = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);

        if (!sdlcontext.window)
        {
            std::cerr << "[SDL] SDL_CreateWindowWithProperties failed: " << SDL_GetError() << "\n";
            SDL_Quit();
            return false;
        }

#if defined(_WIN32)
        {
            SDL_PropertiesID windowProps = SDL_GetWindowProperties(sdlcontext.window);
            if (!windowProps)
            {
                std::cerr << "[SDL] SDL_GetWindowProperties failed: " << SDL_GetError() << "\n";
                SDL_DestroyWindow(sdlcontext.window);
                SDL_Quit();
                return false;
            }

            sdlcontext.hwnd = static_cast<HWND>(
                SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));

            if (!sdlcontext.hwnd)
            {
                std::cerr << "[SDL] Failed to retrieve HWND\n";
                SDL_DestroyWindow(sdlcontext.window);
                SDL_Quit();
                return false;
            }

            if (ctx)
                ctx->hwnd = sdlcontext.hwnd;

            if (ctx && ctx->windowData)
            {
                const HWND previousHwnd = ctx->windowData->hwnd;
                const HDC previousHdc = ctx->windowData->hdc;
                ctx->windowData->hwnd = sdlcontext.hwnd;
                ctx->windowData->host_hwnd = hostWnd;
                ctx->windowData->hdc = nullptr;
                ctx->hdc = nullptr;

                if (hostWnd && previousHwnd && previousHwnd != sdlcontext.hwnd)
                {
                    if (previousHdc)
                        ::ReleaseDC(previousHwnd, previousHdc);

                    auto& threads = core::Threads();
                    auto it = threads.find(previousHwnd);
                    if (it != threads.end())
                    {
                        threads.emplace(sdlcontext.hwnd, std::move(it->second));
                        threads.erase(it);
                    }
                }
            }
        }
#else
        if (ctx)
            ctx->hwnd = nullptr;
#endif

        SDL_SetWindowTitle(sdlcontext.window, windowTitle.c_str());

        auto create_renderer = [&](const char* name, const char* label) -> SDL_Renderer*
        {
            SDL_Renderer* renderer = SDL_CreateRenderer(sdlcontext.window, name);
            if (renderer)
            {
                std::cerr << "[SDL] Created renderer with " << label << ".\n";
                return renderer;
            }

            std::cerr << "[SDL] SDL_CreateRenderer (" << label << ") failed: "
                      << SDL_GetError() << "\n";
            return nullptr;
        };

        sdlcontext.renderer = create_renderer(nullptr, "default renderer");
        if (!sdlcontext.renderer)
        {
            sdlcontext.renderer = create_renderer("software", "software renderer");
        }
        if (!sdlcontext.renderer)
        {
            std::cerr << "[SDL] SDL_CreateRenderer failed after fallbacks: "
                      << SDL_GetError() << "\n";
            SDL_DestroyWindow(sdlcontext.window);
            SDL_Quit();
            return false;
        }

        const int vsyncResult = SDL_SetRenderVSync(sdlcontext.renderer, 1);
        if (vsyncResult != 0)
        {
            std::cerr << "[SDL] SDL_SetRenderVSync failed: " << SDL_GetError() << "\n";
            if (sdlcontext.parent)
            {
                sdlcontext.useFrameLimiter = true;
                sdlcontext.lastFrameTime = std::chrono::steady_clock::now();
            }
        }
        else
        {
            sdlcontext.useFrameLimiter = false;
        }

        init_renderer(sdlcontext.renderer);
        sdltextures::sdl_renderer = sdlcontext.renderer;

        refresh_dimensions(ctx);

#if defined(_WIN32)
        if (hostWnd)
        {
            if (sdlcontext.parent)
            {
                SetParent(sdlcontext.hwnd, sdlcontext.parent);

                LONG_PTR style = GetWindowLongPtr(sdlcontext.hwnd, GWL_STYLE);
                style &= ~WS_OVERLAPPEDWINDOW;
                style |= WS_CHILD | WS_VISIBLE;
                SetWindowLongPtr(sdlcontext.hwnd, GWL_STYLE, style);

                almondnamespace::core::MakeDockable(sdlcontext.hwnd, sdlcontext.parent);
            }

            if (!windowTitle.empty())
            {
                std::wstring wideTitle(windowTitle.begin(), windowTitle.end());
                SetWindowTextW(sdlcontext.hwnd, wideTitle.c_str());
            }

            RECT client{};
            GetClientRect(hostWnd, &client);

            const int width = (std::max)(1, static_cast<int>(client.right - client.left));
            const int height = (std::max)(1, static_cast<int>(client.bottom - client.top));

            sdlcontext.width = width;
            sdlcontext.height = height;

            SetWindowPos(
                sdlcontext.hwnd, nullptr, 0, 0, width, height,
                SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

            if (sdlcontext.onResize)
                sdlcontext.onResize(width, height);

            if (sdlcontext.parent)
                PostMessage(sdlcontext.parent, WM_SIZE, 0, MAKELPARAM(width, height));

            ::ShowWindow(hostWnd, SW_HIDE);
            ::DestroyWindow(hostWnd);
        }
#endif

        SDL_ShowWindow(sdlcontext.window);
        sdlcontext.running = true;
        state::get_sdl_state().running = true;

        atlasmanager::register_backend_uploader(core::ContextType::SDL,
            [](const TextureAtlas& atlas)
            {
                sdltextures::ensure_uploaded(atlas);
            });

        return true;
    }

    inline bool sdl_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        const core::ContextType backendType = ctx ? ctx->type : core::ContextType::SDL;

        std::uintptr_t windowId = 0u;
#if defined(_WIN32)
        if (ctx && ctx->windowData)
            windowId = reinterpret_cast<std::uintptr_t>(ctx->windowData->hwnd);
        else
            windowId = reinterpret_cast<std::uintptr_t>(sdlcontext.hwnd);
#else
        if (ctx && ctx->windowData)
            windowId = reinterpret_cast<std::uintptr_t>(ctx->windowData->hwnd);
        else
            windowId = 0u;
#endif

        almond::diagnostics::FrameTiming frameTimer{ backendType, windowId, "SDL" };

        SDL_Event sdl_event{};
        while (SDL_PollEvent(&sdl_event))
        {
            if (sdl_event.type == SDL_EVENT_QUIT)
            {
                sdlcontext.running = false;
                state::get_sdl_state().mark_should_close(true);
                return false;
            }

            if (sdl_event.type == SDL_EVENT_WINDOW_RESIZED && sdlcontext.onResize)
                sdlcontext.onResize(sdl_event.window.data1, sdl_event.window.data2);
        }

        refresh_dimensions(ctx);

        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(sdlcontext.framebufferWidth),
            telemetry::RendererTelemetryTags{ backendType, windowId, "width" });

        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(sdlcontext.framebufferHeight),
            telemetry::RendererTelemetryTags{ backendType, windowId, "height" });

        const auto color = core::clear_color_for_context(core::ContextType::SDL);
        SDL_SetRenderDrawColor(
            sdl_renderer.renderer,
            static_cast<std::uint8_t>(color[0] * 255.0f),
            static_cast<std::uint8_t>(color[1] * 255.0f),
            static_cast<std::uint8_t>(color[2] * 255.0f),
            static_cast<std::uint8_t>(color[3] * 255.0f));
        SDL_RenderClear(sdl_renderer.renderer);

        const std::size_t depth = queue.depth();
        telemetry::emit_gauge(
            "renderer.command_queue.depth",
            static_cast<std::int64_t>(depth),
            telemetry::RendererTelemetryTags{ backendType, windowId });

        queue.drain();

        SDL_RenderPresent(sdl_renderer.renderer);

        if (sdlcontext.parent && sdlcontext.useFrameLimiter)
        {
            using clock = std::chrono::steady_clock;
            constexpr auto frameDuration = std::chrono::milliseconds(16);
            const auto now = clock::now();
            const auto elapsed = now - sdlcontext.lastFrameTime;
            if (elapsed < frameDuration)
            {
                std::this_thread::sleep_for(frameDuration - elapsed);
            }
            sdlcontext.lastFrameTime = clock::now();
        }

        frameTimer.finish();

        return true;
    }

    inline void sdl_clear()
    {
        const auto color = core::clear_color_for_context(core::ContextType::SDL);
        SDL_SetRenderDrawColor(
            sdl_renderer.renderer,
            static_cast<std::uint8_t>(color[0] * 255.0f),
            static_cast<std::uint8_t>(color[1] * 255.0f),
            static_cast<std::uint8_t>(color[2] * 255.0f),
            static_cast<std::uint8_t>(color[3] * 255.0f));
        SDL_RenderClear(sdl_renderer.renderer);
    }

    inline void sdl_present()
    {
        SDL_RenderPresent(sdl_renderer.renderer);
    }

    inline void sdl_cleanup(std::shared_ptr<almondnamespace::core::Context>& ctx)
    {
        (void)ctx;

        if (sdlcontext.renderer)
        {
            SDL_DestroyRenderer(sdlcontext.renderer);
            sdlcontext.renderer = nullptr;
        }

        if (sdlcontext.window)
        {
            SDL_DestroyWindow(sdlcontext.window);
            sdlcontext.window = nullptr;
        }

        SDL_Quit();
        sdlcontext.running = false;
        state::get_sdl_state().running = false;
        state::get_sdl_state().window.sdl_window = nullptr;
        sdltextures::sdl_renderer = nullptr;
        sdltextures::clear_gpu_atlases();

#if defined(_WIN32)
        sdlcontext.hwnd = nullptr;
        sdlcontext.parent = nullptr;
#endif
    }

    inline void set_window_position_centered()
    {
        if (state::get_sdl_state().window.sdl_window)
        {
            SDL_SetWindowPosition(
                state::get_sdl_state().window.sdl_window,
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED);
        }
    }

    inline void set_window_position(int x, int y)
    {
        if (state::get_sdl_state().window.sdl_window)
            SDL_SetWindowPosition(state::get_sdl_state().window.sdl_window, x, y);
    }

    inline void set_window_fullscreen(bool fullscreen)
    {
        if (state::get_sdl_state().window.sdl_window)
        {
            SDL_SetWindowFullscreen(
                state::get_sdl_state().window.sdl_window,
                fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        }
    }

    inline void set_window_borderless(bool borderless)
    {
        if (state::get_sdl_state().window.sdl_window)
            SDL_SetWindowBordered(state::get_sdl_state().window.sdl_window, !borderless);
    }

    inline void set_window_is_resizable(bool resizable)
    {
        if (state::get_sdl_state().window.sdl_window)
            SDL_SetWindowResizable(state::get_sdl_state().window.sdl_window, resizable);
    }

    inline void set_window_minimized()
    {
        if (state::get_sdl_state().window.sdl_window)
            SDL_MinimizeWindow(state::get_sdl_state().window.sdl_window);
    }

    inline void set_window_size(int width, int height)
    {
        if (state::get_sdl_state().window.sdl_window)
            SDL_SetWindowSize(state::get_sdl_state().window.sdl_window, width, height);
    }

    inline void sdl_set_window_title(const std::string& title)
    {
        if (state::get_sdl_state().window.sdl_window)
            SDL_SetWindowTitle(state::get_sdl_state().window.sdl_window, title.c_str());
    }

    inline std::pair<int, int> get_size() noexcept
    {
        int w = sdlcontext.width;
        int h = sdlcontext.height;

        if (sdlcontext.window)
            SDL_GetWindowSize(sdlcontext.window, &w, &h);

        w = (std::max)(1, w);
        h = (std::max)(1, h);
        return { w, h };
    }

    inline int sdl_get_width()  noexcept { return (std::max)(1, sdlcontext.width); }
    inline int sdl_get_height() noexcept { return (std::max)(1, sdlcontext.height); }

    inline bool SDLIsRunning(std::shared_ptr<core::Context> ctx)
    {
        (void)ctx;
        return sdlcontext.running;
    }

#endif // ALMOND_USING_SDL
} // namespace almondnamespace::sdlcontext
