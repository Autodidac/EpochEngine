// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        EditorFrameResult result{};
        if (!ctx)
            return result;
        epoch_editor_perf::current_frame_watch().mark(epoch_editor_perf::FrameStage::editor_entry);
        epoch_editor_perf::FrameTimer frameTimer{};

        auto& editor = editor_state_for(ctx);
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
        publish_detached_pane_projection(ctx.get(), editor, chat);
        frameTimer.mark_chat_ready();
        if (!chat.pending
            && chat.completionGeneration
                > editor.aiObservedCompletionGeneration)
        {
            editor.aiObservedCompletionGeneration =
                chat.completionGeneration;
            const AiDeferredRequestKind completedKind =
                chat.latestCompletionRequestKind;
#if EPOCH_EDITION_PUBLIC
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
                        editor.aiToolPlan =
                            epochengine::ai::parse_tool_plan(toolingReply);
                        editor.aiToolPlanApplied = false;
                        editor.aiToolStatus = editor.aiToolPlan
                            ? editor.aiToolPlan.message
                                + " Nothing has executed; review the exact host tool "
                                  "and approve it below or reply yes / approve / run it."
                            : editor.aiToolPlan.message;
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
                std::clamp(w * 0.72f, 960.0f, 1680.0f),
                std::clamp(h * 0.76f, 720.0f, 1320.0f)
            },
            {720.0f, 640.0f});
        const gui::Vec2 aiModelConsentModalSize = fit_modal_size({ 660.0f, 430.0f }, { 520.0f, 380.0f });
        const gui::Vec2 voiceConsentModalSize = fit_modal_size({ 580.0f, 300.0f }, { 500.0f, 280.0f });
        // Editor Settings must share ONE geometry between input capture and
        // rendering. A stale 600x462 capture rectangle used to reject clicks
        // in the actual (up to 700x620) settings dialog, particularly after
        // opening the theme selector.
        const gui::Vec2 editorSettingsModalSize =
            fit_modal_size({700.0f, 620.0f}, {420.0f, 520.0f});
        const gui::Vec2 editorSettingsModalPos = centered_modal_position(editorSettingsModalSize);
        const core::RoutedPanelDockDragProjection nativeDockProjection =
            core::routed_panel_dock_drag_projection();
        const bool pointerGrabActive = nativeDockProjection.active
            || editor.paneTitleDragActive
            || editor.layoutDrag != EditorLayoutDrag::None
            || editor.floatingGuiWindow.floating.dragging
            || editor.floatingGuiWindow.floating.resizing;
#if EPOCH_EDITION_PUBLIC
        constexpr bool nativeToolApprovalModalVisible = false;
#endif
        const gui::Vec2 aboutModalSize = fit_modal_size({600.0f, 416.0f}, {360.0f, 260.0f});
        auto modal_visible_now = [&editor, nativeToolApprovalModalVisible]() noexcept -> bool
        {
            return nativeToolApprovalModalVisible || editor.showAboutModal
                || editor.showSettingsModal
                || editor.showPackageManagerModal
                || editor.showAiModelManagerModal
                || editor.showAiModelConsentModal
                || editor.showVoiceConsentModal
                || editor.showUpdateConfirmModal
                || editor.showSourceUpdateConfirmModal;
        };

        const bool modalVisible = modal_visible_now();
        if (modalVisible || pointerGrabActive)
        {
            editor.openMenu = TopMenu::None;
            // Modal windows own all editor interaction until dismissed. The
            // EpochGui hit region protects widgets, while this flag also stops
            // scene picking, dragging, camera navigation, and document tools.
            result.scene_input_captured = true;
        }

        auto floating_gui_visible = [&editor]() noexcept -> bool
        {
            return editor.floatingGuiWindow.visible;
        };

        // Print Screen is an editor-level shortcut, even when a text editor or
        // modal owns keyboard input. Poll ONLY while this editor context owns
        // foreground focus; never register a system-wide hotkey or keyboard hook.
        // The existing clipboard capture runs after the present/menu frame.
        if (ctx->has_input_focus_safe())
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
            ctx->has_input_focus_safe()
            && !modalVisible
            && !pointerGrabActive
            && editor.openMenu == TopMenu::None
            && !floating_gui_visible();
        const bool shortcutInputAvailable = workspaceShortcutInputAvailable
            && !gui::keyboard_input_captured();
        if (shortcutInputAvailable)
        {
            const bool controlDown =
                ctx->is_key_held_safe(input::Key::LeftControl)
                || ctx->is_key_held_safe(input::Key::RightControl);
            const bool shiftDown =
                ctx->is_key_held_safe(input::Key::LeftShift)
                || ctx->is_key_held_safe(input::Key::RightShift);
            if (controlDown && ctx->is_key_down_safe(input::Key::Z))
            {
                if (shiftDown)
                    (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::redo);
                else
                    (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::undo);
                result.scene_input_captured = true;
            }
            else if (controlDown && ctx->is_key_down_safe(input::Key::Y))
            {
                (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::redo);
                result.scene_input_captured = true;
            }
            else if (ctx->is_key_down_safe(input::Key::Delete))
            {
                (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::delete_selection);
                result.scene_input_captured = true;
            }
            else if (ctx->is_key_down_safe(input::Key::Escape)
                && editor_has_selection(editor))
            {
                clear_editor_selection(editor);
                push_editor_log(editor, "[edit] Selection cleared.");
                result.scene_input_captured = true;
            }
        }

        const bool overlayPriorityActive =
            editor.openMenu != TopMenu::None
            || modalVisible
            || floating_gui_visible()
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
        else if (editor.showSettingsModal)
            gui::begin_modal_background_capture(editorSettingsModalPos, editorSettingsModalSize);
        else if (editor.showAiModelManagerModal)
            gui::begin_modal_background_capture(centered_modal_position(
                fit_modal_size({ (std::min)(1000.0f, w * 0.90f), h * 0.90f }, { 500.0f, 480.0f })),
                fit_modal_size({ (std::min)(1000.0f, w * 0.90f), h * 0.90f }, { 500.0f, 480.0f }));
        else if (editor.showPackageManagerModal)
            gui::begin_modal_background_capture(centered_modal_position(packageManagerModalSize), packageManagerModalSize);
        else if (editor.showAboutModal)
            gui::begin_modal_background_capture(centered_modal_position(aboutModalSize), aboutModalSize);

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
        int supportedToolPaneCount = 0;
        for (std::size_t index = 0; index < toolPaneRoutes.size(); ++index)
        {
            if (pane_supported(static_cast<EditorToolPane>(index)))
                ++supportedToolPaneCount;
        }
        const int windowMenuItemCount = supportedToolPaneCount + 1;
        auto pane_family_enabled = [&](EditorToolPane pane) noexcept
        {
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

        const auto toolbarActivity = editor.taskScheduler
            ? editor.taskScheduler->activity() : editor_tasks::ActivitySnapshot{};
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
        // Resource telemetry leads the status strip and survives compression.
        // Trailing identity fields yield before overlapping menus or tabs.
        // The top Tasks chip is an entry point; the detailed workbench owns
        // the task graph, diagnostics and self-iteration timeline.
        // Compact right-aligned identity / activity / backend / distance. No
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
        const double chromePacing = ctx->windowData
            ? ctx->windowData->editorFramePacingHz.load(std::memory_order_relaxed) : 0.0;
        const std::string toolbarFps = std::isfinite(editor.chromeFpsSample)
                && editor.chromeFpsMeasured
            ? chromeThrottle > 0.0
                ? epochengine::format_text("{:.0f} FPS / {:.0f} {} cap", editor.chromeFpsSample,
                    chromeThrottle, chromeThrottle == 10.0 ? "build" : "CPU")
                : chromePacing > 0.0
                    ? epochengine::format_text("{:.0f} FPS / {:.0f} cap", editor.chromeFpsSample, chromePacing)
                    : epochengine::format_text("{:.0f} FPS / uncapped", editor.chromeFpsSample)
            : std::string{"-- FPS"};
        const auto toolbarResources = platform::work_admission::sample_host_resources();
        const bool toolbarCpuValid = toolbarResources.cpu_valid
            && std::isfinite(toolbarResources.cpu_busy_fraction);
        const std::string toolbarCpu = toolbarCpuValid
            ? std::to_string(static_cast<unsigned>(std::clamp(
                toolbarResources.cpu_busy_fraction, 0.0, 1.0) * 100.0 + 0.5)) + "%"
            : "--";
        const bool toolbarRamValid = toolbarResources.memory_valid
            && toolbarResources.total_memory_bytes > 0u
            && toolbarResources.available_memory_bytes <= toolbarResources.total_memory_bytes;
        const std::string toolbarRam = toolbarRamValid
            ? epochengine::format_text("{:.1f}G", static_cast<double>(
                toolbarResources.available_memory_bytes) / (1024.0 * 1024.0 * 1024.0))
            : "--";
        const std::string toolbarRamPercent = toolbarRamValid
            ? epochengine::format_text("{:.0f}%", 100.0 * (1.0 - static_cast<double>(
                toolbarResources.available_memory_bytes) / toolbarResources.total_memory_bytes))
            : "--";
        const std::string toolbarCpuLabel = "CPU " + toolbarCpu;
        const std::string toolbarRamLabel = "RAM " + toolbarRamPercent;
        const std::string toolbarCpuCompact = "C " + toolbarCpu;
        const std::string toolbarRamCompact = "R " + toolbarRamPercent;
        const std::string toolbarRamDetail = toolbarRamLabel + " (" + toolbarRam + " free)";
        const std::string toolbarFpsCompact = std::isfinite(editor.chromeFpsSample)
                && editor.chromeFpsMeasured
            ? epochengine::format_text("{:.0f} FPS", editor.chromeFpsSample)
            : "-- FPS";
        constexpr std::size_t toolbarFpsIndex = 4u;
        constexpr std::size_t toolbarTaskIndex = 5u;
        const std::array<std::string, 8> toolbarStatus{{
            toolbarCpuLabel, toolbarRamLabel,
            std::string("v") + epochengine::GetEngineVersionString(),
            epochengine::format_text("{} {} {} {}", toolbarArch, toolbarCompiler,
#if defined(_MSC_VER) && _MSC_VER >= 1930 && _MSC_VER < 1950
                "2022",
#else
                toolbarOs,
#endif
                build_configuration_label()),
            toolbarFps,
            epochengine::format_text("Thread {} / Queued {} / Idle {}",
                toolbarRunning, toolbarQueued, toolbarActivity.idle_workers),
            renderer_name(ctx),
            epochengine::format_text("Zoom {}x", static_cast<int>((std::max)(0.0f,
                std::floor(previewgrid::camera_distance_for(ctx.get())))))
        }};
        const std::array<std::string, 8> toolbarCompactStatus{{
            toolbarCpuCompact, toolbarRamCompact, toolbarStatus[2],
            epochengine::format_text("{} {} {}", toolbarArch, toolbarCompiler,
                build_configuration_label()),
            toolbarFpsCompact,
            epochengine::format_text("{}T / {}Q / {}I",
                toolbarRunning, toolbarQueued, toolbarActivity.idle_workers),
            toolbarStatus[6],
            epochengine::format_text("{}x", static_cast<int>((std::max)(0.0f,
                std::floor(previewgrid::camera_distance_for(ctx.get())))))
        }};
        const std::array<std::string, 8> toolbarMinimalStatus{{
            toolbarCpuCompact, toolbarRamCompact, toolbarStatus[2], toolbarArch.data(),
            toolbarFpsCompact, toolbarCompactStatus[toolbarTaskIndex],
            toolbarStatus[6], toolbarCompactStatus[7]
        }};
        std::array<gui::StatusStripItem, 8> toolbarStatusItems{};
        for (std::size_t i = 0u; i < toolbarStatusItems.size(); ++i)
            toolbarStatusItems[i] = {
                toolbarStatus[i], toolbarCompactStatus[i], toolbarMinimalStatus[i]};
        toolbarStatusItems[0].compression_priority = 200u;
        toolbarStatusItems[1].compression_priority = 200u;
        toolbarStatusItems[2].compression_priority = 1u;
        toolbarStatusItems[3].compression_priority = 80u;
        toolbarStatusItems[toolbarFpsIndex].compression_priority = 40u;
        toolbarStatusItems[toolbarTaskIndex].compression_priority = 100u;
        toolbarStatusItems[6].compression_priority = 20u;
        toolbarStatusItems[7].compression_priority = 300u;
        toolbarStatusItems[0].width_reference = "CPU 100%";
        toolbarStatusItems[0].compact_width_reference = "C 100%";
        toolbarStatusItems[0].minimal_width_reference = "C 100%";
        toolbarStatusItems[1].width_reference = "RAM 100%";
        toolbarStatusItems[1].compact_width_reference = "R 100%";
        toolbarStatusItems[1].minimal_width_reference = "R 100%";
        const auto resourceTone = [](bool valid, double busy) noexcept
        {
            return !valid ? gui::SemanticTone::neutral
                : busy >= 0.90 ? gui::SemanticTone::error
                : busy >= 0.75 ? gui::SemanticTone::warning
                : gui::SemanticTone::success;
        };
        toolbarStatusItems[0].tone = resourceTone(toolbarCpuValid, toolbarResources.cpu_busy_fraction);
        toolbarStatusItems[1].tone = resourceTone(toolbarRamValid, toolbarRamValid
            ? 1.0 - static_cast<double>(toolbarResources.available_memory_bytes)
                / toolbarResources.total_memory_bytes : 0.0);
        toolbarStatusItems[1].tooltip = toolbarRamDetail;
        toolbarStatusItems[toolbarFpsIndex].tone = chromeThrottle > 0.0 ? gui::SemanticTone::warning
            : editor.chromeFpsSample >= 50.0 ? gui::SemanticTone::success
            : editor.chromeFpsSample >= 25.0 ? gui::SemanticTone::warning
            : editor.chromeFpsMeasured ? gui::SemanticTone::error : gui::SemanticTone::neutral;
        // Keep status on the PRIMARY chrome row. Reserve the right-hand
        // width before laying out the menu, allowing its existing overflow
        // handling to work without pushing status onto a second row.
        // Use measured labels rather than a fixed fraction of the viewport.
        // Leave room for File and the menu overflow on narrow windows;
        // unused central chrome space should not force abbreviated task counts.
        const auto updateToolbarState = editor_update_modal::update_toolbar_state(update_modal_flags());
        const float toolbarCommandReserve = updateToolbarState.visible ? 210.0f : 0.0f;
        constexpr float toolbarStatusTop = 13.0f;
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
