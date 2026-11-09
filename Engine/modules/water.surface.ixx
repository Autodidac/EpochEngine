// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <cstdint>

export module water.surface;

import water.system;
import asset.mesh_artifact;

export namespace epochengine::water::surface
{
    [[nodiscard]] WaterBodyDesc default_body() noexcept;
    // Opaque analytic query geometry, independent of rendering and clocks.
    // cells is the number of grid cells on each horizontal axis (2..64).
    [[nodiscard]] asset::mesh::Result build(
        const WaterBodyDesc& body,
        double sample_time,
        std::uint32_t cells = 32) noexcept;
}
