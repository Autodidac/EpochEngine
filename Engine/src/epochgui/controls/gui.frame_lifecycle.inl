    // Opt-in diagnostic bookkeeping only; never participates in admission.
    struct PointerFrameTraceState
    {
        unsigned frames{};
        bool active{}, query_logged{}, blocked_logged{};
    };
    static thread_local PointerFrameTraceState g_pointerFrameTrace{};

// Epoch feature implementation; included by its owning module only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
    // -----------------------------
    // Exported API implementations
    // -----------------------------

    void set_cursor(Vec2 position) noexcept
    {
        if (!g_frame.insideWindow) return;
        g_frame.cursor = position;
    }

    void advance_cursor(Vec2 delta) noexcept
    {
        if (!g_frame.insideWindow) return;
        g_frame.cursor.x += delta.x;
        g_frame.cursor.y += delta.y;
    }

    void push_input(const InputEvent& e) noexcept
    {
        queue_input_event(g_pendingEvents, e);
    }

    void push_input_for_context(const core::Context* ctx, const InputEvent& e) noexcept
    {
        if (!ctx)
        {
            push_input(e);
            return;
        }

        std::scoped_lock lock(g_contextPendingEventsMutex);
        try
        {
            queue_input_event(g_contextPendingEvents[ctx], e);
        }
        catch (...)
        {
            // Creating the per-context bucket may allocate. Dropping one
            // transient event is safer than terminating the native pump.
        }
    }

    int consume_mouse_wheel_delta() noexcept
    {
        // Scene/canvas consumers use this accessor. UI scroll controls consume
        // their own clipped wheel state; background hover must not zoom a scene.
        if ((g_frame.ctx && !g_frame.ctx->has_input_focus_safe())
            || !point_in_modal_input_capture(g_frame.mousePos))
        {
            g_frame.mouseWheelDelta = 0;
            return 0;
        }

        const int delta = g_frame.mouseWheelDelta;
        g_frame.mouseWheelDelta = 0;
        return delta;
    }

    void cleanup_context(const core::Context* ctx) noexcept
    {
        forget_upload_state(ctx);
        {
            std::scoped_lock lock(g_contextPendingEventsMutex);
            g_contextPendingEvents.erase(ctx);
        }
        std::scoped_lock lock(g_deferredBatchMutex);
        g_deferredDrawBatches.erase(ctx);
        g_topLayerDrawBatches.erase(ctx);
        g_contextMouseDownStates.erase(ctx);
        g_contextRightMouseDownStates.erase(ctx);
        g_contextActiveWidgets.erase(ctx);
        g_contextPressedButtonKeys.erase(ctx);
        const std::string scrollPrefix = std::to_string(reinterpret_cast<std::uintptr_t>(ctx)) + "|";
        for (auto it = g_scrollTextStates.begin(); it != g_scrollTextStates.end();)
        {
            if (it->first.starts_with(scrollPrefix))
                it = g_scrollTextStates.erase(it);
            else
                ++it;
        }
        for (auto it = g_scrollAreaStates.begin(); it != g_scrollAreaStates.end();)
        {
            if (it->first.starts_with(scrollPrefix))
                it = g_scrollAreaStates.erase(it);
            else
                ++it;
        }
        for (auto it = g_selectBoxStates.begin(); it != g_selectBoxStates.end();)
        {
            if (it->first.starts_with(scrollPrefix))
                it = g_selectBoxStates.erase(it);
            else
                ++it;
        }
        for (auto it = g_sourceEditorStates.begin(); it != g_sourceEditorStates.end();)
        {
            if (it->first.starts_with(scrollPrefix))
                it = g_sourceEditorStates.erase(it);
            else
                ++it;
        }
        for (auto it = g_assetGridContextMenuStates.begin();
             it != g_assetGridContextMenuStates.end();)
        {
            if (it->first.starts_with(scrollPrefix))
                it = g_assetGridContextMenuStates.erase(it);
            else
                ++it;
        }
        for (auto it = g_nodeGraphCanvasStates.begin();
             it != g_nodeGraphCanvasStates.end();)
        {
            if (it->first.starts_with(scrollPrefix))
                it = g_nodeGraphCanvasStates.erase(it);
            else
                ++it;
        }
    }

    void refresh_context_resources(const core::Context* ctx) noexcept
    {
        forget_upload_state(ctx);
    }

    std::uint64_t deferred_batch_generation(const core::Context* ctx) noexcept
    {
        if (!ctx || !uses_deferred_gui_batch(ctx))
            return 0;

        std::scoped_lock lock(g_deferredBatchMutex);
        const auto it = g_deferredDrawBatches.find(ctx);
        if (it == g_deferredDrawBatches.end())
            return 0;

        return it->second.generation;
    }

    std::uint64_t top_layer_batch_generation(const core::Context* ctx) noexcept
    {
        if (!ctx)
            return 0;

        std::scoped_lock lock(g_deferredBatchMutex);
        const auto it = g_topLayerDrawBatches.find(ctx);
        if (it == g_topLayerDrawBatches.end())
            return 0;

        return it->second.generation;
    }

    std::uint64_t replayed_top_layer_batch_generation(const core::Context* ctx) noexcept
    {
        if (!ctx)
            return 0;

        std::scoped_lock lock(g_deferredBatchMutex);
        const auto it = g_topLayerDrawBatches.find(ctx);
        if (it == g_topLayerDrawBatches.end())
            return 0;

        return it->second.replayedGeneration;
    }

    bool render_deferred_batch(core::Context* ctx) noexcept
    {
        if (!ctx || !uses_deferred_gui_batch(ctx))
            return false;

        std::shared_ptr<std::vector<QueuedSpriteDraw>> draws;
        {
            std::scoped_lock lock(g_deferredBatchMutex);
            const auto it = g_deferredDrawBatches.find(ctx);
            if (it == g_deferredDrawBatches.end())
                return false;
            draws = it->second.draws;
        }

        if (!draws || draws->empty())
            return false;

        auto atlases = epochengine::atlasmanager::get_atlas_vector_snapshot();
        std::span<const TextureAtlas* const> span(atlases.data(), atlases.size());
        for (const auto& draw : *draws)
            ctx->draw_sprite_safe(draw.handle, span, draw.x, draw.y, draw.w, draw.h);
        return true;
    }

    bool render_top_layer_batch(core::Context* ctx) noexcept
    {
        if (!ctx)
            return false;

        std::shared_ptr<std::vector<QueuedSpriteDraw>> draws;
        std::uint64_t generation = 0;
        {
            std::scoped_lock lock(g_deferredBatchMutex);
            const auto it = g_topLayerDrawBatches.find(ctx);
            if (it == g_topLayerDrawBatches.end())
                return false;
            draws = it->second.draws;
            generation = it->second.generation;
        }

        if (!draws || draws->empty())
            return false;

        auto atlases = epochengine::atlasmanager::get_atlas_vector_snapshot();
        std::span<const TextureAtlas* const> span(atlases.data(), atlases.size());
        for (const auto& draw : *draws)
            ctx->draw_sprite_safe(draw.handle, span, draw.x, draw.y, draw.w, draw.h);
        {
            std::scoped_lock lock(g_deferredBatchMutex);
            const auto it = g_topLayerDrawBatches.find(ctx);
            if (it != g_topLayerDrawBatches.end())
            {
                it->second.replayedGeneration = (std::max)(it->second.replayedGeneration, generation);
            }
        }
        return true;
    }

    // A complete stationary click can arrive between two slow UI frames.
    // Retain both edges without inventing a click for a drag across controls.
    [[nodiscard]] static bool completed_stationary_click(
        std::span<const InputEvent> events, int button, Vec2 finalPosition) noexcept
    {
        bool pressed{};
        Vec2 origin{};
        for (const auto& event : events)
        {
            if (event.mouse_button != button) continue;
            if (event.type == EventType::MouseDown)
            {
                pressed = true;
                origin = event.mouse_pos;
            }
            else if (event.type == EventType::MouseUp && pressed)
            {
                if (origin.x == event.mouse_pos.x && origin.y == event.mouse_pos.y
                    && origin.x == finalPosition.x && origin.y == finalPosition.y)
                    return true;
                pressed = false;
            }
        }
        return false;
    }

    void begin_frame(const std::shared_ptr<core::Context>& ctx, float dt, Vec2 mouse_pos, bool mouse_down) noexcept
    {
        core::Context* rawCtx = ctx.get();
        if (rawCtx)
        {
            try { ensure_backend_upload(*rawCtx); }
            catch (...) { /* GUI optional */ }
        }

        g_frame.ctxShared = ctx;
        g_frame.ctx = rawCtx;
        g_frame.visitedActiveWidget = nullptr;
        g_frame.deltaTime = dt;

        // Constant-time wrap: repeated subtraction can spin for an enormous
        // number of iterations when an interrupted frame has an extreme dt.
        if (std::isfinite(dt) && dt >= 0.0f && std::isfinite(g_frame.caretTimer))
            g_frame.caretTimer = std::fmod(
                std::fmod(g_frame.caretTimer, kCaretBlinkPeriod)
                    + std::fmod(dt, kCaretBlinkPeriod), kCaretBlinkPeriod);
        else
            g_frame.caretTimer = 0.0f;

        g_frame.caretVisible = g_frame.caretTimer < (kCaretBlinkPeriod * 0.5f);

        ContextPointerButtonState leftButton{.down = !rawCtx && g_frame.mouseDown};
        ContextPointerButtonState rightButton{.down = !rawCtx && g_frame.mouseRightDown};
        if (rawCtx)
        {
            if (const auto it = g_contextMouseDownStates.find(rawCtx); it != g_contextMouseDownStates.end())
                leftButton = it->second;
            if (const auto it = g_contextRightMouseDownStates.find(rawCtx); it != g_contextRightMouseDownStates.end())
                rightButton = it->second;
        }
        bool prevMouseDown = leftButton.down;
        bool prevMouseRightDown = rightButton.down;
        // Once this context receives queued button edges, they own both held
        // and released state until context cleanup. A stale platform sample
        // must not invent a release or another press after Up/cancellation.
        // Contexts without queued edges retain the sampler-only fallback.
        bool currentMouseDown = leftButton.queued_edge_owned ? leftButton.down : mouse_down;
        bool currentMouseRightDown = rightButton.down;
        Vec2 currentMousePos = mouse_pos;
        g_frame.mouseWheelDelta = 0;

        g_frame.events.clear();
        if (!g_pendingEvents.empty())
        {
            g_frame.events.insert(g_frame.events.end(), g_pendingEvents.begin(), g_pendingEvents.end());
            g_pendingEvents.clear();
        }

        if (rawCtx)
        {
            std::scoped_lock lock(g_contextPendingEventsMutex);
            if (const auto it = g_contextPendingEvents.find(rawCtx); it != g_contextPendingEvents.end() && !it->second.empty())
            {
                g_frame.events.insert(g_frame.events.end(), it->second.begin(), it->second.end());
                it->second.clear();
            }
        }

        std::size_t traceDown{}, traceUp{}, traceMove{}, traceFocusLoss{};
        if (core::cli::trace_menu_button0_rect)
            for (const auto& event : g_frame.events)
            {
                traceDown += event.type == EventType::MouseDown;
                traceUp += event.type == EventType::MouseUp;
                traceMove += event.type == EventType::MouseMove;
                traceFocusLoss += event.type == EventType::FocusLost;
            }
        const bool nativeContext = rawCtx && (rawCtx->get_hwnd() || rawCtx->windowData);
        const bool keyboardAllowed = !nativeContext || rawCtx->has_input_focus_safe();
        g_frame.keyboardFocusCancelled = !keyboardAllowed
            || std::ranges::any_of(g_frame.events, [](const InputEvent& event) noexcept
            { return event.type == EventType::FocusLost; });
        const bool pointerAllowed = keyboardAllowed
            && (!nativeContext || rawCtx->has_pointer_input_safe());
        const bool wheelAllowed = !nativeContext || rawCtx->has_hover_scroll_safe();
        if (filter_frame_input(keyboardAllowed, pointerAllowed, wheelAllowed))
        {
            prevMouseDown = false;
            prevMouseRightDown = false;
            currentMouseDown = false;
            currentMouseRightDown = false;
            leftButton.down = false;
            rightButton.down = false;
            if (rawCtx)
                g_contextPressedButtonKeys[rawCtx] = 0;
        }

        for (const auto& evt : g_frame.events)
        {
            switch (evt.type)
            {
            case EventType::MouseMove: currentMousePos = evt.mouse_pos; break;
            case EventType::MouseDown:
                if (evt.mouse_button == 1)
                {
                    currentMouseRightDown = true;
                    rightButton.queued_edge_owned = true;
                }
                else
                {
                    currentMouseDown = true;
                    leftButton.queued_edge_owned = true;
                }
                currentMousePos = evt.mouse_pos;
                break;
            case EventType::MouseUp:
                if (evt.mouse_button == 1)
                {
                    currentMouseRightDown = false;
                    rightButton.queued_edge_owned = true;
                }
                else
                {
                    currentMouseDown = false;
                    leftButton.queued_edge_owned = true;
                }
                currentMousePos = evt.mouse_pos;
                break;
            case EventType::MouseWheel:
                currentMousePos = evt.mouse_pos;
                g_frame.mouseWheelDelta += evt.wheel_delta;
                break;
            default: break;
            }
        }

        g_frame.prevMouseDown = prevMouseDown;
        g_frame.mouseDown = currentMouseDown;
        g_frame.prevMouseRightDown = prevMouseRightDown;
        g_frame.mouseRightDown = currentMouseRightDown;
        g_frame.mousePos = currentMousePos;
        const bool leftTap = !prevMouseDown && !currentMouseDown
            && completed_stationary_click(g_frame.events, 0, currentMousePos);
        const bool rightTap = !prevMouseRightDown && !currentMouseRightDown
            && completed_stationary_click(g_frame.events, 1, currentMousePos);
        g_frame.justPressed = (!prevMouseDown && currentMouseDown) || leftTap;
        g_frame.justReleased = (prevMouseDown && !currentMouseDown) || leftTap;
        g_frame.rightJustPressed = (!prevMouseRightDown && currentMouseRightDown) || rightTap;
        g_frame.rightJustReleased = (prevMouseRightDown && !currentMouseRightDown) || rightTap;
        if (g_frame.rightJustPressed)
            retire_context_menus_for_right_press();
        g_frame.queuedDraws.clear();
        g_frame.topLayerDraws.clear();
        g_frame.pendingSelectPopups.clear();
        g_frame.topLayerDepth = 0;

        if (rawCtx)
        {
            if (g_frame.justPressed)
                g_contextPressedButtonKeys[rawCtx] = 0;
            g_contextMouseDownStates[rawCtx] = {currentMouseDown, leftButton.queued_edge_owned};
            g_contextRightMouseDownStates[rawCtx] = {currentMouseRightDown, rightButton.queued_edge_owned};
        }

        reset_frame();
        // Latch capture across the entire input frame even if the popup is
        // closed halfway through painting. A closing click must never fall
        // through to widgets rendered after the dropdown.
        g_frame.selectPopupCapturedThisFrame = any_select_box_open(g_frame.ctx);
        g_pointerFrameTrace.active = core::cli::trace_menu_button0_rect
            && g_pointerFrameTrace.frames < 120u
            && (traceDown || traceUp || traceFocusLoss || mouse_down
                || prevMouseDown || prevMouseRightDown || currentMouseDown || currentMouseRightDown);
        g_pointerFrameTrace.query_logged = g_pointerFrameTrace.blocked_logged = false;
        if (g_pointerFrameTrace.active)
        {
            ++g_pointerFrameTrace.frames;
            try
            {
                epochengine::logger::info("PointerTrace.Gui", "frame=" + std::to_string(g_pointerFrameTrace.frames)
                    + " ctx=" + std::to_string(reinterpret_cast<std::uintptr_t>(rawCtx))
                    + " sampled_left=" + std::to_string(mouse_down)
                    + " previous=" + std::to_string(prevMouseDown) + "," + std::to_string(prevMouseRightDown)
                    + " queued=" + std::to_string(leftButton.queued_edge_owned) + "," + std::to_string(rightButton.queued_edge_owned)
                    + " held=" + std::to_string(currentMouseDown) + "," + std::to_string(currentMouseRightDown)
                    + " edges=" + std::to_string(g_frame.justPressed) + "," + std::to_string(g_frame.justReleased)
                    + " events_down_up_move_focus=" + std::to_string(traceDown) + "," + std::to_string(traceUp)
                        + "," + std::to_string(traceMove) + "," + std::to_string(traceFocusLoss)
                    + " focus_pointer=" + std::to_string(keyboardAllowed) + "," + std::to_string(pointerAllowed)
                    + " point=" + std::to_string(currentMousePos.x) + "," + std::to_string(currentMousePos.y)
                    + " selection_capture=" + std::to_string(g_frame.selectPopupCapturedThisFrame));
            }
            catch (...) { /* Diagnostics cannot change input. */ }
        }
        for (auto& [key, state] : g_selectBoxStates) {
            (void)key;
            if (state.owner == rawCtx)
                state.visited_this_frame = false;
        }
        reset_edit_box_visits(rawCtx);
    }

    static void render_pending_select_popups() noexcept;

    void end_frame() noexcept
    {
        // Closed tabs/windows must not retain a stale invisible input owner.
        for (auto& [key, state] : g_selectBoxStates) {
            (void)key;
            if (state.owner == g_frame.ctx && !state.visited_this_frame) {
                state.open = false;
                state.alignSelectedOnOpen = false;
            }
        }
        render_pending_select_popups();
        retire_hidden_edit_boxes(g_frame.ctx);
        // A text/document owner that was not rendered or dispatched in this
        // context's frame is hidden. Release only its input claim; retain its
        // text history/caret and every other context's active control.
        if (const auto active = g_contextActiveWidgets.find(g_frame.ctx);
            active != g_contextActiveWidgets.end()
            && active->second != g_frame.visitedActiveWidget)
            active->second = nullptr;
        flush_queued_draws();
        g_frame.ctxShared.reset();
        g_frame.ctx = nullptr;
        g_frame.insideWindow = false;
        g_frame.events.clear();
        g_frame.justPressed = false;
        g_frame.justReleased = false;
        g_frame.rightJustPressed = false;
        g_frame.rightJustReleased = false;
        g_frame.mousePressConsumed = false;
        g_frame.mouseReleaseConsumed = false;
        g_frame.rightPressConsumed = false;
        g_frame.rightReleaseConsumed = false;
        g_frame.modalInput.clear();
        g_frame.modalHeldEvents.clear();
        g_frame.modalHeldWheel = 0;
        g_frame.modalInputSuspended = false;
        g_frame.modalOwnerDrawn = false;
        g_frame.modalOwnerDrawing = false;
        g_frame.mouseWheelDelta = 0;
        g_frame.pendingSelectPopups.clear();
        g_frame.topLayerDepth = 0;
    }

    ScopedInputSuppression::ScopedInputSuppression(bool suppress) noexcept
        : gate_{g_frame.modalInput, suppress}, active_{suppress}
    {
        if (!active_) return;
        events_.swap(g_frame.events);
        wheel_ = g_frame.mouseWheelDelta;
        mouseDown_ = g_frame.mouseDown;
        previousMouseDown_ = g_frame.prevMouseDown;
        mouseRightDown_ = g_frame.mouseRightDown;
        previousMouseRightDown_ = g_frame.prevMouseRightDown;
        justPressed_ = g_frame.justPressed;
        justReleased_ = g_frame.justReleased;
        rightJustPressed_ = g_frame.rightJustPressed;
        rightJustReleased_ = g_frame.rightJustReleased;
        g_frame.mouseWheelDelta = 0;
        g_frame.mouseDown = g_frame.prevMouseDown = false;
        g_frame.mouseRightDown = g_frame.prevMouseRightDown = false;
        g_frame.justPressed = g_frame.justReleased = false;
        g_frame.rightJustPressed = g_frame.rightJustReleased = false;
    }

    ScopedInputSuppression::~ScopedInputSuppression() noexcept
    {
        if (!active_) return;
        events_.swap(g_frame.events);
        g_frame.mouseWheelDelta = wheel_;
        g_frame.mouseDown = mouseDown_;
        g_frame.prevMouseDown = previousMouseDown_;
        g_frame.mouseRightDown = mouseRightDown_;
        g_frame.prevMouseRightDown = previousMouseRightDown_;
        g_frame.justPressed = justPressed_;
        g_frame.justReleased = justReleased_;
        g_frame.rightJustPressed = rightJustPressed_;
        g_frame.rightJustReleased = rightJustReleased_;
    }

    Vec2 mouse_position() noexcept
    {
        return g_frame.mousePos;
    }

    std::optional<Vec2> mouse_press_position() noexcept
    {
        const auto trace_query = [&](bool admitted, std::string_view reason) noexcept
        {
            if (!g_pointerFrameTrace.active
                || (g_pointerFrameTrace.query_logged && (admitted || g_pointerFrameTrace.blocked_logged))) return;
            g_pointerFrameTrace.query_logged = true;
            g_pointerFrameTrace.blocked_logged |= !admitted;
            try
            {
                epochengine::logger::info("PointerTrace.Query", "frame=" + std::to_string(g_pointerFrameTrace.frames)
                    + " ctx=" + std::to_string(reinterpret_cast<std::uintptr_t>(g_frame.ctx))
                    + " admitted=" + std::to_string(admitted) + " reason=" + std::string{reason}
                    + " consumed=" + std::to_string(g_frame.mousePressConsumed)
                    + " pointer_allowed=" + std::to_string(g_frame.pointerInputAllowed)
                    + " selection=" + std::to_string(g_frame.selectPopupCapturedThisFrame) + "," + std::to_string(g_frame.selectPopupOwnsInput)
                    + " menu=" + std::to_string(g_frame.menuPopupBlocksBackground) + "," + std::to_string(g_frame.menuPopupOwnsInput)
                    + " modal_allows=" + std::to_string(point_in_modal_input_capture(g_frame.mousePos))
                    + " clip=" + std::to_string(g_frame.insideWindow) + ":"
                        + std::to_string(g_frame.contentMin.x) + "," + std::to_string(g_frame.contentMin.y)
                        + "," + std::to_string(g_frame.contentMax.x) + "," + std::to_string(g_frame.contentMax.y));
            }
            catch (...) { /* Diagnostics cannot change input. */ }
        };
        if (!g_frame.ctx || g_frame.mousePressConsumed)
        { trace_query(false, g_frame.ctx ? "consumed" : "no_context"); return {}; }
        bool queuedPress = false;
        for (const auto& event : g_frame.events)
        {
            if (event.type != EventType::MouseDown || event.mouse_button != 0)
                continue;
            queuedPress = true;
            if (std::isfinite(event.mouse_pos.x) && std::isfinite(event.mouse_pos.y)
                && point_in_modal_input_capture(event.mouse_pos))
            { trace_query(true, "queued_down"); return event.mouse_pos; }
        }
        if (!queuedPress && left_press_available()
            && std::isfinite(g_frame.mousePos.x) && std::isfinite(g_frame.mousePos.y))
        { trace_query(true, "sampled_press"); return g_frame.mousePos; }
        trace_query(false, queuedPress ? "queued_down_blocked" : "no_press");
        return {};
    }

    bool is_mouse_down() noexcept
    {
        return g_frame.mouseDown && point_in_modal_input_capture(g_frame.mousePos);
    }

    bool drag_surface_hit_test(Vec2 position,
        const DragSurfaceOptions& options) noexcept
    {
        if (!g_frame.ctx || !options.enabled
            || options.size.x <= 0.0f || options.size.y <= 0.0f)
            return false;
        const float padding = (std::max)(0.0f, options.hit_padding);
        const float minimumExtent = (std::max)(1.0f, options.minimum_hit_extent);
        const float hitWidth = (std::max)(
            options.size.x + padding * 2.0f,
            minimumExtent);
        const float hitHeight = (std::max)(
            options.size.y + padding * 2.0f,
            minimumExtent);
        const Vec2 hitPosition{
            options.position.x - (hitWidth - options.size.x) * 0.5f,
            options.position.y - (hitHeight - options.size.y) * 0.5f};
        return point_in_rect(
                position,
                hitPosition.x,
                hitPosition.y,
                hitWidth,
                hitHeight)
            && point_in_active_clip(position);
    }

    DragSurfaceResult drag_surface(
        DragSurfaceState& state,
        const DragSurfaceOptions& options) noexcept
    {
        DragSurfaceResult result{};
        if (!g_frame.ctx || !options.enabled
            || options.size.x <= 0.0f || options.size.y <= 0.0f)
        {
            if (!g_frame.mouseDown) state = {};
            return result;
        }
        result.hovered = drag_surface_hit_test(g_frame.mousePos, options);

        if (!point_in_modal_input_capture(g_frame.mousePos)
            || (!state.pending && g_frame.mousePressConsumed)
            || (g_frame.mouseReleaseConsumed && !g_frame.mouseDown))
        {
            state = {};
            return result;
        }

        gui_lib::input::PointerDragState owned{
            {state.press_position.x, state.press_position.y}, state.pending, state.dragging};
        const auto advance = [&](Vec2 position, bool down, bool pressed,
            bool released, bool cancelled = false)
        {
            const auto update = gui_lib::input::update_pointer_drag(owned, {
                .position = {position.x, position.y}, .down = down,
                .pressed = pressed, .released = released,
                .press_allowed = !g_frame.mousePressConsumed
                    && drag_surface_hit_test(position, options),
                .cancelled = cancelled}, options.drag_threshold);
            result.delta = {update.delta.x, update.delta.y};
            result.pressed |= update.pressed;
            result.dragging = update.dragging;
            result.released |= update.released;
        };

        // Button clicks retain their existing stationary-edge contract. Drags
        // additionally need the admitted press origin and intermediate motion,
        // even when Down/Move/Up all arrive before the next GUI frame.
        // Focus filtering may remove its cancellation event but retain a later
        // release. A reset previous-held state cancels the old gesture before
        // admitting any real fresh press remaining in this queue.
        if (!g_frame.prevMouseDown && !g_frame.events.empty()) owned = {};
        bool down = owned.pending;
        bool sawPress = false;
        bool sawRelease = false;
        for (const auto& event : g_frame.events)
        {
            if (event.type == EventType::FocusLost)
            {
                advance(event.mouse_pos, false, false, false, true);
                down = false;
            }
            else if (event.type == EventType::MouseDown && event.mouse_button == 0)
            {
                sawPress = true;
                down = true;
                advance(event.mouse_pos, true, true, false);
            }
            else if (event.type == EventType::MouseUp && event.mouse_button == 0)
            {
                sawRelease = true;
                down = false;
                advance(event.mouse_pos, false, false, true);
            }
            else if (event.type == EventType::MouseMove && owned.pending)
                advance(event.mouse_pos, down, false, false);
            if (result.released) break;
        }
        if (!result.released)
            advance(g_frame.mousePos, g_frame.mouseDown,
                g_frame.justPressed && !sawPress, g_frame.justReleased && !sawRelease);
        state = {{owned.press_position.x, owned.press_position.y}, owned.pending, owned.dragging};
        if (result.dragging || result.released)
            g_frame.mousePressConsumed = true;
        if (result.released)
            g_frame.mouseReleaseConsumed = true;
        return result;
    }

    bool is_mouse_over_content_clip() noexcept
    {
        return point_in_active_clip(g_frame.mousePos);
    }

    bool was_mouse_pressed() noexcept
    {
        return left_press_available();
    }

    bool was_mouse_released() noexcept
    {
        return left_release_available();
    }

    bool is_mouse_right_down() noexcept
    {
        return g_frame.mouseRightDown && point_in_modal_input_capture(g_frame.mousePos);
    }

    bool was_mouse_right_pressed() noexcept
    {
        return right_press_available();
    }

    bool was_mouse_right_released() noexcept
    {
        return right_release_available();
    }
    bool select_box_popup_open() noexcept
    { return any_select_box_open(g_frame.ctx); }
    bool keyboard_input_captured() noexcept
    {
        // A modal owns the complete interaction channel, not only pointer
        // coordinates inside its panel. Editor camera/navigation shortcuts
        // must not leak through while a modal text field is idle.
        if (g_frame.modalInput.keyboard_captured())
            return true;
        if (!g_frame.ctx)
            return any_select_box_open(g_frame.ctx);
        const void* contextKey = static_cast<const void*>(g_frame.ctx);
        const auto active = g_contextActiveWidgets.find(contextKey);
        return (active != g_contextActiveWidgets.end() && active->second != nullptr)
            || any_select_box_open(g_frame.ctx);
    }

    static void suspend_covered_input() noexcept
    {
        if (g_frame.modalInputSuspended)
            return;
        g_frame.modalHeldEvents = std::move(g_frame.events);
        g_frame.events.clear();
        g_frame.modalHeldWheel = g_frame.mouseWheelDelta;
        g_frame.mouseWheelDelta = 0;
        g_frame.modalInputSuspended = true;
    }

    void begin_modal_input_capture(Vec2 position, Vec2 size) noexcept
    {
        // Plain bounded capture is also used by floating windows. Only
        // exclusive modals suppress all input before rendering their owner.
        g_frame.modalInput.begin({
            { position.x, position.y },
            { size.x, size.y }
        });
    }

    void begin_modal_background_capture(Vec2 position, Vec2 size) noexcept
    {
        begin_modal_input_capture(position, size);
        g_frame.modalInput.require_owner_scope();
        suspend_covered_input();
    }

    void block_input_until_clear() noexcept
    {
        g_frame.modalInput.block_until_clear();
    }

    void block_background_for_popup() noexcept
    {
        g_frame.menuPopupBlocksBackground = true;
    }

    void allow_background_for_popup() noexcept
    {
        // The application chrome gets a narrow input phase for switching menus.
        g_frame.menuPopupBlocksBackground = false;
    }

    void begin_application_popup_input() noexcept
    {
        // Only the owning application popup is allowed; unrelated floating
        // top-layer windows remain blocked even if rendered earlier.
        g_frame.menuPopupOwnsInput = true;
    }

    void end_application_popup_input() noexcept
    {
        g_frame.menuPopupOwnsInput = false;
    }

    void clear_modal_input_capture() noexcept
    {
        // Clear once before setting up the frame's topmost modal. Never
        // release a closing click to the background in the same frame.
        g_frame.modalInput.clear();
        g_frame.modalHeldEvents.clear();
        g_frame.modalHeldWheel = 0;
        g_frame.modalInputSuspended = false;
        g_frame.modalOwnerDrawn = false;
        g_frame.modalOwnerDrawing = false;
    }

