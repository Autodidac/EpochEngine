// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/voxel_authoring_tools/voxel_authoring_tools.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace epoch::extensions::voxel_authoring
{
    namespace
    {
        [[nodiscard]] constexpr std::uint64_t mix(std::uint64_t value) noexcept
        {
            value ^= value >> 30U;
            value *= 0xbf58476d1ce4e5b9ULL;
            value ^= value >> 27U;
            value *= 0x94d049bb133111ebULL;
            value ^= value >> 31U;
            return value;
        }

        [[nodiscard]] constexpr std::int64_t squared_distance(VoxelCoord a, VoxelCoord b) noexcept
        {
            const auto dx = static_cast<std::int64_t>(a.x) - b.x;
            const auto dy = static_cast<std::int64_t>(a.y) - b.y;
            const auto dz = static_cast<std::int64_t>(a.z) - b.z;
            return dx * dx + dy * dy + dz * dz;
        }
    }

    std::size_t VoxelCoordHasher::operator()(const VoxelCoord& coord) const noexcept
    {
        const auto x = mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(coord.x)));
        const auto y = mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(coord.y)) + 0x9e3779b97f4a7c15ULL);
        const auto z = mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(coord.z)) + 0x632be59bd9b4e019ULL);
        return static_cast<std::size_t>(mix(x ^ (y << 1U) ^ (z << 2U)));
    }

    void SparseVoxelVolume::clear() noexcept
    {
        cells_.clear();
        bounds_ = {};
    }

    void SparseVoxelVolume::set(VoxelCoord coord, VoxelCell cell)
    {
        cells_[coord] = cell;
        include(coord);
    }

    VoxelCell SparseVoxelVolume::sample(VoxelCoord coord) const noexcept
    {
        const auto found = cells_.find(coord);
        return found == cells_.end() ? VoxelCell{} : found->second;
    }

    bool SparseVoxelVolume::contains(VoxelCoord coord) const noexcept
    {
        return cells_.contains(coord);
    }

    std::size_t SparseVoxelVolume::occupied_count() const noexcept
    {
        return cells_.size();
    }

    Bounds SparseVoxelVolume::bounds() const noexcept
    {
        return bounds_;
    }

    std::vector<EditRecord> SparseVoxelVolume::apply_sphere_brush(const SphereBrush& brush)
    {
        std::vector<EditRecord> edits;
        const auto radius = std::max(0, brush.radius);
        const auto radiusSquared = static_cast<std::int64_t>(radius) * radius;

        for (std::int32_t z = brush.center.z - radius; z <= brush.center.z + radius; ++z)
        {
            for (std::int32_t y = brush.center.y - radius; y <= brush.center.y + radius; ++y)
            {
                for (std::int32_t x = brush.center.x - radius; x <= brush.center.x + radius; ++x)
                {
                    const VoxelCoord coord{x, y, z};
                    if (squared_distance(coord, brush.center) > radiusSquared)
                    {
                        continue;
                    }

                    const auto before = sample(coord);
                    auto after = before;
                    after.density += brush.densityDelta;
                    after.material = brush.material;
                    set(coord, after);
                    edits.push_back(EditRecord{coord, before, after});
                }
            }
        }

        return edits;
    }

    std::vector<VoxelCoord> SparseVoxelVolume::occupied_coords() const
    {
        std::vector<VoxelCoord> coords;
        coords.reserve(cells_.size());
        for (const auto& [coord, cell] : cells_)
        {
            (void)cell;
            coords.push_back(coord);
        }

        std::ranges::sort(coords, {}, [](const VoxelCoord& coord) {
            return std::tuple{coord.z, coord.y, coord.x};
        });

        return coords;
    }

    void SparseVoxelVolume::include(VoxelCoord coord) noexcept
    {
        if (!bounds_.valid)
        {
            bounds_ = Bounds{coord, coord, true};
            return;
        }

        bounds_.min.x = std::min(bounds_.min.x, coord.x);
        bounds_.min.y = std::min(bounds_.min.y, coord.y);
        bounds_.min.z = std::min(bounds_.min.z, coord.z);
        bounds_.max.x = std::max(bounds_.max.x, coord.x);
        bounds_.max.y = std::max(bounds_.max.y, coord.y);
        bounds_.max.z = std::max(bounds_.max.z, coord.z);
    }

    std::vector<EditRecord> replay_brushes(SparseVoxelVolume& volume, std::span<const SphereBrush> brushes)
    {
        std::vector<EditRecord> edits;
        for (const auto& brush : brushes)
        {
            auto brushEdits = volume.apply_sphere_brush(brush);
            edits.insert(edits.end(), brushEdits.begin(), brushEdits.end());
        }
        return edits;
    }
}
