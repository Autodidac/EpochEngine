// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        auto render_ai_chat_tool_pane =
            [&](EditorToolDockRegion region)
        {
        // The Assistant center is the verified candidate workbench.
        // AI Chat owns only its selected dock and its per-turn Activity timeline.
        if (active_pane_for_region(region) != "pane.ai_chat"
            || !pane_available("pane.ai_chat"))
            return;
        voiceChatVisibleThisFrame = true;
        const gui::Vec2 chat_render_pos = tool_group_content_position(region);
        const gui::Vec2 availableChatSize = tool_group_content_size(region);
        const bool modelConsentPending = editor.aiMethodSelection < 0 &&
            editor.aiDeferredRequestKind != AiDeferredRequestKind::None
            && !epochengine::ai::is_model_use_confirmed();
        if (modelConsentPending)
            (void)poll_ai_model_inventory(editor);
        const bool consentInventoryPending =
            editor.aiModelInventoryPending.has_value();
        const bool consentModelReady = !consentInventoryPending
            && !editor.aiPendingModelSelection.empty();
        const bool planAwaitingApproval = !editor.aiProjectScriptDraft.empty()
            || (editor.aiAuthoringPlan && !editor.aiAuthoringPlanApplied && !editor.aiAuthoringApplyPending);
        const bool toolPlanAwaitingApproval = editor.aiToolPlan
            && !editor.aiToolPlanApplied;
        const bool toolTestPending =
            editor.aiToolTestPending.has_value();
        const std::array applyJobActions{
            gui::ConsoleWindowActionSpec{.label = "Cancel Authoring Job", .width = 180.0f, .activate_on_press = true}};
        const std::array planActions{
            gui::ConsoleWindowActionSpec{
                .label = editor.aiProjectScriptDraft.empty() ? "Apply Plan" : "Save Script",
                .width = 132.0f,
                .activate_on_press = true},
            gui::ConsoleWindowActionSpec{
                .label = "Discard",
                .width = 88.0f,
                .activate_on_press = true}
        };
        const std::array scriptActions{
            gui::ConsoleWindowActionSpec{.label = "Save Script", .width = 132.0f, .activate_on_press = true},
            gui::ConsoleWindowActionSpec{.label = "Save, Build + Run", .width = 180.0f, .activate_on_press = true},
            gui::ConsoleWindowActionSpec{.label = "Discard", .width = 88.0f, .activate_on_press = true}
        };
        const std::array toolActions{
            gui::ConsoleWindowActionSpec{
                .label =
                    editor.aiToolPlan.plan.call.tool == "project.run"
                        ? "Approve Project Run"
                        : editor.aiToolPlan.plan.call.tool == "project.test"
                            ? "Approve Project Test"
                            : "Approve Tool",
                .width = 184.0f,
                .activate_on_press = true},
            gui::ConsoleWindowActionSpec{
                .label = "Discard",
                .width = 88.0f,
                .activate_on_press = true}
        };
        const std::array toolTestActions{
            gui::ConsoleWindowActionSpec{
                .label = "Cancel Project Test",
                .width = 176.0f,
                .activate_on_press = true}
        };
        const std::array consentActions{
            gui::ConsoleWindowActionSpec{
                .label = consentInventoryPending
                    ? "Finding Models..."
                    : "Use Selected Model",
                .width = 172.0f,
                .enabled = consentModelReady,
                .activate_on_press = true},
            gui::ConsoleWindowActionSpec{
                .label = "Not Now",
                .width = 92.0f,
                .activate_on_press = true}
        };
        const std::array intentClarificationActions{
            gui::ConsoleWindowActionSpec{
                .label = "Engine Source", .width = 132.0f,
                .activate_on_press = true, .tone = gui::SemanticTone::neutral},
            gui::ConsoleWindowActionSpec{
                .label = "Project Change", .width = 132.0f,
                .activate_on_press = true, .tone = gui::SemanticTone::neutral},
            gui::ConsoleWindowActionSpec{
                .label = "Project Tool", .width = 112.0f,
                .activate_on_press = true, .tone = gui::SemanticTone::neutral},
            gui::ConsoleWindowActionSpec{
                .label = "Just Answer", .width = 108.0f,
                .activate_on_press = true, .tone = gui::SemanticTone::neutral}
        };
        const std::array criticalClarificationActions{
            gui::ConsoleWindowActionSpec{
                .label = "Cancel Question", .width = 132.0f,
                .activate_on_press = true, .tone = gui::SemanticTone::neutral}
        };
        // Missing-model decisions are actual host-owned controls. Plain chat
        // messages with Copy/Edit affordances must not masquerade as actions.
        const auto modelChoiceState = epochengine::ai::native_runtime::snapshot(false);
        const bool modelChoiceButtonsVisible = editor.aiMethodSelection <= 0
            && !modelChoiceState.loaded && !modelChoiceState.busy
            && !chat.pending && !chat.worker.joinable()
            ;
        const std::array modelChoiceActions{
            gui::ConsoleWindowActionSpec{
                .label="Choose Model", .width=124.0f,
                .activate_on_press=true, .tone=gui::SemanticTone::info},
            gui::ConsoleWindowActionSpec{
                .label="Restore Saved Set", .width=166.0f,
                .activate_on_press=true, .tone=gui::SemanticTone::neutral}
        };
        // Popover content is rendered above the composer; no separate toolbar
        // consumes the conversation's vertical space.
#if EPOCH_EDITION_PUBLIC
        constexpr bool engineWorkActive = false;
        constexpr bool selfCodingPaused = false;
        constexpr bool showEngineTask = false;
        constexpr std::string_view aiChatModeTitle = "Assistant";
#endif
        const std::span<const gui::ConsoleWindowActionSpec> messageActions =
            editor.aiAuthoringApplyPending
            ? std::span<const gui::ConsoleWindowActionSpec>{applyJobActions}
            : editor.aiIntentClarificationPending
                && editor.aiIntentClarificationResumeIntent == AiChatIntent::Ambiguous
            ? std::span<const gui::ConsoleWindowActionSpec>{intentClarificationActions}
            : editor.aiIntentClarificationPending
                ? std::span<const gui::ConsoleWindowActionSpec>{criticalClarificationActions}
            : toolTestPending
            ? std::span<const gui::ConsoleWindowActionSpec>{toolTestActions}
            : toolPlanAwaitingApproval
                ? std::span<const gui::ConsoleWindowActionSpec>{toolActions}
            : !editor.aiProjectScriptDraft.empty()
            ? std::span<const gui::ConsoleWindowActionSpec>{scriptActions}
            : planAwaitingApproval
            ? std::span<const gui::ConsoleWindowActionSpec>{planActions}
            : modelConsentPending
                ? std::span<const gui::ConsoleWindowActionSpec>{consentActions}
            : modelChoiceButtonsVisible
                ? std::span<const gui::ConsoleWindowActionSpec>{modelChoiceActions}
            : std::span<const gui::ConsoleWindowActionSpec>{};
        const gui::Vec2 chat_render_size{
            (std::max)(0.0f, availableChatSize.x),
            (std::max)(0.0f, availableChatSize.y) };
        // AI Chat is rendered only in its selected dock region. Its title
        // remains draggable independently of the Assistant workbench.
        handle_pane_title_drag_to_tab(
            "pane.ai_chat", aiChatModeTitle,
            chat_render_pos, chat_render_size);
        // Consecutive token deltas form selectable, wrapped conversation blocks.
        const auto voiceOwner=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&editor));
        // Start discovery during editor use, rather than on the first voice
        // button click. Discovery enumerates providers without opening a mic.
        if (!epochengine::ai::voice_runtime::snapshot().probed)
            epochengine::ai::voice_runtime::probe();
        const auto voiceState=epochengine::ai::voice_runtime::snapshot();
        // Windows speech events are finalized utterances, not partial guesses.
        // Preserve the user's editable draft. Only spoken control commands or
        // explicitly enabled conversation utterances are auto-submitted.
        std::vector<std::string> spokenAiInstructions{};
        for (const auto& transcript : epochengine::ai::voice_runtime::transcripts_since(
                editor.aiVoiceTranscriptSequence, voiceOwner))
        {
            editor.aiVoiceTranscriptSequence = transcript.sequence;
            if (voiceState.owner != voiceOwner
                || transcript.generation != voiceState.generation) continue;
            const bool activeModel = chat.pending || chat.worker.joinable()
                ;
            // A recognizer hypothesis is never a verified instruction. Even
            // in conversation mode, let the user correct transcription before
            // it can issue a tool call or change a running coding campaign.
            (void)activeModel;
            if (editor.aiVoiceConversation && transcript.text.size() <= 4096u)
            {
                // Conversation mode is explicit opt-in. Dispatch through the
                // ordinary typed-message and steering permission gates below.
                spokenAiInstructions.push_back(transcript.text);
            }
            else if (chat.input.size() + transcript.text.size() + 1u <= 65536u)
            {
                const std::string spoken = (chat.input.empty() ? "" : " ")
                    + transcript.text;
                if (!gui::paste_text_in_edit_box(chat.input, spoken, 65536u))
                    chat.append_status("Dictation could not insert text; microphone remains active.");
            }
            else chat.append_status("VOICE: Transcript exceeds the editable input limit; nothing was submitted.");
        }
        if(editor.aiVoiceConversation&&voiceState.owner==voiceOwner&&chat.lines.size()>editor.aiVoiceSpokenLines){
            for(std::size_t n=editor.aiVoiceSpokenLines;n<chat.lines.size();++n){
                const auto& line=chat.lines[n];
                if(line.starts_with("Assistant: ")&&line.find("```")==std::string::npos)
                    (void)epochengine::ai::voice_runtime::speak(voiceOwner,line.substr(11u));
            }
            editor.aiVoiceSpokenLines=chat.lines.size();
        }
        for(const auto& event:epochengine::ai::native_runtime::activities_since(chat.nativeActivitySequence)){
            chat.nativeActivitySequence=event.sequence;
            chat.remember_activity(event.group,event.text);
        }
        // The operator has ONE chronological agentic transcript. Host calls,
        // tool results and raw tokens live under their owning request; the
        // Details toggle expands those disclosures IN PLACE.
        const auto& displayedChatLines = chat.lines;
        const auto chatLineRoles = ai_chat_message_roles(displayedChatLines);
        const auto chatRetryableLines = chat.retryable_lines();
        // Readiness advances even while AI Controls is closed.
        if (editor.aiMethodSelection == 1 && editor.aiAutomaticModelSelection)
        {
            const auto discovered = poll_ai_model_inventory(editor);
            if (!editor.aiModelInventoryPending && !discovered.empty())
            {
                if (editor.aiPendingModelSelection.empty())
                    editor.aiPendingModelSelection = suggest_discovered_ai_model(discovered);
                if (confirm_pending_ai_model(editor)) editor.aiAutomaticModelSelection = false;
            }
        }
        const auto nativeReadiness=epochengine::ai::native_runtime::snapshot(false);
        // Repair an already-loaded native set after a stale/old workspace or
        // when the Model Manager was closed before startup finished. Only the
        // *unset* transport is adopted: never override an explicit Local API,
        // Direct GGUF, MCP or Codex choice or a queued request on one of them.
        if (editor.aiMethodSelection < 0 && nativeReadiness.selected
            && nativeReadiness.loaded && editor.aiChatQueuedMethodSelection < 0)
            (void)select_native_chat_transport(editor);
#if EPOCH_EDITION_PUBLIC
        constexpr bool codexReady = false;
        constexpr bool codexBusy = false;
#endif
        const bool methodReady=editor.aiMethodSelection==0
            ?nativeReadiness.selected&&nativeReadiness.loaded&&!nativeReadiness.busy
            :editor.aiMethodSelection==2?epochengine::ai::direct_runtime_status().ready()
            :editor.aiMethodSelection==4?codexReady
            :editor.aiMethodSelection==3||epochengine::ai::is_model_use_confirmed();
        const bool chatCanAccept = !chat.requestRetirementFailed
            && chat.queuedInput.empty() && !editor.aiAuthoringApplyPending
            && editor.aiDeferredRequestKind == AiDeferredRequestKind::None;
        const bool chatWorking = chat.pending || chat.worker.joinable()
            || codexBusy || engineWorkActive;
        const bool chatCanQueue = chatCanAccept && !chatWorking;
        const bool chatStopAvailable = chatWorking || chat.requestRetirementFailed
            || !chat.queuedInput.empty()
            || editor.aiDeferredRequestKind != AiDeferredRequestKind::None;
        const auto queuedModelIdentity = [&]() -> std::string
        {
            if (editor.aiMethodSelection == 0)
                return !nativeModelId.empty() ? nativeModelId : nativeReadiness.model;
            if (editor.aiMethodSelection == 1)
            {
                if (!epochengine::ai::is_model_use_confirmed()) return {};
                return !editor.aiPendingModelSelection.empty()
                    ? editor.aiPendingModelSelection : epochengine::ai::active_model_name();
            }
            if (editor.aiMethodSelection == 2)
                return epochengine::ai::active_model_name();
            return {}; // Codex/MCP selections are governed by their own session.
        };
        const std::string aiChatPanelId = epochengine::format_text(
            "editor-ai-chat.{}",
            editor.projectId.empty() ? "no-project" : editor.projectId);
        // The model identity must come from the *selected transport*, never
        // from the global local-API manifest when a native/Codex model is active.
        // The exact ID is copyable from the plus menu even in narrow docks.
        const std::string currentModelIdentity = [&]() -> std::string
        {
            switch (editor.aiMethodSelection)
            {
                case 0:
                    return !nativeReadiness.model.empty() ? nativeReadiness.model
                        : !nativeModelId.empty() ? nativeModelId : "No native model selected";
                case 1:
                {
                    const auto name = epochengine::ai::active_model_name();
                    return !name.empty() ? name
                        : !editor.aiPendingModelSelection.empty()
                            ? editor.aiPendingModelSelection : "No local API model selected";
                }
                case 2:
                {
                    const auto direct = epochengine::ai::direct_runtime_status();
                    return !direct.model.empty()
                        ? std::filesystem::path{direct.model}.filename().string()
                        : "No GGUF model selected";
                }
                default:
                    return !nativeReadiness.model.empty() && nativeReadiness.selected
                        ? nativeReadiness.model : "No model selected";
            }
        }();
        // The + menu is the only UI for switching native tool approval modes.
        // It changes host policy, never grants blanket access and never
        // bypasses the per-call approval prompt or sandbox safety restrictions.
        // One direct EngCoder-style context menu. No nested category state,
        // editor theme setting, or duplicated status controls inside +.
        const std::array chatComposerActions{
            gui::ConsoleWindowActionSpec{.label="Add files"},
            gui::ConsoleWindowActionSpec{.label="Add folder"},
            gui::ConsoleWindowActionSpec{.label="Paste image, files or text"},
            gui::ConsoleWindowActionSpec{.label="Add images..."},
            gui::ConsoleWindowActionSpec{.label="Copy conversation"},
            gui::ConsoleWindowActionSpec{.label="Copy exact model ID"},
            gui::ConsoleWindowActionSpec{.label="Connection settings"},
            gui::ConsoleWindowActionSpec{.label="Model Manager"},
            gui::ConsoleWindowActionSpec{.label="Set project goal..."},
            gui::ConsoleWindowActionSpec{.label="Voice conversation (auto-send)"},
            gui::ConsoleWindowActionSpec{.label="Remove attached images"}};
        // Buttons describe their actions, not an arbitrary raw model ID or a
        // backend candidate state. The active model identity and exact activity
        // stage remain available in Model Manager and request details.
        // Avoid truncated, misleading labels at small dock widths.
        const float visibleControlsWidth = (std::max)(0.0f,
            chat_render_size.x - 16.0f);
        constexpr float thinkControlWidth = 124.0f;
        constexpr float activityControlWidth = 88.0f;
        constexpr float fixedFooterWidth =
            thinkControlWidth + activityControlWidth + 16.0f;
        const float modelControlWidth = (std::clamp)(
            visibleControlsWidth - fixedFooterWidth, 78.0f, 100.0f);
        // ConsoleWindowActionSpec::label is a string_view: give the composed
        // text a lifetime spanning console_window(), not a temporary string.
        // A transport placeholder is not a model identity. In particular,
        // never persist a thinking preference under "No native model selected".
        const bool hasThinkingModel = [&]() -> bool
        {
            switch (editor.aiMethodSelection)
            {
            case 0:
                return nativeReadiness.selected && !nativeReadiness.model.empty();
            case 1:
                return epochengine::ai::is_model_use_confirmed()
                    && currentModelIdentity != "No local API model selected";
            case 2:
                return !epochengine::ai::direct_runtime_status().model.empty();
            default:
                return false;
            }
        }();
        const std::string thinkControlLabel = hasThinkingModel
                ? "Think: " + epochengine::ai::model_thinking_mode(currentModelIdentity)
                : "Think: --";
        const std::array compactChatActions{
            gui::ConsoleWindowActionSpec{
                .label = thinkControlLabel,
                .width = thinkControlWidth,
                .enabled = hasThinkingModel && !chatWorking,
                .activate_on_press = true},
            gui::ConsoleWindowActionSpec{
                .label = "Models",
                .width = modelControlWidth, .activate_on_press = true},
            gui::ConsoleWindowActionSpec{
                .label = "Details",
                .width = activityControlWidth, .activate_on_press = true,
                .tone = chat.showActivities
                    ? gui::SemanticTone::info : gui::SemanticTone::neutral}
        };
        // Resident selection is immediate and separate from installing,
        // loading or tuning models. List every ACTUALLY resident native model.
        const auto availableNativeModels = epochengine::ai::native_runtime::snapshot(true);
        std::vector<std::string> residentPickerIds{};
        std::vector<std::string> residentPickerLabels{};
        std::vector<int> residentPickerMethods{};
        for (const auto& model : availableNativeModels.models)
        {
            if (!model.active) continue;
            residentPickerIds.push_back(model.id);
            residentPickerMethods.push_back(0);
            residentPickerLabels.push_back("Native · " + model.id
                + (nativeReadiness.model == model.id && editor.aiMethodSelection == 0
                    ? " (selected)" : " (loaded)"));
        }
        // snapshot(true) returns provider-observed native inventory; an
        // external API's installed/catalogue models are not necessarily loaded.
        const auto addProvider = [&](int method, std::string label, std::string id = {})
        {
            residentPickerMethods.push_back(method);
            residentPickerIds.push_back(std::move(id));
            residentPickerLabels.push_back(std::move(label));
        };
        addProvider(1, "Local API · saved connection");
        const auto directRuntime = epochengine::ai::direct_runtime_status();
        addProvider(2, directRuntime.ready() ? "Direct GGUF · " + directRuntime.model
            : "Direct GGUF · setup required");
        std::vector<gui::ConsoleWindowActionSpec> residentPickerActions{};
        residentPickerActions.reserve(residentPickerLabels.size() + 1u);
        for (std::size_t i = 0; i < residentPickerLabels.size(); ++i)
            residentPickerActions.push_back(gui::ConsoleWindowActionSpec{
                .label = residentPickerLabels[i],
                .enabled = !chatWorking && chat.queuedInput.empty()
                    && !availableNativeModels.busy
                    && (residentPickerMethods[i] != 2 || directRuntime.ready()),
                .tone = editor.aiMethodSelection == residentPickerMethods[i]
                    && (residentPickerMethods[i] == 0
                        ? residentPickerIds[i] == nativeReadiness.model
#if EPOCH_EDITION_PUBLIC
                        : true)
#endif
                    ? gui::SemanticTone::info : gui::SemanticTone::neutral});
        residentPickerActions.push_back(gui::ConsoleWindowActionSpec{
            .label = "Model settings..."});
        // Show the CURRENT observed state above the transcript, not a
        // generation counter or a duplicate source-console window.  A queued
        // prompt is never described as active inference.
        std::string currentAction{};
        if (!chat.queuedInput.empty())
        {
            const auto queuedFor = chat.queuedInputAt.time_since_epoch().count() == 0
                ? 0LL : std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - chat.queuedInputAt).count();
            currentAction = "Queued (not sent) · " + std::to_string(queuedFor / 60)
                + "m " + std::to_string(queuedFor % 60) + "s";
            if (!chat.queuedBlockReported.empty())
                currentAction += " · " + chat.queuedBlockReported;
            if (queuedFor >= 180)
                currentAction += " · Delayed: check Model Manager or Stop; draft retained";
        }
        else if (chat.pending || chat.worker.joinable())
        {
            const auto seconds = chat.elapsed_milliseconds() / 1000u;
            // The default view uses observed lifecycle states in plain language.
            // Full transport phases, model IDs and byte counters stay in Details.
            const auto stage = chat.pending ? chat.pending->stage.load(std::memory_order_acquire)
                : ai::ModelRequestStage::completed;
            switch (stage)
            {
            case ai::ModelRequestStage::queued: currentAction = "Waiting to start"; break;
            case ai::ModelRequestStage::sending: currentAction = "Sending your request"; break;
            case ai::ModelRequestStage::request_sent:
            case ai::ModelRequestStage::awaiting_response: currentAction = "Waiting for the model response"; break;
            case ai::ModelRequestStage::receiving:
                currentAction = engineWorkActive ? "Preparing changes" : "Responding"; break;
            case ai::ModelRequestStage::completed: currentAction = "Checking the response"; break;
            case ai::ModelRequestStage::cancelled: currentAction = "Stopping"; break;
            case ai::ModelRequestStage::failed: currentAction = "Request failed"; break;
            case ai::ModelRequestStage::retirement_failed: currentAction = "Restart required after request failure"; break;
            }
            currentAction += " · " + std::to_string(seconds / 60u)
                + "m " + std::to_string(seconds % 60u) + "s";
            // The HTTP transport reports bytes/events, not token counts.
            // t/s remains explicitly unavailable until measured token usage
            // is reported; never relabel bytes/sec as tokens/sec.
            currentAction += " · t/s pending";
            if (chat.pending)
            {
                std::scoped_lock lock(chat.pending->progressMutex);
                const auto& progress = chat.pending->progress;
                if (progress.tps_known)
                    currentAction.replace(currentAction.find("t/s pending"),
                        std::string{"t/s pending"}.size(),
                        epochengine::format_text("{:.1f} t/s",
                            progress.measured_tps));
                // Prompt size and detailed byte/event counters remain in
                // technical Activity evidence, not the visible status line.
            }
        }
        else if (editor.aiMethodSelection == 0 && nativeReadiness.busy)
            currentAction = nativeReadiness.phase.empty()
                ? "Loading model..." : nativeReadiness.phase;
        else if (methodReady && editor.aiMethodSelection >= 0 && editor.aiMethodSelection <= 2)
            currentAction = "Ready to chat";
        bool codexImageRoute = false;
        // Visible before Send, including an idle/unsent draft. The model
        // picker is an explicit override; conditional review never replaces it.
            currentAction += "\nSend to: " + currentModelIdentity;
        if (nativeReadiness.busy && !nativeReadiness.loaded)
            currentAction += "\nModels are loading. Avoid closing until ready; closing interrupts this load.";
        if (!chat.draftReferenceImages.empty() && editor.aiMethodSelection == 0
            && !codexImageRoute && !nativeReadiness.image_input_supported)
            currentAction += nativeReadiness.image_input_known
                ? "\nImages: this loaded runtime has no vision input. Choose a vision connection in Models."
                : "\nImages: native vision input is unconfirmed. Choose a confirmed vision connection in Models.";
        else if (!chat.draftReferenceImages.empty() && editor.aiMethodSelection == 0 && !codexImageRoute)
            currentAction += "\nImages: ready on the selected model · "
                + std::to_string(chat.draftReferenceImages.size()) + " attached";
        // Keep the last observed generation rate visible through compilation.
        // A completed request's rate is not current compiler throughput.
        if (!chat.pending && !currentAction.empty())
        {
            for (auto it = chat.lineRunDetails.rbegin(); it != chat.lineRunDetails.rend(); ++it)
            {
                if (it->tps.empty()) continue;
                currentAction += "\nLast generation · " + it->tps.substr(0u, it->tps.find(" ("));
                break;
            }
        }
        std::vector<std::string_view> referenceLabels;
        for (const auto& image : chat.draftReferenceImages) referenceLabels.push_back(image.name);
        std::vector<gui::ChatImageAttachment> referencePreviews;
        for (const auto& image : chat.draftReferenceImages)
        {
            auto preview = chat.referencePreviews.contains(image.client_id)
                ? chat.referencePreviews.at(image.client_id) : gui::ChatImageAttachment{};
            preview.name = image.name;
            referencePreviews.push_back(std::move(preview));
        }
        gui::ConsoleWindowOptions opts{
            .title = aiChatModeTitle,
            .position = chat_render_pos,
            .size = chat_render_size,
            .lines = displayedChatLines,
            .line_roles = chatLineRoles,
            .line_run_details = std::span<const gui::ChatMessageRunInfo>{chat.lineRunDetails},
            .retryable_lines = chatRetryableLines,
            .retry_enabled = chatCanQueue && !editor.aiIntentClarificationPending,
            .message_bubbles = true,
            .conversation_style = true,
            .expand_activity = chat.showActivities,
            .incremental_line_measurements = false,
            .max_visible_lines = 180u,
            .log_id = aiChatPanelId,
            .follow_tail = editor.aiChatFollowTail,
            .scroll_to_end_generation =
                editor.aiChatScrollToEndGeneration,
            .input = &chat.input,
            .max_input_chars = 65536,
            .multiline_input = true,
            .chat_composer = true,
            .composer_microphone = true,
            .composer_microphone_active = voiceState.owner == voiceOwner,
            .composer_pause = engineWorkActive || selfCodingPaused,
            .composer_paused = selfCodingPaused,
            .composer_attachment_labels = referenceLabels,
            .composer_paste = [&]() { return paste_ai_chat_clipboard(chat); },
            // Dock tab already names this pane: avoid a second full-width header.
            // C++23 designated initializers must follow ConsoleWindowOptions declaration order (MSVC C7560).
            .show_title_bar = false,
            .composer_menu_actions = chatComposerActions,
            .model_picker_actions = residentPickerActions,
            .model_picker_open = editor.aiChatModelPickerOpen,
            .composer_placeholder = "What should we create?",
            .input_keyboard_policy = gui::TextInputKeyboardPolicy::focused,
            .show_send_button = true,
            .send_button_enabled = chatCanQueue || chatStopAvailable || selfCodingPaused
                || (engineWorkActive && !chat.input.empty())
                || epochengine::editor::ai_chat_goal::parse(chat.input).action
                    != epochengine::editor::ai_chat_goal::Action::none,
            .send_button_width = 30.0f,
            .send_button_label = (!chat.input.empty() || !chat.draftReferenceFiles.empty()) ? ">"
                : chatStopAvailable ? "X"
                : !chat.queuedInput.empty() ? "..." : ">",
            // A project goal is created, edited and controlled by messages in
            // the normal composer. Never reserve a separate goal-setup strip.
            .task_label = {},
            .task_value = currentAction,
            .task_wrap = true,
            .task_edit_buffer = nullptr,
            .task_editing = false,
            .task_actions = {},
            .activity_text = {}, // No separate status panel: one compact footer row.
            .message_actions = messageActions,
            .footer_actions = compactChatActions,
            .composer_image_previews = referencePreviews,
            .composer_files = chat.draftReferenceFiles,
        };

        gui::ConsoleWindowResult r = gui::console_window(opts);
        for (const auto& file : r.dropped_files)
            (void)append_ai_chat_attachment(chat, std::filesystem::path{std::u8string{file.begin(), file.end()}});
        if (r.model_picker_dismissed)
            editor.aiChatModelPickerOpen = false;
        const auto stopCurrentWork = [&]()
            {
                epochengine::ai::voice_runtime::stop(voiceOwner);
                editor.aiVoiceSession.cancel();
                editor.aiVoiceConversation = false;
                editor.aiProjectScriptRepairQueued = false;
                if (editor.aiProjectScriptRunAfterBuild)
                    (void)cancel_script_build(editor);
                // Cancellation is request-level: do not unload weights.
                // Cancel both live inference and not-yet-dispatched turns.
                const bool hadQueuedRequest = !chat.queuedInput.empty();
                if (hadQueuedRequest)
                {
                    if (chat.input.empty()) chat.input = chat.queuedDraftText;
                    chat.restore_reference_images(chat.queuedReferenceImages);
                    chat.queuedInput.clear();
                    chat.queuedInputAt = {};
                    chat.queuedBlockReported.clear();
                    editor.aiChatQueuedMethodSelection = -1;
                    editor.aiChatQueuedModel.clear();
                }
                bool hadDeferredRequest = false;
                if (editor.aiDeferredRequestKind == AiDeferredRequestKind::Chat)
                {
                    hadDeferredRequest = true;
                    if (chat.input.empty()) chat.input = editor.aiDeferredDisplay;
                    editor.aiDeferredRequestKind = AiDeferredRequestKind::None;
                    editor.aiDeferredDispatchQueued = false;
                    editor.aiDeferredPrompt.clear();
                    editor.aiDeferredDisplay.clear();
                    editor.aiDeferredModelLease.reset();
                }
                // Interrupt the active transport immediately instead of waiting
                // for the session controller to reach its next host stage.
                const bool hadPendingModelRequest = chat.cancel_pending();
#if EPOCH_EDITION_PUBLIC
                constexpr bool hadPendingCodexTurn = false;
#endif
                if (hadPendingModelRequest || hadPendingCodexTurn)
                    chat.append_status("STOP requested; waiting for the active transport to retire. Model remains loaded.");
                else if (hadQueuedRequest || hadDeferredRequest)
                    chat.append_status("STOP: queued request cancelled before dispatch; draft preserved. Model remains loaded.");
            };
        if (r.model_picker_action_index)
        {
            editor.aiChatModelPickerOpen = false;
            const std::size_t index = *r.model_picker_action_index;
            if (index < residentPickerIds.size() && !chatWorking
                && chat.queuedInput.empty())
            {
                // No implicit loading, and do not switch the model mid-turn.
                ++editor.aiChatUserModelSelectionRevision;
                const int method = residentPickerMethods[index];
                if (method == 0)
                {
                    const bool selected = epochengine::ai::native_runtime::use_loaded(
                        residentPickerIds[index]);
                    if (!selected || !select_native_chat_transport(editor))
                        chat.append_status("Could not select that resident model; it remains loaded.");
                }
                else
                {
#if EPOCH_EDITION_PUBLIC
                {
#endif
                        (void)epochengine::ai::native_runtime::select_for_epoch_requests(false);
                        editor.aiMethodSelection = method;
                        editor.aiAutomaticModelSelection = method == 1;
                        if (method == 1)
                        {
                            epochengine::ai::select_openai_compatible_runtime();
                            editor.aiModelInventoryScanned = false;
                            editor.aiPendingModelSelection.clear();
                            request_ai_model_inventory_refresh(editor);
                        }
                        else if (method == 2)
                            (void)epochengine::ai::select_direct_runtime(
                                directRuntime.executable, directRuntime.model);
                    }
                }
            }
            else if (index == residentPickerIds.size())
            {
                editor.showAiModelManagerModal = true;
                editor.aiEndpointDraft = epochengine::ai::active_local_api_endpoint().base_url;
                editor.aiModelManagerTab = editor.aiMethodSelection == 1 ? 1
                    : editor.aiMethodSelection == 2 ? 2 : 0;
            }
        }
        if (r.footer_action_index)
        {
            const auto index = *r.footer_action_index;
            if (index == 0u)
            {
                if (hasThinkingModel && !chatWorking)
                    (void)epochengine::ai::cycle_model_thinking_mode(currentModelIdentity);
            }
            else if (index == 1u)
            {
                editor.aiChatModelPickerOpen = !editor.aiChatModelPickerOpen;
            }
            else if (index == 2u)
            {
                chat.showActivities = !chat.showActivities;
                if (editor.aiChatFollowTail)
                    ++editor.aiChatScrollToEndGeneration;
            }
        }
        if (r.composer_voice_action_index)
        {
            activate_ai_voice_control(editor, chat, *r.composer_voice_action_index);
        }
        if (r.attachment_remove_index && *r.attachment_remove_index < chat.draftReferenceImages.size())
            chat.draftReferenceImages.erase(chat.draftReferenceImages.begin() + *r.attachment_remove_index);
        if (r.file_remove_index && *r.file_remove_index < chat.draftReferenceFiles.size())
            chat.draftReferenceFiles.erase(chat.draftReferenceFiles.begin() + *r.file_remove_index);
        if (r.composer_menu_action_index)
        {
            // The reusable composer returns the flat action index unchanged.
            std::size_t action = *r.composer_menu_action_index;
#if EPOCH_EDITION_PUBLIC
            constexpr std::size_t visibleActions[]{0u,1u,2u,3u,4u,5u,6u,7u,11u,12u,14u};
            if (action >= std::size(visibleActions)) return;
            action = visibleActions[action];
#endif
            switch (action)
            {
            case 0u: add_ai_chat_file_to_input(editor, chat, false); break;
            case 1u: add_ai_chat_file_to_input(editor, chat, true); break;
            case 2u:
                if (!paste_ai_chat_clipboard(chat))
                {
                    const auto clipboard = gui::clipboard_text();
                    if (clipboard.empty()) chat.append_status("Clipboard has no supported image, files or text.");
                    else if (chat.input.size() + clipboard.size() <= 65536u)
                        (void)gui::paste_text_in_edit_box(chat.input, clipboard, 65536u);
                    else chat.append_status("Clipboard text exceeds the input limit.");
                }
                break;
            case 3u: add_ai_chat_file_to_input(editor, chat, false, true); break;
            case 14u: chat.draftReferenceImages.clear(); chat.draftReferenceFiles.clear(); break;
            case 4u:
            {
                if (!gui::set_clipboard_text(chat.copyable_transcript()))
                    chat.append_status("Could not copy conversation.");
                break;
            }
            case 5u:
                if (!gui::set_clipboard_text(currentModelIdentity))
                    chat.append_status("Could not copy the selected model ID.");
                break;
            case 6u:
                editor.showAiModelManagerModal = true;
                editor.aiModelManagerSection = 2;
                editor.aiEndpointDraft = epochengine::ai::active_local_api_endpoint().base_url;
                break;
            case 7u:
                editor.showAiModelManagerModal = true;
                editor.aiEndpointDraft = epochengine::ai::active_local_api_endpoint().base_url;
                editor.aiModelManagerTab = editor.aiMethodSelection == 1 ? 1
                    : editor.aiMethodSelection == 2 ? 2 : 0;
                break;
            case 12u:
                activate_ai_voice_control(editor, chat, 1u);
                break;
            case 11u:
                // A discoverable chat shortcut, not a separate goal form.
                // It only edits the draft; the user must explicitly send it.
                if (!epochengine::editor::ai_chat_goal::starts_with(
                        epochengine::editor::ai_chat_goal::trim(chat.input), "project goal:"))
                    chat.input = "Project goal: " + chat.input;
                break;
            default: break;
            }
        }
        editor.aiChatFollowTail = r.log_at_end;
        if (r.copy_line_index
            && *r.copy_line_index < chat.lines.size())
        {
            (void)gui::set_clipboard_text(
                chat.copyable_line(*r.copy_line_index));
        }
        if (r.edit_line_index
            && *r.edit_line_index < chat.lines.size())
        {
            const std::string& original = chat.lines[*r.edit_line_index];
            if (original.starts_with("User: "))
            {
                if (chat.input.empty())
                    chat.input = original.substr(6u); // Editable new prompt, retains history.
                else
                    chat.append_status("Composer has an unsent draft; copy it before replacing it with an earlier prompt.");
            }
        }

        if (r.retry_line_index && chatCanQueue && !editor.aiIntentClarificationPending)
        {
            if (const auto text = chat.retry_input(*r.retry_line_index))
            {
                if (chat.queue_input(*text, true))
                {
                    editor.aiChatQueuedMethodSelection = editor.aiMethodSelection;
                    editor.aiChatQueuedModel = queuedModelIdentity();
                    editor.aiChatQueuedUserModelSelectionRevision =
                        editor.aiChatUserModelSelectionRevision;
                    chat.append_status(methodReady ? "Message queued for dispatch."
                        : "Message queued; waiting for the selected model to load.");
                    editor.aiChatFollowTail = true;
                    ++editor.aiChatScrollToEndGeneration;
                }
            }
        }
        // One routing path for keyboard Send, text, and finalized voice.
        // Important: while working, typed text MUST NOT invoke Stop merely
        // because the send icon previously served as Stop for an empty draft.
#if EPOCH_EDITION_PUBLIC
        const auto submitLiveInstruction = [&](std::string_view instruction, bool) -> bool
        {
            const auto clean = trim_ai_command(instruction);
            if (clean == "stop" || clean == "/stop") { stopCurrentWork(); return true; }
            if (chat.pending || chat.worker.joinable())
            {
                if (chat.queuedInput.empty() && chat.queue_input(std::string{clean}, true))
                {
                    editor.aiChatQueuedMethodSelection = editor.aiMethodSelection;
                    editor.aiChatQueuedModel = queuedModelIdentity();
                    editor.aiChatQueuedUserModelSelectionRevision = editor.aiChatUserModelSelectionRevision;
                    return true;
                }
                chat.append_status("Wait for the current response, or stop it before sending another request.");
                return false;
            }
            return submit_ai_chat_input(std::string{clean});
        };
#endif
        for (const auto& speech : spokenAiInstructions)
        {
            if (!submitLiveInstruction(speech, true)
                && chat.input.empty() && chat.queuedInput.empty())
                chat.input = speech;
        }
        // Host-owned goal instructions must work even while models load or
        // another model is responding; they never enter inference as a prompt.
        if ((r.input.submitted || r.send_clicked) && !chat.input.empty()
            && epochengine::editor::ai_chat_goal::parse(chat.input).action
                != epochengine::editor::ai_chat_goal::Action::none)
        {
            if (submit_ai_chat_input(chat.input))
            {
                chat.input.clear();
                editor.aiChatFollowTail = true;
                ++editor.aiChatScrollToEndGeneration;
            }
        }
        else if ((r.input.submitted || r.send_clicked) && !chat.input.empty()
            && (chatWorking || selfCodingPaused
                ))
        {
            if (submitLiveInstruction(chat.input, false))
            {
                chat.input.clear();
                editor.aiChatFollowTail = true;
                ++editor.aiChatScrollToEndGeneration;
            }
        }
        else if (r.send_clicked && chatStopAvailable)
        {
            stopCurrentWork();
        }
        else if ((r.input.submitted || r.send_clicked) && chatCanQueue)
        {
            const std::string text = chat.input.empty() && !chat.draftReferenceImages.empty() && chat.draftReferenceFiles.empty()
                ? "Describe the attached image(s)." : chat.input;
            if (!chat.draftReferenceImages.empty() && editor.aiMethodSelection == 0
                && !codexImageRoute && !nativeReadiness.image_input_supported)
            {
                chat.append_status(nativeReadiness.image_input_known
                    ? "This loaded native runtime reports no vision input. Your prompt and images are retained. Select a vision-capable Local API connection, or configure a matching native vision loader before sending."
                    : "Native image input is not confirmed. Your prompt and images are retained. Select a confirmed vision-capable connection before sending.");
                return;
            }
            bool selectedCodexAcceptsImages = false;
            if (!chat.draftReferenceImages.empty() && (editor.aiMethodSelection >= 2 || codexImageRoute)
                && !selectedCodexAcceptsImages)
            {
#if EPOCH_EDITION_PUBLIC
                chat.append_status("Images are attached. Select a confirmed image-capable Native or Local API model before sending. Your prompt and images are retained.");
#endif
                return;
            }
            std::size_t imageBytes{};
            for (const auto& image : chat.draftReferenceImages) imageBytes += image.bytes.size();
            if (chat.draftReferenceImages.size() > 4u || imageBytes > 4u * 1024u * 1024u)
            {
                chat.append_status("Your restored attachments are retained. Remove images until at most four totaling 4 MiB remain before sending.");
                return;
            }
            const bool retryable = !editor.aiIntentClarificationPending
                && !ai_confirmation(text).has_value();
            if (chat.queue_input(text, retryable))
            {
                editor.aiChatQueuedMethodSelection = editor.aiMethodSelection;
                editor.aiChatQueuedModel = queuedModelIdentity();
                editor.aiChatQueuedUserModelSelectionRevision =
                    editor.aiChatUserModelSelectionRevision;
                chat.append_status(methodReady ? "Message queued for dispatch."
                    : "Message queued; waiting for the selected model to load.");
                chat.input.clear();
                editor.aiChatFollowTail = true;
                ++editor.aiChatScrollToEndGeneration;
            }
            else if (!chat.draftReferenceFiles.empty())
                chat.append_status("The request was not queued. Files remain attached; keep the message and file contents within 64 KiB.");
        }

        std::optional<std::size_t> messageActionIndex = r.message_action_index;
        if (!messageActionIndex && editor.aiQueuedMessageActionIndex)
        {
            messageActionIndex = editor.aiQueuedMessageActionIndex;
            editor.aiQueuedMessageActionIndex.reset();
        }
        if (messageActionIndex)
        {
            if (editor.aiAuthoringApplyPending)
            {
                editor.aiAuthoringApplyCancelled = true;
                (void)editor_task_scheduler(editor).cancel(editor.aiAuthoringApplyCancellation);
                chat.append_status("Authoring cancellation requested. The live scene was not changed; the private worker result will be discarded.");
            }
            else if (editor.aiIntentClarificationPending
                && editor.aiIntentClarificationResumeIntent
                    != AiChatIntent::Ambiguous)
            {
                editor.aiIntentClarificationPending = false;
                editor.aiIntentClarificationResumeIntent = AiChatIntent::Ambiguous;
                editor.aiIntentClarificationRequest.clear();
                editor.aiIntentClarificationQuestion.clear();
                chat.append_status(
                    "Blocking question cancelled; no staged project action was applied.");
            }
            else if (editor.aiIntentClarificationPending)
            {
                const std::string request =
                    std::exchange(editor.aiIntentClarificationRequest, {});
                editor.aiIntentClarificationPending = false;
                editor.aiIntentClarificationResumeIntent = AiChatIntent::Ambiguous;
                const AiChatIntent route = *messageActionIndex == 0u
                    ? AiChatIntent::EngineSource
                    : *messageActionIndex == 1u
                        ? AiChatIntent::ProjectAuthoring
                        : *messageActionIndex == 2u
                            ? AiChatIntent::ProjectTooling
                            : AiChatIntent::ProjectChat;
                chat.append_status(*messageActionIndex == 0u
                    ? "Routing clarification: Engine Source."
                    : *messageActionIndex == 1u
                        ? "Routing clarification: Project Change."
                        : *messageActionIndex == 2u
                            ? "Routing clarification: Project Tool."
                            : "Routing clarification: Just Answer.");
                (void)dispatch_ai_intent(route, request);
            }
            else if (modelConsentPending && *messageActionIndex == 0u)
            {
                if (confirm_pending_ai_model(editor))
                {
                    editor.aiDeferredDispatchQueued = true;
                }
            }
            else if (modelConsentPending)
            {
                const bool declinedGoalMilestone =
                    editor.aiDeferredRequestKind
                        == AiDeferredRequestKind::Authoring
                    && editor.aiAuthoringPlanForGoal
                    && editor.aiGoalActive;
                const bool declinedToolRequest =
                    editor.aiDeferredRequestKind
                        == AiDeferredRequestKind::Tooling;
                editor.aiDeferredPrompt.clear();
                editor.aiDeferredDisplay.clear();
                editor.aiDeferredRequestKind =
                    AiDeferredRequestKind::None;
                editor.aiDeferredModelLease.reset();
                editor.aiDeferredDispatchQueued = false;
                editor.showAiModelConsentModal = false;
                if (declinedGoalMilestone)
                {
                    editor.aiGoalRunning = false;
                    editor.aiGoalPlanNextQueued = false;
                }
                const std::string declinedStatus = declinedGoalMilestone
                        ? "Local-model use was not approved; the goal is paused."
                    : declinedToolRequest
                        ? "Local-model use was not approved; the waiting tool request was discarded."
                        : "Local-model use was not approved; the waiting request was discarded.";
                editor.aiAuthoringStatus = declinedStatus;
                if (declinedToolRequest)
                    editor.aiToolStatus = declinedStatus;
                editor.aiModelConsentStatus =
                    "Local-model use was not approved.";
                chat.append_status(declinedStatus);
                push_editor_log(editor, "[ai] " + declinedStatus);
            }
            else if (toolTestPending)
            {
                (void)editor_task_scheduler(editor).cancel(
                    editor.aiToolTestCancellation);
                editor.aiToolStatus =
                    "Cancellation requested for the approved project.test task.";
                chat.append_status(editor.aiToolStatus);
                push_editor_log(
                    editor,
                    "[ai-tool] " + editor.aiToolStatus);
            }
            else if (toolPlanAwaitingApproval
                && *messageActionIndex == 0u)
            {
                (void)dispatch_ai_tool_plan();
                chat.append_status(editor.aiToolStatus);
            }
            else if (toolPlanAwaitingApproval)
            {
                editor.aiToolPlan = {};
                editor.aiToolPlanApplied = false;
                editor.aiToolStatus =
                    "The staged active-project tool plan was discarded without execution.";
                chat.append_status(editor.aiToolStatus);
            }
            else if (!editor.aiProjectScriptDraft.empty())
            {
                if (*messageActionIndex <= 1u)
                {
                    const bool accepted = apply_ai_project_script(editor);
                    if (accepted && *messageActionIndex == 1u)
                    {
                        editor.aiProjectScriptRunAfterBuild = true;
                        editor.aiProjectScriptRunRoot = editor.projectRoot;
                        editor.aiProjectScriptRunPath = editor.scriptEditorPath;
                        editor.aiProjectScriptRunSource = editor.scriptEditorText;
                        editor.aiProjectScriptRunObjective = editor.aiAuthoringRequest;
                        start_script_build(editor, "Approved Project Assistant script");
                        editor.aiProjectScriptBuildGeneration = editor.scriptBuildGeneration;
                        if (!editor.scriptBuildPending) editor.aiProjectScriptRunAfterBuild = false;
                        editor.aiAuthoringStatus = editor.scriptBuildStatus;
                    }
                }
                else
                {
                    editor.aiProjectScriptDraft.clear();
                    editor.aiAuthoringStatus = "Project script draft discarded without saving or execution.";
                }
                chat.append_status(editor.aiAuthoringStatus);
            }
            else if (planAwaitingApproval && *messageActionIndex == 0u)
            {
                const bool queued = queue_ai_authoring_apply(editor);
                chat.append_status(editor.aiAuthoringStatus);
                if (!queued)
                    editor.surfaceSettleFrames = (std::max)(editor.surfaceSettleFrames, 1);
            }
            else if (planAwaitingApproval)
            {
                const bool goalPlan = editor.aiAuthoringPlanForGoal;
                editor.aiAuthoringPlan = {};
                editor.aiAuthoringPlanApplied = false;
                if (goalPlan && editor.aiGoalActive)
                {
                    editor.aiGoalRunning = false;
                    editor.aiGoalPlanNextQueued = false;
                }
                editor.aiAuthoringStatus = goalPlan && editor.aiGoalActive
                    ? "The staged milestone was discarded; the goal is paused."
                    : "The staged authoring plan was discarded.";
                chat.append_status(editor.aiAuthoringStatus);
            }
            else if (modelChoiceButtonsVisible)
            {
                if (*messageActionIndex == 0u)
                {
                    editor.showAiModelManagerModal = true;
                    editor.aiModelManagerTab = 0;
                    editor.aiModelManagerSection = 0;
                }
                else
                {
                    const bool accepted = epochengine::ai::native_runtime::load_saved_set();
                    if (accepted)
                    {
                        (void)select_native_chat_transport(editor);
                        chat.append_status("Restoring the saved model set. "
                            "Loaded models are reused; missing models load sequentially.");
                    }
                    else
                    {
                        chat.append_status("Could not start saved-model loading: "
                            + epochengine::ai::native_runtime::snapshot(false).phase);
                        editor.showAiModelManagerModal = true;
                        editor.aiModelManagerTab = 0;
                        editor.aiModelManagerSection = 0;
                    }
                }
            }
        }
        };
        render_ai_chat_tool_pane(EditorToolDockRegion::Left);
        render_ai_chat_tool_pane(EditorToolDockRegion::Right);
        render_ai_chat_tool_pane(EditorToolDockRegion::BottomLeft);
        render_ai_chat_tool_pane(EditorToolDockRegion::BottomRight);
        if(!voiceChatVisibleThisFrame){
            epochengine::ai::voice_runtime::stop(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&editor)));
            editor.aiVoiceSession.cancel();
            editor.aiVoiceConversation=false;
        }

        if (nativeDockProjection.active)
        {
            reset_pane_title_drag();
            render_native_pane_dock_projection(nativeDockProjection);
        }
        else
        {
            core::publish_routed_panel_tab_drop_target(
                {}, core::RoutedPanelDockTarget::none, 0xffffffffu);
            update_pane_dock_drag();
        }

        // The application menu has its own popup ownership, but must never
        // release exclusive modal capture while a confirmation dialog is open.
