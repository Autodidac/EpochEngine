// SPDX-License-Identifier: MPL-2.0
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

module epoch.gui.command_menu;

namespace epochengine::gui_lib
{
    namespace
    {
        constexpr std::size_t maximum_items = 1024u;
        constexpr std::size_t maximum_depth = 8u;

        [[nodiscard]] bool finite(Vec2 value) noexcept
        { return std::isfinite(value.x) && std::isfinite(value.y); }

        [[nodiscard]] bool inside(Rect rect, Vec2 point) noexcept
        {
            return finite(point) && rect.size.x > 0.0f && rect.size.y > 0.0f
                && point.x >= rect.position.x && point.y >= rect.position.y
                && point.x < rect.position.x + rect.size.x
                && point.y < rect.position.y + rect.size.y;
        }

        [[nodiscard]] bool selectable(const CommandMenuItemSpec& item) noexcept
        { return item.enabled && !item.label.empty(); }

        [[nodiscard]] std::optional<std::size_t> index_of(
            std::span<const CommandMenuItemSpec> items,
            std::optional<std::uint64_t> id) noexcept
        {
            if (id)
                for (std::size_t index{}; index < items.size(); ++index)
                    if (items[index].id == *id) return index;
            return {};
        }

        [[nodiscard]] std::optional<std::size_t> next_item(
            std::span<const CommandMenuItemSpec> items,
            std::optional<std::size_t> current, int direction, bool wrap) noexcept
        {
            const auto count = static_cast<std::ptrdiff_t>(items.size());
            auto index = current ? static_cast<std::ptrdiff_t>(*current)
                : direction > 0 ? -1 : count;
            for (std::size_t visited{}; visited < items.size(); ++visited)
            {
                index += direction;
                if (index < 0 || index >= count)
                {
                    if (!wrap) break;
                    index = index < 0 ? count - 1 : 0;
                }
                if (selectable(items[static_cast<std::size_t>(index)]))
                    return static_cast<std::size_t>(index);
            }
            return current;
        }

        [[nodiscard]] Rect inner_rect(Rect popup, Vec2 requested_padding) noexcept
        {
            const Vec2 padding{
                (std::min)(requested_padding.x, (std::max)(0.0f, popup.size.x - 1.0f) * 0.5f),
                (std::min)(requested_padding.y, (std::max)(0.0f, popup.size.y - 1.0f) * 0.5f)};
            return {{popup.position.x + padding.x, popup.position.y + padding.y},
                {(std::max)(0.0f, popup.size.x - 2.0f * padding.x),
                    (std::max)(0.0f, popup.size.y - 2.0f * padding.y)}};
        }

        [[nodiscard]] bool valid_tree(std::span<const CommandMenuItemSpec> items,
            std::array<std::uint64_t,maximum_items>& ids, std::size_t& count, std::size_t depth) noexcept
        {
            if(depth>=maximum_depth||items.size()>maximum_items-count) return false;
            for(const auto& item:items)
            {
                if(count>=maximum_items) return false;
                if(!item.id||!std::isfinite(item.preferred_width)||item.preferred_width<0.0f
                    ||(!item.children.empty()&&!item.children.begin())
                    ||std::find(ids.begin(),ids.begin()+count,item.id)!=ids.begin()+count) return false;
                ids[count++]=item.id;
                if(!item.children.empty()&&!valid_tree(item.children,ids,count,depth+1u)) return false;
            }
            return true;
        }

        [[nodiscard]] bool child_contains(const CommandMenuState& state,Vec2 point) noexcept
        {
            for(const auto& child:state.submenu)
                if(child.popup.open&&(inside(child.popup.rect,point)||child_contains(child,point))) return true;
            return false;
        }

        [[nodiscard]] bool enabled_leaf(std::span<const CommandMenuItemSpec> items,std::uint64_t id) noexcept
        {
            for(const auto& item:items)
                if(selectable(item))
                {
                    if(item.id==id) return item.children.empty();
                    if(!item.children.empty()&&enabled_leaf(item.children,id)) return true;
                }
            return false;
        }
    }

    CommandMenuMeasure measure_command_menu(std::span<const CommandMenuItemSpec> items,
        const CommandMenuOptions& options) noexcept
    {
        CommandMenuMeasure result{};
        if (items.empty() || items.size() > maximum_items
            || !finite(options.owner.position) || !finite(options.owner.size)
            || options.owner.size.x < 0.0f || options.owner.size.y < 0.0f
            || !finite(options.viewport_size) || options.viewport_size.x <= 0.0f
            || options.viewport_size.y <= 0.0f || !finite(options.padding)
            || options.padding.x < 0.0f || options.padding.y < 0.0f
            || !std::isfinite(options.row_height) || options.row_height <= 0.0f
            || !std::isfinite(options.minimum_width) || options.minimum_width < 0.0f
            || !std::isfinite(options.maximum_height) || options.maximum_height < 0.0f
            || !std::isfinite(options.scrollbar_width) || options.scrollbar_width <= 0.0f
            || !std::isfinite(options.scrollbar_gap) || options.scrollbar_gap < 0.0f
            || !std::isfinite(options.gap) || options.gap < 0.0f
            || !std::isfinite(options.margin) || options.margin < 0.0f)
            return result;
        float width{};
        for (std::size_t index{}; index < items.size(); ++index)
        {
            const auto& item = items[index];
            if (!item.id || !std::isfinite(item.preferred_width) || item.preferred_width < 0.0f)
                return result;
            // Menus are bounded. This allocation-free check prevents duplicate
            // command identities from changing activation when rows are removed.
            for (std::size_t previous{}; previous < index; ++previous)
                if (items[previous].id == item.id) return result;
            width = (std::max)(width, item.preferred_width);
        }
        result.content_height = static_cast<float>(items.size()) * options.row_height;
        result.preferred_size = {(std::max)(options.minimum_width, width + 2.0f * options.padding.x),
            result.content_height + 2.0f * options.padding.y};
        if (options.maximum_height > 0.0f)
            result.preferred_size.y = (std::min)(result.preferred_size.y, options.maximum_height);
        result.valid = finite(result.preferred_size) && result.preferred_size.x > 0.0f
            && result.preferred_size.y > 0.0f && std::isfinite(result.content_height);
        return result;
    }

    bool command_menu_hover_exit_due(CommandMenuState& state,
        bool keep_alive, float delta_seconds, float delay_seconds) noexcept
    {
        if (!state.popup.open || keep_alive)
        {
            state.hover_exit_seconds = 0.0f;
            return false;
        }
        if (!std::isfinite(delta_seconds) || delta_seconds <= 0.0f
            || !std::isfinite(delay_seconds) || delay_seconds <= 0.0f)
            return false;
        state.hover_exit_seconds = (std::min)(delay_seconds,
            state.hover_exit_seconds + delta_seconds);
        return state.hover_exit_seconds >= delay_seconds;
    }

    void close_command_menu(CommandMenuState& state) noexcept
    {
        state.popup.open = false;
        state.popup.pressed_inside = false;
        state.popup.pressed_owner = false;
        state.focused_id.reset();
        state.pressed_id.reset();
        state.scroll_offset = 0.0f;
        state.hover_exit_seconds = 0.0f;
        state.dragging_scrollbar = false;
        state.drag_grab_offset = 0.0f;
        state.pointer_initialized = false;
        state.open_submenu_id.reset();
        state.submenu.clear();
    }

    static CommandMenuLayout update_flat_command_menu(CommandMenuState& state,
        std::span<const CommandMenuItemSpec> items, const CommandMenuOptions& options,
        const CommandMenuInput& input)
    {
        CommandMenuLayout result{};
        const bool was_open = state.popup.open;
        const auto measure = measure_command_menu(items, options);
        result.valid = measure.valid;
        if (!measure.valid)
        {
            close_command_menu(state);
            result.popup.closed = was_open;
            result.pointer_captured = was_open && (input.popup.mouse_pressed || input.popup.mouse_released);
            return result;
        }
        result.preferred_size = measure.preferred_size;
        result.content_height = measure.content_height;
        PopupOptions popupOptions{.owner = options.owner,
            .preferred_size = measure.preferred_size, .viewport_size = options.viewport_size,
            .placement = options.placement, .gap = options.gap, .margin = options.margin};
        // Ask the existing placement owner whether the body really overflows.
        // Add the scrollbar's width before final placement, never a phantom row.
        const auto initial = place_popup(popupOptions, input.popup);
        const bool overflow = measure.content_height > inner_rect(initial, options.padding).size.y + 0.5f;
        if (overflow)
            popupOptions.preferred_size.x += options.scrollbar_width + options.scrollbar_gap;
        auto popupInput = input.popup;
        const auto press_position = input.press_position.value_or(input.popup.mouse_position);
        if (popupInput.mouse_pressed) popupInput.mouse_position = press_position;
        popupInput.close_requested = popupInput.close_requested || input.focus_lost;
        result.popup = update_popup(state.popup, popupOptions, popupInput);
        if (input.popup.mouse_pressed) state.pressed_id.reset();
        result.popup.hovered = result.popup.visible && inside(result.popup.popup, input.popup.mouse_position);
        result.popup.owner_hovered = inside(result.popup.owner, input.popup.mouse_position);
        result.pointer_captured = (result.popup.visible && result.popup.hovered)
            || (was_open && (input.popup.mouse_pressed || input.popup.mouse_released));
        result.keyboard_captured = was_open && (input.popup.escape_pressed || input.focus_lost);

        const auto activate = [&](std::size_t index)
        {
            if(!items[index].children.empty())
            {
                state.open_submenu_id=items[index].id;
                return;
            }
            result.activated_index = index;
            result.activated_id = items[index].id;
            result.popup.closed = result.popup.closed || state.popup.open;
            result.popup.visible = false;
            result.popup.hovered = false;
            close_command_menu(state);
        };
        if (!input.focus_lost && !input.popup.close_requested && !input.popup.escape_pressed
            && input.shortcut_id)
            if (const auto index = index_of(items, input.shortcut_id); index && selectable(items[*index]))
            {
                activate(*index);
                result.keyboard_captured = true;
                return result;
            }
        if (!result.popup.visible)
        {
            close_command_menu(state);
            return result;
        }

        result.content_clip = inner_rect(result.popup.popup, options.padding);
        result.maximum_scroll_offset = overflow
            ? (std::max)(0.0f, measure.content_height - result.content_clip.size.y) : 0.0f;
        if (result.popup.opened)
        {
            state.scroll_offset = 0.0f;
            state.pressed_id.reset();
            state.dragging_scrollbar = false;
            state.focused_id.reset();
        }
        state.scroll_offset = std::isfinite(state.scroll_offset)
            ? (std::clamp)(state.scroll_offset, 0.0f, result.maximum_scroll_offset) : 0.0f;
        if (!overflow) state.dragging_scrollbar = false;
        auto focused = index_of(items, state.focused_id);
        if (focused && !selectable(items[*focused])) focused.reset();
        if (!focused) focused = next_item(items, {}, 1, false);
        const bool navigation = input.up_pressed || input.down_pressed || input.home_pressed
            || input.end_pressed || input.page_up_pressed || input.page_down_pressed;
        if (input.home_pressed) focused = next_item(items, {}, 1, false);
        else if (input.end_pressed) focused = next_item(items, {}, -1, false);
        else if (input.up_pressed || input.down_pressed)
            focused = next_item(items, focused, input.up_pressed ? -1 : 1, true);
        else if (input.page_up_pressed || input.page_down_pressed)
        {
            const auto steps = static_cast<std::size_t>((std::max)(1.0f,
                std::floor(result.content_clip.size.y / options.row_height)));
            for (std::size_t step{}; step < (std::min)(steps, items.size()); ++step)
                focused = next_item(items, focused, input.page_up_pressed ? -1 : 1, false);
        }
        if (focused && navigation)
        {
            const float top = static_cast<float>(*focused) * options.row_height;
            if (options.row_height > result.content_clip.size.y || top < state.scroll_offset)
                state.scroll_offset = top;
            else if (top + options.row_height > state.scroll_offset + result.content_clip.size.y)
                state.scroll_offset = top + options.row_height - result.content_clip.size.y;
            state.scroll_offset = (std::clamp)(state.scroll_offset, 0.0f, result.maximum_scroll_offset);
        }
        result.keyboard_captured = navigation || input.accept_pressed || input.left_pressed || input.right_pressed;
        result.owner_navigation = input.left_pressed ? -1 : input.right_pressed ? 1 : 0;
        if(input.right_pressed&&focused&&!items[*focused].children.empty())
        {
            state.open_submenu_id=items[*focused].id;
            result.owner_navigation=0;
        }
        if (overflow)
        {
            const float width = (std::min)(options.scrollbar_width, result.content_clip.size.x);
            const Rect track{{result.content_clip.position.x + result.content_clip.size.x - width,
                    result.content_clip.position.y}, {width, result.content_clip.size.y}};
            auto scrollbar = make_scrollbar_layout({.track = track,
                .content_extent = measure.content_height, .scroll_offset = state.scroll_offset});
            if (inside(result.popup.popup, input.popup.mouse_position)
                && std::isfinite(input.wheel_delta) && input.wheel_delta != 0.0f)
            {
                state.scroll_offset = (std::clamp)(state.scroll_offset - input.wheel_delta,
                    0.0f, result.maximum_scroll_offset);
                result.wheel_scrolled = true;
            }
            if (input.popup.mouse_pressed && inside(track, press_position))
            {
                state.dragging_scrollbar = true;
                state.pressed_id.reset();
                state.drag_grab_offset = scrollbar_grab_offset(scrollbar, press_position.y,
                    inside(scrollbar.thumb, press_position));
            }
            if (state.dragging_scrollbar)
            {
                result.pointer_captured = true;
                state.scroll_offset = scrollbar_offset_from_pointer(scrollbar,
                    input.popup.mouse_position.y, state.drag_grab_offset);
            }
            if (input.popup.mouse_released || (!input.pointer_down && !input.popup.mouse_pressed))
                state.dragging_scrollbar = false;
            scrollbar = make_scrollbar_layout({.track = track,
                .content_extent = measure.content_height, .scroll_offset = state.scroll_offset});
            result.scrollbar = scrollbar;
            result.content_clip.size.x = (std::max)(0.0f,
                result.content_clip.size.x - width - options.scrollbar_gap);
        }
        result.scroll_offset = state.scroll_offset;
        result.rows.reserve(items.size());
        const bool pointer_moved = !state.pointer_initialized
            || input.popup.mouse_position.x != state.last_pointer.x
            || input.popup.mouse_position.y != state.last_pointer.y;
        const bool pointer_inside = inside(result.content_clip, input.popup.mouse_position);
        for (std::size_t index{}; index < items.size(); ++index)
        {
            const Rect rect{{result.content_clip.position.x, result.content_clip.position.y
                    + static_cast<float>(index) * options.row_height - state.scroll_offset},
                {result.content_clip.size.x, options.row_height}};
            const bool visible = rect.position.y + rect.size.y > result.content_clip.position.y
                && rect.position.y < result.content_clip.position.y + result.content_clip.size.y;
            const bool hovered = pointer_inside && inside(rect, input.popup.mouse_position);
            if (hovered)
            {
                result.hovered_index = index;
                if (selectable(items[index]) && (pointer_moved || input.popup.mouse_pressed)) focused = index;
            }
            result.rows.push_back({items[index].id, index, rect, visible, hovered, false});
        }
        if (finite(input.popup.mouse_position))
        {
            state.last_pointer = input.popup.mouse_position;
            state.pointer_initialized = true;
        }
        state.focused_id = focused ? std::optional{items[*focused].id} : std::nullopt;
        result.focused_index = focused;
        if (focused) result.rows[*focused].focused = true;
        if (input.popup.mouse_pressed && inside(result.content_clip, press_position))
        {
            state.pressed_id.reset();
            for (const auto& row : result.rows)
                if (inside(row.rect, press_position) && selectable(items[row.item_index]))
                {
                    state.pressed_id = row.id;
                    break;
                }
        }
        if (input.popup.mouse_released)
        {
            if (result.hovered_index && selectable(items[*result.hovered_index])
                && state.pressed_id == std::optional{items[*result.hovered_index].id})
                activate(*result.hovered_index);
            state.pressed_id.reset();
        }
        if (input.accept_pressed && focused && result.popup.visible) activate(*focused);
        return result;
    }

    CommandMenuLayout update_command_menu(CommandMenuState& state,
        std::span<const CommandMenuItemSpec> items, const CommandMenuOptions& options,
        const CommandMenuInput& input)
    {
        std::array<std::uint64_t,maximum_items> ids{};std::size_t count{};
        if(!valid_tree(items,ids,count,0u))
        {
            CommandMenuLayout failed{};failed.popup.closed=state.popup.open;
            failed.pointer_captured=state.popup.open&&(input.popup.mouse_pressed||input.popup.mouse_released);
            close_command_menu(state);return failed;
        }
        if(input.shortcut_id&&!input.focus_lost&&!input.popup.close_requested&&!input.popup.escape_pressed
            &&enabled_leaf(items,*input.shortcut_id))
        {
            CommandMenuLayout activated{};activated.valid=true;activated.activated_id=input.shortcut_id;
            activated.popup.closed=state.popup.open;activated.keyboard_captured=true;
            activated.activated_index=index_of(items,input.shortcut_id);
            close_command_menu(state);return activated;
        }
        const bool hadChild=!state.submenu.empty()&&state.submenu.front().popup.open;
        const auto previousChildOwner=state.open_submenu_id;
        const bool pointerMoved=!state.pointer_initialized||state.last_pointer.x!=input.popup.mouse_position.x
            ||state.last_pointer.y!=input.popup.mouse_position.y;
        auto parentInput=input;parentInput.shortcut_id.reset();
        if(hadChild)
        {
            parentInput.up_pressed=parentInput.down_pressed=parentInput.home_pressed=parentInput.end_pressed=false;
            parentInput.page_up_pressed=parentInput.page_down_pressed=parentInput.accept_pressed=false;
            parentInput.left_pressed=parentInput.right_pressed=false;
            if(child_contains(state,input.press_position.value_or(input.popup.mouse_position)))
                parentInput.popup.mouse_pressed=parentInput.popup.mouse_released=false;
            if(child_contains(state,input.popup.mouse_position)) parentInput.wheel_delta=0.0f;
        }
        auto result=update_flat_command_menu(state,items,options,parentInput);
        if(!result.popup.visible) return result;
        if(result.hovered_index&&(pointerMoved||input.popup.mouse_pressed))
        {
            const auto& hovered=items[*result.hovered_index];
            if(selectable(hovered)&&!hovered.children.empty()) state.open_submenu_id=hovered.id;
            else if(selectable(hovered)) state.open_submenu_id.reset();
        }
        const auto childOwner=index_of(items,state.open_submenu_id);
        if(!childOwner||!selectable(items[*childOwner])||items[*childOwner].children.empty()
            ||*childOwner>=result.rows.size()||!result.rows[*childOwner].visible)
        {
            state.open_submenu_id.reset();state.submenu.clear();return result;
        }
        // The child's focused/pressed IDs must never transfer to a sibling group.
        if(state.open_submenu_id!=previousChildOwner) state.submenu.clear();
        const bool opening=state.submenu.empty()||!state.submenu.front().popup.open;
        if(state.submenu.empty()) state.submenu.emplace_back();
        auto childOptions=options;childOptions.owner=result.rows[*childOwner].rect;
        childOptions.placement=PopupPlacement::right;childOptions.gap=0.0f;
        auto childInput=input;childInput.popup.open_requested=opening;childInput.popup.toggle_requested=false;
        childInput.shortcut_id.reset();
        if(opening)
        {
            childInput.popup.mouse_pressed=childInput.popup.mouse_released=false;
            childInput.up_pressed=childInput.down_pressed=childInput.home_pressed=childInput.end_pressed=false;
            childInput.page_up_pressed=childInput.page_down_pressed=childInput.accept_pressed=false;
            childInput.left_pressed=childInput.right_pressed=false;
        }
        // A click on the parent group belongs to that group, not an outside
        // click closing its already-open child in the same update.
        if(input.popup.mouse_pressed&&inside(childOptions.owner,input.press_position.value_or(input.popup.mouse_position)))
            childInput.popup.mouse_pressed=childInput.popup.mouse_released=false;
        auto child=update_command_menu(state.submenu.front(),items[*childOwner].children,childOptions,childInput);
        result.pointer_captured=result.pointer_captured||child.pointer_captured;
        result.keyboard_captured=result.keyboard_captured||child.keyboard_captured;
        result.wheel_scrolled=result.wheel_scrolled||child.wheel_scrolled;
        if(child.activated_id)
        {
            result.activated_id=child.activated_id;result.activated_index.reset();
            result.popup.visible=false;result.popup.closed=true;close_command_menu(state);return result;
        }
        if(child.owner_navigation<0)
        {
            state.open_submenu_id.reset();state.submenu.clear();return result;
        }
        if(child.owner_navigation>0) result.owner_navigation=child.owner_navigation;
        if(!child.popup.visible)
        {
            state.open_submenu_id.reset();state.submenu.clear();return result;
        }
        result.submenus.push_back(std::move(child));
        return result;
    }
}
