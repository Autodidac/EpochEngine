// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] static previewgrid::CameraMode snapshot_camera_mode(std::uint8_t value) noexcept
        {
            switch (value)
            {
            case 1: return previewgrid::CameraMode::FPS;
            case 2: return previewgrid::CameraMode::Canvas2D;
            case 0:
            default: return previewgrid::CameraMode::Editor;
            }
        }

        [[nodiscard]] static EditorMainSurface snapshot_main_surface(std::uint8_t value) noexcept
        {
            switch (value)
            {
            case 1: return EditorMainSurface::Game2D;
            case 2: return EditorMainSurface::Assets;
            case 3: return EditorMainSurface::Project;
            case 4: return EditorMainSurface::ForestFactory;
            case 5: return EditorMainSurface::Timeline;
            case 6: return EditorMainSurface::AISandbox;
            case 8: return EditorMainSurface::PlantLab;
            case 7: return EditorMainSurface::Systems;
            case 0:
            default: return EditorMainSurface::Scene;
            }
        }

        [[nodiscard]] static EditorWorkspaceTab snapshot_workspace_tab(EditorWorkspaceTab value) noexcept
        {
            switch (value)
            {
            case EditorWorkspaceTab::Project:
            case EditorWorkspaceTab::Scripts:
            case EditorWorkspaceTab::Assets:
            case EditorWorkspaceTab::AI:
            case EditorWorkspaceTab::Systems:
            case EditorWorkspaceTab::Output:
                return value;
            default:
                return EditorWorkspaceTab::Output;
            }
        }

        [[nodiscard]] static core::ScenePreviewMode snapshot_preview_mode(core::ScenePreviewMode value) noexcept
        {
            switch (value)
            {
            case core::ScenePreviewMode::Editor:
            case core::ScenePreviewMode::None:
                return value;
            default:
                return core::ScenePreviewMode::Editor;
            }
        }

        [[nodiscard]] static input::ProfilePreset snapshot_input_profile(std::uint8_t value) noexcept
        {
            switch (static_cast<input::ProfilePreset>(value))
            {
            case input::ProfilePreset::RuntimeWASD:
            case input::ProfilePreset::ArrowPilot:
            case input::ProfilePreset::LeftHanded:
            case input::ProfilePreset::EditorDefault:
                return static_cast<input::ProfilePreset>(value);
            case input::ProfilePreset::Count:
            default:
                return input::ProfilePreset::EditorDefault;
            }
        }

        [[nodiscard]] static gui::ThemePreference snapshot_theme_preference(gui::ThemePreference value) noexcept
        {
            switch (value)
            {
            case gui::ThemePreference::Light:
            case gui::ThemePreference::Dark:
            case gui::ThemePreference::ProfessionalDark:
            case gui::ThemePreference::ClassicLauncher:
            case gui::ThemePreference::MidnightBlue:
            case gui::ThemePreference::EmberForge:
            case gui::ThemePreference::ForestTerminal:
            case gui::ThemePreference::AuroraSteel:
            case gui::ThemePreference::CodexBlack:
            case gui::ThemePreference::FollowSystemDark:
            case gui::ThemePreference::ThemeCycle:
                return value;
            case gui::ThemePreference::StudioNavy:
                return gui::ThemePreference::MidnightBlue; // Migrated saved blue theme.
            case gui::ThemePreference::RemovedLegacyTheme:
                return gui::ThemePreference::Dark; // Map saved obsolete ordinal 11 to Dark.
            default:
                return gui::ThemePreference::FollowSystemDark;
            }
        }

        [[nodiscard]] static std::filesystem::path editor_layout_preferences_root()
        {
            if (const auto data = core::path::candidate_data_root(); !data.empty())
                return data / "config";
#if defined(_WIN32)
            if (const auto localAppData = core::env::get("LOCALAPPDATA");
                localAppData && !localAppData->empty())
            {
                return std::filesystem::path{localAppData->c_str()}
                    / "EpochEngine" / "config";
            }
#else
            if (const auto xdgConfigHome = core::env::get("XDG_CONFIG_HOME");
                xdgConfigHome && !xdgConfigHome->empty())
            {
                return std::filesystem::path{xdgConfigHome->c_str()}
                    / "epochengine";
            }
            if (const auto home = core::env::get("HOME");
                home && !home->empty())
            {
                return std::filesystem::path{home->c_str()}
                    / ".config" / "epochengine";
            }
#endif
            if (const auto runtimeRoot = core::path::runtime_root_dir();
                !runtimeRoot.empty())
            {
                return runtimeRoot / "config";
            }
            return std::filesystem::current_path() / "config";
        }

        [[nodiscard]] static std::string_view editor_layout_preferences_stem(
            EditorApplicationKind application) noexcept
        {
            switch (application)
            {
            case EditorApplicationKind::PlantLab:
                return "plant_lab";
            case EditorApplicationKind::GuiEditor:
                return "gui_editor";
            case EditorApplicationKind::Standard:
            default:
                return "standard_editor";
            }
        }

        [[nodiscard]] static std::filesystem::path editor_layout_preferences_path(
            EditorApplicationKind application)
        {
            return editor_layout_preferences_root()
                / (std::string{editor_layout_preferences_stem(application)}
                    + ".layout");
        }

        [[nodiscard]] static std::string serialize_editor_layout_preferences(
            const EditorState& state)
        {
            std::ostringstream output{};
            output << "EPOCH_EDITOR_LAYOUT 9\n";
            output << "application "
                << static_cast<unsigned int>(state.applicationKind) << '\n';
            output << "splits "
                << state.bottomGridSplit << ' '
                << state.outlinerSplit << ' '
                << state.inspectorSplit << ' '
                << state.dockSplit << '\n';
            auto workspaceBottomGridSplits = state.workspaceBottomGridSplits;
            auto workspaceDockSplits = state.workspaceDockSplits;
            auto workspaceOutputFollow = state.workspaceOutputFollow;
            auto workspaceAiChatFollow = state.workspaceAiChatFollow;
            const std::size_t activeSurface = editor_main_surface_index(
                state.mainSurface);
            workspaceBottomGridSplits[activeSurface] = state.bottomGridSplit;
            workspaceDockSplits[activeSurface] = state.dockSplit;
            workspaceOutputFollow[activeSurface] = state.outputFollowTail;
            workspaceAiChatFollow[activeSurface] = state.aiChatFollowTail;
            output << "workspace_splits " << kEditorMainSurfaceCount;
            for (std::size_t index = 0u; index < kEditorMainSurfaceCount; ++index)
                output << ' ' << workspaceBottomGridSplits[index]
                    << ' ' << workspaceDockSplits[index];
            output << '\n';
            output << "workspace_follow " << kEditorMainSurfaceCount;
            for (std::size_t index = 0u; index < kEditorMainSurfaceCount; ++index)
                output << ' ' << static_cast<unsigned int>(workspaceOutputFollow[index])
                    << ' ' << static_cast<unsigned int>(workspaceAiChatFollow[index]);
            output << '\n';
            output << "visibility "
                << static_cast<unsigned int>(state.showOutliner) << ' '
                << static_cast<unsigned int>(state.showInspector) << ' '
                << static_cast<unsigned int>(state.showConsoleDock) << ' '
                << static_cast<unsigned int>(state.showAiChat) << '\n';
            for (std::size_t index = 0; index < kEditorToolPaneCount; ++index)
            {
                output << "pane " << index << ' '
                    << static_cast<unsigned int>(state.toolPaneOpen[index]) << ' '
                    << static_cast<unsigned int>(state.toolPaneDockRegions[index]) << ' '
                    << state.toolPaneTabOrder[index]
                    << '\n';
            }
            output << "active "
                << state.activeLeftPaneRoute << ' '
                << state.activeRightPaneRoute << ' '
                << state.activeBottomLeftPaneRoute << ' '
                << state.activeBottomRightPaneRoute << '\n';
            output << "theme "
                << static_cast<unsigned int>(state.themePreference) << ' '
                << static_cast<unsigned int>(state.roundedRectangles) << '\n';
            output << "theme_cycle_interval "
                << gui::normalize_theme_cycle_interval(state.themeCycleIntervalSeconds) << '\n';
            return output.str();
        }

        [[nodiscard]] static bool valid_layout_boolean(unsigned int value) noexcept
        {
            return value <= 1u;
        }

        [[nodiscard]] static bool valid_active_pane_route(
            const EditorState& state,
            std::string_view route,
            EditorToolDockRegion region) noexcept
        {
            const auto pane = editor_tool_pane_from_route(route);
            if (!pane)
                return false;
            const std::size_t index = tool_pane_index(*pane);
            return state.toolPaneOpen[index]
                && state.toolPaneDockRegions[index] == region;
        }

        [[nodiscard]] static std::string first_open_pane_route(
            const EditorState& state,
            EditorToolDockRegion region,
            std::string_view fallback)
        {
            constexpr auto routes = editor_tool_pane_routes();
            std::optional<std::size_t> first{};
            for (std::size_t index = 0; index < routes.size(); ++index)
            {
                if (state.toolPaneOpen[index]
                    && state.toolPaneDockRegions[index] == region)
                {
                    if (!first
                        || state.toolPaneTabOrder[index]
                            < state.toolPaneTabOrder[*first])
                        first = index;
                }
            }
            if (first)
                return std::string{routes[*first]};
            return std::string{fallback};
        }

        [[nodiscard]] static bool decode_editor_layout_preferences(
            EditorState& state,
            std::string_view encoded)
        {
            std::istringstream input{std::string{encoded}};
            std::string token{};
            unsigned int schema{};
            if (!(input >> token >> schema)
                || token != "EPOCH_EDITOR_LAYOUT" || schema < 1u || schema > 10u)
            {
                return false;
            }

            unsigned int application{};
            if (!(input >> token >> application)
                || token != "application"
                || application != static_cast<unsigned int>(state.applicationKind))
            {
                return false;
            }

            float bottomGridSplit{};
            float outlinerSplit{};
            float inspectorSplit{};
            float dockSplit{};
            if (!(input >> token >> bottomGridSplit >> outlinerSplit
                    >> inspectorSplit >> dockSplit)
                || token != "splits"
                || !std::isfinite(bottomGridSplit)
                || !std::isfinite(outlinerSplit)
                || !std::isfinite(inspectorSplit)
                || !std::isfinite(dockSplit))
            {
                return false;
            }

            auto workspaceBottomGridSplits = state.workspaceBottomGridSplits;
            auto workspaceDockSplits = state.workspaceDockSplits;
            auto workspaceOutputFollow = state.workspaceOutputFollow;
            auto workspaceAiChatFollow = state.workspaceAiChatFollow;
            if (schema >= 3u)
            {
                std::size_t count{};
                if (!(input >> token >> count)
                    || token != "workspace_splits"
                    || count != kEditorMainSurfaceCount)
                {
                    return false;
                }
                for (std::size_t index = 0u; index < count; ++index)
                {
                    if (!(input >> workspaceBottomGridSplits[index]
                            >> workspaceDockSplits[index])
                        || !std::isfinite(workspaceBottomGridSplits[index])
                        || !std::isfinite(workspaceDockSplits[index]))
                    {
                        return false;
                    }
                    workspaceBottomGridSplits[index] = std::clamp(
                        workspaceBottomGridSplits[index], 0.25f, 0.75f);
                    workspaceDockSplits[index] = std::clamp(
                        workspaceDockSplits[index], 0.08f, 0.80f);
                }
                if (!(input >> token >> count)
                    || token != "workspace_follow"
                    || count != kEditorMainSurfaceCount)
                {
                    return false;
                }
                for (std::size_t index = 0u; index < count; ++index)
                {
                    unsigned int outputFollow{};
                    unsigned int aiChatFollow{};
                    if (!(input >> outputFollow >> aiChatFollow)
                        || !valid_layout_boolean(outputFollow)
                        || !valid_layout_boolean(aiChatFollow))
                    {
                        return false;
                    }
                    workspaceOutputFollow[index] = outputFollow != 0u;
                    workspaceAiChatFollow[index] = aiChatFollow != 0u;
                }
            }

            unsigned int showOutliner{};
            unsigned int showInspector{};
            unsigned int showConsoleDock{};
            unsigned int showAiChat{};
            if (!(input >> token >> showOutliner >> showInspector
                    >> showConsoleDock >> showAiChat)
                || token != "visibility"
                || !valid_layout_boolean(showOutliner)
                || !valid_layout_boolean(showInspector)
                || !valid_layout_boolean(showConsoleDock)
                || !valid_layout_boolean(showAiChat))
            {
                return false;
            }

            auto paneOpen = state.toolPaneOpen;
            auto paneRegions = state.toolPaneDockRegions;
            auto paneTabOrder = default_tool_pane_tab_order();
            const std::size_t storedPaneCount = schema < 7u
                ? kEditorToolPaneCount - 2u
                : schema < 8u ? kEditorToolPaneCount - 1u
                : kEditorToolPaneCount;
            for (std::size_t expected = 0; expected < storedPaneCount; ++expected)
            {
                std::size_t index{};
                unsigned int open{};
                unsigned int region{};
                std::uint32_t tabOrder = static_cast<std::uint32_t>(expected);
                if (!(input >> token >> index >> open >> region)
                    || token != "pane"
                    || index != expected
                    || !valid_layout_boolean(open)
                    || region > static_cast<unsigned int>(
                        schema == 1u
                            ? EditorToolDockRegion::BottomLeft
                            : EditorToolDockRegion::BottomRight))
                {
                    return false;
                }
                if (schema >= 4u
                    && (!(input >> tabOrder) || tabOrder >= kEditorToolPaneCount))
                    return false;
                paneOpen[index] = open != 0u;
                paneRegions[index] = static_cast<EditorToolDockRegion>(region);
                paneTabOrder[index] = tabOrder;
                if (schema == 1u
                    && expected == tool_pane_index(EditorToolPane::AiChat)
                    && paneRegions[index] == EditorToolDockRegion::BottomLeft)
                {
                    paneRegions[index] = EditorToolDockRegion::BottomRight;
                }
            }

            std::string activeLeft{};
            std::string activeRight{};
            std::string activeBottomLeft{};
            std::string activeBottomRight{ "pane.ai_chat" };
            if (!(input >> token >> activeLeft >> activeRight >> activeBottomLeft)
                || token != "active")
            {
                return false;
            }
            if (schema >= 2u && !(input >> activeBottomRight))
                return false;

            unsigned int theme{};
            unsigned int rounded{};
            if (!(input >> token >> theme >> rounded)
                || token != "theme"
                // Accept the retired v0.90.132 theme ordinal only to recover user settings.
                // It is not a selectable or implemented theme.
                || theme > 13u
                || !valid_layout_boolean(rounded))
            {
                return false;
            }
            std::uint32_t themeCycleIntervalSeconds{30u};
            if (schema >= 6u)
            {
                if (!(input >> token >> themeCycleIntervalSeconds)
                    || token != "theme_cycle_interval"
                    || gui::normalize_theme_cycle_interval(themeCycleIntervalSeconds)
                        != themeCycleIntervalSeconds)
                {
                    return false;
                }
            }

            if (schema <= 4u)
            {
                constexpr float epsilon = 0.0005f;
                if (std::abs(dockSplit - 0.24f) <= epsilon)
                    dockSplit = 0.28f;
                for (float& value : workspaceDockSplits)
                {
                    if (std::abs(value - 0.24f) <= epsilon)
                        value = 0.28f;
                }
            }

            state.bottomGridSplit = gui_lib::normalize_bottom_dock_column_fraction(bottomGridSplit);
            state.outlinerSplit = std::clamp(outlinerSplit, 0.06f, 0.70f);
            state.inspectorSplit = std::clamp(inspectorSplit, 0.06f, 0.70f);
            state.dockSplit = std::clamp(dockSplit, 0.08f, 0.80f);
            const std::size_t activeSurface = editor_main_surface_index(
                state.mainSurface);
            workspaceBottomGridSplits[activeSurface] = state.bottomGridSplit;
            workspaceDockSplits[activeSurface] = state.dockSplit;
            state.workspaceBottomGridSplits = workspaceBottomGridSplits;
            state.workspaceDockSplits = workspaceDockSplits;
            state.workspaceOutputFollow = workspaceOutputFollow;
            state.workspaceAiChatFollow = workspaceAiChatFollow;
            state.outputFollowTail = workspaceOutputFollow[activeSurface];
            state.aiChatFollowTail = workspaceAiChatFollow[activeSurface];
            state.dockPreferenceSurface = state.mainSurface;
            state.showOutliner = showOutliner != 0u;
            state.showInspector = showInspector != 0u;
            state.showConsoleDock = showConsoleDock != 0u;
            state.showAiChat = showAiChat != 0u;
            state.toolPaneOpen = paneOpen;
            state.toolPaneDockRegions = paneRegions;
            state.toolPaneTabOrder = paneTabOrder;
            state.activeLeftPaneRoute = std::move(activeLeft);
            state.activeRightPaneRoute = std::move(activeRight);
            state.activeBottomLeftPaneRoute = std::move(activeBottomLeft);
            state.activeBottomRightPaneRoute = std::move(activeBottomRight);
            // Retire the old settings pane without changing persisted pane indices.
            if (state.activeLeftPaneRoute == "pane.ai_controls") state.activeLeftPaneRoute = "pane.world_outliner";
            if (state.activeRightPaneRoute == "pane.ai_controls") state.activeRightPaneRoute = "pane.ai_chat";
            if (state.activeBottomLeftPaneRoute == "pane.ai_controls") state.activeBottomLeftPaneRoute = "pane.output";
            if (state.activeBottomRightPaneRoute == "pane.ai_controls") state.activeBottomRightPaneRoute = "pane.ai_development";
            if (schema < 8u)
            {
                // Upgrade old default layouts, but leave explicit v8 user
                // re-docking intact. Keep old pane indices stable.
                const auto chatIndex = tool_pane_index(EditorToolPane::AiChat);
                state.toolPaneDockRegions[chatIndex] = EditorToolDockRegion::Right;
                state.toolPaneOpen[chatIndex] = true;
                state.activeRightPaneRoute = "pane.ai_chat";
                if (state.activeBottomRightPaneRoute == "pane.ai_chat")
                    state.activeBottomRightPaneRoute = "pane.ai_development";
            }
            if (!valid_active_pane_route(
                    state, state.activeLeftPaneRoute, EditorToolDockRegion::Left))
            {
                state.activeLeftPaneRoute = first_open_pane_route(
                    state, EditorToolDockRegion::Left, "pane.world_outliner");
            }
            if (!valid_active_pane_route(
                    state, state.activeRightPaneRoute, EditorToolDockRegion::Right))
            {
                state.activeRightPaneRoute = first_open_pane_route(
                    state, EditorToolDockRegion::Right, "pane.properties");
            }
            if (!valid_active_pane_route(
                    state, state.activeBottomLeftPaneRoute, EditorToolDockRegion::BottomLeft))
            {
                state.activeBottomLeftPaneRoute = first_open_pane_route(
                    state, EditorToolDockRegion::BottomLeft, "pane.output");
            }
            if (!valid_active_pane_route(
                    state, state.activeBottomRightPaneRoute, EditorToolDockRegion::BottomRight))
            {
                state.activeBottomRightPaneRoute = first_open_pane_route(
                    state, EditorToolDockRegion::BottomRight, "pane.ai_chat");
            }
            state.themePreference = theme == 13u
                ? gui::ThemePreference::FollowSystemDark
                : snapshot_theme_preference(static_cast<gui::ThemePreference>(theme));
            state.themeCycleIntervalSeconds = themeCycleIntervalSeconds;
            state.roundedRectangles = rounded != 0u;
            return true;
        }

        static void load_editor_layout_preferences(EditorState& state)
        {
            state.layoutPreferencesLoaded = true;
            state.layoutPreferencesApplication = state.applicationKind;
            state.layoutPreferencesSnapshot.clear();

            const std::filesystem::path path =
                editor_layout_preferences_path(state.applicationKind);
            std::error_code error{};
            const auto status = std::filesystem::symlink_status(path, error);
            if (error || !std::filesystem::exists(status))
                return;
            if (std::filesystem::is_symlink(status)
                || !std::filesystem::is_regular_file(status))
            {
                push_editor_log(state,
                    "[layout] Ignored unsafe editor layout preference file.");
                return;
            }

            const auto size = std::filesystem::file_size(path, error);
            if (error || size > 64u * 1024u)
            {
                push_editor_log(state,
                    "[layout] Ignored invalid editor layout preference file.");
                return;
            }

            std::ifstream file{path, std::ios::binary};
            std::ostringstream buffer{};
            if (!file || !(buffer << file.rdbuf()))
                return;
            const std::string encoded = buffer.str();
            if (!decode_editor_layout_preferences(state, encoded))
            {
                push_editor_log(state,
                    "[layout] Ignored malformed editor layout preferences; defaults remain active.");
                return;
            }
            // v0.90.132 wrote schema 10 and ordinal 13 for an accidentally
            // introduced theme. Keep all other saved layout choices, restore
            // the system dark/light theme, then rewrite into schema 9 during
            // the next normal settings persistence pass.
            state.layoutPreferencesSnapshot = encoded.starts_with("EPOCH_EDITOR_LAYOUT 10\n")
                ? encoded : serialize_editor_layout_preferences(state);
        }

        [[nodiscard]] static bool write_editor_layout_preferences(
            const std::filesystem::path& destination,
            std::string_view encoded)
        {
            std::error_code error{};
            std::filesystem::create_directories(destination.parent_path(), error);
            if (error)
                return false;

            const auto destinationStatus =
                std::filesystem::symlink_status(destination, error);
            if (!error && std::filesystem::exists(destinationStatus)
                && (std::filesystem::is_symlink(destinationStatus)
                    || !std::filesystem::is_regular_file(destinationStatus)))
            {
                return false;
            }
            error.clear();

            const std::filesystem::path temporary = destination.parent_path()
                / (destination.filename().string() + ".write");
            const auto temporaryStatus =
                std::filesystem::symlink_status(temporary, error);
            if (!error && std::filesystem::exists(temporaryStatus))
            {
                if (std::filesystem::is_symlink(temporaryStatus)
                    || !std::filesystem::is_regular_file(temporaryStatus)
                    || !std::filesystem::remove(temporary, error)
                    || error)
                {
                    return false;
                }
            }
            error.clear();

            const auto bytes = std::as_bytes(std::span<const char>{
                encoded.data(), encoded.size()});
            if (!platform::filesystem::exclusive_create_and_write(
                    temporary, bytes, error))
            {
                return false;
            }
            if (!platform::filesystem::atomic_replace_same_filesystem(
                    temporary, destination, error))
            {
                std::error_code ignored{};
                (void)std::filesystem::remove(temporary, ignored);
                return false;
            }
            return true;
        }

        static void persist_editor_layout_preferences(EditorState& state, bool force = false)
        {
            if (!state.layoutPreferencesLoaded
                || state.layoutPreferencesApplication != state.applicationKind)
            {
                return;
            }
            const auto now = std::chrono::steady_clock::now();
            if (!force && now < state.layoutPreferencesNextCheck)
                return;
            state.layoutPreferencesNextCheck = now + std::chrono::milliseconds{250};
            const std::string encoded = serialize_editor_layout_preferences(state);
            if (encoded == state.layoutPreferencesSnapshot)
                return;

            if (write_editor_layout_preferences(
                    editor_layout_preferences_path(state.applicationKind), encoded))
            {
                state.layoutPreferencesSnapshot = encoded;
                state.layoutPreferencesSaveErrorLogged = false;
            }
            else if (!state.layoutPreferencesSaveErrorLogged)
            {
                push_editor_log(state,
                    "[layout] Editor layout preferences could not be saved.");
                state.layoutPreferencesSaveErrorLogged = true;
            }
        }

