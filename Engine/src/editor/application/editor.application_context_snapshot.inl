// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
    struct EditorPlantLabPreview final
    {
        forest::ForestAssetDocument document{};
        forest::CompiledForestAsset compiled{};
    };

    EditorContextSnapshot editor_capture_context_snapshot(const core::Context* ctx)
    {
        EditorContextSnapshot snapshot{};
        if (!ctx)
            return snapshot;

        auto& storage = editor_storage();
        std::scoped_lock lock(storage.mutex);
        const auto it = storage.states.find(ctx);
        if (it == storage.states.end())
            return snapshot;

        const EditorState& editor = it->second;
        snapshot.valid = true;
        if (editor.plantLabDocumentInitialized && editor.plantLabCompiled.valid
            && editor.plantLabCompiled.sourceRevision == editor.plantLabDocument.revision
            && editor.plantLabCompiled.sourceContentHash == editor.plantLabDocument.contentHash)
            snapshot.plant_lab_preview = std::make_shared<const EditorPlantLabPreview>(
                EditorPlantLabPreview{editor.plantLabDocument, editor.plantLabCompiled});
        snapshot.project_id = editor.projectId;
        snapshot.project_name = editor.projectName;
        snapshot.project_root = editor.projectRoot;
        snapshot.project_scene_path = editor.projectScenePath;
        snapshot.project_manifest = editor.projectManifest;
        snapshot.project_template = editor.projectTemplate;
        snapshot.project_kind = editor.projectKind;
        snapshot.active_script = editor.activeScript;
        snapshot.active_runtime_scene = editor.activeRuntimeScene;
        snapshot.active_world = editor.activeWorld;
        snapshot.project_status = editor.projectStatus;
        snapshot.project_build_status = editor.projectBuildStatus;
        snapshot.script_build_status = editor.scriptBuildStatus;
        snapshot.script_editor_path = editor.scriptEditorPath;
        snapshot.script_editor_text = editor.scriptEditorText;
        snapshot.script_editor_status = editor.scriptEditorStatus;
        snapshot.script_editor_dirty = editor.scriptEditorDirty;
        snapshot.project_run_backend = editor.projectRunBackend;
        snapshot.project_run_frame_limit_fps = snapshot_frame_limit(editor.projectRunFrameLimitFps, 60.0);
        snapshot.canvas2d_project = sanitize_canvas2d_project(editor.canvas2dProject);
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
        snapshot.tile_map_tile_extent = editor.tileMapTileExtent;
        if (editor.tileMapWorkspace
            && editor.tileMapWorkspace->has_document())
        {
            auto serialized = editor.tileMapWorkspace->document()->serialize();
            if (!serialized)
            {
                snapshot.valid = false;
                return snapshot;
            }
            snapshot.tile_map_document = std::move(serialized.bytes);
            snapshot.tile_map_dirty = editor.tileMapWorkspace->dirty();
            const auto& tileMapView = editor.tileMapWorkspace->ui_state();
            snapshot.tile_map_tool = static_cast<std::uint8_t>(tileMapView.tool);
            snapshot.tile_map_selected_palette = tileMapView.selected_palette;
            snapshot.tile_map_selected_layer = tileMapView.selected_layer;
            snapshot.tile_map_zoom = tileMapView.zoom;
            snapshot.tile_map_pan_x = tileMapView.pan.x;
            snapshot.tile_map_pan_y = tileMapView.pan.y;
        }
#endif
        snapshot.project_camera_mode = static_cast<std::uint8_t>(editor.projectCameraMode);
        snapshot.input_profile_preset = static_cast<std::uint8_t>(editor.inputProfilePreset);
        snapshot.theme_preference = snapshot_theme_preference(editor.themePreference);
        snapshot.theme_cycle_interval_seconds = editor.themeCycleIntervalSeconds;
        snapshot.rounded_rectangles = editor.roundedRectangles;
        snapshot.editor_frame_limit_fps = snapshot_frame_limit(editor.editorFrameLimitFps, 120.0);
        snapshot.editor_frame_vsync_requested = editor.editorFrameVsyncRequested;
        snapshot.selected_project_file = editor.selectedProjectFile;
        snapshot.selected_asset_path = editor.selectedAssetPath;
        snapshot.entities.reserve(editor.entities.size());
        for (const EditorEntity& entity : editor.entities)
            snapshot.entities.emplace_back(capture_snapshot_entity(entity));
        snapshot.selected_entity = editor.selectedEntity;
        snapshot.selected_entity_id = editor.selectedEntityId;
        snapshot.selected_entity_ids.assign(
            editor.selectedEntityIds.begin(),
            editor.selectedEntityIds.end());
        snapshot.log_lines = editor.logLines;
        snapshot.helpers_visible = editor.helpersVisible;
        snapshot.time_snapshot = editor.timeSnapshot;
        snapshot.scene_document = capture_editor_scene_projection(editor);
        snapshot.time_control = snapshot_time_control(editor.timeControl);
        snapshot.preview_mode = snapshot_preview_mode(editor.previewMode);
        snapshot.workspace_tab = snapshot_workspace_tab(editor.workspaceTab);
        snapshot.dock_status_tab = snapshot_workspace_tab(editor.dockStatusTab);
        snapshot.main_surface = static_cast<std::uint8_t>(editor.mainSurface);
        snapshot.game2d_gui_workspace = editor.game2DGuiWorkspace;
        snapshot.bottom_grid_split = gui_lib::normalize_bottom_dock_column_fraction(editor.bottomGridSplit);
        snapshot.outliner_split = snapshot_layout_split(editor.outlinerSplit, 0.20f);
        snapshot.inspector_split = snapshot_layout_split(editor.inspectorSplit, 0.33f);
        snapshot.dock_split = snapshot_layout_split(editor.dockSplit, 0.28f);
        snapshot.workspace_bottom_grid_splits = editor.workspaceBottomGridSplits;
        snapshot.workspace_dock_splits = editor.workspaceDockSplits;
        snapshot.workspace_output_follow = editor.workspaceOutputFollow;
        snapshot.workspace_ai_chat_follow = editor.workspaceAiChatFollow;
        const std::size_t activeSurface = editor_main_surface_index(
            editor.mainSurface);
        snapshot.workspace_bottom_grid_splits[activeSurface] =
            snapshot.bottom_grid_split;
        snapshot.workspace_dock_splits[activeSurface] = snapshot.dock_split;
        snapshot.workspace_output_follow[activeSurface] =
            editor.outputFollowTail;
        snapshot.workspace_ai_chat_follow[activeSurface] =
            editor.aiChatFollowTail;
        snapshot.show_outliner = editor.showOutliner;
        snapshot.show_inspector = editor.showInspector;
        snapshot.show_console_dock = editor.showConsoleDock;
        snapshot.show_ai_chat = editor.showAiChat;
        constexpr auto toolPaneRoutes = editor_tool_pane_routes();
        snapshot.tool_panes.reserve(toolPaneRoutes.size());
        for (std::size_t index = 0; index < toolPaneRoutes.size(); ++index)
        {
            snapshot.tool_panes.push_back(EditorToolPaneSnapshot{
                .route = std::string(toolPaneRoutes[index]),
                .open = editor.toolPaneOpen[index],
                .dock_region = static_cast<std::uint8_t>(
                    editor.toolPaneDockRegions[index]),
                .tab_order = editor.toolPaneTabOrder[index]
            });
        }
        snapshot.active_left_pane_route = editor.activeLeftPaneRoute;
        snapshot.active_right_pane_route = editor.activeRightPaneRoute;
        snapshot.active_bottom_left_pane_route = editor.activeBottomLeftPaneRoute;
        snapshot.active_bottom_right_pane_route = editor.activeBottomRightPaneRoute;
        snapshot.project_notes_visible = editor.projectNotesVisible;
        snapshot.ai_workspace_domain = static_cast<std::uint8_t>(editor.aiWorkspaceDomain);
        snapshot.application_kind = editor.applicationKind;
        snapshot.systems_render_zoom = finite_or(editor.systems.renderZoom, 1.15f);
        snapshot.systems_task_zoom = finite_or(editor.systems.taskZoom, 1.15f);
        snapshot.systems_render_pan = editor.systems.renderPan;
        snapshot.systems_task_pan = editor.systems.taskPan;
        snapshot.camera = previewgrid::capture_camera_rig_snapshot(ctx);
        return snapshot;
    }

    bool editor_restore_plant_lab_preview(
        const std::shared_ptr<core::Context>& ctx,
        const EditorContextSnapshot& snapshot)
    {
        if (!ctx || !snapshot.valid || !snapshot.plant_lab_preview)
            return false;
        const auto& preview = *snapshot.plant_lab_preview;
        if (!preview.compiled.valid
            || preview.compiled.sourceRevision != preview.document.revision
            || preview.compiled.sourceContentHash != preview.document.contentHash)
            return false;
        // Called only for the newly-created application context. Never apply
        // the source project's full snapshot or change its current application.
        editor_load_application(ctx, EditorApplicationKind::PlantLab);
        auto& editor = editor_state_for(ctx);
        if (editor.applicationKind != EditorApplicationKind::PlantLab)
            return false;
        editor.plantLabDocument = preview.document;
        editor.plantLabCompiled = preview.compiled;
        editor.plantLabCompiledRevision = preview.document.revision;
        editor.plantLabCompiledContentHash = preview.document.contentHash;
        editor.plantLabDocumentInitialized = true;
        ensure_plant_lab_preview_entities(editor);
        push_editor_log(editor, "[plant] Opened the selected compiled plant in a separate Plant Lab application.");
        return true;
    }

    bool editor_restore_context_snapshot(core::Context* ctx, const EditorContextSnapshot& snapshot)
    {
        if (!ctx || !snapshot.valid)
            return false;

        auto& storage = editor_storage();
        std::scoped_lock lock(storage.mutex);
        EditorState& editor = storage.states[ctx];
        editor.initialized = true;
        editor.openMenu = TopMenu::None;
        editor.projectId = snapshot.project_id;
        if (!editor.projectId.empty())
            editor.projectPickerId = editor.projectId;
        editor.projectName = snapshot.project_name;
        editor.projectRoot = snapshot.project_root;
        editor.projectScenePath = snapshot.project_scene_path;
        editor.projectManifest = snapshot.project_manifest;
        editor.projectTemplate = snapshot.project_template;
        editor.projectKind = snapshot.project_kind;
        editor.activeScript = snapshot.active_script;
        editor.activeRuntimeScene = snapshot.active_runtime_scene;
        editor.activeWorld = snapshot.active_world;
        editor.projectStatus = snapshot.project_status;
        editor.projectBuildStatus = snapshot.project_build_status;
        editor.scriptBuildStatus = snapshot.script_build_status;
        editor.scriptEditorPath = snapshot.script_editor_path;
        editor.scriptEditorText = snapshot.script_editor_text;
        editor.scriptEditorStatus = snapshot.script_editor_status;
        editor.scriptEditorDirty = snapshot.script_editor_dirty;
        editor.projectBuildPending.reset();
        editor.projectBuildRunAfterBuild = false;
        editor.projectBuildRunScene.clear();
        editor.projectBuildOutputPath.clear();
        editor.projectBuildRunBackend.clear();
        editor.projectRunBackend = snapshot.project_run_backend.empty() ? std::string{ "opengl" } : snapshot.project_run_backend;
        editor.projectRunFrameLimitFps = snapshot_frame_limit(snapshot.project_run_frame_limit_fps, 60.0);
        editor.canvas2dProject = sanitize_canvas2d_project(snapshot.canvas2d_project);
        editor.projectCameraMode = snapshot_camera_mode(snapshot.project_camera_mode);
        editor.viewportCameraMode = snapshot.camera.valid
            ? snapshot.camera.mode
            : application_camera_mode(
                editor_application_profile(snapshot.application_kind).camera_policy);
        editor.projectBuildRunCameraMode = editor.projectCameraMode;
        editor.inputProfilePreset = snapshot_input_profile(snapshot.input_profile_preset);
        editor.projectBuildRunInputProfile = editor.inputProfilePreset;
        editor.themePreference = snapshot_theme_preference(snapshot.theme_preference);
        editor.themeCycleIntervalSeconds = gui::normalize_theme_cycle_interval(
            snapshot.theme_cycle_interval_seconds);
        editor.roundedRectangles = snapshot.rounded_rectangles;
        editor.editorFrameLimitFps = snapshot_frame_limit(snapshot.editor_frame_limit_fps, 120.0);
        editor.editorFrameVsyncRequested = snapshot.editor_frame_vsync_requested;
        if (ctx->windowData)
            ctx->windowData->editorFramePacingOverride.publish(
                editor.editorFrameVsyncRequested ? perf::frame_pacing_mode::vsync
                    : editor.editorFrameLimitFps > 0.0 ? perf::frame_pacing_mode::target_hz
                    : perf::frame_pacing_mode::uncapped,
                editor.editorFrameLimitFps);
        editor.selectedProjectFile = snapshot.selected_project_file;
        editor.selectedAssetPath = snapshot.selected_asset_path;
        editor.projectTextures.reset();
        editor.projectTextureStatus = "Texture Library will reopen exact scene revisions on demand.";
        editor.guiDocument.reset();
        editor.guiDocumentRoot = {};
        editor.guiDocumentWidgets.clear();
        editor.guiCanvasWidgetDrag = {};
        editor.guiCanvasWidgetDragHandle = {};
        editor.guiProjectionTombstones.clear();
        editor.guiDocumentLoadAttempted = false;
        editor.guiDocumentSavedRevision = 0u;
        editor.guiDocumentSourcePath.clear();
        editor.guiDocumentStatus = editor.projectId.empty()
            ? "No project GUI document is open."
            : "GUI document will reopen from the restored project.";
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
        editor.tileMapWorkspace.reset();
        editor.tileMapStatus = "Tile-map workspace will reopen on demand.";
        switch (snapshot.tile_map_tile_extent)
        {
        case 16u:
        case 32u:
        case 64u:
            editor.tileMapTileExtent = snapshot.tile_map_tile_extent;
            break;
        default:
            editor.tileMapTileExtent = 32u;
            break;
        }
        if (!snapshot.tile_map_document.empty())
        {
            auto* tileMap = tilemap_workspace_controller(editor);
            if (!tileMap
                || !tileMap->restore_context_handoff(
                    snapshot.tile_map_document,
                    snapshot.tile_map_dirty))
            {
                return false;
            }
            tileMap->restore_view_state(
                snapshot.tile_map_tool,
                snapshot.tile_map_selected_palette,
                snapshot.tile_map_selected_layer,
                snapshot.tile_map_zoom,
                {snapshot.tile_map_pan_x, snapshot.tile_map_pan_y});
            editor.tileMapStatus = std::string(tileMap->status());
        }
#endif
        editor.entities.clear();
        editor.entities.reserve(snapshot.entities.size());
        for (const EditorContextSnapshotEntity& entity : snapshot.entities)
            editor.entities.emplace_back(restore_snapshot_entity(entity));
        if (!snapshot.scene_document.scene_id.empty())
        {
            (void)rebuild_editor_scene_document_from_snapshot(
                editor,
                snapshot.scene_document,
                "Restore context scene document");
        }
        else
        {
            (void)rebuild_editor_scene_document(
                editor,
                "Restore context scene mirror");
        }
        editor.projectLifecycle.clear_project();
        if (!editor.projectId.empty())
        {
            const std::uint64_t projectKey =
                project_assets::stable_project_identity(editor.projectId);
            if (editor.projectLifecycle.select_project(projectKey))
            {
                (void)editor.projectLifecycle.record_scene_commit(
                    editor.sceneDocumentRevision);
            }
        }
        normalize_editor_entity_ids(editor);
        editor.selectedEntityIds.assign(
            snapshot.selected_entity_ids.begin(),
            snapshot.selected_entity_ids.end());
        editor.selectedEntityId = snapshot.selected_entity_id;
        if (editor.selectedEntityIds.empty()
            && editor.selectedEntityId != scene::kInvalidSceneObjectId)
        {
            editor.selectedEntityIds.push_back(editor.selectedEntityId);
        }
        if (const auto selected = editor_entity_index(editor, editor.selectedEntityId))
        {
            editor.selectedEntity = *selected;
            synchronize_editor_selection(editor);
        }
        else
            select_editor_entity(editor, snapshot.selected_entity);
        editor.sceneDragEntityId = scene::kInvalidSceneObjectId;
        editor.logLines = snapshot.log_lines;
        editor.helpersVisible = snapshot.helpers_visible;
        editor.applicationKind = snapshot.application_kind;
        const auto& application = editor_application_profile(editor.applicationKind);
        editor.sceneLightingDirty = true;
        editor.sceneInteractionDirty = true;
        editor.sceneDragActive = false;
        editor.sceneLeftWasHeld = false;
        editor.timeSnapshot = snapshot.time_snapshot;
        editor.timeControl = snapshot_time_control(snapshot.time_control);
        editor.previewMode = snapshot_preview_mode(snapshot.preview_mode);
        editor.workspaceTab = snapshot_workspace_tab(snapshot.workspace_tab);
        editor.dockStatusTab = snapshot_workspace_tab(snapshot.dock_status_tab);
        editor.mainSurface = snapshot_main_surface(snapshot.main_surface);
        editor.game2DGuiWorkspace = snapshot.game2d_gui_workspace
            || editor_application_owns_dedicated_gui_workspace(editor.applicationKind);
        if (!editor_application_supports_surface(application, editor.mainSurface))
            editor.mainSurface = application.default_surface;
        // A native-context restore retains the user's selected/closed panes.
        // Companions are activated on the next explicit workspace selection.
        editor.workspaceCompanionsPending = false;
        editor.bottomGridSplit = gui_lib::normalize_bottom_dock_column_fraction(snapshot.bottom_grid_split);
        editor.outlinerSplit = snapshot_layout_split(snapshot.outliner_split, 0.20f);
        editor.inspectorSplit = snapshot_layout_split(snapshot.inspector_split, 0.33f);
        editor.dockSplit = snapshot_layout_split(snapshot.dock_split, 0.28f);
        for (std::size_t index = 0u; index < kEditorMainSurfaceCount; ++index)
        {
            editor.workspaceBottomGridSplits[index] = gui_lib::normalize_bottom_dock_column_fraction(
                snapshot.workspace_bottom_grid_splits[index]);
            editor.workspaceDockSplits[index] = snapshot_layout_split(
                snapshot.workspace_dock_splits[index], 0.28f);
            editor.workspaceOutputFollow[index] =
                snapshot.workspace_output_follow[index];
            editor.workspaceAiChatFollow[index] =
                snapshot.workspace_ai_chat_follow[index];
        }
        const std::size_t activeSurface = editor_main_surface_index(
            editor.mainSurface);
        editor.workspaceBottomGridSplits[activeSurface] = editor.bottomGridSplit;
        editor.workspaceDockSplits[activeSurface] = editor.dockSplit;
        editor.outputFollowTail = editor.workspaceOutputFollow[activeSurface];
        editor.aiChatFollowTail = editor.workspaceAiChatFollow[activeSurface];
        editor.dockPreferenceSurface = editor.mainSurface;
        editor.showOutliner = snapshot.show_outliner && application.panes.outliner;
        editor.showInspector = snapshot.show_inspector && application.panes.inspector;
        editor.showConsoleDock = snapshot.show_console_dock && application.panes.console;
        editor.showAiChat = snapshot.show_ai_chat && application.panes.ai_chat;
        editor.toolPaneOpen = default_tool_pane_open_state();
        editor.toolPaneDockRegions = default_tool_pane_regions();
        editor.toolPaneTabOrder = default_tool_pane_tab_order();
        for (const auto& pane : snapshot.tool_panes)
        {
            const bool legacyOutputFilterRoute =
                pane.route == "pane.project_status"
                || pane.route == "pane.asset_status"
                || pane.route == "pane.ai_status"
                || pane.route == "pane.systems_status";
            if (legacyOutputFilterRoute)
                continue;

            const auto id = editor_tool_pane_from_route(pane.route);
            if (!id)
                continue;
            const std::size_t index = tool_pane_index(*id);
            editor.toolPaneOpen[index] = pane.open;
            const std::uint8_t boundedRegion =
                (std::min)(pane.dock_region, std::uint8_t{3u});
            editor.toolPaneDockRegions[index] =
                static_cast<EditorToolDockRegion>(boundedRegion);
            editor.toolPaneTabOrder[index] = pane.tab_order < kEditorToolPaneCount
                ? pane.tab_order
                : static_cast<std::uint32_t>(index);
            // A snapshot is an operator-saved layout. Preserve its dock region
            // instead of forcing chat/settings back to defaults on restore.
        }
        auto canonicalPaneRoute = [](std::string_view route) -> std::string
        {
            const auto pane = editor_tool_pane_from_route(route);
            if (route == "pane.ai_controls") return "pane.world_outliner";
            return pane
                ? std::string(
                    editor_tool_pane_routes()[tool_pane_index(*pane)])
                : std::string(route);
        };
        editor.activeLeftPaneRoute = snapshot.active_left_pane_route.empty()
            ? "pane.world_outliner"
            : canonicalPaneRoute(snapshot.active_left_pane_route);
        // A previously saved right-side AI Settings active tab must not
        // override the new left dock placement on restore.
        if (editor.activeLeftPaneRoute == "pane.ai_chat" || editor.activeLeftPaneRoute == "pane.ai_controls")
            editor.activeLeftPaneRoute = "pane.world_outliner";
        editor.activeRightPaneRoute = snapshot.active_right_pane_route.empty()
            ? "pane.ai_chat"
            : canonicalPaneRoute(snapshot.active_right_pane_route);
        editor.activeBottomLeftPaneRoute = snapshot.active_bottom_left_pane_route.empty()
            ? "pane.output"
            : canonicalPaneRoute(snapshot.active_bottom_left_pane_route);
        editor.activeBottomRightPaneRoute = snapshot.active_bottom_right_pane_route.empty()
            || snapshot.active_bottom_right_pane_route == "pane.ai_chat"
            ? "pane.ai_development"
            : canonicalPaneRoute(snapshot.active_bottom_right_pane_route);
        editor.layoutDrag = EditorLayoutDrag::None;
        editor.surfaceSettleFrames = 1;
        editor.lastLayoutExtent = {};
        editor.dockableGuiHost = {};
        editor.floatingGuiWindow = {
            .id = 1,
            .mode = gui::DockableWindowMode::floating,
            .dock_slot = gui::DockSlot::right,
            .floating = { .open = false },
            .visible = false
        };
        editor.detachedPanelHostStatus = "Pane popouts are routed context panels; context selection stays in the editor toolbar.";
        editor.projectNotesVisible = snapshot.project_notes_visible;
        editor.systems.renderZoom = (std::clamp)(finite_or(snapshot.systems_render_zoom, 1.15f), 0.50f, 3.0f);
        editor.systems.taskZoom = (std::clamp)(finite_or(snapshot.systems_task_zoom, 1.15f), 0.50f, 3.0f);
        editor.systems.renderPan = (std::clamp)(snapshot.systems_render_pan, -32, 32);
        editor.systems.taskPan = (std::clamp)(snapshot.systems_task_pan, -32, 32);
        editor.systems.graphInputCooldownFrames = 0;
        editor.systems.renderSurface = {};
        editor.systems.taskSurface = {};
        editor.systems.supportSurface = {};
        editor.systems.aiLoopSurface = {};
        editor.aiWorkspaceDomain = snapshot_ai_workspace_domain(snapshot.ai_workspace_domain);
        editor.showAboutModal = false;
        editor.showSettingsModal = false;
        editor.showPackageManagerModal = false;
        editor.showAiModelConsentModal = false;
        editor.showVoiceConsentModal = false;
        epochengine::ai::voice_runtime::stop(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&editor)));
        editor.aiVoiceSession.cancel();
        editor.aiDeferredPrompt.clear();
        editor.aiDeferredDisplay.clear();
        editor.aiDeferredRequestKind = AiDeferredRequestKind::None;
        editor.aiDeferredModelLease.reset();
        editor.aiDeferredDispatchQueued = false;
        cancel_ai_work_admission(editor);
        editor.aiModelMemoryApproval.revoke(editor.aiWorkAdmission);
        editor.showUpdateConfirmModal = false;
        editor.showSourceUpdateConfirmModal = false;
        editor.updateState = EditorUpdateState::Idle;
        editor.updateStatus = "Updates have not been checked.";
        editor.updateCheckPending.reset();
        editor.lastUpdateCheck = {};
        editor.autoUpdateCheckQueued = true;
        editor.updateInstallPending = false;
        editor.updateSourceInstallPending = false;
        editor.updateSourceCancelRequested = false;
        editor.updateProjectSourceDownloadPending = false;
        editor.updateOperationStartedAt = {};
        editor.updateRetryAfter = {};
        editor.updateRestartCountdownArmed = false;
        editor.updateRestartCountdownStartedAt = {};
        editor.updateConfirmModalStableSize = {};
        editor.updateConfirmModalStableViewport = {};
        editor.sourceUpdateConfirmModalStableSize = {};
        editor.sourceUpdateConfirmModalStableViewport = {};
        editor.automationCommand = EditorAutomationCommand::None;
        editor.automationConsumed = true;
        editor.aiContinuousBuildEnabled = false;
        editor.aiContinuousBuildPending.reset();
        editor.aiContinuousBuildFingerprint.clear();
        editor.aiContinuousBuildStatus = "Manual OS AI evidence gate is idle.";
        if (snapshot.camera.valid)
            (void)previewgrid::restore_camera_rig_snapshot(ctx, snapshot.camera);
        else
            previewgrid::set_camera_mode(ctx, editor.viewportCameraMode);
        input::set_active_profile(editor.inputProfilePreset);
        ctx->set_scene_preview_mode(editor.previewMode);
        publish_editor_preview_markers(ctx, editor);
        return true;
    }

    bool editor_run_script(const core::Context* ctx, std::string_view script_name)
    {
        if (!ctx)
            return false;

        std::string projectRoot{};
        std::string projectId{};
        {
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto it = storage.states.find(ctx);
            if (it != storage.states.end())
            {
                projectRoot = it->second.projectRoot;
                projectId = it->second.projectId;
            }
        }

        scripting::ScriptLoadReport report;
        const std::string projectModelAsset = editor_project_demo_model_path(projectId);
        EpochScriptHost host{
            .user_data = const_cast<core::Context*>(ctx),
            .log = &script_log_callback,
            .rotate_all_entities_yaw = &script_rotate_all_entities_yaw_callback,
            .queue_model_load = &script_queue_model_load_callback,
            .project_model_asset = projectModelAsset.empty() ? nullptr : projectModelAsset.c_str(),
            .request_engine_scene = &script_request_engine_scene_callback,
            .attach_selected_entity_oscillator = &script_attach_selected_entity_oscillator_callback,
            .arrange_cubes = &script_arrange_cubes_callback,
            .attach_cube_oscillators = &script_attach_cube_oscillators_callback
        };

        const std::string sourcePath = editor_resolve_script_source_path(script_name, projectRoot);
        const bool ok = scripting::load_or_reload_script(
            std::filesystem::path{ sourcePath },
            script_name,
            &host,
            &report);

        auto& storage = editor_storage();
        std::scoped_lock lock(storage.mutex);
        const auto it = storage.states.find(ctx);
        if (it != storage.states.end())
        {
            push_editor_log(it->second, std::string("[script] Source path: ") + sourcePath);
            for (const auto& message : report.messages())
                push_editor_log(it->second, message);
            push_editor_log(
                it->second,
                ok
                ? std::string("[script] Run completed successfully.")
                : std::string("[script] Run failed."));
        }

        return ok;
    }

