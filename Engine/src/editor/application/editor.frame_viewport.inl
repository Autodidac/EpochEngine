// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell

        bool guiDocumentWorkspace =
            editor.mainSurface == EditorMainSurface::Game2D
            && (editor.game2DGuiWorkspace
                || editor_application_owns_dedicated_gui_workspace(
                    editor.applicationKind));
        bool active_center_uses_scene =
            main_surface_uses_scene(editor.mainSurface)
            && !guiDocumentWorkspace;
        if (editor.surfaceSettleFrames > 0)
            --editor.surfaceSettleFrames;
        // Document tabs provide the surface identity. A second generic
        // "Editor Workbench" header only consumes valuable vertical space.
        gui::begin_window(
            active_center_uses_scene ? std::string_view{} : std::string_view{"Editor Workbench"},
            viewport_pos,
            viewport_size,
            !active_center_uses_scene,
            false);
        if (editor.mainSurface == EditorMainSurface::Game2D
            && !editor_application_owns_dedicated_gui_workspace(
                editor.applicationKind))
        {
            const std::array modeTabs{
                gui::TabButtonSpec{
                    .id = "game2d.workspace.tilemap",
                    .label = "Tile Map",
                    .width = 0.0f,
                    .active = !guiDocumentWorkspace},
                gui::TabButtonSpec{
                    .id = "game2d.workspace.gui",
                    .label = "GUI Overlay",
                    .width = 0.0f,
                    .active = guiDocumentWorkspace}
            };
            if (const auto selected = gui::tab_bar_buttons(
                    modeTabs, 31.0f, 2.0f);
                selected.selected_index)
            {
                guiDocumentWorkspace = *selected.selected_index == 1u;
                active_center_uses_scene = !guiDocumentWorkspace;
                editor.outlinerToolTab = guiDocumentWorkspace
                    ? OutlinerToolTab::Gui
                    : OutlinerToolTab::TileMap;
                editor.game2DGuiWorkspace = guiDocumentWorkspace;
                activate_workspace_companions(EditorMainSurface::Game2D);
                push_editor_log(
                    editor,
                    guiDocumentWorkspace
                        ? "[editor] 2D / UI switched to GUI Overlay placement."
                        : "[editor] 2D / UI switched to Tile Map authoring.");
            }
            const gui::Vec2 instructionPos = gui::cursor_position();
            const std::string_view instruction = guiDocumentWorkspace
                    ? "Place and adjust project GUI elements here. Full widget "
                      "construction remains in the dedicated Epoch GUI Editor."
                    : "Paint tiles, manage layers, and attach project textures. "
                      "Undo and Redo stay on this tile-map document.";
            const float instructionWidth = (std::max)(1.0f,
                viewport_pos.x + viewport_size.x - instructionPos.x - 6.0f);
            gui::wrapped_label(instruction, instructionWidth);
            // Reserve the complete measured text block plus a gap before
            // drawing the scene title; a clipped draw is not layout height.
            gui::set_cursor({instructionPos.x, instructionPos.y
                + gui::measure_wrapped_label_height(instruction, instructionWidth) + 8.0f});
        }
        if (active_center_uses_scene)
        {
            if (editor.mainSurface == EditorMainSurface::ForestFactory)
            {
                const bool ready = ensure_forest_factory_preview(editor);
                gui::label("Plant Preview");
                gui::wrapped_label("Adjust the plant here. Add to Scene places a copy in your project.",
                    (std::max)(80.0f, viewport_size.x - 16.0f));
                const std::array<gui::InlineButtonSpec, 6> edits{{
                    {"Height -", 94.0f, ready}, {"Height +", 94.0f, ready},
                    {"Branches -", 106.0f, ready}, {"Branches +", 106.0f, ready},
                    {"Length -", 94.0f, ready}, {"Length +", 94.0f, ready}}};
                if (const auto clicked = gui::inline_button_row(edits, 28.0f, 6.0f))
                {
                    using Property = forest::ForestProfileProperty;
                    const auto& profile = editor.plantLabDocument.profile;
                    const auto property = *clicked < 2u ? Property::TargetHeightMeters
                        : *clicked < 4u ? Property::BranchLevels : Property::BranchLengthMeters;
                    const float value = *clicked < 2u ? profile.config.targetHeightMeters
                        : *clicked < 4u ? static_cast<float>(profile.branch.levels)
                        : profile.branch.branchLengthMeters;
                    const float step = *clicked < 2u ? 0.25f : *clicked < 4u ? 1.0f : 0.1f;
                    if (forest::apply_forest_profile_edit(editor.plantLabDocument,
                            property, value + (*clicked % 2u == 0u ? -step : step)))
                        (void)ensure_forest_factory_preview(editor);
                }
                const std::array<gui::InlineButtonSpec, 5> actions{{
                    {"Undo", 76.0f, ready}, {"Redo", 76.0f, ready},
                    {"Add to Scene", 132.0f, ready && has_open_project(editor)},
                    {"Back to Scene", 134.0f}, {"Advanced Plant Lab", 172.0f, ready}}};
                if (const auto clicked = gui::inline_button_row(actions, 28.0f, 6.0f))
                {
                    if (*clicked < 2u)
                    {
                        const bool changed = *clicked == 0u
                            ? forest::undo_forest_profile_edit(editor.plantLabDocument)
                            : forest::redo_forest_profile_edit(editor.plantLabDocument);
                        if (changed) (void)ensure_forest_factory_preview(editor);
                    }
                    else if (*clicked == 2u)
                    {
                        if (place_forest_factory_asset(editor) > 0u)
                        {
                            (void)save_editor_scene_snapshot(editor);
                            apply_editor_surface(EditorMainSurface::Scene, "Add Plant to Scene");
                            handle_scene_tool(editor, ctx.get(), "focus_selection");
                        }
                    }
                    else if (*clicked == 3u)
                        apply_editor_surface(EditorMainSurface::Scene, "Plant Preview");
                    else
                    {
                        result.command = EditorCommand::OpenContextWindow;
                        result.command_argument = "application.plant_lab";
                        result.requested_context_type = ctx->type;
                    }
                }
                gui::wrapped_label(editor.plantLabStatus, (std::max)(80.0f, viewport_size.x - 16.0f));
            }
            const std::string_view sceneTitle = main_surface_title(editor.mainSurface);
            const gui::Vec2 scene_pos = gui::cursor_position();
            const float sceneAvailableWidth = (std::max)(48.0f, viewport_pos.x + viewport_size.x - scene_pos.x);
            const float sceneAvailableHeight = (std::max)(48.0f, viewport_pos.y + viewport_size.y - scene_pos.y);
            // Keep the entire compact timeline above the lower dock splitter.
            // It needs space for the title, progress, scrubber, controls and
            // state. At short heights the dedicated Timeline tab remains usable.
            const float timelineStripHeadingHeight = gui::line_height() + 8.0f;
            const float timelineStripBodyOffset = timelineStripHeadingHeight + 6.0f;
            const float timelineStripRequiredHeight = timelineStripBodyOffset
                + timeline_compact_controls_height((std::max)(1.0f, sceneAvailableWidth - 16.0f)) + 6.0f;
            constexpr float sceneMinHeightWithTimeline = 100.0f;
            const bool showSceneTimeline = editor.mainSurface != EditorMainSurface::ForestFactory && sceneAvailableHeight >=
                timelineStripRequiredHeight + sceneMinHeightWithTimeline;
            const float timelineStripGap = 0.0f;
            const float timelineStripHeight = showSceneTimeline
                ? timelineStripRequiredHeight : 0.0f;
            const gui::Vec2 scene_size{
                sceneAvailableWidth,
                (std::max)(72.0f, sceneAvailableHeight - timelineStripHeight - timelineStripGap)
            };
            result.scene_viewport = gui::scene_viewport(sceneTitle, scene_pos, scene_size);
            // The scene Run button keeps the existing verified project launch
            // semantics. Fullscreen In Editor instead runs the saved scene in
            // the current window (no child process); Escape returns to editor.
            constexpr std::array<std::string_view, 2> kSceneRunModes{
                "Project Run", "Fullscreen In Editor (Esc)"};
            const float runButtonWidth = 62.0f;
            const float runModeWidth = (std::min)(172.0f,
                (std::max)(100.0f, scene_size.x * 0.20f));
            const bool showSceneRun = application.allow_project_run
                && has_open_project(editor) && editor.mainSurface != EditorMainSurface::ForestFactory;
            if (showSceneRun)
            {
                gui::set_cursor({scene_pos.x + 96.0f, scene_pos.y + 2.0f});
                if (gui::button("Run", {runButtonWidth, 20.0f})
                    && !menusBlockedByModal && !toolbarControlsBlockedByMenu)
                {
                    if (editor.sceneRunFullscreenInEditor
                        || editor.projectKind == "Engine Development")
                        play_active_context();
                    else
                        launch_active_project_context();
                }
                if (scene_size.x >= 440.0f)
                {
                    gui::set_cursor({scene_pos.x + 96.0f + runButtonWidth + 4.0f,
                        scene_pos.y + 2.0f});
                    const auto modeChoice = gui::select_box(gui::SelectBoxOptions{
                        .id = "scene-run-mode-select",
                        .placeholder = "Run mode",
                        .selected = kSceneRunModes[editor.sceneRunFullscreenInEditor ? 1u : 0u],
                        .options = kSceneRunModes,
                        .size = {runModeWidth, 20.0f},
                        .row_height = 26.0f,
                        .max_visible_options = 2u,
                        .enabled = !menusBlockedByModal});
                    if (modeChoice.changed && modeChoice.selected_index)
                        editor.sceneRunFullscreenInEditor = *modeChoice.selected_index == 1u;
                    result.scene_input_captured = result.scene_input_captured || modeChoice.opened;
                }
                else
                {
                    // At narrow widths the dropdown becomes a directly
                    // actionable mode toggle; Run itself stays visible.
                    gui::set_cursor({scene_pos.x + 96.0f + runButtonWidth + 4.0f,
                        scene_pos.y + 2.0f});
                    if (gui::button(editor.sceneRunFullscreenInEditor ? "Fullscreen" : "Project",
                        {runModeWidth, 20.0f}))
                        editor.sceneRunFullscreenInEditor = !editor.sceneRunFullscreenInEditor;
                }
            }
            constexpr std::array<std::string_view, 8> kEditorViewLabels{
                "Perspective",
                "Orthographic",
                "Front",
                "Back",
                "Left",
                "Right",
                "Top",
                "Bottom"
            };
            constexpr std::array<previewgrid::EditorView, 8> kEditorViews{
                previewgrid::EditorView::Perspective,
                previewgrid::EditorView::FreeOrthographic,
                previewgrid::EditorView::Front,
                previewgrid::EditorView::Back,
                previewgrid::EditorView::Left,
                previewgrid::EditorView::Right,
                previewgrid::EditorView::Top,
                previewgrid::EditorView::Bottom
            };
            const float viewSelectWidth = (std::min)(148.0f, (std::max)(112.0f, scene_size.x * 0.22f));
            const float focusButtonWidth = 58.0f;
            const bool showViewSelect = !showSceneRun
                || scene_size.x >= (96.0f + runButtonWidth + runModeWidth
                    + viewSelectWidth + focusButtonWidth + 32.0f);
            gui::set_cursor({
                scene_pos.x + scene_size.x - viewSelectWidth - focusButtonWidth - 14.0f,
                scene_pos.y + 2.0f
            });
            if (showViewSelect)
            {
            const auto editorViewSelect = gui::select_box(gui::SelectBoxOptions{
                .id = "scene-viewport-view-select",
                .placeholder = "Camera view",
                .selected = previewgrid::editor_view_name(previewgrid::editor_view_for(ctx.get())),
                .options = std::span<const std::string_view>{ kEditorViewLabels.data(), kEditorViewLabels.size() },
                .size = { viewSelectWidth, 20.0f },
                .row_height = 26.0f,
                .max_visible_options = kEditorViewLabels.size()
            });
            if (editorViewSelect.changed && editorViewSelect.selected_index
                && *editorViewSelect.selected_index < kEditorViews.size())
            {
                (void)previewgrid::set_editor_view(
                    ctx.get(),
                    kEditorViews[*editorViewSelect.selected_index]);
            }
            result.scene_input_captured = result.scene_input_captured || editorViewSelect.opened;
            }
            gui::set_cursor({ scene_pos.x + scene_size.x - focusButtonWidth - 8.0f, scene_pos.y + 2.0f });
            if (gui::button("Focus", { focusButtonWidth, 20.0f }))
            {
                if (editor.mainSurface == EditorMainSurface::ForestFactory)
                    focus_forest_factory_preview(ctx.get(), editor);
                else handle_scene_tool(editor, ctx.get(), "focus_selection");
            }
            ctx->set_scene_preview_mode(editor.previewMode);
            const int viewportX = (std::max)(0, static_cast<int>(std::floor(result.scene_viewport.position.x)));
            const int viewportY = (std::max)(0, static_cast<int>(std::floor(result.scene_viewport.position.y)));
            const int viewportRight = (std::max)(
                viewportX,
                static_cast<int>(std::ceil(result.scene_viewport.position.x + result.scene_viewport.size.x)));
            const int viewportBottom = (std::max)(
                viewportY,
                static_cast<int>(std::ceil(result.scene_viewport.position.y + result.scene_viewport.size.y)));
            ctx->set_scene_viewport(core::RenderViewport{
                viewportX,
                viewportY,
                (std::max)(0, viewportRight - viewportX),
                (std::max)(0, viewportBottom - viewportY)
            });
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
            if (editor.mainSurface == EditorMainSurface::Game2D)
            {
                if (auto* tileMap = tilemap_workspace_controller(editor))
                {
                    const auto tileMapFrame =
                        editor_tilemaps::render_canvas_workspace(
                            *tileMap,
                            editor_tilemaps::CanvasRenderOptions{
                                .viewport = result.scene_viewport,
                                .interactive = true,
                                .show_inspector = editor.showInspector});
                    result.scene_input_captured = result.scene_input_captured
                        || tileMapFrame.input_captured;
                    editor.tileMapStatus = std::string(tileMap->status());
                }
            }
#endif
            if (ctx->action_pressed_safe(input::Action::FrameSelection)
                && editor.mainSurface != EditorMainSurface::ForestFactory)
                handle_scene_tool(editor, ctx.get(), "focus_selection");
            if (editor.mainSurface != EditorMainSurface::ForestFactory)
            {
                update_scene_object_interaction(ctx, editor, result);
                draw_scene_marquee_overlay(editor);
                publish_editor_canvas2d_scene(ctx.get(), editor);
            }
            else (void)canvas2d::scene_content::retire(ctx.get());
            publish_editor_preview_markers(ctx.get(), editor);

            if (showSceneTimeline)
            {
                const gui::Vec2 timelinePos{ scene_pos.x, scene_pos.y + scene_size.y + timelineStripGap };
                gui::panel_rect(timelinePos, { scene_size.x, timelineStripHeight });
                gui::titlebar_rect(timelinePos, { scene_size.x, timelineStripHeadingHeight });
                gui::set_cursor({ timelinePos.x + 8.0f, timelinePos.y + 4.0f });
                gui::label("Timeline");
                gui::set_cursor({ timelinePos.x + 8.0f, timelinePos.y + timelineStripBodyOffset });
                render_timeline_time_controls((std::max)(1.0f, scene_size.x - 16.0f), true);
            }
        }
        else
        {
            const gui::Vec2 centerSurfacePos = gui::cursor_position();
            const gui::Vec2 centerSurfaceSize{
                (std::max)(48.0f, viewport_pos.x + viewport_size.x - centerSurfacePos.x - 6.0f),
                (std::max)(48.0f, viewport_pos.y + viewport_size.y - centerSurfacePos.y - 6.0f)
            };
            result.scene_viewport = gui::WidgetBounds{ .position = centerSurfacePos, .size = centerSurfaceSize };
            ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
            ctx->clear_scene_viewport();
            (void)canvas2d::scene_content::retire(ctx.get());

            gui::label(std::string(main_surface_title(editor.mainSurface)));
            if (editor.mainSurface == EditorMainSurface::Project
                && has_open_project(editor))
            {
                if (editor.projectWorkspaceSection > 4u)
                    editor.projectWorkspaceSection = 0u;
                const std::array projectWorkspaceTabs{
                    gui::TabButtonSpec{
                        .id = "project.workspace.defaults",
                        .label = "Project Defaults",
                        .width = 132.0f,
                        .active = editor.projectWorkspaceSection == 0u},
                    gui::TabButtonSpec{
                        .id = "project.workspace.input",
                        .label = "Input Manager",
                        .width = 122.0f,
                        .active = editor.projectWorkspaceSection == 1u},
                    gui::TabButtonSpec{
                        .id = "project.workspace.audio",
                        .label = "Audio",
                        .width = 72.0f,
                        .active = editor.projectWorkspaceSection == 2u},
                    gui::TabButtonSpec{
                        .id = "project.workspace.canvas2d",
                        .label = "Canvas2D",
                        .width = 92.0f,
                        .active = editor.projectWorkspaceSection == 3u},
                    gui::TabButtonSpec{
                        .id = "project.workspace.run",
                        .label = "Run + Build",
                        .width = 108.0f,
                        .active = editor.projectWorkspaceSection == 4u}
                };
                if (const auto tabs = gui::tab_bar_buttons(
                        projectWorkspaceTabs, 31.0f, 2.0f);
                    tabs.selected_index)
                {
                    editor.projectWorkspaceSection = *tabs.selected_index;
                }
            }
            const gui::Vec2 centerScrollStart = gui::cursor_position();
            const float centerWidth = (std::max)(1.0f, centerSurfaceSize.x - 12.0f);
            const float centerScrollHeight = (std::max)(
                1.0f,
                viewport_pos.y + viewport_size.y - centerScrollStart.y - 14.0f);
            std::string centerScrollId = std::string("center-surface-")
                + std::string(main_surface_title(editor.mainSurface));
            if (editor.mainSurface == EditorMainSurface::Project)
            {
                centerScrollId += "-"
                    + std::to_string(editor.projectWorkspaceSection);
            }
            const bool centerHasNestedSourceEditor =
#if EPOCH_EDITION_PUBLIC
                false
#endif
                || (editor.mainSurface == EditorMainSurface::Assets
                    && editor.assetWorkspaceSection == 2u);
            (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                .id = centerScrollId,
                .size = { (std::max)(1.0f, centerSurfaceSize.x), centerScrollHeight },
                .draw_background = false,
                .show_scrollbar = !centerHasNestedSourceEditor,
                .capture_wheel = !centerHasNestedSourceEditor
            });

            switch (editor.mainSurface)
            {
            case EditorMainSurface::Project:
            {
                gui::label("Project Session");
                const bool projectOpen = has_open_project(editor);
                if (gui::button(editor.projectCreationOpen ? "Cancel New Project" : "New Project...",
                        {(std::min)(centerWidth, 206.0f), 30.0f}))
                    editor.projectCreationOpen = !editor.projectCreationOpen;
                if (editor.projectCreationOpen)
                {
                    constexpr std::string_view kinds[]{"Game", "Software / Tool", "Epoch Editor"};
                    constexpr EditorProjectKind projectKinds[]{
                        EditorProjectKind::Game, EditorProjectKind::Tool, EditorProjectKind::Editor};
                    const std::size_t selectedKind = (std::min)(
                        editor.projectCreationKind, std::size(kinds) - 1u);
                    const auto kind = gui::select_box(gui::SelectBoxOptions{
                        .id = "new-project-kind", .placeholder = "Project type",
                        .selected = kinds[selectedKind],
                        .options = kinds, .size = {(std::min)(centerWidth, 340.0f), 30.0f}});
                    if (kind.changed && kind.selected_index)
                        editor.projectCreationKind = *kind.selected_index;
                    gui::wrapped_label("Creates a new project folder using the selected starter. Existing projects are never overwritten.", centerWidth);
                    if (gui::button("Create Project", {(std::min)(centerWidth, 206.0f), 30.0f}))
                    {
                        const auto created = editor_create_project_shell(
                            projectKinds[selectedKind],
                            EditorProjectInputProvision::project_default
                            );
                        editor.projectStatus = created.summary;
                        if (created.succeeded)
                        {
                            const auto admitted = editor_admit_project_manifest(created.manifest_path);
                            if (admitted.succeeded && editor_open_project(ctx, admitted.project_id))
                                editor.projectCreationOpen = false;
                            else
                                editor.projectStatus = "Project created at " + created.root_path
                                    + "; open it with Open Project File. " + admitted.summary;
                        }
                        push_editor_log(editor, "[project] " + editor.projectStatus);
                    }
                }

                const std::array projectSessionActions{
                    gui::InlineButtonSpec{
                        .label = projectOpen
                            ? "Open Another Project File..."
                            : "Open Project File...",
                        .width = 206.0f},
                    gui::InlineButtonSpec{
                        .label = "Close Project",
                        .width = 118.0f,
                        .enabled = projectOpen}
                };
                if (const auto action = gui::inline_button_row(
                        projectSessionActions, 30.0f, 6.0f))
                {
                    if (*action == 0u)
                        (void)open_project_manifest_from_dialog(editor, ctx);
                    else
                        (void)editor_close_project(ctx);
                }

                if (!has_open_project(editor))
                {
                    gui::label("No Project Open");
                    gui::wrapped_label(
                        "Choose New Project to create a starter, or open a project.epoch.json manifest. Opening loads that project authoring session without rewriting its source; Close Project releases the session without deleting files.",
                        centerWidth);
                    gui::wrapped_label(editor.projectStatus, centerWidth);
                    break;
                }

                const auto& projectProfile = active_project_profile(editor);
                if (editor.projectWorkspaceSection == 0u)
                {
                    gui::label("Project Defaults");
                const std::filesystem::path outputExe = project_output_exe_path(editor.projectRoot);
                const std::filesystem::path buildLog = project_build_log_path(editor.projectRoot);
                gui::label(std::string("Active Project: ") + editor.projectName);
                gui::property_row("[project] Kind", editor.projectKind);
                gui::property_row("[project] Root", display_project_path(editor.projectRoot), 108.0f);
                gui::property_row("[project] Manifest", file_ready_summary(editor.projectManifest), 108.0f);
                gui::property_row("[project] Scene", file_ready_summary(editor.projectScenePath), 108.0f);
                gui::property_row("[project] Output", file_ready_summary(outputExe.string()), 108.0f);
                gui::property_row("[project] Build log", file_ready_summary(buildLog.string()), 108.0f);
                gui::wrapped_label(
                    "This surface is the project launcher/control workspace. Save, build, and run happen here or from the centered Run button; logs remain mirrored below.",
                    centerWidth);
                const auto runChoices = project_run_backend_choices();
                std::vector<std::string_view> runChoiceLabels;
                runChoiceLabels.reserve(runChoices.size());
                for (const auto& choice : runChoices)
                    runChoiceLabels.emplace_back(choice.label);
                const auto runBackendSelect = gui::select_box(gui::SelectBoxOptions{
                    .id = "project-run-backend-select",
                    .placeholder = "Choose project runtime backend",
                    .selected = project_run_backend_label(editor.projectRunBackend),
                    .options = std::span<const std::string_view>{ runChoiceLabels.data(), runChoiceLabels.size() },
                    .size = { (std::min)(centerWidth, 360.0f), 30.0f },
                    .row_height = 28.0f,
                    .max_visible_options = 6
                });
                if (runBackendSelect.changed && runBackendSelect.selected_index && *runBackendSelect.selected_index < runChoices.size())
                {
                    editor.projectRunBackend = std::string(runChoices[*runBackendSelect.selected_index].argument);
                    push_editor_log(editor, std::string("[project] Project Run backend set to ") + std::string(runChoices[*runBackendSelect.selected_index].label) + ".");
                }
                gui::property_row("[project] Run mode", std::string(project_run_backend_label(editor.projectRunBackend)), 108.0f);
                gui::property_row("[project] Capability policy", projectProfile.renderer_capability.id, 148.0f);
                gui::property_row(
                    "[project] Run admission",
                    renderer_admission_summary(
                        renderer_backend_kind(editor.projectRunBackend),
                        projectProfile.renderer_capability),
                    148.0f);
                if (frame_limit_control("project-run-frame-limit-select", editor.projectRunFrameLimitFps,
                        editor.projectFrameLimitEdit, centerWidth))
                {
                    push_editor_log(editor, "[project] Project Run frame limit set to "
                        + frame_limit_argument(editor.projectRunFrameLimitFps) + " FPS.");
                }
                gui::property_row("[project] Frame limit", std::string(frame_limit_label(editor.projectRunFrameLimitFps)), 108.0f);
                const bool activeProjectIsEngineSandbox = editor.projectId == "sandbox";
                if (!activeProjectIsEngineSandbox)
                {
                    const auto cameraChoices = project_camera_choices();
                    std::vector<std::string_view> cameraChoiceLabels;
                    cameraChoiceLabels.reserve(cameraChoices.size());
                    for (const auto& choice : cameraChoices)
                        cameraChoiceLabels.emplace_back(choice.label);
                    const auto cameraSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-camera-style-select",
                        .placeholder = "Choose project camera style",
                        .selected = project_camera_label(editor.projectCameraMode),
                        .options = std::span<const std::string_view>{ cameraChoiceLabels.data(), cameraChoiceLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 3
                    });
                    if (cameraSelect.changed && cameraSelect.selected_index && *cameraSelect.selected_index < cameraChoices.size())
                    {
                        editor.projectCameraMode = cameraChoices[*cameraSelect.selected_index].mode;
                        if (editor.projectCameraMode == previewgrid::CameraMode::Canvas2D)
                        {
                            ensure_2d_canvas_entity(editor);
                            previewgrid::reset_camera(ctx.get());
                        }
                        push_editor_log(editor, std::string("[project] Project camera style set to ") + std::string(cameraChoices[*cameraSelect.selected_index].label) + ".");
                    }
                    gui::property_row("[project] Camera style", std::string(project_camera_label(editor.projectCameraMode)), 108.0f);
                }
                else
                {
                    gui::property_row("[project] Camera style", "Engine engine development mirrors the editor workbench.", 108.0f);
                }
                const auto inputChoices = input_profile_choices();
                std::vector<std::string_view> inputChoiceLabels;
                inputChoiceLabels.reserve(inputChoices.size());
                for (const auto& choice : inputChoices)
                    inputChoiceLabels.emplace_back(choice.label);
                const auto inputSelect = gui::select_box(gui::SelectBoxOptions{
                    .id = "project-input-profile-select",
                    .placeholder = "Choose shared input profile",
                    .selected = input_profile_label(editor.inputProfilePreset),
                    .options = std::span<const std::string_view>{ inputChoiceLabels.data(), inputChoiceLabels.size() },
                    .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                    .row_height = 28.0f,
                    .max_visible_options = 4
                });
                if (inputSelect.changed && inputSelect.selected_index && *inputSelect.selected_index < inputChoices.size())
                {
                    editor.inputProfilePreset = inputChoices[*inputSelect.selected_index].preset;
                    input::set_active_profile(editor.inputProfilePreset);
                    push_editor_log(editor, std::string("[input] Shared profile set to ") + std::string(input_profile_label(editor.inputProfilePreset)) + ".");
                }
                gui::property_row("[editor] Camera input", std::string(input_profile_label(editor.inputProfilePreset)), 108.0f);
                gui::label("New Project Self-Iteration");
                gui::wrapped_label(
                    "Creation default only. Existing projects are never changed. Enabled kits may propose project-source edits only, require explicit approval, and never auto-connect, listen, bind, serve, or launch a model.",
                    centerWidth);
                }

                if (editor.projectWorkspaceSection >= 1u
                    && editor.projectWorkspaceSection <= 3u)
                {
                    editor.canvas2dProject = sanitize_canvas2d_project(editor.canvas2dProject);
                }

                if (editor.projectWorkspaceSection == 1u)
                {
                    gui::label("Input Manager");
                    gui::wrapped_label(
                        "These bindings belong to the active project. Edits stay in a validated draft until Apply publishes the canonical source and compiled Library artifact.",
                        centerWidth);
                    const bool inputLoaded = load_project_input_settings(editor);
                    const auto projectInput = inputLoaded
                        ? editor.projectInputSettings->snapshot()
                        : editor_project_input_settings::SettingsSnapshot{};
                    gui::property_row(
                        "[input] Source",
                        inputLoaded
                            ? std::string{project_input::canonical_source_path}
                            : std::string{"Not loaded"},
                        132.0f);
                    gui::property_row(
                        "[input] Profile",
                        projectInput.display_name.empty()
                            ? std::string{"Not declared"}
                            : projectInput.display_name,
                        132.0f);
                    gui::property_row(
                        "[input] Actions",
                        epochengine::format_text(
                            "{} actions | {} bindings",
                            projectInput.action_count,
                            projectInput.binding_count),
                        132.0f);
                    gui::property_row(
                        "[input] Draft",
                        inputLoaded
                            ? epochengine::format_text(
                                "{} | applied r{} | working r{}",
                                projectInput.dirty ? "Modified" : "Clean",
                                projectInput.applied_revision.sequence,
                                projectInput.working_revision.sequence)
                            : std::string{"Unavailable"},
                        132.0f);
                    gui::property_row(
                        "[input] Validation",
                        editor.projectInputSettingsStatus,
                        132.0f);
                    if (!projectInput.evidence.conflicts.empty())
                    {
                        for (const auto& conflict : projectInput.evidence.conflicts)
                        {
                            gui::wrapped_label(
                                epochengine::format_text(
                                    "Conflict: binding {} and binding {} resolve to the same physical input.",
                                    conflict.binding.value,
                                    conflict.conflicting_binding.value),
                                centerWidth);
                        }
                    }
                    if (inputLoaded)
                    {
                        const auto physicalControllerSnapshot =
                            controller_input::snapshot();
                        gui::property_row(
                            "[input] Keyboard",
                            "Canonical project bindings sampled live",
                            132.0f);
                        gui::property_row(
                            "[input] Controller",
                            physicalControllerSnapshot.provider_available
                                ? epochengine::format_text(
                                    "{} | {} connected | snapshot {}",
                                    controller_input::physical_provider_name(),
                                    controller_input::connected_count(),
                                    physicalControllerSnapshot.revision)
                                : epochengine::format_text(
                                    "{} unavailable",
                                    controller_input::physical_provider_name()),
                            132.0f);
                        gui::property_row(
                            "[input] Dead zone",
                            project_controller_dead_zone_evidence(
                                projectInput.controller_dead_zone_q15,
                                projectInput.uniform_controller_dead_zone),
                            132.0f);

                        gui::label("Project Controls");
                        const auto recordInputEdit =
                            [&](const editor_project_input_settings::SettingsEvidence& evidence)
                            {
                                editor.projectInputSettingsStatus =
                                    project_input_settings_evidence(evidence);
                                push_editor_log(
                                    editor,
                                    std::string{evidence
                                        ? "[input] Draft updated: "
                                        : "[input][ERROR] Draft rejected: "}
                                        + editor.projectInputSettingsStatus);
                            };

                        const auto keyChoices = project_key_choices();
                        std::vector<std::string_view> keyChoiceLabels{};
                        keyChoiceLabels.reserve(keyChoices.size());
                        for (const auto key : keyChoices)
                        {
                            keyChoiceLabels.emplace_back(
                                epochengine::project_input::key_code_name(key));
                        }

                        const auto buttonChoices =
                            project_controller_button_choices();
                        std::vector<std::string_view> buttonChoiceLabels{};
                        buttonChoiceLabels.reserve(buttonChoices.size());
                        for (const auto button : buttonChoices)
                        {
                            buttonChoiceLabels.emplace_back(
                                epochengine::project_input::
                                    controller_button_name(button));
                        }

                        const auto axisChoices =
                            project_controller_axis_choices();
                        std::vector<std::string_view> axisChoiceLabels{};
                        axisChoiceLabels.reserve(axisChoices.size());
                        for (const auto axis : axisChoices)
                        {
                            axisChoiceLabels.emplace_back(
                                epochengine::project_input::
                                    controller_axis_name(axis));
                        }
                        const auto slotLabels =
                            project_controller_slot_labels();

                        const bool hasControllerAxis = std::any_of(
                            projectInput.bindings.begin(),
                            projectInput.bindings.end(),
                            [](const editor_project_input_settings::BindingView& binding)
                            {
                                return binding.device
                                    == project_input::BindingDevice::controller_axis;
                            });
                        if (hasControllerAxis)
                        {
                            const auto deadZoneChoices =
                                project_controller_dead_zone_choices();
                            std::vector<std::string_view> deadZoneLabels{};
                            deadZoneLabels.reserve(deadZoneChoices.size());
                            for (const auto& choice : deadZoneChoices)
                                deadZoneLabels.emplace_back(choice.label);
                            const auto deadZoneSelect = gui::select_box(
                                gui::SelectBoxOptions{
                                    .id = "project-input-controller-dead-zone",
                                    .placeholder = "Controller dead zone",
                                    .selected =
                                        project_controller_dead_zone_label(
                                            projectInput
                                                .controller_dead_zone_q15,
                                            projectInput
                                                .uniform_controller_dead_zone),
                                    .options =
                                        std::span<const std::string_view>{
                                            deadZoneLabels.data(),
                                            deadZoneLabels.size()},
                                    .size = {
                                        (std::min)(centerWidth, 320.0f),
                                        30.0f},
                                    .row_height = 27.0f,
                                    .max_visible_options = 8});
                            if (deadZoneSelect.changed
                                && deadZoneSelect.selected_index
                                && *deadZoneSelect.selected_index
                                    < deadZoneChoices.size())
                            {
                                recordInputEdit(
                                    editor.projectInputSettings
                                        ->set_controller_dead_zone(
                                            deadZoneChoices[
                                                *deadZoneSelect.selected_index]
                                                .value_q15));
                            }
                        }

                        for (const auto& binding : projectInput.bindings)
                        {
                            gui::label(binding.action_label);
                            gui::property_row(
                                "[binding] Device",
                                binding.source_label,
                                132.0f);
                            switch (binding.device)
                            {
                            case project_input::BindingDevice::keyboard:
                            {
                                const auto bindingSelect = gui::select_box(
                                    gui::SelectBoxOptions{
                                        .id = "project-input-key-"
                                            + std::to_string(
                                                binding.id.value),
                                        .placeholder = "Choose project key",
                                        .selected =
                                            project_key_label(binding.code),
                                        .options =
                                            std::span<const std::string_view>{
                                                keyChoiceLabels.data(),
                                                keyChoiceLabels.size()},
                                        .size = {
                                            (std::min)(centerWidth, 320.0f),
                                            30.0f},
                                        .row_height = 27.0f,
                                        .max_visible_options = 8});
                                if (bindingSelect.changed
                                    && bindingSelect.selected_index
                                    && *bindingSelect.selected_index
                                        < keyChoices.size())
                                {
                                    recordInputEdit(
                                        editor.projectInputSettings
                                            ->rebind_keyboard(
                                                binding.id,
                                                keyChoices[
                                                    *bindingSelect
                                                        .selected_index]));
                                }
                                break;
                            }
                            case project_input::BindingDevice::controller_button:
                            {
                                const auto sourceSelect = gui::select_box(
                                    gui::SelectBoxOptions{
                                        .id =
                                            "project-input-controller-button-"
                                            + std::to_string(
                                                binding.id.value),
                                        .placeholder =
                                            "Choose controller button",
                                        .selected =
                                            epochengine::project_input::
                                                controller_button_name(
                                                    static_cast<
                                                        epochengine::
                                                            project_input::
                                                                ControllerButton>(
                                                        binding.code)),
                                        .options =
                                            std::span<const std::string_view>{
                                                buttonChoiceLabels.data(),
                                                buttonChoiceLabels.size()},
                                        .size = {
                                            (std::min)(centerWidth, 320.0f),
                                            30.0f},
                                        .row_height = 27.0f,
                                        .max_visible_options = 8});
                                if (sourceSelect.changed
                                    && sourceSelect.selected_index
                                    && *sourceSelect.selected_index
                                        < buttonChoices.size())
                                {
                                    recordInputEdit(
                                        editor.projectInputSettings
                                            ->rebind_controller_button(
                                                binding.id,
                                                buttonChoices[
                                                    *sourceSelect
                                                        .selected_index],
                                                binding.controller_slot));
                                }
                                const auto slotSelect = gui::select_box(
                                    gui::SelectBoxOptions{
                                        .id =
                                            "project-input-controller-button-slot-"
                                            + std::to_string(
                                                binding.id.value),
                                        .placeholder = "Choose controller",
                                        .selected =
                                            project_controller_slot_label(
                                                binding.controller_slot),
                                        .options = slotLabels,
                                        .size = {
                                            (std::min)(centerWidth, 320.0f),
                                            30.0f},
                                        .row_height = 27.0f,
                                        .max_visible_options = 4});
                                if (slotSelect.changed
                                    && slotSelect.selected_index
                                    && *slotSelect.selected_index
                                        < slotLabels.size())
                                {
                                    recordInputEdit(
                                        editor.projectInputSettings
                                            ->rebind_controller_button(
                                                binding.id,
                                                static_cast<project_input::ControllerButton>(
                                                    binding.code),
                                                static_cast<std::uint8_t>(
                                                    *slotSelect.selected_index)));
                                }
                                break;
                            }
                            case project_input::BindingDevice::controller_axis:
                            {
                                const auto sourceSelect = gui::select_box(
                                    gui::SelectBoxOptions{
                                        .id =
                                            "project-input-controller-axis-"
                                            + std::to_string(
                                                binding.id.value),
                                        .placeholder =
                                            "Choose controller axis",
                                        .selected =
                                            epochengine::project_input::
                                                controller_axis_name(
                                                    static_cast<
                                                        epochengine::
                                                            project_input::
                                                                ControllerAxis>(
                                                        binding.code)),
                                        .options =
                                            std::span<const std::string_view>{
                                                axisChoiceLabels.data(),
                                                axisChoiceLabels.size()},
                                        .size = {
                                            (std::min)(centerWidth, 320.0f),
                                            30.0f},
                                        .row_height = 27.0f,
                                        .max_visible_options = 6});
                                if (sourceSelect.changed
                                    && sourceSelect.selected_index
                                    && *sourceSelect.selected_index
                                        < axisChoices.size())
                                {
                                    recordInputEdit(
                                        editor.projectInputSettings
                                            ->rebind_controller_axis(
                                                binding.id,
                                                axisChoices[
                                                    *sourceSelect
                                                        .selected_index],
                                                binding.controller_slot));
                                }
                                const auto slotSelect = gui::select_box(
                                    gui::SelectBoxOptions{
                                        .id =
                                            "project-input-controller-axis-slot-"
                                            + std::to_string(
                                                binding.id.value),
                                        .placeholder = "Choose controller",
                                        .selected =
                                            project_controller_slot_label(
                                                binding.controller_slot),
                                        .options = slotLabels,
                                        .size = {
                                            (std::min)(centerWidth, 320.0f),
                                            30.0f},
                                        .row_height = 27.0f,
                                        .max_visible_options = 4});
                                if (slotSelect.changed
                                    && slotSelect.selected_index
                                    && *slotSelect.selected_index
                                        < slotLabels.size())
                                {
                                    recordInputEdit(
                                        editor.projectInputSettings
                                            ->rebind_controller_axis(
                                                binding.id,
                                                static_cast<project_input::ControllerAxis>(
                                                    binding.code),
                                                static_cast<std::uint8_t>(
                                                    *slotSelect.selected_index)));
                                }
                                break;
                            }
                            }
                        }

                        const auto actionSnapshot =
                            editor.projectInputSettings->snapshot();
                        const std::array inputDraftActions{
                            gui::InlineButtonSpec{
                                .label = "Apply Changes",
                                .width = 132.0f,
                                .enabled = actionSnapshot.dirty
                                    && actionSnapshot.evidence.conflicts.empty()},
                            gui::InlineButtonSpec{
                                .label = "Discard Draft",
                                .width = 124.0f,
                                .enabled = actionSnapshot.dirty},
                            gui::InlineButtonSpec{
                                .label = "Stage Defaults",
                                .width = 128.0f},
                            gui::InlineButtonSpec{
                                .label = "Reload Disk",
                                .width = 112.0f,
                                .enabled = !actionSnapshot.dirty}};
                        if (const auto action = gui::inline_button_row(
                                inputDraftActions, 30.0f, 6.0f))
                        {
                            if (*action == 0u)
                            {
                                const auto applied =
                                    apply_project_input_settings(editor);
                                push_editor_log(
                                    editor,
                                    std::string{applied.succeeded
                                        ? "[input] " : "[input][ERROR] "}
                                        + applied.summary);
                            }
                            else if (*action == 1u)
                            {
                                recordInputEdit(
                                    editor.projectInputSettings->discard());
                            }
                            else if (*action == 2u)
                            {
                                recordInputEdit(
                                    editor.projectInputSettings
                                        ->reset_defaults());
                            }
                            else
                            {
                                (void)load_project_input_settings(editor, true);
                                push_editor_log(
                                    editor,
                                    "[input] Reloaded project input from disk.");
                            }
                        }
                    }
                    else if (gui::button(
                        "Retry Input Load",
                        {(std::min)(centerWidth, 220.0f), 30.0f}))
                    {
                        (void)load_project_input_settings(editor, true);
                    }
                }

                if (editor.projectWorkspaceSection == 2u)
                {
                    gui::label("Audio Settings");
                    const auto projectAudio =
                        editor_project_audio_profile_summary(
                            editor.projectId);
                    gui::label("Project Audio");
                    gui::property_row(
                        "[audio] Cues",
                        epochengine::format_text(
                            "{} cues | {} buses",
                            projectAudio.cue_count,
                            projectAudio.bus_count),
                        132.0f);
                    gui::property_row(
                        "[audio] Source",
                        projectAudio.source_materialized
                            ? std::string{"Saved"}
                            : std::string{"Default draft"},
                        132.0f);
                    gui::property_row(
                        "[audio] Status",
                        projectAudio.diagnostic,
                        132.0f);
                    if (gui::button(
                            "Open Project Audio",
                            {(std::min)(centerWidth, 220.0f), 30.0f}))
                    {
                        editor.assetWorkspaceSection = 4u;
                        editor.outlinerToolTab = OutlinerToolTab::Assets;
                        open_editor_surface(
                            EditorMainSurface::Assets,
                            "Project audio controls");
                    }
                }

                if (editor.projectWorkspaceSection == 3u)
                {
                    gui::label("Canvas2D Settings");
                    gui::wrapped_label(
                        "Logical presentation, sampling, density, batching, chunking, and texture budgets for this project.",
                        centerWidth);
                    gui::property_row(
                        "[texture] Policy",
                        projectProfile.renderer_capability.allow_experimental
                            ? "Experimental providers allowed"
                            : "Validated providers only",
                        132.0f);
                    gui::property_row(
                        "[texture] Upload budget",
                        epochengine::format_text(
                            "{} MiB/frame",
                            renderer_capability_profile(ctx).recommended_budgets.upload_budget_bytes
                                / (1024u * 1024u)),
                        132.0f);
                    gui::property_row(
                        "[texture] Residency budget",
                        epochengine::format_text(
                            "{} MiB",
                            renderer_capability_profile(ctx).recommended_budgets.vram_budget_bytes
                                / (1024u * 1024u)),
                        132.0f);
                    if (auto* controller = project_texture_controller(editor))
                    {
                        const auto textureMetrics = controller->metrics();
                        gui::property_row(
                            "[texture] Library",
                            epochengine::format_text(
                                "{} textures | {} KiB decoded",
                                textureMetrics.texture_count,
                                textureMetrics.decoded_bytes / 1024u
                                    + (textureMetrics.decoded_bytes % 1024u != 0u
                                        ? 1u
                                        : 0u)),
                            132.0f);
                    }

                    const auto resolutionChoices = canvas2d_resolution_choices();
                    std::vector<std::string_view> resolutionLabels;
                    resolutionLabels.reserve(resolutionChoices.size());
                    for (const auto& choice : resolutionChoices)
                        resolutionLabels.emplace_back(choice.label);
                    const auto resolutionSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-canvas2d-resolution-select",
                        .placeholder = "Logical canvas resolution",
                        .selected = canvas2d_resolution_label(editor.canvas2dProject),
                        .options = std::span<const std::string_view>{ resolutionLabels.data(), resolutionLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 4
                    });
                    if (resolutionSelect.changed && resolutionSelect.selected_index
                        && *resolutionSelect.selected_index < resolutionChoices.size())
                    {
                        const auto& choice = resolutionChoices[*resolutionSelect.selected_index];
                        editor.canvas2dProject.logical_width = choice.width;
                        editor.canvas2dProject.logical_height = choice.height;
                        push_editor_log(editor, std::string("[canvas2d] Logical extent set to ") + std::string(choice.label) + ".");
                    }
                    gui::property_row("[canvas2d] Logical extent", std::string(canvas2d_resolution_label(editor.canvas2dProject)), 132.0f);

                    static constexpr std::array<std::string_view, 4> scaleLabels{
                        "Integer fit", "Fractional fit", "Fill and crop", "Stretch"
                    };
                    const auto scaleSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-canvas2d-scale-select",
                        .placeholder = "Presentation scaling",
                        .selected = canvas2d_scale_policy_label(editor.canvas2dProject.scale_policy),
                        .options = std::span<const std::string_view>{ scaleLabels.data(), scaleLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 4
                    });
                    if (scaleSelect.changed && scaleSelect.selected_index)
                    {
                        editor.canvas2dProject.scale_policy = static_cast<std::uint8_t>(*scaleSelect.selected_index);
                        push_editor_log(editor, std::string("[canvas2d] Presentation scale set to ")
                            + std::string(canvas2d_scale_policy_label(editor.canvas2dProject.scale_policy)) + ".");
                    }

                    static constexpr std::array<std::string_view, 2> samplingLabels{ "Nearest", "Linear" };
                    const auto samplingSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-canvas2d-sampling-select",
                        .placeholder = "Canvas sampling",
                        .selected = canvas2d_sampling_policy_label(editor.canvas2dProject.sampling_policy),
                        .options = std::span<const std::string_view>{ samplingLabels.data(), samplingLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 2
                    });
                    if (samplingSelect.changed && samplingSelect.selected_index)
                    {
                        editor.canvas2dProject.sampling_policy = static_cast<std::uint8_t>(*samplingSelect.selected_index);
                        push_editor_log(editor, std::string("[canvas2d] Sampling set to ")
                            + std::string(canvas2d_sampling_policy_label(editor.canvas2dProject.sampling_policy)) + ".");
                    }

                    static constexpr std::array<Canvas2DBudgetChoice, 5> pixelsPerUnitChoices{ {
                        { "16 pixels per world unit", 16u },
                        { "32 pixels per world unit", 32u },
                        { "64 pixels per world unit", 64u },
                        { "100 pixels per world unit", 100u },
                        { "128 pixels per world unit", 128u }
                    } };
                    std::array<std::string_view, pixelsPerUnitChoices.size()> pixelsPerUnitLabels{};
                    std::string_view selectedPixelsPerUnit = "Custom pixel density";
                    for (std::size_t index = 0; index < pixelsPerUnitChoices.size(); ++index)
                    {
                        pixelsPerUnitLabels[index] = pixelsPerUnitChoices[index].label;
                        if (editor.canvas2dProject.pixels_per_world_unit == static_cast<float>(pixelsPerUnitChoices[index].value))
                            selectedPixelsPerUnit = pixelsPerUnitChoices[index].label;
                    }
                    const auto pixelsPerUnitSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-canvas2d-pixels-per-unit-select",
                        .placeholder = "Pixel density",
                        .selected = selectedPixelsPerUnit,
                        .options = std::span<const std::string_view>{ pixelsPerUnitLabels.data(), pixelsPerUnitLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 5
                    });
                    if (pixelsPerUnitSelect.changed && pixelsPerUnitSelect.selected_index)
                        editor.canvas2dProject.pixels_per_world_unit = static_cast<float>(pixelsPerUnitChoices[*pixelsPerUnitSelect.selected_index].value);

                    const auto spriteBudgetChoices = canvas2d_sprite_budget_choices();
                    std::vector<std::string_view> spriteBudgetLabels;
                    spriteBudgetLabels.reserve(spriteBudgetChoices.size());
                    for (const auto& choice : spriteBudgetChoices)
                        spriteBudgetLabels.emplace_back(choice.label);
                    const auto spriteBudgetSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-canvas2d-sprite-budget-select",
                        .placeholder = "Sprite batch budget",
                        .selected = canvas2d_sprite_budget_label(editor.canvas2dProject.maximum_sprites_per_batch),
                        .options = std::span<const std::string_view>{ spriteBudgetLabels.data(), spriteBudgetLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 4
                    });
                    if (spriteBudgetSelect.changed && spriteBudgetSelect.selected_index
                        && *spriteBudgetSelect.selected_index < spriteBudgetChoices.size())
                    {
                        editor.canvas2dProject.maximum_sprites_per_batch = spriteBudgetChoices[*spriteBudgetSelect.selected_index].value;
                        push_editor_log(editor, std::string("[canvas2d] ")
                            + std::string(spriteBudgetChoices[*spriteBudgetSelect.selected_index].label) + ".");
                    }

                    static constexpr std::array<Canvas2DBudgetChoice, 4> batchCapChoices{ {
                        { "64 logical batches", 64u },
                        { "256 logical batches", 256u },
                        { "1024 logical batches", 1024u },
                        { "4096 logical batches", 4096u }
                    } };
                    std::array<std::string_view, batchCapChoices.size()> batchCapLabels{};
                    std::string_view selectedBatchCap = "Custom logical batch cap";
                    for (std::size_t index = 0; index < batchCapChoices.size(); ++index)
                    {
                        batchCapLabels[index] = batchCapChoices[index].label;
                        if (editor.canvas2dProject.maximum_batches == batchCapChoices[index].value)
                            selectedBatchCap = batchCapChoices[index].label;
                    }
                    const auto batchCapSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-canvas2d-batch-cap-select",
                        .placeholder = "Logical batch cap",
                        .selected = selectedBatchCap,
                        .options = std::span<const std::string_view>{ batchCapLabels.data(), batchCapLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 4
                    });
                    if (batchCapSelect.changed && batchCapSelect.selected_index)
                        editor.canvas2dProject.maximum_batches = batchCapChoices[*batchCapSelect.selected_index].value;

                    const auto chunkChoices = canvas2d_chunk_extent_choices();
                    std::vector<std::string_view> chunkLabels;
                    chunkLabels.reserve(chunkChoices.size());
                    for (const auto& choice : chunkChoices)
                        chunkLabels.emplace_back(choice.label);
                    const auto chunkSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-canvas2d-chunk-select",
                        .placeholder = "Tile chunk extent",
                        .selected = canvas2d_chunk_extent_label(editor.canvas2dProject.tile_chunk_extent),
                        .options = std::span<const std::string_view>{ chunkLabels.data(), chunkLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 5
                    });
                    if (chunkSelect.changed && chunkSelect.selected_index && *chunkSelect.selected_index < chunkChoices.size())
                    {
                        editor.canvas2dProject.tile_chunk_extent = chunkChoices[*chunkSelect.selected_index].value;
                        push_editor_log(editor, std::string("[canvas2d] Tile chunk set to ")
                            + std::string(chunkChoices[*chunkSelect.selected_index].label) + ".");
                    }

                    if (gui::button(editor.canvas2dProject.pixel_snapping ? "Pixel Snap: On" : "Pixel Snap: Off", { 180.0f, 30.0f }))
                    {
                        editor.canvas2dProject.pixel_snapping = !editor.canvas2dProject.pixel_snapping;
                        push_editor_log(editor, editor.canvas2dProject.pixel_snapping
                            ? "[canvas2d] Pixel snapping enabled."
                            : "[canvas2d] Pixel snapping disabled.");
                    }
                }
                if (editor.projectWorkspaceSection == 4u)
                {
                    gui::label("Run + Build");
                    gui::wrapped_label(
                        "Save the active project, play the composed runtime in this editor, build a new child artifact generation, or launch the accepted standalone executable.",
                        centerWidth);
                if (gui::button("Save Active Project", { 220.0f, 30.0f }))
                    repair_active_project_evidence(editor);
                if (application.allow_project_run)
                {
                    gui::wrapped_label(
                        "Play In Editor uses the saved scene in this editor. Build validates a new child artifact generation. Launch owns, focuses, and stops the accepted standalone runtime.",
                        centerWidth);
                    if (gui::button("Play In Editor", {220.0f, 30.0f}))
                        play_active_context();

                    if (editor.projectBuildPending)
                    {
                        if (gui::button(
                                "Cancel Project Build",
                                {220.0f, 30.0f}))
                        {
                            (void)cancel_project_build(editor);
                        }
                    }
                    else if (gui::button(
                        "Build Active Project", {220.0f, 30.0f}))
                    {
                        start_project_build(
                            editor, false, "Project surface");
                    }

                    const bool runtimeActive =
                        editor.projectRuntimeSnapshot
                        && editor.projectRuntimeSnapshot->active();
                    if (runtimeActive)
                    {
                        if (gui::button(
                                "Focus Standalone Runtime",
                                {220.0f, 30.0f}))
                        {
                            (void)focus_project_runtime(editor);
                        }
                        if (gui::button(
                                "Stop Standalone Runtime",
                                {220.0f, 30.0f}))
                        {
                            (void)stop_project_runtime(
                                editor,
                                platform::child_process::StopMode::graceful);
                        }
                        if (editor.projectRuntimeForceStopAvailable
                            && gui::button(
                                "Force Stop Runtime",
                                {220.0f, 30.0f}))
                        {
                            (void)stop_project_runtime(
                                editor,
                                platform::child_process::StopMode::force);
                        }
                    }
                    else if (gui::button(
                        "Launch Single Context", {220.0f, 30.0f}))
                    {
                        launch_active_project_context();
                    }
                    gui::wrapped_label(
                        editor.projectRuntimeStatus, centerWidth);
                }
                else
                {
                    gui::property_row("[project] Runtime", "Not owned by this dedicated editor", 108.0f);
                }
                gui::wrapped_label(editor.projectStatus, centerWidth);
                gui::wrapped_label(editor.projectBuildStatus, centerWidth);
                if (!editor.projectBuildDiagnosticTail.empty())
                {
                    gui::label("Build Diagnostics");
                    gui::wrapped_label(
                        editor.projectBuildDiagnosticTail, centerWidth);
                }
                }
                break;
            }
            case EditorMainSurface::Assets:
            {
                gui::label("Asset Manager");
                const std::array workspaceTabs{
                    gui::TabButtonSpec{
                        .id = "assets.workspace.browser",
                        .label = "Browser",
                        .width = 88.0f,
                        .active = editor.assetWorkspaceSection == 0u},
                    gui::TabButtonSpec{
                        .id = "assets.workspace.textures",
                        .label = "Textures",
                        .width = 92.0f,
                        .active = editor.assetWorkspaceSection == 1u},
                    gui::TabButtonSpec{
                        .id = "assets.workspace.scripts",
                        .label = "Scripts",
                        .width = 80.0f,
                        .active = editor.assetWorkspaceSection == 2u},
                    gui::TabButtonSpec{
                        .id = "assets.workspace.graph",
                        .label = "Graph",
                        .width = 72.0f,
                        .active = editor.assetWorkspaceSection == 3u},
                    gui::TabButtonSpec{
                        .id = "assets.workspace.audio",
                        .label = "Audio",
                        .width = 72.0f,
                        .active = editor.assetWorkspaceSection == 4u}
                };
                if (const auto tabs = gui::tab_bar_buttons(
                        workspaceTabs, 31.0f, 2.0f);
                    tabs.selected_index)
                {
                    editor.assetWorkspaceSection = *tabs.selected_index;
                }

                if (editor.assetWorkspaceSection == 0u)
                {
                    gui::label("Project Browser");
                gui::property_row("[assets] Project", editor.projectName, 120.0f);
                gui::property_row(
                    "[assets] Folder",
                    editor.assetBrowserFolder.empty()
                        ? std::string{"Project"}
                        : editor.assetBrowserFolder,
                    120.0f);
                if (!editor.assetBrowserFolder.empty()
                    && gui::button("Up", {54.0f, 28.0f}))
                {
                    leave_asset_browser_folder(editor);
                }
                draw_asset_category_tabs(editor, true);
                (void)gui::edit_box(
                    editor.assetSearch,
                    {centerWidth, 30.0f},
                    96u,
                    false);

                const auto assetEntries = collect_asset_manager_entries(
                    editor.projectRoot,
                    editor.assetBrowserFolder,
                    512u);
                auto* textureController = project_texture_controller(editor);
                const auto textureForEntry = [&](const EditorBrowserEntry& entry)
                    -> const editor_project_textures::TextureCatalogEntry*
                {
                    if (!textureController || entry.kind != "TEXTURE")
                        return nullptr;
                    return project_texture_for_asset_path(
                        *textureController,
                        editor.projectRoot,
                        entry.path);
                };
                const auto gridKind = [](const EditorBrowserEntry& entry) noexcept
                {
                    if (entry.directory)
                        return gui::AssetGridItemKind::Folder;
                    if (entry.kind == "IMAGE")
                        return gui::AssetGridItemKind::Image;
                    if (entry.kind == "TEXTURE")
                        return gui::AssetGridItemKind::Texture;
                    if (entry.kind == "MODEL")
                        return gui::AssetGridItemKind::Model;
                    if (entry.kind == "AUDIO")
                        return gui::AssetGridItemKind::Audio;
                    if (entry.kind == "MAP")
                        return gui::AssetGridItemKind::Scene;
                    if (entry.kind == "GUI" || entry.kind == "SCRIPT"
                        || entry.kind == "CODE" || entry.kind == "TEXT")
                    {
                        return gui::AssetGridItemKind::Document;
                    }
                    return gui::AssetGridItemKind::Generic;
                };
                struct AssetGridPresentation final
                {
                    std::size_t entry_index{};
                    std::uint64_t id{};
                    std::string label{};
                    std::string detail{};
                    std::string search_terms{};
                    SpriteHandle image{};
                    gui::Vec2 source_size{};
                    gui::AssetGridItemKind kind{
                        gui::AssetGridItemKind::Generic};
                    std::string logical_texture{};
                };
                std::vector<AssetGridPresentation> presentations{};
                presentations.reserve(assetEntries.size());
                for (std::size_t index = 0u;
                     index < assetEntries.size();
                     ++index)
                {
                    const EditorBrowserEntry& entry = assetEntries[index];
                    if (!asset_category_visible(editor.assetCategory, entry.kind))
                        continue;

                    AssetGridPresentation presentation{};
                    presentation.entry_index = index;
                    presentation.id = asset_graph_id(entry.path);
                    presentation.label = std::filesystem::path{entry.path}
                        .filename()
                        .string();
                    presentation.detail = entry.directory
                        ? (entry.child_count_truncated
                            ? epochengine::format_text(
                                "{}+ items", entry.child_count)
                            : epochengine::format_text(
                                "{} item{}",
                                entry.child_count,
                                entry.child_count == 1u ? "" : "s"))
                        : epochengine::format_text(
                            "{} | {} KiB",
                            entry.kind,
                            (entry.size + 1023u) / 1024u);
                    presentation.search_terms = entry.label + " " + entry.path
                        + (entry.directory ? " folder directory" : "");
                    presentation.kind = gridKind(entry);

                    if (const auto* texture = textureForEntry(entry))
                    {
                        presentation.logical_texture = texture->logical_path;
                        if (const auto* thumbnail =
                                project_texture_thumbnail_state(
                                    editor,
                                    *textureController,
                                    *texture))
                        {
                            presentation.image = thumbnail->sprite;
                            presentation.source_size = {
                                static_cast<float>(thumbnail->source_width),
                                static_cast<float>(thumbnail->source_height)};
                            presentation.detail = epochengine::format_text(
                                "TEXTURE | {}x{} | {} KiB",
                                thumbnail->source_width,
                                thumbnail->source_height,
                                (entry.size + 1023u) / 1024u);
                        }
                    }
                    else if (entry.kind == "TEXTURE")
                    {
                        if (const auto* thumbnail =
                                project_source_texture_thumbnail(
                                    editor,
                                    entry.path))
                        {
                            presentation.image = thumbnail->sprite;
                            presentation.source_size = {
                                static_cast<float>(thumbnail->source_width),
                                static_cast<float>(thumbnail->source_height)};
                            presentation.detail = epochengine::format_text(
                                "TEXTURE | {}x{} | {} KiB",
                                thumbnail->source_width,
                                thumbnail->source_height,
                                (entry.size + 1023u) / 1024u);
                        }
                    }
                    presentations.push_back(std::move(presentation));
                }

                std::vector<gui::AssetGridItem> gridItems{};
                gridItems.reserve(presentations.size());
                std::optional<std::uint64_t> selectedGridId{};
                for (const AssetGridPresentation& presentation : presentations)
                {
                    const EditorBrowserEntry& entry =
                        assetEntries[presentation.entry_index];
                    const bool selected = editor.selectedAssetPath == entry.path
                        || (!presentation.logical_texture.empty()
                            && (editor.selectedAssetPath
                                    == presentation.logical_texture
                                || (textureController
                                    && textureController->selected_logical_path()
                                        == presentation.logical_texture)));
                    if (selected)
                        selectedGridId = presentation.id;
                    gridItems.push_back(gui::AssetGridItem{
                        .id = presentation.id,
                        .label = presentation.label,
                        .detail = presentation.detail,
                        .search_terms = presentation.search_terms,
                        .image = presentation.image,
                        .source_size = presentation.source_size,
                        .kind = presentation.kind
                    });
                }

                const float assetGridHeight = (std::clamp)(
                    centerScrollHeight - 180.0f,
                    300.0f,
                    680.0f);
                const std::array assetContextActions{
                    gui::ContextMenuActionSpec{
                        .id = "open",
                        .label = "Open"},
                    gui::ContextMenuActionSpec{
                        .id = "details",
                        .label = "Show Details"},
                    gui::ContextMenuActionSpec{
                        .id = "copy_path",
                        .label = "Copy Path"}
                };
                const gui::AssetGridResult gridResult = gui::asset_grid({
                    .id = "asset-manager.browser-grid",
                    .items = gridItems,
                    .context_actions = assetContextActions,
                    .query = editor.assetSearch,
                    .size = {centerWidth, assetGridHeight},
                    .tile_size = {148.0f, 166.0f},
                    .gap = {8.0f, 8.0f},
                    .padding = {8.0f, 8.0f},
                    .image_height = 108.0f,
                    .scroll_offset = editor.assetGridScroll,
                    .selected_id = selectedGridId
                });
                editor.assetGridScroll = gridResult.scroll_offset;
                editor.assetGridVisibleIds = gridResult.visible_ids;

                const auto presentationForId =
                    [&](std::uint64_t id)
                    {
                        return std::find_if(
                            presentations.begin(),
                            presentations.end(),
                            [id](
                                const AssetGridPresentation&
                                    candidate)
                            {
                                return candidate.id == id;
                            });
                    };
                const auto selectAssetPresentation =
                    [&](const AssetGridPresentation&
                            presentation,
                        bool open)
                    {
                        const EditorBrowserEntry& entry =
                            assetEntries[
                                presentation.entry_index];
                        editor.selectedAssetPath =
                            entry.path;
                        editor.selectedProjectFile =
                            entry.path;
                        if (!open)
                            return;
                        if (entry.directory)
                        {
                            enter_asset_browser_folder(
                                editor, entry);
                            return;
                        }
                        if (const auto* texture =
                                textureForEntry(entry))
                        {
                            (void)activate_project_texture(
                                editor,
                                ctx,
                                *textureController,
                                *texture);
                            editor.assetWorkspaceSection =
                                1u;
                        }
                        else if (entry.kind == "IMAGE")
                        {
                            editor.projectTextureStatus =
                                "PNG/JPEG is visible as a project reference; texture compilation currently accepts BMP, TGA, PPM, and .epoch_texture sources.";
                        }
                        else if (entry.kind == "AUDIO")
                        {
                            const auto audio =
                                editor_project_audio_profile_summary(
                                    editor.projectId);
                            const auto cue = std::find_if(
                                audio.cues.begin(),
                                audio.cues.end(),
                                [&entry](const auto& candidate)
                                {
                                    return candidate.logical_path
                                        == entry.path;
                                });
                            if (cue != audio.cues.end())
                            {
                                editor.selectedProjectAudioCueId =
                                    cue->stable_id;
                            }
                            editor.assetWorkspaceSection = 4u;
                        }
                        if (std::filesystem::path{
                                entry.path}
                                .filename()
                                .string()
                                .ends_with(
                                    ".ascript.cpp"))
                        {
                            editor.activeScript =
                                script_id_from_source_path(
                                    std::filesystem::path{
                                        entry.path});
                            editor.scriptBuildStatus =
                                "Selected script asset: "
                                + entry.path;
                            editor.assetWorkspaceSection =
                                2u;
                            (void)load_script_source_editor(
                                editor,
                                entry.path,
                                false);
                        }
                        push_editor_log(
                            editor,
                            "[assets] Opened "
                                + entry.path);
                    };

                if (gridResult.context_target_id)
                {
                    const auto contextTarget =
                        presentationForId(
                            *gridResult.context_target_id);
                    if (contextTarget
                        != presentations.end())
                    {
                        selectAssetPresentation(
                            *contextTarget,
                            false);
                        if (gridResult.selected_action_id)
                        {
                            if (*gridResult
                                    .selected_action_id
                                == "open")
                            {
                                selectAssetPresentation(
                                    *contextTarget,
                                    true);
                            }
                            else if (*gridResult
                                    .selected_action_id
                                == "details")
                            {
                                editor.showInspector = true;
                                editor.inspectorToolTab =
                                    InspectorToolTab::
                                        Properties;
                            }
                            else if (*gridResult
                                    .selected_action_id
                                == "copy_path")
                            {
                                (void)gui::
                                    set_clipboard_text(
                                        assetEntries[
                                            contextTarget
                                                ->entry_index]
                                            .path);
                            }
                        }
                    }
                }
                else if (gridResult.activated_id)
                {
                    const auto activated =
                        presentationForId(
                            *gridResult.activated_id);
                    if (activated != presentations.end())
                    {
                        selectAssetPresentation(
                            *activated,
                            true);
                    }
                }
                gui::property_row(
                    "[assets] Visible",
                    epochengine::format_text(
                        "{} / {}",
                        gridResult.matched_count,
                        presentations.size()),
                    120.0f);
                gui::property_row(
                    "[assets] Selected",
                    editor.selectedAssetPath.empty()
                        ? std::string{"(none)"}
                        : editor.selectedAssetPath,
                    120.0f);
                if (gridResult.truncated)
                {
                    gui::wrapped_label(
                        "The current asset view reached its bounded catalog or visible-tile limit.",
                        centerWidth);
                }

                }
                else if (editor.assetWorkspaceSection == 1u)
                {
                    gui::label("Texture Workspace");
                    draw_texture_source_controls(
                        editor,
                        ctx,
                        (std::min)(centerWidth, 620.0f));

                if (auto* textures =
                        project_texture_controller(editor))
                {
                    gui::property_row(
                        "[texture] Compiled",
                        std::to_string(textures->catalog().size()),
                        120.0f);
                    if (editor.assetCategory == 0u
                        || editor.assetCategory == 1u)
                    {
                        gui::label("Compiled Texture Library");
                        draw_project_texture_catalog(
                            editor,
                            ctx,
                            *textures,
                            (std::min)(centerWidth, 560.0f),
                            "central.assets",
                            true);
                    }
                    draw_active_texture_authoring(
                        editor,
                        ctx,
                        (std::min)(centerWidth, 560.0f),
                        "central.assets",
                        300.0f);
                    draw_texture_application_controls(
                        editor,
                        (std::min)(centerWidth, 620.0f));
                }
                }
                else if (editor.assetWorkspaceSection == 2u)
                {
                const auto codeWorkspace = draw_project_code_workspace(
                    editor,
                    centerWidth,
                    centerScrollHeight);
                if (codeWorkspace.returnToScene)
                {
                    apply_editor_surface(
                        EditorMainSurface::Scene,
                        "Project Scripts");
                }
                if (codeWorkspace.handled)
                    break;
                const std::string activeScriptSource =
                    editor_resolve_script_source_path(
                        editor.activeScript,
                        editor.projectRoot);
                {
                    gui::label("Script Assets");
                    gui::property_row("[script asset] Active", editor.activeScript, 120.0f);
                    gui::property_row("[script asset] Source", activeScriptSource, 120.0f);
                    gui::property_row(
                        "[script asset] Source exists",
                        std::filesystem::exists(std::filesystem::path{ activeScriptSource }) ? "true" : "false",
                        120.0f);
                    if (const auto* activeScript = active_script_profile(editor))
                    {
                        gui::property_row("[script asset] Build", activeScript->build_action, 120.0f);
                        gui::property_row("[script asset] Run", "active project only", 120.0f);
                        gui::property_row("[script asset] Hint", activeScript->diagnostic_hint, 120.0f);
                    }
                    gui::property_row(
                        "[script asset] Build state",
                        editor.scriptBuildPending ? "Running" : "Idle",
                        120.0f);
                    gui::property_row(
                        "[script asset] Output",
                        editor.scriptBuildTarget.empty() ? std::string("(not built this session)") : editor.scriptBuildTarget,
                        120.0f);
                    gui::wrapped_label(editor.scriptBuildStatus, centerWidth);
                    gui::property_row("[script asset] New", "type a safe id, then create a project-local .ascript.cpp", 120.0f);
                    (void)gui::edit_box(editor.newScriptName, { centerWidth, 28.0f }, 64, false);
                    if (editor.scriptBuildPending)
                    {
                        gui::progress_bar(gui::ProgressBarOptions{
                            .label = "Compiling C++23 Script Asset",
                            .status = editor.scriptBuildStatus,
                            .value = 0.0f,
                            .size = { (std::min)(centerWidth, 520.0f), 20.0f },
                            .show_percent = false,
                            .activity = true
                        });
                        if (gui::button(
                                "Cancel Script Build",
                                { (std::min)(220.0f, centerWidth), 30.0f }))
                        {
                            (void)cancel_script_build(editor);
                        }
                    }
                    else
                    {
                        if (gui::button("Create Script Asset", { (std::min)(220.0f, centerWidth), 30.0f }))
                            create_project_script_starter(editor);
                        if (gui::button("Build Selected Script Asset", { (std::min)(240.0f, centerWidth), 30.0f }))
                            start_script_build(editor, "Scripting workspace build request");
                    }
                    draw_script_source_editor(
                        editor,
                        activeScriptSource,
                        centerWidth,
                        (std::clamp)(
                            centerScrollHeight - 360.0f,
                            320.0f,
                            760.0f));
                }
                }
                else if (editor.assetWorkspaceSection == 3u)
                {
                    gui::label("Asset Dependency Graph");
                    gui::wrapped_label(
                        "This graph projects project files and authenticated "
                        "compiled texture artifacts. Selecting a texture node "
                        "opens its real texture workspace state.",
                        centerWidth);
                    draw_asset_dependency_graph(
                        editor,
                        (std::min)(centerWidth, 760.0f));
                }
                else
                {
                    draw_project_audio_workspace(
                        editor,
                        (std::min)(centerWidth, 720.0f));
                }
                break;
            }
            case EditorMainSurface::PlantLab:
            {
                const auto forestPackage = std::find_if(
                    epochengine::package_registry::kKnownPackages.begin(),
                    epochengine::package_registry::kKnownPackages.end(),
                    [](const epochengine::package_registry::PackageDescriptor& package) {
                        return package.id == epochengine::package_registry::kEngineForestFactoryPackageId;
                    });
                (void)ensure_plant_lab_compiled_asset(editor);
                const auto& document = editor.plantLabDocument;
                const auto& profile = document.profile;
                const auto& compiled = editor.plantLabCompiled;
                const auto& geometry = compiled.preview;
                const auto stats = geometry.stats;
                const auto& voxelSummary = compiled.voxelOccupancy;
                const bool plantAssetReady =
                    ensure_plant_lab_compiled_asset(editor);
                const auto& plantAsset = editor.plantLabCompiled;
                const std::filesystem::path manifestPath =
                    resolve_editor_path(std::filesystem::path{ editor.projectRoot })
                    / "assets" / "packages" / "engine_forest_factory.package.json";
                const std::filesystem::path profilePath =
                    resolve_editor_path(std::filesystem::path{ editor.projectRoot })
                    / "assets" / "packages" / "engine_forest_factory" / "default.forest.json";
                const bool manifestReady = std::filesystem::exists(manifestPath);
                const bool profileReady = std::filesystem::exists(profilePath);

                gui::label("Plant Lab");
                gui::wrapped_label(
                    "Separate temporal graph and parametric tree/forest authoring editor. Plant Lab owns custom tree assets, forest configurations, and live preview; Forest Factory later places those outputs in standard project scenes.",
                    centerWidth);
                gui::property_row("[forest] Editor name", std::string(epochengine::forest::kPlantLabWorkspace), 148.0f);
                gui::property_row("[forest] Technique", std::string(epochengine::forest::kForestFactoryTechnique), 148.0f);
                gui::property_row("[forest] Reference repo", std::string(epochengine::forest::kForestFactoryReferenceRepo), 148.0f);
                gui::property_row("[forest] Package", forestPackage != epochengine::package_registry::kKnownPackages.end() ? std::string(forestPackage->displayName) : std::string("(missing registry entry)"), 148.0f);
                gui::property_row("[forest] Package source", forestPackage != epochengine::package_registry::kKnownPackages.end() ? std::string(forestPackage->externalSourceRepo) : std::string("(missing registry entry)"), 148.0f);
                gui::property_row("[forest] Manifest", manifestReady ? display_project_path(manifestPath) : std::string("missing - activate through Package Manager"), 148.0f);
                gui::property_row("[forest] Profile", profileReady ? display_project_path(profilePath) : std::string("missing - activate through Package Manager"), 148.0f);
                gui::property_row(
                    "[forest] Source revision",
                    std::to_string(document.revision),
                    148.0f);
                gui::property_row(
                    "[forest] Content",
                    epochengine::format_text(
                        "{:016x}", document.contentHash),
                    148.0f);
                gui::property_row(
                    "[forest] History",
                    epochengine::format_text(
                        "{} / {} semantic edits",
                        document.historyCursor,
                        document.journal.size()),
                    148.0f);
                gui::property_row(
                    "[forest] Compiler",
                    editor.plantLabStatus,
                    148.0f);
                gui::property_row(
                    "[forest] Project asset",
                    editor.plantLabAssetStatus,
                    148.0f);
                gui::property_row(
                    "[forest] Voxel LODs",
                    std::to_string(compiled.voxelLods.levels.size()),
                    148.0f);
                gui::property_row("[forest] Preset", std::string(epochengine::forest::preset_name(profile.preset)), 148.0f);
                gui::property_row("[forest] Mode", profile.previewMode == epochengine::forest::ForestPreviewMode::Mode3D ? "3D" : "2D", 148.0f);
                gui::property_row("[forest] Stage", std::string(epochengine::forest::stage_name(profile.editStage)), 148.0f);
                gui::property_row("[forest] Seed", std::to_string(profile.seed.value), 148.0f);
                gui::property_row("[forest] Time", epochengine::format_text("{:.2f}s / {:.2f}s", profile.temporal.timeSeconds, profile.temporal.durationSeconds), 148.0f);
                gui::property_row("[forest] Speed", epochengine::format_text("{:.2f}x", profile.temporal.speed), 148.0f);
                gui::property_row("[forest] Nodes", std::to_string(stats.nodes), 148.0f);
                gui::property_row("[forest] Branches", std::to_string(stats.branches), 148.0f);
                gui::property_row("[forest] Leaves", std::to_string(stats.leaves), 148.0f);
                gui::property_row("[forest] Preview segments", std::to_string(geometry.segmentCount), 148.0f);
                gui::property_row("[forest] Verts", std::to_string(stats.vertices), 148.0f);
                gui::property_row("[forest] Tris", std::to_string(stats.triangles), 148.0f);
                gui::property_row("[forest] Voxel chunk",
                    epochengine::format_text(
                        "{}x{}x{} @ {:.2f}m",
                        voxelSummary.chunk.cellsX,
                        voxelSummary.chunk.cellsY,
                        voxelSummary.chunk.cellsZ,
                        voxelSummary.chunk.cellSizeMeters),
                    148.0f);
                gui::property_row("[forest] Active voxels", std::to_string(voxelSummary.activeCells), 148.0f);
                gui::property_row("[forest] Dense bytes", std::to_string(voxelSummary.denseBytes), 148.0f);
                gui::property_row("[forest] Voxel split",
                    epochengine::format_text(
                        "trunk {} | branch {} | foliage {}",
                        voxelSummary.trunkCells,
                        voxelSummary.branchCells,
                        voxelSummary.foliageCells),
                    148.0f);
                gui::wrapped_label(
                    "Plant Lab authors deterministic tree assets and forest configurations with temporal graph, foliage, and voxel LOD intent. Forest Factory browsing, placement, scene overrides, and package activation remain explicit standard-editor actions.",
                    centerWidth);
                const auto refreshPlantLab = [&]()
                {
                    editor.plantLabCompiledRevision = 0u;
                    editor.plantLabCompiledContentHash = 0u;
                    ensure_plant_lab_preview_entities(editor);
                };
                const auto applyPlantEdit = [&](
                    epochengine::forest::ForestProfileProperty property,
                    float requestedValue,
                    std::string_view label)
                {
                    if (!epochengine::forest::apply_forest_profile_edit(
                            editor.plantLabDocument,
                            property,
                            requestedValue))
                    {
                        editor.plantLabStatus =
                            std::string(label)
                            + " is already at its validated value.";
                        return;
                    }
                    refreshPlantLab();
                    push_editor_log(
                        editor,
                        epochengine::format_text(
                            "[plant] {} committed as semantic revision {}.",
                            label,
                            editor.plantLabDocument.revision));
                };

                std::array<gui::InlineButtonSpec, 3> historyActions{{
                    {"Undo", 104.0f},
                    {"Redo", 104.0f},
                    {"Reset Default", 142.0f}
                }};
                if (auto clicked =
                        gui::inline_button_row(historyActions, 30.0f, 8.0f))
                {
                    bool changed = false;
                    if (*clicked == 0)
                    {
                        changed =
                            epochengine::forest::undo_forest_profile_edit(
                                editor.plantLabDocument);
                        if (!changed)
                            editor.plantLabStatus =
                                "No Plant Lab semantic edit is available to undo.";
                    }
                    else if (*clicked == 1)
                    {
                        changed =
                            epochengine::forest::redo_forest_profile_edit(
                                editor.plantLabDocument);
                        if (!changed)
                            editor.plantLabStatus =
                                "No Plant Lab semantic edit is available to redo.";
                    }
                    else
                    {
                        editor.plantLabDocument =
                            epochengine::forest::
                                make_default_plant_lab_document();
                        editor.plantLabDocumentInitialized = true;
                        changed = true;
                    }
                    if (changed)
                    {
                        refreshPlantLab();
                        push_editor_log(
                            editor,
                            epochengine::format_text(
                                "[plant] {} produced revision {}.",
                                *clicked == 0
                                    ? "Undo"
                                    : (*clicked == 1
                                        ? "Redo"
                                        : "Default reset"),
                                editor.plantLabDocument.revision));
                    }
                }

                std::array<gui::InlineButtonSpec, 4> structureActions{{
                    {"Height -", 106.0f},
                    {"Height +", 106.0f},
                    {"Branches -", 118.0f},
                    {"Branches +", 118.0f}
                }};
                if (auto clicked =
                        gui::inline_button_row(
                            structureActions,
                            30.0f,
                            8.0f))
                {
                    if (*clicked == 0)
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                TargetHeightMeters,
                            editor.plantLabDocument.profile.config
                                .targetHeightMeters - 0.25f,
                            "Target height");
                    else if (*clicked == 1)
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                TargetHeightMeters,
                            editor.plantLabDocument.profile.config
                                .targetHeightMeters + 0.25f,
                            "Target height");
                    else if (*clicked == 2)
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                BranchLevels,
                            static_cast<float>(
                                editor.plantLabDocument.profile.branch.levels)
                                - 1.0f,
                            "Branch levels");
                    else
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                BranchLevels,
                            static_cast<float>(
                                editor.plantLabDocument.profile.branch.levels)
                                + 1.0f,
                            "Branch levels");
                }

                std::array<gui::InlineButtonSpec, 4> shapeActions{{
                    {"Length -", 106.0f},
                    {"Length +", 106.0f},
                    {"Growth -1s", 118.0f},
                    {"Growth +1s", 118.0f}
                }};
                if (auto clicked =
                        gui::inline_button_row(shapeActions, 30.0f, 8.0f))
                {
                    if (*clicked == 0)
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                BranchLengthMeters,
                            editor.plantLabDocument.profile.branch
                                .branchLengthMeters - 0.10f,
                            "Branch length");
                    else if (*clicked == 1)
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                BranchLengthMeters,
                            editor.plantLabDocument.profile.branch
                                .branchLengthMeters + 0.10f,
                            "Branch length");
                    else if (*clicked == 2)
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                GrowthTimeSeconds,
                            editor.plantLabDocument.profile.temporal.timeSeconds
                                - 1.0f,
                            "Growth time");
                    else
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                GrowthTimeSeconds,
                            editor.plantLabDocument.profile.temporal.timeSeconds
                                + 1.0f,
                            "Growth time");
                }

                std::array<gui::InlineButtonSpec, 2> previewActions{{
                    {"Rebuild Preview", 164.0f},
                    {"Select Lead Tip", 154.0f}
                }};
                if (auto clicked =
                        gui::inline_button_row(previewActions, 30.0f, 8.0f))
                {
                    if (*clicked == 0)
                    {
                        refreshPlantLab();
                        push_editor_log(
                            editor,
                            "[plant] Rebuilt the compiled temporal preview.");
                    }
                    else
                    {
                        const auto selected = std::find_if(
                            editor.entities.begin(),
                            editor.entities.end(),
                            [](const EditorEntity& entity)
                            {
                                return entity.name.rfind(
                                    "PlantLabCanopy_",
                                    0u) == 0u;
                            });
                        if (selected != editor.entities.end())
                        {
                            select_editor_entity(
                                editor,
                                static_cast<std::size_t>(
                                    std::distance(
                                        editor.entities.begin(),
                                        selected)));
                            push_editor_log(
                                editor,
                                "[plant] Selected the first visible lead tip.");
                        }
                        else
                        {
                            refreshPlantLab();
                        }
                    }
                }

                const std::array<gui::InlineButtonSpec, 2> persistenceActions{{
                    {"Publish To Project", 178.0f},
                    {"Reopen Published", 168.0f}
                }};
                if (const auto clicked = gui::inline_button_row(
                        persistenceActions, 30.0f, 8.0f))
                {
                    if (*clicked == 0)
                    {
                        (void)publish_plant_lab_project_asset(editor);
                    }
                    else if (reopen_plant_lab_project_asset(editor, true))
                    {
                        ensure_plant_lab_preview_entities(editor);
                    }
                }

                if (gui::button("Open Package Manager", { 220.0f, 30.0f }))
                {
                    editor.showPackageManagerModal = true;
                    editor.selectedPackageId = std::string(epochengine::package_registry::kEngineForestFactoryPackageId);
                    editor.packageInstallStatus = manifestReady && profileReady
                        ? "Forest Factory project package is already staged."
                        : "Select Install to stage the Forest Factory project package.";
                    push_editor_log(editor, "[forest] Package Manager opened for Forest Factory.");
                }
                if (gui::button("Stage Forest Factory Package", { 240.0f, 30.0f }))
                {
                    if (forestPackage != epochengine::package_registry::kKnownPackages.end())
                        (void)stage_forest_factory_package_opt_in(editor, *forestPackage);
                    else
                        editor.packageInstallStatus = "Forest Factory package registry entry is missing.";
                }
                break;
            }
            case EditorMainSurface::Timeline:
            {
                ensure_timeline_defaults(editor);
                auto timelineStats = timeline_stats_from_editor(editor);
                epochengine::timeline::sync_to_simulation(editor.timelineState, timelineStats);
                epochengine::timeline::clamp_state(editor.timelineState);
                epochengine::saveload::clamp_streaming_save_config(editor.streamingSaveConfig);
                editor.streamingSaveStatus.active = editor.streamingSaveConfig.enabled;
                if (epochengine::saveload::should_capture_checkpoint(
                    editor.streamingSaveConfig,
                    editor.streamingSaveStatus,
                    timelineStats))
                {
                    epochengine::saveload::mark_checkpoint_captured(
                        editor.streamingSaveStatus,
                        editor.streamingSaveConfig,
                        timelineStats);
                    editor.timelineEvents.push_back(epochengine::timeline::make_event_from_stats(
                        "save",
                        epochengine::timeline::TimelineEventKind::Checkpoint,
                        timelineStats,
                        editor.streamingSaveStatus.last_snapshot_label,
                        "PersistentLevel",
                        editor.streamingSaveStatus.last_output_path));
                    epochengine::timeline::sort_events(editor.timelineEvents);
                    editor.lastCheckpointRecord = epochengine::saveload::make_checkpoint_record(
                        editor.streamingSaveConfig,
                        editor.streamingSaveStatus,
                        timelineStats,
                        0u,
                        editor.timelineEvents.size());
                    push_editor_log(editor, "[timeline] Auto-staged timeline checkpoint: " + editor.streamingSaveStatus.last_snapshot_label);
                }

                const std::array timelineSections{
                    gui::TabButtonSpec{
                        .id = "timeline.sequence",
                        .label = "Sequence",
                        .width = 104.0f,
                        .active = editor.timelineWorkspaceSection
                            == TimelineWorkspaceSection::sequence},
                    gui::TabButtonSpec{
                        .id = "timeline.checkpoints",
                        .label = "Checkpoints",
                        .width = 116.0f,
                        .active = editor.timelineWorkspaceSection
                            == TimelineWorkspaceSection::save_restore},
                    gui::TabButtonSpec{
                        .id = "timeline.media",
                        .label = "Media",
                        .width = 84.0f,
                        .active = editor.timelineWorkspaceSection
                            == TimelineWorkspaceSection::media},
                    gui::TabButtonSpec{
                        .id = "timeline.diagnostics",
                        .label = "Diagnostics",
                        .width = 112.0f,
                        .active = editor.timelineWorkspaceSection
                            == TimelineWorkspaceSection::diagnostics}
                };
                const auto section = gui::responsive_tab_bar_buttons(
                    gui::ResponsiveTabBarOptions{
                        .overflow_id = "timeline-workspace-overflow",
                        .overflow_label = "Timeline tools",
                        .tabs = timelineSections,
                        .available_width = centerWidth,
                        .overflow_width = 132.0f,
                        .height = 30.0f,
                        .gap = 2.0f,
                        .presentation = gui::TabBarPresentation::Workbench,
                        .keyboard_navigation = true});
                if (section.selected_index)
                {
                    editor.timelineWorkspaceSection =
                        static_cast<TimelineWorkspaceSection>(
                            *section.selected_index);
                }

                const auto stageTimelineCheckpoint = [&]()
                {
                    epochengine::saveload::mark_checkpoint_captured(
                        editor.streamingSaveStatus,
                        editor.streamingSaveConfig,
                        timelineStats);
                    editor.timelineEvents.push_back(
                        epochengine::timeline::make_event_from_stats(
                            "save",
                            epochengine::timeline::TimelineEventKind::Checkpoint,
                            timelineStats,
                            editor.streamingSaveStatus.last_snapshot_label,
                            "PersistentLevel",
                            editor.streamingSaveStatus.last_output_path));
                    epochengine::timeline::sort_events(editor.timelineEvents);
                    editor.lastCheckpointRecord =
                        epochengine::saveload::make_checkpoint_record(
                            editor.streamingSaveConfig,
                            editor.streamingSaveStatus,
                            timelineStats,
                            0u,
                            editor.timelineEvents.size());
                    push_editor_log(
                        editor,
                        "[timeline] Manual checkpoint evidence staged: "
                            + editor.streamingSaveStatus.last_snapshot_label);
                };

                if (editor.timelineWorkspaceSection
                    == TimelineWorkspaceSection::sequence)
                {
                    gui::label("Sequence");
                    gui::wrapped_label(
                        "The Timeline drives Epoch's shared simulation clock and visible event keys. Playback changes the editor playhead; Record Gate controls whether visible editor events may be added to the sequence.",
                        centerWidth);
                    gui::property_row(
                        "Playhead",
                        epochengine::format_text(
                            "{:.2f}s / {:.2f}s | frame {}",
                            editor.timelineState.playhead_seconds,
                            editor.timelineState.duration_seconds,
                            editor.timelineState.playhead_frame),
                        118.0f);
                    gui::property_row(
                        "Sequence",
                        epochengine::format_text(
                            "{} tracks | {} keys | {}",
                            epochengine::timeline::enabled_track_count(
                                editor.timelineTracks),
                            editor.timelineEvents.size(),
                            editor.timelineState.recording
                                ? "record gate armed"
                                : "record gate paused"),
                        118.0f);

                    render_timeline_time_controls(centerWidth, true);
                    const std::array sequenceActions{
                        gui::InlineButtonSpec{
                            .label = editor.timelineState.recording
                                ? "Stop Recording"
                                : "Arm Record Gate",
                            .width = 142.0f },
                        gui::InlineButtonSpec{
                            .label = "Add Checkpoint Key",
                            .width = 166.0f }
                    };
                    if (const auto action = gui::inline_button_row(
                            sequenceActions, 28.0f, 6.0f))
                    {
                        if (*action == 0u)
                        {
                            editor.timelineState.recording =
                                !editor.timelineState.recording;
                            push_editor_log(
                                editor,
                                editor.timelineState.recording
                                    ? "[timeline] Event recording gate armed."
                                    : "[timeline] Event recording gate paused.");
                        }
                        else
                        {
                            stageTimelineCheckpoint();
                        }
                    }
                    if (const auto* nextEvent =
                            epochengine::timeline::next_event_after(
                                editor.timelineEvents,
                                editor.timelineState.playhead_seconds))
                    {
                        gui::property_row(
                            "Next key",
                            epochengine::timeline::describe_event(*nextEvent),
                            118.0f);
                    }
                    else
                    {
                        gui::property_row("Next key", "(none)", 118.0f);
                    }
                    for (const auto& track : editor.timelineTracks)
                    {
                        gui::property_row(
                            track.label,
                            track.enabled ? "Enabled" : "Muted",
                            118.0f);
                    }
                }
                else if (editor.timelineWorkspaceSection
                    == TimelineWorkspaceSection::save_restore)
                {
                    gui::label("Checkpoint Staging");
                    gui::wrapped_label(
                        "Checkpoint keys currently stage reviewable timeline evidence. A complete scene serializer, writer, and restore reader are still required before Epoch can call these recoverable saves.",
                        centerWidth);
                    const auto activeSaveProfile =
                        epochengine::saveload::detect_streaming_save_profile(
                            editor.streamingSaveConfig);
                    gui::property_row(
                        "Mode",
                        std::string(epochengine::saveload::mode_name(
                            editor.streamingSaveConfig.mode)),
                        118.0f);
                    gui::property_row(
                        "Profile",
                        std::string(epochengine::saveload::stream_profile_name(
                            activeSaveProfile)),
                        118.0f);
                    gui::property_row(
                        "Retention",
                        epochengine::saveload::describe_retention(
                            editor.streamingSaveConfig),
                        118.0f);
                    gui::property_row(
                        "Last key",
                        editor.streamingSaveStatus.last_snapshot_label.empty()
                            ? std::string{"(none staged)"}
                            : editor.streamingSaveStatus.last_snapshot_label,
                        118.0f);
                    gui::property_row(
                        "Writer",
                        "Not connected; evidence remains staged locally",
                        118.0f);
                    gui::property_row(
                        "Restore",
                        "Unavailable until scene payload round-trip is verified",
                        118.0f);

                    const std::array streamButtons{
                        gui::InlineButtonSpec{
                            .label = editor.streamingSaveConfig.enabled
                                ? "Pause Stream"
                                : "Arm Stream",
                            .width = 118.0f },
                        gui::InlineButtonSpec{ .label = "Stage Key Now", .width = 116.0f },
                        gui::InlineButtonSpec{ .label = "Every 15s", .width = 92.0f },
                        gui::InlineButtonSpec{ .label = "Every 120f", .width = 96.0f },
                        gui::InlineButtonSpec{ .label = "On Timeline Key", .width = 128.0f }
                    };
                    if (const auto action = gui::inline_button_row(
                            streamButtons, 28.0f, 6.0f))
                    {
                        switch (*action)
                        {
                        case 0:
                            editor.streamingSaveConfig.enabled =
                                !editor.streamingSaveConfig.enabled;
                            editor.streamingSaveStatus.active =
                                editor.streamingSaveConfig.enabled;
                            editor.streamingSaveStatus.message =
                                editor.streamingSaveConfig.enabled
                                    ? "Checkpoint evidence staging is armed."
                                    : "Checkpoint evidence staging is paused.";
                            push_editor_log(
                                editor,
                                editor.streamingSaveConfig.enabled
                                    ? "[timeline] Checkpoint evidence staging armed."
                                    : "[timeline] Checkpoint evidence staging paused.");
                            break;
                        case 1:
                            stageTimelineCheckpoint();
                            break;
                        case 2:
                            epochengine::saveload::apply_streaming_save_profile(
                                editor.streamingSaveConfig,
                                epochengine::saveload::StreamingSaveProfile::
                                    EditorInterval15s);
                            push_editor_log(
                                editor,
                                "[timeline] Checkpoint cadence set to 15 seconds.");
                            break;
                        case 3:
                            epochengine::saveload::apply_streaming_save_profile(
                                editor.streamingSaveConfig,
                                epochengine::saveload::StreamingSaveProfile::
                                    EditorFrame120);
                            push_editor_log(
                                editor,
                                "[timeline] Checkpoint cadence set to 120 frames.");
                            break;
                        case 4:
                            epochengine::saveload::apply_streaming_save_profile(
                                editor.streamingSaveConfig,
                                epochengine::saveload::StreamingSaveProfile::
                                    TimelineKeyed);
                            push_editor_log(
                                editor,
                                "[timeline] Checkpoint cadence follows timeline keys.");
                            break;
                        default:
                            break;
                        }
                    }
                    gui::wrapped_label(
                        editor.streamingSaveStatus.message,
                        centerWidth);
                }
                else if (editor.timelineWorkspaceSection
                    == TimelineWorkspaceSection::media)
                {
                    gui::label("Media Preview");
                const auto mediaAvailability =
                    epochengine::media::timeline_preview::
                        presentation_availability(editor.mediaTimelinePreview);
                if (mediaAvailability
                    == epochengine::media::timeline_preview::
                        PresentationAvailability::no_source)
                {
                    gui::wrapped_label(
                        "No admitted media source. Add a verified project media asset to enable metadata and transport. The Video workspace never substitutes scene geometry or placeholder frames.",
                        centerWidth);
                    gui::property_row(
                        "[media] Source",
                        "(none admitted)",
                        132.0f);
                    gui::property_row(
                        "[media] Presentation",
                        "No pixels available",
                        132.0f);
                    gui::property_row(
                        "[media] Transport",
                        "Disabled until source admission",
                        132.0f);
                }
                else if (const auto& source =
                    editor.mediaTimelinePreview.source)
                {
                    gui::wrapped_label(
                        "Source metadata is admitted. Pixel presentation remains unavailable until a separately verified decoder frame provider is connected.",
                        centerWidth);
                    gui::property_row(
                        "[media] Source",
                        source->logical_path,
                        132.0f);
                    gui::property_row(
                        "[media] Provider",
                        source->provider_revision,
                        132.0f);
                    gui::property_row(
                        "[media] Presentation",
                        "Metadata only; no decoded frame",
                        132.0f);
                    gui::property_row(
                        "[media] Transport",
                        epochengine::media::timeline_preview::transport_enabled(
                                editor.mediaTimelinePreview)
                            ? "Metadata transport ready"
                            : "Disabled",
                        132.0f);
                }

                }
                else
                {
                    gui::label("Timeline Diagnostics");
                    gui::wrapped_label(
                        "Detailed simulation, lane-layout, and checkpoint-contract evidence for debugging. Normal sequencing and checkpoint controls remain in their dedicated sections.",
                        centerWidth);
                    render_timeline_time_controls(centerWidth, false);
                gui::property_row("[timeline] Frame", std::to_string(timelineStats.frame_index), 132.0f);
                gui::property_row("[timeline] Simulated", format_seconds(timelineStats.simulated_seconds), 132.0f);
                gui::property_row("[timeline] Fixed step", std::string(format_ms(timelineStats.fixed_dt_seconds)) + " / " + format_rate(timelineStats.fixed_dt_seconds), 132.0f);
                gui::property_row("[timeline] Step budget", std::to_string(timelineStats.step_budget), 132.0f);
                gui::property_row("[timeline] Time scale", epochengine::format_text("{:.2f}x", timelineStats.time_scale), 132.0f);
                gui::property_row("[timeline] Playhead", epochengine::format_text("{:.2f}s / frame {}", editor.timelineState.playhead_seconds, editor.timelineState.playhead_frame), 132.0f);
                gui::property_row("[timeline] Duration", epochengine::format_text("{:.2f}s", editor.timelineState.duration_seconds), 132.0f);
                gui::property_row("[timeline] Tracks", epochengine::format_text("{} enabled / {}", epochengine::timeline::enabled_track_count(editor.timelineTracks), editor.timelineTracks.size()), 132.0f);
                gui::property_row("[timeline] Keys", std::to_string(editor.timelineEvents.size()), 132.0f);
                const epochengine::timeline::TimelineViewConfig activeTimelineView{
                    .visible_start_seconds = (std::max)(0.0, editor.timelineState.playhead_seconds - 5.0),
                    .visible_duration_seconds = 10.0,
                    .pixel_width = (std::max)(320.0f, centerWidth)
                };
                const epochengine::timeline::TimelineLaneLayoutConfig activeLaneLayout{
                    .pixel_width = (std::max)(320.0f, centerWidth),
                    .header_width = 132.0,
                    .lane_height = 24.0,
                    .lane_gap = 4.0,
                    .top_padding = 0.0
                };
                const auto activeTimelineLanes = epochengine::timeline::make_lane_geometry(
                    editor.timelineTracks,
                    activeLaneLayout);
                const auto activeTimelineMarkers = epochengine::timeline::make_event_markers(
                    editor.timelineTracks,
                    editor.timelineEvents,
                    activeTimelineView,
                    activeLaneLayout,
                    editor.timelineState.duration_seconds);
                gui::property_row(
                    "[timeline] View",
                    epochengine::timeline::describe_view(
                        editor.timelineState,
                        editor.timelineTracks,
                        editor.timelineEvents,
                        activeTimelineView),
                    132.0f);
                gui::property_row(
                    "[timeline] Layout",
                    epochengine::timeline::describe_lane_layout(activeTimelineLanes, activeTimelineMarkers),
                    132.0f);
                const auto activeSaveProfile = epochengine::saveload::detect_streaming_save_profile(editor.streamingSaveConfig);
                if (const auto* profileDescriptor = epochengine::saveload::find_streaming_save_profile(activeSaveProfile))
                {
                    gui::property_row("[timeline] Profile id", std::string(profileDescriptor->id), 132.0f);
                    gui::property_row("[timeline] Profile detail", std::string(profileDescriptor->summary), 132.0f);
                }
                const auto timelineKeyedProfilePlan = epochengine::saveload::make_streaming_save_profile_change_plan(
                    editor.streamingSaveConfig,
                    epochengine::saveload::StreamingSaveProfile::TimelineKeyed);
                gui::property_row("[timeline] Stream mode", std::string(epochengine::saveload::mode_name(editor.streamingSaveConfig.mode)), 132.0f);
                gui::property_row("[timeline] Save profile", std::string(epochengine::saveload::stream_profile_name(activeSaveProfile)), 132.0f);
                gui::property_row("[timeline] Profile gate", epochengine::saveload::streaming_save_profile_change_summary(timelineKeyedProfilePlan), 132.0f);
                gui::property_row("[timeline] Retention", epochengine::saveload::describe_retention(editor.streamingSaveConfig), 132.0f);
                const auto activeSaveCadence = epochengine::saveload::make_streaming_save_cadence_plan(
                    editor.streamingSaveConfig,
                    editor.streamingSaveStatus,
                    timelineStats);
                gui::property_row("[timeline] Next capture", epochengine::saveload::streaming_save_cadence_summary(activeSaveCadence), 132.0f);
                gui::property_row("[timeline] Stream state", epochengine::saveload::describe_streaming_save(editor.streamingSaveConfig, editor.streamingSaveStatus), 132.0f);
                gui::property_row("[timeline] Last key", editor.streamingSaveStatus.last_snapshot_label.empty() ? std::string("(none staged)") : editor.streamingSaveStatus.last_snapshot_label, 132.0f);
                gui::property_row("[timeline] Last record", epochengine::saveload::checkpoint_record_summary(editor.lastCheckpointRecord), 132.0f);
                gui::property_row("[timeline] Target", editor.streamingSaveStatus.last_output_path.empty() ? editor.streamingSaveConfig.target_root : editor.streamingSaveStatus.last_output_path, 132.0f);
                gui::property_row("[timeline] Manifest", epochengine::saveload::join_stream_path(editor.streamingSaveConfig.target_root, "manifest.timeline.log"), 132.0f);
                if (editor.lastCheckpointRecord.valid)
                {
                    const auto restorePlan = epochengine::saveload::make_checkpoint_restore_plan(
                        editor.streamingSaveConfig,
                        editor.lastCheckpointRecord);
                    const std::array retentionRecords{
                        editor.lastCheckpointRecord
                    };
                    const auto retentionPlan = epochengine::saveload::make_checkpoint_retention_plan(
                        editor.streamingSaveConfig,
                        retentionRecords);
                    gui::property_row(
                        "[timeline] Writer gate",
                        epochengine::saveload::checkpoint_writer_approval_summary(epochengine::saveload::StreamingCheckpointWriteApproval{}),
                        132.0f);
                    gui::property_row(
                        "[timeline] Cleanup",
                        epochengine::saveload::checkpoint_retention_plan_summary(retentionPlan),
                        132.0f);
                    gui::property_row(
                        "[timeline] Scene payload",
                        restorePlan.scene_payload_path,
                        132.0f);
                    gui::property_row(
                        "[timeline] Restore",
                        epochengine::saveload::checkpoint_restore_plan_summary(restorePlan),
                        132.0f);
                }

                }
                break;
            }
#if EPOCH_EDITION_PUBLIC
            case EditorMainSurface::AISandbox:
            {
                gui::wrapped_label("Ask about your project, plan a scene change, or choose a model connection. Engine source tools are available in the Development edition.", centerWidth);
                if (gui::inline_button_row(std::array{gui::InlineButtonSpec{.label="Model Manager"}},
                        30.0f, 6.0f, gui::InlineButtonRowOptions{
                            .sizing=gui::InlineButtonSizing::FitContent,
                            .alignment=gui::InlineButtonAlignment::Center,
                            .wrap=true, .available_width=centerWidth}))
                    editor.showAiModelManagerModal = true;
                if (editor.taskScheduler)
                {
                    const auto activity = editor.taskScheduler->activity();
                    gui::property_row("Tasks", std::to_string(activity.running_tasks) + " running / "
                        + std::to_string(activity.queued_tasks + activity.waiting_tasks) + " queued");
                }
                break;
            }
#endif
            case EditorMainSurface::Systems:
            {
                if (!editor.systems.panel)
                    editor.systems.panel = std::make_unique<editor_systems_panel::Panel>();
                auto& registry = epochengine::systems::Registry::instance();
                const std::size_t hardwareThreadCount = (std::max)(
                    std::size_t{1},
                    std::thread::hardware_concurrency() > 0
                        ? static_cast<std::size_t>(std::thread::hardware_concurrency())
                        : std::size_t{6});
                const auto& projectProfile = active_project_profile(editor);
                const auto editorBackend = renderer_backend_kind(ctx);
                const auto runBackend = renderer_backend_kind(editor.projectRunBackend);
                editor.systems.panel->render(
                    centerWidth,
                    registry,
                    editor_systems_panel::Snapshot{
                        .renderer = renderer_name(ctx),
                        .capability = renderer_capability_profile_summary(ctx),
                        .editor_admission = renderer_admission_summary(
                            editorBackend,
                            projectProfile.renderer_capability),
                        .project_admission = renderer_admission_summary(
                            runBackend,
                            projectProfile.renderer_capability),
                        .resource_spine = renderer_resource_spine_summary(ctx),
                        .proof_stages = renderer_capability_proof_stage_status(ctx),
                        .rtt_overall = renderer_sampled_rtt_rollup_status(ctx),
                        .rtt_presentation = renderer_sampled_rtt_presentation_status(ctx),
                        .mesh_model = renderer_native_mesh_model_status(ctx),
                        .next_gate = renderer_next_feature_gate(ctx),
                        .backend_guidance = backend_runtime_guidance(
                            ctx,
                            recommended_support_tier(ctx, hardwareThreadCount)),
                        .convergence_focus = backend_convergence_focus(ctx),
                        .support_tier = recommended_support_tier(
                            ctx,
                            hardwareThreadCount),
                        .project_build = editor.projectBuildPending
                            ? std::string{"Running"}
                            : editor.projectBuildStatus,
                        .script_build = editor.scriptBuildPending
                            ? std::string{"Running"}
                            : editor.scriptBuildStatus,
                        .live_threads = epochengine::systems::threading::live_thread_count(),
                        .hardware_threads = hardwareThreadCount
                    },
                    editor.taskScheduler ? &editor.taskScheduler->graph() : nullptr,
                    "Editor Project and Script Tasks");
                break;
            }
            case EditorMainSurface::Game2D:
            {
                if (!guiDocumentWorkspace)
                    break;
                const bool dedicatedGuiEditor =
                    editor.applicationKind
                        == EditorApplicationKind::GuiEditor;
                if (!dedicatedGuiEditor)
                {
                    editor.guiDocumentTab = 0u;
                    gui::label("Project GUI Placement");
                    const gui::Vec2 documentPosition =
                        gui::cursor_position();
                    const gui::Vec2 documentSize{
                        centerWidth,
                        (std::max)(1.0f, centerScrollStart.y + centerScrollHeight
                            - documentPosition.y - 4.0f)};
                    render_gui_document_canvas(
                        editor,
                        documentPosition,
                        documentSize,
                        true);
                }
                else
                {
                    const auto layout =
                        editor_workspace::make_workspace_layout(
                            editor_workspace::ApplicationKind::gui_editor);
                    constexpr std::array roles{
                        editor_workspace::ContentRole::gui_canvas,
                        editor_workspace::ContentRole::gui_runtime_preview,
                        editor_workspace::ContentRole::gui_component_graph,
                        editor_workspace::ContentRole::gui_style_document
                    };
                    constexpr std::array widths{
                        82.0f, 132.0f, 132.0f, 78.0f};
                    std::array<gui::TabButtonSpec, roles.size()> tabs{};
                    for (std::size_t index = 0u;
                        index < roles.size();
                        ++index)
                    {
                        if (const auto* tab =
                                layout.find_content(roles[index]))
                        {
                            tabs[index] = {
                                .id = tab->canonical_name,
                                .label = tab->label,
                                .width = widths[index],
                                .active = editor.guiDocumentTab == index
                            };
                        }
                    }
                    if (const auto selected =
                            gui::tab_bar_buttons(tabs);
                        selected.selected_index)
                    {
                        editor.guiDocumentTab =
                            *selected.selected_index;
                    }

                    const gui::Vec2 documentPosition =
                        gui::cursor_position();
                    const gui::Vec2 documentSize{
                        centerWidth,
                        (std::max)(1.0f, centerScrollStart.y + centerScrollHeight
                            - documentPosition.y - 4.0f)};
                    if (editor.guiDocumentTab < 2u)
                    {
                        render_gui_document_canvas(
                            editor,
                            documentPosition,
                            documentSize,
                            editor.guiDocumentTab == 0u);
                    }
                    else if (auto* document =
                            synchronize_gui_document(editor))
                    {
                        const auto widgets = document->widgets();
                        if (editor.guiDocumentTab == 2u)
                        {
                            for (const auto& widget : widgets)
                            {
                                if (widget.handle
                                    == editor.guiDocumentRoot)
                                {
                                    continue;
                                }
                                if (gui::button_selected(
                                        widget.descriptor.name,
                                        {centerWidth, 30.0f},
                                        gui_widget_selected(
                                            editor,
                                            widget.handle)))
                                {
                                    select_gui_widget(
                                        editor,
                                        widget.handle);
                                }
                            }
                        }
                        else
                        {
                            for (const auto& widget : widgets)
                            {
                                if (widget.handle
                                    == editor.guiDocumentRoot)
                                {
                                    continue;
                                }
                                gui::property_row(
                                    widget.descriptor.name,
                                    widget.descriptor.style.style_class.empty()
                                        ? gui_widget_display_text(widget)
                                        : widget.descriptor.style.style_class,
                                    180.0f);
                            }
                        }
                    }
                }                break;
            }
            case EditorMainSurface::Scene:
            default:
                break;
            }

            gui::end_scroll_area();
        }
        gui::end_window();

        constexpr float toolGroupTabHeight = 38.0f;
        auto tool_group_position = [&](EditorToolDockRegion region) noexcept
        {
            switch (region)
            {
            case EditorToolDockRegion::Right:
                return details_pos;
            case EditorToolDockRegion::BottomLeft:
                return bottomLeftPosition;
            case EditorToolDockRegion::BottomRight:
                return bottomRightPosition;
            case EditorToolDockRegion::Left:
            default:
                return outliner_pos;
            }
        };
        auto tool_group_size = [&](EditorToolDockRegion region) noexcept
        {
            switch (region)
            {
            case EditorToolDockRegion::Right:
                return details_size;
            case EditorToolDockRegion::BottomLeft:
                return bottomLeftSize;
            case EditorToolDockRegion::BottomRight:
                return bottomRightSize;
            case EditorToolDockRegion::Left:
            default:
                return outliner_size;
            }
        };
        auto tool_group_content_position = [&](EditorToolDockRegion region) noexcept
        {
            const gui::Vec2 position = tool_group_position(region);
            return gui::Vec2{ position.x, position.y + toolGroupTabHeight };
        };
        auto tool_group_content_size = [&](EditorToolDockRegion region) noexcept
        {
            const gui::Vec2 size = tool_group_size(region);
            return gui::Vec2{
                size.x,
                (std::max)(0.0f, size.y - toolGroupTabHeight) };
        };
        auto tool_pane_label = [&](std::string_view route) noexcept
        {
            for (std::size_t i = 0; i < toolPaneRoutes.size(); ++i)
                if (toolPaneRoutes[i] == route)
                    return toolPaneLabels[i];
            return std::string_view{ "Tool" };
        };
        auto render_tool_group_tabs = [&](EditorToolDockRegion region)
        {
            if (!region_has_visible_pane(region))
                return;

            std::vector<gui::TabButtonSpec> tabs{};
            std::vector<std::string_view> routes{};
            const std::string_view active = active_pane_for_region(region);
            for (const std::size_t i : ordered_pane_indices_for_region(region))
            {
                const std::string_view route = toolPaneRoutes[i];
                routes.push_back(route);
                tabs.push_back(gui::TabButtonSpec{
                    .id = route,
                    .label = toolPaneLabels[i],
                    .width = 0.0f,
                    .active = active == route,
                    .closable = true,
                    .dirty = false,
                    .enabled = true
                });
            }
            if (tabs.empty())
                return;

            const gui::Vec2 position = tool_group_position(region);
            const gui::Vec2 size = tool_group_size(region);
            gui::begin_window(
                {},
                position,
                { size.x, toolGroupTabHeight });
            gui::set_cursor({ position.x + 3.0f, position.y + 2.0f });
            const auto tabResult =
                gui::tab_bar_buttons(tabs, toolGroupTabHeight - 3.0f, 1.0f,
                    gui::TabBarPresentation::Document, size);
            const std::size_t regionIndex = tool_dock_region_index(region);
            directTabRoutes[regionIndex] = routes;
            directTabBounds[regionIndex].clear();
            directTabBounds[regionIndex].reserve(tabResult.items.size());
            for (const auto& item : tabResult.items)
            {
                directTabBounds[regionIndex].push_back({
                    {item.position.x, item.position.y},
                    {item.size.x, item.size.y}
                });
            }
            directTabStripBounds[regionIndex] = {
                {position.x + 3.0f, position.y + 2.0f},
                {(std::max)(1.0f, size.x - 6.0f), toolGroupTabHeight - 3.0f}
            };
            const std::optional<std::size_t> activationIndex =
                tabResult.pressed_index
                    ? tabResult.pressed_index
                    : tabResult.selected_index;
            if (activationIndex && *activationIndex < routes.size())
            {
                set_active_pane(region, routes[*activationIndex]);
                result.scene_input_captured = true;
            }
            if (tabResult.closed_index
                && *tabResult.closed_index < routes.size())
            {
                const std::string_view closed = routes[*tabResult.closed_index];
                set_pane_shown(closed, false);
                push_editor_log(
                    editor,
                    "[window] " + std::string(tool_pane_label(closed))
                    + " hidden. Reopen it from Window.");
                result.scene_input_captured = true;
            }
            if (tabResult.pressed_index
                && *tabResult.pressed_index < routes.size())
            {
                const std::string_view route =
                    routes[*tabResult.pressed_index];
                editor.paneTitleDragActive = true;
                editor.paneTitleDragRoute.assign(route);
                editor.paneTitleDragLabel.assign(tool_pane_label(route));
                editor.paneTitleDragStart = mouse;
                result.scene_input_captured = true;
            }
            gui::end_window();
        };

        render_tool_group_tabs(EditorToolDockRegion::Left);
        render_tool_group_tabs(EditorToolDockRegion::Right);
        render_tool_group_tabs(EditorToolDockRegion::BottomLeft);
        render_tool_group_tabs(EditorToolDockRegion::BottomRight);

        const auto render_structural_tool_pane =
            [&](EditorToolDockRegion region)
        {
            const std::string_view active = active_pane_for_region(region);
            const auto pane = editor_tool_pane_from_route(active);
            if (!pane)
                return;
            const gui::Vec2 position = tool_group_content_position(region);
            const gui::Vec2 size = tool_group_content_size(region);
            const std::string_view label =
                toolPaneLabels[tool_pane_index(*pane)];
            switch (*pane)
            {
            case EditorToolPane::WorldOutliner:
                render_outliner_window(
                    position, size, OutlinerToolTab::World, active, label);
                break;
            case EditorToolPane::AssetBrowser:
                render_outliner_window(
                    position, size, OutlinerToolTab::Assets, active, label);
                break;
            case EditorToolPane::GuiHierarchy:
                render_outliner_window(
                    position, size, OutlinerToolTab::Gui, active, label);
                break;
            case EditorToolPane::ScriptBrowser:
                render_outliner_window(
                    position, size, OutlinerToolTab::Scripting, active, label);
                break;
            case EditorToolPane::TileMap:
                render_outliner_window(
                    position, size, OutlinerToolTab::TileMap, active, label);
                break;
            case EditorToolPane::Properties:
                render_inspector_window(
                    position, size, InspectorToolTab::Properties, active, label);
                break;
            case EditorToolPane::WorldSettings:
                render_inspector_window(
                    position, size, InspectorToolTab::WorldSettings, active, label);
                break;
            case EditorToolPane::AiControls:
                break; // Retired persistence slot; no separate settings surface.
            case EditorToolPane::AiDevelopment:
                render_inspector_window(
                    position, size, InspectorToolTab::AiDevelopment, active, label);
                break;
            default:
                break;
            }
        };
        render_structural_tool_pane(EditorToolDockRegion::Left);
        render_structural_tool_pane(EditorToolDockRegion::Right);
        render_structural_tool_pane(EditorToolDockRegion::BottomLeft);
        render_structural_tool_pane(EditorToolDockRegion::BottomRight);

        const bool outlinerSplitHovered = layout_outliner_visible && editor_point_in_rect(mouse, outliner_split_pos, outliner_split_size);
        const bool inspectorSplitHovered = layout_inspector_visible && editor_point_in_rect(mouse, inspector_split_pos, inspector_split_size);
        const bool bottomSplitHovered = bottom_visible && editor_point_in_rect(mouse, bottom_split_pos, bottom_split_size);
        const bool bottomColumnsSplitHovered = splitBottomColumns && editor_point_in_rect(mouse, bottomColumnsSplitPosition, bottomColumnsSplitSize);
