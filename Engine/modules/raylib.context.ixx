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

#include "core.format_text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
#   include <../src/platform.framework.hpp>
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
import diagnostics.engine;
import atlas.manager;
import image.writer;
import package.registry;
import render.arcade;
import render.preview_grid;

import raylib.state;
import raylib.textures;
import raylib.renderer;
import raylib.api;
#if !defined(_WIN32)
import raylib.context_linux;
#endif


#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)

namespace epochengine::raylibcontext
{
    using NativeWindowHandle = void*;

    export struct RaylibNativeDiagnostics
    {
        std::uintptr_t hwnd = 0;
        std::uintptr_t parent = 0;
        std::uintptr_t dc = 0;
        std::uintptr_t glContext = 0;
        std::uintptr_t currentDc = 0;
        std::uintptr_t currentGlContext = 0;
        std::uintptr_t windowFromDc = 0;
        bool windowValid = false;
        bool windowVisible = false;
        bool childStyle = false;
        bool currentMatchesExpected = false;
        int clientWidth = 0;
        int clientHeight = 0;
        std::uint32_t ownerThread = 0;
        std::uint32_t currentThread = 0;
    };

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
        inline void debug_expect_raylib_current(const epochengine::raylibstate::RaylibState& st, const char* where)
        {
#if defined(_DEBUG)
            const auto dc = current_dc();
            const auto rc = current_context();
            if (dc != st.hdc || rc != st.hglrc)
            {
                logger::warn(
                    "Raylib",
                    epochengine::format_text("raylib context not current at {} (current dc/rc != raylib dc/rc)", where));
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

            ::ShowWindow(hwnd, SW_HIDE);

            // Detach from any parent and restore overlapped style.
            ::SetParent(hwnd, nullptr);

            LONG_PTR newStyle = style;
            newStyle &= ~static_cast<LONG_PTR>(WS_CHILD | WS_VISIBLE);
            newStyle |= static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW);

            ::SetWindowLongPtrW(hwnd, GWL_STYLE, newStyle);
            ::SetWindowPos(hwnd,
                nullptr,
                0,
                0,
                0,
                0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_HIDEWINDOW);
        }

        inline void adopt_raylib_window(
            epochengine::raylibstate::RaylibState& st,
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
                // GLFW owns this HWND and its WGL surface. Preserve the proven
                // parent-first adoption order before changing top-level styles.
                if (::GetParent(raylibHwnd) != st.parent)
                    ::SetParent(raylibHwnd, st.parent);


                LONG_PTR style = ::GetWindowLongPtrW(raylibHwnd, GWL_STYLE);
                style &= ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW | WS_POPUP);
                style |= (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
                ::SetWindowLongPtrW(raylibHwnd, GWL_STYLE, style);

                LONG_PTR exStyle = ::GetWindowLongPtrW(raylibHwnd, GWL_EXSTYLE);
                exStyle &= ~static_cast<LONG_PTR>(
                    WS_EX_APPWINDOW
                    | WS_EX_TOOLWINDOW
                    | WS_EX_TOPMOST
                    | WS_EX_WINDOWEDGE
                    | WS_EX_CLIENTEDGE
                    | WS_EX_DLGMODALFRAME);
                exStyle |= WS_EX_NOPARENTNOTIFY;
                ::SetWindowLongPtrW(raylibHwnd, GWL_EXSTYLE, exStyle);

                RECT client{};
                const HWND sizeSource = parent ? parent : st.parent;
                ::GetClientRect(sizeSource, &client);
                const int width = (std::max)(1, static_cast<int>(client.right - client.left));
                const int height = (std::max)(1, static_cast<int>(client.bottom - client.top));
                st.width = static_cast<unsigned>(width);
                st.height = static_cast<unsigned>(height);

                if (parent
                    && parent != st.parent
                    && parent != raylibHwnd
                    && ::IsWindow(parent) != FALSE)
                {
                    ::ShowWindow(parent, SW_HIDE);
                }
                ::SetWindowPos(raylibHwnd,
                    HWND_TOP,
                    0,
                    0,
                    width,
                    height,
                    SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                ::ShowWindow(raylibHwnd, SW_SHOWNA);
                ::UpdateWindow(raylibHwnd);

                epochengine::core::MakeDockable(raylibHwnd, st.parent);
                ::SetFocus(raylibHwnd);
            }

            if (ctx)
            {
                ctx->width = static_cast<int>(st.width);
                ctx->height = static_cast<int>(st.height);
                ctx->framebufferWidth = (std::max)(1, epochengine::raylib_api::get_render_width());
                ctx->framebufferHeight = (std::max)(1, epochengine::raylib_api::get_render_height());
                ctx->hwnd = raylibHwnd;
                ctx->hdc = st.hdc;
                ctx->hglrc = st.hglrc;
                ctx->native_window = raylibHwnd;
                ctx->native_drawable = st.hdc;
                ctx->native_gl_context = st.hglrc;
                if (ctx->windowData)
                {
                    const HWND stableHost = ctx->windowData->host_hwnd
                        ? ctx->windowData->host_hwnd
                        : ctx->windowData->hwnd;
                    const HDC previousDc = ctx->windowData->hdc;
                    if (stableHost
                        && stableHost != raylibHwnd
                        && previousDc
                        && ctx->windowData->ownsNativeDc)
                    {
                        // Keep the CS_OWNDC placeholder pairing intact while
                        // GLFW owns the active surface. It is released with the
                        // parked host after Raylib has closed its own context.
                        ctx->windowData->parked_host_hdc = previousDc;
                        ctx->windowData->ownsParkedHostDc = true;

                    }

                    // hwnd always names the live surface. The original Epoch
                    // placeholder remains host_hwnd and keys the render thread
                    // while Raylib's GLFW window becomes the backend child.
                    // This is the stable multicontext handle contract.
                    ctx->windowData->hwnd = raylibHwnd;
                    ctx->windowData->host_hwnd = stableHost ? stableHost : parent;
                    ctx->windowData->hwndChild = raylibHwnd;
                    ctx->windowData->hdc = st.hdc;
                    ctx->windowData->glContext = st.hglrc;
                    ctx->windowData->usesSharedContext = false;
                    ctx->windowData->ownsNativeDc = false;
                    ctx->windowData->ownsNativeGlContext = false;
                    ctx->windowData->set_size(static_cast<int>(st.width), static_cast<int>(st.height));
                }
            }

            epochengine::core::RequestActiveParentLayout();
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

            const auto image = epochengine::raylib_api::load_image_from_screen();
            if (!image.data || image.width <= 0 || image.height <= 0)
            {
                logger::warn("Raylib", "Failed to read raylib frame for capture.");
                return;
            }

            if (image.format != epochengine::raylib_api::pixelformat_rgba8)
            {
                logger::warn(
                    "Raylib",
                    epochengine::format_text("Unsupported raylib capture format {}; expected RGBA8.", image.format));
                epochengine::raylib_api::unload_image(image);
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

            epochengine::raylib_api::unload_image(image);
        }

        [[nodiscard]] inline epochengine::raylib_api::Color to_raylib_color(
            const epochengine::previewgrid::Vec3& color) noexcept
        {
            const auto clamp_channel = [](float value) noexcept -> std::uint8_t
            {
                const float scaled = (std::clamp)(value, 0.0f, 1.0f) * 255.0f;
                return static_cast<std::uint8_t>(scaled);
            };

            return epochengine::raylib_api::Color{
                clamp_channel(color.x),
                clamp_channel(color.y),
                clamp_channel(color.z),
                255
            };
        }

        [[nodiscard]] inline bool project_preview_vertex(
            const epochengine::previewgrid::Mat4& mvp,
            const epochengine::previewgrid::Vec3& position,
            const core::RenderViewport& viewport,
            epochengine::raylib_api::Vector2& out) noexcept
        {
            const auto clip = epochengine::previewgrid::transform_point(mvp, position);
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

        struct RaylibArcadePreviewTarget final
        {
            epochengine::raylib_api::RenderTexture2D target{};
            int width = 0;
            int height = 0;
            std::uint64_t frame = 0u;
        };

        [[nodiscard]] inline RaylibArcadePreviewTarget& arcade_preview_target() noexcept
        {
            static RaylibArcadePreviewTarget target{};
            return target;
        }

        [[nodiscard]] constexpr bool raylib_arcade_sampled_preview_contract() noexcept
        {
            constexpr auto route = epochengine::previewgrid::object_preview_geometry_route(
                epochengine::previewgrid::ObjectPreviewPrimitive::EngineArcadeScreen);
            return route.sampled_surface
                && !route.solid_scene
                && !route.marker_wire
                && epochengine::render_arcade::kScreenSceneNode.sampled_render_surface
                && !epochengine::render_arcade::kScreenSceneNode.diagnostic_overlay
                && epochengine::package_registry::engine_arcade_render_texture_width() > 0u
                && epochengine::package_registry::engine_arcade_render_texture_height() > 0u;
        }

        static_assert(raylib_arcade_sampled_preview_contract());

        inline void destroy_arcade_preview_target() noexcept
        {
            auto& preview = arcade_preview_target();
            if (preview.target.id != 0u)
                epochengine::raylib_api::unload_render_texture(preview.target);
            preview = {};
        }

        [[nodiscard]] inline bool ensure_arcade_preview_target() noexcept
        {
            auto& preview = arcade_preview_target();
            const auto descriptor = epochengine::render_arcade::make_screen_render_texture_desc();
            const int width = static_cast<int>(descriptor.width);
            const int height = static_cast<int>(descriptor.height);
            if (width <= 0 || height <= 0)
                return false;

            if (preview.target.id != 0u
                && preview.target.texture.id != 0u
                && preview.width == width
                && preview.height == height)
            {
                return true;
            }

            destroy_arcade_preview_target();
            preview.target = epochengine::raylib_api::load_render_texture(width, height);
            if (preview.target.id == 0u || preview.target.texture.id == 0u)
            {
                preview = {};
                return false;
            }

            preview.width = width;
            preview.height = height;
            return true;
        }

        [[nodiscard]] inline bool update_arcade_preview_target() noexcept
        {
            if (!ensure_arcade_preview_target())
                return false;

            auto& preview = arcade_preview_target();
            epochengine::raylib_api::begin_texture_mode(preview.target);
            epochengine::raylib_api::clear_background(
                epochengine::raylib_api::Color{ 4u, 6u, 11u, 255u });
            epochengine::render_arcade::emit_arcade_attract_pattern(
                preview.width,
                preview.height,
                preview.frame++,
                [](const epochengine::render_arcade::ArcadePreviewRect& rect)
                {
                    const auto to_channel = [](float value) noexcept -> std::uint8_t
                    {
                        return static_cast<std::uint8_t>(
                            (std::clamp)(value, 0.0f, 1.0f) * 255.0f + 0.5f);
                    };
                    epochengine::raylib_api::draw_rectangle_rec(
                        epochengine::raylib_api::Rectangle{
                            static_cast<float>(rect.x),
                            static_cast<float>(rect.y),
                            static_cast<float>(rect.width),
                            static_cast<float>(rect.height)
                        },
                        epochengine::raylib_api::Color{
                            to_channel(rect.color[0]),
                            to_channel(rect.color[1]),
                            to_channel(rect.color[2]),
                            to_channel(rect.color[3])
                        });
                });
            epochengine::raylib_api::end_texture_mode();
            return true;
        }

        inline void render_engine_arcade_sampled_surface_preview(
            const std::vector<epochengine::previewgrid::ObjectMarker>& markers,
            const epochengine::previewgrid::Mat4& mvp,
            const core::RenderViewport& viewport) noexcept
        {
            const auto& preview = arcade_preview_target();
            if (preview.target.texture.id == 0u)
                return;

            for (const auto& marker : markers)
            {
                if (marker.primitive != epochengine::previewgrid::ObjectPreviewPrimitive::EngineArcadeScreen)
                    continue;

                const float halfX = (std::max)(std::abs(marker.scale.x) * 0.5f, 0.25f);
                const float halfY = (std::max)(std::abs(marker.scale.y) * 0.5f, 0.18f);
                const float z =
                    epochengine::render_arcade::screen_sample_plane_z(
                        marker.position.z, marker.scale.z);
                const epochengine::previewgrid::Vec3 world[4]{
                    { marker.position.x - halfX, marker.position.y - halfY, z },
                    { marker.position.x + halfX, marker.position.y - halfY, z },
                    { marker.position.x + halfX, marker.position.y + halfY, z },
                    { marker.position.x - halfX, marker.position.y + halfY, z }
                };
                epochengine::raylib_api::Vector2 projected[4]{};
                bool visible = true;
                for (std::size_t i = 0; i < 4u; ++i)
                {
                    if (!project_preview_vertex(mvp, world[i], viewport, projected[i]))
                    {
                        visible = false;
                        break;
                    }
                }
                if (!visible)
                    continue;

                epochengine::raylib_api::draw_texture_quad(
                    preview.target.texture,
                    projected[3],
                    projected[0],
                    projected[1],
                    projected[2],
                    epochengine::raylib_api::white);
            }
        }

        inline void raylib_stop_rendering_backend(epochengine::raylibstate::RaylibState& st)
        {
            destroy_arcade_preview_target();
            if (!st.renderingActive)
                return;

            epochengine::raylibtextures::shutdown_current_context_backend();

            if (st.frameActive)
            {
                if (st.frameInTextureMode)
                    epochengine::raylib_api::end_texture_mode();
                else
                    epochengine::raylib_api::end_drawing();
                st.frameActive = false;
                st.frameInTextureMode = false;
            }

            epochengine::raylibrenderer::release_canvas2d_scene_renderer(
                st.owner_ctx);

            if (st.offscreen.id != 0)
            {
                epochengine::raylib_api::unload_render_texture(st.offscreen);
                st.offscreen = {};
                st.offscreenWidth = 0;
                st.offscreenHeight = 0;
            }

            st.renderingActive = false;
        }

        inline void ensure_frame_started(epochengine::raylibstate::RaylibState& st)
        {
            if (st.frameActive)
                return;

            epochengine::raylib_api::begin_drawing();
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

            const int framebufferWidth =
                (std::max)(1, epochengine::raylib_api::get_render_width());
            const int framebufferHeight =
                (std::max)(1, epochengine::raylib_api::get_render_height());
            if (epochengine::raylibrenderer::render_canvas2d_scene_content(
                    ctx,
                    viewport,
                    framebufferWidth,
                    framebufferHeight))
            {
                return;
            }

            const auto sampledSurfaceMarkers =
                epochengine::previewgrid::sampled_render_surface_markers_for(ctx.get());
            const bool arcadePreviewReady =
                !sampledSurfaceMarkers.empty() && update_arcade_preview_target();

            const auto clearColor = epochengine::previewgrid::kClearColor;
            epochengine::raylib_api::begin_scissor_mode(
                viewport.x,
                viewport.y,
                viewport.width,
                viewport.height);

            epochengine::raylib_api::draw_rectangle_rec(
                epochengine::raylib_api::Rectangle{
                    static_cast<float>(viewport.x),
                    static_cast<float>(viewport.y),
                    static_cast<float>(viewport.width),
                    static_cast<float>(viewport.height)
                },
                epochengine::raylib_api::Color{
                    static_cast<std::uint8_t>((std::clamp)(clearColor[0], 0.0f, 1.0f) * 255.0f),
                    static_cast<std::uint8_t>((std::clamp)(clearColor[1], 0.0f, 1.0f) * 255.0f),
                    static_cast<std::uint8_t>((std::clamp)(clearColor[2], 0.0f, 1.0f) * 255.0f),
                    static_cast<std::uint8_t>((std::clamp)(clearColor[3], 0.0f, 1.0f) * 255.0f)
                });
            const auto cameraMode = epochengine::previewgrid::camera_mode_for(ctx.get());

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
            const auto gridGeometry = epochengine::previewgrid::grid_geometry_for(ctx.get());
            const auto& vertices = gridGeometry->vertices;
            const auto& indices = gridGeometry->indices;

            for (std::size_t i = 0; i + 1 < indices.size(); i += 2)
            {
                const auto firstIndex = static_cast<std::size_t>(indices[i]);
                const auto secondIndex = static_cast<std::size_t>(indices[i + 1]);
                if (firstIndex >= vertices.size() || secondIndex >= vertices.size())
                    continue;

                epochengine::raylib_api::Vector2 a{};
                epochengine::raylib_api::Vector2 b{};
                if (!project_preview_vertex(mvp, vertices[firstIndex].position, viewport, a)
                    || !project_preview_vertex(mvp, vertices[secondIndex].position, viewport, b))
                {
                    continue;
                }

                epochengine::raylib_api::draw_line_v(
                    a,
                    b,
                    to_raylib_color(vertices[firstIndex].color));
            }

            const auto solidVertices = epochengine::previewgrid::object_solid_vertices_for(ctx.get());
            for (std::size_t i = 0; i + 2 < solidVertices.size(); i += 3)
            {
                if (!epochengine::previewgrid::clockwise_solid_triangle_faces_camera(
                        solidVertices[i].position,
                        solidVertices[i + 1].position,
                        solidVertices[i + 2].position,
                        camera.eye))
                {
                    continue;
                }

                epochengine::raylib_api::Vector2 a{};
                epochengine::raylib_api::Vector2 b{};
                epochengine::raylib_api::Vector2 c{};
                if (!project_preview_vertex(mvp, solidVertices[i].position, viewport, a)
                    || !project_preview_vertex(mvp, solidVertices[i + 1].position, viewport, b)
                    || !project_preview_vertex(mvp, solidVertices[i + 2].position, viewport, c))
                {
                    continue;
                }

                const auto color = to_raylib_color(solidVertices[i].color);
                epochengine::raylib_api::draw_triangle(a, b, c, color);
                epochengine::raylib_api::draw_triangle(c, b, a, color);
            }

            if (epochengine::raylib_api::has_loaded_models()
                && cameraMode != epochengine::previewgrid::CameraMode::Canvas2D)
            {
                constexpr float kRadiansToDegrees = 57.29577951308232f;
                const int renderHeight =
                    (std::max)(1, epochengine::raylib_api::get_render_height());
                const int renderWidth =
                    (std::max)(1, epochengine::raylib_api::get_render_width());
                const int viewportY = renderHeight - (viewport.y + viewport.height);

                epochengine::raylib_api::set_viewport(
                    viewport.x,
                    viewportY,
                    viewport.width,
                    viewport.height);
                epochengine::raylib_api::begin_mode_3d(
                    epochengine::raylib_api::Camera3D{
                        .position = { camera.eye.x, camera.eye.y, camera.eye.z },
                        .target = { camera.target.x, camera.target.y, camera.target.z },
                        .up = { camera.up.x, camera.up.y, camera.up.z },
                        .fovy = camera.projection
                                == epochengine::render_camera::ProjectionKind::orthographic
                            ? camera.orthographicVerticalSize
                            : camera.fovRadians * kRadiansToDegrees,
                        .projection = camera.projection
                                == epochengine::render_camera::ProjectionKind::orthographic
                            ? epochengine::raylib_api::camera_orthographic
                            : epochengine::raylib_api::camera_perspective
                    });
                epochengine::raylib_api::draw_loaded_models();
                epochengine::raylib_api::end_mode_3d();
                epochengine::raylib_api::set_viewport(
                    0,
                    0,
                    renderWidth,
                    renderHeight);
            }

            if (arcadePreviewReady)
                render_engine_arcade_sampled_surface_preview(
                    sampledSurfaceMarkers, mvp, viewport);

            const auto markerVertices = epochengine::previewgrid::look_marker_vertices_for(ctx.get());
            const std::size_t markerCount = epochengine::previewgrid::look_marker_vertex_count_for(ctx.get());
            for (std::size_t i = 0; i + 1 < markerCount; i += 2)
            {
                epochengine::raylib_api::Vector2 a{};
                epochengine::raylib_api::Vector2 b{};
                if (!project_preview_vertex(mvp, markerVertices[i].position, viewport, a)
                    || !project_preview_vertex(mvp, markerVertices[i + 1].position, viewport, b))
                {
                    continue;
                }

                epochengine::raylib_api::draw_line_v(
                    a,
                    b,
                    to_raylib_color(markerVertices[i].color));
            }

            const auto objectVertices = epochengine::previewgrid::object_marker_vertices_for(ctx.get());
            for (std::size_t i = 0; i + 1 < objectVertices.size(); i += 2)
            {
                epochengine::raylib_api::Vector2 a{};
                epochengine::raylib_api::Vector2 b{};
                if (!project_preview_vertex(mvp, objectVertices[i].position, viewport, a)
                    || !project_preview_vertex(mvp, objectVertices[i + 1].position, viewport, b))
                {
                    continue;
                }

                epochengine::raylib_api::draw_line_v(
                    a,
                    b,
                    to_raylib_color(objectVertices[i].color));
            }

            epochengine::raylib_api::end_scissor_mode();
        }

    }

    export inline void raylib_resize(int w, int h)
    {
        auto& state = epochengine::raylibstate::s_raylibstate;
        const int clampedW = (std::max)(1, w);
        const int clampedH = (std::max)(1, h);
        int framebufferW = clampedW;
        int framebufferH = clampedH;
        state.width = static_cast<unsigned>(clampedW);
        state.height = static_cast<unsigned>(clampedH);

#if defined(_WIN32)
        if (state.hwnd && ::IsWindow(state.hwnd) != FALSE)
        {
            const HWND liveParent = ::GetParent(state.hwnd);
            state.dockedChildWindow =
                state.parent
                && ::IsWindow(state.parent) != FALSE
                && liveParent == state.parent;
        }
#endif

        if (epochengine::raylib_api::is_window_ready())
        {
            // This callback is routed through Raylib's owner-thread queue.
            // Keep GLFW/raylib's framebuffer bookkeeping synchronized with
            // the externally docked child dimensions before the next frame.
            const int renderWidth = epochengine::raylib_api::get_render_width();
            const int renderHeight = epochengine::raylib_api::get_render_height();
            if (renderWidth != clampedW || renderHeight != clampedH)
                epochengine::raylib_api::set_window_size(clampedW, clampedH);

            framebufferW = (std::max)(1, epochengine::raylib_api::get_render_width());
            framebufferH = (std::max)(1, epochengine::raylib_api::get_render_height());
        }

        if (state.owner_ctx)
        {
            state.owner_ctx->width = clampedW;
            state.owner_ctx->height = clampedH;
            state.owner_ctx->framebufferWidth = framebufferW;
            state.owner_ctx->framebufferHeight = framebufferH;
            if (state.owner_ctx->windowData)
                state.owner_ctx->windowData->set_size(clampedW, clampedH);
        }

        if (state.userResize)
            state.userResize(clampedW, clampedH);
    }

    export inline bool raylib_initialize(
        std::shared_ptr<core::Context> ctx,
        NativeWindowHandle parent = nullptr,
        unsigned width = 0,
        unsigned height = 0,
        std::function<void(int, int)> resizeCallback = nullptr,
        std::string title = {})
    {
        auto& st = epochengine::raylibstate::s_raylibstate;

        if (width == 0)  width = static_cast<unsigned>(core::cli::window_width);
        if (height == 0) height = static_cast<unsigned>(core::cli::window_height);

        st.width = (std::max)(1u, width);
        st.height = (std::max)(1u, height);

        if (!title.empty())
            title_storage() = std::move(title);

        // Raylib is process-global. Re-entry is valid only for the exact owner
        // that initialized the still-live native window on this render thread.
        if (st.running)
        {
            const bool sameOwner = ctx && st.owner_ctx == ctx.get();
#if defined(_WIN32)
            const bool sameThread = st.owner_thread_id == detail::current_thread_token();
#else
            const bool sameThread = st.owner_thread == detail::current_thread_token();
#endif
            if (sameOwner && sameThread && epochengine::raylib_api::is_window_ready())
                return true;

            logger::warn(
                "Raylib",
                "Rejected initialization while another Raylib context or owner thread is still active.");
            return false;
        }

#if defined(_WIN32)
        // Capture whatever context the multiplexer/docking currently has bound.
        const HDC   previousDC = detail::current_dc();
        const HGLRC previousContext = detail::current_context();
#endif

        const auto fail_initialization = [&](const char* message) -> bool
        {
            logger::warn("Raylib", message);
            if (epochengine::raylib_api::is_window_ready())
                epochengine::raylib_api::close_window();
#if defined(_WIN32)
            if (previousDC && previousContext)
                (void)detail::make_current(previousDC, previousContext);
            else
                detail::clear_current();
#endif
            st = {};
            return false;
        };

        st.owner_ctx = ctx.get();
#if defined(_WIN32)
        st.owner_thread_id = detail::current_thread_token();
#else
        st.owner_thread = detail::current_thread_token();
#endif
        st.userResize = std::move(resizeCallback);

        if (const auto backendResize =
                st.userResize.target<void(*)(int, int)>();
            backendResize && *backendResize == &raylib_resize)
        {
            // A reused Context may still expose the stable backend callback.
            // It is not an external callback and must never call itself.
            st.userResize = {};
        }

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

        epochengine::raylib_api::set_config_flags(
            static_cast<unsigned>(epochengine::raylib_api::flag_msaa_4x_hint));
        epochengine::raylib_api::set_trace_log_level(epochengine::raylib_api::log_warning);

        epochengine::raylib_api::init_window(
            static_cast<int>(st.width),
            static_cast<int>(st.height),
            title_storage().c_str());

        if (!epochengine::raylib_api::is_window_ready())
            return fail_initialization("Raylib did not create a ready native window.");

#if defined(_WIN32)
        // Raylib creates its own OpenGL context. Capture it now so we bind the right rc later.
        st.hdc = detail::current_dc();
        st.hglrc = detail::current_context();
        if (!st.hglrc)
            return fail_initialization("Failed to capture Raylib OpenGL context after initialization.");
#if defined(_DEBUG) && EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS && EPOCH_ENABLE_RAYLIB_CONFIRMATION_LOGS
        logger::info(
            "Raylib",
            epochengine::format_text(
                "Updated raylib GL context dc={:p} rc={:p}",
                static_cast<const void*>(st.hdc),
                static_cast<const void*>(st.hglrc)));
#endif

        const HWND raylibHwnd = static_cast<HWND>(epochengine::raylib_api::get_window_handle());
        const HWND parentHwnd = static_cast<HWND>(parent);
        HWND adoptedHwnd = raylibHwnd;
        for (int attempt = 0; attempt < 5 && !adoptedHwnd; ++attempt)
        {
            epochengine::raylib_api::begin_drawing();
            epochengine::raylib_api::end_drawing();
            detail::sleep_short_ms(10);
            adoptedHwnd = static_cast<HWND>(epochengine::raylib_api::get_window_handle());
        }
        if (adoptedHwnd)
        {
            detail::adopt_raylib_window(st, ctx, parentHwnd, adoptedHwnd);
        }
        else if (parentHwnd)
        {
            return fail_initialization("Failed to acquire Raylib window handle for docking.");
        }
        if (!st.hdc)
            return fail_initialization("Failed to capture Raylib window DC after initialization.");

#endif

        epochengine::raylib_api::set_target_fps(0);

        st.onResize = raylib_resize;

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

        st.running = true;
        st.renderingActive = true;
        st.cleanupIssued = false;
        st.cleanupRequested = false;
        st.currentFailureStreak = 0;
        st.currentFailureWarned = false;

        epochengine::atlasmanager::register_backend_uploader(
            epochengine::core::ContextType::RayLib,
            epochengine::raylibtextures::ensure_uploaded);

#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS && EPOCH_ENABLE_RAYLIB_CONFIRMATION_LOGS
        logger::info(
            "Raylib",
            epochengine::format_text(
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
        auto& st = epochengine::raylibstate::s_raylibstate;
        if (!st.hdc || !st.hglrc)
            return false;

        return detail::make_current(st.hdc, st.hglrc);
#else
        return linux::make_native_context_current();
#endif
    }

    export inline RaylibNativeDiagnostics raylib_native_diagnostics() noexcept
    {
        RaylibNativeDiagnostics diagnostics{};
#if defined(_WIN32)
        const auto& st = epochengine::raylibstate::s_raylibstate;
        const HWND hwnd = st.hwnd;
        const HDC currentDc = detail::current_dc();
        const HGLRC currentContext = detail::current_context();

        diagnostics.hwnd = reinterpret_cast<std::uintptr_t>(hwnd);
        diagnostics.parent = reinterpret_cast<std::uintptr_t>(hwnd ? ::GetParent(hwnd) : nullptr);
        diagnostics.dc = reinterpret_cast<std::uintptr_t>(st.hdc);
        diagnostics.glContext = reinterpret_cast<std::uintptr_t>(st.hglrc);
        diagnostics.currentDc = reinterpret_cast<std::uintptr_t>(currentDc);
        diagnostics.currentGlContext = reinterpret_cast<std::uintptr_t>(currentContext);
        diagnostics.windowFromDc = reinterpret_cast<std::uintptr_t>(
            st.hdc ? ::WindowFromDC(st.hdc) : nullptr);
        diagnostics.windowValid = hwnd && ::IsWindow(hwnd) != FALSE;
        diagnostics.windowVisible = hwnd && ::IsWindowVisible(hwnd) != FALSE;
        diagnostics.childStyle = hwnd
            && (::GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) != 0;
        diagnostics.currentMatchesExpected =
            detail::contexts_match(st.hdc, st.hglrc, currentDc, currentContext);
        diagnostics.ownerThread = st.owner_thread_id;
        diagnostics.currentThread = detail::current_thread_token();

        RECT client{};
        if (hwnd && ::GetClientRect(hwnd, &client) != FALSE)
        {
            diagnostics.clientWidth = static_cast<int>(client.right - client.left);
            diagnostics.clientHeight = static_cast<int>(client.bottom - client.top);
        }
#endif
        return diagnostics;
    }

    namespace detail
    {
        void raylib_cleanup_owner_thread(epochengine::core::Context* ctx);
    }

    export inline bool raylib_process()
    {
        auto& st = epochengine::raylibstate::s_raylibstate;
        if (st.cleanupRequested)
        {
            detail::raylib_cleanup_owner_thread(st.owner_ctx);
            return false;
        }
        if (!st.running)
            return false;

        const std::uintptr_t windowId = st.hwnd
            ? reinterpret_cast<std::uintptr_t>(st.hwnd)
            : (st.owner_ctx && st.owner_ctx->windowData
                ? reinterpret_cast<std::uintptr_t>(st.owner_ctx->windowData->hwnd)
                : 0);

        diagnostics::FrameTiming frameTimer{
            epochengine::core::ContextType::RayLib,
            windowId,
            "Raylib"
        };

#if defined(_WIN32)
        if (!raylib_make_current())
        {
            const bool windowHandleInvalid = st.hwnd && (::IsWindow(st.hwnd) == FALSE);
            const bool raylibRequestedClose = epochengine::raylib_api::window_should_close();
            const bool windowClosing =
                (st.owner_ctx && st.owner_ctx->windowData && st.owner_ctx->windowData->get_should_close())
                || !st.renderingActive
                || windowHandleInvalid
                || raylibRequestedClose;
            if (windowClosing)
            {
                st.running = false;
                return false;
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
            return false;
        }
        st.currentFailureStreak = 0;
        st.currentFailureWarned = false;
#endif

        if (epochengine::raylib_api::window_should_close())
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

            return false;
        }

#if defined(_WIN32)
        // Expect the multiplexer to have activated this backend before calling process.
        detail::debug_expect_raylib_current(st, "raylib_process");
#endif

        frameTimer.finish();
        return true;
    }

    export inline void raylib_idle_frame()
    {
        auto& st = epochengine::raylibstate::s_raylibstate;
        if (!st.running || !st.renderingActive)
            return;

#if defined(_WIN32)
        if (!raylib_make_current())
            return;
#endif

        if (!st.frameActive)
        {
            epochengine::raylib_api::begin_drawing();
            st.frameActive = true;
            st.frameInTextureMode = false;
        }

        epochengine::raylib_api::end_drawing();
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

        auto& st = epochengine::raylibstate::s_raylibstate;
        if (!st.running || !st.renderingActive)
            return;

#if defined(_WIN32)
        if (!raylib_make_current())
            return;
#endif

        detail::ensure_frame_started(st);

#if EPOCH_USE_CLEAR_COLOR
        const auto clearColor = core::clear_color_for_context(core::ContextType::RayLib);
        epochengine::raylib_api::clear_background(
            epochengine::raylib_api::Color{
                static_cast<unsigned char>(std::clamp(clearColor[0], 0.0f, 1.0f) * 255.0f),
                static_cast<unsigned char>(std::clamp(clearColor[1], 0.0f, 1.0f) * 255.0f),
                static_cast<unsigned char>(std::clamp(clearColor[2], 0.0f, 1.0f) * 255.0f),
                static_cast<unsigned char>(std::clamp(clearColor[3], 0.0f, 1.0f) * 255.0f)
            });
#endif
    }

    export inline void raylib_render_scene_preview(const std::shared_ptr<core::Context>& ctx)
    {
        auto& st = epochengine::raylibstate::s_raylibstate;
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
        auto& st = epochengine::raylibstate::s_raylibstate;
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
                epochengine::raylib_api::end_texture_mode();
            else
                epochengine::raylib_api::end_drawing();
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
        inline void raylib_cleanup_owner_thread(epochengine::core::Context* ctx)
        {
            auto& st = epochengine::raylibstate::s_raylibstate;

#if defined(_WIN32)
            const HDC   previousDC = detail::current_dc();
            const HGLRC previousContext = detail::current_context();
            const bool previousWasRaylib =
                detail::contexts_match(previousDC, previousContext, st.hdc, st.hglrc);
            const bool window_alive = (st.hwnd != nullptr) && (::IsWindow(st.hwnd) != FALSE);
#endif

            epochengine::atlasmanager::unregister_backend_uploader(
                epochengine::core::ContextType::RayLib);

#if defined(_WIN32)
            (void)raylib_make_current();
#endif

            detail::raylib_stop_rendering_backend(st);
            epochengine::raylib_api::unload_all_models();

            if (ctx && ctx->windowData)
            {
#if defined(_WIN32)
                ctx->windowData->hdc = nullptr;
                ctx->windowData->glContext = nullptr;
                ctx->windowData->usesSharedContext = false;
#endif
            }

            if (epochengine::raylib_api::is_window_ready())
            {
#if defined(_WIN32)
                if (window_alive)
                {
                    // If docked as child, detach to top-level before closing to avoid teardown deadlocks.
                    detail::promote_raylib_to_top_level(st.hwnd);
                    epochengine::raylib_api::close_window();
                }
#else
                epochengine::raylib_api::close_window();
#endif
            }

#if defined(_WIN32)
            if (st.ownsDC && st.hdc && st.hwnd)
                ::ReleaseDC(st.hwnd, st.hdc);

            // CloseWindow destroys raylib's GLFW context. Never leave that
            // deleted HGLRC current on the render thread or restore it as the
            // previous binding during a later backend replacement.
            detail::clear_current();
            if (!previousWasRaylib && previousDC && previousContext)
                (void)detail::make_current(previousDC, previousContext);
#endif

            st = {};
        }
    }

    export inline void raylib_cleanup(std::shared_ptr<core::Context> ctx)
    {
        auto& st = epochengine::raylibstate::s_raylibstate;
        if (!st.running
            && !st.owner_ctx
            && !st.cleanupRequested
            && !epochengine::raylib_api::is_window_ready())
        {
            return;
        }
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
        epochengine::raylib_api::set_window_title(title_storage().c_str());
    }

    export inline int raylib_get_width()
    {
        return static_cast<int>(epochengine::raylibstate::s_raylibstate.width);
    }

    export inline int raylib_get_height()
    {
        return static_cast<int>(epochengine::raylibstate::s_raylibstate.height);
    }

    export inline bool raylib_is_running() noexcept
    {
        return epochengine::raylibstate::s_raylibstate.running;
    }

    export inline epochengine::raylib_api::Vector2 raylib_get_mouse_position()
    {
        return epochengine::raylib_api::get_mouse_position();
    }

    export inline NativeWindowHandle raylib_get_native_window()
    {
        return epochengine::raylibstate::s_raylibstate.hwnd;
    }
}

#endif // EPOCH_USING_RAYLIB
