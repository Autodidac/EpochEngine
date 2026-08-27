/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <memory>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

module editor.canvas2d_scene;

import visuals.engine;
import render.canvas2d_cpu;

namespace epochengine::editor_canvas2d
{
    namespace
    {
        [[nodiscard]] constexpr bool helper(const EntityView& entity) noexcept
        {
            return entity.type == "Light" || entity.type == "Camera"
                || entity.category == "Editor";
        }

        [[nodiscard]] constexpr bool structural(const EntityView& entity) noexcept
        {
            return entity.type == "Level" || entity.type == "Canvas2D";
        }

        [[nodiscard]] constexpr bool gui_widget(
            const EntityView& entity) noexcept
        {
            return entity.type.starts_with("Gui")
                || entity.category == "UI";
        }

        [[nodiscard]] constexpr visuals::Rgb color_for(
            const EntityView& entity) noexcept
        {
            if (entity.selected)
                return visuals::object_selected();
            if (entity.gui.authored)
            {
                const auto& color = entity.type == "GuiText"
                    ? entity.gui.foreground
                    : entity.gui.background;
                return {color[0], color[1], color[2]};
            }
            if (entity.type == "Light")
                return visuals::object_light();
            if (entity.type == "Spawn")
                return visuals::object_spawn();
            if (entity.type == "Camera")
                return visuals::object_camera();
            if (entity.type == "GuiPanel")
                return {0.16f, 0.22f, 0.29f};
            if (entity.type == "GuiButton")
                return {0.12f, 0.48f, 0.72f};
            if (entity.type == "GuiText")
                return {0.76f, 0.82f, 0.90f};
            if (entity.type == "GuiImage")
                return {0.52f, 0.36f, 0.68f};
            if (entity.type == "GuiImageButton")
                return {0.18f, 0.58f, 0.66f};
            if (entity.type == "GuiTabSet")
                return {0.46f, 0.40f, 0.72f};
            if (entity.type == "GuiTextInput")
                return {0.25f, 0.46f, 0.40f};
            if (entity.type == "GuiSlider")
                return {0.75f, 0.48f, 0.18f};
            if (entity.type == "GuiScrollArea")
                return {0.20f, 0.28f, 0.36f};
            if (entity.category == "World" || entity.type == "Ground")
                return visuals::object_world();
            if (entity.editor_only || entity.category == "Editor")
                return visuals::object_editor_helper();
            return visuals::object_default();
        }

        [[nodiscard]] constexpr canvas2d::Float2 size_for(
            const EntityView& entity) noexcept
        {
            if (entity.type == "Spawn")
                return {0.55f, 0.85f};
            if (entity.type == "Light")
                return {0.35f, 0.35f};
            if (entity.type == "Camera")
                return {0.65f, 0.42f};
            if (entity.type == "CollectibleSet")
                return {0.35f, 0.35f};
            return {
                (std::max)(0.20f, std::abs(entity.scale[0])),
                (std::max)(0.20f, std::abs(entity.scale[1]))};
        }

        [[nodiscard]] constexpr canvas2d::SpriteHandle sprite_handle(
            const EntityView& entity) noexcept
        {
            std::uint64_t mixed = entity.stable_id;
            mixed ^= mixed >> 33u;
            mixed *= 0xff51afd7ed558ccdull;
            mixed ^= mixed >> 33u;
            std::uint32_t index = static_cast<std::uint32_t>(mixed ^ (mixed >> 32u));
            if (index == canvas2d::invalid_index)
                --index;
            return {index, entity.generation};
        }

        [[nodiscard]] constexpr canvas2d::LinearColor linear_color(
            const EntityView& entity,
            visuals::Rgb color) noexcept
        {
            const float enabledOpacity =
                entity.gui.enabled ? 1.0f : 0.55f;
            const float alpha = entity.gui.authored
                ? (std::clamp)(
                    entity.gui.opacity
                        * entity.gui.background[3]
                        * enabledOpacity,
                    0.0f,
                    1.0f)
                : 1.0f;
            if (entity.material.logical_texture
                && (entity.type == "GuiImage"
                    || entity.type == "GuiImageButton"))
            {
                return {1.0f, 1.0f, 1.0f, alpha};
            }
            return {color.r, color.g, color.b, alpha};
        }

        [[nodiscard]] constexpr canvas2d::SpritePhase phase_for(
            const EntityView& entity) noexcept
        {
            if (entity.type == "Ground"
                || entity.type == "GuiPanel"
                || entity.type == "GuiScrollArea")
            {
                return canvas2d::SpritePhase::background;
            }
            if (helper(entity) || entity.type == "GuiText")
                return canvas2d::SpritePhase::overlay;
            return canvas2d::SpritePhase::world;
        }

        [[nodiscard]] constexpr std::int32_t order_for(
            const EntityView& entity) noexcept
        {
            if (entity.type == "Ground")
                return -1'000;
            if (entity.type == "GuiSafeArea")
                return -120;
            if (entity.type == "GuiPanel")
                return -100;
            if (entity.type == "GuiScrollArea")
                return -80;
            if (entity.type == "GuiImage")
                return -20;
            if (entity.type == "GuiTabSet")
                return 20;
            if (entity.type == "GuiImageButton")
                return 30;
            if (entity.type == "GuiText")
                return 100;
            return 0;
        }
    }

    BuildResult build_scene(const BuildRequest& request) noexcept
    {
        BuildResult result{};
        result.diagnostics.submitted_entities = request.entities.size();
        if (request.source_revision == 0
            || canvas2d::validate(request.project) != canvas2d::ResultCode::success)
        {
            return result;
        }
        if (request.entities.size() > request.project.maximum_sprites_per_batch
            * static_cast<std::uint64_t>(request.project.maximum_batches))
        {
            result.code = BuildCode::capacity_exceeded;
            return result;
        }

        canvas2d::scene_content::SceneContent content{};
        content.project = request.project;
        content.camera.pixels_per_world_unit = request.project.pixels_per_world_unit;
        content.camera.pixel_snap = request.project.pixel_snap;
        content.camera.y_axis = canvas2d::CanvasYAxis::up;
        content.source_revision = request.source_revision;
        content.resources = request.resources;
        const auto background = visuals::frame_background();
        const auto letterbox = visuals::scene_background();
        content.clear_color = {
            background[0], background[1], background[2], background[3]};
        content.letterbox_color = {
            letterbox[0], letterbox[1], letterbox[2], letterbox[3]};

        std::unordered_set<std::uint64_t> handles{};
        try
        {
            content.sprites.reserve(request.entities.size());
            handles.reserve(request.entities.size());
            for (const EntityView& entity : request.entities)
            {
                if (!entity.visible)
                {
                    ++result.diagnostics.hidden_entities;
                    continue;
                }
                if (entity.editor_only)
                {
                    ++result.diagnostics.editor_only_entities;
                    if (!request.include_helpers)
                        continue;
                }
                if (structural(entity))
                {
                    ++result.diagnostics.structural_entities;
                    continue;
                }
                if (helper(entity))
                {
                    ++result.diagnostics.helper_entities;
                    if (!request.include_helpers)
                        continue;
                }
                if (entity.gui.authored && gui_widget(entity))
                {
                    ++result.diagnostics.styled_gui_widgets;
                    if (!entity.gui.enabled)
                        ++result.diagnostics.disabled_gui_widgets;
                }
                if (entity.stable_id == 0 || entity.generation == 0)
                {
                    result.code = BuildCode::invalid_identity;
                    return result;
                }

                const canvas2d::SpriteHandle handle = sprite_handle(entity);
                const std::uint64_t packed =
                    (static_cast<std::uint64_t>(handle.index) << 32u)
                    | handle.generation;
                if (!handles.insert(packed).second)
                {
                    result.code = BuildCode::identity_collision;
                    return result;
                }

                canvas2d::SpriteMaterialDeclaration material{};
                material.stable_key = entity.material.stable_key;
                material.sampler = entity.material.sampler;
                material.alpha = entity.material.alpha;
                material.color_space = entity.material.color_space;
                material.alpha_cutoff = entity.material.alpha_cutoff;
                if (entity.material.logical_texture)
                {
                    material.source = canvas2d::SpriteSourceKind::texture;
                    material.logical_texture = entity.material.logical_texture;
                    ++result.diagnostics.textured_materials;
                }
                else
                {
                    material.source = canvas2d::SpriteSourceKind::solid_color;
                    ++result.diagnostics.solid_materials;
                }

                canvas2d::SpriteSubmission sprite{};
                sprite.sprite = handle;
                sprite.material = material;
                sprite.transform.position = {
                    entity.position[0], entity.position[1]};
                sprite.transform.size = size_for(entity);
                sprite.transform.rotation_radians = entity.rotation[2]
                    * std::numbers::pi_v<float> / 180.0f;
                sprite.tint = linear_color(entity, color_for(entity));
                sprite.phase = phase_for(entity);
                sprite.order = order_for(entity);
                sprite.stable_sequence = entity.stable_id;
                content.sprites.push_back(sprite);
            }
        }
        catch (...)
        {
            result.code = BuildCode::allocation_failure;
            return result;
        }

        const auto closure = canvas2d::scene_content::validate_resource_closure(
            content.sprites, content.resources);
        if (closure != canvas2d::scene_content::ResourceClosureCode::ready)
        {
            result.code = BuildCode::invalid_resources;
            return result;
        }
        if (!content.valid())
        {
            result.code = BuildCode::invalid_scene;
            return result;
        }
        result.diagnostics.emitted_sprites = content.sprites.size();
        result.code = BuildCode::ready;
        result.content = std::move(content);
        return result;
    }

    ContractFailure run_contract() noexcept
    {
        BuildRequest invalid{};
        invalid.source_revision = 0;
        if (build_scene(invalid).code != BuildCode::invalid_request)
            return ContractFailure::invalid_request;

        canvas2d::ProjectSettings project{};
        project.logical_canvas = {64, 36};
        project.pixels_per_world_unit = 4.0f;
        const canvas2d::LogicalTextureReference logical{17, 23};
        using ContractPixels = std::array<canvas2d::cpu::Rgba8, 4>;
        std::shared_ptr<const ContractPixels> pixels{};
        try
        {
            pixels = std::make_shared<const ContractPixels>(ContractPixels{{
                {255, 0, 0, 255},
                {0, 255, 0, 255},
                {0, 0, 255, 255},
                {255, 255, 255, 255}}});
        }
        catch (...)
        {
            return ContractFailure::textured_material;
        }
        const std::array bindings{canvas2d::cpu::TextureView{
            .logical = logical,
            .extent = {2, 2},
            .row_stride_pixels = 2,
            .pixels = *pixels,
            .color_space = canvas2d::SpriteColorSpace::linear,
            .alpha_encoding = canvas2d::cpu::AlphaEncoding::straight}};
        canvas2d::scene_content::ResourceLease resources{};
        resources.owner = canvas2d::scene_content::ResourceLifetime::retain(
            pixels);
        resources.bindings = {bindings, {}};

        const std::array<EntityView, 4> entities{{
            EntityView{
                .stable_id = 1,
                .generation = 1,
                .type = "Ground",
                .category = "World",
                .position = {0.0f, -2.0f, 0.0f},
                .scale = {12.0f, 1.0f, 1.0f}},
            EntityView{
                .stable_id = 2,
                .generation = 1,
                .type = "Spawn",
                .category = "Gameplay",
                .selected = true,
                .material = MaterialView{
                    .stable_key = 23,
                    .logical_texture = logical}},
            EntityView{
                .stable_id = 3,
                .generation = 1,
                .type = "Canvas2D",
                .category = "World"},
            EntityView{
                .stable_id = 4,
                .generation = 1,
                .type = "Camera",
                .category = "Editor",
                .visible = false}
        }};
        BuildResult built = build_scene(BuildRequest{
            .project = project,
            .entities = entities,
            .resources = resources,
            .source_revision = 7,
            .include_helpers = true});
        if (!built)
            return ContractFailure::build;
        if (built.content.sprites.size() != 2
            || built.diagnostics.structural_entities != 1
            || built.diagnostics.hidden_entities != 1
            || built.diagnostics.solid_materials != 1
            || built.diagnostics.textured_materials != 1)
        {
            return ContractFailure::filtering;
        }
        if (built.content.sprites[0].sprite == built.content.sprites[1].sprite)
            return ContractFailure::identity;
        const auto selected = visuals::object_selected();
        if (built.content.sprites[1].tint
            != canvas2d::LinearColor{selected.r, selected.g, selected.b, 1.0f})
        {
            return ContractFailure::selected_color;
        }
        if (built.content.sprites[1].material.source
                != canvas2d::SpriteSourceKind::texture
            || built.content.sprites[1].material.logical_texture != logical)
        {
            return ContractFailure::textured_material;
        }
        if (canvas2d::scene_content::validate_resource_closure(
                built.content.sprites, built.content.resources)
            != canvas2d::scene_content::ResourceClosureCode::ready)
        {
            return ContractFailure::resource_closure;
        }

        BuildRequest missingResources{
            .project = project,
            .entities = entities,
            .source_revision = 8,
            .include_helpers = true};
        if (build_scene(missingResources).code != BuildCode::invalid_resources)
            return ContractFailure::resource_closure;

        const int owner = 0;
        const auto published = canvas2d::scene_content::publish(
            &owner, std::move(built.content));
        const auto acquired = canvas2d::scene_content::acquire(&owner);
        const auto frame = canvas2d::scene_content::compile(acquired, {128, 72});
        const auto raster = canvas2d::cpu::rasterize(
            frame, acquired.content->resources.bindings);
        (void)canvas2d::scene_content::retire(&owner);
        if (!published || !acquired || !frame || !raster)
            return ContractFailure::cpu_frame;
        return ContractFailure::none;
    }
}
