// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
    bool editor_ai_model_selection_contract() noexcept
    {
        try
        {
            std::size_t baseline{};
            {
                std::scoped_lock lock(aiModelSelectionMutex);
                baseline = aiModelSelectionUsers;
            }
            const auto count = [] {
                std::scoped_lock lock(aiModelSelectionMutex);
                return aiModelSelectionUsers;
            };
            {
                auto deferred = std::make_shared<AiModelSelectionLease>();
                auto sourceHandoff = deferred;
                if (count() != baseline + 1u) return false;
                auto request = std::make_shared<AiChatRequestState>();
                if (count() != baseline + 2u) return false;
                deferred.reset();
                (void)request->cancellation.request_stop();
                if (count() != baseline + 2u) return false;
                request.reset();
                // Reply handoff retains the original selection. Cancellation
                // alone never releases an active worker's selection lease.
                if (count() != baseline + 1u) return false;
                sourceHandoff.reset();
            }
            return count() == baseline;
        }
        catch (...) { return false; }
    }

    bool editor_ai_request_cancellation_contract() noexcept
    {
        // Exercise the actual chat ownership/pump paths without starting a
        // model, worker, renderer, or transport. Transport has its own contract.
        try
        {
            auto projectContext = std::make_unique<EditorState>();
            projectContext->projectName = "Project Assistant Canary";
            projectContext->projectId = "project.assistant.canary";
            projectContext->projectStatus = "ready";
            projectContext->projectBuildStatus = "build idle";
            projectContext->sceneDocumentRevision = 42u;
            projectContext->activeScript = "starter";
            const std::string projectPrompt = build_project_assistant_chat_prompt(
                *projectContext, "What is selected and is the build ready?");
            if (projectPrompt.find("EPOCH_PROJECT_ASSISTANT_CONTEXT_V1")
                    == std::string::npos
                || projectPrompt.find("Project Assistant Canary")
                    == std::string::npos
                || projectPrompt.find("Scene revision: 42")
                    == std::string::npos
                || projectPrompt.find("USER_REQUEST")
                    == std::string::npos
                || projectPrompt.find("What is selected and is the build ready?")
                    == std::string::npos)
                return false;

            AiChat projectChannel{};
            projectChannel.pendingWorkload = ai::InferenceWorkload::chat;
            projectChannel.pendingRequestKind = AiDeferredRequestKind::Chat;
            projectChannel.pending = std::make_shared<AiChatRequestState>();
            projectChannel.pending->reply =
                "EPOCH_TOOL_PLAN_V1 mentioned as ordinary project conversation";
            projectChannel.pending->ready.store(true, std::memory_order_release);
            projectChannel.pump();
            if (projectChannel.latestCompletionRequestKind
                    != AiDeferredRequestKind::Chat
                || projectChannel.latestCompletionWorkload
                    != ai::InferenceWorkload::chat
                )
                return false;

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

