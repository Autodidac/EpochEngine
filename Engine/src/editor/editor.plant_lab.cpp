// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include "core.format_text.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

module editor.application;

import forest.factory;

namespace epochengine
{
    const EditorApplicationProfile& plant_lab_editor_application() noexcept
    {
        static constexpr EditorApplicationProfile profile{
            .kind = EditorApplicationKind::PlantLab,
            .id = "epoch.plant_lab",
            .display_name = "Plant Lab",
            .project_id = "plantlab",
            .scene_id = "editor:plant_lab",
            .scene_source_path = "Engine/src/editor/editor.plant_lab.cpp",
            .purpose = "Procedural plant design, temporal growth authoring, preview, and Forest Factory asset output.",
            .default_surface = EditorApplicationSurface::PlantLab,
            .camera_policy = EditorCameraPolicy::Perspective,
            .initial_dock = EditorApplicationDock::Assets,
            .surface_mask = editor_surface_mask(
                EditorApplicationSurface::PlantLab,
                EditorApplicationSurface::Assets,
                EditorApplicationSurface::Project,
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

        auto profile = forest::default_profile(forest::ForestPreset::Tree);
        profile.config.targetHeightMeters = 4.2f;
        profile.config.trunkRadiusMeters = 0.09f;
        profile.branch.levels = 4u;
        profile.branch.branchLengthMeters = 0.92f;
        profile.branch.startHeightMeters = 0.24f;
        const auto geometry = forest::build_preview_geometry(profile);

        constexpr float preview_scale = 0.58f;
        const auto scaled_position = [](const auto value) noexcept
        {
            return std::array<float, 3>{
                value.x * preview_scale,
                value.y * preview_scale,
                value.z * preview_scale
            };
        };

        if (geometry.segmentCount > 0u)
        {
            const auto& trunk = geometry.segments[0];
            const float height = (std::max)(0.32f, (trunk.end.y - trunk.start.y) * preview_scale);
            const float width = (std::max)(0.12f, trunk.radius * 2.8f * preview_scale);
            scene.entities.push_back({
                "PlantLabTrunk",
                "ForestTrunk",
                "PlantLabPreview",
                {
                    (trunk.start.x + trunk.end.x) * 0.5f * preview_scale,
                    (trunk.start.y + trunk.end.y) * 0.5f * preview_scale,
                    (trunk.start.z + trunk.end.z) * 0.5f * preview_scale
                },
                {},
                { width, height, width },
                true,
                true
            });
        }

        const std::size_t branch_budget = (std::min)(
            geometry.segmentCount > 0u ? geometry.segmentCount - 1u : 0u,
            std::size_t{ 32u });
        const std::size_t branch_step = branch_budget > 0u
            ? (std::max)(std::size_t{ 1u }, (geometry.segmentCount - 1u) / branch_budget)
            : 1u;
        std::size_t branch_ordinal = 0u;
        for (std::size_t i = 1u;
            i < geometry.segmentCount && branch_ordinal < branch_budget;
            i += branch_step, ++branch_ordinal)
        {
            const auto& segment = geometry.segments[i];
            const float size = (std::clamp)(segment.radius * 2.1f * preview_scale, 0.055f, 0.16f);
            scene.entities.push_back({
                epochengine::format_text("PlantLabBranch_{:02}", branch_ordinal),
                "ForestBranchJoint",
                "PlantLabPreview",
                scaled_position(segment.end),
                {},
                { size, size, size },
                true,
                true
            });
        }

        const std::size_t canopy_budget = (std::min)(geometry.leafCount, std::size_t{ 28u });
        const std::size_t leaf_step = canopy_budget > 0u
            ? (std::max)(std::size_t{ 1u }, geometry.leafCount / canopy_budget)
            : 1u;
        std::size_t canopy_ordinal = 0u;
        for (std::size_t i = 0u;
            i < geometry.leafCount && canopy_ordinal < canopy_budget;
            i += leaf_step, ++canopy_ordinal)
        {
            const auto& leaf = geometry.leaves[i];
            const float size = (std::clamp)(leaf.size * 1.15f * preview_scale, 0.10f, 0.24f);
            scene.entities.push_back({
                epochengine::format_text("PlantLabCanopy_{:02}", canopy_ordinal),
                "ForestFoliageCluster",
                "PlantLabPreview",
                scaled_position(leaf.position),
                {},
                { size, size * 0.62f, size },
                true,
                true
            });
        }
        return scene;
    }
}
