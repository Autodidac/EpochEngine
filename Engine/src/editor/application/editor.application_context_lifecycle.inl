// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
    void cleanup_chat_context(const core::Context* ctx)
    {
        if (!ctx) return;

        auto& chatStorage = chat_storage();
        auto& editorStorage = editor_storage();
        decltype(chatStorage.chats)::node_type retiredChat{};
        decltype(editorStorage.states)::node_type retiredState{};
        {
            std::scoped_lock lock(chatStorage.mutex, editorStorage.mutex);
            retiredChat = chatStorage.chats.extract(ctx);
            retiredState = editorStorage.states.extract(ctx);
            std::erase_if(
                editorStorage.detachedAiChatCommands,
                [ctx](const DetachedAiChatCommand& command)
                {
                    return command.ownerContext == ctx;
                });
            if (editorStorage.detachedPaneProjection.ownerContext == ctx)
                editorStorage.detachedPaneProjection = {};
        }
        if (!retiredChat.empty() && retiredChat.mapped())
            (void)retiredChat.mapped()->cancel_pending();
        if (!retiredState.empty())
        {
            persist_editor_layout_preferences(retiredState.mapped(), true);
        }
        if (!retiredChat.empty())
            retire_detached_chat(retiredChat.mapped());
        if (!retiredState.empty())
            finish_ai_work_owner_close(retiredState.mapped(),
                !retiredChat.empty() && retiredChat.mapped()
                    && retiredChat.mapped()->requestRetirementFailed);
        (void)epochengine::canvas2d::scene_content::retire(ctx);
        epochengine::previewgrid::cleanup_context(ctx);
    }

    void shutdown_chat_system()
    {
        epochengine::ai::native_runtime::request_shutdown();
        epochengine::ai::cancel_engine_ai_request();
        auto& chatStorage = chat_storage();
        auto& editorStorage = editor_storage();
        decltype(chatStorage.chats) retiredChats{};
        decltype(editorStorage.states) retiredStates{};
        bool shutdownAi{};
        {
            std::scoped_lock lock(chatStorage.mutex, editorStorage.mutex);
            retiredChats.swap(chatStorage.chats);
            retiredStates.swap(editorStorage.states);
            editorStorage.detachedPaneRoutes.clear();
            editorStorage.paneRedockRequests.clear();
            editorStorage.detachedAiChatCommands.clear();
            editorStorage.detachedPaneProjection = {};
            editorStorage.passiveContextScores = {};
            shutdownAi = chatStorage.bot_initialized;
            chatStorage.bot_initialized = false;
        }

        for (const auto& [ctx, chat] : retiredChats)
        {
            (void)ctx;
            if (chat)
                (void)chat->cancel_pending();
        }
        for (auto& [ctx, state] : retiredStates)
        {
            (void)ctx;
            persist_editor_layout_preferences(state, true);
        }
        // Stop every context before joining any scheduler: a slow unrelated
        // task must not leave later contexts launching work or running previews.
        for (auto& [ctx, state] : retiredStates)
        {
            (void)epochengine::canvas2d::scene_content::retire(ctx);
            epochengine::previewgrid::cleanup_context(ctx);
        }
        for (const auto& [ctx, chat] : retiredChats)
        {
            (void)ctx;
            retire_detached_chat(chat);
        }

        for (auto& [ctx, state] : retiredStates)
        {
            const auto found = retiredChats.find(ctx);
            finish_ai_work_owner_close(state, found != retiredChats.end()
                && found->second && found->second->requestRetirementFailed);
        }

        if (shutdownAi)
            epochengine::ai::shutdown_engine_ai();
        epochengine::ai::native_runtime::finish_shutdown();

    }

    void editor_mark_context_panel_detached(std::string_view route_id, bool detached)
    {
        set_detached_pane_route(route_id, detached);
    }

    void editor_notify_context_panel_closed(std::string_view route_id)
    {
        set_detached_pane_route(route_id, false);
        const auto pane = editor_tool_pane_from_route(route_id);
        if (!pane)
            return;

        auto& storage = editor_storage();
        std::scoped_lock lock(storage.mutex);
        for (auto& [context, editor] : storage.states)
        {
            (void)context;
            const std::size_t index = tool_pane_index(*pane);
            editor.toolPaneOpen[index] = true;
            if (is_outliner_tool_pane(*pane))
                editor.showOutliner = true;
            else if (is_inspector_tool_pane(*pane))
                editor.showInspector = true;
            else if (is_console_tool_pane(*pane))
                editor.showConsoleDock = true;
            else if (*pane == EditorToolPane::AiChat)
                editor.showAiChat = true;

            const EditorToolDockRegion region = editor.toolPaneDockRegions[index];
            std::string& active =
                region == EditorToolDockRegion::Left
                ? editor.activeLeftPaneRoute
                : region == EditorToolDockRegion::Right
                    ? editor.activeRightPaneRoute
                    : region == EditorToolDockRegion::BottomLeft
                        ? editor.activeBottomLeftPaneRoute
                        : editor.activeBottomRightPaneRoute;
            active.assign(route_id);
            editor.detachedPanelHostStatus =
                std::string(route_id) + " returned after its native window closed.";
            editor.surfaceSettleFrames = (std::max)(editor.surfaceSettleFrames, 1);
        }
    }

    void editor_redock_context_panel(
        std::string_view route_id,
        std::uint8_t dock_target,
        std::uint32_t tab_insertion_index)
    {
        set_detached_pane_route(route_id, false);
        if (dock_target < 1u || dock_target > 4u)
            return;

        const EditorToolDockRegion region =
            dock_target == 1u ? EditorToolDockRegion::Left
            : dock_target == 2u ? EditorToolDockRegion::Right
            : dock_target == 3u ? EditorToolDockRegion::BottomLeft
            : EditorToolDockRegion::BottomRight;

        auto& storage = editor_storage();
        std::scoped_lock lock(storage.mutex);
        for (auto& [context, editor] : storage.states)
        {
            (void)context;
            const auto pane = editor_tool_pane_from_route(route_id);
            if (!pane)
                continue;

            const std::size_t index = tool_pane_index(*pane);
            editor.toolPaneOpen[index] = true;
            editor.toolPaneDockRegions[index] = region;
            if (tab_insertion_index != 0xffffffffu)
            {
                std::vector<std::size_t> ordered{};
                ordered.reserve(kEditorToolPaneCount);
                for (std::size_t candidate = 0u;
                    candidate < kEditorToolPaneCount; ++candidate)
                {
                    if (candidate != index
                        && editor.toolPaneOpen[candidate]
                        && editor.toolPaneDockRegions[candidate] == region)
                    {
                        ordered.push_back(candidate);
                    }
                }
                std::stable_sort(
                    ordered.begin(),
                    ordered.end(),
                    [&](std::size_t left, std::size_t right)
                    {
                        const auto leftOrder = editor.toolPaneTabOrder[left];
                        const auto rightOrder = editor.toolPaneTabOrder[right];
                        return leftOrder != rightOrder
                            ? leftOrder < rightOrder
                            : left < right;
                    });
                const std::size_t insertion = (std::min)(
                    static_cast<std::size_t>(tab_insertion_index),
                    ordered.size());
                ordered.insert(
                    ordered.begin() + static_cast<std::ptrdiff_t>(insertion),
                    index);
                for (std::size_t order = 0u; order < ordered.size(); ++order)
                {
                    editor.toolPaneTabOrder[ordered[order]] =
                        static_cast<std::uint32_t>(order);
                }
            }
            if (is_outliner_tool_pane(*pane))
                editor.showOutliner = true;
            else if (is_inspector_tool_pane(*pane))
                editor.showInspector = true;
            else if (is_console_tool_pane(*pane))
                editor.showConsoleDock = true;
            else if (*pane == EditorToolPane::AiChat)
                editor.showAiChat = true;

            std::string& active =
                region == EditorToolDockRegion::Left
                ? editor.activeLeftPaneRoute
                : region == EditorToolDockRegion::Right
                    ? editor.activeRightPaneRoute
                    : region == EditorToolDockRegion::BottomLeft
                        ? editor.activeBottomLeftPaneRoute
                        : editor.activeBottomRightPaneRoute;
            active.assign(route_id);
            editor.detachedPanelHostStatus =
                std::string(route_id)
                + (tab_insertion_index == 0xffffffffu
                    ? " redocked from its native window."
                    : " inserted into an exact native-window tab slot.");
            editor.surfaceSettleFrames =
                (std::max)(editor.surfaceSettleFrames, 1);
        }
    }

    bool editor_open_project(
        const std::shared_ptr<core::Context>& ctx,
        std::string_view project_id)
    {
        if (!ctx || project_id.empty()
            || !editor_find_project_profile(project_id))
        {
            return false;
        }

        auto& editor = editor_state_for(ctx);
        set_project(editor, project_id, true);
        epochengine::previewgrid::set_camera_mode(
            ctx.get(), editor.viewportCameraMode);
        epochengine::previewgrid::reset_camera(ctx.get());
        return editor.projectId == project_id
            && editor.projectLifecycle.evidence().project_selected;
    }

    bool editor_close_project(
        const std::shared_ptr<core::Context>& ctx)
    {
        if (!ctx)
            return false;

        auto& editor = editor_state_for(ctx);
        return close_active_project(
            editor,
            editor_application_profile(editor.applicationKind),
            true);
    }

    void editor_load_project(const std::shared_ptr<core::Context>& ctx, std::string_view project_id)
    {
        if (!ctx)
            return;

        if (const auto* application = editor_application_for_project(project_id))
        {
            editor_load_application(ctx, application->kind);
            return;
        }

        (void)editor_open_project(ctx, project_id);
    }

    void editor_load_application(
        const std::shared_ptr<core::Context>& ctx,
        EditorApplicationKind applicationKind)
    {
        if (!ctx)
            return;

        const auto& application = editor_application_profile(applicationKind);
        auto& editor = editor_state_for(ctx);
        persist_editor_layout_preferences(editor, true);
        if (application.project_id.empty())
        {
            if (!close_active_project(editor, application, true))
                return;
        }
        else
        {
            set_project(editor, application.project_id, true);
        }
        apply_application_profile(editor, application);
        // Editor-specific deferred catalog discovery, including after returning
        // to the launcher and selecting a different editor application.
        editor.aiNativeAutoScanAttempted = false;
        editor.aiNativeAutoScanDelayFrames = 4u;
        editor.layoutPreferencesLoaded = false;
        editor.layoutPreferencesApplication = application.kind;
        load_editor_layout_preferences(editor);
        editor.previewMode = core::ScenePreviewMode::Editor;
        editor.surfaceSettleFrames = (std::max)(editor.surfaceSettleFrames, 2);
        epochengine::previewgrid::set_camera_mode(ctx.get(), editor.viewportCameraMode);

        push_editor_log(
            editor,
            std::string("[editor] Opened dedicated ") + std::string(application.display_name)
                + " application with scene " + std::string(application.scene_id) + ".");
        epochengine::previewgrid::reset_camera(ctx.get());
    }

    void editor_suppress_startup_update_check(const std::shared_ptr<core::Context>& ctx)
    {
        if (!ctx)
            return;

        auto& editor = editor_state_for(ctx);
        editor.autoUpdateCheckQueued = true;
        if (editor.updateCheckPending.has_value()
            || editor.updateState == EditorUpdateState::SourceWorkerRunning
            || editor.updateState == EditorUpdateState::RestartReady)
        {
            return;
        }

        editor.showUpdateConfirmModal = false;
        editor.showSourceUpdateConfirmModal = false;
    }

    void editor_reset_transient_ui(const core::Context* ctx)
    {
        if (!ctx)
            return;

        auto& storage = editor_storage();
        std::unique_lock lock(storage.mutex);
        const auto it = storage.states.find(ctx);
        if (it == storage.states.end())
            return;

        auto& parked = it->second;

        it->second.openMenu = TopMenu::None;
        it->second.showAboutModal = false;
        it->second.showSettingsModal = false;
        it->second.showPackageManagerModal = false;
        it->second.showAiModelConsentModal = false;
        it->second.showVoiceConsentModal = false;
        epochengine::ai::voice_runtime::stop(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&it->second)));
        it->second.aiVoiceSession.cancel();
        it->second.aiDeferredPrompt.clear();
        it->second.aiDeferredDisplay.clear();
        it->second.aiDeferredRequestKind = AiDeferredRequestKind::None;
        it->second.aiDeferredModelLease.reset();
        it->second.aiDeferredDispatchQueued = false;
        cancel_ai_work_admission(it->second);
        it->second.aiModelMemoryApproval.revoke(it->second.aiWorkAdmission);
        it->second.showUpdateConfirmModal = false;
        it->second.showSourceUpdateConfirmModal = false;
        it->second.previewMode = core::ScenePreviewMode::Editor;
        it->second.viewportCameraMode = epochengine::previewgrid::CameraMode::Editor;
        lock.unlock();
        // Menu contexts still receive the existing time-snapshot owner tick.
        // Stop admission now, then drain only ready results there without a UI
        // sleep, scheduler join, source edit, or loss of the saved project goal.
        {
            auto& chats = chat_storage();
            std::scoped_lock chatLock(chats.mutex);
            const auto found = chats.chats.find(ctx);
            if (found != chats.chats.end() && found->second)
                (void)found->second->cancel_pending();
        }
        epochengine::previewgrid::set_camera_mode(ctx, epochengine::previewgrid::CameraMode::Editor);
        epochengine::previewgrid::reset_camera(ctx);
    }

    void editor_set_time_snapshot(const core::Context* ctx, const EditorTimeSnapshot& snapshot)
    {
        if (!ctx)
            return;

        const auto passiveObservation = passive_context_observation_for(ctx);
        auto& storage = editor_storage();
        std::unique_lock lock(storage.mutex);
        const auto it = storage.states.find(ctx);
        if (it == storage.states.end())
            return;

        auto& state = it->second;
        state.timeSnapshot = snapshot;
        update_passive_context_scoring(storage, state, ctx, snapshot, passiveObservation);
#if EPOCH_EDITION_PUBLIC
        lock.unlock();
#endif
    }

    bool editor_engine_work_blocks_context_switch(const core::Context* ctx)
    {
        if (!ctx) return false;
        // Read owning-context state without touching its task/worker lifetime.
        // This is a query, never a cancellation or renderer-context teardown.
        bool ownedWork{};
        {
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto found = storage.states.find(ctx);
            if (found == storage.states.end()) return false;
            const EditorState& editor = found->second;
            ownedWork = ai_work_pending(editor)
                || editor.aiAuthoringAwaitingReply
                || editor.aiGoalRunning;
        }
        if (ownedWork) return true;
        // Chat generation can be active independently of the Assistant panel.
        // A renderer switch must not destroy its worker either.
        auto& chats = chat_storage();
        std::scoped_lock lock(chats.mutex);
        const auto found = chats.chats.find(ctx);
        return found != chats.chats.end() && found->second
            && (found->second->pending || found->second->worker.joinable()
                || found->second->requestRetirementFailed);
    }

    void editor_set_context_selection_status(const core::Context* ctx, std::string_view status)
    {
        if (!ctx)
            return;

        auto& storage = editor_storage();
        std::scoped_lock lock(storage.mutex);
        const auto it = storage.states.find(ctx);
        if (it == storage.states.end())
            return;

        auto& state = it->second;
        state.contextSelectionStatus = status.empty()
            ? std::string{ "Context follows the active editor window." }
            : std::string{ status };
        push_editor_log(state, std::string{ "[context] " } + state.contextSelectionStatus);
    }

    EditorTimeControl editor_time_control(const core::Context* ctx)
    {
        if (!ctx)
            return {};

        auto& storage = editor_storage();
        std::scoped_lock lock(storage.mutex);
        const auto it = storage.states.find(ctx);
        return it == storage.states.end() ? EditorTimeControl{} : it->second.timeControl;
    }

    void editor_consume_time_step_request(const core::Context* ctx)
    {
        if (!ctx)
            return;

        auto& storage = editor_storage();
        std::scoped_lock lock(storage.mutex);
        const auto it = storage.states.find(ctx);
        if (it == storage.states.end())
            return;

        it->second.timeControl.step_once = false;
    }

