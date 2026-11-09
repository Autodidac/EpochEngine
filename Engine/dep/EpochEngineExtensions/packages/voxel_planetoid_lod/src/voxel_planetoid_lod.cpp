// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/voxel_planetoid_lod/voxel_planetoid_lod.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace epoch::extensions::voxel_planetoid_lod
{
    namespace
    {
        [[nodiscard]] double hash_noise(std::int32_t x, std::int32_t y, std::int32_t z, std::uint32_t seed) noexcept
        {
            auto value = static_cast<std::uint64_t>(seed) + 0x9e3779b97f4a7c15ULL;
            value ^= static_cast<std::uint64_t>(static_cast<std::int64_t>(x)) * 0xbf58476d1ce4e5b9ULL;
            value ^= static_cast<std::uint64_t>(static_cast<std::int64_t>(y)) * 0x94d049bb133111ebULL;
            value ^= static_cast<std::uint64_t>(static_cast<std::int64_t>(z)) * 0x632be59bd9b4e019ULL;
            value ^= value >> 30U;
            value *= 0xbf58476d1ce4e5b9ULL;
            value ^= value >> 27U;
            value *= 0x94d049bb133111ebULL;
            value ^= value >> 31U;
            return static_cast<double>(value & 0xffffU) / 32767.5 - 1.0;
        }

        [[nodiscard]] double terrain_noise(Vec3 position, std::uint32_t seed) noexcept
        {
            const auto scale = 0.03125;
            const auto x = static_cast<std::int32_t>(std::floor(position.x * scale));
            const auto y = static_cast<std::int32_t>(std::floor(position.y * scale));
            const auto z = static_cast<std::int32_t>(std::floor(position.z * scale));
            return (hash_noise(x, y, z, seed) * 0.65) + (hash_noise(x * 2, y * 2, z * 2, seed + 17U) * 0.35);
        }
    }

    double length(Vec3 value) noexcept
    {
        return std::sqrt((value.x * value.x) + (value.y * value.y) + (value.z * value.z));
    }

    VoxelSample sample_planetoid(const PlanetoidSettings& settings, Vec3 position) noexcept
    {
        const auto radialDistance = length(position);
        const auto terrain = terrain_noise(position, settings.seed) * settings.terrainAmplitude;
        const auto surfaceRadius = settings.radius + terrain;
        const auto density = static_cast<float>(surfaceRadius - radialDistance);
        const auto heightBand = radialDistance < settings.radius ? 1U : 2U;
        const auto biome = terrain > 4.0 ? 3U : (terrain < -4.0 ? 2U : 1U);
        return VoxelSample{density, static_cast<std::uint16_t>(heightBand), static_cast<std::uint16_t>(biome)};
    }

    std::uint8_t choose_lod(double cameraDistance, std::span<const LodShell> shells) noexcept
    {
        for (const auto& shell : shells)
        {
            if (cameraDistance <= shell.maxDistance)
            {
                return shell.lod;
            }
        }
        return shells.empty() ? 0U : shells.back().lod;
    }

    ChunkKey chunk_key_for_position(const PlanetoidSettings& settings, Vec3 position, std::uint8_t lod) noexcept
    {
        const auto lodScale = static_cast<double>(std::uint32_t{1} << lod);
        const auto size = settings.chunkWorldSize * lodScale;
        return ChunkKey{
            ChunkCoord{
                static_cast<std::int32_t>(std::floor(position.x / size)),
                static_cast<std::int32_t>(std::floor(position.y / size)),
                static_cast<std::int32_t>(std::floor(position.z / size))
            },
            lod
        };
    }

    std::vector<ChunkKey> visible_chunk_keys(const PlanetoidSettings& settings, Vec3 cameraPosition, std::span<const LodShell> shells, std::int32_t shellRadius)
    {
        std::vector<ChunkKey> keys;
        const auto lod = choose_lod(std::max(0.0, length(cameraPosition) - settings.radius), shells);
        const auto center = chunk_key_for_position(settings, cameraPosition, lod);
        const auto radius = std::max(0, shellRadius);

        for (std::int32_t z = -radius; z <= radius; ++z)
        {
            for (std::int32_t y = -radius; y <= radius; ++y)
            {
                for (std::int32_t x = -radius; x <= radius; ++x)
                {
                    keys.push_back(ChunkKey{
                        ChunkCoord{center.coord.x + x, center.coord.y + y, center.coord.z + z},
                        lod
                    });
                }
            }
        }

        std::ranges::sort(keys, {}, [](const ChunkKey& key) {
            return std::tuple{key.lod, key.coord.z, key.coord.y, key.coord.x};
        });

        return keys;
    }
}
