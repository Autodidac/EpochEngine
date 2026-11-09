// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace epoch::extensions::voxel_authoring
{
    struct VoxelCoord
    {
        std::int32_t x{};
        std::int32_t y{};
        std::int32_t z{};

        friend constexpr bool operator==(const VoxelCoord&, const VoxelCoord&) = default;
    };

    struct Bounds
    {
        VoxelCoord min{};
        VoxelCoord max{};
        bool valid{};
    };

    struct VoxelCell
    {
        float density{};
        std::uint16_t material{};
    };

    struct SphereBrush
    {
        VoxelCoord center{};
        std::int32_t radius{1};
        float densityDelta{1.0F};
        std::uint16_t material{1};
    };

    struct EditRecord
    {
        VoxelCoord coord{};
        VoxelCell before{};
        VoxelCell after{};
    };

    struct VoxelCoordHasher
    {
        [[nodiscard]] std::size_t operator()(const VoxelCoord& coord) const noexcept;
    };

    class SparseVoxelVolume
    {
    public:
        void clear() noexcept;
        void set(VoxelCoord coord, VoxelCell cell);
        [[nodiscard]] VoxelCell sample(VoxelCoord coord) const noexcept;
        [[nodiscard]] bool contains(VoxelCoord coord) const noexcept;
        [[nodiscard]] std::size_t occupied_count() const noexcept;
        [[nodiscard]] Bounds bounds() const noexcept;

        [[nodiscard]] std::vector<EditRecord> apply_sphere_brush(const SphereBrush& brush);
        [[nodiscard]] std::vector<VoxelCoord> occupied_coords() const;

    private:
        std::unordered_map<VoxelCoord, VoxelCell, VoxelCoordHasher> cells_;
        Bounds bounds_{};

        void include(VoxelCoord coord) noexcept;
    };

    [[nodiscard]] std::vector<EditRecord> replay_brushes(SparseVoxelVolume& volume, std::span<const SphereBrush> brushes);
}
