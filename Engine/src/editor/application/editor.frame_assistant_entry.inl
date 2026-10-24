// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        auto submit_ai_request = [&](
            std::string prompt,
            std::string display,
            AiDeferredRequestKind requestKind) -> bool
        {
            if (trim_ai_command(prompt).empty() || trim_ai_command(display).empty())
            {
                chat.append_status("Empty AI dispatch was blocked; no model request sent.");
                return false;
            }
            std::scoped_lock selectionLock(aiModelSelectionMutex);
            if (chat.pending
                || editor.aiDeferredRequestKind
                    != AiDeferredRequestKind::None)
            {
                push_editor_log(
                    editor,
                    "[ai] Chat is still processing the previous request.");
                return false;
            }

#if EPOCH_EDITION_PUBLIC
            if (!project_ai_scope_allowed(editor.projectRoot))
            {
                chat.append_status("Choose a game or application project. Engine source is unavailable to this edition's assistant.");
                return false;
            }
#endif
            const auto selection = epochengine::ai::local_model_selection();
            chat.statusRequestKind = requestKind;
            if (requestKind == AiDeferredRequestKind::Chat)
                chat.append_user_message(display, display);
#if EPOCH_EDITION_PUBLIC
            constexpr bool needsCodingChoice = false;
#endif
            // Request preparation can restore preferences, initialize a provider or
            // touch disk. Keep it on the existing AiChat request jthread; never
            // perform it during a GUI input/frame callback. Unconfirmed model
            // use still requires operator consent before enqueuing the worker.
            if (needsCodingChoice || !epochengine::ai::is_model_use_confirmed())
            {
                (void)stage_ai_model_consent(
                    editor,
                    requestKind,
                    std::move(prompt),
                    std::move(display));
                push_editor_log(
                    editor,
                    "[ai] Request staged until selected-model use is confirmed.");
                if (needsCodingChoice)
                    editor.aiModelConsentStatus = "Legacy Nemotron cannot silently replace the coding provider. "
                        "For Qwen Next Flash, Load Native Qwen Flash Next and Use Native for Epoch. "
                        "For the standalone fallback, choose Epoch-local Qwen3.8 27B. "
                        "Your objective is retained; no request has been sent.";
                return false;
            }

            editor.aiDeferredPrompt = std::move(prompt);
            editor.aiDeferredDisplay = std::move(display);
            editor.aiDeferredRequestKind = requestKind;
            editor.aiDeferredModelLease = std::make_shared<AiModelSelectionLease>();
            editor.aiDeferredDispatchQueued = true;
            return true;
        };

        for (auto& command : consume_detached_ai_chat_commands(ctx.get()))
        {
            switch (command.kind)
            {
            case DetachedAiChatCommandKind::submit:
                if (command.prompt.empty() || is_ws_only(command.prompt))
                    break;
                if (chat.pending
                    || !chat.queuedInput.empty()
                    || editor.aiDeferredRequestKind
                        != AiDeferredRequestKind::None)
                {
                    // Preserve the exact detached draft until the canonical
                    // session can accept it. Do not turn a one-frame projection
                    // race into dropped user input.
                    (void)queue_detached_ai_chat_command(
                        ctx.get(),
                        DetachedAiChatCommandKind::submit,
                        std::move(command.prompt));
                    editor.aiAuthoringStatus =
                        "Detached AI message retained; waiting for the current request to finish.";
                    break;
                }
                {
                    if (chat.queue_input(command.prompt, !editor.aiIntentClarificationPending
                            && !ai_chat_confirmation(command.prompt).has_value()))
                    {
                        editor.aiAuthoringStatus =
                            "Detached Project Assistant message queued with active-project context; waiting for the shared AI/build work lane.";
                        push_editor_log(
                            editor,
                            "[ai-project] Detached Project Assistant message queued on the canonical editor session.");
                    }
                }
                break;
            case DetachedAiChatCommandKind::retry_queued:
                if (editor.aiDeferredRequestKind
                        == AiDeferredRequestKind::Chat
                    && !editor.aiDeferredDispatchQueued)
                {
                    editor.aiDeferredDispatchQueued = true;
                    editor.aiAuthoringStatus =
                        "Retry queued for the canonical AI session.";
                }
                break;
            case DetachedAiChatCommandKind::cancel_queued:
                if (editor.aiDeferredRequestKind
                    == AiDeferredRequestKind::Chat)
                {
                    chat.append_status(
                        "Queued message cancelled before transport. Its text: "
                        + editor.aiDeferredDisplay);
                    editor.aiDeferredRequestKind =
                        AiDeferredRequestKind::None;
                    editor.aiDeferredDispatchQueued = false;
                    editor.aiDeferredPrompt.clear();
                    editor.aiDeferredDisplay.clear();
                    editor.aiDeferredModelLease.reset();
                    editor.showAiModelConsentModal = false;
                    editor.aiAuthoringStatus =
                        "Queued detached AI message cancelled before transport.";
                }
                break;
            case DetachedAiChatCommandKind::stop_pending:
                if (chat.pending)
                    (void)chat.cancel_pending();
                break;
            }
        }

        auto submit_ai_prompt = [&](std::string prompt, std::string_view logLine)
        {
            const std::string display = prompt;
            if (submit_ai_request(
                    std::move(prompt),
                    display,
                    AiDeferredRequestKind::Chat))
            {
                push_editor_log(editor, std::string(logLine));
            }
        };
