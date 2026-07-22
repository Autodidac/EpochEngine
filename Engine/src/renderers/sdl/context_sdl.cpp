module;

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <string>

#include <include/engine.config.hpp>

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

module core.context;

import context.multiplexer;

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
import engine.gui;
import engine.input;
import atlas.manager;
import atlas.texture;
import context.commandqueue;
import context.type;
import core.logger;
import image.loader;
import render.preview_grid;
import package.registry;
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
        epochengine::TextureAtlas&,
        std::string,
        const epochengine::ImageData&) noexcept
    {
        return 0u;
    }

    std::uint32_t default_add_atlas(const epochengine::TextureAtlas& atlas) noexcept
    {
        const int idx = atlas.get_index();
        return static_cast<std::uint32_t>(idx >= 0 ? idx + 1 : 1);
    }

    void bind_default_input(const std::shared_ptr<epochengine::core::Context>& ctx)
    {
        ctx->is_key_held = [](epochengine::input::Key key) { return epochengine::input::is_key_held(key); };
        ctx->is_key_down = [](epochengine::input::Key key) { return epochengine::input::is_key_down(key); };
        ctx->get_mouse_position = [](int& x, int& y)
        {
            x = epochengine::input::mouseX.load(std::memory_order_relaxed);
            y = epochengine::input::mouseY.load(std::memory_order_relaxed);
        };
        ctx->is_mouse_button_held = [](epochengine::input::MouseButton button) { return epochengine::input::is_mouse_button_held(button); };
        ctx->is_mouse_button_down = [](epochengine::input::MouseButton button) { return epochengine::input::is_mouse_button_down(button); };
    }

    [[nodiscard]] Uint8 to_sdl_channel(float value) noexcept
    {
        const float scaled = (std::clamp)(value, 0.0f, 1.0f) * 255.0f;
        return static_cast<Uint8>(scaled);
    }

    [[nodiscard]] bool project_preview_vertex(
        const epochengine::previewgrid::Mat4& mvp,
        const epochengine::previewgrid::Vec3& position,
        const epochengine::core::RenderViewport& viewport,
        float& outX,
        float& outY) noexcept
    {
        const auto clip = epochengine::previewgrid::transform_point(mvp, position);
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



    struct SdlArcadeScreenPreviewTarget
    {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
        std::uint64_t frame = 0;
    };

    SdlArcadeScreenPreviewTarget& arcade_screen_preview_target() noexcept
    {
        static SdlArcadeScreenPreviewTarget target{};
        return target;
    }

    void destroy_arcade_screen_preview_target() noexcept
    {
        SdlArcadeScreenPreviewTarget& target = arcade_screen_preview_target();
        if (target.texture)
            SDL_DestroyTexture(target.texture);
        target = {};
    }

    bool ensure_arcade_screen_preview_target(SDL_Renderer* renderer) noexcept
    {
        if (!renderer)
            return false;

        SdlArcadeScreenPreviewTarget& target = arcade_screen_preview_target();
        const int width = static_cast<int>(epochengine::package_registry::engine_arcade_render_texture_width());
        const int height = static_cast<int>(epochengine::package_registry::engine_arcade_render_texture_height());
        if (width <= 0 || height <= 0)
            return false;

        if (target.texture && target.width == width && target.height == height)
            return true;

        destroy_arcade_screen_preview_target();
        target.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, width, height);
        if (!target.texture)
        {
            epochengine::sdlcontext::check_sdl_error("SDL_CreateTexture engine_arcade.screen");
            return false;
        }

        (void)SDL_SetTextureBlendMode(target.texture, SDL_BLENDMODE_BLEND);
        (void)SDL_SetTextureScaleMode(target.texture, SDL_SCALEMODE_NEAREST);
        target.width = width;
        target.height = height;
        return true;
    }

    void fill_arcade_preview_rect(SDL_Renderer* renderer, int x, int y, int width, int height, Uint8 r, Uint8 g, Uint8 b) noexcept
    {
        if (!renderer || width <= 0 || height <= 0)
            return;
        const SDL_FRect rect{ static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height) };
        (void)SDL_SetRenderDrawColor(renderer, r, g, b, 255u);
        (void)SDL_RenderFillRect(renderer, &rect);
    }

    void render_arcade_attract_pattern(SDL_Renderer* renderer) noexcept
    {
        SdlArcadeScreenPreviewTarget& target = arcade_screen_preview_target();
        if (!renderer || !target.texture || target.width <= 0 || target.height <= 0)
            return;

        SDL_Texture* previousTarget = SDL_GetRenderTarget(renderer);
        if (!SDL_SetRenderTarget(renderer, target.texture))
        {
            epochengine::sdlcontext::check_sdl_error("SDL_SetRenderTarget engine_arcade.screen");
            epochengine::sdlcontext::state::get_sdl_state().renderFaulted = true;
            return;
        }

        const int width = target.width;
        const int height = target.height;
        (void)SDL_SetRenderDrawColor(renderer, 4u, 6u, 11u, 255u);
        (void)SDL_RenderClear(renderer);
        fill_arcade_preview_rect(renderer, 18, 18, width - 36, height - 36, 6u, 19u, 23u);
        fill_arcade_preview_rect(renderer, 24, 24, width - 48, 6, 18u, 242u, 158u);
        fill_arcade_preview_rect(renderer, 24, height - 30, width - 48, 6, 18u, 242u, 158u);
        fill_arcade_preview_rect(renderer, 24, 24, 6, height - 48, 18u, 242u, 158u);
        fill_arcade_preview_rect(renderer, width - 30, 24, 6, height - 48, 18u, 242u, 158u);
        for (int y = 52; y < height - 52; y += 32)
        {
            const Uint8 tone = ((y / 32) % 2 == 0) ? 13u : 9u;
            fill_arcade_preview_rect(renderer, 44, y, width - 88, 3, tone, static_cast<Uint8>(tone + 9u), static_cast<Uint8>(tone + 17u));
        }
        const int cell = (std::max)(14, width / 24);
        const int playLeft = 72;
        const int playBottom = 92;
        const int playWidth = width - 144;
        const int playHeight = height - 184;
        const int frame = static_cast<int>(target.frame++ % 240u);
        const int phase = frame / 12;
        const int headColumn = phase % (std::max)(1, playWidth / cell);
        const int lane = (phase / 5) % 6;
        const int headY = playBottom + lane * cell;
        for (int i = 0; i < 9; ++i)
        {
            const int segment = (std::max)(0, headColumn - i);
            const int sx = playLeft + segment * cell;
            const int sy = headY - ((i / 5) * cell);
            const Uint8 green = static_cast<Uint8>((std::max)(51, 230 - i * 17));
            fill_arcade_preview_rect(renderer, sx, sy, cell - 3, cell - 3, 20u, green, 122u);
        }
        const int fruitX = playLeft + ((phase * 5 + 7) % (std::max)(1, playWidth / cell)) * cell;
        const int fruitY = playBottom + ((phase * 3 + 2) % (std::max)(1, playHeight / cell)) * cell;
        fill_arcade_preview_rect(renderer, fruitX, fruitY, cell, cell, 245u, 71u, 51u);
        fill_arcade_preview_rect(renderer, fruitX + 3, fruitY + 3, cell - 6, cell - 6, 255u, 209u, 64u);
        const int pulse = 16 + (frame % 48);
        fill_arcade_preview_rect(renderer, width / 2 - 112, height - 82, 224, 10, 26u, 89u, 184u);
        fill_arcade_preview_rect(renderer, width / 2 - 112, height - 82, (std::min)(224, pulse * 5), 10, 66u, 209u, 255u);

        if (!SDL_SetRenderTarget(renderer, previousTarget))
        {
            epochengine::sdlcontext::check_sdl_error("SDL_SetRenderTarget engine_arcade.screen restore");
            epochengine::sdlcontext::state::get_sdl_state().renderFaulted = true;
        }
    }

    void render_engine_arcade_sampled_surface_preview(
        const std::shared_ptr<epochengine::core::Context>& ctx,
        const epochengine::previewgrid::Mat4& mvp,
        const epochengine::core::RenderViewport& viewport) noexcept
    {
        if (!ctx || !s_renderer)
            return;
        const auto markers = epochengine::previewgrid::sampled_render_surface_markers_for(ctx.get());
        if (markers.empty() || !ensure_arcade_screen_preview_target(s_renderer))
            return;

        render_arcade_attract_pattern(s_renderer);
        SdlArcadeScreenPreviewTarget& target = arcade_screen_preview_target();
        for (const auto& marker : markers)
        {
            const float halfX = (std::max)(std::abs(marker.scale.x) * 0.5f, 0.25f);
            const float halfY = (std::max)(std::abs(marker.scale.y) * 0.5f, 0.18f);
            const float z = marker.position.z - (std::max)(std::abs(marker.scale.z) * 0.5f, 0.018f) - 0.012f;
            const epochengine::previewgrid::Vec3 world[4]{
                { marker.position.x - halfX, marker.position.y - halfY, z },
                { marker.position.x + halfX, marker.position.y - halfY, z },
                { marker.position.x + halfX, marker.position.y + halfY, z },
                { marker.position.x - halfX, marker.position.y + halfY, z }
            };
            SDL_Vertex vertices[4]{};
            const SDL_FPoint uvs[4]{ {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f} };
            bool visible = true;
            for (int i = 0; i < 4; ++i)
            {
                float x = 0.0f;
                float y = 0.0f;
                if (!project_preview_vertex(mvp, world[i], viewport, x, y))
                {
                    visible = false;
                    break;
                }
                vertices[i].position = SDL_FPoint{ x, y };
                vertices[i].color = SDL_FColor{ 1.0f, 1.0f, 1.0f, 1.0f };
                vertices[i].tex_coord = uvs[i];
            }
            if (!visible)
                continue;
            const int indices[6]{ 0, 1, 2, 0, 2, 3 };
            if (!SDL_RenderGeometry(s_renderer, target.texture, vertices, 4, indices, 6))
            {
                epochengine::sdlcontext::check_sdl_error("SDL_RenderGeometry engine_arcade.screen");
                epochengine::sdlcontext::state::get_sdl_state().renderFaulted = true;
                return;
            }
        }
    }
    void render_scene_preview(const std::shared_ptr<epochengine::core::Context>& ctx)
    {
        if (!ctx || !s_renderer)
            return;

        const auto viewport = ctx->scene_viewport();
        if (!viewport.valid() || ctx->scene_preview_mode() != epochengine::core::ScenePreviewMode::Editor)
            return;

        SDL_Rect clipRect{ viewport.x, viewport.y, viewport.width, viewport.height };
        (void)SDL_SetRenderClipRect(s_renderer, &clipRect);

        const auto clearColor = epochengine::previewgrid::kClearColor;
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

        const auto camera = epochengine::previewgrid::camera_for(ctx.get());
        const float aspect = viewport.height > 0
            ? (viewport.width / static_cast<float>(viewport.height))
            : 1.0f;
        const auto proj = epochengine::previewgrid::projection_for(ctx.get(), aspect, camera);
        const auto view = epochengine::previewgrid::look_at(
            camera.eye,
            camera.target,
            camera.up);
        const auto mvp = epochengine::previewgrid::multiply(proj, view);
        auto draw_projected_line = [&](const auto& aVertex, const auto& bVertex) noexcept
        {
            float ax = 0.0f;
            float ay = 0.0f;
            float bx = 0.0f;
            float by = 0.0f;
            if (!project_preview_vertex(mvp, aVertex.position, viewport, ax, ay)
                || !project_preview_vertex(mvp, bVertex.position, viewport, bx, by))
            {
                return;
            }

            const auto color = aVertex.color;
            (void)SDL_SetRenderDrawColor(
                s_renderer,
                to_sdl_channel(color.x),
                to_sdl_channel(color.y),
                to_sdl_channel(color.z),
                255u);
            (void)SDL_RenderLine(s_renderer, ax, ay, bx, by);
        };

        const auto vertices = epochengine::previewgrid::grid_vertices();
        const auto indices = epochengine::previewgrid::grid_indices();

        for (std::size_t i = 0; i + 1 < indices.size(); i += 2)
        {
            const auto firstIndex = static_cast<std::size_t>(indices[i]);
            const auto secondIndex = static_cast<std::size_t>(indices[i + 1]);
            if (firstIndex >= vertices.size() || secondIndex >= vertices.size())
                continue;

            draw_projected_line(vertices[firstIndex], vertices[secondIndex]);
        }

        render_engine_arcade_sampled_surface_preview(ctx, mvp, viewport);

        const auto markerVertices = epochengine::previewgrid::look_marker_vertices_for(ctx.get());
        const std::size_t markerCount = epochengine::previewgrid::look_marker_vertex_count_for(ctx.get());
        for (std::size_t i = 0; i + 1 < markerCount; i += 2)
        {
            draw_projected_line(markerVertices[i], markerVertices[i + 1]);
        }

        const auto objectVertices = epochengine::previewgrid::object_marker_vertices_for(ctx.get());
        for (std::size_t i = 0; i + 1 < objectVertices.size(); i += 2)
        {
            draw_projected_line(objectVertices[i], objectVertices[i + 1]);
        }

        (void)SDL_SetRenderClipRect(s_renderer, nullptr);
    }

    void refresh_dimensions(const std::shared_ptr<epochengine::core::Context>& ctx) noexcept
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

        auto& state = epochengine::sdlcontext::state::get_sdl_state();
        state.window.sdl_window = s_window;
        state.set_dimensions(s_width, s_height);
    }

    void sync_docked_child_size(const std::shared_ptr<epochengine::core::Context>& ctx) noexcept
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

    void request_host_shutdown(const std::shared_ptr<epochengine::core::Context>& ctx) noexcept
    {
        s_running = false;
        auto& state = epochengine::sdlcontext::state::get_sdl_state();
        state.running = false;
        state.mark_should_close(true);
        if (ctx && ctx->windowData)
            ctx->windowData->set_should_close(true);
    }

    void sdl_initialize_adapter()
    {
        auto ctx = epochengine::core::get_current_render_context();
        if (!ctx)
            return;

        ctx->init_failed = false;
        s_width = (std::max)(1, ctx->width);
        s_height = (std::max)(1, ctx->height);

#if defined(_WIN32)
        s_hostWindow = ctx->get_hwnd();
        const bool hostedWindow =
            s_hostWindow
            && ::IsWindow(s_hostWindow) != FALSE;
#endif

        if (static_cast<int>(SDL_Init(SDL_INIT_VIDEO)) < 0)
        {
            ctx->init_failed = true;
            epochengine::logger::error("SDL", std::string("SDL_Init failed: ") + SDL_GetError());
            return;
        }

        const std::string title = (ctx->windowData && !ctx->windowData->titleNarrow.empty())
            ? ctx->windowData->titleNarrow
            : (ctx->backendName.empty() ? "SDL" : ctx->backendName);

        SDL_PropertiesID props = SDL_CreateProperties();
        if (!props)
        {
            ctx->init_failed = true;
            epochengine::logger::error("SDL", std::string("SDL_CreateProperties failed: ") + SDL_GetError());
            SDL_Quit();
            return;
        }

        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, s_width);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, s_height);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
#if defined(_WIN32)
        if (hostedWindow)
            SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
#endif

        s_window = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);
        if (!s_window)
        {
            ctx->init_failed = true;
            epochengine::logger::error("SDL", std::string("SDL_CreateWindowWithProperties failed: ") + SDL_GetError());
            SDL_Quit();
            return;
        }

        s_renderer = SDL_CreateRenderer(s_window, nullptr);
        if (!s_renderer)
        {
            ctx->init_failed = true;
            epochengine::logger::error("SDL", std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
            SDL_DestroyWindow(s_window);
            s_window = nullptr;
            SDL_Quit();
            return;
        }

        epochengine::sdlcontext::init_renderer(s_renderer);
        epochengine::sdltextures::sdl_renderer = s_renderer;

#if defined(_WIN32)
        SDL_PropertiesID windowProps = SDL_GetWindowProperties(s_window);
        if (!windowProps)
        {
            ctx->init_failed = true;
            epochengine::logger::error("SDL", std::string("SDL_GetWindowProperties failed: ") + SDL_GetError());
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
            epochengine::logger::error("SDL", "Failed to retrieve SDL HWND");
            SDL_DestroyRenderer(s_renderer);
            SDL_DestroyWindow(s_window);
            s_renderer = nullptr;
            s_window = nullptr;
            SDL_Quit();
            return;
        }

        if (hostedWindow)
        {
            const HWND dockParent = ::GetParent(s_hostWindow);
            if (dockParent && ::IsWindow(dockParent) != FALSE)
            {
                s_dockParent = dockParent;
                ::SetParent(s_childWindow, dockParent);

                LONG_PTR style = ::GetWindowLongPtrW(s_childWindow, GWL_STYLE);
                style &= ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW);
                style |= WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
                ::SetWindowLongPtrW(s_childWindow, GWL_STYLE, style);
                epochengine::core::MakeDockable(s_childWindow, dockParent);

                RECT client{};
                ::GetClientRect(s_hostWindow, &client);
                s_width = (std::max)(1, static_cast<int>(client.right - client.left));
                s_height = (std::max)(1, static_cast<int>(client.bottom - client.top));

                // Keep SDL's internal window/backbuffer size aligned with the dock slot
                // before showing so the pane does not flash as a top-level window.
                SDL_SetWindowSize(s_window, s_width, s_height);
                ::SetWindowPos(
                    s_childWindow,
                    nullptr,
                    0,
                    0,
                    s_width,
                    s_height,
                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
                ::RedrawWindow(
                    s_childWindow,
                    nullptr,
                    nullptr,
                    RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

                if (s_hostWindow != s_childWindow)
                    ::ShowWindow(s_hostWindow, SW_HIDE);
            }
            else
            {
                s_dockParent = nullptr;
                LONG_PTR style = ::GetWindowLongPtrW(s_childWindow, GWL_STYLE);
                style &= ~static_cast<LONG_PTR>(WS_CHILD);
                style |= WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
                ::SetWindowLongPtrW(s_childWindow, GWL_STYLE, style);
                ::SetWindowPos(
                    s_childWindow,
                    nullptr,
                    0,
                    0,
                    0,
                    0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

                if (s_hostWindow != s_childWindow)
                    ::ShowWindow(s_hostWindow, SW_HIDE);
            }
        }

#endif

        refresh_dimensions(ctx);
        if (ctx->onResize)
            ctx->onResize(s_width, s_height);
        if (ctx->windowData)
        {
#if defined(_WIN32)
            HWND previousHwnd = ctx->windowData->hwnd;
            HDC previousHdc = ctx->windowData->hdc;
            if (previousHwnd && previousHwnd != s_childWindow)
            {
                ctx->windowData->ownsNativeDc = false;
                if (previousHdc)
                    ::ReleaseDC(previousHwnd, previousHdc);
            }
            ctx->windowData->hwnd = s_childWindow ? s_childWindow : s_hostWindow;
            ctx->windowData->host_hwnd = s_hostWindow;
            ctx->windowData->hwndChild = s_childWindow;
            ctx->windowData->hdc = nullptr;
            ctx->windowData->ownsNativeDc = false;
            ctx->windowData->ownsNativeGlContext = false;
#endif
            ctx->windowData->sdl_window = s_window;
            ctx->windowData->set_size(s_width, s_height);
        }

        auto& state = epochengine::sdlcontext::state::get_sdl_state();
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
        epochengine::atlasmanager::register_backend_uploader(
            epochengine::core::ContextType::SDL,
            [](const epochengine::TextureAtlas& atlas)
            {
                epochengine::sdltextures::ensure_uploaded(atlas);
            });
    }

    void sdl_cleanup_adapter()
    {
        epochengine::atlasmanager::unregister_backend_uploader(epochengine::core::ContextType::SDL);
        epochengine::sdltextures::clear_gpu_atlases();
        epochengine::sdltextures::sdl_renderer = nullptr;

        s_running = false;
        auto& state = epochengine::sdlcontext::state::get_sdl_state();
        state.running = false;
        state.renderFaulted = false;
        state.mark_should_close(false);
        state.window.sdl_window = nullptr;
#if defined(_WIN32)
        if (s_childWindow && ::IsWindow(s_childWindow) != FALSE)
        {
            ::ShowWindow(s_childWindow, SW_HIDE);
            if (::GetParent(s_childWindow))
            {
                LONG_PTR style = ::GetWindowLongPtrW(s_childWindow, GWL_STYLE);
                style &= ~static_cast<LONG_PTR>(WS_CHILD);
                style |= WS_POPUP;
                ::SetWindowLongPtrW(s_childWindow, GWL_STYLE, style);
                ::SetParent(s_childWindow, nullptr);
                ::SetWindowPos(
                    s_childWindow,
                    nullptr,
                    0,
                    0,
                    0,
                    0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_HIDEWINDOW);
            }
        }
        if (s_hostWindow && s_hostWindow != s_childWindow && ::IsWindow(s_hostWindow) != FALSE)
            ::ShowWindow(s_hostWindow, SW_HIDE);
#endif
        destroy_arcade_screen_preview_target();

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
        std::shared_ptr<epochengine::core::Context> ctx,
        epochengine::core::CommandQueue& queue)
    {
        if (!ctx || !s_running || !s_window || !s_renderer)
            return false;

        auto& state = epochengine::sdlcontext::state::get_sdl_state();
        const bool closeRequested =
            state.renderFaulted
            || state.shouldClose
            || state.window.get_should_close()
            || (ctx->windowData && ctx->windowData->get_should_close());
        if (closeRequested)
        {
            request_host_shutdown(ctx);
            queue.clear();
            return false;
        }

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

        const auto clearColor = epochengine::core::clear_color_for_context(epochengine::core::ContextType::SDL);
        (void)SDL_SetRenderDrawColor(
            s_renderer,
            to_sdl_channel(clearColor[0]),
            to_sdl_channel(clearColor[1]),
            to_sdl_channel(clearColor[2]),
            to_sdl_channel(clearColor[3]));
        SDL_RenderClear(s_renderer);

        epochengine::atlasmanager::process_pending_uploads(epochengine::core::ContextType::SDL);
        (void)queue.drain();
        (void)epochengine::gui::render_deferred_batch(ctx.get());
        render_scene_preview(ctx);
        (void)epochengine::gui::render_top_layer_batch(ctx.get());
        epochengine::sdlcontext::end_frame();
        if (state.renderFaulted)
        {
            return false;
        }
        return s_running;
    }
}

namespace epochengine::core::detail
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
        ctx->draw_sprite = epochengine::sdltextures::draw_sprite;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const epochengine::TextureAtlas& atlas)
        {
            return default_add_atlas(atlas);
        };
        bind_default_input(ctx);
        AddContextForBackend(ContextType::SDL, std::move(ctx));
    }
}

#endif
