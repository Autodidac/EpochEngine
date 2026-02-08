/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondEngine - Modular C++ Game Engine                   *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 **************************************************************/
 // acontext.softrenderer.quad.ixx

module;

#include <include/aengine.config.hpp> // for ALMOND_USING Macros

export module acontext.softrenderer.quad;

import <algorithm>;
import <cstdint>;
import <cstring>;

import acontext.softrenderer.textures; // BackendData, Texture, TexturePtr, create_texture
import aatlas.manager;                 // atlasmanager::atlas_vector (and atlas types)
import aatlas.texture;                 // TextureAtlas

export namespace almondnamespace::anativecontext
{
#if defined(ALMOND_USING_SOFTWARE_RENDERER)

    export int clamp_int(int v, int lo, int hi) noexcept
    {
        return (v < lo) ? lo : (v > hi) ? hi : v;
    }

    // Assumes atlas->pixel_data is RGBA8 (width*height*4 bytes).
    export void upload_rgba8_to_software_texture(Texture& tex, const almondnamespace::TextureAtlas& atlas)
    {
        const auto expected = std::size_t(atlas.width) * std::size_t(atlas.height) * 4u;
        if (atlas.width <= 0 || atlas.height <= 0) return;
        if (atlas.pixel_data.size() < expected) return;

        // tex.pixels is assumed to be packed 0xAARRGGBB (uint32_t) framebuffer style.
        // Convert RGBA8 -> AARRGGBB.
        const std::uint8_t* src = reinterpret_cast<const std::uint8_t*>(atlas.pixel_data.data());
        const std::size_t pixelCount = std::size_t(atlas.width) * std::size_t(atlas.height);

        if (tex.width != atlas.width || tex.height != atlas.height) return;
        if (tex.pixels.size() < pixelCount) return;

        for (std::size_t i = 0; i < pixelCount; ++i)
        {
            const std::uint8_t r = src[i * 4 + 0];
            const std::uint8_t g = src[i * 4 + 1];
            const std::uint8_t b = src[i * 4 + 2];
            const std::uint8_t a = src[i * 4 + 3];

            tex.pixels[i] =
                (std::uint32_t(a) << 24) |
                (std::uint32_t(r) << 16) |
                (std::uint32_t(g) << 8) |
                (std::uint32_t(b) << 0);
        }
    }

    // Draw a textured quad into the software framebuffer.
    inline void draw_textured_quad(
        BackendData& backend,
        const Texture& tex,
        int dstX, int dstY, int dstW, int dstH)
    {
        if (backend.srState.framebuffer.empty()) return;
        if (tex.width <= 0 || tex.height <= 0) return;
        if (dstW <= 0 || dstH <= 0) return;

        const int fbW = backend.srState.width;
        const int fbH = backend.srState.height;

        // Integer mapping avoids the “u==1.0 => src==width” OOB.
        for (int y = 0; y < dstH; ++y)
        {
            const int fbY = dstY + y;
            if (fbY < 0 || fbY >= fbH) continue;

            const int srcY = clamp_int((y * tex.height) / dstH, 0, tex.height - 1);

            for (int x = 0; x < dstW; ++x)
            {
                const int fbX = dstX + x;
                if (fbX < 0 || fbX >= fbW) continue;

                const int srcX = clamp_int((x * tex.width) / dstW, 0, tex.width - 1);

                const std::uint32_t src = tex.sample(srcX, srcY);
                backend.srState.framebuffer[fbY * fbW + fbX] = src;
            }
        }
    }

    // High-level entry: blit first atlas onto framebuffer.
    inline void render_first_atlas_quad(BackendData& backend)
    {
        if (atlasmanager::atlas_vector.empty()) return;

        const auto* atlas = atlasmanager::atlas_vector[0];
        if (!atlas) return;

        // Ensure pixels exist and are RGBA8-sized.
        {
            const auto expected = std::size_t(atlas->width) * std::size_t(atlas->height) * 4u;
            if (atlas->pixel_data.size() != expected)
            {
                // If your atlas type supports rebuild_pixels(), do it:
                // const_cast<TextureAtlas*>(atlas)->rebuild_pixels();
                // (Leaving commented because I can’t assume mutability here.)
            }
        }

        auto it = backend.textures.find(atlas);
        if (it == backend.textures.end())
        {
            auto tex = create_texture(atlas->width, atlas->height);

            // Convert RGBA8 atlas bytes -> packed software texture pixels.
            upload_rgba8_to_software_texture(*tex, *atlas);

            backend.textures[atlas] = tex;
            it = backend.textures.find(atlas);
        }

        TexturePtr tex = it->second;
        if (!tex) return;

        draw_textured_quad(backend, *tex, 0, 0, backend.srState.width, backend.srState.height);
    }

#endif // ALMOND_USING_SOFTWARE_RENDERER
} // namespace almondnamespace::anativecontext
