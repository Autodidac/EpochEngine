// SPDX-License-Identifier: MPL-2.0
#include "../include/gui/rounded_corner_raster.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

int main()
{
    using namespace epochengine::gui_lib::rounded_corner_raster;
    assert(rgba(0, Corner::top_left, 1, 2, 3).empty());
    assert(rgba(257, Corner::top_left, 1, 2, 3).empty());
    constexpr std::uint32_t n = 48;
    std::array<std::vector<std::uint8_t>, 4> corners{
        rgba(n, Corner::top_left, 17, 18, 19, 208),
        rgba(n, Corner::top_right, 17, 18, 19, 208),
        rgba(n, Corner::bottom_right, 17, 18, 19, 208),
        rgba(n, Corner::bottom_left, 17, 18, 19, 208)
    };
    const auto a = [&](std::size_t quadrant, std::uint32_t x, std::uint32_t y)
    {
        return corners[quadrant][(static_cast<std::size_t>(y) * n + x) * 4U + 3U];
    };
    for (const auto& corner : corners)
    {
        assert(corner.size() == n * n * 4U);
        assert(corner[0] == 17U && corner[1] == 18U && corner[2] == 19U);
    }
    assert(a(0, 0, 0) == 0);
    assert(a(0, n - 1, n - 1) == 208);
    assert(a(1, n - 1, 0) == 0);
    assert(a(2, n - 1, n - 1) == 0);
    assert(a(3, 0, n - 1) == 0);
    for (std::uint32_t y = 0; y < n; ++y)
        for (std::uint32_t x = 0; x < n; ++x)
        {
            assert(a(0, x, y) == a(1, n - 1 - x, y));
            assert(a(0, x, y) == a(2, n - 1 - x, n - 1 - y));
            assert(a(0, x, y) == a(3, x, n - 1 - y));
        }
    // Antialiasing must produce transitional alpha values, not polygon stair-steps.
    bool antialiased = false;
    for (std::uint32_t y = 0; y < n; ++y)
        for (std::uint32_t x = 0; x < n; ++x)
            antialiased |= a(0, x, y) > 0 && a(0, x, y) < 208;
    assert(antialiased);
    const auto translucent = rgba(n, Corner::top_left, 6, 7, 8, 80);
    assert(translucent[(static_cast<std::size_t>(n - 1) * n + n - 1) * 4U + 3U] == 80);
}
