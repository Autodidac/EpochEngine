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
module;

#include "../include/epoch.config.hpp"
#include "../src/epoch.common.hpp"
#include "../include/core.stl_types.hpp"

#include <array>   // for std::array (pass resource lists without std::vector)

export module render.visibility;

import render.graph;
import render.device;
import platform.budgets;

export namespace epochengine
{
    struct InstanceData
    {
        u32 mesh_id = 0;
        u32 material_id = 0;
        u32 transform_id = 0;
        u32 flags = 0;
    };

    struct VisibilityResources
    {
        GraphResource instance_buffer{};
        GraphResource visible_instance_ids{};
        GraphResource visibility_buffer{};
    };

    struct VisibilityPassConfig
    {
        u32 max_instances = 1'000'000;
        bool build_visibility_buffer = true;
    };

    [[nodiscard]] inline VisibilityResources declare_visibility_resources(
        GraphBuilder& g,
        const FramePolicy& p,
        const VisibilityPassConfig& cfg)
    {
        BufferDesc instances{};
        instances.size_bytes = static_cast<u64>(cfg.max_instances) * sizeof(InstanceData);
        instances.gpu_only = true;
        instances.storage = true;
        instances.debug_name = "instances";

        BufferDesc visible{};
        visible.size_bytes = static_cast<u64>(cfg.max_instances) * sizeof(u32);
        visible.gpu_only = true;
        visible.storage = true;
        visible.indirect = true;
        visible.debug_name = "visible_instance_ids";

        VisibilityResources r{};
        // Use a non-ambiguous conversion to epochengine::string_view:
        r.instance_buffer = g.create_buffer(epochengine::to_view(std::string_view{ "instances" }), instances);
        r.visible_instance_ids = g.create_buffer(epochengine::to_view(std::string_view{ "visible_instance_ids" }), visible);

        if (p.use_visibility_buffer && cfg.build_visibility_buffer)
        {
            TextureDesc vis{};
            vis.width = p.render_w;
            vis.height = p.render_h;
            vis.mip_levels = 1;
            vis.sampled = true;
            vis.storage = true;
            vis.render_target = true;
            vis.debug_name = "visibility_buffer";

            r.visibility_buffer = g.create_texture(epochengine::to_view(std::string_view{ "visibility_buffer" }), vis);
        }

        return r;
    }

    inline void add_visibility_passes(
        GraphBuilder& g,
        const FramePolicy& p,
        const VisibilityResources& r)
    {
        // Pass 1: CullInstancesCS
        {
            const std::array<GraphResource, 1> reads{ r.instance_buffer };
            const std::array<GraphResource, 1> writes{ r.visible_instance_ids };

            auto pass1 = g.add_pass(
                epochengine::to_view(std::string_view{ "CullInstancesCS" }),
                epochengine::span<const GraphResource>{ reads.data(), reads.size() },
                epochengine::span<const GraphResource>{ writes.data(), writes.size() },
                [=](ICommandContext& ctx)
                {
                    (void)p;
                    ctx.debug_marker("TODO: dispatch CullInstancesCS");
                }
            );
        }

        // Pass 2: VisibilityBufferRaster
        if (p.use_visibility_buffer && r.visibility_buffer)
        {
            const std::array<GraphResource, 1> reads{ r.visible_instance_ids };
            const std::array<GraphResource, 1> writes{ r.visibility_buffer };

            auto pass2 = g.add_pass(
                epochengine::to_view(std::string_view{ "VisibilityBufferRaster" }),
                epochengine::span<const GraphResource>{ reads.data(), reads.size() },
                epochengine::span<const GraphResource>{ writes.data(), writes.size() },
                [=](ICommandContext& ctx)
                {
                    ctx.debug_marker("TODO: render visibility buffer");
                }
            );
        }
    }
} // namespace epoch
