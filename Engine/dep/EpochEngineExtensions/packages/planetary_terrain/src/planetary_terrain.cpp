// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/planetary_terrain/planetary_terrain.hpp"

#include <algorithm>
#include <cmath>

namespace epoch::extensions::planetary_terrain
{
    namespace
    {
        constexpr double kPi = 3.141592653589793238462643383279502884;

        [[nodiscard]] double normalized_hash(std::int32_t x, std::int32_t y, std::uint32_t seed) noexcept
        {
            auto value = static_cast<std::uint64_t>(seed) + 0x9e3779b97f4a7c15ULL;
            value ^= static_cast<std::uint64_t>(static_cast<std::int64_t>(x)) * 0xbf58476d1ce4e5b9ULL;
            value ^= static_cast<std::uint64_t>(static_cast<std::int64_t>(y)) * 0x94d049bb133111ebULL;
            value ^= value >> 30U;
            value *= 0xbf58476d1ce4e5b9ULL;
            value ^= value >> 27U;
            value *= 0x94d049bb133111ebULL;
            value ^= value >> 31U;
            return static_cast<double>(value & 0xffffU) / 65535.0;
        }

        [[nodiscard]] double octave_noise(double latitude, double longitude, std::uint32_t seed) noexcept
        {
            double value = 0.0;
            double amplitude = 1.0;
            double totalAmplitude = 0.0;
            for (std::int32_t octave = 0; octave < 5; ++octave)
            {
                const auto scale = static_cast<double>(1 << octave);
                const auto x = static_cast<std::int32_t>(std::floor((longitude + kPi) * scale * 32.0));
                const auto y = static_cast<std::int32_t>(std::floor((latitude + (kPi * 0.5)) * scale * 32.0));
                value += normalized_hash(x, y, seed + static_cast<std::uint32_t>(octave * 31)) * amplitude;
                totalAmplitude += amplitude;
                amplitude *= 0.5;
            }
            return (value / totalAmplitude) * 2.0 - 1.0;
        }

        [[nodiscard]] Biome classify_biome(double height, float humidity, float temperature) noexcept
        {
            if (height < -10.0)
            {
                return Biome::Ocean;
            }
            if (height < 25.0)
            {
                return Biome::Coast;
            }
            if (height > 1500.0)
            {
                return Biome::Peaks;
            }
            if (height > 650.0 || temperature < 0.2F)
            {
                return Biome::Highlands;
            }
            (void)humidity;
            return Biome::Plains;
        }
    }

    TerrainSample sample_terrain(const PlanetProfile& profile, GeoCoord coord) noexcept
    {
        const auto latitude = std::clamp(coord.latitudeRadians, -kPi * 0.5, kPi * 0.5);
        const auto longitude = std::remainder(coord.longitudeRadians, kPi * 2.0);
        const auto elevationNoise = octave_noise(latitude, longitude, profile.seed);
        const auto humidityNoise = octave_noise(latitude + 3.1, longitude - 1.7, profile.seed + 101U);
        const auto height = (elevationNoise * profile.terrainAmplitudeMeters) - profile.oceanLevelMeters;
        const auto polar = std::abs(latitude) / (kPi * 0.5);
        const auto temperature = static_cast<float>(std::clamp(1.0 - polar - (height / 9000.0), 0.0, 1.0));
        const auto humidity = static_cast<float>(std::clamp((humidityNoise + 1.0) * 0.5, 0.0, 1.0));
        return TerrainSample{height, classify_biome(height, humidity, temperature), humidity, temperature};
    }

    TerrainTile tile_for_coord(GeoCoord coord, std::uint8_t lod) noexcept
    {
        const auto tileCount = static_cast<std::int32_t>(1U << std::min<std::uint8_t>(lod, 20U));
        const auto u = (std::remainder(coord.longitudeRadians, kPi * 2.0) + kPi) / (kPi * 2.0);
        const auto v = (std::clamp(coord.latitudeRadians, -kPi * 0.5, kPi * 0.5) + (kPi * 0.5)) / kPi;
        return TerrainTile{
            0,
            std::clamp(static_cast<std::int32_t>(std::floor(u * tileCount)), 0, tileCount - 1),
            std::clamp(static_cast<std::int32_t>(std::floor(v * tileCount)), 0, tileCount - 1),
            lod
        };
    }

    std::vector<TerrainTile> neighborhood(TerrainTile center, std::int32_t radius)
    {
        std::vector<TerrainTile> tiles;
        const auto safeRadius = std::max(0, radius);
        for (std::int32_t y = -safeRadius; y <= safeRadius; ++y)
        {
            for (std::int32_t x = -safeRadius; x <= safeRadius; ++x)
            {
                tiles.push_back(TerrainTile{center.face, center.x + x, center.y + y, center.lod});
            }
        }
        return tiles;
    }
}
