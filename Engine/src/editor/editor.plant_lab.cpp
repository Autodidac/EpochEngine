// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include "core.format_text.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

module editor.application;

import forest.factory;

namespace epochengine
{
    namespace
    {
        constexpr float kPlantLabPreviewScale = 0.58f;

        [[nodiscard]] std::array<float, 3> plant_lab_scaled_position(
            float x,
            float y,
            float z) noexcept
        {
            return {
                x * kPlantLabPreviewScale,
                y * kPlantLabPreviewScale,
                z * kPlantLabPreviewScale};
        }

        [[nodiscard]] std::array<float, 3> plant_lab_segment_rotation(
            float startX,
            float startY,
            float startZ,
            float endX,
            float endY,
            float endZ) noexcept
        {
            constexpr float radiansToDegrees = 57.295779513082320876f;
            const float dx = endX - startX;
            const float dy = endY - startY;
            const float dz = endZ - startZ;
            const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (!(length > 0.000001f))
                return {};
            return {
                std::atan2(dz / length, dy / length) * radiansToDegrees,
                0.0f,
                std::asin((std::clamp)(-dx / length, -1.0f, 1.0f))
                    * radiansToDegrees};
        }
    }

    const EditorApplicationProfile& plant_lab_editor_application() noexcept
    {
        static constexpr EditorApplicationProfile profile{
            .kind = EditorApplicationKind::PlantLab,
            .id = "epoch.plant_lab",
            .display_name = "Plant Lab",
            .project_id = {},
            .scene_id = "editor:plant_lab",
            .scene_source_path = "Engine/src/editor/editor.plant_lab.cpp",
            .purpose = "Procedural plant design, temporal growth authoring, preview, and Forest Factory asset output.",
            .default_surface = EditorApplicationSurface::PlantLab,
            .camera_policy = EditorCameraPolicy::Perspective,
            .initial_dock = EditorApplicationDock::Assets,
            .surface_mask = editor_surface_mask(
                EditorApplicationSurface::PlantLab,
                EditorApplicationSurface::Assets,
                EditorApplicationSurface::Timeline),
            .panes = {
                .outliner = true,
                .inspector = true,
                .console = true,
                .ai_chat = false
            },
            .allow_project_run = false,
            .allow_entity_authoring = false,
            .allow_package_management = true,
            .allow_floating_panes = true
        };
        return profile;
    }

    EditorApplicationScene make_plant_lab_editor_scene()
    {
        EditorApplicationScene scene{
            .owner = EditorApplicationKind::PlantLab,
            .scene_id = "editor:plant_lab",
            .world_name = "PlantLabWorkspace",
            .entities = {
                { "PlantLabGround", "Ground", "World", { 0.0f, -0.25f, 0.0f }, {}, { 12.0f, 0.5f, 12.0f } },
                { "PlantLabCamera", "Camera", "Editor", { 5.8f, 4.4f, 7.2f }, { -25.0f, -142.0f, 0.0f } },
                { "PlantLabSun", "Light", "Lighting", { 2.0f, 6.5f, 1.5f }, { -48.0f, 32.0f, 0.0f } }
            }
        };

        auto document = forest::make_default_plant_lab_document();
        const auto compiledAsset = forest::compile_forest_asset(document);
        const auto& segments = compiledAsset.sample.segments;
        const auto& geometry = compiledAsset.preview;

        const std::size_t segment_budget = (std::min)(
            segments.size(), std::size_t{ 48u });
        const std::size_t segment_step = segment_budget > 0u
            ? (std::max)(std::size_t{ 1u }, segments.size() / segment_budget)
            : 1u;
        std::size_t segment_ordinal = 0u;
        for (std::size_t i = 0u;
            i < segments.size() && segment_ordinal < segment_budget;
            i += segment_step, ++segment_ordinal)
        {
            const auto& segment = segments[i];
            const float dx = segment.end.x - segment.start.x;
            const float dy = segment.end.y - segment.start.y;
            const float dz = segment.end.z - segment.start.z;
            const float length = (std::max)(
                0.04f,
                std::sqrt(dx * dx + dy * dy + dz * dz)
                    * kPlantLabPreviewScale);
            const float startDiameter = (std::clamp)(
                segment.radius_start_meters * 2.0f * kPlantLabPreviewScale,
                0.025f,
                0.24f);
            const float endDiameter = (std::clamp)(
                segment.radius_end_meters * 2.0f * kPlantLabPreviewScale,
                0.012f,
                startDiameter);
            scene.entities.push_back({
                i == 0u
                    ? std::string{"PlantLabTrunk"}
                    : epochengine::format_text("PlantLabBranch_{:02}", segment_ordinal),
                i == 0u ? "ForestTrunk" : "ForestBranchSegment",
                "PlantLabPreview",
                {
                    (segment.start.x + segment.end.x) * 0.5f * kPlantLabPreviewScale,
                    (segment.start.y + segment.end.y) * 0.5f * kPlantLabPreviewScale,
                    (segment.start.z + segment.end.z) * 0.5f * kPlantLabPreviewScale
                },
                plant_lab_segment_rotation(
                    segment.start.x,
                    segment.start.y,
                    segment.start.z,
                    segment.end.x,
                    segment.end.y,
                    segment.end.z),
                { startDiameter, length, endDiameter },
                true,
                true
            });
        }

        const std::size_t canopy_budget = (std::min)(
            geometry.leafCount,
            std::size_t{ 28u });
        const std::size_t leaf_step = canopy_budget > 0u
            ? (std::max)(std::size_t{ 1u }, geometry.leafCount / canopy_budget)
            : 1u;
        std::size_t canopy_ordinal = 0u;
        for (std::size_t i = 0u;
            i < geometry.leafCount && canopy_ordinal < canopy_budget;
            i += leaf_step, ++canopy_ordinal)
        {
            const auto& leaf = geometry.leaves[i];
            const float size = (std::clamp)(
                leaf.size * 1.15f * kPlantLabPreviewScale,
                0.10f,
                0.24f);
            scene.entities.push_back({
                epochengine::format_text("PlantLabCanopy_{:02}", canopy_ordinal),
                "ForestFoliageCluster",
                "PlantLabPreview",
                plant_lab_scaled_position(
                    leaf.position.x,
                    leaf.position.y,
                    leaf.position.z),
                {},
                { size, size * 0.62f, size },
                true,
                true
            });
        }
        return scene;
    }
}
