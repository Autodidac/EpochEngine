// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        auto play_active_context = [&]()
        {
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

        auto launch_active_project_context = [&]()
        {
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
            if (!save_project_gui_document(editor)
                || !save_editor_scene_snapshot(editor))
            {
                editor.projectBuildStatus =
                    "Run blocked because GUI and scene source revisions were not both committed.";
                push_editor_log(
                    editor, "[project] " + editor.projectBuildStatus);
                return;
            }
            repair_active_project_evidence(editor);

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
                project_existing_output_exe_path(editor.projectRoot);
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
                    std::string{"Launch Single Context - "} + freshness.reason);
                return;
            case project_lifecycle::Action::wait_for_build:
                editor.projectBuildStatus =
                    "Project build is active. Use Cancel Project Build or wait for completion.";
                return;
            case project_lifecycle::Action::focus_runtime:
                (void)focus_project_runtime(editor);
                editor.projectBuildStatus = editor.projectRuntimeStatus;
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
                    editor.inputProfilePreset))
            {
                editor.projectBuildStatus = editor.projectRuntimeStatus;
                push_editor_log(
                    editor,
                    "[project] Runtime launch refused: "
                        + editor.projectRuntimeStatus);
                return;
            }

            editor.projectBuildStatus = editor.projectRuntimeStatus;
            push_editor_log(
                editor,
                std::string{"[project] Owned standalone launch accepted for "}
                    + sceneId + " using "
                    + std::string{project_run_backend_label(runBackend)} + ".");
            append_project_note(
                editor,
                "Launch Single Context",
                "Launching the accepted child artifact through the owned runtime supervisor.",
                editor.projectRuntimeStatus);
        };
        auto dispatch_ai_tool_plan = [&]() -> bool
        {
            if (!editor.aiToolPlan
                || editor.aiToolPlanApplied)
            {
                editor.aiToolStatus =
                    "No unapplied validated AI tool plan is available.";
                return false;
            }

            const auto& call = editor.aiToolPlan.plan.call;
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
            if (!validation)
            {
                editor.aiToolStatus =
                    "The approved tool call failed host validation: "
                    + validation.message;
                push_editor_log(
                    editor,
                    "[ai-tool] " + editor.aiToolStatus);
                return false;
            }

            if (call.tool == "project.inspect")
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
            else if (call.tool == "project.save")
            {
                repair_active_project_evidence(editor);
                editor.aiToolStatus =
                    "project.save returned host evidence: "
                    + editor.projectStatus;
            }
            else if (call.tool == "project.build")
            {
                start_project_build(
                    editor,
                    false,
                    "operator-approved AI project.build");
                editor.aiToolStatus =
                    "project.build returned host evidence: "
                    + editor.projectBuildStatus;
            }
            else if (call.tool == "project.run")
            {
                launch_active_project_context();
                editor.aiToolStatus =
                    "project.run returned host evidence: "
                    + editor.projectBuildStatus;
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
                                    editor.projectRoot));
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
            push_editor_log(
                editor,
                "[ai-tool] " + editor.aiToolStatus);
            return true;
        };

        auto poll_project_build = [&]()
        {
            if (!editor.projectBuildPending
                || editor.projectBuildPending->wait_for(0ms)
                    != std::future_status::ready)
            {
                return;
            }

            try
            {
                const auto build = editor.projectBuildPending->get();
                const bool shouldRun = editor.projectBuildRunAfterBuild;
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
                        runInputProfile))
                {
                    editor.projectBuildStatus = editor.projectRuntimeStatus;
                    push_editor_log(
                        editor,
                        "[project] Runtime launch refused: "
                            + editor.projectRuntimeStatus);
                    return;
                }

                editor.projectBuildStatus = editor.projectRuntimeStatus;
                push_editor_log(
                    editor,
                    std::string{"[project] Accepted build launched in standalone "}
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

            if (testedProject != editor.projectId)
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
        poll_model_package_install(editor);

