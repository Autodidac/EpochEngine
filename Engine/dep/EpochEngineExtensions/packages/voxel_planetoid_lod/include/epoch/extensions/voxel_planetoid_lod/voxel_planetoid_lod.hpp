// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace epoch::extensions::voxel_planetoid_lod
{
    struct Vec3
    {
        double x{};
        double y{};
        double z{};
    };

    struct ChunkCoord
    {
        std::int32_t x{};
        std::int32_t y{};
        std::int32_t z{};
    };

    struct ChunkKey
    {
        ChunkCoord coord{};
        std::uint8_t lod{};

        friend constexpr bool operator==(const ChunkKey&, const ChunkKey&) = default;
    };

    struct LodShell
    {
        double maxDistance{};
        std::uint8_t lod{};
    };

    struct PlanetoidSettings
    {
        double radius{256.0};
        double terrainAmplitude{18.0};
        double chunkWorldSize{32.0};
        std::uint32_t seed{1};
    };

    struct VoxelSample
    {
        float density{};
        std::uint16_t material{};
        std::uint16_t biome{};
    };

    [[nodiscard]] double length(Vec3 value) noexcept;
    [[nodiscard]] VoxelSample sample_planetoid(const PlanetoidSettings& settings, Vec3 position) noexcept;
    [[nodiscard]] std::uint8_t choose_lod(double cameraDistance, std::span<const LodShell> shells) noexcept;
    [[nodiscard]] ChunkKey chunk_key_for_position(const PlanetoidSettings& settings, Vec3 position, std::uint8_t lod) noexcept;
    [[nodiscard]] std::vector<ChunkKey> visible_chunk_keys(const PlanetoidSettings& settings, Vec3 cameraPosition, std::span<const LodShell> shells, std::int32_t shellRadius);
}
