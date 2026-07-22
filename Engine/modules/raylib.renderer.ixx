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

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <span>

#include <include/engine.config.hpp>

export module raylib.renderer;

import raylib.state;
import raylib.textures;
import atlas.texture;
import spritehandle;
import raylib.api;

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)

namespace epochengine::raylibrenderer
{
    export struct RaylibSpriteDiagnostics
    {
        std::uint32_t attempted = 0;
        std::uint32_t resolved = 0;
        std::uint32_t uploadReady = 0;
        std::uint32_t textureReady = 0;
        std::uint32_t frameReady = 0;
        std::uint32_t submitted = 0;
    };

    inline std::atomic_uint32_t s_spriteAttempts{ 0 };
    inline std::atomic_uint32_t s_spriteResolved{ 0 };
    inline std::atomic_uint32_t s_spriteUploadReady{ 0 };
    inline std::atomic_uint32_t s_spriteTextureReady{ 0 };
    inline std::atomic_uint32_t s_spriteFrameReady{ 0 };
    inline std::atomic_uint32_t s_spriteSubmitted{ 0 };

    export inline void reset_sprite_diagnostics() noexcept
    {
        s_spriteAttempts.store(0, std::memory_order_relaxed);
        s_spriteResolved.store(0, std::memory_order_relaxed);
        s_spriteUploadReady.store(0, std::memory_order_relaxed);
        s_spriteTextureReady.store(0, std::memory_order_relaxed);
        s_spriteFrameReady.store(0, std::memory_order_relaxed);
        s_spriteSubmitted.store(0, std::memory_order_relaxed);
    }

    export [[nodiscard]] inline RaylibSpriteDiagnostics sprite_diagnostics() noexcept
    {
        return {
            s_spriteAttempts.load(std::memory_order_relaxed),
            s_spriteResolved.load(std::memory_order_relaxed),
            s_spriteUploadReady.load(std::memory_order_relaxed),
            s_spriteTextureReady.load(std::memory_order_relaxed),
            s_spriteFrameReady.load(std::memory_order_relaxed),
            s_spriteSubmitted.load(std::memory_order_relaxed)
        };
    }

    // DO NOT call BeginDrawing/EndDrawing here.
    // The context layer owns frame boundaries; this renderer only issues draw calls.
    export inline void begin_frame() {}
    export inline void end_frame() {}

    export inline void draw_sprite(
        SpriteHandle handle,
        std::span<const TextureAtlas* const> atlases,
        float x, float y, float width, float height) noexcept
    {
        s_spriteAttempts.fetch_add(1, std::memory_order_relaxed);

        if (!handle.is_valid())
            return;

        const int a = static_cast<int>(handle.atlasIndex);
        const int i = static_cast<int>(handle.localIndex);
        if (a < 0 || a >= static_cast<int>(atlases.size()))
            return;

        const TextureAtlas* atlas = atlases[static_cast<std::size_t>(a)];
        if (!atlas)
            return;

        AtlasRegion r{};
        if (!atlas->try_get_entry_info(i, r))
            return;
        s_spriteResolved.fetch_add(1, std::memory_order_relaxed);


        auto& st = epochengine::raylibstate::s_raylibstate;
        if (!st.running)
            return;

        if (!st.frameActive && st.offscreen.id != 0)
        {
            epochengine::raylib_api::begin_texture_mode(st.offscreen);
            st.frameActive = true;
            st.frameInTextureMode = true;
        }

        // Upload (this will no-op if cached + correct version).
        if (epochengine::raylibtextures::ensure_uploaded(*atlas))
            s_spriteUploadReady.fetch_add(1, std::memory_order_relaxed);

        epochengine::raylib_api::Texture2D tex{};
        if (!epochengine::raylibtextures::try_copy_texture(*atlas, tex)
            || tex.id == 0)
            return;

        s_spriteTextureReady.fetch_add(1, std::memory_order_relaxed);

        if (!st.frameActive)
            return;

        s_spriteFrameReady.fetch_add(1, std::memory_order_relaxed);

        const epochengine::raylib_api::Rectangle src{
            static_cast<float>(r.x),
            static_cast<float>(r.y),
            static_cast<float>(r.width),
            static_cast<float>(r.height)
        };

        const auto fit = epochengine::raylibstate::get_last_viewport_fit();

        const float viewportScale = (fit.scale > 0.0f) ? fit.scale : 1.0f;
        const float designWidth = static_cast<float>((std::max)(1, fit.refW));
        const float designHeight = static_cast<float>((std::max)(1, fit.refH));

        const float baseOffsetX = static_cast<float>(fit.vpX);
        const float baseOffsetY = static_cast<float>(fit.vpY);

        const bool normalized =
            (x >= 0.f && x <= 1.f && y >= 0.f && y <= 1.f) &&
            ((width <= 1.f && width >= 0.f) || width <= 0.f) &&
            ((height <= 1.f && height >= 0.f) || height <= 0.f);

        float px{}, py{}, pw{}, ph{};
        if (normalized)
        {
            px = baseOffsetX + x * designWidth * viewportScale;
            py = baseOffsetY + y * designHeight * viewportScale;

            const float scaledW = (width > 0.f)
                ? (width * designWidth * viewportScale)
                : (static_cast<float>(r.width) * viewportScale);

            const float scaledH = (height > 0.f)
                ? (height * designHeight * viewportScale)
                : (static_cast<float>(r.height) * viewportScale);

            pw = (std::max)(scaledW, 1.0f);
            ph = (std::max)(scaledH, 1.0f);
        }
        else
        {
            px = baseOffsetX + x * viewportScale;
            py = baseOffsetY + y * viewportScale;

            const float scaledW = (width > 0.f)
                ? (width * viewportScale)
                : (static_cast<float>(r.width) * viewportScale);

            const float scaledH = (height > 0.f)
                ? (height * viewportScale)
                : (static_cast<float>(r.height) * viewportScale);

            pw = (std::max)(scaledW, 1.0f);
            ph = (std::max)(scaledH, 1.0f);
        }

        const epochengine::raylib_api::Rectangle dst{ px, py, pw, ph };

        epochengine::raylib_api::draw_texture_pro(
            tex,
            src,
            dst,
            epochengine::raylib_api::Vector2{ 0.0f, 0.0f },
            0.0f,
            epochengine::raylib_api::white);
        s_spriteSubmitted.fetch_add(1, std::memory_order_relaxed);

    }
}

#endif // EPOCH_USING_RAYLIB
