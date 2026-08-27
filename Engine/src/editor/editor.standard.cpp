// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <string>
#include <utility>
#include <vector>

module editor.application;

import scene.tier0;

namespace epochengine
{
    const EditorApplicationProfile& standard_editor_application() noexcept
    {
        static constexpr EditorApplicationProfile profile{
            .kind = EditorApplicationKind::Standard,
            .id = "epoch.editor",
            .display_name = "Epoch Editor",
            .project_id = "projectlauncher",
            .scene_id = "editor:standard",
            .scene_source_path = "Engine/src/editor/editor.standard.cpp",
            .purpose = "General scene, project, asset, timeline, and system authoring with dockable AI tools.",
            .default_surface = EditorApplicationSurface::Scene,
            .camera_policy = EditorCameraPolicy::Perspective,
            .initial_dock = EditorApplicationDock::Output,
            .surface_mask = editor_surface_mask(
                EditorApplicationSurface::Scene,
                EditorApplicationSurface::Assets,
                EditorApplicationSurface::Project,
                EditorApplicationSurface::Game2D,
                EditorApplicationSurface::ForestFactory,
                EditorApplicationSurface::Timeline,
                EditorApplicationSurface::AISandbox,
                EditorApplicationSurface::Systems),
            .panes = {
                .outliner = true,
                .inspector = true,
                .console = true,
                .ai_chat = true
            },
            .allow_project_run = true,
            .allow_entity_authoring = true,
            .allow_package_management = true,
            .allow_floating_panes = true
        };
        return profile;
    }

    EditorApplicationScene make_standard_editor_scene()
    {
        EditorApplicationScene scene{
            .owner = EditorApplicationKind::Standard,
            .scene_id = "editor:standard",
            .world_name = "LauncherWorkspace"
        };

        const auto built = scene_tier0::make_default_scene();
        if (built)
        {
            scene.entities.reserve(built.scene.objects.size() + 1u);
            for (const auto& object : built.scene.objects)
            {
                EditorSceneSeedEntity entity{};
                entity.name = std::string{ object.canonicalName };
                entity.position = {
                    object.transform.position.x,
                    object.transform.position.y,
                    object.transform.position.z
                };
                entity.rotation = {
                    object.transform.rotationDegrees.x,
                    object.transform.rotationDegrees.y,
                    object.transform.rotationDegrees.z
                };
                entity.scale = {
                    object.transform.scale.x,
                    object.transform.scale.y,
                    object.transform.scale.z
                };

                switch (object.kind)
                {
                case scene_tier0::ObjectKind::camera:
                    entity.type = "Camera";
                    entity.category = "Editor";
                    break;
                case scene_tier0::ObjectKind::ground:
                    entity.type = "Ground";
                    entity.category = "World";
                    entity.position[1] -= 0.25f;
                    entity.scale = {
                        built.scene.groundTerrain.descriptor.sampleSpacing,
                        0.5f,
                        built.scene.groundTerrain.descriptor.sampleSpacing
                    };
                    break;
                case scene_tier0::ObjectKind::directional_light:
                    entity.type = "Light";
                    entity.category = "Lighting";
                    break;
                case scene_tier0::ObjectKind::spawn_point:
                    entity.type = "Spawn";
                    entity.category = "Gameplay";
                    break;
                }
                scene.entities.push_back(std::move(entity));
            }
        }
        else
        {
            scene.entities = {
                { "Ground", "Ground", "World", { 0.0f, -0.25f, 0.0f }, {}, { 16.0f, 0.5f, 16.0f } },
                { "PlayerSpawn", "Spawn", "Gameplay", { -2.5f, 0.05f, 2.5f } },
                { "PrimaryCamera", "Camera", "Editor", { 7.0f, 5.5f, 8.0f }, { -24.0f, -139.0f, 0.0f } },
                { "Sun", "Light", "Lighting", { 0.0f, 6.0f, 0.0f }, { -50.0f, -35.0f, 0.0f } }
            };
        }

        return scene;
    }
}
