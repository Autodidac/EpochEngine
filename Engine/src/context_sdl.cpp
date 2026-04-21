#include <algorithm>
#include <atomic>
#include <cmath>
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
import atlas.manager;
import atlas.texture;
import context.commandqueue;
import core.logger;
import image.loader;
import render.preview_grid;
import sdl.renderer;
import sdl.state;
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
    HWND s_dockParent = nullptr;
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

    [[nodiscard]] Uint8 to_sdl_channel(float value) noexcept
    {
        const float scaled = (std::clamp)(value, 0.0f, 1.0f) * 255.0f;
        return static_cast<Uint8>(scaled);
    }

    [[nodiscard]] bool project_preview_vertex(
        const epochnamespace::previewgrid::Mat4& mvp,
        const epochnamespace::previewgrid::Vec3& position,
        const epochnamespace::core::RenderViewport& viewport,
        float& outX,
        float& outY) noexcept
    {
        const auto clip = epochnamespace::previewgrid::transform_point(mvp, position);
        if (clip.w <= 1.0e-4f)
            return false;

        const float invW = 1.0f / clip.w;
        const float ndcX = clip.x * invW;
        const float ndcY = clip.y * invW;
        if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
            return false;

        outX = static_cast<float>(viewport.x)
            + ((ndcX * 0.5f) + 0.5f) * static_cast<float>(viewport.width);
        outY = static_cast<float>(viewport.y)
            + ((-ndcY * 0.5f) + 0.5f) * static_cast<float>(viewport.height);
        return true;
    }

    void render_scene_preview(const std::shared_ptr<epochnamespace::core::Context>& ctx)
    {
        if (!ctx || !s_renderer)
            return;

        const auto viewport = ctx->scene_viewport();
        if (!viewport.valid() || ctx->scene_preview_mode() != epochnamespace::core::ScenePreviewMode::Editor)
            return;

        SDL_Rect clipRect{ viewport.x, viewport.y, viewport.width, viewport.height };
        (void)SDL_SetRenderClipRect(s_renderer, &clipRect);

        const auto clearColor = epochnamespace::previewgrid::kClearColor;
        const SDL_FRect background{
            static_cast<float>(viewport.x),
            static_cast<float>(viewport.y),
            static_cast<float>(viewport.width),
            static_cast<float>(viewport.height)
        };

        (void)SDL_SetRenderDrawColor(
            s_renderer,
            to_sdl_channel(clearColor[0]),
            to_sdl_channel(clearColor[1]),
            to_sdl_channel(clearColor[2]),
            to_sdl_channel(clearColor[3]));
        (void)SDL_RenderFillRect(s_renderer, &background);

        const auto camera = epochnamespace::previewgrid::camera_for(ctx.get());
        const float aspect = viewport.height > 0
            ? (viewport.width / static_cast<float>(viewport.height))
            : 1.0f;
        const auto proj = epochnamespace::previewgrid::perspective(
            camera.fovRadians,
            aspect,
            camera.nearPlane,
            camera.farPlane);
        const auto view = epochnamespace::previewgrid::look_at(
            camera.eye,
            camera.target,
            camera.up);
        const auto mvp = epochnamespace::previewgrid::multiply(proj, view);
        const auto vertices = epochnamespace::previewgrid::grid_vertices();
        const auto indices = epochnamespace::previewgrid::grid_indices();

        for (std::size_t i = 0; i + 1 < indices.size(); i += 2)
        {
            const auto firstIndex = static_cast<std::size_t>(indices[i]);
            const auto secondIndex = static_cast<std::size_t>(indices[i + 1]);
            if (firstIndex >= vertices.size() || secondIndex >= vertices.size())
                continue;

            float ax = 0.0f;
            float ay = 0.0f;
            float bx = 0.0f;
            float by = 0.0f;
            if (!project_preview_vertex(mvp, vertices[firstIndex].position, viewport, ax, ay)
                || !project_preview_vertex(mvp, vertices[secondIndex].position, viewport, bx, by))
            {
                continue;
            }

            const auto color = vertices[firstIndex].color;
            (void)SDL_SetRenderDrawColor(
                s_renderer,
                to_sdl_channel(color.x),
                to_sdl_channel(color.y),
                to_sdl_channel(color.z),
                255u);
            (void)SDL_RenderLine(s_renderer, ax, ay, bx, by);
        }

        const auto markerVertices = epochnamespace::previewgrid::look_marker_vertices_for(ctx.get());
        const std::size_t markerCount = epochnamespace::previewgrid::look_marker_vertex_count_for(ctx.get());
        for (std::size_t i = 0; i + 1 < markerCount; i += 2)
        {
            float ax = 0.0f;
            float ay = 0.0f;
            float bx = 0.0f;
            float by = 0.0f;
            if (!project_preview_vertex(mvp, markerVertices[i].position, viewport, ax, ay)
                || !project_preview_vertex(mvp, markerVertices[i + 1].position, viewport, bx, by))
            {
                continue;
            }

            const auto color = markerVertices[i].color;
            (void)SDL_SetRenderDrawColor(
                s_renderer,
                to_sdl_channel(color.x),
                to_sdl_channel(color.y),
                to_sdl_channel(color.z),
                255u);
            (void)SDL_RenderLine(s_renderer, ax, ay, bx, by);
        }

        (void)SDL_SetRenderClipRect(s_renderer, nullptr);
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
        else if (s_hostWindow && ::IsWindow(s_hostWindow) != FALSE)
        {
            RECT client{};
            if (::GetClientRect(s_hostWindow, &client))
            {
                s_width = (std::max)(1, static_cast<int>(client.right - client.left));
                s_height = (std::max)(1, static_cast<int>(client.bottom - client.top));
            }
        }
#else
        if (s_window)
        {
            int w = 0;
            int h = 0;
            SDL_GetWindowSize(s_window, &w, &h);
            s_width = (std::max)(1, w);
            s_height = (std::max)(1, h);
        }
#endif

        if (ctx)
        {
            ctx->width = s_width;
            ctx->height = s_height;
            ctx->framebufferWidth = s_width;
            ctx->framebufferHeight = s_height;
            if (ctx->windowData)
            {
                ctx->windowData->sdl_window = s_window;
                ctx->windowData->set_size(s_width, s_height);
            }
        }

        auto& state = epochnamespace::sdlcontext::state::get_sdl_state();
        state.window.sdl_window = s_window;
        state.set_dimensions(s_width, s_height);
    }

    void sync_docked_child_size(const std::shared_ptr<epochnamespace::core::Context>& ctx) noexcept
    {
#if defined(_WIN32)
        if (!s_childWindow || ::IsWindow(s_childWindow) == FALSE)
        {
            return;
        }

        RECT client{};
        UINT positionFlags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW;
        const HWND childParent = ::GetParent(s_childWindow);
        if (s_hostWindow && ::IsWindow(s_hostWindow) != FALSE && childParent == s_hostWindow)
        {
            if (!::GetClientRect(s_hostWindow, &client))
                return;
        }
        else
        {
            if (!::GetClientRect(s_childWindow, &client))
                return;
            positionFlags |= SWP_NOMOVE;
        }

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
            positionFlags);

        if (s_window)
            SDL_SetWindowSize(s_window, width, height);

        refresh_dimensions(ctx);
        if (ctx && ctx->onResize)
            ctx->onResize(width, height);
#else
        (void)ctx;
#endif
    }

    void request_host_shutdown(const std::shared_ptr<epochnamespace::core::Context>& ctx) noexcept
    {
        s_running = false;
        auto& state = epochnamespace::sdlcontext::state::get_sdl_state();
        state.running = false;
        state.mark_should_close(true);
        if (ctx && ctx->windowData)
            ctx->windowData->set_should_close(true);
    }

    void sdl_initialize_adapter()
    {
        auto ctx = epochnamespace::core::get_current_render_context();
        if (!ctx)
            return;

        ctx->init_failed = false;
        s_width = (std::max)(1, ctx->width);
        s_height = (std::max)(1, ctx->height);

#if defined(_WIN32)
        s_hostWindow = ctx->get_hwnd();
#endif

        if (static_cast<int>(SDL_Init(SDL_INIT_VIDEO)) < 0)
        {
            ctx->init_failed = true;
            epochnamespace::logger::error("SDL", std::string("SDL_Init failed: ") + SDL_GetError());
            return;
        }

        const std::string title = (ctx->windowData && !ctx->windowData->titleNarrow.empty())
            ? ctx->windowData->titleNarrow
            : (ctx->backendName.empty() ? "SDL" : ctx->backendName);

        SDL_PropertiesID props = SDL_CreateProperties();
        if (!props)
        {
            ctx->init_failed = true;
            epochnamespace::logger::error("SDL", std::string("SDL_CreateProperties failed: ") + SDL_GetError());
            SDL_Quit();
            return;
        }

        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, s_width);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, s_height);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);

        s_window = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);
        if (!s_window)
        {
            ctx->init_failed = true;
            epochnamespace::logger::error("SDL", std::string("SDL_CreateWindowWithProperties failed: ") + SDL_GetError());
            SDL_Quit();
            return;
        }

        s_renderer = SDL_CreateRenderer(s_window, nullptr);
        if (!s_renderer)
        {
            ctx->init_failed = true;
            epochnamespace::logger::error("SDL", std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
            SDL_DestroyWindow(s_window);
            s_window = nullptr;
            SDL_Quit();
            return;
        }

        epochnamespace::sdlcontext::init_renderer(s_renderer);
        epochnamespace::sdltextures::sdl_renderer = s_renderer;

#if defined(_WIN32)
        SDL_PropertiesID windowProps = SDL_GetWindowProperties(s_window);
        if (!windowProps)
        {
            ctx->init_failed = true;
            epochnamespace::logger::error("SDL", std::string("SDL_GetWindowProperties failed: ") + SDL_GetError());
            SDL_DestroyRenderer(s_renderer);
            SDL_DestroyWindow(s_window);
            s_renderer = nullptr;
            s_window = nullptr;
            SDL_Quit();
            return;
        }

        s_childWindow = static_cast<HWND>(
            SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        if (!s_childWindow)
        {
            ctx->init_failed = true;
            epochnamespace::logger::error("SDL", "Failed to retrieve SDL HWND");
            SDL_DestroyRenderer(s_renderer);
            SDL_DestroyWindow(s_window);
            s_renderer = nullptr;
            s_window = nullptr;
            SDL_Quit();
            return;
        }

        if (s_hostWindow && ::IsWindow(s_hostWindow) != FALSE)
        {
            const HWND dockParent = ::GetParent(s_hostWindow);
            const HWND liveDockParent = dockParent ? dockParent : s_hostWindow;
            s_dockParent = liveDockParent;
            ::SetParent(s_childWindow, liveDockParent);

            LONG_PTR style = ::GetWindowLongPtrW(s_childWindow, GWL_STYLE);
            style &= ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW);
            style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
            ::SetWindowLongPtrW(s_childWindow, GWL_STYLE, style);
            epochnamespace::core::MakeDockable(s_childWindow, liveDockParent);

            RECT client{};
            const HWND sizeSource = s_hostWindow ? s_hostWindow : liveDockParent;
            ::GetClientRect(sizeSource, &client);
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

            // Keep SDL's internal window/backbuffer size aligned with the dock slot
            // before the first present so the pane does not stay blank until resize.
            SDL_SetWindowSize(s_window, s_width, s_height);
            ::RedrawWindow(
                s_childWindow,
                nullptr,
                nullptr,
                RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

            if (s_hostWindow != s_childWindow)
                ::ShowWindow(s_hostWindow, SW_HIDE);
        }

#endif

        refresh_dimensions(ctx);
        if (ctx->onResize)
            ctx->onResize(s_width, s_height);
        if (ctx->windowData)
        {
            HWND previousHwnd = ctx->windowData->hwnd;
            HDC previousHdc = ctx->windowData->hdc;
            if (previousHwnd && previousHwnd != s_childWindow)
            {
                if (previousHdc)
                    ::ReleaseDC(previousHwnd, previousHdc);

                auto& threads = epochnamespace::core::Threads();
                auto it = threads.find(previousHwnd);
                if (it != threads.end())
                {
                    threads.emplace(s_childWindow, std::move(it->second));
                    threads.erase(it);
                }
            }
#if defined(_WIN32)
            ctx->windowData->hwnd = s_childWindow ? s_childWindow : s_hostWindow;
            ctx->windowData->host_hwnd = s_hostWindow;
            ctx->windowData->hwndChild = s_childWindow;
            ctx->windowData->hdc = nullptr;
#endif
            ctx->windowData->sdl_window = s_window;
            ctx->windowData->set_size(s_width, s_height);
        }

        auto& state = epochnamespace::sdlcontext::state::get_sdl_state();
        state.window.sdl_window = s_window;
        state.set_dimensions(s_width, s_height);
        state.mark_should_close(false);
        state.renderFaulted = false;
        state.running = true;

 #if defined(_WIN32)
        ctx->hwnd = s_childWindow ? s_childWindow : s_hostWindow;
        ctx->native_window = s_childWindow ? s_childWindow : s_hostWindow;
        ctx->hdc = nullptr;
        ctx->native_drawable = nullptr;
        ctx->hglrc = nullptr;
        ctx->native_gl_context = nullptr;
        if (s_dockParent && ::IsWindow(s_dockParent) != FALSE)
            ::PostMessageW(s_dockParent, WM_SIZE, 0, 0);
 #endif

        s_running = true;
        SDL_ShowWindow(s_window);
        epochnamespace::atlasmanager::register_backend_uploader(
            epochnamespace::core::ContextType::SDL,
            [](const epochnamespace::TextureAtlas& atlas)
            {
                epochnamespace::sdltextures::ensure_uploaded(atlas);
            });
    }

    void sdl_cleanup_adapter()
    {
        epochnamespace::atlasmanager::unregister_backend_uploader(epochnamespace::core::ContextType::SDL);
        epochnamespace::sdltextures::clear_gpu_atlases();
        epochnamespace::sdltextures::sdl_renderer = nullptr;

        s_running = false;
        auto& state = epochnamespace::sdlcontext::state::get_sdl_state();
        state.running = false;
        state.renderFaulted = false;
        state.mark_should_close(false);
        state.window.sdl_window = nullptr;
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
        SDL_Quit();

#if defined(_WIN32)
        s_hostWindow = nullptr;
        s_childWindow = nullptr;
        s_dockParent = nullptr;
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

        auto& state = epochnamespace::sdlcontext::state::get_sdl_state();
        if (state.renderFaulted || state.shouldClose || state.window.get_should_close())
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

        state.window.sdl_window = s_window;
        state.set_dimensions(s_width, s_height);
        state.running = s_running;

        const auto clearColor = epochnamespace::core::clear_color_for_context(epochnamespace::core::ContextType::SDL);
        (void)SDL_SetRenderDrawColor(
            s_renderer,
            to_sdl_channel(clearColor[0]),
            to_sdl_channel(clearColor[1]),
            to_sdl_channel(clearColor[2]),
            to_sdl_channel(clearColor[3]));
        SDL_RenderClear(s_renderer);

        epochnamespace::atlasmanager::process_pending_uploads(epochnamespace::core::ContextType::SDL);
        render_scene_preview(ctx);
        (void)queue.drain();
        (void)epochnamespace::gui::render_deferred_batch(ctx.get());
        epochnamespace::sdlcontext::end_frame();
        if (state.renderFaulted)
        {
            return false;
        }
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
