/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier: MPL-2.0             *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
// Epoch system notes (auto-checked):
// Epoch STL: honor core.stl_types.hpp types/operators; fix callers, not core overloads.
// Epoch Platform: keep native code isolated behind platform-specific interfaces.
module;

#include "../include/core.stl_types.hpp"

export module platform.capabilities;

export namespace epochengine
{
    enum class GpuTier : u8 { tier_a_desktop = 3, tier_b_mid = 2, tier_c_mobile = 1 };

    // Unknown capabilities grant no optional features or resource limits.
    // Backend adapters report only features actually enabled on their device.
    struct Capabilities
    {
        bool descriptor_indexing = false;
        bool bindless_textures   = false;
        bool sparse_resources    = false;

        bool indirect_draw       = false;
        bool multi_draw_indirect = false;
        bool subgroup_ops        = false;
        bool async_compute       = false;

        bool ray_tracing         = false;
        bool mesh_shaders        = false;

        u32 max_sampled_images   = 0;
        u32 max_samplers         = 0;
        u32 max_storage_buffers  = 0;
        u32 max_uniform_buffers  = 0;

        epochengine::string device_name{};
        epochengine::string api_name{};
    };

    struct TierKey
    {
        GpuTier tier = GpuTier::tier_c_mobile;
        bool use_bindless = false;
        bool use_sparse   = false;
        bool use_async    = false;

        u32 to_u32() const noexcept
        {
            return (static_cast<u32>(tier) << 24)
                 | (use_bindless ? (1u << 0) : 0u)
                 | (use_sparse   ? (1u << 1) : 0u)
                 | (use_async    ? (1u << 2) : 0u);
        }
    };

    [[nodiscard]] inline TierKey make_tier_key(const Capabilities& caps) noexcept
    {
        TierKey k{};
        if (caps.mesh_shaders || caps.ray_tracing || (caps.bindless_textures && caps.sparse_resources))
            k.tier = GpuTier::tier_a_desktop;
        else if (caps.bindless_textures || caps.subgroup_ops || caps.multi_draw_indirect)
            k.tier = GpuTier::tier_b_mid;
        else
            k.tier = GpuTier::tier_c_mobile;

        k.use_bindless = caps.bindless_textures && caps.descriptor_indexing;
        k.use_sparse   = caps.sparse_resources;
        k.use_async    = caps.async_compute;
        return k;
    }
} // namespace epoch
