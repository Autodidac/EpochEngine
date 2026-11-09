// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#pragma once

#include <cstdint>
#include <vector>

namespace epoch::extensions::planetary_terrain
{
    enum class Biome : std::uint8_t
    {
        Ocean,
        Coast,
        Plains,
        Highlands,
        Peaks
    };

    struct PlanetProfile
    {
        double radiusMeters{6'371'000.0};
        double terrainAmplitudeMeters{2200.0};
        double oceanLevelMeters{0.0};
        std::uint32_t seed{1};
    };

    struct GeoCoord
    {
        double latitudeRadians{};
        double longitudeRadians{};
    };

    struct TerrainSample
    {
        double heightMeters{};
        Biome biome{Biome::Plains};
        float humidity{};
        float temperature{};
    };

    struct TerrainTile
    {
        std::int32_t face{};
        std::int32_t x{};
        std::int32_t y{};
        std::uint8_t lod{};
    };

    [[nodiscard]] TerrainSample sample_terrain(const PlanetProfile& profile, GeoCoord coord) noexcept;
    [[nodiscard]] TerrainTile tile_for_coord(GeoCoord coord, std::uint8_t lod) noexcept;
    [[nodiscard]] std::vector<TerrainTile> neighborhood(TerrainTile center, std::int32_t radius);
}
