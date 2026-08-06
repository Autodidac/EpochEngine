// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

module editor.application;

namespace epochengine
{
    const EditorApplicationProfile& gui_editor_application() noexcept
    {
        static constexpr EditorApplicationProfile profile{
            .kind = EditorApplicationKind::GuiEditor,
            .id = "epoch.gui_editor",
            .display_name = "GUI Editor",
            .project_id = "twodstudio",
            .scene_id = "editor:gui",
            .scene_source_path = "Engine/src/editor/editor.gui.cpp",
            .purpose = "GUI document, Canvas2D, sprite, layout, and 2D interaction authoring.",
            .default_surface = EditorApplicationSurface::Game2D,
            .camera_policy = EditorCameraPolicy::Canvas2D,
            .initial_dock = EditorApplicationDock::Project,
            .surface_mask = editor_surface_mask(
                EditorApplicationSurface::Game2D,
                EditorApplicationSurface::Assets,
                EditorApplicationSurface::Project,
                EditorApplicationSurface::Timeline),
            .panes = {
                .outliner = true,
                .inspector = true,
                .console = true,
                .ai_chat = false
            },
            .allow_project_run = true,
            .allow_entity_authoring = false,
            .allow_package_management = true,
            .allow_floating_panes = true
        };
        return profile;
    }

    EditorApplicationScene make_gui_editor_scene()
    {
        return {
            .owner = EditorApplicationKind::GuiEditor,
            .scene_id = "editor:gui",
            .world_name = "GuiEditorWorkspace",
            .entities = {
                { "GuiDocument", "Level", "UI" },
                { "CanvasCamera", "Camera", "Editor", { 0.0f, 7.5f, 0.0f }, { -90.0f, 0.0f, 0.0f } },
                { "RootCanvas", "Canvas2D", "UI", {}, {}, { 16.0f, 9.0f, 1.0f } },
                { "SafeArea", "GuiSafeArea", "UI", {}, {}, { 14.4f, 8.1f, 1.0f }, true, true },
                { "PrimaryPanel", "GuiPanel", "UI", { 0.0f, 0.0f, 0.1f }, {}, { 6.0f, 3.5f, 1.0f } },
                { "PrimaryAction", "GuiButton", "UI", { 0.0f, -0.8f, 0.2f }, {}, { 2.4f, 0.7f, 1.0f } },
                { "InteractionOrigin", "Spawn", "UI", { -4.0f, -2.8f, 0.0f } }
            }
        };
    }
}
