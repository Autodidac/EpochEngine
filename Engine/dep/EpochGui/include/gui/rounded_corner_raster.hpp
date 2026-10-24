// SPDX-License-Identifier: MPL-2.0
// EpochGUI: analytic inverse-circle coverage for true antialiased rounded corners.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace epochengine::gui_lib::rounded_corner_raster
{
    enum class Corner : std::uint8_t
    {
        top_left, top_right, bottom_right, bottom_left
    };

    // A quarter-circle is an opaque interior minus the circular corner cutout.
    // RGBA is straight alpha; supersampling keeps the curve smooth through
    // both up/downscaling and different GUI backends. No per-frame allocation.
    [[nodiscard]] inline std::vector<std::uint8_t> rgba(
        std::uint32_t radius, Corner corner,
        std::uint8_t red, std::uint8_t green, std::uint8_t blue,
        std::uint8_t alpha = 255U)
    {
        if (radius == 0U)
            return {};
        // Never accept unbounded input from a UI dimension.
        if (radius > 256U)
            return {};

        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(radius) * radius * 4U, 0U);
        const float r = static_cast<float>(radius);
        const float centerX = corner == Corner::top_left || corner == Corner::bottom_left
            ? r : 0.0f;
        const float centerY = corner == Corner::top_left || corner == Corner::top_right
            ? r : 0.0f;
        constexpr std::uint32_t samples = 4U;
        constexpr float inverseSamples = 1.0f / static_cast<float>(samples);
        constexpr float sampleCount = static_cast<float>(samples * samples);
        for (std::uint32_t y = 0U; y < radius; ++y)
        {
            for (std::uint32_t x = 0U; x < radius; ++x)
            {
                unsigned covered = 0U;
                for (std::uint32_t sy = 0U; sy < samples; ++sy)
                {
                    for (std::uint32_t sx = 0U; sx < samples; ++sx)
                    {
                        const float px = static_cast<float>(x)
                            + (static_cast<float>(sx) + 0.5f) * inverseSamples - centerX;
                        const float py = static_cast<float>(y)
                            + (static_cast<float>(sy) + 0.5f) * inverseSamples - centerY;
                        covered += (px * px + py * py <= r * r) ? 1U : 0U;
                    }
                }
                const std::size_t offset =
                    (static_cast<std::size_t>(y) * radius + x) * 4U;
                pixels[offset] = red;
                pixels[offset + 1U] = green;
                pixels[offset + 2U] = blue;
                pixels[offset + 3U] = static_cast<std::uint8_t>(
                    std::lround(static_cast<float>(alpha)
                        * static_cast<float>(covered) / sampleCount));
            }
        }
        return pixels;
    }
}
