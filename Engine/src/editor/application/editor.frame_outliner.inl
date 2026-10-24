// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        auto render_outliner_window = [&](
            gui::Vec2 pane_pos,
            gui::Vec2 pane_size,
            OutlinerToolTab pane_tab,
            std::string_view pane_route,
            std::string_view pane_label)
        {
        if (pane_size.x > 1.0f && pane_size.y > 1.0f)
        {
        editor.outlinerToolTab = pane_tab;
        // The dock tab already owns the visible title; avoid a second title
        // taking content height and causing duplicate headings.
        gui::begin_window(pane_label, pane_pos, pane_size, true, false);
        handle_pane_title_drag_to_tab(
            pane_route, pane_label, pane_pos, pane_size);
        const gui::Vec2 outlinerScrollStart = gui::cursor_position();
        const float outlinerScrollHeight = (std::max)(
            48.0f,
            pane_pos.y + pane_size.y - outlinerScrollStart.y - 4.0f);
        (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
            .id = "world-outliner-body",
            .size = { (std::max)(80.0f, pane_size.x - 12.0f), outlinerScrollHeight },
            .draw_background = true,
            .show_scrollbar = true
        });
        const float outlinerWidth = (std::max)(150.0f, pane_size.x - 18.0f);

        if (editor.outlinerToolTab == OutlinerToolTab::World)
        {
        gui::label(std::string("Scene: ") + editor.activeWorld);
        gui::label(std::string("Entities: ") + std::to_string(editor.entities.size()));
        gui::label(std::string("Selected: ") + std::to_string(editor.selectedEntityIds.size()));
        gui::property_row("[outliner] Root", display_project_path(editor.projectRoot), 84.0f);
        gui::property_row("[outliner] Scene file", file_ready_summary(editor.projectScenePath), 104.0f);

        std::vector<editor_hierarchy::EntityRecord> hierarchyEntities{};
        hierarchyEntities.reserve(editor.entities.size());
        for (std::size_t index = 0u; index < editor.entities.size(); ++index)
        {
            const EditorEntity& entity = editor.entities[index];
            hierarchyEntities.push_back(editor_hierarchy::EntityRecord{
                .canonical_id = entity.sceneObjectId,
                .label = entity.name,
                .type = entity.type,
                .category = entity.category,
                .order = static_cast<std::uint64_t>(index),
                .visible = entity.visible,
                .locked = !application.allow_entity_authoring});
        }
        const editor_hierarchy::Result hierarchyRefresh =
            editor.worldHierarchy.refresh(
                editor_scene_identity(editor),
                hierarchyEntities,
                editor.selectedEntityId);
        if (!hierarchyRefresh)
        {
            editor.worldHierarchyStatus =
                "Hierarchy refresh rejected: "
                + std::string(editor_hierarchy::code_name(
                    hierarchyRefresh.code));
        }
        else
        {
            const editor_hierarchy::Snapshot hierarchySnapshot =
                editor.worldHierarchy.snapshot();
            if (hierarchySnapshot.filter != editor.worldHierarchyFilter)
            {
                const editor_hierarchy::Result filterResult =
                    editor.worldHierarchy.set_filter(
                        editor.worldHierarchyFilter);
                if (!filterResult)
                {
                    editor.worldHierarchyStatus =
                        "Hierarchy filter rejected: "
                        + std::string(editor_hierarchy::code_name(
                            filterResult.code));
                }
                else
                {
                    editor.worldHierarchyStatus = "Hierarchy synchronized.";
                }
            }
            else
            {
                editor.worldHierarchyStatus = "Hierarchy synchronized.";
            }
        }

        const auto route_selected_hierarchy_action =
            [&](editor_hierarchy::Action action) -> bool
        {
            const editor_hierarchy::NodeId selectedNode =
                editor.worldHierarchy.node_for_entity(
                    editor.selectedEntityId);
            const editor_hierarchy::Route route =
                editor.worldHierarchy.route(action, selectedNode);
            if (!route)
            {
                editor.worldHierarchyStatus =
                    "Hierarchy action rejected: "
                    + std::string(editor_hierarchy::code_name(route.code));
                return false;
            }
            return true;
        };

        if (application.allow_entity_authoring)
        {
            const bool worldSurfaceActive = workspace_command_surface(editor)
                == editor_workspace_commands::Surface::world;
            const std::array outlinerWorldButtons{
                gui::InlineButtonSpec{ .label = "+ Cube", .width = 72.0f, .enabled = worldSurfaceActive },
                gui::InlineButtonSpec{ .label = "+ Ground", .width = 86.0f, .enabled = worldSurfaceActive },
                gui::InlineButtonSpec{ .label = "+ Light", .width = 72.0f, .enabled = worldSurfaceActive }
            };
            if (const auto action = gui::inline_button_row(outlinerWorldButtons, 26.0f, 5.0f))
            {
                switch (*action)
                {
                case 0: add_entity(editor, "cube"); break;
                case 1: add_entity(editor, "ground"); break;
                case 2: add_entity(editor, "light"); break;
                default: break;
                }
            }
            const std::array outlinerBatchButtons{
                gui::InlineButtonSpec{ .label = "+ 4 Cubes", .width = 88.0f, .enabled = worldSurfaceActive },
                gui::InlineButtonSpec{ .label = "+ 8 Cubes", .width = 88.0f, .enabled = worldSurfaceActive }
            };
            if (const auto action = gui::inline_button_row(
                    outlinerBatchButtons, 26.0f, 5.0f))
            {
                (void)add_cube_batch(editor, *action == 0u ? 4u : 8u);
            }

            const std::array outlinerRuntimeButtons{
                gui::InlineButtonSpec{ .label = "+ Spawn", .width = 78.0f, .enabled = worldSurfaceActive },
                gui::InlineButtonSpec{ .label = "+ Camera", .width = 88.0f, .enabled = worldSurfaceActive }
            };
            if (const auto action = gui::inline_button_row(outlinerRuntimeButtons, 26.0f, 5.0f))
            {
                switch (*action)
                {
                case 0: add_entity(editor, "spawn"); break;
                case 1: add_entity(editor, "camera"); break;
                default: break;
                }
            }

            const std::array worldHistoryButtons{
                gui::InlineButtonSpec{ .label = "Undo", .width = 70.0f, .enabled = worldSurfaceActive && editor.sceneDocument.can_undo() },
                gui::InlineButtonSpec{ .label = "Redo", .width = 70.0f, .enabled = worldSurfaceActive && editor.sceneDocument.can_redo() }
            };
            if (const auto action = gui::inline_button_row(
                    worldHistoryButtons, 26.0f, 5.0f))
            {
                if (*action == 0u)
                    (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::undo);
                else
                    (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::redo);
            }

            const bool hasSelection = editor_has_selection(editor);
            const std::array outlinerMutationButtons{
                gui::InlineButtonSpec{ .label = "Duplicate", .width = 96.0f, .enabled = worldSurfaceActive && hasSelection },
                gui::InlineButtonSpec{ .label = "Delete", .width = 74.0f, .enabled = worldSurfaceActive && hasSelection }
            };
            if (const auto action = gui::inline_button_row(
                    outlinerMutationButtons, 26.0f, 5.0f))
            {
                if (*action == 0u
                    && route_selected_hierarchy_action(
                        editor_hierarchy::Action::duplicate_entity))
                    (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::duplicate_selection);
                else if (*action == 1u
                    && route_selected_hierarchy_action(
                        editor_hierarchy::Action::delete_entity))
                    (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::delete_selection);
            }
            const std::array outlinerSelectionButtons{
                gui::InlineButtonSpec{ .label = "Focus", .width = 70.0f, .enabled = worldSurfaceActive && hasSelection },
                gui::InlineButtonSpec{ .label = "Deselect", .width = 86.0f, .enabled = hasSelection }
            };
            if (const auto action = gui::inline_button_row(
                    outlinerSelectionButtons, 26.0f, 5.0f))
            {
                if (*action == 0u
                    && route_selected_hierarchy_action(
                        editor_hierarchy::Action::focus))
                    (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::focus_selection);
                else if (*action == 1u
                    && route_selected_hierarchy_action(
                        editor_hierarchy::Action::deselect))
                    clear_editor_selection(editor);
            }
        }
        else
        {
            gui::property_row("[outliner] Authoring", "Owned by this dedicated application scene", 92.0f);
            const bool hasSelection = editor_has_selection(editor);
            const std::array selectionActions{
                gui::InlineButtonSpec{ .label = "Focus", .width = 70.0f, .enabled = hasSelection },
                gui::InlineButtonSpec{ .label = "Deselect", .width = 86.0f, .enabled = hasSelection }
            };
            if (const auto action = gui::inline_button_row(
                    selectionActions, 26.0f, 5.0f))
            {
                if (*action == 0u
                    && route_selected_hierarchy_action(
                        editor_hierarchy::Action::focus))
                    handle_scene_tool(editor, ctx.get(), "focus_selection");
                else if (*action == 1u
                    && route_selected_hierarchy_action(
                        editor_hierarchy::Action::deselect))
                    clear_editor_selection(editor);
            }
        }

        gui::label("Hierarchy filter");
        (void)gui::edit_box(
            editor.worldHierarchyFilter,
            {outlinerWidth, 28.0f},
            160u,
            false);

        const float hierarchyRowHeight = 30.0f;
        const float hierarchyViewportHeight = (std::clamp)(
            outlinerScrollHeight * 0.55f, 112.0f, 420.0f);
        const editor_hierarchy::Snapshot hierarchySnapshot =
            editor.worldHierarchy.snapshot();
        const float hierarchyContentHeight = (std::max)(
            hierarchyViewportHeight,
            static_cast<float>(hierarchySnapshot.visible_rows)
                * hierarchyRowHeight);

        if (editor.selectedEntityId != editor.worldHierarchyScrollSelection)
        {
            const editor_hierarchy::ScrollPlan scroll =
                editor.worldHierarchy.scroll_to_entity(
                    editor.selectedEntityId,
                    editor_hierarchy::Viewport{
                        .row_height = hierarchyRowHeight,
                        .viewport_height = hierarchyViewportHeight,
                        .scroll_y = editor.worldHierarchyScrollY,
                        .overscan_rows = 2u});
            if (scroll)
                editor.worldHierarchyRequestedScrollY = scroll.scroll_y;
            editor.worldHierarchyScrollSelection = editor.selectedEntityId;
        }

        const gui::ScrollAreaResult hierarchyScroll = gui::begin_scroll_area(
            gui::ScrollAreaOptions{
                .id = "world-outliner-hierarchy-tree",
                .size = {outlinerWidth, hierarchyViewportHeight},
                .content_height = hierarchyContentHeight,
                .requested_scroll_y = editor.worldHierarchyRequestedScrollY,
                .draw_background = true,
                .show_scrollbar = true,
                .capture_wheel = true});
        editor.worldHierarchyRequestedScrollY = -1.0f;
        editor.worldHierarchyScrollY = hierarchyScroll.scroll_y;
        const editor_hierarchy::LayoutPlan hierarchyLayout =
            editor.worldHierarchy.plan_rows(editor_hierarchy::Viewport{
                .row_height = hierarchyRowHeight,
                .viewport_height = hierarchyViewportHeight,
                .scroll_y = hierarchyScroll.scroll_y,
                .overscan_rows = 2u});
        const gui::Vec2 hierarchyOrigin = gui::cursor_position();
        const std::span<const editor_hierarchy::RowView> hierarchyRows =
            editor.worldHierarchy.rows();
        if (hierarchyLayout)
        {
            for (const editor_hierarchy::LayoutRow& layoutRow
                : hierarchyLayout.rows)
            {
                if (layoutRow.row_index >= hierarchyRows.size())
                    continue;
                const editor_hierarchy::RowView& row =
                    hierarchyRows[layoutRow.row_index];
                gui::set_cursor({
                    hierarchyOrigin.x,
                    hierarchyOrigin.y + layoutRow.top});
                std::string label(row.depth * 2u, ' ');
                if (row.group)
                {
                    label += row.has_children
                        ? (row.expanded ? "[-] " : "[+] ")
                        : "    ";
                }
                else
                {
                    label += row.selected ? "> " : "  ";
                }
                label += row.label;
                if (!row.detail.empty())
                    label += "  |  " + row.detail;
                if (row.locked)
                    label += "  [locked]";

                if (!gui::button(
                        label,
                        {outlinerWidth - 12.0f, hierarchyRowHeight - 2.0f}))
                {
                    continue;
                }

                if (row.group)
                {
                    const editor_hierarchy::Result toggled =
                        editor.worldHierarchy.toggle_expanded(row.node_id);
                    if (!toggled)
                    {
                        editor.worldHierarchyStatus =
                            "Hierarchy expansion rejected: "
                            + std::string(editor_hierarchy::code_name(
                                toggled.code));
                    }
                    continue;
                }

                const editor_hierarchy::Route routed =
                    editor.worldHierarchy.route(
                        editor_hierarchy::Action::select,
                        row.node_id);
                if (!routed || routed.targets.empty())
                {
                    editor.worldHierarchyStatus =
                        "Hierarchy selection rejected: "
                        + std::string(editor_hierarchy::code_name(
                            routed.code));
                    continue;
                }
                if (const auto index = editor_entity_index(
                        editor, routed.targets.front()))
                {
                    select_editor_entity(editor, *index);
                    (void)editor.worldHierarchy.synchronize_selection(
                        routed.targets.front());
                    editor.worldHierarchyScrollSelection =
                        routed.targets.front();
                    push_editor_log(
                        editor,
                        std::string("[select] ") + row.label);
                }
            }
        }
        gui::set_cursor({
            hierarchyOrigin.x,
            hierarchyOrigin.y + hierarchyContentHeight});
        gui::end_scroll_area();
        gui::property_row(
            "Hierarchy",
            editor.worldHierarchyStatus,
            84.0f);
        }
        else if (editor.outlinerToolTab == OutlinerToolTab::Assets)
        {
            if (editor.mainSurface == EditorMainSurface::Assets)
            {
                gui::label("Asset Navigator");
                gui::property_row(
                    "Project",
                    editor.projectName,
                    70.0f);
                gui::property_row(
                    "Folder",
                    editor.assetBrowserFolder.empty()
                        ? std::string{"Project"}
                        : editor.assetBrowserFolder,
                    70.0f);

                const std::array assetSections{
                    gui::TabButtonSpec{
                        .id = "outliner.asset_browser",
                        .label = "Browse",
                        .width = 64.0f,
                        .active = editor.assetWorkspaceSection == 0u},
                    gui::TabButtonSpec{
                        .id = "outliner.asset_textures",
                        .label = "Textures",
                        .width = 72.0f,
                        .active = editor.assetWorkspaceSection == 1u},
                    gui::TabButtonSpec{
                        .id = "outliner.asset_scripts",
                        .label = "Scripts",
                        .width = 64.0f,
                        .active = editor.assetWorkspaceSection == 2u},
                    gui::TabButtonSpec{
                        .id = "outliner.asset_graph",
                        .label = "Graph",
                        .width = 58.0f,
                        .active = editor.assetWorkspaceSection == 3u},
                    gui::TabButtonSpec{
                        .id = "outliner.asset_audio",
                        .label = "Audio",
                        .width = 62.0f,
                        .active = editor.assetWorkspaceSection == 4u}
                };
                if (const auto section = gui::tab_bar_buttons(
                        assetSections, 27.0f, 1.0f);
                    section.selected_index)
                {
                    editor.assetWorkspaceSection =
                        *section.selected_index;
                }

                if (editor.assetWorkspaceSection == 0u)
                {
                    (void)gui::edit_box(
                        editor.assetSearch,
                        {outlinerWidth, 28.0f},
                        96u,
                        false);
                    if (!editor.assetBrowserFolder.empty()
                        && gui::button("Up One Folder", {128.0f, 26.0f}))
                    {
                        leave_asset_browser_folder(editor);
                    }

                    const auto entries = collect_asset_manager_entries(
                        editor.projectRoot,
                        editor.assetBrowserFolder,
                        512u);
                    for (const auto& entry : entries)
                    {
                        if (!asset_category_visible(
                                editor.assetCategory, entry.kind))
                        {
                            continue;
                        }
                        if (gui::button_selected(
                                entry.label,
                                {outlinerWidth, 27.0f},
                                editor.selectedProjectFile
                                    == entry.path))
                        {
                            if (entry.directory)
                            {
                                enter_asset_browser_folder(editor, entry);
                                break;
                            }
                            editor.selectedProjectFile = entry.path;
                            editor.selectedAssetPath = entry.path;
                            if (entry.kind == "SCRIPT")
                            {
                                editor.activeScript =
                                    script_id_from_source_path(
                                        std::filesystem::path{
                                            entry.path});
                                editor.assetWorkspaceSection = 2u;
                                (void)load_script_source_editor(
                                    editor, entry.path, false);
                            }
                            else if (entry.kind == "AUDIO")
                            {
                                const auto audio =
                                    editor_project_audio_profile_summary(
                                        editor.projectId);
                                const auto cue = std::find_if(
                                    audio.cues.begin(),
                                    audio.cues.end(),
                                    [&entry](const auto& candidate)
                                    {
                                        return candidate.logical_path
                                            == entry.path;
                                    });
                                editor.selectedProjectAudioCueId =
                                    cue == audio.cues.end()
                                        ? 0u
                                        : cue->stable_id;
                                editor.assetWorkspaceSection = 4u;
                            }
                        }
                    }
                }

                gui::property_row(
                    "Selected",
                    editor.selectedAssetPath.empty()
                        ? std::string{"None"}
                        : editor.selectedAssetPath,
                    70.0f);
                gui::end_scroll_area();
                gui::end_window();
                return;
            }
            gui::label(std::string("Assets: ") + editor.projectName);
            gui::property_row("Root", display_project_path(editor.projectRoot), 64.0f);
            gui::property_row(
                "Folder",
                editor.assetBrowserFolder.empty()
                    ? std::string{"Project"}
                    : editor.assetBrowserFolder,
                64.0f);
            if (!editor.assetBrowserFolder.empty()
                && gui::button("Up", {54.0f, 26.0f}))
            {
                leave_asset_browser_folder(editor);
            }

            draw_asset_category_tabs(editor, false);
            (void)gui::edit_box(
                editor.assetSearch,
                {outlinerWidth, 28.0f},
                96u,
                false);

            draw_texture_source_controls(
                editor,
                ctx,
                outlinerWidth);

            auto* textureController =
                project_texture_controller(editor);
            if (textureController)
            {
                const auto catalog = textureController->catalog();
                const auto metrics = textureController->metrics();
                gui::property_row(
                    "Compiled",
                    epochengine::format_text(
                        "{} | {} KiB",
                        catalog.size(),
                        metrics.decoded_bytes / 1024u
                            + (metrics.decoded_bytes % 1024u != 0u
                                ? 1u
                                : 0u)),
                    76.0f);
                draw_project_texture_catalog(
                    editor,
                    ctx,
                    *textureController,
                    outlinerWidth,
                    "outliner.assets",
                    false);
                draw_active_texture_authoring(
                    editor,
                    ctx,
                    outlinerWidth,
                    "outliner.assets",
                    176.0f);
            }

            draw_texture_application_controls(editor, outlinerWidth);

            const auto containsNoCase = [](
                std::string_view value,
                std::string_view query) noexcept
            {
                if (query.empty())
                    return true;
                const auto equal = [](char left, char right) noexcept
                {
                    return std::tolower(
                        static_cast<unsigned char>(left))
                        == std::tolower(
                            static_cast<unsigned char>(right));
                };
                return std::search(
                    value.begin(), value.end(),
                    query.begin(), query.end(),
                    equal) != value.end();
            };
            const auto entries = collect_asset_manager_entries(
                editor.projectRoot,
                editor.assetBrowserFolder,
                512u);
            std::size_t visibleCount = 0u;
            for (const auto& entry : entries)
            {
                const bool categoryVisible = asset_category_visible(
                    editor.assetCategory,
                    entry.kind);
                if (!categoryVisible
                    || (!containsNoCase(entry.label, editor.assetSearch)
                        && !containsNoCase(entry.path, editor.assetSearch)))
                {
                    continue;
                }
                ++visibleCount;
                if (gui::button_selected(
                        entry.label,
                        {outlinerWidth, 27.0f},
                        editor.selectedProjectFile == entry.path))
                {
                    if (entry.directory)
                    {
                        enter_asset_browser_folder(editor, entry);
                        break;
                    }
                    editor.selectedProjectFile = entry.path;
                    editor.selectedAssetPath = entry.path;
                    if (entry.kind == "IMAGE")
                    {
                        editor.projectTextureStatus =
                            "PNG/JPEG is visible as a project reference; texture compilation currently accepts BMP, TGA, PPM, and .epoch_texture sources.";
                    }
                    if (entry.kind == "SCRIPT")
                    {
                        editor.activeScript =
                            script_id_from_source_path(
                                std::filesystem::path{entry.path});
                        (void)load_script_source_editor(
                            editor, entry.path, false);
                        editor.outlinerToolTab =
                            OutlinerToolTab::Scripting;
                    }
                }
            }
            gui::property_row(
                "Visible",
                epochengine::format_text(
                    "{} / {}", visibleCount, entries.size()),
                64.0f);
        }
        else if (editor.outlinerToolTab == OutlinerToolTab::Gui)
        {
            const auto isGuiEntity = [](const EditorEntity& entity) noexcept
            {
                return is_gui_authoring_entity(entity);
            };
            auto* guiDocument = synchronize_gui_document(editor);
            const auto guiMetrics = guiDocument
                ? guiDocument->metrics()
                : authoring::gui::DocumentMetrics{};
            const std::size_t widgetCount = guiMetrics.active_widgets > 0u
                ? guiMetrics.active_widgets - 1u
                : 0u;

            const bool fullGuiAuthoring =
                editor.applicationKind
                    == EditorApplicationKind::GuiEditor;
            if (!fullGuiAuthoring
                && editor.guiAuthoringSection != 1u
                && editor.guiAuthoringSection != 2u)
            {
                editor.guiAuthoringSection = 1u;
            }

            gui::label(
                fullGuiAuthoring
                    ? "GUI Editor"
                    : "GUI Placement");
            gui::property_row(
                "Revision",
                std::to_string(guiMetrics.revision_sequence),
                76.0f);
            gui::property_row(
                "Widgets",
                std::to_string(widgetCount),
                76.0f);
            gui::property_row(
                "Source",
                editor.guiDocumentSourcePath.empty()
                    ? std::string{"Unsaved GUI document"}
                    : editor.guiDocumentSourcePath,
                58.0f);

            if (fullGuiAuthoring)
            {
                const bool projectOpen =
                    has_open_project(editor);
                gui::property_row(
                    "Project",
                    projectOpen
                        ? editor.projectName
                        : std::string{"No project open"},
                    72.0f);
                const float projectOpenWidth =
                    (std::max)(96.0f, outlinerWidth * 0.61f);
                const float projectCloseWidth =
                    (std::max)(
                        76.0f,
                        outlinerWidth - projectOpenWidth - 4.0f);
                const std::array projectActions{
                    gui::InlineButtonSpec{
                        .label = projectOpen
                            ? "Open Another..."
                            : "Open Project...",
                        .width = projectOpenWidth},
                    gui::InlineButtonSpec{
                        .label = "Close",
                        .width = projectCloseWidth,
                        .enabled = projectOpen}
                };
                if (const auto action =
                        gui::inline_button_row(
                            projectActions,
                            27.0f,
                            4.0f))
                {
                    if (*action == 0u)
                    {
                        (void)open_project_manifest_from_dialog(
                            editor,
                            ctx);
                    }
                    else
                    {
                        (void)editor_close_project(ctx);
                    }
                }

                const std::array newOpenActions{
                    gui::InlineButtonSpec{
                        .label = "New Blank",
                        .width = outlinerWidth * 0.48f},
                    gui::InlineButtonSpec{
                        .label = "Open GUI...",
                        .width = outlinerWidth * 0.48f}
                };
                if (const auto action =
                        gui::inline_button_row(
                            newOpenActions,
                            27.0f,
                            4.0f))
                {
                    if (*action == 0u)
                    {
                        (void)apply_gui_template(
                            editor,
                            GuiTemplatePreset::blank_canvas);
                    }
                    else
                    {
                        (void)open_gui_document_from_dialog(
                            editor);
                    }
                }

                const std::array saveReloadActions{
                    gui::InlineButtonSpec{
                        .label = "Save GUI",
                        .width = outlinerWidth * 0.48f,
                        .enabled = projectOpen},
                    gui::InlineButtonSpec{
                        .label = "Reload",
                        .width = outlinerWidth * 0.48f,
                        .enabled = projectOpen}
                };
                if (const auto action =
                        gui::inline_button_row(
                            saveReloadActions,
                            27.0f,
                            4.0f))
                {
                    if (*action == 0u)
                    {
                        (void)project_gui_document_layout(
                            editor);
                        const bool sceneSaved =
                            save_editor_scene_snapshot(editor);
                        const bool guiSaved =
                            save_project_gui_document(editor);
                        push_editor_log(
                            editor,
                            sceneSaved && guiSaved
                                ? "[gui] Project GUI and placement projection saved."
                                : "[gui] Save requires an open project with scene and GUI destinations.");
                    }
                    else
                    {
                        (void)reload_project_gui_document(
                            editor);
                    }
                }

                constexpr std::array<std::string_view, 5>
                    templateLabels{
                        "Blank Canvas",
                        "Desktop App",
                        "Dashboard",
                        "Mobile App",
                        "Game HUD"};
                const std::size_t templateIndex = (std::min)(
                    editor.guiTemplatePreset,
                    templateLabels.size() - 1u);
                const auto templateSelect =
                    gui::select_box(gui::SelectBoxOptions{
                        .id = "gui-template-preset",
                        .placeholder = "Choose template",
                        .selected =
                            templateLabels[templateIndex],
                        .options = templateLabels,
                        .size = {outlinerWidth, 28.0f},
                        .row_height = 28.0f,
                        .max_visible_options =
                            templateLabels.size()});
                if (templateSelect.selected_index)
                {
                    editor.guiTemplatePreset =
                        *templateSelect.selected_index;
                    editor.guiDocumentReplaceArmed = false;
                }
                gui::wrapped_label(
                    kGuiTemplateCatalog[templateIndex]
                        .purpose,
                    outlinerWidth);
                const std::array templateActions{
                    gui::InlineButtonSpec{
                        .label = editor.guiDocumentReplaceArmed
                            ? "Confirm Apply"
                            : "Apply Template",
                        .width = outlinerWidth * 0.54f},
                    gui::InlineButtonSpec{
                        .label = "Save Template...",
                        .width = outlinerWidth * 0.42f}
                };
                if (const auto action =
                        gui::inline_button_row(
                            templateActions,
                            27.0f,
                            4.0f))
                {
                    if (*action == 0u)
                    {
                        (void)apply_gui_template(
                            editor,
                            kGuiTemplateCatalog[
                                templateIndex].preset);
                    }
                    else
                    {
                        (void)save_gui_template_from_dialog(
                            editor);
                    }
                }
            }
            else
            {
                const std::array placementActions{
                    gui::InlineButtonSpec{
                        .label = "Save Placement",
                        .width = outlinerWidth * 0.62f,
                        .enabled =
                            has_open_project(editor)},
                    gui::InlineButtonSpec{
                        .label = "Reload",
                        .width = outlinerWidth * 0.34f,
                        .enabled =
                            has_open_project(editor)}
                };
                if (const auto action =
                        gui::inline_button_row(
                            placementActions,
                            27.0f,
                            4.0f))
                {
                    if (*action == 0u)
                    {
                        (void)project_gui_document_layout(
                            editor);
                        const bool sceneSaved =
                            save_editor_scene_snapshot(editor);
                        const bool guiSaved =
                            save_project_gui_document(editor);
                        push_editor_log(
                            editor,
                            sceneSaved && guiSaved
                                ? "[gui] GUI placement saved."
                                : "[gui] Placement save requires scene and GUI source evidence.");
                    }
                    else
                    {
                        (void)reload_project_gui_document(
                            editor);
                    }
                }
            }

            gui::wrapped_label(
                editor.guiDocumentStatus,
                outlinerWidth);

            if (fullGuiAuthoring)
            {
                const std::array sections{
                    gui::TabButtonSpec{ .id = "gui.widgets", .label = "Widgets", .width = 76.0f, .active = editor.guiAuthoringSection == 0u },
                    gui::TabButtonSpec{ .id = "gui.layout", .label = "Layout", .width = 70.0f, .active = editor.guiAuthoringSection == 1u },
                    gui::TabButtonSpec{ .id = "gui.hierarchy", .label = "Hierarchy", .width = 86.0f, .active = editor.guiAuthoringSection == 2u },
                    gui::TabButtonSpec{ .id = "gui.flow", .label = "Flow", .width = 54.0f, .active = editor.guiAuthoringSection == 3u }
                };
                if (const auto tabs =
                        gui::tab_bar_buttons(
                            sections,
                            29.0f,
                            1.0f);
                    tabs.selected_index)
                {
                    editor.guiAuthoringSection =
                        *tabs.selected_index;
                }
            }
            else
            {
                const std::array placementSections{
                    gui::TabButtonSpec{ .id = "gui.placement", .label = "Placement", .width = 92.0f, .active = editor.guiAuthoringSection == 1u },
                    gui::TabButtonSpec{ .id = "gui.hierarchy", .label = "Hierarchy", .width = 92.0f, .active = editor.guiAuthoringSection == 2u }
                };
                if (const auto tabs =
                        gui::tab_bar_buttons(
                            placementSections,
                            29.0f,
                            1.0f);
                    tabs.selected_index)
                {
                    editor.guiAuthoringSection =
                        *tabs.selected_index == 0u
                            ? 1u
                            : 2u;
                }
            }

            const std::array documentHistoryActions{
                gui::InlineButtonSpec{
                    .label = "Undo",
                    .width = 62.0f,
                    .enabled = guiDocument && guiDocument->can_undo()},
                gui::InlineButtonSpec{
                    .label = "Redo",
                    .width = 62.0f,
                    .enabled = guiDocument && guiDocument->can_redo()},
                gui::InlineButtonSpec{
                    .label = "Delete",
                    .width = 70.0f,
                    .enabled = guiDocument
                        && selected_gui_widget(editor).has_value()}
            };
            if (const auto action = gui::inline_button_row(
                    documentHistoryActions, 27.0f, 4.0f))
            {
                if (*action == 0u)
                    (void)undo_gui_document(editor);
                else if (*action == 1u)
                    (void)redo_gui_document(editor);
                else if (guiDocument)
                    (void)remove_selected_gui_widget(editor, *guiDocument);
            }

            if (editor_has_selection(editor)
                && gui::button(
                    "Deselect Widget",
                    {outlinerWidth, 27.0f}))
            {
                clear_editor_selection(editor);
                push_editor_log(editor, "[edit] GUI widget deselected.");
            }

            if (fullGuiAuthoring && editor.guiAuthoringSection == 0u)
            {
                const std::array primaryActions{
                    gui::InlineButtonSpec{ .label = "+ Panel", .width = 72.0f },
                    gui::InlineButtonSpec{ .label = "+ Button", .width = 76.0f },
                    gui::InlineButtonSpec{ .label = "+ Text", .width = 64.0f }
                };
                if (const auto action = gui::inline_button_row(
                        primaryActions, 26.0f, 4.0f))
                {
                    constexpr std::array<std::string_view, 3> archetypes{
                        "gui_panel", "gui_button", "gui_text"};
                    add_entity(editor, archetypes[*action]);
                }
                const std::array visualActions{
                    gui::InlineButtonSpec{ .label = "+ Image", .width = 72.0f },
                    gui::InlineButtonSpec{ .label = "+ Image Button", .width = 116.0f },
                    gui::InlineButtonSpec{ .label = "+ Tabs", .width = 68.0f }
                };
                if (const auto action = gui::inline_button_row(
                        visualActions, 26.0f, 4.0f))
                {
                    constexpr std::array<std::string_view, 3> archetypes{
                        "gui_image", "gui_image_button", "gui_tabs"};
                    add_entity(editor, archetypes[*action]);
                }
                const std::array inputActions{
                    gui::InlineButtonSpec{ .label = "+ Input", .width = 72.0f },
                    gui::InlineButtonSpec{ .label = "+ Slider", .width = 72.0f },
                    gui::InlineButtonSpec{ .label = "+ Scroll", .width = 76.0f }
                };
                if (const auto action = gui::inline_button_row(
                        inputActions, 26.0f, 4.0f))
                {
                    constexpr std::array<std::string_view, 3> archetypes{
                        "gui_input", "gui_slider", "gui_scroll"};
                    add_entity(editor, archetypes[*action]);
                }
            }
            else if (editor.guiAuthoringSection == 1u)
            {
                const auto selected = editor_entity_index(
                    editor, editor.selectedEntityId);
                if (!selected
                    || !isGuiEntity(editor.entities[*selected]))
                {
                    gui::wrapped_label(
                        "Select a GUI widget in Hierarchy or on the canvas.",
                        outlinerWidth);
                }
                else
                {
                    const EditorEntity current =
                        editor.entities[*selected];
                    gui::property_row(
                        "Selected", current.name, 72.0f);
                    gui::property_row(
                        "Position",
                        epochengine::format_text(
                            "{:.2f}, {:.2f}",
                            current.position[0],
                            current.position[1]),
                        72.0f);
                    gui::property_row(
                        "Size",
                        epochengine::format_text(
                            "{:.2f} x {:.2f}",
                            current.scale[0],
                            current.scale[1]),
                        72.0f);
                    const auto commitTransform = [&](
                        float dx,
                        float dy,
                        float dw,
                        float dh)
                    {
                        EditorEntity changed = current;
                        changed.position[0] += dx;
                        changed.position[1] += dy;
                        changed.scale[0] = (std::max)(
                            0.20f, changed.scale[0] + dw);
                        changed.scale[1] = (std::max)(
                            0.20f, changed.scale[1] + dh);
                        (void)transform_editor_scene_entity(
                            editor,
                            changed,
                            "Edit GUI widget layout");
                    };
                    const std::array moveActions{
                        gui::InlineButtonSpec{ .label = "Left", .width = 62.0f },
                        gui::InlineButtonSpec{ .label = "Right", .width = 62.0f },
                        gui::InlineButtonSpec{ .label = "Up", .width = 54.0f },
                        gui::InlineButtonSpec{ .label = "Down", .width = 62.0f }
                    };
                    if (const auto action = gui::inline_button_row(
                            moveActions, 26.0f, 4.0f))
                    {
                        constexpr std::array<std::array<float, 2>, 4> deltas{{
                            {-0.10f, 0.0f},
                            {0.10f, 0.0f},
                            {0.0f, 0.10f},
                            {0.0f, -0.10f}
                        }};
                        commitTransform(
                            deltas[*action][0],
                            deltas[*action][1],
                            0.0f,
                            0.0f);
                    }
                    const std::array sizeActions{
                        gui::InlineButtonSpec{ .label = "Width -", .width = 76.0f },
                        gui::InlineButtonSpec{ .label = "Width +", .width = 76.0f },
                        gui::InlineButtonSpec{ .label = "Height -", .width = 80.0f },
                        gui::InlineButtonSpec{ .label = "Height +", .width = 80.0f }
                    };
                    if (const auto action = gui::inline_button_row(
                            sizeActions, 26.0f, 4.0f))
                    {
                        constexpr std::array<std::array<float, 2>, 4> deltas{{
                            {-0.10f, 0.0f},
                            {0.10f, 0.0f},
                            {0.0f, -0.10f},
                            {0.0f, 0.10f}
                        }};
                        commitTransform(
                            0.0f, 0.0f,
                            deltas[*action][0],
                            deltas[*action][1]);
                    }
                    const bool imageWidget =
                        current.type == "GuiImage"
                        || current.type == "GuiImageButton";
                    const std::array materialActions{
                        gui::InlineButtonSpec{
                            .label = "Assign Image",
                            .width = 106.0f,
                            .enabled = imageWidget},
                        gui::InlineButtonSpec{
                            .label = "Clear Image",
                            .width = 96.0f,
                            .enabled = imageWidget},
                        gui::InlineButtonSpec{
                            .label = "Focus",
                            .width = 64.0f}
                    };
                    if (const auto action = gui::inline_button_row(
                            materialActions, 26.0f, 4.0f))
                    {
                        if (*action == 0u)
                        {
                            assign_selected_project_texture(editor);
                            editor.guiContentEditorEntityId =
                                scene::kInvalidSceneObjectId;
                        }
                        else if (*action == 1u)
                        {
                            clear_selected_project_texture(editor);
                            editor.guiContentEditorEntityId =
                                scene::kInvalidSceneObjectId;
                        }
                        else
                            handle_scene_tool(
                                editor,
                                ctx.get(),
                                "focus_selection");
                    }

                    if (guiDocument)
                    {
                        const auto mapped = editor.guiDocumentWidgets.find(
                            current.sceneObjectId);
                        const auto widget = mapped
                                == editor.guiDocumentWidgets.end()
                            ? std::optional<authoring::gui::WidgetView>{}
                            : guiDocument->widget(mapped->second);
                        if (widget)
                        {
                            hydrate_gui_content_editor(
                                editor,
                                current.sceneObjectId,
                                *widget);
                            gui::label("Content");
                            const auto contentKind =
                                widget->descriptor.kind;
                            const bool hasPrimaryContent =
                                contentKind
                                    != authoring::gui::WidgetKind::canvas
                                && contentKind
                                    != authoring::gui::WidgetKind::scroll_area
                                && contentKind
                                    != authoring::gui::WidgetKind::tab_set;
                            const bool hasSecondaryContent =
                                contentKind
                                    == authoring::gui::WidgetKind::image
                                || contentKind
                                    == authoring::gui::WidgetKind::image_button
                                || contentKind
                                    == authoring::gui::WidgetKind::text_input;
                            if (hasPrimaryContent)
                            {
                                gui::property_row(
                                    contentKind
                                            == authoring::gui::WidgetKind::image
                                        || contentKind
                                            == authoring::gui::WidgetKind::image_button
                                        ? "Source"
                                        : "Value",
                                    editor.guiContentPrimary.empty()
                                        ? std::string{"(empty)"}
                                        : editor.guiContentPrimary,
                                    62.0f);
                                (void)gui::edit_box(
                                    editor.guiContentPrimary,
                                    {outlinerWidth, 28.0f},
                                    contentKind
                                            == authoring::gui::WidgetKind::image
                                        || contentKind
                                            == authoring::gui::WidgetKind::image_button
                                        ? 1'024u
                                        : 8'192u,
                                    contentKind
                                        == authoring::gui::WidgetKind::text);
                            }
                            if (hasSecondaryContent)
                            {
                                gui::property_row(
                                    contentKind
                                            == authoring::gui::WidgetKind::text_input
                                        ? "Placeholder"
                                        : contentKind
                                            == authoring::gui::WidgetKind::image
                                        ? "Alt text"
                                        : "Label",
                                    editor.guiContentSecondary.empty()
                                        ? std::string{"(empty)"}
                                        : editor.guiContentSecondary,
                                    78.0f);
                                (void)gui::edit_box(
                                    editor.guiContentSecondary,
                                    {outlinerWidth, 28.0f},
                                    2'048u,
                                    false);
                            }
                            gui::property_row(
                                "Action",
                                editor.guiActionEditor.empty()
                                    ? std::string{"(none)"}
                                    : editor.guiActionEditor,
                                62.0f);
                            (void)gui::edit_box(
                                editor.guiActionEditor,
                                {outlinerWidth, 28.0f},
                                256u,
                                false);
                            gui::property_row(
                                "Tooltip",
                                editor.guiTooltipEditor.empty()
                                    ? std::string{"(none)"}
                                    : editor.guiTooltipEditor,
                                62.0f);
                            (void)gui::edit_box(
                                editor.guiTooltipEditor,
                                {outlinerWidth, 28.0f},
                                1'024u,
                                false);
                            if (gui::button(
                                    "Apply Content",
                                    {128.0f, 27.0f}))
                            {
                                const bool changed =
                                    apply_gui_content_editor(
                                        editor,
                                        *guiDocument,
                                        *widget);
                                if (changed)
                                {
                                    (void)project_gui_document_layout(editor);
                                }
                                editor.guiDocumentStatus = changed
                                    ? "Widget content and action updated and reprojected."
                                    : "Widget content and action were unchanged.";
                            }

                            if (contentKind
                                    == authoring::gui::WidgetKind::image
                                || contentKind
                                    == authoring::gui::WidgetKind::image_button)
                            {
                                gui::label("Project Texture Picker");
                                if (auto* textures =
                                        project_texture_controller(editor))
                                {
                                    draw_project_texture_catalog(
                                        editor,
                                        ctx,
                                        *textures,
                                        outlinerWidth,
                                        "gui.image_picker",
                                        false);
                                    const auto* selectedTexture =
                                        textures->selected();
                                    const std::array imageActions{
                                        gui::InlineButtonSpec{
                                            .label = "Use Selected",
                                            .width = 108.0f,
                                            .enabled =
                                                selectedTexture != nullptr},
                                        gui::InlineButtonSpec{
                                            .label = "Open Texture",
                                            .width = 108.0f,
                                            .enabled =
                                                selectedTexture != nullptr}
                                    };
                                    if (const auto imageAction =
                                            gui::inline_button_row(
                                                imageActions,
                                                27.0f,
                                                4.0f))
                                    {
                                        if (*imageAction == 0u
                                            && selectedTexture)
                                        {
                                            editor.guiContentPrimary =
                                                selectedTexture->logical_path;
                                            const bool changed =
                                                apply_gui_content_editor(
                                                    editor,
                                                    *guiDocument,
                                                    *widget);
                                            if (changed)
                                            {
                                                (void)project_gui_document_layout(
                                                    editor);
                                            }
                                            editor.guiDocumentStatus = changed
                                                ? "Selected project texture applied to the GUI widget."
                                                : "The GUI widget already uses the selected texture.";
                                        }
                                        else if (*imageAction == 1u)
                                        {
                                            editor.assetWorkspaceSection = 1u;
                                            open_editor_surface(
                                                EditorMainSurface::Assets,
                                                "GUI image picker");
                                        }
                                    }
                                }

                                bool preserveAspect =
                                    contentKind
                                        == authoring::gui::WidgetKind::image
                                    ? std::get<
                                        authoring::gui::ImageContent>(
                                            widget->descriptor.content)
                                        .preserve_aspect
                                    : std::get<
                                        authoring::gui::ImageButtonContent>(
                                            widget->descriptor.content)
                                        .preserve_aspect;
                                if (gui::toggle_switch(
                                        "Preserve aspect",
                                        preserveAspect,
                                        {outlinerWidth, 27.0f}))
                                {
                                    auto content =
                                        widget->descriptor.content;
                                    if (auto* image = std::get_if<
                                            authoring::gui::ImageContent>(
                                                &content))
                                    {
                                        image->preserve_aspect =
                                            preserveAspect;
                                    }
                                    else if (auto* imageButton =
                                        std::get_if<
                                            authoring::gui::
                                                ImageButtonContent>(
                                                    &content))
                                    {
                                        imageButton->preserve_aspect =
                                            preserveAspect;
                                    }
                                    if (guiDocument->update_content(
                                            widget->handle,
                                            std::move(content)))
                                    {
                                        (void)project_gui_document_layout(
                                            editor);
                                        editor.guiDocumentStatus =
                                            "Image fitting policy updated.";
                                    }
                                }
                            }
                            else if (contentKind
                                == authoring::gui::WidgetKind::text)
                            {
                                auto content = std::get<
                                    authoring::gui::TextContent>(
                                        widget->descriptor.content);
                                if (gui::toggle_switch(
                                        "Word wrap",
                                        content.wrap,
                                        {outlinerWidth, 27.0f}))
                                {
                                    (void)guiDocument->update_content(
                                        widget->handle,
                                        content);
                                    (void)project_gui_document_layout(editor);
                                    editor.guiDocumentStatus =
                                        "Text wrapping updated.";
                                }
                            }
                            else if (contentKind
                                == authoring::gui::WidgetKind::text_input)
                            {
                                auto content = std::get<
                                    authoring::gui::TextInputContent>(
                                        widget->descriptor.content);
                                bool changed = false;
                                changed |= gui::toggle_switch(
                                    "Multiline",
                                    content.multiline,
                                    {outlinerWidth, 27.0f});
                                changed |= gui::toggle_switch(
                                    "Read only",
                                    content.read_only,
                                    {outlinerWidth, 27.0f});
                                gui::label("Maximum Length");
                                const std::array lengthTabs{
                                    gui::SegmentedButtonSpec{
                                        "64", 48.0f,
                                        content.maximum_length == 64u},
                                    gui::SegmentedButtonSpec{
                                        "256", 54.0f,
                                        content.maximum_length == 256u},
                                    gui::SegmentedButtonSpec{
                                        "4096", 62.0f,
                                        content.maximum_length == 4'096u}
                                };
                                if (const auto length =
                                        gui::segmented_button_row(
                                            lengthTabs,
                                            25.0f,
                                            3.0f))
                                {
                                    constexpr std::array<
                                        std::uint32_t,
                                        3> lengths{
                                            64u, 256u, 4'096u};
                                    content.maximum_length =
                                        lengths[*length];
                                    if (content.text.size()
                                        > content.maximum_length)
                                    {
                                        content.text.resize(
                                            content.maximum_length);
                                    }
                                    changed = true;
                                }
                                if (changed)
                                {
                                    (void)guiDocument->update_content(
                                        widget->handle,
                                        content);
                                    editor.guiDocumentStatus =
                                        "Text input policy updated.";
                                }
                            }
                            else if (contentKind
                                == authoring::gui::WidgetKind::slider)
                            {
                                auto content = std::get<
                                    authoring::gui::SliderContent>(
                                        widget->descriptor.content);
                                gui::label("Slider Range");
                                const std::array rangeTabs{
                                    gui::SegmentedButtonSpec{
                                        "0..1", 54.0f,
                                        content.minimum == 0.0
                                            && content.maximum == 1.0},
                                    gui::SegmentedButtonSpec{
                                        "-1..1", 58.0f,
                                        content.minimum == -1.0
                                            && content.maximum == 1.0},
                                    gui::SegmentedButtonSpec{
                                        "0..100", 68.0f,
                                        content.minimum == 0.0
                                            && content.maximum == 100.0}
                                };
                                bool changed = false;
                                if (const auto range =
                                        gui::segmented_button_row(
                                            rangeTabs,
                                            25.0f,
                                            3.0f))
                                {
                                    constexpr std::array<
                                        std::array<double, 2>,
                                        3> ranges{{
                                            {0.0, 1.0},
                                            {-1.0, 1.0},
                                            {0.0, 100.0}
                                        }};
                                    content.minimum =
                                        ranges[*range][0];
                                    content.maximum =
                                        ranges[*range][1];
                                    content.value = (std::clamp)(
                                        content.value,
                                        content.minimum,
                                        content.maximum);
                                    changed = true;
                                }
                                const auto slider = gui::slider({
                                    .id = "gui.widget.slider_value",
                                    .label = "Value",
                                    .minimum = static_cast<float>(
                                        content.minimum),
                                    .maximum = static_cast<float>(
                                        content.maximum),
                                    .value = static_cast<float>(
                                        content.value),
                                    .step = static_cast<float>(
                                        content.step),
                                    .size = {outlinerWidth, 28.0f}
                                });
                                if (slider.changed)
                                {
                                    content.value = slider.value;
                                    changed = true;
                                }
                                if (changed)
                                {
                                    (void)guiDocument->update_content(
                                        widget->handle,
                                        content);
                                    (void)project_gui_document_layout(editor);
                                    editor.guiDocumentStatus =
                                        "Slider range or value updated.";
                                }
                            }
                            else if (contentKind
                                == authoring::gui::WidgetKind::scroll_area)
                            {
                                auto content = std::get<
                                    authoring::gui::ScrollAreaContent>(
                                        widget->descriptor.content);
                                bool changed = false;
                                changed |= gui::toggle_switch(
                                    "Horizontal scroll",
                                    content.allow_horizontal,
                                    {outlinerWidth, 27.0f});
                                changed |= gui::toggle_switch(
                                    "Vertical scroll",
                                    content.allow_vertical,
                                    {outlinerWidth, 27.0f});
                                const auto horizontal = gui::slider({
                                    .id = "gui.widget.scroll_x",
                                    .label = "Scroll X",
                                    .minimum = 0.0f,
                                    .maximum = 1.0f,
                                    .value = content.horizontal_offset,
                                    .step = 0.01f,
                                    .size = {outlinerWidth, 28.0f},
                                    .enabled =
                                        content.allow_horizontal
                                });
                                const auto vertical = gui::slider({
                                    .id = "gui.widget.scroll_y",
                                    .label = "Scroll Y",
                                    .minimum = 0.0f,
                                    .maximum = 1.0f,
                                    .value = content.vertical_offset,
                                    .step = 0.01f,
                                    .size = {outlinerWidth, 28.0f},
                                    .enabled =
                                        content.allow_vertical
                                });
                                if (horizontal.changed)
                                {
                                    content.horizontal_offset =
                                        horizontal.value;
                                    changed = true;
                                }
                                if (vertical.changed)
                                {
                                    content.vertical_offset =
                                        vertical.value;
                                    changed = true;
                                }
                                if (changed)
                                {
                                    (void)guiDocument->update_content(
                                        widget->handle,
                                        content);
                                    editor.guiDocumentStatus =
                                        "Scroll-area policy updated.";
                                }
                            }
                            else if (contentKind
                                == authoring::gui::WidgetKind::tab_page)
                            {
                                auto content = std::get<
                                    authoring::gui::TabPageContent>(
                                        widget->descriptor.content);
                                if (gui::toggle_switch(
                                        "Closeable tab",
                                        content.closeable,
                                        {outlinerWidth, 27.0f}))
                                {
                                    (void)guiDocument->update_content(
                                        widget->handle,
                                        content);
                                    editor.guiDocumentStatus =
                                        "Tab close policy updated.";
                                }
                            }

                            gui::label("Layout Flow");
                            const std::array flowTabs{
                                gui::SegmentedButtonSpec{
                                    "Free", 54.0f,
                                    widget->descriptor.layout.flow
                                        == authoring::gui::LayoutFlow::absolute},
                                gui::SegmentedButtonSpec{
                                    "Row", 52.0f,
                                    widget->descriptor.layout.flow
                                        == authoring::gui::LayoutFlow::horizontal},
                                gui::SegmentedButtonSpec{
                                    "Column", 66.0f,
                                    widget->descriptor.layout.flow
                                        == authoring::gui::LayoutFlow::vertical},
                                gui::SegmentedButtonSpec{
                                    "Overlay", 68.0f,
                                    widget->descriptor.layout.flow
                                        == authoring::gui::LayoutFlow::overlay}
                            };
                            if (const auto flow =
                                    gui::segmented_button_row(
                                        flowTabs, 25.0f, 3.0f))
                            {
                                constexpr std::array<
                                    authoring::gui::LayoutFlow,
                                    4> values{
                                        authoring::gui::LayoutFlow::absolute,
                                        authoring::gui::LayoutFlow::horizontal,
                                        authoring::gui::LayoutFlow::vertical,
                                        authoring::gui::LayoutFlow::overlay
                                    };
                                auto layout = widget->descriptor.layout;
                                layout.flow = values[*flow];
                                const auto changed =
                                    guiDocument->update_layout(
                                        widget->handle,
                                        std::move(layout));
                                editor.guiDocumentStatus = changed
                                    ? "Widget layout flow updated."
                                    : "Widget layout flow was unchanged.";
                            }

                            gui::label("Appearance");
                            const std::array appearanceActions{
                                gui::InlineButtonSpec{
                                    .label = "Neutral",
                                    .width = 72.0f},
                                gui::InlineButtonSpec{
                                    .label = "Accent",
                                    .width = 68.0f},
                                gui::InlineButtonSpec{
                                    .label = "Alert",
                                    .width = 58.0f},
                                gui::InlineButtonSpec{
                                    .label = "Clear",
                                    .width = 58.0f}
                            };
                            if (const auto appearance =
                                    gui::inline_button_row(
                                        appearanceActions,
                                        25.0f,
                                        3.0f))
                            {
                                auto style = widget->descriptor.style;
                                if (*appearance == 0u)
                                {
                                    style.background =
                                        {0.16f, 0.17f, 0.19f, 1.0f};
                                    style.foreground =
                                        {0.94f, 0.94f, 0.94f, 1.0f};
                                    style.border =
                                        {0.32f, 0.34f, 0.38f, 1.0f};
                                    style.accent =
                                        {0.20f, 0.56f, 0.86f, 1.0f};
                                }
                                else if (*appearance == 1u)
                                {
                                    style.background =
                                        {0.08f, 0.24f, 0.34f, 1.0f};
                                    style.foreground =
                                        {0.92f, 0.97f, 1.0f, 1.0f};
                                    style.border =
                                        {0.18f, 0.62f, 0.72f, 1.0f};
                                    style.accent =
                                        {0.20f, 0.76f, 0.88f, 1.0f};
                                }
                                else if (*appearance == 2u)
                                {
                                    style.background =
                                        {0.34f, 0.10f, 0.12f, 1.0f};
                                    style.foreground =
                                        {1.0f, 0.94f, 0.92f, 1.0f};
                                    style.border =
                                        {0.76f, 0.26f, 0.22f, 1.0f};
                                    style.accent =
                                        {0.96f, 0.48f, 0.20f, 1.0f};
                                }
                                else
                                {
                                    style.background.alpha = 0.0f;
                                    style.border.alpha = 0.0f;
                                }
                                const auto changed =
                                    guiDocument->update_style(
                                        widget->handle,
                                        std::move(style));
                                editor.guiDocumentStatus = changed
                                    ? "Widget appearance updated."
                                    : "Widget appearance was unchanged.";
                            }

                            const auto& currentStyle =
                                widget->descriptor.style;
                            const std::array radiusTabs{
                                gui::SegmentedButtonSpec{
                                    "Square", 64.0f,
                                    currentStyle.corner_radius == 0.0f},
                                gui::SegmentedButtonSpec{
                                    "2 px", 52.0f,
                                    currentStyle.corner_radius == 2.0f},
                                gui::SegmentedButtonSpec{
                                    "4 px", 52.0f,
                                    currentStyle.corner_radius == 4.0f},
                                gui::SegmentedButtonSpec{
                                    "8 px", 52.0f,
                                    currentStyle.corner_radius == 8.0f}
                            };
                            if (const auto radius =
                                    gui::segmented_button_row(
                                        radiusTabs, 25.0f, 3.0f))
                            {
                                constexpr std::array<float, 4> values{
                                    0.0f, 2.0f, 4.0f, 8.0f};
                                auto style = currentStyle;
                                style.corner_radius = values[*radius];
                                (void)guiDocument->update_style(
                                    widget->handle,
                                    std::move(style));
                                editor.guiDocumentStatus =
                                    "Widget corner radius updated.";
                            }

                            const std::array opacityTabs{
                                gui::SegmentedButtonSpec{
                                    "25%", 52.0f,
                                    currentStyle.opacity == 0.25f},
                                gui::SegmentedButtonSpec{
                                    "50%", 52.0f,
                                    currentStyle.opacity == 0.50f},
                                gui::SegmentedButtonSpec{
                                    "75%", 52.0f,
                                    currentStyle.opacity == 0.75f},
                                gui::SegmentedButtonSpec{
                                    "100%", 60.0f,
                                    currentStyle.opacity == 1.0f}
                            };
                            if (const auto opacity =
                                    gui::segmented_button_row(
                                        opacityTabs, 25.0f, 3.0f))
                            {
                                constexpr std::array<float, 4> values{
                                    0.25f, 0.50f, 0.75f, 1.0f};
                                auto style = currentStyle;
                                style.opacity = values[*opacity];
                                (void)guiDocument->update_style(
                                    widget->handle,
                                    std::move(style));
                                editor.guiDocumentStatus =
                                    "Widget opacity updated.";
                            }

                            auto interaction =
                                widget->descriptor.interaction;
                            bool interactionChanged = false;
                            interactionChanged |= gui::toggle_switch(
                                "Visible",
                                interaction.visible,
                                {outlinerWidth, 27.0f});
                            interactionChanged |= gui::toggle_switch(
                                "Enabled",
                                interaction.enabled,
                                {outlinerWidth, 27.0f});
                            interactionChanged |= gui::toggle_switch(
                                "Keyboard focus",
                                interaction.focusable,
                                {outlinerWidth, 27.0f});
                            interactionChanged |= gui::toggle_switch(
                                "Pointer input",
                                interaction.accepts_pointer,
                                {outlinerWidth, 27.0f});
                            if (interactionChanged)
                            {
                                const auto changed =
                                    guiDocument->update_interaction(
                                        widget->handle,
                                        std::move(interaction));
                                editor.guiDocumentStatus = changed
                                    ? "Widget interaction policy updated."
                                    : "Widget interaction policy was unchanged.";
                            }

                            if (widget->descriptor.kind
                                == authoring::gui::WidgetKind::tab_set)
                            {
                                gui::label("Tab Pages");
                                const auto* content = std::get_if<
                                    authoring::gui::TabSetContent>(
                                        &widget->descriptor.content);
                                std::vector<authoring::gui::WidgetHandle>
                                    pageHandles{};
                                std::vector<std::string> pageIds{};
                                std::vector<std::string> pageLabels{};
                                pageHandles.reserve(widget->children.size());
                                pageIds.reserve(widget->children.size());
                                pageLabels.reserve(widget->children.size());
                                for (const auto child : widget->children)
                                {
                                    const auto page =
                                        guiDocument->widget(child);
                                    if (!page)
                                        continue;
                                    const auto* pageContent = std::get_if<
                                        authoring::gui::TabPageContent>(
                                            &page->descriptor.content);
                                    pageHandles.push_back(child);
                                    pageIds.push_back(format_text(
                                        "gui.tab.{}.{}",
                                        child.index,
                                        child.generation));
                                    pageLabels.push_back(pageContent
                                        ? pageContent->label
                                        : page->descriptor.name);
                                }
                                std::vector<gui::TabButtonSpec> tabButtons{};
                                tabButtons.reserve(pageHandles.size());
                                const float tabWidth = pageHandles.empty()
                                    ? 80.0f
                                    : (std::clamp)(
                                        (outlinerWidth
                                            - static_cast<float>(
                                                pageHandles.size() - 1u))
                                            / static_cast<float>(
                                                pageHandles.size()),
                                        68.0f,
                                        118.0f);
                                const bool documentDirty =
                                    guiDocument->revision().sequence
                                        != editor.guiDocumentSavedRevision;
                                for (std::size_t pageIndex = 0u;
                                     pageIndex < pageHandles.size();
                                     ++pageIndex)
                                {
                                    tabButtons.push_back({
                                        .id = pageIds[pageIndex],
                                        .label = pageLabels[pageIndex],
                                        .width = tabWidth,
                                        .active = content
                                            && content->selected_page
                                                == pageHandles[pageIndex],
                                        .closable =
                                            pageHandles.size() > 1u,
                                        .dirty = documentDirty});
                                }
                                const gui::TabBarResult tabResult =
                                    gui::tab_bar_buttons(
                                        tabButtons,
                                        29.0f,
                                        1.0f);
                                if (tabResult.selected_index
                                    && *tabResult.selected_index
                                        < pageHandles.size())
                                {
                                    (void)guiDocument->update_content(
                                        widget->handle,
                                        authoring::gui::TabSetContent{
                                            pageHandles[
                                                *tabResult.selected_index]});
                                    editor.guiDocumentStatus =
                                        "Active GUI tab updated.";
                                }
                                if (tabResult.closed_index
                                    && *tabResult.closed_index
                                        < pageHandles.size()
                                    && pageHandles.size() > 1u)
                                {
                                    const std::size_t closed =
                                        *tabResult.closed_index;
                                    const auto closedHandle =
                                        pageHandles[closed];
                                    if (content
                                        && content->selected_page
                                            == closedHandle)
                                    {
                                        const std::size_t replacementIndex =
                                            closed + 1u
                                                < pageHandles.size()
                                            ? closed + 1u
                                            : closed - 1u;
                                        (void)guiDocument->update_content(
                                            widget->handle,
                                            authoring::gui::TabSetContent{
                                                pageHandles[
                                                    replacementIndex]});
                                    }
                                    const auto removed =
                                        guiDocument->remove_widget(
                                            closedHandle);
                                    editor.guiDocumentStatus = removed
                                        ? "Tab page removed."
                                        : epochengine::format_text(
                                            "Tab removal rejected: {}.",
                                            authoring::gui::
                                                result_code_name(
                                                    removed.code));
                                }
                                (void)gui::edit_box(
                                    editor.newGuiTabName,
                                    {outlinerWidth, 27.0f},
                                    64u,
                                    false);
                                if (gui::button(
                                        "Add Tab Page",
                                        {112.0f, 27.0f}))
                                {
                                    std::string label =
                                        editor.newGuiTabName.empty()
                                        ? epochengine::format_text(
                                            "Tab {}",
                                            widget->children.size() + 1u)
                                        : editor.newGuiTabName;
                                    authoring::gui::WidgetDescriptor page{};
                                    page.kind =
                                        authoring::gui::WidgetKind::tab_page;
                                    page.name = label;
                                    page.layout.flow =
                                        authoring::gui::LayoutFlow::overlay;
                                    page.layout.width_rule =
                                        authoring::gui::SizeRule::fill;
                                    page.layout.height_rule =
                                        authoring::gui::SizeRule::fill;
                                    page.content =
                                        authoring::gui::TabPageContent{
                                            std::move(label),
                                            true};
                                    const auto created =
                                        guiDocument->create_widget(
                                            std::move(page),
                                            widget->handle);
                                    editor.guiDocumentStatus = created
                                        ? "Tab page added."
                                        : epochengine::format_text(
                                            "Tab page rejected: {}.",
                                            authoring::gui::
                                                result_code_name(
                                                    created.code));
                                }
                            }
                        }
                    }
                }
            }
            else if (editor.guiAuthoringSection == 2u)
            {
                std::optional<authoring::gui::WidgetHandle>
                    selectedWidget{};
                if (guiDocument)
                {
                    const auto selectedMapping =
                        editor.guiDocumentWidgets.find(
                            editor.selectedEntityId);
                    if (selectedMapping
                        != editor.guiDocumentWidgets.end()
                        && guiDocument->widget(
                            selectedMapping->second))
                    {
                        selectedWidget = selectedMapping->second;
                    }
                }

                const std::array hierarchyActions{
                    gui::InlineButtonSpec{
                        .label = "Duplicate",
                        .width = 88.0f,
                        .enabled = selectedWidget.has_value()},
                    gui::InlineButtonSpec{
                        .label = "Delete",
                        .width = 68.0f,
                        .enabled = selectedWidget.has_value()},
                    gui::InlineButtonSpec{
                        .label = "Focus",
                        .width = 64.0f,
                        .enabled = selectedWidget.has_value()}
                };
                if (const auto action = gui::inline_button_row(
                        hierarchyActions, 26.0f, 4.0f))
                {
                    if (*action == 0u)
                    {
                        (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::duplicate_selection);
                    }
                    else if (*action == 1u
                        && guiDocument
                        && selectedWidget)
                    {
                        (void)remove_selected_gui_widget(
                            editor, *guiDocument);
                    }
                    else
                    {
                        handle_scene_tool(
                            editor,
                            ctx.get(),
                            "focus_selection");
                    }
                }

                if (guiDocument && selectedWidget)
                {
                    const auto selectedView =
                        guiDocument->widget(*selectedWidget);
                    if (selectedView && selectedView->parent)
                    {
                        const auto parentView =
                            guiDocument->widget(
                                selectedView->parent);
                        if (parentView)
                        {
                            const auto sibling = std::find(
                                parentView->children.begin(),
                                parentView->children.end(),
                                *selectedWidget);
                            const std::size_t position =
                                sibling == parentView->children.end()
                                ? parentView->children.size()
                                : static_cast<std::size_t>(
                                    std::distance(
                                        parentView->children.begin(),
                                        sibling));
                            std::optional<
                                authoring::gui::WidgetHandle>
                                    nestingTarget{};
                            if (position > 0u)
                            {
                                const auto candidate =
                                    guiDocument->widget(
                                        parentView->children[
                                            position - 1u]);
                                if (candidate
                                    && (candidate->descriptor.kind
                                            == authoring::gui::
                                                WidgetKind::panel
                                        || candidate->descriptor.kind
                                            == authoring::gui::
                                                WidgetKind::scroll_area
                                        || candidate->descriptor.kind
                                            == authoring::gui::
                                                WidgetKind::tab_page))
                                {
                                    nestingTarget =
                                        candidate->handle;
                                }
                            }

                            const std::array structureActions{
                                gui::InlineButtonSpec{
                                    .label = "Move Up",
                                    .width = 78.0f,
                                    .enabled = position > 0u},
                                gui::InlineButtonSpec{
                                    .label = "Move Down",
                                    .width = 92.0f,
                                    .enabled = position + 1u
                                        < parentView->children.size()},
                                gui::InlineButtonSpec{
                                    .label = "To Root",
                                    .width = 72.0f,
                                    .enabled = selectedView->parent
                                        != editor.guiDocumentRoot},
                                gui::InlineButtonSpec{
                                    .label = "Nest",
                                    .width = 58.0f,
                                    .enabled =
                                        nestingTarget.has_value()}
                            };
                            if (const auto structure =
                                    gui::inline_button_row(
                                        structureActions,
                                        26.0f,
                                        4.0f))
                            {
                                authoring::gui::MutationResult changed{};
                                if (*structure == 0u)
                                {
                                    changed =
                                        guiDocument->reorder_widget(
                                            *selectedWidget,
                                            static_cast<std::uint32_t>(
                                                position - 1u));
                                }
                                else if (*structure == 1u)
                                {
                                    changed =
                                        guiDocument->reorder_widget(
                                            *selectedWidget,
                                            static_cast<std::uint32_t>(
                                                position + 1u));
                                }
                                else if (*structure == 2u)
                                {
                                    changed =
                                        guiDocument->reparent_widget(
                                            *selectedWidget,
                                            editor.guiDocumentRoot);
                                }
                                else if (nestingTarget)
                                {
                                    changed =
                                        guiDocument->reparent_widget(
                                            *selectedWidget,
                                            *nestingTarget);
                                }

                                if (changed)
                                {
                                    (void)project_gui_document_layout(
                                        editor);
                                    editor.guiDocumentStatus =
                                        "GUI hierarchy operation committed.";
                                }
                                else
                                {
                                    editor.guiDocumentStatus =
                                        epochengine::format_text(
                                            "GUI hierarchy operation rejected: {}.",
                                            authoring::gui::
                                                result_code_name(
                                                    changed.code));
                                }
                            }
                        }
                    }
                }

                if (!guiDocument)
                {
                    gui::wrapped_label(
                        "The GUI hierarchy requires a valid GUI document.",
                        outlinerWidth);
                }
                else
                {
                    struct HierarchyRow final
                    {
                        authoring::gui::WidgetHandle handle{};
                        std::uint32_t depth{};
                    };
                    std::vector<HierarchyRow> rows{};
                    std::vector<HierarchyRow> stack{};
                    const auto root =
                        guiDocument->widget(
                            editor.guiDocumentRoot);
                    if (root)
                    {
                        rows.reserve(
                            guiDocument->metrics().active_widgets);
                        stack.reserve(
                            guiDocument->metrics().active_widgets);
                        for (auto child =
                                root->children.rbegin();
                             child != root->children.rend();
                             ++child)
                        {
                            stack.push_back({*child, 0u});
                        }
                    }
                    while (!stack.empty())
                    {
                        const HierarchyRow row = stack.back();
                        stack.pop_back();
                        rows.push_back(row);
                        const auto view =
                            guiDocument->widget(row.handle);
                        if (!view)
                            continue;
                        for (auto child =
                                view->children.rbegin();
                             child != view->children.rend();
                             ++child)
                        {
                            stack.push_back({
                                *child,
                                row.depth + 1u});
                        }
                    }

                    for (const HierarchyRow& row : rows)
                    {
                        const auto view =
                            guiDocument->widget(row.handle);
                        if (!view)
                            continue;
                        const auto mapped = std::find_if(
                            editor.guiDocumentWidgets.begin(),
                            editor.guiDocumentWidgets.end(),
                            [&](const auto& entry)
                            {
                                return entry.second == row.handle;
                            });
                        const bool hasProxy =
                            mapped
                                != editor.guiDocumentWidgets.end();
                        std::string label(
                            static_cast<std::size_t>(
                                row.depth) * 2u,
                            ' ');
                        label += view->descriptor.name;
                        label += " | ";
                        label += authoring::gui::widget_kind_name(
                            view->descriptor.kind);
                        if (!hasProxy)
                            label += " | document";

                        if (gui::button_selected(
                                label,
                                {outlinerWidth, 27.0f},
                                selectedWidget
                                    && *selectedWidget
                                        == row.handle))
                        {
                            if (hasProxy)
                            {
                                const auto entityIndex =
                                    editor_entity_index(
                                        editor,
                                        mapped->first);
                                if (entityIndex)
                                    select_editor_entity(
                                        editor,
                                        *entityIndex);
                            }
                            else
                            {
                                editor.guiDocumentStatus =
                                    "Document-owned widget selected in the hierarchy; scene projection is not available yet.";
                            }
                        }
                    }
                }
            }            else
            {
                if (!guiDocument)
                {
                    gui::wrapped_label(
                        "The GUI structure graph requires a valid GUI document.",
                        outlinerWidth);
                }
                else
                {
                    const std::vector<authoring::gui::WidgetView> widgets =
                        guiDocument->widgets();
                    std::vector<gui::NodeGraphCanvasNode> graphNodes{};
                    std::vector<gui::NodeGraphCanvasEdge> graphEdges{};
                    std::vector<std::string> graphSubtitles{};
                    std::vector<std::pair<
                        std::uint64_t,
                        authoring::gui::WidgetHandle>> graphHandles{};
                    std::vector<std::pair<
                        std::uint64_t,
                        authoring::gui::WidgetHandle>> graphEdgeChildren{};
                    std::vector<std::uint32_t> depths{};
                    graphNodes.reserve(widgets.size());
                    graphEdges.reserve(widgets.size());
                    graphSubtitles.reserve(widgets.size());
                    graphHandles.reserve(widgets.size());
                    graphEdgeChildren.reserve(widgets.size());
                    depths.reserve(widgets.size());

                    const auto graphNodeId = [](
                        authoring::gui::WidgetHandle handle) noexcept
                    {
                        return (static_cast<std::uint64_t>(
                                    handle.generation) << 32u)
                            | (static_cast<std::uint64_t>(
                                    handle.index) + 1u);
                    };
                    const auto depthOf = [&](
                        const authoring::gui::WidgetView& widget)
                    {
                        std::uint32_t depth = 0u;
                        auto parent = widget.parent;
                        while (parent && depth < 64u)
                        {
                            ++depth;
                            const auto parentView =
                                guiDocument->widget(parent);
                            if (!parentView)
                                break;
                            parent = parentView->parent;
                        }
                        return depth;
                    };

                    std::uint32_t maximumDepth = 0u;
                    for (const auto& widget : widgets)
                    {
                        const std::uint32_t depth = depthOf(widget);
                        depths.push_back(depth);
                        maximumDepth = (std::max)(maximumDepth, depth);
                    }
                    const std::size_t columnCount =
                        static_cast<std::size_t>(maximumDepth) + 1u;
                    std::vector<std::size_t> columnOrdinals(
                        columnCount, 0u);
                    const float horizontalGap = 18.0f;
                    const float availableNodeWidth =
                        (outlinerWidth - 28.0f
                            - horizontalGap
                                * static_cast<float>(maximumDepth))
                        / static_cast<float>(columnCount);
                    const float graphNodeWidth = (std::clamp)(
                        availableNodeWidth, 78.0f, 128.0f);
                    constexpr float graphNodeHeight = 48.0f;
                    constexpr float verticalGap = 14.0f;
                    std::vector<gui::Vec2> nodePositions(
                        widgets.size());
                    std::size_t maximumRows = 1u;

                    for (std::size_t index = 0u;
                         index < widgets.size();
                         ++index)
                    {
                        const auto& widget = widgets[index];
                        const std::uint32_t depth = depths[index];
                        const std::size_t ordinal =
                            columnOrdinals[depth]++;
                        maximumRows = (std::max)(
                            maximumRows, ordinal + 1u);
                        const gui::Vec2 position{
                            14.0f + static_cast<float>(depth)
                                * (graphNodeWidth + horizontalGap),
                            18.0f + static_cast<float>(ordinal)
                                * (graphNodeHeight + verticalGap)};
                        nodePositions[index] = position;

                        std::string subtitle{
                            authoring::gui::widget_kind_name(
                                widget.descriptor.kind)};
                        if (!widget.descriptor.interaction.action.empty())
                        {
                            subtitle += " -> ";
                            subtitle +=
                                widget.descriptor.interaction.action;
                        }
                        graphSubtitles.push_back(std::move(subtitle));

                        gui::NodeGraphNodeRole role =
                            gui::NodeGraphNodeRole::control;
                        switch (widget.descriptor.kind)
                        {
                        case authoring::gui::WidgetKind::canvas:
                            role = gui::NodeGraphNodeRole::document;
                            break;
                        case authoring::gui::WidgetKind::panel:
                        case authoring::gui::WidgetKind::scroll_area:
                        case authoring::gui::WidgetKind::tab_set:
                        case authoring::gui::WidgetKind::tab_page:
                            role = gui::NodeGraphNodeRole::container;
                            break;
                        default:
                            break;
                        }

                        bool selected = false;
                        const auto mapped = std::find_if(
                            editor.guiDocumentWidgets.begin(),
                            editor.guiDocumentWidgets.end(),
                            [&](const auto& entry)
                            {
                                return entry.second == widget.handle
                                    && entry.first
                                        == editor.selectedEntityId;
                            });
                        selected = mapped
                            != editor.guiDocumentWidgets.end();
                        const std::uint64_t id =
                            graphNodeId(widget.handle);
                        graphNodes.push_back({
                            .id = id,
                            .title = widget.descriptor.name,
                            .subtitle = graphSubtitles.back(),
                            .position = position,
                            .size = {
                                graphNodeWidth,
                                graphNodeHeight},
                            .role = role,
                            .selected = selected,
                            .enabled =
                                widget.descriptor.interaction.enabled
                        });
                        graphHandles.emplace_back(id, widget.handle);
                    }

                    for (std::size_t index = 0u;
                         index < widgets.size();
                         ++index)
                    {
                        const auto& widget = widgets[index];
                        if (!widget.parent)
                            continue;
                        const auto parent = std::find_if(
                            widgets.begin(),
                            widgets.end(),
                            [&](const auto& candidate)
                            {
                                return candidate.handle
                                    == widget.parent;
                            });
                        if (parent == widgets.end())
                            continue;
                        const std::size_t parentIndex =
                            static_cast<std::size_t>(
                                std::distance(widgets.begin(), parent));
                        const std::uint64_t sourceId =
                            graphNodeId(parent->handle);
                        const std::uint64_t targetId =
                            graphNodeId(widget.handle);
                        std::uint64_t edgeId = sourceId;
                        edgeId ^= targetId
                            + 0x9e3779b97f4a7c15ull
                            + (edgeId << 6u) + (edgeId >> 2u);
                        if (edgeId == 0u)
                            edgeId = 1u;
                        graphEdges.push_back({
                            .id = edgeId,
                            .source_node = sourceId,
                            .target_node = targetId,
                            .source = {
                                nodePositions[parentIndex].x
                                    + graphNodeWidth,
                                nodePositions[parentIndex].y
                                    + graphNodeHeight * 0.5f},
                            .target = {
                                nodePositions[index].x,
                                nodePositions[index].y
                                    + graphNodeHeight * 0.5f},
                            .enabled = true
                        });
                        graphEdgeChildren.emplace_back(
                            edgeId, widget.handle);
                    }

                    const float graphHeight = (std::clamp)(
                        36.0f + static_cast<float>(maximumRows)
                            * (graphNodeHeight + verticalGap),
                        220.0f,
                        420.0f);
                    gui::label("GUI Structure Graph");
                    gui::property_row(
                        "Graph",
                        epochengine::format_text(
                            "{} nodes | {} edges | revision {}",
                            graphNodes.size(),
                            graphEdges.size(),
                            guiMetrics.revision_sequence),
                        58.0f);
                    bool fitGraph = false;
                    bool resetGraph = false;
                    const std::array graphViewActions{
                        gui::InlineButtonSpec{
                            .label = "Fit",
                            .width = 54.0f},
                        gui::InlineButtonSpec{
                            .label = "Reset View",
                            .width = 92.0f}
                    };
                    if (const auto action = gui::inline_button_row(
                            graphViewActions, 27.0f, 4.0f))
                    {
                        fitGraph = *action == 0u;
                        resetGraph = *action == 1u;
                    }
                    const gui::NodeGraphCanvasResult graphResult =
                        gui::node_graph_canvas({
                            .id = "gui.document.structure_graph",
                            .size = {outlinerWidth, graphHeight},
                            .nodes = graphNodes,
                            .edges = graphEdges,
                            .grid_step = 24.0f,
                            .interactive = true,
                            .allow_node_movement = true,
                            .allow_connections = true,
                            .allow_disconnection = true,
                            .fit_to_content = fitGraph,
                            .reset_view = resetGraph,
                            .enabled = true
                        });
                    if (graphResult.clicked_node)
                    {
                        const auto selectedHandle = std::find_if(
                            graphHandles.begin(),
                            graphHandles.end(),
                            [&](const auto& entry)
                            {
                                return entry.first
                                    == *graphResult.clicked_node;
                            });
                        if (selectedHandle != graphHandles.end())
                        {
                            const auto mapped = std::find_if(
                                editor.guiDocumentWidgets.begin(),
                                editor.guiDocumentWidgets.end(),
                                [&](const auto& entry)
                                {
                                    return entry.second
                                        == selectedHandle->second;
                                });
                            if (mapped
                                != editor.guiDocumentWidgets.end())
                            {
                                const auto entityIndex =
                                    editor_entity_index(
                                        editor, mapped->first);
                                if (entityIndex)
                                {
                                    select_editor_entity(
                                        editor, *entityIndex);
                                    editor.guiDocumentStatus =
                                        "Graph node selected for GUI editing.";
                                }
                            }
                            else
                            {
                                editor.guiDocumentStatus =
                                    "The selected graph node is document-owned and has no scene proxy.";
                            }
                        }
                    }
                    if (graphResult.connection)
                    {
                        const auto source = std::find_if(
                            graphHandles.begin(),
                            graphHandles.end(),
                            [&](const auto& entry)
                            {
                                return entry.first
                                    == graphResult.connection->source_node;
                            });
                        const auto target = std::find_if(
                            graphHandles.begin(),
                            graphHandles.end(),
                            [&](const auto& entry)
                            {
                                return entry.first
                                    == graphResult.connection->target_node;
                            });
                        if (source == graphHandles.end()
                            || target == graphHandles.end())
                        {
                            editor.guiDocumentStatus =
                                "GUI graph connection referenced a stale widget.";
                        }
                        else if (target->second == editor.guiDocumentRoot)
                        {
                            editor.guiDocumentStatus =
                                "The GUI document root cannot be reparented.";
                        }
                        else
                        {
                            const auto changed =
                                guiDocument->reparent_widget(
                                    target->second,
                                    source->second);
                            if (changed)
                            {
                                (void)project_gui_document_layout(editor);
                                editor.guiDocumentStatus =
                                    "GUI graph connection committed to the document hierarchy.";
                            }
                            else
                            {
                                editor.guiDocumentStatus =
                                    epochengine::format_text(
                                        "GUI graph connection rejected: {}.",
                                        authoring::gui::result_code_name(
                                            changed.code));
                            }
                        }
                    }
                    if (!graphResult.disconnected_edges.empty())
                    {
                        const std::uint64_t edgeId =
                            graphResult.disconnected_edges.front();
                        const auto child = std::find_if(
                            graphEdgeChildren.begin(),
                            graphEdgeChildren.end(),
                            [&](const auto& entry)
                            {
                                return entry.first == edgeId;
                            });
                        if (child == graphEdgeChildren.end())
                        {
                            editor.guiDocumentStatus =
                                "GUI graph disconnect referenced a stale edge.";
                        }
                        else
                        {
                            const auto changed =
                                guiDocument->reparent_widget(
                                    child->second,
                                    editor.guiDocumentRoot);
                            if (changed)
                            {
                                (void)project_gui_document_layout(editor);
                                editor.guiDocumentStatus =
                                    "GUI graph edge disconnected; widget moved to the document root.";
                            }
                            else
                            {
                                editor.guiDocumentStatus =
                                    epochengine::format_text(
                                        "GUI graph disconnect rejected: {}.",
                                        authoring::gui::result_code_name(
                                            changed.code));
                            }
                        }
                    }
                    if (graphResult.clicked_background)
                    {
                        editor.guiDocumentStatus =
                            "GUI graph background selected; choose a node to inspect its canonical widget.";
                    }

                    std::size_t actionCount = 0u;
                    for (const auto& widget : widgets)
                    {
                        if (!widget.descriptor.interaction.action.empty())
                            ++actionCount;
                    }
                    gui::property_row(
                        "Actions",
                        std::to_string(actionCount),
                        72.0f);
                    const auto selected = editor_entity_index(
                        editor, editor.selectedEntityId);
                    const bool editableSelection = selected
                        && isGuiEntity(editor.entities[*selected]);
                    const std::array graphActions{
                        gui::InlineButtonSpec{
                            .label = "Edit Node",
                            .width = 86.0f,
                            .enabled = editableSelection},
                        gui::InlineButtonSpec{
                            .label = "Duplicate",
                            .width = 84.0f,
                            .enabled = editableSelection},
                        gui::InlineButtonSpec{
                            .label = "Delete",
                            .width = 66.0f,
                            .enabled = editableSelection}
                    };
                    if (const auto action = gui::inline_button_row(
                            graphActions, 27.0f, 4.0f))
                    {
                        if (*action == 0u)
                            editor.guiAuthoringSection = 1u;
                        else if (*action == 1u)
                            (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::duplicate_selection);
                        else if (guiDocument)
                            (void)remove_selected_gui_widget(
                                editor, *guiDocument);
                    }

                    if (actionCount > 0u)
                    {
                        gui::label("Action Bindings");
                        for (const auto& widget : widgets)
                        {
                            if (widget.descriptor.interaction.action.empty())
                                continue;
                            if (gui::button(
                                    widget.descriptor.name + " -> "
                                        + widget.descriptor.interaction.action,
                                    {outlinerWidth, 27.0f}))
                            {
                                const auto mapped = std::find_if(
                                    editor.guiDocumentWidgets.begin(),
                                    editor.guiDocumentWidgets.end(),
                                    [&](const auto& entry)
                                    {
                                        return entry.second
                                            == widget.handle;
                                    });
                                if (mapped
                                    == editor.guiDocumentWidgets.end())
                                {
                                    continue;
                                }
                                const auto entityIndex =
                                    editor_entity_index(
                                        editor, mapped->first);
                                if (!entityIndex)
                                    continue;
                                select_editor_entity(
                                    editor, *entityIndex);
                                editor.guiAuthoringSection = 1u;
                            }
                        }
                    }
                }
            }
        }
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
        else if (editor.outlinerToolTab == OutlinerToolTab::TileMap)
        {
            gui::label("Temporal Tile Map");
            if (auto* tileMap = tilemap_workspace_controller(editor))
            {
                const auto map = tileMap->snapshot();
                const auto metrics = tileMap->metrics();
                gui::property_row("Source", std::string(tileMap->logical_path()), 66.0f);
                gui::property_row(
                    "Extent",
                    epochengine::format_text(
                        "{} x {} tiles",
                        map.descriptor.extent_tiles.x,
                        map.descriptor.extent_tiles.y),
                    66.0f);
                gui::property_row("Layers", std::to_string(map.layers.size()), 66.0f);
                gui::property_row("Palette", std::to_string(map.palette.size()), 66.0f);

                const std::array mapHistoryActions{
                    gui::InlineButtonSpec{.label = "Save", .width = 70.0f},
                    gui::InlineButtonSpec{.label = "Undo", .width = 70.0f},
                    gui::InlineButtonSpec{.label = "Redo", .width = 70.0f}};
                if (const auto action = gui::inline_button_row(
                        mapHistoryActions, 26.0f, 4.0f))
                {
                    if (*action == 0u)
                    {
                        const auto saved = tileMap->save_and_publish();
                        editor.tileMapStatus = std::string(tileMap->status());
                        push_editor_log(
                            editor,
                            saved
                                ? "[tilemap] Saved source and compiled artifact."
                                : "[tilemap] Save/publish failed: "
                                    + editor.tileMapStatus);
                    }
                    else if (*action == 1u)
                    {
                        (void)tileMap->undo();
                        editor.tileMapStatus = std::string(tileMap->status());
                    }
                    else
                    {
                        (void)tileMap->redo();
                        editor.tileMapStatus = std::string(tileMap->status());
                    }
                }

                if (gui::button("Add Layer", {outlinerWidth, 26.0f}))
                {
                    (void)tileMap->create_layer(
                        "Layer " + std::to_string(map.layers.size() + 1u));
                    editor.tileMapStatus = std::string(tileMap->status());
                }

                const std::array tileExtentChoices{
                    gui::SegmentedButtonSpec{"16 px", 68.0f, editor.tileMapTileExtent == 16u},
                    gui::SegmentedButtonSpec{"32 px", 68.0f, editor.tileMapTileExtent == 32u},
                    gui::SegmentedButtonSpec{"64 px", 68.0f, editor.tileMapTileExtent == 64u}};
                if (const auto extent = gui::segmented_button_row(
                        tileExtentChoices, 26.0f, 4.0f))
                {
                    constexpr std::array<std::uint32_t, 3> extents{16u, 32u, 64u};
                    editor.tileMapTileExtent = extents[*extent];
                }
                if (gui::button("Attach Selected Texture", {outlinerWidth, 28.0f}))
                {
                    auto* textures = project_texture_controller(editor);
                    const auto* selected = textures ? textures->selected() : nullptr;
                    if (!textures || !selected)
                    {
                        editor.tileMapStatus =
                            "Select a compiled project texture on the Assets tab first.";
                    }
                    else
                    {
                        (void)tileMap->attach_texture(
                            *selected,
                            textures->pipeline().project_key(),
                            {editor.tileMapTileExtent, editor.tileMapTileExtent});
                        editor.tileMapStatus = std::string(tileMap->status());
                    }
                }
                gui::property_row(
                    "Edits",
                    epochengine::format_text(
                        "{} mutations | {} painted",
                        metrics.mutations,
                        metrics.painted_cells),
                    66.0f);
                gui::wrapped_label(editor.tileMapStatus, outlinerWidth);
            }
            else
            {
                gui::wrapped_label(editor.tileMapStatus, outlinerWidth);
            }
        }
#endif
        else
        {
            const std::string activeSource =
                editor_resolve_script_source_path(editor.activeScript, editor.projectRoot);
            if (gui::button(
                    "Open Large Script Editor",
                    {outlinerWidth, 30.0f}))
            {
                const std::string normalizedActiveSource =
                    display_project_path(activeSource);
                if (editor.scriptEditorDirty
                    && editor.scriptEditorPath != normalizedActiveSource)
                {
                    editor.scriptEditorStatus =
                        "Save or reload the modified source before opening the project script editor.";
                }
                else
                {
                    editor.mainSurface = EditorMainSurface::Assets;
                    editor.assetWorkspaceSection = 2u;
                    editor.workspaceTab = EditorWorkspaceTab::Assets;
                    editor.outlinerToolTab = OutlinerToolTab::Scripting;
                    // The script editor is content, not a dock preset.
                    editor.surfaceSettleFrames = 1;
                    (void)load_script_source_editor(
                        editor, activeSource, false);
                    push_editor_log(
                        editor,
                        "[script] Opened the active project script in the large source editor.");
                }
            }
            if (editor.mainSurface == EditorMainSurface::Assets)
            {
                gui::label("Script Navigator");
                gui::property_row(
                    "Active",
                    editor.activeScript,
                    66.0f);
                gui::property_row(
                    "State",
                    editor.scriptEditorDirty
                        ? std::string{"Modified"}
                        : editor.scriptEditorStatus,
                    66.0f);
                (void)gui::edit_box(
                    editor.newScriptName,
                    {outlinerWidth, 28.0f},
                    64u,
                    false);
                const std::array scriptNavigatorActions{
                    gui::InlineButtonSpec{
                        .label = "New",
                        .width = 66.0f},
                    gui::InlineButtonSpec{
                        .label = "Save",
                        .width = 66.0f},
                    gui::InlineButtonSpec{
                        .label = "Build",
                        .width = 72.0f}
                };
                if (const auto action = gui::inline_button_row(
                        scriptNavigatorActions, 27.0f, 4.0f))
                {
                    if (*action == 0u)
                        create_project_script_starter(editor);
                    else if (*action == 1u)
                        (void)save_script_source_editor(editor);
                    else
                        start_script_build(
                            editor,
                            "Script navigator build request");
                }

                const auto scripts =
                    collect_script_browser_entries(editor.projectRoot);
                for (const auto& entry : scripts)
                {
                    if (gui::button_selected(
                            entry.label,
                            {outlinerWidth, 28.0f},
                            editor.scriptEditorPath == entry.path))
                    {
                        editor.activeScript =
                            script_id_from_source_path(
                                std::filesystem::path{entry.path});
                        editor.selectedProjectFile = entry.path;
                        editor.selectedAssetPath = entry.path;
                        (void)load_script_source_editor(
                            editor, entry.path, false);
                    }
                }
                gui::wrapped_label(
                    editor.scriptBuildStatus,
                    outlinerWidth);
                gui::end_scroll_area();
                gui::end_window();
                return;
            }
            gui::label(std::string("Script: ") + editor.activeScript);
            gui::property_row("Source", display_project_path(activeSource), 64.0f);
            (void)gui::edit_box(editor.newScriptName, { outlinerWidth, 28.0f }, 64, false);

            const std::array scriptActions{
                gui::InlineButtonSpec{ .label = "Create", .width = 76.0f },
                gui::InlineButtonSpec{ .label = "Save", .width = 68.0f },
                gui::InlineButtonSpec{ .label = "Build", .width = 72.0f }
            };
            if (const auto action = gui::inline_button_row(scriptActions, 26.0f, 4.0f))
            {
                if (*action == 0)
                {
                    create_project_script_starter(editor);
                }
                else if (*action == 1)
                {
                    (void)save_script_source_editor(editor);
                }
                else
                {
                    if (editor.scriptEditorDirty)
                        (void)save_script_source_editor(editor);
                    const auto build = editor_build_script(editor.activeScript, editor.projectRoot);
                    editor.scriptBuildStatus = build.summary;
                    push_editor_log(
                        editor,
                        std::string("[script] ")
                            + (build.succeeded ? "Validation passed. " : "Validation failed. ")
                            + build.summary);
                }
            }

            const auto scripts = collect_script_browser_entries(editor.projectRoot);
            for (const auto& entry : scripts)
            {
                const bool selected = editor.scriptEditorPath == entry.path;
                if (gui::button_selected(entry.label, { outlinerWidth, 26.0f }, selected))
                {
                    editor.activeScript = script_id_from_source_path(std::filesystem::path{ entry.path });
                    editor.selectedProjectFile = entry.path;
                    (void)load_script_source_editor(editor, entry.path, false);
                }
            }

            if (editor.scriptEditorPath.empty() && std::filesystem::exists(std::filesystem::path{ activeSource }))
                (void)load_script_source_editor(editor, activeSource, false);
            draw_script_source_editor(
                editor,
                editor.scriptEditorPath.empty() ? activeSource : editor.scriptEditorPath,
                outlinerWidth,
                220.0f);
            gui::wrapped_label(editor.scriptEditorStatus, outlinerWidth);
            gui::wrapped_label(editor.scriptBuildStatus, outlinerWidth);
        }
        gui::end_scroll_area();
        gui::end_window();
        }
        };

