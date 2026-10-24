// Epoch feature implementation; included by its owning module only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
    SelectBoxResult select_box(const SelectBoxOptions& options) noexcept
    {
        SelectBoxResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx)
            return result;

        ensure_resources();

        const Vec2 start = g_frame.cursor;
        const float availableWidth = (std::max)(1.0f, content_available_width(start.x));
        const float requestedWidth = options.size.x > 0.0f
            ? (std::max)(96.0f, options.size.x)
            : availableWidth;
        const float width = (std::max)(1.0f, (std::min)(requestedWidth, availableWidth));
        const float rowHeight = (std::max)(22.0f, options.row_height);
        const float closedHeight = options.size.y > 0.0f
            ? (std::max)(rowHeight, options.size.y)
            : rowHeight;
        const std::string key = widget_state_key(
            options.id.empty() ? std::string_view{ "__select_box" } : options.id);
        auto& state = g_selectBoxStates[key];
        state.owner = g_frame.ctx;
        state.visited_this_frame = true;
        if(!options.enabled){state.open=false;state.alignSelectedOnOpen=false;}
        // An already-open dropdown alone can receive the mouse. The owner
        // scope is restored on every return, including early exits.
        struct SelectPointerScope final {
            bool& slot;
            bool old;
            explicit SelectPointerScope(bool& target, bool owner) noexcept
                : slot(target), old(target) { slot = owner; }
            ~SelectPointerScope() { slot = old; }
        } pointerScope{g_frame.selectPopupOwnsInput, state.open && options.enabled};

        std::string selectedLabel = options.selected.empty()
            ? std::string(options.placeholder)
            : std::string(options.selected);
        selectedLabel += state.open ? "  ^" : "  v";

        bool toggledThisFrame = false;
        const bool pressed=options.enabled?button(selectedLabel,{width,closedHeight}):[&]{
            const std::array disabled{InlineButtonSpec{.label=selectedLabel,.width=width,.enabled=false}};
            (void)inline_button_row(disabled,closedHeight,0.0f);return false;
        }();
        if (pressed)
        {
            const bool nextOpen = !state.open;
            for (auto& [otherKey, otherState] : g_selectBoxStates)
            {
                (void)otherKey;
                if (otherState.owner != g_frame.ctx) continue;
                otherState.open = false;
                otherState.alignSelectedOnOpen = false;
            }
            state.open = nextOpen;
            state.alignSelectedOnOpen = nextOpen;
            if (nextOpen) {
                g_frame.selectPopupCapturedThisFrame = true;
                g_frame.selectPopupOwnsInput = true;
            }
            toggledThisFrame = true;
        }
        const Vec2 afterClosedCursor = g_frame.cursor;

        result.opened = state.open;
        if (!state.open || options.options.empty())
        {
            if (options.options.empty()) state.open = false;
            state.alignSelectedOnOpen = false;
            return result;
        }

        const std::size_t visibleCount = (std::max)(
            std::size_t{ 1 },
            (std::min)(options.options.size(), options.max_visible_options == 0
                ? options.options.size()
                : options.max_visible_options));
        const float optionPitch = rowHeight + 2.0f;
        const float requestedListHeight = static_cast<float>(visibleCount) * optionPitch - 2.0f;
        const float contentHeight = static_cast<float>(options.options.size()) * rowHeight
            + static_cast<float>(options.options.size() - 1u) * 2.0f;
        const std::string listId = key + "-list";
        gui_lib::Rect popupViewport{
            {0.0f, 0.0f},
            {static_cast<float>((std::max)(1, g_frame.ctx->get_width_safe())),
                static_cast<float>((std::max)(1, g_frame.ctx->get_height_safe()))}};
        if (g_frame.modalInput.mode() == gui_lib::input::ModalInputMode::bounded)
        {
            const auto modal = g_frame.modalInput.bounds();
            const float right = (std::min)(popupViewport.size.x, modal.position.x + modal.size.x);
            const float bottom = (std::min)(popupViewport.size.y, modal.position.y + modal.size.y);
            popupViewport.position = {(std::max)(0.0f, modal.position.x),
                (std::max)(0.0f, modal.position.y)};
            popupViewport.size = {(std::max)(1.0f, right - popupViewport.position.x),
                (std::max)(1.0f, bottom - popupViewport.position.y)};
        }
        // Use the portable library's placement for both pointer tests and the
        // deferred overlay. A visible option must stay inside modal input bounds.
        const auto placement = gui_lib::place_popup(gui_lib::PopupOptions{
            .owner = {{start.x - popupViewport.position.x, start.y - popupViewport.position.y},
                {width, closedHeight}},
            .preferred_size = {width, requestedListHeight},
            .viewport_size = popupViewport.size,
            .gap = (std::max)(0.0f, afterClosedCursor.y - start.y - closedHeight)},
            {});
        const Vec2 listPos{placement.position.x + popupViewport.position.x,
            placement.position.y + popupViewport.position.y};
        const float listWidth = placement.size.x;
        const float listHeight = placement.size.y;

        if (state.alignSelectedOnOpen)
        {
            auto& scrollState = g_scrollAreaStates[scroll_panel_key(listId)];
            const auto selectedIt = std::find(options.options.begin(), options.options.end(), options.selected);
            if (selectedIt != options.options.end())
            {
                const auto selectedIndex = static_cast<std::size_t>(std::distance(options.options.begin(), selectedIt));
                const float selectedY = static_cast<float>(selectedIndex) * optionPitch;
                const float maxScroll = (std::max)(0.0f, contentHeight - listHeight);
                scrollState.scrollY = (std::clamp)(selectedY - rowHeight, 0.0f, maxScroll);
            }
            state.alignSelectedOnOpen = false;
        }

        if (!toggledThisFrame && (left_press_available() || right_press_available()))
        {
            const bool pressedClosed = point_in_rect(g_frame.mousePos, start.x, start.y, width, closedHeight);
            const bool pressedList = point_in_rect(g_frame.mousePos, listPos.x, listPos.y, listWidth, listHeight);
            if (!pressedClosed && !pressedList)
            {
                state.open = false;
                result.opened = false;
                if (left_press_available())
                    consume_left_press();
                if (right_press_available())
                    consume_right_press();
                return result;
            }
        }

        auto& scrollState = g_scrollAreaStates[scroll_panel_key(listId)];
        scrollState.contentHeight = (std::max)(listHeight, contentHeight);
        const float maxScroll = (std::max)(0.0f, scrollState.contentHeight - listHeight);
        if (!std::isfinite(scrollState.scrollY))
            scrollState.scrollY = 0.0f;
        scrollState.scrollY = (std::clamp)(scrollState.scrollY, 0.0f, maxScroll);

        const bool listHovered = point_in_rect(g_frame.mousePos, listPos.x, listPos.y, listWidth, listHeight)
            && point_in_modal_input_capture(g_frame.mousePos);
        if (listHovered && g_frame.mouseWheelDelta != 0)
        {
            const float wheelSteps = static_cast<float>(g_frame.mouseWheelDelta) / 120.0f;
            const float step = line_advance_amount(kFontScale) * 3.0f;
            scrollState.scrollY = (std::clamp)(scrollState.scrollY - wheelSteps * step, 0.0f, maxScroll);
            g_frame.mouseWheelDelta = 0;
        }

        const bool showScrollbar = contentHeight > listHeight + 1.0f;
        const float scrollbarWidth = showScrollbar ? 10.0f : 0.0f;
        const float itemWidth = (std::max)(1.0f, listWidth - scrollbarWidth - 2.0f);
        for (std::size_t i = 0; i < options.options.size(); ++i)
        {
            const float optionY = listPos.y - scrollState.scrollY + static_cast<float>(i) * optionPitch;
            if (optionY + rowHeight < listPos.y || optionY > listPos.y + listHeight)
                continue;

            const Vec2 optionPos{ listPos.x, optionY };
            const bool hovered = point_in_rect(g_frame.mousePos, optionPos.x, optionPos.y, itemWidth, rowHeight)
                && point_in_rect(g_frame.mousePos, listPos.x, listPos.y, listWidth, listHeight)
                && point_in_modal_input_capture(g_frame.mousePos);
            const std::size_t pressKey = widget_press_key(options.options[i], optionPos, { itemWidth, rowHeight });
            auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
            if (hovered && left_press_available())
            {
                pressedKey = pressKey;
                consume_left_press();
            }

            const bool clicked = left_release_available() && hovered && pressedKey == pressKey;
            if (g_frame.justReleased && pressedKey == pressKey)
            {
                if (hovered)
                    consume_left_release();
                pressedKey = 0;
            }

            g_frame.lastButtonBounds = WidgetBounds{ .position = optionPos, .size = { itemWidth, rowHeight } };

            if (clicked)
            {
                result.changed = true;
                result.selected_index = i;
                state.open = false;
            }
        }
        if (point_in_rect(g_frame.mousePos, listPos.x, listPos.y, listWidth, listHeight))
        {
            if (left_press_available())
                consume_left_press();
            if (left_release_available())
                consume_left_release();
            if (right_press_available())
                consume_right_press();
            if (right_release_available())
                consume_right_release();
        }

        if (state.open)
        {
            PendingSelectPopup popup{};
            popup.scrollKey = scroll_panel_key(listId);
            popup.position = listPos;
            popup.width = listWidth;
            popup.height = listHeight;
            popup.rowHeight = rowHeight;
            popup.optionPitch = optionPitch;
            popup.contentHeight = contentHeight;
            popup.showScrollbar = showScrollbar;
            popup.selected = std::string(options.selected);
            popup.options.reserve(options.options.size());
            for (std::string_view option : options.options)
                popup.options.emplace_back(option);
            g_frame.pendingSelectPopups.push_back(std::move(popup));
        }

        set_cursor(afterClosedCursor);
        return result;
    }

    static void draw_progress_bar_layout(
        const gui_lib::ProgressBarLayout& layout,
        std::string_view label,
        std::string_view status,
        bool show_percent,
        bool activity,
        float activity_phase) noexcept
    {
        if (!g_frame.ctx)
            return;

        const auto& palette = active_palette();
        draw_sprite(palette.panelBackground,
            layout.track.position.x,
            layout.track.position.y,
            layout.track.size.x,
            layout.track.size.y);
        draw_sprite(palette.consoleBackground,
            layout.inner.position.x,
            layout.inner.position.y,
            layout.inner.size.x,
            layout.inner.size.y);

        if (layout.fill.size.x > 0.0f && layout.fill.size.y > 0.0f)
        {
            draw_sprite(palette.textFieldActive,
                layout.fill.position.x,
                layout.fill.position.y,
                layout.fill.size.x,
                layout.fill.size.y);
        }

        if (activity && layout.inner.size.x > 8.0f && layout.inner.size.y > 4.0f)
        {
            const float normalizedPhase = activity_phase - std::floor(activity_phase);
            const float stripeWidth = std::clamp(layout.inner.size.x * 0.18f, 24.0f, 96.0f);
            const float travel = layout.inner.size.x + stripeWidth;
            const float stripeX = layout.inner.position.x + normalizedPhase * travel - stripeWidth;
            ContentClipScope progressClip(
                { layout.inner.position.x, layout.inner.position.y },
                { layout.inner.position.x + layout.inner.size.x, layout.inner.position.y + layout.inner.size.y });
            draw_sprite(palette.buttonHover,
                stripeX,
                layout.inner.position.y,
                stripeWidth,
                layout.inner.size.y);
        }

        draw_sprite(palette.buttonHover,
            layout.track.position.x,
            layout.track.position.y,
            layout.track.size.x,
            1.0f);
        draw_sprite(palette.buttonHover,
            layout.track.position.x,
            layout.track.position.y + layout.track.size.y - 1.0f,
            layout.track.size.x,
            1.0f);
        draw_sprite(palette.buttonHover,
            layout.track.position.x,
            layout.track.position.y,
            1.0f,
            layout.track.size.y);
        draw_sprite(palette.buttonHover,
            layout.track.position.x + layout.track.size.x - 1.0f,
            layout.track.position.y,
            1.0f,
            layout.track.size.y);

        std::string labelText;
        if (!label.empty())
            labelText = std::string(label);
        if (!status.empty())
        {
            if (!labelText.empty())
                labelText += " - ";
            labelText += status;
        }
        if (show_percent)
        {
            if (!labelText.empty())
                labelText += " ";
            labelText += std::to_string(static_cast<int>(std::round(layout.fraction * 100.0f)));
            labelText += "%";
        }

        if (!labelText.empty())
        {
            const std::string fitted = fit_text_to_width(
                labelText,
                (std::max)(1.0f, layout.track.size.x - 2.0f * kContentPadding),
                kFontScale);
            const std::string_view displayLabel = fitted.empty()
                ? std::string_view{ labelText }
                : std::string_view{ fitted };
            const float textY = layout.track.position.y
                + std::floor((std::max)(0.0f, (layout.track.size.y - base_line_height(kFontScale)) * 0.5f))
                + 1.0f;
            draw_text_line(displayLabel, layout.track.position.x + kContentPadding, textY, kFontScale);
        }
    }

    void progress_bar(const ProgressBarOptions& options) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx)
            return;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        const float clipLeft = has_content_clip() ? g_frame.contentMin.x : g_frame.origin.x;
        const float clipRight = content_right();
        const float drawX = (std::min)((std::max)(pos.x, clipLeft), clipRight);
        const float availableWidth = (std::max)(1.0f, clipRight - drawX);
        const float requestedWidth = options.size.x > 0.0f
            ? (std::max)(1.0f, options.size.x)
            : availableWidth;
        const float width = (std::max)(1.0f, (std::min)(requestedWidth, availableWidth));
        const Vec2 drawPos{ drawX, pos.y };
        const float height = (std::max)(14.0f, options.size.y > 0.0f ? options.size.y : 18.0f);
        const auto layout = gui_lib::make_progress_bar_layout(gui_lib::ProgressBarLayoutOptions{
            .track = gui_lib::Rect{ to_lib(drawPos), { width, height } },
            .value = std::clamp(options.value, 0.0f, 1.0f),
            .minimum = 0.0f,
            .maximum = 1.0f,
            .padding = 2.0f,
            .direction = gui_lib::ProgressBarDirection::left_to_right
        });

        ContentClipScope localClip(
            { drawPos.x, drawPos.y },
            { drawPos.x + width, drawPos.y + height });
        draw_progress_bar_layout(
            layout,
            options.label,
            options.status,
            options.show_percent,
            options.activity,
            options.activity_phase);

        advance_cursor({ 0.0f, height + kContentPadding });
    }

    LoadingScreenResult loading_screen(const LoadingScreenOptions& options) noexcept
    {
        LoadingScreenResult result{};
        if (!g_frame.ctx)
            return result;

        ensure_resources();

        const Vec2 viewportPos = options.viewport_position;
        const Vec2 viewportSize = options.viewport_size.x > 0.0f && options.viewport_size.y > 0.0f
            ? options.viewport_size
            : (g_frame.insideWindow ? g_frame.windowSize : Vec2{ 1.0f, 1.0f });
        const float titleScale = options.title_scale > 0.0f ? options.title_scale : kTitleScale;
        const float messageScale = options.message_scale > 0.0f ? options.message_scale : kFontScale;
        const float statusScale = options.status_scale > 0.0f ? options.status_scale : kFontScale;
        const auto layout = gui_lib::make_loading_screen_layout(gui_lib::LoadingScreenLayoutOptions{
            .viewport = gui_lib::Rect{ to_lib(viewportPos), to_lib(viewportSize) },
            .preferred_panel_size = to_lib(options.panel_size),
            .minimum_panel_size = { 360.0f, 220.0f },
            .margin = 32.0f,
            .padding = 28.0f,
            .gap = 14.0f,
            .title_height = (std::max)(line_advance_amount(titleScale) + 4.0f, 30.0f),
            .message_height = (std::max)(76.0f, line_advance_amount(messageScale) * 3.0f + 10.0f),
            .progress_height = 24.0f,
            .status_height = line_advance_amount(statusScale) + 4.0f,
            .action_height = options.reserve_action_row ? 64.0f : 0.0f,
            .progress_padding = 2.0f,
            .progress_value = std::clamp(options.progress, 0.0f, 1.0f)
        });

        result.visible = layout.visible;
        result.panel = from_lib(layout.panel);
        result.progress = from_lib(layout.progress.track);
        result.action = from_lib(layout.action);
        if (!layout.visible)
            return result;

        if (options.capture_input)
            begin_modal_input_capture(viewportPos, viewportSize);

        const auto& palette = active_palette();
        ContentClipClearScope clearClip;

        if (options.dim_background)
        {
            draw_sprite(palette.modalScrim,
                viewportPos.x,
                viewportPos.y,
                viewportSize.x,
                viewportSize.y);
        }

        draw_sprite(palette.windowBackground,
            layout.panel.position.x,
            layout.panel.position.y,
            layout.panel.size.x,
            layout.panel.size.y);
        draw_sprite(palette.titleBar,
            layout.panel.position.x,
            layout.panel.position.y,
            layout.panel.size.x,
            4.0f);
        draw_sprite(palette.panelBackground,
            layout.panel.position.x,
            layout.panel.position.y + layout.panel.size.y - 2.0f,
            layout.panel.size.x,
            2.0f);
        draw_sprite(palette.panelBackground,
            layout.panel.position.x,
            layout.panel.position.y,
            2.0f,
            layout.panel.size.y);
        draw_sprite(palette.panelBackground,
            layout.panel.position.x + layout.panel.size.x - 2.0f,
            layout.panel.position.y,
            2.0f,
            layout.panel.size.y);

        if (!options.title.empty())
            draw_text_line(options.title, layout.title.position.x, layout.title.position.y, titleScale);

        if (!options.message.empty())
        {
            draw_wrapped_text(
                options.message,
                layout.message.position.x,
                layout.message.position.y,
                layout.message.size.x,
                messageScale);
        }

        draw_progress_bar_layout(
            layout.progress,
            options.progress_label,
            options.progress_status,
            options.show_percent,
            options.activity,
            options.activity_phase);

        if (!options.progress_status.empty())
            draw_text_line(options.progress_status, layout.status.position.x, layout.status.position.y, statusScale);

        return result;
    }

    void text_box(std::string_view text, Vec2 size) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        const float baseHeight = base_line_height(kFontScale);
        const float minWidth = space_advance(kFontScale) * 4.0f;

        const float availableWidth = (std::max)(minWidth, content_available_width(pos.x));
        float width = static_cast<float>(size.x);
        if (width <= 0.0f)
        {
            const float estimated = measure_text_width(text, kFontScale) + 2.0f * kBoxInnerPadding;
            width = (std::min)((std::max)(minWidth, estimated), availableWidth);
        }
        else
        {
            width = (std::min)((std::max)(width, minWidth), availableWidth);
        }

        const float contentWidth = (std::max)(1.0f, width - 2.0f * kBoxInnerPadding);

        float height = static_cast<float>(size.y);
        if (height <= 0.0f)
        {
            const float textHeight = measure_wrapped_text_height(text, contentWidth, kFontScale);
            height = textHeight + 2.0f * kBoxInnerPadding;
        }
        else
        {
            height = (std::max)(height, baseHeight + 2.0f * kBoxInnerPadding);
        }

        draw_sprite(active_palette().panelBackground, pos.x, pos.y, width, height);
        draw_wrapped_text(text, pos.x + kBoxInnerPadding, pos.y + kBoxInnerPadding, contentWidth, kFontScale);

        advance_cursor({ 0.0f, height + kContentPadding });
    }

    ScrollAreaResult begin_scroll_area(const ScrollAreaOptions& options) noexcept
    {
        ScrollAreaResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx)
            return result;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        if (!std::isfinite(pos.x) || !std::isfinite(pos.y))
            return result;

        const float availableWidth = (std::max)(1.0f, content_available_width(pos.x));
        const float requestedWidth = options.size.x > 0.0f
            ? (std::max)(48.0f, options.size.x)
            : availableWidth;
        const float width = (std::max)(1.0f, (std::min)(requestedWidth, availableWidth));
        // A scrolled dock can shrink below 48px when the Output splitter
        // moves; clipping must never extend over the adjacent dock.
        const float height = options.size.y > 0.0f
            ? (std::max)(1.0f, options.size.y)
            : 180.0f;
        if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0f || height <= 0.0f)
            return result;

        const std::string id = options.id.empty()
            ? std::to_string(reinterpret_cast<std::uintptr_t>(g_frame.ctx)) + ":scroll-area"
            : std::string(options.id);
        const std::string stateKey = scroll_panel_key(id);
        auto& state = g_scrollAreaStates[stateKey];

        float estimatedContentHeight = (std::max)(
            height,
            options.content_height > 0.0f ? options.content_height : state.contentHeight);
        if (!std::isfinite(estimatedContentHeight))
        {
            state.contentHeight = height;
            estimatedContentHeight = height;
        }
        const float maxScroll = (std::max)(0.0f, estimatedContentHeight - height);
        const float priorMaxScroll = (std::max)(0.0f, state.contentHeight - state.viewportHeight);
        const bool wasFollowing = state.followTail
            && (state.contentHeight <= state.viewportHeight
                || state.scrollY >= priorMaxScroll - 1.5f);
        if (wasFollowing)
            state.scrollY = maxScroll;
        state.viewportHeight = height;
        if (!std::isfinite(state.scrollY))
            state.scrollY = 0.0f;
        state.scrollY = (std::clamp)(state.scrollY, 0.0f, maxScroll);
        if (std::isfinite(options.requested_scroll_y)
            && options.requested_scroll_y >= 0.0f)
        {
            state.scrollY = (std::clamp)(
                options.requested_scroll_y, 0.0f, maxScroll);
        }

        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        const bool selectList = is_select_box_list_id(id);
        const bool selectBoxCapturesWheel = any_select_box_open(g_frame.ctx) && !selectList;
        if (options.capture_wheel && hovered
            && g_frame.mouseWheelDelta != 0 && !selectBoxCapturesWheel)
        {
            const float wheelSteps = static_cast<float>(g_frame.mouseWheelDelta) / 120.0f;
            const float step = line_advance_amount(kFontScale) * 3.0f;
            state.scrollY = (std::clamp)(state.scrollY - wheelSteps * step, 0.0f, maxScroll);
            g_frame.mouseWheelDelta = 0;
            result.wheel_scrolled = true;
            state.followTail = state.scrollY >= maxScroll - 1.5f;
        }

        const auto& palette = active_palette();
        if (options.draw_background)
            draw_sprite(palette.consoleBackground, pos.x, pos.y, width, height);

        const float scrollbarWidth = (options.show_scrollbar && estimatedContentHeight > height + 1.0f)
            ? 10.0f
            : 0.0f;
        const float contentWidth = (std::max)(1.0f, width - scrollbarWidth - 2.0f);

        if (scrollbarWidth > 0.0f)
        {
            const float trackX = pos.x + width - scrollbarWidth;
            const float trackY = pos.y;
            const auto scrollbar = gui_lib::make_scrollbar_layout({
                .track = {{trackX, trackY}, {scrollbarWidth, height}},
                .content_extent = estimatedContentHeight, .scroll_offset = state.scrollY });
            const float thumbHeight = scrollbar.thumb.size.y;
            const float thumbY = scrollbar.thumb.position.y;
            const bool trackHovered = point_in_rect(g_frame.mousePos, trackX, trackY, scrollbarWidth, height)
                && point_in_active_clip(g_frame.mousePos);
            const bool thumbHovered = point_in_rect(g_frame.mousePos, trackX, thumbY, scrollbarWidth, thumbHeight)
                && point_in_active_clip(g_frame.mousePos);

            if (left_press_available() && trackHovered)
            {
                state.draggingScrollbar = true;
                state.dragGrabOffset = gui_lib::scrollbar_grab_offset(scrollbar, g_frame.mousePos.y, thumbHovered);
                consume_left_press();
            }

            if (!g_frame.mouseDown)
                state.draggingScrollbar = false;

            if (state.draggingScrollbar)
            {
                state.scrollY = gui_lib::scrollbar_offset_from_pointer(scrollbar, g_frame.mousePos.y, state.dragGrabOffset);
                state.followTail = state.scrollY >= maxScroll - 1.5f;
            }
        }

        g_scrollAreaStack.push_back(ScrollAreaFrame{
            .key = stateKey,
            .ownerWindowKey = g_frame.windowKey,
            .ownerWindowDepth = g_windowFrameStack.size(),
            .previousCursor = g_frame.cursor,
            .previousMin = g_frame.contentMin,
            .previousMax = g_frame.contentMax,
            .viewportMin = pos,
            .viewportMax = { pos.x + width, pos.y + height },
            .viewportHeight = height,
            .viewportWidth = width,
            .scrollY = state.scrollY,
            .showScrollbar = options.show_scrollbar
        });

        g_frame.contentMin = {
            (std::max)(g_frame.contentMin.x, pos.x),
            (std::max)(g_frame.contentMin.y, pos.y)
        };
        g_frame.contentMax = {
            (std::min)(g_frame.contentMax.x, pos.x + contentWidth),
            (std::min)(g_frame.contentMax.y, pos.y + height)
        };
        g_frame.cursor = { pos.x, pos.y - state.scrollY };

        result.scroll_y = state.scrollY;
        result.content_height = estimatedContentHeight;
        return result;
    }

    void end_scroll_area() noexcept
    {
        if (!g_frame.insideWindow || g_scrollAreaStack.empty())
            return;

        if (g_scrollAreaStack.back().ownerWindowDepth
                != g_windowFrameStack.size()
            || g_scrollAreaStack.back().ownerWindowKey
                != g_frame.windowKey)
        {
            return;
        }

        const ScrollAreaFrame frame = g_scrollAreaStack.back();
        g_scrollAreaStack.pop_back();

        auto& state = g_scrollAreaStates[frame.key];
        const float drawnContentHeight = (std::max)(
            frame.viewportHeight,
            (g_frame.cursor.y + frame.scrollY) - frame.viewportMin.y);
        const float previousMax = (std::max)(0.0f, state.contentHeight - frame.viewportHeight);
        const bool stayAtBottom = state.followTail && state.scrollY >= previousMax - 1.5f;
        state.contentHeight = std::isfinite(drawnContentHeight)
            ? drawnContentHeight
            : frame.viewportHeight;
        const float maxScroll = (std::max)(0.0f, state.contentHeight - frame.viewportHeight);
        if (!std::isfinite(state.scrollY))
            state.scrollY = 0.0f;
        state.scrollY = stayAtBottom ? maxScroll
            : (std::clamp)(state.scrollY, 0.0f, maxScroll);

        g_frame.contentMin = frame.previousMin;
        g_frame.contentMax = frame.previousMax;
        g_frame.cursor = {
            frame.previousCursor.x,
            frame.viewportMax.y + kContentPadding
        };

        if (frame.showScrollbar && state.contentHeight > frame.viewportHeight + 1.0f)
        {
            const auto& palette = active_palette();
            const float scrollbarWidth = 10.0f;
            const float trackX = frame.viewportMax.x - scrollbarWidth;
            const float trackY = frame.viewportMin.y;
            ContentClipScope scrollbarClip{
                frame.viewportMin,
                frame.viewportMax };
            draw_sprite(palette.textField, trackX, trackY, scrollbarWidth, frame.viewportHeight);

            const auto scrollbar = gui_lib::make_scrollbar_layout({
                .track = {{trackX, trackY}, {scrollbarWidth, frame.viewportHeight}},
                .content_extent = state.contentHeight, .scroll_offset = state.scrollY });
            const float thumbHeight = scrollbar.thumb.size.y;
            const float thumbY = scrollbar.thumb.position.y;
            draw_sprite(palette.buttonActive, trackX, thumbY, scrollbarWidth, thumbHeight);
        }
    }

    ScrollTextPanelResult scroll_text_panel(const ScrollTextPanelOptions& options) noexcept
    {
        ScrollTextPanelResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx)
            return result;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        const float availableWidth = (std::max)(1.0f, content_available_width(pos.x));
        const float requestedWidth = options.size.x > 0.0f
            ? (std::max)(64.0f, options.size.x)
            : availableWidth;
        const float width = (std::max)(1.0f, (std::min)(requestedWidth, availableWidth));
        const float height = options.size.y > 0.0f
            ? (std::max)(48.0f, options.size.y)
            : 160.0f;
        const auto& palette = active_palette();

        draw_sprite(palette.consoleBackground, pos.x, pos.y, width, height);

        const float scrollbarWidth = options.lines.empty() ? 0.0f : 10.0f;
        // Chat content has breathing room even inside a narrow dock. The
        // scrollbar remains outside the conversation column.
        const float chatGutter = options.conversation_style ? 12.0f : 0.0f;
        const float contentX = pos.x + kBoxInnerPadding + chatGutter;
        const float contentY = pos.y + kBoxInnerPadding;
        const float contentWidth = (std::max)(1.0f,
            width - 2.0f * (kBoxInnerPadding + chatGutter) - scrollbarWidth);
        const float contentHeight = (std::max)(1.0f, height - 2.0f * kBoxInnerPadding);
        const float linePitch = line_advance_amount(kFontScale);
        const float rowGap = options.conversation_style
            ? gui_lib::chat_surface::message_gap
            : options.message_bubbles ? 8.0f : 2.0f;
        const float edgePadding = kGlyphRasterPadding;
        const float messageInsetX = options.conversation_style
            ? gui_lib::chat_surface::message_inset_x
            : options.message_bubbles ? 12.0f : 6.0f;
        const float messageInsetY = options.conversation_style
            ? gui_lib::chat_surface::message_inset_y
            : options.message_bubbles ? 8.0f : 3.0f;
        const std::size_t lineCount = options.lines.size();

        const auto line_role = [&](std::size_t lineIndex) noexcept
        {
            // Native reasoning/tool streams are disclosures authored by the
            // responding model; host System 1 events retain engine styling.
            if (options.conversation_style
                && lineIndex < options.line_run_details.size())
            {
                const auto& info = options.line_run_details[lineIndex];
                if (info.stream_block && info.route == "Tool call")
                    return TextMessageRole::engine;
                if (info.stream_block && !info.model.empty())
                    return TextMessageRole::assistant;
            }
            return lineIndex < options.line_roles.size()
                ? options.line_roles[lineIndex]
                : TextMessageRole::neutral;
        };
        const auto role_is_styled = [&](TextMessageRole role) noexcept
        {
            if (!options.conversation_style)
                return role != TextMessageRole::neutral;
            // Ordinary agent activity is text, not a wall of orange badges.
            // Preserve emphasis for actionable warnings and failures.
            return role == TextMessageRole::user
                || role == TextMessageRole::assistant
                || role == TextMessageRole::engine
                || role == TextMessageRole::warning
                || role == TextMessageRole::error
                || role == TextMessageRole::success;
        };
        const auto run_info = [&](std::size_t lineIndex) noexcept
            -> const ChatMessageRunInfo*
        {
            if (!options.conversation_style
                || lineIndex >= options.line_run_details.size()
                || !options.line_run_details[lineIndex].present())
                return nullptr;
            return &options.line_run_details[lineIndex];
        };
        const auto image_info = [&](std::size_t lineIndex) noexcept
            -> const ChatMessageRunInfo*
        {
            return options.conversation_style && lineIndex < options.line_run_details.size()
                    && !options.line_run_details[lineIndex].images.empty()
                ? &options.line_run_details[lineIndex] : nullptr;
        };
        const auto reference_height = [&](std::size_t lineIndex) noexcept
            { const auto* info = image_info(lineIndex);
              return info ? 78.0f * (std::min)(info->images.size(), std::size_t{4u}) : 0.0f; };
        const auto run_key = [&](std::size_t lineIndex) noexcept -> std::size_t
        {
            const auto* info = run_info(lineIndex);
            if (!info) return 0u;
            if (info->run_id != 0u)
                return std::hash<std::uint64_t>{}(info->run_id);
            const auto& original = options.lines[lineIndex];
            const std::hash<std::string_view> h{};
            // A duration and model name are LIVE telemetry and can change
            // every frame. They are not part of disclosure identity.
            return h(std::string_view{original})
                ^ (h(std::string_view{info->route}) << 1u);
        };
        const auto detail_text = [&](const ChatMessageRunInfo& info)
        {
            if (!info.files.empty())
            {
                std::string contents;
                for (const auto& file : info.files)
                {
                    if (!contents.empty()) contents += "\n\n";
                    contents += file.name + "\n" + file.content;
                }
                return contents;
            }
            if (info.stream_block)
                return info.evidence.empty() ? std::string{"Waiting for model output..."}
                    : info.evidence;
            std::string details = std::string{"Model       "} + info.model
                + "\nRoute        " + info.route
                + "\nArchitect  " + info.architect
                + "\nDuration   " + info.duration
                + "\nStatus       " + info.status;
            if (!info.evidence.empty())
                details += "\n" + info.evidence;
            return details;
        };
        // The disclosure label and its arrow share ONE text run. Separate
        // right-aligned arrow drawing can drift to the far edge of AI Chat.
        const auto run_header_label = [&](std::size_t lineIndex) -> std::string
        {
            const auto* info = run_info(lineIndex);
            std::string header{};
            if (info && !info->files.empty())
            {
                const auto& file = info->files.front();
                header = file.name + " · " + std::to_string(file.content.size()) + " B";
                if (info->files.size() > 1u)
                    header += " · +" + std::to_string(info->files.size() - 1u) + " files";
            }
            else if (info && info->stream_block)
            {
                if (info->route == "Activity")
                {
                    // Compact Codex-style disclosure: one readable
                    // stage; tool calls are children of the same disclosure.
                    // The parent names the activity group; model speed
                    // belongs to its nested inference-completed detail.
                    header = "Activity";
                    if (info->stream_live)
                        header += " · Working";
                    else if (info->status == "Failed")
                        header += " · Failed";
                    else if (info->status == "Stopped"
                        || info->status == "Cancelled")
                        header += " · Stopped";
                    // Overall duration belongs in request details;
                    // the Activity parent labels the nested timeline.
                }
                else if (info->route == "Tool call")
                    header = "Tool · " + (info->model.empty() ? "unknown" : info->model)
                        + " · " + (info->status.empty() ? "reviewed" : info->status);
                else
                    header = "Model activity · " + info->route
                        + (info->stream_live ? " · live" : " · finished");
            }
            else if (info)
            {
                const std::string runState = info->status == "Cancelled"
                    ? " · stopped after " : " · ran for ";
                header = (info->model.empty() ? "Model" : info->model)
                    + runState
                    + (info->duration.empty() ? "time unavailable" : info->duration)
                    // Transport progress presently reports bytes/events, not
                    // tokens; never divide bytes by duration and call it t/s.
                    + " · " + (info->tps.empty()
                        ? std::string{"t/s unavailable"} : info->tps)
                    ;
                // Keep model-run titles free of leading glyphs too.
            }
            return header;
        };
        const auto inline_disclosure_text = [&](std::string_view label,
            float availableWidth, bool expanded) -> std::string
        {
            const std::string_view suffix = expanded ? "  v" : "  >";
            const float labelWidth = (std::max)(1.0f,
                availableWidth - measure_text_width(suffix, kFontScale) - 4.0f);
            return fit_text_to_width(label, labelWidth, kFontScale)
                + std::string{suffix};
        };
        const auto run_header_height = [&](std::size_t lineIndex) noexcept
        {
            if (run_info(lineIndex))
                return gui_lib::chat_surface::run_header_height;
            // Normal replies are read directly, without repeating "Epoch
            // Assistant" before every paragraph. Only actual model runs and
            // expandable tool/activity traces have a header.
            return 0.0f;
        };

        const auto line_is_bubble = [&](std::size_t lineIndex) noexcept
        {
            if (!options.message_bubbles || !role_is_styled(line_role(lineIndex)))
                return false;
            if (!options.conversation_style) return true;
            // Only the actual conversation receives chat boxes. Host activity
            // and diagnostics remain quiet disclosures, never fake replies.
            const auto role = line_role(lineIndex);
            // Codex/ChatGPT-style assistant output is a clear text column;
            // only the operator's messages use filled bubbles. Model trace
            // disclosures have a separate compact treatment.
            return role == TextMessageRole::user;
        };
        const auto line_max_box_width = [&](std::size_t lineIndex) noexcept
        {
            return line_role(lineIndex) == TextMessageRole::user
                ? contentWidth * 0.88f : contentWidth;
        };
        const auto line_text_width = [&](std::size_t lineIndex) noexcept
        {
            const float inset = role_is_styled(line_role(lineIndex))
                ? messageInsetX : 0.0f;
            return (std::max)(1.0f, line_max_box_width(lineIndex) - inset * 2.0f);
        };

        const auto line_view = [&](std::size_t lineIndex) noexcept -> std::string_view
        {
            if (const auto* info = run_info(lineIndex); info && info->stream_block)
                return {}; // Header is rendered once above the collapsible content.
            std::string_view line{ options.lines[lineIndex] };
            if (options.max_line_chars > 0 && line.size() > options.max_line_chars)
                line = gui_lib::utf8_text_layout::prefix(line, options.max_line_chars);
            if (options.conversation_style)
            {
                const TextMessageRole role = line_role(lineIndex);
                if (role == TextMessageRole::user && line.starts_with("User: "))
                    line.remove_prefix(6u);
                else if (role == TextMessageRole::assistant && line.starts_with("Assistant: "))
                    line.remove_prefix(11u);
                else if (line.starts_with("Agent: "))
                    line.remove_prefix(7u);
            }
            return line;
        };

        const auto bubble_box_width = [&](std::size_t lineIndex) noexcept
        {
            const float maximum = line_max_box_width(lineIndex);
            if (!line_is_bubble(lineIndex)
                || line_role(lineIndex) != TextMessageRole::user) return maximum;
            const auto line = line_view(lineIndex);
            const auto* info = run_info(lineIndex);
            if (image_info(lineIndex) || (info && !info->files.empty())) return maximum;
            if (line.find('\n') != std::string_view::npos || (info && info->stream_block))
                return maximum;
            const float headerWidth = 0.0f;
            // Message actions live underneath the bubble, not inside it.
            const float minimumWidth = 54.0f;
            return (std::min)(maximum, (std::max)({minimumWidth,
                measure_text_width(line, kFontScale) + messageInsetX * 2.0f,
                headerWidth}));
        };
        const auto bubble_box_x = [&](std::size_t lineIndex) noexcept
        {
            const float width = bubble_box_width(lineIndex);
            return line_is_bubble(lineIndex) && line_role(lineIndex) == TextMessageRole::user
                ? contentX + contentWidth - width : contentX;
        };

        const std::string id = options.id.empty()
            ? std::to_string(reinterpret_cast<std::uintptr_t>(options.lines.data()))
            : std::string(options.id);
        auto& state = g_scrollTextStates[scroll_panel_key(id)];
        if (state.expandedRuns.size() > 1024u)
            state.expandedRuns.clear(); // bounded state after conversation pruning
        if (state.expandedActivityItems.size() > 4096u)
            state.expandedActivityItems.clear(); // no state inheritance after pruning
        // Streaming state must never reset operator disclosure choices.
        // Activity and token-section expansion is explicit and stable across
        // async event snapshots, completion and the next render frame.
        const auto run_expanded = [&](std::size_t lineIndex) noexcept
        {
            const auto it = state.expandedRuns.find(run_key(lineIndex));
            // The default follows the persistent Details preference, NOT
            // info->stream_live. Live model events must not cause the arrow
            // to alternate between v/> without operator input.
            const auto* info = run_info(lineIndex);
            return it != state.expandedRuns.end() ? it->second
                : (info && info->stream_block && options.expand_activity);
        };
        // A fixed child row within the outer per-turn Worked disclosure.
        // All controls use this geometry for layout, drawing and hit testing.
        const float activityItemHeaderHeight = linePitch + 8.0f;
        const float activityItemPadding = 8.0f;
        constexpr float activityIndent = 16.0f;
        const auto activity_detail = [](const ChatActivityEvent& item) -> std::string_view
        {
            // A host message's detail must not repeat the group in both
            // the disclosure label and the expanded child content.
            std::string_view detail{item.detail};
            const std::string prefix = item.group + ": ";
            if (detail.starts_with(prefix)) detail.remove_prefix(prefix.size());
            // Status-only events have no hidden extra detail. Other events
            // frequently repeat their summary before the useful explanation;
            // keep only the information that is not already in the title.
            const std::string_view summary{item.summary};
            // Preserve an existing event's disclosure even when its recorded
            // detail equals the summary: the earlier suppression removed
            // child arrows and their click targets from the Activity trail.
            // The duplicate is shown only on explicit expansion.
            if (detail == summary) return detail;
            if (detail.starts_with(summary) && detail.size() > summary.size())
            {
                const auto remainder = detail.substr(summary.size());
                if (remainder.starts_with(": ") || remainder.starts_with(". "))
                    detail = remainder.substr(2u);
                else if (remainder.starts_with(' '))
                    detail = remainder.substr(1u);
            }
            return detail;
        };
        const auto activity_item_key = [&](std::size_t index, std::size_t ordinal)
            -> std::size_t
        {
            auto key = run_key(index);
            // Ordinals are append-only for each immutable run ID; never use
            // pointer addresses or the current vector index for identity.
            key ^= (ordinal + 1u) + std::size_t{0x9e3779b9u}
                + (key << 6u) + (key >> 2u);
            return key;
        };
        const auto activity_item_expanded = [&](std::size_t index,
            std::size_t ordinal) noexcept
        {
            const auto it = state.expandedActivityItems.find(
                activity_item_key(index, ordinal));
            if (it != state.expandedActivityItems.end()) return it->second;
            const auto* info = run_info(index);
            if (!info || ordinal >= info->activity_events.size()
                || activity_detail(info->activity_events[ordinal]).empty())
                return false;
            // Details reveals token text immediately and stays stable across
            // streaming/completion; explicit per-item choices take precedence.
            const auto& item = info->activity_events[ordinal];
            return options.expand_activity && item.group == "Model"
                && (item.summary == "Reasoning tokens"
                    || item.summary == "Response tokens"
                    || item.summary == "Tool arguments");
        };
        const auto activity_item_height = [&](std::size_t index,
            std::size_t ordinal, float detailsWidth) -> float
        {
            const auto* info = run_info(index);
            if (!info || ordinal >= info->activity_events.size()) return 0.0f;
            float height = activityItemHeaderHeight;
            if (activity_item_expanded(index, ordinal))
                height += measure_wrapped_text_height(
                    activity_detail(info->activity_events[ordinal]),
                    (std::max)(1.0f, detailsWidth - 2.0f * activityItemPadding
                        - 2.0f * activityIndent),
                    kFontScale) + 2.0f * activityItemPadding;
            return height;
        };
        if (options.incremental_line_measurements)
        {
            state.measurements.resize(lineCount);
            state.detailMeasurements.resize(lineCount);
        }
        else
        {
            state.measurements.clear();
            state.detailMeasurements.clear();
        }

        // The same measured detail height drives layout AND the draw pass.
        // Previously every open/streaming reasoning disclosure was wrapped and
        // measured twice per frame, including on idle frames.
        std::vector<float> detailHeights(lineCount, 0.0f);
        std::vector<float> bodyHeights(lineCount, 0.0f);
        std::vector<float> lineHeights{};
        lineHeights.reserve(lineCount);
        float totalTextHeight = edgePadding * 2.0f;
        const auto* fontAsset = active_font_cache().asset;
        for (std::size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex)
        {
            const std::string_view line = line_view(lineIndex);
            const TextMessageRole role = line_role(lineIndex);
            const float textWidth = line_text_width(lineIndex);
            float textHeight = linePitch;
            if (options.wrap_lines && !line.empty())
            {
                if (options.incremental_line_measurements)
                {
                    auto& cached = state.measurements[lineIndex];
                    // Raw-output blocks are append-only. A changed allocation,
                    // byte count, text width, font, or message role invalidates
                    // that one block's measured layout.
                    if (cached.data == line.data()
                        && cached.bytes == line.size()
                        && cached.width == textWidth
                        && cached.font == fontAsset
                        && cached.role == role)
                        textHeight = cached.height;
                    else
                    {
                        textHeight = measure_wrapped_text_height(
                            line, textWidth, kFontScale);
                        cached = {line.data(), line.size(), textWidth,
                            textHeight, role, fontAsset};
                    }
                }
                else
                    textHeight = measure_wrapped_text_height(
                        line, textWidth, kFontScale);
            }
            const bool stream = run_info(lineIndex) && run_info(lineIndex)->stream_block;
            const bool attachmentsOnly = line.empty() &&
                (image_info(lineIndex) || (run_info(lineIndex) && !run_info(lineIndex)->files.empty()));
            bodyHeights[lineIndex] = stream ? 0.0f
                : (attachmentsOnly ? 0.0f : (std::max)(linePitch, textHeight)) + reference_height(lineIndex);
            const float verticalInset = stream ? 4.0f
                : role_is_styled(role) ? messageInsetY : 0.0f;
            const bool messageActions = options.conversation_style && !stream
                && (role == TextMessageRole::user || role == TextMessageRole::assistant);
            float rowHeight = bodyHeights[lineIndex]
                + (stream ? 7.0f : rowGap) + verticalInset * 2.0f
                + run_header_height(lineIndex)
                + (messageActions ? 23.0f : 0.0f);
            if (const auto* info = run_info(lineIndex); info && run_expanded(lineIndex))
            {
                const std::string details = info->route == "Activity"
                    && !info->activity_events.empty()
                    ? std::string{} : detail_text(*info);
                const float width = (std::max)(1.0f,
                    line_max_box_width(lineIndex) - 2.0f * messageInsetX);
                if (info->route == "Activity" && !info->activity_events.empty())
                {
                    float measured = 0.0f;
                    for (std::size_t ordinal = 0u;
                        ordinal < info->activity_events.size(); ++ordinal)
                        measured += activity_item_height(lineIndex, ordinal, width);
                    detailHeights[lineIndex] = measured
                        + 2.0f * gui_lib::chat_surface::run_details_padding;
                    rowHeight += detailHeights[lineIndex];
                    lineHeights.push_back(rowHeight);
                    totalTextHeight += rowHeight;
                    continue;
                }
                float measured{};
                if (options.incremental_line_measurements)
                {
                    auto& cached = state.detailMeasurements[lineIndex];
                    const auto digest = std::hash<std::string_view>{}(details);
                    const auto key = run_key(lineIndex);
                    if (cached.run == key && cached.content_hash == digest
                        && cached.width == width && cached.font == fontAsset)
                        measured = cached.height;
                    else
                    {
                        measured = measure_wrapped_text_height(details, width, kFontScale);
                        cached = {key, digest, width, measured, fontAsset};
                    }
                }
                else
                    measured = measure_wrapped_text_height(details, width, kFontScale);
                detailHeights[lineIndex] = measured
                    + 2.0f * gui_lib::chat_surface::run_details_padding;
                rowHeight += detailHeights[lineIndex];
            }
            lineHeights.push_back(rowHeight);
            totalTextHeight += rowHeight;
        }

        const float maxScrollY = (std::max)(0.0f, totalTextHeight - contentHeight);
        const float previousMaxScrollY = (std::max)(0.0f, state.lastContentPixelHeight - state.lastViewportHeight);
        const bool wasAtBottom = state.followTail
            && (state.lastContentPixelHeight == 0.0f
                || state.scrollY >= previousMaxScrollY - 1.5f);
        if (options.scroll_to_end_generation != 0u
            && options.scroll_to_end_generation != state.scrollToEndGeneration)
        {
            state.scrollY = maxScrollY;
            state.followTail = true;
            state.scrollToEndGeneration = options.scroll_to_end_generation;
        }
        else if (wasAtBottom && (totalTextHeight != state.lastContentPixelHeight
            || contentHeight != state.lastViewportHeight))
            state.scrollY = maxScrollY;
        else
            state.scrollY = (std::clamp)(state.scrollY, 0.0f, maxScrollY);
        state.lastLineCount = lineCount;
        state.lastContentPixelHeight = totalTextHeight;
        state.lastViewportHeight = contentHeight;

        // Reconcile streaming scroll first, then hit-test the single jump control before message
        // selection pass. Otherwise an underlying disclosure can consume its
        // press, making the visible button look clickable but do nothing.
        // The EpochGUI circular primitive uses a matching round hitbox.
        if ((options.conversation_style || options.show_scroll_to_end) && !state.contextMenuOpen
            && maxScrollY > 1.5f && state.scrollY < maxScrollY - 1.5f
            && contentWidth >= 120.0f && contentHeight >= 65.0f)
        {
            constexpr float latestDiameter = 34.0f;
            const Vec2 latestPos{
                contentX + (contentWidth - latestDiameter) * 0.5f,
                contentY + contentHeight - latestDiameter - 9.0f};
            const Vec2 savedCursor = g_frame.cursor;
            begin_top_layer();
            set_cursor(latestPos);
            if (button_with_state("↓", {latestDiameter, latestDiameter},
                    false, true, SemanticTone::neutral, true))
            {
                state.scrollY = maxScrollY;
                state.followTail = true;
            }
            end_top_layer();
            set_cursor(savedCursor);
        }
        const auto selected_line_range = [&]() noexcept -> std::pair<std::size_t, std::size_t>
        {
            if (!state.hasSelection || lineCount == 0u)
                return { 0u, 0u };

            const std::size_t lastLine = lineCount - 1u;
            const std::size_t first = (std::min)(state.selectionAnchorLine, state.selectedLine);
            const std::size_t last = (std::max)(state.selectionAnchorLine, state.selectedLine);
            return {
                (std::min)(first, lastLine),
                (std::min)(last, lastLine)
            };
        };

        // Clipboard follows the visible hierarchical disclosure, including
        // each tool's reviewed identity and result rather than the old flat
        // host-evidence blob or just the opaque "Agent: Activity" label.
        const auto export_line = [&](std::size_t index) -> std::string
        {
            if (const auto* info = run_info(index); info && info->stream_block)
            {
                std::string out = info->route;
                if (info->route == "Activity" && !info->activity_events.empty())
                {
                    for (const auto& item : info->activity_events)
                    {
                        out += "\n  " + item.summary;
                        if (item.tool && !item.status.empty())
                            out += " · " + item.status;
                        if (const auto detail = activity_detail(item); !detail.empty())
                            out += "\n      " + std::string{detail};
                    }
                }
                else out += "\n" + info->evidence;
                return out;
            }
            std::string exported{line_view(index)};
            if (const auto* info = run_info(index); info && !info->files.empty())
            {
                for (const auto& file : info->files)
                    exported += "\n\n" + file.name + "\n" + file.content;
            }
            return exported;
        };
        const auto selected_lines_text = [&]() -> std::string
        {
            if (!state.hasSelection || lineCount == 0u)
                return {};

            const auto [first, last] = selected_line_range();
            std::string text{};
            for (std::size_t lineIndex = first; lineIndex <= last && lineIndex < lineCount; ++lineIndex)
            {
                if (!text.empty())
                    text.push_back('\n');
                text += export_line(lineIndex);
            }
            return text;
        };

        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        if (options.selectable && hovered)
        {
            for (const auto& event : g_frame.events)
            {
                if (event.type != EventType::KeyDown || !event.ctrl_down) continue;
                if (event.key == 'A' || event.key == 'a')
                {
                    state.selectionAnchorLine = 0u;
                    state.selectedLine = lineCount == 0u ? 0u : lineCount - 1u;
                    state.hasSelection = lineCount != 0u;
                }
                else if (event.key == 'C' || event.key == 'c')
                    (void)clipboard_write_text(selected_lines_text());
            }
        }
        if (hovered && g_frame.mouseWheelDelta != 0)
        {
            const int wheelSteps = (std::max)(1, std::abs(g_frame.mouseWheelDelta) / 120);
            if (g_frame.mouseWheelDelta > 0)
                state.scrollY = (std::max)(0.0f, state.scrollY - static_cast<float>(wheelSteps) * linePitch * 3.0f);
            else
                state.scrollY = (std::min)(maxScrollY, state.scrollY + static_cast<float>(wheelSteps) * linePitch * 3.0f);

            g_frame.mouseWheelDelta = 0;
            result.wheel_scrolled = true;
            result.user_scrolled = true;
            state.followTail = state.scrollY >= maxScrollY - 1.5f;
        }

        const bool canScroll = maxScrollY > 0.5f;
        if (scrollbarWidth > 0.0f && canScroll)
        {
            const float trackX = pos.x + width - kBoxInnerPadding - scrollbarWidth;
            const float trackY = contentY;
            const auto scrollbar = gui_lib::make_scrollbar_layout({
                .track = {{trackX, trackY}, {scrollbarWidth, contentHeight}},
                .content_extent = totalTextHeight, .scroll_offset = state.scrollY });
            const float thumbHeight = scrollbar.thumb.size.y;
            const float thumbY = scrollbar.thumb.position.y;
            const bool trackHovered = point_in_rect(g_frame.mousePos, trackX, trackY, scrollbarWidth, contentHeight)
                && point_in_active_clip(g_frame.mousePos);
            const bool thumbHovered = point_in_rect(g_frame.mousePos, trackX, thumbY, scrollbarWidth, thumbHeight)
                && point_in_active_clip(g_frame.mousePos);

            if (left_press_available() && trackHovered)
            {
                state.draggingScrollbar = true;
                state.dragGrabOffset = gui_lib::scrollbar_grab_offset(scrollbar, g_frame.mousePos.y, thumbHovered);
                consume_left_press();
            }

            if (!g_frame.mouseDown)
                state.draggingScrollbar = false;

            if (state.draggingScrollbar)
            {
                state.scrollY = gui_lib::scrollbar_offset_from_pointer(scrollbar, g_frame.mousePos.y, state.dragGrabOffset);
                result.first_visible_line = state.firstLine;
                result.user_scrolled = true;
                state.followTail = state.scrollY >= maxScrollY - 1.5f;
            }
        }

        result.first_visible_line = (std::min)(state.firstLine, lineCount);
        bool openedContextMenuThisFrame = false;
        // Popup dimensions follow the actual GUI button font metrics. The old
        // fixed 28px rows let buttons extend beyond the popup and viewport.
        const float contextMenuPadding = 5.0f;
        const float contextMenuGap = kContentPadding;
        const float contextButtonHeight = (std::max)(28.0f,
            base_line_height(kFontScale) + 2.0f * kBoxInnerPadding);
        // Evaluate against the current context line, not the line that was
        // selected when the menu was last drawn. The geometry and Retry action
        // must share this predicate and its scope.
        const auto context_line_is_retryable = [&]() noexcept -> bool
        {
            return state.contextLineIndex < options.retryable_lines.size()
                && options.retryable_lines[state.contextLineIndex] != 0u;
        };
        const auto context_menu_geometry = [&]() noexcept
        {
            const float desiredWidth = (std::max)(188.0f,
                measure_text_width("Copy Selection", kFontScale)
                    + 2.0f * kContentPadding + 2.0f * contextMenuPadding);
            return gui_lib::chat_context_popup::place(
                {contentX, contentY, contentWidth, contentHeight},
                state.contextMenuPos.x, state.contextMenuPos.y,
                desiredWidth, contextButtonHeight, context_line_is_retryable() ? 5u : 4u,
                contextMenuPadding, contextMenuGap);
        };
        // Own input while the popup is open, BEFORE scanning chat rows. A
        // click over a popup button must never also select/edit the text behind it.
        const bool popupOwnedInput = state.contextMenuOpen;
        {
            ContentClipScope clip{
                { contentX, contentY },
                { contentX + contentWidth, contentY + contentHeight }
            };

            // Defer disclosure toggles until the frame ends so the measured
            // row geometry and the draw pass always describe the same state.
            std::optional<std::pair<std::size_t, bool>> activityToggle{};
            bool foundFirstVisible = false;
            float rowY = contentY + edgePadding - state.scrollY;
            for (std::size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex)
            {
                const float rowHeight = lineHeights[lineIndex];
                const float rowBottom = rowY + rowHeight;

                if (rowBottom < contentY)
                {
                    rowY += rowHeight;
                    continue;
                }

                if (rowY > contentY + contentHeight)
                    break;

                if (!foundFirstVisible)
                {
                    state.firstLine = lineIndex;
                    result.first_visible_line = lineIndex;
                    foundFirstVisible = true;
                }

                // Compute the message geometry once for hit-testing AND rendering.
                // Previously these were declared after the nested-activity
                // hit-test, which broke the Windows MSVC Release build.
                const float boxWidth = bubble_box_width(lineIndex);
                const float boxX = bubble_box_x(lineIndex);
                const bool lineHovered = hovered && !popupOwnedInput
                    && point_in_rect(g_frame.mousePos, contentX, rowY, contentWidth, rowHeight);
                const TextMessageRole actionRole = line_role(lineIndex);
                const bool isStream = run_info(lineIndex) && run_info(lineIndex)->stream_block;
                const bool messageActions = options.conversation_style && !isStream
                    && (actionRole == TextMessageRole::user || actionRole == TextMessageRole::assistant);
                // Compact, icon-only actions below the message. No permanent
                // 'Copy'/'Edit' prose and no action-space inside the bubble.
                const float actionsY = rowBottom - rowGap - 23.0f;
                const float actionsRight = bubble_box_x(lineIndex)
                    + bubble_box_width(lineIndex) - 4.0f;
                const float copyX = actionsRight - 26.0f;
                const float editX = copyX - 30.0f;
                const bool copyHovered = messageActions && lineHovered
                    && point_in_rect(g_frame.mousePos, copyX, actionsY, 26.0f, 23.0f);
                const bool editHovered = messageActions && actionRole == TextMessageRole::user
                    && lineHovered && point_in_rect(g_frame.mousePos,
                        editX, actionsY, 26.0f, 23.0f);
                if (copyHovered && left_press_available())
                {
                    result.copy_line_index = lineIndex;
                    consume_left_press();
                }
                if (editHovered && left_press_available())
                {
                    result.edit_line_index = lineIndex;
                    consume_left_press();
                }
                const float runHeaderY = rowY + (isStream ? 4.0f
                    : (role_is_styled(line_role(lineIndex)) ? messageInsetY : 0.0f))
                    + (isStream ? 0.0f : bodyHeights[lineIndex]);
                // Click only the drawn Activity disclosure, not empty space
                // across the full chat pane. This matches the same text used
                // by the draw pass and keeps the hitbox away from adjacent UI.
                const float runHeaderWidth = run_info(lineIndex)
                    ? (std::min)(boxWidth - 2.0f * messageInsetX,
                        measure_text_width(inline_disclosure_text(
                            run_header_label(lineIndex),
                            (std::max)(1.0f, boxWidth - 2.0f * messageInsetX),
                            run_expanded(lineIndex)), kFontScale) + 10.0f)
                    : 0.0f;
                const bool runHovered = run_info(lineIndex) && lineHovered
                    && point_in_rect(g_frame.mousePos, boxX + messageInsetX,
                        runHeaderY, (std::max)(1.0f, runHeaderWidth),
                        run_header_height(lineIndex));
                if (runHovered && left_press_available())
                {
                    state.expandedRuns[run_key(lineIndex)] = !run_expanded(lineIndex);
                    consume_left_press();
                }
                bool activityItemHovered = false;
                if (isStream && run_expanded(lineIndex))
                {
                    const auto* info = run_info(lineIndex);
                    if (info && info->route == "Activity")
                    {
                        const float activityWidth = (std::max)(1.0f,
                            boxWidth - 2.0f * messageInsetX);
                        float itemY = runHeaderY + run_header_height(lineIndex)
                            + gui_lib::chat_surface::run_details_padding;
                        for (std::size_t ordinal = 0u;
                            ordinal < info->activity_events.size(); ++ordinal)
                        {
                            const auto& item = info->activity_events[ordinal];
                            const bool hasDetails = !activity_detail(item).empty();
                            const std::string label = item.summary + (item.tool && !item.status.empty()
                                    ? " · " + item.status : "");
                            const float itemLabelWidth = (std::max)(1.0f,
                                activityWidth - activityIndent - 2.0f * activityItemPadding);
                            const std::string itemHeader = hasDetails
                                ? inline_disclosure_text(label, itemLabelWidth,
                                    activity_item_expanded(lineIndex, ordinal))
                                : fit_text_to_width(label, itemLabelWidth, kFontScale);
                            const float itemX = boxX + messageInsetX
                                + activityIndent + activityItemPadding;
                            const bool hit = lineHovered && hasDetails
                                && point_in_rect(g_frame.mousePos, itemX, itemY,
                                    (std::min)(itemLabelWidth,
                                        measure_text_width(itemHeader, kFontScale) + 10.0f),
                                    activityItemHeaderHeight);
                            activityItemHovered = activityItemHovered || hit;
                            if (hit && left_press_available())
                            {
                                const auto key = activity_item_key(lineIndex, ordinal);
                                // Read effective expansion, including the
                                // default-open state of live raw token streams.
                                activityToggle = {key,
                                    !activity_item_expanded(lineIndex, ordinal)};
                                consume_left_press();
                            }
                            itemY += activity_item_height(lineIndex, ordinal,
                                activityWidth);
                        }
                    }
                }

                if (options.selectable && left_press_available() && lineHovered
                    && !runHovered && !activityItemHovered && !copyHovered && !editHovered)
                {
                    state.selectionAnchorLine = lineIndex;
                    state.selectedLine = lineIndex;
                    state.hasSelection = true;
                    state.draggingSelection = true;
                    result.selected_line = lineIndex;
                    consume_left_press();
                }
                else if (options.selectable && state.draggingSelection && g_frame.mouseDown && lineHovered)
                {
                    state.selectedLine = lineIndex;
                    state.hasSelection = true;
                    result.selected_line = lineIndex;
                }

                if (options.selectable && right_press_available() && lineHovered)
                {
                    const auto [firstSelected, lastSelected] = selected_line_range();
                    if (!state.hasSelection || lineIndex < firstSelected || lineIndex > lastSelected)
                    {
                        state.selectionAnchorLine = lineIndex;
                        state.selectedLine = lineIndex;
                        state.hasSelection = true;
                        result.selected_line = lineIndex;
                    }
                    state.contextMenuOpen = true;
                    state.contextMenuPos = g_frame.mousePos;
                    state.contextLineIndex = lineIndex;
                    state.contextLineText = options.lines[lineIndex];
                    openedContextMenuThisFrame = true;
                    consume_right_press();
                }

                const TextMessageRole role = line_role(lineIndex);
                const bool styledRole = role_is_styled(role);
                const std::string_view line = line_view(lineIndex);
                const bool bubble = line_is_bubble(lineIndex);
                // Drawing, hover/copy hit-testing and message bounds share
                // one geometry calculation. No controls float in empty space.
                const auto bubbleStyle = gui_lib::rounded_rect::RoundedRectStyle{
                    .enabled = true,
                    .control_radius = options.conversation_style
                        ? gui_lib::chat_surface::user_bubble_radius : 8.0f};
                const float visibleMessageHeight = rowHeight - rowGap
                    - (messageActions ? 23.0f : 0.0f);
                if (styledRole)
                {
                    const auto& semantic = active_semantic_pair(
                        semantic_tone_for_message_role(role));
                    if (bubble)
                    {
                        // Neutral rounded answer cards, distinct from blue
                        // user turns and from semantic errors/diagnostics.
                        const auto background = options.conversation_style
                            && role == TextMessageRole::assistant
                            ? palette.panelBackground : semantic.background;
                        draw_rounded_sprite(background, boxX, rowY,
                            boxWidth, visibleMessageHeight, bubbleStyle);
                    }
                    else if (options.conversation_style &&
                        (isStream || role == TextMessageRole::assistant))
                    {
                        // Never paint the assistant response as a full-width
                        // card or diagnostic rail. Activity stays subdued.
                    }
                    else if (options.conversation_style)
                    {
                        draw_sprite(semantic.accent, boxX, rowY,
                            3.0f, rowHeight - rowGap);
                    }
                    else
                    {
                        draw_sprite(semantic.background, boxX, rowY, boxWidth, rowHeight - rowGap);
                        draw_sprite(semantic.accent, boxX, rowY, 4.0f, rowHeight - rowGap);
                    }
                }

                const auto [firstSelected, lastSelected] = selected_line_range();
                if (options.selectable && state.hasSelection && lineIndex >= firstSelected && lineIndex <= lastSelected)
                {
                    if (bubble) draw_rounded_sprite(palette.buttonActive, boxX, rowY,
                        boxWidth, visibleMessageHeight, bubbleStyle);
                    else if (!(options.conversation_style && isStream))
                        draw_sprite(palette.buttonActive, contentX - 2.0f, rowY - 1.0f,
                            contentWidth + 4.0f, rowHeight);
                }
                else if (lineHovered)
                {
                    if (bubble) draw_rounded_sprite(palette.buttonHover, boxX, rowY,
                        boxWidth, visibleMessageHeight, bubbleStyle);
                    else if (!(options.conversation_style &&
                        (role == TextMessageRole::assistant || isStream)))
                        draw_sprite(palette.buttonHover, contentX - 2.0f, rowY - 1.0f,
                            contentWidth + 4.0f, rowHeight);
                }

                // Read the answer first. Model/timing and expandable evidence
                // appear BELOW it, never before or over the reply.
                const float insetY = isStream ? 4.0f
                    : (styledRole ? messageInsetY : 0.0f);
                const float textX = boxX + (styledRole ? messageInsetX : 0.0f);
                const float textY = rowY + insetY
                    + (isStream ? run_header_height(lineIndex) : 0.0f);
                const float textWidth = (std::max)(1.0f,
                    boxWidth - (styledRole ? messageInsetX * 2.0f : 0.0f));
                if (!isStream)
                {
                    if (options.wrap_lines)
                        draw_wrapped_text(line, textX, textY, textWidth, kFontScale,
                            options.conversation_style && !bubble &&
                            (role == TextMessageRole::engine || role == TextMessageRole::system
                                || role == TextMessageRole::warning || role == TextMessageRole::success
                                || role == TextMessageRole::error));
                    else
                    {
                        const std::string fitted = fit_text_to_width(line, textWidth, kFontScale);
                        draw_text_line(fitted.empty() ? line : std::string_view{fitted},
                            textX, textY, kFontScale);
                    }
                }
                if (const auto* info = image_info(lineIndex))
                {
                    const auto count = (std::min)(info->images.size(), std::size_t{4u});
                    const float imageY = textY + bodyHeights[lineIndex] - reference_height(lineIndex);
                    const auto savedCursor = cursor_position();
                    for (std::size_t index = 0u; index < count; ++index)
                    {
                        const auto& attachment = info->images[index];
                        const float rowY = imageY + index * 78.0f;
                        const float thumb = (std::min)(64.0f, textWidth);
                        const float nameX = textX + thumb + 10.0f;
                        const float nameWidth = (std::max)(1.0f, textWidth - thumb - 10.0f);
                        set_cursor({textX, rowY});
                        (void)image_box({.id = attachment.id, .sprite = attachment.preview,
                            .size = {thumb, 64.0f}, .source_size = attachment.source_size,
                            .fit = ImageFit::Contain});
                        draw_text_line(fit_text_to_width(attachment.name, nameWidth, kFontScale),
                            nameX, rowY + 7.0f, kFontScale);
                        draw_wrapped_text(attachment.status, nameX, rowY + 32.0f,
                            nameWidth, kFontScale, true);
                    }
                    set_cursor(savedCursor);
                }
                if (run_header_height(lineIndex) > 0.0f)
                {
                    const auto* info = run_info(lineIndex);
                    const float headerY = isStream ? rowY + insetY
                        : textY + bodyHeights[lineIndex];
                    // Draw title, status and disclosure glyph in one font run:
                    // "Activity · Working  >", never two independently placed items.
                    const std::string inlineHeader = inline_disclosure_text(
                        run_header_label(lineIndex),
                        (std::max)(1.0f, boxWidth - 2.0f * messageInsetX),
                        run_expanded(lineIndex));
                    draw_text_line(inlineHeader, boxX + messageInsetX,
                        headerY + 3.0f, kFontScale);
                    if (info && run_expanded(lineIndex))
                    {
                        const float panelY = headerY + run_header_height(lineIndex);
                        // The Activity tree has plain disclosure labels.
                        // Rounded panels belong around expanded content, not
                        // behind every dropdown title or the entire event list.
                        if (info->route != "Activity")
                            draw_rounded_sprite(palette.panelBackground,
                                boxX + 2.0f, panelY,
                                (std::max)(1.0f, boxWidth - 4.0f),
                                detailHeights[lineIndex],
                                gui_lib::rounded_rect::RoundedRectStyle{
                                    .enabled = true, .control_radius = 10.0f});
                        if (info->route == "Activity" && !info->activity_events.empty())
                        {
                            const float activityWidth = (std::max)(1.0f,
                                boxWidth - 2.0f * messageInsetX);
                            float itemY = panelY
                                + gui_lib::chat_surface::run_details_padding;
                            for (std::size_t ordinal = 0u;
                                ordinal < info->activity_events.size(); ++ordinal)
                            {
                                const auto& item = info->activity_events[ordinal];
                                const bool expanded = activity_item_expanded(
                                    lineIndex, ordinal);
                                const float height = activity_item_height(
                                    lineIndex, ordinal, activityWidth);
                                const float itemX = boxX + messageInsetX
                                    + activityIndent;
                                const bool itemHovered = !popupOwnedInput && hovered
                                    && point_in_rect(g_frame.mousePos, itemX,
                                        itemY, activityWidth - activityIndent,
                                        activityItemHeaderHeight);
                                const float textX = itemX + activityItemPadding;
                                const bool hasDetails = !activity_detail(item).empty();
                                const std::string label =
                                    item.summary
                                    + (item.tool && !item.status.empty()
                                        ? " · " + item.status : "");
                                // Child disclosures use the same inline positioning
                                // as the Activity parent. No far-right arrow lane.
                                const float labelWidth = (std::max)(1.0f,
                                    activityWidth - activityIndent
                                        - 2.0f * activityItemPadding);
                                const std::string childHeader = hasDetails
                                    ? inline_disclosure_text(label, labelWidth, expanded)
                                    : fit_text_to_width(label, labelWidth, kFontScale);
                                draw_text_line(childHeader, textX,
                                    itemY + 4.0f, kFontScale);
                                if (expanded)
                                {
                                    const float innerY = itemY
                                        + activityItemHeaderHeight;
                                    draw_rounded_sprite(palette.consoleBackground,
                                        itemX + 2.0f, innerY,
                                        (std::max)(1.0f,
                                            activityWidth - activityIndent - 4.0f),
                                        (std::max)(1.0f, height - activityItemHeaderHeight),
                                        gui_lib::rounded_rect::RoundedRectStyle{
                                            .enabled = true, .control_radius = 10.0f});
                                    draw_wrapped_text(activity_detail(item),
                                        textX + activityIndent,
                                        innerY + activityItemPadding,
                                        (std::max)(1.0f,
                                            activityWidth - 2.0f * activityItemPadding
                                                - 2.0f * activityIndent),
                                        kFontScale, !item.tool && item.group != "Model",
                                        item.tool || item.summary == "Tool arguments"
                                            || (item.summary == "Response tokens"
                                                && activity_detail(item).find("```") != std::string_view::npos));
                                }
                                itemY += height;
                            }
                        }
                        else draw_wrapped_text(detail_text(*info),
                            boxX + messageInsetX,
                            panelY + gui_lib::chat_surface::run_details_padding,
                            (std::max)(1.0f, boxWidth - 2.0f * messageInsetX),
                            kFontScale);
                    }
                }
                if (messageActions && lineHovered)
                {
                    // Sprite-drawn symbols avoid fragile Unicode fallback
                    // glyphs and remain legible on Windows at any DPI scale.
                    const auto& ink = palette.textFieldActive;
                    if (copyHovered)
                        draw_rounded_sprite(palette.buttonHover, copyX, actionsY,
                            26.0f, 23.0f, bubbleStyle);
                    // Two overlapped paper outlines = Copy.
                    const float cx = copyX + 9.0f, cy = actionsY + 4.0f;
                    draw_sprite(ink, cx, cy, 10.0f, 1.5f);
                    draw_sprite(ink, cx, cy, 1.5f, 12.0f);
                    draw_sprite(ink, cx + 8.5f, cy, 1.5f, 12.0f);
                    draw_sprite(ink, cx, cy + 10.5f, 10.0f, 1.5f);
                    draw_sprite(ink, cx - 3.0f, cy + 3.0f, 1.5f, 12.0f);
                    draw_sprite(ink, cx - 3.0f, cy + 13.5f, 10.0f, 1.5f);
                    if (actionRole == TextMessageRole::user)
                    {
                        if (editHovered)
                            draw_rounded_sprite(palette.buttonHover, editX, actionsY,
                                26.0f, 23.0f, bubbleStyle);
                        // Small pencil and underline = Edit.
                        const float ex = editX + 7.0f, ey = actionsY + 5.0f;
                        for (int segment = 0; segment < 9; ++segment)
                            draw_sprite(ink, ex + static_cast<float>(segment),
                                ey + 9.0f - static_cast<float>(segment), 2.4f, 2.4f);
                        draw_sprite(ink, ex, ey + 14.0f, 12.0f, 1.5f);
                    }
                }
                rowY += rowHeight;
            }
            if (activityToggle.has_value())
                state.expandedActivityItems[activityToggle->first] =
                    activityToggle->second;
        }

        if (!g_frame.mouseDown)
            state.draggingSelection = false;

        if (scrollbarWidth > 0.0f)
        {
            const float trackX = pos.x + width - kBoxInnerPadding - scrollbarWidth;
            const float trackY = contentY;
            draw_sprite(palette.textField, trackX, trackY, scrollbarWidth, contentHeight);

            if (canScroll)
            {
                const auto scrollbar = gui_lib::make_scrollbar_layout({
                    .track = {{trackX, trackY}, {scrollbarWidth, contentHeight}},
                    .content_extent = totalTextHeight, .scroll_offset = state.scrollY });
                const float thumbHeight = scrollbar.thumb.size.y;
                const float thumbY = scrollbar.thumb.position.y;
                draw_sprite(palette.buttonActive, trackX, thumbY, scrollbarWidth, thumbHeight);
            }
        }

        if (state.hasSelection && state.selectedLine < lineCount)
            result.selected_line = state.selectedLine;

        // A trimmed/replaced log must not replay whichever message inherited
        // the old row index while the popup was open.
        if (state.contextMenuOpen
            && (state.contextLineIndex >= options.lines.size()
                || !options.lines[state.contextLineIndex].starts_with(state.contextLineText)))
            state.contextMenuOpen = false;

        if (state.contextMenuOpen)
        {
            const auto geometry = context_menu_geometry();
            const float rowHeight = geometry.button_height;
            const float menuPadding = geometry.padding;
            const float menuWidth = geometry.bounds.width;
            const float menuHeight = geometry.bounds.height;
            const Vec2 menuPos{geometry.bounds.x, geometry.bounds.y};

            const bool hoveredMenu = point_in_rect(g_frame.mousePos, menuPos.x, menuPos.y, menuWidth, menuHeight);
            if (!openedContextMenuThisFrame && (left_press_available() || right_press_available()) && !hoveredMenu)
            {
                // Dismissal consumes the pointer event: the underlying chat
                // must not respond to the same outside click.
                consume_left_press();
                consume_right_press();
                state.contextMenuOpen = false;
            }

            if (state.contextMenuOpen)
            {
                const Vec2 savedCursor = g_frame.cursor;
                const Vec2 savedOrigin = g_frame.origin;
                const Vec2 savedWindowSize = g_frame.windowSize;
                const Vec2 savedContentMin = g_frame.contentMin;
                const Vec2 savedContentMax = g_frame.contentMax;
                const bool savedInsideWindow = g_frame.insideWindow;
                const std::string savedWindowKey = g_frame.windowKey;
                const std::uint64_t savedWidgetSerial = g_frame.widgetSerial;

                begin_top_layer();
                const auto& menuPalette = active_palette();
                draw_rounded_sprite(menuPalette.windowBackground,
                    menuPos.x, menuPos.y, menuWidth, menuHeight,
                    gui_lib::rounded_rect::RoundedRectStyle{
                        .enabled = true, .control_radius = 10.0f});

                g_frame.insideWindow = true;
                g_frame.windowKey = id + "-text-context-menu";
                g_frame.widgetSerial = 0;
                g_frame.origin = menuPos;
                g_frame.windowSize = { menuWidth, menuHeight };
                g_frame.contentMin = { menuPos.x + menuPadding, menuPos.y + menuPadding };
                g_frame.contentMax = { menuPos.x + menuWidth - menuPadding, menuPos.y + menuHeight - menuPadding };
                const auto place_context_action = [&](std::size_t row)
                {
                    set_cursor({menuPos.x + menuPadding,
                        menuPos.y + menuPadding + static_cast<float>(row)
                            * (rowHeight + geometry.gap)});
                };
                place_context_action(0u);
                if (button("Select All", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    state.selectionAnchorLine = 0u;
                    state.selectedLine = lineCount > 0u ? lineCount - 1u : 0u;
                    state.hasSelection = lineCount > 0u;
                    state.contextMenuOpen = false;
                }
                place_context_action(1u);
                if (button("Copy Selection", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    (void)clipboard_write_text(selected_lines_text());
                    state.contextMenuOpen = false;
                }
                place_context_action(2u);
                if (button("Copy All", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    std::string transcript;
                    for (std::size_t index = 0u;
                        index < options.lines.size(); ++index)
                    {
                        if (!transcript.empty()) transcript += "\n\n";
                        transcript += export_line(index);
                    }
                    (void)clipboard_write_text(transcript);
                    state.contextMenuOpen = false;
                }
                place_context_action(3u);
                if (button("Clear Selection", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    state.hasSelection = false;
                    state.contextMenuOpen = false;
                }
                if (context_line_is_retryable()) place_context_action(4u);
                if (context_line_is_retryable() && button_with_state("Retry Request",
                        {menuWidth - 2.0f * menuPadding, rowHeight},
                        false, options.retry_enabled))
                {
                    result.retry_line_index = state.contextLineIndex;
                    state.contextMenuOpen = false;
                }
                // Padding between controls is still part of the popup input
                // surface. Do not pass through presses or releases in a gap.
                if (hoveredMenu)
                {
                    consume_left_press();
                    consume_left_release();
                    consume_right_press();
                    consume_right_release();
                }

                g_frame.cursor = savedCursor;
                g_frame.origin = savedOrigin;
                g_frame.windowSize = savedWindowSize;
                g_frame.contentMin = savedContentMin;
                g_frame.contentMax = savedContentMax;
                g_frame.insideWindow = savedInsideWindow;
                g_frame.windowKey = savedWindowKey;
                g_frame.widgetSerial = savedWidgetSerial;
                end_top_layer();
            }
        }

        result.scroll_y = state.scrollY;
        result.maximum_scroll_y = maxScrollY;
        result.at_end = state.scrollY >= maxScrollY - 1.5f;
        advance_cursor({ 0.0f, height + kContentPadding });
        return result;
    }

