#include <algorithm>
#include <atomic>
#include <memory>
#include <string>

#include <include/aengine.config.hpp>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#endif

#include "core_context_backends.hpp"

import core.context;
import context.multiplexer;

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
import aengine.gui;
import aengine.input;
import atlas.texture;
import context.commandqueue;
import core.logger;
import image.loader;
import sdl.renderer;
import sdl.textures;

namespace
{
    SDL_Window* s_window = nullptr;
    SDL_Renderer* s_renderer = nullptr;
    bool s_running = false;
    int s_width = 0;
    int s_height = 0;

#if defined(_WIN32)
    HWND s_hostWindow = nullptr;
    HWND s_childWindow = nullptr;
#endif

    std::uint32_t default_add_texture(
        epochnamespace::TextureAtlas&,
        std::string,
        const epochnamespace::ImageData&) noexcept
    {
        return 0u;
    }

    std::uint32_t default_add_atlas(const epochnamespace::TextureAtlas& atlas) noexcept
    {
        const int idx = atlas.get_index();
        return static_cast<std::uint32_t>(idx >= 0 ? idx + 1 : 1);
    }

    void bind_default_input(const std::shared_ptr<epochnamespace::core::Context>& ctx)
    {
        ctx->is_key_held = [](epochnamespace::input::Key key) { return epochnamespace::input::is_key_held(key); };
        ctx->is_key_down = [](epochnamespace::input::Key key) { return epochnamespace::input::is_key_down(key); };
        ctx->get_mouse_position = [](int& x, int& y)
        {
            x = epochnamespace::input::mouseX.load(std::memory_order_relaxed);
            y = epochnamespace::input::mouseY.load(std::memory_order_relaxed);
        };
        ctx->is_mouse_button_held = [](epochnamespace::input::MouseButton button) { return epochnamespace::input::is_mouse_button_held(button); };
        ctx->is_mouse_button_down = [](epochnamespace::input::MouseButton button) { return epochnamespace::input::is_mouse_button_down(button); };
    }

    void request_host_shutdown(const std::shared_ptr<epochnamespace::core::Context>& ctx) noexcept
    {
        s_running = false;
        if (ctx && ctx->windowData)
            ctx->windowData->set_should_close(true);
#if defined(_WIN32)
        HWND closeTarget = s_childWindow;
        if (!closeTarget || ::IsWindow(closeTarget) == FALSE)
            closeTarget = s_hostWindow;
        if (closeTarget && ::IsWindow(closeTarget) != FALSE)
            ::PostMessageW(closeTarget, WM_CLOSE, 0, 0);
#endif
    }

    void refresh_dimensions(const std::shared_ptr<epochnamespace::core::Context>& ctx) noexcept
    {
#if defined(_WIN32)
        if (s_childWindow && ::IsWindow(s_childWindow) != FALSE)
        {
            RECT client{};
            if (::GetClientRect(s_childWindow, &client))
            {
                s_width = (std::max)(1, static_cast<int>(client.right - client.left));
                s_height = (std::max)(1, static_cast<int>(client.bottom - client.top));
            }
        }
        else
#endif
        if (s_window)
            SDL_GetWindowSize(s_window, &s_width, &s_height);

        s_width = (std::max)(1, s_width);
        s_height = (std::max)(1, s_height);

        if (ctx)
        {
            ctx->width = s_width;
            ctx->height = s_height;
            ctx->virtualWidth = s_width;
            ctx->virtualHeight = s_height;
            ctx->framebufferWidth = s_width;
            ctx->framebufferHeight = s_height;
            if (ctx->windowData)
            {
                ctx->windowData->sdl_window = s_window;
                ctx->windowData->width = s_width;
                ctx->windowData->height = s_height;
            }
        }
    }

    void sync_docked_child_size(const std::shared_ptr<epochnamespace::core::Context>& ctx) noexcept
    {
#if defined(_WIN32)
        if (!s_hostWindow || !s_childWindow
            || ::IsWindow(s_hostWindow) == FALSE
            || ::IsWindow(s_childWindow) == FALSE)
            return;

        RECT client{};
        if (!::GetClientRect(s_hostWindow, &client))
            return;

        const int width = (std::max)(1, static_cast<int>(client.right - client.left));
        const int height = (std::max)(1, static_cast<int>(client.bottom - client.top));
        if (width == s_width && height == s_height)
            return;

        s_width = width;
        s_height = height;
        ::SetWindowPos(
            s_childWindow,
            nullptr,
            0,
            0,
            width,
            height,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        refresh_dimensions(ctx);

        if (ctx && ctx->onResize)
            ctx->onResize(width, height);
#else
        (void)ctx;
#endif
    }

    void sdl_initialize_adapter()
    {
        auto ctx = epochnamespace::core::get_current_render_context();
        if (!ctx)
            return;

        s_width = (std::max)(1, ctx->width);
        s_height = (std::max)(1, ctx->height);

#if defined(_WIN32)
        s_hostWindow = ctx->get_hwnd();
#endif

        if (!s_running)
        {
            if (SDL_WasInit(SDL_INIT_VIDEO) == 0 && static_cast<int>(SDL_Init(SDL_INIT_VIDEO)) < 0)
            {
                epochnamespace::logger::error("SDL", std::string("SDL_Init failed: ") + SDL_GetError());
                return;
            }

            SDL_PropertiesID props = SDL_CreateProperties();
            if (!props)
            {
                epochnamespace::logger::error("SDL", std::string("SDL_CreateProperties failed: ") + SDL_GetError());
                return;
            }

            SDL_SetStringProperty(
                props,
                SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                ctx->backendName.empty() ? "SDL" : ctx->backendName.c_str());
            SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, s_width);
            SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, s_height);
            SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);

            s_window = SDL_CreateWindowWithProperties(props);
            SDL_DestroyProperties(props);
            if (!s_window)
            {
                epochnamespace::logger::error("SDL", std::string("SDL_CreateWindowWithProperties failed: ") + SDL_GetError());
                return;
            }

            s_renderer = SDL_CreateRenderer(s_window, nullptr);
            if (!s_renderer)
            {
                epochnamespace::logger::error("SDL", std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
                SDL_DestroyWindow(s_window);
                s_window = nullptr;
                return;
            }

#if defined(_WIN32)
            SDL_PropertiesID windowProps = SDL_GetWindowProperties(s_window);
            if (!windowProps)
            {
                epochnamespace::logger::error("SDL", std::string("SDL_GetWindowProperties failed: ") + SDL_GetError());
                SDL_DestroyRenderer(s_renderer);
                SDL_DestroyWindow(s_window);
                s_renderer = nullptr;
                s_window = nullptr;
                return;
            }

            s_childWindow = static_cast<HWND>(
                SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
            if (!s_childWindow)
            {
                epochnamespace::logger::error("SDL", "Failed to retrieve SDL HWND");
                SDL_DestroyRenderer(s_renderer);
                SDL_DestroyWindow(s_window);
                s_renderer = nullptr;
                s_window = nullptr;
                return;
            }

            if (s_hostWindow && ::IsWindow(s_hostWindow) != FALSE)
            {
                ::SetParent(s_childWindow, s_hostWindow);

                LONG_PTR style = ::GetWindowLongPtrW(s_childWindow, GWL_STYLE);
                style &= ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW);
                style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
                ::SetWindowLongPtrW(s_childWindow, GWL_STYLE, style);

                epochnamespace::core::MakeDockable(s_childWindow, s_hostWindow);

                RECT client{};
                ::GetClientRect(s_hostWindow, &client);
                s_width = (std::max)(1, static_cast<int>(client.right - client.left));
                s_height = (std::max)(1, static_cast<int>(client.bottom - client.top));

                ::SetWindowPos(
                    s_childWindow,
                    nullptr,
                    0,
                    0,
                    s_width,
                    s_height,
                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

                ::ShowWindow(s_hostWindow, SW_SHOWNA);
            }
#endif

            epochnamespace::sdlcontext::init_renderer(s_renderer);
            s_running = true;
        }

        if (!s_window || !s_renderer)
            return;

        if (ctx)
        {
#if defined(_WIN32)
            const HWND liveWindow = s_childWindow ? s_childWindow : s_hostWindow;
            ctx->hwnd = liveWindow;
            ctx->native_window = liveWindow;
#endif
            if (ctx->windowData)
            {
                ctx->windowData->sdl_window = s_window;
#if defined(_WIN32)
                ctx->windowData->hwnd = liveWindow;
                ctx->windowData->host_hwnd = s_hostWindow;
                ctx->windowData->hwndChild = s_childWindow;
                ctx->windowData->hdc = nullptr;
#endif
            }
        }

        refresh_dimensions(ctx);
        sync_docked_child_size(ctx);

#if defined(_WIN32)
        HWND focusWindow = s_childWindow ? s_childWindow : s_hostWindow;
        if (focusWindow && ::IsWindow(focusWindow) != FALSE)
            ::SetFocus(focusWindow);
#endif
    }

    void sdl_cleanup_adapter()
    {
        s_running = false;

        if (s_renderer)
        {
            SDL_DestroyRenderer(s_renderer);
            s_renderer = nullptr;
        }

        if (s_window)
        {
            SDL_DestroyWindow(s_window);
            s_window = nullptr;
        }

        if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
            SDL_QuitSubSystem(SDL_INIT_VIDEO);

#if defined(_WIN32)
        s_hostWindow = nullptr;
        s_childWindow = nullptr;
#endif
        s_width = 0;
        s_height = 0;
    }

    bool sdl_process_adapter(
        std::shared_ptr<epochnamespace::core::Context> ctx,
        epochnamespace::core::CommandQueue& queue)
    {
        if (!ctx || !s_running || !s_window || !s_renderer)
            return false;

#if defined(_WIN32)
        if (s_childWindow && ::IsWindow(s_childWindow) == FALSE)
            return false;
#endif

        SDL_Event event{};
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                request_host_shutdown(ctx);
                return false;
            }

            if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                refresh_dimensions(ctx);
                if (ctx->onResize)
                    ctx->onResize(ctx->framebufferWidth, ctx->framebufferHeight);
            }
        }

        sync_docked_child_size(ctx);
        refresh_dimensions(ctx);

        epochnamespace::sdlcontext::begin_frame();
        SDL_RenderClear(s_renderer);
        (void)queue.drain();
        (void)epochnamespace::gui::render_deferred_batch(ctx.get());
        epochnamespace::sdlcontext::end_frame();
        return s_running;
    }
}

namespace epochnamespace::core::detail
{
    void register_sdl_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::SDL;
        ctx->backendName = "SDL";
        ctx->initialize = sdl_initialize_adapter;
        ctx->cleanup = sdl_cleanup_adapter;
        ctx->process = sdl_process_adapter;
        ctx->clear = nullptr;
        ctx->present = nullptr;
        ctx->get_width = []() { return s_width; };
        ctx->get_height = []() { return s_height; };
        ctx->draw_sprite = epochnamespace::sdltextures::draw_sprite;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const epochnamespace::TextureAtlas& atlas)
        {
            return default_add_atlas(atlas);
        };
        bind_default_input(ctx);
        AddContextForBackend(ContextType::SDL, std::move(ctx));
    }
}
#endif
