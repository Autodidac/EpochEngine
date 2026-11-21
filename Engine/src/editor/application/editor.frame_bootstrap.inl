// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        EditorFrameResult result{};
        if (!ctx)
            return result;
        epoch_editor_perf::current_frame_watch().mark(epoch_editor_perf::FrameStage::editor_entry);
        epoch_editor_perf::FrameTimer frameTimer{};

        auto& editor = editor_state_for(ctx);
        retire_planetary_preview_publication(editor);
        advance_editor_timeline(editor);
        pump_script_build(editor);
        const bool scriptOperationSettled = editor.aiProjectScriptRunAfterBuild && !editor.scriptBuildPending;
        if (scriptOperationSettled)
        {
            editor.aiProjectScriptRunAfterBuild = false;
            const bool current = editor.scriptBuildCompletedGeneration == editor.aiProjectScriptBuildGeneration
                && editor.projectRoot == editor.aiProjectScriptRunRoot
                && editor.scriptEditorPath == editor.aiProjectScriptRunPath
                && !editor.scriptEditorDirty && editor.scriptEditorText == editor.aiProjectScriptRunSource
                && editor.sceneDocumentRevision == editor.aiProjectScriptSceneRevision;
            bool diskCurrent{};
            if (current)
            {
                std::error_code error{};
                const auto size = std::filesystem::file_size(editor.aiProjectScriptRunPath, error);
                if (!error && size == editor.aiProjectScriptRunSource.size())
                {
                    std::ifstream file{editor.aiProjectScriptRunPath, std::ios::binary};
                    const std::string bytes{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
                    diskCurrent = file && bytes == editor.aiProjectScriptRunSource;
                }
            }
            if (!current || !diskCurrent)
                editor.aiAuthoringStatus = "Script build settled, but its project, scene or source changed. Automatic Run was cancelled.";
            else if (editor.scriptBuildSucceeded)
                editor.aiAuthoringStatus = editor_run_script(ctx.get(), editor.activeScript)
                    ? "Reviewed project script built and ran through the Epoch host."
                    : "Script built, but execution failed. Inspect the retained script diagnostics.";
            else
            {
                editor.aiProjectScriptCompilerDiagnostics = editor.scriptBuildStatus;
                editor.aiProjectScriptRepairQueued = true;
                editor.aiAuthoringRequest = editor.aiProjectScriptRunObjective;
                editor.aiAuthoringStatus = "Script build failed. The actual compiler diagnostics are retained for a corrected draft; Run was skipped.";
            }
            push_editor_log(editor, "[ai-project] " + editor.aiAuthoringStatus);
        }
        if (!editor.automationConsumed
            && editor.automationCommand
                == EditorAutomationCommand::AssetsInteractionProof)
        {
            editor.automationConsumed = true;
            const bool passed =
                run_assets_interaction_proof(editor, ctx);
            result.command = EditorCommand::Exit;
            result.command_argument = passed
                ? "assets_interaction_proof_pass"
                : "assets_interaction_proof_fail";
            result.scene_input_captured = true;
            return result;
        }
        const auto& application = editor_application_profile(editor.applicationKind);
        if (!editor_application_supports_surface(application, editor.mainSurface))
            editor.mainSurface = application.default_surface;
        editor.showOutliner = editor.showOutliner && application.panes.outliner;
        editor.showInspector = editor.showInspector && application.panes.inspector;
        editor.showConsoleDock = editor.showConsoleDock && application.panes.console;
        editor.showAiChat = editor.showAiChat && application.panes.ai_chat;
        gui::set_theme_cycle_interval(editor.themeCycleIntervalSeconds);
        const gui::ScopedTheme editorThemeScope{ editor.themePreference };
        gui::publish_native_window_theme(editor.themePreference);
        const gui::ScopedRoundedRectangles roundedRectScope{ editor.roundedRectangles };
        const std::shared_ptr<AiChat> chatState = chat_state_for(ctx);
        auto& chat = *chatState;
        if (scriptOperationSettled) chat.append_status(editor.aiAuthoringStatus);
        // Request ownership is independent of whether this application shows
        // an AI Chat pane (for example after switching to GUI or Plant Lab).
        chat.pump();
        bool automationInputReserved = editor_automation_input_reserved(editor, chat);
        bool automationEmergencyStopRequested = automationInputReserved
            && ctx->has_input_focus_safe()
            && (gui::dispatch_keyboard_shortcuts([](const gui::InputEvent& event) { return event.key == 27; })
                || ctx->is_key_down_safe(input::Key::Escape));
        if (!automationInputReserved)
            apply_detached_gui_hierarchy_selections(ctx.get(), editor);
        else
            (void)consume_detached_gui_hierarchy_selections(ctx.get());
        bool focusedCompanionOwnerChanged{};
        publish_detached_pane_projection(ctx.get(), editor, chat,
            &focusedCompanionOwnerChanged);
        frameTimer.mark_chat_ready();
        if (!chat.pending
            && chat.completionGeneration
                > editor.aiObservedCompletionGeneration)
        {
            editor.aiObservedCompletionGeneration =
                chat.completionGeneration;
            const AiDeferredRequestKind completedKind =
                chat.latestCompletionRequestKind;
#if !EPOCH_SOURCE_SELF_CODING
            constexpr bool sourceCompletion = false;
#endif
            if (completedKind == AiDeferredRequestKind::Authoring)
            {
                const bool obsoleteAuthoringReply =
                    chat.completionGeneration
                        <= editor.aiAuthoringIgnoredThroughGeneration;
                const bool expectedAuthoringReply =
                    editor.aiAuthoringAwaitingReply
                    && chat.completionGeneration
                        > editor.aiAuthoringRequestedGeneration;
                editor.aiAuthoringAwaitingReply = false;

                if (obsoleteAuthoringReply)
                {
                    editor.aiAuthoringPlan = {};
                    editor.aiAuthoringPlanApplied = false;
                    editor.aiAuthoringPlanForGoal = false;
                    editor.aiAuthoringStatus =
                        "Discarded an obsolete Project Assistant authoring reply because its goal changed.";
                    push_editor_log(
                        editor, "[ai-project] " + editor.aiAuthoringStatus);
                }
                else if (expectedAuthoringReply
                    && !consume_failed_project_ai_reply(editor, chat, completedKind))
                {
                    const std::string authoringReply = recover_prefixed_ai_protocol(
                        chat.latestRawReply,
                        editor.aiProjectScriptRequested ? "EPOCH_PROJECT_SCRIPT_V1" : "EPOCH_AUTHORING_PLAN_V1",
                        "EPOCH_AUTHORING_QUESTION_V1");
                    const auto blockingQuestion = !editor.aiAuthoringPlanForGoal
                        ? parse_ai_blocking_question(
                            authoringReply,
                            "EPOCH_AUTHORING_QUESTION_V1")
                        : std::optional<std::string>{};
                    if (blockingQuestion)
                    {
                        editor.aiAuthoringPlan = {};
                        editor.aiAuthoringPlanApplied = false;
                        editor.aiIntentClarificationPending = true;
                        editor.aiIntentClarificationResumeIntent =
                            AiChatIntent::ProjectAuthoring;
                        editor.aiIntentClarificationRequest =
                            editor.aiAuthoringRequest;
                        editor.aiIntentClarificationQuestion = *blockingQuestion;
                        editor.aiGoalRunning = editor.aiGoalPlanNextQueued = false;
                        editor.aiAuthoringStatus =
                            "I need one critical detail before I can stage the project change: "
                            + *blockingQuestion
                            + " Reply normally in chat; no special syntax or button is required.";
                        chat.append_status(editor.aiAuthoringStatus);
                        push_editor_log(
                            editor,
                            "[ai-project] Blocking authoring clarification: "
                                + *blockingQuestion);
                    }
                    else if (editor.aiProjectScriptRequested)
                    {
                        const auto source = parse_ai_project_script(authoringReply);
                        editor.aiAuthoringPlan = {};
                        editor.aiGoalRunning = editor.aiGoalPlanNextQueued = false;
                        editor.aiProjectScriptDraft = source.value_or(std::string{});
                        editor.aiAuthoringStatus = source
                            ? "Project script staged for " + editor.aiProjectScriptPath
                                + ". Review the source below, then Save Script or explicitly Save, Build + Run."
                            : "The model did not return a complete project script. No source was saved; use Retry.";
                        chat.append_status(editor.aiAuthoringStatus);
                        if (source) chat.append_status("Project script source:\n" + *source);
                        push_editor_log(editor, "[ai-project] " + editor.aiAuthoringStatus);
                    }
                    else
                    {
                        auto parsedPlan = epochengine::ai::parse_authoring_plan(
                            authoringReply);
                        bool recoveredBareCall = false;
                        if (!parsedPlan)
                        {
                            if (const auto recovered =
                                    recover_bare_ai_authoring_call(
                                        authoringReply))
                            {
                                auto recoveredPlan =
                                    epochengine::ai::parse_authoring_plan(
                                        *recovered);
                                if (recoveredPlan)
                                {
                                    parsedPlan = std::move(recoveredPlan);
                                    recoveredBareCall = true;
                                }
                            }
                        }

                        editor.aiAuthoringPlan = std::move(parsedPlan);
                        if (!editor.aiAuthoringPlan)
                            editor.aiGoalRunning = editor.aiGoalPlanNextQueued = false;
                        editor.aiAuthoringPlanApplied = false;
                        editor.aiAuthoringStatus = editor.aiAuthoringPlan
                            ? (recoveredBareCall
                                ? std::string{
                                    "Recovered the model's single bare CALL into "
                                    "the strict Epoch authoring envelope. "}
                                : std::string{})
                                + editor.aiAuthoringPlan.message
                                + " No scene changes have been applied yet; review the "
                                  "staged call and approve it below or simply reply "
                                  "yes / apply / do it."
                            : editor.aiAuthoringPlan.message;
                        chat.append_status(editor.aiAuthoringStatus);
                        push_editor_log(
                            editor,
                            "[ai-project] Authoring plan "
                                + std::string{
                                    epochengine::ai::authoring_plan_code_name(
                                        editor.aiAuthoringPlan.code)}
                                + ": " + editor.aiAuthoringStatus);
                    }
                }
            }
            else if (completedKind == AiDeferredRequestKind::Tooling)
            {
                const bool expectedToolReply =
                    editor.aiToolAwaitingReply
                    && chat.completionGeneration
                        > editor.aiToolRequestedGeneration;
                editor.aiToolAwaitingReply = false;
                if (expectedToolReply) editor.aiToolModelRecovery = {};
                if (expectedToolReply
                    && !consume_failed_project_ai_reply(editor, chat, completedKind))
                {
                    const std::string toolingReply = recover_prefixed_ai_protocol(
                        chat.latestRawReply,
                        "EPOCH_TOOL_PLAN_V1",
                        "EPOCH_TOOL_QUESTION_V1");
                    const auto blockingQuestion = parse_ai_blocking_question(
                        toolingReply,
                        "EPOCH_TOOL_QUESTION_V1");
                    if (blockingQuestion)
                    {
                        editor.aiToolPlan = {};
                        editor.aiToolPlanApplied = false;
                        editor.aiIntentClarificationPending = true;
                        editor.aiIntentClarificationResumeIntent =
                            AiChatIntent::ProjectTooling;
                        editor.aiIntentClarificationRequest =
                            editor.aiToolRequest;
                        editor.aiToolStatus =
                            "I need one critical detail before I can choose the project tool: "
                            + *blockingQuestion
                            + " Reply normally in chat; no special syntax or button is required.";
                        chat.append_status(editor.aiToolStatus);
                        push_editor_log(
                            editor,
                            "[ai-tool] Blocking tool clarification: "
                                + *blockingQuestion);
                    }
                    else
                    {
                        editor.aiToolPlan = admit_ai_project_tool_reply(editor.aiToolBoundRequest,toolingReply);
                        editor.aiToolPlanApplied = false;
                        editor.aiToolStatus = editor.aiToolPlan
                            ? editor.aiToolPlan.message
                                + " Nothing has executed; review the exact host tool "
                                  "and approve it below or reply yes / approve / run it."
                            : editor.aiToolPlan.message;
                        if (editor.aiToolPlan)
                        {
                            editor.aiToolStatus += "\nExact host operation: " + editor.aiToolPlan.plan.call.tool;
                            for (const auto& argument : editor.aiToolPlan.plan.call.arguments)
                                editor.aiToolStatus += " " + argument.name + "=" + argument.value;
                            editor.aiToolStatus += "\nOnly this one operation is proposed. Later requested steps have not run.";
                        }
                        if (editor.aiToolPlan
                            && (editor.aiToolPlan.plan.call.tool == "package.catalog"
                                || editor.aiToolPlan.plan.call.tool == "package.install"
                                || editor.aiToolPlan.plan.call.tool == "terrain.generate"))
                            editor.aiToolStatus = ai_project_package_tool_description(editor.aiToolPlan.plan.call)
                                + " Review and approve the host action below. Nothing has executed.";
                        if (!editor.aiToolPlan && editor.aiMethodSelection == 0
                            && editor.aiToolProjectId == editor.projectId
                            && editor.aiToolProjectRoot == editor.projectRoot
                            && editor.aiToolProjectGeneration == editor.projectLifecycle.evidence().generations.selected_project.generation
                            && editor.aiToolSceneRevision == editor.sceneDocumentRevision
                            && editor.aiToolGuiRevision == (editor.guiDocument ? editor.guiDocument->revision().sequence : 0u))
                        {
                            // Only a completed, rejected model proposal reaches
                            // this branch. Missing packages, host execution errors,
                            // cancellations and transport failures cannot score it.
                            editor.aiToolModelRecovery = editor_ai_project_model_recovery::offer_native(
                                editor.aiToolBoundRequest,
                                editor_ai_project_model_recovery::current_binding(editor, chat.completionGeneration),
                                chat.latestCanonicalModelKey, ai::native_runtime::snapshot(true));
                            const auto recovery = editor_ai_project_model_recovery::disclosure(editor.aiToolModelRecovery);
                            if (!recovery.empty()) editor.aiToolStatus += "\n" + recovery;
                        }
                        chat.append_status(editor.aiToolStatus);
                        push_editor_log(
                            editor,
                            "[ai-tool] Tool plan "
                                + std::string{
                                    epochengine::ai::tool_plan_code_name(
                                        editor.aiToolPlan.code)}
                                + ": " + editor.aiToolStatus);
                    }
                }
            }
            else if (completedKind == AiDeferredRequestKind::Chat)
            {
                // AiChat::pump already appended the assistant response. Keep
                // ordinary Project Assistant conversation out of the authoring
                // and tool parsers even if the model happens to mention one of
                // their protocol markers in prose.
                editor.aiAuthoringStatus =
                    chat.latestTerminalFailure == ai::ModelTerminalFailure::none
                        ? "Project Assistant response received."
                        : "Project Assistant request ended without an applicable response.";
            }
            else if (completedKind != AiDeferredRequestKind::None)
            {
                push_editor_log(
                    editor,
                    "[ai] Ignored a completed request with an unsupported channel tag.");
            }
        }
        if (editor.autoUpdateCheckQueued)
        {
            editor.autoUpdateCheckQueued = false;
            if (!core::cli::smoke_requested)
                start_editor_update_check(editor, true);
        }
        pump_editor_update_check(editor);
        pump_editor_source_update_worker(editor);

        auto resolve_layout_extent = [&]() noexcept
        {
            int resolvedWidth = 0;
            int resolvedHeight = 0;

            // Docked/direct-child backends can report stale renderer dimensions while
            // the parent window is actively resizing. The GUI owns the full client
            // layout, so use the live window extent first and let backend dimensions
            // remain a fallback only for non-windowed/headless contexts.
            if (ctx->windowData)
            {
                const int liveWidth = ctx->windowData->get_width();
                const int liveHeight = ctx->windowData->get_height();
                if (liveWidth > 0 && liveHeight > 0)
                {
                    resolvedWidth = liveWidth;
                    resolvedHeight = liveHeight;
                }
            }

            if (resolvedWidth <= 0 || resolvedHeight <= 0)
            {
                resolvedWidth = ctx->get_width_safe();
                resolvedHeight = ctx->get_height_safe();
            }

            return gui::Vec2{
                static_cast<float>((std::max)(1, resolvedWidth)),
                static_cast<float>((std::max)(1, resolvedHeight))
            };
        };

        // Editor GUI layout must follow the live client pane, not a stale
        // startup framebuffer, or docked panes can hide Inspector/AI Chat.
        const gui::Vec2 layoutExtent = resolve_layout_extent();
        const float w = layoutExtent.x;
        const float h = layoutExtent.y;
        const auto centered_modal_position = [&](const gui::Vec2 modalSize) noexcept -> gui::Vec2
            {
                return {
                    (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                    (std::max)(0.0f, (h - modalSize.y) * 0.5f)
                };
            };
        const auto fit_modal_size = [&](const gui::Vec2 desired, const gui::Vec2 minimum) noexcept -> gui::Vec2
            {
                const float maxWidth = (std::max)(1.0f, w - 64.0f);
                const float maxHeight = (std::max)(1.0f, h - 64.0f);
                const float minWidth = (std::min)(minimum.x, maxWidth);
                const float minHeight = (std::min)(minimum.y, maxHeight);
                return {
                    std::clamp(desired.x, minWidth, maxWidth),
                    std::clamp(desired.y, minHeight, maxHeight)
                };
            };
        const auto measure_modal_text = [](const std::string_view text, const float width) noexcept -> float
            {
                return gui::measure_wrapped_label_height(text, width);
            };
        const auto stabilize_modal_size = [&](gui::Vec2& stableSize, gui::Vec2& stableViewport, const gui::Vec2 measuredSize, const bool visible) noexcept -> gui::Vec2
            {
                if (!visible)
                {
                    stableSize = {};
                    stableViewport = {};
                    return measuredSize;
                }

                const bool viewportChanged =
                    std::abs(stableViewport.x - w) > 0.5f
                    || std::abs(stableViewport.y - h) > 0.5f;
                if (stableSize.x <= 0.0f || stableSize.y <= 0.0f || viewportChanged)
                {
                    stableSize = measuredSize;
                    stableViewport = { w, h };
                    return stableSize;
                }

                stableSize.x = std::clamp((std::max)(stableSize.x, measuredSize.x), 1.0f, (std::max)(1.0f, w - 64.0f));
                stableSize.y = std::clamp((std::max)(stableSize.y, measuredSize.y), 1.0f, (std::max)(1.0f, h - 64.0f));
                return stableSize;
            };
        const auto update_modal_flags = [&]() noexcept -> editor_update_modal::UpdateFlags
            {
                const bool sourceOnlyUpdate =
                    editor.lastUpdateCheck.source_update_available
                    && !editor.lastUpdateCheck.packaged_update_available;
                const bool installableUpdate =
                    editor.lastUpdateCheck.update_available
                    || editor.lastUpdateCheck.force_required
                    || editor.lastUpdateCheck.packaged_update_available
                    || editor.lastUpdateCheck.source_update_available
                    || editor.lastUpdateCheck.packaged_handoff_staged;
                const bool sourceWorkerRunning = editor.updateState == EditorUpdateState::SourceWorkerRunning;
                const bool updateRunning = editor.updateCheckPending.has_value() || sourceWorkerRunning;
                const bool restartReady = editor.updateState == EditorUpdateState::RestartReady;

                return editor_update_modal::UpdateFlags{
                    .checkFailed = editor.updateState == EditorUpdateState::Failed,
                    .sourceOnlyUpdate = sourceOnlyUpdate,
                    .sourceWorkerRunning = sourceWorkerRunning,
                    .updateRunning = updateRunning,
                    .restartReady = restartReady,
                    .installableUpdate = installableUpdate,
                    .projectSourceDownload = editor.updateProjectSourceDownloadPending,
                    .sourceCancelAvailable = editor_source_cancel_available(editor),
                    .sourceAuthorizationRunning =
                        editor_private_source_access_pending(editor)
                        && !editor.updateSourceCancelRequested,
                    .sourceApprovalPending = editor_private_source_approval_pending(editor)
                };
            };
        const auto updateConfirmModalLayout = editor_update_modal::measure_update_layout(
            { w, h },
            update_modal_flags(),
            editor.updateStatus,
            measure_modal_text);
        const gui::Vec2 measuredUpdateConfirmModalSize{
            updateConfirmModalLayout.size.x,
            updateConfirmModalLayout.size.y
        };
        const gui::Vec2 updateConfirmModalSize = stabilize_modal_size(
            editor.updateConfirmModalStableSize,
            editor.updateConfirmModalStableViewport,
            measuredUpdateConfirmModalSize,
            editor.showUpdateConfirmModal);
        const auto sourceUpdateConfirmModalLayout = editor_update_modal::measure_source_layout(
            { w, h },
            measure_modal_text);
        const gui::Vec2 measuredSourceUpdateConfirmModalSize{
            sourceUpdateConfirmModalLayout.size.x,
            sourceUpdateConfirmModalLayout.size.y
        };
        const gui::Vec2 sourceUpdateConfirmModalSize = stabilize_modal_size(
            editor.sourceUpdateConfirmModalStableSize,
            editor.sourceUpdateConfirmModalStableViewport,
            measuredSourceUpdateConfirmModalSize,
            editor.showSourceUpdateConfirmModal);
        const gui::Vec2 packageManagerModalSize = fit_modal_size(
            {
                std::clamp(w * 0.86f, 720.0f, 1700.0f),
                std::clamp(h * 0.88f, 500.0f, 1400.0f)
            },
            {540.0f, 420.0f});
        const gui::Vec2 aiModelConsentModalSize = fit_modal_size({ 660.0f, 430.0f }, { 520.0f, 380.0f });
        const gui::Vec2 voiceConsentModalSize = fit_modal_size({ 580.0f, 300.0f }, { 500.0f, 280.0f });
        // Settings uses the same retained, viewport-constrained shell as Model Manager.
        const gui::Vec2 editorSettingsDefaultSize{
            (std::min)(860.0f, w * 0.88f), (std::min)(800.0f, h * 0.88f)};
        const gui::FloatingWindowOptions editorSettingsWindowOptions{
            .id="epoch-editor-settings", .title="Editor Settings",
            .default_position={(std::max)(0.0f, (w-editorSettingsDefaultSize.x)*0.5f),
                (std::max)(0.0f, (h-editorSettingsDefaultSize.y)*0.5f)},
            .default_size=editorSettingsDefaultSize,
            .min_size={(std::min)(440.0f,w), (std::min)(320.0f,h)},
            .viewport_size={w,h}, .movable=true, .resizable=true, .closable=true,
            .draw_background=true, .top_layer=true, .capture_input=true,
            .constrain_to_viewport=true, .dim_background=true};
        if (editor.showSettingsModal)
        {
            editor.editorSettingsWindow.open = true;
            gui::normalize_floating_window(editor.editorSettingsWindow, editorSettingsWindowOptions);
        }
        if (!editor.showSettingsModal || !ctx->has_input_focus_safe())
        {
            editor.editorSettingsWindow.dragging = false;
            editor.editorSettingsWindow.resizing = false;
            editor.editorSettingsWindow.close_pressed = false;
        }
        const gui::Vec2 aiModelManagerDefaultSize{
            (std::min)(1000.0f, w * 0.88f), (std::min)(800.0f, h * 0.88f)};
        gui::FloatingWindowOptions aiModelManagerWindowOptions{
            .id="epoch-model-manager", .title="Model Manager",
            .default_position={(std::max)(0.0f, (w-aiModelManagerDefaultSize.x)*0.5f),
                (std::max)(0.0f, (h-aiModelManagerDefaultSize.y)*0.5f)},
            .default_size=aiModelManagerDefaultSize,
            .min_size={(std::min)(440.0f,w), (std::min)(320.0f,h)},
            .viewport_size={w,h}, .movable=true, .resizable=true, .closable=true,
            .draw_background=true, .top_layer=true, .capture_input=false,
            .constrain_to_viewport=true, .dim_background=true};
        if (editor.showAiModelManagerModal)
        {
            if (!editor.aiModelManagerOpenLogged) editor.aiModelManagerWindow.open = true;
            gui::normalize_floating_window(editor.aiModelManagerWindow, aiModelManagerWindowOptions);
        }
        if (!editor.showAiModelManagerModal || !ctx->has_input_focus_safe())
        {
            editor.aiModelManagerWindow.dragging = false;
            editor.aiModelManagerWindow.resizing = false;
            editor.aiModelManagerWindow.close_pressed = false;
        }
        const core::RoutedPanelDockDragProjection nativeDockProjection =
            core::routed_panel_dock_drag_projection();
        const bool otherPointerGrabActive = nativeDockProjection.active
            || editor.paneTitleDragActive
            || editor.layoutDrag != EditorLayoutDrag::None
            || editor.floatingGuiWindow.floating.dragging
            || editor.floatingGuiWindow.floating.resizing;
        const bool pointerGrabActive = otherPointerGrabActive
            || (editor.showSettingsModal && (editor.editorSettingsWindow.dragging
                || editor.editorSettingsWindow.resizing))
            || (editor.showAiModelManagerModal && (editor.aiModelManagerWindow.dragging
                || editor.aiModelManagerWindow.resizing));
#if !EPOCH_SOURCE_SELF_CODING
        constexpr bool nativeToolApprovalModalVisible = false;
#endif
        const gui::Vec2 aboutModalSize = fit_modal_size({600.0f, 416.0f}, {360.0f, 260.0f});
        auto modal_visible_now = [&editor, nativeToolApprovalModalVisible]() noexcept -> bool
        {
            return nativeToolApprovalModalVisible || editor.showAboutModal
                || editor.showSettingsModal
                || editor.showPackageManagerModal
                || editor.showAiModelConsentModal
                || editor.showVoiceConsentModal
                || editor.showUpdateConfirmModal
                || editor.showSourceUpdateConfirmModal;
        };

        const bool modalVisible = modal_visible_now();
        if (modalVisible)
        {
            editor.aiModelManagerWindow.dragging = false;
            editor.aiModelManagerWindow.resizing = false;
            editor.aiModelManagerWindow.close_pressed = false;
        }
        const auto aiModelManagerPress = gui::mouse_press_position();
        const bool aiModelManagerInputCovered = editor.showAiModelManagerModal
            && editor.aiModelManagerWindow.open && !modalVisible
            && editor.openMenu == TopMenu::None
            && !editor.outlinerContextMenu.menu.popup.open
            && (editor.aiModelManagerWindow.dragging || editor.aiModelManagerWindow.resizing
                || (!otherPointerGrabActive
                    && ((gui::is_mouse_over_content_clip()
                            && editor_point_in_rect(gui::mouse_position(),
                                editor.aiModelManagerWindow.position, editor.aiModelManagerWindow.size))
                        || (aiModelManagerPress && editor_point_in_rect(*aiModelManagerPress,
                            editor.aiModelManagerWindow.position, editor.aiModelManagerWindow.size)))));
        aiModelManagerWindowOptions.capture_input = aiModelManagerInputCovered;
        if (aiModelManagerInputCovered) result.scene_input_captured = true;
        if (editor.outlinerContextMenu.menu.popup.open
            && (!ctx->has_input_focus_safe()
                || gui::was_mouse_right_pressed()
                || editor.timeSnapshot.frame_index < editor.outlinerContextMenu.last_frame
                || editor.timeSnapshot.frame_index - editor.outlinerContextMenu.last_frame > 1u))
            gui::close_command_menu(editor.outlinerContextMenu.menu);
        if (editor.outlinerContextMenu.menu.popup.open)
        {
            gui::block_background_for_popup();
            result.scene_input_captured = true;
        }
        if (modalVisible || pointerGrabActive)
        {
            editor.openMenu = TopMenu::None;
            // Modal windows own all editor interaction until dismissed. The
            // EpochGui hit region protects widgets, while this flag also stops
            // scene picking, dragging, camera navigation, and document tools.
            result.scene_input_captured = true;
        }

        auto floating_gui_visible = [&editor, aiModelManagerInputCovered]() noexcept -> bool
        {
            return editor.floatingGuiWindow.visible || aiModelManagerInputCovered;
        };

        // Print Screen is an editor-level shortcut, even when a text editor or
        // modal owns keyboard input. Poll ONLY while this editor context owns
        // foreground focus; never register a system-wide hotkey or keyboard hook.
        // The existing clipboard capture runs after the present/menu frame.
        if (!automationInputReserved && ctx->has_input_focus_safe())
        {
            bool printScreenHeld = ctx->is_key_held_safe(input::Key::PrintScreen);
#if defined(_WIN32)
            // Some Windows backends report VK_SNAPSHOT on key release only.
            // The asynchronous state is inspected solely while focused.
            printScreenHeld = printScreenHeld || ((::GetAsyncKeyState(VK_SNAPSHOT) & 0x8000) != 0);
#endif
            const bool printScreenPressed =
                ctx->is_key_down_safe(input::Key::PrintScreen)
                || (printScreenHeld && !editor.printScreenHeld);
            editor.printScreenHeld = printScreenHeld;
            const bool modified =
                ctx->is_key_held_safe(input::Key::LeftAlt)
                || ctx->is_key_held_safe(input::Key::RightAlt)
                || ctx->is_key_held_safe(input::Key::LeftControl)
                || ctx->is_key_held_safe(input::Key::RightControl)
                || ctx->is_key_held_safe(input::Key::LeftSuper)
                || ctx->is_key_held_safe(input::Key::RightSuper);
            // The active renderer may not translate F12 into the shared key
            // table. Check its focused Win32 state as a second source, with
            // edge detection so one held key produces only one capture.
            bool f12Held = ctx->is_key_held_safe(input::Key::F12);
#if defined(_WIN32)
            f12Held = f12Held || ((::GetAsyncKeyState(VK_F12) & 0x8000) != 0);
#endif
            const bool fallbackScreenshot = ctx->is_key_down_safe(input::Key::F12)
                || (f12Held && !editor.f12Held);
            editor.f12Held = f12Held;
            if ((printScreenPressed || fallbackScreenshot) && !modified
                && editor.screenshotCaptureAfterFrames == 0u)
            {
                editor.screenshotCaptureAfterFrames = 2u;
                result.scene_input_captured = true;
            }
        }
        else
        {
            editor.printScreenHeld = false;
            editor.f12Held = false;
        }

        const bool workspaceShortcutInputAvailable =
            !automationInputReserved && ctx->has_input_focus_safe()
            && !editor.outlinerContextMenu.menu.popup.open
            && !modalVisible
            && !pointerGrabActive
            && editor.openMenu == TopMenu::None
            && !floating_gui_visible();
        const bool overlayPriorityActive =
            editor.openMenu != TopMenu::None
            || editor.outlinerContextMenu.menu.popup.open
            || modalVisible
            || floating_gui_visible()
            || gui::select_box_popup_open()
            || pointerGrabActive;
        ctx->set_gui_overlay_priority(overlayPriorityActive);

        // A selected editor may scan installed model profiles after first paint.
        // This is a read-only asynchronous catalog update, NOT saved-set loading.
        // The project launcher never runs this editor-frame path.
        if (!editor.aiNativeAutoScanAttempted)
        {
            if (editor.aiNativeAutoScanDelayFrames != 0u)
                --editor.aiNativeAutoScanDelayFrames;
            else
            {
                editor.aiNativeAutoScanAttempted = true;
                const auto nativeState = epochengine::ai::native_runtime::snapshot(false);
                if (!nativeState.busy && !nativeState.loaded)
                    (void)epochengine::ai::native_runtime::refresh();
            }
        }

        frameTimer.mark_ui_start();
        gui::clear_modal_input_capture();
        if (editor.showUpdateConfirmModal)
            gui::begin_modal_background_capture(centered_modal_position(updateConfirmModalSize), updateConfirmModalSize);
        else if (editor.showSourceUpdateConfirmModal)
            gui::begin_modal_background_capture(centered_modal_position(sourceUpdateConfirmModalSize), sourceUpdateConfirmModalSize);
        else if (editor.showAiModelConsentModal)
            gui::begin_modal_background_capture(centered_modal_position(aiModelConsentModalSize), aiModelConsentModalSize);
        else if (editor.showVoiceConsentModal)
            gui::begin_modal_background_capture(centered_modal_position(voiceConsentModalSize), voiceConsentModalSize);
        else if (!editor.showSettingsModal && editor.showPackageManagerModal)
            gui::begin_modal_background_capture(centered_modal_position(packageManagerModalSize), packageManagerModalSize);
        else if (!editor.showSettingsModal && editor.showAboutModal)
            gui::begin_modal_background_capture(centered_modal_position(aboutModalSize), aboutModalSize);

        // Settings remains exclusive; restore its events only around its floating
        // owner, then keep later background controls suppressed for this frame.
        const bool editorSettingsOwnsInput = editor.showSettingsModal
            && !nativeToolApprovalModalVisible
            && !editor.showUpdateConfirmModal && !editor.showSourceUpdateConfirmModal
            && !editor.showAiModelConsentModal && !editor.showVoiceConsentModal;
        std::optional<gui::ScopedInputSuppression> editorSettingsBackgroundInput;
        if (editorSettingsOwnsInput) editorSettingsBackgroundInput.emplace();

        // Reserve covered events before background controls draw; restore them
        // immediately before the same floating owner draws later in this frame.
        std::optional<gui::ScopedInputSuppression> aiModelManagerBackgroundInput;
        if (aiModelManagerInputCovered) aiModelManagerBackgroundInput.emplace();

        // A host resize must not reinterpret the last splitter-drag pixel in
        // a new client width. Keep the user's fraction even for one-pixel steps.
        if (editor.layoutDrag == EditorLayoutDrag::BottomColumns
            && editor.lastLayoutExtent.x > 0.0f
            && editor.lastLayoutExtent.x != layoutExtent.x)
            editor.layoutDrag = EditorLayoutDrag::None;
        if (editor.lastLayoutExtent.x > 0.0f
            && editor.lastLayoutExtent.y > 0.0f
            && (std::abs(editor.lastLayoutExtent.x - layoutExtent.x) > 1.0f
                || std::abs(editor.lastLayoutExtent.y - layoutExtent.y) > 1.0f))
        {
            editor.surfaceSettleFrames = (std::max)(editor.surfaceSettleFrames, 1);
        }
        editor.lastLayoutExtent = layoutExtent;

        auto clamp_layout = [](float value, float lo, float hi) noexcept
        {
            if (hi < lo)
                hi = lo;
            return std::clamp(value, lo, hi);
        };

        if (editor.dockPreferenceSurface != editor.mainSurface)
        {
            const std::size_t previous = editor_main_surface_index(
                editor.dockPreferenceSurface);
            editor.workspaceBottomGridSplits[previous] = editor.bottomGridSplit;
            editor.workspaceDockSplits[previous] = editor.dockSplit;
            editor.workspaceOutputFollow[previous] = editor.outputFollowTail;
            editor.workspaceAiChatFollow[previous] = editor.aiChatFollowTail;

            const std::size_t active = editor_main_surface_index(
                editor.mainSurface);
            // Second-row workspace selection must never replace a user's
            // dock geometry. Older builds stored per-workspace sizes and
            // silently loaded them on every click; retain the stored fields
            // only for backward-compatible preference serialization.
            editor.workspaceBottomGridSplits[active] = editor.bottomGridSplit;
            editor.workspaceDockSplits[active] = editor.dockSplit;
            editor.outputFollowTail = editor.workspaceOutputFollow[active];
            editor.aiChatFollowTail = editor.workspaceAiChatFollow[active];
            editor.dockPreferenceSurface = editor.mainSurface;
            editor.layoutDrag = EditorLayoutDrag::None;
            editor.surfaceSettleFrames = (std::max)(
                editor.surfaceSettleFrames, 1);
        }

        editor.outlinerSplit = std::clamp(editor.outlinerSplit, 0.06f, 0.70f);
        editor.inspectorSplit = std::clamp(editor.inspectorSplit, 0.06f, 0.70f);
        editor.dockSplit = std::clamp(editor.dockSplit, 0.08f, 0.80f);

        // Presentation only: stored pane visibility and dock fractions remain
        // untouched, so Stop, Escape and Exit restore the authored layout.
        const bool projectViewportMaximized = project_runtime_viewport_maximized(editor);
        constexpr auto toolPaneRoutes = editor_tool_pane_routes();
        constexpr auto toolPaneLabels = editor_tool_pane_labels();
        auto pane_supported = [&](EditorToolPane pane) noexcept
        {
#if !(EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR)
            if (pane == EditorToolPane::TileMap)
                return false;
#endif
            if (is_outliner_tool_pane(pane))
                return application.panes.outliner;
            if (pane == EditorToolPane::AiControls)
                return false; // Retired persisted slot; Model Manager owns settings.
            if (is_inspector_tool_pane(pane))
                return application.panes.inspector;
            if (is_console_tool_pane(pane))
                return application.panes.console;
            return pane == EditorToolPane::AiChat
                && application.panes.ai_chat;
        };
        auto pane_family_enabled = [&](EditorToolPane pane) noexcept
        {
            if (projectViewportMaximized) return false;
            if (!pane_supported(pane))
                return false;
            if (is_outliner_tool_pane(pane))
                return editor.showOutliner;
            if (is_inspector_tool_pane(pane))
                return editor.showInspector;
            if (is_console_tool_pane(pane))
                return editor.showConsoleDock;
            return pane == EditorToolPane::AiChat && editor.showAiChat;
        };
        auto pane_is_shown = [&](std::string_view route) noexcept
        {
            const auto pane = editor_tool_pane_from_route(route);
            return pane
                && pane_family_enabled(*pane)
                && editor.toolPaneOpen[tool_pane_index(*pane)];
        };
        auto set_pane_shown = [&](std::string_view route, bool shown) noexcept
        {
            const auto pane = editor_tool_pane_from_route(route);
            if (!pane)
                return;
            editor.toolPaneOpen[tool_pane_index(*pane)] = shown;
            if (!shown)
                return;
            if (is_outliner_tool_pane(*pane))
                editor.showOutliner = true;
            else if (is_inspector_tool_pane(*pane))
                editor.showInspector = true;
            else if (is_console_tool_pane(*pane))
                editor.showConsoleDock = true;
            else if (*pane == EditorToolPane::AiChat)
                editor.showAiChat = true;
        };
        auto pane_dock_region = [&](std::string_view route) noexcept
        {
            const auto pane = editor_tool_pane_from_route(route);
            return pane
                ? editor.toolPaneDockRegions[tool_pane_index(*pane)]
                : EditorToolDockRegion::Left;
        };
        auto set_pane_dock_region = [&](std::string_view route, EditorToolDockRegion region) noexcept
        {
            const auto pane = editor_tool_pane_from_route(route);
            if (pane)
                editor.toolPaneDockRegions[tool_pane_index(*pane)] = region;
        };
        auto active_pane_storage = [&](EditorToolDockRegion region) -> std::string&
        {
            switch (region)
            {
            case EditorToolDockRegion::Right:
                return editor.activeRightPaneRoute;
            case EditorToolDockRegion::BottomLeft:
                return editor.activeBottomLeftPaneRoute;
            case EditorToolDockRegion::BottomRight:
                return editor.activeBottomRightPaneRoute;
            case EditorToolDockRegion::Left:
            default:
                return editor.activeLeftPaneRoute;
            }
        };
        auto pane_available = [&](std::string_view route) noexcept
        {
            return pane_is_shown(route) && !pane_route_is_detached(route);
        };
        auto ordered_pane_indices_for_region = [&](EditorToolDockRegion region)
        {
            std::vector<std::size_t> indices{};
            indices.reserve(kEditorToolPaneCount);
            for (std::size_t index = 0; index < toolPaneRoutes.size(); ++index)
            {
                if (pane_available(toolPaneRoutes[index])
                    && editor.toolPaneDockRegions[index] == region)
                    indices.push_back(index);
            }
            std::sort(indices.begin(), indices.end(), [&](std::size_t left, std::size_t right)
            {
                const std::uint32_t leftOrder = editor.toolPaneTabOrder[left];
                const std::uint32_t rightOrder = editor.toolPaneTabOrder[right];
                return leftOrder != rightOrder ? leftOrder < rightOrder : left < right;
            });
            return indices;
        };
        auto active_pane_for_region = [&](EditorToolDockRegion region) -> std::string_view
        {
            const std::string& configured = active_pane_storage(region);
            if (pane_available(configured) && pane_dock_region(configured) == region)
                return configured;
            for (const std::size_t index : ordered_pane_indices_for_region(region))
                return toolPaneRoutes[index];
            return {};
        };
        auto region_has_visible_pane = [&](EditorToolDockRegion region) noexcept
        {
            return !active_pane_for_region(region).empty();
        };
        auto set_active_pane = [&](EditorToolDockRegion region, std::string_view route)
        {
            active_pane_storage(region).assign(route);
        };


        const bool layout_outliner_visible =
            region_has_visible_pane(EditorToolDockRegion::Left);
        const bool layout_inspector_visible =
            region_has_visible_pane(EditorToolDockRegion::Right);
        const bool bottom_left_visible =
            region_has_visible_pane(EditorToolDockRegion::BottomLeft);
        const bool bottom_right_visible =
            region_has_visible_pane(EditorToolDockRegion::BottomRight);
        const bool bottom_visible = bottom_left_visible || bottom_right_visible;

        std::optional<taskgraph::GraphSnapshot> frameTaskSnapshot;
        const auto observe_frame_tasks = [&]() -> const taskgraph::GraphSnapshot&
        {
            if (!frameTaskSnapshot)
                frameTaskSnapshot = editor.taskScheduler
                    ? editor.taskScheduler->snapshot(0u) : taskgraph::GraphSnapshot{};
            return *frameTaskSnapshot;
        };
        const auto task_counts_for = [&](const editor_tasks::ActivitySnapshot& toolbarActivity)
        {
            const bool toolbarModelQueued = chat.pending
                && chat.pending->stage.load(std::memory_order_acquire) == ai::ModelRequestStage::queued;
            const auto toolbarRunning = toolbarActivity.running_tasks
                + (chat.pending && !toolbarModelQueued ? 1u : 0u)
                ;
            const auto toolbarQueued = toolbarActivity.queued_tasks + toolbarActivity.waiting_tasks
                + (toolbarModelQueued ? 1u : 0u)
                + (editor.aiDeferredRequestKind != AiDeferredRequestKind::None
                    && !editor.aiWorkLocalHttp ? 1u : 0u)
                + (ai_work_pending(editor) ? 1u : 0u);
            return std::array{toolbarRunning, toolbarQueued};
        };

#if defined(_WIN32)
        constexpr std::string_view toolbarOs = "Windows";
#elif defined(__linux__)
        constexpr std::string_view toolbarOs = "Linux";
#elif defined(__APPLE__)
        constexpr std::string_view toolbarOs = "macOS";
#else
        constexpr std::string_view toolbarOs = "Desktop";
#endif
#if defined(_M_X64) || defined(__x86_64__)
        constexpr std::string_view toolbarArch = "x64";
#elif defined(_M_ARM64) || defined(__aarch64__)
        constexpr std::string_view toolbarArch = "ARM64";
#else
        constexpr std::string_view toolbarArch = "x86";
#endif
#if defined(_MSC_VER)
        constexpr std::string_view toolbarCompiler = "MSVC";
#elif defined(__clang__)
        constexpr std::string_view toolbarCompiler = "Clang";
#elif defined(__GNUC__)
        constexpr std::string_view toolbarCompiler = "GCC";
#else
        constexpr std::string_view toolbarCompiler = "C++";
#endif
        std::optional<platform::work_admission::GpuResourceSample> frameGpuSample;
        const auto observe_frame_gpu = [&]() -> const platform::work_admission::GpuResourceSample&
        {
            if (!frameGpuSample)
                frameGpuSample = platform::work_admission::sample_gpu_telemetry();
            return *frameGpuSample;
        };
        constexpr std::size_t toolbarScopeIndex = 4u;
        constexpr std::size_t toolbarContextIndex = 5u;
        constexpr std::size_t toolbarFpsIndex = 6u;
        constexpr std::size_t toolbarTaskIndex = 7u;
        constexpr std::size_t toolbarVersionIndex = 8u;
        constexpr std::size_t toolbarBuildIndex = 9u;
        constexpr std::size_t toolbarTierIndex = 10u;
        std::array<gui::StatusStripItem, 11> toolbarStatusItems{};
        // The status item views borrow these strings through the toolbar draw.
        std::array<std::string, 11> toolbarStatus{}, toolbarCompactStatus{}, toolbarMinimalStatus{};
        std::string toolbarCpuDetail, toolbarRamDetail, toolbarGpuDetail, toolbarVramDetail, toolbarTierDetail, toolbarTaskDetail;
        std::string toolbarVramWidthReference;
        const unsigned int telemetryDetail = editor.showTelemetry ? (std::min)(editor.telemetryDetail, 3u) : 4u;
        const bool collectToolbarResources = telemetryDetail < 3u;
        if (editor.showTelemetry)
        {
            const auto toolbarActivity = telemetryDetail < 2u
                ? editor_tasks::activity_snapshot(observe_frame_tasks()) : editor_tasks::ActivitySnapshot{};
            const auto [toolbarRunning, toolbarQueued] = task_counts_for(toolbarActivity);
            // Resource telemetry leads the status strip and survives compression.
            // Trailing identity fields yield before overlapping menus or tabs.
            // The top Tasks chip is an entry point; the detailed workbench owns
            // the task graph, diagnostics and self-iteration timeline.
            // Compact right-aligned identity / activity / backend / support tier. No
            // labels repeated on every frame; each chip remains individually actionable.
            // Actual active tasks are shown, never a fabricated worker count.
            if (ctx->windowData)
            {
                const double chromeNow = std::chrono::duration<double>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                (void)editor.chromeFrameRate.observe(
                    ctx->windowData->successfulFrameGeneration.load(std::memory_order_acquire),
                    chromeNow, ctx->windowData);
                const double sample = editor.chromeFrameRate.live_rate();
                editor.chromeFpsSample = sample;
                editor.chromeFpsMeasured = editor.chromeFrameRate.has_sample();
            }
            else
            {
                editor.chromeFrameRate = {};
                editor.chromeFpsSample = 0.0;
                editor.chromeFpsMeasured = false;
            }
            const double chromeThrottle = ctx->windowData
                ? ctx->windowData->editorThrottleHz.load(std::memory_order_relaxed) : 0.0;
            const bool chromeNativeVsync = ctx->windowData
                && ctx->windowData->editorNativeVsyncActive.load(std::memory_order_acquire);
            const double chromePacing = ctx->windowData
                ? ctx->windowData->editorFramePacingHz.load(std::memory_order_relaxed) : 0.0;
            const bool chromeCompilerActive = epochengine::editor::build_pressure::active();
            const bool chromeWorkActive = ctx->windowData
                && ctx->windowData->editorForegroundWorkActive.load(std::memory_order_relaxed);
            const auto chromeActivity = ctx->frame_pacing_activity();
            const bool chromeBackground = chromeActivity == perf::frame_activity::background;
            const std::string_view toolbarFpsPhase = editor.projectBuildPending || chromeCompilerActive
                ? "Build" : chromeWorkActive ? "Working" : editor.projectRuntimeProcess.valid() ? "Project host"
                : chromeBackground ? "Unfocused" : "Editor";
            const bool chromeThrottleLimited = chromeThrottle > 0.0
                && (!(chromePacing > 0.0) || chromeThrottle < chromePacing);
            const double chromeEffectiveCap = chromeThrottleLimited ? chromeThrottle : chromePacing;
            const std::string_view chromeCapReason = chromeThrottleLimited
                ? chromeCompilerActive || editor.projectBuildPending ? "build " : chromeWorkActive ? "work " : "CPU "
                : chromeActivity == perf::frame_activity::minimized ? "minimized " : "pacing ";
            const bool chromeRateMeasured = std::isfinite(editor.chromeFpsSample)
                && editor.chromeFpsMeasured;
            const bool chromeShowsBackgroundCap = chromeRateMeasured && chromeBackground
                && !chromeThrottleLimited && !chromeNativeVsync && chromeEffectiveCap > 0.0;
            const std::string toolbarFps = chromeRateMeasured
                ? chromeShowsBackgroundCap
                    ? std::round(editor.chromeFpsSample) == std::round(chromeEffectiveCap)
                        ? epochengine::format_text("{:.0f} FPS unfocused", chromeEffectiveCap)
                        : epochengine::format_text("{:.0f}/{:.0f} FPS unfocused",
                            editor.chromeFpsSample, chromeEffectiveCap)
                : chromeThrottleLimited
                    ? epochengine::format_text("{:.0f} FPS / {:.0f} {}cap", editor.chromeFpsSample,
                        chromeEffectiveCap, chromeCapReason)
                    : chromeNativeVsync
                        ? epochengine::format_text("{:.0f} FPS / VSync", editor.chromeFpsSample)
                        : chromeEffectiveCap > 0.0
                            ? epochengine::format_text("{:.0f} FPS / {:.0f} {}cap", editor.chromeFpsSample,
                                chromeEffectiveCap, chromeCapReason)
                            : epochengine::format_text("{:.0f} FPS / uncapped", editor.chromeFpsSample)
                : std::string{"-- FPS"};
            const std::string toolbarFpsFull = toolbarFpsPhase == "Editor" || (chromeShowsBackgroundCap && toolbarFpsPhase == "Unfocused")
                ? toolbarFps : epochengine::format_text("{} {}", toolbarFpsPhase, toolbarFps);
            // Compression must not conceal why the measured rate is limited.
            // The strip can omit other identity fields before losing cap evidence.
            const std::string toolbarFpsCompact = toolbarFpsFull;
            const bool epochUsage = editor.showEpochResourceUsage;
            const auto toolbarResources = epochUsage || !collectToolbarResources ? platform::work_admission::ResourceSample{}
                : platform::work_admission::sample_host_resources();
            const auto epochResources = epochUsage && collectToolbarResources ? platform::work_admission::sample_epoch_resources()
                : platform::work_admission::EpochResourceSample{};
            const auto toolbarGpu = epochUsage || !collectToolbarResources ? epochResources.gpu : observe_frame_gpu();
            const double toolbarCpuFraction = epochUsage
                ? epochResources.cpu_busy_fraction : toolbarResources.cpu_busy_fraction;
            const bool toolbarCpuValid = (epochUsage ? epochResources.cpu_valid : toolbarResources.cpu_valid)
                && std::isfinite(toolbarCpuFraction) && toolbarCpuFraction >= 0.0 && toolbarCpuFraction <= 1.0;
            const std::string toolbarCpu = toolbarCpuValid
                ? epochengine::format_text("{:.0f}%", toolbarCpuFraction * 100.0)
                : "--";
            const auto totalRamBytes = epochUsage
                ? epochResources.total_memory_bytes : toolbarResources.total_memory_bytes;
            const bool hostRamValid = toolbarResources.memory_valid
                && toolbarResources.available_memory_bytes <= totalRamBytes;
            const auto usedRamBytes = epochUsage ? epochResources.resident_memory_bytes
                : hostRamValid ? totalRamBytes - toolbarResources.available_memory_bytes : 0u;
            const bool toolbarRamValid = (epochUsage ? epochResources.memory_valid : hostRamValid)
                && totalRamBytes > 0u && usedRamBytes <= totalRamBytes;
            const double toolbarRamFraction = toolbarRamValid
                ? static_cast<double>(usedRamBytes) / totalRamBytes : 0.0;
            const std::string toolbarRamPercent = toolbarRamValid
                ? epochengine::format_text("{:.0f}%", toolbarRamFraction * 100.0) : "--";
            const std::string toolbarCpuLabel = std::string{"CPU "} + toolbarCpu;
            const std::string toolbarRamLabel = "RAM " + toolbarRamPercent;
            const std::string toolbarCpuCompact = std::string{"C "} + toolbarCpu;
            const std::string toolbarRamCompact = "R " + toolbarRamPercent;
            const bool toolbarGpuSourceAvailable = epochUsage
                ? toolbarGpu.source != platform::work_admission::GpuTelemetrySource::unavailable
                : toolbarGpu.source == platform::work_admission::GpuTelemetrySource::nvidia_nvml;
            const bool toolbarGpuValid = toolbarGpuSourceAvailable
                && toolbarGpu.busy_valid && std::isfinite(toolbarGpu.busy_fraction)
                && toolbarGpu.busy_fraction >= 0.0 && toolbarGpu.busy_fraction <= 1.0;
            const bool toolbarVramValid = toolbarGpuSourceAvailable
                && toolbarGpu.memory_valid && toolbarGpu.dedicated_total_bytes > 0u
                && toolbarGpu.dedicated_used_bytes <= toolbarGpu.dedicated_total_bytes;
            const double toolbarVramFraction = toolbarVramValid
                ? static_cast<double>(toolbarGpu.dedicated_used_bytes) / toolbarGpu.dedicated_total_bytes : 0.0;
            const std::string toolbarGpuPercent = toolbarGpuValid
                ? epochengine::format_text("{:.0f}%", toolbarGpu.busy_fraction * 100.0) : "--";
            const std::string toolbarVramPercent = toolbarVramValid
                ? epochengine::format_text("{:.0f}%", toolbarVramFraction * 100.0) : "--";
            const std::string toolbarGpuLabel = "GPU " + toolbarGpuPercent;
            constexpr double bytesPerGiB = 1024.0 * 1024.0 * 1024.0;
            const std::string toolbarVramLabel = toolbarVramValid
                ? epochengine::format_text("VRAM {:.1f}/{:.1f} GiB",
                    static_cast<double>(toolbarGpu.dedicated_used_bytes) / bytesPerGiB,
                    static_cast<double>(toolbarGpu.dedicated_total_bytes) / bytesPerGiB)
                : "VRAM --";
            const std::string toolbarGpuCompact = "G " + toolbarGpuPercent;
            const std::string toolbarVramCompact = "V " + toolbarVramPercent;
            // Capacity is immutable between samples; reserve its full numeric width
            // so changing used bytes does not move menus or status fields.
            toolbarVramWidthReference = toolbarGpuSourceAvailable && toolbarGpu.dedicated_total_bytes > 0u
                ? epochengine::format_text("VRAM {:.1f}/{:.1f} GiB",
                    static_cast<double>(toolbarGpu.dedicated_total_bytes) / bytesPerGiB,
                    static_cast<double>(toolbarGpu.dedicated_total_bytes) / bytesPerGiB)
                : "VRAM --";
            if (epochUsage)
            {
                const std::string coverage = epochResources.process_count == 0u
                    ? "Epoch process usage is unavailable."
                    : epochengine::format_text("Epoch scope covers {} observed processes{}.",
                        epochResources.process_count, epochResources.coverage_complete
                            ? "" : " with partial coverage");
                toolbarCpuDetail = (toolbarCpuValid ? "Share of host CPU time. " : "Process CPU usage unavailable. ") + coverage;
                toolbarRamDetail = toolbarRamValid ? epochengine::format_text(
                    "{:.1f} GiB resident across processes. Shared pages can count more than once. {}",
                    static_cast<double>(usedRamBytes) / bytesPerGiB, coverage)
                    : "Process RAM usage unavailable. " + coverage;
                toolbarGpuDetail = (toolbarGpuValid
                    ? "Busiest attributed GPU engine across observed processes. "
                    : "Process GPU usage unavailable. ") + coverage;
                toolbarVramDetail = (toolbarVramValid
                    ? toolbarVramLabel + ". Sum of observed process allocations and adapter capacities. "
                    : "Process VRAM usage unavailable. ") + coverage;
                if (toolbarGpuValid && toolbarGpu.activity_partial)
                    toolbarGpuDetail += " GPU counter coverage is partial.";
                if (toolbarVramValid && toolbarGpu.memory_partial)
                    toolbarVramDetail += " VRAM counter coverage is partial.";
            }
            else
            {
                toolbarCpuDetail = "CPU use across the host.";
                toolbarRamDetail = toolbarRamValid ? epochengine::format_text(
                    "{} ({:.1f} GiB free on the host).", toolbarRamLabel,
                    static_cast<double>(toolbarResources.available_memory_bytes) / bytesPerGiB)
                    : "Host RAM usage unavailable.";
                toolbarGpuDetail = toolbarGpuSourceAvailable
                    ? epochengine::format_text("NVML adapter {} of {}: {}. Whole-adapter load across all applications and the driver.",
                        toolbarGpu.adapter_index, toolbarGpu.adapter_count, toolbarGpu.adapter_name.data())
                    : "Host GPU telemetry unavailable.";
                toolbarVramDetail = toolbarVramValid
                    ? toolbarVramLabel + ". Whole adapter, including all applications and the driver."
                    : "Host dedicated GPU memory unavailable.";
            }
            const std::string_view resourceScopeAction = epochUsage
                ? " Click any resource badge for Host totals." : " Click any resource badge for Epoch process usage.";
            toolbarCpuDetail += resourceScopeAction;
            toolbarRamDetail += resourceScopeAction;
            toolbarGpuDetail += resourceScopeAction;
            toolbarVramDetail += resourceScopeAction;
            const auto toolbarHardwareThreads = std::thread::hardware_concurrency();
            const double toolbarSuggestedFps = perf::target_fps_for(
                recommended_editor_performance_tier(ctx, toolbarHardwareThreads));
            const std::string toolbarPacingName = recommended_support_tier(ctx, toolbarHardwareThreads);
            const std::string toolbarPacingCompact = toolbarPacingName.starts_with("Tier ")
                ? "T" + toolbarPacingName.substr(5) : toolbarPacingName;
            const std::string toolbarPacingTier = toolbarPacingCompact;
            const std::string toolbarRendererName = renderer_name(ctx);
            const auto& toolbarContextTitle = ctx->windowData && !ctx->windowData->titleNarrow.empty()
                ? ctx->windowData->titleNarrow : toolbarRendererName;
            const std::string toolbarContextLabel = toolbarContextTitle.starts_with(toolbarRendererName)
                ? toolbarContextTitle
                : epochengine::format_text("{} | {}", toolbarRendererName, toolbarContextTitle);
            const std::string toolbarScopeLabel = epochUsage ? "Epoch" : "Host";
            toolbarStatus = {{
                toolbarCpuLabel, toolbarRamLabel, toolbarGpuLabel, toolbarVramLabel, toolbarScopeLabel,
                toolbarContextLabel,
                toolbarFpsFull,
                epochengine::format_text("Total {} / Ready {} / Queued {}",
                    toolbarActivity.running_tasks, toolbarActivity.idle_workers, toolbarQueued),
                std::string("v") + epochengine::GetEngineVersionString(),
                epochengine::format_text("{} {} {} {}", toolbarArch, toolbarCompiler,
#if defined(_MSC_VER) && _MSC_VER >= 1930 && _MSC_VER < 1950
                    "2022",
#else
                    toolbarOs,
#endif
                    build_configuration_label()),
                toolbarPacingTier
            }};
            toolbarCompactStatus = {{
                toolbarCpuCompact, toolbarRamCompact, toolbarGpuCompact, toolbarVramCompact, toolbarScopeLabel,
                toolbarContextLabel,
                toolbarFpsCompact,
                epochengine::format_text("Total {} / Ready {} / Queued {}",
                    toolbarActivity.running_tasks, toolbarActivity.idle_workers, toolbarQueued),
                toolbarStatus[toolbarVersionIndex],
                epochengine::format_text("{} {} {}", toolbarArch, toolbarCompiler,
                    build_configuration_label()),
                toolbarPacingCompact
            }};
            toolbarMinimalStatus = {{
                toolbarCpuCompact, toolbarRamCompact, toolbarGpuCompact, toolbarVramCompact, toolbarScopeLabel,
                toolbarContextLabel, toolbarFpsCompact, toolbarCompactStatus[toolbarTaskIndex],
                toolbarStatus[toolbarVersionIndex], toolbarArch.data(), toolbarPacingCompact
            }};
            for (std::size_t i = 0u; i < toolbarStatusItems.size(); ++i)
                toolbarStatusItems[i] = {
                    toolbarStatus[i], toolbarCompactStatus[i], toolbarMinimalStatus[i]};
            toolbarStatusItems[toolbarScopeIndex].tooltip = epochUsage
                ? "Resource scope: Editor and observed child processes, not the entire PC. Click to show Host totals."
                : "Resource scope: entire PC. Click to show Editor and observed child processes.";
            toolbarStatusItems[0].compression_priority = 200u;
            toolbarStatusItems[1].compression_priority = 200u;
            toolbarStatusItems[2].compression_priority = 200u;
            toolbarStatusItems[3].compression_priority = 200u;
            toolbarStatusItems[toolbarBuildIndex].compression_priority = 400u;
            toolbarStatusItems[toolbarFpsIndex].compression_priority = 40u;
            toolbarStatusItems[toolbarTaskIndex].compression_priority = 100u;
            toolbarStatusItems[toolbarTierIndex].compression_priority = 300u;
            toolbarStatusItems[0].width_reference = "CPU 100%";
            toolbarStatusItems[0].compact_width_reference = "C 100%";
            toolbarStatusItems[0].minimal_width_reference = toolbarStatusItems[0].compact_width_reference;
            toolbarStatusItems[1].width_reference = "RAM 100%";
            toolbarStatusItems[1].compact_width_reference = "R 100%";
            toolbarStatusItems[1].minimal_width_reference = "R 100%";
            toolbarStatusItems[2].width_reference = "GPU 100%";
            toolbarStatusItems[2].compact_width_reference = "G 100%";
            toolbarStatusItems[2].minimal_width_reference = "G 100%";
            toolbarStatusItems[3].width_reference = toolbarVramWidthReference;
            toolbarStatusItems[3].compact_width_reference = "V 100%";
            toolbarStatusItems[3].minimal_width_reference = "V 100%";
            const auto resourceTone = [](bool valid, double busy) noexcept
            {
                return !valid ? gui::SemanticTone::neutral
                    : busy >= 0.90 ? gui::SemanticTone::error
                    : busy >= 0.75 ? gui::SemanticTone::warning
                    : gui::SemanticTone::success;
            };
            toolbarStatusItems[0].tone = resourceTone(toolbarCpuValid, toolbarCpuFraction);
            toolbarStatusItems[0].tooltip = toolbarCpuDetail;
            toolbarStatusItems[1].tone = resourceTone(toolbarRamValid, toolbarRamFraction);
            toolbarStatusItems[1].tooltip = toolbarRamDetail;
            toolbarStatusItems[2].tone = resourceTone(toolbarGpuValid, toolbarGpu.busy_fraction);
            toolbarStatusItems[3].tone = resourceTone(toolbarVramValid, toolbarVramFraction);
            toolbarStatusItems[2].tooltip = toolbarGpuDetail;
            toolbarStatusItems[3].tooltip = toolbarVramDetail;
            toolbarTierDetail = epochengine::format_text(
                "Suggested foreground Editor pacing is {:.0f} FPS for the active renderer and hardware threads. "
                "The FPS badge shows the current cap. Click for Systems and renderer feature details.",
                toolbarSuggestedFps);
            toolbarStatusItems[toolbarTierIndex].tooltip = toolbarTierDetail;
            toolbarStatusItems[toolbarTaskIndex].tone = toolbarActivity.running_tasks > 0u ? gui::SemanticTone::info
                : toolbarQueued > 0u ? gui::SemanticTone::warning : gui::SemanticTone::muted;
            toolbarTaskDetail = epochengine::format_text(
                "Total counts running Editor pool workers; Ready counts available workers. Pool capacity: {}. "
                "Queued includes waiting Editor and external Assistant work. External/Assistant active: {} "
                "(not counted as pool workers). GPU model activity is not an Editor pool worker. Planet workers: Planet LOD.",
                toolbarActivity.worker_count,
                toolbarRunning > toolbarActivity.running_tasks ? toolbarRunning - toolbarActivity.running_tasks : 0u);
            toolbarStatusItems[toolbarTaskIndex].tooltip = toolbarTaskDetail;
            toolbarStatusItems[toolbarFpsIndex].tone = chromeThrottle > 0.0 ? gui::SemanticTone::warning
                : editor.chromeFpsSample >= 50.0 ? gui::SemanticTone::success
                : editor.chromeFpsSample >= 25.0 ? gui::SemanticTone::warning
                : editor.chromeFpsMeasured ? gui::SemanticTone::error : gui::SemanticTone::neutral;
            toolbarStatusItems[toolbarFpsIndex].tooltip =
                "Editor window presentation rate during the named phase. The cap reflects this host's pacing, "
                "background/minimized state or CPU/build throttle. Two unfocused numbers show rate/limit. "
                "Pacing cap is the active limit selected by settings or launch options; "
                "the hardware tier recommendation does not override an explicit limit. "
                "Project host is not the child's FPS; "
                "the project has its own frame limit in Project Run Settings.";
        }
        else
        {
            // On reopen, measure a new visible frame interval.
            editor.chromeFrameRate = {};
            editor.chromeFpsSample = 0.0;
            editor.chromeFpsMeasured = false;
        }
        // Keep status on the PRIMARY chrome row. Reserve the right-hand
        // width before laying out the menu, allowing its existing overflow
        // handling to work without pushing status onto a second row.
        // Use measured labels rather than a fixed fraction of the viewport.
        // Leave room for File and the menu overflow on narrow windows;
        // unused central chrome space should not force abbreviated task counts.
        bool suppressUpdateAvailability = false;
        const auto updateToolbarState = editor_update_modal::update_toolbar_state(
            update_modal_flags(), suppressUpdateAvailability);
        const float toolbarCommandReserve = updateToolbarState.visible ? 210.0f : 0.0f;
        constexpr float toolbar_h = 84.0f;
        const float splitter_w = 10.0f;
        const float splitter_h = 10.0f;
        const gui_lib::BottomDockHeightLayout bottomDockLayout =
            gui_lib::make_bottom_dock_height_layout({
                .viewport_height = h,
                .toolbar_height = toolbar_h,
                .splitter_height = splitter_h,
                .requested_fraction = editor.dockSplit,
                .minimum_bottom_height = 190.0f,
                .minimum_center_height = 220.0f,
                .maximum_bottom_fraction = 0.72f,
                .visible = bottom_visible
            });
        const float bottom_h = bottomDockLayout.bottom_height;
        const float bottom_split_h = bottomDockLayout.splitter_height;

        const auto sideColumns = gui_lib::make_side_dock_layout({
            .viewport_width = w,
            .requested_left_fraction = editor.outlinerSplit,
            .requested_right_fraction = editor.inspectorSplit,
            .splitter_width = splitter_w,
            .minimum_center_width = 180.0f,
            .left_visible = layout_outliner_visible,
            .right_visible = layout_inspector_visible
        });
        const float left_w = sideColumns.left_width;
        const float right_w = sideColumns.right_width;
        const float left_split_w = sideColumns.left_splitter_width;
        const float right_split_w = sideColumns.right_splitter_width;

        const gui::Vec2 toolbar_pos{ 0.0f, 0.0f };
        const gui::Vec2 toolbar_size{ w, toolbar_h };

        const gui::Vec2 bottom_split_pos{ 0.0f, (std::max)(0.0f, h - bottom_h - bottom_split_h) };
        const gui::Vec2 bottom_split_size{ w, bottom_split_h };
        const gui::Vec2 bottom_pos{ 0.0f, (std::max)(0.0f, h - bottom_h) };
        const gui::Vec2 bottom_size{ w, bottom_h };
        const gui_lib::BottomDockColumnOptions bottomColumnOptions{
            .viewport_width = bottom_size.x,
            .splitter_width = splitter_w,
            .requested_fraction = editor.bottomGridSplit,
            .left_visible = bottom_left_visible,
            .right_visible = bottom_right_visible
        };
        const auto bottomColumns = gui_lib::make_bottom_dock_column_layout(bottomColumnOptions);
        const bool splitBottomColumns = bottomColumns.split;
        const float bottomColumnGap = bottomColumns.splitter_width;
        const float bottomLeftWidth = bottomColumns.left_width;
        const float bottomRightWidth = bottomColumns.right_width;
        const gui::Vec2 bottomLeftPosition = bottom_pos;
        const gui::Vec2 bottomLeftSize{ bottomLeftWidth, bottom_h };
        const gui::Vec2 bottomColumnsSplitPosition{
            bottomLeftWidth,
            bottom_pos.y };
        const gui::Vec2 bottomColumnsSplitSize{
            bottomColumnGap,
            bottom_h };
        const gui::Vec2 bottomRightPosition{
            bottomColumns.right_offset,
            bottom_pos.y };
        const gui::Vec2 bottomRightSize{ bottomRightWidth, bottom_h };

        const float main_y = toolbar_h;
        const float main_h = bottomDockLayout.center_height;

        const gui::Vec2 outliner_pos{ 0.0f, main_y };
        const gui::Vec2 outliner_size{ left_w, main_h };
        const gui::Vec2 outliner_split_pos{ left_w, main_y };
        const gui::Vec2 outliner_split_size{ left_split_w, main_h };

        const gui::Vec2 details_pos{ (std::max)(0.0f, w - right_w), main_y };
        const gui::Vec2 details_size{ right_w, main_h };
        const gui::Vec2 inspector_split_pos{ (std::max)(0.0f, details_pos.x - right_split_w), main_y };
        const gui::Vec2 inspector_split_size{ right_split_w, main_h };

        const gui::Vec2 viewport_pos{ left_w + left_split_w, main_y };
        const gui::Vec2 viewport_size{
            sideColumns.center_width,
            main_h
        };

        auto emit_command = [&](
            EditorCommand command,
            std::string_view argument = {},
            core::ContextType requestedContextType = core::ContextType::None)
        {
            result.command = command;
            result.command_argument.assign(argument.begin(), argument.end());
            result.requested_context_type = requestedContextType;
        };

        auto request_context_backend_selection = [&](const ContextBackendOption& option)
        {
            editor.openMenu = TopMenu::None;
            const bool targetAlreadyActive = ctx && option.type == ctx->type;
            const bool replacementInProgress = editor_context_replacement_in_progress();
            if (!editor_context_selection_allowed(
                    option.selectable,
                    targetAlreadyActive,
                    replacementInProgress))
            {
                if (!option.selectable)
                {
                    editor.contextSelectionStatus =
                        option.label + " is unavailable: "
                        + (option.unavailable_reason.empty()
                            ? std::string{ "this build cannot replace the editor with that backend." }
                            : option.unavailable_reason + ".");
                }
                else if (targetAlreadyActive)
                {
                    editor.contextSelectionStatus =
                        option.label + " is already the active editor backend.";
                }
                else
                {
                    editor.contextSelectionStatus =
                        "A backend replacement is already in progress. Wait for the restored editor frame before choosing another backend.";
                }
                push_editor_log(editor, "[context] " + editor.contextSelectionStatus);
                return;
            }

            if (editor_engine_work_blocks_context_switch(ctx.get()))
            {
                // Never let a renderer preference accidentally destroy a
                // running model, candidate sandbox, or unfinished validation.
                editor.contextSelectionStatus =
                    "Renderer switch to " + option.label
                    + " held: Assistant or AI inference is active. "
                      "Finish or explicitly Stop the active request, then switch. "
                      "The current backend and candidate are unchanged.";
                push_editor_log(editor, "[context] " + editor.contextSelectionStatus);
                return;
            }
            editor.contextSelectionStatus =
                "Requested " + option.label + " for this editor session.";
            emit_command(EditorCommand::SwitchContext, option.label, option.type);
            push_editor_log(editor, "[context] Requested " + option.label + " from Editor Settings.");
        };

        const auto trim_ai_command = [](std::string_view value) noexcept
        {
            while (!value.empty()
                && (value.front() == ' ' || value.front() == '\t'
                    || value.front() == '\r' || value.front() == '\n'))
            {
                value.remove_prefix(1u);
            }
            while (!value.empty()
                && (value.back() == ' ' || value.back() == '\t'
                    || value.back() == '\r' || value.back() == '\n'))
            {
                value.remove_suffix(1u);
            }
            return value;
        };
