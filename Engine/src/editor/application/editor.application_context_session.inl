// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] std::shared_ptr<AiChat> chat_state_for(
            const std::shared_ptr<core::Context>& ctx)
        {
            auto& storage = chat_storage();
            std::scoped_lock lock(storage.mutex);

            if (!storage.bot_initialized)
            {
                epochengine::ai::init_engine_ai();
                storage.bot_initialized = true;
            }

            auto [it, inserted] = storage.chats.try_emplace(
                ctx.get(), std::make_shared<AiChat>());
            return it->second;
        }

        EditorState& editor_state_for(const std::shared_ptr<core::Context>& ctx)
        {
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            auto [it, inserted] = storage.states.try_emplace(ctx.get());
            if (inserted)
            {
                it->second.initialized = true;
                if (core::cli::frame_limit_explicit)
                {
                    it->second.editorFrameLimitFps = core::cli::frame_limit_fps;
                    it->second.editorFrameVsyncRequested = core::cli::frame_vsync_requested;
                }
                else
                {
                    it->second.editorFrameLimitFps = perf::target_fps_for(
                        recommended_editor_performance_tier(ctx, std::thread::hardware_concurrency()));
                }
                it->second.automationCommand = read_editor_automation_command();
                it->second.timeControl.fixed_dt_seconds = 1.0 / 60.0;
                it->second.timeControl.time_scale = 1.0;
                it->second.timeControl.max_steps_per_frame = 8;
                const std::string requestedProjectId = read_requested_editor_project_id();
                if (!requestedProjectId.empty() && editor_find_project_profile(requestedProjectId))
                    set_project(it->second, requestedProjectId, false);
                else
                    (void)close_active_project(it->second, standard_editor_application(), false);
                load_editor_layout_preferences(it->second);
                if (ctx && ctx->windowData)
                    ctx->windowData->editorFramePacingOverride.publish(
                        it->second.editorFrameVsyncRequested ? perf::frame_pacing_mode::vsync
                            : it->second.editorFrameLimitFps > 0.0 ? perf::frame_pacing_mode::target_hz
                            : perf::frame_pacing_mode::uncapped,
                        it->second.editorFrameLimitFps);
                if (const auto requestedWorkspace = requested_editor_workspace_tab())
                {
                    it->second.workspaceTab = *requestedWorkspace;
                    it->second.mainSurface = initial_editor_main_surface(*requestedWorkspace);
                }
                if (ctx)
                    epochengine::previewgrid::set_camera_mode(ctx.get(), it->second.viewportCameraMode);
                push_editor_log(it->second, "[info] Editor scene initialized.");
                push_editor_log(it->second, "[info] Use the bottom dock tabs for Project, Scripts, AI, Systems, and Output evidence. Dedicated editor windows/views are still being built.");
                push_editor_log(it->second, "[info] Scene viewport is owned by the active backend.");
                push_editor_log(it->second, "[info] Systems now tracks the shared simulation-time spine for pacing and fixed-step diagnostics.");
                if (!requestedProjectId.empty() && it->second.projectId == requestedProjectId)
                    push_editor_log(it->second, std::string("[project] Preloaded active project from environment: ") + requestedProjectId + ".");
                if (it->second.automationCommand == EditorAutomationCommand::SmartUpdate)
                {
                    it->second.autoUpdateCheckQueued = false;
                    push_editor_log(it->second, "[info] Auto command armed: smart update.");
                }
                else if (it->second.automationCommand == EditorAutomationCommand::SourceUpdate)
                {
                    it->second.autoUpdateCheckQueued = false;
                    push_editor_log(it->second, "[info] Auto command armed: source update.");
                }
                else if (it->second.automationCommand == EditorAutomationCommand::AssetsInteractionProof)
                {
                    it->second.autoUpdateCheckQueued = false;
                    push_editor_log(it->second, "[info] Auto command armed: Assets interaction proof.");
                }
                else if (it->second.automationCommand
                    == EditorAutomationCommand::CandidateLabSmokeChoose
                    || it->second.automationCommand
                        == EditorAutomationCommand::CandidateLabSmokeKeep)
                {
                    it->second.autoUpdateCheckQueued = false;
                    push_editor_log(it->second, "[candidate-lab] Native PID/context comparison smoke armed.");
                }
                else if (it->second.automationCommand
                    == EditorAutomationCommand::SelfCodingLocalSmoke)
                {
                    it->second.autoUpdateCheckQueued = false;
                    it->second.mainSurface = EditorMainSurface::AISandbox;
                    it->second.workspaceTab = EditorWorkspaceTab::AI;
                    it->second.aiWorkspaceDomain = AiWorkspaceDomain::Control;
                    it->second.showInspector = true;
                    it->second.showAiChat = true;
                    const auto developmentIndex =
                        tool_pane_index(EditorToolPane::AiDevelopment);
                    it->second.toolPaneOpen[developmentIndex] = true;
                    it->second.toolPaneDockRegions[developmentIndex] =
                        EditorToolDockRegion::BottomRight;
                    it->second.activeBottomRightPaneRoute = "pane.ai_development";
                    it->second.selfCodingSmokeWarmupFrames = 5u;
                    push_editor_log(
                        it->second,
                        "[self-coding-smoke] Plain-language local-model Candidate Lab smoke armed.");
                }
            }
            return it->second;
        }

    } // namespace

    namespace
    {
        void request_editor_task_session_stop(EditorState& editor)
        {
            // Signal every common owned cancellable task before TaskGraph
            // destruction drains its workers. Keep futures and transcripts
            // alive until owned process cancellation settles. Private source
            // work retains its separate cancellation owner below.
            editor.projectBuildRunAfterBuild = false;
            editor.aiProjectScriptRunAfterBuild = false;
            editor.aiProjectScriptRepairQueued = false;
            editor.aiToolTestAfterBuild = false;
            if (editor.projectBuildPending && editor.taskScheduler
                && editor.projectBuildCancellation.valid())
                (void)cancel_project_build(editor);
            if (editor.taskScheduler)
                for (const auto ticket : {editor.scriptBuildCancellation,
                    editor.planetaryCancellation, editor.planetaryPreviewCancellation,
                    editor.assetGenerationCancellation, editor.modelShorelineCancellation, editor.aiDirectRuntimeCancellation,
                    editor.aiAuthoringApplyCancellation, editor.aiToolTestCancellation,
                    editor.aiContinuousBuildCancellation})
                    if (ticket.valid()) (void)editor.taskScheduler->cancel(ticket);
            if (editor.modelShorelinePending && editor.modelImportReview)
            {
                editor.modelImportReview->shoreline.reset();
                editor.aiToolStatus = editor.assetGeneration.status = "Shoreline import stopped. The scene is unchanged.";
            }
        }

        // Called by the owning context while parking, or after detachment for
        // close. No storage lock spans native attachment removal or a join.
        [[nodiscard]] bool editor_automation_input_reserved(const EditorState& editor, const AiChat& chat)
        {
            // Derive this every frame from owned work, never from a persisted
            // goal/status latch. Review, setup and an idle retained plan unlock.
            // Streaming LOD is deliberately excluded: navigation drives it.
            const bool hostWork = editor.aiDirectRuntimePending.has_value()
                || editor.aiAuthoringApplyPending.has_value()
                || editor.aiToolTestPending.has_value()
                || editor.aiContinuousBuildPending.has_value()
                || editor.projectBuildPending.has_value()
                || (editor.aiProjectScriptRunAfterBuild && editor.scriptBuildPending.has_value())
                || editor.assetGenerationPending.has_value()
                || editor.modelShorelinePending.has_value()
                || editor.forestSurfacePending.has_value()
                || editor.planetaryAttachmentPending.has_value()
                || editor.planetaryGeneratePending.has_value()
                || editor.planetaryFeatureReviewPending.has_value()
                || editor.planetaryFeatureAdmissionPending.has_value()
                || editor.waterAttachmentPending.has_value();
            if (hostWork) return true;
            return chat.pending || chat.worker.joinable();
        }

        void request_editor_automation_stop(EditorState& editor)
        {
            request_editor_task_session_stop(editor);
            if (editor.taskScheduler && editor.forestSurfaceCancellation.valid())
                (void)editor.taskScheduler->cancel(editor.forestSurfaceCancellation);
            if (editor.forestSurfacePlacement && editor.forestSurfacePlacement->refreshAttachments)
                editor.projectStatus = "Model and spawn placement refresh cancelled.";
            // A ready result can no longer observe cancellation. Revoke its
            // scene commit now; the normal poll still owns future retirement.
            editor.forestSurfacePlacement.reset();
            editor.aiGoalRunning = false;
            editor.aiGoalPlanNextQueued = false;
            editor.aiGoalTools.stopped = true;
            editor.aiGoalTools.waiting = false;
            if (editor.aiPackageRequest) editor.aiPackageRequest->cancelled = true;
            // Preserve pending futures and execution receipts until their owner
            // settles them. Stop cannot turn a cancelled operation into success.
        }

        void retire_detached_chat(const std::shared_ptr<AiChat>& chat)
        {
            if (!chat)
                return;
            // Interrupt only this request, including serial-gate waits. Keep
            // its state alive until the transport worker has actually returned.
            (void)chat->cancel_pending();
            if (chat->worker.joinable())
                chat->worker.join();
            if (chat->pending && chat->pending->stage.load(std::memory_order_acquire)
                    == ai::ModelRequestStage::retirement_failed)
                chat->requestRetirementFailed = true;
        }

    }

    namespace
    {
        [[nodiscard]] bool consume_failed_project_ai_reply(EditorState& editor,
            const AiChat& chat, AiDeferredRequestKind kind, bool publishTrace = true)
        {
            if (chat.latestTerminalFailure == ai::ModelTerminalFailure::none
                || (kind != AiDeferredRequestKind::Authoring
                    && kind != AiDeferredRequestKind::Tooling)) return false;
            const std::string status = chat.latestTerminalStatus.empty()
                ? "The model request failed before a completed plan was received. No change was applied."
                : chat.latestTerminalStatus;
            if (kind == AiDeferredRequestKind::Authoring)
            {
                editor.aiAuthoringPlan = {};
                editor.aiAuthoringPlanApplied = false;
                if (editor.aiAuthoringPlanForGoal)
                {
                    editor.aiGoalRunning = false;
                    editor.aiGoalPlanNextQueued = false;
                }
                editor.aiAuthoringPlanForGoal = false;
                editor.aiAuthoringStatus = status;
            }
            else
            {
                editor.aiToolPlan = {};
                editor.aiToolPlanApplied = false;
                editor.aiToolStatus = status;
            }
            if (publishTrace)
                push_editor_log(editor, (kind == AiDeferredRequestKind::Authoring
                    ? "[ai-project] " : "[ai-tool] ") + status);
            return true;
        }
