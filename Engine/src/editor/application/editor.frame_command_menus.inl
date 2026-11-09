// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        if (editor.openMenu != TopMenu::None && !modalVisible)
            gui::clear_modal_input_capture();

        enum class CommandMenuGroup : std::uint8_t { root, tools, workspaces };
        CommandMenuGroup commandMenuGroup{CommandMenuGroup::root};
        struct CommandMenuAction
        {
            std::string label{}, shortcut{};
            gui::MenuItemOptions options{};
            CommandMenuGroup group{};
            std::function<void()> invoke{};
        };
        std::vector<CommandMenuAction> commandMenuActions{};
        auto menu_item = [&](std::string_view title, auto&& on_click,
            gui::MenuItemOptions options = {})
        {
            if (options.command_id == 0u)
                options.command_id = (static_cast<std::uint64_t>(editor.openMenu) << 32u)
                    | (commandMenuActions.size() + 1u);
            // Store text before creating views: vector growth and dynamic
            // Undo/Return labels must not leave the popup with stale strings.
            commandMenuActions.push_back(CommandMenuAction{
                std::string{title}, std::string{options.shortcut}, options, commandMenuGroup,
                std::forward<decltype(on_click)>(on_click)});
        };
        auto open_dropdown = [&](std::string_view, TopMenu menu, auto&& body)
        {
            if (!modal_visible_now() && editor.openMenu == menu)
                body();
        };

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
            const bool standaloneGui=application.kind==EditorApplicationKind::GuiEditor && !has_open_project(editor);
            menu_item(standaloneGui ? "Save GUI..." : "Save Project", [&, standaloneGui]() {
                if(standaloneGui) (void)save_gui_document_from_dialog(editor);
                else repair_active_project_evidence(editor);
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
            const auto menuUndo = resolve_surface_edit(editor, SurfaceEditAction::undo);
            const auto menuRedo = resolve_surface_edit(editor, SurfaceEditAction::redo);
            const std::string menuUndoLabel = menuUndo.enabled ? "Undo " + menuUndo.operation_label : "Undo";
            const std::string menuRedoLabel = menuRedo.enabled ? "Redo " + menuRedo.operation_label : "Redo";
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
                menu_item("Import Mesh...", [&]() {
                    editor.openMenu = TopMenu::None;
                    editor.meshImportOpen = true;
                    editor.assetWorkspaceSection = 0u;
                    open_editor_surface(EditorMainSurface::Assets, "Import Mesh");
                }, {.enabled = !editor.projectRoot.empty(), .separator_before = true});
                menu_item("Reload Meshes", [&]() {
                    for (auto& entity : editor.entities)
                    {
                        entity.meshSnapshot.reset();
                        entity.meshSnapshotFrame.reset();
                        entity.meshLoadAttempted = false;
                    }
                    editor.sceneInteractionDirty = true;
                    editor.projectStatus = "Reloading the scene's mesh artifacts.";
                }, {.enabled = std::any_of(editor.entities.begin(), editor.entities.end(),
                    [](const EditorEntity& entity) { return entity.meshAsset.has_value(); })});
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
                commandMenuGroup=CommandMenuGroup::tools;
                // Assistant must be reachable from Window even if the menu is
                // clipped by the viewport or the original dock was closed.
                if (pane_supported(EditorToolPane::AiChat))
                {
                    constexpr std::string_view route = "pane.ai_chat";
                    const bool detached = pane_route_is_detached(route);
                    const bool shown = pane_is_shown(route);
                    menu_item(detached ? "Return Assistant Chat" : "Assistant Chat", [&, route, detached, shown]() {
                            if (detached) restore_detached_pane_to_dock(route);
                            set_pane_shown(route, detached || !shown);
                            if (detached || !shown)
                                set_active_pane(pane_dock_region(route), route);
                            push_editor_log(editor, detached || !shown
                                ? "[window] Assistant Chat restored with retained conversation."
                                : "[window] Assistant Chat hidden; conversation retained.");
                        }, {.indicator = gui::MenuItemIndicator::Check, .checked = shown && !detached,
                            .command_id = 0x10000u + static_cast<std::uint64_t>(EditorToolPane::AiChat)});
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
                        }, {.indicator = gui::MenuItemIndicator::Check, .checked = shown && !detached,
                            .command_id = 0x10000u + index});
                }

                commandMenuGroup=CommandMenuGroup::workspaces;
                for(std::size_t index=0;index<kEditorWorkspaceSpecs.size();++index)
                {
                    const auto& workspace=kEditorWorkspaceSpecs[index];
                    if(!editor_workspace_supported(editor,workspace)) continue;
                    const bool shown=editor_workspace_visible(editor,workspace);
                    menu_item(workspace.label,[&, workspace, shown]() {
                        if(shown) close_editor_workspace_tab(workspace);
                        else
                        {
                            if(workspace.gui_workspace) (void)select_editor_2d_workspace(editor,*workspace.gui_workspace);
                            open_editor_surface(workspace.surface,"Window menu");
                        }
                    }, {.indicator=gui::MenuItemIndicator::Check,.checked=shown,.separator_before=index==0u,
                        .command_id=0x30000u+index});
                }

                commandMenuGroup=CommandMenuGroup::root;
                menu_item(
                    "Reset Editor Layout", [&]() {
                        reset_editor_layout(editor);
                        push_editor_log(
                            editor,
                            "[window] Editor layout reset.");
                    }, {.separator_before = true, .command_id = 0x20000u});

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
            const auto show_task_status = [&]() {
                editor.openMenu = TopMenu::None;
                open_editor_surface(EditorMainSurface::Systems, "Tasks status");
            };
            // These are observed jobs, not thread capacity or the independent
            // Planet provider pool. Keep the counts readable without color.
            menu_item(epochengine::format_text("Active work: {}", toolbarRunning),
                show_task_status, {.tone = toolbarRunning > 0u
                    ? gui::SemanticTone::info : gui::SemanticTone::muted});
            menu_item(epochengine::format_text("Queued work: {}", toolbarQueued),
                show_task_status, {.tone = toolbarQueued > 0u
                    ? gui::SemanticTone::warning : gui::SemanticTone::muted});
            menu_item(epochengine::format_text("Idle Editor workers: {} / {}",
                toolbarActivity.idle_workers, toolbarActivity.worker_count),
                show_task_status, {.tone = gui::SemanticTone::muted});
            if (editor.taskScheduler)
            {
                const auto tasks = editor.taskScheduler->snapshot(0u);
                menu_item(epochengine::format_text("Completed Editor tasks: {}", tasks.completedCount),
                    show_task_status, {.tone = tasks.completedCount > 0u
                        ? gui::SemanticTone::success : gui::SemanticTone::muted});
                menu_item(epochengine::format_text("Failed Editor tasks: {}", tasks.failedCount),
                    show_task_status, {.tone = tasks.failedCount > 0u
                        ? gui::SemanticTone::error : gui::SemanticTone::muted});
                menu_item(epochengine::format_text("Cancelled Editor tasks: {}", tasks.cancelledCount),
                    show_task_status, {.tone = gui::SemanticTone::neutral});
            }
            menu_item("Open Tasks & Threads", [&]() {
                    editor.openMenu = TopMenu::None;
                    open_editor_surface(EditorMainSurface::AISandbox, "Tasks menu");
                }, {.separator_before = true});
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

        if (editor.openMenu == TopMenu::None || modal_visible_now())
        {
            gui::close_command_menu(editor.commandMenuState);
            editor.commandMenuOwner = TopMenu::None;
        }
        else
        {
            const bool menuOwnerChanged = editor.commandMenuOwner != editor.openMenu;
            if (menuOwnerChanged)
            {
                gui::close_command_menu(editor.commandMenuState);
                editor.commandMenuOwner = editor.openMenu;
            }
            std::vector<gui::MenuItemOptions> commandMenuItems{};
            std::array<std::vector<gui::MenuItemOptions>,2> commandMenuGroups{};
            commandMenuItems.reserve(commandMenuActions.size());
            for (const auto& command : commandMenuActions)
            {
                auto options = command.options;
                options.label = command.label;
                options.shortcut = command.shortcut;
                if(command.group==CommandMenuGroup::root) commandMenuItems.push_back(options);
                else commandMenuGroups[command.group==CommandMenuGroup::tools?0u:1u].push_back(options);
            }
            if(editor.openMenu==TopMenu::Window)
                commandMenuItems.insert(commandMenuItems.begin(),{
                    gui::MenuItemOptions{.label="Tool Windows",.command_id=0x40000u,.children=commandMenuGroups[0]},
                    gui::MenuItemOptions{.label="Workspace Windows",.command_id=0x40001u,.children=commandMenuGroups[1]}});
            const auto ownerBounds = top_menu_button_bounds(editor.openMenu);
            const auto menuResult = gui::command_menu(editor.commandMenuState,
                commandMenuItems, gui::CommandMenuOptions{
                    .owner = ownerBounds.value_or(gui::WidgetBounds{}),
                    .viewport_size = {w, h},
                    .maximum_height = ownerBounds
                        ? (std::max)(1.0f, h - ownerBounds->position.y
                            - ownerBounds->size.y - 10.0f) : 0.0f,
                    .open_requested = menuOwnerChanged,
                    .close_requested = !ownerBounds.has_value() || commandMenuItems.empty(),
                    .dismiss_on_hover_exit = true});
            result.scene_input_captured = result.scene_input_captured || menuResult.captured;
            const auto activated=std::find_if(commandMenuActions.begin(),commandMenuActions.end(),
                [&](const auto& action){return menuResult.activated_id==action.options.command_id;});
            if (menuResult.activated_id && activated!=commandMenuActions.end())
            {
                editor.openMenu = TopMenu::None;
                editor.commandMenuOwner = TopMenu::None;
                editor.suppressedMenuHover = hoveredMenu;
                gui::close_command_menu(editor.commandMenuState);
                activated->invoke();
            }
            else if (menuResult.owner_navigation != 0)
            {
                for (std::size_t index = 0; index < topMenus.size(); ++index)
                {
                    if (topMenus[index].menu != editor.openMenu) continue;
                    const auto next = menuResult.owner_navigation < 0
                        ? (index + topMenus.size() - 1u) % topMenus.size()
                        : (index + 1u) % topMenus.size();
                    editor.openMenu = topMenus[next].menu;
                    break;
                }
                editor.commandMenuOwner = TopMenu::None;
                editor.suppressedMenuHover = hoveredMenu;
                gui::close_command_menu(editor.commandMenuState);
            }
            else if (!menuResult.visible)
            {
                editor.openMenu = TopMenu::None;
                editor.commandMenuOwner = TopMenu::None;
                editor.suppressedMenuHover = hoveredMenu;
            }
        }

        #include "../editor.update_modal_view.inl"
