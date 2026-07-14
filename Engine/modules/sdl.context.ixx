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
 // sdl.context.ixx
module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

// -----------------------------------------------------------------------------
// Global module fragment: macros + C headers MUST live here.
// -----------------------------------------------------------------------------

// SDL wants this defined BEFORE including SDL headers.
#define SDL_MAIN_HANDLED

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>   // HWND, RECT, LONG_PTR, SetParent, GetWindowLongPtr, etc.
#  ifdef min
#    undef min
#  endif
#  ifdef max
#    undef max
#  endif
#endif

#include <include/engine.config.hpp> // for EPOCH_USING Macros

#include <chrono>

#include <SDL3/SDL.h>    // keep in GMF because itâ€™s a C header with macros

export module sdl.context;

// Project
import engine.platform;              // fine, but does NOT replace windows.h for this TU
import core.context;
import context.window;
import context.commandqueue;
import context.control;
import context.type;
import atlas.manager;
import atlas.texture;
import sdl.state;
import sdl.renderer;
import sdl.textures;
import context.multiplexer;   // MakeDockable(...)
import core.commandline;
import core.logger;
import image.writer;
import engine.diagnostics;
import engine.telemetry;
import render.preview_grid;
import package.registry;

// Std
//import <chrono>;  // as include for intellisense stability, this can probably be changed in the future

export namespace epochnamespace::sdlcontext
{
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)

    inline void capture_frame_if_requested(const int width, const int height, const std::uintptr_t windowId)
    {
        const auto capturePath = core::cli::reserve_capture_path("sdl", windowId);
        if (capturePath.empty() || width <= 0 || height <= 0 || !sdl_renderer.renderer)
            return;

        SDL_Surface* surface = SDL_RenderReadPixels(sdl_renderer.renderer, nullptr);
        if (!surface)
        {
            check_sdl_error("SDL_RenderReadPixels");
            return;
        }

        const bool saved = SDL_SaveBMP(surface, capturePath.string().c_str());
        SDL_DestroySurface(surface);

        if (saved)
            logger::info("SDL", "Captured frame to " + capturePath.string());
        else
            logger::warn("SDL", "Failed to write capture to " + capturePath.string());
    }

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
    };

    inline SDLState sdlcontext{};

    namespace detail
    {
        inline void destroy_arcade_screen_preview_target() noexcept;
    }

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

        detail::destroy_arcade_screen_preview_target();

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

    namespace detail
    {
        [[nodiscard]] inline Uint8 to_sdl_channel(float value) noexcept
        {
            const float scaled = (std::clamp)(value, 0.0f, 1.0f) * 255.0f;
            return static_cast<Uint8>(scaled);
        }

        [[nodiscard]] inline bool project_preview_vertex(
            const epochnamespace::previewgrid::Mat4& mvp,
            const epochnamespace::previewgrid::Vec3& position,
            const core::RenderViewport& viewport,
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



        struct SdlArcadeScreenPreviewTarget
        {
            SDL_Texture* texture = nullptr;
            int width = 0;
            int height = 0;
            std::uint64_t frame = 0;
        };

        inline SdlArcadeScreenPreviewTarget& arcade_screen_preview_target() noexcept
        {
            static SdlArcadeScreenPreviewTarget target{};
            return target;
        }

        inline void destroy_arcade_screen_preview_target() noexcept
        {
            SdlArcadeScreenPreviewTarget& target = arcade_screen_preview_target();
            if (target.texture)
                SDL_DestroyTexture(target.texture);
            target = {};
        }

        [[nodiscard]] inline bool ensure_arcade_screen_preview_target(SDL_Renderer* renderer) noexcept
        {
            if (!renderer)
                return false;

            SdlArcadeScreenPreviewTarget& target = arcade_screen_preview_target();
            const int width = static_cast<int>(epoch::package_registry::engine_arcade_render_texture_width());
            const int height = static_cast<int>(epoch::package_registry::engine_arcade_render_texture_height());
            if (width <= 0 || height <= 0)
                return false;

            if (target.texture && target.width == width && target.height == height)
                return true;

            destroy_arcade_screen_preview_target();
            target.texture = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_RGBA32,
                SDL_TEXTUREACCESS_TARGET,
                width,
                height);
            if (!target.texture)
            {
                check_sdl_error("SDL_CreateTexture engine_arcade.screen");
                return false;
            }

            (void)SDL_SetTextureBlendMode(target.texture, SDL_BLENDMODE_BLEND);
            (void)SDL_SetTextureScaleMode(target.texture, SDL_SCALEMODE_NEAREST);
            target.width = width;
            target.height = height;
            return true;
        }

        inline void fill_arcade_preview_rect(SDL_Renderer* renderer, int x, int y, int width, int height, Uint8 r, Uint8 g, Uint8 b) noexcept
        {
            if (!renderer || width <= 0 || height <= 0)
                return;
            const SDL_FRect rect{
                static_cast<float>(x),
                static_cast<float>(y),
                static_cast<float>(width),
                static_cast<float>(height)
            };
            (void)SDL_SetRenderDrawColor(renderer, r, g, b, 255u);
            (void)SDL_RenderFillRect(renderer, &rect);
        }

        inline void render_arcade_attract_pattern(SDL_Renderer* renderer) noexcept
        {
            SdlArcadeScreenPreviewTarget& target = arcade_screen_preview_target();
            if (!renderer || !target.texture || target.width <= 0 || target.height <= 0)
                return;

            SDL_Texture* previousTarget = SDL_GetRenderTarget(renderer);
            if (!SDL_SetRenderTarget(renderer, target.texture))
            {
                check_sdl_error("SDL_SetRenderTarget engine_arcade.screen");
                state::get_sdl_state().renderFaulted = true;
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
                check_sdl_error("SDL_SetRenderTarget engine_arcade.screen restore");
                state::get_sdl_state().renderFaulted = true;
            }
        }

        inline void render_engine_arcade_sampled_surface_preview(
            const std::shared_ptr<core::Context>& ctx,
            const epochnamespace::previewgrid::Mat4& mvp,
            const core::RenderViewport& viewport) noexcept
        {
            SDL_Renderer* const renderer = sdl_renderer.renderer;
            if (!ctx || !renderer)
                return;

            const auto markers = epochnamespace::previewgrid::sampled_render_surface_markers_for(ctx.get());
            if (markers.empty() || !ensure_arcade_screen_preview_target(renderer))
                return;

            render_arcade_attract_pattern(renderer);
            SdlArcadeScreenPreviewTarget& target = arcade_screen_preview_target();
            for (const auto& marker : markers)
            {
                const float halfX = (std::max)(std::abs(marker.scale.x) * 0.5f, 0.25f);
                const float halfY = (std::max)(std::abs(marker.scale.y) * 0.5f, 0.18f);
                const float z = marker.position.z - (std::max)(std::abs(marker.scale.z) * 0.5f, 0.018f) - 0.012f;
                const epochnamespace::previewgrid::Vec3 world[4]{
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
                if (!SDL_RenderGeometry(renderer, target.texture, vertices, 4, indices, 6))
                {
                    check_sdl_error("SDL_RenderGeometry engine_arcade.screen");
                    state::get_sdl_state().renderFaulted = true;
                    return;
                }
            }
        }
        inline void render_scene_preview(const std::shared_ptr<core::Context>& ctx)
        {
            if (!ctx || !sdl_renderer.renderer)
                return;

            const auto viewport = ctx->scene_viewport();
            if (!viewport.valid() || ctx->scene_preview_mode() != core::ScenePreviewMode::Editor)
                return;

            SDL_Rect clipRect{ viewport.x, viewport.y, viewport.width, viewport.height };
            (void)SDL_SetRenderClipRect(sdl_renderer.renderer, &clipRect);

            const auto clearColor = epochnamespace::previewgrid::kClearColor;
            const SDL_FRect background{
                static_cast<float>(viewport.x),
                static_cast<float>(viewport.y),
                static_cast<float>(viewport.width),
                static_cast<float>(viewport.height)
            };

            (void)SDL_SetRenderDrawColor(
                sdl_renderer.renderer,
                to_sdl_channel(clearColor[0]),
                to_sdl_channel(clearColor[1]),
                to_sdl_channel(clearColor[2]),
                to_sdl_channel(clearColor[3]));
            (void)SDL_RenderFillRect(sdl_renderer.renderer, &background);

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
                    sdl_renderer.renderer,
                    to_sdl_channel(color.x),
                    to_sdl_channel(color.y),
                    to_sdl_channel(color.z),
                    255u);
                if (!SDL_RenderLine(sdl_renderer.renderer, ax, ay, bx, by))
                {
                    check_sdl_error("SDL_RenderLine");
                    state::get_sdl_state().renderFaulted = true;
                    break;
                }
            }

            const auto solidVertices = epochnamespace::previewgrid::object_solid_vertices_for(ctx.get());
            for (std::size_t i = 0; i + 2 < solidVertices.size(); i += 3)
            {
                float ax = 0.0f;
                float ay = 0.0f;
                float bx = 0.0f;
                float by = 0.0f;
                float cx = 0.0f;
                float cy = 0.0f;
                if (!project_preview_vertex(mvp, solidVertices[i].position, viewport, ax, ay)
                    || !project_preview_vertex(mvp, solidVertices[i + 1].position, viewport, bx, by)
                    || !project_preview_vertex(mvp, solidVertices[i + 2].position, viewport, cx, cy))
                {
                    continue;
                }

                const auto color = solidVertices[i].color;
                const SDL_FColor faceColor{
                    (std::clamp)(color.x, 0.0f, 1.0f),
                    (std::clamp)(color.y, 0.0f, 1.0f),
                    (std::clamp)(color.z, 0.0f, 1.0f),
                    1.0f
                };
                SDL_Vertex triangle[3]{};
                triangle[0].position = SDL_FPoint{ ax, ay };
                triangle[0].color = faceColor;
                triangle[0].tex_coord = SDL_FPoint{ 0.0f, 0.0f };
                triangle[1].position = SDL_FPoint{ bx, by };
                triangle[1].color = faceColor;
                triangle[1].tex_coord = SDL_FPoint{ 0.0f, 0.0f };
                triangle[2].position = SDL_FPoint{ cx, cy };
                triangle[2].color = faceColor;
                triangle[2].tex_coord = SDL_FPoint{ 0.0f, 0.0f };
                if (!SDL_RenderGeometry(sdl_renderer.renderer, nullptr, triangle, 3, nullptr, 0))
                {
                    check_sdl_error("SDL_RenderGeometry");
                    state::get_sdl_state().renderFaulted = true;
                    break;
                }
            }

            render_engine_arcade_sampled_surface_preview(ctx, mvp, viewport);

            const auto drawPreviewLines = [&](const auto& lineVertices, std::size_t vertexCount) noexcept
            {
                for (std::size_t i = 0; i + 1 < vertexCount; i += 2)
                {
                    float ax = 0.0f;
                    float ay = 0.0f;
                    float bx = 0.0f;
                    float by = 0.0f;
                    if (!project_preview_vertex(mvp, lineVertices[i].position, viewport, ax, ay)
                        || !project_preview_vertex(mvp, lineVertices[i + 1].position, viewport, bx, by))
                    {
                        continue;
                    }

                    const auto color = lineVertices[i].color;
                    (void)SDL_SetRenderDrawColor(
                        sdl_renderer.renderer,
                        to_sdl_channel(color.x),
                        to_sdl_channel(color.y),
                        to_sdl_channel(color.z),
                        255u);
                    if (!SDL_RenderLine(sdl_renderer.renderer, ax, ay, bx, by))
                    {
                        check_sdl_error("SDL_RenderLine");
                        state::get_sdl_state().renderFaulted = true;
                        break;
                    }
                }
            };

            const auto markerVertices = epochnamespace::previewgrid::look_marker_vertices_for(ctx.get());
            drawPreviewLines(
                markerVertices,
                epochnamespace::previewgrid::look_marker_vertex_count_for(ctx.get()));

            const auto objectVertices = epochnamespace::previewgrid::object_marker_vertices_for(ctx.get());
            drawPreviewLines(objectVertices, objectVertices.size());

            (void)SDL_SetRenderClipRect(sdl_renderer.renderer, nullptr);
        }
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
        const bool hostedWindow = hostWnd && ::IsWindow(hostWnd) != FALSE;
        HWND dockParent = hostedWindow ? ::GetParent(hostWnd) : nullptr;
        sdlcontext.parent = dockParent;
#endif

        auto& sharedState = state::get_sdl_state();
        sharedState.renderFaulted = false;

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
            logger::error("SDL", std::string("SDL_Init failed: ") + SDL_GetError());
            return false;
        }

        if (windowTitle.empty())
            windowTitle = "SDL3 Window";

        SDL_PropertiesID props = SDL_CreateProperties();
        if (!props)
        {
            logger::error("SDL", std::string("SDL_CreateProperties failed: ") + SDL_GetError());
            SDL_Quit();
            return false;
        }

        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, windowTitle.c_str());
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, sdlcontext.width);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, sdlcontext.height);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
#if defined(_WIN32)
        if (hostedWindow)
            SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
#endif

        sdlcontext.window = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);

        if (!sdlcontext.window)
        {
            logger::error("SDL", std::string("SDL_CreateWindowWithProperties failed: ") + SDL_GetError());
            SDL_Quit();
            return false;
        }

#if defined(_WIN32)
        {
            SDL_PropertiesID windowProps = SDL_GetWindowProperties(sdlcontext.window);
            if (!windowProps)
            {
                logger::error("SDL", std::string("SDL_GetWindowProperties failed: ") + SDL_GetError());
                SDL_DestroyWindow(sdlcontext.window);
                SDL_Quit();
                return false;
            }

            sdlcontext.hwnd = static_cast<HWND>(
                SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));

            if (!sdlcontext.hwnd)
            {
                logger::error("SDL", "Failed to retrieve HWND");
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
                ctx->windowData->hwndChild = sdlcontext.hwnd;
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
                logger::info("SDL", std::string("Created renderer with ") + label + '.');
                return renderer;
            }

            logger::error("SDL", std::string("SDL_CreateRenderer (") + label + ") failed: " + SDL_GetError());
            return nullptr;
        };

        sdlcontext.renderer = create_renderer(nullptr, "default renderer");
        if (!sdlcontext.renderer)
        {
            sdlcontext.renderer = create_renderer("software", "software renderer");
        }
        if (!sdlcontext.renderer)
        {
            logger::error("SDL", std::string("SDL_CreateRenderer failed after fallbacks: ") + SDL_GetError());
            SDL_DestroyWindow(sdlcontext.window);
            SDL_Quit();
            return false;
        }

        const int vsyncResult = SDL_SetRenderVSync(sdlcontext.renderer, 0);
        if (vsyncResult != 0)
        {
            const char* const sdlError = SDL_GetError();
            const bool hasDetail = sdlError && sdlError[0] != '\0';
            std::string message = "Render VSync disable unavailable";
            if (hasDetail)
                message += std::string(": ") + sdlError;
            message += "; relying on engine core frame limiter";
            logger::warn("SDL", message);
        }
        sdlcontext.useFrameLimiter = false;

        init_renderer(sdlcontext.renderer);
        sdltextures::sdl_renderer = sdlcontext.renderer;

        refresh_dimensions(ctx);

#if defined(_WIN32)
        if (hostedWindow)
        {
            if (sdlcontext.parent)
            {
                SetParent(sdlcontext.hwnd, sdlcontext.parent);

                LONG_PTR style = GetWindowLongPtr(sdlcontext.hwnd, GWL_STYLE);
                style &= ~WS_OVERLAPPEDWINDOW;
                style |= WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
                SetWindowLongPtr(sdlcontext.hwnd, GWL_STYLE, style);

                epochnamespace::core::MakeDockable(sdlcontext.hwnd, sdlcontext.parent);

                RECT client{};
                HWND sizeSource = hostWnd ? hostWnd : sdlcontext.parent;
                GetClientRect(sizeSource, &client);

                const int width = (std::max)(1, static_cast<int>(client.right - client.left));
                const int height = (std::max)(1, static_cast<int>(client.bottom - client.top));

                sdlcontext.width = width;
                sdlcontext.height = height;

                // Keep SDL's internal window/backbuffer size aligned with the dock slot
                // before showing so startup does not flash a top-level window.
                SDL_SetWindowSize(sdlcontext.window, width, height);
                SetWindowPos(
                    sdlcontext.hwnd, nullptr, 0, 0, width, height,
                    SWP_NOZORDER | SWP_FRAMECHANGED);
                RedrawWindow(
                    sdlcontext.hwnd, nullptr, nullptr,
                    RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

                if (sdlcontext.onResize)
                    sdlcontext.onResize(width, height);

                if (hostWnd && hostWnd != sdlcontext.hwnd && ::IsWindow(hostWnd) != FALSE)
                    ShowWindow(hostWnd, SW_HIDE);

                PostMessage(sdlcontext.parent, WM_SIZE, 0, MAKELPARAM(width, height));
            }

            if (!windowTitle.empty())
            {
                std::wstring wideTitle(windowTitle.begin(), windowTitle.end());
                SetWindowTextW(sdlcontext.hwnd, wideTitle.c_str());
            }

            if (hostWnd && hostWnd != sdlcontext.hwnd && ::IsWindow(hostWnd) != FALSE)
                ::ShowWindow(hostWnd, SW_HIDE);
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

        logger::info(
            "SDL",
            std::string("Initialized ")
                + std::to_string(sdlcontext.width)
                + "x"
                + std::to_string(sdlcontext.height));

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

        diagnostics::FrameTiming frameTimer{ backendType, windowId, "SDL" };

        SDL_Event sdl_event{};
        while (SDL_PollEvent(&sdl_event))
        {
            if (sdl_event.type == SDL_EVENT_QUIT)
            {
                sdlcontext.running = false;
                state::get_sdl_state().mark_should_close(true);
                return false;
            }

            if (sdl_event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                sdlcontext.running = false;
                state::get_sdl_state().mark_should_close(true);
                return false;
            }

            if (sdl_event.type == SDL_EVENT_WINDOW_RESIZED && sdlcontext.onResize)
                sdlcontext.onResize(sdl_event.window.data1, sdl_event.window.data2);
        }

        auto& sharedState = state::get_sdl_state();
        refresh_dimensions(ctx);

        const bool closeRequested =
            sharedState.shouldClose
            || sharedState.window.get_should_close()
            || (ctx && ctx->windowData && ctx->windowData->get_should_close())
            || !sdlcontext.running
            || !sdlcontext.window;

        if (closeRequested)
        {
            sharedState.mark_should_close(true);
            sharedState.running = false;
            sdlcontext.running = false;
            queue.clear();
            frameTimer.finish();
            return false;
        }

        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(sdlcontext.framebufferWidth),
            telemetry::RendererTelemetryTags{ backendType, windowId, "width" });

        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(sdlcontext.framebufferHeight),
            telemetry::RendererTelemetryTags{ backendType, windowId, "height" });

        const std::size_t depth = queue.depth();
        telemetry::emit_gauge(
            "renderer.command_queue.depth",
            static_cast<std::int64_t>(depth),
            telemetry::RendererTelemetryTags{ backendType, windowId });

        if (sharedState.renderFaulted || !sdl_renderer.renderer)
        {
            sharedState.mark_should_close(true);
            sharedState.running = false;
            sdlcontext.running = false;
            queue.clear();
            frameTimer.finish();
            return false;
        }

        detail::render_scene_preview(ctx);

        queue.drain();

        if (sharedState.renderFaulted)
        {
            sharedState.mark_should_close(true);
            sharedState.running = false;
            sdlcontext.running = false;
            queue.clear();
            frameTimer.finish();
            return false;
        }

        capture_frame_if_requested(
            sdlcontext.framebufferWidth,
            sdlcontext.framebufferHeight,
            windowId);

        if (!SDL_RenderPresent(sdl_renderer.renderer))
        {
            check_sdl_error("SDL_RenderPresent");
            sharedState.renderFaulted = true;
            sharedState.mark_should_close(true);
            sharedState.running = false;
            sdlcontext.running = false;
            queue.clear();
            frameTimer.finish();
            return false;
        }

        frameTimer.finish();

        return true;
    }

    inline void sdl_clear()
    {
    }

    inline void sdl_present()
    {
        SDL_RenderPresent(sdl_renderer.renderer);
    }

    inline void sdl_cleanup(std::shared_ptr<epochnamespace::core::Context>& ctx)
    {
        (void)ctx;

        detail::destroy_arcade_screen_preview_target();

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
                fullscreen);
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

#endif // EPOCH_USING_SDL
} // namespace epochnamespace::sdlcontext
