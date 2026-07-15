module;

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
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
#  include <wingdi.h>
#endif

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
#include "../modules/sfml.compat.hpp"
#endif

module core.context;

import context.multiplexer;
import context.type;

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
import engine.gui;
import engine.input;
import atlas.manager;
import atlas.texture;
import context.commandqueue;
import core.logger;
import image.loader;
import render.preview_grid;
import sfml.state;
import sfml.textures;

namespace
{
    std::unique_ptr<sf::RenderWindow> s_window{};
    int s_width = 0;
    int s_height = 0;
    bool s_logged_activate_failure = false;

#if defined(_WIN32)
    HWND s_hostWindow = nullptr;
    HWND s_childWindow = nullptr;
    HWND s_dockParent = nullptr;
    HDC s_hdc = nullptr;
    HGLRC s_glContext = nullptr;
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

    [[nodiscard]] sf::Color to_sfml_color(const epochnamespace::previewgrid::Vec3& color) noexcept
    {
        const auto clamp_channel = [](float value) noexcept -> std::uint8_t
        {
            const float scaled = (std::clamp)(value, 0.0f, 1.0f) * 255.0f;
            return static_cast<std::uint8_t>(scaled);
        };

        return sf::Color(
            clamp_channel(color.x),
            clamp_channel(color.y),
            clamp_channel(color.z));
    }

    [[nodiscard]] bool project_preview_vertex(
        const epochnamespace::previewgrid::Mat4& mvp,
        const epochnamespace::previewgrid::Vec3& position,
        const epochnamespace::core::RenderViewport& viewport,
        sf::Vector2f& out) noexcept
    {
        const auto clip = epochnamespace::previewgrid::transform_point(mvp, position);
        if (clip.w <= 1.0e-4f)
            return false;

        const float invW = 1.0f / clip.w;
        const float ndcX = clip.x * invW;
        const float ndcY = clip.y * invW;
        if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
            return false;

        out.x = static_cast<float>(viewport.x)
            + ((ndcX * 0.5f) + 0.5f) * static_cast<float>(viewport.width);
        out.y = static_cast<float>(viewport.y)
            + ((-ndcY * 0.5f) + 0.5f) * static_cast<float>(viewport.height);
        return true;
    }

    void render_scene_preview(const std::shared_ptr<epochnamespace::core::Context>& ctx)
    {
        if (!ctx || !s_window)
            return;

        const auto viewport = ctx->scene_viewport();
        if (!viewport.valid() || ctx->scene_preview_mode() != epochnamespace::core::ScenePreviewMode::Editor)
            return;

        const auto windowSize = s_window->getSize();
        if (windowSize.x == 0u || windowSize.y == 0u)
            return;

        const float invWidth = 1.0f / static_cast<float>(windowSize.x);
        const float invHeight = 1.0f / static_cast<float>(windowSize.y);
        const float viewportLeft = (std::clamp)(viewport.x * invWidth, 0.0f, 1.0f);
        const float viewportTop = (std::clamp)(viewport.y * invHeight, 0.0f, 1.0f);
        const float viewportWidth = (std::clamp)(viewport.width * invWidth, 0.0f, 1.0f - viewportLeft);
        const float viewportHeight = (std::clamp)(viewport.height * invHeight, 0.0f, 1.0f - viewportTop);

        const auto previousView = s_window->getView();
        sf::View previewView{ epoch::sfml_compat::float_rect(
            0.0f,
            0.0f,
            static_cast<float>(viewport.width),
            static_cast<float>(viewport.height)) };
        previewView.setViewport(epoch::sfml_compat::float_rect(
            viewportLeft,
            viewportTop,
            viewportWidth,
            viewportHeight));
        s_window->setView(previewView);

        const auto clearColor = epochnamespace::previewgrid::kClearColor;
        sf::RectangleShape background{};
        background.setPosition(sf::Vector2f(0.0f, 0.0f));
        background.setSize(sf::Vector2f(
            static_cast<float>(viewport.width),
            static_cast<float>(viewport.height)));
        background.setFillColor(sf::Color(
            static_cast<std::uint8_t>(clearColor[0] * 255.0f),
            static_cast<std::uint8_t>(clearColor[1] * 255.0f),
            static_cast<std::uint8_t>(clearColor[2] * 255.0f),
            static_cast<std::uint8_t>(clearColor[3] * 255.0f)));
        sf::RenderStates renderStates{};
        s_window->draw(background, renderStates);

        const auto camera = epochnamespace::previewgrid::camera_for(ctx.get());
        const float aspect = viewport.height > 0
            ? (viewport.width / static_cast<float>(viewport.height))
            : 1.0f;
        const auto proj = epochnamespace::previewgrid::projection_for(ctx.get(), aspect, camera);
        const auto view = epochnamespace::previewgrid::look_at(
            camera.eye,
            camera.target,
            camera.up);
        const auto mvp = epochnamespace::previewgrid::multiply(proj, view);
        const auto vertices = epochnamespace::previewgrid::grid_vertices();
        const auto indices = epochnamespace::previewgrid::grid_indices();
        sf::VertexArray lines(sf::PrimitiveType::Lines);

        for (std::size_t i = 0; i + 1 < indices.size(); i += 2)
        {
            const auto firstIndex = static_cast<std::size_t>(indices[i]);
            const auto secondIndex = static_cast<std::size_t>(indices[i + 1]);
            if (firstIndex >= vertices.size() || secondIndex >= vertices.size())
                continue;

            sf::Vector2f a{};
            sf::Vector2f b{};
            if (!project_preview_vertex(mvp, vertices[firstIndex].position, viewport, a)
                || !project_preview_vertex(mvp, vertices[secondIndex].position, viewport, b))
            {
                continue;
            }

            a.x -= static_cast<float>(viewport.x);
            a.y -= static_cast<float>(viewport.y);
            b.x -= static_cast<float>(viewport.x);
            b.y -= static_cast<float>(viewport.y);

            lines.append(sf::Vertex(a, to_sfml_color(vertices[firstIndex].color)));
            lines.append(sf::Vertex(b, to_sfml_color(vertices[firstIndex].color)));
        }

        if (lines.getVertexCount() > 0)
            s_window->draw(lines, renderStates);

        const auto markerVertices = epochnamespace::previewgrid::look_marker_vertices_for(ctx.get());
        const std::size_t markerCount = epochnamespace::previewgrid::look_marker_vertex_count_for(ctx.get());
        if (markerCount > 0)
        {
            sf::VertexArray markerLines(sf::PrimitiveType::Lines);
            for (std::size_t i = 0; i + 1 < markerCount; i += 2)
            {
                sf::Vector2f a{};
                sf::Vector2f b{};
                if (!project_preview_vertex(mvp, markerVertices[i].position, viewport, a)
                    || !project_preview_vertex(mvp, markerVertices[i + 1].position, viewport, b))
                {
                    continue;
                }

                a.x -= static_cast<float>(viewport.x);
                a.y -= static_cast<float>(viewport.y);
                b.x -= static_cast<float>(viewport.x);
                b.y -= static_cast<float>(viewport.y);

                markerLines.append(sf::Vertex(a, to_sfml_color(markerVertices[i].color)));
                markerLines.append(sf::Vertex(b, to_sfml_color(markerVertices[i].color)));
            }

            if (markerLines.getVertexCount() > 0)
                s_window->draw(markerLines, renderStates);
        }

        const auto objectVertices = epochnamespace::previewgrid::object_marker_vertices_for(ctx.get());
        if (!objectVertices.empty())
        {
            sf::VertexArray objectLines(sf::PrimitiveType::Lines);
            for (std::size_t i = 0; i + 1 < objectVertices.size(); i += 2)
            {
                sf::Vector2f a{};
                sf::Vector2f b{};
                if (!project_preview_vertex(mvp, objectVertices[i].position, viewport, a)
                    || !project_preview_vertex(mvp, objectVertices[i + 1].position, viewport, b))
                {
                    continue;
                }

                a.x -= static_cast<float>(viewport.x);
                a.y -= static_cast<float>(viewport.y);
                b.x -= static_cast<float>(viewport.x);
                b.y -= static_cast<float>(viewport.y);

                objectLines.append(sf::Vertex(a, to_sfml_color(objectVertices[i].color)));
                objectLines.append(sf::Vertex(b, to_sfml_color(objectVertices[i].color)));
            }

            if (objectLines.getVertexCount() > 0)
                s_window->draw(objectLines, renderStates);
        }

        s_window->setView(previousView);
    }

    void apply_view_size() noexcept
    {
        if (!s_window)
            return;

        const sf::Vector2u size(
            static_cast<unsigned>((std::max)(1, s_width)),
            static_cast<unsigned>((std::max)(1, s_height)));
        s_window->setSize(size);
        s_window->setView(sf::View(epoch::sfml_compat::float_rect(
            0.0f,
            0.0f,
            static_cast<float>(size.x),
            static_cast<float>(size.y))));
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
            const auto size = s_window->getSize();
            s_width = (std::max)(1, static_cast<int>(size.x));
            s_height = (std::max)(1, static_cast<int>(size.y));
        }
#endif

        if (ctx)
        {
            ctx->width = s_width;
            ctx->height = s_height;
            ctx->framebufferWidth = s_width;
            ctx->framebufferHeight = s_height;
            if (ctx->windowData)
                ctx->windowData->set_size(s_width, s_height);
        }

        epochnamespace::sfmlcontext::state::s_sfmlstate.set_dimensions(s_width, s_height);
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
            s_window->setSize(sf::Vector2u(static_cast<unsigned>(width), static_cast<unsigned>(height)));

        apply_view_size();
        refresh_dimensions(ctx);
        if (ctx && ctx->onResize)
            ctx->onResize(width, height);
#else
        (void)ctx;
#endif
    }

    void request_host_shutdown(const std::shared_ptr<epochnamespace::core::Context>& ctx) noexcept
    {
        epochnamespace::sfmlcontext::state::s_sfmlstate.mark_should_close(true);
        epochnamespace::sfmlcontext::state::s_sfmlstate.running = false;
        if (ctx && ctx->windowData)
            ctx->windowData->set_should_close(true);
    }

    void sfml_initialize_adapter()
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

        const std::string title = (ctx->windowData && !ctx->windowData->titleNarrow.empty())
            ? ctx->windowData->titleNarrow
            : (ctx->backendName.empty() ? "SFML" : ctx->backendName);

        s_window = epoch::sfml_compat::make_render_window(
            epoch::sfml_compat::video_mode(
                static_cast<unsigned>(s_width),
                static_cast<unsigned>(s_height),
                32u),
            title,
            sf::ContextSettings{});

        if (!s_window || !s_window->isOpen())
        {
            ctx->init_failed = true;
            return;
        }

        s_window->setVerticalSyncEnabled(false);
        s_window->setFramerateLimit(0);
        s_window->setKeyRepeatEnabled(false);
        (void)s_window->setActive(true);
        (void)s_window->setActive(false);

#if defined(_WIN32)
        s_childWindow = static_cast<HWND>(epoch::sfml_compat::native_handle(*s_window));
        if (!s_childWindow)
        {
            ctx->init_failed = true;
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

            // Keep SFML's render target size aligned with the dock slot before the
            // first display so startup does not depend on a later resize event.
            s_window->setSize(sf::Vector2u(static_cast<unsigned>(s_width), static_cast<unsigned>(s_height)));
            ::RedrawWindow(
                s_childWindow,
                nullptr,
                nullptr,
                RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

            if (s_hostWindow != s_childWindow)
                ::ShowWindow(s_hostWindow, SW_HIDE);
        }

        if (!s_window->setActive(true))
        {
            ctx->init_failed = true;
            return;
        }

        s_hdc = s_childWindow ? ::GetDC(s_childWindow) : nullptr;
        s_glContext = ::wglGetCurrentContext();
        (void)s_window->setActive(false);

        const HWND primaryWindow = s_childWindow ? s_childWindow : s_hostWindow;
        ctx->hdc = s_hdc;
        ctx->hglrc = s_glContext;
        ctx->hwnd = primaryWindow;
        ctx->native_window = s_childWindow ? s_childWindow : s_hostWindow;
        ctx->native_drawable = s_hdc;
        ctx->native_gl_context = s_glContext;
#endif

        apply_view_size();
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
#endif
            ctx->windowData->sfml_window = s_window.get();
#if defined(_WIN32)
            ctx->windowData->hwnd = s_childWindow ? s_childWindow : s_hostWindow;
            ctx->windowData->host_hwnd = s_hostWindow;
            ctx->windowData->hwndChild = s_childWindow;
            ctx->windowData->hdc = s_hdc;
            ctx->windowData->ownsNativeDc = false;
            ctx->windowData->ownsNativeGlContext = false;
#endif
            ctx->windowData->set_size(s_width, s_height);
        }

        auto& state = epochnamespace::sfmlcontext::state::s_sfmlstate;
        state.window.sfml_window = s_window.get();
        state.set_dimensions(s_width, s_height);
        state.mark_should_close(false);
        state.running = true;

#if defined(_WIN32)
        const HWND focusWindow = s_childWindow ? s_childWindow : s_hostWindow;
        if (focusWindow && ::IsWindow(focusWindow) != FALSE)
        {
            ::SetFocus(focusWindow);
            s_window->requestFocus();
        }
        if (s_dockParent && ::IsWindow(s_dockParent) != FALSE)
            ::PostMessageW(s_dockParent, WM_SIZE, 0, 0);
#endif

        epochnamespace::atlasmanager::register_backend_uploader(
            epochnamespace::core::ContextType::SFML,
            [](const epochnamespace::TextureAtlas& atlas)
            {
                epochnamespace::sfmlcontext::ensure_uploaded(atlas);
            });
    }

    void sfml_cleanup_adapter()
    {
        epochnamespace::atlasmanager::unregister_backend_uploader(epochnamespace::core::ContextType::SFML);
        epochnamespace::sfmlcontext::clear_gpu_atlases();

        auto& state = epochnamespace::sfmlcontext::state::s_sfmlstate;
        state.window.sfml_window = nullptr;
        state.running = false;
        state.mark_should_close(false);

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

        if (s_window && s_window->isOpen())
            s_window->close();
        s_window.reset();

#if defined(_WIN32)
        if (s_hdc && s_childWindow)
            ::ReleaseDC(s_childWindow, s_hdc);
        s_hostWindow = nullptr;
        s_childWindow = nullptr;
        s_dockParent = nullptr;
        s_hdc = nullptr;
        s_glContext = nullptr;
#endif
        s_width = 0;
        s_height = 0;
        s_logged_activate_failure = false;
    }

    bool sfml_process_adapter(
        std::shared_ptr<epochnamespace::core::Context> ctx,
        epochnamespace::core::CommandQueue& queue)
    {
        if (!ctx || !s_window || !s_window->isOpen())
            return false;

        auto& state = epochnamespace::sfmlcontext::state::s_sfmlstate;
        const bool closeRequested =
            state.shouldClose
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

        if (!s_window->setActive(true))
        {
            if (!s_logged_activate_failure)
            {
                epochnamespace::logger::error(
                    "Context.SFML",
                    std::string("SFML setActive failed. size=")
                        + std::to_string(s_width)
                        + "x"
                        + std::to_string(s_height));
                s_logged_activate_failure = true;
            }
            return false;
        }

#if EPOCH_SFML_HAS_V3_API
        while (const auto event = s_window->pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                request_host_shutdown(ctx);
                (void)s_window->setActive(false);
                return false;
            }

            if (event->is<sf::Event::Resized>())
            {
                refresh_dimensions(ctx);
                apply_view_size();
                if (ctx->onResize)
                    ctx->onResize(ctx->framebufferWidth, ctx->framebufferHeight);
            }
        }
#else
        sf::Event event{};
        while (s_window->pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                request_host_shutdown(ctx);
                (void)s_window->setActive(false);
                return false;
            }

            if (event.type == sf::Event::Resized)
            {
                refresh_dimensions(ctx);
                apply_view_size();
                if (ctx->onResize)
                    ctx->onResize(ctx->framebufferWidth, ctx->framebufferHeight);
            }
        }
#endif

        sync_docked_child_size(ctx);
        refresh_dimensions(ctx);

        epochnamespace::atlasmanager::process_pending_uploads(epochnamespace::core::ContextType::SFML);
        s_window->resetGLStates();

        const auto clearColor = epochnamespace::core::clear_color_for_context(epochnamespace::core::ContextType::SFML);
        s_window->clear(sf::Color(
            static_cast<std::uint8_t>(clearColor[0] * 255.0f),
            static_cast<std::uint8_t>(clearColor[1] * 255.0f),
            static_cast<std::uint8_t>(clearColor[2] * 255.0f),
            static_cast<std::uint8_t>(clearColor[3] * 255.0f)));

        s_window->resetGLStates();
        (void)queue.drain();
        s_window->resetGLStates();
        render_scene_preview(ctx);
        s_window->resetGLStates();
        (void)epochnamespace::gui::render_deferred_batch(ctx.get());
        s_window->resetGLStates();
        (void)epochnamespace::gui::render_top_layer_batch(ctx.get());
        s_window->display();
        (void)s_window->setActive(false);
        return s_window->isOpen();
    }
}

namespace epochnamespace::core::detail
{
    void register_sfml_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::SFML;
        ctx->backendName = "SFML";
        ctx->initialize = sfml_initialize_adapter;
        ctx->cleanup = sfml_cleanup_adapter;
        ctx->process = sfml_process_adapter;
        ctx->clear = nullptr;
        ctx->present = nullptr;
        ctx->get_width = []() { return s_width; };
        ctx->get_height = []() { return s_height; };
        ctx->draw_sprite = epochnamespace::sfmlcontext::draw_sprite;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const epochnamespace::TextureAtlas& atlas)
        {
            return default_add_atlas(atlas);
        };
        bind_default_input(ctx);
        AddContextForBackend(ContextType::SFML, std::move(ctx));
    }
}

#endif
