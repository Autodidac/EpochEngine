// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        if (layout_outliner_visible && outliner_split_size.x > 1.0f && outliner_split_size.y > 1.0f)
            gui::splitter_bar(outliner_split_pos, outliner_split_size, outlinerSplitHovered, editor.layoutDrag == EditorLayoutDrag::Outliner);
        if (layout_inspector_visible && inspector_split_size.x > 1.0f && inspector_split_size.y > 1.0f)
            gui::splitter_bar(inspector_split_pos, inspector_split_size, inspectorSplitHovered, editor.layoutDrag == EditorLayoutDrag::Inspector);
        if (bottom_visible && bottom_split_size.x > 1.0f && bottom_split_size.y > 1.0f)
            gui::splitter_bar(bottom_split_pos, bottom_split_size, bottomSplitHovered, editor.layoutDrag == EditorLayoutDrag::Dock);
        if (splitBottomColumns && bottomColumnsSplitSize.x > 1.0f && bottomColumnsSplitSize.y > 1.0f)
            gui::splitter_bar(bottomColumnsSplitPosition, bottomColumnsSplitSize, bottomColumnsSplitHovered, editor.layoutDrag == EditorLayoutDrag::BottomColumns);

        auto render_console_tool_pane = [&](
            EditorToolDockRegion region,
            std::string_view pane_route,
            std::string_view pane_label)
        {
        const gui::Vec2 console_render_pos =
            tool_group_content_position(region);
        const gui::Vec2 console_render_size =
            tool_group_content_size(region);
        // The dock strip owns the pane title and its close control.
        gui::begin_window(
            pane_label,
            console_render_pos,
            console_render_size,
            true,
            false);
        handle_pane_title_drag_to_tab(
            pane_route,
            pane_label,
            console_render_pos,
            console_render_size);

        if (editor.dockStatusTab == EditorWorkspaceTab::Scripts)
            editor.dockStatusTab = EditorWorkspaceTab::Output;

        constexpr std::array outputFilterValues{
            EditorWorkspaceTab::Output,
            EditorWorkspaceTab::Project,
            EditorWorkspaceTab::Assets,
            EditorWorkspaceTab::AI,
            EditorWorkspaceTab::Systems
        };
        constexpr std::array<std::string_view, outputFilterValues.size()>
            outputFilterLabels{
                "All",
                "Project",
                "Assets",
                "AI Output",
                "Systems"
            };
        constexpr std::array<float, outputFilterValues.size()>
            outputFilterWidths{ 58.0f, 74.0f, 70.0f, 90.0f, 78.0f };

        auto outputFilterSpec = [&](std::size_t index) {
            return gui::SegmentedButtonSpec{
                outputFilterLabels[index],
                outputFilterWidths[index],
                editor.dockStatusTab == outputFilterValues[index]
            };
        };
        auto applyOutputFilter = [&](std::size_t index) {
            if (index < outputFilterValues.size())
                editor.dockStatusTab = outputFilterValues[index];
        };

        if (console_render_size.x >= 424.0f)
        {
            const std::array filters{
                outputFilterSpec(0u),
                outputFilterSpec(1u),
                outputFilterSpec(2u),
                outputFilterSpec(3u),
                outputFilterSpec(4u)
            };
            if (const auto selected =
                    gui::segmented_button_row(filters, 27.0f, 3.0f))
            {
                applyOutputFilter(*selected);
            }
        }
        else
        {
            const std::array primaryFilters{
                outputFilterSpec(0u),
                outputFilterSpec(1u),
                outputFilterSpec(2u)
            };
            if (const auto selected =
                    gui::segmented_button_row(primaryFilters, 27.0f, 3.0f))
            {
                applyOutputFilter(*selected);
            }

            const std::array secondaryFilters{
                outputFilterSpec(3u),
                outputFilterSpec(4u)
            };
            if (const auto selected =
                    gui::segmented_button_row(secondaryFilters, 27.0f, 3.0f))
            {
                applyOutputFilter(*selected + 3u);
            }
        }

        auto dockLine = [](std::string_view label, std::string_view value) {
            std::string line{ label };
            line += ": ";
            line += value;
            return line;
        };
        auto renderDockStatusPanel = [&](std::string_view id, const std::vector<std::string>& lines) {
            const gui::Vec2 panelCursor = gui::cursor_position();
            const float panelHeight = (std::max)(72.0f, console_render_pos.y + console_render_size.y - panelCursor.y - 12.0f);
            (void)gui::scroll_text_panel(gui::ScrollTextPanelOptions{
                .id = std::string("editor-dock-status-") + std::string(id),
                .size = { (std::max)(180.0f, console_render_size.x - 24.0f), panelHeight },
                .lines = lines,
                .max_line_chars = 1024,
                .selectable = false,
                .stick_to_bottom = false
            });
        };

        const bool dockUsesOuterScroll = false;
        if (dockUsesOuterScroll)
        {
            const gui::Vec2 dockScrollStart = gui::cursor_position();
            const float dockScrollHeight = (std::max)(
                60.0f,
                console_render_pos.y + console_render_size.y - dockScrollStart.y - 4.0f);
            (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                .id = std::string("console-dock-body-") + std::to_string(static_cast<int>(editor.dockStatusTab)),
                .size = { (std::max)(120.0f, console_render_size.x - 12.0f), dockScrollHeight },
                .draw_background = true,
                .show_scrollbar = true
            });
        }

        constexpr bool showDockEditorControls = false;
        switch (editor.dockStatusTab)
        {
        case EditorWorkspaceTab::Project:
        {
            const auto* activeProfile = editor_find_project_profile(editor.projectId);
            if (!activeProfile)
                activeProfile = &editor_default_project_profile();

            const std::filesystem::path outputExe = project_output_exe_path(editor.projectRoot);
            const std::filesystem::path buildLog = project_build_log_path(editor.projectRoot);

            const std::vector<std::string> dockLines{
                dockLine("[project] Active", activeProfile->display_name),
                dockLine("[project] Kind", editor.projectKind),
                dockLine("[project] Root", display_project_path(editor.projectRoot)),
                dockLine("[project] Scene", display_project_path(editor.projectScenePath)),
                dockLine("[project] Runtime", activeProfile->runtime_scene_id),
                dockLine("[project] Run backend", project_run_backend_label(editor.projectRunBackend)),
                dockLine("[project] Capability policy", activeProfile->renderer_capability.id),
                dockLine(
                    "[project] Run admission",
                    renderer_admission_summary(
                        renderer_backend_kind(editor.projectRunBackend),
                        activeProfile->renderer_capability)),
                dockLine("[project] Frame limit", frame_limit_label(editor.projectRunFrameLimitFps)),
                dockLine("[project] Output", display_project_path(outputExe)),
                dockLine("[project] Build log", display_project_path(buildLog)),
                dockLine("[project] Play target", path_exists(outputExe) ? "ready" : "build required"),
                "[project] Controls live in the central Project workspace. Bottom Dock is status-only."
            };
            renderDockStatusPanel("project", dockLines);
            break;
        }
        case EditorWorkspaceTab::Scripts:
        {
            const std::string activeScriptSource = editor_resolve_script_source_path(editor.activeScript, editor.projectRoot);
            const float scriptsContentWidth = (std::max)(180.0f, console_render_size.x - 24.0f);
            gui::property_row("[script] Active", editor.activeScript);
            gui::property_row("[script] Source", activeScriptSource);
            gui::property_row(
                "[script] Source exists",
                std::filesystem::exists(std::filesystem::path{ activeScriptSource }) ? "true" : "false");
            if (const auto* activeScript = active_script_profile(editor))
            {
                gui::property_row("[script] Build", activeScript->build_action);
                gui::property_row("[script] Run", activeScript->run_action);
                gui::property_row("[script] Hint", activeScript->diagnostic_hint);
            }
            gui::wrapped_label(
                "Scripts compile with the engine/project and use the editor host API for callbacks. Create project-local starters here and validate them with Build Selected Script. The centered Run button builds and launches the active project shell.",
                scriptsContentWidth);
            gui::wrapped_label(editor.scriptBuildStatus, scriptsContentWidth);

            gui::property_row("[script] New script", "type a safe id, then create a project-local .ascript.cpp");
            (void)gui::edit_box(editor.newScriptName, { scriptsContentWidth, 28.0f }, 64, false);
            if (showDockEditorControls && gui::button("Create Project Script Starter", { (std::min)(280.0f, scriptsContentWidth), 30.0f }))
                create_project_script_starter(editor);

            if (showDockEditorControls)
            {
                for (const auto& script : editor_script_profiles())
                {
                    const std::string buttonLabel = std::string(script.display_name);
                    const std::string resolvedSource = editor_resolve_script_source_path(script.id, editor.projectRoot);
                    if (gui::button(buttonLabel, { (std::max)(180.0f, console_render_size.x - 24.0f), 28.0f }))
                    {
                        editor.activeScript = std::string(script.id);
                        editor.scriptBuildStatus = std::string("Selected script source: ") + resolvedSource;
                        (void)load_script_source_editor(editor, resolvedSource, false);
                        push_editor_log(editor, std::string("[script] Selected ") + editor.activeScript + ".");
                    }
                    gui::property_row("  source", resolvedSource);
                    gui::wrapped_label(script.description, (std::max)(160.0f, console_render_size.x - 36.0f));
                }
            }

            const auto scriptEntries = collect_script_browser_entries(editor.projectRoot);
            gui::property_row("[script] Project/engine script files", std::to_string(scriptEntries.size()));
            if (showDockEditorControls)
            {
                for (const auto& entry : scriptEntries)
                {
                    if (gui::button(entry.label, { scriptsContentWidth, 28.0f }))
                    {
                        editor.activeScript = entry.kind;
                        editor.selectedProjectFile = entry.path;
                        editor.scriptBuildStatus = "Selected script source: " + entry.path;
                        push_editor_log(editor, "[script] Selected source " + entry.path);
                    }
                }
            }

            if (showDockEditorControls
                && gui::button(
                    editor.scriptBuildPending
                        ? "Cancel Script Build"
                        : "Build Selected Script",
                    { 180.0f, 30.0f }))
            {
                if (editor.scriptBuildPending)
                    (void)cancel_script_build(editor);
                else
                    start_script_build(editor, "Scripts dock build request");
            }

            const auto projectEntries = collect_project_browser_entries(editor.projectRoot);
            gui::property_row("[files] Active project browser", std::to_string(projectEntries.size()) + " visible entries");
            gui::wrapped_label("Console Dock is status-only. Use the central Asset Browser / Scripts surface for file browsing, script editing, and build actions.", scriptsContentWidth);
            if (showDockEditorControls)
            {
                for (const auto& entry : projectEntries)
                {
                    if (gui::button(entry.label, { scriptsContentWidth, 26.0f }))
                    {
                        editor.selectedProjectFile = entry.path;
                        if (entry.kind == "SCRIPT")
                        {
                            editor.activeScript = script_id_from_source_path(std::filesystem::path{ entry.path });
                            editor.scriptBuildStatus = "Selected project script: " + entry.path;
                            (void)load_script_source_editor(editor, entry.path, false);
                        }
                        push_editor_log(editor, "[files] Selected " + entry.path);
                    }
                }
                draw_script_source_editor(editor, activeScriptSource, scriptsContentWidth, 180.0f);
            }
            gui::property_row("[files] Selected", editor.selectedProjectFile.empty() ? std::string("(none)") : editor.selectedProjectFile);
            break;
        }
        case EditorWorkspaceTab::Assets:
        {
            const auto modelSummary = editor_project_model_summary(editor.projectId);
            const std::string activeScriptSource = editor_resolve_script_source_path(editor.activeScript, editor.projectRoot);
            const std::vector<std::string> dockLines{
                dockLine("[assets] Project", editor.projectName),
                dockLine("[assets] Project root", display_project_path(editor.projectRoot)),
                dockLine("[assets] Scene", display_project_path(editor.projectScenePath)),
                dockLine("[assets] Demo model", modelSummary.asset_path.empty() ? std::string("(none)") : modelSummary.asset_path),
                dockLine("[assets] Model path", modelSummary.resolved_path.empty() ? std::string("(unresolved)") : modelSummary.resolved_path),
                dockLine("[assets] Model parsed", modelSummary.parsed ? "true" : "false"),
                dockLine("[assets] Selected", editor.selectedAssetPath.empty() ? std::string("(none)") : editor.selectedAssetPath),
                dockLine("[script asset] Active", editor.activeScript),
                dockLine("[script asset] Source", activeScriptSource),
                dockLine("[script asset] Source exists", std::filesystem::exists(std::filesystem::path{ activeScriptSource }) ? "true" : "false"),
                "[assets] Use the central Asset Browser for file cards, script creation, validation, and source editing."
            };
            renderDockStatusPanel("assets", dockLines);
            break;
        }
#if EPOCH_EDITION_PUBLIC
        case EditorWorkspaceTab::AI:
        {
            const std::vector<std::string> dockLines{
                "Assistant",
                dockLine("Provider", ai::active_provider_summary()),
                dockLine("Model", ai::active_model_name().empty()
                    ? "None selected" : ai::active_model_name()),
                dockLine("Connection", ai::model_connection_status()),
                dockLine("Project", editor.projectName.empty()
                    ? "No project open" : editor.projectName),
                "Use Model Manager to choose a connection. Project changes are staged for your review."
            };
            renderDockStatusPanel("ai", dockLines);
            break;
        }
#endif
        case EditorWorkspaceTab::Systems:
        {
            const auto orderedSystems = epochengine::systems::Registry::instance().ordered_systems();
            const std::size_t hardwareThreadCount = (std::max)(std::size_t{ 1 },
                std::thread::hardware_concurrency() > 0
                ? static_cast<std::size_t>(std::thread::hardware_concurrency())
                : std::size_t{ 6 });
            const std::size_t liveThreadCount = epochengine::systems::threading::live_thread_count();
            const std::string supportTier = recommended_support_tier(ctx, hardwareThreadCount);
            const auto eventRenderDebug = epochengine::render_event_debug::snapshot();

            const std::vector<std::string> dockLines{
                dockLine("[systems] Renderer", renderer_name(ctx)),
                dockLine("[systems] Ownership model", backend_ownership_model(ctx)),
                dockLine("[systems] Preview camera", preview_camera_name(ctx)),
                dockLine("[systems] Runtime target", editor.activeRuntimeScene),
                dockLine("[systems] Contexts", context_backend_summary(available_context_backend_options())),
                dockLine("[context] Passive score", editor.passiveContextScoreStatus),
                dockLine("[context] Recommendation", editor.passiveContextRecommendation),
                dockLine("[systems] Registered systems", std::to_string(orderedSystems.size)),
                dockLine("[systems] Live threads", std::to_string(liveThreadCount)),
                dockLine("[systems] CPU threads", std::to_string(hardwareThreadCount)),
                dockLine("[systems] Support tier", supportTier),
                dockLine("[visual] Profile", std::string(epochengine::visuals::active_profile_name())),
                dockLine("[visual] Parity gate", std::string(epochengine::visuals::parity_gate())),
                dockLine("[renderer] Resource spine", renderer_resource_spine_summary(ctx)),
                dockLine("[renderer] Event path", std::string(epochengine::render_event_debug::path_name(eventRenderDebug.path))),
                dockLine("[renderer] Event fallback", std::string(epochengine::render_event_debug::fallback_name(eventRenderDebug.fallback))),
                dockLine("[renderer] Dirty regions", std::to_string(eventRenderDebug.dirty_regions)),
                dockLine("[renderer] Dirty coverage", epochengine::format_text("{:.2f}%", eventRenderDebug.dirty_coverage_percent)),
                dockLine("[renderer] Event render time", epochengine::format_text("{} us", eventRenderDebug.render_time_us)),
                dockLine("[renderer] A/B benchmark",
                    eventRenderDebug.selective.samples == 0u || eventRenderDebug.full_baseline.samples == 0u
                        ? std::string("off/warming")
                        : epochengine::format_text(
                            "Selective {:.1f} us avg / Full {:.1f} us avg / {:.2f}x",
                            eventRenderDebug.selective.average_time_us,
                            eventRenderDebug.full_baseline.average_time_us,
                            eventRenderDebug.average_speedup)),
                dockLine("[renderer] Proof stages", renderer_capability_proof_stage_status(ctx)),
                dockLine("[rtt] Descriptor", renderer_sampled_rtt_descriptor_status(ctx)),
                dockLine("[rtt] Graph", renderer_sampled_rtt_graph_status(ctx)),
                dockLine("[rtt] Hook/adaptor", renderer_sampled_rtt_hook_status(ctx)),
                dockLine("[rtt] Live allocation", renderer_sampled_rtt_live_status(ctx)),
                dockLine("[rtt] Presentation", renderer_sampled_rtt_presentation_status(ctx)),
                dockLine("[rtt] Overall", renderer_sampled_rtt_rollup_status(ctx)),
                dockLine("[renderer] Sampled RTT", renderer_native_sampled_rtt_status(ctx)),
                dockLine("[rtt] Scene surface", renderer_scene_sampled_surface_status(ctx)),
                dockLine("[renderer] Next gate", renderer_next_feature_gate(ctx)),
                dockLine("[build] Compiler", compiler_identity()),
                dockLine("[build] Configuration", build_configuration_label()),
                dockLine("[build] Compiled", __DATE__ " " __TIME__),
                dockLine("[build] Executable", core::path::executable_path().generic_string()),
                "[systems] Use the central System Info workspace for graph surfaces, backend details, and live system lists. Video owns time controls and timeline/video authoring."
            };
            renderDockStatusPanel("systems", dockLines);
            break;
        }
        case EditorWorkspaceTab::Output:
        default:
            {
                const gui::Vec2 outputCursor = gui::cursor_position();
                const float outputHeight = (std::max)(72.0f, console_render_pos.y + console_render_size.y - outputCursor.y - 12.0f);
                const std::string outputPanelId = epochengine::format_text(
                    "editor-output-log.{}.{}",
                    editor.projectId.empty() ? "no-project" : editor.projectId,
                    editor_main_surface_index(editor.mainSurface));
                const gui::ScrollTextPanelResult outputScroll =
                    gui::scroll_text_panel(gui::ScrollTextPanelOptions{
                    .id = outputPanelId,
                    .size = { (std::max)(180.0f, console_render_size.x - 24.0f), outputHeight },
                    .lines = editor.logLines,
                    .max_line_chars = 1024,
                    .selectable = true,
                    .stick_to_bottom = editor.outputFollowTail,
                    .scroll_to_end_generation =
                        editor.outputScrollToEndGeneration,
                    .show_scroll_to_end = true
                });
                editor.outputFollowTail = outputScroll.at_end;
            }
            break;
        }
        if (dockUsesOuterScroll)
            gui::end_scroll_area();
        gui::end_window();
        };

        auto render_active_console_in_region =
            [&](EditorToolDockRegion region)
        {
            const std::string_view route = active_pane_for_region(region);
            const auto pane = editor_tool_pane_from_route(route);
            if (!pane || !is_console_tool_pane(*pane))
                return;
            const std::string_view label =
                toolPaneLabels[tool_pane_index(*pane)];
            render_console_tool_pane(region, route, label);
        };
        render_active_console_in_region(EditorToolDockRegion::Left);
        render_active_console_in_region(EditorToolDockRegion::Right);
        render_active_console_in_region(EditorToolDockRegion::BottomLeft);
        render_active_console_in_region(EditorToolDockRegion::BottomRight);
        bool voiceChatVisibleThisFrame{};
