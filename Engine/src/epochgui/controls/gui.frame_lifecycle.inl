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

        bool prevMouseDown = g_frame.mouseDown;
        bool prevMouseRightDown = g_frame.mouseRightDown;
        if (rawCtx)
        {
            if (const auto it = g_contextMouseDownStates.find(rawCtx); it != g_contextMouseDownStates.end())
                prevMouseDown = it->second;
            if (const auto it = g_contextRightMouseDownStates.find(rawCtx); it != g_contextRightMouseDownStates.end())
                prevMouseRightDown = it->second;
        }
        bool currentMouseDown = mouse_down;
        bool currentMouseRightDown = prevMouseRightDown;
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

        const bool nativeContext = rawCtx && (rawCtx->get_hwnd() || rawCtx->windowData);
        const bool keyboardAllowed = !nativeContext || rawCtx->has_input_focus_safe();
        const bool pointerAllowed = keyboardAllowed
            && (!nativeContext || rawCtx->has_pointer_input_safe());
        const bool wheelAllowed = !nativeContext || rawCtx->has_hover_scroll_safe();
        if (filter_frame_input(keyboardAllowed, pointerAllowed, wheelAllowed))
        {
            prevMouseDown = false;
            prevMouseRightDown = false;
            currentMouseDown = false;
            currentMouseRightDown = false;
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
                    currentMouseRightDown = true;
                else
                    currentMouseDown = true;
                currentMousePos = evt.mouse_pos;
                break;
            case EventType::MouseUp:
                if (evt.mouse_button == 1)
                    currentMouseRightDown = false;
                else
                    currentMouseDown = false;
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
            g_contextMouseDownStates[rawCtx] = currentMouseDown;
            g_contextRightMouseDownStates[rawCtx] = currentMouseRightDown;
        }

        reset_frame();
        // Latch capture across the entire input frame even if the popup is
        // closed halfway through painting. A closing click must never fall
        // through to widgets rendered after the dropdown.
        g_frame.selectPopupCapturedThisFrame = any_select_box_open(g_frame.ctx);
        for (auto& [key, state] : g_selectBoxStates) {
            (void)key;
            if (state.owner == rawCtx)
                state.visited_this_frame = false;
        }
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

    Vec2 mouse_position() noexcept
    {
        return g_frame.mousePos;
    }

    bool is_mouse_down() noexcept
    {
        return g_frame.mouseDown && point_in_modal_input_capture(g_frame.mousePos);
    }

    DragSurfaceResult drag_surface(
        DragSurfaceState& state,
        const DragSurfaceOptions& options) noexcept
    {
        DragSurfaceResult result{};
        if (!g_frame.ctx || !options.enabled
            || options.size.x <= 0.0f || options.size.y <= 0.0f)
        {
            if (!g_frame.mouseDown)
                state = {};
            return result;
        }

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
        result.hovered = point_in_rect(
                g_frame.mousePos,
                hitPosition.x,
                hitPosition.y,
                hitWidth,
                hitHeight)
            && point_in_active_clip(g_frame.mousePos);

        if (!state.pending && result.hovered && left_press_available())
        {
            state.press_position = g_frame.mousePos;
            state.pending = true;
            state.dragging = false;
            result.pressed = true;
        }

        if (!state.pending)
            return result;

        result.delta = {
            g_frame.mousePos.x - state.press_position.x,
            g_frame.mousePos.y - state.press_position.y};
        const float threshold = (std::max)(0.0f, options.drag_threshold);
        if (g_frame.mouseDown && !state.dragging
            && result.delta.x * result.delta.x
                + result.delta.y * result.delta.y
                >= threshold * threshold)
        {
            state.dragging = true;
        }
        result.dragging = state.dragging && g_frame.mouseDown;
        if (result.dragging)
            g_frame.mousePressConsumed = true;

        if (g_frame.justReleased)
        {
            result.released = state.dragging;
            if (result.released && left_release_available())
                consume_left_release();
            state = {};
        }
        else if (!g_frame.mouseDown)
        {
            state = {};
        }
        return result;
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

