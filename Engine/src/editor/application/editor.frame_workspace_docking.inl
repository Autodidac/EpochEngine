// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        const auto activate_workspace_companions = [&](EditorMainSurface surface)
        {
            // Only the final explicit view choice in this frame owns activation.
            for (auto& pending : result.companion_pane_activations)
                pending.clear();
            std::string_view primary{};
            switch (surface)
            {
            case EditorMainSurface::Scene:
            case EditorMainSurface::ForestFactory:
            case EditorMainSurface::PlantLab:
                primary = "pane.world_outliner";
                break;
            case EditorMainSurface::Game2D:
                primary = editor.game2DGuiWorkspace
                    ? "pane.gui_hierarchy" : "pane.tile_map";
                break;
            case EditorMainSurface::Assets:
                primary = "pane.asset_browser";
                break;
            default: return;
            }
            const auto activate = [&](std::string_view route)
            {
                const auto pane = editor_tool_pane_from_route(route);
                if (!pane || !pane_supported(*pane)) return;
                set_pane_shown(route, true);
                // Keep detached panes and their existing dock destinations.
                // Selecting a workspace never resets the user's dock layout.
                if (!pane_route_is_detached(route))
                    set_active_pane(pane_dock_region(route), route);
                else
                {
                    for (auto& pending : result.companion_pane_activations)
                    {
                        if (pending.empty())
                        {
                            pending.assign(route);
                            break;
                        }
                    }
                }
            };
            activate("pane.properties");
            // If both companions share a custom group, the primary wins.
            activate(primary);
        };

        if (editor.workspaceCompanionsPending)
        {
            editor.workspaceCompanionsPending = false;
            activate_workspace_companions(editor.mainSurface);
        }

        auto apply_editor_surface = [&](EditorMainSurface surface, std::string_view source)
        {
            if (!editor_application_supports_surface(application, surface))
            {
                push_editor_log(
                    editor,
                    std::string("[editor] ") + std::string(application.display_name)
                        + " does not own the requested " + std::string(main_surface_title(surface)) + " surface.");
                return;
            }

            if (surface == EditorMainSurface::AISandbox
                && main_surface_is_ai_authoring(editor.mainSurface))
            {
                editor.aiAuthoringSurface = editor.mainSurface;
            }
            if (main_surface_is_ai_authoring(surface))
            {
                editor.aiAuthoringSurface = surface;
                editor.aiAuthoringSurfaceRestoreQueued = false;
            }

            const bool changedSurface = editor.mainSurface != surface;
            if (changedSurface && editor.mainSurface == EditorMainSurface::ForestFactory
                && editor.forestProjectCamera && ctx)
            {
                (void)previewgrid::restore_camera_rig_snapshot(ctx.get(), *editor.forestProjectCamera);
                editor.forestProjectCamera.reset();
            }
            if (changedSurface && surface == EditorMainSurface::ForestFactory && ctx)
                editor.forestProjectCamera = previewgrid::capture_camera_rig_snapshot(ctx.get());
            editor.mainSurface = surface;
            if (changedSurface)
                editor.surfaceSettleFrames = 1;

            switch (surface)
            {
            case EditorMainSurface::Scene:
                editor.workspaceTab = EditorWorkspaceTab::Output;
                editor.previewMode = core::ScenePreviewMode::Editor;
                editor.viewportCameraMode = previewgrid::CameraMode::Editor;
                if (ctx)
                {
                    const auto currentCameraMode = epochengine::previewgrid::camera_mode_for(ctx.get());
                    if (currentCameraMode == epochengine::previewgrid::CameraMode::Canvas2D)
                        epochengine::previewgrid::set_camera_mode(ctx.get(), editor.viewportCameraMode);
                }
                push_editor_log(editor, std::string("[editor] Scene workbench opened from ") + std::string(source) + ".");
                break;
            case EditorMainSurface::Game2D:
                editor.workspaceTab = EditorWorkspaceTab::Project;
                editor.game2DGuiWorkspace = editor.game2DGuiWorkspace
                    || editor_application_owns_dedicated_gui_workspace(editor.applicationKind);
                editor.outlinerToolTab = editor.game2DGuiWorkspace
                    ? OutlinerToolTab::Gui : OutlinerToolTab::TileMap;
                editor.previewMode = core::ScenePreviewMode::Editor;
                editor.viewportCameraMode = previewgrid::CameraMode::Canvas2D;
                if (ctx)
                {
                    epochengine::previewgrid::set_camera_mode(ctx.get(), epochengine::previewgrid::CameraMode::Canvas2D);
                    epochengine::previewgrid::reset_camera(ctx.get());
                }
                push_editor_log(editor, editor.game2DGuiWorkspace
                    ? "[editor] 2D / UI opened in GUI Overlay mode."
                    : "[editor] 2D / UI opened in Tile Map mode with the locked Canvas2D camera.");
                break;
            case EditorMainSurface::Assets:
                editor.workspaceTab = EditorWorkspaceTab::Assets;
                editor.outlinerToolTab = OutlinerToolTab::Assets;
                push_editor_log(editor, "[assets] Asset Manager workspace opened.");
                break;
            case EditorMainSurface::Project:
                editor.workspaceTab = EditorWorkspaceTab::Project;
                push_editor_log(editor, "[project] Project Workspace opened.");
                break;
            case EditorMainSurface::ForestFactory:
                editor.workspaceTab = EditorWorkspaceTab::Assets;
                editor.previewMode = core::ScenePreviewMode::Editor;
                editor.viewportCameraMode = previewgrid::CameraMode::Editor;
                if (ctx)
                {
                    epochengine::previewgrid::set_camera_mode(ctx.get(), epochengine::previewgrid::CameraMode::Editor);
                    focus_forest_factory_preview(ctx.get(), editor);
                }
                push_editor_log(
                    editor,
                    "[forest] Plant preview opened. Edit the plant, then Add to Scene; the project scene is unchanged.");
                break;
            case EditorMainSurface::PlantLab:
                editor.workspaceTab = EditorWorkspaceTab::Assets;
                editor.previewMode = core::ScenePreviewMode::Editor;
                editor.viewportCameraMode = previewgrid::CameraMode::Editor;
                ensure_plant_lab_preview_entities(editor);
                if (ctx)
                {
                    epochengine::previewgrid::set_camera_mode(ctx.get(), epochengine::previewgrid::CameraMode::Editor);
                    epochengine::previewgrid::reset_camera(ctx.get());
                }
                push_editor_log(editor, "[plant] Plant Lab opened with its procedural vegetation authoring preview.");
                break;
            case EditorMainSurface::Timeline:
                editor.workspaceTab = EditorWorkspaceTab::Output;
                epochengine::saveload::clamp_streaming_save_config(editor.streamingSaveConfig);
                push_editor_log(
                    editor,
                    "[timeline] Timeline opened; media transport remains disabled until the project admits a source.");
                break;
            case EditorMainSurface::AISandbox:
                editor.surfaceSettleFrames = 1;
                editor.workspaceTab = EditorWorkspaceTab::AI;
                editor.aiWorkspaceDomain = AiWorkspaceDomain::Control;
#if EPOCH_EDITION_PUBLIC
                push_editor_log(editor, "[assistant] Project Assistant workspace opened.");
#endif
                break;
            case EditorMainSurface::Systems:
                editor.workspaceTab = EditorWorkspaceTab::Systems;
                push_editor_log(editor, "[systems] System Info opened.");
                break;
            default:
                break;
            }
            // This route runs for a requested view transition, never as a
            // per-frame visibility rule. The user may close/select panes again.
            activate_workspace_companions(surface);
            // Companion activation leaves dock geometry and destinations intact.
        };

        if (editor.aiAuthoringSurfaceRestoreQueued)
        {
            editor.aiAuthoringSurfaceRestoreQueued = false;
            apply_editor_surface(
                editor.aiAuthoringSurface,
                "AI authoring request");
        }

        if (editor.requestedMainSurface)
        {
            const auto surface = *editor.requestedMainSurface;
            editor.requestedMainSurface.reset();
            apply_editor_surface(surface, "admitted Assistant request");
        }

        auto open_editor_surface = [&](EditorMainSurface surface, std::string_view source)
        {
            // An explicit view choice supersedes an older queued transition.
            editor.requestedMainSurface.reset();
            apply_editor_surface(surface, source);
        };

        auto request_pane_region = [&](
            std::string_view route,
            std::string_view label,
            EditorToolDockRegion region)
        {
            editor.openMenu = TopMenu::None;
            const EditorToolDockRegion previousRegion =
                pane_dock_region(route);
            if (pane_route_is_detached(route))
                restore_detached_pane_to_dock(route);
            set_pane_shown(route, true);
            if (previousRegion != region)
            {
                std::uint32_t appendOrder{};
                for (std::size_t index = 0u; index < kEditorToolPaneCount; ++index)
                {
                    if (editor.toolPaneDockRegions[index] == region)
                        appendOrder = (std::max)(appendOrder,
                            editor.toolPaneTabOrder[index] + 1u);
                }
                if (const auto pane = editor_tool_pane_from_route(route))
                    editor.toolPaneTabOrder[tool_pane_index(*pane)] = appendOrder;
            }
            set_pane_dock_region(route, region);
            set_active_pane(region, route);
            if (previousRegion != region)
            {
                const std::string_view fallback =
                    active_pane_for_region(previousRegion);
                active_pane_storage(previousRegion).assign(fallback);
            }
            const std::string_view regionLabel =
                region == EditorToolDockRegion::Left ? "left"
                : region == EditorToolDockRegion::Right ? "right"
                : region == EditorToolDockRegion::BottomLeft ? "bottom left"
                : "bottom right";
            editor.detachedPanelHostStatus =
                std::string(label) + " docked in the "
                + std::string(regionLabel) + " tool group.";
            push_editor_log(
                editor,
                std::string("[window] ") + std::string(label)
                + " docked in the " + std::string(regionLabel)
                + " tool group.");
        };

        auto request_pane_external_window = [&](std::string_view route, std::string_view label)
        {
            editor.openMenu = TopMenu::None;
            set_pane_shown(route, true);
            set_active_pane(pane_dock_region(route), route);
            editor.detachedPanelHostStatus =
                std::string(label)
                + " requested as an explicit routed native window.";
            emit_command(
                EditorCommand::OpenContextWindow,
                route,
                ctx ? ctx->type : core::ContextType::None);
            push_editor_log(
                editor,
                std::string("[window] ") + std::string(label)
                + " external window requested.");
        };

        auto render_floating_gui_window = [&]()
        {
            if (!editor.floatingGuiWindow.visible || modal_visible_now())
                return;

            const bool docked = editor.floatingGuiWindow.mode == gui::DockableWindowMode::docked;
            const gui::Vec2 defaultFloatingPosition{
                (std::max)(24.0f, w - 560.0f),
                (std::max)(118.0f, h * 0.22f)
            };
            const gui::Vec2 dockedPosition{
                (std::max)(24.0f, viewport_pos.x + viewport_size.x - 520.0f),
                (std::max)(toolbar_h + 18.0f, viewport_pos.y + 20.0f)
            };
            const gui::Vec2 dockedSize{
                (std::min)(500.0f, (std::max)(320.0f, viewport_size.x - 32.0f)),
                300.0f
            };

            const gui::FloatingWindowOptions floatingOptions{
                .default_position = {
                    defaultFloatingPosition.x,
                    defaultFloatingPosition.y
                },
                .default_size = { 500.0f, 300.0f },
                .min_size = { 320.0f, 196.0f },
                .viewport_size = { w, h },
                .movable = true,
                .resizable = true,
                .closable = true,
                .draw_background = true,
                .top_layer = true,
                .capture_input = true
            };
            const gui::DockableWindowOptions dockableOptions{
                .title = "Floating GUI",
                .docked_frame = { .position = dockedPosition, .size = dockedSize },
                .floating = floatingOptions,
                .viewport_size = { w, h },
                .title_bar_height = 30.0f,
                .content_padding = 6.0f,
                .action_button_width = 72.0f,
                .action_button_gap = 4.0f,
                .allow_dock = true,
                .allow_float = true,
                .allow_detach = false,
                .allow_close = true,
                .fallback_dock_slot = gui::DockSlot::right
            };

            result.scene_input_captured = true;
            (void)gui::update_dockable_window(
                editor.dockableGuiHost,
                editor.floatingGuiWindow,
                dockableOptions,
                {});

            auto apply_dockable_action = [&](gui::DockableWindowAction action)
            {
                const auto actionResult = gui::update_dockable_window(
                    editor.dockableGuiHost,
                    editor.floatingGuiWindow,
                    dockableOptions,
                    gui::DockableWindowInput{
                        .requested_action = action,
                        .requested_dock_slot = gui::DockSlot::right
                    });

                if (actionResult.dock_requested)
                {
                    editor.detachedPanelHostStatus = "Floating GUI docked into the editor-owned GUI host.";
                    push_editor_log(editor, "[window] Floating GUI docked.");
                }
                else if (actionResult.float_requested)
                {
                    editor.detachedPanelHostStatus = "Floating GUI undocked into a top-layer panel.";
                    push_editor_log(editor, "[window] Floating GUI floated.");
                }
                else if (actionResult.close_requested)
                {
                    editor.detachedPanelHostStatus = "Floating GUI closed.";
                    push_editor_log(editor, "[window] Floating GUI closed.");
                }
            };

            auto draw_panel_contents = [&](float contentWidth)
            {
                const bool currentlyDocked = editor.floatingGuiWindow.mode == gui::DockableWindowMode::docked;
                const std::string_view dockAction = currentlyDocked ? "Float" : "Dock";

                gui::label(currentlyDocked ? "Docked GUI" : "Floating GUI");
                gui::wrapped_label(
                    "This panel is backed by the reusable EpochGui dockable-window state model. Dock and Float now move through the same GUI library path; whole-editor backend selection stays in Settings.",
                    contentWidth);
                gui::property_row("Renderer", renderer_name(ctx), 112.0f);
                gui::property_row("Module", "epoch.gui", 112.0f);
                gui::property_row("Mode", currentlyDocked ? "Docked GUI panel" : "Floating GUI panel", 112.0f);
                gui::property_row("Context", "Editor Settings owns backend selection", 112.0f);

                const std::array actions{
                    gui::InlineButtonSpec{ .label = "System Info", .width = 112.0f },
                    gui::InlineButtonSpec{ .label = "Project", .width = 88.0f },
                    gui::InlineButtonSpec{ .label = dockAction, .width = 78.0f },
                    gui::InlineButtonSpec{ .label = "Close", .width = 72.0f }
                };
                if (const auto action = gui::inline_button_row(actions, 28.0f, 8.0f))
                {
                    switch (*action)
                    {
                    case 0:
                        open_editor_surface(EditorMainSurface::Systems, "dockable gui");
                        break;
                    case 1:
                        open_editor_surface(EditorMainSurface::Project, "dockable gui");
                        break;
                    case 2:
                        apply_dockable_action(currentlyDocked
                            ? gui::DockableWindowAction::float_window
                            : gui::DockableWindowAction::dock);
                        break;
                    case 3:
                        apply_dockable_action(gui::DockableWindowAction::close);
                        break;
                    default:
                        break;
                    }
                }
            };

            if (editor.floatingGuiWindow.mode == gui::DockableWindowMode::docked)
            {
                gui::begin_top_layer();
                gui::begin_window("Floating GUI Dock", dockedPosition, dockedSize, true);
                draw_panel_contents((std::max)(1.0f, dockedSize.x - 16.0f));
                gui::end_window();
                gui::end_top_layer();
                editor.detachedPanelHostStatus = "Floating GUI docked; context selection stays in the toolbar.";
            }
            else
            {
                const auto floating = gui::begin_floating_window(
                    editor.floatingGuiWindow.floating,
                    floatingOptions);
                if (!floating.begun)
                    return;

                draw_panel_contents((std::max)(1.0f, floating.content.size.x - 8.0f));
                gui::end_floating_window();

                if (editor.floatingGuiWindow.visible
                    && (!editor.floatingGuiWindow.floating.open || floating.close_requested))
                {
                    apply_dockable_action(gui::DockableWindowAction::close);
                }
                else if (floating.moved || floating.resized || floating.focused)
                {
                    editor.detachedPanelHostStatus = "Floating GUI active; context selection stays in the toolbar.";
                }
            }
        };


        // A menu opened in the preceding frame owns pointer events before
        // floating windows or other panes process the next click.
        if (editor.openMenu != TopMenu::None)
            gui::block_background_for_popup();
        render_floating_gui_window();

        // Capture the previously presented frame two ticks after the Tools
        // menu closes, so the clipboard image shows the engine, not its menu.
        // This is an own-window BitBlt only; no model/provider work runs here.
        if (editor.screenshotCaptureAfterFrames != 0u
            && --editor.screenshotCaptureAfterFrames == 0u)
        {
            const auto capture = editor_clipboard_screenshot::copy_window(
                reinterpret_cast<std::uintptr_t>(ctx->get_hwnd()));
            push_editor_log(editor, capture.ok
                ? "[screenshot] " + capture.message
                : "[screenshot] Capture failed: " + capture.message);
        }

