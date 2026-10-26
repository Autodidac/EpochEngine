// Epoch feature implementation; included by its owning module only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
    std::optional<WidgetBounds> last_button_bounds() noexcept
    {
        return g_frame.lastButtonBounds;
    }

    Vec2 cursor_position() noexcept
    {
        return g_frame.cursor;
    }

    float line_height() noexcept
    {
        try { ensure_resources(); }
        catch (...) { return 16.0f; }
        return line_advance_amount(kFontScale);
    }

    float glyph_width() noexcept
    {
        try { ensure_resources(); }
        catch (...) { return 8.0f; }
        return space_advance(kFontScale);
    }

    float titled_window_total_height(float content_height) noexcept
    {
        try { ensure_resources(); }
        catch (...) { return content_height + 48.0f; }

        const float titleHeight = line_advance_amount(kTitleScale);
        const float titleBarHeight = titleHeight + 2.0f * kTitleBarPadding;
        return titleBarHeight + 2.0f * kContentPadding + (std::max)(0.0f, content_height);
    }

    void label(std::string_view text) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;

        draw_text_line(text, g_frame.cursor.x, g_frame.cursor.y, kFontScale);
        advance_cursor({ 0.0f, line_advance_amount(kFontScale) });
    }

    void status_label(std::string_view text, SemanticTone tone, Vec2 position) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx || text.empty()) return;
        ensure_resources();
        const bool light = active_palette_is_light();
        const GuiFontCache* tinted = nullptr;
        switch (tone)
        {
        case SemanticTone::success:
            tinted = light && g_resources.statusSuccessLightFont.asset
                ? &g_resources.statusSuccessLightFont : &g_resources.statusSuccessFont;
            break;
        case SemanticTone::warning:
            tinted = light && g_resources.statusWarningLightFont.asset
                ? &g_resources.statusWarningLightFont : &g_resources.statusWarningFont;
            break;
        case SemanticTone::error:
            tinted = light && g_resources.statusErrorLightFont.asset
                ? &g_resources.statusErrorLightFont : &g_resources.statusErrorFont;
            break;
        case SemanticTone::info:
            tinted = light && g_resources.statusInfoLightFont.asset
                ? &g_resources.statusInfoLightFont : &g_resources.statusInfoFont;
            break;
        default: break;
        }
        const auto* previous = g_temporaryTextFont;
        if (tinted && tinted->asset) g_temporaryTextFont = tinted;
        draw_text_line(text, position.x, position.y, kFontScale);
        g_temporaryTextFont = previous;
    }

    void section_heading(std::string_view text) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;
        ensure_resources();
        const float width = content_available_width(g_frame.cursor.x);
        const float titleHeight = line_advance_amount(kTitleScale);
        draw_sprite(active_palette().panelBackground, g_frame.cursor.x,
            g_frame.cursor.y, width, titleHeight + 10.0f);
        const std::string fitted = fit_text_to_width(text,
            (std::max)(1.0f, width - 12.0f), kTitleScale);
        draw_text_line(fitted, g_frame.cursor.x + 6.0f,
            g_frame.cursor.y + 3.0f, kTitleScale);
        advance_cursor({0.0f, titleHeight + 14.0f});
    }

    void workflow_step(std::string_view state, std::string_view description,
        SemanticTone tone, float width) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;
        ensure_resources();
        const float availableWidth = content_available_width(g_frame.cursor.x);
        const float rowWidth = (std::max)(1.0f,
            width > 0.0f ? (std::min)(availableWidth, width) : availableWidth);
        const bool inlineState = rowWidth >= 220.0f;
        const float padding = 9.0f;
        const float stateWidth = inlineState ? 78.0f : rowWidth - padding * 2.0f;
        const float textX = g_frame.cursor.x + padding + (inlineState ? stateWidth : 0.0f);
        const float descWidth = (std::max)(space_advance(kFontScale),
            rowWidth - padding * 2.0f - (inlineState ? stateWidth : 0.0f));
        const float statusHeight = measure_wrapped_text_height(state,
            (std::max)(space_advance(kFontScale), stateWidth), kFontScale);
        const float descHeight = measure_wrapped_text_height(description,
            descWidth, kFontScale);
        const float mainHeight = inlineState ? (std::max)(statusHeight, descHeight)
            : statusHeight + 2.0f + descHeight;
        const float rowHeight = (std::max)(line_advance_amount(kFontScale) + padding * 2.0f,
            mainHeight + padding * 2.0f);
        const auto& semantic = active_semantic_pair(tone);
        draw_rounded_sprite(semantic.background, g_frame.cursor.x,
            g_frame.cursor.y, rowWidth, rowHeight,
            gui_lib::rounded_rect::RoundedRectStyle{.enabled=true, .control_radius=7.0f});
        draw_sprite(semantic.accent, g_frame.cursor.x, g_frame.cursor.y + 4.0f,
            3.0f, rowHeight - 8.0f);
        draw_wrapped_text(state, g_frame.cursor.x + padding,
            g_frame.cursor.y + padding, stateWidth, kFontScale, true);
        draw_wrapped_text(description, textX,
            g_frame.cursor.y + padding + (inlineState ? 0.0f : statusHeight + 2.0f),
            descWidth, kFontScale);
        advance_cursor({0.0f, rowHeight + 5.0f});
    }

    void wrapped_label(std::string_view text, float width) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;

        const float availableWidth = content_available_width(g_frame.cursor.x);
        const float wrapWidth = (std::max)(space_advance(kFontScale),
            width > 0.0f ? (std::min)(availableWidth, width) : availableWidth);

        const float drawnHeight = draw_wrapped_text(
            text,
            g_frame.cursor.x,
            g_frame.cursor.y,
            wrapWidth,
            kFontScale);
        advance_cursor({ 0.0f, drawnHeight });
    }

    void semantic_block(
        std::string_view text,
        SemanticTone tone,
        float width) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;
        ensure_resources();

        constexpr float padding = 8.0f;
        constexpr float accentWidth = 4.0f;
        const float availableWidth = content_available_width(g_frame.cursor.x);
        const float blockWidth = width > 0.0f
            ? (std::min)(availableWidth, (std::max)(32.0f, width))
            : availableWidth;
        const float textWidth = (std::max)(
            space_advance(kFontScale),
            blockWidth - accentWidth - padding * 2.0f);
        const float textHeight = measure_wrapped_text_height(
            text, textWidth, kFontScale);
        const float blockHeight = (std::max)(
            line_advance_amount(kFontScale) + padding * 2.0f,
            textHeight + padding * 2.0f);
        const auto& semantic = active_semantic_pair(tone);
        draw_sprite(
            semantic.background,
            g_frame.cursor.x,
            g_frame.cursor.y,
            blockWidth,
            blockHeight);
        draw_sprite(
            semantic.accent,
            g_frame.cursor.x,
            g_frame.cursor.y,
            accentWidth,
            blockHeight);
        draw_wrapped_text(
            text,
            g_frame.cursor.x + accentWidth + padding,
            g_frame.cursor.y + padding,
            textWidth,
            kFontScale);
        advance_cursor({0.0f, blockHeight + kContentPadding});
    }

    void property_row(std::string_view labelText, std::string_view valueText, float label_width) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;

        const Vec2 pos = g_frame.cursor;
        const float availableWidth = content_available_width(g_frame.cursor.x);
        const float minReadableValueWidth = space_advance(kFontScale) * 14.0f;
        const float labelWidth = std::clamp(
            label_width,
            space_advance(kFontScale) * 6.0f,
            (std::max)(space_advance(kFontScale) * 6.0f, availableWidth * 0.55f));
        const float gap = 10.0f;
        const float valueX = pos.x + labelWidth + gap;
        const float valueWidth = (std::max)(space_advance(kFontScale), availableWidth - labelWidth - gap);

        if (valueWidth < minReadableValueWidth)
        {
            const std::string clippedLabel = fit_text_to_width(labelText, availableWidth, kFontScale);
            draw_text_line(clippedLabel, pos.x, pos.y, kFontScale);

            const float nextLineY = pos.y + line_advance_amount(kFontScale);
            const float valueHeight = valueText.empty()
                ? line_advance_amount(kFontScale)
                : draw_wrapped_text(valueText, pos.x, nextLineY, availableWidth, kFontScale);

            advance_cursor({
                0.0f,
                line_advance_amount(kFontScale) + (std::max)(line_advance_amount(kFontScale), valueHeight)
            });
            return;
        }

        const std::string clippedLabel = fit_text_to_width(labelText, labelWidth, kFontScale);
        draw_text_line(clippedLabel, pos.x, pos.y, kFontScale);

        const float valueHeight = valueText.empty()
            ? line_advance_amount(kFontScale)
            : draw_wrapped_text(valueText, valueX, pos.y, valueWidth, kFontScale);

        advance_cursor({ 0.0f, (std::max)(line_advance_amount(kFontScale), valueHeight) });
    }

    float wrapped_text_height(std::string_view text, float width) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return 0.0f;

        const float availableWidth = content_available_width(g_frame.cursor.x);
        const float wrapWidth = (std::max)(space_advance(kFontScale),
            width > 0.0f ? (std::min)(availableWidth, width) : availableWidth);

        return measure_wrapped_text_height(text, wrapWidth, kFontScale);
    }

    float measure_wrapped_label_height(std::string_view text, float width) noexcept
    {
        try { ensure_resources(); }
        catch (...) { return 0.0f; }

        const float wrapWidth = width > 0.0f
            ? (std::max)(space_advance(kFontScale), width)
            : 1.0f;
        return measure_wrapped_text_height(text, wrapWidth, kFontScale);
    }

    [[nodiscard]] static std::size_t edit_box_line_start(
        std::string_view text,
        std::size_t index) noexcept
    {
        index = (std::min)(index, text.size());
        if (index == 0u)
            return 0u;
        const std::size_t newline = text.rfind('\n', index - 1u);
        return newline == std::string_view::npos ? 0u : newline + 1u;
    }

    [[nodiscard]] static std::size_t edit_box_line_end(
        std::string_view text,
        std::size_t index) noexcept
    {
        index = (std::min)(index, text.size());
        const std::size_t newline = text.find('\n', index);
        return newline == std::string_view::npos ? text.size() : newline;
    }

    [[nodiscard]] static float edit_box_x_for_index(
        std::string_view text,
        std::size_t lineStart,
        std::size_t index,
        float scale) noexcept
    {
        lineStart = (std::min)(lineStart, text.size());
        index = (std::min)((std::max)(lineStart, index), text.size());
        return measure_text_width(text.substr(lineStart, index - lineStart), scale);
    }

    [[nodiscard]] static gui_lib::TextGlyphMetrics edit_box_glyph_metrics(
        std::string_view text, std::size_t index, const void* context)
    {
        const float scale = *static_cast<const float*>(context);
        const auto decoded = decode_utf8_codepoint(text, index);
        const auto codepoint = safe_draw_codepoint(decoded.codepoint);
        return {decoded.byte_count,
            glyph_advance_with_kerning(codepoint, next_drawable_char(text, index), scale),
            is_wrap_space(codepoint)};
    }

    [[nodiscard]] static std::vector<gui_lib::TextVisualLine> edit_box_visual_lines(
        std::string_view text, float wrapWidth, float scale, bool multiline)
    {
        return gui_lib::text_visual_lines(text, wrapWidth, multiline,
            {edit_box_glyph_metrics, &scale, space_advance(scale)});
    }

    [[nodiscard]] static std::size_t edit_box_index_from_point(
        std::string_view text, float pointerX, float pointerY, float textX, float textY,
        gui_lib::Vec2 scroll, float lineAdvance, float scale, bool multiline, float wrapWidth) noexcept
    {
        const auto lines = edit_box_visual_lines(text, wrapWidth, scale, multiline);
        return gui_lib::text_index_from_point(text, lines,
            {pointerX - textX + scroll.x, pointerY - textY + scroll.y}, lineAdvance, multiline,
            {edit_box_glyph_metrics, &scale, space_advance(scale)});
    }

    [[nodiscard]] static gui_lib::TextControlMetrics edit_box_text_metrics(
        const gui_lib::TextControlState& state,
        float scale,
        float lineAdvance,
        float caretHeight,
        float wrapWidth,
        bool multiline) noexcept
    {
        const std::string_view text{ state.text };
        const auto lines = edit_box_visual_lines(text, wrapWidth, scale, multiline);
        float maximumWidth = 0.0f;
        for (const auto& line : lines)
        {
            maximumWidth = (std::max)(maximumWidth,
                measure_text_width(text.substr(
                    line.first, line.past_last - line.first), scale));
        }

        const std::size_t caret = (std::min)(state.caret, text.size());
        const std::size_t caretLine = gui_lib::text_visual_line_for_index(lines, caret);
        const auto line = lines[caretLine];
        return gui_lib::TextControlMetrics{
            .content_size = {
                multiline ? (std::min)(maximumWidth, wrapWidth) : maximumWidth,
                static_cast<float>(lines.size()) * lineAdvance
            },
            .caret_position = {
                edit_box_x_for_index(text, line.first, caret, scale),
                static_cast<float>(caretLine) * lineAdvance
            },
            .caret_size = { 1.0f, caretHeight },
            .valid = true
        };
    }

    EditBoxResult edit_box(
        std::string& text,
        Vec2 size,
        std::size_t max_chars,
        bool multiline,
        TextInputKeyboardPolicy keyboard_policy,
        bool submit_on_enter,
        std::string_view placeholder,
        bool stable_focus,
        bool integrated_composer,
        const std::function<bool()>& paste_handler) noexcept
    {
        EditBoxResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx)
            return result;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        const float baseHeight = base_line_height(kFontScale);
        const float lineAdvance = line_advance_amount(kFontScale);
        const float minWidth = space_advance(kFontScale) * 4.0f;
        const float minHeight = baseHeight + 2.0f * kBoxInnerPadding;
        const float width = (std::max)(static_cast<float>(size.x), minWidth);
        const float height = (std::max)(static_cast<float>(size.y), minHeight);
        const float contentWidth = (std::max)(1.0f, width - 2.0f * kBoxInnerPadding);
        const float contentHeight = (std::max)(1.0f, height - 2.0f * kBoxInnerPadding);
        const float textX = pos.x + kBoxInnerPadding;
        const float textY = pos.y + kBoxInnerPadding;
        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);

        const void* textKey = static_cast<const void*>(&text);
        // A growing chat composer changes both its position and size on every
        // soft-wrap boundary. Keep its focus/caret state bound to the same text
        // object instead of hashing a moving rectangle into a new widget ID.
        const void* id = stable_focus ? textKey
            : widget_focus_key("edit-box", textKey, pos, {width, height});
        const void* ctxKey = static_cast<const void*>(g_frame.ctx);
        const auto& palette = active_palette();
        auto& widget = g_editBoxTextStates[id];
        auto& menuState = g_editBoxMenuStates[id];

        if (!widget.initialized)
        {
            widget.control.text = text;
            widget.control.anchor = text.size();
            widget.control.caret = text.size();
            widget.initialized = true;
        }
        else if (widget.control.text != text)
        {
            widget.control.text = text;
            gui_lib::normalize_text_control(widget.control);
        }

        const std::size_t limit = max_chars == 0u
            ? (std::numeric_limits<std::size_t>::max)()
            : max_chars;
        const gui_lib::TextControlOptions options{
            .viewport_size = { contentWidth, contentHeight },
            .content_padding = {},
            .maximum_bytes = limit,
            .multiline = multiline,
            .read_only = false,
            .accept_tab = multiline
        };

        const auto metrics = [&]()
        {
            return edit_box_text_metrics(widget.control, kFontScale, lineAdvance, baseHeight, contentWidth, multiline);
        };
        const auto dispatch = [&](gui_lib::TextControlCommand command,
                                  std::string_view payload = {},
                                  std::size_t requestedCaret = gui_lib::invalid_text_index,
                                  bool extendSelection = false,
                                  bool focusRequested = false,
                                  bool blurRequested = false)
        {
            gui_lib::TextControlInput input{};
            input.command = command;
            input.text = payload;
            input.requested_caret = requestedCaret;
            input.metrics = metrics();
            input.extend_selection = extendSelection;
            input.focus_requested = focusRequested;
            input.blur_requested = blurRequested;
            const auto update = gui_lib::update_text_control(widget.control, options, input);
            if (update.clipboard_write_requested)
                (void)clipboard_write_text(update.clipboard_text);
            result.changed = result.changed || update.text_changed;
            return update;
        };

        const bool leftPressed = left_press_available();
        const bool pointerPressed = leftPressed || right_press_available();
        const bool shiftHeld = std::any_of(
            g_frame.events.begin(),
            g_frame.events.end(),
            [](const InputEvent& event) noexcept
            {
                return event.shift_down;
            });
        const void* activeBefore = ctxKey ? g_contextActiveWidgets[ctxKey] : nullptr;
        if (pointerPressed)
        {
            if (hovered)
            {
                if (ctxKey)
                    g_contextActiveWidgets[ctxKey] = id;
                g_frame.caretTimer = 0.0f;
                g_frame.caretVisible = true;
            }
            else if (activeBefore == id && !menuState.open)
            {
                if (ctxKey)
                    g_contextActiveWidgets[ctxKey] = nullptr;
                widget.draggingSelection = false;
            }
        }

        bool active = ctxKey && g_contextActiveWidgets[ctxKey] == id;
        if (active != widget.control.focused)
            (void)dispatch(gui_lib::TextControlCommand::none, {}, gui_lib::invalid_text_index, false, active, !active);

        if (g_textFieldSelectAllStates[textKey])
        {
            if (ctxKey)
                g_contextActiveWidgets[ctxKey] = id;
            active = true;
            (void)dispatch(gui_lib::TextControlCommand::select_all, {}, gui_lib::invalid_text_index, false, true, false);
            g_textFieldSelectAllStates[textKey] = false;
            g_frame.caretTimer = 0.0f;
            g_frame.caretVisible = true;
        }

        if (active && leftPressed && hovered)
        {
            const std::size_t requested = edit_box_index_from_point(
                widget.control.text,
                g_frame.mousePos.x,
                g_frame.mousePos.y,
                textX,
                textY,
                widget.control.scroll,
                lineAdvance,
                kFontScale,
                multiline,
                contentWidth);
            (void)dispatch(
                gui_lib::TextControlCommand::set_caret,
                {},
                requested,
                shiftHeld);
            widget.draggingSelection = true;
        }
        else if (active && widget.draggingSelection && g_frame.mouseDown)
        {
            const std::size_t requested = edit_box_index_from_point(
                widget.control.text,
                g_frame.mousePos.x,
                g_frame.mousePos.y,
                textX,
                textY,
                widget.control.scroll,
                lineAdvance,
                kFontScale,
                multiline,
                contentWidth);
            (void)dispatch(
                gui_lib::TextControlCommand::set_caret,
                {},
                requested,
                true);
        }
        if (!g_frame.mouseDown)
            widget.draggingSelection = false;

        const bool keyboardInputEnabled = active
            && g_frame.modalInput.owner_input_allowed()
            && (keyboard_policy == TextInputKeyboardPolicy::focused || hovered);
        if (keyboardInputEnabled)
        {
            for (const auto& event : g_frame.events)
            {
                if (event.type == EventType::TextInput)
                {
                    (void)dispatch(gui_lib::TextControlCommand::insert_text, event.text);
                    continue;
                }
                if (event.type != EventType::KeyDown)
                    continue;

                const int key = event.key;
                if (event.ctrl_down && (key == 'Z' || key == 'z'))
                    (void)dispatch(event.shift_down
                        ? gui_lib::TextControlCommand::redo
                        : gui_lib::TextControlCommand::undo);
                else if (event.ctrl_down && (key == 'Y' || key == 'y'))
                    (void)dispatch(gui_lib::TextControlCommand::redo);
                else if (event.ctrl_down && (key == 'A' || key == 'a'))
                    (void)dispatch(gui_lib::TextControlCommand::select_all);
                else if (event.ctrl_down && (key == 'C' || key == 'c'))
                    (void)dispatch(gui_lib::TextControlCommand::copy_selection);
                else if (event.ctrl_down && (key == 'X' || key == 'x'))
                    (void)dispatch(gui_lib::TextControlCommand::cut_selection);
                else if (event.ctrl_down && (key == 'V' || key == 'v'))
                {
                    if (!paste_handler || !paste_handler())
                        (void)dispatch(gui_lib::TextControlCommand::paste_text, clipboard_read_text());
                }
                else if (key == 8)
                    (void)dispatch(gui_lib::TextControlCommand::erase_backward);
                else if (key == 46 || key == 127)
                    (void)dispatch(gui_lib::TextControlCommand::erase_forward);
                else if (key == 37)
                    (void)dispatch(
                        event.ctrl_down ? gui_lib::TextControlCommand::move_word_left
                                        : gui_lib::TextControlCommand::move_left,
                        {},
                        gui_lib::invalid_text_index,
                        event.shift_down);
                else if (key == 39)
                    (void)dispatch(
                        event.ctrl_down ? gui_lib::TextControlCommand::move_word_right
                                        : gui_lib::TextControlCommand::move_right,
                        {},
                        gui_lib::invalid_text_index,
                        event.shift_down);
                else if (key == 38 && multiline)
                    (void)dispatch(gui_lib::TextControlCommand::move_up, {}, gui_lib::invalid_text_index, event.shift_down);
                else if (key == 40 && multiline)
                    (void)dispatch(gui_lib::TextControlCommand::move_down, {}, gui_lib::invalid_text_index, event.shift_down);
                else if (key == 36)
                    (void)dispatch(
                        event.ctrl_down ? gui_lib::TextControlCommand::move_document_start
                                        : gui_lib::TextControlCommand::move_line_start,
                        {},
                        gui_lib::invalid_text_index,
                        event.shift_down);
                else if (key == 35)
                    (void)dispatch(
                        event.ctrl_down ? gui_lib::TextControlCommand::move_document_end
                                        : gui_lib::TextControlCommand::move_line_end,
                        {},
                        gui_lib::invalid_text_index,
                        event.shift_down);
                else if (key == 27)
                {
                    if (ctxKey)
                        g_contextActiveWidgets[ctxKey] = nullptr;
                    active = false;
                    widget.draggingSelection = false;
                    (void)dispatch(gui_lib::TextControlCommand::none, {}, gui_lib::invalid_text_index, false, false, true);
                }
                else if (key == 13)
                {
                    if (!multiline || (submit_on_enter && !event.shift_down && !event.ctrl_down))
                        result.submitted = true;
                    else
                        (void)dispatch(gui_lib::TextControlCommand::insert_text, "\n");
                }
            }
        }

        gui_lib::update_text_control_scroll(widget.control, options, metrics());
        text = widget.control.text;
        result.active = active;

        // The outer composer draws the continuous rounded surface. Repainting
        // a rectangular field here created the old three-box appearance.
        if (!integrated_composer)
            draw_sprite(
                keyboardInputEnabled ? palette.textFieldActive : palette.textField,
                pos.x, pos.y, width, height);
        {
            ContentClipScope clip(
                { pos.x + 1.0f, pos.y + 1.0f },
                { pos.x + width - 1.0f, pos.y + height - 1.0f });
            const auto selection = gui_lib::text_selection(widget.control);
            const std::string_view view{ widget.control.text };
            if (view.empty() && !placeholder.empty())
            {
                const auto& halfFont = active_palette_is_light()
                    ? g_resources.halfTextLightFont : g_resources.halfTextFont;
                const auto* previous = g_temporaryTextFont;
                if (halfFont.asset) g_temporaryTextFont = &halfFont;
                draw_text_line(placeholder, textX, textY, kFontScale);
                g_temporaryTextFont = previous;
            }
            const auto visualLines = edit_box_visual_lines(
                view, contentWidth, kFontScale, multiline);
            for (std::size_t lineIndex = 0u; lineIndex < visualLines.size(); ++lineIndex)
            {
                const auto line = visualLines[lineIndex];
                const float drawX = textX - widget.control.scroll.x;
                const float drawY = textY
                    + static_cast<float>(lineIndex) * lineAdvance
                    - widget.control.scroll.y;

                if (!selection.empty()
                    && selection.past_last >= line.first
                    && selection.first <= line.past_last)
                {
                    const std::size_t highlightBegin = (std::max)(
                        selection.first, line.first);
                    const std::size_t highlightEnd = (std::min)(
                        selection.past_last, line.past_last);
                    const float highlightX = drawX
                        + edit_box_x_for_index(
                            view, line.first, highlightBegin, kFontScale);
                    const float highlightWidth = edit_box_x_for_index(
                        view, line.first, highlightEnd, kFontScale)
                        - edit_box_x_for_index(
                            view, line.first, highlightBegin, kFontScale);
                    if (highlightWidth > 0.0f)
                        draw_sprite(
                            palette.buttonActive,
                            highlightX,
                            drawY - 1.0f,
                            highlightWidth,
                            baseHeight + 2.0f);
                }

                draw_text_line(
                    view.substr(line.first, line.past_last - line.first),
                    drawX,
                    drawY,
                    kFontScale);
            }

            if (keyboardInputEnabled)
            {
                const std::size_t caret = (std::min)(widget.control.caret, view.size());
                const std::size_t caretLine = gui_lib::text_visual_line_for_index(
                    visualLines, caret);
                const auto line = visualLines[caretLine];
                const float caretX = textX - widget.control.scroll.x
                    + edit_box_x_for_index(view, line.first, caret, kFontScale);
                const float caretY = textY - widget.control.scroll.y
                    + static_cast<float>(caretLine) * lineAdvance;
                draw_caret(caretX, caretY, (std::min)(contentHeight, baseHeight));
            }
        }

        bool openedContextMenuThisFrame = false;
        if (right_press_available() && hovered)
        {
            consume_right_press();
            menuState.open = true;
            menuState.pos = g_frame.mousePos;
            openedContextMenuThisFrame = true;
        }

        if (menuState.open)
        {
            const float rowHeight = (std::max)(28.0f,
                base_line_height(kFontScale) + 2.0f * kBoxInnerPadding);
            const float menuPadding = 4.0f;
            const float requestedMenuWidth = 172.0f;
            const auto placement = gui_lib::chat_context_popup::place(
                {g_frame.contentMin.x, g_frame.contentMin.y,
                    (std::max)(0.0f, g_frame.contentMax.x - g_frame.contentMin.x),
                    (std::max)(0.0f, g_frame.contentMax.y - g_frame.contentMin.y)},
                menuState.pos.x, menuState.pos.y, requestedMenuWidth, rowHeight, 4u,
                menuPadding, kContentPadding);
            const Vec2 menuPos{placement.bounds.x, placement.bounds.y};
            const float menuWidth = placement.bounds.width;
            const float menuHeight = placement.bounds.height;

            const bool hoveredMenu = point_in_rect(g_frame.mousePos, menuPos.x, menuPos.y, menuWidth, menuHeight);
            if (!openedContextMenuThisFrame && (left_press_available() || right_press_available()) && !hoveredMenu)
            {
                consume_left_press();
                consume_right_press();
                menuState.open = false;
            }

            if (menuState.open)
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
                g_frame.windowKey = std::string("edit-box-context-menu-")
                    + std::to_string(reinterpret_cast<std::uintptr_t>(id));
                g_frame.widgetSerial = 0;
                g_frame.origin = menuPos;
                g_frame.windowSize = { menuWidth, menuHeight };
                g_frame.contentMin = { menuPos.x + menuPadding, menuPos.y + menuPadding };
                g_frame.contentMax = { menuPos.x + menuWidth - menuPadding, menuPos.y + menuHeight - menuPadding };
                const auto place_edit_menu_action = [&](std::size_t row)
                {
                    set_cursor({menuPos.x + menuPadding,
                        menuPos.y + menuPadding + static_cast<float>(row)
                            * (rowHeight + kContentPadding)});
                };
                place_edit_menu_action(0u);
                if (button("Select All", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    (void)dispatch(gui_lib::TextControlCommand::select_all);
                    menuState.open = false;
                }
                place_edit_menu_action(1u);
                if (button("Copy", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    (void)dispatch(gui_lib::TextControlCommand::copy_selection);
                    menuState.open = false;
                }
                place_edit_menu_action(2u);
                if (button("Cut", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    (void)dispatch(gui_lib::TextControlCommand::cut_selection);
                    menuState.open = false;
                }
                place_edit_menu_action(3u);
                if (button("Paste", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    if (!paste_handler || !paste_handler())
                        (void)dispatch(gui_lib::TextControlCommand::paste_text, clipboard_read_text());
                    menuState.open = false;
                }
                text = widget.control.text;

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

        advance_cursor({ 0.0f, height + kContentPadding });
        return result;
    }

    bool paste_text_in_edit_box(std::string& text,
        std::string_view inserted, std::size_t maximum_bytes) noexcept
    {
        if (inserted.empty()) return false;
        const void* key = static_cast<const void*>(&text);
        auto& widget = g_editBoxTextStates[key];
        if (!widget.initialized)
        {
            widget.control.text = text;
            widget.control.anchor = text.size();
            widget.control.caret = text.size();
            widget.initialized = true;
        }
        else if (widget.control.text != text)
        {
            widget.control.text = text;
            widget.control.undo_history.clear();
            widget.control.redo_history.clear();
            gui_lib::normalize_text_control(widget.control);
        }
        gui_lib::TextControlOptions options{};
        options.maximum_bytes = maximum_bytes;
        options.multiline = true;
        const gui_lib::TextControlInput input{
            .command = gui_lib::TextControlCommand::paste_text,
            .text = inserted};
        const auto result = gui_lib::update_text_control(widget.control, options, input);
        if (result.text_changed) text = widget.control.text;
        return result.text_changed;
    }

    void select_all_text_in_edit_box(std::string& text) noexcept
    {
        g_textFieldSelectAllStates[static_cast<const void*>(&text)] = true;
        g_frame.caretTimer = 0.0f;
        g_frame.caretVisible = true;
    }
    [[nodiscard]] static std::size_t line_count_for_text(std::string_view text) noexcept
    {
        if (text.empty())
            return 1u;

        std::size_t count = 1u;
        for (const char ch : text)
            if (ch == '\n')
                ++count;
        return count;
    }

    [[nodiscard]] static std::size_t line_start_for_index(std::string_view text, std::size_t targetLine) noexcept
    {
        if (targetLine == 0u)
            return 0u;

        std::size_t line = 0u;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '\n')
            {
                ++line;
                if (line == targetLine)
                    return (std::min)(i + 1u, text.size());
            }
        }

        return text.size();
    }

    [[nodiscard]] static std::string_view line_view_from_offset(std::string_view text, std::size_t& offset) noexcept
    {
        if (offset >= text.size())
            return {};

        const std::size_t start = offset;
        while (offset < text.size() && text[offset] != '\n')
            ++offset;

        const std::size_t end = offset;
        if (offset < text.size() && text[offset] == '\n')
            ++offset;

        return text.substr(start, end - start);
    }

    [[nodiscard]] static std::pair<std::size_t, std::size_t> source_editor_selection_range(
        const SourceEditorState& state,
        std::size_t textSize) noexcept
    {
        if (!state.hasSelection || state.cursorIndex == state.selectionAnchor)
            return { 0u, 0u };

        const std::size_t begin = (std::min)(state.cursorIndex, state.selectionAnchor);
        const std::size_t end = (std::max)(state.cursorIndex, state.selectionAnchor);
        return {
            (std::min)(begin, textSize),
            (std::min)(end, textSize)
        };
    }

    [[nodiscard]] static bool source_editor_has_selection(const SourceEditorState& state, std::size_t textSize) noexcept
    {
        const auto [begin, end] = source_editor_selection_range(state, textSize);
        return end > begin;
    }

    static void clamp_source_editor_cursor(SourceEditorState& state, std::size_t textSize) noexcept
    {
        state.cursorIndex = (std::min)(state.cursorIndex, textSize);
        state.selectionAnchor = (std::min)(state.selectionAnchor, textSize);
        state.hasSelection = state.hasSelection && state.cursorIndex != state.selectionAnchor;
    }

    [[nodiscard]] static std::size_t source_editor_line_end_for_start(std::string_view text, std::size_t lineStart) noexcept
    {
        std::size_t end = (std::min)(lineStart, text.size());
        while (end < text.size() && text[end] != '\n')
            ++end;
        return end;
    }

    [[nodiscard]] static float source_editor_x_for_index(
        std::string_view text,
        std::size_t lineStart,
        std::size_t index,
        float scale) noexcept
    {
        index = (std::min)(index, text.size());
        lineStart = (std::min)(lineStart, index);
        return measure_text_width(text.substr(lineStart, index - lineStart), scale);
    }

    [[nodiscard]] static std::size_t source_editor_index_from_point(
        std::string_view text,
        float pointerX,
        float pointerY,
        float textX,
        float firstTextY,
        float scrollY,
        float lineAdvance,
        float scale) noexcept
    {
        const float relativeY = (std::max)(0.0f, pointerY - firstTextY + scrollY);
        const std::size_t lineIndex = static_cast<std::size_t>(relativeY / (std::max)(1.0f, lineAdvance));
        const std::size_t lineStart = line_start_for_index(text, lineIndex);
        const std::size_t lineEnd = source_editor_line_end_for_start(text, lineStart);
        const float localX = pointerX - textX;

        if (localX <= 0.0f)
            return lineStart;

        float penX = 0.0f;
        for (std::size_t i = lineStart; i < lineEnd; ++i)
        {
            const std::size_t byteIndex = i;
            const DecodedUtf8Codepoint decoded =
                decode_utf8_codepoint(text, byteIndex);
            const char32_t codepoint =
                safe_draw_codepoint(decoded.codepoint);
            i += (std::max)(
                std::size_t{1u}, decoded.byte_count) - 1u;

            const float advance = glyph_advance_with_kerning(
                codepoint,
                next_drawable_char(text, byteIndex),
                scale);
            if (localX <= penX + advance * 0.5f)
                return byteIndex;
            penX += advance;
        }

        return lineEnd;
    }

    static void source_editor_set_cursor(SourceEditorState& state, std::size_t index, bool extendSelection, std::size_t textSize) noexcept
    {
        index = (std::min)(index, textSize);
        if (!extendSelection)
            state.selectionAnchor = index;
        state.cursorIndex = index;
        state.hasSelection = state.cursorIndex != state.selectionAnchor;
        state.verticalColumnActive = false;
    }

    [[nodiscard]] static std::size_t source_editor_line_for_index(
        const std::string_view text,
        const std::size_t index) noexcept
    {
        return static_cast<std::size_t>(std::count(
            text.begin(),
            text.begin() + static_cast<std::ptrdiff_t>(
                (std::min)(index, text.size())),
            '\n'));
    }

    [[nodiscard]] static std::size_t source_editor_line_start_for_cursor(
        const std::string_view text,
        const std::size_t index) noexcept
    {
        const std::size_t clamped = (std::min)(index, text.size());
        if (clamped == 0u)
            return 0u;
        const std::size_t newline = text.rfind('\n', clamped - 1u);
        return newline == std::string_view::npos ? 0u : newline + 1u;
    }

    [[nodiscard]] static std::size_t source_editor_column_for_index(
        const std::string_view text,
        const std::size_t lineStart,
        const std::size_t index) noexcept
    {
        std::size_t column{};
        for (std::size_t cursor = (std::min)(lineStart, text.size());
            cursor < (std::min)(index, text.size());)
        {
            cursor = next_utf8_codepoint_start(text, cursor);
            ++column;
        }
        return column;
    }

    [[nodiscard]] static std::size_t source_editor_index_for_column(
        const std::string_view text,
        const std::size_t lineStart,
        const std::size_t column) noexcept
    {
        const std::size_t lineEnd = source_editor_line_end_for_start(
            text, lineStart);
        std::size_t cursor = (std::min)(lineStart, lineEnd);
        for (std::size_t current{};
            current < column && cursor < lineEnd;
            ++current)
        {
            cursor = next_utf8_codepoint_start(text, cursor);
        }
        return (std::min)(cursor, lineEnd);
    }

    static void source_editor_move_vertical(
        SourceEditorState& state,
        const std::string_view text,
        const std::ptrdiff_t lineDelta,
        const bool extendSelection) noexcept
    {
        const std::size_t cursor = (std::min)(state.cursorIndex, text.size());
        const std::size_t currentLine = source_editor_line_for_index(text, cursor);
        const std::size_t lineCount = line_count_for_text(text);
        if (!state.verticalColumnActive)
        {
            state.preferredColumn = source_editor_column_for_index(
                text,
                source_editor_line_start_for_cursor(text, cursor),
                cursor);
            state.verticalColumnActive = true;
        }

        const std::ptrdiff_t targetSigned = (std::clamp)(
            static_cast<std::ptrdiff_t>(currentLine) + lineDelta,
            std::ptrdiff_t{0},
            static_cast<std::ptrdiff_t>(lineCount - 1u));
        const std::size_t targetLine = static_cast<std::size_t>(targetSigned);
        const std::size_t target = source_editor_index_for_column(
            text,
            line_start_for_index(text, targetLine),
            state.preferredColumn);
        if (!extendSelection)
            state.selectionAnchor = target;
        state.cursorIndex = target;
        state.hasSelection = state.cursorIndex != state.selectionAnchor;
    }

    [[nodiscard]] static float source_editor_clamped_scroll(
        const float requested,
        const float contentExtent,
        const float viewportExtent) noexcept
    {
        return (std::clamp)(
            requested,
            0.0f,
            (std::max)(0.0f, contentExtent - viewportExtent));
    }

    [[nodiscard]] bool source_editor_navigation_contract() noexcept
    {
        constexpr std::string_view text{"ab\nx\n1234\n\xe7\x95\x8cz"};
        SourceEditorState state{
            .cursorIndex = 2u,
            .selectionAnchor = 2u};
        source_editor_move_vertical(state, text, 1, false);
        if (state.cursorIndex != 4u || state.preferredColumn != 2u)
            return false;
        source_editor_move_vertical(state, text, 1, false);
        if (state.cursorIndex != 7u)
            return false;
        source_editor_move_vertical(state, text, -50, true);
        if (state.cursorIndex != 2u || !state.hasSelection)
            return false;
        source_editor_move_vertical(state, text, 50, false);
        if (source_editor_line_for_index(text, state.cursorIndex) != 3u
            || source_editor_column_for_index(
                text,
                source_editor_line_start_for_cursor(text, state.cursorIndex),
                state.cursorIndex) != 2u)
        {
            return false;
        }
        return source_editor_clamped_scroll(-12.0f, 500.0f, 100.0f) == 0.0f
            && source_editor_clamped_scroll(900.0f, 500.0f, 100.0f) == 400.0f
            && source_editor_clamped_scroll(20.0f, 80.0f, 100.0f) == 0.0f;
    }

    [[nodiscard]] static std::string source_editor_selected_text(std::string_view text, const SourceEditorState& state)
    {
        const auto [begin, end] = source_editor_selection_range(state, text.size());
        if (end <= begin)
            return std::string(text);
        return std::string(text.substr(begin, end - begin));
    }

    static bool source_editor_delete_selection(std::string& text, SourceEditorState& state)
    {
        const auto [begin, end] = source_editor_selection_range(state, text.size());
        if (end <= begin)
        {
            state.hasSelection = false;
            state.selectionAnchor = state.cursorIndex;
            return false;
        }

        text.erase(begin, end - begin);
        state.cursorIndex = begin;
        state.selectionAnchor = begin;
        state.hasSelection = false;
        state.verticalColumnActive = false;
        return true;
    }

    static void source_editor_insert_text_limited(
        std::string& text,
        SourceEditorState& state,
        std::string_view incoming,
        std::size_t limit,
        bool& changed,
        std::size_t& invalidUtf8Replacements)
    {
        if (source_editor_delete_selection(text, state))
            changed = true;
        state.verticalColumnActive = false;

        const std::size_t cursor = (std::min)(state.cursorIndex, text.size());
        const std::size_t available = limit > text.size() ? limit - text.size() : 0u;
        if (available == 0u)
            return;

        std::string filtered{};
        filtered.reserve((std::min)(incoming.size(), available));
        std::size_t incomingIndex = 0u;
        while (incomingIndex < incoming.size()
            && filtered.size() < available)
        {
            const unsigned char lead =
                static_cast<unsigned char>(incoming[incomingIndex]);
            if (lead == static_cast<unsigned char>('\r'))
            {
                ++incomingIndex;
                continue;
            }
            if (lead == static_cast<unsigned char>('\n'))
            {
                filtered.push_back('\n');
                ++incomingIndex;
                continue;
            }
            if (lead == static_cast<unsigned char>('\t'))
            {
                filtered.push_back('\t');
                ++incomingIndex;
                continue;
            }
            if (lead < 32u)
            {
                ++incomingIndex;
                continue;
            }

            const DecodedUtf8Codepoint decoded =
                decode_utf8_codepoint(incoming, incomingIndex);
            const std::size_t byteCount =
                (std::max)(std::size_t{1u}, decoded.byte_count);
            if (lead >= 0x80u && decoded.codepoint == U'?'
                && byteCount == 1u)
            {
                constexpr std::string_view replacement{"\xEF\xBF\xBD", 3u};
                if (replacement.size() > available - filtered.size())
                    break;
                filtered.append(replacement);
                ++invalidUtf8Replacements;
                ++incomingIndex;
                continue;
            }
            if (byteCount > available - filtered.size())
                break;
            filtered.append(incoming.substr(incomingIndex, byteCount));
            incomingIndex += byteCount;
        }

        if (filtered.empty())
            return;

        text.insert(cursor, filtered);
        state.cursorIndex = cursor + filtered.size();
        state.selectionAnchor = state.cursorIndex;
        state.hasSelection = false;
        changed = true;
    }

    SourceEditorResult source_editor(std::string& text, const SourceEditorOptions& options) noexcept
    {
        SourceEditorResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx)
            return result;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        const float availableWidth = (std::max)(1.0f, content_available_width(pos.x));
        const float requestedWidth = options.size.x > 0.0f
            ? (std::max)(96.0f, options.size.x)
            : availableWidth;
        const float width = (std::max)(1.0f, (std::min)(requestedWidth, availableWidth));
        const float height = (std::max)(120.0f, options.size.y > 0.0f ? options.size.y : 220.0f);
        const std::string id = options.id.empty()
            ? std::to_string(reinterpret_cast<std::uintptr_t>(&text))
            : std::string(options.id);
        auto& state = g_sourceEditorStates[scroll_panel_key(id)];
        const std::string scrollId = id + "-scroll";
        auto& scrollState = g_scrollAreaStates[scroll_panel_key(scrollId)];

        const float scrollbarReserve = 12.0f;
        const float editorWidth = (std::max)(64.0f, width - scrollbarReserve);
        const float contentWidth = (std::max)(1.0f, editorWidth - 2.0f * kBoxInnerPadding);
        const float lineAdvance = line_advance_amount(kFontScale);
        const float baseHeight = base_line_height(kFontScale);
        const std::size_t lineCount = line_count_for_text(text);
        const float textHeight = static_cast<float>(lineCount) * lineAdvance
            + 2.0f * kBoxInnerPadding
            + kContentPadding;
        const float editorContentHeight = (std::max)(height - 4.0f, textHeight);
        const bool hoveredEditor = point_in_rect(g_frame.mousePos, pos.x, pos.y, editorWidth, height)
            && point_in_active_clip(g_frame.mousePos);
        const void* textId = static_cast<const void*>(&text);
        const void* ctxKey = static_cast<const void*>(g_frame.ctx);
        const auto& palette = active_palette();
        const std::size_t limit = (options.max_chars == 0)
            ? std::numeric_limits<std::size_t>::max()
            : options.max_chars;

        const bool pointerPressed = left_press_available() || right_press_available();
        const void* currentActiveWidget = ctxKey ? g_contextActiveWidgets[ctxKey] : nullptr;
        clamp_source_editor_cursor(state, text.size());
        if (options.goto_generation != 0u
            && options.goto_generation != state.lastGotoGeneration)
        {
            const std::size_t targetLine = (std::max)(std::size_t{ 1u }, options.goto_line);
            state.cursorIndex = line_start_for_index(text, targetLine - 1u);
            state.selectionAnchor = state.cursorIndex;
            state.hasSelection = false;
            state.verticalColumnActive = false;
            state.lastGotoGeneration = options.goto_generation;
            scrollState.scrollY = static_cast<float>(targetLine - 1u) * lineAdvance;
        }
        if (pointerPressed)
        {
            if (hoveredEditor)
            {
                g_contextActiveWidgets[ctxKey] = textId;
                g_frame.caretTimer = 0.0f;
                g_frame.caretVisible = true;
            }
            else if (currentActiveWidget == textId && !state.contextMenuOpen)
            {
                g_contextActiveWidgets[ctxKey] = nullptr;
                state.draggingSelection = false;
            }
        }

        const bool active = g_contextActiveWidgets[ctxKey] == textId;
        result.edit.active = active;

        if (active && g_frame.modalInput.owner_input_allowed())
        {
            const auto replace_selection_if_needed = [&]() noexcept
            {
                if (source_editor_delete_selection(text, state))
                    result.edit.changed = true;
            };

            for (const auto& evt : g_frame.events)
            {
                switch (evt.type)
                {
                case EventType::TextInput:
                    if (!options.read_only)
                        source_editor_insert_text_limited(
                            text, state, evt.text, limit,
                            result.edit.changed,
                            result.invalid_utf8_replacements);
                    break;

                case EventType::KeyDown:
                    if (evt.ctrl_down && (evt.key == 'Z' || evt.key == 'z'))
                    {
                        if (!options.read_only)
                        {
                            result.undo_requested = !evt.shift_down;
                            result.redo_requested = evt.shift_down;
                        }
                    }
                    else if (evt.ctrl_down
                        && (evt.key == 'Y' || evt.key == 'y'))
                    {
                        if (!options.read_only)
                            result.redo_requested = true;
                    }
                    else if (evt.ctrl_down && (evt.key == 'C' || evt.key == 'c'))
                    {
                        result.copied = clipboard_write_text(source_editor_selected_text(text, state));
                    }
                    else if (evt.ctrl_down && (evt.key == 'X' || evt.key == 'x'))
                    {
                        result.cut = clipboard_write_text(source_editor_selected_text(text, state));
                        if (!options.read_only && source_editor_has_selection(state, text.size()))
                        {
                            replace_selection_if_needed();
                        }
                    }
                    else if (evt.ctrl_down && (evt.key == 'V' || evt.key == 'v'))
                    {
                        if (!options.read_only)
                        {
                            source_editor_insert_text_limited(
                                text, state, clipboard_read_text(), limit,
                                result.edit.changed,
                                result.invalid_utf8_replacements);
                            result.pasted = true;
                        }
                    }
                    else if (evt.ctrl_down && (evt.key == 'A' || evt.key == 'a'))
                    {
                        state.selectionAnchor = 0u;
                        state.cursorIndex = text.size();
                        state.hasSelection = !text.empty();
                        state.verticalColumnActive = false;
                        result.selected_all = state.hasSelection;
                    }
                    else if (evt.key == 8 || evt.key == 127)
                    {
                        if (!options.read_only && source_editor_has_selection(state, text.size()))
                        {
                            replace_selection_if_needed();
                        }
                        else if (!options.read_only && evt.key == 8 && state.cursorIndex > 0u)
                        {
                            const std::size_t eraseIndex =
                                previous_utf8_codepoint_start(
                                    text, state.cursorIndex);
                            text.erase(
                                eraseIndex,
                                state.cursorIndex - eraseIndex);
                            state.cursorIndex = eraseIndex;
                            state.selectionAnchor = eraseIndex;
                            state.verticalColumnActive = false;
                            result.edit.changed = true;
                        }
                        else if (!options.read_only && evt.key == 127
                            && state.cursorIndex < text.size())
                        {
                            const std::size_t eraseEnd =
                                next_utf8_codepoint_start(
                                    text, state.cursorIndex);
                            text.erase(
                                state.cursorIndex,
                                eraseEnd - state.cursorIndex);
                            state.verticalColumnActive = false;
                            result.edit.changed = true;
                        }
                    }
                    else if (evt.key == 27)
                    {
                        g_contextActiveWidgets[ctxKey] = nullptr;
                        state.draggingSelection = false;
                        state.hasSelection = false;
                        state.selectionAnchor = state.cursorIndex;
                        result.edit.active = false;
                    }
                    else if (evt.key == 13)
                    {
                        if (!options.read_only)
                            source_editor_insert_text_limited(
                                text, state, "\n", limit,
                                result.edit.changed,
                                result.invalid_utf8_replacements);
                    }
                    else if (evt.key == 37) // VK_LEFT
                    {
                        const std::size_t next =
                            previous_utf8_codepoint_start(
                                text, state.cursorIndex);
                        source_editor_set_cursor(state, next, evt.shift_down, text.size());
                    }
                    else if (evt.key == 39) // VK_RIGHT
                    {
                        const std::size_t next =
                            next_utf8_codepoint_start(
                                text, state.cursorIndex);
                        source_editor_set_cursor(state, next, evt.shift_down, text.size());
                    }
                    else if (evt.key == 38) // VK_UP
                    {
                        source_editor_move_vertical(
                            state, text, -1, evt.shift_down);
                    }
                    else if (evt.key == 40) // VK_DOWN
                    {
                        source_editor_move_vertical(
                            state, text, 1, evt.shift_down);
                    }
                    else if (evt.key == 33 || evt.key == 34) // VK_PRIOR / VK_NEXT
                    {
                        const std::size_t visibleLines = (std::max)(
                            std::size_t{1u},
                            static_cast<std::size_t>((std::max)(
                                lineAdvance,
                                height - 2.0f * kBoxInnerPadding)
                                / (std::max)(1.0f, lineAdvance)));
                        const std::ptrdiff_t pageLines = static_cast<std::ptrdiff_t>(
                            visibleLines > 1u ? visibleLines - 1u : 1u);
                        source_editor_move_vertical(
                            state,
                            text,
                            evt.key == 33 ? -pageLines : pageLines,
                            evt.shift_down);
                    }
                    else if (evt.ctrl_down && evt.key == 36) // Ctrl+Home
                    {
                        source_editor_set_cursor(
                            state, 0u, evt.shift_down, text.size());
                    }
                    else if (evt.ctrl_down && evt.key == 35) // Ctrl+End
                    {
                        source_editor_set_cursor(
                            state, text.size(), evt.shift_down, text.size());
                    }
                    else if (evt.key == 36) // VK_HOME
                    {
                        source_editor_set_cursor(
                            state,
                            source_editor_line_start_for_cursor(
                                text, state.cursorIndex),
                            evt.shift_down,
                            text.size());
                    }
                    else if (evt.key == 35) // VK_END
                    {
                        const std::size_t currentLineStart =
                            source_editor_line_start_for_cursor(
                                text, state.cursorIndex);
                        source_editor_set_cursor(
                            state,
                            source_editor_line_end_for_start(
                                text, currentLineStart),
                            evt.shift_down,
                            text.size());
                    }
                    break;

                default:
                    break;
                }
            }
        }

        const bool revealCaret = active
            && std::any_of(
                g_frame.events.begin(),
                g_frame.events.end(),
                [](const InputEvent& event)
                {
                    if (event.type == EventType::TextInput)
                        return true;
                    if (event.type != EventType::KeyDown)
                        return false;
                    return event.key == 8 || event.key == 127
                        || event.key == 13 || event.key == 33
                        || event.key == 34 || event.key == 35
                        || event.key == 36 || event.key == 37
                        || event.key == 38 || event.key == 39
                        || event.key == 40
                        || (event.ctrl_down
                            && (event.key == 'A' || event.key == 'a'
                                || event.key == 'V' || event.key == 'v'
                                || event.key == 'X' || event.key == 'x'));
                });
        if (revealCaret)
        {
            const auto cursorEnd = text.begin()
                + static_cast<std::ptrdiff_t>(
                    (std::min)(state.cursorIndex, text.size()));
            const std::size_t caretLine = static_cast<std::size_t>(
                std::count(text.begin(), cursorEnd, '\n'));
            const float caretTop =
                static_cast<float>(caretLine) * lineAdvance;
            const float caretBottom = caretTop + baseHeight
                + 2.0f * kBoxInnerPadding;
            if (caretTop < scrollState.scrollY)
                scrollState.scrollY = caretTop;
            else if (caretBottom > scrollState.scrollY + height)
                scrollState.scrollY = caretBottom - height;

            std::size_t caretLineStart{};
            for (std::size_t index = 0u; index < (std::min)(state.cursorIndex, text.size()); ++index)
                if (text[index] == '\n')
                    caretLineStart = index + 1u;
            const float caretOffsetX = source_editor_x_for_index(
                text, caretLineStart, (std::min)(state.cursorIndex, text.size()), kFontScale);
            if (caretOffsetX < state.scrollX)
                state.scrollX = caretOffsetX;
            else if (caretOffsetX > state.scrollX + contentWidth - space_advance(kFontScale))
                state.scrollX = caretOffsetX - contentWidth + space_advance(kFontScale);
        }

        scrollState.scrollY = source_editor_clamped_scroll(
            scrollState.scrollY,
            editorContentHeight,
            height);

        float maximumLineWidth{};
        std::size_t measuredOffset{};
        for (std::size_t line = 0u; line < lineCount; ++line)
        {
            const std::size_t lineStart = measuredOffset;
            const std::string_view lineText = line_view_from_offset(text, measuredOffset);
            maximumLineWidth = (std::max)(maximumLineWidth,
                source_editor_x_for_index(text, lineStart, lineStart + lineText.size(), kFontScale));
        }
        const float maximumScrollX = (std::max)(0.0f,
            maximumLineWidth - contentWidth + kBoxInnerPadding);
        state.scrollX = source_editor_clamped_scroll(
            state.scrollX,
            maximumLineWidth + kBoxInnerPadding,
            contentWidth);
        const bool horizontalWheel = std::any_of(
            g_frame.events.begin(), g_frame.events.end(),
            [](const InputEvent& event)
            {
                return event.type == EventType::MouseWheel && event.shift_down;
            });
        if (hoveredEditor && horizontalWheel && g_frame.mouseWheelDelta != 0)
        {
            const float steps = static_cast<float>(g_frame.mouseWheelDelta) / 120.0f;
            state.scrollX = (std::clamp)(state.scrollX
                - steps * space_advance(kFontScale) * 6.0f,
                0.0f,
                maximumScrollX);
            g_frame.mouseWheelDelta = 0;
        }

        draw_sprite(active ? palette.textFieldActive : palette.textField, pos.x, pos.y, editorWidth, height);

        const auto scroll = begin_scroll_area(ScrollAreaOptions{
            .id = scrollId,
            .size = { width, height },
            .content_height = editorContentHeight,
            .draw_background = false,
            .show_scrollbar = true
        });

        const float textX = g_frame.cursor.x + kBoxInnerPadding - state.scrollX;
        const float viewportTextY = pos.y + kBoxInnerPadding;
        const float firstTextY = g_frame.cursor.y + kBoxInnerPadding;
        const std::size_t firstVisibleLine = static_cast<std::size_t>((std::max)(0.0f, scroll.scroll_y) / (std::max)(1.0f, lineAdvance));
        const std::size_t visibleLineBudget = static_cast<std::size_t>(height / (std::max)(1.0f, lineAdvance)) + 4u;

        if (active && left_press_available() && hoveredEditor)
        {
            const std::size_t cursor = source_editor_index_from_point(
                text,
                g_frame.mousePos.x,
                g_frame.mousePos.y,
                textX,
                viewportTextY,
                scroll.scroll_y,
                lineAdvance,
                kFontScale);
            source_editor_set_cursor(state, cursor, false, text.size());
            state.draggingSelection = true;
            consume_left_press();
        }
        else if (active && state.draggingSelection && g_frame.mouseDown)
        {
            const std::size_t cursor = source_editor_index_from_point(
                text,
                g_frame.mousePos.x,
                g_frame.mousePos.y,
                textX,
                viewportTextY,
                scroll.scroll_y,
                lineAdvance,
                kFontScale);
            source_editor_set_cursor(state, cursor, true, text.size());
        }

        if (!g_frame.mouseDown)
            state.draggingSelection = false;

        std::size_t offset = line_start_for_index(text, firstVisibleLine);
        float lineY = firstTextY + static_cast<float>(firstVisibleLine) * lineAdvance;
        const auto [selectionBegin, selectionEnd] = source_editor_selection_range(state, text.size());
        for (std::size_t line = 0u; line < visibleLineBudget && firstVisibleLine + line < lineCount; ++line)
        {
            const std::size_t lineStart = offset;
            const std::string_view lineText = line_view_from_offset(text, offset);
            const std::size_t lineEnd = lineStart + lineText.size();

            if (selectionEnd > selectionBegin && selectionEnd >= lineStart && selectionBegin <= lineEnd)
            {
                const std::size_t highlightBegin = (std::max)(selectionBegin, lineStart);
                const std::size_t highlightEnd = (std::min)(selectionEnd, lineEnd);
                const float highlightX = textX + source_editor_x_for_index(text, lineStart, highlightBegin, kFontScale);
                float highlightWidth = source_editor_x_for_index(text, lineStart, highlightEnd, kFontScale)
                    - source_editor_x_for_index(text, lineStart, highlightBegin, kFontScale);
                if (selectionEnd > lineEnd && highlightEnd == lineEnd)
                    highlightWidth += space_advance(kFontScale) * 0.75f;
                if (highlightWidth > 0.0f)
                    draw_sprite(palette.buttonActive, highlightX, lineY - 1.0f, highlightWidth, baseHeight + 2.0f);
            }

            draw_text_line(lineText, textX, lineY, kFontScale);
            lineY += lineAdvance;
        }

        if (active && !source_editor_has_selection(state, text.size()))
        {
            const std::size_t cursor = (std::min)(state.cursorIndex, text.size());
            std::size_t caretLineIndex = 0u;
            std::size_t caretLineStart = 0u;
            for (std::size_t i = 0u; i < cursor; ++i)
            {
                if (text[i] == '\n')
                {
                    ++caretLineIndex;
                    caretLineStart = i + 1u;
                }
            }

            const float caretX = textX
                + source_editor_x_for_index(text, caretLineStart, cursor, kFontScale);
            const float caretY = firstTextY + static_cast<float>(caretLineIndex) * lineAdvance;
            if (caretY + baseHeight >= pos.y && caretY <= pos.y + height)
                draw_caret(caretX, caretY, baseHeight);
        }

        g_frame.cursor = { pos.x, pos.y - scroll.scroll_y + editorContentHeight };
        end_scroll_area();

        result.horizontal_scroll = state.scrollX;
        result.vertical_scroll = scrollState.scrollY;
        const std::size_t resultCursor = (std::min)(state.cursorIndex, text.size());
        std::size_t resultLineStart{};
        for (std::size_t index = 0u; index < resultCursor; ++index)
            if (text[index] == '\n')
                resultLineStart = index + 1u;
        result.cursor_line = static_cast<std::size_t>(
            std::count(text.begin(), text.begin()
                + static_cast<std::ptrdiff_t>(resultCursor), '\n')) + 1u;
        result.cursor_column = 1u;
        for (std::size_t index = resultLineStart; index < resultCursor;)
        {
            index = next_utf8_codepoint_start(text, index);
            ++result.cursor_column;
        }

        bool openedThisFrame = false;
        if (options.show_context_menu && right_press_available() && hoveredEditor)
        {
            consume_right_press();
            state.contextMenuOpen = true;
            state.contextMenuPos = g_frame.mousePos;
            openedThisFrame = true;
        }

        if (state.contextMenuOpen)
        {
            const float rowHeight = 28.0f;
            const float menuPadding = 4.0f;
            const float menuWidth = 172.0f;
            const float menuHeight = menuPadding * 2.0f + rowHeight * 6.0f + kContentPadding * 5.0f;
            Vec2 menuPos = state.contextMenuPos;
            menuPos.x = (std::min)(menuPos.x, (std::max)(0.0f, g_frame.origin.x + g_frame.windowSize.x - menuWidth - kContentPadding));
            menuPos.y = (std::min)(menuPos.y, (std::max)(0.0f, g_frame.origin.y + g_frame.windowSize.y - menuHeight - kContentPadding));

            const bool hoveredMenu = point_in_rect(g_frame.mousePos, menuPos.x, menuPos.y, menuWidth, menuHeight);
            if (!openedThisFrame && (left_press_available() || right_press_available()) && !hoveredMenu)
                state.contextMenuOpen = false;

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
                const auto& palette = active_palette();
                draw_sprite(palette.windowBackground, menuPos.x, menuPos.y, menuWidth, menuHeight);
                draw_sprite(palette.titleBar, menuPos.x, menuPos.y, menuWidth, 2.0f);
                draw_sprite(palette.titleBar, menuPos.x, menuPos.y, 2.0f, menuHeight);
                draw_sprite(palette.panelBackground, menuPos.x + menuWidth - 2.0f, menuPos.y, 2.0f, menuHeight);
                draw_sprite(palette.panelBackground, menuPos.x, menuPos.y + menuHeight - 2.0f, menuWidth, 2.0f);

                g_frame.insideWindow = true;
                g_frame.windowKey = id + "-context-menu";
                g_frame.widgetSerial = 0;
                g_frame.origin = menuPos;
                g_frame.windowSize = { menuWidth, menuHeight };
                g_frame.contentMin = { menuPos.x + menuPadding, menuPos.y + menuPadding };
                g_frame.contentMax = { menuPos.x + menuWidth - menuPadding, menuPos.y + menuHeight - menuPadding };
                set_cursor(g_frame.contentMin);

                if (button_with_state(
                        "Undo",
                        { menuWidth - 2.0f * menuPadding, rowHeight },
                        false,
                        options.can_undo))
                {
                    result.undo_requested = true;
                    state.contextMenuOpen = false;
                }
                if (button_with_state(
                        "Redo",
                        { menuWidth - 2.0f * menuPadding, rowHeight },
                        false,
                        options.can_redo))
                {
                    result.redo_requested = true;
                    state.contextMenuOpen = false;
                }
                if (button("Select All", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    state.selectionAnchor = 0u;
                    state.cursorIndex = text.size();
                    state.hasSelection = !text.empty();
                    result.selected_all = state.hasSelection;
                    state.contextMenuOpen = false;
                }
                if (button("Copy", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    result.copied = clipboard_write_text(source_editor_selected_text(text, state));
                    state.contextMenuOpen = false;
                }
                if (button("Cut", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    result.cut = clipboard_write_text(source_editor_selected_text(text, state));
                    if (!options.read_only && result.cut && source_editor_has_selection(state, text.size()))
                    {
                        (void)source_editor_delete_selection(text, state);
                        result.edit.changed = true;
                    }
                    state.contextMenuOpen = false;
                }
                if (button("Paste", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    if (!options.read_only)
                    {
                        source_editor_insert_text_limited(
                            text,
                            state,
                            clipboard_read_text(),
                            options.max_chars,
                            result.edit.changed,
                            result.invalid_utf8_replacements);
                        result.pasted = true;
                    }
                    state.contextMenuOpen = false;
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

        return result;
    }

    std::string clipboard_text() noexcept
    {
        return clipboard_read_text();
    }

    bool set_clipboard_text(std::string_view text) noexcept
    {
        return clipboard_write_text(text);
    }

