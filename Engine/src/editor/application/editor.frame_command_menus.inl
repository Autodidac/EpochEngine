// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        if (editor.openMenu != TopMenu::None && !modalVisible)
            gui::clear_modal_input_capture();

        open_dropdown("File", TopMenu::File, [&]()
        {
            menu_item("Open Launcher", [&]() {
                emit_command(EditorCommand::OpenLauncher);
                push_editor_log(editor, "[file] Opening launcher.");
            });
            menu_item("Open Project...", [&]() {
                editor.openMenu = TopMenu::None;
                open_editor_surface(EditorMainSurface::Project, "File menu");
                (void)open_project_manifest_from_dialog(editor, ctx);
            });
            menu_item("Close Project", [&]() {
                (void)editor_close_project(ctx);
                editor.openMenu = TopMenu::None;
            });
            menu_item("Save Project", [&]() {
                repair_active_project_evidence(editor);
            });
            menu_item("Settings", [&]() {
                emit_command(EditorCommand::Settings);
                editor.openMenu = TopMenu::None;
                editor.showSettingsModal = true;
                push_editor_log(editor, "[file] Settings selected.");
            }, {.separator_before = true});
            menu_item("Exit", [&]() {
                emit_command(EditorCommand::Exit);
                push_editor_log(editor, "[file] Exit selected.");
            });
        });

        open_dropdown("Edit", TopMenu::Edit, [&]()
        {
            const auto remove = resolve_surface_edit(editor, SurfaceEditAction::delete_selection);
            const auto focus = resolve_surface_edit(editor, SurfaceEditAction::focus_selection);
            menu_item(menuUndoLabel, [&]() {
                (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::undo);
            }, {.shortcut = "Ctrl+Z", .enabled = menuUndo.enabled});
            menu_item(menuRedoLabel, [&]() {
                (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::redo);
            }, {.shortcut = "Ctrl+Y", .enabled = menuRedo.enabled});
            menu_item("Delete Selected", [&]() {
                (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::delete_selection);
            }, {.shortcut = "Del", .enabled = remove.enabled, .separator_before = true});
            menu_item("Deselect", [&]() {
                clear_editor_selection(editor);
                push_editor_log(editor, "[edit] Selection cleared.");
            }, {.shortcut = "Esc", .enabled = editor_has_selection(editor)});
            menu_item("Focus Selection", [&]() {
                (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::focus_selection);
            }, {.enabled = focus.enabled});
            menu_item("Reset Camera", [&]() {
                handle_scene_tool(editor, ctx.get(), "reset_camera");
                epochengine::previewgrid::reset_camera(ctx.get());
            }, {.separator_before = true});
            menu_item("Scene Helpers", [&]() {
                handle_scene_tool(editor, ctx.get(), "toggle_helpers");
            }, {.indicator = gui::MenuItemIndicator::Check, .checked = editor.helpersVisible});
        });

        open_dropdown("Asset", TopMenu::Asset, [&]()
        {
            menu_item("Open Asset Browser", [&]() {
                open_editor_surface(EditorMainSurface::Assets, "Asset menu");
            });
            if (application.allow_package_management)
            {
                menu_item("Package Manager...", [&]() {
                    editor.openMenu = TopMenu::None;
                    editor.showPackageManagerModal = true;
                    editor.workspaceTab = EditorWorkspaceTab::Assets;
                    push_editor_log(editor, "[assets] Package Manager opened.");
                });
            }
            if (application.allow_entity_authoring)
            {
                menu_item("Add Static Mesh", [&]() {
                    open_editor_surface(EditorMainSurface::Scene, "Add Static Mesh");
                    add_entity(editor, "cube");
                }, {.separator_before = true});
                menu_item("Add Light", [&]() {
                    open_editor_surface(EditorMainSurface::Scene, "Add Light");
                    add_entity(editor, "light");
                });
                menu_item("Add Spawn", [&]() {
                    open_editor_surface(EditorMainSurface::Scene, "Add Spawn");
                    add_entity(editor, "spawn");
                });
                menu_item("Duplicate Selected", [&]() {
                    (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::duplicate_selection);
                }, {.enabled = resolve_surface_edit(editor,
                    SurfaceEditAction::duplicate_selection).enabled, .separator_before = true});
                menu_item("Delete Selected", [&]() {
                    (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::delete_selection);
                }, {.shortcut = "Del", .enabled = resolve_surface_edit(editor,
                    SurfaceEditAction::delete_selection).enabled});
            }
        });

        open_dropdown("Window", TopMenu::Window, [&]()
            {
                // Assistant must be reachable from Window even if the menu is
                // clipped by the viewport or the original dock was closed.
                if (pane_supported(EditorToolPane::AiChat))
                {
                    constexpr std::string_view route = "pane.ai_chat";
                    const bool detached = pane_route_is_detached(route);
                    const bool shown = pane_is_shown(route);
                    menu_item(detached ? "Return Assistant" : "Assistant", [&, detached, shown]() {
                            if (detached) restore_detached_pane_to_dock(route);
                            set_pane_shown(route, detached || !shown);
                            if (detached || !shown)
                                set_active_pane(pane_dock_region(route), route);
                            push_editor_log(editor, detached || !shown
                                ? "[window] Assistant restored with retained conversation."
                                : "[window] Assistant hidden; conversation retained.");
                        }, {.indicator = gui::MenuItemIndicator::Check, .checked = shown && !detached});
                }
                for (std::size_t index = 0;
                    index < toolPaneRoutes.size(); ++index)
                {
                    const auto pane = static_cast<EditorToolPane>(index);
                    if (pane == EditorToolPane::AiChat
                        || !pane_supported(pane))
                        continue;

                    const std::string_view route = toolPaneRoutes[index];
                    const std::string_view label = toolPaneLabels[index];
                    const bool detached =
                        pane_route_is_detached(route);
                    const bool shown = pane_is_shown(route);
                    const std::string actionLabel =
                        detached
                            ? "Return " + std::string(label)
                            : std::string(label);
                    menu_item(
                        actionLabel, [&, route, label, detached, shown]() {
                            if (detached)
                            {
                                restore_detached_pane_to_dock(route);
                                set_pane_shown(route, true);
                                set_active_pane(pane_dock_region(route), route);
                                push_editor_log(
                                    editor,
                                    "[window] " + std::string(label)
                                    + " returning to its remembered dock.");
                                return;
                            }
                            set_pane_shown(route, !shown);
                            if (!shown)
                                set_active_pane(pane_dock_region(route), route);
                            push_editor_log(
                                editor,
                                "[window] " + std::string(label)
                                + (!shown ? " shown." : " hidden."));
                        }, {.indicator = gui::MenuItemIndicator::Check, .checked = shown && !detached});
                }

                menu_item(
                    "Reset Editor Layout", [&]() {
                        reset_editor_layout(editor);
                        push_editor_log(
                            editor,
                            "[window] Editor layout reset.");
                    }, {.separator_before = true});

            });
        const bool guiApplication = application.kind == EditorApplicationKind::GuiEditor;
        open_dropdown("Tools", TopMenu::Tools, [&]()
        {
            if (guiApplication)
            {
                menu_item("Canvas Camera", [&]() {
                    editor.viewportCameraMode = epochengine::previewgrid::CameraMode::Canvas2D;
                    open_editor_surface(EditorMainSurface::Game2D, "Tools menu");
                    push_editor_log(editor, "[tools] Camera mode set to locked GUI canvas.");
                }, {.indicator = gui::MenuItemIndicator::Radio,
                    .checked = editor.viewportCameraMode == epochengine::previewgrid::CameraMode::Canvas2D});
            }
            else
            {
                menu_item("Editor Camera", [&]() {
                    editor.viewportCameraMode = epochengine::previewgrid::CameraMode::Editor;
                    epochengine::previewgrid::set_camera_mode(ctx.get(), editor.viewportCameraMode);
                    push_editor_log(editor, "[tools] Camera mode set to Editor.");
                }, {.indicator = gui::MenuItemIndicator::Radio,
                    .checked = editor.viewportCameraMode == epochengine::previewgrid::CameraMode::Editor});
                menu_item("First-Person Camera", [&]() {
                    editor.viewportCameraMode = epochengine::previewgrid::CameraMode::FPS;
                    epochengine::previewgrid::set_camera_mode(ctx.get(), editor.viewportCameraMode);
                    push_editor_log(editor, "[tools] Camera mode set to FPS.");
                }, {.indicator = gui::MenuItemIndicator::Radio,
                    .checked = editor.viewportCameraMode == epochengine::previewgrid::CameraMode::FPS});
            }
            menu_item("Reset Preview Camera", [&]() {
                epochengine::previewgrid::reset_camera(ctx.get());
                push_editor_log(editor, "[tools] Preview camera reset.");
            });
            menu_item("Copy Window Screenshot", [&]() {
                    editor.openMenu = TopMenu::None;
                    editor.screenshotCaptureAfterFrames = 2u;
                }, {.shortcut = "F12", .separator_before = true});
            menu_item("Model Manager...", [&]() {
                    editor.openMenu = TopMenu::None;
                    editor.aiModelManagerTab = editor.aiMethodSelection == 1 ? 1
                        : editor.aiMethodSelection == 2 ? 2 : 0;
                    editor.showAiModelManagerModal = true;
                });
            menu_item("Export Logs & Diagnostics...", [&]() {
                    editor.openMenu = TopMenu::None;
                    request_manual_engine_diagnostics(editor, "Tools menu");
                });
        });

        open_dropdown("Tasks", TopMenu::Tasks, [&]()
        {
            menu_item("Open Tasks & Threads", [&]() {
                    editor.openMenu = TopMenu::None;
                    open_editor_surface(EditorMainSurface::AISandbox, "Tasks menu");
                });
            menu_item("Tasks Diagnostics", [&]() {
                    editor.openMenu = TopMenu::None;
                    editor.aiTasksDetailsVisible = true;
                    open_editor_surface(EditorMainSurface::AISandbox, "Tasks diagnostics");
                });
        });

        open_dropdown("Help", TopMenu::Help, [&]()
        {
            menu_item("About Epoch", [&]() {
                editor.openMenu = TopMenu::None;
                editor.showAboutModal = true;
            });
            menu_item("Current Project Info", [&]() {
                editor.openMenu = TopMenu::None;
                editor.projectWorkspaceSection = 0u;
                open_editor_surface(EditorMainSurface::Project, "Project details");
                push_editor_log(editor, std::string("[help] Active project: ") + editor.projectName);
            });
        });

        #include "../editor.update_modal_view.inl"
