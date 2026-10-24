// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        auto request_ai_authoring_plan = [&](bool goalMilestone = false) -> bool
        {
            if (editor.aiProjectScriptRunAfterBuild)
            {
                chat.append_status("The reviewed script is still building. Stop or wait for its result before requesting another draft.");
                return false;
            }
            if (editor.aiAuthoringApplyPending)
            {
                chat.append_status("An approved authoring job is still running. Cancel or wait for publication before requesting another plan.");
                return false;
            }
            chat.statusRequestKind = AiDeferredRequestKind::Authoring;
            if (goalMilestone
                && (!editor.aiGoalActive || !editor.aiGoalRunning))
            {
                editor.aiAuthoringStatus =
                    "The persistent goal is paused or unavailable.";
                return false;
            }
            const std::string_view request = goalMilestone
                ? std::string_view{editor.aiGoal}
                : std::string_view{editor.aiAuthoringRequest};
            if (request.empty())
            {
                editor.aiAuthoringStatus =
                    "Describe at least one scene or GUI change.";
                return false;
            }

            if (editor_ai_source_intent::requires_engine_source(request))
            {
                editor.aiGoalRunning = false;
                editor.aiGoalPlanNextQueued = false;
                editor.aiAuthoringStatus =
                    "Editor theme/settings changes require Assistant, not scene/GUI authoring.";
                return start_engine_source_iteration(request);
            }

            set_project_clarification_objective(editor, request);
            if (!editor.aiProjectScriptRepairQueued) editor.aiProjectScriptCompilerDiagnostics.clear();
            editor.aiProjectScriptRequested = ai_project_script_requested(request);
            editor.aiProjectScriptDraft.clear();
            if (editor.aiProjectScriptRequested && !prepare_ai_project_script(editor))
            {
                chat.append_status(editor.aiAuthoringStatus.empty()
                    ? "A writable project script destination could not be verified." : editor.aiAuthoringStatus);
                return false;
            }

            if (!main_surface_is_ai_authoring(editor.mainSurface))
                editor.aiAuthoringSurfaceRestoreQueued = true;

            editor.aiAuthoringPlanForGoal = goalMilestone;
            const std::string display = goalMilestone
                ? epochengine::format_text(
                    "Goal milestone {}: {}",
                    editor.aiGoalCompletedMilestones + 1u,
                    editor.aiGoal)
                : "Plan: " + editor.aiAuthoringRequest;
            chat.append_user_message(display, "/plan " + std::string{request});
            const std::string prompt = build_ai_authoring_prompt(
                editor,
                request,
                goalMilestone);
            // Tool and authoring requests use the same background model
            // preparation path as chat and Assistant. This GUI callback
            // only checks prior consent; no provider is initialized here.
            if (!epochengine::ai::is_model_use_confirmed())
            {
                (void)stage_ai_model_consent(
                    editor,
                    AiDeferredRequestKind::Authoring,
                    prompt,
                    display);
                editor.aiGoalPlanNextQueued = false;
                editor.aiAuthoringStatus =
                    "Check the selected model and endpoint in Model Manager before sending this plan.";
                chat.append_status(editor.aiAuthoringStatus);
                return true;
            }

            editor.aiAuthoringRequestedGeneration =
                chat.completionGeneration;
            const bool submitted = submit_ai_request(
                prompt,
                display,
                AiDeferredRequestKind::Authoring);
            editor.aiAuthoringAwaitingReply = submitted;
            editor.aiAuthoringPlan = {};
            editor.aiAuthoringPlanApplied = false;
            editor.aiGoalPlanNextQueued = false;
            editor.aiAuthoringStatus = submitted
                ? goalMilestone
                    ? "Waiting for one bounded goal milestone from the selected local model."
                    : "Waiting for a bounded authoring plan from the selected local model."
                : "The local model is busy; wait for its current response.";
            return submitted;
        };

        auto request_ai_tool_plan = [&]() -> bool
        {
            chat.statusRequestKind = AiDeferredRequestKind::Tooling;
            std::string_view request{editor.aiToolRequest};
            while (!request.empty()
                && (request.front() == ' ' || request.front() == '\t'
                    || request.front() == '\r'
                    || request.front() == '\n'))
            {
                request.remove_prefix(1u);
            }
            while (!request.empty()
                && (request.back() == ' ' || request.back() == '\t'
                    || request.back() == '\r'
                    || request.back() == '\n'))
            {
                request.remove_suffix(1u);
            }
            if (request.empty())
            {
                editor.aiToolStatus =
                    "Usage: /tool <one active-project operation>.";
                chat.append_status(editor.aiToolStatus);
                return false;
            }

#if EPOCH_EDITION_PUBLIC
            if (editor_ai_source_intent::requires_engine_source(request))
                return start_engine_source_iteration(request);
            if (!project_ai_scope_allowed(editor.projectRoot))
            {
                chat.append_status("Project tools require a game or application project, not the engine source.");
                return false;
            }
#endif
            chat.append_user_message("Tool plan: " + std::string{request},
                "/tool " + std::string{request});
            std::string prompt = epochengine::ai::tool_plan_protocol_prompt();
            prompt +=
                "\n\nACTIVE_PROJECT_EVIDENCE_V1\nPROJECT_ID ";
            prompt += editor.projectId.empty()
                ? "(none)"
                : editor.projectId;
            prompt += "\nPROJECT_NAME ";
            prompt += editor.projectName.empty()
                ? "(none)"
                : editor.projectName;
            prompt += "\nPROJECT_KIND ";
            prompt += editor.projectKind.empty()
                ? "(none)"
                : editor.projectKind;
            prompt += "\nSCENE_REVISION ";
            prompt += std::to_string(editor.sceneDocumentRevision);
            prompt += "\nPROJECT_STATUS ";
            prompt += editor.projectStatus;
            prompt += "\nBUILD_STATUS ";
            prompt += editor.projectBuildStatus;
            prompt += "\nEND_ACTIVE_PROJECT_EVIDENCE_V1\n\nOBJECTIVE\n";
            prompt += request;
            prompt +=
                "\nEND_OBJECTIVE\nSelect the one smallest host operation that advances the objective. The operator must approve the exact call before Epoch executes it.";

            const std::string display =
                "Tool plan: " + std::string{request};
            // Tool and authoring requests use the same background model
            // preparation path as chat and Assistant. This GUI callback
            // only checks prior consent; no provider is initialized here.
            if (!epochengine::ai::is_model_use_confirmed())
            {
                (void)stage_ai_model_consent(
                    editor,
                    AiDeferredRequestKind::Tooling,
                    prompt,
                    display);
                editor.aiToolStatus =
                    "Check the selected model and endpoint in Model Manager before sending this tool plan.";
                chat.append_status(editor.aiToolStatus);
                return true;
            }

            editor.aiToolRequestedGeneration =
                chat.completionGeneration;
            const bool submitted = submit_ai_request(
                prompt,
                display,
                AiDeferredRequestKind::Tooling);
            editor.aiToolAwaitingReply = submitted;
            editor.aiToolPlan = {};
            editor.aiToolPlanApplied = false;
            editor.aiToolStatus = submitted
                ? "Waiting for one bounded active-project tool from the selected local model."
                : "The local model is busy; wait for its current response.";
            return submitted;
        };

        auto dispatch_deferred_ai_request = [&]() -> bool
        {
            std::scoped_lock selectionLock(aiModelSelectionMutex);
            if (editor.aiDeferredRequestKind == AiDeferredRequestKind::None
                || !epochengine::ai::is_model_use_confirmed()
                )
            {
                return false;
            }
            // A completed native turn can have no pending request while its
            // jthread is still joinable.  Consuming the deferred source prompt
            // during that retirement window causes submit_with_display() to
            // report busy and the old fallback to cancel the entire candidate.
            // Do not take or move the queued prompt until its worker has retired.
            if (chat.pending || chat.worker.joinable() || chat.requestRetirementFailed)
            {
                editor.aiAuthoringStatus = chat.requestRetirementFailed
                    ? "Previous model worker retirement is unconfirmed. The exact queued request is retained; restart Epoch after checking the worker."
                    : "Waiting for the previous model request to retire. The exact queued request and candidate are retained.";
                return false;
            }
            // Model Manager may unload/reload or reselect a native model
            // between the frame's preliminary readiness check and this lock.
            // Keep the request deferred until a resident model is ready.
            if (editor.aiMethodSelection == 0)
            {
                const auto selectedNative = epochengine::ai::native_runtime::snapshot(false);
                if (!selectedNative.selected || !selectedNative.loaded || selectedNative.busy)
                {
                    editor.aiAuthoringStatus =
                        "Waiting for the selected native model to finish loading. The exact queued request and candidate are retained.";
                    return false;
                }
            }

            {
                // Preserve a queued project-assistant prompt during comparison;
                // it must not start AI alongside an owned Assistant operation.
                if (!reserve_project_ai_work_at(editor,
                        platform::work_admission::now_milliseconds(), ai_heavy_work_coordinator()))
                    return false;
            }

            const AiDeferredRequestKind requestKind =
                editor.aiDeferredRequestKind;
            std::string prompt = std::move(editor.aiDeferredPrompt);
            std::string display = std::move(editor.aiDeferredDisplay);
            editor.aiDeferredRequestKind = AiDeferredRequestKind::None;
            editor.aiDeferredDispatchQueued = false;
            editor.showAiModelConsentModal = false;

#if EPOCH_EDITION_PUBLIC
            const auto workload = requestKind == AiDeferredRequestKind::Authoring
                || requestKind == AiDeferredRequestKind::Tooling
                    ? ai::InferenceWorkload::authoring : ai::InferenceWorkload::chat;
#endif
            const bool submitted = chat.submit_with_display(
                std::move(prompt),
                std::move(display),
                workload,
                requestKind,
                false);
            editor.aiDeferredModelLease.reset();
            if (requestKind == AiDeferredRequestKind::Tooling)
            {
                editor.aiToolRequestedGeneration =
                    chat.completionGeneration;
                editor.aiToolAwaitingReply = submitted;
                editor.aiToolPlan = {};
                editor.aiToolPlanApplied = false;
                editor.aiToolStatus = submitted
                    ? "Waiting for one bounded active-project tool from the approved local model."
                    : "The local model is busy; the waiting tool request was not sent.";
            }
            else if (requestKind == AiDeferredRequestKind::Authoring)
            {
                editor.aiAuthoringRequestedGeneration =
                    chat.completionGeneration;
                editor.aiAuthoringAwaitingReply = submitted;
                editor.aiAuthoringPlan = {};
                editor.aiAuthoringPlanApplied = false;
                editor.aiGoalPlanNextQueued = false;
                editor.aiAuthoringStatus = submitted
                    ? editor.aiAuthoringPlanForGoal
                        ? "Waiting for one bounded goal milestone from the approved local model."
                        : "Waiting for a bounded authoring plan from the approved local model."
                    : "The local model is busy; the waiting request was not sent.";
            }
            else if (requestKind == AiDeferredRequestKind::Chat)
            {
                editor.aiAuthoringStatus = submitted
                    ? "Project Assistant request started with bounded active-project context."
                    : "Project Assistant request could not start; its queued request was not applied.";
                if (submitted)
                {
                    chat.append_status(
                        "Project Assistant request started; waiting for the selected model endpoint.");
                    chat.input.clear();
                }
                else
                {
                    chat.append_status(editor.aiAuthoringStatus);
                }
            }
            if (!submitted && !ai_source_native_work_active(editor, chat))
                release_ai_work_lease(editor);
            return submitted;
        };

        if (poll_ai_authoring_apply(editor)) chat.append_status(editor.aiAuthoringStatus);

        const auto ai_command_argument = [&trim_ai_command](
            std::string_view input,
            std::string_view command) -> std::optional<std::string_view>
        {
            input = trim_ai_command(input);
            if (input == command)
                return std::string_view{};
            if (input.size() > command.size()
                && input.starts_with(command)
                && (input[command.size()] == ' '
                    || input[command.size()] == '\t'))
            {
                return trim_ai_command(input.substr(command.size() + 1u));
            }
            return std::nullopt;
        };
        auto invalidate_ai_goal_work = [&]()
        {
            const bool invalidatingGoalAuthoring =
                editor.aiAuthoringPlanForGoal;
            if (invalidatingGoalAuthoring
                && editor.aiAuthoringAwaitingReply
                && chat.pending
                && chat.pendingRequestKind
                    == AiDeferredRequestKind::Authoring)
            {
                const std::uint64_t pendingGeneration =
                    chat.completionGeneration + 1u;
                editor.aiAuthoringIgnoredThroughGeneration = (std::max)(
                    editor.aiAuthoringIgnoredThroughGeneration,
                    pendingGeneration);
                (void)chat.cancel_pending();
            }

            // A goal can also be waiting for model consent or shared-lane
            // admission without having started a worker yet. Discard that exact
            // deferred goal request when the goal changes; never touch ordinary
            // Project Assistant chat, tools, or Assistant requests here.
            if (invalidatingGoalAuthoring
                && editor.aiDeferredRequestKind
                    == AiDeferredRequestKind::Authoring)
            {
                editor.aiDeferredPrompt.clear();
                editor.aiDeferredDisplay.clear();
                editor.aiDeferredRequestKind =
                    AiDeferredRequestKind::None;
                editor.aiDeferredModelLease.reset();
                editor.aiDeferredDispatchQueued = false;
                editor.showAiModelConsentModal = false;
            }

            if (invalidatingGoalAuthoring)
            {
                editor.aiAuthoringPlan = {};
                editor.aiAuthoringPlanApplied = false;
            }
            editor.aiAuthoringAwaitingReply = false;
            editor.aiAuthoringPlanForGoal = false;
            editor.aiGoalPlanNextQueued = false;
        };
        const auto ai_lower_text = [](std::string_view value)
        {
            std::string lowered;
            lowered.reserve(value.size());
            for (const unsigned char ch : value)
            {
                lowered.push_back(static_cast<char>(
                    ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch));
            }
            return lowered;
        };
        const auto ai_has_any = [](const std::string& value,
            const std::span<const std::string_view> phrases) noexcept
        {
            return std::ranges::any_of(
                phrases,
                [&value](const std::string_view phrase)
                {
                    return value.find(phrase) != std::string::npos;
                });
        };
        const auto ai_has_any_word = [](const std::string& value,
            const std::span<const std::string_view> words) noexcept
        {
            const auto wordChar = [](const char ch) noexcept
            {
                return (ch >= 'a' && ch <= 'z')
                    || (ch >= '0' && ch <= '9') || ch == '_';
            };
            for (const std::string_view word : words)
            {
                std::size_t at = value.find(word);
                while (at != std::string::npos)
                {
                    const bool leftOk = at == 0u
                        || !wordChar(value[at - 1u]);
                    const std::size_t right = at + word.size();
                    const bool rightOk = right >= value.size()
                        || !wordChar(value[right]);
                    if (leftOk && rightOk)
                        return true;
                    at = value.find(word, at + 1u);
                }
            }
            return false;
        };
        const auto classify_ai_intent = [&](std::string_view request) noexcept
        {
            const std::string lower = ai_lower_text(trim_ai_command(request));
            if (lower.empty())
                return AiChatIntent::ProjectChat;

            constexpr std::array mutationTerms{
                std::string_view{"fix"}, std::string_view{"change"},
                std::string_view{"add"}, std::string_view{"implement"},
                std::string_view{"remove"}, std::string_view{"delete"},
                std::string_view{"rewrite"}, std::string_view{"refactor"},
                std::string_view{"update"}, std::string_view{"create"},
                std::string_view{"make"}, std::string_view{"move"},
                std::string_view{"rotate"}, std::string_view{"scale"},
                std::string_view{"set"}, std::string_view{"generate"},
                std::string_view{"build"}, std::string_view{"compile"},
                std::string_view{"run"}, std::string_view{"test"},
                std::string_view{"repair"}, std::string_view{"modify"},
                std::string_view{"replace"}, std::string_view{"rename"},
                std::string_view{"wire"}, std::string_view{"integrate"},
                std::string_view{"connect"}, std::string_view{"convert"},
                std::string_view{"migrate"}, std::string_view{"optimize"},
                std::string_view{"improve"}, std::string_view{"upgrade"},
                std::string_view{"advance"}, std::string_view{"enable"},
                std::string_view{"disable"}, std::string_view{"resolve"},
                std::string_view{"correct"}, std::string_view{"patch"},
                std::string_view{"finish"}, std::string_view{"code"},
                std::string_view{"write"}, std::string_view{"develop"}};
            constexpr std::array engineTerms{
                std::string_view{"engine source"}, std::string_view{"epochengine"},
                std::string_view{"epochgui"}, std::string_view{"self-coding"},
                std::string_view{"self coding"}, std::string_view{"source code"},
                std::string_view{"c++"}, std::string_view{".cpp"},
                std::string_view{".ixx"}, std::string_view{"cmake"},
                std::string_view{"msvc"}, std::string_view{"compiler error"},
                std::string_view{"compile error"}, std::string_view{"linker"},
                std::string_view{"renderer backend"}, std::string_view{"vulkan backend"},
                std::string_view{"opengl backend"}, std::string_view{"ai assistant"},
                std::string_view{"project assistant routing"}, std::string_view{"epoch ai"},
                std::string_view{"ai chat"}, std::string_view{"ai controls"},
                std::string_view{"editor ui"}, std::string_view{"editor gui"},
                std::string_view{"editor dock"}, std::string_view{"editor panel"},
                std::string_view{"editor toolbar"},
                std::string_view{"agents.md"},
                std::string_view{"missioncache.md"},
                std::string_view{"mission_cache.md"},
                std::string_view{"development documentation"},
                std::string_view{"documentation mission"}};
            constexpr std::array engineWords{
                std::string_view{"engine"}};
            constexpr std::array projectTerms{
                std::string_view{"scene"}, std::string_view{"world"},
                std::string_view{"project"}, std::string_view{"object"},
                std::string_view{"entity"}, std::string_view{"cube"},
                std::string_view{"camera"}, std::string_view{"light"},
                std::string_view{"asset"}, std::string_view{"sprite"},
                std::string_view{"mesh"},
                std::string_view{"material"}, std::string_view{"texture"},
                std::string_view{"level"}, std::string_view{"3d model"}};
            constexpr std::array toolTerms{
                std::string_view{"run project"}, std::string_view{"test project"},
                std::string_view{"build project"}, std::string_view{"project build"},
                std::string_view{"project test"}, std::string_view{"project run"},
                std::string_view{"inspect project"}, std::string_view{"project diagnostics"}};
            constexpr std::array questionStarts{
                std::string_view{"what "}, std::string_view{"why "},
                std::string_view{"how "}, std::string_view{"when "},
                std::string_view{"where "}, std::string_view{"who "},
                std::string_view{"can you explain"}, std::string_view{"could you explain"},
                std::string_view{"explain "}, std::string_view{"describe "},
                std::string_view{"tell me"}, std::string_view{"show me"},
                std::string_view{"should i "}, std::string_view{"should we "},
                std::string_view{"do i "}, std::string_view{"do we "},
                std::string_view{"is it "}, std::string_view{"are there "},
                std::string_view{"update me"}, std::string_view{"give me"}};

            // Source-owned mission documents are first-class engine objectives,
            // not ordinary Project Assistant chat or generated project scripts.
            const bool documentMission = lower.find("agents.md") != std::string::npos
                || lower.find("missioncache.md") != std::string::npos
                || lower.find("mission_cache.md") != std::string::npos
                || lower.find("documentation mission") != std::string::npos;
            // Read-only natural-language requests must never be inferred as
            // permission for source edits or executable tools. An explicit
            // modification verb still takes precedence ("analyze and fix").
            constexpr std::array readOnlyTerms{
                std::string_view{"analyze"}, std::string_view{"analyse"},
                std::string_view{"analysis"}, std::string_view{"report"},
                std::string_view{"review"}, std::string_view{"audit"},
                std::string_view{"inspect"}, std::string_view{"assess"},
                std::string_view{"summarize"}, std::string_view{"summarise"}};
            constexpr std::array explicitEditTerms{
                std::string_view{"fix"}, std::string_view{"edit"},
                std::string_view{"implement"}, std::string_view{"modify"},
                std::string_view{"patch"}, std::string_view{"apply"},
                std::string_view{"delete"}, std::string_view{"remove"},
                std::string_view{"refactor"}, std::string_view{"replace"},
                std::string_view{"build"}, std::string_view{"compile"},
                std::string_view{"run"}, std::string_view{"test"},
                std::string_view{"create"}, std::string_view{"change"},
                std::string_view{"update"}, std::string_view{"integrate"},
                std::string_view{"code"}};
            if (ai_has_any_word(lower, readOnlyTerms)
                && !ai_has_any_word(lower, explicitEditTerms))
                return AiChatIntent::ProjectChat; // No mutation/candidate authority.
            if (documentMission) return AiChatIntent::EngineSource;
            const bool mutation = ai_has_any_word(lower, mutationTerms);
            const bool engineTarget = ai_has_any(lower, engineTerms)
                || ai_has_any_word(lower, engineWords);
            const bool projectTarget = ai_has_any(lower, projectTerms);
            const bool projectTool = ai_has_any(lower, toolTerms);
            const bool explanatoryQuestion = std::ranges::any_of(
                questionStarts,
                [&lower](const std::string_view prefix)
                {
                    return lower.starts_with(prefix);
                });

            // Explanatory questions stay conversational even when they mention
            // mutation verbs ("how do I fix ...?").  Polite action requests
            // such as "can you add a cube?" are still routed as changes.
            if (!mutation || explanatoryQuestion)
                return AiChatIntent::ProjectChat;
            if (editor_ai_source_intent::requires_engine_source(request))
                return AiChatIntent::EngineSource;
            if (ai_project_script_requested(request))
                return AiChatIntent::ProjectAuthoring;
            if (engineTarget)
                return AiChatIntent::EngineSource;
            if (projectTool)
                return AiChatIntent::ProjectTooling;
            if (projectTarget)
                return AiChatIntent::ProjectAuthoring;
            return AiChatIntent::Ambiguous;
        };
        const auto ai_confirmation = [&](std::string_view request)
            -> std::optional<bool>
        {
            return ai_chat_confirmation(request);
        };
        const auto clarification_route = [&](std::string_view request)
            -> std::optional<AiChatIntent>
        {
            const std::string lower = ai_lower_text(trim_ai_command(request));
            if (lower.find("engine") != std::string::npos
                || lower.find("source") != std::string::npos
                || lower.find("code") != std::string::npos
                || lower.find("self-coding") != std::string::npos
                || lower.find("self coding") != std::string::npos)
                return AiChatIntent::EngineSource;
            if (lower.find("tool") != std::string::npos
                || lower.find("run") != std::string::npos
                || lower.find("test") != std::string::npos
                || lower.find("build project") != std::string::npos)
                return AiChatIntent::ProjectTooling;
            if (lower.find("project") != std::string::npos
                || lower.find("scene") != std::string::npos
                || lower.find("world") != std::string::npos
                || lower.find("gui") != std::string::npos
                || lower.find("ui") != std::string::npos
                || lower.find("asset") != std::string::npos)
                return AiChatIntent::ProjectAuthoring;
            if (lower.find("answer") != std::string::npos
                || lower.find("chat") != std::string::npos
                || lower.find("explain") != std::string::npos
                || lower.find("just tell") != std::string::npos
                || lower.find("project assistant") != std::string::npos)
                return AiChatIntent::ProjectChat;
            return std::nullopt;
        };
#if EPOCH_EDITION_PUBLIC
        start_engine_source_iteration = [&](std::string_view) -> bool
        {
            chat.append_status("This edition assists your projects. Engine source editing is available only in the Development edition. No request was sent.");
            return true;
        };
#endif
        auto dispatch_ai_intent = [&](AiChatIntent intent,
            std::string_view request, bool continuingClarification = false) -> bool
        {
            const std::string_view input = trim_ai_command(request);
            if (input.empty())
            {
                chat.append_status("The submitted request has no message body. Nothing was dispatched; your draft remains available.");
                return false;
            }
#if EPOCH_EDITION_PUBLIC
            if (editor_ai_source_intent::requires_engine_source(input))
                return start_engine_source_iteration(input);
            if (!project_ai_scope_allowed(editor.projectRoot))
            {
                chat.append_status("Choose a game or application project before asking for project assistance.");
                return false;
            }
#endif
#if EPOCH_EDITION_PUBLIC
            chat.statusRequestKind = AiDeferredRequestKind::Chat;
#endif
            switch (intent)
            {
            case AiChatIntent::EngineSource:
                return start_engine_source_iteration(input);
            case AiChatIntent::ProjectAuthoring:
                // A human can select Project Authoring explicitly, but this
                // lane has no C++ source/theme-registry operations. Dispatch
                // to the source owner instead of requesting hidden widget IDs.
                if (editor_ai_source_intent::requires_engine_source(input))
                    return start_engine_source_iteration(input);
                set_project_clarification_objective(editor, input, !continuingClarification);
                editor.aiAuthoringRequest = std::string{input};
                chat.append_status(
                    "Intent routed to Project Assistant authoring. The exact scene/GUI change will remain staged until approval.");
                return request_ai_authoring_plan(false);
            case AiChatIntent::ProjectTooling:
                editor.aiToolRequest = std::string{input};
                chat.append_status(
                    "Intent routed to a guarded Project Assistant tool request. The exact host operation will require approval.");
                return request_ai_tool_plan();
            case AiChatIntent::ProjectChat:
                {
                    // Classify the *user's actual request*, never the lengthy
                    // Project Assistant evidence wrapper. Otherwise even "hi"
                    // looks like a multi-file engine/coding instruction to the
                    // native three-tier Architect, which incorrectly selects
                    // the largest foundation and wastes scarce memory.
                    // Project-specific questions still receive project evidence.
                    const std::string lower = ai_lower_text(input);
                    const bool needsProjectEvidence = ai_has_any(lower,
                        std::array<std::string_view, 17u>{
                            "this project", "the project", "my project", "project hub",
                            "active project", "current project", "our project",
                            "this scene", "the scene", "current scene", "my scene",
                            "selected object", "the object", "scene hierarchy",
                            "my assets", "project status", "build status"});
                    const std::string projectPrompt = needsProjectEvidence
                        ? build_project_assistant_chat_prompt(editor, input)
                        : std::string{input};
                    const bool queued = submit_ai_request(
                        projectPrompt,
                        std::string{input},
                        AiDeferredRequestKind::Chat);
                    if (queued)
                    {
                        chat.append_status(
                            "Project Assistant request queued with the current active-project context.");
                    }
                    else if (editor.aiDeferredRequestKind
                        == AiDeferredRequestKind::Chat)
                    {
                        chat.append_status(
                            "Project Assistant request is retained and will start after model confirmation or the current AI work lane becomes available.");
                    }
                    else if (chat.pending || chat.worker.joinable())
                    {
                        chat.append_status(
                            "Project Assistant is waiting for the current model request to retire before accepting another message.");
                    }
                    return queued;
                }
            case AiChatIntent::Ambiguous:
            default:
                return false;
            }
        };
        const auto queue_text_message_action = [&](std::string_view request) -> bool
        {
            const auto decision = ai_confirmation(request);
            if (!decision)
                return false;

            const bool modelConsentPending =
                editor.aiDeferredRequestKind != AiDeferredRequestKind::None
                && !epochengine::ai::is_model_use_confirmed();
            const bool planAwaitingApproval = !editor.aiProjectScriptDraft.empty()
                || (editor.aiAuthoringPlan && !editor.aiAuthoringPlanApplied && !editor.aiAuthoringApplyPending);
            const bool toolPlanAwaitingApproval = editor.aiToolPlan
                && !editor.aiToolPlanApplied;
            const bool toolTestPending = editor.aiToolTestPending.has_value();
#if EPOCH_EDITION_PUBLIC
            constexpr bool sourceActionPending = false;
#endif
            if (toolTestPending)
            {
                if (*decision)
                    return false;
                editor.aiQueuedMessageActionIndex = 0u;
            }
            else if (toolPlanAwaitingApproval || planAwaitingApproval
                || modelConsentPending || sourceActionPending)
            {
                editor.aiQueuedMessageActionIndex = *decision ? 0u : !editor.aiProjectScriptDraft.empty() ? 2u : 1u;
            }
            else
            {
                return false;
            }

            chat.append_user_message(std::string{trim_ai_command(request)});
            chat.append_status(
                *decision
                    ? "Interpreted your reply as approval of the currently staged action."
                    : "Interpreted your reply as rejection of the currently staged action.");
            return true;
        };
