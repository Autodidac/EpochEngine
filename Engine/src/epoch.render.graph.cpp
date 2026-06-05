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
        asset.name = epoch::string(name);
        asset.desc = desc;
        asset.plan = make_render_texture_asset_plan(desc);
        asset.color_texture = create_texture(name, asset.plan.color_texture);
        asset.render_target = create_render_target(name, asset.plan.render_target);
        m_render_texture_assets.push_back(asset);
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
        g.render_texture_assets = m_render_texture_assets;
        g.passes    = m_passes;

        auto resolve_texture = [&g](GraphResource handle) -> GraphTexture*
        {
            const u32 resourceIndex = handle.value;
            if (resourceIndex == 0 || resourceIndex > g.resources.size())
                return nullptr;

            const ResourceDecl& resource = g.resources[resourceIndex - 1u];
            if (resource.kind != ResourceKind::texture || resource.index >= g.textures.size())
                return nullptr;

            return &g.textures[resource.index];
        };

        auto resolve_render_target = [&g](GraphResource handle) -> GraphRenderTarget*
        {
            const u32 resourceIndex = handle.value;
            if (resourceIndex == 0 || resourceIndex > g.resources.size())
                return nullptr;

            const ResourceDecl& resource = g.resources[resourceIndex - 1u];
            if (resource.kind != ResourceKind::render_target || resource.index >= g.render_targets.size())
                return nullptr;

            return &g.render_targets[resource.index];
        };

        auto append_binding = [&g](CommandResourceBindings& bindings, GraphResource handle, bool write)
        {
            const u32 resourceIndex = handle.value;
            if (resourceIndex == 0 || resourceIndex > g.resources.size())
                return;

            const ResourceDecl& resource = g.resources[resourceIndex - 1u];
            switch (resource.kind)
            {
            case ResourceKind::buffer:
                if (resource.index < g.buffers.size() && g.buffers[resource.index].backend)
                {
                    if (write)
                        bindings.write_buffers.push_back(g.buffers[resource.index].backend);
                    else
                        bindings.read_buffers.push_back(g.buffers[resource.index].backend);
                }
                break;
            case ResourceKind::texture:
                if (resource.index < g.textures.size() && g.textures[resource.index].backend)
                {
                    if (write)
                        bindings.write_textures.push_back(g.textures[resource.index].backend);
                    else
                        bindings.read_textures.push_back(g.textures[resource.index].backend);
                }
                break;
            case ResourceKind::render_target:
                if (resource.index < g.render_targets.size() && g.render_targets[resource.index].backend)
                {
                    if (write)
                        bindings.write_render_targets.push_back(g.render_targets[resource.index].backend);
                    else
                        bindings.read_render_targets.push_back(g.render_targets[resource.index].backend);
                }
                break;
            }
        };

        for (auto& asset : g.render_texture_assets)
        {
            asset.backend = dev.create_render_texture_asset(asset.desc);

            if (GraphTexture* texture = resolve_texture(asset.color_texture))
            {
                if (asset.backend.color_texture)
                {
                    texture->backend = asset.backend.color_texture;
                    texture->owned_by_render_texture_asset = true;
                }
            }

            if (GraphRenderTarget* renderTarget = resolve_render_target(asset.render_target))
            {
                if (asset.backend.render_target)
                {
                    renderTarget->backend = asset.backend.render_target;
                    renderTarget->owned_by_render_texture_asset = true;
                }
            }
        }

        for (auto& b : g.buffers)  b.backend = dev.create_buffer(b.desc);
        for (auto& t : g.textures)
        {
            if (!t.backend)
                t.backend = dev.create_texture(t.desc);
        }
        for (auto& rt : g.render_targets)
        {
            if (!rt.backend)
                rt.backend = dev.create_render_target(rt.desc);
        }

        for (auto& pass : g.passes)
        {
            pass.bindings = {};
            for (const GraphResource resourceHandle : pass.reads)
                append_binding(pass.bindings, resourceHandle, false);
            if (pass.render_target_resource)
                append_binding(pass.bindings, pass.render_target_resource, true);
            for (const GraphResource resourceHandle : pass.writes)
                append_binding(pass.bindings, resourceHandle, true);
        }

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
            ctx.bind_resources(p.bindings);
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
        for (auto& asset : render_texture_assets)
        {
            if (asset.backend.color_texture || asset.backend.sampler || asset.backend.render_target)
                dev.destroy(asset.backend);
        }

        for (auto& b : buffers)  if (b.backend) dev.destroy(b.backend);
        for (auto& t : textures)
        {
            if (t.backend && !t.owned_by_render_texture_asset)
                dev.destroy(t.backend);
        }
        for (auto& rt : render_targets)
        {
            if (rt.backend && !rt.owned_by_render_texture_asset)
                dev.destroy(rt.backend);
        }
    }
} // namespace epoch
