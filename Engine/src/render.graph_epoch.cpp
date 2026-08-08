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

#include "../include/core.stl_types.hpp"

module render.graph;

namespace epochengine
{
    GraphResource GraphBuilder::create_buffer(epochengine::string_view name, const BufferDesc& desc)
    {
        const u32 idx = static_cast<u32>(m_buffers.size());
        m_buffers.push_back(GraphBuffer{ desc, {} });

        ResourceDecl r{
            .kind = ResourceKind::buffer,
            .name = epochengine::string{name},   // requires ctor from epochengine::string_view
            .index = idx
        };
        m_resources.push_back(std::move(r));

        return GraphResource{ static_cast<u32>(m_resources.size()) };
    }


    GraphResource GraphBuilder::create_texture(epochengine::string_view name, const TextureDesc& desc)
    {
        const u32 idx = static_cast<u32>(m_textures.size());
        m_textures.push_back(GraphTexture{ desc, {} });
        m_resources.push_back(ResourceDecl{ ResourceKind::texture, epochengine::string(name), idx });
        return GraphResource{ static_cast<u32>(m_resources.size()) };
    }

    GraphResource GraphBuilder::create_sampler(epochengine::string_view name, const SamplerDesc& desc)
    {
        const u32 idx = static_cast<u32>(m_samplers.size());
        m_samplers.push_back(GraphSampler{ desc, {} });
        m_resources.push_back(ResourceDecl{ ResourceKind::sampler, epochengine::string(name), idx });
        return GraphResource{ static_cast<u32>(m_resources.size()) };
    }

    GraphResource GraphBuilder::create_material(epochengine::string_view name,
                                                const MaterialDesc& desc,
                                                epochengine::array_view<const GraphMaterialTextureSlot> texture_slots)
    {
        const u32 idx = static_cast<u32>(m_materials.size());
        GraphMaterial material{};
        material.desc = desc;
        material.texture_slots.assign(texture_slots.begin(), texture_slots.end());
        m_materials.push_back(std::move(material));
        m_resources.push_back(ResourceDecl{ ResourceKind::material, epochengine::string(name), idx });
        return GraphResource{ static_cast<u32>(m_resources.size()) };
    }

    GraphResource GraphBuilder::create_render_target(epochengine::string_view name, const RenderTargetDesc& desc)
    {
        const u32 idx = static_cast<u32>(m_render_targets.size());
        m_render_targets.push_back(GraphRenderTarget{ desc, {} });
        m_resources.push_back(ResourceDecl{ ResourceKind::render_target, epochengine::string(name), idx });
        return GraphResource{ static_cast<u32>(m_resources.size()) };
    }

    GraphResource GraphBuilder::create_mesh(epochengine::string_view name,
                                            const MeshDesc& desc,
                                            GraphResource vertex_buffer,
                                            GraphResource index_buffer,
                                            GraphResource material)
    {
        const u32 idx = static_cast<u32>(m_meshes.size());
        GraphMesh mesh{};
        mesh.desc = desc;
        mesh.vertex_buffer = vertex_buffer;
        mesh.index_buffer = index_buffer;
        mesh.material = material;
        m_meshes.push_back(std::move(mesh));
        m_resources.push_back(ResourceDecl{ ResourceKind::mesh, epochengine::string(name), idx });
        return GraphResource{ static_cast<u32>(m_resources.size()) };
    }

    GraphResource GraphBuilder::create_model(epochengine::string_view name,
                                             const ModelDesc& desc,
                                             epochengine::array_view<const GraphModelMeshSlot> mesh_slots)
    {
        const u32 idx = static_cast<u32>(m_models.size());
        GraphModel model{};
        model.desc = desc;
        model.mesh_slots.assign(mesh_slots.begin(), mesh_slots.end());
        m_models.push_back(std::move(model));
        m_resources.push_back(ResourceDecl{ ResourceKind::model, epochengine::string(name), idx });
        return GraphResource{ static_cast<u32>(m_resources.size()) };
    }

    GraphRenderTextureAsset GraphBuilder::create_render_texture_asset(
        epochengine::string_view name,
        const RenderTextureAssetDesc& desc)
    {
        GraphRenderTextureAsset asset{};
        asset.name = epochengine::string(name);
        asset.desc = desc;
        asset.plan = make_render_texture_asset_plan(desc);
        asset.color_texture = create_texture(name, asset.plan.color_texture);
        asset.sampler = create_sampler(name, asset.plan.sampler);
        asset.render_target = create_render_target(name, asset.plan.render_target);
        m_render_texture_assets.push_back(asset);
        return asset;
    }

    GraphPass GraphBuilder::add_pass(epochengine::string_view name,
                                     epochengine::array_view<const GraphResource> reads,
                                     epochengine::array_view<const GraphResource> writes,
                                     epochengine::function_ref<void(ICommandContext&)> fn)
    {
        PassDecl p{};
        p.name = epochengine::string(name);
        p.reads.assign(reads.begin(), reads.end());
        p.writes.assign(writes.begin(), writes.end());
        p.execute = std::move(fn);
        m_passes.push_back(std::move(p));
        return GraphPass{ static_cast<u32>(m_passes.size()) };
    }

    GraphPass GraphBuilder::add_render_pass(epochengine::string_view name,
                                            GraphResource target,
                                            epochengine::array_view<const GraphResource> reads,
                                            epochengine::array_view<const GraphResource> writes,
                                            const RenderPassDesc& pass,
                                            epochengine::function_ref<void(ICommandContext&)> fn)
    {
        PassDecl p{};
        p.name = epochengine::string(name);
        p.reads.assign(reads.begin(), reads.end());
        p.writes.assign(writes.begin(), writes.end());
        p.render_target_resource = target;
        p.render_pass = pass;
        p.execute = std::move(fn);
        m_passes.push_back(std::move(p));
        return GraphPass{ static_cast<u32>(m_passes.size()) };
    }

    void GraphBuilder::add_model_draw(GraphPass pass, GraphResource model)
    {
        if (!pass || !model)
            return;

        const u32 passIndex = pass.value - 1u;
        if (passIndex >= m_passes.size())
            return;

        m_passes[passIndex].draw_models.push_back(GraphModelDraw{
            .model = model,
            .backend = {}
        });
    }

    CompiledGraph GraphBuilder::compile(IRenderDevice& dev) const
    {
        CompiledGraph g{};
        g.resources = m_resources;
        g.buffers   = m_buffers;
        g.textures  = m_textures;
        g.samplers = m_samplers;
        g.materials = m_materials;
        g.render_targets = m_render_targets;
        g.meshes = m_meshes;
        g.models = m_models;
        g.render_texture_assets = m_render_texture_assets;
        g.passes    = m_passes;

        auto resolve_buffer = [&g](GraphResource handle) -> GraphBuffer*
        {
            const u32 resourceIndex = handle.value;
            if (resourceIndex == 0 || resourceIndex > g.resources.size())
                return nullptr;

            const ResourceDecl& resource = g.resources[resourceIndex - 1u];
            if (resource.kind != ResourceKind::buffer || resource.index >= g.buffers.size())
                return nullptr;

            return &g.buffers[resource.index];
        };

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

        auto resolve_sampler = [&g](GraphResource handle) -> GraphSampler*
        {
            const u32 resourceIndex = handle.value;
            if (resourceIndex == 0 || resourceIndex > g.resources.size())
                return nullptr;

            const ResourceDecl& resource = g.resources[resourceIndex - 1u];
            if (resource.kind != ResourceKind::sampler || resource.index >= g.samplers.size())
                return nullptr;

            return &g.samplers[resource.index];
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

        auto resolve_material = [&g](GraphResource handle) -> GraphMaterial*
        {
            const u32 resourceIndex = handle.value;
            if (resourceIndex == 0 || resourceIndex > g.resources.size())
                return nullptr;

            const ResourceDecl& resource = g.resources[resourceIndex - 1u];
            if (resource.kind != ResourceKind::material || resource.index >= g.materials.size())
                return nullptr;

            return &g.materials[resource.index];
        };

        auto resolve_mesh = [&g](GraphResource handle) -> GraphMesh*
        {
            const u32 resourceIndex = handle.value;
            if (resourceIndex == 0 || resourceIndex > g.resources.size())
                return nullptr;

            const ResourceDecl& resource = g.resources[resourceIndex - 1u];
            if (resource.kind != ResourceKind::mesh || resource.index >= g.meshes.size())
                return nullptr;

            return &g.meshes[resource.index];
        };

        auto resolve_model = [&g](GraphResource handle) -> GraphModel*
        {
            const u32 resourceIndex = handle.value;
            if (resourceIndex == 0 || resourceIndex > g.resources.size())
                return nullptr;

            const ResourceDecl& resource = g.resources[resourceIndex - 1u];
            if (resource.kind != ResourceKind::model || resource.index >= g.models.size())
                return nullptr;

            return &g.models[resource.index];
        };

        auto append_binding = [&g, &resolve_texture, &resolve_sampler](CommandResourceBindings& bindings, GraphResource handle, bool write)
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
                    {
                        bindings.write_textures.push_back(g.textures[resource.index].backend);
                    }
                    else
                    {
                        bindings.read_textures.push_back(g.textures[resource.index].backend);
                        if (g.textures[resource.index].sampled_sampler)
                            bindings.read_samplers.push_back(g.textures[resource.index].sampled_sampler);
                    }
                }
                break;
            case ResourceKind::sampler:
                if (!write && resource.index < g.samplers.size() && g.samplers[resource.index].backend)
                    bindings.read_samplers.push_back(g.samplers[resource.index].backend);
                break;
            case ResourceKind::material:
                if (resource.index < g.materials.size() && g.materials[resource.index].backend)
                {
                    GraphMaterial& material = g.materials[resource.index];
                    if (write)
                    {
                        bindings.write_materials.push_back(material.backend);
                    }
                    else
                    {
                        bindings.read_materials.push_back(material.backend);
                        for (const GraphMaterialTextureSlot& slot : material.texture_slots)
                        {
                            const GraphTexture* texture = resolve_texture(slot.texture);
                            if (!texture || !texture->backend)
                                continue;

                            const GraphSampler* explicitSampler = resolve_sampler(slot.sampler);
                            const SamplerHandle sampler = explicitSampler && explicitSampler->backend
                                ? explicitSampler->backend
                                : texture->sampled_sampler;

                            if (slot.slot == MaterialTextureSlot::render_surface
                                && (!texture->owned_by_render_texture_asset
                                    || !texture->sampled_sampler
                                    || !sampler
                                    || (explicitSampler && explicitSampler->backend != texture->sampled_sampler)))
                            {
                                continue;
                            }

                            bindings.read_material_textures.push_back(MaterialTextureBinding{
                                .slot = slot.slot,
                                .texture = texture->backend,
                                .sampler = sampler
                            });
                            if (sampler)
                                bindings.read_samplers.push_back(sampler);
                        }
                    }
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
            case ResourceKind::mesh:
                if (resource.index < g.meshes.size() && g.meshes[resource.index].backend)
                {
                    if (write)
                        bindings.write_meshes.push_back(g.meshes[resource.index].backend);
                    else
                        bindings.read_meshes.push_back(g.meshes[resource.index].backend);
                }
                break;
            case ResourceKind::model:
                if (resource.index < g.models.size() && g.models[resource.index].backend)
                {
                    if (write)
                        bindings.write_models.push_back(g.models[resource.index].backend);
                    else
                        bindings.read_models.push_back(g.models[resource.index].backend);
                }
                break;
            }
        };

        for (auto& asset : g.render_texture_assets)
        {
            asset.backend = dev.create_render_texture_asset(asset.desc);
            const bool satisfiesRequirements = asset.backend.satisfies(asset.plan.backend_requirements);

            if (!satisfiesRequirements)
                continue;

            if (GraphTexture* texture = resolve_texture(asset.color_texture))
            {
                if (asset.backend.color_texture)
                {
                    texture->backend = asset.backend.color_texture;
                    texture->sampled_sampler = asset.backend.sampler;
                    texture->owned_by_render_texture_asset = true;
                }
            }

            if (GraphSampler* sampler = resolve_sampler(asset.sampler))
            {
                if (asset.backend.sampler)
                {
                    sampler->backend = asset.backend.sampler;
                    sampler->owned_by_render_texture_asset = true;
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
        for (auto& sampler : g.samplers)
        {
            if (!sampler.backend)
                sampler.backend = dev.create_sampler(sampler.desc);
        }
        for (auto& rt : g.render_targets)
        {
            if (!rt.backend)
                rt.backend = dev.create_render_target(rt.desc);
        }
        for (auto& material : g.materials)
        {
            if (!material.backend)
                material.backend = dev.create_material(material.desc);
        }
        for (auto& mesh : g.meshes)
        {
            MeshDesc resolvedDesc = mesh.desc;
            if (GraphBuffer* vertexBuffer = resolve_buffer(mesh.vertex_buffer))
            {
                if (vertexBuffer->backend)
                    resolvedDesc.vertex_buffer = vertexBuffer->backend;
            }
            if (GraphBuffer* indexBuffer = resolve_buffer(mesh.index_buffer))
            {
                if (indexBuffer->backend)
                    resolvedDesc.index_buffer = indexBuffer->backend;
            }
            if (GraphMaterial* material = resolve_material(mesh.material))
            {
                if (material->backend)
                    resolvedDesc.material = material->backend;
            }
            if (!mesh.backend)
                mesh.backend = dev.create_mesh(resolvedDesc);
        }
        for (auto& model : g.models)
        {
            ModelDesc resolvedDesc = model.desc;
            for (const GraphModelMeshSlot& slot : model.mesh_slots)
            {
                GraphMesh* mesh = resolve_mesh(slot.mesh);
                if (!mesh || !mesh->backend)
                    continue;

                ModelMeshDesc meshDesc{};
                meshDesc.mesh = mesh->backend;
                if (GraphMaterial* material = resolve_material(slot.material))
                    meshDesc.material = material->backend;
                meshDesc.node_name = slot.node_name.empty() ? nullptr : slot.node_name.c_str();
                for (u32 i = 0; i < 16; ++i)
                    meshDesc.transform[i] = slot.transform[i];
                resolvedDesc.meshes.push_back(meshDesc);
            }
            if (!model.backend)
                model.backend = dev.create_model(resolvedDesc);
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
            for (GraphModelDraw& draw : pass.draw_models)
            {
                if (GraphModel* model = resolve_model(draw.model))
                    draw.backend = model->backend;
            }
            if (!pass.bindings.empty())
                pass.binding_set = dev.create_binding_set(pass.bindings);
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
            if (p.binding_set)
                ctx.bind_binding_set(p.binding_set);
            ctx.bind_resources(p.bindings);
            if (p.render_target)
                ctx.begin_render_pass(p.render_target, p.render_pass);
            if (p.execute) p.execute(ctx);
            for (const GraphModelDraw& draw : p.draw_models)
            {
                if (draw.backend)
                    ctx.draw_model(draw.backend);
            }
            if (p.render_target)
                ctx.end_render_pass();
            ctx.barrier();
        }
        ctx.end();
    }

    void CompiledGraph::destroy(IRenderDevice& dev) noexcept
    {
        for (auto& pass : passes)
        {
            if (pass.binding_set)
                dev.destroy(pass.binding_set);
        }

        for (auto& asset : render_texture_assets)
        {
            if (asset.backend.color_texture || asset.backend.sampler || asset.backend.render_target)
                dev.destroy(asset.backend);
        }

        for (auto& model : models)
        {
            if (model.backend)
                dev.destroy(model.backend);
        }
        for (auto& mesh : meshes)
        {
            if (mesh.backend)
                dev.destroy(mesh.backend);
        }
        for (auto& material : materials)
        {
            if (material.backend)
                dev.destroy(material.backend);
        }
        for (auto& b : buffers)  if (b.backend) dev.destroy(b.backend);
        for (auto& t : textures)
        {
            if (t.backend && !t.owned_by_render_texture_asset)
                dev.destroy(t.backend);
        }
        for (auto& sampler : samplers)
        {
            if (sampler.backend && !sampler.owned_by_render_texture_asset)
                dev.destroy(sampler.backend);
        }
        for (auto& rt : render_targets)
        {
            if (rt.backend && !rt.owned_by_render_texture_asset)
                dev.destroy(rt.backend);
        }
    }
} // namespace epoch
