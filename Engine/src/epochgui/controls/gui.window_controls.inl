// Epoch feature implementation; included by its owning module only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
    static void render_select_popup(const PendingSelectPopup& popup) noexcept
    {
        if (!g_frame.ctx || popup.options.empty())
            return;
        if (!std::isfinite(popup.position.x) || !std::isfinite(popup.position.y)
            || !std::isfinite(popup.width) || !std::isfinite(popup.height)
            || popup.width <= 0.0f || popup.height <= 0.0f)
        {
            return;
        }

        auto& scrollState = g_scrollAreaStates[popup.scrollKey];
        scrollState.contentHeight = (std::max)(popup.height, popup.contentHeight);
        const float maxScroll = (std::max)(0.0f, scrollState.contentHeight - popup.height);
        if (!std::isfinite(scrollState.scrollY))
            scrollState.scrollY = 0.0f;
        scrollState.scrollY = (std::clamp)(scrollState.scrollY, 0.0f, maxScroll);

        const bool showScrollbar = popup.showScrollbar && scrollState.contentHeight > popup.height + 1.0f;
        const float scrollbarWidth = showScrollbar ? 10.0f : 0.0f;
        const float contentWidth = (std::max)(1.0f, popup.width - scrollbarWidth - 2.0f);
        const auto& palette = active_palette();

        draw_sprite(palette.consoleBackground, popup.position.x, popup.position.y, popup.width, popup.height);

        {
            ContentClipScope clip(
                popup.position,
                { popup.position.x + contentWidth, popup.position.y + popup.height });

            for (std::size_t i = 0; i < popup.options.size(); ++i)
            {
                const float rowY = popup.position.y - scrollState.scrollY + static_cast<float>(i) * popup.optionPitch;
                if (rowY + popup.rowHeight < popup.position.y || rowY > popup.position.y + popup.height)
                    continue;

                const bool active = popup.options[i] == popup.selected;
                const SpriteHandle background = active ? palette.buttonActive : palette.buttonNormal;
                draw_sprite(background, popup.position.x, rowY, contentWidth, popup.rowHeight);
                if (active)
                {
                    draw_sprite(palette.textFieldActive, popup.position.x, rowY, 2.0f, popup.rowHeight);
                    draw_sprite(palette.textFieldActive, popup.position.x, rowY, contentWidth, 2.0f);
                }

                const std::string fitted = fit_text_to_width(
                    popup.options[i],
                    (std::max)(1.0f, contentWidth - 20.0f),
                    kFontScale);
                const std::string_view displayLabel = fitted.empty()
                    ? std::string_view{ popup.options[i] }
                    : std::string_view{ fitted };
                const float textY = rowY + std::floor((std::max)(0.0f, (popup.rowHeight - base_line_height(kFontScale)) * 0.5f)) + 1.0f;
                draw_text_line(displayLabel, popup.position.x + kContentPadding, textY, kFontScale);
            }
        }

        if (showScrollbar)
        {
            const float trackX = popup.position.x + popup.width - scrollbarWidth;
            const float trackY = popup.position.y;
            draw_sprite(palette.textField, trackX, trackY, scrollbarWidth, popup.height);

            const float visibleRatio = popup.height / scrollState.contentHeight;
            const float thumbHeight = (std::min)(
                popup.height,
                (std::max)(18.0f, popup.height * visibleRatio));
            const float scrollRatio = maxScroll > 0.0f ? scrollState.scrollY / maxScroll : 0.0f;
            const float thumbY = trackY + (popup.height - thumbHeight) * scrollRatio;
            draw_sprite(palette.buttonActive, trackX, thumbY, scrollbarWidth, thumbHeight);
        }
    }

    static void render_pending_select_popups() noexcept
    {
        if (g_frame.pendingSelectPopups.empty())
            return;

        begin_top_layer();
        {
            ContentClipClearScope clearClip;
            for (const PendingSelectPopup& popup : g_frame.pendingSelectPopups)
                render_select_popup(popup);
        }
        end_top_layer();

        g_frame.pendingSelectPopups.clear();
    }

    void begin_window(std::string_view title, Vec2 position, Vec2 size) noexcept
    {
        begin_window(title, position, size, true);
    }

    void begin_window(std::string_view title, Vec2 position, Vec2 size, bool draw_background) noexcept
    {
        begin_window(title, position, size, draw_background, true);
    }

    void begin_window(std::string_view title, Vec2 position, Vec2 size, bool draw_background, bool show_title_bar) noexcept
    {
        if (!g_frame.ctx) return;

        try { ensure_resources(); }
        catch (...) { return; }

        g_windowFrameStack.push_back(WindowFrameState{
            .cursor = g_frame.cursor,
            .origin = g_frame.origin,
            .windowSize = g_frame.windowSize,
            .contentMin = g_frame.contentMin,
            .contentMax = g_frame.contentMax,
            .windowKey = g_frame.windowKey,
            .widgetSerial = g_frame.widgetSerial,
            .scrollAreaDepth = g_scrollAreaStack.size(),
            .insideWindow = g_frame.insideWindow
        });

        g_frame.origin = position;
        g_frame.windowSize = size;
        g_frame.insideWindow = true;
        g_frame.windowKey.assign(title.begin(), title.end());
        g_frame.widgetSerial = 0;
        g_frame.contentMin = position;
        g_frame.contentMax = { position.x + size.x, position.y + size.y };
        const auto& palette = active_palette();

        if (draw_background)
        {
            // Docked panes tile the viewport: rounded cutouts or offset shadows
            // expose black notches and dark seams at splitter intersections.
            // Floating/modal top layers keep their independent silhouettes.
            const bool floatingSurface = g_frame.topLayerDepth > 0
                || !g_floatingWindowTopLayerStack.empty();
            if (floatingSurface && g_resources.windowShadow.is_valid())
            {
                constexpr float shadowOffset = 4.0f;
                draw_sprite_raw(
                    g_resources.windowShadow,
                    position.x + shadowOffset,
                    position.y + shadowOffset,
                    size.x,
                    size.y);
            }
            if (floatingSurface)
                draw_sprite(palette.windowBackground, position.x, position.y, size.x, size.y);
            else
                draw_sprite_raw(palette.windowBackground, position.x, position.y, size.x, size.y);
        }

        const bool hasTitleBar = show_title_bar && !title.empty();
        float titleBarHeight = 0.0f;
        if (hasTitleBar)
        {
            const float titleHeight = line_advance_amount(kTitleScale);
            titleBarHeight = titleHeight + 2.0f * kTitleBarPadding;
            const float titleTextY = position.y + (titleBarHeight - titleHeight) * 0.5f;
            draw_sprite(palette.titleBar, position.x, position.y, size.x, titleBarHeight);
            const std::string fittedTitle = fit_text_to_width(
                title,
                (std::max)(1.0f, size.x - 2.0f * kContentPadding),
                kTitleScale);
            draw_text_line(
                fittedTitle.empty() ? title : std::string_view{ fittedTitle },
                position.x + kContentPadding,
                titleTextY,
                kTitleScale);
        }

        const bool fullBleedContent = !draw_background && title.empty();
        g_frame.contentMin = fullBleedContent
            ? position
            : Vec2{
                position.x + kContentPadding,
                position.y + titleBarHeight + kContentPadding
            };
        g_frame.contentMax = fullBleedContent
            ? Vec2{ position.x + (std::max)(0.0f, size.x), position.y + (std::max)(0.0f, size.y) }
            : Vec2{
                position.x + (std::max)(kContentPadding, size.x - kContentPadding),
                position.y + (std::max)(titleBarHeight + kContentPadding, size.y - kContentPadding)
            };

        set_cursor(g_frame.contentMin);
    }

    void end_window() noexcept
    {
        if (g_windowFrameStack.empty())
        {
            g_frame.insideWindow = false;
            g_frame.contentMin = {};
            g_frame.contentMax = {};
            g_frame.windowKey.clear();
            g_frame.widgetSerial = 0;
            return;
        }

        const WindowFrameState previous = std::move(g_windowFrameStack.back());
        g_windowFrameStack.pop_back();
        while (g_scrollAreaStack.size() > previous.scrollAreaDepth)
        {
            const std::string leakedKey =
                std::move(g_scrollAreaStack.back().key);
            g_scrollAreaStack.pop_back();
            if (auto state = g_scrollAreaStates.find(leakedKey);
                state != g_scrollAreaStates.end())
            {
                state->second.draggingScrollbar = false;
            }
        }
        g_frame.cursor = previous.cursor;
        g_frame.origin = previous.origin;
        g_frame.windowSize = previous.windowSize;
        g_frame.contentMin = previous.contentMin;
        g_frame.contentMax = previous.contentMax;
        g_frame.windowKey = previous.windowKey;
        g_frame.widgetSerial = previous.widgetSerial;
        g_frame.insideWindow = previous.insideWindow;
    }

    void begin_top_layer() noexcept
    {
        ++g_frame.topLayerDepth;
    }

    void end_top_layer() noexcept
    {
        if (g_frame.topLayerDepth > 0)
            --g_frame.topLayerDepth;
    }

    void begin_modal_window(const ModalWindowOptions& options) noexcept
    {
        if (!g_frame.ctx) return;

        try { ensure_resources(); }
        catch (...) { return; }

        if (!g_frame.modalInput.active())
        {
            begin_modal_input_capture(options.position, options.size);
            g_frame.modalInput.require_owner_scope();
        }
        suspend_covered_input();
        // One dialog owns a frame's input. Extra dialogs can paint but are
        // inert until the owner closes (prevents two layers consuming a click).
        g_frame.modalOwnerDrawing = !g_frame.modalOwnerDrawn;
        if (g_frame.modalOwnerDrawing)
        {
            g_frame.modalOwnerDrawn = true;
            g_frame.modalInput.enter_owner_scope();
            if (g_frame.modalInputSuspended)
            {
                g_frame.events = std::move(g_frame.modalHeldEvents);
                g_frame.mouseWheelDelta = g_frame.modalHeldWheel;
                g_frame.modalHeldWheel = 0;
            }
        }
        else
        {
            g_frame.modalInput.leave_owner_scope();
        }
        begin_top_layer();

        if (options.dim_background && options.viewport_size.x > 0.0f && options.viewport_size.y > 0.0f)
        {
            const auto& palette = active_palette();
            draw_sprite(palette.modalScrim, 0.0f, 0.0f, options.viewport_size.x, options.viewport_size.y);
        }

        begin_window(options.title, options.position, options.size);
    }

    void end_modal_window() noexcept
    {
        end_window();
        end_top_layer();
        // Do not release key, text or wheel events to any later background
        // controls, including after Close changes editor state mid-frame.
        g_frame.modalInput.leave_owner_scope();
        if (g_frame.modalOwnerDrawing)
        {
            g_frame.events.clear();
            g_frame.mouseWheelDelta = 0;
            g_frame.modalOwnerDrawing = false;
        }
    }

    FloatingWindowResult begin_floating_window(
        FloatingWindowState& state,
        const FloatingWindowOptions& options) noexcept
    {
        FloatingWindowResult result{};
        if (!g_frame.ctx || !state.open)
            return result;

        try { ensure_resources(); }
        catch (...) { return result; }

        const float titleHeight = line_advance_amount(kTitleScale) + 2.0f * kTitleBarPadding;
        gui_lib::FloatingWindowState coreState = to_lib(state);
        const bool wasInteracting = coreState.dragging || coreState.resizing || coreState.close_pressed;
        const gui_lib::FloatingWindowOptions coreOptions{
            .default_position = to_lib(options.default_position),
            .default_size = to_lib(options.default_size),
            .min_size = to_lib(options.min_size),
            .viewport_size = to_lib(options.viewport_size),
            .title_bar_height = titleHeight,
            .content_padding = kContentPadding,
            .movable = options.movable,
            .resizable = options.resizable,
            .closable = options.closable
        };
        const gui_lib::FloatingWindowInput coreInput{
            .mouse_position = to_lib(g_frame.mousePos),
            .mouse_down = g_frame.mouseDown,
            .mouse_pressed = left_press_available(),
            .mouse_released = left_release_available()
        };

        const gui_lib::FloatingWindowLayout layout =
            gui_lib::update_floating_window(coreState, coreOptions, coreInput);
        from_lib(coreState, state);

        result.focused = layout.focused;
        result.hovered = layout.hovered;
        result.moved = layout.moved;
        result.resized = layout.resized;
        result.close_requested = layout.close_requested;
        result.title_hovered = layout.title_hovered;
        result.close_hovered = layout.close_hovered;
        result.resize_hovered = layout.resize_hovered;
        result.window = from_lib(layout.window);
        result.title_bar = from_lib(layout.title_bar);
        result.content = from_lib(layout.content);
        result.close_button = from_lib(layout.close_button);
        result.resize_handle = from_lib(layout.resize_handle);

        // Frame chrome can own a press before child rendering; the content
        // must receive its own button/input hit tests before the host absorbs it.
        const bool chromePress = floating_input::chrome_owns_press(
            layout.hovered, options.movable, layout.title_hovered,
            layout.close_hovered, layout.resize_hovered);
        if (coreInput.mouse_pressed && chromePress)
            consume_left_press();
        if (coreInput.mouse_released && wasInteracting)
            consume_left_release();
        if (layout.close_requested || !state.open)
            return result;

        if (options.capture_input && !g_frame.modalInput.active())
            begin_modal_input_capture(state.position, state.size);

        if (options.top_layer)
            begin_top_layer();
        g_floatingWindowTopLayerStack.push_back(options.top_layer);
        g_floatingWindowPointerCoverStack.push_back(layout.hovered);

        begin_window(options.title, state.position, state.size, options.draw_background);
        if (!options.id.empty())
            g_frame.windowKey.assign(options.id.begin(), options.id.end());

        const auto& palette = active_palette();
        if (options.closable)
        {
            const SpriteHandle closeBackground =
                state.close_pressed ? palette.buttonActive
                : result.close_hovered ? palette.buttonHover
                : palette.buttonNormal;
            draw_sprite(
                closeBackground,
                result.close_button.position.x,
                result.close_button.position.y,
                result.close_button.size.x,
                result.close_button.size.y);
            if (result.close_hovered || state.close_pressed)
            {
                draw_sprite(
                    palette.textFieldActive,
                    result.close_button.position.x,
                    result.close_button.position.y,
                    result.close_button.size.x,
                    2.0f);
            }

            draw_close_mark(result.close_button.position, result.close_button.size);
        }

        if (options.resizable)
        {
            const SpriteHandle handleSprite =
                state.resizing || result.resize_hovered ? palette.textFieldActive : palette.buttonHover;
            const float x = result.resize_handle.position.x;
            const float y = result.resize_handle.position.y;
            const float w = result.resize_handle.size.x;
            const float h = result.resize_handle.size.y;
            draw_sprite(handleSprite, x + w - 10.0f, y + h - 3.0f, 8.0f, 2.0f);
            draw_sprite(handleSprite, x + w - 6.0f, y + h - 7.0f, 4.0f, 2.0f);
        }

        set_cursor(result.content.position);
        result.begun = true;
        return result;
    }

    void end_floating_window() noexcept
    {
        if (g_floatingWindowTopLayerStack.empty())
            return;

        const bool topLayer = g_floatingWindowTopLayerStack.back();
        g_floatingWindowTopLayerStack.pop_back();
        // Child widgets have now handled eligible input. Swallow leftover
        // clicks inside this window so nothing behind it can activate.
        if (!g_floatingWindowPointerCoverStack.empty())
        {
            const bool covered = g_floatingWindowPointerCoverStack.back();
            g_floatingWindowPointerCoverStack.pop_back();
            if (floating_input::absorb_unhandled_pointer(covered,
                    !left_press_available()))
                consume_left_press();
            if (covered && left_release_available())
                consume_left_release();
            if (covered && right_press_available())
                consume_right_press();
            if (covered && right_release_available())
                consume_right_release();
        }
        end_window();
        if (topLayer)
            end_top_layer();
    }

    DockGuideLayout make_dock_guide_layout(const DockGuideOptions& options) noexcept
    {
        return from_lib(gui_lib::make_dock_guide_layout(to_lib(options)));
    }

    void render_dock_guide_overlay(
        const DockGuideLayout& layout,
        const DockGuideOverlayOptions& options) noexcept
    {
        if (!g_frame.ctx || layout.count == 0U)
            return;

        ensure_resources();
        begin_top_layer();
        ContentClipClearScope clearClip;

        const auto drawPreview = [&](const WidgetBounds& preview)
        {
            if (preview.size.x <= 1.0f || preview.size.y <= 1.0f)
                return;

            draw_sprite(
                g_resources.dockGuidePreview,
                preview.position.x,
                preview.position.y,
                preview.size.x,
                preview.size.y);
            draw_sprite(
                g_resources.dockGuide,
                preview.position.x,
                preview.position.y,
                preview.size.x,
                (std::min)(28.0f, preview.size.y));
            if (!options.moving_label.empty())
            {
                draw_text_line(
                    options.moving_label,
                    preview.position.x + 10.0f,
                    preview.position.y + 5.0f,
                    0.86f);
            }
        };

        if (layout.hovered_target != DockGuideTarget::none)
            drawPreview(layout.hovered_preview);
        else if (options.show_floating_preview)
            drawPreview(options.floating_preview);

        for (std::uint32_t index = 0U; index < layout.count; ++index)
        {
            const DockGuide& guide = layout.guides[index];
            if (guide.target == DockGuideTarget::none)
                continue;

            const bool contextTarget =
                guide.target == DockGuideTarget::left_context
                || guide.target == DockGuideTarget::right_context;
            const SpriteHandle fill = contextTarget
                ? (guide.hovered ? g_resources.dockContextHover : g_resources.dockContextGuide)
                : (guide.hovered ? g_resources.dockGuideHover : g_resources.dockGuide);
            draw_sprite(
                fill,
                guide.target_bounds.position.x,
                guide.target_bounds.position.y,
                guide.target_bounds.size.x,
                guide.target_bounds.size.y);

            if (contextTarget)
            {
                draw_sprite(
                    guide.hovered ? g_resources.dockContextHover : g_resources.dockGuide,
                    guide.target_bounds.position.x + 10.0f,
                    guide.target_bounds.position.y + guide.target_bounds.size.y - 10.0f,
                    (std::max)(0.0f, guide.target_bounds.size.x - 20.0f),
                    4.0f);
            }

            std::string_view label{};
            switch (guide.target)
            {
            case DockGuideTarget::left_tabs: label = options.left_tabs_label; break;
            case DockGuideTarget::right_tabs: label = options.right_tabs_label; break;
            case DockGuideTarget::bottom_left_tabs: label = options.bottom_left_tabs_label; break;
            case DockGuideTarget::bottom_right_tabs: label = options.bottom_right_tabs_label; break;
            case DockGuideTarget::left_context: label = options.left_context_label; break;
            case DockGuideTarget::right_context: label = options.right_context_label; break;
            case DockGuideTarget::float_window: label = options.floating_label; break;
            default: break;
            }

            if (!label.empty())
            {
                const float scale = contextTarget ? 0.86f : 0.80f;
                const float labelWidth = measure_text_width(label, scale);
                const float labelHeight = line_advance_amount(scale);
                draw_text_line(
                    label,
                    guide.target_bounds.position.x
                        + (std::max)(6.0f, (guide.target_bounds.size.x - labelWidth) * 0.5f),
                    guide.target_bounds.position.y
                        + (std::max)(6.0f, (guide.target_bounds.size.y - labelHeight) * 0.5f),
                    scale);
            }
        }

        end_top_layer();
    }

    void render_dock_tab_insertion_overlay(
        const DockTabInsertionOverlayOptions& options) noexcept
    {
        if (!g_frame.ctx
            || options.insertion_marker.size.x <= 0.0f
            || options.insertion_marker.size.y <= 0.0f
            || options.insertion_ghost.size.x <= 0.0f
            || options.insertion_ghost.size.y <= 0.0f)
            return;

        ensure_resources();
        begin_top_layer();
        ContentClipClearScope clearClip;
        draw_sprite(
            g_resources.dockGuidePreview,
            options.insertion_ghost.position.x,
            options.insertion_ghost.position.y,
            options.insertion_ghost.size.x,
            options.insertion_ghost.size.y);
        draw_sprite(
            options.no_op ? g_resources.dockGuide : g_resources.dockGuideHover,
            options.insertion_marker.position.x,
            options.insertion_marker.position.y,
            options.insertion_marker.size.x,
            options.insertion_marker.size.y);
        if (!options.moving_label.empty())
        {
            const std::string fitted = fit_text_to_width(
                options.moving_label,
                (std::max)(1.0f, options.insertion_ghost.size.x - 16.0f),
                0.80f);
            draw_text_line(
                fitted.empty() ? options.moving_label : std::string_view{fitted},
                options.insertion_ghost.position.x + 8.0f,
                options.insertion_ghost.position.y
                    + (std::max)(3.0f,
                        (options.insertion_ghost.size.y - line_advance_amount(0.80f))
                            * 0.5f),
                0.80f);
        }
        end_top_layer();
    }

    DockableWindowResult update_dockable_window(
        DockableWindowHostState& host,
        DockableWindowState& state,
        const DockableWindowOptions& options,
        const DockableWindowInput& input) noexcept
    {
        gui_lib::DockableWindowHostState coreHost = to_lib(host);
        gui_lib::DockableWindowState coreState = to_lib(state);
        const gui_lib::DockableWindowResult coreResult = gui_lib::update_dockable_window(
            coreHost,
            coreState,
            to_lib(options),
            to_lib(input));

        from_lib(coreHost, host);
        from_lib(coreState, state);
        return from_lib(coreResult);
    }

    void focus_dockable_window(
        DockableWindowHostState& host,
        DockableWindowState& state) noexcept
    {
        gui_lib::DockableWindowHostState coreHost = to_lib(host);
        gui_lib::DockableWindowState coreState = to_lib(state);
        gui_lib::focus_dockable_window(coreHost, coreState);
        from_lib(coreHost, host);
        from_lib(coreState, state);
    }

    WidgetBounds scene_viewport(std::string_view title, Vec2 position, Vec2 size) noexcept
    {
        WidgetBounds bounds{};
        if (!g_frame.ctx) return bounds;

        try { ensure_resources(); }
        catch (...) { return bounds; }

        const auto& palette = active_palette();
        const float width = (std::max)(0.0f, size.x);
        const float height = (std::max)(0.0f, size.y);
        const float border = 2.0f;

        const bool hasTitleBar = !title.empty();
        const float titleHeight = hasTitleBar ? line_advance_amount(kTitleScale) : 0.0f;
        const float titleBarHeight = hasTitleBar ? (titleHeight + 2.0f * kTitleBarPadding) : 0.0f;
        if (hasTitleBar)
        {
            const float titleTextY = position.y + (titleBarHeight - titleHeight) * 0.5f;
            draw_sprite(palette.titleBar, position.x, position.y, width, titleBarHeight);
            const std::string fittedTitle = fit_text_to_width(
                title,
                (std::max)(1.0f, width - 2.0f * kContentPadding),
                kTitleScale);
            draw_text_line(
                fittedTitle.empty() ? title : std::string_view{ fittedTitle },
                position.x + kContentPadding,
                titleTextY,
                kTitleScale);
        }

        const float topBorderHeight = hasTitleBar ? 0.0f : border;
        const float contentY = position.y + titleBarHeight + topBorderHeight;
        const float contentHeight = (std::max)(0.0f, height - titleBarHeight - topBorderHeight - border);
        const float contentWidth = (std::max)(0.0f, width - border * 2.0f);

        const core::RenderPath renderPath = render_path_for_context(g_frame.ctx);
        const bool needsViewportFallback = !backend_owns_scene_viewport(g_frame.ctx);

        if (needsViewportFallback && contentWidth > 0.0f && contentHeight > 0.0f)
        {
            draw_sprite(palette.consoleBackground, position.x + border, contentY, contentWidth, contentHeight);

            const float inset = 18.0f;
            const float cardX = position.x + border + inset;
            const float cardY = contentY + inset;
            const float cardW = (std::max)(48.0f, contentWidth - inset * 2.0f);
            const float cardH = (std::max)(48.0f, contentHeight - inset * 2.0f);
            draw_sprite(palette.panelBackground, cardX, cardY, cardW, cardH);

            const float previewScale = 1.15f;
            const float lineHeight = line_advance_amount(previewScale);
            draw_text_line(std::string(viewport_fallback_name(g_frame.ctx)) + " Preview", cardX + 16.0f, cardY + 16.0f, previewScale);
            draw_text_line("Scene output pending backend pass", cardX + 16.0f, cardY + 16.0f + lineHeight + 10.0f, kFontScale);
            draw_text_line("GUI remains live in this viewport", cardX + 16.0f, cardY + 16.0f + lineHeight * 2.0f + 18.0f, kFontScale);
        }

        if (contentHeight > 0.0f)
        {
            draw_sprite(palette.panelBackground, position.x, contentY, border, contentHeight);
            draw_sprite(palette.panelBackground,
                position.x + (std::max)(0.0f, width - border),
                contentY,
                border,
                contentHeight);
        }
        if (height > border)
        {
            if (!hasTitleBar)
            {
                draw_sprite(palette.panelBackground,
                    position.x,
                    position.y,
                    width,
                    border);
            }
            draw_sprite(palette.panelBackground,
                position.x,
                position.y + (std::max)(0.0f, height - border),
                width,
                border);
        }

        bounds.position = { position.x + border, contentY };
        bounds.size = { contentWidth, contentHeight };
        return bounds;
    }

    void selection_outline(Vec2 position, Vec2 size, float thickness) noexcept
    {
        if (!g_frame.ctx || size.x <= 0.0f || size.y <= 0.0f)
            return;

        try { ensure_resources(); }
        catch (...) { return; }

        const float border = (std::clamp)(
            thickness,
            1.0f,
            (std::max)(1.0f, (std::min)(size.x, size.y) * 0.5f));
        const SpriteHandle accent = active_palette().buttonActive;
        draw_sprite(accent,
            position.x, position.y, size.x, border);
        draw_sprite(accent,
            position.x, position.y + size.y - border, size.x, border);
        draw_sprite(accent,
            position.x, position.y, border, size.y);
        draw_sprite(accent,
            position.x + size.x - border, position.y, border, size.y);
    }

    void panel_rect(Vec2 position, Vec2 size) noexcept
    {
        if (!g_frame.ctx || size.x <= 0.0f || size.y <= 0.0f)
            return;

        try { ensure_resources(); }
        catch (...) { return; }

        draw_sprite(active_palette().panelBackground, position.x, position.y, size.x, size.y);
    }

    void titlebar_rect(Vec2 position, Vec2 size) noexcept
    {
        if (!g_frame.ctx || size.x <= 0.0f || size.y <= 0.0f)
            return;

        try { ensure_resources(); }
        catch (...) { return; }

        draw_sprite(active_palette().titleBar, position.x, position.y, size.x, size.y);
    }

    void splitter_bar(Vec2 position, Vec2 size, bool hovered, bool active) noexcept
    {
        ensure_resources();
        if (!g_frame.ctx || size.x <= 0.0f || size.y <= 0.0f)
            return;

        const auto& palette = active_palette();
        draw_sprite_raw(palette.windowBackground, position.x, position.y, size.x, size.y);
        // Fixed high-visibility edges plus a central grab rail. The old idle
        // divider was a single title-bar-coloured pixel on a dark background.
        const SpriteHandle rail = active ? palette.buttonActive
            : hovered ? palette.textFieldActive : palette.textFieldActive;
        const bool vertical = size.y >= size.x;
        if (vertical && size.x >= 5.0f)
        {
            const float x = position.x + std::floor(size.x * 0.5f);
            // End the visible rails inside the splitter, not across the
            // neighboring dock headers/corners. The entire 10px divider
            // remains draggable, but no line sticks into an adjoining pane.
            constexpr float endInset = 8.0f;
            const float railHeight = (std::max)(0.0f, size.y - 2.0f * endInset);
            if (railHeight > 0.0f)
            {
                draw_sprite(palette.titleBar, position.x, position.y + endInset, 1.0f, railHeight);
                draw_sprite(rail, x - 1.0f, position.y + endInset, 2.0f, railHeight);
                draw_sprite(palette.titleBar, position.x + size.x - 1.0f,
                    position.y + endInset, 1.0f, railHeight);
            }
            // Centre grip shows that this boundary is draggable.
            for (int n = -1; n <= 1; ++n)
                draw_sprite(palette.buttonActive, x - 3.0f,
                    position.y + size.y * 0.5f + static_cast<float>(n) * 7.0f,
                    6.0f, 2.0f);
        }
        else if (!vertical && size.y >= 5.0f)
        {
            const float y = position.y + std::floor(size.y * 0.5f);
            constexpr float endInset = 8.0f;
            const float railWidth = (std::max)(0.0f, size.x - 2.0f * endInset);
            if (railWidth > 0.0f)
            {
                draw_sprite(palette.titleBar, position.x + endInset, position.y, railWidth, 1.0f);
                draw_sprite(rail, position.x + endInset, y - 1.0f, railWidth, 2.0f);
                draw_sprite(palette.titleBar, position.x + endInset,
                    position.y + size.y - 1.0f, railWidth, 1.0f);
            }
            for (int n = -1; n <= 1; ++n)
                draw_sprite(palette.buttonActive,
                    position.x + size.x * 0.5f + static_cast<float>(n) * 7.0f,
                    y - 3.0f, 2.0f, 6.0f);
        }
    }
