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
 // acontext.softrenderer.context.ixx
 //
 // Key fixes vs your header / the broken module you had:
 //  - NO direct ctx->hwnd / ctx->hdc member access (your Context has those private now).
 //  - Backend does not assume it owns/destroys any HWND.
 //  - Uses C++23 requires-expressions to *optionally* grab HWND/HDC via accessors if they exist.
 //    Otherwise, you must pass parentWnd explicitly from the multiplexer (recommended).
 //

module;

//#include "aplatform.hpp"
#include <include/aengine.config.hpp> // for EPOCH_USING Macros

#if defined(_WIN32)
#   ifdef EPOCH_USING_WINMAIN
#       include "../include/aframework.hpp"
#   endif
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#endif

#include <chrono>

export module acontext.softrenderer.context;

import aengine.platform;

import <algorithm>;
//import <chrono>;
import <cmath>;
import <cstdint>;
import <functional>;
import <iostream>;
import <memory>;
import <mutex>;
import <utility>;
import <vector>;

import aengine.core.context;             // epochnamespace::core::Context
import aengine.context.commandqueue;     // epochnamespace::core::CommandQueue

import acontext.softrenderer.state;      // s_softrendererstate, SoftRendState
import acontext.softrenderer.textures;   // Texture, TexturePtr (as in your project)
import acontext.softrenderer.renderer;   // SoftwareRenderer (as in your project)
import aatlas.manager;                  // atlasmanager::atlas_vector (as in your header)
import aengine.diagnostics;
import aengine.gui;
import aengine.telemetry;
import epoch.render.preview_grid;

namespace epochnamespace::anativecontext
{
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)

    // These stay module-internal; nobody else should poke them directly.
    inline TexturePtr       cubeTexture{};
    inline SoftwareRenderer renderer{};

#if defined(_WIN32)
    // --- Optional accessors for HWND/HDC without assuming member names exist ---
    template <class T>
    static HWND try_get_hwnd(const T& ctx)
    {
        if constexpr (requires { ctx.get_hwnd(); })
            return reinterpret_cast<HWND>(ctx.get_hwnd());
        else if constexpr (requires { ctx.hwnd(); })
            return reinterpret_cast<HWND>(ctx.hwnd());
        else if constexpr (requires { ctx.native_hwnd(); })
            return reinterpret_cast<HWND>(ctx.native_hwnd());
        else if constexpr (requires { ctx.native_window_handle(); })
            return reinterpret_cast<HWND>(ctx.native_window_handle());
        else
            return nullptr;
    }

    template <class T>
    static HDC try_get_hdc(const T& ctx)
    {
        if constexpr (requires { ctx.get_hdc(); })
            return reinterpret_cast<HDC>(ctx.get_hdc());
        else if constexpr (requires { ctx.hdc(); })
            return reinterpret_cast<HDC>(ctx.hdc());
        else if constexpr (requires { ctx.native_hdc(); })
            return reinterpret_cast<HDC>(ctx.native_hdc());
        else
            return nullptr;
    }
#endif
}

export namespace epochnamespace::anativecontext
{
    int get_width();
    int get_height();

    namespace detail
    {
        inline void refresh_dimensions(core::Context& ctx) noexcept
        {
            auto& sr = s_softrendererstate;
            ctx.width = (std::max)(1, sr.width);
            ctx.height = (std::max)(1, sr.height);
            ctx.virtualWidth = ctx.width;
            ctx.virtualHeight = ctx.height;
            ctx.framebufferWidth = ctx.width;
            ctx.framebufferHeight = ctx.height;

            if (ctx.windowData)
                ctx.windowData->set_size(ctx.width, ctx.height);
        }

        [[nodiscard]] inline std::uint32_t pack_color(float r, float g, float b, float a = 1.0f) noexcept
        {
            const auto clamp_channel = [](float value) noexcept -> std::uint32_t
            {
                return static_cast<std::uint32_t>((std::clamp)(value, 0.0f, 1.0f) * 255.0f);
            };

            return (clamp_channel(a) << 24)
                | (clamp_channel(r) << 16)
                | (clamp_channel(g) << 8)
                | clamp_channel(b);
        }

        inline void fill_rect(int x, int y, int width, int height, std::uint32_t color) noexcept
        {
            auto& sr = s_softrendererstate;
            if (sr.framebuffer.empty() || sr.width <= 0 || sr.height <= 0 || width <= 0 || height <= 0)
                return;

            const int x0 = (std::max)(0, x);
            const int y0 = (std::max)(0, y);
            const int x1 = (std::min)(sr.width, x + width);
            const int y1 = (std::min)(sr.height, y + height);
            if (x0 >= x1 || y0 >= y1)
                return;

            for (int py = y0; py < y1; ++py)
            {
                const std::size_t rowOffset = static_cast<std::size_t>(py) * static_cast<std::size_t>(sr.width);
                for (int px = x0; px < x1; ++px)
                    sr.framebuffer[rowOffset + static_cast<std::size_t>(px)] = color;
            }
        }

        inline void draw_line(int x0, int y0, int x1, int y1, std::uint32_t color) noexcept
        {
            auto& sr = s_softrendererstate;
            if (sr.framebuffer.empty() || sr.width <= 0 || sr.height <= 0)
                return;

            int dx = std::abs(x1 - x0);
            const int sx = x0 < x1 ? 1 : -1;
            int dy = -std::abs(y1 - y0);
            const int sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;

            for (;;)
            {
                if (x0 >= 0 && x0 < sr.width && y0 >= 0 && y0 < sr.height)
                {
                    sr.framebuffer[
                        static_cast<std::size_t>(y0) * static_cast<std::size_t>(sr.width)
                        + static_cast<std::size_t>(x0)] = color;
                }

                if (x0 == x1 && y0 == y1)
                    break;

                const int twiceErr = err * 2;
                if (twiceErr >= dy)
                {
                    err += dy;
                    x0 += sx;
                }
                if (twiceErr <= dx)
                {
                    err += dx;
                    y0 += sy;
                }
            }
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

        inline void render_scene_preview(const core::Context& ctx) noexcept
        {
            const auto viewport = ctx.scene_viewport();
            if (!viewport.valid() || ctx.scene_preview_mode() != core::ScenePreviewMode::Editor)
                return;

            const auto clearColor = epochnamespace::previewgrid::kClearColor;
            fill_rect(
                viewport.x,
                viewport.y,
                viewport.width,
                viewport.height,
                pack_color(clearColor[0], clearColor[1], clearColor[2], clearColor[3]));

            const auto camera = epochnamespace::previewgrid::kCamera;
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
                draw_line(
                    static_cast<int>(std::lround(ax)),
                    static_cast<int>(std::lround(ay)),
                    static_cast<int>(std::lround(bx)),
                    static_cast<int>(std::lround(by)),
                    pack_color(color.x, color.y, color.z));
            }
        }
    }

    void softrenderer_resize(int width, int height)
    {
        auto& sr = s_softrendererstate;
        sr.width = (std::max)(1, width);
        sr.height = (std::max)(1, height);

        sr.framebuffer.assign(
            std::size_t(sr.width) * std::size_t(sr.height),
            0xFF000000u);

#if defined(_WIN32)
        sr.bmi.bmiHeader.biWidth = sr.width;
        sr.bmi.bmiHeader.biHeight = -sr.height;
#endif
    }

    // Exported API (what your engine calls)
    bool softrenderer_initialize(
        std::shared_ptr<core::Context> ctx,
#if defined(_WIN32)
        HWND parentWnd = nullptr,
#else
        void* parentWnd = nullptr,
#endif
        unsigned int w = 400,
        unsigned int h = 300,
        std::function<void(int, int)> onResize = nullptr)
    {
        if (!ctx)
        {
            std::cerr << "[ SoftRenderer ] - Invalid context\n";
            return false;
        }

        auto& sr = s_softrendererstate;
        sr.width = static_cast<int>(w);
        sr.height = static_cast<int>(h);
        sr.running = true;

        ctx->get_width = get_width;
        ctx->get_height = get_height;

        epochnamespace::anativecontext::detail::refresh_dimensions(*ctx);

        std::weak_ptr<core::Context> weakCtx = ctx;
        ctx->onResize = [weakCtx, resize = std::move(onResize)](int newWidth, int newHeight) mutable
            {
                softrenderer_resize(newWidth, newHeight);
                if (auto locked = weakCtx.lock())
                    epochnamespace::anativecontext::detail::refresh_dimensions(*locked);
                if (resize)
                    resize(newWidth, newHeight);
            };
        sr.onResize = ctx->onResize;

        // Allocate framebuffer
        sr.framebuffer.assign(std::size_t(w) * std::size_t(h), 0xFF000000u);

#if defined(_WIN32)
        // Prefer explicit parentWnd from multiplexer; fall back to accessor if it exists.
        HWND resolvedParent = parentWnd;
        if (!resolvedParent)
            resolvedParent = try_get_hwnd(*ctx);

        if (!resolvedParent)
        {
            std::cerr << "[ SoftRenderer ] - No parent HWND available. Pass parentWnd from multiplexer.\n";
            return false;
        }

        sr.parent = resolvedParent;
        sr.hwnd = resolvedParent; // store for presentation target only (NOT owned)

        // Prebuild BITMAPINFO for StretchDIBits
        ZeroMemory(&sr.bmi, sizeof(BITMAPINFO));
        sr.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        sr.bmi.bmiHeader.biWidth = sr.width;
        sr.bmi.bmiHeader.biHeight = -sr.height; // top-down
        sr.bmi.bmiHeader.biPlanes = 1;
        sr.bmi.bmiHeader.biBitCount = 32;
        sr.bmi.bmiHeader.biCompression = BI_RGB;

        std::cout << "[ SoftRenderer ] - Initialized. HWND=" << sr.hwnd
            << " (" << sr.width << "x" << sr.height << ")\n";
#else
        (void)parentWnd;
        std::cout << "[ SoftRenderer ] - Initialized (non-Win32) "
            << sr.width << "x" << sr.height << "\n";
#endif

        // Demo texture (kept from your header)
        if (!cubeTexture)
        {
            cubeTexture = std::make_shared<Texture>(64, 64);
            for (int y = 0; y < cubeTexture->height; ++y)
            {
                for (int x = 0; x < cubeTexture->width; ++x)
                {
                    cubeTexture->pixels[std::size_t(y) * std::size_t(cubeTexture->width) + std::size_t(x)] =
                        ((x / 8 + y / 8) % 2) ? 0xFFFF0000u : 0xFF00FF00u;
                }
            }
        }

        return true;
    }

    void softrenderer_draw_quad(SoftRendState& softstate)
    {
        if (atlasmanager::atlas_vector.empty()) return;

        const auto* atlas = atlasmanager::atlas_vector.back();
        if (!atlas) return;

        const int w = atlas->width;
        const int h = atlas->height;
        if (w <= 0 || h <= 0) return;

        const std::size_t expected = std::size_t(w) * std::size_t(h) * 4u;
        if (atlas->pixel_data.size() < expected) return;

        const int dstW = (std::max)(1, softstate.width);
        const int dstH = (std::max)(1, softstate.height);

        for (int y = 0; y < softstate.height; ++y)
        {
            const int texY = std::clamp((y * h) / dstH, 0, h - 1);

            for (int x = 0; x < softstate.width; ++x)
            {
                const int texX = std::clamp((x * w) / dstW, 0, w - 1);

                const std::size_t src = (std::size_t(texY) * std::size_t(w) + std::size_t(texX)) * 4u;
                const std::uint8_t r = atlas->pixel_data[src + 0];
                const std::uint8_t g = atlas->pixel_data[src + 1];
                const std::uint8_t b = atlas->pixel_data[src + 2];
                const std::uint8_t a = atlas->pixel_data[src + 3];

                // If you want straight blit ignoring alpha, force a=255.
                const std::uint32_t packed =
                    (std::uint32_t(a) << 24) |
                    (std::uint32_t(r) << 16) |
                    (std::uint32_t(g) << 8) |
                    (std::uint32_t(b) << 0);

                softstate.framebuffer[std::size_t(y) * std::size_t(softstate.width) + std::size_t(x)] = packed;
            }
        }
    }


    inline void draw_sprite(
        SpriteHandle handle,
        std::span<const TextureAtlas* const> atlases,
        float x, float y, float width, float height) noexcept
    {
        if (!handle.is_valid())
            return;

        const int atlasIdx = static_cast<int>(handle.atlasIndex);
        const int localIdx = static_cast<int>(handle.localIndex);

        if (atlasIdx < 0 || atlasIdx >= static_cast<int>(atlases.size()))
            return;

        const TextureAtlas* atlas = atlases[atlasIdx];
        if (!atlas)
            return;

        AtlasRegion region{};
        if (!atlas->try_get_entry_info(localIdx, region))
            return;

        // Ensure pixels exist
        if (atlas->pixel_data.empty())
            const_cast<TextureAtlas*>(atlas)->rebuild_pixels();

        auto& sr = s_softrendererstate;
        if (sr.framebuffer.empty() || sr.width <= 0 || sr.height <= 0)
            return;

        // Normalize coords if caller uses 0..1
        float drawX = x;
        float drawY = y;
        float drawW = width;
        float drawH = height;

        const bool wNorm = (drawW > 0.f && drawW <= 1.f);
        const bool hNorm = (drawH > 0.f && drawH <= 1.f);

        if (wNorm)
        {
            if (drawX >= 0.f && drawX <= 1.f) drawX *= static_cast<float>(sr.width);
            drawW = (std::max)(drawW * static_cast<float>(sr.width), 1.0f);
        }
        if (hNorm)
        {
            if (drawY >= 0.f && drawY <= 1.f) drawY *= static_cast<float>(sr.height);
            drawH = (std::max)(drawH * static_cast<float>(sr.height), 1.0f);
        }

        if (drawW <= 0.f) drawW = static_cast<float>(region.width);
        if (drawH <= 0.f) drawH = static_cast<float>(region.height);

        const int destX = static_cast<int>(std::floor(drawX));
        const int destY = static_cast<int>(std::floor(drawY));
        const int destW = (std::max)(1, static_cast<int>(std::lround(drawW)));
        const int destH = (std::max)(1, static_cast<int>(std::lround(drawH)));

        const int clipX0 = (std::max)(0, destX);
        const int clipY0 = (std::max)(0, destY);
        const int clipX1 = (std::min)(sr.width, destX + destW);
        const int clipY1 = (std::min)(sr.height, destY + destH);
        if (clipX0 >= clipX1 || clipY0 >= clipY1)
            return;

        const int srcW = static_cast<int>((std::max)(1u, region.width));
        const int srcH = static_cast<int>((std::max)(1u, region.height));

        const float invDestW = 1.0f / static_cast<float>(destW);
        const float invDestH = 1.0f / static_cast<float>(destH);

        // atlas->pixel_data is byte RGBA, matching softrenderer_draw_quad().
        for (int py = clipY0; py < clipY1; ++py)
        {
            const float v = (py - destY) * invDestH;
            const int sampleY = std::clamp(static_cast<int>(std::floor(v * srcH)), 0, srcH - 1);

            for (int px = clipX0; px < clipX1; ++px)
            {
                const float u = (px - destX) * invDestW;
                const int sampleX = std::clamp(static_cast<int>(std::floor(u * srcW)), 0, srcW - 1);

                const int atlasX = static_cast<int>(region.x) + sampleX;
                const int atlasY = static_cast<int>(region.y) + sampleY;

                // bounds (no signed/unsigned mismatch)
                if (static_cast<unsigned>(atlasX) >= static_cast<unsigned>(atlas->width) ||
                    static_cast<unsigned>(atlasY) >= static_cast<unsigned>(atlas->height))
                    continue;

                const size_t srcIndex =
                    (static_cast<size_t>(atlasY) * static_cast<size_t>(atlas->width) + static_cast<size_t>(atlasX)) * 4u;

                if (srcIndex + 3u >= static_cast<size_t>(atlas->pixel_data.size()))
                    continue;

                const uint8_t srcR = atlas->pixel_data[srcIndex + 0];
                const uint8_t srcG = atlas->pixel_data[srcIndex + 1];
                const uint8_t srcB = atlas->pixel_data[srcIndex + 2];
                const uint8_t srcA = atlas->pixel_data[srcIndex + 3];
                if (srcA == 0)
                    continue;

                const size_t dstIndex =
                    static_cast<size_t>(py) * static_cast<size_t>(sr.width) + static_cast<size_t>(px);

                const uint32_t dst = sr.framebuffer[dstIndex];

                const float a = static_cast<float>(srcA) / 255.0f;
                const float ia = 1.0f - a;

                const uint8_t dstR = static_cast<uint8_t>((dst >> 16) & 0xFF);
                const uint8_t dstG = static_cast<uint8_t>((dst >> 8) & 0xFF);
                const uint8_t dstB = static_cast<uint8_t>(dst & 0xFF);

                const uint8_t outR = static_cast<uint8_t>(srcR * a + dstR * ia + 0.5f);
                const uint8_t outG = static_cast<uint8_t>(srcG * a + dstG * ia + 0.5f);
                const uint8_t outB = static_cast<uint8_t>(srcB * a + dstB * ia + 0.5f);

                sr.framebuffer[dstIndex] =
                    (0xFFu << 24) | (uint32_t(outR) << 16) | (uint32_t(outG) << 8) | uint32_t(outB);
            }
        }
    }


    bool softrenderer_process(core::Context& ctx, core::CommandQueue& queue)
    {
        auto& sr = s_softrendererstate;
        epochnamespace::anativecontext::detail::refresh_dimensions(ctx);
        const std::uintptr_t windowId = ctx.windowData
            ? reinterpret_cast<std::uintptr_t>(ctx.windowData->hwnd)
            : 0;

        diagnostics::FrameTiming frameTimer{ ctx.type, windowId, "Software" };
#if EPOCH_USE_CLEAR_COLOR        // Clear
        const auto clearColor = core::clear_color_for_context(core::ContextType::Software);
        const auto clearR = static_cast<std::uint8_t>(
            std::clamp(clearColor[0], 0.0f, 1.0f) * 255.0f);
        const auto clearG = static_cast<std::uint8_t>(
            std::clamp(clearColor[1], 0.0f, 1.0f) * 255.0f);
        const auto clearB = static_cast<std::uint8_t>(
            std::clamp(clearColor[2], 0.0f, 1.0f) * 255.0f);
        const auto clearA = static_cast<std::uint8_t>(
            std::clamp(clearColor[3], 0.0f, 1.0f) * 255.0f);
        const std::uint32_t packedColor =
            (std::uint32_t(clearA) << 24)
            | (std::uint32_t(clearR) << 16)
            | (std::uint32_t(clearG) << 8)
            | std::uint32_t(clearB);
        std::fill(sr.framebuffer.begin(), sr.framebuffer.end(), packedColor);

        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(sr.width),
            telemetry::RendererTelemetryTags{ ctx.type, windowId, "width" });
        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(sr.height),
            telemetry::RendererTelemetryTags{ ctx.type, windowId, "height" });
        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(sr.framebuffer.size()),
            telemetry::RendererTelemetryTags{ ctx.type, windowId, "buffer_length" });

#endif
        // debug fullscreen atlas blit
        // Optional draw (disabled in your header)
        //softrenderer_draw_quad(sr);

        detail::render_scene_preview(ctx);

        // Drain commands
        {
            std::size_t depth = 0;
            {
                depth = queue.depth();
            }
            telemetry::emit_gauge(
                "renderer.command_queue.depth",
                static_cast<std::int64_t>(depth),
                telemetry::RendererTelemetryTags{ ctx.type, windowId });
        }
        queue.drain();
        if (ctx.windowData && ctx.windowData->context)
            ::epochnamespace::gui::render_deferred_batch(ctx.windowData->context);

#if defined(_WIN32)
        // Present
        // Prefer HDC accessor if it exists; otherwise use GetDC on the stored HWND.
        HDC hdc = try_get_hdc(ctx);
        bool tempDC = false;

        if (!hdc && sr.hwnd)
        {
            hdc = GetDC(sr.hwnd);
            tempDC = (hdc != nullptr);
        }

                if (hdc)
        {
            StretchDIBits(
                hdc,
                0, 0, sr.width, sr.height,
                0, 0, sr.width, sr.height,
                sr.framebuffer.data(),
                &sr.bmi,
                DIB_RGB_COLORS,
                SRCCOPY);
            GdiFlush();

            if (sr.hwnd)
                RedrawWindow(sr.hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);

            if (tempDC && sr.hwnd)
                ReleaseDC(sr.hwnd, hdc);
        }
#endif

        frameTimer.finish();
        return true;
    }

    void softrenderer_cleanup(std::shared_ptr<core::Context>& /*ctx*/)
    {
        auto& sr = s_softrendererstate;

        sr.framebuffer.clear();
        cubeTexture.reset();

        // DO NOT DestroyWindow here. This backend does not own the window.
        sr.hwnd = nullptr;
        sr.parent = nullptr;
        sr.running = false;

        sr = {}; // reset remaining fields

        std::cout << "[ SoftRenderer ] - Cleanup complete\n";
    }

    int get_width() { return s_softrendererstate.width; }
    int get_height() { return s_softrendererstate.height; }

#else
    // If you build without EPOCH_USING_SOFTWARE_RENDERER, keep linkable stubs.
    bool softrenderer_initialize(std::shared_ptr<core::Context>, void*, unsigned, unsigned, std::function<void(int, int)>)
    {
        std::cerr << "[ SoftRenderer ] - Not built (EPOCH_USING_SOFTWARE_RENDERER not defined)\n";
        return false;
    }
    bool softrenderer_process(core::Context&, core::CommandQueue&) { return false; }
    void softrenderer_cleanup(std::shared_ptr<epochnamespace::core::Context>&) {}
    int get_width() { return 0; }
    int get_height() { return 0; }
#endif
} // namespace epochnamespace::anativecontext
