// Epoch feature implementation; included by its owning module only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
    ConsoleWindowResult console_window(const ConsoleWindowOptions& options) noexcept
    {
        ConsoleWindowResult result{};
        if (!g_frame.ctx) return result;

        // Reserve popup input ownership before the transcript and composer
        // process this frame's press, not after they could click through.
        const std::string composerMenuId = options.log_id.empty()
            ? std::string{options.title} : std::string{options.log_id};
        const auto composerMenu = g_consoleComposerMenus.find(composerMenuId);
        const bool composerPopupWasOpen = options.chat_composer
            && composerMenu != g_consoleComposerMenus.end() && composerMenu->second.open;
        if (composerPopupWasOpen
            || (options.model_picker_open && !options.model_picker_actions.empty()))
            block_background_for_popup();
        begin_window(options.title, options.position, options.size, true, options.show_title_bar);
        if (!g_frame.insideWindow || !g_frame.ctx) { end_window(); return result; }

        // A drop belongs only to the visible chat surface under its own native
        // coordinates. Never attach a scene drop or deliver it to another dock.
        if (options.chat_composer)
            for (auto event = g_frame.events.begin(); event != g_frame.events.end();)
            {
                if (event->type == EventType::FileDrop
                    && point_in_rect(event->mouse_pos, options.position.x, options.position.y,
                        options.size.x, options.size.y)
                    && g_frame.modalInput.pointer_allowed({event->mouse_pos.x, event->mouse_pos.y})
                    && popup_input::allows_pointer({
                        .selection_open_this_frame = g_frame.selectPopupCapturedThisFrame,
                        .current_selection_owner = g_frame.selectPopupOwnsInput,
                        .application_menu_open_this_frame = g_frame.menuPopupBlocksBackground,
                        .application_menu_owner = g_frame.menuPopupOwnsInput}))
                {
                    if (result.dropped_files.size() + event->files.size() <= 16u)
                        result.dropped_files.insert(result.dropped_files.end(),
                            event->files.begin(), event->files.end());
                    event = g_frame.events.erase(event);
                }
                else ++event;
            }

        ensure_resources();
        const float availableWidth = (std::max)(0.0f, options.size.x - 2.0f * kContentPadding);

        const float controlRowHeight = base_line_height(kFontScale)
            + 2.0f * kBoxInnerPadding;
        const bool taskVisible = options.task_editing
            || !options.task_label.empty() || !options.task_value.empty()
            || !options.task_actions.empty();
        std::string taskDisplayText{};
        if (taskVisible && !options.task_editing)
        {
            if (!options.task_label.empty())
                taskDisplayText = std::string{options.task_label} + ": ";
            taskDisplayText += options.task_value;
        }
        const float taskValueHeight = !taskVisible
            ? 0.0f
            : options.task_editing || (!options.task_wrap
                    && options.task_tone == SemanticTone::neutral)
                ? controlRowHeight
                : (std::max)(
                    controlRowHeight,
                    measure_wrapped_text_height(
                        taskDisplayText,
                        (std::max)(1.0f, availableWidth - 20.0f),
                        kFontScale) + 24.0f);
        // Multiple actions reflow rather than shrinking to illegible widths.
        // Height reservation uses the SAME row-break rule as hit testing.
        const auto action_rows = [&](std::span<const ConsoleWindowActionSpec> actions,
            float gap) noexcept -> std::size_t
        {
            if (actions.empty()) return 0u;
            const float room = (std::max)(1.0f, availableWidth);
            const float spacing = (std::max)(0.0f, gap);
            std::size_t rows = 1u;
            float consumed = 0.0f;
            for (const auto& action : actions)
            {
                const float width = (std::min)(room,
                    (std::max)(32.0f, action.width));
                if (consumed > 0.0f && consumed + spacing + width > room)
                {
                    ++rows;
                    consumed = 0.0f;
                }
                consumed += (consumed > 0.0f ? spacing : 0.0f) + width;
            }
            return rows;
        };
        const auto action_height = [&](std::span<const ConsoleWindowActionSpec> actions,
            float gap) noexcept
        {
            const std::size_t rows = action_rows(actions, gap);
            return rows == 0u ? 0.0f
                : static_cast<float>(rows) * controlRowHeight
                    + static_cast<float>(rows - 1u) * 4.0f;
        };
        const float taskActionHeight = action_height(
            options.task_actions, options.task_action_gap);
        // Measure the text against exactly the room left by in-bar controls.
        // A narrower dock moves the controls beneath the input instead of
        // overlaying text, selection, or the caret.
        const bool composerMic = options.chat_composer
            && options.composer_microphone
            && gui_lib::chat_composer::microphone_fits(
                availableWidth, options.composer_pause);
        const float compactTextWidth = gui_lib::chat_composer::single_line_text_width(
            availableWidth, composerMic, options.composer_pause);
        const float compactWrapWidth = (std::max)(1.0f,
            compactTextWidth - 2.0f * kBoxInnerPadding);
        const bool composerMultiline = options.chat_composer && options.input
            && (compactTextWidth < 80.0f
                || edit_box_visual_lines(*options.input,
                    compactWrapWidth, kFontScale, true).size() > 1u);
        const float expandedWrapWidth = (std::max)(1.0f,
            gui_lib::chat_composer::expanded_text_width(availableWidth)
                - 2.0f * kBoxInnerPadding);
        const std::size_t composerLines = options.chat_composer && options.input
            ? edit_box_visual_lines(*options.input,
                composerMultiline ? expandedWrapWidth : compactWrapWidth,
                kFontScale, true).size()
            : 1u;
        const float ordinaryFieldHeight = options.multiline_input
            ? 3.0f * base_line_height(kFontScale) + 2.0f * kBoxInnerPadding
            : controlRowHeight;
        const auto composerLayout = gui_lib::chat_composer::measure(
            availableWidth, line_advance_amount(kFontScale),
            base_line_height(kFontScale) + 2.0f * kBoxInnerPadding,
            composerLines, composerMultiline, composerMic, options.composer_pause);
        const float fieldHeight = options.input
            ? options.chat_composer ? composerLayout.input.height
                : ordinaryFieldHeight
            : 0.0f;
        const float headerHeight = action_height(
            options.header_actions, options.header_action_gap);
        const float topHeaderHeight = options.header_actions_at_input ? 0.0f : headerHeight;
        const float activityHeight = options.activity_text.empty() ? 0.0f
            : 2.0f * base_line_height(kFontScale) + 2.0f * kBoxInnerPadding;
        float inlineHeight = options.inline_controls
            ? (std::max)(0.0f, (std::min)(options.inline_controls_height, options.size.y * 0.35f))
            : 0.0f;
        const float messageHeight = action_height(
            options.message_actions, options.message_action_gap);
        const float footerHeight = action_height(
            options.footer_actions, options.footer_action_gap);
        const float interControlGap = options.input && !options.footer_actions.empty()
            ? kContentPadding
            : 0.0f;

        const float taskBlockHeight = taskValueHeight
            + (taskValueHeight > 0.0f && taskActionHeight > 0.0f
                ? kContentPadding : 0.0f)
            + taskActionHeight;
        const float contentTopY = g_frame.cursor.y;
        const float contentX = g_frame.cursor.x;
        const float contentBottomY = options.position.y + options.size.y - kContentPadding;
        const float headerY = contentTopY + taskBlockHeight
            + (taskBlockHeight > 0.0f && topHeaderHeight > 0.0f
                ? kContentPadding : 0.0f);
        float logTopY = headerY + topHeaderHeight
            + (taskBlockHeight > 0.0f && topHeaderHeight == 0.0f
                ? kContentPadding : 0.0f)
            + (topHeaderHeight > 0.0f ? kContentPadding : 0.0f)
            + (activityHeight > 0.0f ? activityHeight + kContentPadding : 0.0f)
            + (inlineHeight > 0.0f ? inlineHeight + kContentPadding : 0.0f);
        const float composerHeight = options.chat_composer && options.input
            ? composerLayout.background.height : fieldHeight;
        auto& filePreview = g_consoleFilePreviews[composerMenuId];
        if (!filePreview.name.empty() && std::ranges::none_of(options.composer_files,
                [&](const auto& file) { return file.name == filePreview.name && file.content == filePreview.content; }))
            filePreview = {};
        const std::size_t attachmentColumns = availableWidth >= 400.0f ? 2u : 1u;
        const auto imageCount = (std::min)(options.composer_image_previews.size(), std::size_t{4u});
        const float imageHeight = imageCount != 0u
            ? static_cast<float>((imageCount + attachmentColumns - 1u) / attachmentColumns) * 60.0f
            : options.composer_attachment_labels.empty() ? 0.0f : 30.0f;
        const float fileHeight = static_cast<float>((options.composer_files.size() + attachmentColumns - 1u)
            / attachmentColumns) * 40.0f;
        const float filePreviewHeight = filePreview.name.empty() ? 0.0f : 142.0f;
        const float attachmentHeight = imageHeight + fileHeight + filePreviewHeight;
        const float controlHeight = attachmentHeight + composerHeight + interControlGap + footerHeight;
        float reservedBottom = 0.0f;
        if (options.header_actions_at_input && headerHeight > 0.0f)
            reservedBottom += headerHeight + kContentPadding;
        if (messageHeight > 0.0f)
            reservedBottom += messageHeight + kContentPadding;
        if (controlHeight > 0.0f)
            reservedBottom += controlHeight + kContentPadding;

        if (inlineHeight > 0.0f)
        {
            const auto remaining = (std::max)(0.0f,
                contentBottomY - (logTopY - inlineHeight) - reservedBottom - 48.0f);
            const auto bounded = (std::min)(inlineHeight, remaining);
            logTopY -= inlineHeight - bounded;
            inlineHeight = bounded;
            if (inlineHeight == 0.0f) logTopY -= kContentPadding;
        }

        const float logHeight = (std::max)(0.0f, contentBottomY - logTopY - reservedBottom);
        const Vec2 logPos{g_frame.cursor.x, logTopY};

        const auto render_actions = [&](
            const std::span<const ConsoleWindowActionSpec> actions,
            const float actionGap,
            const float actionY,
            std::optional<std::size_t>& selectedAction)
        {
            if (actions.empty())
                return;

            const float gap = (std::max)(0.0f, actionGap);
            const float right = contentX + availableWidth;
            float actionX = contentX;
            float rowY = actionY;
            for (std::size_t index = 0u; index < actions.size(); ++index)
            {
                const ConsoleWindowActionSpec& action = actions[index];
                const float actionWidth = (std::min)(availableWidth,
                    (std::max)(32.0f, action.width));
                if (actionX > contentX && actionX + actionWidth > right)
                {
                    actionX = contentX;
                    rowY += controlRowHeight + 4.0f;
                }
                const Vec2 actionPosition{actionX, rowY};
                const Vec2 actionSize{actionWidth, controlRowHeight};
                bool activatedOnPress = false;
                if (action.enabled && action.activate_on_press
                    && point_in_rect(
                        g_frame.mousePos,
                        actionPosition.x,
                        actionPosition.y,
                        actionSize.x,
                        actionSize.y)
                    && point_in_active_clip(g_frame.mousePos)
                    && left_press_available())
                {
                    activatedOnPress = true;
                    consume_left_press();
                }
                set_cursor({actionX, rowY});
                const bool clicked = button_with_state(
                    action.label,
                    actionSize,
                    false,
                    action.enabled,
                    action.tone);
                if (activatedOnPress
                    || (!action.activate_on_press && clicked))
                {
                    selectedAction = index;
                }
                actionX += actionWidth + gap;
            }
        };

        if (taskVisible)
        {
            set_cursor({contentX, contentTopY});
            if (options.task_editing && options.task_edit_buffer)
            {
                result.task_input = edit_box(
                    *options.task_edit_buffer,
                    {availableWidth, taskValueHeight},
                    options.task_max_input_chars,
                    false);
            }
            else if (options.task_wrap)
            {
                // Neutral status text, not another colored notification or
                // horizontally clipped pseudo-edit field.
                draw_wrapped_text(taskDisplayText, contentX + 8.0f,
                    contentTopY + 8.0f,
                    (std::max)(1.0f, availableWidth - 20.0f), kFontScale);
            }
            else if (options.task_tone != SemanticTone::neutral)
            {
                semantic_block(taskDisplayText, options.task_tone, availableWidth);
            }
            else
            {
                text_box(taskDisplayText, {availableWidth, taskValueHeight});
            }
            render_actions(
                options.task_actions,
                options.task_action_gap,
                contentTopY + taskValueHeight
                    + (taskActionHeight > 0.0f ? kContentPadding : 0.0f),
                result.task_action_index);
        }

        if (!options.header_actions_at_input)
            render_actions(options.header_actions, options.header_action_gap,
                headerY, result.header_action_index);

        float detailY = headerY + topHeaderHeight
            + (topHeaderHeight > 0.0f ? kContentPadding : 0.0f)
            + (taskBlockHeight > 0.0f && topHeaderHeight == 0.0f ? kContentPadding : 0.0f);
        if (activityHeight > 0.0f)
        {
            set_cursor({contentX, detailY});
            const std::vector<std::string> activityLines{std::string{options.activity_text}};
            (void)scroll_text_panel(ScrollTextPanelOptions{
                .id = std::string{options.log_id} + ".activity",
                .size = {availableWidth, activityHeight}, .lines = activityLines,
                .selectable = true});
            detailY += activityHeight + kContentPadding;
        }
        if (inlineHeight > 0.0f)
        {
            set_cursor({contentX, detailY});
            const std::string settingsId = std::string{options.log_id} + ".settings";
            (void)begin_scroll_area(ScrollAreaOptions{
                .id = settingsId, .size = {availableWidth, inlineHeight}});
            options.inline_controls((std::max)(1.0f, availableWidth - 20.0f));
            end_scroll_area();
        }

        if (availableWidth > 0.0f && logHeight > 0.0f)
        {
            set_cursor(logPos);
            const std::string panelId = options.log_id.empty()
                ? std::string(options.title) + ".log"
                : std::string(options.log_id);
            const ScrollTextPanelResult log = scroll_text_panel(ScrollTextPanelOptions{
                .id = panelId,
                .size = { availableWidth, logHeight },
                .lines = options.lines,
                .line_roles = options.line_roles,
                .line_run_details = options.line_run_details,
                .retryable_lines = options.retryable_lines,
                .retry_enabled = options.retry_enabled,
                .message_bubbles = options.message_bubbles,
                .conversation_style = options.conversation_style,
                .expand_activity = options.expand_activity,
                .incremental_line_measurements = options.incremental_line_measurements,
                .max_line_chars = options.max_visible_lines == 0 ? 768u : options.max_visible_lines * 16u,
                .selectable = true,
                .stick_to_bottom = options.follow_tail,
                .scroll_to_end_generation = options.scroll_to_end_generation
            });
            result.log_user_scrolled = log.user_scrolled;
            result.log_at_end = log.at_end;
            result.log_scroll_y = log.scroll_y;
            result.log_maximum_scroll_y = log.maximum_scroll_y;
            result.retry_line_index = log.retry_line_index;
            result.copy_line_index = log.copy_line_index;
            result.edit_line_index = log.edit_line_index;
        }

        const float messageY = logPos.y + logHeight
            + (messageHeight > 0.0f ? kContentPadding : 0.0f);
        render_actions(
            options.message_actions,
            options.message_action_gap,
            messageY,
            result.message_action_index);

        const float controlsY = messageY + messageHeight + attachmentHeight
            + (controlHeight > 0.0f ? kContentPadding : 0.0f)
            + (options.header_actions_at_input && headerHeight > 0.0f
                ? headerHeight + kContentPadding : 0.0f);
        if (options.header_actions_at_input)
            render_actions(options.header_actions, options.header_action_gap,
                controlsY - headerHeight - kContentPadding, result.header_action_index);
        set_cursor({ logPos.x, controlsY });

        if (options.input)
        {
            if (!options.composer_image_previews.empty())
            {
                const auto count = imageCount;
                const float cell = (std::max)(1.0f, (availableWidth - 6.0f * (attachmentColumns - 1u)) / attachmentColumns);
                for (std::size_t index = 0u; index < count; ++index)
                {
                    const auto& attachment = options.composer_image_previews[index];
                    const float x = logPos.x + (index % attachmentColumns) * (cell + 6.0f);
                    const float y = controlsY - attachmentHeight + (index / attachmentColumns) * 60.0f;
                    draw_rounded_sprite(active_palette().panelBackground, x, y, cell, 54.0f,
                        gui_lib::rounded_rect::RoundedRectStyle{.enabled = true, .control_radius = 8.0f});
                    set_cursor({x + 3.0f, y + 3.0f});
                    (void)image_box({.id = attachment.id, .sprite = attachment.preview,
                        .size = {48.0f, 48.0f}, .source_size = attachment.source_size,
                        .fit = ImageFit::Contain});
                    draw_text_line(fit_text_to_width(attachment.name, (std::max)(1.0f, cell - 92.0f), kFontScale),
                        x + 57.0f, y + 7.0f, kFontScale);
                    draw_text_line("Image attached", x + 57.0f, y + 30.0f, kFontScale);
                    set_cursor({x + cell - 30.0f, y + 13.0f});
                    if (button_with_state("x", {26.0f, 26.0f}, false, true, SemanticTone::neutral))
                        result.attachment_remove_index = index;
                }
                set_cursor({logPos.x, controlsY});
            }
            if (!options.composer_files.empty())
            {
                const float cell = (std::max)(1.0f,
                    (availableWidth - 6.0f * (attachmentColumns - 1u)) / attachmentColumns);
                for (std::size_t index = 0u; index < options.composer_files.size(); ++index)
                {
                    const auto& file = options.composer_files[index];
                    const float x = logPos.x + (index % attachmentColumns) * (cell + 6.0f);
                    const float y = controlsY - attachmentHeight + imageHeight
                        + (index / attachmentColumns) * 40.0f;
                    set_cursor({x, y});
                    const bool expanded = filePreview.name == file.name && filePreview.content == file.content;
                    const auto label = file.name + " · " + std::to_string(file.content.size()) + " B";
                    if (button_with_state(fit_text_to_width(label, (std::max)(1.0f, cell - 50.0f), kFontScale),
                            {(std::max)(1.0f, cell - 32.0f), 34.0f}, expanded, true, SemanticTone::info))
                        filePreview = expanded ? ComposerFilePreview{} : ComposerFilePreview{file.name, file.content};
                    set_cursor({x + cell - 28.0f, y + 4.0f});
                    if (button_with_state("x", {26.0f, 26.0f}, false, true, SemanticTone::neutral))
                        result.file_remove_index = index;
                }
                if (filePreviewHeight > 0.0f)
                {
                    set_cursor({logPos.x, controlsY - filePreviewHeight});
                    (void)source_editor(filePreview.content, SourceEditorOptions{
                        .id = composerMenuId + ".file-preview", .size = {availableWidth, 136.0f},
                        .read_only = true});
                }
                set_cursor({logPos.x, controlsY});
            }
            else if (options.composer_image_previews.empty() && !options.composer_attachment_labels.empty())
            {
                const auto count = options.composer_attachment_labels.size();
                const float chipWidth = (std::min)(180.0f, (availableWidth - 4.0f * (count - 1u)) / static_cast<float>(count));
                for (std::size_t index = 0u; index < count; ++index)
                {
                    set_cursor({logPos.x + index * (chipWidth + 4.0f), controlsY - attachmentHeight});
                    if (button_with_state(std::string{options.composer_attachment_labels[index]} + " x",
                            {chipWidth, 26.0f}, false, true, SemanticTone::info))
                        result.attachment_remove_index = index;
                }
                set_cursor({logPos.x, controlsY});
            }
            const float rowY = g_frame.cursor.y;

            if (options.chat_composer)
            {
                // EpochGUI owns the geometry. All three controls stay inside
                // the single rounded surface; they never consume text space.
                ScopedRoundedRectangles roundedComposer{g_frame.roundedRectStyle.enabled};
                draw_rounded_sprite(active_palette().textField,
                    logPos.x, rowY, composerLayout.background.width,
                    composerLayout.background.height,
                    gui_lib::rounded_rect::RoundedRectStyle{
                        .enabled = true,
                        .control_radius = gui_lib::chat_surface::composer_radius});
                const auto at = [&](const gui_lib::chat_composer::Rect& r)
                    -> Vec2 { return {logPos.x + r.x, rowY + r.y}; };
                set_cursor(at(composerLayout.add));
                const auto& menuId = composerMenuId;
                auto& menu = g_consoleComposerMenus[menuId];
                if (composerPopupWasOpen) begin_application_popup_input();
                const bool menuClicked = button_with_state("",
                    {composerLayout.add.width, composerLayout.add.height},
                    false, true, SemanticTone::neutral, true);
                if (composerPopupWasOpen) end_application_popup_input();
                const float addCenterX = logPos.x + composerLayout.add.x + composerLayout.add.width * 0.5f;
                const float addCenterY = rowY + composerLayout.add.y + composerLayout.add.height * 0.5f;
                const auto addMark = active_palette().buttonActive;
                draw_sprite(addMark, addCenterX - 6.0f, addCenterY - 1.0f, 12.0f, 2.0f);
                draw_sprite(addMark, addCenterX - 1.0f, addCenterY - 6.0f, 2.0f, 12.0f);
                if (menuClicked)
                {
                    menu.open = !menu.open;
                    menu.group.reset();
                }
                set_cursor(at(composerLayout.input));
                result.input = edit_box(*options.input,
                    {composerLayout.input.width, composerLayout.input.height},
                    options.max_input_chars, true, options.input_keyboard_policy,
                    true, options.composer_placeholder, true, true, options.composer_paste);
                if (composerLayout.has_microphone)
                {
                    set_cursor(at(composerLayout.microphone));
                    // Neutral microphone mark centered geometrically on the
                    // circular control (not a blue AI-specific pictogram).
                    const bool microphoneClicked = button_with_state("",
                        {composerLayout.microphone.width,
                            composerLayout.microphone.height},
                        false, true, SemanticTone::neutral, true);
                    const float centerX = logPos.x + composerLayout.microphone.x
                        + composerLayout.microphone.width * 0.5f;
                    const float centerY = rowY + composerLayout.microphone.y
                        + composerLayout.microphone.height * 0.5f;
                    const auto mark = active_palette().buttonActive;
                    // Bounds are symmetric around (centerX, centerY), including
                    // the top capsule and lower stem. The old glyph was offset
                    // 2.5px downward and used an unrelated bright-blue accent.
                    draw_rounded_sprite(mark, centerX - 3.0f, centerY - 9.0f,
                        6.0f, 11.0f,
                        gui_lib::rounded_rect::RoundedRectStyle{
                            .enabled = true, .control_radius = 3.0f});
                    draw_sprite(mark, centerX - 6.0f, centerY - 3.0f, 2.0f, 7.0f);
                    draw_sprite(mark, centerX + 4.0f, centerY - 3.0f, 2.0f, 7.0f);
                    draw_sprite(mark, centerX - 5.0f, centerY + 3.0f, 10.0f, 2.0f);
                    draw_sprite(mark, centerX - 1.0f, centerY + 4.0f, 2.0f, 4.0f);
                    draw_sprite(mark, centerX - 5.0f, centerY + 8.0f, 10.0f, 2.0f);
                    // A diagonal strike means the microphone is OFF; never
                    // imply recording when Windows has not acquired the owner.
                    if (!options.composer_microphone_active)
                    {
                        for (int i = 0; i < 9; ++i)
                            draw_sprite(mark, centerX - 9.0f + i * 2.0f,
                                centerY + 8.0f - i * 2.0f, 2.5f, 2.5f);
                    }
                    if (microphoneClicked)
                        result.composer_voice_action_index = 0u;
                }
                if (composerLayout.has_pause)
                {
                    set_cursor(at(composerLayout.pause));
                    if (button_with_state(options.composer_paused ? ">" : "II",
                            {composerLayout.pause.width, composerLayout.pause.height},
                            false, true, options.composer_paused
                                ? SemanticTone::success : SemanticTone::neutral, true))
                        result.composer_pause_clicked = true;
                }
                set_cursor(at(composerLayout.send));
                if (button_with_state(options.send_button_label,
                        {composerLayout.send.width, composerLayout.send.height},
                        false, options.send_button_enabled,
                        options.send_button_label == "X" ? SemanticTone::warning
                            : SemanticTone::info, true)
                    && options.send_button_enabled)
                    result.send_clicked = true;

                const bool groupedMenu = !options.composer_menu_groups.empty();
                if (menu.open && (groupedMenu || !options.composer_menu_actions.empty()))
                {
                    begin_application_popup_input();
                    // At most one category is expanded; never draw a 12-item
                    // popup that overflows the chat dock in a small window.
                    if (groupedMenu && menu.group
                        && *menu.group >= options.composer_menu_groups.size())
                        menu.group.reset();
                    const std::span<const ConsoleWindowActionSpec> menuActions =
                        groupedMenu && menu.group
                        ? options.composer_menu_groups[*menu.group].actions
                        : groupedMenu ? std::span<const ConsoleWindowActionSpec>{}
                            : options.composer_menu_actions;
                    // A bounded scroll surface keeps every action in one menu.
                    // Reuse normal GUI scrolling instead of hidden menu pages.
                    constexpr float itemHeight = 32.0f;
                    const bool groupSelected = groupedMenu && menu.group.has_value();
                    const std::size_t totalItems = groupedMenu && !groupSelected
                        ? options.composer_menu_groups.size() : menuActions.size();
                    const std::size_t backRows = groupSelected ? 1u : 0u;
                    const std::size_t firstItem = 0u;
                    const std::size_t lastItem = totalItems;
                    const float menuWidth = (std::min)(availableWidth, 320.0f);
                    const float menuHeight = (std::min)(
                        itemHeight * static_cast<float>(totalItems + backRows) + 12.0f,
                        (std::max)(64.0f, rowY - g_frame.origin.y - 12.0f));
                    const Vec2 menuPosition{logPos.x,
                        (std::max)(g_frame.origin.y + 4.0f,
                            rowY - menuHeight - 7.0f)};
                    const bool inMenu = point_in_rect(g_frame.mousePos,
                        menuPosition.x, menuPosition.y, menuWidth, menuHeight);
                    if (!menuClicked && left_press_available() && !inMenu
                        && !point_in_rect(g_frame.mousePos,
                            logPos.x + composerLayout.add.x,
                            rowY + composerLayout.add.y,
                            composerLayout.add.width,
                            composerLayout.add.height))
                    {
                        menu.open = false;
                        menu.group.reset();
                    }
                    if (menu.open)
                    {
                        begin_top_layer();
                        begin_window("", menuPosition, {menuWidth, menuHeight});
                        set_cursor({menuPosition.x + 4.0f, menuPosition.y + 4.0f});
                        (void)begin_scroll_area(ScrollAreaOptions{
                            .id = menuId + "/composer-actions",
                            .size = {menuWidth - 8.0f, menuHeight - 8.0f},
                            .draw_background = false, .show_scrollbar = true});
                        const Vec2 menuStart = cursor_position();
                        const auto drawMenuButton = [&](std::size_t row,
                            std::string_view label, bool enabled, SemanticTone tone)
                        {
                            set_cursor({menuStart.x, menuStart.y + itemHeight * static_cast<float>(row)});
                            return button_with_state(label,
                                {menuWidth - 30.0f, itemHeight - 2.0f},
                                false, enabled, tone);
                        };
                        if (groupedMenu && !menu.group)
                        {
                            for (std::size_t i = firstItem; i < lastItem; ++i)
                            {
                                const auto& group = options.composer_menu_groups[i];
                                if (drawMenuButton(i - firstItem, group.label,
                                    !group.actions.empty(), SemanticTone::neutral))
                                {
                                    menu.group = i;
                                                }
                            }
                        }
                        else
                        {
                            if (groupSelected && drawMenuButton(0u,
                                    "< Back to menu", true, SemanticTone::muted))
                            {
                                menu.group.reset();
                                        }
                            for (std::size_t i = firstItem; i < lastItem; ++i)
                            {
                                const auto& action = menuActions[i];
                                if (drawMenuButton(i - firstItem + backRows,
                                        action.label, action.enabled, action.tone))
                                {
                                    result.composer_menu_action_index = i;
                                    if (groupSelected)
                                        result.composer_menu_group_index = menu.group;
                                    menu.open = false;
                                    menu.group.reset();
                                                }
                            }
                        }
                        end_scroll_area();
                        end_window();
                        end_top_layer();
                    }
                    end_application_popup_input();
                }
            }
            else if (options.show_send_button)
            {
                const float buttonWidth = (std::max)(32.0f, options.send_button_width);
                const float inputWidth = (std::max)(1.0f, availableWidth - buttonWidth - kContentPadding);

                set_cursor({ logPos.x, rowY });
                Vec2 inputSize{ inputWidth, fieldHeight };
                result.input = edit_box(
                    *options.input,
                    inputSize,
                    options.max_input_chars,
                    options.multiline_input,
                    options.input_keyboard_policy);

                set_cursor({ logPos.x + inputWidth + kContentPadding, rowY });
                if (button(options.send_button_label, { buttonWidth, fieldHeight }) && options.send_button_enabled)
                    result.send_clicked = true;
            }
            else
            {
                Vec2 inputSize{ availableWidth, fieldHeight };
                result.input = edit_box(
                    *options.input,
                    inputSize,
                    options.max_input_chars,
                    options.multiline_input,
                    options.input_keyboard_policy);
            }
        }

        if (options.model_picker_open && !options.model_picker_actions.empty())
        {
            const float itemHeight = 31.0f;
            const float menuWidth = availableWidth;
            // Keep every resident model reachable by scrolling, including
            // when many models are loaded or the editor is vertically docked.
            const float fullHeight = 10.0f + (itemHeight + kContentPadding)
                * static_cast<float>(options.model_picker_actions.size());
            const float menuHeight = (std::min)(fullHeight,
                (std::max)(64.0f, controlsY - options.position.y - 14.0f));
            const Vec2 menuPos{contentX + (std::max)(0.0f, availableWidth - menuWidth),
                (std::max)(g_frame.origin.y + 4.0f,
                    controlsY - menuHeight - 5.0f)};
            // Outside clicks dismiss this picker without activating controls
            // beneath it; selection requires explicit ownership of this layer.
            if (g_frame.justPressed && !g_frame.mousePressConsumed
                && !point_in_rect(g_frame.mousePos, menuPos.x, menuPos.y,
                    menuWidth, menuHeight))
                result.model_picker_dismissed = true;
            begin_application_popup_input();
            begin_top_layer();
            begin_window("", menuPos, {menuWidth, menuHeight});
            set_cursor({menuPos.x + 4.0f, menuPos.y + 4.0f});
            const auto scroll = begin_scroll_area(ScrollAreaOptions{
                .id = std::string{options.log_id} + ".resident-models",
                .size = {menuWidth - 8.0f, menuHeight - 8.0f},
                .content_height = fullHeight,
                .draw_background = false,
                .show_scrollbar = true});
            (void)scroll;
            for (std::size_t i = 0; i < options.model_picker_actions.size(); ++i)
            {
                const auto& item = options.model_picker_actions[i];
                if (button_with_state(item.label,
                        {menuWidth - 22.0f, itemHeight - 2.0f},
                        false, item.enabled, item.tone))
                    result.model_picker_action_index = i;
            }
            end_scroll_area();
            end_window();
            end_top_layer();
            end_application_popup_input();
        }
        if (!options.footer_actions.empty())
        {
            const float footerY = controlsY + composerHeight + interControlGap;
            render_actions(
                options.footer_actions,
                options.footer_action_gap,
                footerY,
                result.footer_action_index);
        }

        end_window();
        return result;
    }
