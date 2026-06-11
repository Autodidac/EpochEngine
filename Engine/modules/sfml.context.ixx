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

 // sfml.context.ixx
module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

// -----------------------------------------------------------------------------
// Global module fragment: macros + C headers MUST live here.
// -----------------------------------------------------------------------------
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <wingdi.h>
#  ifdef min
#    undef min
#  endif
#  ifdef max
#    undef max
#  endif
#endif

#include <include/engine.config.hpp>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/ContextSettings.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/WindowEnums.hpp>
#include <SFML/Graphics.hpp>
#include <glad/glad.h>

export module sfml.context;

import core.context;
import core.commandline;
import context.type;
import context.window;
import context.commandqueue;
import context.multiplexer;
import atlas.manager;
import atlas.texture;
import image.writer;
import sfml.state;
import sfml.textures;
import engine.diagnostics;
import core.logger;
import engine.telemetry;
import render.preview_grid;


export namespace epochnamespace::sfmlcontext
{
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)

    // SFML NOTE:
    // - SFML's default RenderTarget path uses legacy/fixed-function OpenGL calls.
    // - Requesting a Core profile context will cause GL_INVALID_OPERATION spam.
    // - Let SFML own context activation (setActive). Do NOT wglMakeCurrent manually.

    struct SFMLState
    {
        std::unique_ptr<sf::RenderWindow> window{};

#if defined(_WIN32)
        HWND  parent = nullptr;
        HWND  hwnd = nullptr;
        HDC   hdc = nullptr;   // informational / optional
        HGLRC glContext = nullptr;   // informational / optional
#endif

        unsigned int width = 400;
        unsigned int height = 300;
        bool running = false;
        bool gpuAtlasesReleased = false;
        std::function<void(int, int)> onResize{};
    };

    inline SFMLState sfmlcontext{};

    inline void capture_frame_if_requested(const int width, const int height, const std::uintptr_t windowId)
    {
        const auto capturePath = core::cli::reserve_capture_path("sfml", windowId);
        if (capturePath.empty() || !sfmlcontext.window || width <= 0 || height <= 0)
            return;

        sf::Texture frameTexture{};
        if (!frameTexture.resize(sf::Vector2u{
            static_cast<unsigned int>(width),
            static_cast<unsigned int>(height) }))
        {
            logger::warn("SFML", "Failed to allocate SFML capture texture.");
            return;
        }

        frameTexture.update(*sfmlcontext.window);
        const sf::Image image = frameTexture.copyToImage();
        const auto* sourcePixels = image.getPixelsPtr();
        if (!sourcePixels)
        {
            logger::warn("SFML", "Failed to read SFML window pixels for capture.");
            return;
        }

        std::vector<std::uint8_t> pixels(
            sourcePixels,
            sourcePixels + (static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u));

        if (a_writeImage(capturePath, pixels, width, height, false))
            logger::info("SFML", "Captured frame to " + capturePath.string());
        else
            logger::warn("SFML", "Failed to write capture to " + capturePath.string());
    }

    inline void refresh_dimensions(const std::shared_ptr<core::Context>& ctx) noexcept
    {
        if (!ctx) return;

        ctx->width = static_cast<int>((std::max)(1u, sfmlcontext.width));
        ctx->height = static_cast<int>((std::max)(1u, sfmlcontext.height));

        ctx->virtualWidth = ctx->width;
        ctx->virtualHeight = ctx->height;

        ctx->framebufferWidth = ctx->width;
        ctx->framebufferHeight = ctx->height;
    }

    inline void release_sfml_gpu_atlases_active() noexcept
    {
        if (sfmlcontext.gpuAtlasesReleased)
            return;

        clear_gpu_atlases();
        sfmlcontext.gpuAtlasesReleased = true;
    }

    namespace detail
    {
        [[nodiscard]] inline bool can_touch_window_gl() noexcept
        {
            if (!sfmlcontext.window || !sfmlcontext.window->isOpen())
                return false;

#if defined(_WIN32)
            if (sfmlcontext.hwnd && (::IsWindow(sfmlcontext.hwnd) == FALSE))
                return false;
#endif

            return true;
        }

        inline void deactivate_if_possible() noexcept
        {
            if (!can_touch_window_gl())
                return;

            (void)sfmlcontext.window->setActive(false);
        }

        [[nodiscard]] inline sf::Color to_sfml_color(
            const epochnamespace::previewgrid::Vec3& color) noexcept
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

        [[nodiscard]] inline bool project_preview_vertex(
            const epochnamespace::previewgrid::Mat4& mvp,
            const epochnamespace::previewgrid::Vec3& position,
            const core::RenderViewport& viewport,
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

        inline void render_scene_preview(const std::shared_ptr<core::Context>& ctx)
        {
            if (!ctx || !sfmlcontext.window)
                return;

            const auto viewport = ctx->scene_viewport();
            if (!viewport.valid() || ctx->scene_preview_mode() != core::ScenePreviewMode::Editor)
                return;

            const auto windowSize = sfmlcontext.window->getSize();
            if (windowSize.x == 0u || windowSize.y == 0u)
                return;

            const float invWidth = 1.0f / static_cast<float>(windowSize.x);
            const float invHeight = 1.0f / static_cast<float>(windowSize.y);
            const float viewportLeft = (std::clamp)(viewport.x * invWidth, 0.0f, 1.0f);
            const float viewportTop = (std::clamp)(viewport.y * invHeight, 0.0f, 1.0f);
            const float viewportWidth = (std::clamp)(viewport.width * invWidth, 0.0f, 1.0f - viewportLeft);
            const float viewportHeight = (std::clamp)(viewport.height * invHeight, 0.0f, 1.0f - viewportTop);

            const auto previousView = sfmlcontext.window->getView();
            sf::View previewView{ sf::FloatRect(
                { 0.0f, 0.0f },
                {
                    static_cast<float>(viewport.width),
                    static_cast<float>(viewport.height) }) };
            previewView.setViewport(sf::FloatRect(
                { viewportLeft, viewportTop },
                { viewportWidth, viewportHeight }));
            sfmlcontext.window->setView(previewView);

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
            sfmlcontext.window->draw(background, renderStates);

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
                lines.append(sf::Vertex(b, to_sfml_color(vertices[secondIndex].color)));
            }

            if (lines.getVertexCount() > 0)
                sfmlcontext.window->draw(lines, renderStates);

            sf::VertexArray solids(sf::PrimitiveType::Triangles);
            const auto solidVertices = epochnamespace::previewgrid::object_solid_vertices_for(ctx.get());
            for (std::size_t i = 0; i + 2 < solidVertices.size(); i += 3)
            {
                sf::Vector2f a{};
                sf::Vector2f b{};
                sf::Vector2f c{};
                if (!project_preview_vertex(mvp, solidVertices[i].position, viewport, a)
                    || !project_preview_vertex(mvp, solidVertices[i + 1].position, viewport, b)
                    || !project_preview_vertex(mvp, solidVertices[i + 2].position, viewport, c))
                {
                    continue;
                }

                a.x -= static_cast<float>(viewport.x);
                a.y -= static_cast<float>(viewport.y);
                b.x -= static_cast<float>(viewport.x);
                b.y -= static_cast<float>(viewport.y);
                c.x -= static_cast<float>(viewport.x);
                c.y -= static_cast<float>(viewport.y);

                const auto color = to_sfml_color(solidVertices[i].color);
                solids.append(sf::Vertex(a, color));
                solids.append(sf::Vertex(b, color));
                solids.append(sf::Vertex(c, color));
            }

            if (solids.getVertexCount() > 0)
                sfmlcontext.window->draw(solids, renderStates);

            const auto appendPreviewLines = [&](const auto& lineVertices, std::size_t vertexCount)
            {
                for (std::size_t i = 0; i + 1 < vertexCount; i += 2)
                {
                    sf::Vector2f a{};
                    sf::Vector2f b{};
                    if (!project_preview_vertex(mvp, lineVertices[i].position, viewport, a)
                        || !project_preview_vertex(mvp, lineVertices[i + 1].position, viewport, b))
                    {
                        continue;
                    }

                    a.x -= static_cast<float>(viewport.x);
                    a.y -= static_cast<float>(viewport.y);
                    b.x -= static_cast<float>(viewport.x);
                    b.y -= static_cast<float>(viewport.y);

                    lines.append(sf::Vertex(a, to_sfml_color(lineVertices[i].color)));
                    lines.append(sf::Vertex(b, to_sfml_color(lineVertices[i].color)));
                }
            };

            lines.clear();
            const auto markerVertices = epochnamespace::previewgrid::look_marker_vertices_for(ctx.get());
            appendPreviewLines(
                markerVertices,
                epochnamespace::previewgrid::look_marker_vertex_count_for(ctx.get()));
            const auto objectVertices = epochnamespace::previewgrid::object_marker_vertices_for(ctx.get());
            appendPreviewLines(objectVertices, objectVertices.size());

            if (lines.getVertexCount() > 0)
                sfmlcontext.window->draw(lines, renderStates);

            sfmlcontext.window->setView(previousView);
        }
    }

    inline bool sfml_initialize(
        std::shared_ptr<core::Context> ctx,
#if defined(_WIN32)
        HWND parentWnd = nullptr,
#else
        void* parentWnd = nullptr,
#endif
        unsigned int w = 400,
        unsigned int h = 300,
        std::function<void(int, int)> onResize = nullptr,
        std::string windowTitle = {})
    {
        const unsigned int clampedWidth = (std::max)(1u, w);
        const unsigned int clampedHeight = (std::max)(1u, h);

        sfmlcontext.width = clampedWidth;
        sfmlcontext.height = clampedHeight;

#if defined(_WIN32)
        HWND hostWnd = parentWnd;
        HWND dockParent = hostWnd ? ::GetParent(hostWnd) : nullptr;
        sfmlcontext.parent = dockParent ? dockParent : hostWnd;
#else
        (void)parentWnd;
#endif

        std::weak_ptr<core::Context> weakCtx = ctx;
        auto userResize = std::move(onResize);

        sfmlcontext.onResize =
            [weakCtx, userResize = std::move(userResize)](int width, int height) mutable
            {
                sfmlcontext.width = static_cast<unsigned int>((std::max)(1, width));
                sfmlcontext.height = static_cast<unsigned int>((std::max)(1, height));

                if (sfmlcontext.window)
                    sfmlcontext.window->setView(sf::View(
                        sf::FloatRect(
                            { 0.0f, 0.0f },
                            {
                                static_cast<float>(sfmlcontext.width),
                                static_cast<float>(sfmlcontext.height) })));

                auto locked = weakCtx.lock();
                refresh_dimensions(locked);

                state::s_sfmlstate.set_dimensions(
                    static_cast<int>(sfmlcontext.width),
                    static_cast<int>(sfmlcontext.height));

                if (userResize)
                    userResize(static_cast<int>(sfmlcontext.width), static_cast<int>(sfmlcontext.height));
            };

        if (ctx)
            ctx->onResize = sfmlcontext.onResize;

        // IMPORTANT: request a compatibility-ish context.
        // Using 2.1 is the safest choice for SFML's default RenderTarget path.
        sf::ContextSettings settings{};
        settings.majorVersion = 2;
        settings.minorVersion = 1;
        settings.attributeFlags = sf::ContextSettings::Default;

        if (windowTitle.empty() && ctx && ctx->windowData && !ctx->windowData->titleNarrow.empty())
            windowTitle = ctx->windowData->titleNarrow;
        if (windowTitle.empty())
            windowTitle = "SFML Window";

                {
            sf::VideoMode mode(sf::Vector2u{ sfmlcontext.width, sfmlcontext.height }, 32u);
            sfmlcontext.window = std::make_unique<sf::RenderWindow>(
                mode, windowTitle, sf::Style::Default, sf::State::Windowed, settings);
        }

        if (!sfmlcontext.window || !sfmlcontext.window->isOpen())
        {
            logger::error("SFML", "Failed to create SFML window");
            return false;
        }

        sfmlcontext.window->setVerticalSyncEnabled(false);
        sfmlcontext.window->setFramerateLimit(0);

        auto* windowPtr = sfmlcontext.window.get();

        if (ctx && ctx->windowData)
        {
            ctx->windowData->sfml_window = windowPtr;
            ctx->windowData->set_size(
                static_cast<int>(sfmlcontext.width),
                static_cast<int>(sfmlcontext.height));
        }

        state::s_sfmlstate.window.sfml_window = windowPtr;

#if defined(_WIN32)
        sfmlcontext.hwnd = static_cast<HWND>(sfmlcontext.window->getNativeHandle());
        sfmlcontext.hdc = GetDC(sfmlcontext.hwnd);

#if !defined(EPOCH_MAIN_HEADLESS)
                if (ctx)
        {
            ctx->hwnd = sfmlcontext.hwnd;
            ctx->native_window = sfmlcontext.hwnd;
        }

        if (ctx && ctx->windowData)
        {
            ctx->windowData->hwnd = sfmlcontext.hwnd;
            ctx->windowData->host_hwnd = hostWnd;
            ctx->windowData->hwndChild = sfmlcontext.hwnd;
            ctx->windowData->set_size(
                static_cast<int>(sfmlcontext.width),
                static_cast<int>(sfmlcontext.height));
        }
#endif

        // Ensure the SFML context is current *on this thread* before capturing HGLRC.
        if (!sfmlcontext.window->setActive(true))
        {
            logger::error("SFML", "Failed to activate SFML window for context capture");
            return false;
        }

        sfmlcontext.glContext = wglGetCurrentContext();
        if (!sfmlcontext.glContext)
        {
            logger::error("SFML", "Failed to get OpenGL context");
            sfmlcontext.window->setActive(false);
            return false;
        }

        // Detach for now; render thread will reactivate per-frame.
        sfmlcontext.window->setActive(false);

                if (sfmlcontext.parent)
        {
            SetParent(sfmlcontext.hwnd, sfmlcontext.parent);

            LONG_PTR style = GetWindowLongPtr(sfmlcontext.hwnd, GWL_STYLE);
            style &= ~WS_OVERLAPPEDWINDOW;
            style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
            SetWindowLongPtr(sfmlcontext.hwnd, GWL_STYLE, style);

            epochnamespace::core::MakeDockable(sfmlcontext.hwnd, sfmlcontext.parent);

            RECT client{};
            HWND sizeSource = hostWnd ? hostWnd : sfmlcontext.parent;
            GetClientRect(sizeSource, &client);
            const int width = static_cast<int>((std::max)(static_cast<LONG>(1), client.right - client.left));
            const int height = static_cast<int>((std::max)(static_cast<LONG>(1), client.bottom - client.top));

            sfmlcontext.width = static_cast<unsigned int>(width);
            sfmlcontext.height = static_cast<unsigned int>(height);

            SetWindowPos(
                sfmlcontext.hwnd, nullptr, 0, 0, width, height,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

            // Keep the SFML render target size aligned with the dock slot before
            // the first display so startup does not depend on a resize event.
            sfmlcontext.window->setSize(sf::Vector2u(static_cast<unsigned>(width), static_cast<unsigned>(height)));
            RedrawWindow(
                sfmlcontext.hwnd, nullptr, nullptr,
                RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

            if (hostWnd && hostWnd != sfmlcontext.hwnd && ::IsWindow(hostWnd) != FALSE)
                ShowWindow(hostWnd, SW_HIDE);

            if (sfmlcontext.onResize)
                sfmlcontext.onResize(width, height);

#if !defined(EPOCH_MAIN_HEADLESS)
            if (ctx && ctx->windowData)
                ctx->windowData->set_size(width, height);
#endif

            // The grid was computed against the placeholder host before SFML swapped in its child HWND.
            // Trigger one parent relayout so the tracked render HWND lands in the correct dock slot.
            PostMessage(sfmlcontext.parent, WM_SIZE, 0, MAKELPARAM(width, height));
        }
#endif

        refresh_dimensions(ctx);

        state::s_sfmlstate.set_dimensions(
            static_cast<int>(sfmlcontext.width),
            static_cast<int>(sfmlcontext.height));

        state::s_sfmlstate.running = true;
        sfmlcontext.running = true;
        sfmlcontext.gpuAtlasesReleased = false;

        logger::info(
            "SFML",
            std::string("Initialized ")
                + std::to_string(sfmlcontext.width)
                + "x"
                + std::to_string(sfmlcontext.height));

        atlasmanager::register_backend_uploader(
            core::ContextType::SFML,
            [](const TextureAtlas& atlas)
            {
                // IMPORTANT: uploader must assume the SFML context is current in sfml_process.
                sfmlcontext::ensure_uploaded(atlas);
            });

        return true;
    }

    inline bool sfml_should_close()
    {
        return !sfmlcontext.window || !sfmlcontext.window->isOpen();
    }

    inline bool sfml_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        if (!sfmlcontext.running || !sfmlcontext.window || !sfmlcontext.window->isOpen())
            return false;

        const core::ContextType backendType = ctx ? ctx->type : core::ContextType::SFML;

        std::uintptr_t windowId = 0u;
#if defined(_WIN32)
        if (ctx && ctx->windowData)
            windowId = reinterpret_cast<std::uintptr_t>(ctx->windowData->hwnd);
        else
            windowId = reinterpret_cast<std::uintptr_t>(sfmlcontext.hwnd);
#else
        if (ctx && ctx->windowData)
            windowId = reinterpret_cast<std::uintptr_t>(ctx->windowData->hwnd);
        else
            windowId = 0u;
#endif

        diagnostics::FrameTiming frameTimer{ backendType, windowId, "SFML" };

        // If the HWND is already dead (e.g., external teardown), bail before any GL calls.
#if defined(_WIN32)
        if (sfmlcontext.hwnd && ::IsWindow(sfmlcontext.hwnd) == FALSE)
        {
            sfmlcontext.running = false;
            state::s_sfmlstate.running = false;
            return false;
        }
#endif

        // Let SFML own activation. Do NOT call wglMakeCurrent manually.
        if (!sfmlcontext.window->setActive(true))
        {
#if defined(_WIN32)
            if (!(sfmlcontext.hwnd && ::IsWindow(sfmlcontext.hwnd) != FALSE))
            {
                sfmlcontext.running = false;
                state::s_sfmlstate.running = false;
                return false;
            }
#endif
            logger::error("SFML", "Failed to activate SFML window during render");
            sfmlcontext.running = false;
            state::s_sfmlstate.running = false;
            return false;
        }

        const auto renderFlags = queue.render_flags_snapshot();
        const bool hasSfmlDraws =
            (renderFlags & static_cast<std::uint8_t>(core::RenderPath::SFML)) != 0u;
        const bool hasOpenGLDraws =
            (renderFlags & static_cast<std::uint8_t>(core::RenderPath::OpenGL)) != 0u;
        const bool hasVulkanDraws =
            (renderFlags & static_cast<std::uint8_t>(core::RenderPath::Vulkan)) != 0u;
        const bool hasQueuedCommands = queue.depth() > 0;
        const bool useSharedScenePreview =
            ctx
            && ctx->scene_viewport().valid()
            && ctx->scene_preview_mode() == core::ScenePreviewMode::Editor;
        const bool useOpenGLPath =
            !useSharedScenePreview
            && (hasOpenGLDraws || (hasQueuedCommands && !hasSfmlDraws && !hasVulkanDraws));
        const bool shouldResetSfmlState = hasSfmlDraws || useSharedScenePreview;

        if (shouldResetSfmlState)
        {
            // Reset before doing any SFML draw calls.
            sfmlcontext.window->resetGLStates();
        }

        // If uploads use OpenGL, they must run while the SFML context is current.
        atlasmanager::process_pending_uploads(core::ContextType::SFML);

        // Reset again in case uploads touched state.
        if (shouldResetSfmlState)
            sfmlcontext.window->resetGLStates();

        while (const auto event = sfmlcontext.window->pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                release_sfml_gpu_atlases_active();
                sfmlcontext.window->close();
                sfmlcontext.running = false;
                state::s_sfmlstate.running = false;
                state::s_sfmlstate.mark_should_close(true);
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                const int w = static_cast<int>((std::max)(1u, resized->size.x));
                const int h = static_cast<int>((std::max)(1u, resized->size.y));
                if (sfmlcontext.onResize) sfmlcontext.onResize(w, h);
            }
        }

        if (!sfmlcontext.running || !sfmlcontext.window->isOpen())
        {
            detail::deactivate_if_possible();
            return false;
        }

        refresh_dimensions(ctx);

        const int framebufferWidth = ctx
            ? ctx->framebufferWidth
            : static_cast<int>((std::max)(1u, sfmlcontext.width));
        const int framebufferHeight = ctx
            ? ctx->framebufferHeight
            : static_cast<int>((std::max)(1u, sfmlcontext.height));

        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(framebufferWidth),
            telemetry::RendererTelemetryTags{ backendType, windowId, "width" });

        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(framebufferHeight),
            telemetry::RendererTelemetryTags{ backendType, windowId, "height" });
#if EPOCH_USE_CLEAR_COLOR
        const auto clearColor = useSharedScenePreview
            ? epochnamespace::previewgrid::kClearColor
            : core::clear_color_for_context(core::ContextType::SFML);
        const auto r = static_cast<std::uint8_t>(clearColor[0] * 255.0f);
        const auto g = static_cast<std::uint8_t>(clearColor[1] * 255.0f);
        const auto b = static_cast<std::uint8_t>(clearColor[2] * 255.0f);

        if (useOpenGLPath)
        {
            glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        else
        {
            sfmlcontext.window->clear(sf::Color(r, g, b));
        }
#endif

        if (useSharedScenePreview)
            detail::render_scene_preview(ctx);

        queue.drain();

        if (!sfmlcontext.running || !sfmlcontext.window->isOpen())
        {
            detail::deactivate_if_possible();
            return false;
        }

        sfmlcontext.window->display();
        capture_frame_if_requested(framebufferWidth, framebufferHeight, windowId);

        frameTimer.finish();

        detail::deactivate_if_possible();
        return sfmlcontext.running;
    }

    inline void sfml_cleanup(std::shared_ptr<epochnamespace::core::Context>& ctx)
    {
        // Stop new uploads immediately.
        atlasmanager::unregister_backend_uploader(core::ContextType::SFML);

        if (ctx && ctx->windowData)
            ctx->windowData->sfml_window = nullptr;

        state::s_sfmlstate.window.sfml_window = nullptr;
        state::s_sfmlstate.running = false;
        sfmlcontext.running = false;

        if (sfmlcontext.window && sfmlcontext.window->isOpen())
        {
            bool canTouchGl = true;
#if defined(_WIN32)
            canTouchGl = sfmlcontext.hwnd && (::IsWindow(sfmlcontext.hwnd) != FALSE);
#endif
            if (canTouchGl && !sfmlcontext.gpuAtlasesReleased && sfmlcontext.window->setActive(true))
            {
                release_sfml_gpu_atlases_active();
                detail::deactivate_if_possible();
            }

            sfmlcontext.window->close();
        }
        sfmlcontext.window.reset();
        sfmlcontext.gpuAtlasesReleased = false;

#if defined(_WIN32)
        if (sfmlcontext.hdc && sfmlcontext.hwnd)
            ReleaseDC(sfmlcontext.hwnd, sfmlcontext.hdc);

        sfmlcontext.hwnd = nullptr;
        sfmlcontext.hdc = nullptr;
        sfmlcontext.glContext = nullptr;
        sfmlcontext.parent = nullptr;
#endif
    }

    inline bool SFMLIsRunning(std::shared_ptr<core::Context> ctx)
    {
        (void)ctx;
        return sfmlcontext.running;
    }

#endif // EPOCH_USING_SFML
} // namespace epochnamespace::sfmlcontext
