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
#include "../include/core.stl_types.hpp"

export module render.graph;

import render.device;

export namespace epochengine
{
    struct GraphResourceTag {};
    struct GraphPassTag {};
    using GraphResource = Handle<GraphResourceTag, u32>;
    using GraphPass     = Handle<GraphPassTag, u32>;

    enum class ResourceKind : u8 { buffer, texture, sampler, material, render_target, mesh, model };

    struct GraphBuffer { BufferDesc desc{}; BufferHandle backend{}; };
    struct GraphTexture
    {
        TextureDesc desc{};
        TextureHandle backend{};
        SamplerHandle sampled_sampler{};
        bool owned_by_render_texture_asset{ false };
    };

    struct GraphSampler
    {
        SamplerDesc desc{};
        SamplerHandle backend{};
        bool owned_by_render_texture_asset{ false };
    };

    struct GraphMaterialTextureSlot
    {
        MaterialTextureSlot slot = MaterialTextureSlot::base_color;
        GraphResource texture{};
        GraphResource sampler{};
    };

    struct GraphMaterial
    {
        MaterialDesc desc{};
        epochengine::small_vector<GraphMaterialTextureSlot> texture_slots{};
        MaterialHandle backend{};
    };

    struct GraphRenderTarget
    {
        RenderTargetDesc desc{};
        RenderTargetHandle backend{};
        bool owned_by_render_texture_asset{ false };
    };

    struct GraphMesh
    {
        MeshDesc desc{};
        GraphResource vertex_buffer{};
        GraphResource index_buffer{};
        GraphResource material{};
        MeshHandle backend{};
    };

    struct GraphModelMeshSlot
    {
        GraphResource mesh{};
        GraphResource material{};
        epochengine::string node_name{};
        float transform[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
    };

    struct GraphModel
    {
        ModelDesc desc{};
        epochengine::small_vector<GraphModelMeshSlot> mesh_slots{};
        ModelHandle backend{};
    };

    struct GraphModelDraw
    {
        GraphResource model{};
        ModelHandle backend{};
    };

    struct ResourceDecl { ResourceKind kind{}; epochengine::string name{}; u32 index = 0; };

    struct GraphRenderTextureAsset
    {
        epochengine::string name{};
        RenderTextureAssetDesc desc{};
        GraphResource color_texture{};
        GraphResource sampler{};
        GraphResource render_target{};
        RenderTextureAssetPlan plan{};
        RenderTextureAssetHandles backend{};
    };

    struct PassDecl
    {
        epochengine::string name{};
        epochengine::small_vector<GraphResource> reads{};
        epochengine::small_vector<GraphResource> writes{};
        GraphResource render_target_resource{};
        RenderTargetHandle render_target{};
        BindingSetHandle binding_set{};
        RenderPassDesc render_pass{};
        CommandResourceBindings bindings{};
        epochengine::small_vector<GraphModelDraw> draw_models{};
        epochengine::function_ref<void(ICommandContext&)> execute{};
    };

    class GraphBuilder
    {
    public:
        [[nodiscard]] GraphResource create_buffer(epochengine::string_view name, const BufferDesc& desc);
        [[nodiscard]] GraphResource create_texture(epochengine::string_view name, const TextureDesc& desc);
        [[nodiscard]] GraphResource create_sampler(epochengine::string_view name, const SamplerDesc& desc);
        [[nodiscard]] GraphResource create_material(epochengine::string_view name,
                                                    const MaterialDesc& desc,
                                                    epochengine::array_view<const GraphMaterialTextureSlot> texture_slots = {});
        [[nodiscard]] GraphResource create_render_target(epochengine::string_view name, const RenderTargetDesc& desc);
        [[nodiscard]] GraphResource create_mesh(epochengine::string_view name,
                                                const MeshDesc& desc,
                                                GraphResource vertex_buffer = {},
                                                GraphResource index_buffer = {},
                                                GraphResource material = {});
        [[nodiscard]] GraphResource create_model(epochengine::string_view name,
                                                 const ModelDesc& desc,
                                                 epochengine::array_view<const GraphModelMeshSlot> mesh_slots = {});
        [[nodiscard]] GraphRenderTextureAsset create_render_texture_asset(
            epochengine::string_view name,
            const RenderTextureAssetDesc& desc);
        [[nodiscard]] GraphPass add_pass(epochengine::string_view name,
                                         epochengine::array_view<const GraphResource> reads,
                                         epochengine::array_view<const GraphResource> writes,
                                         epochengine::function_ref<void(ICommandContext&)> fn);
        [[nodiscard]] GraphPass add_render_pass(epochengine::string_view name,
                                                GraphResource target,
                                                epochengine::array_view<const GraphResource> reads,
                                                epochengine::array_view<const GraphResource> writes,
                                                const RenderPassDesc& pass,
                                                epochengine::function_ref<void(ICommandContext&)> fn);
        void add_model_draw(GraphPass pass, GraphResource model);

        struct CompiledGraph compile(IRenderDevice& dev) const;

    private:
        epochengine::small_vector<ResourceDecl> m_resources{};
        epochengine::small_vector<GraphBuffer>  m_buffers{};
        epochengine::small_vector<GraphTexture> m_textures{};
        epochengine::small_vector<GraphSampler> m_samplers{};
        epochengine::small_vector<GraphMaterial> m_materials{};
        epochengine::small_vector<GraphRenderTarget> m_render_targets{};
        epochengine::small_vector<GraphMesh> m_meshes{};
        epochengine::small_vector<GraphModel> m_models{};
        epochengine::small_vector<GraphRenderTextureAsset> m_render_texture_assets{};
        epochengine::small_vector<PassDecl>     m_passes{};
    };

    struct CompiledGraph
    {
        epochengine::small_vector<ResourceDecl> resources{};
        epochengine::small_vector<GraphBuffer>  buffers{};
        epochengine::small_vector<GraphTexture> textures{};
        epochengine::small_vector<GraphSampler> samplers{};
        epochengine::small_vector<GraphMaterial> materials{};
        epochengine::small_vector<GraphRenderTarget> render_targets{};
        epochengine::small_vector<GraphMesh> meshes{};
        epochengine::small_vector<GraphModel> models{};
        epochengine::small_vector<GraphRenderTextureAsset> render_texture_assets{};
        epochengine::small_vector<PassDecl>     passes{};

        void execute(IRenderDevice& dev);
        void destroy(IRenderDevice& dev) noexcept;
    };
} // namespace epoch
