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
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
 /**************************************************************
 *   Epoch Engine - Modern Renderer Skeleton (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

#include "../include/epoch.config.hpp"
#include "../src/epoch.common.hpp"
#include "../include/_epoch.stl_types.hpp"

export module platform.capabilities;

export namespace epochengine
{
    enum class GpuTier : u8 { tier_a_desktop = 3, tier_b_mid = 2, tier_c_mobile = 1 };

    struct Capabilities
    {
        bool descriptor_indexing = false;
        bool bindless_textures   = false;
        bool sparse_resources    = false;

        bool indirect_draw       = true;
        bool multi_draw_indirect = false;
        bool subgroup_ops        = false;
        bool async_compute       = false;

        bool ray_tracing         = false;
        bool mesh_shaders        = false;

        u32 max_sampled_images   = 4096;
        u32 max_samplers         = 2048;
        u32 max_storage_buffers  = 256;
        u32 max_uniform_buffers  = 256;

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
