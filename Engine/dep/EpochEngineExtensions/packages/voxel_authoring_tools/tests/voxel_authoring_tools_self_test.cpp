// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/voxel_authoring_tools/voxel_authoring_tools.hpp"

#include <array>
#include <stdexcept>

using namespace epoch::extensions::voxel_authoring;

int main()
{
    SparseVoxelVolume volume;
    const std::array brushes{
        SphereBrush{VoxelCoord{0, 0, 0}, 2, 1.0F, 7},
        SphereBrush{VoxelCoord{1, 0, 0}, 1, -0.5F, 3}
    };

    const auto edits = replay_brushes(volume, brushes);
    if (edits.empty())
    {
        throw std::runtime_error("voxel authoring brush replay produced no edits");
    }

    const auto center = volume.sample(VoxelCoord{0, 0, 0});
    if (center.material != 3 || center.density <= 0.0F)
    {
        throw std::runtime_error("voxel authoring brush material/density replay failed");
    }

    const auto bounds = volume.bounds();
    if (!bounds.valid || bounds.min.x > -2 || bounds.max.x < 2)
    {
        throw std::runtime_error("voxel authoring bounds tracking failed");
    }

    const auto coords = volume.occupied_coords();
    if (coords.size() != volume.occupied_count())
    {
        throw std::runtime_error("voxel authoring occupied coordinate list mismatch");
    }

    return 0;
}
