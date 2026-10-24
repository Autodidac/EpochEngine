// SPDX-License-Identifier: MPL-2.0
#include "../include/gui/circle_raster.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>

int main()
{
    using epochengine::gui_lib::circle_raster::rgba;
    assert(rgba(0, 1, 2, 3).empty());
    constexpr std::uint32_t size = 32;
    const auto pixels = rgba(size, 10, 20, 30, 240);
    assert(pixels.size() == size * size * 4u);
    const auto a = [&](std::uint32_t x, std::uint32_t y)
    { return pixels[(static_cast<std::size_t>(y) * size + x) * 4u + 3u]; };
    assert(a(0, 0) == 0 && a(31, 0) == 0);
    assert(a(16, 16) == 240 && a(15, 15) == 240);
    assert(a(0, 16) > 0 && a(0, 16) < 240);
    assert(a(0, 16) == a(31, 16));
    assert(a(16, 0) == a(16, 31));
    assert(a(8, 8) == a(23, 23));
    const auto semi = rgba(32, 1, 2, 3, 80);
    assert(semi[(16u * size + 16u) * 4u + 3u] == 80u);
}
