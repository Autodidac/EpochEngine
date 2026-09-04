        // Included from editor.application.cpp inside editor_run after toolbar/menu rendering.
        // Owns update confirmation, project source download confirmation, and update automation UI.

        if (editor.showUpdateConfirmModal)
        {
            editor.openMenu = TopMenu::None;
            const bool sourceOnlyUpdate =
                editor.lastUpdateCheck.source_update_available
                && !editor.lastUpdateCheck.packaged_update_available;
            const bool sourceWorkerRunning =
                editor.updateState == EditorUpdateState::SourceWorkerRunning
                || (editor.updateSourceInstallPending && updater::source_update_worker_active());
            const bool sourceCancelRequested = editor.updateSourceCancelRequested;
            const bool sourceCancelAvailable = sourceWorkerRunning && editor_source_cancel_available(editor);
            const bool sourceAuthorizationRunning = editor_private_source_access_pending(editor);
            const bool updateRunning = editor.updateCheckPending.has_value() || sourceWorkerRunning;
            const bool restartReady = editor.updateState == EditorUpdateState::RestartReady;
            if (restartReady)
                arm_editor_update_restart_countdown(editor);
            const int restartCountdownSeconds = restartReady
                ? static_cast<int>(std::ceil(editor_update_restart_seconds_remaining(editor)))
                : 0;
            const std::string restartButtonLabel = restartReady
                ? epochengine::format_text("Restart Now ({}s)", restartCountdownSeconds)
                : std::string{ "Restart Now" };
            const gui::Vec2 modalSize = updateConfirmModalSize;
            const gui::Vec2 modalPos{
                (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                (std::max)(0.0f, (h - modalSize.y) * 0.5f)
            };
            constexpr float modalContentInset = 28.0f;
            constexpr float buttonHeight = 30.0f;
            constexpr float buttonBottomPad = 18.0f;
            constexpr float buttonTopPad = 12.0f;
            constexpr float buttonGap = 16.0f;
            constexpr float buttonStackGap = 8.0f;
            const float contentX = modalPos.x + modalContentInset;
            const float contentRight = modalPos.x + modalSize.x - modalContentInset;
            const float contentWidth = (std::max)(1.0f, contentRight - contentX);
            const editor_update_modal::UpdateFlags flags = update_modal_flags();
            const float cancelButtonWidth = (sourceWorkerRunning || sourceAuthorizationRunning) ? 148.0f : 120.0f;
            const float primaryButtonWidth = (std::min)(220.0f, (std::max)(160.0f, contentWidth * 0.34f));
            const float advancedButtonWidth = (std::min)(190.0f, (std::max)(156.0f, contentWidth * 0.28f));
            const bool showCancelButton =
                sourceAuthorizationRunning
                    ? !sourceCancelRequested
                : sourceWorkerRunning
                    ? (!sourceCancelRequested && sourceCancelAvailable)
                    : (!updateRunning && !restartReady);
            const bool showPrimaryButton =
                !updateRunning
                && (flags.checkFailed || flags.installableUpdate || restartReady || sourceWorkerRunning);
            const bool showAdvancedSourceButton =
                updater::private_source_access_enabled() && !updateRunning && !restartReady;
            const editor_update_modal::ActionStrip actionStrip =
                editor_update_modal::update_action_strip(flags, contentWidth);
            const float actionStripHeight = actionStrip.height;
            const bool actionStripStacked = actionStrip.stacked;
            const float buttonY = modalPos.y + modalSize.y - actionStripHeight - buttonBottomPad;
            const float contentBottom = (std::max)(modalPos.y + 64.0f, buttonY - buttonTopPad);
            const std::string updateStatusLine = editor_update_modal::trim_status(editor.updateStatus);
            const auto requestUpdateRestart = [&]() {
                if (editor.lastUpdateCheck.packaged_handoff_staged
                    && !updater::launch_staged_update_handoff())
                {
                    editor.updateState = EditorUpdateState::Failed;
                    editor.updateStatus = "Restart failed because the staged update replacement could not be launched. Check logs/epoch_update_handoff.log beside the executable.";
                    push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                    return;
                }

                editor.showUpdateConfirmModal = false;
                clear_editor_update_restart_countdown(editor);
                editor.updateStatus = "Restarting Epoch to finish the staged update replacement.";
                push_editor_log(editor, "[update] Restart requested after verified update replacement.");
                emit_command(EditorCommand::Exit);
            };
            const bool autoRestartNow = restartReady && editor_update_restart_countdown_expired(editor);
            if (autoRestartNow)
            {
                requestUpdateRestart();
            }
            if (!autoRestartNow)
            {
                gui::begin_modal_window(gui::ModalWindowOptions{
                    .title = "Update Epoch",
                    .position = modalPos,
                    .size = modalSize,
                    .viewport_size = { w, h },
                    .dim_background = true
                });
            const gui::Vec2 contentPos = gui::cursor_position();
            float cursorY = contentPos.y;
            const auto emitWrapped = [&](const std::string_view text, const float gap) {
                if (cursorY >= contentBottom)
                    return;
                const float textHeight = gui::wrapped_text_height(text, contentWidth);
                if (cursorY + textHeight > contentBottom)
                    return;
                gui::set_cursor({ contentX, cursorY });
                gui::wrapped_label(text, contentWidth);
                cursorY += textHeight + gap;
            };
            emitWrapped(editor_update_modal::intro_text(flags), 8.0f);
            emitWrapped(updateStatusLine, 10.0f);
            if (restartReady)
            {
                emitWrapped(epochengine::format_text("Epoch will restart automatically in {} second{}.",
                    restartCountdownSeconds,
                    restartCountdownSeconds == 1 ? "" : "s"), 10.0f);
            }
            if (cursorY + 22.0f <= contentBottom)
            {
                const float progressWidth = (std::max)(1.0f, (std::min)(contentWidth, 560.0f));
                const std::string progressLabel = sourceAuthorizationRunning
                    ? "Source authorization"
                    : sourceWorkerRunning
                    ? "Source rebuild"
                    : editor.updateProjectSourceDownloadPending ? "Project source"
                    : updateRunning ? "Update"
                    : restartReady ? "Update staged"
                    : flags.checkFailed ? "Update check" : "Update ready";
                const std::string progressStatus = sourceAuthorizationRunning
                    ? "approval / encrypted transfer"
                    : sourceWorkerRunning
                    ? (!sourceCancelRequested && !sourceCancelAvailable)
                        ? epochengine::format_text(
                            "{} - cancel update available in {}s",
                            editor_source_worker_progress_status(editor, sourceCancelRequested),
                            editor_source_cancel_arm_seconds_remaining(editor))
                        : editor_source_worker_progress_status(editor, sourceCancelRequested)
                    : editor.updateProjectSourceDownloadPending
                        ? "downloading / extracting"
                    : updateRunning
                        ? "downloading / staging"
                        : restartReady
                            ? epochengine::format_text("auto restart in {}s", restartCountdownSeconds)
                            : flags.checkFailed ? "failed - retry available" : "ready";
                gui::set_cursor({ contentX, cursorY });
                gui::progress_bar(gui::ProgressBarOptions{
                    .label = progressLabel,
                    .status = progressStatus,
                    .value = editor_update_progress_value(editor),
                    .size = { progressWidth, 22.0f },
                    .show_percent = !sourceWorkerRunning && !flags.checkFailed,
                    .activity = updateRunning || sourceWorkerRunning,
                    .activity_phase = editor_update_activity_phase(editor)
                });
                cursorY += 36.0f;
            }
            emitWrapped(editor_update_modal::cache_text(flags), 8.0f);
            const std::string actionText = editor_update_modal::action_text(flags);
            emitWrapped(actionText, 8.0f);
            if (actionStripStacked)
            {
                float stackedButtonY = buttonY;
                if (showCancelButton)
                {
                    gui::set_cursor({ contentX, stackedButtonY });
                    if (sourceAuthorizationRunning)
                    {
                        if (gui::button("Cancel Source Access", { contentWidth, buttonHeight }))
                        {
                            request_editor_private_source_access_cancel(editor);
                        }
                    }
                    else if (sourceWorkerRunning)
                    {
                        if (gui::button("Cancel Update", { contentWidth, buttonHeight }))
                        {
                            request_editor_source_update_cancel(editor);
                        }
                    }
                    else if (!updateRunning && !restartReady && gui::button("Cancel", { contentWidth, buttonHeight }))
                    {
                        editor.showUpdateConfirmModal = false;
                        push_editor_log(editor, "[command] Update canceled.");
                    }
                    stackedButtonY += buttonHeight + buttonStackGap;
                }
                const std::string primaryUpdateLabel = restartReady
                    ? restartButtonLabel
                    : flags.checkFailed ? std::string{ "Retry Check" }
                    : sourceOnlyUpdate ? std::string{ "Update From Source" } : std::string{ "Install Release" };
                if (showPrimaryButton)
                {
                    gui::set_cursor({ contentX, stackedButtonY });
                    if (gui::button(primaryUpdateLabel, { contentWidth, buttonHeight }))
                    {
                        if (restartReady)
                        {
                            requestUpdateRestart();
                        }
                        else
                        {
                            if (flags.checkFailed)
                            {
                                start_editor_update_check(editor);
                            }
                            else if (sourceOnlyUpdate)
                            {
                                push_editor_log(editor, "[command] Source update confirmed from smart update modal.");
                                start_editor_source_update_install(editor);
                            }
                            else
                            {
                                push_editor_log(editor, "[command] Smart update confirmed.");
                                start_editor_update_install(editor);
                            }
                        }
                    }
                    stackedButtonY += buttonHeight + buttonStackGap;
                }
                if (showAdvancedSourceButton)
                {
                    gui::set_cursor({ contentX, stackedButtonY });
                    if (gui::button("Source Options...", { contentWidth, buttonHeight }))
                    {
                        editor.showUpdateConfirmModal = false;
                        editor.showSourceUpdateConfirmModal = true;
                        push_editor_log(editor, "[command] Authorized source options requested.");
                    }
                }
            }
            else
            {
                gui::set_cursor({ contentX, buttonY });
                if (showCancelButton && sourceAuthorizationRunning)
                {
                    if (gui::button("Cancel Source Access", { cancelButtonWidth, buttonHeight }))
                    {
                        request_editor_private_source_access_cancel(editor);
                    }
                }
                else if (showCancelButton && sourceWorkerRunning)
                {
                    if (gui::button("Cancel Update", { cancelButtonWidth, buttonHeight }))
                    {
                        request_editor_source_update_cancel(editor);
                    }
                }
                else if (showCancelButton && !updateRunning && !restartReady && gui::button("Cancel", { 120.0f, buttonHeight }))
                {
                    editor.showUpdateConfirmModal = false;
                    push_editor_log(editor, "[command] Update canceled.");
                }

                const float primaryButtonX = showCancelButton
                    ? (std::min)(contentRight - primaryButtonWidth, contentX + cancelButtonWidth + buttonGap)
                    : contentRight - primaryButtonWidth;
                gui::set_cursor({ primaryButtonX, buttonY });
                const std::string primaryUpdateLabel = restartReady
                    ? restartButtonLabel
                    : flags.checkFailed ? std::string{ "Retry Check" }
                    : sourceOnlyUpdate ? std::string{ "Update From Source" } : std::string{ "Install Release" };
                if (showPrimaryButton && gui::button(primaryUpdateLabel, { primaryButtonWidth, buttonHeight }))
                {
                    if (restartReady)
                    {
                        requestUpdateRestart();
                    }
                    else
                    {
                        if (flags.checkFailed)
                        {
                            start_editor_update_check(editor);
                        }
                        else if (sourceOnlyUpdate)
                        {
                            push_editor_log(editor, "[command] Source update confirmed from smart update modal.");
                            start_editor_source_update_install(editor);
                        }
                        else
                        {
                            push_editor_log(editor, "[command] Smart update confirmed.");
                            start_editor_update_install(editor);
                        }
                    }
                }
                const float advancedButtonX = (std::max)(contentX, contentRight - advancedButtonWidth);
                gui::set_cursor({ advancedButtonX, buttonY });
                if (showAdvancedSourceButton && gui::button("Source Options...", { advancedButtonWidth, buttonHeight }))
                {
                    editor.showUpdateConfirmModal = false;
                    editor.showSourceUpdateConfirmModal = true;
                    push_editor_log(editor, "[command] Authorized source options requested.");
                }
            }
            gui::end_modal_window();
            }
        }

        if (editor.showSourceUpdateConfirmModal)
        {
            editor.openMenu = TopMenu::None;
            const gui::Vec2 modalSize = sourceUpdateConfirmModalSize;
            const gui::Vec2 modalPos{
                (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                (std::max)(0.0f, (h - modalSize.y) * 0.5f)
            };
            constexpr float modalContentInset = 24.0f;
            const float contentX = modalPos.x + modalContentInset;
            const float contentRight = modalPos.x + modalSize.x - modalContentInset;
            const float contentWidth = (std::max)(1.0f, contentRight - contentX);
            constexpr float buttonHeight = 30.0f;
            constexpr float buttonGap = 16.0f;
            constexpr float buttonStackGap = 8.0f;
            const editor_update_modal::ActionStrip sourceActionStrip =
                editor_update_modal::source_action_strip(contentWidth);
            const float sourceActionStripHeight = sourceActionStrip.height;
            const bool sourceActionStripStacked = sourceActionStrip.stacked;
            const std::string pairDeviceLabel =
                updater::private_source_device_registered()
                    ? "Re-pair This Device"
                    : "Pair This Device";
            const auto armDevicePairing = [&]() {
                updater::reset_private_source_device_registration();
                editor.updateStatus =
                    "Device pairing is armed. Choose Build / Update From Source "
                    "or Download Source Project to begin explicit browser enrollment.";
                push_editor_log(
                    editor,
                    "[command] Source-device pairing armed; waiting for an explicit "
                    "source action.");
            };
            const float actionBaseY = modalPos.y + modalSize.y - sourceActionStripHeight - editor_update_modal::kButtonBottomPad;
            const float contentBottom = (std::max)(modalPos.y + 64.0f, actionBaseY - editor_update_modal::kButtonTopPad);
            gui::begin_modal_window(gui::ModalWindowOptions{
                .title = "Authorized Source Options",
                .position = modalPos,
                .size = modalSize,
                .viewport_size = { w, h },
                .dim_background = true
            });
            const gui::Vec2 contentPos = gui::cursor_position();
            float cursorY = contentPos.y;
            for (std::size_t line = 0u; line < 4u; ++line)
            {
                const std::string_view text = editor_update_modal::source_line(line);
                const float textHeight = gui::wrapped_text_height(text, contentWidth);
                if (cursorY + textHeight > contentBottom)
                    break;
                gui::set_cursor({ contentX, cursorY });
                gui::wrapped_label(text, contentWidth);
                cursorY += textHeight + 12.0f;
            }
            const float actionX = contentX;
            float actionY = actionBaseY;
            if (sourceActionStripStacked)
            {
                gui::set_cursor({ actionX, actionY });
                if (gui::button("Back", { contentWidth, buttonHeight }))
                {
                    editor.showSourceUpdateConfirmModal = false;
                    editor.showUpdateConfirmModal = true;
                }
                actionY += buttonHeight + buttonStackGap;
                gui::set_cursor({ actionX, actionY });
                if (gui::button(pairDeviceLabel, { contentWidth, buttonHeight }))
                {
                    armDevicePairing();
                }
                actionY += buttonHeight + buttonStackGap;
                gui::set_cursor({ actionX, actionY });
                if (gui::button("Build / Update From Source", { contentWidth, buttonHeight }))
                {
                    editor.showSourceUpdateConfirmModal = false;
                    editor.showUpdateConfirmModal = true;
                    push_editor_log(editor, "[command] Authorized source build confirmed.");
                    start_editor_source_update_install(editor);
                }
                actionY += buttonHeight + buttonStackGap;
                gui::set_cursor({ actionX, actionY });
                if (gui::button("Download Source Project", { contentWidth, buttonHeight }))
                {
                    editor.showSourceUpdateConfirmModal = false;
                    editor.showUpdateConfirmModal = true;
                    push_editor_log(editor, "[command] Project source code download confirmed.");
                    start_editor_project_source_code_download(editor);
                }
            }
            else
            {
                gui::set_cursor({ actionX, actionY });
                if (gui::button("Back", { 104.0f, buttonHeight }))
                {
                    editor.showSourceUpdateConfirmModal = false;
                    editor.showUpdateConfirmModal = true;
                }
                gui::set_cursor({ actionX + 104.0f + buttonGap, actionY });
                if (gui::button(pairDeviceLabel, { 168.0f, buttonHeight }))
                {
                    armDevicePairing();
                }
                gui::set_cursor({ actionX + 104.0f + 168.0f + 2.0f * buttonGap, actionY });
                if (gui::button("Build / Update From Source", { 208.0f, buttonHeight }))
                {
                    editor.showSourceUpdateConfirmModal = false;
                    editor.showUpdateConfirmModal = true;
                    push_editor_log(editor, "[command] Authorized source build confirmed.");
                    start_editor_source_update_install(editor);
                }
                gui::set_cursor({ actionX + 104.0f + 168.0f + 208.0f + 3.0f * buttonGap, actionY });
                if (gui::button("Download Source Project", { 220.0f, buttonHeight }))
                {
                    editor.showSourceUpdateConfirmModal = false;
                    editor.showUpdateConfirmModal = true;
                    push_editor_log(editor, "[command] Project source code download confirmed.");
                    start_editor_project_source_code_download(editor);
                }
            }
            gui::end_modal_window();
        }

        // Enables deterministic smoke coverage of GUI-only updater actions.
        if (!editor.automationConsumed)
        {
            switch (editor.automationCommand)
            {
            case EditorAutomationCommand::SmartUpdate:
                editor.automationConsumed = true;
                if (!try_claim_editor_automation_command(editor.automationCommand))
                    break;
                push_editor_log(editor, "[command] Auto command triggered: smart update.");
                append_editor_automation_trace("triggered smart-update");
                start_editor_update_install(editor);
                break;
            case EditorAutomationCommand::SourceUpdate:
                editor.automationConsumed = true;
                if (!try_claim_editor_automation_command(editor.automationCommand))
                    break;
                push_editor_log(editor, "[command] Auto command triggered: project source code download.");
                append_editor_automation_trace("triggered source-update");
                start_editor_project_source_code_download(editor);
                break;
            case EditorAutomationCommand::AssetsInteractionProof:
            case EditorAutomationCommand::CandidateLabSmokeChoose:
            case EditorAutomationCommand::CandidateLabSmokeKeep:
            case EditorAutomationCommand::SelfCodingLocalSmoke:
                break;
            case EditorAutomationCommand::None:
                break;
            }
        }
