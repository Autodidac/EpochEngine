// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Engine adapters for the existing reusable floating-window and docking types.
module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

module gui.engine;

import epoch.gui;

namespace epochengine::gui::docking_adapter_detail
{
    [[nodiscard]] gui_lib::Vec2 to_lib(Vec2 v) noexcept
    {
        return { v.x, v.y };
    }

    [[nodiscard]] Vec2 from_lib(gui_lib::Vec2 v) noexcept
    {
        return { v.x, v.y };
    }

    [[nodiscard]] gui_lib::FloatingWindowState to_lib(const FloatingWindowState& state) noexcept
    {
        return gui_lib::FloatingWindowState{
            .position = to_lib(state.position),
            .size = to_lib(state.size),
            .drag_offset = to_lib(state.drag_offset),
            .resize_origin_mouse = to_lib(state.resize_origin_mouse),
            .resize_origin_size = to_lib(state.resize_origin_size),
            .open = state.open,
            .initialized = state.initialized,
            .dragging = state.dragging,
            .resizing = state.resizing,
            .close_pressed = state.close_pressed,
            .focus_order = state.focus_order
        };
    }

    void from_lib(const gui_lib::FloatingWindowState& source, FloatingWindowState& state) noexcept
    {
        state.position = from_lib(source.position);
        state.size = from_lib(source.size);
        state.drag_offset = from_lib(source.drag_offset);
        state.resize_origin_mouse = from_lib(source.resize_origin_mouse);
        state.resize_origin_size = from_lib(source.resize_origin_size);
        state.open = source.open;
        state.initialized = source.initialized;
        state.dragging = source.dragging;
        state.resizing = source.resizing;
        state.close_pressed = source.close_pressed;
        state.focus_order = source.focus_order;
    }

    [[nodiscard]] WidgetBounds from_lib(gui_lib::Rect rect) noexcept
    {
        return WidgetBounds{
            .position = from_lib(rect.position),
            .size = from_lib(rect.size)
        };
    }

    [[nodiscard]] gui_lib::Rect to_lib(WidgetBounds bounds) noexcept
    {
        return gui_lib::Rect{
            .position = to_lib(bounds.position),
            .size = to_lib(bounds.size)
        };
    }

    [[nodiscard]] gui_lib::DockSlot to_lib(DockSlot slot) noexcept
    {
        switch (slot)
        {
        case DockSlot::left: return gui_lib::DockSlot::left;
        case DockSlot::right: return gui_lib::DockSlot::right;
        case DockSlot::top: return gui_lib::DockSlot::top;
        case DockSlot::bottom: return gui_lib::DockSlot::bottom;
        case DockSlot::center: return gui_lib::DockSlot::center;
        case DockSlot::none:
        default: return gui_lib::DockSlot::none;
        }
    }

    [[nodiscard]] DockSlot from_lib(gui_lib::DockSlot slot) noexcept
    {
        switch (slot)
        {
        case gui_lib::DockSlot::left: return DockSlot::left;
        case gui_lib::DockSlot::right: return DockSlot::right;
        case gui_lib::DockSlot::top: return DockSlot::top;
        case gui_lib::DockSlot::bottom: return DockSlot::bottom;
        case gui_lib::DockSlot::center: return DockSlot::center;
        case gui_lib::DockSlot::none:
        default: return DockSlot::none;
        }
    }

    [[nodiscard]] gui_lib::DockGuideTarget to_lib(DockGuideTarget target) noexcept
    {
        switch (target)
        {
        case DockGuideTarget::left_tabs: return gui_lib::DockGuideTarget::left_tabs;
        case DockGuideTarget::right_tabs: return gui_lib::DockGuideTarget::right_tabs;
        case DockGuideTarget::bottom_left_tabs: return gui_lib::DockGuideTarget::bottom_left_tabs;
        case DockGuideTarget::bottom_right_tabs: return gui_lib::DockGuideTarget::bottom_right_tabs;
        case DockGuideTarget::left_context: return gui_lib::DockGuideTarget::left_context;
        case DockGuideTarget::right_context: return gui_lib::DockGuideTarget::right_context;
        case DockGuideTarget::float_window: return gui_lib::DockGuideTarget::float_window;
        case DockGuideTarget::none:
        default: return gui_lib::DockGuideTarget::none;
        }
    }

    [[nodiscard]] DockGuideTarget from_lib(gui_lib::DockGuideTarget target) noexcept
    {
        switch (target)
        {
        case gui_lib::DockGuideTarget::left_tabs: return DockGuideTarget::left_tabs;
        case gui_lib::DockGuideTarget::right_tabs: return DockGuideTarget::right_tabs;
        case gui_lib::DockGuideTarget::bottom_left_tabs: return DockGuideTarget::bottom_left_tabs;
        case gui_lib::DockGuideTarget::bottom_right_tabs: return DockGuideTarget::bottom_right_tabs;
        case gui_lib::DockGuideTarget::left_context: return DockGuideTarget::left_context;
        case gui_lib::DockGuideTarget::right_context: return DockGuideTarget::right_context;
        case gui_lib::DockGuideTarget::float_window: return DockGuideTarget::float_window;
        case gui_lib::DockGuideTarget::none:
        default: return DockGuideTarget::none;
        }
    }

    [[nodiscard]] gui_lib::DockGuideOptions to_lib(const DockGuideOptions& options) noexcept
    {
        return gui_lib::DockGuideOptions{
            .guide_bounds = to_lib(options.guide_bounds),
            .left_tabs_preview = to_lib(options.left_tabs_preview),
            .right_tabs_preview = to_lib(options.right_tabs_preview),
            .bottom_left_tabs_preview = to_lib(options.bottom_left_tabs_preview),
            .bottom_right_tabs_preview = to_lib(options.bottom_right_tabs_preview),
            .left_context_preview = to_lib(options.left_context_preview),
            .right_context_preview = to_lib(options.right_context_preview),
            .floating_preview = to_lib(options.floating_preview),
            .pointer = to_lib(options.pointer),
            .guide_extent = options.guide_extent,
            .guide_gap = options.guide_gap,
            .allow_side_tabs = options.allow_side_tabs,
            .allow_bottom_tabs = options.allow_bottom_tabs,
            .allow_contexts = options.allow_contexts,
            .allow_float = options.allow_float,
            .center_context_guides_in_previews =
                options.center_context_guides_in_previews
        };
    }

    [[nodiscard]] DockGuideLayout from_lib(const gui_lib::DockGuideLayout& source) noexcept
    {
        DockGuideLayout result{};
        result.count = (std::min)(source.count, std::uint32_t{7});
        result.hovered_target = from_lib(source.hovered_target);
        result.hovered_preview = from_lib(source.hovered_preview);
        for (std::uint32_t i = 0; i < result.count; ++i)
        {
            result.guides[i] = DockGuide{
                .target = from_lib(source.guides[i].target),
                .target_bounds = from_lib(source.guides[i].target_bounds),
                .preview_bounds = from_lib(source.guides[i].preview_bounds),
                .hovered = source.guides[i].hovered
            };
        }
        return result;
    }

    [[nodiscard]] gui_lib::DockableWindowMode to_lib(DockableWindowMode mode) noexcept
    {
        switch (mode)
        {
        case DockableWindowMode::docked: return gui_lib::DockableWindowMode::docked;
        case DockableWindowMode::detached: return gui_lib::DockableWindowMode::detached;
        case DockableWindowMode::floating:
        default: return gui_lib::DockableWindowMode::floating;
        }
    }

    [[nodiscard]] DockableWindowMode from_lib(gui_lib::DockableWindowMode mode) noexcept
    {
        switch (mode)
        {
        case gui_lib::DockableWindowMode::docked: return DockableWindowMode::docked;
        case gui_lib::DockableWindowMode::detached: return DockableWindowMode::detached;
        case gui_lib::DockableWindowMode::floating:
        default: return DockableWindowMode::floating;
        }
    }

    [[nodiscard]] gui_lib::DockableWindowAction to_lib(DockableWindowAction action) noexcept
    {
        switch (action)
        {
        case DockableWindowAction::focus: return gui_lib::DockableWindowAction::focus;
        case DockableWindowAction::dock: return gui_lib::DockableWindowAction::dock;
        case DockableWindowAction::float_window: return gui_lib::DockableWindowAction::float_window;
        case DockableWindowAction::detach: return gui_lib::DockableWindowAction::detach;
        case DockableWindowAction::close: return gui_lib::DockableWindowAction::close;
        case DockableWindowAction::none:
        default: return gui_lib::DockableWindowAction::none;
        }
    }

    [[nodiscard]] DockableWindowAction from_lib(gui_lib::DockableWindowAction action) noexcept
    {
        switch (action)
        {
        case gui_lib::DockableWindowAction::focus: return DockableWindowAction::focus;
        case gui_lib::DockableWindowAction::dock: return DockableWindowAction::dock;
        case gui_lib::DockableWindowAction::float_window: return DockableWindowAction::float_window;
        case gui_lib::DockableWindowAction::detach: return DockableWindowAction::detach;
        case gui_lib::DockableWindowAction::close: return DockableWindowAction::close;
        case gui_lib::DockableWindowAction::none:
        default: return DockableWindowAction::none;
        }
    }

    [[nodiscard]] gui_lib::DockableWindowHostState to_lib(const DockableWindowHostState& host) noexcept
    {
        return gui_lib::DockableWindowHostState{
            .active_window_id = host.active_window_id,
            .next_focus_order = host.next_focus_order,
            .changed_this_frame = host.changed_this_frame
        };
    }

    void from_lib(const gui_lib::DockableWindowHostState& source, DockableWindowHostState& host) noexcept
    {
        host.active_window_id = source.active_window_id;
        host.next_focus_order = source.next_focus_order;
        host.changed_this_frame = source.changed_this_frame;
    }

    [[nodiscard]] gui_lib::DockableWindowState to_lib(const DockableWindowState& state) noexcept
    {
        return gui_lib::DockableWindowState{
            .id = state.id,
            .mode = to_lib(state.mode),
            .dock_slot = to_lib(state.dock_slot),
            .floating = to_lib(state.floating),
            .visible = state.visible,
            .initialized = state.initialized,
            .active = state.active,
            .detach_requested = state.detach_requested,
            .close_requested = state.close_requested,
            .focus_order = state.focus_order
        };
    }

    void from_lib(const gui_lib::DockableWindowState& source, DockableWindowState& state) noexcept
    {
        state.id = source.id;
        state.mode = from_lib(source.mode);
        state.dock_slot = from_lib(source.dock_slot);
        from_lib(source.floating, state.floating);
        state.visible = source.visible;
        state.initialized = source.initialized;
        state.active = source.active;
        state.detach_requested = source.detach_requested;
        state.close_requested = source.close_requested;
        state.focus_order = source.focus_order;
    }

    [[nodiscard]] gui_lib::DockableWindowOptions to_lib(const DockableWindowOptions& options) noexcept
    {
        return gui_lib::DockableWindowOptions{
            .title = options.title,
            .docked_frame = to_lib(options.docked_frame),
            .floating = gui_lib::FloatingWindowOptions{
                .default_position = to_lib(options.floating.default_position),
                .default_size = to_lib(options.floating.default_size),
                .min_size = to_lib(options.floating.min_size),
                .viewport_size = to_lib(options.floating.viewport_size),
                .movable = options.floating.movable,
                .resizable = options.floating.resizable,
                .closable = options.floating.closable,
                .constrain_to_viewport = options.floating.constrain_to_viewport
            },
            .viewport_size = to_lib(options.viewport_size),
            .title_bar_height = options.title_bar_height,
            .content_padding = options.content_padding,
            .action_button_width = options.action_button_width,
            .action_button_gap = options.action_button_gap,
            .allow_dock = options.allow_dock,
            .allow_float = options.allow_float,
            .allow_detach = options.allow_detach,
            .allow_close = options.allow_close,
            .fallback_dock_slot = to_lib(options.fallback_dock_slot)
        };
    }

    [[nodiscard]] gui_lib::DockableWindowInput to_lib(const DockableWindowInput& input) noexcept
    {
        return gui_lib::DockableWindowInput{
            .mouse_position = to_lib(input.mouse_position),
            .mouse_down = input.mouse_down,
            .mouse_pressed = input.mouse_pressed,
            .mouse_released = input.mouse_released,
            .requested_action = to_lib(input.requested_action),
            .requested_dock_slot = to_lib(input.requested_dock_slot)
        };
    }

    [[nodiscard]] DockableWindowChrome from_lib(const gui_lib::DockableWindowChrome& chrome) noexcept
    {
        return DockableWindowChrome{
            .frame = from_lib(chrome.frame),
            .title_bar = from_lib(chrome.title_bar),
            .content = from_lib(chrome.content),
            .dock_button = from_lib(chrome.dock_button),
            .float_button = from_lib(chrome.float_button),
            .detach_button = from_lib(chrome.detach_button),
            .close_button = from_lib(chrome.close_button),
            .visible = chrome.visible,
            .hovered = chrome.hovered,
            .title_hovered = chrome.title_hovered,
            .dock_hovered = chrome.dock_hovered,
            .float_hovered = chrome.float_hovered,
            .detach_hovered = chrome.detach_hovered,
            .close_hovered = chrome.close_hovered,
            .active = chrome.active
        };
    }

    [[nodiscard]] DockableWindowResult from_lib(const gui_lib::DockableWindowResult& result) noexcept
    {
        return DockableWindowResult{
            .chrome = from_lib(result.chrome),
            .mode = from_lib(result.mode),
            .action = from_lib(result.action),
            .dock_slot = from_lib(result.dock_slot),
            .changed = result.changed,
            .focused = result.focused,
            .dock_requested = result.dock_requested,
            .float_requested = result.float_requested,
            .detach_requested = result.detach_requested,
            .close_requested = result.close_requested
        };
    }

}
