/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module project.gui_runtime;

namespace epochengine::project_gui_runtime
{
    namespace
    {
        [[nodiscard]] bool finite(Float2 value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        [[nodiscard]] constexpr bool contains(Rect bounds, Float2 point) noexcept
        {
            return !bounds.empty() && point.x >= bounds.x && point.y >= bounds.y
                && point.x < bounds.x + bounds.width
                && point.y < bounds.y + bounds.height;
        }

        [[nodiscard]] constexpr Rect intersect(Rect left, Rect right) noexcept
        {
            const float x = (std::max)(left.x, right.x);
            const float y = (std::max)(left.y, right.y);
            const float maximumX = (std::min)(
                left.x + left.width, right.x + right.width);
            const float maximumY = (std::min)(
                left.y + left.height, right.y + right.height);
            return {
                x,
                y,
                (std::max)(0.0f, maximumX - x),
                (std::max)(0.0f, maximumY - y)};
        }

        [[nodiscard]] float clamp_extent(
            float value,
            float minimum,
            float maximum) noexcept
        {
            return (std::clamp)(value, minimum, maximum);
        }

        [[nodiscard]] bool valid_utf8(std::string_view text) noexcept
        {
            std::size_t index{};
            while (index < text.size())
            {
                const auto first = static_cast<std::uint8_t>(text[index]);
                if (first == 0u)
                    return false;
                if (first < 0x80u)
                {
                    ++index;
                    continue;
                }
                std::size_t count{};
                std::uint32_t codepoint{};
                if ((first & 0xe0u) == 0xc0u)
                {
                    count = 2u;
                    codepoint = first & 0x1fu;
                }
                else if ((first & 0xf0u) == 0xe0u)
                {
                    count = 3u;
                    codepoint = first & 0x0fu;
                }
                else if ((first & 0xf8u) == 0xf0u)
                {
                    count = 4u;
                    codepoint = first & 0x07u;
                }
                else
                {
                    return false;
                }
                if (index + count > text.size())
                    return false;
                for (std::size_t offset = 1u; offset < count; ++offset)
                {
                    const auto next = static_cast<std::uint8_t>(text[index + offset]);
                    if ((next & 0xc0u) != 0x80u)
                        return false;
                    codepoint = (codepoint << 6u) | (next & 0x3fu);
                }
                if ((count == 2u && codepoint < 0x80u)
                    || (count == 3u && codepoint < 0x800u)
                    || (count == 4u && codepoint < 0x1'0000u)
                    || codepoint > 0x10'ffffu
                    || (codepoint >= 0xd800u && codepoint <= 0xdfffu))
                {
                    return false;
                }
                index += count;
            }
            return true;
        }
    }

    RuntimeSession::RuntimeSession(RuntimeLimits limits) noexcept
        : limits_(limits)
    {
    }

    RuntimeSession::RuntimeSession(
        asset::gui::CompiledGuiArtifact artifact,
        RuntimeLimits limits) noexcept
        : limits_(limits)
    {
        (void)replace(std::move(artifact));
    }

    bool RuntimeSession::valid() const noexcept
    {
        return valid_ && limits_.valid()
            && static_cast<bool>(artifact_.identity)
            && state_.size() == artifact_.widgets.size();
    }

    const asset::gui::CompiledGuiArtifact* RuntimeSession::artifact() const noexcept
    {
        return valid() ? &artifact_ : nullptr;
    }

    RuntimeMetrics RuntimeSession::metrics() const noexcept
    {
        return metrics_;
    }

    asset::gui::WidgetId RuntimeSession::focused_widget() const noexcept
    {
        return valid() && focused_ < artifact_.widgets.size()
            ? artifact_.widgets[focused_].id
            : asset::gui::WidgetId{};
    }

    RuntimeCode RuntimeSession::replace(
        asset::gui::CompiledGuiArtifact artifact) noexcept
    {
        if (!limits_.valid() || !asset::gui::validate(artifact, limits_.artifact))
            return reject(RuntimeCode::invalid_artifact);
        try
        {
            std::vector<MutableState> next{};
            events_.reserve(limits_.maximum_pending_events);
            next.reserve(artifact.widgets.size());
            for (const auto& widget : artifact.widgets)
            {
                next.push_back({
                    .text = widget.kind == asset::gui::WidgetKind::text_input
                        ? widget.primary_text : std::string{},
                    .slider_value = widget.value,
                    .horizontal_offset = widget.horizontal_offset,
                    .vertical_offset = widget.vertical_offset,
                    .selected_child = widget.selected_child});
            }
            artifact_ = std::move(artifact);
            state_ = std::move(next);
            events_.clear();
            last_frame_ = {};
            focused_ = asset::gui::invalid_widget_index;
            frame_revision_ = 0u;
            valid_ = true;
            ++metrics_.artifact_replacements;
            return RuntimeCode::ready;
        }
        catch (...)
        {
            return reject(RuntimeCode::allocation_failure);
        }
    }

    RuntimeFrame RuntimeSession::build_frame(Float2 viewport) noexcept
    {
        RuntimeFrame frame{};
        frame.viewport = viewport;
        if (!valid())
            return frame;
        if (!finite(viewport) || viewport.x <= 0.0f || viewport.y <= 0.0f)
        {
            frame.code = RuntimeCode::invalid_viewport;
            ++metrics_.rejected_operations;
            return frame;
        }

        try
        {
            frame.scale = (std::min)(
                viewport.x / artifact_.virtual_width,
                viewport.y / artifact_.virtual_height);
            if (!std::isfinite(frame.scale) || frame.scale <= 0.0f)
            {
                frame.code = RuntimeCode::invalid_viewport;
                ++metrics_.rejected_operations;
                return frame;
            }
            const Float2 content{
                artifact_.virtual_width * frame.scale,
                artifact_.virtual_height * frame.scale};
            frame.offset = {
                (viewport.x - content.x) * 0.5f,
                (viewport.y - content.y) * 0.5f};
            frame.widgets.reserve(artifact_.widgets.size());

            const Rect viewportClip{0.0f, 0.0f, viewport.x, viewport.y};
            const Rect rootBounds{
                frame.offset.x, frame.offset.y, content.x, content.y};

            std::function<void(std::uint32_t, Rect, Rect, bool)> project;
            project = [&](std::uint32_t index, Rect bounds, Rect inheritedClip,
                          bool ancestorsVisible)
            {
                const auto& source = artifact_.widgets[index];
                const auto& mutableState = state_[index];
                const bool visible = ancestorsVisible && source.visible
                    && !intersect(bounds, inheritedClip).empty();
                const Rect effectiveClip = intersect(bounds, inheritedClip);
                frame.widgets.push_back({
                    .index = index,
                    .id = source.id,
                    .kind = source.kind,
                    .bounds = bounds,
                    .clip = effectiveClip,
                    .visible = visible,
                    .enabled = source.enabled,
                    .focused = focused_ == index,
                    .accepts_pointer = source.accepts_pointer,
                    .name = source.name,
                    .primary_text = source.kind == asset::gui::WidgetKind::text_input
                        ? std::string_view{mutableState.text}
                        : std::string_view{source.primary_text},
                    .secondary_text = source.secondary_text,
                    .asset_path = source.asset_path,
                    .value = source.kind == asset::gui::WidgetKind::slider
                        ? mutableState.slider_value : source.value,
                    .style = &source.style});

                if (source.children.empty())
                    return;

                const float scale = frame.scale;
                Rect contentBounds{
                    bounds.x + source.layout.padding.left * scale,
                    bounds.y + source.layout.padding.top * scale,
                    (std::max)(0.0f, bounds.width
                        - (source.layout.padding.left
                            + source.layout.padding.right) * scale),
                    (std::max)(0.0f, bounds.height
                        - (source.layout.padding.top
                            + source.layout.padding.bottom) * scale)};
                if (source.kind == asset::gui::WidgetKind::scroll_area)
                {
                    contentBounds.x -= mutableState.horizontal_offset * scale;
                    contentBounds.y -= mutableState.vertical_offset * scale;
                }
                Rect childClip = inheritedClip;
                if (source.layout.clip_children
                    || source.kind == asset::gui::WidgetKind::scroll_area
                    || source.kind == asset::gui::WidgetKind::tab_set)
                {
                    childClip = intersect(inheritedClip, bounds);
                }

                if (source.kind == asset::gui::WidgetKind::tab_set)
                {
                    const float headerHeight = (std::min)(
                        32.0f * scale, contentBounds.height);
                    const float headerWidth = source.children.empty()
                        ? 0.0f
                        : contentBounds.width
                            / static_cast<float>(source.children.size());
                    for (std::size_t childOffset = 0u;
                         childOffset < source.children.size(); ++childOffset)
                    {
                        const auto child = source.children[childOffset];
                        const auto& page = artifact_.widgets[child];
                        frame.tab_headers.push_back({
                            .tab_set = index,
                            .page = child,
                            .bounds = {
                                contentBounds.x + headerWidth
                                    * static_cast<float>(childOffset),
                                contentBounds.y,
                                headerWidth,
                                headerHeight},
                            .label = page.primary_text,
                            .selected = mutableState.selected_child == child,
                            .enabled = visible && source.enabled && page.enabled,
                            .closeable = page.closeable});
                    }
                    Rect pageBounds{
                        contentBounds.x,
                        contentBounds.y + headerHeight,
                        contentBounds.width,
                        (std::max)(0.0f, contentBounds.height - headerHeight)};
                    for (const auto child : source.children)
                    {
                        project(
                            child,
                            pageBounds,
                            childClip,
                            visible && mutableState.selected_child == child);
                    }
                    return;
                }

                const auto child_extent = [&](const asset::gui::CompiledWidget& child,
                                              bool horizontal) noexcept
                {
                    const auto rule = horizontal
                        ? child.layout.width_rule : child.layout.height_rule;
                    const float requested = (horizontal
                        ? child.layout.width : child.layout.height) * scale;
                    const float minimum = (horizontal
                        ? child.layout.minimum_width
                        : child.layout.minimum_height) * scale;
                    const float maximum = (horizontal
                        ? child.layout.maximum_width
                        : child.layout.maximum_height) * scale;
                    return std::pair{rule, clamp_extent(requested, minimum, maximum)};
                };

                if (source.layout.flow == asset::gui::LayoutFlow::horizontal
                    || source.layout.flow == asset::gui::LayoutFlow::vertical)
                {
                    const bool horizontal =
                        source.layout.flow == asset::gui::LayoutFlow::horizontal;
                    const float available = horizontal
                        ? contentBounds.width : contentBounds.height;
                    const float spacing = source.layout.spacing * scale;
                    float fixed = spacing * static_cast<float>(
                        source.children.size() > 1u
                            ? source.children.size() - 1u : 0u);
                    float fillWeight{};
                    for (const auto childIndex : source.children)
                    {
                        const auto& child = artifact_.widgets[childIndex];
                        const auto [rule, extent] = child_extent(child, horizontal);
                        if (rule == asset::gui::SizeRule::fill)
                            fillWeight += (std::max)(1.0f, child.layout.flex_grow);
                        else
                            fixed += extent;
                    }
                    const float remaining = (std::max)(0.0f, available - fixed);
                    float cursor = horizontal ? contentBounds.x : contentBounds.y;
                    for (const auto childIndex : source.children)
                    {
                        const auto& child = artifact_.widgets[childIndex];
                        const auto [mainRule, fixedExtent] =
                            child_extent(child, horizontal);
                        const float mainExtent = mainRule == asset::gui::SizeRule::fill
                            ? remaining * (std::max)(1.0f, child.layout.flex_grow)
                                / (std::max)(1.0f, fillWeight)
                            : fixedExtent;
                        const auto [crossRule, crossExtent] =
                            child_extent(child, !horizontal);
                        const float crossAvailable = horizontal
                            ? contentBounds.height : contentBounds.width;
                        const float resolvedCross = crossRule == asset::gui::SizeRule::fill
                            ? crossAvailable : (std::min)(crossAvailable, crossExtent);
                        Rect childBounds = horizontal
                            ? Rect{cursor, contentBounds.y, mainExtent, resolvedCross}
                            : Rect{contentBounds.x, cursor, resolvedCross, mainExtent};
                        project(childIndex, childBounds, childClip, visible);
                        cursor += mainExtent + spacing;
                    }
                    return;
                }

                for (const auto childIndex : source.children)
                {
                    const auto& child = artifact_.widgets[childIndex];
                    const float x = contentBounds.x + child.layout.x * scale;
                    const float y = contentBounds.y + child.layout.y * scale;
                    const float width = child.layout.width_rule
                            == asset::gui::SizeRule::fill
                        ? (std::max)(0.0f, contentBounds.width
                            - child.layout.x * scale)
                        : clamp_extent(
                            child.layout.width * scale,
                            child.layout.minimum_width * scale,
                            child.layout.maximum_width * scale);
                    const float height = child.layout.height_rule
                            == asset::gui::SizeRule::fill
                        ? (std::max)(0.0f, contentBounds.height
                            - child.layout.y * scale)
                        : clamp_extent(
                            child.layout.height * scale,
                            child.layout.minimum_height * scale,
                            child.layout.maximum_height * scale);
                    project(
                        childIndex,
                        {x, y, width, height},
                        childClip,
                        visible);
                }
            };

            project(artifact_.root, rootBounds, viewportClip, true);
            frame.code = RuntimeCode::ready;
            frame.revision = ++frame_revision_;
            last_frame_ = frame;
            ++metrics_.frames_built;
            return frame;
        }
        catch (...)
        {
            frame.code = RuntimeCode::allocation_failure;
            ++metrics_.rejected_operations;
            return frame;
        }
    }

    RuntimeCode RuntimeSession::pointer_release(Float2 point) noexcept
    {
        if (!valid() || !last_frame_)
            return reject(RuntimeCode::invalid_artifact);
        if (!finite(point))
            return reject(RuntimeCode::invalid_value);

        for (auto iterator = last_frame_.tab_headers.rbegin();
             iterator != last_frame_.tab_headers.rend(); ++iterator)
        {
            if (iterator->enabled && contains(iterator->bounds, point))
            {
                ++metrics_.pointer_hits;
                return select_tab(
                    artifact_.widgets[iterator->tab_set].id,
                    artifact_.widgets[iterator->page].id);
            }
        }

        for (auto iterator = last_frame_.widgets.rbegin();
             iterator != last_frame_.widgets.rend(); ++iterator)
        {
            if (!iterator->visible || !iterator->accepts_pointer
                || !contains(iterator->clip, point)
                || !contains(iterator->bounds, point))
            {
                continue;
            }
            ++metrics_.pointer_hits;
            if (!iterator->enabled)
                return reject(RuntimeCode::disabled_widget);

            const auto index = iterator->index;
            const auto& widget = artifact_.widgets[index];
            const bool needsFocus = widget.focusable && focused_ != index;
            const bool emitsAction = widget.kind == asset::gui::WidgetKind::button
                || widget.kind == asset::gui::WidgetKind::image_button
                || !widget.action.empty();
            const bool changesSlider = widget.kind == asset::gui::WidgetKind::slider;
            const std::size_t eventCount = static_cast<std::size_t>(needsFocus)
                + static_cast<std::size_t>(emitsAction)
                + static_cast<std::size_t>(changesSlider);
            const auto eventLimit = static_cast<std::size_t>(
                limits_.maximum_pending_events);
            if (eventCount > eventLimit
                || events_.size() > eventLimit - eventCount)
            {
                return reject(RuntimeCode::action_limit_exceeded);
            }

            if (needsFocus)
            {
                focused_ = index;
                ++metrics_.focus_changes;
                const auto emitted = emit({
                    .kind = EventKind::focus_changed,
                    .widget = widget.id});
                if (emitted != RuntimeCode::ready)
                    return emitted;
            }
            if (changesSlider)
            {
                const double fraction = iterator->bounds.width <= 0.0f
                    ? 0.0
                    : (std::clamp)(
                        static_cast<double>(
                            (point.x - iterator->bounds.x)
                                / iterator->bounds.width),
                        0.0,
                        1.0);
                const RuntimeCode changed = set_slider(
                    widget.id,
                    widget.minimum
                        + (widget.maximum - widget.minimum) * fraction);
                if (changed != RuntimeCode::ready
                    && changed != RuntimeCode::no_change)
                {
                    return changed;
                }
            }
            if (emitsAction)
            {
                const auto emitted = emit({
                    .kind = EventKind::activated,
                    .widget = widget.id,
                    .action = widget.action});
                if (emitted != RuntimeCode::ready)
                    return emitted;
                ++metrics_.actions_emitted;
            }
            return RuntimeCode::ready;
        }
        return RuntimeCode::no_change;
    }

    bool RuntimeSession::pointer_captured(Float2 point) const noexcept
    {
        if (!valid() || !last_frame_ || !finite(point))
            return false;
        for (auto iterator = last_frame_.tab_headers.rbegin();
             iterator != last_frame_.tab_headers.rend(); ++iterator)
        {
            if (iterator->enabled && contains(iterator->bounds, point))
                return true;
        }
        for (auto iterator = last_frame_.widgets.rbegin();
             iterator != last_frame_.widgets.rend(); ++iterator)
        {
            if (iterator->visible && iterator->enabled
                && iterator->accepts_pointer
                && contains(iterator->clip, point)
                && contains(iterator->bounds, point))
            {
                return true;
            }
        }
        return false;
    }
    bool RuntimeSession::keyboard_captured() const noexcept
    {
        if (!valid() || focused_ >= artifact_.widgets.size()
            || !runtime_visible(focused_))
        {
            return false;
        }
        const auto kind = artifact_.widgets[focused_].kind;
        return kind == asset::gui::WidgetKind::text_input
            || kind == asset::gui::WidgetKind::slider;
    }
    RuntimeCode RuntimeSession::focus(asset::gui::WidgetId widget) noexcept
    {
        const auto index = index_of(widget);
        if (!index)
            return reject(RuntimeCode::invalid_widget);
        const auto& source = artifact_.widgets[*index];
        if (!runtime_visible(*index))
            return reject(RuntimeCode::hidden_widget);
        if (!source.enabled)
            return reject(RuntimeCode::disabled_widget);
        if (!source.focusable)
            return reject(RuntimeCode::wrong_widget_kind);
        if (focused_ == *index)
            return RuntimeCode::no_change;
        if (events_.size() >= limits_.maximum_pending_events)
            return reject(RuntimeCode::action_limit_exceeded);
        focused_ = *index;
        ++metrics_.focus_changes;
        return emit({.kind = EventKind::focus_changed, .widget = source.id});
    }

    RuntimeCode RuntimeSession::focus_next(bool reverse) noexcept
    {
        if (!valid())
            return reject(RuntimeCode::invalid_artifact);
        std::vector<std::uint32_t> candidates{};
        try
        {
            for (std::uint32_t index = 0u; index < artifact_.widgets.size(); ++index)
            {
                const auto& widget = artifact_.widgets[index];
                if (runtime_visible(index) && widget.enabled && widget.focusable)
                    candidates.push_back(index);
            }
            if (candidates.empty())
                return RuntimeCode::no_change;
            std::sort(candidates.begin(), candidates.end(),
                [this](std::uint32_t left, std::uint32_t right)
                {
                    const auto& a = artifact_.widgets[left];
                    const auto& b = artifact_.widgets[right];
                    const auto tabA = a.tab_index < 0
                        ? (std::numeric_limits<std::int32_t>::max)() : a.tab_index;
                    const auto tabB = b.tab_index < 0
                        ? (std::numeric_limits<std::int32_t>::max)() : b.tab_index;
                    return tabA != tabB ? tabA < tabB : a.id < b.id;
                });
            auto current = std::find(candidates.begin(), candidates.end(), focused_);
            std::size_t position{};
            if (current != candidates.end())
                position = static_cast<std::size_t>(current - candidates.begin());
            const std::size_t next = current == candidates.end()
                ? (reverse ? candidates.size() - 1u : 0u)
                : reverse
                    ? (position == 0u ? candidates.size() - 1u : position - 1u)
                    : (position + 1u) % candidates.size();
            return focus(artifact_.widgets[candidates[next]].id);
        }
        catch (...)
        {
            return reject(RuntimeCode::allocation_failure);
        }
    }

    RuntimeCode RuntimeSession::select_tab(
        asset::gui::WidgetId tabSet,
        asset::gui::WidgetId page) noexcept
    {
        const auto tabIndex = index_of(tabSet);
        const auto pageIndex = index_of(page);
        if (!tabIndex || !pageIndex)
            return reject(RuntimeCode::invalid_widget);
        const auto& tab = artifact_.widgets[*tabIndex];
        if (tab.kind != asset::gui::WidgetKind::tab_set
            || artifact_.widgets[*pageIndex].kind
                != asset::gui::WidgetKind::tab_page
            || std::find(tab.children.begin(), tab.children.end(), *pageIndex)
                == tab.children.end())
        {
            return reject(RuntimeCode::wrong_widget_kind);
        }
        if (!tab.visible || !artifact_.widgets[*pageIndex].visible)
            return reject(RuntimeCode::hidden_widget);
        if (!tab.enabled || !artifact_.widgets[*pageIndex].enabled)
            return reject(RuntimeCode::disabled_widget);
        if (state_[*tabIndex].selected_child == *pageIndex)
            return RuntimeCode::no_change;
        if (events_.size() >= limits_.maximum_pending_events)
            return reject(RuntimeCode::action_limit_exceeded);
        state_[*tabIndex].selected_child = *pageIndex;
        ++metrics_.tab_changes;
        return emit({
            .kind = EventKind::tab_changed,
            .widget = tab.id,
            .related = artifact_.widgets[*pageIndex].id,
            .action = tab.action});
    }

    RuntimeCode RuntimeSession::set_text(
        asset::gui::WidgetId widget,
        std::string text) noexcept
    {
        const auto index = index_of(widget);
        if (!index)
            return reject(RuntimeCode::invalid_widget);
        const auto& source = artifact_.widgets[*index];
        if (source.kind != asset::gui::WidgetKind::text_input)
            return reject(RuntimeCode::wrong_widget_kind);
        if (source.read_only)
            return reject(RuntimeCode::read_only);
        if (!valid_utf8(text)
            || text.size() > source.maximum_length
            || text.size() > limits_.maximum_text_input_bytes)
        {
            return reject(RuntimeCode::text_limit_exceeded);
        }
        if (state_[*index].text == text)
            return RuntimeCode::no_change;
        if (events_.size() >= limits_.maximum_pending_events)
            return reject(RuntimeCode::action_limit_exceeded);
        state_[*index].text = std::move(text);
        ++metrics_.text_changes;
        return emit({
            .kind = EventKind::text_changed,
            .widget = source.id,
            .action = source.action,
            .text = state_[*index].text});
    }

    RuntimeCode RuntimeSession::append_text(
        asset::gui::WidgetId widget,
        std::string_view text) noexcept
    {
        const auto index = index_of(widget);
        if (!index)
            return reject(RuntimeCode::invalid_widget);
        if (!valid_utf8(text))
            return reject(RuntimeCode::text_limit_exceeded);
        try
        {
            std::string next = state_[*index].text;
            next.append(text);
            return set_text(widget, std::move(next));
        }
        catch (...)
        {
            return reject(RuntimeCode::allocation_failure);
        }
    }

    RuntimeCode RuntimeSession::erase_text_backward(
        asset::gui::WidgetId widget) noexcept
    {
        const auto index = index_of(widget);
        if (!index)
            return reject(RuntimeCode::invalid_widget);
        const auto& source = artifact_.widgets[*index];
        if (source.kind != asset::gui::WidgetKind::text_input)
            return reject(RuntimeCode::wrong_widget_kind);
        if (source.read_only)
            return reject(RuntimeCode::read_only);
        if (state_[*index].text.empty())
            return RuntimeCode::no_change;
        std::string next = state_[*index].text;
        std::size_t erase = next.size() - 1u;
        while (erase > 0u
            && (static_cast<std::uint8_t>(next[erase]) & 0xc0u) == 0x80u)
        {
            --erase;
        }
        next.erase(erase);
        return set_text(widget, std::move(next));
    }

    RuntimeCode RuntimeSession::set_slider(
        asset::gui::WidgetId widget,
        double value) noexcept
    {
        const auto index = index_of(widget);
        if (!index)
            return reject(RuntimeCode::invalid_widget);
        const auto& source = artifact_.widgets[*index];
        if (source.kind != asset::gui::WidgetKind::slider)
            return reject(RuntimeCode::wrong_widget_kind);
        if (!std::isfinite(value))
            return reject(RuntimeCode::invalid_value);
        if (!source.enabled)
            return reject(RuntimeCode::disabled_widget);
        const double clamped = (std::clamp)(value, source.minimum, source.maximum);
        const double steps = std::round((clamped - source.minimum) / source.step);
        const double snapped = (std::clamp)(
            source.minimum + steps * source.step,
            source.minimum,
            source.maximum);
        if (state_[*index].slider_value == snapped)
            return RuntimeCode::no_change;
        if (events_.size() >= limits_.maximum_pending_events)
            return reject(RuntimeCode::action_limit_exceeded);
        state_[*index].slider_value = snapped;
        ++metrics_.slider_changes;
        return emit({
            .kind = EventKind::slider_changed,
            .widget = source.id,
            .action = source.action,
            .value = snapped});
    }

    std::vector<RuntimeEvent> RuntimeSession::drain_events() noexcept
    {
        return std::exchange(events_, {});
    }

    std::optional<std::uint32_t> RuntimeSession::index_of(
        asset::gui::WidgetId widget) const noexcept
    {
        if (!valid() || !widget)
            return std::nullopt;
        for (std::uint32_t index = 0u; index < artifact_.widgets.size(); ++index)
        {
            if (artifact_.widgets[index].id == widget)
                return index;
        }
        return std::nullopt;
    }

    bool RuntimeSession::runtime_visible(std::uint32_t index) const noexcept
    {
        if (!valid() || index >= artifact_.widgets.size())
            return false;
        if (last_frame_)
        {
            const auto found = std::find_if(
                last_frame_.widgets.begin(), last_frame_.widgets.end(),
                [index](const WidgetView& view) { return view.index == index; });
            return found != last_frame_.widgets.end() && found->visible;
        }

        auto current = index;
        while (current != asset::gui::invalid_widget_index)
        {
            const auto& widget = artifact_.widgets[current];
            if (!widget.visible)
                return false;
            if (widget.parent != asset::gui::invalid_widget_index)
            {
                const auto& parent = artifact_.widgets[widget.parent];
                if (parent.kind == asset::gui::WidgetKind::tab_set
                    && widget.kind == asset::gui::WidgetKind::tab_page
                    && state_[widget.parent].selected_child != current)
                {
                    return false;
                }
            }
            current = widget.parent;
        }
        return true;
    }

    RuntimeCode RuntimeSession::reject(RuntimeCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return code;
    }

    RuntimeCode RuntimeSession::emit(RuntimeEvent event) noexcept
    {
        if (events_.size() >= limits_.maximum_pending_events)
            return reject(RuntimeCode::action_limit_exceeded);
        try
        {
            events_.push_back(std::move(event));
            return RuntimeCode::ready;
        }
        catch (...)
        {
            return reject(RuntimeCode::allocation_failure);
        }
    }
}
