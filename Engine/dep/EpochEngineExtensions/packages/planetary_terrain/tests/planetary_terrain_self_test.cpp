// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/planetary_terrain/planetary_terrain.hpp"

#include <stdexcept>

using namespace epoch::extensions::planetary_terrain;

int main()
{
    const PlanetProfile profile{6'371'000.0, 2200.0, 0.0, 73U};
    const auto equator = sample_terrain(profile, GeoCoord{0.0, 0.0});
    const auto pole = sample_terrain(profile, GeoCoord{1.57079632679, 0.0});

    if (equator.temperature < pole.temperature)
    {
        throw std::runtime_error("planetary terrain temperature profile is inverted");
    }

    const auto tile = tile_for_coord(GeoCoord{0.1, 0.2}, 5U);
    const auto nearTiles = neighborhood(tile, 2);
    if (nearTiles.size() != 25U)
    {
        throw std::runtime_error("planetary terrain tile neighborhood size mismatch");
    }

    return 0;
}
