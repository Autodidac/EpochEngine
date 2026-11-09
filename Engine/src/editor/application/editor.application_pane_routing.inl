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

        [[nodiscard]] static bool editor_workspace_supported(
            const EditorState& editor, const EditorWorkspaceSpec& workspace) noexcept
        {
            return editor_application_supports_surface(editor_application_profile(editor.applicationKind), workspace.surface)
                && !(workspace.surface == EditorMainSurface::Game2D && workspace.gui_workspace == false
                    && editor_application_owns_dedicated_gui_workspace(editor.applicationKind));
        }

        [[nodiscard]] static bool editor_workspace_active(
            const EditorState& editor, const EditorWorkspaceSpec& workspace) noexcept
        {
            return editor.mainSurface == workspace.surface
                && (!workspace.gui_workspace || (editor.game2DGuiWorkspace
                    || editor_application_owns_dedicated_gui_workspace(editor.applicationKind)) == *workspace.gui_workspace);
        }

        [[nodiscard]] static bool editor_workspace_visible(
            const EditorState& editor, const EditorWorkspaceSpec& workspace) noexcept
        {
            if (!editor.workspaceVisibilityCustomized)
                return workspace.default_open || editor_workspace_active(editor, workspace);
            return editor_workspace_active(editor,workspace)
                || std::find(editor.visibleWorkspaceIds.begin(),editor.visibleWorkspaceIds.end(),workspace.id)
                != editor.visibleWorkspaceIds.end();
        }

        static bool set_editor_workspace_visible(EditorState& editor, std::string_view id, bool visible)
        {
            const auto found=std::find_if(kEditorWorkspaceSpecs.begin(),kEditorWorkspaceSpecs.end(),
                [&](const auto& workspace){return workspace.id==id;});
            if(found==kEditorWorkspaceSpecs.end()||!editor_workspace_supported(editor,*found)) return false;
            if(!editor.workspaceVisibilityCustomized)
            {
                for(const auto& workspace:kEditorWorkspaceSpecs)
                    if(editor_workspace_supported(editor,workspace)&&editor_workspace_visible(editor,workspace))
                        editor.visibleWorkspaceIds.emplace_back(workspace.id);
                editor.workspaceVisibilityCustomized=true;
            }
            const auto saved=std::find(editor.visibleWorkspaceIds.begin(),editor.visibleWorkspaceIds.end(),id);
            if(visible&&saved==editor.visibleWorkspaceIds.end()) editor.visibleWorkspaceIds.emplace_back(id);
            else if(!visible&&saved!=editor.visibleWorkspaceIds.end()) editor.visibleWorkspaceIds.erase(saved);
            return true;
        }

        static void reveal_editor_workspace_surface(EditorState& editor, EditorMainSurface surface)
        {
            for(const auto& workspace:kEditorWorkspaceSpecs)
                if(workspace.surface==surface && (!workspace.gui_workspace
                    || (editor.game2DGuiWorkspace||editor_application_owns_dedicated_gui_workspace(editor.applicationKind))==*workspace.gui_workspace))
                    (void)set_editor_workspace_visible(editor,workspace.id,true);
        }

        [[nodiscard]] static const EditorWorkspaceSpec* editor_workspace_fallback(
            EditorState& editor, std::string_view closing)
        {
            for(const auto& workspace:kEditorWorkspaceSpecs)
                if(workspace.id!=closing&&editor_workspace_supported(editor,workspace)
                    &&editor_workspace_visible(editor,workspace)) return &workspace;
            for(const auto& workspace:kEditorWorkspaceSpecs)
                if(editor_workspace_supported(editor,workspace))
                { (void)set_editor_workspace_visible(editor,workspace.id,true);return &workspace; }
            return nullptr;
        }

        // The two top-level document tabs reuse the same Canvas2D authoring
        // surface and its persisted GUI-mode bit; there is no second document.
        [[nodiscard]] static bool select_editor_2d_workspace(EditorState& editor, bool guiWorkspace) noexcept
        {
            guiWorkspace = guiWorkspace
                || editor_application_owns_dedicated_gui_workspace(editor.applicationKind);
            if(editor.game2DGuiWorkspace != guiWorkspace) editor.surfaceSettleFrames = 1;
            editor.game2DGuiWorkspace = guiWorkspace;
            reveal_editor_workspace_surface(editor, EditorMainSurface::Game2D);
            editor.outlinerToolTab = guiWorkspace ? OutlinerToolTab::Gui : OutlinerToolTab::TileMap;
            return guiWorkspace;
        }

        [[nodiscard]] static std::string_view main_surface_title(EditorMainSurface surface) noexcept
        {
            switch (surface)
            {
            case EditorMainSurface::Scene:
                return "3D Preview";
            case EditorMainSurface::Game2D:
                return "2D / UI";
            case EditorMainSurface::Assets:
                return "Assets";
            case EditorMainSurface::AssetGeneration:
                return "Asset Generation";
            case EditorMainSurface::Project:
                return "Project";
            case EditorMainSurface::ForestFactory:
                return "Forest Factory";
            case EditorMainSurface::PlantLab:
                return "Plant Lab";
            case EditorMainSurface::Timeline:
                return "Timeline";
            case EditorMainSurface::AISandbox:
                return "Automation";
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
            editor.workspaceVisibilityCustomized=false;
            editor.visibleWorkspaceIds.clear();
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
            std::string projectId{};
            std::string projectRoot{};
            std::string guiDocumentSourcePath{};
            std::string activeWorld{};
            std::vector<EditorEntity> entities{};
            std::shared_ptr<const editor_hierarchy::GuiHierarchyProjection> guiHierarchy{};
            authoring::gui::WidgetHandle guiSelectedWidget{};
            scene::SceneObjectId selectedEntityId{
                scene::kInvalidSceneObjectId};
            std::vector<std::string> logLines{};
            std::vector<std::string> chatLines{};
            std::shared_ptr<const std::vector<gui::ChatMessageRunInfo>> chatRunDetails{};
            std::string aiStatus{};
            bool aiDeferredChatQueued{};
            bool aiDeferredDispatchQueued{};
            bool aiRequestPending{};
            bool automationInputReserved{};
            bool aiDetachedSubmitQueued{};
            core::ScenePreviewMode previewMode{
                core::ScenePreviewMode::Editor};
            std::uint64_t sceneRevision{};
        };

        struct DetachedGuiHierarchySelection final
        {
            const core::Context* ownerContext{};
            std::string projectId{};
            std::string projectRoot{};
            std::string sourcePath{};
            std::shared_ptr<const editor_hierarchy::GuiHierarchyProjection> hierarchy{};
            authoring::gui::WidgetHandle widget{};
        };

        struct EditorStorage
        {
            std::mutex mutex{};
            std::map<const core::Context*, EditorState> states{};
            std::unordered_map<std::string, bool> detachedPaneRoutes{};
            std::unordered_map<std::string, bool> paneRedockRequests{};
            std::vector<DetachedAiChatCommand> detachedAiChatCommands{};
            std::vector<DetachedGuiHierarchySelection> detachedGuiHierarchySelections{};
            DetachedPaneProjection detachedPaneProjection{};
            std::uint64_t detachedProjectionGeneration{};
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
        [[nodiscard]] std::optional<authoring::gui::WidgetHandle>
        selected_gui_widget(const EditorState& state);

        [[nodiscard]] std::shared_ptr<const editor_hierarchy::GuiHierarchyProjection>
        gui_hierarchy_projection(EditorState& editor)
        {
            const auto* document = editor.guiDocument.get();
            if (!document || !document->valid())
            {
                editor.guiHierarchyProjection.reset();
                return {};
            }
            const auto& cached = editor.guiHierarchyProjection;
            if (cached && cached->document == document->document()
                && cached->branch == document->branch()
                && cached->revision == document->revision()
                && cached->root == editor.guiDocumentRoot
                && editor.guiHierarchyProjectId == editor.projectId
                && editor.guiHierarchyProjectRoot == editor.projectRoot
                && editor.guiHierarchySourcePath == editor.guiDocumentSourcePath)
                return cached;
            auto projection = editor_hierarchy::project_gui_hierarchy(
                *document, editor.guiDocumentRoot);
            editor.guiHierarchyProjection = projection.ready
                ? std::make_shared<const editor_hierarchy::GuiHierarchyProjection>(
                    std::move(projection))
                : nullptr;
            editor.guiHierarchyProjectId = editor.projectId;
            editor.guiHierarchyProjectRoot = editor.projectRoot;
            editor.guiHierarchySourcePath = editor.guiDocumentSourcePath;
            return editor.guiHierarchyProjection;
        }

        void publish_detached_pane_projection(
            const core::Context* ownerContext,
            EditorState& editor,
            const AiChat& chat,
            bool* focusedOwnerChanged = nullptr)
        {
            if (focusedOwnerChanged) *focusedOwnerChanged = false;
            if (!ownerContext) return;
            const auto ownsCurrentState = [&](const auto& storage)
            {
                const auto found = storage.states.find(ownerContext);
                return found != storage.states.end()
                    && std::addressof(found->second) == std::addressof(editor);
            };
            // Focus callbacks belong to the native/backend owner, never to the
            // Editor storage lock. Floating panes retain their last canonical
            // owner until another canonical Editor actually gains focus.
            // This runs on EVERY editor frame. A canonical/docked editor must
            // not deep-copy its entire scene and conversation into an unused
            // popout snapshot. In particular, a long chat made this cost grow
            // throughout an otherwise idle editor session.
            bool chatDetached = false;
            bool hierarchyDetached = false;
            bool guiHierarchyDetached = false;
            bool outputDetached = false;
            bool ownerBeforeCopy = false;
            bool noDetachedPanes = false;
            std::uint64_t publicationBeforeCopy{};
            {
                auto& storage = editor_storage();
                std::scoped_lock lock(storage.mutex);
                if (!ownsCurrentState(storage)
                    || storage.detachedProjectionGeneration == (std::numeric_limits<std::uint64_t>::max)())
                    return;
                publicationBeforeCopy = storage.detachedProjectionGeneration;
                noDetachedPanes = storage.detachedPaneRoutes.empty();
                ownerBeforeCopy =
                    storage.detachedPaneProjection.ownerContext == ownerContext;
                for (const auto& [route, detached] : storage.detachedPaneRoutes)
                {
                    if (!detached) continue;
                    if (const auto pane = editor_tool_pane_from_route(route))
                    {
                        chatDetached |= *pane == EditorToolPane::AiChat;
                        guiHierarchyDetached |= *pane == EditorToolPane::GuiHierarchy;
                        hierarchyDetached |= is_outliner_tool_pane(*pane)
                            || is_inspector_tool_pane(*pane);
                        outputDetached |= is_console_tool_pane(*pane);
                    }
                }
            }
            const bool focusedBeforeCopy = ownerContext->has_input_focus_safe();
            if (!noDetachedPanes)
            {
                auto& storage = editor_storage();
                std::scoped_lock lock(storage.mutex);
                // Native focus queries can interleave retirement/replacement.
                // Refuse before touching the caller's old payload or caches.
                if (!ownsCurrentState(storage)) return;
            }
            if (noDetachedPanes)
            {
                auto& storage = editor_storage();
                std::scoped_lock lock(storage.mutex);
                if (!ownsCurrentState(storage)
                    || storage.detachedProjectionGeneration == (std::numeric_limits<std::uint64_t>::max)()
                    || (storage.detachedProjectionGeneration != publicationBeforeCopy
                        && storage.detachedPaneProjection.ownerContext != ownerContext))
                    return;
                if (storage.detachedPaneRoutes.empty()
                    && (focusedBeforeCopy || storage.detachedPaneProjection.ready))
                {
                    // Remember identity only while docked. The first popout
                    // can take focus before the source's next frame; requiring
                    // focus then would leave it without a canonical producer.
                    // No scene, conversation or hierarchy payload is copied.
                    const auto* retainedOwner = focusedBeforeCopy
                        ? ownerContext
                        : storage.detachedPaneProjection.ownerContext;
                    storage.detachedPaneProjection = {
                        .ownerContext = retainedOwner};
                    ++storage.detachedProjectionGeneration;
                }
                editor.detachedChatRunSnapshot.reset();
                return;
            }
            if (!focusedBeforeCopy && !ownerBeforeCopy) return;
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

            authoring::gui::WidgetHandle guiSelection{};
            if (guiHierarchyDetached)
            {
                if (const auto selected = selected_gui_widget(editor))
                    guiSelection = *selected;
            }
            DetachedPaneProjection projection{
                .ready = true,
                .ownerContext = ownerContext,
                .applicationKind = editor.applicationKind,
                .projectName = editor.projectName,
                .projectId = guiHierarchyDetached ? editor.projectId : std::string{},
                .projectRoot = guiHierarchyDetached ? editor.projectRoot : std::string{},
                .guiDocumentSourcePath = guiHierarchyDetached ? editor.guiDocumentSourcePath : std::string{},
                .activeWorld = editor.activeWorld,
                .entities = hierarchyDetached ? editor.entities : std::vector<EditorEntity>{},
                .guiHierarchy = guiHierarchyDetached ? gui_hierarchy_projection(editor) : nullptr,
                .guiSelectedWidget = guiSelection,
                .selectedEntityId = editor.selectedEntityId,
                .logLines = outputDetached ? editor.logLines : std::vector<std::string>{},
                .chatLines = chatDetached ? chat.lines : std::vector<std::string>{},
                .chatRunDetails = editor.detachedChatRunSnapshot,
                .aiStatus = chatDetached ? editor.aiAuthoringStatus : std::string{},
                .aiDeferredChatQueued = chatDetached && editor.aiDeferredRequestKind
                    == AiDeferredRequestKind::Chat,
                .aiDeferredDispatchQueued = chatDetached && editor.aiDeferredDispatchQueued,
                .aiRequestPending = chatDetached && static_cast<bool>(chat.pending),
                .automationInputReserved = editor_automation_input_reserved(editor, chat),
                .previewMode = editor.previewMode,
                .sceneRevision = editor.sceneDocumentRevision
            };
            const bool focusedBeforePublish = ownerContext->has_input_focus_safe();
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            // A newer publisher wins even if the last native focus result is
            // now stale. Owner equality alone misses a foreign owner's refresh.
            if (!ownsCurrentState(storage)
                || storage.detachedProjectionGeneration == (std::numeric_limits<std::uint64_t>::max)()
                || (storage.detachedProjectionGeneration != publicationBeforeCopy
                    && storage.detachedPaneProjection.ownerContext != ownerContext))
                return;
            // A window can be re-docked between the first lock and publishing.
            if (storage.detachedPaneRoutes.empty())
            {
                storage.detachedPaneProjection = {};
                ++storage.detachedProjectionGeneration;
                return;
            }
            if (!focusedBeforePublish
                && storage.detachedPaneProjection.ownerContext != ownerContext)
                return;
            projection.aiDetachedSubmitQueued = chatDetached && std::any_of(
                storage.detachedAiChatCommands.begin(),
                storage.detachedAiChatCommands.end(),
                [ownerContext](const DetachedAiChatCommand& command)
                {
                    return command.ownerContext == ownerContext
                        && command.kind == DetachedAiChatCommandKind::submit;
                });
            const bool committedFocusedOwnerChange = focusedBeforePublish
                && storage.detachedPaneProjection.ownerContext != ownerContext;
            storage.detachedPaneProjection = std::move(projection);
            ++storage.detachedProjectionGeneration;
            // Publish this edge only after exact-state and generation admission.
            // Popout focus and ordinary payload refreshes cannot retrigger it.
            if (focusedOwnerChanged) *focusedOwnerChanged = committedFocusedOwnerChange;
        }

        [[nodiscard]] DetachedPaneProjection
            detached_pane_projection()
        {
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            return storage.detachedPaneProjection;
        }

        [[nodiscard]] bool queue_detached_gui_hierarchy_selection(
            const DetachedPaneProjection& projection,
            authoring::gui::WidgetHandle widget)
        {
            if (!projection.ready || !projection.ownerContext || !projection.guiHierarchy
                || !projection.guiHierarchy->ready || !widget)
                return false;
            const auto& rows = projection.guiHierarchy->rows;
            if (std::none_of(rows.begin(), rows.end(),
                    [widget](const auto& row) { return row.handle == widget; }))
                return false;
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto& current = storage.detachedPaneProjection;
            if (!storage.states.contains(projection.ownerContext)
                || !storage.detachedPaneRoutes.contains("pane.gui_hierarchy")
                || !current.ready || current.automationInputReserved || current.ownerContext != projection.ownerContext
                || current.guiHierarchy != projection.guiHierarchy
                || current.projectId != projection.projectId
                || current.projectRoot != projection.projectRoot
                || current.guiDocumentSourcePath != projection.guiDocumentSourcePath)
                return false;
            // Selection supersedes an older click for the same owner; the mailbox
            // remains bounded and no mutable document crosses context threads.
            std::erase_if(storage.detachedGuiHierarchySelections,
                [&](const auto& command) { return command.ownerContext == projection.ownerContext; });
            if (storage.detachedGuiHierarchySelections.size() >= 16u) return false;
            storage.detachedGuiHierarchySelections.push_back({projection.ownerContext,
                projection.projectId, projection.projectRoot, projection.guiDocumentSourcePath,
                projection.guiHierarchy, widget});
            return true;
        }

        [[nodiscard]] std::vector<DetachedGuiHierarchySelection>
        consume_detached_gui_hierarchy_selections(const core::Context* ownerContext)
        {
            std::vector<DetachedGuiHierarchySelection> commands{};
            if (!ownerContext) return commands;
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto& current = storage.detachedPaneProjection;
            for (auto it = storage.detachedGuiHierarchySelections.begin();
                 it != storage.detachedGuiHierarchySelections.end();)
            {
                if (it->ownerContext != ownerContext) { ++it; continue; }
                if (storage.states.contains(ownerContext)
                    && storage.detachedPaneRoutes.contains("pane.gui_hierarchy")
                    && current.ready && !current.automationInputReserved && current.ownerContext == ownerContext
                    && current.guiHierarchy == it->hierarchy
                    && current.projectId == it->projectId
                    && current.projectRoot == it->projectRoot
                    && current.guiDocumentSourcePath == it->sourcePath)
                    commands.push_back(std::move(*it));
                it = storage.detachedGuiHierarchySelections.erase(it);
            }
            return commands;
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
            ++state.outputLogRevision;
            constexpr std::size_t kMaxLogLines = 512;
            if (state.logLines.size() > kMaxLogLines)
                state.logLines.erase(state.logLines.begin(), state.logLines.begin() + static_cast<std::ptrdiff_t>(state.logLines.size() - kMaxLogLines));
        }

        [[nodiscard]] static bool editor_output_contains(std::string_view line,
            std::string_view query) noexcept
        {
            const auto fold = [](unsigned char value) noexcept
            { return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value; };
            return query.empty() || std::search(line.begin(), line.end(), query.begin(), query.end(),
                [&](unsigned char left, unsigned char right) { return fold(left) == fold(right); }) != line.end();
        }

        [[nodiscard]] static EditorWorkspaceTab editor_output_category(std::string_view line) noexcept
        {
            if (!line.starts_with('[')) return EditorWorkspaceTab::Output;
            const auto end = line.find(']');
            if (end == line.npos || end > 64u) return EditorWorkspaceTab::Output;
            const auto tag = line.substr(1u, end - 1u);
            const auto matches = [&](std::string_view value)
            { return tag.size() == value.size() && editor_output_contains(tag, value); };
            for (const auto value : {"project", "build", "script", "run", "runtime", "scene", "world", "edit", "save", "load", "undo", "redo", "tilemap", "gui"})
                if (matches(value)) return EditorWorkspaceTab::Project;
            for (const auto value : {"asset", "assets", "texture", "material", "mesh", "import", "forest", "plant", "planet", "planetary", "package", "packages"})
                if (matches(value)) return EditorWorkspaceTab::Assets;
            for (const auto value : {"ai", "assistant", "automation", "engine-mission", "model", "models", "native", "codex", "source"})
                if (matches(value)) return EditorWorkspaceTab::AI;
            for (const auto value : {"systems", "system", "context", "renderer", "render", "performance", "fps", "time", "input", "rtt", "diagnostics"})
                if (matches(value)) return EditorWorkspaceTab::Systems;
            return EditorWorkspaceTab::Output; // General is the exact complement of the named categories.
        }

        [[nodiscard]] static const std::vector<std::string>& filtered_editor_output(EditorState& state)
        {
            if (state.outputFilterRevision == state.outputLogRevision
                && state.outputFilterTab == state.dockStatusTab
                && state.outputFilterGeneral == state.outputGeneralOnly
                && state.outputFilterSearch == state.outputSearch)
                return state.outputFilteredLines;
            state.outputFilteredLines.clear();
            for (const auto& line : state.logLines)
            {
                const auto category = editor_output_category(line);
                const bool selected = state.outputGeneralOnly ? category == EditorWorkspaceTab::Output
                    : state.dockStatusTab == EditorWorkspaceTab::Output || category == state.dockStatusTab;
                if (selected && editor_output_contains(line, state.outputSearch))
                    state.outputFilteredLines.push_back(line);
            }
            state.outputFilterRevision = state.outputLogRevision;
            state.outputFilterTab = state.dockStatusTab;
            state.outputFilterGeneral = state.outputGeneralOnly;
            state.outputFilterSearch = state.outputSearch;
            return state.outputFilteredLines;
        }

        static void record_editor_run_output(EditorState& state, std::string_view detail)
        {
            if (detail == state.outputLastRunDetail) return;
            state.outputLastRunDetail.assign(detail);
            if (!detail.empty()) push_editor_log(state, "[run] " + std::string{detail});
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
