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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#if !defined(_WIN32)
#include <thread>
#endif

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif

// Win32 has BOOL CloseWindow(HWND). Raylib has void CloseWindow(void).
// Prevent the collision in this TU by temporarily renaming Win32's symbol name during header include.
//#   define CloseWindow CloseWindow_Win32
#   include <include/framework.hpp>
//#   undef CloseWindow

#   include <wingdi.h> // HGLRC + wgl*
#   ifdef min
#       undef min
#   endif
#   ifdef max
#       undef max
#   endif
#endif

#include <include/engine.config.hpp> // for EPOCH_USING Macros

export module raylib.context;

import core.context;
import core.commandline;
import core.logger;
import context.type;
import context.multiplexer;
import engine.diagnostics;
import atlas.manager;
import image.writer;
import render.preview_grid;

import raylib.state;
import raylib.textures;
import raylib.renderer;
import raylib.api;


#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)

namespace epochnamespace::raylibcontext
{
    using NativeWindowHandle = void*;

    inline std::string& title_storage()
    {
        static std::string s_title = "epochengine";
        return s_title;
    }

#if defined(_WIN32)
    namespace detail
    {
        [[nodiscard]] inline unsigned long current_thread_token() noexcept { return ::GetCurrentThreadId(); }
        inline void sleep_short_ms(const unsigned long milliseconds) noexcept { ::Sleep(milliseconds); }

        [[nodiscard]] inline HGLRC current_context() noexcept { return ::wglGetCurrentContext(); }
        [[nodiscard]] inline HDC   current_dc() noexcept { return ::wglGetCurrentDC(); }

        inline bool make_current(HDC dc, HGLRC rc) noexcept
        {
            return ::wglMakeCurrent(dc, rc) != FALSE;
        }

        inline void clear_current() noexcept
        {
            (void)::wglMakeCurrent(nullptr, nullptr);
        }

        [[nodiscard]] inline bool contexts_match(HDC a_dc, HGLRC a_rc, HDC b_dc, HGLRC b_rc) noexcept
        {
            return a_dc == b_dc && a_rc == b_rc;
        }

        // Debug helper: verify the multiplexer has made the raylib context current
        inline void debug_expect_raylib_current(const epochnamespace::raylibstate::RaylibState& st, const char* where)
        {
#if defined(_DEBUG)
            const auto dc = current_dc();
            const auto rc = current_context();
            if (dc != st.hdc || rc != st.hglrc)
            {
                logger::warn(
                    "Raylib",
                    std::format("raylib context not current at {} (current dc/rc != raylib dc/rc)", where));
            }
#else
            (void)st; (void)where;
#endif
        }

        // If the raylib window is docked as a WS_CHILD, promote it to a top-level window
        // before letting raylib destroy it. This avoids edge cases where the dock host or
        // its thread is already tearing down, which can deadlock inside DestroyWindow.
        inline void promote_raylib_to_top_level(HWND hwnd) noexcept
        {
            if (!hwnd || ::IsWindow(hwnd) == FALSE)
                return;

            const LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
            const bool isChild = (style & WS_CHILD) != 0;
            const HWND parent = ::GetParent(hwnd);

            if (!isChild && !parent)
                return;

            RECT rc{};
            ::GetWindowRect(hwnd, &rc);

            // Detach from any parent and restore overlapped style.
            ::SetParent(hwnd, nullptr);

            LONG_PTR newStyle = style;
            newStyle &= ~static_cast<LONG_PTR>(WS_CHILD);
            newStyle |= static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW | WS_VISIBLE);

            ::SetWindowLongPtrW(hwnd, GWL_STYLE, newStyle);
            ::SetWindowPos(hwnd,
                HWND_TOP,
                rc.left,
                rc.top,
                (std::max)(1, static_cast<int>(rc.right - rc.left)),
                (std::max)(1, static_cast<int>(rc.bottom - rc.top)),
                SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }

        inline void adopt_raylib_window(
            epochnamespace::raylibstate::RaylibState& st,
            std::shared_ptr<core::Context> ctx,
            HWND parent,
            HWND raylibHwnd)
        {
            if (!raylibHwnd)
            {
                logger::warn("Raylib", "Raylib returned a null window handle.");
                return;
            }

            HWND dockParent = parent;
            if (parent && parent != raylibHwnd)
            {
                if (const HWND hostParent = ::GetParent(parent); hostParent && hostParent != raylibHwnd)
                    dockParent = hostParent;
            }

            st.parent = dockParent ? dockParent : parent;
            st.hwnd = raylibHwnd;
            st.dockedChildWindow = (st.parent && st.parent != raylibHwnd);

            if (st.parent && st.parent != raylibHwnd)
            {
                if (::GetParent(raylibHwnd) != st.parent)
                {
                    ::SetParent(raylibHwnd, st.parent);
                }

                LONG_PTR style = ::GetWindowLongPtrW(raylibHwnd, GWL_STYLE);
                style &= ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW);
                style |= (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
                ::SetWindowLongPtrW(raylibHwnd, GWL_STYLE, style);

                RECT client{};
                const HWND sizeSource = parent ? parent : st.parent;
                ::GetClientRect(sizeSource, &client);
                const int width = (std::max)(1, static_cast<int>(client.right - client.left));
                const int height = (std::max)(1, static_cast<int>(client.bottom - client.top));
                st.width = static_cast<unsigned>(width);
                st.height = static_cast<unsigned>(height);
                ::SetWindowPos(raylibHwnd,
                    nullptr,
                    0,
                    0,
                    width,
                    height,
                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

                epochnamespace::core::MakeDockable(raylibHwnd, st.parent);
                ::SetFocus(raylibHwnd);

                if (parent && parent != raylibHwnd && ::IsWindow(parent) != FALSE)
                    ::ShowWindow(parent, SW_HIDE);
            }

            if (ctx)
            {
                ctx->hwnd = raylibHwnd;
                ctx->native_window = raylibHwnd;
                if (ctx->windowData)
                {
                    const HWND previousHost = ctx->windowData->hwnd;
                    ctx->windowData->hwnd = raylibHwnd;
                    ctx->windowData->host_hwnd = previousHost ? previousHost : parent;
                    ctx->windowData->hwndChild = raylibHwnd;
                    ctx->windowData->set_size(static_cast<int>(st.width), static_cast<int>(st.height));
                }
            }

            epochnamespace::core::RequestActiveParentLayout();
        }

    }
#endif

#if !defined(_WIN32)
    namespace detail
    {
        [[nodiscard]] inline std::thread::id current_thread_token() noexcept { return std::this_thread::get_id(); }
        inline void sleep_short_ms(const unsigned long milliseconds) noexcept
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }
    }
#endif

    namespace detail
    {
        inline void capture_frame_if_requested(const std::uintptr_t windowId)
        {
            const auto capturePath = core::cli::reserve_capture_path("raylib", windowId);
            if (capturePath.empty())
                return;

            const auto image = epochnamespace::raylib_api::load_image_from_screen();
            if (!image.data || image.width <= 0 || image.height <= 0)
            {
                logger::warn("Raylib", "Failed to read raylib frame for capture.");
                return;
            }

            if (image.format != epochnamespace::raylib_api::pixelformat_rgba8)
            {
                logger::warn(
                    "Raylib",
                    std::format("Unsupported raylib capture format {}; expected RGBA8.", image.format));
                epochnamespace::raylib_api::unload_image(image);
                return;
            }

            const auto* sourcePixels = static_cast<const std::uint8_t*>(image.data);
            std::vector<std::uint8_t> pixels(
                sourcePixels,
                sourcePixels + (static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4u));

            if (a_writeImage(capturePath, pixels, image.width, image.height, false))
                logger::info("Raylib", "Captured frame to " + capturePath.string());
            else
                logger::warn("Raylib", "Failed to write capture to " + capturePath.string());

            epochnamespace::raylib_api::unload_image(image);
        }

        [[nodiscard]] inline epochnamespace::raylib_api::Color to_raylib_color(
            const epochnamespace::previewgrid::Vec3& color) noexcept
        {
            const auto clamp_channel = [](float value) noexcept -> std::uint8_t
            {
                const float scaled = (std::clamp)(value, 0.0f, 1.0f) * 255.0f;
                return static_cast<std::uint8_t>(scaled);
            };

            return epochnamespace::raylib_api::Color{
                clamp_channel(color.x),
                clamp_channel(color.y),
                clamp_channel(color.z),
                255
            };
        }

        [[nodiscard]] inline bool project_preview_vertex(
            const epochnamespace::previewgrid::Mat4& mvp,
            const epochnamespace::previewgrid::Vec3& position,
            const core::RenderViewport& viewport,
            epochnamespace::raylib_api::Vector2& out) noexcept
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

        inline void raylib_stop_rendering_backend(epochnamespace::raylibstate::RaylibState& st)
        {
            if (!st.renderingActive)
                return;

            epochnamespace::raylibtextures::shutdown_current_context_backend();

            if (st.frameActive)
            {
                if (st.frameInTextureMode)
                    epochnamespace::raylib_api::end_texture_mode();
                else
                    epochnamespace::raylib_api::end_drawing();
                st.frameActive = false;
                st.frameInTextureMode = false;
            }

            if (st.offscreen.id != 0)
            {
                epochnamespace::raylib_api::unload_render_texture(st.offscreen);
                st.offscreen = {};
                st.offscreenWidth = 0;
                st.offscreenHeight = 0;
            }

            st.renderingActive = false;
        }

        inline void ensure_frame_started(epochnamespace::raylibstate::RaylibState& st)
        {
            if (st.frameActive)
                return;

            epochnamespace::raylib_api::begin_drawing();
            st.frameActive = true;
            st.frameInTextureMode = false;
        }

        inline void render_scene_preview(const std::shared_ptr<core::Context>& ctx)
        {
            if (!ctx)
                return;

            const auto viewport = ctx->scene_viewport();
            if (!viewport.valid() || ctx->scene_preview_mode() != core::ScenePreviewMode::Editor)
                return;

            const auto clearColor = epochnamespace::previewgrid::kClearColor;
            epochnamespace::raylib_api::begin_scissor_mode(
                viewport.x,
                viewport.y,
                viewport.width,
                viewport.height);

            epochnamespace::raylib_api::draw_rectangle_rec(
                epochnamespace::raylib_api::Rectangle{
                    static_cast<float>(viewport.x),
                    static_cast<float>(viewport.y),
                    static_cast<float>(viewport.width),
                    static_cast<float>(viewport.height)
                },
                epochnamespace::raylib_api::Color{
                    static_cast<std::uint8_t>((std::clamp)(clearColor[0], 0.0f, 1.0f) * 255.0f),
                    static_cast<std::uint8_t>((std::clamp)(clearColor[1], 0.0f, 1.0f) * 255.0f),
                    static_cast<std::uint8_t>((std::clamp)(clearColor[2], 0.0f, 1.0f) * 255.0f),
                    static_cast<std::uint8_t>((std::clamp)(clearColor[3], 0.0f, 1.0f) * 255.0f)
                });

            if (epochnamespace::raylib_api::has_loaded_models())
            {
                constexpr float kRadiansToDegrees = 57.29577951308232f;
                const auto camera = epochnamespace::previewgrid::camera_for(ctx.get());
                const int renderHeight = (std::max)(1, epochnamespace::raylib_api::get_render_height());
                const int renderWidth = (std::max)(1, epochnamespace::raylib_api::get_render_width());
                const int viewportY = renderHeight - (viewport.y + viewport.height);

                epochnamespace::raylib_api::set_viewport(
                    viewport.x,
                    viewportY,
                    viewport.width,
                    viewport.height);

                epochnamespace::raylib_api::begin_mode_3d(epochnamespace::raylib_api::Camera3D{
                    .position = { camera.eye.x, camera.eye.y, camera.eye.z },
                    .target = { camera.target.x, camera.target.y, camera.target.z },
                    .up = { camera.up.x, camera.up.y, camera.up.z },
                    .fovy = camera.fovRadians * kRadiansToDegrees,
                    .projection = epochnamespace::raylib_api::camera_perspective
                });
                epochnamespace::raylib_api::draw_grid(20, 1.0f);
                epochnamespace::raylib_api::draw_loaded_models();
                epochnamespace::raylib_api::end_mode_3d();
                epochnamespace::raylib_api::set_viewport(0, 0, renderWidth, renderHeight);
                epochnamespace::raylib_api::end_scissor_mode();
                return;
            }

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

                epochnamespace::raylib_api::Vector2 a{};
                epochnamespace::raylib_api::Vector2 b{};
                if (!project_preview_vertex(mvp, vertices[firstIndex].position, viewport, a)
                    || !project_preview_vertex(mvp, vertices[secondIndex].position, viewport, b))
                {
                    continue;
                }

                epochnamespace::raylib_api::draw_line_v(
                    a,
                    b,
                    to_raylib_color(vertices[firstIndex].color));
            }

            const auto markerVertices = epochnamespace::previewgrid::look_marker_vertices_for(ctx.get());
            const std::size_t markerCount = epochnamespace::previewgrid::look_marker_vertex_count_for(ctx.get());
            for (std::size_t i = 0; i + 1 < markerCount; i += 2)
            {
                epochnamespace::raylib_api::Vector2 a{};
                epochnamespace::raylib_api::Vector2 b{};
                if (!project_preview_vertex(mvp, markerVertices[i].position, viewport, a)
                    || !project_preview_vertex(mvp, markerVertices[i + 1].position, viewport, b))
                {
                    continue;
                }

                epochnamespace::raylib_api::draw_line_v(
                    a,
                    b,
                    to_raylib_color(markerVertices[i].color));
            }

            const auto objectVertices = epochnamespace::previewgrid::object_marker_vertices_for(ctx.get());
            for (std::size_t i = 0; i + 1 < objectVertices.size(); i += 2)
            {
                epochnamespace::raylib_api::Vector2 a{};
                epochnamespace::raylib_api::Vector2 b{};
                if (!project_preview_vertex(mvp, objectVertices[i].position, viewport, a)
                    || !project_preview_vertex(mvp, objectVertices[i + 1].position, viewport, b))
                {
                    continue;
                }

                epochnamespace::raylib_api::draw_line_v(
                    a,
                    b,
                    to_raylib_color(objectVertices[i].color));
            }

            epochnamespace::raylib_api::end_scissor_mode();
        }

    }

    export inline bool raylib_initialize(
        std::shared_ptr<core::Context> ctx,
        NativeWindowHandle parent = nullptr,
        unsigned width = 0,
        unsigned height = 0,
        std::function<void(int, int)> resizeCallback = nullptr,
        std::string title = {})
    {
        auto& st = epochnamespace::raylibstate::s_raylibstate;

        if (width == 0)  width = static_cast<unsigned>(core::cli::window_width);
        if (height == 0) height = static_cast<unsigned>(core::cli::window_height);

        st.width = (std::max)(1u, width);
        st.height = (std::max)(1u, height);

        if (!title.empty())
            title_storage() = std::move(title);

        // Prevent re-initializing raylib (it initializes global state once).
        if (st.running)
        {
            return true;
        }

#if defined(_WIN32)
        // Capture whatever context the multiplexer/docking currently has bound.
        const HDC   previousDC = detail::current_dc();
        const HGLRC previousContext = detail::current_context();
#endif

        st.owner_ctx = ctx.get();
#if defined(_WIN32)
        st.owner_thread_id = detail::current_thread_token();
#else
        st.owner_thread = detail::current_thread_token();
#endif
        st.userResize = std::move(resizeCallback);

#if defined(_WIN32)
        st.hwnd = static_cast<HWND>(parent ? parent : (ctx ? ctx->hwnd : nullptr));
        st.hdc = ctx ? ctx->hdc : detail::current_dc();
        st.hglrc = ctx ? ctx->hglrc : detail::current_context();
        st.ownsDC = false;

        if (!st.hdc || !st.hglrc)
        {
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS && EPOCH_ENABLE_RAYLIB_CONFIRMATION_LOGS
            logger::info("Raylib", "Missing host OpenGL context; raylib will create its own.");
#endif
        }
#else
        st.hwnd = parent ? parent : (ctx ? ctx->hwnd : nullptr);
#endif

#if defined(_WIN32)
        if (st.hdc && st.hglrc &&
            !detail::contexts_match(previousDC, previousContext, st.hdc, st.hglrc))
        {
            (void)detail::make_current(st.hdc, st.hglrc);
        }
#endif

        epochnamespace::raylib_api::set_config_flags(
            static_cast<unsigned>(epochnamespace::raylib_api::flag_msaa_4x_hint));
        epochnamespace::raylib_api::set_trace_log_level(epochnamespace::raylib_api::log_warning);

        epochnamespace::raylib_api::init_window(
            static_cast<int>(st.width),
            static_cast<int>(st.height),
            title_storage().c_str());

#if defined(_WIN32)
        // Raylib creates its own OpenGL context. Capture it now so we bind the right rc later.
        st.hdc = detail::current_dc();
        st.hglrc = detail::current_context();
        if (!st.hglrc)
        {
            logger::warn("Raylib", "Failed to capture Raylib OpenGL context after initialization.");
            st.running = false;
            st.cleanupIssued = false;
            return false;
        }
#if defined(_DEBUG) && EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS && EPOCH_ENABLE_RAYLIB_CONFIRMATION_LOGS
        logger::info(
            "Raylib",
            std::format(
                "Updated raylib GL context dc={:p} rc={:p}",
                static_cast<const void*>(st.hdc),
                static_cast<const void*>(st.hglrc)));
#endif

        const HWND raylibHwnd = static_cast<HWND>(epochnamespace::raylib_api::get_window_handle());
        const HWND parentHwnd = static_cast<HWND>(parent);
        HWND adoptedHwnd = raylibHwnd;
        for (int attempt = 0; attempt < 5 && !adoptedHwnd; ++attempt)
        {
            epochnamespace::raylib_api::begin_drawing();
            epochnamespace::raylib_api::end_drawing();
            detail::sleep_short_ms(10);
            adoptedHwnd = static_cast<HWND>(epochnamespace::raylib_api::get_window_handle());
        }
        if (adoptedHwnd)
        {
            detail::adopt_raylib_window(st, ctx, parentHwnd, adoptedHwnd);
        }
        else if (parentHwnd)
        {
            logger::warn("Raylib", "Failed to acquire Raylib window handle for docking.");
        }
        if (!st.hdc)
        {
            logger::warn("Raylib", "Failed to capture Raylib window DC after initialization.");
            st.running = false;
            st.cleanupIssued = false;
            return false;
        }

        if (ctx && ctx->windowData)
        {
            ctx->windowData->hdc = st.hdc;
            ctx->windowData->glContext = st.hglrc;
            ctx->windowData->usesSharedContext = false;
        }

#endif

        epochnamespace::raylib_api::set_target_fps(0);

        st.onResize = [](int w, int h)
            {
                auto& state = epochnamespace::raylibstate::s_raylibstate;
                const int clampedW = (std::max)(1, w);
                const int clampedH = (std::max)(1, h);
                state.width = static_cast<unsigned>(clampedW);
                state.height = static_cast<unsigned>(clampedH);

                epochnamespace::raylib_api::set_window_size(clampedW, clampedH);

#if defined(_WIN32)
                if (state.hwnd && ::IsWindow(state.hwnd) != FALSE)
                {
                    const HWND liveParent = ::GetParent(state.hwnd);
                    if (state.parent
                        && ::IsWindow(state.parent) != FALSE
                        && liveParent == state.parent)
                    {
                        state.dockedChildWindow = true;
                    }
                    else if (!liveParent
                        || (state.parent
                            && ::IsWindow(state.parent) != FALSE
                            && liveParent != state.parent))
                    {
                        state.dockedChildWindow = false;
                    }

                    if (state.dockedChildWindow
                        && state.parent
                        && ::IsWindow(state.parent) != FALSE
                        && liveParent != state.parent)
                    {
                        ::SetParent(state.hwnd, state.parent);
                    }

                    if (state.dockedChildWindow
                        && state.parent && ::IsWindow(state.parent) != FALSE)
                    {
                        LONG_PTR style = ::GetWindowLongPtrW(state.hwnd, GWL_STYLE);
                        style &= ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW | WS_POPUP);
                        style |= (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
                        ::SetWindowLongPtrW(state.hwnd, GWL_STYLE, style);

                        LONG_PTR exStyle = ::GetWindowLongPtrW(state.hwnd, GWL_EXSTYLE);
                        exStyle &= ~static_cast<LONG_PTR>(
                            WS_EX_APPWINDOW
                            | WS_EX_WINDOWEDGE
                            | WS_EX_CLIENTEDGE
                            | WS_EX_DLGMODALFRAME
                            | WS_EX_TOPMOST);
                        exStyle |= WS_EX_NOPARENTNOTIFY;
                        ::SetWindowLongPtrW(state.hwnd, GWL_EXSTYLE, exStyle);
                    }
                    else
                    {
                        LONG_PTR style = ::GetWindowLongPtrW(state.hwnd, GWL_STYLE);
                        style &= ~static_cast<LONG_PTR>(WS_CHILD);
                        style |= static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW | WS_VISIBLE);
                        ::SetWindowLongPtrW(state.hwnd, GWL_STYLE, style);
                    }

                    ::SetWindowPos(
                        state.hwnd,
                        nullptr,
                        0,
                        0,
                        clampedW,
                        clampedH,
                        SWP_NOZORDER | SWP_NOACTIVATE | ((state.dockedChildWindow && state.parent && ::IsWindow(state.parent) != FALSE) ? 0 : SWP_NOMOVE) | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

                    if (state.dockedChildWindow
                        && state.parent
                        && ::IsWindow(state.parent) != FALSE
                        && state.owner_ctx
                        && state.owner_ctx->windowData
                        && state.owner_ctx->windowData->host_hwnd
                        && ::IsWindow(state.owner_ctx->windowData->host_hwnd) != FALSE
                        && ::GetParent(state.owner_ctx->windowData->host_hwnd) == state.parent)
                    {
                        ::ShowWindow(state.owner_ctx->windowData->host_hwnd, SW_HIDE);
                    }
                }
#endif
                if (state.owner_ctx)
                {
                    state.owner_ctx->width = clampedW;
                    state.owner_ctx->height = clampedH;
                    state.owner_ctx->framebufferWidth = clampedW;
                    state.owner_ctx->framebufferHeight = clampedH;
                    if (state.owner_ctx->windowData)
                        state.owner_ctx->windowData->set_size(clampedW, clampedH);
                }
                if (state.userResize)
                    state.userResize(clampedW, clampedH);
            };

#if defined(_WIN32)
        // Restore previous GL binding for the dock/multiplexer host.
        if (!detail::contexts_match(previousDC, previousContext, st.hdc, st.hglrc))
        {
            if (previousDC && previousContext)
                (void)detail::make_current(previousDC, previousContext);
            else
                detail::clear_current();
        }
#endif

        if (ctx)
            ctx->onResize = st.onResize;

        st.running = true;
        st.renderingActive = true;
        st.cleanupIssued = false;
        st.cleanupRequested = false;
        st.currentFailureStreak = 0;
        st.currentFailureWarned = false;

        epochnamespace::atlasmanager::register_backend_uploader(
            epochnamespace::core::ContextType::RayLib,
            epochnamespace::raylibtextures::ensure_uploaded);

#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS && EPOCH_ENABLE_RAYLIB_CONFIRMATION_LOGS
        logger::info(
            "Raylib",
            std::format(
                "Initialized. hwnd={:p} size={}x{}",
                static_cast<const void*>(st.hwnd),
                st.width,
                st.height));
#endif

        return true;
    }

    export inline bool raylib_make_current() noexcept
    {
#if defined(_WIN32)
        auto& st = epochnamespace::raylibstate::s_raylibstate;
        if (!st.hdc || !st.hglrc)
            return false;

        return detail::make_current(st.hdc, st.hglrc);
#else
        return true;
#endif
    }

    namespace detail
    {
        void raylib_cleanup_owner_thread(epochnamespace::core::Context* ctx);
    }

    export inline void raylib_process()
    {
        auto& st = epochnamespace::raylibstate::s_raylibstate;
        if (st.cleanupRequested)
        {
            detail::raylib_cleanup_owner_thread(st.owner_ctx);
            return;
        }
        if (!st.running)
            return;

        const std::uintptr_t windowId = st.hwnd
            ? reinterpret_cast<std::uintptr_t>(st.hwnd)
            : (st.owner_ctx && st.owner_ctx->windowData
                ? reinterpret_cast<std::uintptr_t>(st.owner_ctx->windowData->hwnd)
                : 0);

        diagnostics::FrameTiming frameTimer{
            epochnamespace::core::ContextType::RayLib,
            windowId,
            "Raylib"
        };

#if defined(_WIN32)
        if (!raylib_make_current())
        {
            const bool windowHandleInvalid = st.hwnd && (::IsWindow(st.hwnd) == FALSE);
            const bool raylibRequestedClose = epochnamespace::raylib_api::window_should_close();
            const bool windowClosing =
                (st.owner_ctx && st.owner_ctx->windowData && st.owner_ctx->windowData->get_should_close())
                || !st.renderingActive
                || windowHandleInvalid
                || raylibRequestedClose;
            if (windowClosing)
            {
                st.running = false;
                return;
            }

            ++st.currentFailureStreak;
            if (!st.currentFailureWarned)
            {
                logger::warn(
                    "Raylib",
                    "Failed to make raylib context current during process; skipping frame.");
                st.currentFailureWarned = true;
            }
            if (st.currentFailureStreak >= 8)
            {
                logger::warn(
                    "Raylib",
                    "Raylib context could not be recovered after repeated attempts; shutting down.");
                st.running = false;
            }
            return;
        }
        st.currentFailureStreak = 0;
        st.currentFailureWarned = false;
#endif

        if (epochnamespace::raylib_api::window_should_close())
        {
            detail::raylib_stop_rendering_backend(st);

            st.running = false;

            if (!st.cleanupIssued)
            {
#if defined(_WIN32)
                const bool on_owner_thread = (st.owner_thread_id == detail::current_thread_token());
#else
                const bool on_owner_thread = true;
#endif

                st.cleanupIssued = true;
                if (on_owner_thread)
                    detail::raylib_cleanup_owner_thread(st.owner_ctx);
                else
                    st.cleanupRequested = true;
            }

            return;
        }

#if defined(_WIN32)
        // Expect the multiplexer to have activated this backend before calling process.
        detail::debug_expect_raylib_current(st, "raylib_process");
#endif

        frameTimer.finish();
    }

    export inline void raylib_idle_frame()
    {
        auto& st = epochnamespace::raylibstate::s_raylibstate;
        if (!st.running || !st.renderingActive)
            return;

#if defined(_WIN32)
        if (!raylib_make_current())
            return;
#endif

        if (!st.frameActive)
        {
            epochnamespace::raylib_api::begin_drawing();
            st.frameActive = true;
            st.frameInTextureMode = false;
        }

        epochnamespace::raylib_api::end_drawing();
        st.frameActive = false;
        st.frameInTextureMode = false;

#if defined(_WIN32)
        detail::clear_current();
#endif
    }

    export inline void raylib_clear(float r, float g, float b, float a)
    {
        (void)r;
        (void)g;
        (void)b;
        (void)a;

        auto& st = epochnamespace::raylibstate::s_raylibstate;
        if (!st.running || !st.renderingActive)
            return;

#if defined(_WIN32)
        if (!raylib_make_current())
            return;
#endif

        detail::ensure_frame_started(st);

#if EPOCH_USE_CLEAR_COLOR
        const auto clearColor = core::clear_color_for_context(core::ContextType::RayLib);
        epochnamespace::raylib_api::clear_background(
            epochnamespace::raylib_api::Color{
                static_cast<unsigned char>(std::clamp(clearColor[0], 0.0f, 1.0f) * 255.0f),
                static_cast<unsigned char>(std::clamp(clearColor[1], 0.0f, 1.0f) * 255.0f),
                static_cast<unsigned char>(std::clamp(clearColor[2], 0.0f, 1.0f) * 255.0f),
                static_cast<unsigned char>(std::clamp(clearColor[3], 0.0f, 1.0f) * 255.0f)
            });
#endif
    }

    export inline void raylib_render_scene_preview(const std::shared_ptr<core::Context>& ctx)
    {
        auto& st = epochnamespace::raylibstate::s_raylibstate;
        if (!st.running || !st.renderingActive)
            return;

#if defined(_WIN32)
        if (!raylib_make_current())
            return;
#endif

        detail::ensure_frame_started(st);
        detail::render_scene_preview(ctx);
    }

    export inline void raylib_present()
    {
        auto& st = epochnamespace::raylibstate::s_raylibstate;
        if (!st.running || !st.renderingActive)
            return;

        if (!raylib_make_current())
            return;
        const std::uintptr_t windowId = st.hwnd
            ? reinterpret_cast<std::uintptr_t>(st.hwnd)
            : (st.owner_ctx && st.owner_ctx->windowData
                ? reinterpret_cast<std::uintptr_t>(st.owner_ctx->windowData->hwnd)
                : 0);
        if (st.frameActive)
        {
            if (st.frameInTextureMode)
                epochnamespace::raylib_api::end_texture_mode();
            else
                epochnamespace::raylib_api::end_drawing();
            st.frameActive = false;
            st.frameInTextureMode = false;
        }

        detail::capture_frame_if_requested(windowId);

#if defined(_WIN32)
        detail::clear_current();
#endif
    }

    namespace detail
    {
        inline void raylib_cleanup_owner_thread(epochnamespace::core::Context* ctx)
        {
            auto& st = epochnamespace::raylibstate::s_raylibstate;

#if defined(_WIN32)
            const HDC   previousDC = detail::current_dc();
            const HGLRC previousContext = detail::current_context();
            const bool window_alive = (st.hwnd != nullptr) && (::IsWindow(st.hwnd) != FALSE);
#endif

            epochnamespace::atlasmanager::unregister_backend_uploader(
                epochnamespace::core::ContextType::RayLib);

#if defined(_WIN32)
            (void)raylib_make_current();
#endif

            detail::raylib_stop_rendering_backend(st);
            epochnamespace::raylib_api::unload_all_models();

            if (ctx && ctx->windowData)
            {
#if defined(_WIN32)
                ctx->windowData->hdc = nullptr;
                ctx->windowData->glContext = nullptr;
                ctx->windowData->usesSharedContext = false;
#endif
            }

            if (epochnamespace::raylib_api::is_window_ready())
            {
#if defined(_WIN32)
                if (window_alive)
                {
                    // If docked as child, detach to top-level before closing to avoid teardown deadlocks.
                    detail::promote_raylib_to_top_level(st.hwnd);
                    epochnamespace::raylib_api::close_window();
                }
#else
                epochnamespace::raylib_api::close_window();
#endif
            }

#if defined(_WIN32)
            if (st.ownsDC && st.hdc && st.hwnd)
                ::ReleaseDC(st.hwnd, st.hdc);

            if (!detail::contexts_match(previousDC, previousContext, st.hdc, st.hglrc))
            {
                if (previousDC && previousContext)
                    (void)detail::make_current(previousDC, previousContext);
                else
                    detail::clear_current();
            }
#endif

            st = {};
        }
    }

    export inline void raylib_cleanup(std::shared_ptr<core::Context> ctx)
    {
        auto& st = epochnamespace::raylibstate::s_raylibstate;
        if (st.cleanupIssued)
            return;

#if defined(_WIN32)
        const bool on_owner_thread = (st.owner_thread_id == detail::current_thread_token());
#else
        const bool on_owner_thread = true;
#endif

        st.cleanupIssued = true;
        st.running = false;

        if (!on_owner_thread)
        {
            st.cleanupRequested = true;
            return;
        }

        detail::raylib_cleanup_owner_thread(ctx.get());
    }

    export inline void raylib_set_window_title(std::string_view title)
    {
        title_storage().assign(title.begin(), title.end());
        epochnamespace::raylib_api::set_window_title(title_storage().c_str());
    }

    export inline int raylib_get_width()
    {
        return static_cast<int>(epochnamespace::raylibstate::s_raylibstate.width);
    }

    export inline int raylib_get_height()
    {
        return static_cast<int>(epochnamespace::raylibstate::s_raylibstate.height);
    }

    export inline bool raylib_is_running() noexcept
    {
        return epochnamespace::raylibstate::s_raylibstate.running;
    }

    export inline epochnamespace::raylib_api::Vector2 raylib_get_mouse_position()
    {
        return epochnamespace::raylib_api::get_mouse_position();
    }

    export inline NativeWindowHandle raylib_get_native_window()
    {
        return epochnamespace::raylibstate::s_raylibstate.hwnd;
    }
}

#endif // EPOCH_USING_RAYLIB
