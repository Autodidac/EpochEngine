// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        gui::begin_window("", toolbar_pos, toolbar_size);
        const float toolbar_button_y = toolbar_pos.y + 8.0f;
        const float toolbar_button_h = 32.0f;
        // The app name owns only its own width; the colored FPS readout
        // belongs beside the task/status chips, never between name and File.
        const float application_label_width =
            application.kind == EditorApplicationKind::Standard ? 118.0f : 172.0f;

        struct TopMenuButton
        {
            TopMenu menu{};
            std::string_view label{};
            float width{};
            float x{};
        };

        std::array<TopMenuButton, 7> topMenus{{
            { TopMenu::File, "File", 68.0f, 0.0f },
            { TopMenu::Edit, "Edit", 68.0f, 0.0f },
            { TopMenu::Asset, "Asset", 76.0f, 0.0f },
            { TopMenu::Window, "Window", 92.0f, 0.0f },
            { TopMenu::Tools, "Tools", 76.0f, 0.0f },
            { TopMenu::Tasks, "Tasks", 76.0f, 0.0f },
            { TopMenu::Help, "Help", 72.0f, 0.0f }
        }};

        const bool menusBlockedByModal = modal_visible_now() || editor.outlinerContextMenu.menu.popup.open;
        const gui::Vec2 toolbarMouse = gui::mouse_position();
        std::vector<gui::ChromeItemSpec> chromeLeft{};
        chromeLeft.reserve(topMenus.size() + 1u);
        chromeLeft.push_back(gui::ChromeItemSpec{
            .id = "chrome.application",
            .label = application.display_name,
            .preferred_width = application_label_width,
            .compact_width = application_label_width,
            .priority = 100u,
            .pinned = false,
            .overflowable = false,
            .enabled = true,
            .selected = false,
            .role = gui::ChromeItemRole::Label
        });
        for (std::size_t index = 0; index < topMenus.size(); ++index)
        {
            const TopMenuButton& item = topMenus[index];
            chromeLeft.push_back(gui::ChromeItemSpec{
                .id = item.label,
                .label = item.label,
                .preferred_width = item.width,
                // If room is insufficient, use overflow; do not squeeze the
                // global menu buttons until labels or hitboxes look cramped.
                .compact_width = item.width,
                .priority = static_cast<std::uint16_t>(index),
                .pinned = false,
                .overflowable = true,
                .enabled = !menusBlockedByModal,
                .selected = editor.openMenu == item.menu,
                .role = gui::ChromeItemRole::Menu
            });
        }
        enum class ChromeCommand : std::uint8_t
        {
            Update
        };
        std::vector<gui::ChromeItemSpec> chromeRight{};
        std::vector<ChromeCommand> chromeCommands{};
        const bool commandsEnabled =
            !menusBlockedByModal && editor.openMenu == TopMenu::None;
        // Run belongs to the 3D Scene title bar, not global application chrome.
        if (updateToolbarState.visible)
        {
            const bool updateButtonActive = editor.showUpdateConfirmModal
                || editor.showSourceUpdateConfirmModal
                || editor.updateInstallPending;
            chromeRight.push_back(gui::ChromeItemSpec{
                .id = "chrome.update",
                .label = updateToolbarState.label,
                .preferred_width = 178.0f,
                .compact_width = 132.0f,
                .priority = 1u,
                .pinned = true,
                .overflowable = true,
                .enabled = commandsEnabled && updateToolbarState.enabled,
                .selected = updateButtonActive,
                .role = gui::ChromeItemRole::Command
            });
            chromeCommands.push_back(ChromeCommand::Update);
        }

        // The status strip occupies the right of the first chrome row.
        // Menus use the remainder and their built-in overflow popup.
        const std::span<const gui::ChromeItemSpec> chromeCenter{};
        float desiredMenuWidth = 24.0f;
        for (const auto& item : chromeLeft) desiredMenuWidth += item.preferred_width + 4.0f;
        const float minimumStatusWidth = gui::status_strip_minimum_width({.items = toolbarStatusItems});
        const std::span<const gui::StatusStripItem> resourceMinimumItems{toolbarStatusItems.data(), 4u};
        const float minimumResourceWidth = gui::status_strip_minimum_width({.items = resourceMinimumItems});
        const float minimumMenuReserve = (std::min)(
            application_label_width + topMenus.front().width + 82.0f + 32.0f,
            (std::max)(106.0f, w - toolbarCommandReserve - minimumResourceWidth));
        // All status labels yield before command menus overflow. Keep every
        // compact status field when possible; at extreme widths retain the
        // leading CPU/RAM/GPU/VRAM fields and File/More rather than overlap controls.
        const float menuReserve = (std::min)(desiredMenuWidth,
            (std::max)(minimumMenuReserve, w - toolbarCommandReserve - minimumStatusWidth));
        const float toolbarStatusWidth = gui::status_strip_preferred_width({
            .items = toolbarStatusItems,
            .available_width = (std::max)(1.0f, updateToolbarState.visible
                ? (std::min)(w - menuReserve - toolbarCommandReserve,
                    (w - toolbarCommandReserve) * 0.5f - 12.0f)
                : w - menuReserve),
            .single_row = true});

        // Only the chrome menu buttons may switch the open application menu.
        // Other panels have already been blocked this frame.
        if (!editor.outlinerContextMenu.menu.popup.open) gui::allow_background_for_popup();
        const gui::ChromeBarResult chromeResult =
            gui::chrome_bar(gui::ChromeBarOptions{
                .id = "editor.primary-chrome",
                .left_items = chromeLeft,
                .center_items = chromeCenter,
                .right_items = {},
                .position = { toolbar_pos.x, toolbar_button_y },
                .size = { updateToolbarState.visible
                    ? (toolbar_size.x - toolbarCommandReserve) * 0.5f - 12.0f
                    : toolbar_size.x - toolbarStatusWidth, toolbar_button_h },
                .item_gap = 4.0f,
                .zone_gap = 12.0f,
                .overflow_width = 82.0f,
                .horizontal_padding = 12.0f
            });

        gui::ChromeBarResult updateChromeResult{};
        if (updateToolbarState.visible)
            updateChromeResult = gui::chrome_bar(gui::ChromeBarOptions{
                .id = "editor.update-chrome",
                .center_items = chromeRight,
                .position = {toolbar_pos.x + (toolbar_size.x - toolbarCommandReserve) * 0.5f, toolbar_button_y},
                .size = {toolbarCommandReserve, toolbar_button_h},
                .item_gap = 4.0f,
                .zone_gap = 0.0f,
                .horizontal_padding = 12.0f});

        const auto chrome_menu_route = [&](std::optional<std::size_t> index) -> TopMenu
        {
            if (menusBlockedByModal || !index || *index == 0u
                || *index > topMenus.size())
            {
                return TopMenu::None;
            }
            return topMenus[*index - 1u].menu;
        };
        const auto hoveredMenu = chrome_menu_route(chromeResult.hovered_left);
        const auto selectedMenu = chrome_menu_route(chromeResult.selected_left);
        if (hoveredMenu != editor.suppressedMenuHover)
            editor.suppressedMenuHover = TopMenu::None;
        // Hover navigates an already open menu bar; opening requires a click.
        if (editor.openMenu != TopMenu::None && hoveredMenu != TopMenu::None
            && hoveredMenu != editor.suppressedMenuHover)
            editor.openMenu = hoveredMenu;
        if (selectedMenu != TopMenu::None)
        {
            editor.suppressedMenuHover = TopMenu::None;
            editor.openMenu = selectedMenu;
        }

        for (std::size_t index = 0; index < topMenus.size(); ++index)
        {
            const std::size_t chromeIndex = index + 1u;
            const bool visible = chromeIndex < chromeResult.left_bounds.size()
                && chromeResult.left_bounds[chromeIndex].size.x > 0.0f;
            const auto actualBounds = visible
                ? chromeResult.left_bounds[chromeIndex] : chromeResult.left_overflow;
            topMenus[index].x = actualBounds.position.x;
            topMenus[index].width = actualBounds.size.x;
        }

        const bool toolbarControlsBlockedByMenu =
            editor.openMenu != TopMenu::None || editor.outlinerContextMenu.menu.popup.open;
        if (toolbarControlsBlockedByMenu)
        {
            result.scene_input_captured = true;
            gui::block_input_until_clear();
            gui::block_background_for_popup();
        }

        if (updateChromeResult.selected_center
            && *updateChromeResult.selected_center < chromeCommands.size()
            && !toolbarControlsBlockedByMenu)
        {
            switch (chromeCommands[*updateChromeResult.selected_center])
            {
            case ChromeCommand::Update:
                if (editor.updateState == EditorUpdateState::RestartReady)
                {
                    editor.updateStatus = "Update handoff has already started. Restart Epoch to let the verified replacement finish.";
                    editor.showUpdateConfirmModal = true;
                    editor.showSourceUpdateConfirmModal = false;
                    push_editor_log(
                        editor,
                        "[update] Restart is ready after verified update handoff.");
                }
                else
                {
                    editor.openMenu = TopMenu::None;
                    editor.showUpdateConfirmModal = true;
                    editor.showSourceUpdateConfirmModal = false;
                    push_editor_log(
                        editor,
                        "[update] Update available. Awaiting confirmation.");
                }
                break;
            }
        }
        const auto statusResult = gui::status_strip(gui::StatusStripOptions{
            .items = toolbarStatusItems,
            .position = {toolbar_pos.x + toolbar_size.x - toolbarStatusWidth,
                toolbar_pos.y + toolbarStatusTop},
            .available_width = toolbarStatusWidth,
            .item_height = 22.0f,
            .align_right = true,
            .single_row = true
        });
        if (statusResult.selected_index && !toolbarControlsBlockedByMenu)
        {
            // The task count is actionable: open its real Tasks & Threads
            // dashboard, rather than sending every status click to Systems.
            if (*statusResult.selected_index == toolbarTaskIndex)
            {
                open_editor_surface(EditorMainSurface::AISandbox, "chrome tasks");
            }
            else
                open_editor_surface(EditorMainSurface::Systems, "chrome status");
        }

        const float tab_y = toolbar_pos.y + 48.0f;
        std::vector<gui::TabButtonSpec> documentTabs{};
        std::vector<const EditorWorkspaceSpec*> documentWorkspaces{};
        for(const auto& workspace:kEditorWorkspaceSpecs)
        {
            if(!editor_workspace_supported(editor,workspace)||!editor_workspace_visible(editor,workspace)) continue;
            documentTabs.push_back(gui::TabButtonSpec{
                .id = workspace.id,
                .label = workspace.label,
                .width = workspace.width,
                .active = editor_workspace_active(editor,workspace),
                .closable = true,
                .dirty = false,
                .enabled = !toolbarControlsBlockedByMenu
            });
            documentWorkspaces.push_back(&workspace);
        }

        gui::set_cursor({ 16.0f, tab_y });
        const auto documentTabResult =
                gui::responsive_tab_bar_buttons(gui::ResponsiveTabBarOptions{
                    .overflow_id = "document-workspace-overflow",
                    .overflow_label = "Workspaces",
                    .tabs = documentTabs,
                    .available_width = (std::max)(120.0f, w - 32.0f),
                    .overflow_width = 132.0f,
                    .height = 36.0f,
                    .gap = 0.0f,
                    .presentation = gui::TabBarPresentation::Workbench,
                    .keyboard_navigation = workspaceShortcutInputAvailable
                        && !editor_point_in_rect(toolbarMouse, outliner_pos, outliner_size)
                        && !editor_point_in_rect(toolbarMouse, details_pos, details_size)
                        && !editor_point_in_rect(toolbarMouse, bottomLeftPosition, bottomLeftSize)
                        && !editor_point_in_rect(toolbarMouse, bottomRightPosition, bottomRightSize)
                });
        if (documentTabResult.closed_index && *documentTabResult.closed_index < documentWorkspaces.size())
        {
            const auto& closed = *documentWorkspaces[*documentTabResult.closed_index];
            close_editor_workspace_tab(closed);
        }
        else if (documentTabResult.selected_index
            && *documentTabResult.selected_index < documentWorkspaces.size())
        {
            const auto& workspace = *documentWorkspaces[*documentTabResult.selected_index];
            if(const auto mode = workspace.gui_workspace)
                (void)select_editor_2d_workspace(editor, *mode);
            open_editor_surface(
                workspace.surface,
                "document tab");
        }
        gui::end_window();

        auto top_menu_button_bounds = [&](TopMenu menu) -> std::optional<gui::WidgetBounds>
        {
            for (const auto& item : topMenus)
            {
                if (item.menu == menu)
                    return gui::WidgetBounds{
                        .position = { item.x, toolbar_button_y },
                        .size = { item.width, toolbar_button_h }
                    };
            }
            return std::nullopt;
        };

        const gui::Vec2 mouse = toolbarMouse;
        // Keep the painted 10px layout gap while making its grab target 16px.
        // Admission and the captured gesture share the same GUI hit geometry.
        const auto layout_divider_options = [&](EditorLayoutDrag role)
        {
            gui::DragSurfaceOptions options{
                .hit_padding = 3.0f, .minimum_hit_extent = 16.0f, .drag_threshold = 0.0f};
            switch (role)
            {
            case EditorLayoutDrag::Outliner:
                options.position=outliner_split_pos;options.size=outliner_split_size;
                options.enabled=layout_outliner_visible;break;
            case EditorLayoutDrag::Inspector:
                options.position=inspector_split_pos;options.size=inspector_split_size;
                options.enabled=layout_inspector_visible;break;
            case EditorLayoutDrag::Dock:
                options.position=bottom_split_pos;options.size=bottom_split_size;
                options.enabled=bottom_visible;break;
            case EditorLayoutDrag::BottomColumns:
                options.position=bottomColumnsSplitPosition;options.size=bottomColumnsSplitSize;
                options.enabled=splitBottomColumns;break;
            default: options.enabled=false;break;
            }
            return options;
        };
        const auto traceDividerBefore = editor.layoutDrag;
        const bool traceDividerPendingBefore = editor.layoutDividerDrag.pending;
        gui::Vec2 traceDividerDelta{};
        bool traceDividerReleased{};
        const bool dividerInputBlocked = automationInputReserved || floating_gui_visible() || modal_visible_now()
            || editor.openMenu != TopMenu::None;
        if (dividerInputBlocked || editor.layoutDrag == EditorLayoutDrag::None)
        {
            editor.layoutDividerDrag = {};
            if (dividerInputBlocked) editor.layoutDrag = EditorLayoutDrag::None;
        }

        // Admission uses the queued press origin, not the pointer's later
        // frame position. Fast Down/Move/Up belongs to the existing drag owner.
        const auto dividerPress = gui::mouse_press_position();
        if (!dividerInputBlocked && editor.layoutDrag == EditorLayoutDrag::None && dividerPress)
        {
            const auto captureDivider = [&](EditorLayoutDrag role, gui::Vec2 position)
            {
                editor.layoutDrag = role;
                editor.layoutDividerDragOffset = role == EditorLayoutDrag::Dock
                    ? dividerPress->y - position.y : dividerPress->x - position.x;
            };
            if (gui::drag_surface_hit_test(*dividerPress, layout_divider_options(EditorLayoutDrag::Outliner)))
                captureDivider(EditorLayoutDrag::Outliner, outliner_split_pos);
            else if (gui::drag_surface_hit_test(*dividerPress, layout_divider_options(EditorLayoutDrag::Inspector)))
                captureDivider(EditorLayoutDrag::Inspector, inspector_split_pos);
            else if (gui::drag_surface_hit_test(*dividerPress, layout_divider_options(EditorLayoutDrag::Dock)))
                captureDivider(EditorLayoutDrag::Dock, bottom_split_pos);
            else if (gui::drag_surface_hit_test(*dividerPress, layout_divider_options(EditorLayoutDrag::BottomColumns)))
                captureDivider(EditorLayoutDrag::BottomColumns, bottomColumnsSplitPosition);
        }
        if (editor.layoutDrag != EditorLayoutDrag::None)
        {
            const auto dividerOptions = layout_divider_options(editor.layoutDrag);
            if (!dividerOptions.enabled || dividerOptions.size.x <= 0.0f || dividerOptions.size.y <= 0.0f)
                editor.layoutDividerDrag = {};
            else
            {
                const auto origin = editor.layoutDividerDrag.pending
                    ? editor.layoutDividerDrag.press_position : dividerPress.value_or(mouse);
                const auto gesture = gui::drag_surface(editor.layoutDividerDrag, dividerOptions);
                if (core::cli::trace_menu_button0_rect)
                { traceDividerDelta=gesture.delta;traceDividerReleased=gesture.released; }
                if (gesture)
                {
                    const gui::Vec2 pointer{origin.x + gesture.delta.x, origin.y + gesture.delta.y};
                    switch (editor.layoutDrag)
                    {
                    case EditorLayoutDrag::Outliner:
                        editor.outlinerSplit=std::clamp((pointer.x-editor.layoutDividerDragOffset)/(std::max)(1.0f,w),0.06f,0.70f);break;
                    case EditorLayoutDrag::Inspector:
                        editor.inspectorSplit=std::clamp((w-pointer.x+editor.layoutDividerDragOffset-right_split_w)/(std::max)(1.0f,w),0.06f,0.70f);break;
                    case EditorLayoutDrag::Dock:
                        editor.dockSplit=std::clamp((h-pointer.y+editor.layoutDividerDragOffset-bottom_split_h)
                            /(std::max)(1.0f,h-toolbar_h-splitter_h),0.08f,0.80f);break;
                    case EditorLayoutDrag::BottomColumns:
                        editor.bottomGridSplit=gui_lib::bottom_dock_column_fraction_from_pointer(
                            bottomColumnOptions,pointer.x-bottom_pos.x,editor.layoutDividerDragOffset);break;
                    default: break;
                    }
                    editor.surfaceSettleFrames=(std::max)(editor.surfaceSettleFrames,1);
                    result.scene_input_captured=true;
                }
            }
            if (!editor.layoutDividerDrag.pending) editor.layoutDrag=EditorLayoutDrag::None;
        }
        if (editor.layoutDrag != EditorLayoutDrag::None)
            result.scene_input_captured = true;

        if (core::cli::trace_menu_button0_rect)
        {
            static thread_local unsigned dividerTraceFrames{};
            if (dividerTraceFrames < 120u && (dividerPress || gui::is_mouse_down()
                || traceDividerBefore != EditorLayoutDrag::None || editor.layoutDrag != EditorLayoutDrag::None))
            {
                ++dividerTraceFrames;
                try
                {
                    const auto rect = [](gui::Vec2 pos,gui::Vec2 size)
                    { return std::to_string(pos.x)+","+std::to_string(pos.y)+","+std::to_string(size.x)+","+std::to_string(size.y); };
                    logger::info("PointerTrace.Divider", "frame=" + std::to_string(dividerTraceFrames)
                        + " ctx=" + std::to_string(reinterpret_cast<std::uintptr_t>(ctx.get()))
                        + " role=" + std::to_string(static_cast<unsigned>(traceDividerBefore)) + "," + std::to_string(static_cast<unsigned>(editor.layoutDrag))
                        + " pending=" + std::to_string(traceDividerPendingBefore) + "," + std::to_string(editor.layoutDividerDrag.pending)
                        + " press=" + (dividerPress ? std::to_string(dividerPress->x)+","+std::to_string(dividerPress->y) : std::string{"none"})
                        + " mouse=" + std::to_string(mouse.x)+","+std::to_string(mouse.y)
                        + " origin=" + std::to_string(editor.layoutDividerDrag.press_position.x)+","+std::to_string(editor.layoutDividerDrag.press_position.y)
                        + " delta=" + std::to_string(traceDividerDelta.x)+","+std::to_string(traceDividerDelta.y)
                        + " released=" + std::to_string(traceDividerReleased)
                        + " blocked_modal_menu_floating=" + std::to_string(dividerInputBlocked)+","+std::to_string(modal_visible_now())
                            + ","+std::to_string(editor.openMenu!=TopMenu::None)+","+std::to_string(floating_gui_visible())
                        + " rect_left="+rect(outliner_split_pos,outliner_split_size)
                        + " rect_right="+rect(inspector_split_pos,inspector_split_size)
                        + " rect_bottom="+rect(bottom_split_pos,bottom_split_size)
                        + " rect_columns="+rect(bottomColumnsSplitPosition,bottomColumnsSplitSize)
                        + " splits="+std::to_string(editor.outlinerSplit)+","+std::to_string(editor.inspectorSplit)
                            + ","+std::to_string(editor.dockSplit)+","+std::to_string(editor.bottomGridSplit));
                }
                catch (...) { /* Diagnostics cannot change layout or input. */ }
            }
        }
        if (modal_visible_now())
            editor.openMenu = TopMenu::None;

        enum class PaneDropTarget : std::uint8_t
        {
            None = 0,
            LeftTabs,
            RightTabs,
            BottomLeftTabs,
            BottomRightTabs,
            LeftContext,
            RightContext,
            Floating
        };

        constexpr std::size_t toolDockRegionCount = 4u;
        const auto tool_dock_region_index = [](EditorToolDockRegion region) noexcept
        {
            return static_cast<std::size_t>(region);
        };
        std::array<gui_lib::Rect, toolDockRegionCount> directTabStripBounds{};
        std::array<std::vector<gui_lib::Rect>, toolDockRegionCount>
            directTabBounds{};
        std::array<std::vector<std::string_view>, toolDockRegionCount>
            directTabRoutes{};
        editor.paneTitleDragFrame = {};

        auto reset_pane_title_drag = [&]() noexcept
        {
            editor.paneTitleDragActive = false;
            editor.paneTitleDragRoute.clear();
            editor.paneTitleDragLabel.clear();
            editor.paneTitleDragStart = {};
            editor.paneTitleDrag = {};
            editor.paneTitleDragFrame = {};
        };
        if (editor.paneTitleDragActive
            && (modal_visible_now() || editor.openMenu != TopMenu::None
                || floating_gui_visible() || !pane_is_shown(editor.paneTitleDragRoute)))
            reset_pane_title_drag();

        auto direct_tab_group = [&](EditorToolDockRegion region)
        {
            gui_lib::DockTabGroup group{};
            group.id = tool_dock_region_index(region) + 1u;
            const std::string_view active = active_pane_for_region(region);
            for (const std::size_t paneIndex : ordered_pane_indices_for_region(region))
            {
                if (group.count >= gui_lib::maximum_dock_tabs)
                    break;
                group.tabs[group.count] = {
                    .id = paneIndex + 1u,
                    .remembered_group_id = group.id,
                    .keyboard_order = group.count,
                    .active = active == toolPaneRoutes[paneIndex],
                    .closable = true
                };
                ++group.count;
            }
            return group;
        };
        auto apply_direct_tab_group = [&](const gui_lib::DockTabGroup& group,
            EditorToolDockRegion region)
        {
            std::array<bool, kEditorToolPaneCount> assigned{};
            std::string_view active{};
            std::uint32_t nextOrder{};
            for (std::uint32_t slot = 0u; slot < group.count; ++slot)
            {
                const std::uint64_t id = group.tabs[slot].id;
                if (id == 0u || id > kEditorToolPaneCount)
                    continue;
                const std::size_t paneIndex = static_cast<std::size_t>(id - 1u);
                assigned[paneIndex] = true;
                editor.toolPaneDockRegions[paneIndex] = region;
                editor.toolPaneTabOrder[paneIndex] = nextOrder++;
                if (group.tabs[slot].active)
                    active = toolPaneRoutes[paneIndex];
            }

            std::vector<std::size_t> hiddenOrDetached{};
            for (std::size_t paneIndex = 0u; paneIndex < kEditorToolPaneCount;
                ++paneIndex)
            {
                if (!assigned[paneIndex]
                    && editor.toolPaneDockRegions[paneIndex] == region)
                    hiddenOrDetached.push_back(paneIndex);
            }
            std::sort(hiddenOrDetached.begin(), hiddenOrDetached.end(),
                [&](std::size_t left, std::size_t right)
                {
                    const std::uint32_t leftOrder = editor.toolPaneTabOrder[left];
                    const std::uint32_t rightOrder = editor.toolPaneTabOrder[right];
                    return leftOrder != rightOrder ? leftOrder < rightOrder : left < right;
                });
            for (const std::size_t paneIndex : hiddenOrDetached)
                editor.toolPaneTabOrder[paneIndex] = nextOrder++;

            if (!active.empty())
                set_active_pane(region, active);
            else if (group.count == 0u)
                active_pane_storage(region).clear();
        };
        auto move_pane_to_direct_tab_slot = [&](std::string_view route,
            EditorToolDockRegion targetRegion,
            std::uint32_t insertionIndex)
        {
            const auto pane = editor_tool_pane_from_route(route);
            if (!pane)
                return false;
            const std::size_t paneIndex = tool_pane_index(*pane);
            const EditorToolDockRegion sourceRegion =
                editor.toolPaneDockRegions[paneIndex];
            gui_lib::DockTabGroup source = direct_tab_group(sourceRegion);
            std::uint32_t sourceIndex = gui_lib::invalid_dock_tab_index;
            for (std::uint32_t index = 0u; index < source.count; ++index)
            {
                if (source.tabs[index].id == paneIndex + 1u)
                {
                    sourceIndex = index;
                    break;
                }
            }
            if (sourceIndex == gui_lib::invalid_dock_tab_index)
                return false;

            if (sourceRegion == targetRegion)
            {
                const auto moved = gui_lib::move_dock_tab(
                    source, source, sourceIndex, insertionIndex);
                if (!moved)
                    return false;
                apply_direct_tab_group(source, sourceRegion);
                return true;
            }

            gui_lib::DockTabGroup target = direct_tab_group(targetRegion);
            const auto moved = gui_lib::move_dock_tab(
                source, target, sourceIndex, insertionIndex);
            if (!moved)
                return false;
            apply_direct_tab_group(source, sourceRegion);
            apply_direct_tab_group(target, targetRegion);
            return true;
        };

        auto capture_pane_title_drag = [&](
            std::string_view route,
            std::string_view label,
            const gui::TabBarResult& tabs)
        {
            if (tabs.drag.pressed)
            {
                editor.paneTitleDragActive = true;
                editor.paneTitleDragRoute.assign(route);
                editor.paneTitleDragLabel.assign(label);
                editor.paneTitleDragStart = tabs.drag_origin;
            }
            if (editor.paneTitleDragActive && editor.paneTitleDragRoute == route)
            {
                editor.paneTitleDragFrame = tabs.drag;
                result.scene_input_captured = true;
                ctx->set_gui_overlay_priority(true);
            }
        };

        const auto pane_drop_target_from_guide = [](gui::DockGuideTarget target) noexcept
        {
            switch (target)
            {
            case gui::DockGuideTarget::left_tabs: return PaneDropTarget::LeftTabs;
            case gui::DockGuideTarget::right_tabs: return PaneDropTarget::RightTabs;
            case gui::DockGuideTarget::bottom_left_tabs: return PaneDropTarget::BottomLeftTabs;
            case gui::DockGuideTarget::bottom_right_tabs: return PaneDropTarget::BottomRightTabs;
            case gui::DockGuideTarget::left_context: return PaneDropTarget::LeftContext;
            case gui::DockGuideTarget::right_context: return PaneDropTarget::RightContext;
            case gui::DockGuideTarget::float_window: return PaneDropTarget::Floating;
            case gui::DockGuideTarget::none:
            default: return PaneDropTarget::None;
            }
        };

        auto update_pane_dock_drag = [&]()
        {
            if (!editor.paneTitleDragActive)
                return;

            result.scene_input_captured = true;
            ctx->set_gui_overlay_priority(true);
            const bool moved = editor.paneTitleDragFrame.dragging
                || editor.paneTitleDragFrame.released;
            if (!moved)
            {
                if (!editor.paneTitleDrag.pending || !gui::is_mouse_down())
                    reset_pane_title_drag();
                return;
            }
            const gui::Vec2 dragMouse{
                editor.paneTitleDragStart.x + editor.paneTitleDragFrame.delta.x,
                editor.paneTitleDragStart.y + editor.paneTitleDragFrame.delta.y};

            const auto sourcePane = editor_tool_pane_from_route(
                editor.paneTitleDragRoute);
            if (sourcePane)
            {
                const EditorToolDockRegion sourceRegion =
                    editor.toolPaneDockRegions[tool_pane_index(*sourcePane)];
                const std::size_t sourceRegionIndex =
                    tool_dock_region_index(sourceRegion);
                std::uint32_t sourceTabIndex = gui_lib::invalid_dock_tab_index;
                gui_lib::Vec2 draggedTabSize{120.0f, 28.0f};
                for (std::size_t index = 0u;
                    index < directTabRoutes[sourceRegionIndex].size(); ++index)
                {
                    if (directTabRoutes[sourceRegionIndex][index]
                        != editor.paneTitleDragRoute)
                        continue;
                    sourceTabIndex = static_cast<std::uint32_t>(index);
                    if (index < directTabBounds[sourceRegionIndex].size())
                        draggedTabSize = directTabBounds[sourceRegionIndex][index].size;
                    break;
                }

                std::optional<EditorToolDockRegion> directTargetRegion{};
                gui_lib::DockTabStripLayout directLayout{};
                if (sourceTabIndex != gui_lib::invalid_dock_tab_index)
                {
                    for (std::size_t regionIndex = 0u;
                        regionIndex < toolDockRegionCount; ++regionIndex)
                    {
                        const auto region = static_cast<EditorToolDockRegion>(
                            regionIndex);
                        const auto& tabBounds = directTabBounds[regionIndex];
                        const auto candidate = gui_lib::make_dock_tab_strip_layout({
                            .strip_bounds = directTabStripBounds[regionIndex],
                            .tab_bounds = tabBounds.empty() ? nullptr : tabBounds.data(),
                            .tab_count = static_cast<std::uint32_t>(tabBounds.size()),
                            .pointer = {dragMouse.x, dragMouse.y},
                            .dragged_tab_size = draggedTabSize,
                            .source_group_id = sourceRegionIndex + 1u,
                            .target_group_id = regionIndex + 1u,
                            .source_index = sourceTabIndex,
                            .marker_extent = 3.0f,
                            .drag_active = true,
                            .target_compatible = true,
                            .cancelled = false
                        });
                        if (!candidate.target_hovered)
                            continue;
                        directTargetRegion = region;
                        directLayout = candidate;
                        break;
                    }
                }

                if (directTargetRegion && directLayout.direct_drop_available)
                {
                    gui::render_dock_tab_insertion_overlay({
                        .insertion_marker = {
                            {directLayout.insertion_marker.position.x,
                                directLayout.insertion_marker.position.y},
                            {directLayout.insertion_marker.size.x,
                                directLayout.insertion_marker.size.y}},
                        .insertion_ghost = {
                            {directLayout.insertion_ghost.position.x,
                                directLayout.insertion_ghost.position.y},
                            {directLayout.insertion_ghost.size.x,
                                directLayout.insertion_ghost.size.y}},
                        .moving_label = editor.paneTitleDragLabel,
                        .no_op = directLayout.no_op
                    });
                    if (editor.paneTitleDragFrame.released)
                    {
                        const bool accepted = directLayout.no_op
                            || move_pane_to_direct_tab_slot(
                                editor.paneTitleDragRoute,
                                *directTargetRegion,
                                directLayout.insertion_index);
                        if (accepted)
                        {
                            editor.detachedPanelHostStatus = directLayout.no_op
                                ? "Tab kept its current direct slot."
                                : "Tab moved through the direct insertion slot; outer docking guides remain available.";
                            push_editor_log(editor,
                                "[window] " + editor.paneTitleDragLabel
                                + (directLayout.no_op
                                    ? " kept its tab position."
                                    : " moved to a direct tab slot."));
                        }
                        else
                        {
                            push_editor_log(editor,
                                "[window] Direct tab move was rejected without changing the layout.");
                        }
                        reset_pane_title_drag();
                    }
                    result.scene_input_captured = true;
                    return;
                }
            }

            const float leftPreviewWidth = (std::max)(left_w, w * 0.24f);
            const float rightPreviewWidth = (std::max)(right_w, w * 0.24f);
            const float bottomPreviewHeight = (std::max)(bottom_h, h * 0.25f);
            const auto bottomPreview = gui_lib::make_bottom_dock_column_layout({
                .viewport_width = w, .splitter_width = splitter_w,
                .requested_fraction = editor.bottomGridSplit });
            const float bottomLeftPreviewWidth = bottomPreview.left_width;
            const float bottomRightPreviewX = bottomPreview.right_offset;
            const float bottomRightPreviewWidth = bottomPreview.right_width;
            const gui::Vec2 floatingPreviewSize{
                (std::min)(520.0f, (std::max)(280.0f, viewport_size.x * 0.48f)),
                (std::min)(420.0f, (std::max)(220.0f, viewport_size.y * 0.52f))
            };
            const gui::DockGuideLayout dockGuides = gui::make_dock_guide_layout(
                gui::DockGuideOptions{
                    .guide_bounds = { viewport_pos, viewport_size },
                    .left_tabs_preview = { { 0.0f, toolbar_h }, { leftPreviewWidth, main_h } },
                    .right_tabs_preview = {
                        { (std::max)(0.0f, w - rightPreviewWidth), toolbar_h },
                        { rightPreviewWidth, main_h } },
                    .bottom_left_tabs_preview = {
                        { 0.0f, (std::max)(toolbar_h, h - bottomPreviewHeight) },
                        { bottomLeftPreviewWidth, bottomPreviewHeight } },
                    .bottom_right_tabs_preview = {
                        { bottomRightPreviewX, (std::max)(toolbar_h, h - bottomPreviewHeight) },
                        { bottomRightPreviewWidth, bottomPreviewHeight } },
                    .left_context_preview = {},
                    .right_context_preview = {},
                    .floating_preview = {
                        { dragMouse.x - floatingPreviewSize.x * 0.5f, dragMouse.y - 24.0f },
                        floatingPreviewSize },
                    .pointer = dragMouse,
                    .guide_extent = 94.0f,
                    .guide_gap = 8.0f,
                    .allow_side_tabs = true,
                    .allow_bottom_tabs = true,
                    .allow_contexts = false,
                    .allow_float = false
                });
            const PaneDropTarget hovered = pane_drop_target_from_guide(
                dockGuides.hovered_target);

            gui::render_dock_guide_overlay(dockGuides, {
                .moving_label = editor.paneTitleDragLabel,
                .floating_preview = {
                    { dragMouse.x - floatingPreviewSize.x * 0.5f, dragMouse.y - 24.0f },
                    floatingPreviewSize },
                .show_floating_preview = hovered == PaneDropTarget::None
            });

            if (editor.paneTitleDragFrame.released)
            {
                const std::string route = editor.paneTitleDragRoute;
                const std::string label = editor.paneTitleDragLabel;
                switch (hovered)
                {
                case PaneDropTarget::LeftTabs:
                    request_pane_region(
                        route, label, EditorToolDockRegion::Left);
                    break;
                case PaneDropTarget::RightTabs:
                    request_pane_region(
                        route, label, EditorToolDockRegion::Right);
                    break;
                case PaneDropTarget::BottomLeftTabs:
                    request_pane_region(
                        route, label, EditorToolDockRegion::BottomLeft);
                    break;
                case PaneDropTarget::BottomRightTabs:
                    request_pane_region(
                        route, label, EditorToolDockRegion::BottomRight);
                    break;
                case PaneDropTarget::LeftContext:
                case PaneDropTarget::RightContext:
                    break;
                case PaneDropTarget::Floating:
                    request_pane_external_window(route, label);
                    break;
                case PaneDropTarget::None:
                    request_pane_external_window(route, label);
                    break;
                default: break;
                }
                reset_pane_title_drag();
            }
            else if (!gui::is_mouse_down())
            {
                reset_pane_title_drag();
            }
        };
        auto render_native_pane_dock_projection = [&](
            const core::RoutedPanelDockDragProjection& projection)
        {
            if (projection.host_overlay)
            {
                core::publish_routed_panel_tab_drop_target(
                    {}, core::RoutedPanelDockTarget::none, 0xffffffffu);
                result.scene_input_captured = true;
                ctx->set_gui_overlay_priority(true);
                return;
            }

            const bool routedPane = projection.routed_panel;
            const float projectionScaleX = projection.host_width > 0.0f
                ? w / projection.host_width
                : 1.0f;
            const float projectionScaleY = projection.host_height > 0.0f
                ? h / projection.host_height
                : 1.0f;
            const gui::Vec2 projectedPointer{
                projection.cursor_x * projectionScaleX,
                projection.cursor_y * projectionScaleY
            };

            std::optional<EditorToolDockRegion> directTargetRegion{};
            gui_lib::DockTabStripLayout directLayout{};
            if (routedPane)
            {
                for (std::size_t regionIndex = 0u;
                    regionIndex < toolDockRegionCount; ++regionIndex)
                {
                    const auto& tabBounds = directTabBounds[regionIndex];
                    const auto candidate = gui_lib::make_dock_tab_strip_layout({
                        .strip_bounds = directTabStripBounds[regionIndex],
                        .tab_bounds = tabBounds.empty() ? nullptr : tabBounds.data(),
                        .tab_count = static_cast<std::uint32_t>(tabBounds.size()),
                        .pointer = {projectedPointer.x, projectedPointer.y},
                        .dragged_tab_size = {160.0f, 28.0f},
                        .source_group_id = toolDockRegionCount + 1u,
                        .target_group_id = regionIndex + 1u,
                        .source_index = gui_lib::invalid_dock_tab_index,
                        .marker_extent = 4.0f,
                        .drag_active = true,
                        .target_compatible = true,
                        .cancelled = false
                    });
                    if (!candidate.target_hovered)
                        continue;
                    directTargetRegion = static_cast<EditorToolDockRegion>(
                        regionIndex);
                    directLayout = candidate;
                    break;
                }
            }

            if (directTargetRegion && directLayout.direct_drop_available)
            {
                gui::render_dock_tab_insertion_overlay({
                    .insertion_marker = {
                        {directLayout.insertion_marker.position.x,
                            directLayout.insertion_marker.position.y},
                        {directLayout.insertion_marker.size.x,
                            directLayout.insertion_marker.size.y}},
                    .insertion_ghost = {
                        {directLayout.insertion_ghost.position.x,
                            directLayout.insertion_ghost.position.y},
                        {directLayout.insertion_ghost.size.x,
                            directLayout.insertion_ghost.size.y}},
                    .moving_label = projection.route,
                    .no_op = false
                });
                const core::RoutedPanelDockTarget target =
                    *directTargetRegion == EditorToolDockRegion::Left
                    ? core::RoutedPanelDockTarget::left_tabs
                    : *directTargetRegion == EditorToolDockRegion::Right
                        ? core::RoutedPanelDockTarget::right_tabs
                        : *directTargetRegion == EditorToolDockRegion::BottomLeft
                            ? core::RoutedPanelDockTarget::bottom_left_tabs
                            : core::RoutedPanelDockTarget::bottom_right_tabs;
                core::publish_routed_panel_tab_drop_target(
                    projection.route,
                    target,
                    directLayout.insertion_index);
                result.scene_input_captured = true;
                ctx->set_gui_overlay_priority(true);
                return;
            }
            core::publish_routed_panel_tab_drop_target(
                {}, core::RoutedPanelDockTarget::none, 0xffffffffu);

            const float leftPreviewWidth = (std::max)(left_w, w * 0.24f);
            const float rightPreviewWidth = (std::max)(right_w, w * 0.24f);
            const float bottomPreviewHeight = (std::max)(bottom_h, h * 0.25f);
            const auto bottomPreview = gui_lib::make_bottom_dock_column_layout({
                .viewport_width = w, .splitter_width = splitter_w,
                .requested_fraction = editor.bottomGridSplit });
            const float bottomLeftPreviewWidth = bottomPreview.left_width;
            const float bottomRightPreviewX = bottomPreview.right_offset;
            const float bottomRightPreviewWidth = bottomPreview.right_width;
            const gui::DockGuideLayout nativeGuides = gui::make_dock_guide_layout(
                gui::DockGuideOptions{
                    .guide_bounds = {
                        { 0.0f, 0.0f },
                        { w, h } },
                    .left_tabs_preview = { { 0.0f, toolbar_h }, { leftPreviewWidth, main_h } },
                    .right_tabs_preview = {
                        { (std::max)(0.0f, w - rightPreviewWidth), toolbar_h },
                        { rightPreviewWidth, main_h } },
                    .bottom_left_tabs_preview = {
                        { 0.0f, (std::max)(toolbar_h, h - bottomPreviewHeight) },
                        { bottomLeftPreviewWidth, bottomPreviewHeight } },
                    .bottom_right_tabs_preview = {
                        { bottomRightPreviewX, (std::max)(toolbar_h, h - bottomPreviewHeight) },
                        { bottomRightPreviewWidth, bottomPreviewHeight } },
                    .left_context_preview = {
                        { 0.0f, toolbar_h },
                        { w * 0.5f, main_h } },
                    .right_context_preview = {
                        { w * 0.5f, toolbar_h },
                        { w * 0.5f, main_h } },
                    .floating_preview = {},
                    .pointer = projectedPointer,
                    .guide_extent = 94.0f,
                    .guide_gap = 8.0f,
                    .allow_side_tabs = routedPane,
                    .allow_bottom_tabs = routedPane,
                    .allow_contexts = true,
                    .allow_float = false
                });

            std::string movingLabel = projection.route;
            if (routedPane)
            {
                if (const auto pane = editor_tool_pane_from_route(projection.route))
                {
                    movingLabel.assign(
                        editor_tool_pane_labels()[tool_pane_index(*pane)]);
                }
            }
            gui::render_dock_guide_overlay(nativeGuides, {
                .moving_label = movingLabel,
                .left_tabs_label = "Left Tabs",
                .right_tabs_label = "Right Tabs",
                .bottom_left_tabs_label = "Bottom Left",
                .bottom_right_tabs_label = "Bottom Right",
                .left_context_label = "Left Context",
                .right_context_label = "Right Context"
            });
            result.scene_input_captured = true;
            ctx->set_gui_overlay_priority(true);
        };
