// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/voxel_planetoid_lod/voxel_planetoid_lod.hpp"

#include <array>
#include <stdexcept>

using namespace epoch::extensions::voxel_planetoid_lod;

int main()
{
    const PlanetoidSettings settings{256.0, 18.0, 32.0, 99U};
    const auto inside = sample_planetoid(settings, Vec3{0.0, 255.0, 0.0});
    const auto outside = sample_planetoid(settings, Vec3{0.0, 330.0, 0.0});
    if (inside.density <= outside.density)
    {
        throw std::runtime_error("planetoid density did not decrease away from surface");
    }

    const std::array shells{
        LodShell{64.0, 0U},
        LodShell{256.0, 1U},
        LodShell{1024.0, 2U}
    };

    if (choose_lod(40.0, shells) != 0U || choose_lod(500.0, shells) != 2U)
    {
        throw std::runtime_error("planetoid LOD shell selection failed");
    }

    const auto keys = visible_chunk_keys(settings, Vec3{0.0, 280.0, 0.0}, shells, 1);
    if (keys.size() != 27U)
    {
        throw std::runtime_error("planetoid visible chunk shell size mismatch");
    }

    return 0;
}
