// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        {
            undo,
            redo,
            duplicate_selection,
            delete_selection,
            focus_selection
        };

        struct SurfaceEditAvailability final
        {
            bool enabled{};
            editor_workspace_commands::CommandId command{
                editor_workspace_commands::CommandId::world_select_entity};
            std::string reason{};
            std::string operation_label{};
            std::optional<editor_authoring_history::HistoryPlan> history{};
        };

        [[nodiscard]] bool editor_project_viewport_covered(const EditorState& state) noexcept
        {
            // These IDs belong to the admitted child lease; detach clears them.
            return state.mainSurface == EditorMainSurface::Scene
                && state.projectRuntimeInEditor && state.projectRuntimeProcess.valid()
                && state.projectRuntimeWindow != 0u && state.projectRuntimeWindowProcess != 0u
                && state.projectRuntimeAttachmentError.empty();
        }

        [[nodiscard]] bool editor_project_viewport_input_reserved(const EditorState& state) noexcept
        {
            // A build reads the authored scene; a live project owns its runtime
            // copy. Keep authoring stable until the build or owned process retires,
            // including the interval before a hosted window is attached.
            return state.projectRuntimeProcess.valid() || state.projectBuildPending.has_value();
        }

        [[nodiscard]] constexpr std::string_view surface_edit_action_name(
            SurfaceEditAction action) noexcept
        {
            switch (action)
            {
            case SurfaceEditAction::undo: return "Undo";
            case SurfaceEditAction::redo: return "Redo";
            case SurfaceEditAction::duplicate_selection: return "Duplicate";
            case SurfaceEditAction::delete_selection: return "Delete";
            case SurfaceEditAction::focus_selection: return "Focus";
            }
            return "Edit command";
        }

        [[nodiscard]] editor_workspace_commands::Surface workspace_command_surface(
            const EditorState& state) noexcept
        {
            using Surface = editor_workspace_commands::Surface;
            if (state.mainSurface == EditorMainSurface::Assets
                && state.assetWorkspaceSection == 2u
                && state.projectCodeWorkspace.controller.snapshot().configured)
            {
                return Surface::code_editor;
            }
            switch (state.mainSurface)
            {
            case EditorMainSurface::Scene: return Surface::world;
            case EditorMainSurface::Game2D:
                return state.game2DGuiWorkspace
                    ? Surface::gui_canvas
                    : Surface::tilemap_canvas;
            case EditorMainSurface::Assets: return Surface::assets;
            case EditorMainSurface::Project: return Surface::project;
            case EditorMainSurface::ForestFactory: return Surface::forest_factory;
            case EditorMainSurface::Timeline: return Surface::timeline;
            case EditorMainSurface::AISandbox: return Surface::ai_development;
            case EditorMainSurface::Systems: return Surface::systems;
            case EditorMainSurface::PlantLab: return Surface::plant_lab;
            case EditorMainSurface::Count: break;
            }
            return Surface::systems;
        }

        [[nodiscard]] std::optional<editor_workspace_commands::CommandId>
        command_for_surface_edit(
            editor_workspace_commands::Surface surface,
            SurfaceEditAction action) noexcept
        {
            using CommandId = editor_workspace_commands::CommandId;
            using Surface = editor_workspace_commands::Surface;
            if (surface == Surface::world)
            {
                switch (action)
                {
                case SurfaceEditAction::undo: return CommandId::world_undo;
                case SurfaceEditAction::redo: return CommandId::world_redo;
                case SurfaceEditAction::duplicate_selection: return CommandId::world_duplicate_entity;
                case SurfaceEditAction::delete_selection: return CommandId::world_delete_entity;
                case SurfaceEditAction::focus_selection: return CommandId::world_focus_entity;
                }
            }
            if (surface == Surface::gui_canvas)
            {
                switch (action)
                {
                case SurfaceEditAction::undo: return CommandId::gui_undo;
                case SurfaceEditAction::redo: return CommandId::gui_redo;
                case SurfaceEditAction::duplicate_selection: return CommandId::gui_duplicate_element;
                case SurfaceEditAction::delete_selection: return CommandId::gui_delete_element;
                case SurfaceEditAction::focus_selection: return CommandId::gui_focus_element;
                }
            }
            if (surface == Surface::plant_lab)
            {
                if (action == SurfaceEditAction::undo) return CommandId::plant_undo;
                if (action == SurfaceEditAction::redo) return CommandId::plant_redo;
                if (action == SurfaceEditAction::focus_selection)
                    return CommandId::plant_focus_element;
            }
            if (surface == Surface::tilemap_canvas)
            {
                if (action == SurfaceEditAction::undo) return CommandId::tilemap_undo;
                if (action == SurfaceEditAction::redo) return CommandId::tilemap_redo;
            }
            if (surface == Surface::code_editor)
            {
                if (action == SurfaceEditAction::undo)
                    return CommandId::code_undo;
                if (action == SurfaceEditAction::redo)
                    return CommandId::code_redo;
            }
            if (surface == Surface::forest_factory
                && action == SurfaceEditAction::focus_selection)
                return CommandId::forest_focus_element;
            return std::nullopt;
        }

        [[nodiscard]] std::optional<editor_workspace_commands::Surface>
        selected_edit_surface(const EditorState& state) noexcept
        {
            using Surface = editor_workspace_commands::Surface;
            if (!editor_entity_index(state, state.selectedEntityId))
                return std::nullopt;
            if (state.guiDocumentWidgets.contains(state.selectedEntityId))
                return Surface::gui_canvas;
            return Surface::world;
        }

        [[nodiscard]] editor_workspace_commands::Surface
        edit_surface_for_action(
            const EditorState& state,
            SurfaceEditAction action) noexcept
        {
            using Surface = editor_workspace_commands::Surface;
            if (action == SurfaceEditAction::duplicate_selection
                || action == SurfaceEditAction::delete_selection
                || action == SurfaceEditAction::focus_selection)
            {
                if (const auto selected = selected_edit_surface(state))
                    return *selected;
            }

            const Surface visible = workspace_command_surface(state);
            if (command_for_surface_edit(visible, action))
                return visible;

            // Global Edit/Asset commands remain useful while inspecting
            // Project, Assets, Systems, Timeline, or AI workspaces. Those
            // workspaces do not become accidental edit documents; commands
            // route to the canonical World document unless a selected GUI
            // element identifies the GUI document explicitly.
            return Surface::world;
        }

        inline constexpr editor_authoring_history::BranchId
            kWorldHistoryBranch{1u};
        inline constexpr editor_authoring_history::BranchId
            kGuiHistoryBranch{2u};
        inline constexpr editor_authoring_history::BranchId
            kPlantHistoryBranch{3u};
        inline constexpr editor_authoring_history::BranchId
            kTileMapHistoryBranch{4u};
        inline constexpr std::uint64_t kCodeHistoryBranchMask =
            0x8000000000000000ull;

        [[nodiscard]] editor_authoring_history::BranchId code_history_branch(
            const editor_code_workspace::DocumentHandle handle) noexcept
        {
            return editor_authoring_history::BranchId{
                kCodeHistoryBranchMask
                    | (static_cast<std::uint64_t>(handle.generation) << 32u)
                    | (static_cast<std::uint64_t>(handle.index) + 1u)};
        }

        [[nodiscard]] std::string plant_history_label(
            const forest::ForestProfileProperty property)
        {
            using Property = forest::ForestProfileProperty;
            switch (property)
            {
            case Property::TargetHeightMeters: return "Change plant height";
            case Property::TrunkRadiusMeters: return "Change trunk radius";
            case Property::BranchLevels: return "Change branch levels";
            case Property::ChildrenPerNode: return "Change child branches";
            case Property::BranchStartHeightMeters: return "Change branch start";
            case Property::BranchLengthMeters: return "Change branch length";
            case Property::BranchAngleDegrees: return "Change branch angle";
            case Property::BranchSpreadDegrees: return "Change branch spread";
            case Property::GrowthDurationSeconds: return "Change growth duration";
            case Property::GrowthTimeSeconds: return "Change growth time";
            case Property::LeafSizeMeters: return "Change leaf size";
            case Property::LeafSitesPerBranch: return "Change leaf count";
            case Property::PreviewMode: return "Change plant dimension";
            case Property::BarkTileIndex: return "Change bark style";
            case Property::BarkTexture: return "Change bark texture";
            case Property::FoliageTexture: return "Change foliage texture";
            }
            return "Change plant property";
        }

        [[nodiscard]] std::optional<editor_authoring_history::BranchSnapshot>
        tilemap_authoring_history_branch(const EditorState& state)
        {
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
            if (state.tileMapWorkspace && state.tileMapWorkspace->has_document())
            {
                const auto& document = *state.tileMapWorkspace->document();
                const auto handle = document.handle();
                const auto metrics = document.metrics();
                const auto cursor = static_cast<std::size_t>(metrics.applied_operation_count);
                const auto retained = static_cast<std::size_t>(metrics.operation_count);
                // Read bounded journal metadata, not a deep copy of every cell
                // delta in history. Bind the current document handle/generation.
                return editor_authoring_history::BranchSnapshot{
                    .id = kTileMapHistoryBranch,
                    .kind = editor_authoring_history::BranchKind::tilemap_document,
                    .generation = (static_cast<std::uint64_t>(handle.generation) << 32u)
                        | (static_cast<std::uint64_t>(handle.index) + 1u),
                    .revision = (std::max)(std::uint64_t{1u}, document.revision().sequence),
                    .cursor = cursor,
                    .retained_entries = retained,
                    .label = "Tile Map",
                    .undo_label = cursor == 0u ? std::string{} : "Tile Map edit",
                    .redo_label = cursor >= retained ? std::string{} : "Tile Map edit",
                    .available = true,
                    .writable = true};
            }
#else
            (void)state;
#endif
            return std::nullopt;
        }

        [[nodiscard]] std::vector<editor_authoring_history::BranchSnapshot>
        authoring_history_branches(const EditorState& state)
        {
            using BranchKind = editor_authoring_history::BranchKind;
            using BranchSnapshot = editor_authoring_history::BranchSnapshot;
            std::vector<BranchSnapshot> branches{};

            if (state.sceneDocument.initialized())
            {
                const auto history = state.sceneDocument.history();
                const std::size_t cursor = static_cast<std::size_t>(
                    std::count_if(
                        history.begin(), history.end(),
                        [](const auto& entry)
                        {
                            return entry.currently_applied;
                        }));
                branches.push_back(BranchSnapshot{
                    .id = kWorldHistoryBranch,
                    .kind = BranchKind::world_document,
                    .generation = (std::max)(
                        std::uint64_t{1u},
                        state.sceneDocument.identity().scene.value),
                    .revision = (std::max)(
                        std::uint64_t{1u},
                        state.sceneDocument.revision().sequence),
                    .cursor = cursor,
                    .retained_entries = history.size(),
                    .label = "World",
                    .undo_label = cursor == 0u
                        ? std::string{}
                        : history[cursor - 1u].label,
                    .redo_label = cursor >= history.size()
                        ? std::string{}
                        : history[cursor].label,
                    .available = true,
                    .writable = true});
            }

            if (state.guiDocument && state.guiDocument->valid())
            {
                const auto journal = state.guiDocument->journal();
                const std::size_t cursor = static_cast<std::size_t>(
                    std::count_if(
                        journal.begin(), journal.end(),
                        [](const auto& entry)
                        {
                            return entry.applied;
                        }));
                const auto label_at = [&journal](const std::size_t index)
                {
                    return index < journal.size()
                        ? epochengine::format_text(
                            "GUI {}",
                            authoring::gui::operation_kind_name(
                                journal[index].kind()))
                        : std::string{};
                };
                const auto identity = state.guiDocument->branch();
                branches.push_back(BranchSnapshot{
                    .id = kGuiHistoryBranch,
                    .kind = BranchKind::gui_document,
                    .generation = (std::max)(
                        std::uint64_t{1u},
                        identity.timeline ^ identity.branch),
                    .revision = (std::max)(
                        std::uint64_t{1u},
                        state.guiDocument->revision().sequence),
                    .cursor = cursor,
                    .retained_entries = journal.size(),
                    .label = "GUI Canvas",
                    .undo_label = cursor == 0u
                        ? std::string{}
                        : label_at(cursor - 1u),
                    .redo_label = label_at(cursor),
                    .available = true,
                    .writable = true});
            }

            if (state.plantLabDocumentInitialized)
            {
                const auto& document = state.plantLabDocument;
                branches.push_back(BranchSnapshot{
                    .id = kPlantHistoryBranch,
                    .kind = BranchKind::plant_document,
                    .generation = (std::max)(
                        std::uint64_t{1u},
                        document.genome.stableHash),
                    .revision = (std::max)(
                        std::uint64_t{1u},
                        document.revision),
                    .cursor = document.historyCursor,
                    .retained_entries = document.journal.size(),
                    .label = "Plant Lab",
                    .undo_label = document.historyCursor == 0u
                        ? std::string{}
                        : plant_history_label(
                            document.journal[
                                document.historyCursor - 1u].property),
                    .redo_label =
                        document.historyCursor >= document.journal.size()
                        ? std::string{}
                        : plant_history_label(
                            document.journal[
                                document.historyCursor].property),
                    .available = true,
                    .writable = true});
            }

            if (const auto tileMap = tilemap_authoring_history_branch(state))
                branches.push_back(*tileMap);

            const auto code =
                state.projectCodeWorkspace.controller.snapshot();
            if (code.configured)
            {
                for (const auto& document : code.documents)
                {
                    branches.push_back(BranchSnapshot{
                        .id = code_history_branch(document.handle),
                        .kind = BranchKind::code_document,
                        .generation = document.handle.generation,
                        .revision = (std::max)(
                            std::uint64_t{1u},
                            document.revision),
                        .cursor = document.history_cursor,
                        .retained_entries = document.history_entries,
                        .label = document.label.empty()
                            ? document.relative_path
                            : document.label,
                        .undo_label = document.undo_label,
                        .redo_label = document.redo_label,
                        .available = true,
                        .writable = document.writable});
                }
            }
            return branches;
        }

        [[nodiscard]] std::optional<editor_authoring_history::BranchId>
        visible_authoring_history_branch(const EditorState& state)
        {
            if (state.mainSurface == EditorMainSurface::Scene)
                return kWorldHistoryBranch;
            if (state.mainSurface == EditorMainSurface::Game2D
                && state.game2DGuiWorkspace)
            {
                return kGuiHistoryBranch;
            }
            if (state.mainSurface == EditorMainSurface::Game2D
                && !state.game2DGuiWorkspace)
            {
                return kTileMapHistoryBranch;
            }
            if (state.mainSurface == EditorMainSurface::PlantLab
                || state.mainSurface == EditorMainSurface::ForestFactory)
                return kPlantHistoryBranch;
            if (state.mainSurface == EditorMainSurface::Assets
                && state.assetWorkspaceSection == 2u)
            {
                const auto active =
                    state.projectCodeWorkspace.controller.active_document();
                if (active)
                    return code_history_branch(active->handle);
            }
            return std::nullopt;
        }

        void synchronize_authoring_history(EditorState& state)
        {
            const auto branches = authoring_history_branches(state);
            (void)state.authoringHistory.synchronize(branches);
            if (const auto visible = visible_authoring_history_branch(state))
                (void)state.authoringHistory.activate(*visible);
        }

        [[nodiscard]] std::optional<editor_code_workspace::DocumentSnapshot>
        code_document_for_history(
            const EditorState& state,
            const editor_authoring_history::BranchId branch)
        {
            const auto workspace =
                state.projectCodeWorkspace.controller.snapshot();
            const auto found = std::find_if(
                workspace.documents.begin(),
                workspace.documents.end(),
                [branch](const auto& document)
                {
                    return code_history_branch(document.handle) == branch;
                });
            return found == workspace.documents.end()
                ? std::nullopt
                : std::optional{*found};
        }

        void finish_authoring_history_transition(
            EditorState& state,
            const editor_authoring_history::HistoryPlan& plan)
        {
            const auto branches = authoring_history_branches(state);
            const auto found = std::find_if(
                branches.begin(),
                branches.end(),
                [&plan](const auto& branch)
                {
                    return branch.id == plan.branch;
                });
            if (found == branches.end()
                || state.authoringHistory.accept(plan, *found)
                    != editor_authoring_history::PlanCode::ready)
            {
                (void)state.authoringHistory.synchronize(branches);
            }
        }

        [[nodiscard]] SurfaceEditAvailability resolve_surface_edit(
            EditorState& state,
            SurfaceEditAction action)
        {
            using namespace editor_workspace_commands;
            SurfaceEditAvailability result{};
            if (action == SurfaceEditAction::undo
                || action == SurfaceEditAction::redo)
            {
                synchronize_authoring_history(state);
                const auto direction = action == SurfaceEditAction::undo
                    ? editor_authoring_history::Direction::undo
                    : editor_authoring_history::Direction::redo;
                const auto plan = state.authoringHistory.plan(direction);
                if (!plan)
                {
                    result.reason = action == SurfaceEditAction::undo
                        ? "No editor document has an edit to undo."
                        : "No editor document has an edit to redo.";
                    return result;
                }
                if (plan.kind == editor_authoring_history::BranchKind::world_document
                    && editor_project_viewport_input_reserved(state))
                {
                    result.reason = "Scene editing is paused while the project builds or runs. Stop it before editing.";
                    return result;
                }
                result.history = plan;
                result.operation_label = plan.operation_label;
                switch (plan.kind)
                {
                case editor_authoring_history::BranchKind::world_document:
                    result.command = action == SurfaceEditAction::undo
                        ? CommandId::world_undo : CommandId::world_redo;
                    break;
                case editor_authoring_history::BranchKind::gui_document:
                    result.command = action == SurfaceEditAction::undo
                        ? CommandId::gui_undo : CommandId::gui_redo;
                    break;
                case editor_authoring_history::BranchKind::plant_document:
                    result.command = action == SurfaceEditAction::undo
                        ? CommandId::plant_undo : CommandId::plant_redo;
                    break;
                case editor_authoring_history::BranchKind::tilemap_document:
                    result.command = action == SurfaceEditAction::undo
                        ? CommandId::tilemap_undo : CommandId::tilemap_redo;
                    break;
                case editor_authoring_history::BranchKind::code_document:
                    result.command = action == SurfaceEditAction::undo
                        ? CommandId::code_undo : CommandId::code_redo;
                    break;
                default:
                    result.reason =
                        "The editor history branch has no executor yet.";
                    return result;
                }
                result.enabled = true;
                return result;
            }
            const Surface surface = edit_surface_for_action(state, action);
            if (surface == Surface::world && editor_project_viewport_input_reserved(state))
            {
                result.reason = "Scene editing is paused while the project builds or runs. Stop it before editing.";
                return result;
            }
            const auto command = command_for_surface_edit(surface, action);
            if (!command)
            {
                result.reason = epochengine::format_text(
                    "{} has no {} command.",
                    surface_name(surface), surface_edit_action_name(action));
                return result;
            }
            result.command = *command;
            const auto* descriptor = find_command(*command);
            if (!descriptor || descriptor->surface != surface)
            {
                result.reason =
                    "Command is absent from the editor command catalog.";
                return result;
            }
            if (descriptor->target == TargetKind::entity
                && !editor_entity_index(state, state.selectedEntityId))
            {
                result.reason =
                    "Select a World entity first; no document changed.";
                return result;
            }
            if (descriptor->target == TargetKind::element)
            {
                if (surface == Surface::gui_canvas)
                {
                    if (!selected_gui_widget(state))
                    {
                        result.reason =
                            "Select a GUI element first.";
                        return result;
                    }
                }
                else if (!editor_entity_index(state, state.selectedEntityId))
                {
                    result.reason =
                        "Select an element first.";
                    return result;
                }
            }
            if (*command == CommandId::world_undo && !state.sceneDocument.can_undo())
                result.reason = "World has no edit to undo.";
            else if (*command == CommandId::world_redo && !state.sceneDocument.can_redo())
                result.reason = "World has no edit to redo.";
            else if (*command == CommandId::gui_undo
                && (!state.guiDocument || !state.guiDocument->can_undo()))
                result.reason = "GUI Canvas has no edit to undo.";
            else if (*command == CommandId::gui_redo
                && (!state.guiDocument || !state.guiDocument->can_redo()))
                result.reason = "GUI Canvas has no edit to redo.";
            else if (*command == CommandId::plant_undo
                && state.plantLabDocument.historyCursor == 0u)
                result.reason = "Plant Lab has no edit to undo.";
            else if (*command == CommandId::plant_redo
                && state.plantLabDocument.historyCursor
                    >= state.plantLabDocument.journal.size())
                result.reason = "Plant Lab has no edit to redo.";
            else
                result.enabled = true;
            return result;
        }

        void handle_scene_tool(
            EditorState& state,
            const core::Context* ctx,
            std::string_view toolId);

        [[nodiscard]] bool execute_surface_edit(
            EditorState& state,
            const core::Context* ctx,
            SurfaceEditAction action)
        {
            using CommandId = editor_workspace_commands::CommandId;
            const SurfaceEditAvailability available = resolve_surface_edit(state, action);
            if (!available.enabled)
            {
                push_editor_log(state, "[edit] " + available.reason);
                return false;
            }
            switch (available.command)
            {
            case CommandId::world_undo:
                if (undo_editor_scene(state))
                {
                    if (available.history)
                        finish_authoring_history_transition(
                            state, *available.history);
                    push_editor_log(state, "[edit] World undo committed.");
                    return true;
                }
                return false;
            case CommandId::gui_undo:
                if (undo_gui_document(state))
                {
                    if (available.history)
                        finish_authoring_history_transition(
                            state, *available.history);
                    return true;
                }
                return false;
            case CommandId::plant_undo:
                if (forest::undo_forest_profile_edit(state.plantLabDocument))
                {
                    state.plantLabCompiledRevision = 0u;
                    state.plantLabCompiledContentHash = 0u;
                    if (available.history)
                        finish_authoring_history_transition(
                            state, *available.history);
                    push_editor_log(state, "[edit] Plant Lab undo committed.");
                    return true;
                }
                return false;
            case CommandId::world_redo:
                if (redo_editor_scene(state))
                {
                    if (available.history)
                        finish_authoring_history_transition(
                            state, *available.history);
                    push_editor_log(state, "[edit] World redo committed.");
                    return true;
                }
                return false;
            case CommandId::gui_redo:
                if (redo_gui_document(state))
                {
                    if (available.history)
                        finish_authoring_history_transition(
                            state, *available.history);
                    return true;
                }
                return false;
            case CommandId::plant_redo:
                if (forest::redo_forest_profile_edit(state.plantLabDocument))
                {
                    state.plantLabCompiledRevision = 0u;
                    state.plantLabCompiledContentHash = 0u;
                    if (available.history)
                        finish_authoring_history_transition(
                            state, *available.history);
                    push_editor_log(state, "[edit] Plant Lab redo committed.");
                    return true;
                }
                return false;
            case CommandId::tilemap_undo:
            case CommandId::tilemap_redo:
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
                if (state.tileMapWorkspace && state.tileMapWorkspace->has_document())
                {
                    const bool changed = static_cast<bool>(available.command == CommandId::tilemap_undo
                        ? state.tileMapWorkspace->undo() : state.tileMapWorkspace->redo());
                    state.tileMapStatus = std::string(state.tileMapWorkspace->status());
                    if (changed && available.history)
                        finish_authoring_history_transition(state, *available.history);
                    push_editor_log(state, "[edit] " + state.tileMapStatus);
                    return changed;
                }
#endif
                return false;
            case CommandId::code_undo:
            case CommandId::code_redo:
                if (available.history)
                {
                    const auto document = code_document_for_history(
                        state, available.history->branch);
                    if (!document)
                        break;
                    auto& workspace = state.projectCodeWorkspace;
                    const auto changed =
                        available.command == CommandId::code_undo
                        ? workspace.controller.undo(
                            document->handle,
                            workspace.authority,
                            document->revision)
                        : workspace.controller.redo(
                            document->handle,
                            workspace.authority,
                            document->revision);
                    workspace.status = changed.reason;
                    if (!changed)
                        return false;
                    workspace.draftHandle.reset();
                    finish_authoring_history_transition(
                        state, *available.history);
                    push_editor_log(
                        state,
                        "[edit] " + changed.reason);
                    return true;
                }
                return false;
            case CommandId::world_duplicate_entity:
                duplicate_selected_entity(state); return true;
            case CommandId::world_delete_entity:
                delete_selected_entity(state); return true;
            case CommandId::gui_duplicate_element:
                if (auto* document = synchronize_gui_document(state))
                    return duplicate_selected_gui_widget(state, *document);
                break;
            case CommandId::gui_delete_element:
                if (auto* document = synchronize_gui_document(state))
                    return remove_selected_gui_widget(state, *document);
                break;
            case CommandId::world_focus_entity:
            case CommandId::gui_focus_element:
            case CommandId::forest_focus_element:
            case CommandId::plant_focus_element:
                handle_scene_tool(state, ctx, "focus_selection"); return true;
            default: break;
            }
            push_editor_log(state,
                "[edit] Routed editor command had no executor.");
            return false;
        }


        [[nodiscard]] std::optional<epochengine::ray::RayDesc> screen_point_to_world_ray(
            const core::Context* ctx,
            const gui::WidgetBounds& viewport,
            const gui::Vec2& mouse,
            const std::array<double,3>& coordinateOrigin = {}) noexcept
        {
            if (!ctx || viewport.size.x <= 1.0f || viewport.size.y <= 1.0f)
                return std::nullopt;

            const auto camera = epochengine::previewgrid::camera_for(ctx);
            const auto absoluteEye=epochengine::previewgrid::camera_world_eye(camera);
            std::array<float,3> localEye{};
            for(std::size_t axis=0;axis<3u;++axis)
            {
                const auto value=absoluteEye[axis]-coordinateOrigin[axis];
                if(!std::isfinite(value)||std::abs(value)>(std::numeric_limits<float>::max)())return {};
                localEye[axis]=static_cast<float>(value);
            }
            const epochengine::render_math::Float3 eye{localEye[0],localEye[1],localEye[2]};
            const auto direction=epochengine::previewgrid::camera_forward(camera);
            const epochengine::render_math::Float3 forward{direction.x,direction.y,direction.z};
            const epochengine::scene_interaction::ScreenPoint screen{ mouse.x, mouse.y };
            const epochengine::scene_interaction::ScreenViewport screenViewport{
                viewport.position.x,
                viewport.position.y,
                viewport.size.x,
                viewport.size.y
            };
            const epochengine::scene_interaction::CameraFrame cameraFrame{
                eye,
                forward,
                { camera.up.x, camera.up.y, camera.up.z }
            };
            if (camera.projection == epochengine::render_camera::ProjectionKind::orthographic)
            {
                return epochengine::scene_interaction::orthographic_screen_ray(
                    screen,
                    screenViewport,
                    cameraFrame,
                    {
                        camera.orthographicVerticalSize,
                        camera.nearPlane,
                        camera.farPlane
                    },
                    1u);
            }
            return epochengine::scene_interaction::perspective_screen_ray(
                screen,
                screenViewport,
                cameraFrame,
                {
                    camera.fovRadians,
                    camera.nearPlane,
                    camera.farPlane
                },
                1u);
        }

        [[nodiscard]] std::optional<std::array<double,3>> forest_brush_surface_direction(
            const std::array<double,3>& origin,const std::array<double,3>& direction,double radius) noexcept
        {
            if(!std::isfinite(radius)||radius<=0)return {};
            double a{},b{},c=-radius*radius;
            for(std::size_t axis=0;axis<3;++axis)
            {a+=direction[axis]*direction[axis];b+=origin[axis]*direction[axis];c+=origin[axis]*origin[axis];}
            const double discriminant=b*b-a*c;
            if(!(a>0)||!std::isfinite(discriminant)||discriminant<0)return {};
            double distance=(-b-std::sqrt(discriminant))/a;
            if(distance<0)distance=(-b+std::sqrt(discriminant))/a;
            if(!std::isfinite(distance)||distance<0)return {};
            std::array<double,3> center{};double square{};
            for(std::size_t axis=0;axis<3;++axis)
            {center[axis]=origin[axis]+direction[axis]*distance;square+=center[axis]*center[axis];}
            if(!(square>0)||!std::isfinite(square))return {};
            for(auto& value:center)value/=std::sqrt(square);
            return center;
        }

        // Collect the gesture while the pointer is down. One verified provider
        // request and one undoable scene transaction commit the released stroke.
        void update_forest_brush(const std::shared_ptr<core::Context>& ctx,
            EditorState& state, EditorFrameResult& frame, bool modalVisible)
        {
            const bool held=ctx->is_mouse_button_held_safe(input::MouseButton::MouseLeft);
            const bool pressed=held&&!state.forestBrushHeld;
            state.forestBrushHeld=held;
            if(state.mainSurface!=EditorMainSurface::ForestFactory||!state.forestTerrainPainting
                ||modalVisible||!ctx->has_input_focus_safe()||state.projectBuildPending||state.projectRuntimeProcess.valid()
                ||state.forestSurfacePending
                ||ctx->is_mouse_button_held_safe(input::MouseButton::MouseRight)
                ||ctx->is_mouse_button_held_safe(input::MouseButton::MouseMiddle)
                ||ctx->is_key_held_safe(input::Key::LeftAlt)||ctx->is_key_held_safe(input::Key::RightAlt))
            {state.forestBrushTerrain=scene::kInvalidSceneObjectId;state.forestBrushDirections.clear();return;}
            if(!held)
            {
                const auto selected=editor_entity_index(state,state.forestBrushTerrain);
                state.forestBrushTerrain=scene::kInvalidSceneObjectId;
                auto stroke=std::exchange(state.forestBrushDirections,{});
                if(!selected||stroke.empty()||frame.scene_input_captured||!ensure_plant_lab_compiled_asset(state))return;
                const auto ordinal=static_cast<std::size_t>(std::count_if(state.entities.begin(),state.entities.end(),
                    [](const auto& entity){return entity.category=="ForestFactory"&&entity.type=="ForestTrunk";}));
                state.forestPlacementCamera=previewgrid::camera_for(ctx.get());
                (void)begin_forest_surface_placement(state,state.entities[*selected],ordinal,state.forestArea,
                    stroke.front(),false,nullptr,stroke);
                return;
            }
            if(frame.scene_input_captured)return;
            int x{},y{};ctx->get_mouse_position_safe(x,y);
            const gui::Vec2 pointer{static_cast<float>(x),static_cast<float>(y)};
            const auto& viewport=frame.scene_viewport;
            if(pointer.x<viewport.position.x||pointer.y<viewport.position.y
                ||pointer.x>=viewport.position.x+viewport.size.x||pointer.y>=viewport.position.y+viewport.size.y)return;
            if(pressed)
            {
                state.forestBrushDirections.clear();
                auto selected=editor_entity_index(state,state.selectedEntityId);
                if(selected&&state.entities[*selected].forestRegion)
                    selected=editor_entity_index(state,state.entities[*selected].forestRegion->terrain_object_id);
                if(selected&&state.entities[*selected].category=="Planetary Terrain")
                    state.forestBrushTerrain=state.entities[*selected].sceneObjectId;
                else
                {
                    const auto planet=std::find_if(state.entities.begin(),state.entities.end(),
                        [](const auto& entity){return entity.category=="Planetary Terrain";});
                    if(planet!=state.entities.end())state.forestBrushTerrain=planet->sceneObjectId;
                }
            }
            const auto selected=editor_entity_index(state,state.forestBrushTerrain);
            if(!selected)return;
            const auto& planet=state.entities[*selected];
            if(!editor_planetary_stream_identity_matches(state,planet))return;
            refresh_editor_planetary_body_inventory(state,planet);
            const auto& policy=*state.planetaryMeshPolicy;
            auto bodyId=policy.settings.body_index;
            if(state.planetaryBodySelection&&state.planetaryBodySelection->entity==planet.sceneObjectId)
                bodyId=state.planetaryBodySelection->body;
            const auto body=std::find_if(policy.generated_bodies.begin(),policy.generated_bodies.end(),
                [&](const auto& value){return value.id==bodyId;});
            if(body==policy.generated_bodies.end())return;
            const auto pivot=scene::world_position(planet.position,planet.positionAnchor);
            if(!pivot)return;
            const auto transform=previewgrid::make_precise_mesh_transform(*pivot,
                {planet.scale[0],planet.scale[1],planet.scale[2]},
                {planet.rotation[0],planet.rotation[1],planet.rotation[2]});
            if(!transform)return;
            const auto camera=previewgrid::camera_for(ctx.get());
            const auto eye=previewgrid::camera_world_eye(camera);
            const auto ray=screen_point_to_world_ray(ctx.get(),viewport,pointer,eye);
            if(!ray)return;
            const std::array<double,3> worldOrigin{eye[0]+ray->origin.x,eye[1]+ray->origin.y,eye[2]+ray->origin.z};
            const std::array<double,3> worldDirection{ray->direction.x,ray->direction.y,ray->direction.z};
            std::array<double,3> origin{},direction{};
            for(std::size_t axis=0;axis<3;++axis)
            {
                for(std::size_t row=0;row<3;++row)
                {
                    origin[axis]+=transform->rotation[row*3+axis]*(worldOrigin[row]-transform->position[row]);
                    direction[axis]+=transform->rotation[row*3+axis]*worldDirection[row];
                }
                origin[axis]=origin[axis]/transform->scale[axis]-body->center_meters[axis]
                    +policy.generated_mesh->coordinates.source_origin_meters[axis];
                direction[axis]/=transform->scale[axis];
            }
            // Ray/sphere only locates the brush. The provider resolves actual
            // terrain elevation and rejects underwater roots before committing.
            const auto hit=forest_brush_surface_direction(origin,direction,body->radius_meters);
            if(!hit)return;
            gui::titlebar_rect({pointer.x-3.0f,pointer.y-3.0f},{6.0f,6.0f});
            const auto& center=*hit;
            auto& stroke=state.forestBrushDirections;
            constexpr std::size_t maximumCenters=16u;
            if(stroke.size()==maximumCenters)return;
            if(!stroke.empty())
            {
                double chord{};
                for(std::size_t axis=0;axis<3;++axis)
                {const auto d=center[axis]-stroke.back()[axis];chord+=d*d;}
                const auto spacing=(std::max)(0.25,state.forestArea.radiusMeters);
                if(chord*body->radius_meters*body->radius_meters<spacing*spacing)return;
            }
            stroke.push_back(center);
            if(stroke.size()==1u)state.plantLabStatus="Release the brush to plant this stroke.";
            else if(stroke.size()==maximumCenters)state.plantLabStatus="Brush stroke is full. Release to plant, then begin another stroke.";
        }

        [[nodiscard]] std::optional<epochengine::previewgrid::Vec3> screen_point_to_world_plane(
            const core::Context* ctx,
            const gui::WidgetBounds& viewport,
            const gui::Vec2& mouse,
            epochengine::previewgrid::Vec3 planePoint,
            epochengine::previewgrid::Vec3 planeNormal,
            const std::array<double, 3>& coordinateOrigin = {}) noexcept
        {
            const auto ray = screen_point_to_world_ray(ctx, viewport, mouse, coordinateOrigin);
            if (!ray)
                return std::nullopt;

            const epochengine::render_math::Float3 point{ planePoint.x, planePoint.y, planePoint.z };
            const auto normal = epochengine::render_math::normalize(
                { planeNormal.x, planeNormal.y, planeNormal.z },
                { 0.0f, 1.0f, 0.0f });
            const float denominator = epochengine::render_math::dot(ray->direction, normal);
            if (std::abs(denominator) <= 1.0e-5f)
                return std::nullopt;
            const float hitDistance = epochengine::render_math::dot(
                epochengine::render_math::subtract(point, ray->origin),
                normal) / denominator;
            if (!std::isfinite(hitDistance)
                || hitDistance < ray->minimumDistance
                || hitDistance > ray->maximumDistance)
            {
                return std::nullopt;
            }

            const auto hit = epochengine::render_math::point_at(ray->origin, ray->direction, hitDistance);
            return epochengine::previewgrid::Vec3{ hit.x, hit.y, hit.z };
        }

        [[nodiscard]] epochengine::previewgrid::Vec3 scene_drag_plane_normal(
            const core::Context* ctx, ScenePointerMode mode) noexcept
        {
            if (mode == ScenePointerMode::move_x || mode == ScenePointerMode::move_z)
                return {0.0f, 1.0f, 0.0f};
            const auto camera = epochengine::previewgrid::camera_for(ctx);
            auto normal = epochengine::previewgrid::camera_forward(camera);
            if (mode == ScenePointerMode::move_free) return normal;
            normal.y = 0.0f;
            return epochengine::previewgrid::dot(normal, normal) > 1.0e-6f
                ? epochengine::previewgrid::normalize(normal)
                : epochengine::previewgrid::Vec3{0.0f, 0.0f, 1.0f};
        }

        void anchor_scene_group_drag(const core::Context* ctx, EditorState& editor,
            const gui::WidgetBounds& viewport, gui::Vec2 mouse)
        {
            editor.sceneDragStartMouse = mouse;
            editor.sceneDragStartPosition = editor.entities[editor.sceneDragEntity].position;
            editor.sceneDragStartPositions.clear();
            editor.sceneDragStartPositions.reserve(editor.selectedEntityIds.size());
            for (const auto id : editor.selectedEntityIds)
                if (const auto index = editor_entity_index(editor, id))
                    editor.sceneDragStartPositions.emplace_back(id, editor.entities[*index].position);
            const auto& position = editor.sceneDragStartPosition;
            const auto hit = screen_point_to_world_plane(ctx, viewport, mouse,
                {position[0], position[1], position[2]}, scene_drag_plane_normal(ctx, editor.sceneDragMode),
                editor.entities[editor.sceneDragEntity].positionAnchor.value_or(std::array<double, 3>{}));
            editor.sceneDragHasPlaneHit = hit.has_value();
            if (hit) editor.sceneDragStartHit = {hit->x, hit->y, hit->z};
        }

        [[nodiscard]] bool complete_scene_click(EditorState& editor, scene::SceneObjectId entity,
            gui::Vec2 position, bool outliner = false, std::optional<std::uint32_t> body = {}) noexcept
        {
            const auto now = std::chrono::steady_clock::now();
            const auto& previous = editor.previousSceneClick;
            const bool doubleClick = previous && previous->entity == entity && previous->body == body
                && previous->outliner == outliner && now - previous->time <= std::chrono::milliseconds{500}
                && std::abs(position.x - previous->position.x) <= 6.0f
                && std::abs(position.y - previous->position.y) <= 6.0f;
            if (doubleClick)
                editor.previousSceneClick.reset();
            else
                editor.previousSceneClick = EditorState::SceneClick{entity, body, position, now, outliner};
            return doubleClick;
        }

        [[nodiscard]] std::optional<scene::SceneObjectId> pick_editor_scene_entity(
            const core::Context* ctx,
            EditorState& state,
            const gui::WidgetBounds& viewport,
            const gui::Vec2& mouse) noexcept
        {
            if (!ensure_editor_scene_interaction(state))
                return std::nullopt;
            const auto queryRay=screen_point_to_world_ray(ctx,viewport,mouse,state.sceneInteractionOrigin);
            if(!queryRay)return {};

            const auto selection = state.sceneInteraction.trace(
                *queryRay,
                epochengine::ray::QueryMode::Closest);
            if (!selection.has_hit())
                return std::nullopt;
            return selection.hit.object.value;
        }
        [[nodiscard]] std::optional<gui::Vec2> project_editor_world_to_screen(
            const core::Context* ctx,
            const gui::WidgetBounds& viewport,
            const std::array<float, 3>& position,
            const std::optional<std::array<double, 3>>& positionAnchor = {}) noexcept
        {
            if (!ctx || viewport.size.x <= 1.0f || viewport.size.y <= 1.0f)
                return std::nullopt;

            const auto camera = epochengine::previewgrid::camera_for(ctx);
            const float aspect = viewport.size.x / viewport.size.y;
            const auto projection = epochengine::previewgrid::projection_for(
                ctx, aspect, camera);
            auto eye = camera.eye;
            auto target = camera.target;
            epochengine::previewgrid::Vec3 projectedPosition{position[0], position[1], position[2]};
            if (positionAnchor || camera.navigationValid)
            {
                const auto absolutePosition = scene::world_position(position, positionAnchor);
                if (!absolutePosition) return std::nullopt;
                const auto absoluteEye = epochengine::previewgrid::camera_world_eye(camera);
                std::array<float, 3> relative{};
                for (std::size_t axis = 0u; axis < relative.size(); ++axis)
                {
                    const double value = (*absolutePosition)[axis] - absoluteEye[axis];
                    if (!std::isfinite(value)
                        || std::abs(value) > (std::numeric_limits<float>::max)()) return std::nullopt;
                    relative[axis] = static_cast<float>(value);
                }
                eye = {};
                target = epochengine::previewgrid::camera_forward(camera);
                projectedPosition = {relative[0], relative[1], relative[2]};
            }
            const auto view = epochengine::previewgrid::look_at(eye, target, camera.up);
            const auto mvp = epochengine::previewgrid::multiply(projection, view);
            const auto clip = epochengine::previewgrid::transform_point(
                mvp,
                projectedPosition);
            if (!std::isfinite(clip.w) || clip.w <= 0.0001f)
                return std::nullopt;

            const float ndcX = clip.x / clip.w;
            const float ndcY = clip.y / clip.w;
            if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
                return std::nullopt;

            return gui::Vec2{
                viewport.position.x
                    + (ndcX * 0.5f + 0.5f) * viewport.size.x,
                viewport.position.y
                    + (1.0f - (ndcY * 0.5f + 0.5f)) * viewport.size.y
            };
        }

        void apply_scene_marquee_selection(
            const core::Context* ctx,
            EditorState& editor,
            const gui::WidgetBounds& viewport) noexcept
        {
            const float left = (std::max)(viewport.position.x,
                (std::min)(editor.sceneMarqueeStart.x, editor.sceneMarqueeCurrent.x));
            const float right = (std::min)(viewport.position.x + viewport.size.x,
                (std::max)(editor.sceneMarqueeStart.x, editor.sceneMarqueeCurrent.x));
            const float top = (std::max)(viewport.position.y,
                (std::min)(editor.sceneMarqueeStart.y, editor.sceneMarqueeCurrent.y));
            const float bottom = (std::min)(viewport.position.y + viewport.size.y,
                (std::max)(editor.sceneMarqueeStart.y, editor.sceneMarqueeCurrent.y));

            if (!editor.sceneMarqueeAdditive)
                clear_editor_selection(editor);

            const auto eye = previewgrid::camera_world_eye(previewgrid::camera_for(ctx));
            for (std::size_t index = 0u; index < editor.entities.size(); ++index)
            {
                const EditorEntity& entity = editor.entities[index];
                if (!entity.visible || is_screen_space_gui_entity(entity))
                    continue;
                if (entity.category == "Planetary Terrain")
                {
                    if (editor_planetary_outliner_only(editor, entity)) continue;
                    if (const auto body = editor_planetary_body_focus(editor, entity))
                    {
                        const auto& bounds = body->bounds;
                        if (eye[0] >= bounds.minimum.x && eye[0] <= bounds.maximum.x
                            && eye[1] >= bounds.minimum.y && eye[1] <= bounds.maximum.y
                            && eye[2] >= bounds.minimum.z && eye[2] <= bounds.maximum.z)
                            continue;
                    }
                }
                const auto projected = project_editor_world_to_screen(
                    ctx, viewport, entity.position, entity.positionAnchor);
                if (!projected
                    || projected->x < left || projected->x > right
                    || projected->y < top || projected->y > bottom)
                {
                    continue;
                }
                if (!editor_entity_selected(editor, entity.sceneObjectId))
                    editor.selectedEntityIds.push_back(entity.sceneObjectId);
                editor.selectedEntity = index;
                editor.selectedEntityId = entity.sceneObjectId;
            }

            if (editor.selectedEntityIds.empty())
            {
                editor.selectedEntityId = scene::kInvalidSceneObjectId;
                editor.selectedEntity = 0u;
            }
            else
            {
                apply_editor_primary_selection_side_effects(editor);
            }
        }

        [[nodiscard]] bool commit_scene_group_drag(EditorState& editor)
        {
            std::vector<authoring::scene::SceneOperation> operations{};
            operations.reserve(editor.sceneDragStartPositions.size());

            for (const auto& [id, startPosition] : editor.sceneDragStartPositions)
            {
                (void)startPosition;
                const auto index = editor_entity_index(editor, id);
                if (!index)
                    continue;
                const auto handle = editor.sceneDocument.find(
                    authoring::scene::ObjectId{id});
                if (!handle)
                    continue;
                const EditorEntity& entity = editor.entities[*index];
                operations.emplace_back(
                    authoring::scene::ObjectTransformChangedOperation{
                        .object = *handle,
                        .value = {
                            .position = {
                                entity.position[0],
                                entity.position[1],
                                entity.position[2]},
                            .rotation_degrees = {
                                entity.rotation[0],
                                entity.rotation[1],
                                entity.rotation[2]},
                            .scale = {
                                entity.scale[0],
                                entity.scale[1],
                                entity.scale[2]}
                        }
                    });
            }

            if (operations.empty())
                return false;

            const auto selection = editor.selectedEntityIds;
            const scene::SceneObjectId primary = editor.selectedEntityId;
            if (!apply_editor_scene_transaction(
                    editor,
                    operations,
                    operations.size() == 1u
                        ? std::string{"Move selected object"}
                        : epochengine::format_text(
                            "Move {} selected objects", operations.size()),
                    primary))
            {
                return false;
            }
            editor.selectedEntityIds = selection;
            editor.selectedEntityId = primary;
            synchronize_editor_selection(editor);
            return true;
        }

        void draw_scene_marquee_overlay(const EditorState& editor) noexcept
        {
            if (!editor.sceneMarqueeActive)
                return;
            const float left = (std::min)(
                editor.sceneMarqueeStart.x, editor.sceneMarqueeCurrent.x);
            const float right = (std::max)(
                editor.sceneMarqueeStart.x, editor.sceneMarqueeCurrent.x);
            const float top = (std::min)(
                editor.sceneMarqueeStart.y, editor.sceneMarqueeCurrent.y);
            const float bottom = (std::max)(
                editor.sceneMarqueeStart.y, editor.sceneMarqueeCurrent.y);
            const float width = right - left;
            const float height = bottom - top;
            if (width < 1.0f || height < 1.0f)
                return;

            constexpr float border = 2.0f;
            gui::titlebar_rect({left, top}, {width, border});
            gui::titlebar_rect({left, bottom - border}, {width, border});
            gui::titlebar_rect({left, top}, {border, height});
            gui::titlebar_rect({right - border, top}, {border, height});
        }

        void update_scene_object_interaction(
            const std::shared_ptr<core::Context>& ctx,
            EditorState& editor,
            EditorFrameResult& result)
        {
            if (!ctx || editor.previewMode != core::ScenePreviewMode::Editor)
            {
                editor.sceneDragActive = false;
                editor.sceneDragStartPositions.clear();
                editor.sceneDragHasPlaneHit = false;
                editor.sceneDragEntityId = scene::kInvalidSceneObjectId;
                editor.sceneMarqueeActive = false;
                editor.sceneLeftWasHeld = false;
                editor.previousSceneClick.reset();
                return;
            }

            if (result.scene_input_captured)
            {
                // A new GUI/runtime owner ends an already-moved gesture. Keep
                // its preview pose in the existing single Undo transaction;
                // never start or advance an object drag under captured input.
                if (editor.sceneDragActive && editor.sceneDragMoved)
                    (void)commit_scene_group_drag(editor);
                editor.sceneDragActive = false;
                editor.sceneDragStartPositions.clear();
                editor.sceneDragHasPlaneHit = false;
                editor.sceneMarqueeActive = false;
                if (editor.previousSceneClick && !editor.previousSceneClick->outliner)
                    editor.previousSceneClick.reset();
                editor.sceneLeftWasHeld = ctx->is_mouse_button_held_safe(
                    epochengine::input::MouseButton::MouseLeft);
                return;
            }

            int mx = 0;
            int my = 0;
            ctx->get_mouse_position_safe(mx, my);
            const gui::Vec2 mouse{
                static_cast<float>(mx),
                static_cast<float>(my)
            };

            const auto& viewport = result.scene_viewport;
            const bool mouseInScene =
                mouse.x >= viewport.position.x
                && mouse.y >= viewport.position.y
                && mouse.x < (viewport.position.x + viewport.size.x)
                && mouse.y < (viewport.position.y + viewport.size.y);

            const bool leftHeld = ctx->is_mouse_button_held_safe(
                epochengine::input::MouseButton::MouseLeft);
            const bool rightHeld = ctx->is_mouse_button_held_safe(
                epochengine::input::MouseButton::MouseRight);
            const bool middleHeld = ctx->is_mouse_button_held_safe(
                epochengine::input::MouseButton::MouseMiddle);
            const bool altHeld = ctx->is_key_held_safe(epochengine::input::Key::LeftAlt)
                || ctx->is_key_held_safe(epochengine::input::Key::RightAlt);
            const bool shiftHeld = ctx->is_key_held_safe(epochengine::input::Key::LeftShift)
                || ctx->is_key_held_safe(epochengine::input::Key::RightShift);
            const bool leftPressed = leftHeld && !editor.sceneLeftWasHeld;

            if (leftPressed && mouseInScene && !rightHeld && !middleHeld && !altHeld)
            {
                const bool moving = !shiftHeld && editor.scenePointerMode != ScenePointerMode::select;
                const auto pickedId = moving
                    ? pick_editor_scene_entity(ctx.get(), editor, viewport, mouse) : std::nullopt;
                const auto picked = pickedId
                    ? editor_entity_index(editor, *pickedId)
                    : std::nullopt;
                if (picked && pickedId)
                {
                    if (!editor_entity_selected(editor, *pickedId))
                        select_editor_entity(editor, *picked);
                    else
                    {
                        editor.selectedEntity = *picked;
                        editor.selectedEntityId = *pickedId;
                        apply_editor_primary_selection_side_effects(editor);
                    }
                    editor.sceneDragActive = true;
                    editor.sceneDragEntity = *picked;
                    editor.sceneDragEntityId = *pickedId;
                    editor.sceneDragMoved = false;
                    editor.sceneDragMode = editor.scenePointerMode;
                    anchor_scene_group_drag(ctx.get(), editor, viewport, mouse);
                    result.scene_input_captured = true;
                }
                else
                {
                    editor.sceneMarqueeActive = true;
                    editor.sceneMarqueeAdditive = shiftHeld;
                    editor.sceneMarqueeStart = mouse;
                    editor.sceneMarqueeCurrent = mouse;
                    result.scene_input_captured = true;
                }
            }

            if (editor.sceneMarqueeActive && leftHeld)
            {
                editor.sceneMarqueeCurrent = mouse;
                result.scene_input_captured = true;
            }

            if (editor.sceneDragActive && leftHeld && !rightHeld && !middleHeld && !altHeld
                && editor.sceneDragEntity < editor.entities.size()
                && (editor.sceneDragMoved
                    || std::abs(mouse.x - editor.sceneDragStartMouse.x) >= 4.0f
                    || std::abs(mouse.y - editor.sceneDragStartMouse.y) >= 4.0f))
            {
                result.scene_input_captured = true;
                editor.sceneDragMoved = true;
                editor.previousSceneClick.reset();

                const float dx = mouse.x - editor.sceneDragStartMouse.x;
                const float dy = mouse.y - editor.sceneDragStartMouse.y;
                epochengine::previewgrid::Vec3 delta{};
                bool haveDelta = false;

                if (editor.sceneDragHasPlaneHit)
                {
                    const auto currentHit = screen_point_to_world_plane(
                        ctx.get(),
                        viewport,
                        mouse,
                        {
                            editor.sceneDragStartPosition[0],
                            editor.sceneDragStartPosition[1],
                            editor.sceneDragStartPosition[2]
                        },
                        scene_drag_plane_normal(ctx.get(), editor.sceneDragMode),
                        editor.entities[editor.sceneDragEntity].positionAnchor.value_or(std::array<double, 3>{}));
                    if (currentHit)
                    {
                        delta = {
                            currentHit->x - editor.sceneDragStartHit[0],
                            currentHit->y - editor.sceneDragStartHit[1],
                            currentHit->z - editor.sceneDragStartHit[2]
                        };
                        haveDelta = true;
                    }
                }
                else
                {
                    const auto camera = epochengine::previewgrid::camera_for(ctx.get());
                    auto forward = epochengine::previewgrid::normalize(
                        epochengine::previewgrid::subtract(camera.target, camera.eye));
                    if (editor.sceneDragMode != ScenePointerMode::move_free) forward.y = 0.0f;
                    forward = epochengine::previewgrid::normalize(forward);
                    if (epochengine::previewgrid::dot(forward, forward) <= 1.0e-6f)
                        forward = { 0.0f, 0.0f, -1.0f };

                    constexpr epochengine::previewgrid::Vec3 kWorldUp{
                        0.0f, 1.0f, 0.0f };
                    auto right = epochengine::previewgrid::normalize(
                        epochengine::previewgrid::cross(forward,
                            editor.sceneDragMode == ScenePointerMode::move_free ? camera.up : kWorldUp));
                    if (epochengine::previewgrid::dot(right, right) <= 1.0e-6f)
                        right = { 1.0f, 0.0f, 0.0f };

                    const float dragScale = (std::clamp)(
                        epochengine::previewgrid::camera_distance_for(ctx.get()) * 0.00175f,
                        0.004f,
                        0.045f);
                    const auto vertical = editor.sceneDragMode == ScenePointerMode::move_free
                        ? epochengine::previewgrid::normalize(epochengine::previewgrid::cross(right, forward)) : forward;
                    delta = epochengine::previewgrid::add(epochengine::previewgrid::scale(right, dx * dragScale),
                        epochengine::previewgrid::scale(vertical, -dy * dragScale));
                    if (editor.sceneDragMode == ScenePointerMode::move_y) delta = {0.0f, -dy * dragScale, 0.0f};
                    haveDelta = true;
                }

                if (haveDelta && std::isfinite(delta.x) && std::isfinite(delta.y) && std::isfinite(delta.z))
                {
                    switch (editor.sceneDragMode)
                    {
                    case ScenePointerMode::move_x: delta.y = delta.z = 0.0f; break;
                    case ScenePointerMode::move_y: delta.x = delta.z = 0.0f; break;
                    case ScenePointerMode::move_z: delta.x = delta.y = 0.0f; break;
                    default: break;
                    }
                    for (const auto& [id, startPosition] : editor.sceneDragStartPositions)
                    {
                        if (const auto index = editor_entity_index(editor, id))
                        {
                            auto& entity = editor.entities[*index];
                            entity.position[0] = startPosition[0] + delta.x;
                            entity.position[1] = startPosition[1] + delta.y;
                            entity.position[2] = startPosition[2] + delta.z;
                        }
                    }
                    editor.sceneLightingDirty = true;
                    editor.sceneInteractionDirty = true;
                }
            }

            if (!leftHeld && editor.sceneDragActive)
            {
                const std::size_t movedCount = editor.sceneDragStartPositions.size();
                if (!editor.sceneDragMoved)
                {
                    if (mouseInScene && complete_scene_click(editor, editor.sceneDragEntityId, mouse))
                        handle_scene_tool(editor, ctx.get(), "focus_selection");
                }
                else if (editor.sceneDragMoved && commit_scene_group_drag(editor))
                {
                    push_editor_log(
                        editor,
                        movedCount == 1u
                            ? "[scene] Moved selected object."
                            : epochengine::format_text(
                                "[scene] Moved {} selected objects as one group.",
                                movedCount));
                }
                editor.sceneDragActive = false;
                editor.sceneDragStartPositions.clear();
                editor.sceneDragHasPlaneHit = false;
                editor.sceneDragEntityId = scene::kInvalidSceneObjectId;
            }

            if (!leftHeld && editor.sceneMarqueeActive)
            {
                editor.sceneMarqueeCurrent = mouse;
                const float width = std::abs(
                    editor.sceneMarqueeCurrent.x - editor.sceneMarqueeStart.x);
                const float height = std::abs(
                    editor.sceneMarqueeCurrent.y - editor.sceneMarqueeStart.y);
                if (width >= 4.0f || height >= 4.0f)
                {
                    editor.previousSceneClick.reset();
                    apply_scene_marquee_selection(
                        ctx.get(), editor, viewport);
                    push_editor_log(
                        editor,
                        epochengine::format_text(
                            "[scene] Marquee selected {} object(s).",
                            editor.selectedEntityIds.size()));
                }
                else
                {
                    const auto pickedId = mouseInScene
                        ? pick_editor_scene_entity(ctx.get(), editor, viewport, editor.sceneMarqueeStart)
                        : std::nullopt;
                    const auto picked = pickedId ? editor_entity_index(editor, *pickedId) : std::nullopt;
                    if (!picked)
                    {
                        editor.previousSceneClick.reset();
                        if (!editor.sceneMarqueeAdditive) clear_editor_selection(editor);
                    }
                    else if (editor.sceneMarqueeAdditive)
                    {
                        editor.previousSceneClick.reset();
                        toggle_editor_entity_selection(editor, *picked);
                    }
                    else
                    {
                        select_editor_entity(editor, *picked);
                        if (complete_scene_click(editor, *pickedId, mouse))
                            handle_scene_tool(editor, ctx.get(), "focus_selection");
                    }
                }
                editor.sceneMarqueeActive = false;
                result.scene_input_captured = true;
            }

            editor.sceneLeftWasHeld = leftHeld;
        }

