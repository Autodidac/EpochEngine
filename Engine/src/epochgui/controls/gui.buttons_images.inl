// Epoch feature implementation; included by its owning module only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
    static bool button_with_state(
        std::string_view label,
        Vec2 size,
        bool selected,
        bool enabled = true,
        SemanticTone tone = SemanticTone::neutral,
        bool circular = false) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return false;

        const Vec2 pos = g_frame.cursor;
        const float baseHeight = base_line_height(kFontScale);
        const float minWidth = space_advance(kFontScale) + 2.0f * kContentPadding;
        const float width = (std::max)(static_cast<float>(size.x), minWidth);
        const float height = (std::max)(static_cast<float>(size.y), baseHeight + 2.0f * kBoxInnerPadding);

        const float centerX = pos.x + width * 0.5f;
        const float centerY = pos.y + height * 0.5f;
        const float radius = (std::min)(width, height) * 0.5f;
        const float dx = g_frame.mousePos.x - centerX;
        const float dy = g_frame.mousePos.y - centerY;
        const bool hovered = enabled
            && point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && (!circular || (dx * dx + dy * dy <= radius * radius))
            && point_in_active_clip(g_frame.mousePos);
        const std::size_t pressKey = widget_press_key(label, pos, { width, height });
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        if (hovered && left_press_available())
        {
            pressedKey = pressKey;
            consume_left_press();
        }

        const bool pressed = (g_frame.mouseDown || g_frame.justReleased) && pressedKey == pressKey;
        const bool clicked = left_release_available() && hovered && pressedKey == pressKey;
        if (g_frame.justReleased && pressedKey == pressKey)
        {
            if (hovered)
                consume_left_release();
            pressedKey = 0;
        }

        const auto& palette = active_palette();
        const bool semantic = enabled && tone != SemanticTone::neutral;

        if (circular)
        {
            const auto style = gui_lib::rounded_rect::RoundedRectStyle{
                .enabled = true, .control_radius = radius};
            if (semantic)
            {
                const auto& sprites = active_semantic_pair(tone);
                draw_rounded_sprite(sprites.accent, pos.x, pos.y, width, height, style);
                draw_rounded_sprite(sprites.background, pos.x + 2.0f, pos.y + 2.0f,
                    width - 4.0f, height - 4.0f, style);
            }
            else
            {
                const auto background = !enabled ? palette.buttonNormal
                    : selected || pressed ? palette.buttonActive
                    : hovered ? palette.buttonHover : palette.buttonNormal;
                draw_rounded_sprite(background, pos.x, pos.y, width, height, style);
            }
        }
        else if (semantic)
        {
            const auto& semanticSprites = active_semantic_pair(tone);
            draw_rounded_sprite(semanticSprites.background,
                pos.x, pos.y, width, height,
                gui_lib::rounded_rect::RoundedRectStyle{
                    .enabled = true,
                    .control_radius = (std::min)(6.0f, height * 0.25f)
                });
            // Keep the semantic accent inset so it cannot square off the
            // antialiased top-left and bottom-left rounded corners.
            draw_sprite(semanticSprites.accent,
                pos.x + 2.0f, pos.y + 3.0f,
                2.0f, (std::max)(0.0f, height - 6.0f));
            if (selected || pressed)
            {
                draw_sprite(semanticSprites.accent, pos.x, pos.y, width, 2.0f);
                draw_sprite(semanticSprites.accent, pos.x, pos.y + height - 2.0f, width, 2.0f);
            }
            else if (hovered)
            {
                draw_sprite(semanticSprites.accent, pos.x, pos.y + height - 2.0f, width, 2.0f);
            }
        }
        else
        {
            const SpriteHandle background = !enabled
                ? palette.textField
                : selected || pressed ? palette.buttonActive
                : hovered ? palette.buttonHover
                : palette.buttonNormal;
            // Compact Windows-like radii, not giant pill controls. Draw with
            // the shared antialiased nine-slice masks. Geometry and hit-tests
            // remain unchanged, including keyboard focus and press ownership.
            draw_rounded_sprite(background, pos.x, pos.y, width, height,
                gui_lib::rounded_rect::RoundedRectStyle{
                    .enabled = true,
                    .control_radius = (std::min)(6.0f, height * 0.25f)
                });
        }

        const std::string fittedLabel = fit_text_to_width(
            label,
            (std::max)(1.0f, width - 2.0f * kContentPadding - 2.0f),
            kFontScale);
        const std::string_view displayLabel = fittedLabel.empty()
            ? label
            : std::string_view{ fittedLabel };
        const float textWidth = measure_text_width(displayLabel, kFontScale) + 2.0f;
        const float textHeight = baseHeight;
        const float minTextX = pos.x + kContentPadding + kButtonTextClipInset;
        const float maxTextX = pos.x + width - kContentPadding - textWidth;
        const float centeredTextX = pos.x + (std::max)(0.0f, (width - textWidth) * 0.5f);
        // Circular buttons have no rectangular left-padding minimum. That
        // clamp shifted compact +/Stop glyphs to the right at 30 pixels.
        const float textX = circular ? centeredTextX
            : maxTextX > minTextX
                ? (std::clamp)(centeredTextX, minTextX, maxTextX)
                : minTextX;
        const float textY = pos.y
            + std::floor((std::max)(0.0f, (height - textHeight) * 0.5f))
            + (circular ? 0.0f : 1.0f);

        draw_text_line(displayLabel, textX, textY, kFontScale);

        g_frame.lastButtonBounds = WidgetBounds{ .position = pos, .size = { width, height } };
        advance_cursor({ 0.0f, height + kContentPadding });

        return clicked;
    }

    bool button(std::string_view label, Vec2 size) noexcept
    {
        return button_with_state(label, size, false);
    }

    bool button_selected(std::string_view label, Vec2 size, bool selected) noexcept
    {
        return button_with_state(label, size, selected);
    }

    struct MenuItemLayout
    {
        WidgetBounds row{}, indicator{}, label{}, shortcut{};
        bool valid{};
    };

    [[nodiscard]] static MenuItemLayout make_menu_item_layout(
        Vec2 position, Vec2 size, float textHeight,
        float measuredLabelWidth, float shortcutWidth) noexcept
    {
        MenuItemLayout layout{};
        if (!std::isfinite(position.x) || !std::isfinite(position.y)
            || !std::isfinite(size.x) || !std::isfinite(size.y)
            || !std::isfinite(textHeight) || !std::isfinite(measuredLabelWidth)
            || !std::isfinite(shortcutWidth)
            || size.x <= 0.0f || size.y <= 0.0f || textHeight <= 0.0f)
            return layout;
        layout.row = {position, size};
        const float padding = (std::min)(8.0f, size.x * 0.25f);
        const float innerWidth = (std::max)(0.0f, size.x - 2.0f * padding);
        const float gutter = (std::min)(22.0f, innerWidth);
        const float labelX = position.x + padding + gutter;
        const float available = (std::max)(0.0f, innerWidth - gutter);
        // Shortcut hints yield before the command label in a narrow viewport.
        const bool showShortcut = shortcutWidth > 0.0f
            && available >= measuredLabelWidth + shortcutWidth + 12.0f;
        const float hintWidth = showShortcut ? shortcutWidth : 0.0f;
        const float labelWidth = (std::max)(0.0f,
            available - (showShortcut ? hintWidth + 12.0f : 0.0f));
        const float textY = position.y + std::floor((size.y - textHeight) * 0.5f);
        const float indicatorExtent = (std::min)({12.0f, gutter, size.y});
        layout.indicator = {{position.x + padding + (gutter - indicatorExtent) * 0.5f,
            position.y + (size.y - indicatorExtent) * 0.5f},
            {indicatorExtent, indicatorExtent}};
        layout.label = {{labelX, textY}, {labelWidth, textHeight}};
        layout.shortcut = {{position.x + size.x - padding - hintWidth, textY},
            {hintWidth, textHeight}};
        layout.valid = true;
        return layout;
    }

    [[nodiscard]] static float preferred_menu_item_width(
        float labelWidth, float shortcutWidth) noexcept
    {
        // Allocation is in pixels; round the complete row up so subtraction of
        // gutters cannot leave a fractional shortfall and trigger ellipsis.
        return std::ceil(38.0f + labelWidth
            + (shortcutWidth > 0.0f ? 12.0f + shortcutWidth : 0.0f));
    }

    [[nodiscard]] static bool menu_item_geometry_contract() noexcept
    {
        for (const float width : {18.0f, 80.0f, 240.0f, 480.0f})
        {
            const auto layout = make_menu_item_layout({7.0f, 11.0f},
                {width, 30.0f}, 22.0f, 200.0f, 56.0f);
            if (!layout.valid || layout.label.size.x < 0.0f
                || layout.indicator.position.x < layout.row.position.x
                || layout.indicator.position.x + layout.indicator.size.x > 7.0f + width
                || layout.label.position.x + layout.label.size.x > 7.0f + width
                || layout.shortcut.position.x + layout.shortcut.size.x > 7.0f + width
                || (layout.shortcut.size.x > 0.0f
                    && layout.label.position.x + layout.label.size.x
                        >= layout.shortcut.position.x))
                return false;
        }
        for (const float shortcutWidth : {0.0f, 43.75f})
        {
            const auto width = preferred_menu_item_width(201.3f, shortcutWidth);
            const auto layout = make_menu_item_layout({}, {width, 30.0f},
                22.0f, 201.3f, shortcutWidth);
            if (!layout.valid || layout.label.size.x < 201.3f
                || layout.shortcut.size.x != shortcutWidth)
                return false;
        }
        const auto compressed = make_menu_item_layout({}, {220.0f, 30.0f},
            22.0f, 160.0f, 50.0f);
        return compressed.shortcut.size.x == 0.0f && compressed.label.size.x >= 160.0f
            && !make_menu_item_layout({}, {0.0f, 30.0f}, 22.0f, 200.0f, 56.0f).valid
            && !make_menu_item_layout({}, {240.0f, 30.0f},
                std::numeric_limits<float>::quiet_NaN(), 200.0f, 56.0f).valid
            && make_menu_item_layout({}, {80.0f, 30.0f}, 22.0f, 200.0f, 56.0f)
                .shortcut.size.x == 0.0f;
    }

    float menu_item_row_height() noexcept
    {
        try { ensure_resources(); }
        catch (...) { return 28.0f; }
        return (std::max)(24.0f, base_line_height(kFontScale) + 6.0f);
    }

    float menu_item_preferred_width(const MenuItemOptions& options) noexcept
    {
        try { ensure_resources(); }
        catch (...) { return 240.0f; }
        return preferred_menu_item_width(measure_text_width(options.label, kFontScale),
            measure_text_width(options.shortcut, kFontScale));
    }

    bool menu_item(const MenuItemOptions& options) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx)
            return false;
        const Vec2 position = g_frame.cursor;
        const float available = (std::max)(0.0f, content_right() - position.x);
        const float width = options.size.x > 0.0f
            ? (std::min)(available, options.size.x) : available;
        const float height = (std::max)(menu_item_row_height(), options.size.y);
        const auto layout = make_menu_item_layout(position, {width, height},
            base_line_height(kFontScale), measure_text_width(options.label, kFontScale),
            measure_text_width(options.shortcut, kFontScale));
        if (!layout.valid)
            return false;
        const bool hovered = point_in_rect(g_frame.mousePos,
                position.x, position.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        const auto key = widget_press_key(options.label, position, {width, height});
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        if (hovered && left_press_available())
        {
            pressedKey = options.enabled ? key : 0u;
            consume_left_press();
        }
        const bool pressed = options.enabled && pressedKey == key
            && (g_frame.mouseDown || g_frame.justReleased);
        const bool clicked = options.enabled && hovered
            && left_release_available() && pressedKey == key;
        if (hovered && left_release_available())
            consume_left_release();
        if (g_frame.justReleased && pressedKey == key)
            pressedKey = 0u;

        const auto& palette = active_palette();
        if (options.enabled && (hovered || pressed))
            draw_rounded_sprite(pressed ? palette.buttonActive : palette.buttonHover,
                position.x, position.y, width, height,
                gui_lib::rounded_rect::RoundedRectStyle{
                    .enabled = true, .control_radius = 3.0f});
        if (options.separator_before)
            draw_sprite(palette.titleBar, position.x + 4.0f, position.y,
                (std::max)(0.0f, width - 8.0f), 1.0f);

        const auto& muted = g_frame.activeTheme == ThemeVariant::DefaultLight
            ? g_resources.halfTextLightFont : g_resources.halfTextFont;
        const auto* previousFont = g_temporaryTextFont;
        if (!options.enabled && muted.asset)
            g_temporaryTextFont = &muted;
        {
            ContentClipScope rowClip(position, {position.x + width, position.y + height});
            const auto label = fit_text_to_width(options.label,
                layout.label.size.x, kFontScale);
            {
                ContentClipScope labelClip(layout.label.position,
                    {layout.label.position.x + layout.label.size.x, position.y + height});
                draw_text_line(label, layout.label.position.x,
                    layout.label.position.y, kFontScale);
            }
            if (layout.shortcut.size.x > 0.0f)
            {
                if (muted.asset) g_temporaryTextFont = &muted;
                draw_text_line(options.shortcut, layout.shortcut.position.x,
                    layout.shortcut.position.y, kFontScale);
            }
            if (options.indicator != MenuItemIndicator::None
                && layout.indicator.size.x >= 10.0f)
            {
                const auto& mark = active_semantic_pair(options.enabled
                    ? SemanticTone::info : SemanticTone::muted);
                const auto p = layout.indicator.position;
                if (options.indicator == MenuItemIndicator::Radio)
                {
                    draw_rounded_sprite(mark.accent, p.x, p.y, 12.0f, 12.0f,
                        {.enabled = true, .control_radius = 6.0f});
                    draw_rounded_sprite(palette.windowBackground, p.x + 1.0f, p.y + 1.0f,
                        10.0f, 10.0f, {.enabled = true, .control_radius = 5.0f});
                    if (options.checked)
                        draw_rounded_sprite(mark.accent, p.x + 3.0f, p.y + 3.0f,
                            6.0f, 6.0f, {.enabled = true, .control_radius = 3.0f});
                }
                else if (options.checked)
                {
                    for (int n = 0; n < 3; ++n)
                        draw_sprite(mark.accent, p.x + static_cast<float>(n),
                            p.y + 5.0f + static_cast<float>(n), 2.0f, 2.0f);
                    for (int n = 0; n < 7; ++n)
                        draw_sprite(mark.accent, p.x + 3.0f + static_cast<float>(n),
                            p.y + 7.0f - static_cast<float>(n), 2.0f, 2.0f);
                }
            }
        }
        g_temporaryTextFont = previousFont;
        g_frame.lastButtonBounds = layout.row;
        advance_cursor({0.0f, height});
        return clicked;
    }

    bool toggle_switch(std::string_view label, bool& value, Vec2 size) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx)
            return false;

        const Vec2 pos = g_frame.cursor;
        const float baseHeight = base_line_height(kFontScale);
        const float width = (std::max)(size.x, 96.0f);
        const float height = (std::max)(size.y, 28.0f);
        const float trackWidth = (std::min)(48.0f, height * 1.9f);
        const float trackHeight = (std::min)(24.0f, height - 4.0f);
        const float trackX = pos.x + width - trackWidth;
        const float trackY = pos.y + (height - trackHeight) * 0.5f;
        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        const std::size_t pressKey = widget_press_key(label, pos, { width, height });
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
        if (clicked)
            value = !value;

        const auto layout = gui_lib::make_toggle_switch_layout({
            .bounds = {
                { trackX, trackY },
                { trackWidth, trackHeight }
            },
            .value = value,
            .padding = 3.0f
        });
        const auto& palette = active_palette();
        const auto pillStyle = gui_lib::rounded_rect::RoundedRectStyle{
            .enabled = true,
            .control_radius = trackHeight * 0.5f,
            .segments_per_corner = 8U
        };
        const SpriteHandle track = value
            ? palette.buttonActive
            : hovered ? palette.buttonHover : palette.buttonNormal;
        draw_rounded_sprite(
            track,
            layout.track.position.x,
            layout.track.position.y,
            layout.track.size.x,
            layout.track.size.y,
            pillStyle);
        draw_rounded_sprite(
            value ? palette.buttonHover : palette.textFieldActive,
            layout.thumb.position.x,
            layout.thumb.position.y,
            layout.thumb.size.x,
            layout.thumb.size.y,
            gui_lib::rounded_rect::RoundedRectStyle{
                .enabled = true,
                .control_radius = layout.thumb.size.y * 0.5f,
                .segments_per_corner = 8U
            });

        const float labelWidth = (std::max)(1.0f, width - trackWidth - 10.0f);
        const std::string fittedLabel = fit_text_to_width(label, labelWidth, kFontScale);
        const std::string_view displayLabel = fittedLabel.empty()
            ? label
            : std::string_view{ fittedLabel };
        const float textY = pos.y + std::floor((std::max)(0.0f, (height - baseHeight) * 0.5f)) + 1.0f;
        draw_text_line(displayLabel, pos.x, textY, kFontScale);

        g_frame.lastButtonBounds = WidgetBounds{ .position = pos, .size = { width, height } };
        advance_cursor({ 0.0f, height + kContentPadding });
        return clicked;
    }

    bool text_link(std::string_view label, Vec2 size, bool selected) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx)
            return false;

        const Vec2 pos = g_frame.cursor;
        const float baseHeight = base_line_height(kFontScale);
        const float minWidth = space_advance(kFontScale) + 2.0f * kContentPadding;
        const float width = (std::max)(static_cast<float>(size.x), minWidth);
        const float height = (std::max)(static_cast<float>(size.y), baseHeight + 2.0f * kBoxInnerPadding);

        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        const std::size_t pressKey = widget_press_key(label, pos, { width, height });
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        if (hovered && left_press_available())
        {
            pressedKey = pressKey;
            consume_left_press();
        }

        const bool pressed = (g_frame.mouseDown || g_frame.justReleased) && pressedKey == pressKey;
        const bool clicked = left_release_available() && hovered && pressedKey == pressKey;
        if (g_frame.justReleased && pressedKey == pressKey)
        {
            if (hovered)
                consume_left_release();
            pressedKey = 0;
        }

        const auto& palette = active_palette();
        if (selected)
        {
            draw_sprite(palette.panelBackground, pos.x, pos.y, width, height);
            draw_sprite(palette.textFieldActive, pos.x, pos.y, 3.0f, height);
        }
        else if (hovered || pressed)
        {
            draw_sprite(palette.buttonHover, pos.x, pos.y, width, height);
        }

        const SpriteHandle accent = selected
            ? palette.textFieldActive
            : hovered || pressed ? palette.buttonHover
            : palette.titleBar;
        draw_sprite(accent, pos.x, pos.y + height - 2.0f, width, 2.0f);

        const std::string fittedLabel = fit_text_to_width(
            label,
            (std::max)(1.0f, width - 2.0f * kContentPadding - 2.0f),
            kFontScale);
        const std::string_view displayLabel = fittedLabel.empty()
            ? label
            : std::string_view{ fittedLabel };
        const float textY = pos.y + std::floor((std::max)(0.0f, (height - baseHeight) * 0.5f)) + 1.0f;
        draw_text_line(displayLabel, pos.x + kContentPadding, textY, kFontScale);

        g_frame.lastButtonBounds = WidgetBounds{ .position = pos, .size = { width, height } };
        advance_cursor({ 0.0f, height + kContentPadding });
        return clicked;
    }

    bool titlebar_close_button(Vec2 window_position, Vec2 window_size) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx || window_size.x < 44.0f || window_size.y < 24.0f)
            return false;

        const float width = 24.0f;
        const float height = 22.0f;
        const Vec2 pos{
            window_position.x + (std::max)(0.0f, window_size.x - width - kContentPadding),
            window_position.y + 5.0f
        };
        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_modal_input_capture(g_frame.mousePos);
        const std::size_t pressKey = widget_press_key("window-close", pos, { width, height });
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        if (hovered && left_press_available())
        {
            pressedKey = pressKey;
            consume_left_press();
        }

        const bool pressed = (g_frame.mouseDown || g_frame.justReleased) && pressedKey == pressKey;
        const bool clicked = left_release_available() && hovered && pressedKey == pressKey;
        if (g_frame.justReleased && pressedKey == pressKey)
        {
            if (hovered)
                consume_left_release();
            pressedKey = 0;
        }

        const auto& palette = active_palette();
        const SpriteHandle background =
            pressed ? palette.buttonActive
            : hovered ? palette.buttonHover
            : palette.buttonNormal;
        draw_sprite(background, pos.x, pos.y, width, height);
        if (hovered || pressed)
        {
            draw_sprite(palette.textFieldActive, pos.x, pos.y, width, 2.0f);
            draw_sprite(palette.textFieldActive, pos.x, pos.y, 2.0f, height);
        }

        draw_close_mark(pos, {width, height});
        g_frame.lastButtonBounds = WidgetBounds{ .position = pos, .size = { width, height } };
        return clicked;
    }

    ImageBoxResult image_box(const ImageBoxOptions& options) noexcept
    {
        ImageBoxResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx)
            return result;

        const Vec2 position = g_frame.cursor;
        const float width = (std::max)(options.size.x, 1.0f);
        const float height = (std::max)(options.size.y, 1.0f);
        const float captionHeight = options.caption.empty()
            ? 0.0f
            : base_line_height(kFontScale) + kBoxInnerPadding;
        const gui_lib::ImageBoxLayout layout = gui_lib::make_image_box_layout({
            .bounds = { to_lib(position), { width, height } },
            .source_extent = {
                options.source_size.x > 0.0f ? options.source_size.x : width,
                options.source_size.y > 0.0f ? options.source_size.y : height },
            .fit = options.fit == ImageFit::Stretch
                ? gui_lib::ImageFitMode::stretch
                : gui_lib::ImageFitMode::contain,
            .padding = kBoxInnerPadding,
            .caption_height = captionHeight
        });

        const bool hovered = options.enabled
            && point_in_rect(g_frame.mousePos, position.x, position.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        const std::string_view identity = !options.id.empty()
            ? options.id
            : (!options.caption.empty() ? options.caption : std::string_view{ "<image-box>" });
        const std::size_t pressKey = widget_press_key(identity, position, { width, height });
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        if (options.interactive && hovered && left_press_available())
        {
            pressedKey = pressKey;
            consume_left_press();
        }

        const bool pressed = options.interactive
            && (g_frame.mouseDown || g_frame.justReleased)
            && pressedKey == pressKey;
        const bool clicked = options.interactive
            && left_release_available()
            && hovered
            && pressedKey == pressKey;
        if (g_frame.justReleased && pressedKey == pressKey)
        {
            if (clicked)
                consume_left_release();
            pressedKey = 0;
        }

        const auto& palette = active_palette();
        const SpriteHandle background = !options.enabled
            ? palette.panelBackground
            : pressed ? palette.buttonActive
            : hovered ? palette.buttonHover
            : palette.buttonNormal;
        draw_sprite(background, position.x, position.y, width, height);

        if (layout.valid && options.sprite.is_valid())
        {
            draw_sprite(
                options.sprite,
                layout.content.position.x,
                layout.content.position.y,
                layout.content.size.x,
                layout.content.size.y);
        }

        if (options.selected)
        {
            draw_sprite(palette.textFieldActive, position.x, position.y, width, 2.0f);
            draw_sprite(palette.textFieldActive, position.x, position.y + height - 2.0f, width, 2.0f);
            draw_sprite(palette.textFieldActive, position.x, position.y, 2.0f, height);
            draw_sprite(palette.textFieldActive, position.x + width - 2.0f, position.y, 2.0f, height);
        }

        if (!options.caption.empty() && layout.caption.size.y > 0.0f)
        {
            const std::string fitted = fit_text_to_width(
                options.caption,
                (std::max)(1.0f, layout.caption.size.x - 2.0f * kButtonTextClipInset),
                kFontScale);
            const std::string_view display = fitted.empty()
                ? options.caption
                : std::string_view{ fitted };
            const float textY = layout.caption.position.y
                + std::floor((std::max)(
                    0.0f,
                    (layout.caption.size.y - base_line_height(kFontScale)) * 0.5f));
            draw_text_line(display, layout.caption.position.x, textY, kFontScale);
        }

        result.bounds = { .position = position, .size = { width, height } };
        result.image_bounds = {
            .position = from_lib(layout.content.position),
            .size = from_lib(layout.content.size) };
        result.hovered = hovered;
        result.clicked = clicked;
        result.valid = layout.valid && options.sprite.is_valid();
        g_frame.lastButtonBounds = result.bounds;
        advance_cursor({ 0.0f, height + kContentPadding });
        return result;
    }

    bool image_button(const ImageButtonOptions& options) noexcept
    {
        if (options.id.empty())
            return false;
        return image_box({
            .id = options.id,
            .sprite = options.sprite,
            .size = options.size,
            .source_size = options.source_size,
            .fit = options.fit,
            .interactive = true,
            .selected = options.selected,
            .enabled = options.enabled
        }).clicked;
    }

    bool image_button(const SpriteHandle& sprite, Vec2 size) noexcept
    {
        return image_box({
            .id = "<image-button>",
            .sprite = sprite,
            .size = size,
            .source_size = size,
            .fit = ImageFit::Contain,
            .interactive = true
        }).clicked;
    }

    AssetGridResult asset_grid(const AssetGridOptions& options) noexcept
    {
        AssetGridResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx || options.id.empty()
            || options.size.x < 1.0f || options.size.y < 1.0f
            || options.items.size() > gui_lib::asset_grid_maximum_items)
        {
            return result;
        }

        const Vec2 origin = g_frame.cursor;
        const float width = (std::max)(1.0f, options.size.x);
        const float height = (std::max)(48.0f, options.size.y);
        const bool hovered = options.enabled
            && point_in_rect(g_frame.mousePos, origin.x, origin.y, width, height)
            && point_in_active_clip(g_frame.mousePos);

        std::vector<const ContextMenuActionSpec*> contextActions{};
        const std::size_t contextActionInspectCount = (std::min)(
            options.context_actions.size(),
            asset_grid_maximum_context_actions);
        contextActions.reserve(contextActionInspectCount);
        result.context_actions_truncated =
            options.context_actions.size() > contextActionInspectCount;
        for (std::size_t index = 0u;
             index < contextActionInspectCount;
             ++index)
        {
            const ContextMenuActionSpec& action = options.context_actions[index];
            if (action.id.empty() || action.label.empty())
                continue;
            const bool duplicate = std::any_of(
                contextActions.begin(),
                contextActions.end(),
                [&](const ContextMenuActionSpec* accepted) noexcept
                {
                    return accepted->id == action.id;
                });
            if (duplicate)
                continue;
            contextActions.push_back(&action);
        }

        const std::string contextStateKey = asset_grid_state_key(options.id);
        auto& contextMenu = g_assetGridContextMenuStates[contextStateKey];
        if (contextActions.empty())
            contextMenu.open = false;
        const bool menuWasOpen = contextMenu.open;

        std::vector<gui_lib::AssetGridItem> libraryItems{};
        libraryItems.reserve(options.items.size());
        const auto libraryKind = [](AssetGridItemKind kind) noexcept
        {
            switch (kind)
            {
            case AssetGridItemKind::Folder: return gui_lib::AssetGridItemKind::folder;
            case AssetGridItemKind::Image: return gui_lib::AssetGridItemKind::image;
            case AssetGridItemKind::Texture: return gui_lib::AssetGridItemKind::texture;
            case AssetGridItemKind::Material: return gui_lib::AssetGridItemKind::material;
            case AssetGridItemKind::Model: return gui_lib::AssetGridItemKind::model;
            case AssetGridItemKind::Audio: return gui_lib::AssetGridItemKind::audio;
            case AssetGridItemKind::Scene: return gui_lib::AssetGridItemKind::scene;
            case AssetGridItemKind::Document: return gui_lib::AssetGridItemKind::document;
            case AssetGridItemKind::Generic:
            default: return gui_lib::AssetGridItemKind::generic;
            }
        };
        for (const AssetGridItem& item : options.items)
        {
            std::optional<gui_lib::AssetGridImageMetadata> imageMetadata{};
            if (item.image.is_valid())
            {
                imageMetadata = gui_lib::AssetGridImageMetadata{
                    .content_key = item.image.pack(),
                    .pixel_width = static_cast<std::uint32_t>((std::max)(1.0f, item.source_size.x)),
                    .pixel_height = static_cast<std::uint32_t>((std::max)(1.0f, item.source_size.y)),
                    .fit = gui_lib::ImageFitMode::contain,
                    .has_alpha = true
                };
            }
            libraryItems.push_back(gui_lib::AssetGridItem{
                .id = { item.id },
                .label = item.label,
                .detail = item.detail,
                .search_terms = item.search_terms,
                .kind = libraryKind(item.kind),
                .activation = gui_lib::AssetGridActivationRole::open_in_tab,
                .image = imageMetadata,
                .enabled = options.enabled && item.enabled
            });
        }

        float requestedScroll = std::isfinite(options.scroll_offset)
            ? (std::max)(0.0f, options.scroll_offset)
            : 0.0f;
        if (hovered && !menuWasOpen && g_frame.mouseWheelDelta != 0)
        {
            const float wheelSteps = static_cast<float>(g_frame.mouseWheelDelta) / 120.0f;
            requestedScroll = (std::max)(0.0f, requestedScroll - wheelSteps * 108.0f);
            g_frame.mouseWheelDelta = 0;
            result.wheel_scrolled = true;
        }

        gui_lib::AssetGridState state{};
        if (options.selected_id.has_value())
            state.selected_id = gui_lib::AssetGridItemId{ *options.selected_id };
        const gui_lib::AssetGridLayoutOptions layoutOptions{
            .viewport = { to_lib(origin), { width, height } },
            .tile_extent = to_lib(options.tile_size),
            .gap = to_lib(options.gap),
            .padding = to_lib(options.padding),
            .image_height = options.image_height,
            .label_height = 24.0f,
            .detail_height = 18.0f,
            .scroll_offset = requestedScroll,
            .maximum_visible_tiles = gui_lib::asset_grid_maximum_visible_tiles,
            .clear_selection_on_empty_press = options.clear_selection_on_empty_press
        };

        const std::size_t pressKey = widget_press_key(options.id, origin, { width, height });
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        const bool pointerPressed = hovered && !menuWasOpen && left_press_available();
        if (pointerPressed)
        {
            pressedKey = pressKey;
            consume_left_press();
        }
        const bool pointerActivated = hovered
            && !menuWasOpen
            && left_release_available()
            && pressedKey == pressKey;
        const bool contextRequested = hovered
            && !contextActions.empty()
            && right_press_available();

        const gui_lib::AssetGridUpdateResult update = gui_lib::update_asset_grid(
            state,
            libraryItems,
            gui_lib::AssetGridFilterOptions{
                .query = options.query,
                .maximum_results = gui_lib::asset_grid_maximum_items,
                .include_disabled = true
            },
            layoutOptions,
            gui_lib::AssetGridInput{
                .pointer_position = to_lib(g_frame.mousePos),
                .pointer_pressed = pointerPressed,
                .pointer_activated = pointerActivated,
                .context_requested = contextRequested,
                .pointer_present = hovered
            });
        if (g_frame.justReleased && pressedKey == pressKey)
        {
            if (pointerActivated)
                consume_left_release();
            pressedKey = 0;
        }

        bool menuOpenedThisFrame = false;
        if (contextRequested && update.context_request_valid
            && update.context_requested_id.has_value())
        {
            contextMenu.position = g_frame.mousePos;
            contextMenu.scrollOffset = 0.0f;
            contextMenu.targetId = update.context_requested_id->value;
            contextMenu.open = true;
            menuOpenedThisFrame = true;
            consume_right_press();
        }

        if (contextMenu.open)
        {
            const bool targetVisible = std::any_of(
                update.view.layout.visible_tiles.begin(),
                update.view.layout.visible_tiles.end(),
                [&](const gui_lib::AssetGridTileLayout& tile) noexcept
                {
                    return tile.enabled && tile.id.value == contextMenu.targetId;
                });
            if (!targetVisible)
                contextMenu.open = false;
        }

        const auto& palette = active_palette();
        draw_sprite(palette.consoleBackground, origin.x, origin.y, width, height);
        const auto kindLabel = [](AssetGridItemKind kind) noexcept -> std::string_view
        {
            switch (kind)
            {
            case AssetGridItemKind::Folder: return "FOLDER";
            case AssetGridItemKind::Image: return "IMAGE";
            case AssetGridItemKind::Texture: return "TEXTURE";
            case AssetGridItemKind::Material: return "MATERIAL";
            case AssetGridItemKind::Model: return "MODEL";
            case AssetGridItemKind::Audio: return "AUDIO";
            case AssetGridItemKind::Scene: return "SCENE";
            case AssetGridItemKind::Document: return "DOCUMENT";
            case AssetGridItemKind::Generic:
            default: return "ASSET";
            }
        };

        for (const gui_lib::AssetGridTileLayout& tile : update.view.layout.visible_tiles)
        {
            if (tile.source_index >= options.items.size())
                continue;
            const AssetGridItem& item = options.items[tile.source_index];
            const SpriteHandle tileBackground = !item.enabled
                ? palette.panelBackground
                : tile.selected ? palette.buttonActive
                : tile.hovered ? palette.buttonHover
                : palette.buttonNormal;
            draw_sprite(tileBackground, tile.tile.position.x, tile.tile.position.y,
                tile.tile.size.x, tile.tile.size.y);

            if (item.image.is_valid() && tile.image.valid)
            {
                draw_sprite(item.image, tile.image.content.position.x,
                    tile.image.content.position.y, tile.image.content.size.x,
                    tile.image.content.size.y);
            }
            else if (tile.image.frame.size.y > 0.0f)
            {
                draw_sprite(palette.panelBackground, tile.image.frame.position.x,
                    tile.image.frame.position.y, tile.image.frame.size.x,
                    tile.image.frame.size.y);
                const std::string_view placeholder = kindLabel(item.kind);
                const float placeholderWidth = measure_text_width(placeholder, kFontScale);
                draw_text_line(placeholder,
                    tile.image.frame.position.x
                        + (std::max)(0.0f, (tile.image.frame.size.x - placeholderWidth) * 0.5f),
                    tile.image.frame.position.y
                        + (std::max)(0.0f, (tile.image.frame.size.y - base_line_height(kFontScale)) * 0.5f),
                    kFontScale);
            }

            const std::string fittedLabel = fit_text_to_width(item.label,
                (std::max)(1.0f, tile.label.size.x - 2.0f * kButtonTextClipInset),
                kFontScale);
            const std::string_view displayLabel = fittedLabel.empty()
                ? item.label
                : std::string_view{ fittedLabel };
            draw_text_line(displayLabel, tile.label.position.x + kButtonTextClipInset,
                tile.label.position.y
                    + (std::max)(0.0f, (tile.label.size.y - base_line_height(kFontScale)) * 0.5f),
                kFontScale);

            if (!item.detail.empty() && tile.detail.size.y > 0.0f)
            {
                const std::string fittedDetail = fit_text_to_width(item.detail,
                    (std::max)(1.0f, tile.detail.size.x - 2.0f * kButtonTextClipInset),
                    kFontScale);
                const std::string_view displayDetail = fittedDetail.empty()
                    ? item.detail
                    : std::string_view{ fittedDetail };
                draw_text_line(displayDetail,
                    tile.detail.position.x + kButtonTextClipInset,
                    tile.detail.position.y, kFontScale);
            }
            if (tile.selected)
            {
                draw_sprite(palette.textFieldActive,
                    tile.selection_indicator.position.x,
                    tile.selection_indicator.position.y,
                    tile.selection_indicator.size.x,
                    tile.selection_indicator.size.y);
            }
        }

        if (contextMenu.open)
        {
            constexpr float menuPadding = 4.0f;
            constexpr float rowHeight = 28.0f;
            constexpr float rowGap = 2.0f;
            constexpr float minimumMenuWidth = 144.0f;
            constexpr float maximumMenuWidth = 280.0f;
            constexpr float scrollbarWidth = 10.0f;

            float measuredMenuWidth = minimumMenuWidth;
            for (const ContextMenuActionSpec* action : contextActions)
            {
                measuredMenuWidth = (std::max)(
                    measuredMenuWidth,
                    measure_text_width(action->label, kFontScale)
                        + 2.0f * kContentPadding);
            }
            measuredMenuWidth = (std::min)(measuredMenuWidth, maximumMenuWidth);

            const Vec2 hostOrigin = g_frame.origin;
            const Vec2 hostSize = g_frame.windowSize;
            const float availableHostWidth = (std::max)(
                48.0f, hostSize.x - 2.0f * menuPadding);
            const float availableHostHeight = (std::max)(
                48.0f, hostSize.y - 2.0f * menuPadding);
            const float menuWidth = (std::min)(
                measuredMenuWidth, availableHostWidth);
            const float menuContentHeight =
                static_cast<float>(contextActions.size()) * rowHeight
                + static_cast<float>(
                    contextActions.empty() ? 0u : contextActions.size() - 1u)
                    * rowGap;
            const float desiredMenuHeight =
                menuContentHeight + 2.0f * menuPadding;
            const float menuHeight = (std::min)(
                desiredMenuHeight, availableHostHeight);
            const float maximumScroll = (std::max)(
                0.0f, desiredMenuHeight - menuHeight);
            contextMenu.scrollOffset = (std::clamp)(
                contextMenu.scrollOffset, 0.0f, maximumScroll);

            Vec2 menuPosition = contextMenu.position;
            menuPosition.x = (std::clamp)(
                menuPosition.x,
                hostOrigin.x + menuPadding,
                (std::max)(
                    hostOrigin.x + menuPadding,
                    hostOrigin.x + hostSize.x - menuWidth - menuPadding));
            menuPosition.y = (std::clamp)(
                menuPosition.y,
                hostOrigin.y + menuPadding,
                (std::max)(
                    hostOrigin.y + menuPadding,
                    hostOrigin.y + hostSize.y - menuHeight - menuPadding));

            const bool hoveredMenu = point_in_rect(
                g_frame.mousePos,
                menuPosition.x,
                menuPosition.y,
                menuWidth,
                menuHeight);
            if (hoveredMenu && g_frame.mouseWheelDelta != 0)
            {
                const float wheelSteps =
                    static_cast<float>(g_frame.mouseWheelDelta) / 120.0f;
                contextMenu.scrollOffset = (std::clamp)(
                    contextMenu.scrollOffset - wheelSteps * rowHeight * 2.0f,
                    0.0f,
                    maximumScroll);
                g_frame.mouseWheelDelta = 0;
            }

            if (!menuOpenedThisFrame && !hoveredMenu
                && left_press_available())
            {
                contextMenu.open = false;
                consume_left_press();
            }
            else if (!menuOpenedThisFrame && !hoveredMenu
                && right_press_available())
            {
                contextMenu.open = false;
                consume_right_press();
            }
            if (contextMenu.open && right_release_available())
                consume_right_release();

            if (contextMenu.open)
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
                draw_sprite(
                    palette.windowBackground,
                    menuPosition.x,
                    menuPosition.y,
                    menuWidth,
                    menuHeight);
                draw_sprite(
                    palette.titleBar,
                    menuPosition.x,
                    menuPosition.y,
                    menuWidth,
                    2.0f);

                g_frame.insideWindow = true;
                g_frame.windowKey = contextStateKey;
                g_frame.widgetSerial = 0;
                g_frame.origin = menuPosition;
                g_frame.windowSize = { menuWidth, menuHeight };
                g_frame.contentMin = {
                    menuPosition.x + menuPadding,
                    menuPosition.y + menuPadding
                };
                g_frame.contentMax = {
                    menuPosition.x + menuWidth - menuPadding
                        - (maximumScroll > 0.0f ? scrollbarWidth : 0.0f),
                    menuPosition.y + menuHeight - menuPadding
                };

                const float actionWidth = (std::max)(
                    1.0f, g_frame.contentMax.x - g_frame.contentMin.x);
                for (std::size_t index = 0u;
                     index < contextActions.size();
                     ++index)
                {
                    const ContextMenuActionSpec& action =
                        *contextActions[index];
                    set_cursor({
                        g_frame.contentMin.x,
                        g_frame.contentMin.y
                            + static_cast<float>(index) * (rowHeight + rowGap)
                            - contextMenu.scrollOffset
                    });
                    if (button_with_state(
                            action.label,
                            { actionWidth, rowHeight },
                            false,
                            action.enabled))
                    {
                        result.context_target_id = contextMenu.targetId;
                        result.selected_action_id = std::string(action.id);
                        contextMenu.open = false;
                        break;
                    }
                }

                if (maximumScroll > 0.0f)
                {
                    const float trackX =
                        menuPosition.x + menuWidth - scrollbarWidth;
                    const float visibleRatio = menuHeight / desiredMenuHeight;
                    const float thumbHeight = (std::max)(
                        18.0f, menuHeight * visibleRatio);
                    const float scrollRatio = maximumScroll > 0.0f
                        ? contextMenu.scrollOffset / maximumScroll
                        : 0.0f;
                    const float thumbY = menuPosition.y
                        + (menuHeight - thumbHeight) * scrollRatio;
                    draw_sprite(
                        palette.textField,
                        trackX,
                        menuPosition.y,
                        scrollbarWidth,
                        menuHeight);
                    draw_sprite(
                        palette.buttonActive,
                        trackX,
                        thumbY,
                        scrollbarWidth,
                        thumbHeight);
                }

                if (hoveredMenu)
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

        result.bounds = { .position = origin, .size = { width, height } };
        if (update.selected_id.has_value())
            result.selected_id = update.selected_id->value;
        if (update.activated_id.has_value())
            result.activated_id = update.activated_id->value;
        if (contextMenu.open)
        {
            result.context_target_id = contextMenu.targetId;
            result.context_menu_open = true;
        }
        result.matched_count = update.view.filter.matched_count;
        result.visible_count = update.view.layout.visible_tiles.size();
        result.visible_ids.reserve(update.view.layout.visible_tiles.size());
        for (const gui_lib::AssetGridTileLayout& tile :
             update.view.layout.visible_tiles)
        {
            result.visible_ids.push_back(tile.id.value);
        }
        result.content_height = update.view.layout.content_extent.y;
        result.scroll_offset = update.view.layout.scroll_offset;
        result.selection_changed = update.selection_changed;
        result.truncated = update.view.filter.source_truncated
            || update.view.filter.results_truncated
            || update.view.layout.items_truncated
            || update.view.layout.visible_tiles_truncated;
        result.valid = update.view.layout.valid;
        g_frame.lastButtonBounds = result.bounds;
        advance_cursor({ 0.0f, height + kContentPadding });
        return result;
    }

