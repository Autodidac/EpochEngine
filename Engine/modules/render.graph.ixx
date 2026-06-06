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
#include "../include/epoch.common.hpp"
#include "../include/_epoch.stl_types.hpp"

export module render.graph;

import render.device;

export namespace epoch
{
    struct GraphResourceTag {};
    struct GraphPassTag {};
    using GraphResource = Handle<GraphResourceTag, u32>;
    using GraphPass     = Handle<GraphPassTag, u32>;

    enum class ResourceKind : u8 { buffer, texture, material, render_target };

    struct GraphBuffer { BufferDesc desc{}; BufferHandle backend{}; };
    struct GraphTexture
    {
        TextureDesc desc{};
        TextureHandle backend{};
        SamplerHandle sampled_sampler{};
        bool owned_by_render_texture_asset{ false };
    };

    struct GraphMaterialTextureSlot
    {
        MaterialTextureSlot slot = MaterialTextureSlot::base_color;
        GraphResource texture{};
    };

    struct GraphMaterial
    {
        MaterialDesc desc{};
        epoch::small_vector<GraphMaterialTextureSlot> texture_slots{};
        MaterialHandle backend{};
    };

    struct GraphRenderTarget
    {
        RenderTargetDesc desc{};
        RenderTargetHandle backend{};
        bool owned_by_render_texture_asset{ false };
    };

    struct ResourceDecl { ResourceKind kind{}; epoch::string name{}; u32 index = 0; };

    struct GraphRenderTextureAsset
    {
        epoch::string name{};
        RenderTextureAssetDesc desc{};
        GraphResource color_texture{};
        GraphResource render_target{};
        RenderTextureAssetPlan plan{};
        RenderTextureAssetHandles backend{};
    };

    struct PassDecl
    {
        epoch::string name{};
        epoch::small_vector<GraphResource> reads{};
        epoch::small_vector<GraphResource> writes{};
        GraphResource render_target_resource{};
        RenderTargetHandle render_target{};
        BindingSetHandle binding_set{};
        RenderPassDesc render_pass{};
        CommandResourceBindings bindings{};
        epoch::function_ref<void(ICommandContext&)> execute{};
    };

    class GraphBuilder
    {
    public:
        [[nodiscard]] GraphResource create_buffer(epoch::string_view name, const BufferDesc& desc);
        [[nodiscard]] GraphResource create_texture(epoch::string_view name, const TextureDesc& desc);
        [[nodiscard]] GraphResource create_material(epoch::string_view name,
                                                    const MaterialDesc& desc,
                                                    epoch::array_view<const GraphMaterialTextureSlot> texture_slots = {});
        [[nodiscard]] GraphResource create_render_target(epoch::string_view name, const RenderTargetDesc& desc);
        [[nodiscard]] GraphRenderTextureAsset create_render_texture_asset(
            epoch::string_view name,
            const RenderTextureAssetDesc& desc);
        [[nodiscard]] GraphPass add_pass(epoch::string_view name,
                                         epoch::array_view<const GraphResource> reads,
                                         epoch::array_view<const GraphResource> writes,
                                         epoch::function_ref<void(ICommandContext&)> fn);
        [[nodiscard]] GraphPass add_render_pass(epoch::string_view name,
                                                GraphResource target,
                                                epoch::array_view<const GraphResource> reads,
                                                epoch::array_view<const GraphResource> writes,
                                                const RenderPassDesc& pass,
                                                epoch::function_ref<void(ICommandContext&)> fn);

        struct CompiledGraph compile(IRenderDevice& dev) const;

    private:
        epoch::small_vector<ResourceDecl> m_resources{};
        epoch::small_vector<GraphBuffer>  m_buffers{};
        epoch::small_vector<GraphTexture> m_textures{};
        epoch::small_vector<GraphMaterial> m_materials{};
        epoch::small_vector<GraphRenderTarget> m_render_targets{};
        epoch::small_vector<GraphRenderTextureAsset> m_render_texture_assets{};
        epoch::small_vector<PassDecl>     m_passes{};
    };

    struct CompiledGraph
    {
        epoch::small_vector<ResourceDecl> resources{};
        epoch::small_vector<GraphBuffer>  buffers{};
        epoch::small_vector<GraphTexture> textures{};
        epoch::small_vector<GraphMaterial> materials{};
        epoch::small_vector<GraphRenderTarget> render_targets{};
        epoch::small_vector<GraphRenderTextureAsset> render_texture_assets{};
        epoch::small_vector<PassDecl>     passes{};

        void execute(IRenderDevice& dev);
        void destroy(IRenderDevice& dev) noexcept;
    };
} // namespace epoch
