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

#include "../include/_epoch.stl_types.hpp"

module render.graph;

namespace epoch
{
    GraphResource GraphBuilder::create_buffer(epoch::string_view name, const BufferDesc& desc)
    {
        const u32 idx = static_cast<u32>(m_buffers.size());
        m_buffers.push_back(GraphBuffer{ desc, {} });

        ResourceDecl r{
            .kind = ResourceKind::buffer,
            .name = epoch::string{name},   // requires ctor from epoch::string_view
            .index = idx
        };
        m_resources.push_back(std::move(r));

        return GraphResource{ static_cast<u32>(m_resources.size()) };
    }


    GraphResource GraphBuilder::create_texture(epoch::string_view name, const TextureDesc& desc)
    {
        const u32 idx = static_cast<u32>(m_textures.size());
        m_textures.push_back(GraphTexture{ desc, {} });
        m_resources.push_back(ResourceDecl{ ResourceKind::texture, epoch::string(name), idx });
        return GraphResource{ static_cast<u32>(m_resources.size()) };
    }

    GraphResource GraphBuilder::create_render_target(epoch::string_view name, const RenderTargetDesc& desc)
    {
        const u32 idx = static_cast<u32>(m_render_targets.size());
        m_render_targets.push_back(GraphRenderTarget{ desc, {} });
        m_resources.push_back(ResourceDecl{ ResourceKind::render_target, epoch::string(name), idx });
        return GraphResource{ static_cast<u32>(m_resources.size()) };
    }

    GraphRenderTextureAsset GraphBuilder::create_render_texture_asset(
        epoch::string_view name,
        const RenderTextureAssetDesc& desc)
    {
        GraphRenderTextureAsset asset{};
        asset.plan = make_render_texture_asset_plan(desc);
        asset.color_texture = create_texture(name, asset.plan.color_texture);
        asset.render_target = create_render_target(name, asset.plan.render_target);
        return asset;
    }

    GraphPass GraphBuilder::add_pass(epoch::string_view name,
                                     epoch::array_view<const GraphResource> reads,
                                     epoch::array_view<const GraphResource> writes,
                                     epoch::function_ref<void(ICommandContext&)> fn)
    {
        PassDecl p{};
        p.name = epoch::string(name);
        p.reads.assign(reads.begin(), reads.end());
        p.writes.assign(writes.begin(), writes.end());
        p.execute = std::move(fn);
        m_passes.push_back(std::move(p));
        return GraphPass{ static_cast<u32>(m_passes.size()) };
    }

    GraphPass GraphBuilder::add_render_pass(epoch::string_view name,
                                            GraphResource target,
                                            epoch::array_view<const GraphResource> reads,
                                            epoch::array_view<const GraphResource> writes,
                                            const RenderPassDesc& pass,
                                            epoch::function_ref<void(ICommandContext&)> fn)
    {
        PassDecl p{};
        p.name = epoch::string(name);
        p.reads.assign(reads.begin(), reads.end());
        p.writes.assign(writes.begin(), writes.end());
        p.render_target_resource = target;
        p.render_pass = pass;
        p.execute = std::move(fn);

        m_passes.push_back(std::move(p));
        return GraphPass{ static_cast<u32>(m_passes.size()) };
    }

    CompiledGraph GraphBuilder::compile(IRenderDevice& dev) const
    {
        CompiledGraph g{};
        g.resources = m_resources;
        g.buffers   = m_buffers;
        g.textures  = m_textures;
        g.render_targets = m_render_targets;
        g.passes    = m_passes;

        for (auto& b : g.buffers)  b.backend = dev.create_buffer(b.desc);
        for (auto& t : g.textures) t.backend = dev.create_texture(t.desc);
        for (auto& rt : g.render_targets) rt.backend = dev.create_render_target(rt.desc);

        for (auto& pass : g.passes)
        {
            if (pass.render_target)
                continue;

            const u32 targetResourceIndex = pass.render_target_resource.value;
            if (targetResourceIndex > 0 && targetResourceIndex <= g.resources.size())
            {
                const ResourceDecl& resource = g.resources[targetResourceIndex - 1u];
                if (resource.kind == ResourceKind::render_target && resource.index < g.render_targets.size())
                {
                    pass.render_target = g.render_targets[resource.index].backend;
                    continue;
                }
            }

            for (const GraphResource resourceHandle : pass.writes)
            {
                const u32 resourceIndex = resourceHandle.value;
                if (resourceIndex == 0 || resourceIndex > g.resources.size())
                    continue;

                const ResourceDecl& resource = g.resources[resourceIndex - 1u];
                if (resource.kind == ResourceKind::render_target && resource.index < g.render_targets.size())
                {
                    pass.render_target = g.render_targets[resource.index].backend;
                    break;
                }
            }
        }

        return g;
    }

    void CompiledGraph::execute(IRenderDevice& dev)
    {
        auto& ctx = dev.acquire_graphics_context();
        ctx.begin("frame_graph");
        for (auto& p : passes)
        {
            ctx.debug_marker(p.name.c_str());
            if (p.render_target)
                ctx.begin_render_pass(p.render_target, p.render_pass);
            if (p.execute) p.execute(ctx);
            if (p.render_target)
                ctx.end_render_pass();
            ctx.barrier();
        }
        ctx.end();
    }

    void CompiledGraph::destroy(IRenderDevice& dev) noexcept
    {
        for (auto& b : buffers)  if (b.backend) dev.destroy(b.backend);
        for (auto& t : textures) if (t.backend) dev.destroy(t.backend);
        for (auto& rt : render_targets) if (rt.backend) dev.destroy(rt.backend);
    }
} // namespace epoch
