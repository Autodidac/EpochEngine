// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        auto submit_ai_chat_input = [&](std::string text) -> bool
        {
            const std::string_view input = trim_ai_command(text);
            if (input.empty())
            {
                chat.append_status("Type a message before sending.");
                return true;
            }

            // Goal lifecycle is owned by chat, not by a second form above it.
            // Explicit goal commands are processed by the host even when the
            // model is unavailable, without granting authority to bypass
            // plan/candidate approval or the project's sandbox policy.
            const auto goalCommand = epochengine::editor::ai_chat_goal::parse(input);
            if (goalCommand.action != epochengine::editor::ai_chat_goal::Action::none)
            {
                chat.append_user_message(std::string{input});
                switch (goalCommand.action)
                {
                case epochengine::editor::ai_chat_goal::Action::set:
                    if (goalCommand.objective.empty())
                    {
                        chat.append_status("Specify the objective after 'Project goal:'. No goal was changed.");
                        return true;
                    }
                    if (editor.aiAuthoringApplyPending)
                    {
                        chat.append_status("A project change is being applied. Wait until it finishes before replacing the goal.");
                        return true;
                    }
                    {
                        const bool changed = !editor.aiGoalActive
                            || editor.aiGoal != goalCommand.objective;
                        invalidate_ai_goal_work();
                        editor.aiGoal.assign(goalCommand.objective);
                        editor.aiGoalDraft = editor.aiGoal;
                        editor.aiGoalActive = true;
                        editor.aiGoalRunning = true;
                        editor.aiGoalEditing = false;
                        if (changed)
                        {
                            editor.aiGoalCompletedMilestones = 0u;
                            editor.aiGoalAttemptedMilestoneSignatures.clear();
                            editor.aiGoalLastMilestoneSignature.clear();
                            editor.aiGoalLastMilestoneReceipt.clear();
                        }
                        editor.aiGoalPlanNextQueued = true;
                        chat.append_status(changed
                            ? "Project goal saved. Milestone planning is queued; changes still require approval."
                            : "Project goal resumed. The next milestone is queued.");
                    }
                    return true;
                case epochengine::editor::ai_chat_goal::Action::pause:
                    if (!editor.aiGoalActive)
                        chat.append_status("No project goal is active. Say 'Project goal: <objective>' to create one.");
                    else
                    {
                        invalidate_ai_goal_work();
                        editor.aiGoalRunning = false;
                        chat.append_status("Project goal paused; the objective and completed milestones are retained.");
                    }
                    return true;
                case epochengine::editor::ai_chat_goal::Action::resume:
                    if (!editor.aiGoalActive)
                        chat.append_status("No project goal is active. Say 'Project goal: <objective>' to create one.");
                    else
                    {
                        editor.aiGoalRunning = true;
                        editor.aiGoalPlanNextQueued = true;
                        chat.append_status("Project goal resumed. Next milestone queued for planning.");
                    }
                    return true;
                case epochengine::editor::ai_chat_goal::Action::erase:
                    invalidate_ai_goal_work();
                    editor.aiGoal.clear();
                    editor.aiGoalDraft.clear();
                    editor.aiGoalActive = false;
                    editor.aiGoalRunning = false;
                    editor.aiGoalEditing = false;
                    editor.aiGoalCompletedMilestones = 0u;
                    editor.aiGoalAttemptedMilestoneSignatures.clear();
                    editor.aiGoalLastMilestoneSignature.clear();
                    editor.aiGoalLastMilestoneReceipt.clear();
                    chat.append_status("Project goal ended. Existing candidate changes were not promoted.");
                    return true;
                case epochengine::editor::ai_chat_goal::Action::status:
                    chat.append_status(!editor.aiGoalActive
                        ? "No active project goal. Say 'Project goal: <objective>' to create one."
                        : "Project goal (" + std::string{editor.aiGoalRunning ? "planning" : "paused"}
                            + ", milestones completed: "
                            + std::to_string(editor.aiGoalCompletedMilestones)
                            + "): " + editor.aiGoal);
                    return true;
                case epochengine::editor::ai_chat_goal::Action::none: break;
                }
            }

            if (editor.aiIntentClarificationPending)
            {
                const std::string lower = ai_lower_text(input);
                if (lower.compare("cancel") == 0
                    || lower.compare("never mind") == 0
                    || lower.compare("nevermind") == 0)
                {
                    chat.append_user_message(std::string{input});
                    editor.aiIntentClarificationPending = false;
                    editor.aiIntentClarificationResumeIntent = AiChatIntent::Ambiguous;
                    editor.aiIntentClarificationRequest.clear();
                    editor.aiIntentClarificationQuestion.clear();
                    chat.append_status("Clarification cancelled.");
                    return true;
                }

                if (editor.aiIntentClarificationResumeIntent
                    != AiChatIntent::Ambiguous)
                {
                    chat.append_user_message(std::string{input});
                    std::string resumed =
                        std::exchange(editor.aiIntentClarificationRequest, {});
                    const AiChatIntent resumeIntent =
                        editor.aiIntentClarificationResumeIntent;
                    editor.aiIntentClarificationPending = false;
                    editor.aiIntentClarificationResumeIntent = AiChatIntent::Ambiguous;
                    if (resumeIntent == AiChatIntent::ProjectAuthoring
                        && editor_ai_source_intent::requires_engine_source(resumed))
                    {
                        // Recover a clarification started by the old router.
                        // Preserve user style/name answers and let the host
                        // discover existing theme definitions from the repo.
                        for (const auto& [question, answer] : editor.aiProjectClarifications)
                            resumed += "\nPrior clarification (" + question + "): " + answer;
                        editor.aiProjectClarifications.clear();
                        editor.aiIntentClarificationQuestion.clear();
                        resumed += "\nUser clarification: ";
                        resumed.append(input);
                        return dispatch_ai_intent(AiChatIntent::EngineSource, resumed, true);
                    }
                    if (resumeIntent == AiChatIntent::ProjectAuthoring)
                    {
                        editor.aiProjectClarifications.emplace_back(
                            std::exchange(editor.aiIntentClarificationQuestion, {}), std::string{input});
                    }
                    else
                    {
                        resumed += "\nUser clarification: ";
                        resumed.append(input.data(), input.size());
                    }
                    return dispatch_ai_intent(resumeIntent, resumed, true);
                }

                if (const auto route = clarification_route(input))
                {
                    chat.append_user_message(std::string{input});
                    const std::string original =
                        std::exchange(editor.aiIntentClarificationRequest, {});
                    editor.aiIntentClarificationPending = false;
                    editor.aiIntentClarificationResumeIntent = AiChatIntent::Ambiguous;
                    return dispatch_ai_intent(*route, original);
                }
                chat.append_user_message(std::string{input});
                chat.append_status(
                    "I still need the target mode: Engine Source, Project Change, Project Tool, or Just Answer. You can type the choice or use the buttons below.");
                return true;
            }

            // A direct answer to a host/model clarification owns the next
            // message.  Only when no clarification is pending may a short
            // yes/no reply operate the currently staged approval controls.
            if (queue_text_message_action(input))
                return true;

            if (const auto mission = ai_command_argument(input, "/mission"))
            {
                if (mission->empty())
                {
                    chat.append_status("Usage: /mission <documentation or engine-work objective>. Epoch retains AGENTS.md and Changes/mission_cache.md in the candidate.");
                    return true;
                }
                return start_engine_source_iteration(*mission);
            }
            if (const auto tool = ai_command_argument(input, "/tool"))
            {
                if (!tool->empty())
                    editor.aiToolRequest = std::string{*tool};
                if (editor.aiToolRequest.empty())
                {
                    editor.aiToolStatus =
                        "Usage: /tool <one active-project operation>.";
                    chat.append_status(editor.aiToolStatus);
                    return true;
                }
                return request_ai_tool_plan();
            }
            if (const auto plan = ai_command_argument(input, "/plan"))
            {
                if (!plan->empty())
                    editor.aiAuthoringRequest = std::string{*plan};
                if (editor.aiAuthoringRequest.empty())
                {
                    editor.aiAuthoringStatus =
                        "Usage: /plan <scene or GUI change>.";
                    chat.append_status(editor.aiAuthoringStatus);
                    return true;
                }
                return request_ai_authoring_plan(false);
            }
            // The header button is now only an explicit one-shot override.
            // Normal messages are routed by intent automatically.
            const AiChatIntent intent = classify_ai_intent(input);
            if (intent == AiChatIntent::Ambiguous)
            {
                editor.aiIntentClarificationPending = true;
                editor.aiIntentClarificationResumeIntent = AiChatIntent::Ambiguous;
                editor.aiIntentClarificationRequest = std::string{input};
                chat.append_user_message(std::string{input});
                chat.append_status(
                    "This sounds like a change request, but the target is ambiguous. Should I change Engine Source, change the Project/Scene, run a Project Tool, or Just Answer? You can type the choice or use the buttons below.");
                return true;
            }
            return dispatch_ai_intent(intent, input);
        };

        if (editor.aiProjectScriptRepairQueued && !chat.pending && !editor.aiAuthoringAwaitingReply
            && !editor.aiAuthoringApplyPending && !source_model_slot_busy(editor, chat)
            )
        {
            if (request_ai_authoring_plan(false)) editor.aiProjectScriptRepairQueued = false;
        }

        const bool goalPlanAwaitingApproval = editor.aiAuthoringPlan
            && !editor.aiAuthoringPlanApplied;
        if (editor.aiGoalPlanNextQueued
            && editor.aiGoalActive
            && editor.aiGoalRunning
            && chat.queuedInput.empty()
            && !editor.aiAuthoringAwaitingReply
            && !editor.aiAuthoringApplyPending
            && !goalPlanAwaitingApproval
            && !source_model_slot_busy(editor, chat)
            && !ai_source_native_work_active(editor, chat))
        {
            (void)request_ai_authoring_plan(true);
        }

        // A message may wait while any selected model is being prepared, but
        // the wait itself is bounded to one hour. Never expire a running request,
        // unload its weights, or replace the chosen transport on expiry.
        namespace queue = epochengine::editor::ai_chat_queue;
        if (!chat.queuedInput.empty() && queue::queued_load_expired(
                chat.queuedInputAt, std::chrono::steady_clock::now()))
        {
            if (chat.input.empty()) chat.input = chat.queuedDraftText;
            chat.restore_reference_images(chat.queuedReferenceImages);
            chat.append_status("Model loading/waiting exceeded 1 hour. The request was not dispatched. Your prompt is preserved in the composer for retry.");
            chat.queuedInput.clear();
            chat.queuedInputAt = {};
            editor.aiChatQueuedMethodSelection = -1;
            editor.aiChatQueuedModel.clear();
        }

        // The native runtime supplies loaded/busy state. Direct GGUF reports
        // readiness directly. A remote API exposes no reliable residency
        // signal; allow up to six seconds after confirmation to finish loading
        // rather than sending into a freshly selected endpoint immediately.
#if EPOCH_EDITION_PUBLIC
        if (editor.aiMethodSelection > 2) editor.aiMethodSelection = -1;
#endif
        const auto queuedNative = epochengine::ai::native_runtime::snapshot(false);
        const bool queuedMethodMatches = editor.aiChatQueuedMethodSelection < 0
            || editor.aiChatQueuedMethodSelection == editor.aiMethodSelection;
        // Automatic routing legitimately changes the *runtime* model; the
        // chat ticket instead pins the operator's selection revision. Do not
        // compare against native_runtime::snapshot().model or the active API
        // model: those are execution details, not a user selection event.
        const bool queuedModelMatches = queue::user_selection_unchanged(
            editor.aiChatQueuedUserModelSelectionRevision,
            editor.aiChatUserModelSelectionRevision);
        const auto queuedTransport = [&]() noexcept
        {
            switch (editor.aiMethodSelection)
            {
                case 0: return queue::Transport::native;
                case 1: return queue::Transport::local_api;
                case 2: return queue::Transport::direct_gguf;
                case 3: return queue::Transport::external_mcp;
                case 4: return queue::Transport::codex;
                default: return queue::Transport::none;
            }
        }();
        const queue::Status queuedStatus{
            .transport = queuedTransport,
            .selected = queuedNative.selected,
            .loaded = queuedNative.loaded,
#if EPOCH_EDITION_PUBLIC
            .busy = editor.aiMethodSelection == 0 && queuedNative.busy,
            .confirmed = ai::is_model_use_confirmed(),
            .inventory_pending = editor.aiModelInventoryPending.has_value(),
            .provider_ready = editor.aiMethodSelection == 2 ? ai::direct_runtime_status().ready() : true,
#endif
            .confirmed_at = editor.aiLocalModelConfirmedAt
        };
        const bool queuedProviderReady = queue::ready_to_dispatch(
            queuedStatus, std::chrono::steady_clock::now());
        // Never leave a queued turn parked silently after a provider/model
        // change. Preserve the draft rather than dispatching on an unintended
        // model, or waiting an hour while the CPU/GPU sit idle.
        if (!chat.queuedInput.empty() && (!queuedMethodMatches || !queuedModelMatches))
        {
            if (chat.input.empty()) chat.input = chat.queuedDraftText;
            chat.restore_reference_images(chat.queuedReferenceImages);
            chat.append_status(std::string{queue::dispatch_block_reason(
                queue::Status{.transport = queuedTransport}, queuedMethodMatches,
                queuedModelMatches, std::chrono::steady_clock::now())}
                + " The queued text is back in the composer; no request was sent.");
            chat.queuedInput.clear();
            chat.queuedInputAt = {};
            chat.queuedBlockReported.clear();
            editor.aiChatQueuedMethodSelection = -1;
            editor.aiChatQueuedModel.clear();
        }
        const bool queueOwnerReady = editor.aiDeferredRequestKind == AiDeferredRequestKind::None
            && !editor.aiAuthoringApplyPending
            && !editor.aiAuthoringAwaitingReply
            && !ai_source_native_work_active(editor, chat);
        const auto queuedBlock = queue::dispatch_block_reason(queuedStatus,
            queuedMethodMatches, queuedModelMatches, std::chrono::steady_clock::now());
        // One actionable diagnosis per state change, after a short grace
        // period. Do not claim a queued prompt is generating or alter weights.
        if (!chat.queuedInput.empty() && chat.queuedInputAt != std::chrono::steady_clock::time_point{}
            && std::chrono::steady_clock::now() - chat.queuedInputAt >= std::chrono::seconds{5})
        {
            const std::string why = !queuedBlock.empty() ? std::string{queuedBlock}
                : !queueOwnerReady ? "Previous agent/work operation is still retiring."
                : std::string{};
            if (!why.empty() && why != chat.queuedBlockReported)
            {
                chat.queuedBlockReported = why;
                // Queued prompts are already visible beside the composer.
                // Do not append a delayed warning underneath their Activity.
                push_editor_log(editor, "[ai-queue] " + why);
            }
        }
        if (queueOwnerReady && queuedMethodMatches
            && queuedModelMatches && queuedProviderReady)
        {
            if (auto queued = chat.take_queued_input())
            {
                editor.aiChatQueuedMethodSelection = -1;
                editor.aiChatQueuedModel.clear();
                struct UserEchoScope final
                {
                    AiChat& chat;
                    explicit UserEchoScope(AiChat& value) : chat(value)
                        { chat.suppressUserEcho = true; }
                    ~UserEchoScope() { chat.suppressUserEcho = false; }
                } echoScope{chat};
                if (!chat.queuedInputRetryable
                    && chat.queuedCompletionGeneration != chat.completionGeneration)
                    chat.append_status("The staged response changed after your message was queued. Please review it and confirm again; the earlier approval was not replayed.");
                else if (!submit_ai_chat_input(*queued))
                {
                    if (chat.input.empty()) chat.input = chat.activeDraftText;
                    chat.restore_reference_images(chat.activeReferenceImages);
                    chat.append_status("The queued request was not admitted by the destination. "
                        "Its text and attachments are retained in the composer; check Assistant status and retry.");
                }
            }
        }

