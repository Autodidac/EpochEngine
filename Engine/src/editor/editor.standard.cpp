// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch Editor: keep editor state and source changes within their owning workflows.
module;

module editor.application;

namespace epochengine
{
    const EditorApplicationProfile& standard_editor_application() noexcept
    {
        static constexpr EditorApplicationProfile profile{
            .kind = EditorApplicationKind::Standard,
            .id = "epoch.editor",
            .display_name = "Epoch Editor",
            .project_id = {},
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

        // Open projects restore their own scene; a fresh editor is not a demo.
        return scene;
    }
}
