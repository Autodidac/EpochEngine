module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

#include <include/engine.config.hpp>

module software.context;

import spritehandle;
import atlas.texture;
import software.state;

namespace epochnamespace::anativecontext
{
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
    namespace detail
    {
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
                auto* rowBegin = s_softrendererstate.framebuffer.data()
                    + static_cast<std::size_t>(py) * static_cast<std::size_t>(sr.width)
                    + static_cast<std::size_t>(x0);
                std::fill_n(rowBegin, static_cast<std::size_t>(x1 - x0), color);
            }
        }

        [[nodiscard]] inline bool try_get_uniform_region_color(
            const TextureAtlas& atlas,
            const AtlasRegion& region,
            std::uint32_t& outColor) noexcept
        {
            if (region.width == 0 || region.height == 0 || atlas.pixel_data.empty())
                return false;

            const auto read_rgba = [&](std::uint32_t x, std::uint32_t y, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b, std::uint8_t& a) noexcept -> bool
            {
                if (x >= atlas.width || y >= atlas.height)
                    return false;

                const std::size_t index =
                    (static_cast<std::size_t>(y) * static_cast<std::size_t>(atlas.width) + static_cast<std::size_t>(x)) * 4u;
                if (index + 3u >= atlas.pixel_data.size())
                    return false;

                r = atlas.pixel_data[index + 0u];
                g = atlas.pixel_data[index + 1u];
                b = atlas.pixel_data[index + 2u];
                a = atlas.pixel_data[index + 3u];
                return true;
            };

            std::uint8_t baseR = 0;
            std::uint8_t baseG = 0;
            std::uint8_t baseB = 0;
            std::uint8_t baseA = 0;
            if (!read_rgba(region.x, region.y, baseR, baseG, baseB, baseA))
                return false;

            for (std::uint32_t y = 0; y < region.height; ++y)
            {
                for (std::uint32_t x = 0; x < region.width; ++x)
                {
                    std::uint8_t r = 0;
                    std::uint8_t g = 0;
                    std::uint8_t b = 0;
                    std::uint8_t a = 0;
                    if (!read_rgba(region.x + x, region.y + y, r, g, b, a))
                        return false;

                    if (r != baseR || g != baseG || b != baseB || a != baseA)
                        return false;
                }
            }

            outColor =
                (static_cast<std::uint32_t>(baseA) << 24)
                | (static_cast<std::uint32_t>(baseR) << 16)
                | (static_cast<std::uint32_t>(baseG) << 8)
                | static_cast<std::uint32_t>(baseB);
            return true;
        }
    }

    void draw_sprite(
        SpriteHandle handle,
        std::span<const TextureAtlas* const> atlases,
        float x,
        float y,
        float width,
        float height) noexcept
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

        if (atlas->pixel_data.empty())
            const_cast<TextureAtlas*>(atlas)->rebuild_pixels();

        auto& sr = s_softrendererstate;
        if (sr.framebuffer.empty() || sr.width <= 0 || sr.height <= 0)
            return;

        float drawX = x;
        float drawY = y;
        float drawW = width;
        float drawH = height;

        const bool wNorm = (drawW > 0.f && drawW <= 1.f);
        const bool hNorm = (drawH > 0.f && drawH <= 1.f);

        if (wNorm)
        {
            if (drawX >= 0.f && drawX <= 1.f)
                drawX *= static_cast<float>(sr.width);
            drawW = (std::max)(drawW * static_cast<float>(sr.width), 1.0f);
        }
        if (hNorm)
        {
            if (drawY >= 0.f && drawY <= 1.f)
                drawY *= static_cast<float>(sr.height);
            drawH = (std::max)(drawH * static_cast<float>(sr.height), 1.0f);
        }

        if (drawW <= 0.f)
            drawW = static_cast<float>(region.width);
        if (drawH <= 0.f)
            drawH = static_cast<float>(region.height);

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

        std::uint32_t uniformColor = 0;
        if (detail::try_get_uniform_region_color(*atlas, region, uniformColor)
            && ((uniformColor >> 24) & 0xFFu) == 0xFFu)
        {
            detail::fill_rect(destX, destY, destW, destH, uniformColor);
            return;
        }

        const int srcW = static_cast<int>((std::max)(1u, region.width));
        const int srcH = static_cast<int>((std::max)(1u, region.height));
        const float invDestW = 1.0f / static_cast<float>(destW);
        const float invDestH = 1.0f / static_cast<float>(destH);

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
                if (static_cast<unsigned>(atlasX) >= static_cast<unsigned>(atlas->width)
                    || static_cast<unsigned>(atlasY) >= static_cast<unsigned>(atlas->height))
                {
                    continue;
                }

                const std::size_t srcIndex =
                    (static_cast<std::size_t>(atlasY) * static_cast<std::size_t>(atlas->width)
                        + static_cast<std::size_t>(atlasX)) * 4u;
                if (srcIndex + 3u >= static_cast<std::size_t>(atlas->pixel_data.size()))
                    continue;

                const std::uint8_t srcR = atlas->pixel_data[srcIndex + 0];
                const std::uint8_t srcG = atlas->pixel_data[srcIndex + 1];
                const std::uint8_t srcB = atlas->pixel_data[srcIndex + 2];
                const std::uint8_t srcA = atlas->pixel_data[srcIndex + 3];
                if (srcA == 0)
                    continue;

                const std::size_t dstIndex =
                    static_cast<std::size_t>(py) * static_cast<std::size_t>(sr.width)
                    + static_cast<std::size_t>(px);

                const std::uint32_t dst = sr.framebuffer[dstIndex];
                const float a = static_cast<float>(srcA) / 255.0f;
                const float ia = 1.0f - a;

                const std::uint8_t dstR = static_cast<std::uint8_t>((dst >> 16) & 0xFFu);
                const std::uint8_t dstG = static_cast<std::uint8_t>((dst >> 8) & 0xFFu);
                const std::uint8_t dstB = static_cast<std::uint8_t>(dst & 0xFFu);

                const std::uint8_t outR = static_cast<std::uint8_t>(srcR * a + dstR * ia + 0.5f);
                const std::uint8_t outG = static_cast<std::uint8_t>(srcG * a + dstG * ia + 0.5f);
                const std::uint8_t outB = static_cast<std::uint8_t>(srcB * a + dstB * ia + 0.5f);

                sr.framebuffer[dstIndex] =
                    (0xFFu << 24) | (std::uint32_t(outR) << 16) | (std::uint32_t(outG) << 8) | std::uint32_t(outB);
            }
        }
    }
#endif
}
