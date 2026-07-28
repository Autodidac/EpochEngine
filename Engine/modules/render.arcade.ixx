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
    struct ArcadeSceneNodeContract final
    {
        std::string_view name{};
        std::string_view type{};
        std::array<float, 3> position{};
        std::array<float, 3> scale{ 1.0f, 1.0f, 1.0f };
        bool sampled_render_surface{};
    };

    inline constexpr std::array<std::string_view, 11> kRuntimeSceneIds{
        "snake",
        "tetris",
        "pacman",
        "frogger",
        "sokoban",
        "match3",
        "sliding",
        "minesweeper",
        "2048",
        "sandsim",
        "cellular"
    };

    inline constexpr ArcadeSceneNodeContract kCabinetBodySceneNode{
        .name = "EngineArcadeCabinetBody",
        .type = "StaticMesh",
        .position = { 0.0f, 0.92f, 0.24f },
        .scale = { 1.38f, 1.30f, 0.54f },
        .sampled_render_surface = false
    };

    inline constexpr ArcadeSceneNodeContract kScreenSceneNode{
        .name = "EngineArcadeScreen",
        .type = "Canvas2D",
        .position = { 0.0f, 1.78f, -0.42f },
        .scale = { 2.22f, 1.22f, 0.06f },
        .sampled_render_surface = true
    };

    [[nodiscard]] constexpr bool is_runtime_scene_id(std::string_view scene_id) noexcept
    {
        for (const std::string_view candidate : kRuntimeSceneIds)
        {
            if (candidate == scene_id)
                return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool package_scene_catalog_matches() noexcept
    {
        std::string_view remaining = package_registry::engine_arcade_scene_ids();
        for (std::size_t index = 0; index < kRuntimeSceneIds.size(); ++index)
        {
            const std::size_t separator = remaining.find(',');
            const std::string_view scene =
                separator == std::string_view::npos ? remaining : remaining.substr(0u, separator);
            if (scene != kRuntimeSceneIds[index])
                return false;

            if (index + 1u == kRuntimeSceneIds.size())
                return separator == std::string_view::npos;
            if (separator == std::string_view::npos)
                return false;
            remaining.remove_prefix(separator + 1u);
        }
        return remaining.empty();
    }

    inline void apply_scene_transform(
        GraphModelMeshSlot& slot,
        const ArcadeSceneNodeContract& scene_node) noexcept
    {
        for (float& value : slot.transform)
            value = 0.0f;

        slot.transform[0] = scene_node.scale[0];
        slot.transform[5] = scene_node.scale[1];
        slot.transform[10] = scene_node.scale[2];
        slot.transform[12] = scene_node.position[0];
        slot.transform[13] = scene_node.position[1];
        slot.transform[14] = scene_node.position[2];
        slot.transform[15] = 1.0f;
    }
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
        const std::string_view defaultScene = package_registry::engine_arcade_default_scene_id();

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
        modelSlot.node_name = epochengine::string{ epochengine::to_view(defaultScene) };
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
        bodySlot.node_name = epochengine::string{ epochengine::to_view(kCabinetBodySceneNode.name) };
        apply_scene_transform(bodySlot, kCabinetBodySceneNode);

        GraphModelMeshSlot screenSlot{};
        screenSlot.mesh = build.mesh;
        screenSlot.material = build.material;
        screenSlot.node_name = epochengine::string{ epochengine::to_view(kScreenSceneNode.name) };
        apply_scene_transform(screenSlot, kScreenSceneNode);
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

    struct ArcadeContractChecks final
    {
        bool default_scene_registered{};
        bool package_catalog_exact{};
        bool sampled_render_texture{};
        bool required_render_surface_binding{};
        bool geometry_storage_consistent{};
        bool scene_integration_consistent{};

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return default_scene_registered
                && package_catalog_exact
                && sampled_render_texture
                && required_render_surface_binding
                && geometry_storage_consistent
                && scene_integration_consistent;
        }
    };

    [[nodiscard]] inline ArcadeContractChecks run_contract_checks()
    {
        const RenderTextureAssetDesc screen = make_screen_render_texture_desc();
        const MaterialDesc screenMaterial = make_cabinet_material_desc();
        const MeshDesc screenSceneMesh = make_screen_scene_mesh_desc();
        const MeshDesc cabinetScreenMesh = make_screen_mesh_desc();
        const MeshDesc cabinetBodyMesh = make_cabinet_body_mesh_desc();
        GraphModelMeshSlot bodySceneSlot{};
        GraphModelMeshSlot screenSceneSlot{};
        apply_scene_transform(bodySceneSlot, kCabinetBodySceneNode);
        apply_scene_transform(screenSceneSlot, kScreenSceneNode);

        const auto vertex_storage_matches = [](const BufferDesc& buffer, const MeshDesc& mesh) noexcept
        {
            return mesh.vertex_count > 0u
                && mesh.vertex_layout.stride_bytes > 0u
                && buffer.size_bytes
                    == static_cast<std::uint64_t>(mesh.vertex_count)
                        * static_cast<std::uint64_t>(mesh.vertex_layout.stride_bytes);
        };
        const auto index_storage_matches = [](const BufferDesc& buffer, const MeshDesc& mesh) noexcept
        {
            const std::uint64_t indexSize =
                mesh.index_format == IndexFormat::uint16 ? sizeof(std::uint16_t)
                : mesh.index_format == IndexFormat::uint32 ? sizeof(std::uint32_t)
                : 0u;
            return mesh.index_count > 0u
                && indexSize > 0u
                && buffer.size_bytes == static_cast<std::uint64_t>(mesh.index_count) * indexSize;
        };

        const bool requiredRenderSurface =
            screenMaterial.texture_slots.size() == 1u
            && screenMaterial.texture_slots.front().slot == MaterialTextureSlot::render_surface
            && screenMaterial.texture_slots.front().required
            && screenMaterial.texture_slots.front().expected_format == TextureFormat::rgba8_unorm;

        return ArcadeContractChecks{
            .default_scene_registered =
                is_runtime_scene_id(package_registry::engine_arcade_default_scene_id()),
            .package_catalog_exact = package_scene_catalog_matches(),
            .sampled_render_texture =
                screen.width == package_registry::engine_arcade_render_texture_width()
                && screen.height == package_registry::engine_arcade_render_texture_height()
                && screen.color_format == TextureFormat::rgba8_unorm
                && screen.sampled_after_render
                && screen.usage == RenderTextureUsage::arcade_cabinet,
            .required_render_surface_binding = requiredRenderSurface,
            .geometry_storage_consistent =
                vertex_storage_matches(make_screen_scene_vertex_buffer_desc(), screenSceneMesh)
                && index_storage_matches(make_screen_scene_index_buffer_desc(), screenSceneMesh)
                && vertex_storage_matches(make_screen_vertex_buffer_desc(), cabinetScreenMesh)
                && index_storage_matches(make_screen_index_buffer_desc(), cabinetScreenMesh)
                && vertex_storage_matches(make_cabinet_body_vertex_buffer_desc(), cabinetBodyMesh)
                && index_storage_matches(make_cabinet_body_index_buffer_desc(), cabinetBodyMesh),
            .scene_integration_consistent =
                kCabinetBodySceneNode.name.compare("EngineArcadeCabinetBody") == 0
                && kCabinetBodySceneNode.type.compare("StaticMesh") == 0
                && !kCabinetBodySceneNode.sampled_render_surface
                && kScreenSceneNode.name.compare("EngineArcadeScreen") == 0
                && kScreenSceneNode.type.compare("Canvas2D") == 0
                && kScreenSceneNode.sampled_render_surface
                && bodySceneSlot.transform[5] == kCabinetBodySceneNode.scale[1]
                && bodySceneSlot.transform[13] == kCabinetBodySceneNode.position[1]
                && screenSceneSlot.transform[0] == kScreenSceneNode.scale[0]
                && screenSceneSlot.transform[12] == kScreenSceneNode.position[0]
                && screenSceneSlot.transform[13] == kScreenSceneNode.position[1]
                && screenSceneSlot.transform[14] == kScreenSceneNode.position[2]
        };
    }

    static_assert(is_runtime_scene_id(package_registry::engine_arcade_default_scene_id()));
    static_assert(package_scene_catalog_matches());
}
