        // Included from editor.cpp inside editor_run after toolbar/menu rendering.
        // Owns update confirmation, advanced source confirmation, and update automation UI.

        if (editor.showUpdateConfirmModal)
        {
            editor.openMenu = TopMenu::None;
            const bool sourceOnlyUpdate =
                editor.lastUpdateCheck.source_update_available
                && !editor.lastUpdateCheck.packaged_update_available;
            const bool sourceWorkerRunning = editor.updateState == EditorUpdateState::SourceWorkerRunning;
            const bool updateRunning = editor.updateCheckPending.has_value() || sourceWorkerRunning;
            const bool restartReady = editor.updateState == EditorUpdateState::RestartReady;
            if (restartReady)
                arm_editor_update_restart_countdown(editor);
            const int restartSeconds = restartReady ? editor_update_restart_countdown_seconds(editor) : 0;
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
            const float cancelButtonWidth = sourceWorkerRunning ? 148.0f : 120.0f;
            const float primaryButtonWidth = (std::min)(220.0f, (std::max)(160.0f, contentWidth * 0.34f));
            const float advancedButtonWidth = (std::min)(190.0f, (std::max)(156.0f, contentWidth * 0.28f));
            const bool showCancelButton = sourceWorkerRunning || (!updateRunning && !restartReady);
            const bool showPrimaryButton = !updateRunning;
            const bool showAdvancedSourceButton = !updateRunning && !restartReady;
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
                    editor.updateStatus = "Restart failed because the staged update handoff could not be launched. Check epoch_update_handoff.log beside the executable.";
                    clear_editor_update_restart_countdown(editor);
                    push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                    return;
                }

                editor.showUpdateConfirmModal = false;
                editor.updateStatus = "Restarting Epoch to finish the staged update handoff.";
                clear_editor_update_restart_countdown(editor);
                push_editor_log(editor, "[update] Restart requested after verified update handoff.");
                emit_command(EditorCommand::Exit);
            };
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
            if (cursorY + 22.0f <= contentBottom)
            {
                const float progressWidth = (std::max)(1.0f, (std::min)(contentWidth, 560.0f));
                gui::set_cursor({ contentX, cursorY });
                gui::progress_bar(gui::ProgressBarOptions{
                    .label = sourceWorkerRunning ? "Source rebuild" : updateRunning ? "Update" : restartReady ? "Update staged" : "Update ready",
                    .status = sourceWorkerRunning ? "cancel available" : updateRunning ? "downloading / staging" : restartReady ? "restart required" : "waiting",
                    .value = editor_update_progress_value(editor),
                    .size = { progressWidth, 22.0f },
                    .show_percent = true
                });
                cursorY += 36.0f;
            }
            emitWrapped(editor_update_modal::cache_text(flags), 8.0f);
            const std::string actionText = editor_update_modal::action_text(flags, restartSeconds);
            emitWrapped(actionText, 8.0f);
            if (actionStripStacked)
            {
                float stackedButtonY = buttonY;
                if (showCancelButton)
                {
                    gui::set_cursor({ contentX, stackedButtonY });
                    if (sourceWorkerRunning)
                    {
                        if (gui::button("Cancel Update", { contentWidth, buttonHeight }))
                        {
                            const bool cancelRequested = updater::request_source_update_cancel();
                            editor.showUpdateConfirmModal = false;
                            editor.updateState = cancelRequested ? EditorUpdateState::Available : EditorUpdateState::Failed;
                            editor.updateInstallPending = false;
                            editor.updateSourceInstallPending = false;
                            editor.updateOperationStartedAt = {};
                            editor.updateStatus = cancelRequested
                                ? "Source update cancel requested. The worker will stop at its next safe checkpoint; Update remains available for retry."
                                : "Source update modal closed, but the cancel marker could not be written; check updater logs.";
                            push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
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
                    ? std::string{ "Restart Now" }
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
                            push_editor_log(editor, "[command] Smart update confirmed.");
                            start_editor_update_install(editor);
                        }
                    }
                    stackedButtonY += buttonHeight + buttonStackGap;
                }
                if (showAdvancedSourceButton)
                {
                    gui::set_cursor({ contentX, stackedButtonY });
                    if (gui::button("Advanced Source...", { contentWidth, buttonHeight }))
                    {
                        editor.showUpdateConfirmModal = false;
                        editor.showSourceUpdateConfirmModal = true;
                        push_editor_log(editor, "[command] Advanced source rebuild requested. Awaiting confirmation.");
                    }
                }
            }
            else
            {
                gui::set_cursor({ contentX, buttonY });
                if (sourceWorkerRunning)
                {
                    if (gui::button("Cancel Update", { cancelButtonWidth, buttonHeight }))
                    {
                        const bool cancelRequested = updater::request_source_update_cancel();
                        editor.showUpdateConfirmModal = false;
                        editor.updateState = cancelRequested ? EditorUpdateState::Available : EditorUpdateState::Failed;
                        editor.updateInstallPending = false;
                        editor.updateSourceInstallPending = false;
                        editor.updateOperationStartedAt = {};
                        editor.updateStatus = cancelRequested
                            ? "Source update cancel requested. The worker will stop at its next safe checkpoint; Update remains available for retry."
                            : "Source update modal closed, but the cancel marker could not be written; check updater logs.";
                        push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                    }
                }
                else if (!updateRunning && !restartReady && gui::button("Cancel", { 120.0f, buttonHeight }))
                {
                    editor.showUpdateConfirmModal = false;
                    push_editor_log(editor, "[command] Update canceled.");
                }

                const float primaryButtonX = showCancelButton
                    ? (std::min)(contentRight - primaryButtonWidth, contentX + cancelButtonWidth + buttonGap)
                    : contentRight - primaryButtonWidth;
                gui::set_cursor({ primaryButtonX, buttonY });
                const std::string primaryUpdateLabel = restartReady
                    ? std::string{ "Restart Now" }
                    : sourceOnlyUpdate ? std::string{ "Update From Source" } : std::string{ "Install Release" };
                if (showPrimaryButton && gui::button(primaryUpdateLabel, { primaryButtonWidth, buttonHeight }))
                {
                    if (restartReady)
                    {
                        requestUpdateRestart();
                    }
                    else
                    {
                        push_editor_log(editor, "[command] Smart update confirmed.");
                        start_editor_update_install(editor);
                    }
                }
                const float advancedButtonX = (std::max)(contentX, contentRight - advancedButtonWidth);
                gui::set_cursor({ advancedButtonX, buttonY });
                if (showAdvancedSourceButton && gui::button("Advanced Source...", { advancedButtonWidth, buttonHeight }))
                {
                    editor.showUpdateConfirmModal = false;
                    editor.showSourceUpdateConfirmModal = true;
                    push_editor_log(editor, "[command] Advanced source rebuild requested. Awaiting confirmation.");
                }
            }
            gui::end_modal_window();
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
            const float actionBaseY = modalPos.y + modalSize.y - sourceActionStripHeight - editor_update_modal::kButtonBottomPad;
            const float contentBottom = (std::max)(modalPos.y + 64.0f, actionBaseY - editor_update_modal::kButtonTopPad);
            gui::begin_modal_window(gui::ModalWindowOptions{
                .title = "Rebuild From Main Source",
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
                if (gui::button("Cancel", { contentWidth, buttonHeight }))
                {
                    editor.showSourceUpdateConfirmModal = false;
                    push_editor_log(editor, "[command] Advanced source rebuild canceled.");
                }
                actionY += buttonHeight + buttonStackGap;
                gui::set_cursor({ actionX, actionY });
                if (gui::button("Start Source Rebuild", { contentWidth, buttonHeight }))
                {
                    editor.showSourceUpdateConfirmModal = false;
                    editor.showUpdateConfirmModal = true;
                    push_editor_log(editor, "[command] Advanced source rebuild confirmed.");
                    start_editor_source_update_install(editor);
                }
            }
            else
            {
                gui::set_cursor({ actionX, actionY });
                if (gui::button("Back", { 120.0f, buttonHeight }))
                {
                    editor.showSourceUpdateConfirmModal = false;
                    editor.showUpdateConfirmModal = true;
                }
                gui::set_cursor({ actionX + 120.0f + buttonGap, actionY });
                if (gui::button("Cancel", { 120.0f, buttonHeight }))
                {
                    editor.showSourceUpdateConfirmModal = false;
                    push_editor_log(editor, "[command] Advanced source rebuild canceled.");
                }
                gui::set_cursor({ actionX + 240.0f + 2.0f * buttonGap, actionY });
                if (gui::button("Start Source Rebuild", { 176.0f, buttonHeight }))
                {
                    editor.showSourceUpdateConfirmModal = false;
                    editor.showUpdateConfirmModal = true;
                    push_editor_log(editor, "[command] Advanced source rebuild confirmed.");
                    start_editor_source_update_install(editor);
                }
            }
            gui::end_modal_window();
        }

        // Enables deterministic smoke coverage of GUI-only updater actions.
        if (!editor.automationConsumed)
        {
            if (editor.automationCommand != EditorAutomationCommand::None
                && !try_claim_editor_automation_command(editor.automationCommand))
            {
                editor.automationConsumed = true;
            }

            switch (editor.automationCommand)
            {
            case EditorAutomationCommand::SmartUpdate:
                if (editor.automationConsumed)
                    break;
                editor.automationConsumed = true;
                push_editor_log(editor, "[command] Auto command triggered: smart update.");
                append_editor_automation_trace("triggered smart-update");
                start_editor_update_install(editor);
                break;
            case EditorAutomationCommand::SourceUpdate:
                if (editor.automationConsumed)
                    break;
                editor.automationConsumed = true;
                push_editor_log(editor, "[command] Auto command triggered: advanced source rebuild.");
                append_editor_automation_trace("triggered source-update");
                start_editor_source_update_install(editor);
                break;
            case EditorAutomationCommand::None:
                break;
            }
        }
