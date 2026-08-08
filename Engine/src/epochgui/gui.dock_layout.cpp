module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

module epoch.gui;

namespace epochengine::gui_lib
{
    namespace
    {
        [[nodiscard]] float sane_or(float value, float fallback) noexcept
        {
            return std::isfinite(value) ? value : fallback;
        }

        [[nodiscard]] float clamp_min(float value, float minimum) noexcept
        {
            return (std::max)(sane_or(value, minimum), minimum);
        }

        [[nodiscard]] Vec2 clamp_size(Vec2 size, Vec2 minimum) noexcept
        {
            return {
                clamp_min(size.x, (std::max)(1.0f, minimum.x)),
                clamp_min(size.y, (std::max)(1.0f, minimum.y))
            };
        }

        [[nodiscard]] Rect sane_rect(Rect rect) noexcept
        {
            rect.position = { sane_or(rect.position.x, 0.0f), sane_or(rect.position.y, 0.0f) };
            rect.size = { clamp_min(rect.size.x, 1.0f), clamp_min(rect.size.y, 1.0f) };
            return rect;
        }

        [[nodiscard]] float dock_fraction(const DockPaneState& pane, const DockLayoutOptions& options) noexcept
        {
            const float base = std::clamp(sane_or(options.edge_fraction, 0.24f), 0.08f, 0.92f);
            const float weight = std::clamp(sane_or(pane.weight, 1.0f), 0.10f, 4.0f);
            return std::clamp(base * weight, 0.05f, 0.95f);
        }

        [[nodiscard]] Rect clamp_rect_to_workspace(Rect rect, const DockLayoutOptions& options) noexcept
        {
            rect = sane_rect(rect);
            const Rect workspace = sane_rect(options.workspace);
            const float visible_margin = (std::max)(1.0f, sane_or(options.visible_margin, 48.0f));

            const float min_x = workspace.position.x + (std::min)(0.0f, workspace.size.x - visible_margin);
            const float min_y = workspace.position.y;
            const float max_x = workspace.position.x + (std::max)(visible_margin, workspace.size.x - visible_margin);
            const float max_y = workspace.position.y + (std::max)(visible_margin, workspace.size.y - visible_margin);

            rect.position.x = std::clamp(sane_or(rect.position.x, workspace.position.x), min_x, max_x);
            rect.position.y = std::clamp(sane_or(rect.position.y, workspace.position.y), min_y, max_y);
            return rect;
        }

        [[nodiscard]] Rect slot_rect(const DockPaneState& pane, const DockLayoutOptions& options) noexcept
        {
            const Rect workspace = sane_rect(options.workspace);
            const Vec2 minimum = clamp_size(pane.min_size, options.min_pane_size);
            const float fraction = dock_fraction(pane, options);

            switch (pane.slot)
            {
            case DockSlot::left:
            {
                const float width = (std::max)(minimum.x, workspace.size.x * fraction);
                return Rect{ workspace.position, { (std::min)(width, workspace.size.x), workspace.size.y } };
            }
            case DockSlot::right:
            {
                const float width = (std::max)(minimum.x, workspace.size.x * fraction);
                const float clamped_width = (std::min)(width, workspace.size.x);
                return Rect{
                    { workspace.position.x + workspace.size.x - clamped_width, workspace.position.y },
                    { clamped_width, workspace.size.y }
                };
            }
            case DockSlot::top:
            {
                const float height = (std::max)(minimum.y, workspace.size.y * fraction);
                return Rect{ workspace.position, { workspace.size.x, (std::min)(height, workspace.size.y) } };
            }
            case DockSlot::bottom:
            {
                const float height = (std::max)(minimum.y, workspace.size.y * fraction);
                const float clamped_height = (std::min)(height, workspace.size.y);
                return Rect{
                    { workspace.position.x, workspace.position.y + workspace.size.y - clamped_height },
                    { workspace.size.x, clamped_height }
                };
            }
            case DockSlot::center:
            case DockSlot::none:
            default:
                return workspace;
            }
        }

        [[nodiscard]] Rect default_popout_rect(
            const DockPaneState& pane,
            const DockLayoutOptions& options,
            std::uint32_t index) noexcept
        {
            const Rect workspace = sane_rect(options.workspace);
            const Vec2 size = clamp_size(options.default_popout_size, pane.min_size);
            const float spacing = (std::max)(0.0f, sane_or(options.popout_spacing, 24.0f));
            const float cascade = static_cast<float>(index % 8U) * spacing;
            return clamp_rect_to_workspace(Rect{
                { workspace.position.x + spacing + cascade, workspace.position.y + spacing + cascade },
                size
            }, options);
        }

        [[nodiscard]] DockPaneState* find_pane(
            DockPaneState* panes,
            std::uint32_t pane_count,
            std::uint32_t pane_id) noexcept
        {
            if (!panes || pane_id == 0U)
                return nullptr;

            for (std::uint32_t i = 0; i < pane_count; ++i)
            {
                if (panes[i].id == pane_id)
                    return &panes[i];
            }

            return nullptr;
        }

        [[nodiscard]] std::uint32_t count_context_windows(
            const DockPaneState* panes,
            std::uint32_t pane_count) noexcept
        {
            if (!panes)
                return 0U;

            std::uint32_t count = 0U;
            for (std::uint32_t i = 0; i < pane_count; ++i)
            {
                if (dock_pane_requests_context_window(panes[i]))
                    ++count;
            }

            return count;
        }

        [[nodiscard]] std::uint32_t hovered_pane_id(
            const DockPaneState* panes,
            std::uint32_t pane_count,
            const DockLayoutOptions& options,
            const DockLayoutInput& input) noexcept
        {
            if (!panes)
                return 0U;

            std::uint32_t best_id = 0U;
            std::uint32_t best_focus = 0U;
            for (std::uint32_t i = 0; i < pane_count; ++i)
            {
                DockPaneLayout layout = make_dock_pane_layout(panes[i], options, input);
                if (!layout.visible || !layout.hovered)
                    continue;

                if (best_id == 0U || panes[i].focus_order >= best_focus)
                {
                    best_id = panes[i].id;
                    best_focus = panes[i].focus_order;
                }
            }

            return best_id;
        }
    }

    bool is_valid_dock_slot(DockSlot slot) noexcept
    {
        return slot == DockSlot::left
            || slot == DockSlot::right
            || slot == DockSlot::top
            || slot == DockSlot::bottom
            || slot == DockSlot::center;
    }

    bool dock_pane_requests_context_window(const DockPaneState& pane) noexcept
    {
        return pane.visible && pane.popped_out;
    }

    DockPaneLayout make_dock_pane_layout(
        const DockPaneState& pane,
        const DockLayoutOptions& options,
        const DockLayoutInput& input) noexcept
    {
        DockPaneLayout layout{};
        layout.id = pane.id;
        layout.slot = pane.slot == DockSlot::none ? DockSlot::center : pane.slot;
        layout.visible = pane.visible;
        layout.popped_out = pane.visible && pane.popped_out;
        layout.docked = pane.visible && !pane.popped_out;
        layout.context_window_requested = dock_pane_requests_context_window(pane);
        layout.active = pane.active;

        if (!layout.visible)
            return layout;

        layout.frame = pane.popped_out ? clamp_rect_to_workspace(pane.popout_rect, options) : slot_rect(pane, options);

        const float title_h = (std::max)(18.0f, sane_or(options.title_bar_height, 28.0f));
        const float padding = (std::max)(0.0f, sane_or(options.content_padding, 6.0f));
        layout.title_bar = Rect{
            layout.frame.position,
            { layout.frame.size.x, (std::min)(title_h, layout.frame.size.y) }
        };
        layout.content = Rect{
            { layout.frame.position.x + padding, layout.frame.position.y + layout.title_bar.size.y + padding },
            {
                (std::max)(1.0f, layout.frame.size.x - 2.0f * padding),
                (std::max)(1.0f, layout.frame.size.y - layout.title_bar.size.y - 2.0f * padding)
            }
        };
        layout.hovered = contains(layout.frame, input.mouse_position);
        return layout;
    }

    void activate_dock_pane(
        DockLayoutState& state,
        DockPaneState* panes,
        std::uint32_t pane_count,
        std::uint32_t pane_id) noexcept
    {
        if (!panes || pane_id == 0U)
            return;

        if (state.next_focus_order == 0U)
            state.next_focus_order = 1U;

        for (std::uint32_t i = 0; i < pane_count; ++i)
        {
            DockPaneState& pane = panes[i];
            if (!pane.visible)
            {
                pane.active = false;
                continue;
            }

            const bool active = pane.id == pane_id;
            pane.active = active;
            if (active)
            {
                pane.focus_order = state.next_focus_order++;
                state.active_pane_id = pane.id;
            }
        }
    }

    void normalize_dock_layout(
        DockLayoutState& state,
        DockPaneState* panes,
        std::uint32_t pane_count,
        const DockLayoutOptions& options) noexcept
    {
        if (state.next_focus_order == 0U)
            state.next_focus_order = 1U;

        if (!panes || pane_count == 0U)
        {
            state.active_pane_id = 0U;
            state.initialized = true;
            return;
        }

        std::uint32_t best_focus = 0U;
        std::uint32_t best_id = 0U;
        for (std::uint32_t i = 0; i < pane_count; ++i)
        {
            DockPaneState& pane = panes[i];
            if (pane.id == 0U)
                pane.id = i + 1U;

            if (!is_valid_dock_slot(pane.slot))
                pane.slot = DockSlot::center;

            pane.min_size = clamp_size(pane.min_size, options.min_pane_size);
            pane.weight = std::clamp(sane_or(pane.weight, 1.0f), 0.10f, 4.0f);

            if (!pane.initialized)
            {
                pane.docked_rect = slot_rect(pane, options);
                pane.popout_rect = default_popout_rect(pane, options, i);
                pane.initialized = true;
            }

            pane.docked_rect = slot_rect(pane, options);
            pane.popout_rect.size = clamp_size(pane.popout_rect.size, pane.min_size);
            pane.popout_rect = clamp_rect_to_workspace(pane.popout_rect, options);
            pane.popout_context_open = dock_pane_requests_context_window(pane);

            if (!pane.visible)
            {
                pane.active = false;
                continue;
            }

            if (pane.focus_order >= state.next_focus_order)
                state.next_focus_order = pane.focus_order + 1U;

            if (pane.active || pane.id == state.active_pane_id || pane.focus_order > best_focus)
            {
                best_focus = pane.focus_order;
                best_id = pane.id;
            }
        }

        if (best_id == 0U)
        {
            for (std::uint32_t i = 0; i < pane_count; ++i)
            {
                if (panes[i].visible)
                {
                    best_id = panes[i].id;
                    break;
                }
            }
        }

        activate_dock_pane(state, panes, pane_count, best_id);
        state.initialized = true;
    }

    DockLayoutResult update_dock_layout(
        DockLayoutState& state,
        DockPaneState* panes,
        std::uint32_t pane_count,
        const DockLayoutOptions& options,
        const DockLayoutInput& input) noexcept
    {
        DockLayoutResult result{};
        normalize_dock_layout(state, panes, pane_count, options);
        const std::uint32_t previous_active = state.active_pane_id;
        const std::uint32_t previous_context_count = count_context_windows(panes, pane_count);

        if (!panes || pane_count == 0U)
            return result;

        std::uint32_t requested_active = input.activate_pane_id;
        if (requested_active == 0U && input.mouse_pressed)
            requested_active = hovered_pane_id(panes, pane_count, options, input);

        if (requested_active != 0U)
        {
            activate_dock_pane(state, panes, pane_count, requested_active);
            result.focus_changed = state.active_pane_id != previous_active;
        }

        if (input.toggle_popout_pane_id != 0U)
        {
            if (DockPaneState* pane = find_pane(panes, pane_count, input.toggle_popout_pane_id))
            {
                if (pane->visible)
                {
                    pane->popped_out = !pane->popped_out;
                    pane->popout_context_open = pane->popped_out;
                    result.changed = true;
                    result.context_windows_changed = true;
                    activate_dock_pane(state, panes, pane_count, pane->id);
                }
            }
        }

        if (input.popout_pane_id != 0U)
        {
            if (DockPaneState* pane = find_pane(panes, pane_count, input.popout_pane_id))
            {
                if (pane->visible && !pane->popped_out)
                {
                    pane->popped_out = true;
                    pane->popout_context_open = true;
                    result.changed = true;
                    result.context_windows_changed = true;
                    activate_dock_pane(state, panes, pane_count, pane->id);
                }
            }
        }

        if (input.redock_pane_id != 0U)
        {
            if (DockPaneState* pane = find_pane(panes, pane_count, input.redock_pane_id))
            {
                if (pane->visible && pane->popped_out)
                {
                    pane->popped_out = false;
                    pane->popout_context_open = false;
                    if (is_valid_dock_slot(input.redock_slot))
                        pane->slot = input.redock_slot;
                    result.changed = true;
                    result.context_windows_changed = true;
                    activate_dock_pane(state, panes, pane_count, pane->id);
                }
            }
        }

        normalize_dock_layout(state, panes, pane_count, options);
        result.active_pane_id = state.active_pane_id;
        result.context_window_count = count_context_windows(panes, pane_count);
        result.focus_changed = result.focus_changed || state.active_pane_id != previous_active;
        result.context_windows_changed = result.context_windows_changed
            || result.context_window_count != previous_context_count;
        result.changed = result.changed || result.focus_changed || result.context_windows_changed;
        return result;
    }

    std::string_view DockLayoutController::name() const noexcept
    {
        return "dock_layout";
    }

    bool DockLayoutController::is_valid_slot(DockSlot slot) const noexcept
    {
        return is_valid_dock_slot(slot);
    }

    bool DockLayoutController::pane_requests_context_window(const DockPaneState& pane) const noexcept
    {
        return dock_pane_requests_context_window(pane);
    }

    DockPaneLayout DockLayoutController::make_pane_layout(
        const DockPaneState& pane,
        const DockLayoutOptions& options,
        const DockLayoutInput& input) const noexcept
    {
        return make_dock_pane_layout(pane, options, input);
    }

    void DockLayoutController::activate_pane(
        DockLayoutState& state,
        DockPaneState* panes,
        std::uint32_t pane_count,
        std::uint32_t pane_id) const noexcept
    {
        activate_dock_pane(state, panes, pane_count, pane_id);
    }

    void DockLayoutController::normalize(
        DockLayoutState& state,
        DockPaneState* panes,
        std::uint32_t pane_count,
        const DockLayoutOptions& options) const noexcept
    {
        normalize_dock_layout(state, panes, pane_count, options);
    }

    DockLayoutResult DockLayoutController::update(
        DockLayoutState& state,
        DockPaneState* panes,
        std::uint32_t pane_count,
        const DockLayoutOptions& options,
        const DockLayoutInput& input) const noexcept
    {
        return update_dock_layout(state, panes, pane_count, options, input);
    }

    const DockLayoutController& dock_layout_controller() noexcept
    {
        static const DockLayoutController controller{};
        return controller;
    }
}
