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

#include <array>
#include <cstdint>
#include <string_view>

#include "../include/epoch.config.hpp"
#include "../src/epoch.common.hpp"
#include "../include/_epoch.stl_types.hpp"

export module render.arcade;

import package.registry;
import render.device;
import render.graph;

export namespace epochengine::render_arcade
{
    struct ArcadeScreenGraphBuild
    {
        GraphRenderTextureAsset screen{};
        GraphResource material{};
        GraphResource vertex_buffer{};
        GraphResource index_buffer{};
        GraphResource mesh{};
        GraphResource model{};
        GraphPass populate_pass{};
    };

    struct ArcadeCabinetGraphBuild
    {
        GraphRenderTextureAsset screen{};
        GraphPass populate_pass{};
        GraphResource screen_scene_material{};
        GraphResource screen_scene_vertex_buffer{};
        GraphResource screen_scene_index_buffer{};
        GraphResource screen_scene_mesh{};
        GraphResource screen_scene_model{};
        GraphResource material{};
        GraphResource vertex_buffer{};
        GraphResource index_buffer{};
        GraphResource mesh{};
        GraphResource body_material{};
        GraphResource body_vertex_buffer{};
        GraphResource body_index_buffer{};
        GraphResource body_mesh{};
        GraphResource model{};
        GraphPass cabinet_pass{};
    };

    [[nodiscard]] inline RenderTextureAssetDesc make_screen_render_texture_desc() noexcept
    {
        RenderTextureAssetDesc desc{};
        desc.width = package_registry::engine_arcade_render_texture_width();
        desc.height = package_registry::engine_arcade_render_texture_height();
        desc.color_format = TextureFormat::rgba8_unorm;
        desc.depth_format = TextureFormat::depth24_stencil8;
        desc.has_depth = true;
        desc.sampled_after_render = true;
        desc.usage = RenderTextureUsage::arcade_cabinet;
        desc.debug_name = "engine_arcade.screen";
        return desc;
    }

    [[nodiscard]] inline MaterialDesc make_cabinet_material_desc()
    {
        MaterialDesc desc{};
        desc.name = "engine_arcade.cabinet_material";
        desc.unlit = true;
        desc.debug_name = "engine_arcade.cabinet_material";
        desc.texture_slots.push_back(MaterialTextureSlotDesc{
            .slot = MaterialTextureSlot::render_surface,
            .name = "screen",
            .expected_format = TextureFormat::rgba8_unorm,
            .required = true
        });
        return desc;
    }

    [[nodiscard]] inline MaterialDesc make_cabinet_body_material_desc() noexcept
    {
        MaterialDesc desc{};
        desc.name = "engine_arcade.cabinet_body_material";
        desc.base_color[0] = 0.18f;
        desc.base_color[1] = 0.28f;
        desc.base_color[2] = 0.46f;
        desc.base_color[3] = 1.0f;
        desc.unlit = true;
        desc.debug_name = "engine_arcade.cabinet_body_material";
        return desc;
    }

    [[nodiscard]] inline BufferDesc make_screen_vertex_buffer_desc() noexcept
    {
        BufferDesc desc{};
        desc.size_bytes = 4u * 5u * sizeof(float);
        desc.gpu_only = true;
        desc.debug_name = "engine_arcade.cabinet.screen_vertices";
        return desc;
    }

    [[nodiscard]] inline MaterialDesc make_screen_scene_material_desc() noexcept
    {
        MaterialDesc desc{};
        desc.name = "engine_arcade.screen_scene.material";
        desc.base_color[0] = 0.10f;
        desc.base_color[1] = 0.85f;
        desc.base_color[2] = 0.55f;
        desc.base_color[3] = 1.0f;
        desc.unlit = true;
        desc.debug_name = "engine_arcade.screen_scene.material";
        return desc;
    }

    [[nodiscard]] inline BufferDesc make_screen_scene_vertex_buffer_desc() noexcept
    {
        BufferDesc desc{};
        desc.size_bytes = 12u * 3u * sizeof(float);
        desc.gpu_only = true;
        desc.debug_name = "engine_arcade.screen_scene.vertices";
        return desc;
    }

    [[nodiscard]] inline BufferDesc make_screen_scene_index_buffer_desc() noexcept
    {
        BufferDesc desc{};
        desc.size_bytes = 18u * sizeof(std::uint16_t);
        desc.gpu_only = true;
        desc.debug_name = "engine_arcade.screen_scene.indices";
        return desc;
    }

    [[nodiscard]] inline MeshDesc make_screen_scene_mesh_desc()
    {
        MeshDesc desc{};
        desc.vertex_count = 12;
        desc.index_count = 18;
        desc.index_format = IndexFormat::uint16;
        desc.topology = PrimitiveTopology::triangles;
        desc.debug_name = "engine_arcade.screen_scene.mesh";
        desc.vertex_layout.stride_bytes = 3u * sizeof(float);
        desc.vertex_layout.attributes.push_back(VertexAttributeDesc{
            .semantic = VertexSemantic::position,
            .location = 0,
            .offset_bytes = 0,
            .component_count = 3,
            .component_format = TextureFormat::rgba32_float,
            .normalized = false
        });
        return desc;
    }

    [[nodiscard]] inline ModelDesc make_screen_scene_model_desc() noexcept
    {
        ModelDesc desc{};
        desc.name = "engine_arcade.screen_scene";
        desc.static_mesh = true;
        desc.debug_name = "engine_arcade.screen_scene";
        return desc;
    }

    [[nodiscard]] inline BufferDesc make_screen_index_buffer_desc() noexcept
    {
        BufferDesc desc{};
        desc.size_bytes = 6u * sizeof(std::uint16_t);
        desc.gpu_only = true;
        desc.debug_name = "engine_arcade.cabinet.screen_indices";
        return desc;
    }

    [[nodiscard]] inline MeshDesc make_screen_mesh_desc()
    {
        MeshDesc desc{};
        desc.vertex_count = 4;
        desc.index_count = 6;
        desc.index_format = IndexFormat::uint16;
        desc.topology = PrimitiveTopology::triangles;
        desc.debug_name = "engine_arcade.cabinet.screen_mesh";
        desc.vertex_layout.stride_bytes = 5u * sizeof(float);
        desc.vertex_layout.attributes.push_back(VertexAttributeDesc{
            .semantic = VertexSemantic::position,
            .location = 0,
            .offset_bytes = 0,
            .component_count = 3,
            .component_format = TextureFormat::rgba32_float,
            .normalized = false
        });
        desc.vertex_layout.attributes.push_back(VertexAttributeDesc{
            .semantic = VertexSemantic::texcoord0,
            .location = 1,
            .offset_bytes = 3u * sizeof(float),
            .component_count = 2,
            .component_format = TextureFormat::rgba32_float,
            .normalized = false
        });
        return desc;
    }

    [[nodiscard]] inline BufferDesc make_cabinet_body_vertex_buffer_desc() noexcept
    {
        BufferDesc desc{};
        desc.size_bytes = 8u * 3u * sizeof(float);
        desc.gpu_only = true;
        desc.debug_name = "engine_arcade.cabinet.body_vertices";
        return desc;
    }

    [[nodiscard]] inline BufferDesc make_cabinet_body_index_buffer_desc() noexcept
    {
        BufferDesc desc{};
        desc.size_bytes = 36u * sizeof(std::uint16_t);
        desc.gpu_only = true;
        desc.debug_name = "engine_arcade.cabinet.body_indices";
        return desc;
    }

    [[nodiscard]] inline MeshDesc make_cabinet_body_mesh_desc()
    {
        MeshDesc desc{};
        desc.vertex_count = 8;
        desc.index_count = 36;
        desc.index_format = IndexFormat::uint16;
        desc.topology = PrimitiveTopology::triangles;
        desc.debug_name = "engine_arcade.cabinet.body_mesh";
        desc.vertex_layout.stride_bytes = 3u * sizeof(float);
        desc.vertex_layout.attributes.push_back(VertexAttributeDesc{
            .semantic = VertexSemantic::position,
            .location = 0,
            .offset_bytes = 0,
            .component_count = 3,
            .component_format = TextureFormat::rgba32_float,
            .normalized = false
        });
        return desc;
    }

    [[nodiscard]] inline ModelDesc make_cabinet_model_desc() noexcept
    {
        ModelDesc desc{};
        desc.name = "engine_arcade.cabinet";
        desc.static_mesh = true;
        desc.debug_name = "engine_arcade.cabinet";
        return desc;
    }

    [[nodiscard]] inline ArcadeScreenGraphBuild add_screen_graph(GraphBuilder& builder)
    {
        const std::string_view screenNameStd = package_registry::engine_arcade_render_texture_name();
        const epochengine::string_view screenName{ screenNameStd.data(), screenNameStd.size() };

        ArcadeScreenGraphBuild build{};
        build.screen = builder.create_render_texture_asset(
            screenName,
            make_screen_render_texture_desc());

        build.material = builder.create_material(
            "engine_arcade.screen_scene.material",
            make_screen_scene_material_desc());
        build.vertex_buffer = builder.create_buffer(
            "engine_arcade.screen_scene.vertices",
            make_screen_scene_vertex_buffer_desc());
        build.index_buffer = builder.create_buffer(
            "engine_arcade.screen_scene.indices",
            make_screen_scene_index_buffer_desc());
        build.mesh = builder.create_mesh(
            "engine_arcade.screen_scene.mesh",
            make_screen_scene_mesh_desc(),
            build.vertex_buffer,
            build.index_buffer,
            build.material);

        GraphModelMeshSlot modelSlot{};
        modelSlot.mesh = build.mesh;
        modelSlot.material = build.material;
        modelSlot.node_name = "arcade_screen_scene_attract_grid";
        const std::array<GraphModelMeshSlot, 1> modelSlots{ modelSlot };
        build.model = builder.create_model(
            "engine_arcade.screen_scene.model",
            make_screen_scene_model_desc(),
            epochengine::span<const GraphModelMeshSlot>{ modelSlots.data(), modelSlots.size() });

        const std::array<GraphResource, 2> reads{ build.material, build.model };
        build.populate_pass = builder.add_render_pass(
            "engine_arcade.screen.populate",
            build.screen.render_target,
            epochengine::span<const GraphResource>{ reads.data(), reads.size() },
            {},
            build.screen.plan.render_pass,
            [](ICommandContext& ctx)
            {
                ctx.debug_marker("engine_arcade.screen.sampled_surface");
            });
        builder.add_model_draw(build.populate_pass, build.model);

        return build;
    }

    [[nodiscard]] inline ArcadeCabinetGraphBuild add_cabinet_graph(GraphBuilder& builder)
    {
        const ArcadeScreenGraphBuild screen = add_screen_graph(builder);

        ArcadeCabinetGraphBuild build{};
        build.screen = screen.screen;
        build.populate_pass = screen.populate_pass;
        build.screen_scene_material = screen.material;
        build.screen_scene_vertex_buffer = screen.vertex_buffer;
        build.screen_scene_index_buffer = screen.index_buffer;
        build.screen_scene_mesh = screen.mesh;
        build.screen_scene_model = screen.model;

        const std::array<GraphMaterialTextureSlot, 1> materialSlots{ GraphMaterialTextureSlot{
            .slot = MaterialTextureSlot::render_surface,
            .texture = build.screen.color_texture,
            .sampler = build.screen.sampler
        } };
        build.material = builder.create_material(
            "engine_arcade.cabinet.material",
            make_cabinet_material_desc(),
            epochengine::span<const GraphMaterialTextureSlot>{ materialSlots.data(), materialSlots.size() });

        build.vertex_buffer = builder.create_buffer(
            "engine_arcade.cabinet.screen_vertices",
            make_screen_vertex_buffer_desc());
        build.index_buffer = builder.create_buffer(
            "engine_arcade.cabinet.screen_indices",
            make_screen_index_buffer_desc());

        build.mesh = builder.create_mesh(
            "engine_arcade.cabinet.screen_mesh",
            make_screen_mesh_desc(),
            build.vertex_buffer,
            build.index_buffer,
            build.material);

        build.body_material = builder.create_material(
            "engine_arcade.cabinet.body_material",
            make_cabinet_body_material_desc());
        build.body_vertex_buffer = builder.create_buffer(
            "engine_arcade.cabinet.body_vertices",
            make_cabinet_body_vertex_buffer_desc());
        build.body_index_buffer = builder.create_buffer(
            "engine_arcade.cabinet.body_indices",
            make_cabinet_body_index_buffer_desc());
        build.body_mesh = builder.create_mesh(
            "engine_arcade.cabinet.body_mesh",
            make_cabinet_body_mesh_desc(),
            build.body_vertex_buffer,
            build.body_index_buffer,
            build.body_material);

        GraphModelMeshSlot bodySlot{};
        bodySlot.mesh = build.body_mesh;
        bodySlot.material = build.body_material;
        bodySlot.node_name = "arcade_cabinet_body";

        GraphModelMeshSlot screenSlot{};
        screenSlot.mesh = build.mesh;
        screenSlot.material = build.material;
        screenSlot.node_name = "arcade_rtt_screen_panel";
        const std::array<GraphModelMeshSlot, 2> modelSlots{ bodySlot, screenSlot };

        build.model = builder.create_model(
            "engine_arcade.cabinet.model",
            make_cabinet_model_desc(),
            epochengine::span<const GraphModelMeshSlot>{ modelSlots.data(), modelSlots.size() });

        const std::array<GraphResource, 3> reads{ build.material, build.body_material, build.model };
        build.cabinet_pass = builder.add_pass(
            "engine_arcade.cabinet.sampled_surface",
            epochengine::span<const GraphResource>{ reads.data(), reads.size() },
            {},
            [](ICommandContext& ctx)
            {
                ctx.debug_marker("engine_arcade.cabinet.rtt_material");
            });
        builder.add_model_draw(build.cabinet_pass, build.model);

        return build;
    }
}
