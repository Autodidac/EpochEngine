// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace epochengine::gui_lib::circle_raster
{
    // RGBA texture with an antialiased circle of the requested diameter.
    // The GPU may scale this sprite, but corners are never stretched separately.
    [[nodiscard]] inline std::vector<std::uint8_t> rgba(
        std::uint32_t diameter,
        std::uint8_t red,
        std::uint8_t green,
        std::uint8_t blue,
        std::uint8_t alpha = 255U)
    {
        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(diameter) * diameter * 4U, 0U);
        if (diameter == 0U)
            return pixels;

        const float radius = static_cast<float>(diameter) * 0.5f;
        for (std::uint32_t y = 0; y < diameter; ++y)
            for (std::uint32_t x = 0; x < diameter; ++x)
            {
                const float dx = static_cast<float>(x) + 0.5f - radius;
                const float dy = static_cast<float>(y) + 0.5f - radius;
                const float coverage = std::clamp(
                    radius + 0.5f - std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
                const std::size_t pixel =
                    (static_cast<std::size_t>(y) * diameter + x) * 4U;
                pixels[pixel] = red;
                pixels[pixel + 1U] = green;
                pixels[pixel + 2U] = blue;
                pixels[pixel + 3U] = static_cast<std::uint8_t>(
                    std::lround(static_cast<float>(alpha) * coverage));
            }
        return pixels;
    }
}
