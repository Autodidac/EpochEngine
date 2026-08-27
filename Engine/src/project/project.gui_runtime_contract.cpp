/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

module project.gui_runtime;

namespace epochengine::project_gui_runtime
{
    namespace
    {
        [[nodiscard]] asset::gui::CompiledWidget widget(
            std::uint64_t id,
            asset::gui::WidgetKind kind,
            std::string name)
        {
            asset::gui::CompiledWidget result{};
            result.id = {id};
            result.kind = kind;
            result.name = std::move(name);
            return result;
        }

        [[nodiscard]] asset::gui::CompiledGuiArtifact artifact()
        {
            using Kind = asset::gui::WidgetKind;
            using Rule = asset::gui::SizeRule;

            asset::gui::CompiledGuiArtifact result{};
            for (std::size_t index = 0u;
                 index < result.identity.source_revision.content.bytes.size();
                 ++index)
            {
                result.identity.source_revision.content.bytes[index] =
                    static_cast<std::uint8_t>(index + 1u);
            }
            result.identity.source_revision.sequence = 7u;
            result.name = "RuntimeGui";
            result.virtual_width = 800.0f;
            result.virtual_height = 600.0f;
            result.root = 0u;

            auto root = widget(1u, Kind::canvas, "Root");
            root.children = {1u};
            root.layout.width = 800.0f;
            root.layout.height = 600.0f;

            auto panel = widget(2u, Kind::panel, "Controls");
            panel.parent = 0u;
            panel.children = {2u, 3u, 4u, 5u};
            panel.layout.flow = asset::gui::LayoutFlow::vertical;
            panel.layout.width_rule = Rule::fill;
            panel.layout.height_rule = Rule::fill;
            panel.layout.width = 800.0f;
            panel.layout.height = 600.0f;
            panel.layout.spacing = 8.0f;
            panel.layout.padding = {12.0f, 12.0f, 12.0f, 12.0f};
            panel.primary_text = "Runtime Controls";

            auto button = widget(3u, Kind::button, "StartButton");
            button.parent = 1u;
            button.primary_text = "Start";
            button.action = "game.start";
            button.focusable = true;
            button.accepts_pointer = true;
            button.tab_index = 0;
            button.layout.width = 180.0f;
            button.layout.height = 44.0f;

            auto input = widget(4u, Kind::text_input, "PlayerName");
            input.parent = 1u;
            input.primary_text = "Epoch";
            input.secondary_text = "Player name";
            input.maximum_length = 32u;
            input.focusable = true;
            input.accepts_pointer = true;
            input.tab_index = 1;
            input.layout.width = 260.0f;
            input.layout.height = 44.0f;

            auto slider = widget(5u, Kind::slider, "Volume");
            slider.parent = 1u;
            slider.primary_text = "Volume";
            slider.action = "audio.volume";
            slider.minimum = 0.0;
            slider.maximum = 1.0;
            slider.value = 0.5;
            slider.step = 0.1;
            slider.focusable = true;
            slider.accepts_pointer = true;
            slider.tab_index = 2;
            slider.layout.width = 260.0f;
            slider.layout.height = 44.0f;

            auto tabs = widget(6u, Kind::tab_set, "Pages");
            tabs.parent = 1u;
            tabs.children = {6u, 7u};
            tabs.selected_child = 6u;
            tabs.action = "page.changed";
            tabs.layout.width_rule = Rule::fill;
            tabs.layout.height_rule = Rule::fill;
            tabs.layout.width = 760.0f;
            tabs.layout.height = 380.0f;

            auto first = widget(7u, Kind::tab_page, "GeneralPage");
            first.parent = 5u;
            first.primary_text = "General";
            first.layout.width_rule = Rule::fill;
            first.layout.height_rule = Rule::fill;

            auto second = widget(8u, Kind::tab_page, "AdvancedPage");
            second.parent = 5u;
            second.primary_text = "Advanced";
            second.closeable = true;
            second.layout.width_rule = Rule::fill;
            second.layout.height_rule = Rule::fill;

            result.widgets = {
                std::move(root), std::move(panel), std::move(button),
                std::move(input), std::move(slider), std::move(tabs),
                std::move(first), std::move(second)};
            result.identity.key = asset::gui::payload_content(result);
            return result;
        }

        [[nodiscard]] const WidgetView* find_widget(
            const RuntimeFrame& frame,
            asset::gui::WidgetKind kind)
        {
            const auto found = std::find_if(
                frame.widgets.begin(), frame.widgets.end(),
                [kind](const WidgetView& view) { return view.kind == kind; });
            return found == frame.widgets.end() ? nullptr : &*found;
        }
    }

    ContractFailure run_contract() noexcept
    {
        auto source = artifact();
        if (!asset::gui::validate(source))
            return ContractFailure::valid_artifact_rejected;

        const auto firstBytes = asset::gui::serialize(source);
        const auto secondBytes = asset::gui::serialize(source);
        const auto restored = firstBytes
            ? asset::gui::deserialize(firstBytes.bytes)
            : asset::gui::DeserializationResult{};
        if (!firstBytes || !secondBytes || firstBytes.bytes != secondBytes.bytes
            || !restored || restored.artifact != source)
        {
            return ContractFailure::artifact_round_trip;
        }

        std::vector<std::byte> corrupted = firstBytes.bytes;
        corrupted[corrupted.size() / 2u] ^= std::byte{0x20u};
        if (asset::gui::deserialize(corrupted).code
            != asset::gui::ArtifactCode::integrity_failure)
        {
            return ContractFailure::corrupted_artifact_accepted;
        }

        auto invalid = source;
        invalid.widgets.front().children = {999u};
        invalid.identity.key = asset::gui::payload_content(invalid);
        if (asset::gui::validate(invalid).code
            != asset::gui::ArtifactCode::malformed_hierarchy)
        {
            return ContractFailure::invalid_hierarchy_accepted;
        }

        RuntimeSession runtime{source};
        const RuntimeFrame frame = runtime.build_frame({1'600.0f, 900.0f});
        const auto* button = find_widget(frame, asset::gui::WidgetKind::button);
        const auto* input = find_widget(frame, asset::gui::WidgetKind::text_input);
        const auto* slider = find_widget(frame, asset::gui::WidgetKind::slider);
        if (!runtime.valid() || !frame || frame.widgets.size() != source.widgets.size()
            || frame.tab_headers.size() != 2u || !button || !input || !slider
            || button->bounds.empty() || input->bounds.empty() || slider->bounds.empty()
            || frame.scale != 1.5f || frame.offset.x != 200.0f
            || frame.offset.y != 0.0f)
        {
            return ContractFailure::frame_projection;
        }

        const Float2 buttonPoint{
            button->bounds.x + button->bounds.width * 0.5f,
            button->bounds.y + button->bounds.height * 0.5f};
        if (!runtime.pointer_captured(buttonPoint)
            || runtime.pointer_release(buttonPoint) != RuntimeCode::ready)
        {
            return ContractFailure::pointer_action;
        }
        auto events = runtime.drain_events();
        if (events.size() != 2u
            || events[0].kind != EventKind::focus_changed
            || events[1].kind != EventKind::activated
            || events[1].action != "game.start")
        {
            return ContractFailure::pointer_action;
        }

        if (runtime.focus_next() != RuntimeCode::ready
            || runtime.focused_widget() != source.widgets[3u].id
            || !runtime.keyboard_captured())
        {
            return ContractFailure::focus_order;
        }
        (void)runtime.drain_events();

        const auto inputId = source.widgets[3u].id;
        if (runtime.set_text(inputId, "Epoch Player") != RuntimeCode::ready
            || runtime.append_text(inputId, " 2") != RuntimeCode::ready
            || runtime.erase_text_backward(inputId) != RuntimeCode::ready)
        {
            return ContractFailure::text_editing;
        }
        events = runtime.drain_events();
        if (events.size() != 3u || events.back().text != "Epoch Player ")
            return ContractFailure::text_editing;

        if (runtime.set_slider(source.widgets[4u].id, 0.84)
                != RuntimeCode::ready)
        {
            return ContractFailure::slider_editing;
        }
        events = runtime.drain_events();
        if (events.size() != 1u || events.front().kind != EventKind::slider_changed
            || events.front().value != 0.8)
        {
            return ContractFailure::slider_editing;
        }

        if (runtime.select_tab(source.widgets[5u].id, source.widgets[7u].id)
                != RuntimeCode::ready)
        {
            return ContractFailure::tab_selection;
        }
        events = runtime.drain_events();
        const RuntimeFrame selected = runtime.build_frame({800.0f, 600.0f});
        const auto advanced = std::find_if(
            selected.tab_headers.begin(), selected.tab_headers.end(),
            [](const TabHeaderView& header) { return header.label == "Advanced"; });
        if (events.size() != 1u || events.front().kind != EventKind::tab_changed
            || advanced == selected.tab_headers.end() || !advanced->selected)
        {
            return ContractFailure::tab_selection;
        }

        RuntimeLimits boundedLimits{};
        boundedLimits.maximum_pending_events = 1u;
        RuntimeSession bounded{source, boundedLimits};
        if (bounded.focus(source.widgets[2u].id) != RuntimeCode::ready
            || bounded.focus(source.widgets[3u].id)
                != RuntimeCode::action_limit_exceeded
            || bounded.focused_widget() != source.widgets[2u].id)
        {
            return ContractFailure::event_budget;
        }

        return ContractFailure::none;
    }
}

#if defined(EPOCH_PROJECT_GUI_RUNTIME_CONTRACT_MAIN)
int main()
{
    return static_cast<int>(
        epochengine::project_gui_runtime::run_contract());
}
#endif
