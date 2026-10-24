// Epoch feature implementation; included by its owning module only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
    std::optional<std::size_t> segmented_button_row(
        std::span<const SegmentedButtonSpec> items,
        float height,
        float gap) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx || items.empty())
            return std::nullopt;

        const Vec2 rowStart = g_frame.cursor;
        std::optional<std::size_t> clicked{};
        static thread_local std::vector<float> itemWidths;
        itemWidths.clear();
        itemWidths.reserve(items.size());
        for (const auto& item : items)
            itemWidths.push_back(item.width);

        const gui_lib::SegmentedControlLayoutOptions layoutOptions{
            .position = to_lib(rowStart),
            .item_widths = std::span<const float>{ itemWidths.data(), itemWidths.size() },
            .height = height,
            .gap = gap
        };
        const auto layout = gui_lib::make_segmented_control_layout(layoutOptions);

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const auto& item = items[i];
            const auto itemLayout = gui_lib::segmented_control_item_layout(
                layoutOptions,
                static_cast<std::uint32_t>(i));
            set_cursor(from_lib(itemLayout.position));
            if (button_with_state(item.label, from_lib(itemLayout.size), item.active))
                clicked = i;
        }

        set_cursor(rowStart);
        advance_cursor({ 0.0f, layout.height + kContentPadding });
        return clicked;
    }

    std::optional<std::size_t> tab_bar(
        std::span<const SegmentedButtonSpec> tabs,
        float height,
        float gap) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx || tabs.empty())
            return std::nullopt;

        const Vec2 rowStart = g_frame.cursor;
        const auto& palette = active_palette();
        std::optional<std::size_t> clicked{};
        float x = rowStart.x;
        float totalWidth = 0.0f;

        for (const auto& tab : tabs)
            totalWidth += (std::max)(1.0f, tab.width) + gap;
        totalWidth = (std::max)(0.0f, totalWidth - gap);

        const float h = (std::max)(height, base_line_height(kFontScale) + 2.0f * kBoxInnerPadding);
        draw_sprite(palette.titleBar,
            rowStart.x,
            rowStart.y + h - 2.0f,
            totalWidth,
            2.0f);

        for (std::size_t i = 0; i < tabs.size(); ++i)
        {
            const auto& tab = tabs[i];
            const float width = (std::max)(1.0f, tab.width);
            const bool hovered = point_in_rect(g_frame.mousePos, x, rowStart.y, width, h)
                && point_in_active_clip(g_frame.mousePos);
            const std::size_t pressKey = widget_press_key(tab.label, { x, rowStart.y }, { width, h });
            auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
            if (hovered && left_press_available())
            {
                pressedKey = pressKey;
                consume_left_press();
            }

            const bool pressed = (g_frame.mouseDown || g_frame.justReleased) && pressedKey == pressKey;
            if (left_release_available() && hovered && pressedKey == pressKey)
            {
                clicked = i;
                consume_left_release();
            }
            if (g_frame.justReleased && pressedKey == pressKey)
                pressedKey = 0;

            const SpriteHandle background =
                tab.active || pressed ? palette.buttonActive
                : hovered ? palette.buttonHover
                : palette.buttonNormal;
            draw_rounded_sprite(background, x, rowStart.y, width, h,
                gui_lib::rounded_rect::RoundedRectStyle{
                    .enabled = true,
                    .control_radius = (std::min)(5.0f, h * 0.25f)
                });

            if (tab.active)
            {
                draw_sprite(palette.panelBackground, x, rowStart.y + h - 2.0f, width, 3.0f);
                draw_sprite(palette.textFieldActive, x, rowStart.y + h - 3.0f, width, 2.0f);
            }

            const std::string fittedLabel = fit_text_to_width(
                tab.label,
                (std::max)(1.0f, width - 2.0f * kContentPadding - 2.0f),
                kFontScale);
            const std::string_view displayLabel = fittedLabel.empty()
                ? tab.label
                : std::string_view{ fittedLabel };
            const float textWidth = measure_text_width(displayLabel, kFontScale) + 2.0f;
            const float textHeight = base_line_height(kFontScale);
            const float minTextX = x + kContentPadding + kButtonTextClipInset;
            const float maxTextX = x + width - kContentPadding - textWidth;
            const float centeredTextX = x + (std::max)(0.0f, (width - textWidth) * 0.5f);
            const float textX = maxTextX > minTextX
                ? (std::clamp)(centeredTextX, minTextX, maxTextX)
                : minTextX;
            const float textY = rowStart.y + std::floor((std::max)(0.0f, (h - textHeight) * 0.5f)) + 1.0f;
            draw_text_line(displayLabel, textX, textY, kFontScale);

            x += width + gap;
        }

        set_cursor(rowStart);
        advance_cursor({ 0.0f, h + kContentPadding });
        return clicked;
    }

    [[nodiscard]] static std::optional<std::size_t> navigate_tab_keys(
        std::span<const TabButtonSpec> tabs, Vec2 stripPosition, Vec2 stripSize,
        Vec2 keyboardRegionSize, bool globalControlTab) noexcept
    {
        if (g_frame.modalInput.keyboard_captured() || any_select_box_open(g_frame.ctx) || !g_frame.ctx)
            return {};
        // Idle strips have no navigation to authorize. Check queued input
        // before asking the native window server for fresh focus ownership.
        const bool hasTabEvent = std::ranges::any_of(g_frame.events, [](const InputEvent& event) noexcept
        {
            return event.type == EventType::KeyDown && event.key == 9 && !event.alt_down;
        });
        if (!hasTabEvent || !g_frame.ctx->has_input_focus_safe()) return {};
        const auto contains = [&](Vec2 size) noexcept
        {
            return g_frame.mousePos.x >= stripPosition.x
                && g_frame.mousePos.y >= stripPosition.y
                && g_frame.mousePos.x < stripPosition.x + size.x
                && g_frame.mousePos.y < stripPosition.y + size.y;
        };
        for (auto event = g_frame.events.begin(); event != g_frame.events.end(); ++event)
        {
            if (event->type != EventType::KeyDown || event->key != 9 || event->alt_down)
                continue;
            if (!event->ctrl_down && keyboard_input_captured()) continue;
            // Plain Tab belongs to controls/scene input away from the tab strip.
            // Ctrl+Tab addresses the containing group, including overflow tabs.
            if (event->ctrl_down ? !globalControlTab && !contains(keyboardRegionSize)
                : !contains(stripSize)) continue;
            std::vector<std::uint8_t> enabled{};
            std::size_t active{};
            for (std::size_t index = 0u; index < tabs.size(); ++index)
            {
                enabled.push_back(tabs[index].enabled ? 1u : 0u);
                if (tabs[index].active) active = index;
            }
            const auto selected = gui_lib::navigate_responsive_tab_strip({
                .enabled = enabled, .active_index = active,
                .intent = event->shift_down ? gui_lib::ResponsiveTabNavigationIntent::previous
                    : gui_lib::ResponsiveTabNavigationIntent::next, .wrap = true});
            if (!selected || *selected == active) return {};
            g_frame.events.erase(event); // One group owns this GUI key event.
            return selected;
        }
        return {};
    }

    TabBarResult tab_bar_buttons(
        std::span<const TabButtonSpec> tabs,
        float height,
        float gap,
        TabBarPresentation presentation,
        Vec2 keyboard_region_size) noexcept
    {
        TabBarResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx || tabs.empty())
            return result;
        result.items.resize(tabs.size());

        const Vec2 rowStart = g_frame.cursor;
        const auto& palette = active_palette();
        static thread_local std::vector<float> widths;
        widths.clear();
        widths.reserve(tabs.size());
        const bool workbenchPresentation =
            presentation == TabBarPresentation::Workbench;
        for (const auto& tab : tabs)
        {
            const float measuredLabel = measure_text_width(
                tab.label, kFontScale);
            const float requestedWidth = workbenchPresentation
                ? gui_lib::resolve_responsive_tab_width({
                    .requested_width = tab.width,
                    .measured_label_width = measuredLabel,
                    .minimum_hit_width = 72.0f,
                    .horizontal_padding = 24.0f,
                    .close_extent = tab.closable ? 18.0f : 0.0f,
                    .dirty_extent = tab.dirty ? 8.0f : 0.0f})
                : tab.width > 0.0f
                    ? tab.width
                    : gui_lib::preferred_tool_tab_width(
                        measuredLabel,
                        tab.closable,
                        tab.dirty);
            widths.push_back((std::max)(1.0f, requestedWidth));
        }

        const float rowHeight = (std::max)(
            height,
            base_line_height(kFontScale) + 2.0f * kBoxInnerPadding);
        const gui_lib::TabButtonLayoutOptions layoutOptions{
            .strip = {
                .position = to_lib(rowStart),
                .item_widths = std::span<const float>{ widths.data(), widths.size() },
                .height = rowHeight,
                .gap = gap },
            .indicator_height = workbenchPresentation ? 2.0f : 3.0f,
            .close_extent = 18.0f,
            .label_padding = workbenchPresentation ? 12.0f : kContentPadding
        };
        const gui_lib::SegmentedControlLayout strip =
            gui_lib::make_segmented_control_layout(layoutOptions.strip);
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];

        if (workbenchPresentation && strip.valid)
        {
            draw_sprite(
                palette.panelBackground,
                strip.bounds.position.x,
                strip.bounds.position.y,
                strip.bounds.size.x,
                strip.bounds.size.y);
        }

        for (std::size_t index = 0; index < tabs.size(); ++index)
        {
            const TabButtonSpec& tab = tabs[index];
            const gui_lib::TabButtonLayout layout =
                gui_lib::make_tab_button_layout(
                    layoutOptions,
                    static_cast<std::uint32_t>(index),
                    tab.active,
                    tab.closable);
            if (!layout.valid)
                continue;

            result.items[index] = {
                .position = from_lib(layout.button.position),
                .size = from_lib(layout.button.size)
            };

            const Vec2 buttonPosition = from_lib(layout.button.position);
            const Vec2 buttonSize = from_lib(layout.button.size);
            const Vec2 closePosition = from_lib(layout.close_button.position);
            const Vec2 closeSize = from_lib(layout.close_button.size);
            const bool closeHovered = tab.enabled
                && tab.closable
                && point_in_rect(
                    g_frame.mousePos,
                    closePosition.x,
                    closePosition.y,
                    closeSize.x,
                    closeSize.y)
                && point_in_active_clip(g_frame.mousePos);
            const bool buttonHovered = tab.enabled
                && point_in_rect(
                    g_frame.mousePos,
                    buttonPosition.x,
                    buttonPosition.y,
                    buttonSize.x,
                    buttonSize.y)
                && point_in_active_clip(g_frame.mousePos)
                && !closeHovered;

            const std::string_view identity = !tab.id.empty() ? tab.id : tab.label;
            const std::size_t buttonKey =
                widget_press_key(identity, buttonPosition, buttonSize);
            std::string closeIdentity{ identity };
            closeIdentity.append(".close");
            const std::size_t closeKey =
                widget_press_key(closeIdentity, closePosition, closeSize);

            if (closeHovered && left_press_available())
            {
                pressedKey = closeKey;
                consume_left_press();
            }
            else if (buttonHovered && left_press_available())
            {
                pressedKey = buttonKey;
                result.pressed_index = index;
                consume_left_press();
            }

            const bool buttonPressed =
                (g_frame.mouseDown || g_frame.justReleased)
                && pressedKey == buttonKey;
            const bool closePressed =
                (g_frame.mouseDown || g_frame.justReleased)
                && pressedKey == closeKey;
            const bool buttonClicked = left_release_available()
                && buttonHovered
                && pressedKey == buttonKey;
            const bool closeClicked = left_release_available()
                && closeHovered
                && pressedKey == closeKey;
            if (g_frame.justReleased
                && (pressedKey == buttonKey || pressedKey == closeKey))
            {
                if (buttonClicked || closeClicked)
                    consume_left_release();
                pressedKey = 0;
            }

            const SpriteHandle background = !tab.enabled
                ? palette.panelBackground
                : tab.active || buttonPressed ? palette.buttonActive
                : buttonHovered ? palette.buttonHover
                : workbenchPresentation ? palette.panelBackground
                : palette.buttonNormal;
            if (!workbenchPresentation || tab.active || buttonPressed || buttonHovered)
            {
                const SpriteHandle tabFill = (background == palette.panelBackground)
                    ? palette.buttonNormal : background;
                draw_rounded_sprite(tabFill,
                    buttonPosition.x, buttonPosition.y,
                    buttonSize.x, buttonSize.y,
                    gui_lib::rounded_rect::RoundedRectStyle{
                        .enabled = true,
                        .control_radius = (std::min)(5.0f, buttonSize.y * 0.25f)
                    });
            }
            const float dirtyReservation = tab.dirty ? 8.0f : 0.0f;
            const float availableTextWidth = (std::max)(
                1.0f,
                layout.label.size.x - dirtyReservation);
            const std::string fitted = fit_text_to_width(
                tab.label,
                availableTextWidth,
                kFontScale);
            const std::string_view display =
                fitted.empty() ? tab.label : std::string_view{ fitted };
            const float textY = layout.label.position.y
                + std::floor((std::max)(
                    0.0f,
                    (layout.label.size.y - base_line_height(kFontScale)) * 0.5f));
            const float textX = workbenchPresentation
                ? layout.label.position.x
                    + (std::max)(
                        0.0f,
                        (layout.label.size.x
                            - measure_text_width(display, kFontScale)) * 0.5f)
                : layout.label.position.x;
            draw_text_line(
                display,
                textX,
                textY,
                kFontScale);

            if (workbenchPresentation && tab.active)
            {
                draw_sprite(
                    palette.textFieldActive,
                    layout.indicator.position.x,
                    layout.indicator.position.y,
                    layout.indicator.size.x,
                    layout.indicator.size.y);
            }

            if (tab.dirty)
            {
                draw_sprite(
                    palette.textFieldActive,
                    layout.label.position.x + availableTextWidth + 2.0f,
                    layout.label.position.y + layout.label.size.y * 0.5f - 2.0f,
                    4.0f,
                    4.0f);
            }

            if (tab.closable)
            {
                const SpriteHandle closeBackground = closePressed
                    ? palette.buttonActive
                    : closeHovered ? palette.textFieldActive
                    : background;
                // Small X control follows the same rounded surface as its tab;
                // do not change its distinct hit target or close behavior.
                draw_rounded_sprite(
                    closeBackground == palette.panelBackground
                        ? palette.buttonNormal : closeBackground,
                    closePosition.x, closePosition.y,
                    closeSize.x, closeSize.y,
                    gui_lib::rounded_rect::RoundedRectStyle{
                        .enabled = true,
                        .control_radius = (std::min)(4.0f, closeSize.y * 0.25f)
                    });
                draw_close_mark(closePosition, closeSize);
            }

            if (buttonClicked)
                result.selected_index = index;
            if (closeClicked)
                result.closed_index = index;
        }

        if (!result.selected_index && !result.closed_index)
        {
            const Vec2 stripSize{content_available_width(rowStart.x), strip.height};
            if (keyboard_region_size.x <= 0.0f || keyboard_region_size.y <= 0.0f)
                keyboard_region_size = stripSize;
            result.selected_index = navigate_tab_keys(tabs, rowStart, stripSize,
                keyboard_region_size, false);
        }
        set_cursor(rowStart);
        advance_cursor({ 0.0f, strip.height + kContentPadding });
        return result;
    }

    [[nodiscard]] static std::vector<float> resolve_inline_button_widths(
        std::span<const InlineButtonSpec> items,
        const float availableWidth,
        const float gap)
    {
        std::vector<float> widths(items.size(), 1.0f);
        std::size_t automaticCount{};
        float fixedWidth{};
        for (std::size_t index = 0u; index < items.size(); ++index)
        {
            if (items[index].width > 0.0f)
            {
                widths[index] = items[index].width;
                fixedWidth += items[index].width;
            }
            else
            {
                ++automaticCount;
            }
        }

        if (automaticCount == 0u)
            return widths;

        const float totalGap = items.size() > 1u
            ? (std::max)(0.0f, gap)
                * static_cast<float>(items.size() - 1u)
            : 0.0f;
        const float automaticWidth = (std::max)(
            1.0f,
            ((std::max)(1.0f, availableWidth) - totalGap - fixedWidth)
                / static_cast<float>(automaticCount));
        for (std::size_t index = 0u; index < items.size(); ++index)
        {
            if (items[index].width <= 0.0f)
                widths[index] = automaticWidth;
        }
        return widths;
    }

    struct InlineButtonRowLayout
    {
        std::vector<WidgetBounds> items{};
        float height{};
        bool valid{};
    };

    // Local coordinates are shared by drawing, hit targets and footer sizing.
    // Text measurement stays outside this pure wrapping/alignment calculation.
    [[nodiscard]] static InlineButtonRowLayout make_inline_button_layout(
        std::span<const float> widths, float availableWidth, float height,
        float gap, bool wrap, InlineButtonAlignment alignment)
    {
        InlineButtonRowLayout result{};
        if (widths.empty() || !std::isfinite(availableWidth) || availableWidth <= 0.0f
            || !std::isfinite(height) || height <= 0.0f || !std::isfinite(gap))
            return result;
        gap = (std::max)(0.0f, gap);
        result.items.reserve(widths.size());
        float x{}, y{};
        std::size_t lineStart{};
        const auto alignLine = [&]()
        {
            if (alignment == InlineButtonAlignment::Start) return;
            const float remaining = (std::max)(0.0f, availableWidth - x);
            const float offset = alignment == InlineButtonAlignment::Center
                ? std::floor(remaining * 0.5f) : remaining;
            for (std::size_t i = lineStart; i < result.items.size(); ++i)
                result.items[i].position.x += offset;
        };
        for (const float requested : widths)
        {
            if (!std::isfinite(requested) || requested <= 0.0f) return {};
            const float width = (std::min)(requested, availableWidth);
            if (wrap && result.items.size() != lineStart
                && x + gap + width > availableWidth)
            {
                alignLine();
                y += height + gap;
                x = 0.0f;
                lineStart = result.items.size();
            }
            if (result.items.size() != lineStart) x += gap;
            result.items.push_back({.position = {x, y}, .size = {width, height}});
            x += width;
        }
        alignLine();
        result.height = y + height;
        result.valid = true;
        return result;
    }

    [[nodiscard]] static InlineButtonRowLayout resolve_inline_button_layout(
        std::span<const InlineButtonSpec> items, float height, float gap,
        const InlineButtonRowOptions& options)
    {
        float availableWidth = content_available_width(g_frame.cursor.x);
        if (options.available_width > 0.0f)
            availableWidth = (std::min)(availableWidth, options.available_width);
        if (options.sizing == InlineButtonSizing::Fill)
        {
            // Existing callers retain equal/fixed-width fill semantics.
            const auto widths = resolve_inline_button_widths(items, availableWidth, gap);
            InlineButtonRowLayout result{};
            float x{};
            for (const float width : widths)
            {
                result.items.push_back({.position = {x, 0.0f}, .size = {width, height}});
                x += width + gap;
            }
            result.height = (std::max)(1.0f, height);
            result.valid = !items.empty();
            return result;
        }
        const float minWidth = space_advance(kFontScale) + 2.0f * kContentPadding;
        height = (std::max)(height, base_line_height(kFontScale) + 2.0f * kBoxInnerPadding);
        std::vector<float> widths{};
        widths.reserve(items.size());
        for (const auto& item : items)
        {
            const float preferred = item.width > 0.0f ? item.width
                : std::ceil(measure_text_width(item.label, kFontScale)
                    + 2.0f * kContentPadding + 2.0f * kButtonTextClipInset + 2.0f);
            widths.push_back((std::max)(minWidth, preferred));
        }
        return make_inline_button_layout(widths, availableWidth, height, gap,
            options.wrap, options.alignment);
    }

    [[nodiscard]] static bool inline_button_auto_width_contract()
    {
        constexpr std::array allAutomatic{
            InlineButtonSpec{.label = "A", .width = 0.0f},
            InlineButtonSpec{.label = "B", .width = 0.0f}};
        const auto equal = resolve_inline_button_widths(
            allAutomatic, 305.0f, 5.0f);
        if (equal.size() != 2u
            || equal[0] != 150.0f
            || equal[1] != 150.0f)
        {
            return false;
        }

        constexpr std::array mixed{
            InlineButtonSpec{.label = "Fixed", .width = 100.0f},
            InlineButtonSpec{.label = "Fill", .width = 0.0f}};
        const auto shared = resolve_inline_button_widths(mixed, 305.0f, 5.0f);
        if (shared.size() != 2u
            || shared[0] != 100.0f
            || shared[1] != 200.0f)
            return false;
        constexpr std::array measured{110.0f, 160.0f, 90.0f};
        const auto normal = make_inline_button_layout(measured, 800.0f, 30.0f,
            6.0f, true, InlineButtonAlignment::Start);
        const auto footer = make_inline_button_layout(measured, 800.0f, 30.0f,
            6.0f, true, InlineButtonAlignment::End);
        const auto narrow = make_inline_button_layout(measured, 280.0f, 30.0f,
            6.0f, true, InlineButtonAlignment::End);
        const auto centered = make_inline_button_layout(measured, 800.0f, 30.0f,
            6.0f, true, InlineButtonAlignment::Center);
        const auto centeredNarrow = make_inline_button_layout(measured, 280.0f, 30.0f,
            6.0f, true, InlineButtonAlignment::Center);
        constexpr std::array disabledItems{
            InlineButtonSpec{.label="A", .width=110.0f},
            InlineButtonSpec{.label="B", .width=160.0f, .enabled=false},
            InlineButtonSpec{.label="C", .width=90.0f}};
        const auto disabledWidths = resolve_inline_button_widths(disabledItems, 800.0f, 6.0f);
        const auto centeredDisabled = make_inline_button_layout(disabledWidths,
            800.0f, 30.0f, 6.0f, true, InlineButtonAlignment::Center);
        const auto oversized = make_inline_button_layout(std::array{900.0f},
            240.0f, 30.0f, 6.0f, true, InlineButtonAlignment::Start);
        return normal.valid && footer.valid && narrow.valid && oversized.valid
            && centered.valid && centeredNarrow.valid && centeredDisabled.valid
            && normal.items.size() == measured.size()
            && normal.items[0].size.x == 110.0f
            && normal.items[1].position.x == 116.0f
            && normal.height == 30.0f
            && footer.items[0].position.x == 428.0f
            && footer.items[2].position.x + footer.items[2].size.x == 800.0f
            && narrow.height == 66.0f
            && narrow.items[0].position.x == 4.0f
            && narrow.items[1].position.x == 120.0f
            && narrow.items[2].position.x == 190.0f
            && narrow.items[2].position.y == 36.0f
            && centered.items[0].position.x == 214.0f
            && centered.items[2].position.x + centered.items[2].size.x == 586.0f
            && centeredNarrow.height == 66.0f
            && centeredNarrow.items[0].position.x == 2.0f
            && centeredNarrow.items[1].position.x == 118.0f
            && centeredNarrow.items[2].position.x == 95.0f
            && centeredNarrow.items[2].position.y == 36.0f
            && centeredDisabled.items[1].position.x == centered.items[1].position.x
            && centeredDisabled.items[1].size.x == centered.items[1].size.x
            && point_in_rect({100.0f, 37.0f}, centeredNarrow.items[2].position.x,
                centeredNarrow.items[2].position.y, centeredNarrow.items[2].size.x,
                centeredNarrow.items[2].size.y)
            && !point_in_rect({115.0f, 1.0f}, centeredNarrow.items[0].position.x,
                centeredNarrow.items[0].position.y, centeredNarrow.items[0].size.x,
                centeredNarrow.items[0].size.y)
            && !point_in_rect({1.0f, 1.0f}, centered.items[0].position.x,
                centered.items[0].position.y, centered.items[0].size.x,
                centered.items[0].size.y)
            && oversized.items[0].size.x == 240.0f
            && !make_inline_button_layout(measured, 0.0f, 30.0f, 6.0f,
                true, InlineButtonAlignment::Start).valid
            && !make_inline_button_layout(std::array{(std::numeric_limits<float>::quiet_NaN)()},
                280.0f, 30.0f, 6.0f, true, InlineButtonAlignment::Start).valid;
    }

    float inline_button_row_height(std::span<const InlineButtonSpec> items,
        float height, float gap, const InlineButtonRowOptions& options) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx || items.empty()) return 0.0f;
        return resolve_inline_button_layout(items, height, gap, options).height;
    }

    std::optional<std::size_t> inline_button_row(
        std::span<const InlineButtonSpec> items,
        float height,
        float gap,
        const InlineButtonRowOptions& options) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx || items.empty())
            return std::nullopt;

        const Vec2 rowStart = g_frame.cursor;
        std::optional<std::size_t> clicked{};
        const auto layout = resolve_inline_button_layout(items, height, gap, options);
        if (!layout.valid) return std::nullopt;

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const auto& item = items[i];
            const auto& bounds = layout.items[i];
            set_cursor({rowStart.x + bounds.position.x, rowStart.y + bounds.position.y});
            if (button_with_state(
                    item.label,
                    bounds.size,
                    false,
                    item.enabled))
            {
                clicked = i;
            }
        }

        set_cursor(rowStart);
        advance_cursor({ 0.0f, layout.height + kContentPadding });
        return clicked;
    }

    TabBarResult responsive_tab_bar_buttons(
        const ResponsiveTabBarOptions& options) noexcept
    {
        TabBarResult result{};
        if (options.tabs.empty())
            return result;

        static thread_local std::vector<float> widths{};
        widths.clear();
        widths.reserve(options.tabs.size());
        const bool workbenchPresentation =
            options.presentation == TabBarPresentation::Workbench;
        std::size_t activeIndex = (std::numeric_limits<std::size_t>::max)();
        float longestLabelWidth{};
        for (std::size_t index = 0; index < options.tabs.size(); ++index)
        {
            const TabButtonSpec& tab = options.tabs[index];
            if (tab.active)
                activeIndex = index;
            const float measuredLabel = measure_text_width(
                tab.label, kFontScale);
            longestLabelWidth = (std::max)(longestLabelWidth, measuredLabel);
            const float requestedWidth = workbenchPresentation
                ? gui_lib::resolve_responsive_tab_width({
                    .requested_width = tab.width,
                    .measured_label_width = measuredLabel,
                    .minimum_hit_width = 72.0f,
                    .horizontal_padding = 24.0f,
                    .close_extent = tab.closable ? 18.0f : 0.0f,
                    .dirty_extent = tab.dirty ? 8.0f : 0.0f})
                : tab.width > 0.0f
                    ? tab.width
                    : gui_lib::preferred_tool_tab_width(
                        measuredLabel,
                        tab.closable,
                        tab.dirty);
            widths.push_back((std::max)(1.0f, requestedWidth));
        }

        const Vec2 keyboardStripPosition = g_frame.cursor;
        const auto apply_keyboard_navigation = [&]()
        {
            if (result.selected_index || result.closed_index) return;
            const Vec2 stripSize{options.available_width, options.height};
            result.selected_index = navigate_tab_keys(options.tabs, keyboardStripPosition,
                stripSize, stripSize, options.keyboard_navigation);
        };

        const gui_lib::ResponsiveTabStripLayout layout =
            gui_lib::make_responsive_tab_strip_layout({
                .item_widths = widths,
                .active_index = activeIndex,
                .available_width = options.available_width,
                .gap = options.gap,
                .overflow_width = (std::max)({
                    options.overflow_width,
                    measure_text_width(options.overflow_label, kFontScale)
                        + 52.0f,
                    longestLabelWidth + 32.0f})
            });
        if (!layout.valid || !layout.overflowed)
        {
            result = tab_bar_buttons(
                options.tabs,
                options.height,
                options.gap,
                options.presentation);
            apply_keyboard_navigation();
            return result;
        }

        const Vec2 rowStart = cursor_position();
        static thread_local std::vector<TabButtonSpec> visibleTabs{};
        visibleTabs.clear();
        visibleTabs.reserve(layout.visible_indices.size());
        for (const std::uint32_t index : layout.visible_indices)
            visibleTabs.push_back(options.tabs[index]);

        if (!visibleTabs.empty())
        {
            const TabBarResult visibleResult = tab_bar_buttons(
                visibleTabs,
                options.height,
                options.gap,
                options.presentation);
            if (visibleResult.pressed_index)
                result.pressed_index = layout.visible_indices[*visibleResult.pressed_index];
            if (visibleResult.selected_index)
                result.selected_index = layout.visible_indices[*visibleResult.selected_index];
            if (visibleResult.closed_index)
                result.closed_index = layout.visible_indices[*visibleResult.closed_index];
        }

        static thread_local std::vector<std::string_view> overflowLabels{};
        static thread_local std::vector<std::uint32_t> overflowSelectableIndices{};
        overflowLabels.clear();
        overflowSelectableIndices.clear();
        overflowLabels.reserve(layout.overflow_indices.size());
        overflowSelectableIndices.reserve(layout.overflow_indices.size());
        std::string_view selectedOverflow{};
        for (const std::uint32_t index : layout.overflow_indices)
        {
            if (!options.tabs[index].enabled)
                continue;
            overflowLabels.push_back(options.tabs[index].label);
            overflowSelectableIndices.push_back(index);
            if (options.tabs[index].active)
                selectedOverflow = options.tabs[index].label;
        }
        const std::string overflowLabel = std::string(options.overflow_label)
            + " (" + std::to_string(layout.overflow_indices.size()) + ")";
        set_cursor({
            rowStart.x + layout.visible_width
                + (visibleTabs.empty() ? 0.0f : options.gap),
            rowStart.y
        });
        if (overflowSelectableIndices.empty())
        {
            (void)button_with_state(
                overflowLabel,
                { layout.overflow_width, options.height },
                false,
                false);
        }
        else
        {
            const SelectBoxResult overflowResult = select_box(SelectBoxOptions{
                .id = options.overflow_id,
                .placeholder = overflowLabel,
                .selected = selectedOverflow,
                .options = overflowLabels,
                .size = { layout.overflow_width, options.height },
                .row_height = options.height,
                .max_visible_options = 10u
            });
            if (overflowResult.changed && overflowResult.selected_index)
            {
                result.selected_index =
                    overflowSelectableIndices[*overflowResult.selected_index];
            }
        }
        apply_keyboard_navigation();
        set_cursor({
            rowStart.x,
            rowStart.y + (std::max)(22.0f, options.height) + kContentPadding
        });
        return result;
    }

    namespace
    {
        struct StatusStripPrepared
        {
            epochgui::status_strip::Layout layout{};
            std::array<std::string_view, epochgui::status_strip::max_items> labels{};
            std::array<float, epochgui::status_strip::max_items> widths{};
        };

        [[nodiscard]] float status_item_text_width(const StatusStripItem& item,
            std::string_view label, int density) noexcept
        {
            const auto reference = density == 0 ? item.width_reference
                : density == 1 ? item.compact_width_reference : item.minimal_width_reference;
            return (std::max)(measure_text_width(label, kFontScale), measure_text_width(reference, kFontScale));
        }

        [[nodiscard]] StatusStripPrepared prepare_status_strip(
            const StatusStripOptions& options) noexcept
        {
            StatusStripPrepared prepared{};
            std::size_t count = (std::min)(options.items.size(),
                epochgui::status_strip::max_items);
            auto& widths = prepared.widths;
            const float gap = (std::max)(0.0f, options.item_gap);
            const float inset = std::clamp(options.horizontal_padding, 0.0f,
                (std::max)(0.0f, (options.available_width - 1.0f) * 0.5f));
            const float usable = (std::max)(1.0f, options.available_width - 2.0f * inset);
            // Select a single common density for the WHOLE row. Previously
            // each chip compacted only when that *individual* chip exceeded
            // the strip width, forcing a perfectly usable top row to wrap.
            const auto fits = [&](int density) noexcept
            {
                float total = count > 0u ? gap * static_cast<float>(count - 1u) : 0.0f;
                for (std::size_t i = 0u; i < count; ++i)
                {
                    const auto& item = options.items[i];
                    const std::string_view label = density == 0 ? item.label
                        : density == 1 && !item.compact_label.empty()
                            ? item.compact_label
                        : !item.minimal_label.empty() ? item.minimal_label
                        : !item.compact_label.empty() ? item.compact_label : item.label;
                    total += status_item_text_width(item, label, density) + 20.0f;
                }
                return total <= usable;
            };
            const bool prioritized = std::any_of(options.items.begin(), options.items.end(),
                [](const auto& item) { return item.compression_priority != 0u; });
            const int density = prioritized || fits(0) ? 0 : fits(1) ? 1 : 2;
            for (std::size_t i = 0u; i < count; ++i)
            {
                const StatusStripItem& item = options.items[i];
                prepared.labels[i] = density == 0 ? item.label
                    : density == 1 && !item.compact_label.empty() ? item.compact_label
                    : !item.minimal_label.empty() ? item.minimal_label
                    : !item.compact_label.empty() ? item.compact_label : item.label;
                widths[i] = status_item_text_width(item, prepared.labels[i], density);
            }
            if (prioritized)
            {
                std::array<std::size_t, epochgui::status_strip::max_items> order{};
                float occupied = gap * static_cast<float>(count > 0u ? count - 1u : 0u);
                for (std::size_t i = 0u; i < count; ++i)
                {
                    order[i] = i;
                    occupied += widths[i] + 20.0f;
                }
                std::sort(order.begin(), order.begin() + count, [&](auto a, auto b)
                {
                    return options.items[a].compression_priority == options.items[b].compression_priority
                        ? a < b : options.items[a].compression_priority > options.items[b].compression_priority;
                });
                for (std::size_t slot = 0u; slot < count && occupied > usable; ++slot)
                {
                    const auto i = order[slot];
                    const std::array labels{options.items[i].compact_label, options.items[i].minimal_label};
                    for (std::size_t variant = 0u; variant < labels.size(); ++variant)
                    {
                        const auto label = labels[variant];
                        if (label.empty() || occupied <= usable) continue;
                        const float width = status_item_text_width(options.items[i], label, static_cast<int>(variant + 1u));
                        if (width >= widths[i]) continue;
                        occupied -= widths[i] - width;
                        widths[i] = width;
                        prepared.labels[i] = label;
                    }
                }
            }
            if (options.single_row)
            {
                float occupied = gap * static_cast<float>(count > 0u ? count - 1u : 0u);
                for (std::size_t i = 0u; i < count; ++i)
                    occupied += widths[i] + 20.0f;
                while (count > 1u && occupied > usable)
                {
                    --count;
                    occupied -= widths[count] + 20.0f + gap;
                }
            }
            prepared.layout = epochgui::status_strip::measure(
                std::span{widths}.first(count), options.available_width,
                options.item_height, options.item_gap, options.row_gap,
                options.horizontal_padding, 20.0f, options.align_right);
            return prepared;
        }
    }

    float status_strip_preferred_width(const StatusStripOptions& options) noexcept
    {
        auto count = (std::min)(options.items.size(),
            epochgui::status_strip::max_items);
        if (count == 0u) return 0.0f;
        // Measure the same density the draw pass will use. Returning the full
        // label allocation after compacting leaves an empty gap that menus
        // cannot reclaim on narrow windows.
        const bool constrained = options.available_width > 0.0f;
        const auto prepared = constrained ? prepare_status_strip(options)
            : StatusStripPrepared{};
        if (constrained) count = prepared.layout.count;
        float width = 2.0f * (std::max)(0.0f, options.horizontal_padding)
            + static_cast<float>(count - 1u) * (std::max)(0.0f, options.item_gap);
        for (std::size_t i = 0u; i < count; ++i)
            width += (constrained ? prepared.widths[i]
                : status_item_text_width(options.items[i], options.items[i].label, 0)) + 20.0f;
        return constrained ? (std::min)(std::ceil(width), options.available_width) : width;
    }

    float status_strip_minimum_width(const StatusStripOptions& options) noexcept
    {
        const auto count = (std::min)(options.items.size(), epochgui::status_strip::max_items);
        if (count == 0u) return 0.0f;
        float width = 2.0f * (std::max)(0.0f, options.horizontal_padding)
            + static_cast<float>(count - 1u) * (std::max)(0.0f, options.item_gap);
        for (std::size_t i = 0u; i < count; ++i)
        {
            const auto& item = options.items[i];
            const auto label = !item.minimal_label.empty() ? item.minimal_label
                : !item.compact_label.empty() ? item.compact_label : item.label;
            width += status_item_text_width(item, label, 2) + 20.0f;
        }
        return std::ceil(width);
    }

    [[nodiscard]] static bool status_strip_allocation_contract() noexcept
    {
        constexpr std::array items{
            StatusStripItem{"Thread 1 / Queued 0 / Idle 4", "1T / 0Q / 4I", "1T"},
            StatusStripItem{"x64 MSVC 2022 Release", "x64 Release", "x64"}};
        const float full = status_strip_preferred_width({.items = items});
        const float minimum = status_strip_minimum_width({.items = items});
        const auto allCompact = prepare_status_strip({.items = items,
            .available_width = minimum, .single_row = true});
        const float budget = full - 1.0f;
        const float fitted = status_strip_preferred_width({
            .items = items, .available_width = budget});
        const auto layout = prepare_status_strip({
            .items = items, .available_width = fitted}).layout;
        // A compact strip must give its spare allocation back to adjacent
        // menus, while rendering at that returned width remains one row.
        constexpr std::array pinnedItems{
            StatusStripItem{"CPU 75% RAM 8G", "CPU75% RAM8G", "CPU75% RAM8G"},
            StatusStripItem{"v0.90.182"}, StatusStripItem{"Windows Release"}};
        const auto narrow = prepare_status_strip({.items = pinnedItems,
            .available_width = 160.0f, .single_row = true});
        constexpr std::array orderedItems{
            StatusStripItem{"CPU 75% / RAM 50%", "75% / 50%", "75% / 50%", SemanticTone::neutral, 100u},
            StatusStripItem{"v0.90.182"},
            StatusStripItem{"Thread 1 / Queued 0 / Idle 4", "1T / 0Q / 4I", "1T / 0Q / 4I", SemanticTone::neutral, 200u}};
        const float orderedWidth = status_strip_preferred_width({.items = orderedItems});
        const auto ordered = prepare_status_strip({.items = orderedItems,
            .available_width = orderedWidth - 1.0f, .single_row = true});
        const std::array stableItems{
            StatusStripItem{.label = "CPU 9%", .compact_label = "C 9%", .compression_priority = 200u,
                .width_reference = "CPU 100%", .compact_width_reference = "C 100%"},
            StatusStripItem{.label = "RAM 9%", .compact_label = "R 9%", .compression_priority = 200u,
                .width_reference = "RAM 100%", .compact_width_reference = "R 100%"},
            StatusStripItem{.label = "Zoom 13x", .compact_label = "13x", .compression_priority = 300u}};
        auto largeValues = stableItems;
        largeValues[0].label = "CPU 100%";
        largeValues[1].label = "RAM 100%";
        const float stableWidth = status_strip_preferred_width({.items = stableItems});
        const auto stable = prepare_status_strip({.items = stableItems,
            .available_width = stableWidth - 1.0f, .single_row = true});
        const auto large = prepare_status_strip({.items = largeValues,
            .available_width = stableWidth - 1.0f, .single_row = true});
        const bool stableNumericAllocation = stableWidth == status_strip_preferred_width({.items = largeValues})
            && stable.layout.count == 3u && large.layout.count == 3u
            && stable.labels[0] == stableItems[0].label && stable.labels[2] == "13x"
            && large.labels[0] == largeValues[0].label && large.labels[2] == "13x"
            && stable.layout.cells[0].width == large.layout.cells[0].width
            && stable.layout.cells[1].width == large.layout.cells[1].width;
        return ordered.labels[0] == orderedItems[0].label
            && ordered.labels[2] == orderedItems[2].compact_label
            && minimum < full && allCompact.layout.count == items.size()
            && status_strip_preferred_width({.items = items, .available_width = minimum,
                .single_row = true}) <= minimum
            && fitted > 0.0f && fitted < budget && layout.rows == 1u
            && layout.count == items.size()
            && layout.cells[1].x + layout.cells[1].width <= fitted
            && narrow.layout.rows == 1u && narrow.layout.count == 1u
            && narrow.labels[0] == pinnedItems[0].minimal_label && stableNumericAllocation;
    }

    float status_strip_height(const StatusStripOptions& options) noexcept
    {
        return prepare_status_strip(options).layout.height;
    }

    StatusStripResult status_strip(const StatusStripOptions& options) noexcept
    {
        StatusStripResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx)
            return result;
        const StatusStripPrepared prepared = prepare_status_strip(options);
        result.rows = prepared.layout.rows;
        result.height = prepared.layout.height;
        const Vec2 saved = cursor_position();
        std::string_view hoverLabel{};
        Vec2 hoverPosition{};
        for (std::size_t i = 0u; i < prepared.layout.count; ++i)
        {
            const auto& cell = prepared.layout.cells[i];
            set_cursor({options.position.x + cell.x,
                options.position.y + cell.y});
            if (button_with_state(prepared.labels[i], {cell.width, cell.height},
                    false, true, options.items[i].tone))
                result.selected_index = i;
            if ((!options.items[i].tooltip.empty() || prepared.labels[i] != options.items[i].label)
                && g_frame.pointerInputAllowed
                && point_in_modal_input_capture(g_frame.mousePos)
                && point_in_rect(g_frame.mousePos, options.position.x + cell.x,
                    options.position.y + cell.y, cell.width, cell.height))
            {
                hoverLabel = options.items[i].tooltip.empty()
                    ? options.items[i].label : options.items[i].tooltip;
                hoverPosition = {options.position.x + cell.x,
                    options.position.y + cell.y + cell.height + 3.0f};
            }
        }
        set_cursor(saved);
        if (!hoverLabel.empty())
        {
            const float width = (std::min)(g_frame.windowSize.x,
                measure_text_width(hoverLabel, kFontScale) + 20.0f);
            hoverPosition.x = std::clamp(hoverPosition.x, g_frame.origin.x,
                (std::max)(g_frame.origin.x, g_frame.origin.x + g_frame.windowSize.x - width));
            begin_top_layer();
            draw_rounded_sprite(active_palette().panelBackground,
                hoverPosition.x, hoverPosition.y, width, 28.0f,
                gui_lib::rounded_rect::RoundedRectStyle{.enabled = true, .control_radius = 4.0f});
            draw_text_line(hoverLabel, hoverPosition.x + 10.0f,
                hoverPosition.y + 4.0f, kFontScale);
            end_top_layer();
        }
        return result;
    }

    ChromeBarResult chrome_bar(const ChromeBarOptions& options) noexcept
    {
        ChromeBarResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx
            || options.size.x <= 0.0f || options.size.y <= 0.0f)
        {
            return result;
        }

        const auto item_width = [&](const ChromeItemSpec& item, bool compact)
        {
            const float textWidth = measure_text_width(item.label, kFontScale);
            float padding = 20.0f;
            switch (item.role)
            {
            case ChromeItemRole::Label:
                padding = 8.0f;
                break;
            case ChromeItemRole::Menu:
                padding = 28.0f;
                break;
            case ChromeItemRole::Command:
                padding = 32.0f;
                break;
            case ChromeItemRole::Status:
                padding = 20.0f;
                break;
            }
            const float preferred = item.preferred_width > 0.0f
                ? item.preferred_width
                : textWidth + padding;
            const float minimum = item.compact_width > 0.0f
                ? item.compact_width
                : (std::max)(48.0f, textWidth * 0.72f + padding);
            return compact
                ? (std::min)(preferred, minimum)
                : preferred;
        };

        static thread_local std::vector<gui_lib::ChromeBarItemOptions> leftCore{};
        static thread_local std::vector<gui_lib::ChromeBarItemOptions> centerCore{};
        static thread_local std::vector<gui_lib::ChromeBarItemOptions> rightCore{};
        const auto prepare_zone = [&](std::span<const ChromeItemSpec> source,
            std::vector<gui_lib::ChromeBarItemOptions>& target)
        {
            target.clear();
            target.reserve(source.size());
            for (const ChromeItemSpec& item : source)
            {
                target.push_back(gui_lib::ChromeBarItemOptions{
                    .preferred_width = item_width(item, false),
                    .compact_width = item_width(item, true),
                    .priority = item.priority,
                    .pinned = item.pinned,
                    .overflowable = item.overflowable
                });
            }
        };
        prepare_zone(options.left_items, leftCore);
        prepare_zone(options.center_items, centerCore);
        prepare_zone(options.right_items, rightCore);

        const gui_lib::ChromeBarLayout layout =
            gui_lib::make_chrome_bar_layout({
                .bounds = {
                    { options.position.x, options.position.y },
                    { options.size.x, options.size.y }
                },
                .left_items = leftCore,
                .center_items = centerCore,
                .right_items = rightCore,
                .item_gap = options.item_gap,
                .zone_gap = options.zone_gap,
                .overflow_width = options.overflow_width,
                .horizontal_padding = options.horizontal_padding,
                .center_shift_x = options.center_shift_x
            });
        if (!layout.valid)
            return result;

        result.left_bounds.resize(layout.left.item_bounds.size());
        result.center_bounds.resize(layout.center.item_bounds.size());
        result.right_bounds.resize(layout.right.item_bounds.size());
        result.left_overflow = from_lib(layout.left.overflow_button);
        result.center_overflow = from_lib(layout.center.overflow_button);
        result.right_overflow = from_lib(layout.right.overflow_button);
        result.compact = layout.density != gui_lib::ChromeDensity::full;
        result.overflowed = layout.density == gui_lib::ChromeDensity::minimal;
        result.valid = true;

        const Vec2 savedCursor = cursor_position();
        const auto render_zone = [&](const gui_lib::ChromeBarZoneLayout& zone,
            std::span<const ChromeItemSpec> items,
            std::vector<WidgetBounds>& bounds,
            std::optional<std::size_t>& hovered,
            std::optional<std::size_t>& selected,
            std::string_view suffix)
        {
            for (std::size_t index = 0; index < zone.item_bounds.size(); ++index)
                bounds[index] = from_lib(zone.item_bounds[index]);

            for (const std::uint32_t index : zone.visible_indices)
            {
                const ChromeItemSpec& item = items[index];
                const WidgetBounds itemBounds = bounds[index];
                set_cursor(itemBounds.position);
                const bool isHovered = item.enabled
                    && point_in_rect(
                        g_frame.mousePos,
                        itemBounds.position.x,
                        itemBounds.position.y,
                        itemBounds.size.x,
                        itemBounds.size.y)
                    && point_in_active_clip(g_frame.mousePos);
                if (isHovered)
                    hovered = index;

                if (item.role == ChromeItemRole::Menu
                    || item.role == ChromeItemRole::Command)
                {
                    if (button_with_state(
                            item.label,
                            itemBounds.size,
                            item.selected,
                            item.enabled))
                    {
                        selected = index;
                    }
                    continue;
                }

                if (item.role == ChromeItemRole::Status)
                {
                    const auto& palette = active_palette();
                    draw_sprite(
                        palette.titleBar,
                        itemBounds.position.x,
                        itemBounds.position.y,
                        itemBounds.size.x,
                        itemBounds.size.y);
                }
                const float inset = item.role == ChromeItemRole::Label
                    ? 2.0f
                    : kContentPadding;
                const std::string fitted = fit_text_to_width(
                    item.label,
                    (std::max)(1.0f, itemBounds.size.x - inset * 2.0f),
                    kFontScale);
                const std::string_view display = fitted.empty()
                    ? item.label
                    : std::string_view{ fitted };
                const float textY = itemBounds.position.y
                    + (std::max)(
                        0.0f,
                        (itemBounds.size.y - base_line_height(kFontScale))
                            * 0.5f)
                    + 1.0f;
                draw_text_line(
                    display,
                    itemBounds.position.x + inset,
                    textY,
                    kFontScale);
            }

            if (zone.overflow_button.size.x <= 0.0f
                || zone.overflow_indices.empty())
            {
                return;
            }

            static thread_local std::vector<std::string_view> labels{};
            static thread_local std::vector<std::uint32_t> mappings{};
            labels.clear();
            mappings.clear();
            labels.reserve(zone.overflow_indices.size());
            mappings.reserve(zone.overflow_indices.size());
            for (const std::uint32_t index : zone.overflow_indices)
            {
                if (!items[index].enabled)
                    continue;
                labels.push_back(items[index].label);
                mappings.push_back(index);
            }

            const WidgetBounds overflowBounds = from_lib(zone.overflow_button);
            const std::string overflowLabel =
                std::string("More (")
                + std::to_string(zone.overflow_indices.size()) + ")";
            set_cursor(overflowBounds.position);
            if (mappings.empty())
            {
                (void)button_with_state(
                    overflowLabel,
                    overflowBounds.size,
                    false,
                    false);
                return;
            }

            const std::string controlId =
                std::string(options.id) + "." + std::string(suffix);
            const SelectBoxResult overflowResult = select_box(SelectBoxOptions{
                .id = controlId,
                .placeholder = overflowLabel,
                .selected = {},
                .options = labels,
                .size = overflowBounds.size,
                .row_height = options.size.y,
                .max_visible_options = 10u
            });
            if (overflowResult.changed && overflowResult.selected_index
                && *overflowResult.selected_index < mappings.size())
            {
                selected = mappings[*overflowResult.selected_index];
            }
        };

        render_zone(
            layout.left,
            options.left_items,
            result.left_bounds,
            result.hovered_left,
            result.selected_left,
            "left-overflow");
        render_zone(
            layout.center,
            options.center_items,
            result.center_bounds,
            result.hovered_center,
            result.selected_center,
            "center-overflow");
        render_zone(
            layout.right,
            options.right_items,
            result.right_bounds,
            result.hovered_right,
            result.selected_right,
            "right-overflow");
        set_cursor(savedCursor);
        return result;
    }


