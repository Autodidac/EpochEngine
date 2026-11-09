// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        auto start_ai_project_test = [&](
            const std::filesystem::path& executable) -> bool
        {
            if (editor.aiToolTestPending)
            {
                editor.aiToolStatus =
                    "An approved project contract test is already running.";
                return false;
            }
            if (editor.projectId.empty() || executable.empty())
            {
                editor.aiToolStatus =
                    "Project test refused because no active project output is available.";
                return false;
            }

            try
            {
                auto submission = schedule_editor_cancellable_task(
                    editor,
                    "AI Approved Project Test",
                    [output = executable,
                     projectId = editor.projectId](
                        std::stop_token cancellation) mutable
                    {
                        return test_ai_project_output(
                            std::move(output),
                            std::move(projectId),
                            cancellation);
                    });
                editor.aiToolTestPending.emplace(
                    std::move(submission.completion));
                editor.aiToolTestCancellation =
                    submission.cancellation;
                editor.aiToolTestProjectId = editor.projectId;
                editor.aiToolTestProjectRoot = editor.projectRoot;
                editor.aiToolTestProjectGeneration = editor.projectLifecycle.evidence().generations.selected_project.generation;
                editor.aiToolStatus =
                    "Approved project.test is running the generated child contract in a hidden host process.";
                push_editor_log(
                    editor,
                    "[ai-tool] " + editor.aiToolStatus);
                return true;
            }
            catch (const std::exception& exception)
            {
                editor.aiToolStatus =
                    std::string{
                        "Approved project.test could not be scheduled: "}
                    + exception.what();
            }
            catch (...)
            {
                editor.aiToolStatus =
                    "Approved project.test could not be scheduled because of an unknown scheduler failure.";
            }
            push_editor_log(
                editor,
                "[ai-tool] " + editor.aiToolStatus);
            return false;
        };

        auto launch_active_project_context = [&](bool inEditor = false)
        {
            if (inEditor && !project_runtime_embedding_supported())
            {
                editor.projectBuildStatus = "Run In Editor requires a dock-capable host on this platform. Choose Separate Window to run externally.";
                push_editor_log(editor, "[project] " + editor.projectBuildStatus);
                return;
            }
            clear_completed_editor_builds(editor, "run request");
            if (editor.projectBuildPending)
            {
                editor.projectBuildStatus =
                    "Project build is active. Use Cancel Project Build or wait for completion.";
                push_editor_log(
                    editor,
                    "[project] Run request kept on the active build generation.");
                return;
            }
            if (editor.aiContinuousBuildPending)
            {
                editor.projectBuildStatus =
                    "Engine-development build is active; project Run remains unavailable until it settles.";
                push_editor_log(
                    editor,
                    "[project] Run request blocked while AI engine development build is active.");
                return;
            }

            const bool selectedScriptAsset = !editor.selectedAssetPath.empty()
                && std::filesystem::path{editor.selectedAssetPath}
                    .filename().string().ends_with(".ascript.cpp");
            if (editor.workspaceTab == EditorWorkspaceTab::Scripts
                || (editor.workspaceTab == EditorWorkspaceTab::Assets
                    && selectedScriptAsset))
            {
                if (selectedScriptAsset)
                {
                    editor.activeScript = script_id_from_source_path(
                        std::filesystem::path{editor.selectedAssetPath});
                }
                editor.scriptBuildStatus =
                    "Build Selected Script validates the script asset; Run remains project-owned.";
                push_editor_log(
                    editor,
                    "[script] Run retained project ownership while a script asset is selected.");
            }

            (void)project_gui_document_layout(editor);
            if (!save_project_gui_document(editor, false)
                || !save_editor_scene_snapshot(editor))
            {
                editor.projectBuildStatus =
                    "Run blocked because GUI and scene source revisions were not both committed.";
                push_editor_log(
                    editor, "[project] " + editor.projectBuildStatus);
                return;
            }
            repair_active_project_evidence(editor);

            const auto dependency=project_planetary::prepare_runtime_dependency(
                resolve_editor_path(std::filesystem::path{editor.projectRoot}));
            if(!dependency.ok)
            {
                editor.projectBuildStatus=dependency.message;
                push_editor_log(editor,"[project] "+dependency.message);
                return;
            }

            const ProjectBuildFreshness freshness =
                project_child_build_freshness(editor);
            if (!editor.projectLifecycle.observe_build_inputs(
                    freshness.inputFingerprint))
            {
                const auto decision = editor.projectLifecycle.decision(
                    project_lifecycle::Request::run);
                editor.projectBuildStatus = epochengine::format_text(
                    "Run blocked while build inputs settle: {} ({}).",
                    project_lifecycle::action_name(decision.action),
                    project_lifecycle::block_reason_name(decision.reason));
                push_editor_log(
                    editor, "[project] " + editor.projectBuildStatus);
                return;
            }

            const std::filesystem::path outputExe =
                project_existing_output_exe_path(editor.projectRoot, editor.projectBuildConfiguration);
            if (!path_exists(outputExe))
            {
                editor.projectLifecycle.observe_output_missing();
                editor.projectBuildAcceptedOutputDigest = {};
            }
            else if (editor.projectBuildAcceptedOutputDigest.valid())
            {
                const auto currentDigest = project_output_digest(outputExe);
                if (!currentDigest
                    || *currentDigest
                        != editor.projectBuildAcceptedOutputDigest)
                {
                    editor.projectLifecycle.observe_output_missing();
                    editor.projectBuildAcceptedOutputDigest = {};
                    push_editor_log(
                        editor,
                        "[project] Existing child executable changed after acceptance; rebuilding.");
                }
            }

            reconcile_project_runtime(editor);
            const project_lifecycle::Decision decision =
                editor.projectLifecycle.decision(
                    project_lifecycle::Request::run);
            switch (decision.action)
            {
            case project_lifecycle::Action::build_project:
            case project_lifecycle::Action::prepare_build_inputs:
            case project_lifecycle::Action::materialize_shell:
                start_project_build(
                    editor,
                    true,
                    std::string{inEditor ? "Run In Editor - " : "Launch Single Context - "} + freshness.reason,
                    inEditor);
                return;
            case project_lifecycle::Action::wait_for_build:
                editor.projectBuildStatus =
                    "Project build is active. Use Cancel Project Build or wait for completion.";
                return;
            case project_lifecycle::Action::focus_runtime:
                if (editor.projectRuntimeInEditor != inEditor)
                {
                    editor.projectBuildStatus = "Stop the current runtime before changing between In Editor and Separate Window.";
                    return;
                }
                editor.projectRuntimeViewportMaximized = inEditor && editor.sceneRunMaximizedInEditor;
                (void)focus_project_runtime(editor);
                return;
            case project_lifecycle::Action::launch_runtime:
                break;
            default:
                editor.projectBuildStatus = epochengine::format_text(
                    "Run blocked: {} ({}).",
                    project_lifecycle::action_name(decision.action),
                    project_lifecycle::block_reason_name(decision.reason));
                push_editor_log(
                    editor, "[project] " + editor.projectBuildStatus);
                return;
            }

            const std::string sceneId = project_runtime_scene_id(editor);
            const std::string runBackend = editor.projectRunBackend.empty()
                ? std::string{"opengl"} : editor.projectRunBackend;
            if (!launch_project_runtime(
                    editor,
                    outputExe,
                    sceneId,
                    runBackend,
                    editor.projectRunFrameLimitFps,
                    editor.projectCameraMode,
                    editor.inputProfilePreset,
                    inEditor))
            {
                push_editor_log(
                    editor,
                    "[project] Runtime launch refused: "
                        + editor.projectRuntimeStatus);
                return;
            }

            push_editor_log(
                editor,
                std::string{inEditor ? "[project] Owned runtime started; in-editor attachment requested for " : "[project] Owned standalone launch accepted for "}
                    + sceneId + " using "
                    + std::string{project_run_backend_label(runBackend)} + ".");
            append_project_note(
                editor,
                "Launch Single Context",
                "Launching the accepted child artifact through the owned runtime supervisor.",
                editor.projectRuntimeStatus);
        };
        auto play_active_context = [&]()
        {
            const auto requirement=project_planetary::runtime_requirement(std::filesystem::path{editor.projectRoot});
            if(!requirement.ok)
            {
                editor.projectStatus=requirement.message;editor.projectBuildStatus=requirement.message;
                push_editor_log(editor,"[project] "+requirement.message);
                return;
            }
            if(requirement.requires_project_runtime)
            {
                editor.projectStatus=requirement.message;
                push_editor_log(editor,"[project] "+requirement.message);
                launch_active_project_context(true);
                return;
            }
            if (!save_editor_scene_snapshot(editor))
            {
                editor.projectStatus = "Play In Editor blocked because the active scene revision was not committed.";
                push_editor_log(editor, "[project] " + editor.projectStatus);
                return;
            }
            const auto& projectProfile = active_project_profile(editor);
            const previewgrid::CameraMode playCameraMode =
                projectProfile.tilemap_path.empty()
                    ? editor.projectCameraMode
                    : previewgrid::CameraMode::Canvas2D;
            const std::string sceneId =
                project_runtime_scene_id(editor)
                + "|camera="
                + std::string(project_camera_argument(playCameraMode))
                + "|input="
                + std::string(input_profile_argument(editor.inputProfilePreset));
            editor.projectStatus = std::string("Play In Editor requested using saved editor scene snapshot: ") + sceneId;
            push_editor_log(editor, std::string("[project] Play In Editor requested for ") + editor.projectName + " using saved scene snapshot: " + sceneId);
            append_project_note(
                editor,
                "Play In Editor",
                "Running the saved active project scene snapshot inside the editor without launching a child process.",
                sceneId);
            emit_command(EditorCommand::RunGame, sceneId);
        };

        const auto complete_tool_goal = [&](std::string_view operation, bool succeeded)
        {
            if (!editor.aiGoalActive || !editor.aiGoalTools.enabled
                || !editor.aiGoalTools.waiting || operation != editor.aiGoalTools.operation()) return;
            const auto generation = editor.projectLifecycle.evidence().generations.selected_project.generation;
            const bool next = editor.aiGoalTools.receipt(operation, succeeded,
                editor.projectId, editor.projectRoot, generation);
            editor.aiGoalLastMilestoneReceipt = editor.aiToolStatus;
            editor.aiGoalCompletedMilestones = editor.aiGoalTools.stage;
            editor.aiGoalPlanNextQueued = next && editor.aiGoalRunning;
            if (!next) editor.aiGoalRunning = false;
        };

        auto dispatch_ai_tool_plan_impl = [&]() -> bool
        {
            if (!editor.aiToolPlan
                || editor.aiToolPlanApplied)
            {
                editor.aiToolStatus =
                    "No unapplied validated AI tool plan is available.";
                return false;
            }

            // Opening/closing clears the staged plan. Do not retain a reference
            // into project-bound UI state across that canonical transition.
            bool synchronousSuccess = true;
            const auto call = editor.aiToolPlan.plan.call;
            if (editor.aiGoalTools.enabled && editor.aiGoalActive && editor.aiGoalRunning)
            {
                if (editor.aiGoalTools.stopped || call.tool != editor.aiGoalTools.operation()
                    || !editor.aiGoalTools.matches(editor.projectId, editor.projectRoot,
                        editor.projectLifecycle.evidence().generations.selected_project.generation))
                {
                    editor.aiToolStatus = "Goal operation or project does not match the next reviewed stage. Nothing ran.";
                    editor.aiGoalRunning = editor.aiGoalPlanNextQueued = false;
                    return false;
                }
            }
            if (const auto expected = ai::project_lifecycle_request_tool(editor.aiToolBoundRequest);
                !expected.empty() && call.tool != expected)
            {
                editor.aiToolStatus = "The proposal does not perform the requested " + std::string{expected}
                    + " operation. Nothing ran; request a corrected plan.";
                return false;
            }
            const auto selection = editor.projectLifecycle.evidence().generations.selected_project;
            if (!ai::project_tool_selection_matches(editor.aiToolProjectId, editor.aiToolProjectRoot,
                    editor.aiToolProjectGeneration, editor.projectId, editor.projectRoot, selection.generation))
            {
                editor.aiToolStatus = "Project selection changed after this request. Nothing ran; request a fresh plan for the current project.";
                return false;
            }
            const bool readsOnly = call.tool == "project.list" || call.tool == "project.inspect"
                || call.tool == "diagnostics.read" || call.tool == "package.catalog";
            const bool stopping = call.tool == "project.stop";
            if (!readsOnly && !stopping
                && (editor.aiToolSceneRevision != editor.sceneDocumentRevision
                    || editor.aiToolGuiRevision != (editor.guiDocument ? editor.guiDocument->revision().sequence : 0u)))
            {
                editor.aiToolStatus = "Project documents changed after this request. Nothing ran; review a fresh plan for the new revision.";
                return false;
            }
            if (!editor.aiToolExecution.empty() && !readsOnly && !stopping)
            {
                editor.aiToolStatus = "An approved project operation is still running. Wait for its result or request project.stop.";
                return false;
            }
            if((call.tool=="forest.configure"||call.tool=="forest.paint"||call.tool=="forest.material")&&!ai_forest_recipe_binding_current(editor))
            {
                editor.aiToolStatus="The Plant Lab recipe changed after this request. Nothing ran; review a fresh forest plan.";
                return false;
            }
            if(const auto selected=validate_ai_project_tool_selection(editor.aiToolBoundRequest,call);!selected)
            {
                editor.aiToolStatus=selected.message;
                push_editor_log(editor,"[ai-tool] "+editor.aiToolStatus);
                return false;
            }
            const auto registry =
                epochengine::ai::make_epoch_project_tool_registry();
            epochengine::ai::McpSessionAuthority authority{
                .session_id = "editor-tool-preview",
                .granted_capabilities =
                    epochengine::ai::McpToolCapability::inspect
                    | epochengine::ai::McpToolCapability::author
                    | epochengine::ai::McpToolCapability::build
                    | epochengine::ai::McpToolCapability::execute
                    | epochengine::ai::McpToolCapability::capture
                    | epochengine::ai::McpToolCapability::diagnose,
                .approved_call_ids = {call.call_id}};
            const auto validation =
                registry.validate(call, authority);
            const auto arguments = epochengine::ai::validate_project_tool_arguments(call);
            if (!validation || !arguments)
            {
                editor.aiToolStatus =
                    "The approved tool call failed host validation: "
                    + (!validation ? validation.message : arguments.message);
                push_editor_log(
                    editor,
                    "[ai-tool] " + editor.aiToolStatus);
                return false;
            }

            bool unsavedOther = editor.scriptEditorDirty || editor.projectCodeWorkspace.controller.has_dirty_documents()
                || (editor.projectInputSettings && editor.projectInputSettings->dirty());
            if (editor.projectTextures)
                if (const auto texture = editor.projectTextures->editable_state()) unsavedOther = unsavedOther || texture->dirty;
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
            if (editor.tileMapWorkspace)
                unsavedOther = unsavedOther || editor.tileMapWorkspace->dirty()
                    || editor.tileMapWorkspace->selected_layer_draft_dirty()
                    || editor.tileMapWorkspace->selected_palette_collision_draft_dirty()
                    || editor.tileMapWorkspace->selected_object_draft_dirty();
#endif
            if (unsavedOther && (call.tool == "project.save" || call.tool == "project.build"
                    || call.tool == "project.run" || call.tool == "project.test"))
            {
                editor.aiToolStatus = call.tool + " paused: save edited code, scripts, input, textures or tile-map drafts in their own editors first. No unsaved document was discarded or tested as though it were saved.";
                return false;
            }
            auto watch_execution = [&](std::string_view operation) {
                editor.aiToolExecution = operation;
                editor.aiToolExecutionProjectId = editor.projectId;
                editor.aiToolExecutionProjectRoot = editor.projectRoot;
                editor.aiToolExecutionProjectGeneration = selection.generation;
                editor.aiToolExecutionBuildGeneration = editor.projectBuildPending
                    ? editor.projectLifecycle.evidence().generations.active_build.build_generation : 0u;
                // Use the normal workspace transition and its companion panes;
                // this observes the approved canonical job, not another action.
                if(operation=="asset.image.generate")editor.requestedMainSurface=EditorMainSurface::AssetGeneration;
                else
                {editor.projectWorkspaceSection = 4u;editor.requestedMainSurface = EditorMainSurface::Project;}
            };
            if (call.tool == "project.list")
            {
                editor.aiToolStatus = "Available game and application projects (host ID | name):";
                std::size_t count{};
                for (const auto& [id, manifest] : editor.aiToolProjectCatalog)
                {
                    const auto* profile = editor_find_project_profile(id);
                    if (!profile || profile->manifest_path != manifest) continue;
                    editor.aiToolStatus += "\n" + id + " | " + std::string{profile->display_name};
                    ++count;
                }
                if (count == 0u) editor.aiToolStatus += "\nNo admitted projects. Use Open Project once to register an existing project.";
            }
            else if (call.tool == "project.open" || call.tool == "project.close" || call.tool == "project.create")
            {
                const bool unsaved = unsavedOther || gui_document_has_unsaved_changes(editor)
                    || (!editor.projectId.empty() && (!editor.projectLifecycle.evidence().scene_committed
                        || editor.sceneDocumentRevision != editor.projectLifecycle.evidence().generations.committed_scene_generation));
                reconcile_project_runtime(editor);
                if (unsaved || editor.projectBuildPending || editor.scriptBuildPending
                    || editor.aiAuthoringApplyPending || editor.aiToolTestPending
                    || editor.aiContinuousBuildPending || planetary_project_busy(editor)
                    || (editor.projectRuntimeSnapshot && editor.projectRuntimeSnapshot->active()))
                {
                    editor.aiToolStatus = "Project switch refused. Save unsaved documents and stop or finish active project operations first. Nothing was discarded.";
                    return false;
                }
                if (call.tool == "project.create")
                {
                    // The normal Open Project path deliberately clears goals.
                    // Restore only this exact approved create after its success.
                    const auto goal = editor.aiGoal;
                    const auto sequence = editor.aiGoalTools;
                    const bool resumeGoal = editor.aiGoalActive && editor.aiGoalRunning && sequence.enabled;
                    const bool created = create_ai_project_from_tool(editor,call,ctx);
                    if (resumeGoal && created)
                    {
                        editor.aiGoal = goal;
                        editor.aiGoalDraft = goal;
                        editor.aiGoalTools = sequence;
                        editor.aiGoalActive = editor.aiGoalRunning = true;
                        editor.aiGoalTools.waiting = true;
                        // Bind before the asynchronous source approval/generation.
                        editor.aiGoalTools.project_id = editor.projectId;
                        editor.aiGoalTools.project_root = editor.projectRoot;
                        editor.aiGoalTools.project_generation = editor.projectLifecycle.evidence().generations.selected_project.generation;
                        if (!editor.aiPackageRequest) complete_tool_goal(call.tool, false);
                    }
                    if (!created) return false;
                }
                else if (call.tool == "project.close")
                {
                    const bool closed = editor_close_project(ctx);
                    editor.aiToolStatus = closed ? "project.close completed; no project is open."
                        : "project.close refused: " + editor.projectStatus;
                }
                else
                {
                    const auto* target = ai::authoring_plan_detail::argument(call, "project");
                    const auto listed = std::find_if(editor.aiToolProjectCatalog.begin(), editor.aiToolProjectCatalog.end(),
                        [&](const auto& item) { return target && item.first == target->value; });
                    const auto* profile = listed == editor.aiToolProjectCatalog.end() ? nullptr : editor_find_project_profile(listed->first);
                    if (!profile || profile->manifest_path != listed->second
                        || (profile->kind != EditorProjectKind::Game && profile->kind != EditorProjectKind::Tool))
                    {
                        editor.aiToolStatus = "project.open refused: choose one unchanged host-listed project ID. No arbitrary path was opened.";
                        return false;
                    }
#if EPOCH_EDITION_PUBLIC
                    if (!project_ai_scope_allowed(profile->root_path))
                    {
                        editor.aiToolStatus = "project.open refused: this project is outside Public project assistance scope.";
                        return false;
                    }
#endif
                    const std::string requestedId = listed->first;
                    const std::filesystem::path manifest{listed->second};
                    const std::string requestedRoot{profile->root_path};
                    const auto inspected = editor_inspect_project_manifest_identity(
                        manifest.generic_string(), requestedId, requestedRoot);
                    if (!inspected.succeeded)
                    {
                        editor.aiToolStatus = "project.open refused: the registered manifest identity changed. " + inspected.summary;
                        return false;
                    }
                    const auto admitted = editor_admit_project_manifest(manifest.generic_string());
                    // Admission can refresh catalog storage. Re-resolve the ID;
                    // never switch to a newly returned, unapproved identity.
                    const bool opened = admitted.succeeded && admitted.project_id == requestedId
                        && editor_open_project(ctx, requestedId);
                    editor.aiToolStatus = opened && editor.projectId == requestedId
                        ? "project.open completed: " + editor.projectId + " | " + editor.projectName
                        : "project.open refused: " + editor.projectStatus;
                    if(opened&&editor.projectId==requestedId)
                        editor.requestedMainSurface=EditorMainSurface::Scene;
                }
            }
            else if (call.tool == "project.stop")
            {
                reconcile_project_runtime(editor);
                bool requested{};
                if (editor.projectBuildPending)
                {
                    const auto& active = editor.projectLifecycle.evidence().generations.active_build;
                    if (active.project != selection || !cancel_project_build(editor))
                    {
                        editor.aiToolStatus = "project.stop refused: no cancellable build belongs to this project selection.";
                        return false;
                    }
                    requested = true;
                }
                if (editor.projectRuntimeSnapshot && editor.projectRuntimeSnapshot->active()
                    && project_runtime_identity_current(editor, *editor.projectRuntimeSnapshot)
                    && stop_project_runtime(editor, platform::child_process::StopMode::graceful))
                    requested = true;
                if (requested)
                {
                    watch_execution("project.stop");
                    editor.aiToolStatus = "project.stop requested cancellation/graceful shutdown; waiting for owned work and process retirement.";
                }
                else editor.aiToolStatus = "project.stop: no active owned build or runtime was stopped. " + editor.projectRuntimeStatus;
            }
            else if (call.tool == "project.inspect")
            {
                editor.aiToolStatus = epochengine::format_text(
                    "project.inspect completed | id={} | kind={} | scene_revision={} | project={} | build={} | runtime={}",
                    editor.projectId.empty() ? "(none)" : editor.projectId,
                    editor.projectKind.empty() ? "(none)" : editor.projectKind,
                    editor.sceneDocumentRevision,
                    editor.projectStatus,
                    editor.projectBuildStatus,
                    editor.projectRuntimeStatus);
            }
            else if (call.tool == "package.catalog" || call.tool == "package.install"
                || call.tool == "terrain.generate")
            {
                if (!dispatch_ai_project_package_tool(editor, call, ctx)) return false;
            }
            else if(call.tool=="asset.image.generate")
            {
                if(editor.assetGenerationPending||editor.projectBuildPending||editor.scriptBuildPending
                    ||planetary_project_busy(editor)||editor.forestSurfacePending)
                {editor.aiToolStatus="Finish or cancel the active generation/build operation before starting this image.";return false;}
                std::string diagnostic;
                const auto binding=editor_asset_generation::image_request_binding(editor.assetGeneration,editor.aiToolBoundRequest,diagnostic);
                if(editor.aiToolImageBinding.empty()||binding!=editor.aiToolImageBinding)
                {editor.aiToolStatus="The prompt, controls or backend changed after this proposal. Nothing was generated; request and approve the new image settings.";return false;}
                dispatch_asset_generation(editor,ctx,editor_asset_generation::Action::generate_image);
                if(!editor.assetGenerationPending)
                {editor.aiToolStatus="Image generation did not start: "+editor.assetGeneration.status;return false;}
                editor.aiToolImageWork=editor.assetGeneration.active_work_directory;
                if(auto* activity=chat.ensure_host_activity())
                    activity->stream_live=true;
                editor.aiToolImageActivityId=chat.inlineHostActivityId;
                watch_execution("asset.image.generate");
                editor.aiToolStatus="Image generation queued through Asset Generation. Waiting for validation and coding-model restoration; no image is approved or imported.";
            }
            else if (call.tool == "forest.configure" || call.tool == "forest.material")
            {
                if(!has_open_project(editor)||editor.projectBuildPending||editor.scriptBuildPending
                    ||planetary_project_busy(editor)||editor.forestSurfacePending
                    ||editor.projectRuntimeProcess.valid()
                    ||(editor.projectRuntimeSnapshot&&editor.projectRuntimeSnapshot->active()))
                {editor.aiToolStatus="Tree configuration requires an open, stopped project with no pending generation or placement.";return false;}
                if(!(call.tool=="forest.configure"?configure_ai_forest_recipe(editor,call):assign_ai_forest_material(editor,call)))return false;
                editor.forestTerrainPainting=false;
                editor.requestedMainSurface=EditorMainSurface::ForestFactory;
                if(ctx)focus_forest_factory_preview(ctx.get(),editor);
            }
            else if (call.tool == "forest.paint")
            {
                if (!has_open_project(editor) || editor.projectBuildPending || editor.scriptBuildPending
                    || planetary_project_busy(editor) || editor.forestSurfacePending
                    || editor.projectRuntimeProcess.valid()
                    || (editor.projectRuntimeSnapshot && editor.projectRuntimeSnapshot->active()))
                { editor.aiToolStatus = "Forest painting requires an open, stopped project with no pending generation or placement."; return false; }
                auto selected = editor_entity_index(editor, editor.selectedEntityId);
                if (!selected || editor.entities[*selected].category != "Planetary Terrain")
                {
                    std::optional<std::size_t> planet;
                    for (std::size_t index = 0; index < editor.entities.size(); ++index)
                        if (editor.entities[index].category == "Planetary Terrain")
                        {
                            if (planet) { editor.aiToolStatus = "Select the target generated planet in 3D Objects before painting a forest."; return false; }
                            planet = index;
                        }
                    if (!planet) { editor.aiToolStatus = "Generate a planet before painting a forest."; return false; }
                    editor.selectedEntityId = editor.entities[*planet].sceneObjectId;
                }
                const auto& radius = ai::authoring_plan_detail::argument(call,"radius_m")->value;
                const auto& density = ai::authoring_plan_detail::argument(call,"density")->value;
                (void)std::from_chars(radius.data(),radius.data()+radius.size(),editor.forestArea.radiusMeters);
                (void)std::from_chars(density.data(),density.data()+density.size(),editor.forestArea.densityPerSquareMeter);
                if(editor.mainSurface!=EditorMainSurface::ForestFactory && ctx)
                    editor.forestPlacementCamera=previewgrid::camera_for(ctx.get());
                editor.aiGoalForestReceipt = false;
                if (!paint_forest_factory_area(editor))
                { editor.aiToolStatus = "forest.paint refused: " + editor.plantLabStatus; return false; }
                watch_execution("forest.paint");
                editor.forestTerrainPainting = true;
                editor.requestedMainSurface = EditorMainSurface::ForestFactory;
                editor.aiToolStatus = "forest.paint queued through Forest Factory; waiting for terrain sampling and scene placement. " + editor.plantLabStatus;
            }
            else if (call.tool == "project.save")
            {
                const bool saved = save_project_gui_document(editor, false) && save_editor_scene_snapshot(editor);
                synchronousSuccess = saved;
                editor.aiToolStatus = std::string{saved ? "project.save scene/GUI completed: " : "project.save failed: "}
                    + editor.projectStatus + " | " + editor.guiDocumentStatus;
            }
            else if (call.tool == "project.build")
            {
                start_project_build(
                    editor,
                    false,
                    "operator-approved AI project.build");
                editor.aiToolStatus =
                    "project.build host status: "
                    + editor.projectBuildStatus;
                if (editor.projectBuildPending) watch_execution("project.build");
            }
            else if (call.tool == "project.run")
            {
                // Use the same selected destination as the viewport Run
                // control, retaining the owned compiler/runtime lifecycle.
                launch_active_project_context(editor.sceneRunFullscreenInEditor);
                editor.aiToolStatus =
                    std::string{editor.sceneRunFullscreenInEditor
                        ? "project.run compiled in-editor host status: "
                        : "project.run compiled standalone host status: "}
                    + "runtime=" + editor.projectRuntimeStatus
                    + " | build=" + editor.projectBuildStatus;
                if (editor.projectBuildPending || (editor.projectRuntimeSnapshot && editor.projectRuntimeSnapshot->active()
                        && project_runtime_identity_current(editor, *editor.projectRuntimeSnapshot)))
                    watch_execution("project.run");
                else
                    return false; // A refused launch must not leave a goal waiting for a job that never started.
            }
            else if (call.tool == "project.test")
            {
                if (editor.projectId.empty()
                    || editor.projectRoot.empty())
                {
                    editor.aiToolStatus =
                        "project.test refused because no active project is open.";
                }
                else if (editor.aiToolTestPending)
                {
                    editor.aiToolStatus =
                        "project.test refused because an approved test is already running.";
                }
                else if (editor.projectBuildPending)
                {
                    editor.aiToolStatus =
                        "project.test refused because another project build owns the current lifecycle generation.";
                }
                else if (editor.projectLifecycle.evidence().runtime_active)
                {
                    editor.aiToolStatus =
                        "project.test refused while the generated runtime is active; stop it before approving a test.";
                }
                else
                {
                    repair_active_project_evidence(editor);
                    const ProjectBuildFreshness freshness =
                        project_child_build_freshness(editor);
                    if (!editor.projectLifecycle.observe_build_inputs(
                            freshness.inputFingerprint))
                    {
                        editor.aiToolStatus =
                            "project.test could not bind the current project input fingerprint.";
                    }
                    else
                    {
                        const auto decision =
                            editor.projectLifecycle.decision(
                                project_lifecycle::Request::run);
                        if (decision.action
                            == project_lifecycle::Action::launch_runtime)
                        {
                            (void)start_ai_project_test(
                                project_existing_output_exe_path(
                                    editor.projectRoot, editor.projectBuildConfiguration));
                        }
                        else if (decision.action
                                == project_lifecycle::Action::build_project
                            || decision.action
                                == project_lifecycle::Action::prepare_build_inputs
                            || decision.action
                                == project_lifecycle::Action::materialize_shell)
                        {
                            editor.aiToolTestAfterBuild = true;
                            start_project_build(
                                editor,
                                false,
                                "operator-approved AI project.test prerequisite");
                            if (!editor.projectBuildPending)
                            {
                                editor.aiToolTestAfterBuild = false;
                                editor.aiToolStatus =
                                    "project.test prerequisite build was rejected: "
                                    + editor.projectBuildStatus;
                            }
                            else
                            {
                                editor.aiToolStatus =
                                    "project.test queued one host-owned prerequisite build; the contract test will start only after accepted artifact evidence.";
                            }
                        }
                        else
                        {
                            editor.aiToolStatus =
                                epochengine::format_text(
                                    "project.test blocked by lifecycle action {} ({}).",
                                    project_lifecycle::action_name(
                                        decision.action),
                                    project_lifecycle::block_reason_name(
                                        decision.reason));
                        }
                    }
                }
            }
            else if (call.tool == "diagnostics.read")
            {
                editor.aiToolStatus = epochengine::format_text(
                    "diagnostics.read completed | project={} | build={} | runtime={} | compiler_tail={}",
                    editor.projectStatus,
                    editor.projectBuildStatus,
                    editor.projectRuntimeStatus,
                    editor.projectBuildDiagnosticTail.empty()
                        ? "(none)"
                        : editor.projectBuildDiagnosticTail);
            }
            else
            {
                editor.aiToolStatus =
                    "The approved tool is not connected to a host dispatcher.";
                return false;
            }

            editor.aiToolPlanApplied = true;
            if (editor.aiGoalTools.enabled && editor.aiGoalActive && editor.aiGoalRunning)
            {
                editor.aiGoalTools.waiting = true;
                if (call.tool == "project.save")
                    complete_tool_goal(call.tool, synchronousSuccess && editor.projectLifecycle.evidence().scene_committed
                        && editor.sceneDocumentRevision == editor.projectLifecycle.evidence().generations.committed_scene_generation
                        && !gui_document_has_unsaved_changes(editor));
                else if(call.tool=="forest.configure"||call.tool=="forest.material")
                {
                    complete_tool_goal(call.tool,synchronousSuccess&&bool(editor.plantLabPublishedArtifact)
                        &&editor.plantLabPublishedArtifact.source_revision==editor.plantLabDocument.revision
                        &&editor.plantLabPublishedArtifact.source_content_hash==editor.plantLabDocument.contentHash);
                }
            }
            editor.aiToolStatus += " Only this approved operation was dispatched; any remaining steps require a new reviewed plan.";
            push_editor_log(
                editor,
                "[ai-tool] " + editor.aiToolStatus);
            return true;
        };

        auto dispatch_ai_tool_plan = [&]() -> bool
        {
            const bool dispatched = dispatch_ai_tool_plan_impl();
            if (!dispatched && editor.aiGoalTools.enabled)
            {
                editor.aiGoalTools.stopped = true;
                editor.aiGoalRunning = editor.aiGoalPlanNextQueued = false;
            }
            return dispatched;
        };

        auto poll_project_build = [&]()
        {
            if (!editor.projectBuildPending) return;
            if (editor.projectBuildPending->wait_for(0ms) != std::future_status::ready)
            {
                refresh_project_build_progress(editor);
                return;
            }

            try
            {
                const auto build = editor.projectBuildPending->get();
                retain_project_build_transcript(editor, build);
                const bool shouldRun = editor.projectBuildRunAfterBuild;
                const bool runInEditor = editor.projectBuildRunInEditor;
                const std::string sceneId = editor.projectBuildRunScene.empty()
                    ? project_runtime_scene_id(editor)
                    : editor.projectBuildRunScene;
                const std::string runBackend =
                    editor.projectBuildRunBackend.empty()
                    ? std::string{"opengl"}
                    : editor.projectBuildRunBackend;
                const double runFrameLimit =
                    editor.projectBuildRunFrameLimitFps;
                const auto runCameraMode =
                    editor.projectBuildRunCameraMode;
                const auto runInputProfile =
                    editor.projectBuildRunInputProfile;
                const std::filesystem::path output =
                    resolved_project_build_output(editor, build);
                const bool accepted =
                    settle_project_build_lifecycle(editor, build);

                editor.projectBuildStatus = build.summary.empty()
                    ? (build.cancelled
                        ? std::string{"Project build cancelled."}
                        : build.succeeded
                            ? std::string{"Project build completed."}
                            : std::string{"Project build failed."})
                    : build.summary;
                if (build.succeeded && !accepted)
                {
                    editor.projectBuildStatus +=
                        " Artifact evidence was rejected.";
                }

                if (accepted)
                {
                    editor.projectBuildDiagnosticTail.clear();
                }
                else if (!build.log_path.empty())
                {
                    editor.projectBuildDiagnosticTail = read_text_tail(
                        std::filesystem::path{build.log_path}, 2'400u);
                }
                else
                {
                    editor.projectBuildDiagnosticTail =
                        editor.projectBuildStatus;
                }

                push_editor_log(
                    editor,
                    std::string{"[project] "}
                        + (build.cancelled
                            ? "Build cancelled. "
                            : accepted
                                ? "Build accepted. "
                                : "Build rejected. ")
                        + editor.projectBuildStatus);
                if (!build.output_path.empty())
                {
                    push_editor_log(
                        editor,
                        "[project] Output: " + build.output_path);
                }
                if (!build.log_path.empty())
                {
                    push_editor_log(
                        editor,
                        "[project] Log: " + build.log_path);
                }

                append_project_note(
                    editor,
                    build.cancelled
                        ? "Project Build Cancelled"
                        : accepted
                            ? "Project Build Accepted"
                            : "Project Build Rejected",
                    editor.projectBuildStatus,
                    accepted
                        ? "Exact input generation and executable digest were accepted."
                        : "No runtime authorization was produced.");

                editor.projectBuildPending.reset();
                reset_project_build_request(editor);

                if (editor.aiToolTestAfterBuild)
                {
                    editor.aiToolTestAfterBuild = false;
                    if (accepted && path_exists(output))
                    {
                        (void)start_ai_project_test(output);
                    }
                    else
                    {
                        editor.aiToolStatus =
                            "project.test prerequisite build did not produce accepted artifact evidence: "
                            + editor.projectBuildStatus;
                        push_editor_log(
                            editor,
                            "[ai-tool] " + editor.aiToolStatus);
                    }
                    chat.append_status(editor.aiToolStatus);
                    return;
                }

                if (!shouldRun)
                    return;
                if (!accepted)
                {
                    push_editor_log(
                        editor,
                        "[project] Run refused because the completed build did not produce accepted artifact evidence.");
                    return;
                }
                if (!path_exists(output))
                {
                    editor.projectLifecycle.observe_output_missing();
                    editor.projectBuildAcceptedOutputDigest = {};
                    editor.projectBuildStatus =
                        "Launch blocked because the accepted child executable disappeared.";
                    push_editor_log(
                        editor, "[project] " + editor.projectBuildStatus);
                    return;
                }

                if (!launch_project_runtime(
                        editor,
                        output,
                        sceneId,
                        runBackend,
                        runFrameLimit,
                        runCameraMode,
                        runInputProfile,
                        runInEditor))
                {
                    push_editor_log(
                        editor,
                        "[project] Runtime launch refused: "
                            + editor.projectRuntimeStatus);
                    return;
                }

                push_editor_log(
                    editor,
                    std::string{runInEditor ? "[project] Accepted build started; in-editor attachment requested using " : "[project] Accepted build launched in standalone "}
                        + std::string{project_run_backend_label(runBackend)}
                        + " mode.");
            }
            catch (const editor_tasks::TaskCancelled&)
            {
                if (editor.aiToolTestAfterBuild)
                {
                    editor.aiToolTestAfterBuild = false;
                    editor.aiToolStatus =
                        "project.test prerequisite build was cancelled before its process tree started.";
                    chat.append_status(editor.aiToolStatus);
                }
                abandon_project_build_lifecycle(editor);
                editor.projectBuildPending.reset();
                reset_project_build_request(editor);
                editor.projectBuildStatus =
                    "Project build cancelled before its process tree started.";
                push_editor_log(
                    editor, "[project] " + editor.projectBuildStatus);
            }
            catch (const std::exception& error)
            {
                if (editor.aiToolTestAfterBuild)
                {
                    editor.aiToolTestAfterBuild = false;
                    editor.aiToolStatus =
                        std::string{
                            "project.test prerequisite build failed while settling: "}
                        + error.what();
                    chat.append_status(editor.aiToolStatus);
                }
                abandon_project_build_lifecycle(editor);
                editor.projectBuildPending.reset();
                reset_project_build_request(editor);
                editor.projectBuildStatus =
                    std::string{"Project build failed while settling: "}
                    + error.what();
                push_editor_log(
                    editor, "[project] " + editor.projectBuildStatus);
            }
            catch (...)
            {
                if (editor.aiToolTestAfterBuild)
                {
                    editor.aiToolTestAfterBuild = false;
                    editor.aiToolStatus =
                        "project.test prerequisite build failed with an unknown settlement error.";
                    chat.append_status(editor.aiToolStatus);
                }
                abandon_project_build_lifecycle(editor);
                editor.projectBuildPending.reset();
                reset_project_build_request(editor);
                editor.projectBuildStatus =
                    "Project build failed with an unknown settlement error.";
                push_editor_log(
                    editor, "[project] " + editor.projectBuildStatus);
            }
        };
        auto pump_ai_tool_test = [&]()
        {
            if (!editor.aiToolTestPending
                || editor.aiToolTestPending->wait_for(0ms)
                    != std::future_status::ready)
            {
                return;
            }

            const std::string testedProject =
                editor.aiToolTestProjectId;
            const std::string testedRoot = editor.aiToolTestProjectRoot;
            const auto testedGeneration = editor.aiToolTestProjectGeneration;
            EditorProjectBuildResult tested{};
            try
            {
                tested = editor.aiToolTestPending->get();
            }
            catch (const editor_tasks::TaskCancelled&)
            {
                tested.cancelled = true;
                tested.summary =
                    "Approved project.test was cancelled before its child process started.";
            }
            catch (const std::exception& error)
            {
                tested.summary =
                    std::string{
                        "Approved project.test failed while collecting evidence: "}
                    + error.what();
            }
            catch (...)
            {
                tested.summary =
                    "Approved project.test failed with an unknown evidence error.";
            }
            editor.aiToolTestPending.reset();
            editor.aiToolTestCancellation = {};
            editor.aiToolTestProjectId.clear();
            editor.aiToolTestProjectRoot.clear();
            editor.aiToolTestProjectGeneration = 0u;

            if (!ai::project_tool_selection_matches(testedProject, testedRoot, testedGeneration,
                    editor.projectId, editor.projectRoot,
                    editor.projectLifecycle.evidence().generations.selected_project.generation))
            {
                editor.aiToolStatus =
                    "A stale project.test completion was ignored after the active project changed.";
            }
            else
            {
                editor.aiToolStatus = tested.summary.empty()
                    ? tested.succeeded
                        ? "Approved project.test passed."
                        : "Approved project.test failed without a summary."
                    : std::move(tested.summary);
            }
            chat.append_status(editor.aiToolStatus);
            push_editor_log(
                editor,
                "[ai-tool] " + editor.aiToolStatus);
        };
        poll_project_build();
        pump_ai_tool_test();
        reconcile_project_runtime(editor);
        if (!editor.aiToolExecution.empty())
        {
            const auto selected = editor.projectLifecycle.evidence().generations.selected_project;
            const bool current = ai::project_tool_selection_matches(
                editor.aiToolExecutionProjectId, editor.aiToolExecutionProjectRoot,
                editor.aiToolExecutionProjectGeneration, editor.projectId, editor.projectRoot, selected.generation);
            const bool running = editor.projectRuntimeSnapshot && editor.projectRuntimeSnapshot->active();
            const bool waiting = current && (editor.projectBuildPending
                || (editor.aiToolExecution == "asset.image.generate" && editor.assetGenerationPending)
                || (editor.aiToolExecution == "forest.paint" && editor.forestSurfacePending)
                || (editor.aiToolExecution == "project.stop" && running)
                || (editor.aiToolExecution == "project.run" && running && editor.projectRuntimeInEditor
                    && !editor.projectRuntimeWindow && editor.projectRuntimeAttachmentError.empty()));
            if (!waiting)
            {
                if (!current)
                {
                    editor.aiToolStatus = editor.aiToolExecution + " result ignored after the project selection changed; no later project was affected.";
                    if(editor.aiToolExecution=="asset.image.generate"&&editor.aiToolImageActivityId!=0u
                        &&chat.inlineHostActivityId==editor.aiToolImageActivityId)
                        chat.end_host_activity(true);
                }
                else if(editor.aiToolExecution=="asset.image.generate")
                {
                    const auto& reference=editor.assetGeneration.reference;
                    const bool generated=reference&&reference->result&&reference->coding_restored&&reference->retirement_verified
                        &&!editor.aiToolImageWork.empty()&&reference->result.work_directory==editor.aiToolImageWork;
                    editor.aiToolStatus=generated
                        ? "Image generated and checked. Working file: "+display_project_path(reference->result.output)
                            +". Preview it in Asset Generation, then Save Image As or approve project import. No reference or texture binding was approved."
                        : "Image generation ended without a newly validated output: "+editor.assetGeneration.status;
                    // A later chat turn may already own the current activity.
                    // Only this image job's approval turn may be settled here.
                    if(editor.aiToolImageActivityId!=0u
                        &&chat.inlineHostActivityId==editor.aiToolImageActivityId)
                        chat.end_host_activity(!generated);
                }
                else if (editor.aiToolExecution == "forest.paint")
                    editor.aiToolStatus = "forest.paint finished its owned operation: " + editor.plantLabStatus;
                else if (editor.aiToolExecution == "project.build")
                    editor.aiToolStatus = std::string{editor.projectLifecycle.evidence().generations.output_artifact.verified
                        && editor.aiToolExecutionBuildGeneration != 0u
                        && editor.projectLifecycle.evidence().generations.output_artifact.build_generation == editor.aiToolExecutionBuildGeneration
                        ? "project.build completed with accepted artifact evidence: " : "project.build ended without accepted artifact evidence: "}
                        + editor.projectBuildStatus;
                else if (editor.aiToolExecution == "project.run")
                    editor.aiToolStatus = std::string{running && editor.projectRuntimeAttachmentError.empty()
                        ? "project.run active: " : "project.run failed or ended before runtime acceptance: "}
                        + editor.projectRuntimeStatus + " | " + editor.projectBuildStatus
                        + (editor.projectRuntimeAttachmentError.empty() ? std::string{} : " | " + editor.projectRuntimeAttachmentError);
                else
                    editor.aiToolStatus = "project.stop completed: the owned build/runtime is no longer active. " + editor.projectRuntimeStatus;
                if (!editor.projectBuildDiagnosticTail.empty() && editor.aiToolExecution != "project.stop"
                    &&editor.aiToolExecution!="asset.image.generate")
                    editor.aiToolStatus += "\nCompiler output: " + editor.projectBuildDiagnosticTail;
                // Return to the preview only if the user has not chosen a
                // different workspace during this project's approved job.
                if(current&&editor.aiToolExecution!="forest.paint"&&editor.aiToolExecution!="asset.image.generate"&&(editor.mainSurface==EditorMainSurface::Project
                    ||editor.requestedMainSurface==EditorMainSurface::Project))
                    editor.requestedMainSurface=EditorMainSurface::Scene;
                if(current&&editor.aiToolExecution=="forest.paint"
                    &&editor.requestedMainSurface.value_or(editor.mainSurface)==EditorMainSurface::ForestFactory)
                    editor.requestedMainSurface=EditorMainSurface::Scene;
                if (editor.aiGoalTools.enabled && editor.aiGoalTools.waiting)
                {
                    const bool succeeded = current && (editor.aiToolExecution == "forest.paint"
                        ? editor.aiGoalForestReceipt
                        : editor.aiToolExecution == "project.run" && running && editor.projectRuntimeAttachmentError.empty());
                    complete_tool_goal(editor.aiToolExecution, succeeded);
                }
                editor.aiToolImageActivityId=0u;
                editor.aiToolExecution.clear();
                chat.append_status(editor.aiToolStatus);
                push_editor_log(editor, "[ai-tool] " + editor.aiToolStatus);
            }
        }
        if (editor.aiGoalPackageReceipt)
        {
            const bool succeeded = *editor.aiGoalPackageReceipt;
            editor.aiGoalPackageReceipt.reset();
            complete_tool_goal("project.create", succeeded);
        }
        poll_model_package_install(editor);

