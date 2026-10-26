// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        struct ContextPtrHash
        {
            std::size_t operator()(const core::Context* p) const noexcept
            {
                return std::hash<const void*>{}(p);
            }
        };

        [[nodiscard]] static bool editor_point_in_rect(gui::Vec2 point, gui::Vec2 pos, gui::Vec2 size) noexcept
        {
            return point.x >= pos.x
                && point.y >= pos.y
                && point.x <= pos.x + size.x
                && point.y <= pos.y + size.y;
        }

        [[nodiscard]] static bool main_surface_uses_scene(EditorMainSurface surface) noexcept
        {
            return surface == EditorMainSurface::Scene
                || surface == EditorMainSurface::Game2D
                || surface == EditorMainSurface::ForestFactory
                || surface == EditorMainSurface::PlantLab;
        }

        [[nodiscard]] static bool main_surface_is_ai_authoring(
            EditorMainSurface surface) noexcept
        {
            return surface == EditorMainSurface::Scene
                || surface == EditorMainSurface::Game2D;
        }

        [[nodiscard]] static std::string_view main_surface_title(EditorMainSurface surface) noexcept
        {
            switch (surface)
            {
            case EditorMainSurface::Scene:
                return "3D Scene";
            case EditorMainSurface::Game2D:
                return "2D / UI";
            case EditorMainSurface::Assets:
                return "Assets";
            case EditorMainSurface::Project:
                return "Project";
            case EditorMainSurface::ForestFactory:
                return "Forest Factory";
            case EditorMainSurface::PlantLab:
                return "Plant Lab";
            case EditorMainSurface::Timeline:
                return "Timeline";
            case EditorMainSurface::AISandbox:
                return "Assistant";
            case EditorMainSurface::Systems:
                return "System Info";
            default:
                return "Workspace";
            }
        }

        static void reset_editor_layout(EditorState& editor) noexcept
        {
            const auto& application = editor_application_profile(editor.applicationKind);
            editor.outlinerSplit = 0.20f;
            editor.inspectorSplit = 0.33f;
            editor.dockSplit = 0.28f;
            editor.bottomGridSplit = 0.32f;
            editor.workspaceBottomGridSplits = filled_workspace_values(0.32f);
            editor.workspaceDockSplits = filled_workspace_values(0.28f);
            editor.workspaceOutputFollow = filled_workspace_values(true);
            editor.workspaceAiChatFollow = filled_workspace_values(true);
            editor.dockPreferenceSurface = editor.mainSurface;
            editor.outputFollowTail = true;
            editor.aiChatFollowTail = true;
            editor.showOutliner = application.panes.outliner;
            editor.showInspector = application.panes.inspector;
            editor.showConsoleDock = application.panes.console;
            editor.showAiChat = application.panes.ai_chat;
            editor.toolPaneOpen = default_tool_pane_open_state();
            editor.toolPaneDockRegions = default_tool_pane_regions();
            editor.toolPaneTabOrder = default_tool_pane_tab_order();
            editor.activeLeftPaneRoute = "pane.world_outliner";
            editor.activeRightPaneRoute = "pane.ai_chat";
            editor.activeBottomLeftPaneRoute = "pane.output";
            editor.activeBottomRightPaneRoute = "pane.ai_development";
            editor.layoutDrag = EditorLayoutDrag::None;
            editor.surfaceSettleFrames = 0;
            editor.lastLayoutExtent = {};
            editor.dockableGuiHost = {};
            editor.floatingGuiWindow = {
                .id = 1,
                .mode = gui::DockableWindowMode::floating,
                .dock_slot = gui::DockSlot::right,
                .floating = { .open = false },
                .visible = false
            };
            editor.detachedPanelHostStatus = "Layout reset. Pane popouts remain separate from context selection.";

            editor.contextSelectionStatus = "Context follows the active editor window.";
            editor.passiveContextScoreStatus = "Passive context scoring is waiting for single-context editor timing.";
            editor.passiveContextRecommendation = "No recommended editor context yet.";
            editor.passiveContextLastFrameMs = 0.0;
            editor.passiveContextHasLastFrame = false;
        }

        struct ContextPtrEq
        {
            bool operator()(const core::Context* a, const core::Context* b) const noexcept
            {
                return a == b;
            }
        };

        struct ChatStorage
        {
            std::mutex mutex{};
            bool bot_initialized{ false };
            std::unordered_map<
                const core::Context*,
                std::shared_ptr<AiChat>,
                ContextPtrHash,
                ContextPtrEq> chats{};
        };

        editor_project_textures::ProjectTextureController*
            project_texture_controller(EditorState& editor);

#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
        [[nodiscard]]
        editor_tilemaps::TileMapWorkspaceController*
            tilemap_workspace_controller(EditorState& editor);
#endif

        enum class DetachedAiChatCommandKind : std::uint8_t
        {
            submit,
            retry_queued,
            cancel_queued,
            stop_pending
        };

        struct DetachedAiChatCommand final
        {
            const core::Context* ownerContext{};
            DetachedAiChatCommandKind kind{DetachedAiChatCommandKind::submit};
            std::string prompt{};
        };

        struct DetachedPaneProjection
        {
            bool ready{};
            const core::Context* ownerContext{};
            EditorApplicationKind applicationKind{
                EditorApplicationKind::Standard};
            std::string projectName{};
            std::string activeWorld{};
            std::vector<EditorEntity> entities{};
            scene::SceneObjectId selectedEntityId{
                scene::kInvalidSceneObjectId};
            std::vector<std::string> logLines{};
            std::vector<std::string> chatLines{};
            std::shared_ptr<const std::vector<gui::ChatMessageRunInfo>> chatRunDetails{};
            std::string aiStatus{};
            bool aiDeferredChatQueued{};
            bool aiDeferredDispatchQueued{};
            bool aiRequestPending{};
            bool aiDetachedSubmitQueued{};
            core::ScenePreviewMode previewMode{
                core::ScenePreviewMode::Editor};
            std::uint64_t sceneRevision{};
        };

        struct EditorStorage
        {
            std::mutex mutex{};
            std::map<const core::Context*, EditorState> states{};
            std::unordered_map<std::string, bool> detachedPaneRoutes{};
            std::unordered_map<std::string, bool> paneRedockRequests{};
            std::vector<DetachedAiChatCommand> detachedAiChatCommands{};
            DetachedPaneProjection detachedPaneProjection{};
            epochengine::context::PassiveContextScoreboard passiveContextScores{};
        };

        ChatStorage& chat_storage()
        {
            static ChatStorage storage;
            return storage;
        }

        EditorStorage& editor_storage()
        {
            static EditorStorage storage;
            return storage;
        }

        // Defined below after the detached-route registry helpers.
        [[nodiscard]] bool pane_route_is_detached(std::string_view route);

        void publish_detached_pane_projection(
            const core::Context* ownerContext,
            EditorState& editor,
            const AiChat& chat)
        {
            // This runs on EVERY editor frame. A canonical/docked editor must
            // not deep-copy its entire scene and conversation into an unused
            // popout snapshot. In particular, a long chat made this cost grow
            // throughout an otherwise idle editor session.
            bool chatDetached = false;
            bool hierarchyDetached = false;
            bool outputDetached = false;
            {
                auto& storage = editor_storage();
                std::scoped_lock lock(storage.mutex);
                if (storage.detachedPaneRoutes.empty())
                {
                    if (storage.detachedPaneProjection.ready)
                        storage.detachedPaneProjection = {};
                    editor.detachedChatRunSnapshot.reset();
                    return;
                }
                for (const auto& [route, detached] : storage.detachedPaneRoutes)
                {
                    if (!detached) continue;
                    if (const auto pane = editor_tool_pane_from_route(route))
                    {
                        chatDetached |= *pane == EditorToolPane::AiChat;
                        hierarchyDetached |= is_outliner_tool_pane(*pane)
                            || is_inspector_tool_pane(*pane);
                        outputDetached |= is_console_tool_pane(*pane);
                    }
                }
            }
            if (chatDetached)
            {
                const auto now = std::chrono::steady_clock::now();
                if (!editor.detachedChatRunSnapshot
                    || ((chat.nativeRawSequence != editor.detachedChatRawSequence
                            || chat.completionGeneration != editor.detachedChatCompletionGeneration)
                        && now - editor.detachedChatRunSnapshotAt
                            >= std::chrono::milliseconds{250}))
                {
                    editor.detachedChatRunSnapshot =
                        std::make_shared<const std::vector<gui::ChatMessageRunInfo>>(
                            chat.lineRunDetails);
                    editor.detachedChatRunSnapshotAt = now;
                    editor.detachedChatRawSequence = chat.nativeRawSequence;
                    editor.detachedChatCompletionGeneration = chat.completionGeneration;
                }
            }
            else editor.detachedChatRunSnapshot.reset();

            DetachedPaneProjection projection{
                .ready = true,
                .ownerContext = ownerContext,
                .applicationKind = editor.applicationKind,
                .projectName = editor.projectName,
                .activeWorld = editor.activeWorld,
                .entities = hierarchyDetached ? editor.entities : std::vector<EditorEntity>{},
                .selectedEntityId = editor.selectedEntityId,
                .logLines = outputDetached ? editor.logLines : std::vector<std::string>{},
                .chatLines = chatDetached ? chat.lines : std::vector<std::string>{},
                .chatRunDetails = editor.detachedChatRunSnapshot,
                .aiStatus = chatDetached ? editor.aiAuthoringStatus : std::string{},
                .aiDeferredChatQueued = chatDetached && editor.aiDeferredRequestKind
                    == AiDeferredRequestKind::Chat,
                .aiDeferredDispatchQueued = chatDetached && editor.aiDeferredDispatchQueued,
                .aiRequestPending = chatDetached && static_cast<bool>(chat.pending),
                .previewMode = editor.previewMode,
                .sceneRevision = editor.sceneDocumentRevision
            };
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            // A window can be re-docked between the first lock and publishing.
            if (storage.detachedPaneRoutes.empty())
            {
                storage.detachedPaneProjection = {};
                return;
            }
            projection.aiDetachedSubmitQueued = chatDetached && std::any_of(
                storage.detachedAiChatCommands.begin(),
                storage.detachedAiChatCommands.end(),
                [ownerContext](const DetachedAiChatCommand& command)
                {
                    return command.ownerContext == ownerContext
                        && command.kind == DetachedAiChatCommandKind::submit;
                });
            storage.detachedPaneProjection = std::move(projection);
        }

        [[nodiscard]] DetachedPaneProjection
            detached_pane_projection()
        {
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            return storage.detachedPaneProjection;
        }

        [[nodiscard]] bool queue_detached_ai_chat_command(
            const core::Context* ownerContext,
            DetachedAiChatCommandKind kind,
            std::string prompt = {})
        {
            if (!ownerContext || prompt.size() > 4096u)
                return false;

            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            constexpr std::size_t maximumQueuedCommands = 16u;
            if (storage.detachedAiChatCommands.size() >= maximumQueuedCommands)
                return false;

            const bool duplicate = std::any_of(
                storage.detachedAiChatCommands.begin(),
                storage.detachedAiChatCommands.end(),
                [ownerContext, kind](const DetachedAiChatCommand& command)
                {
                    return command.ownerContext == ownerContext
                        && command.kind == kind;
                });
            if (duplicate)
                return false;

            storage.detachedAiChatCommands.push_back(DetachedAiChatCommand{
                .ownerContext = ownerContext,
                .kind = kind,
                .prompt = std::move(prompt)
            });
            return true;
        }

        [[nodiscard]] std::vector<DetachedAiChatCommand>
            consume_detached_ai_chat_commands(const core::Context* ownerContext)
        {
            std::vector<DetachedAiChatCommand> commands{};
            if (!ownerContext)
                return commands;

            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            for (auto it = storage.detachedAiChatCommands.begin();
                it != storage.detachedAiChatCommands.end();)
            {
                if (it->ownerContext != ownerContext)
                {
                    ++it;
                    continue;
                }
                commands.push_back(std::move(*it));
                it = storage.detachedAiChatCommands.erase(it);
            }
            return commands;
        }

        [[nodiscard]] bool is_detachable_pane_route(std::string_view route) noexcept
        {
            return editor_tool_pane_from_route(route).has_value();
        }

        void set_detached_pane_route(std::string_view route, bool detached)
        {
            if (!is_detachable_pane_route(route))
                return;

            const std::string key{route};
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            if (detached)
            {
                storage.paneRedockRequests.erase(key);
                storage.detachedPaneRoutes[key] = true;
            }
            else
            {
                storage.detachedPaneRoutes.erase(key);
                storage.paneRedockRequests.erase(key);
            }
        }

        // A native promoted context may already have disappeared by the time
        // its Window-menu action runs. Unhide the canonical dock immediately,
        // while retaining a redock signal for a still-alive external host.
        void restore_detached_pane_to_dock(std::string_view route)
        {
            if (!is_detachable_pane_route(route))
                return;
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const std::string key{route};
            if (storage.detachedPaneRoutes.erase(key) != 0u)
                storage.paneRedockRequests[key] = true;
        }

        [[nodiscard]] bool consume_detached_pane_redock_request(
            std::string_view route)
        {
            if (!is_detachable_pane_route(route))
                return false;

            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto request = storage.paneRedockRequests.find(
                std::string(route));
            if (request == storage.paneRedockRequests.end())
                return false;
            storage.paneRedockRequests.erase(request);
            return true;
        }

        [[nodiscard]] bool pane_route_is_detached(std::string_view route)
        {
            if (!is_detachable_pane_route(route))
                return false;

            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            return storage.detachedPaneRoutes.contains(std::string(route));
        }

        void push_editor_log(EditorState& state, std::string line)
        {
            // Engine-wide output is independent of any candidate workspace.
            const auto engineRoot = state.aiDiagnosticEngineRoot.empty()
                ? editor_ai_session_diagnostics::fallback_engine_root()
                : std::filesystem::path{state.aiDiagnosticEngineRoot};
            (void)editor_ai_session_diagnostics::record_engine(engineRoot,
                "editor", "event", line);
            state.logLines.push_back(std::move(line));
            constexpr std::size_t kMaxLogLines = 10;
            if (state.logLines.size() > kMaxLogLines)
                state.logLines.erase(state.logLines.begin(), state.logLines.begin() + static_cast<std::ptrdiff_t>(state.logLines.size() - kMaxLogLines));
        }

        void request_manual_engine_diagnostics(EditorState& state,
            std::string_view surface) noexcept
        {
            const auto now = std::chrono::steady_clock::now();
            if (state.aiLastManualDiagnosticsExport !=
                    std::chrono::steady_clock::time_point{}
                && now - state.aiLastManualDiagnosticsExport
                    < std::chrono::seconds{30})
            {
                push_editor_log(state,
                    "[diagnostics] Export already requested recently; wait 30 seconds before another ZIP.");
                return;
            }
            state.aiLastManualDiagnosticsExport = now;
#if EPOCH_EDITION_PUBLIC
            (void)surface;
            const auto archive = editor_ai_session_diagnostics::bundle_engine(
                std::filesystem::path{ai::default_evidence_paths().workspace_root});
            if (!archive)
            {
                push_editor_log(state, "[diagnostics] Could not export the engine logs.");
                return;
            }
#endif
            push_editor_log(state,
                "[diagnostics] Engine-wide diagnostics ZIP requested under logs/.");
        }

        // Host lifecycle evidence only: never pass a prompt, reply or source
        // packet here. Keep disk logging independent of compiler/child cleanup.
