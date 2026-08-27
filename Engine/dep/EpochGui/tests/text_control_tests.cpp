#include <array>
#include <cstddef>
#include <cmath>
#include <string>

import epoch.gui;

namespace
{
    using namespace epochengine::gui_lib;

    int check(bool condition, int line)
    {
        return condition ? 0 : line;
    }

    [[nodiscard]] bool approximately(float left, float right) noexcept
    {
        return std::abs(left - right) <= 0.001f;
    }

#define EPOCHGUI_CHECK(condition) \
    do { const int failure = check((condition), __LINE__); if (failure != 0) return failure; } while (false)

    int replacement_and_clipboard()
    {
        TextControlState state{ .text = "hello world", .anchor = 6, .caret = 11 };
        TextControlOptions options{};
        TextControlResult result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::insert_text, .text = "Epoch" });
        EPOCHGUI_CHECK(result.text_changed);
        EPOCHGUI_CHECK(state.text == "hello Epoch");
        EPOCHGUI_CHECK(state.caret == state.text.size());

        state.anchor = 6;
        state.caret = state.text.size();
        result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::copy_selection });
        EPOCHGUI_CHECK(result.clipboard_write_requested);
        EPOCHGUI_CHECK(result.clipboard_text == "Epoch");
        EPOCHGUI_CHECK(state.text == "hello Epoch");

        result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::cut_selection });
        EPOCHGUI_CHECK(result.clipboard_text == "Epoch");
        EPOCHGUI_CHECK(state.text == "hello ");
        return 0;
    }

    int utf8_and_limits()
    {
        TextControlState state{ .text = "A\xc3\xa9" "B", .anchor = 3, .caret = 3 };
        TextControlOptions options{};
        TextControlResult result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::erase_backward });
        EPOCHGUI_CHECK(result.text_changed);
        EPOCHGUI_CHECK(state.text == "AB");
        EPOCHGUI_CHECK(state.caret == 1);

        state = TextControlState{ .text = "ab", .anchor = 2, .caret = 2 };
        options.maximum_bytes = 4;
        result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::insert_text, .text = "\xc3\xa9" "X" });
        EPOCHGUI_CHECK(state.text == "ab\xc3\xa9");
        EPOCHGUI_CHECK(state.text.size() == 4);
        return 0;
    }

    int navigation_and_selection()
    {
        TextControlState state{ .text = "one\n12\nabcdef", .anchor = 2, .caret = 2 };
        TextControlOptions options{};
        TextControlResult result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::move_down });
        EPOCHGUI_CHECK(result.caret.line == 1);
        EPOCHGUI_CHECK(result.caret.column == 2);
        EPOCHGUI_CHECK(state.caret == 6);

        result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::move_down });
        EPOCHGUI_CHECK(result.caret.line == 2);
        EPOCHGUI_CHECK(result.caret.column == 2);
        EPOCHGUI_CHECK(state.caret == 9);

        state = TextControlState{ .text = "alpha  beta", .anchor = 11, .caret = 11 };
        result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::move_word_left });
        EPOCHGUI_CHECK(state.caret == 7);
        result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::move_word_left, .extend_selection = true });
        EPOCHGUI_CHECK(text_selection(state).first == 0);
        EPOCHGUI_CHECK(selected_text(state) == "alpha  ");
        return 0;
    }

    int filtering_read_only_and_scroll()
    {
        TextControlState state{};
        TextControlOptions options{ .viewport_size = { 100.0f, 40.0f }, .multiline = false };
        TextControlResult result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::paste_text, .text = "a\r\nb\tc" });
        EPOCHGUI_CHECK(state.text == "abc");

        state.anchor = 0;
        state.caret = state.text.size();
        options.read_only = true;
        result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::cut_selection });
        EPOCHGUI_CHECK(state.text == "abc");
        EPOCHGUI_CHECK(!result.clipboard_write_requested);

        options.read_only = false;
        options.multiline = true;
        result = update_text_control(
            state,
            options,
            TextControlInput{
                .metrics = TextControlMetrics{
                    .content_size = { 200.0f, 100.0f },
                    .caret_position = { 150.0f, 70.0f },
                    .caret_size = { 2.0f, 20.0f },
                    .valid = true } });
        EPOCHGUI_CHECK(result.scroll_changed);
        EPOCHGUI_CHECK(state.scroll.x == 60.0f);
        EPOCHGUI_CHECK(state.scroll.y == 58.0f);
        return 0;
    }

    int whitespace_and_control_filtering()
    {
        TextControlState state{ .text = "A\xc3\xa9 B", .anchor = 5, .caret = 5 };
        TextControlOptions options{};
        TextControlResult result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::erase_backward });
        EPOCHGUI_CHECK(result.text_changed);
        EPOCHGUI_CHECK(state.text == "A\xc3\xa9 ");
        EPOCHGUI_CHECK(state.caret == 4);

        // Backspace is an edit command; a duplicate control byte from the text lane is ignored.
        result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::insert_text, .text = "\b" });
        EPOCHGUI_CHECK(!result.text_changed);
        EPOCHGUI_CHECK(state.text == "A\xc3\xa9 ");
        EPOCHGUI_CHECK(state.caret == 4);

        result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::erase_backward });
        EPOCHGUI_CHECK(result.text_changed);
        EPOCHGUI_CHECK(state.text == "A\xc3\xa9");
        EPOCHGUI_CHECK(state.caret == 3);

        // Key repeat remains a sequence of erase commands, one codepoint per event.
        result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::erase_backward });
        EPOCHGUI_CHECK(result.text_changed);
        EPOCHGUI_CHECK(state.text == "A");
        EPOCHGUI_CHECK(state.caret == 1);

        result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::insert_text, .text = " X\b Y\x7f" });
        EPOCHGUI_CHECK(result.text_changed);
        EPOCHGUI_CHECK(state.text == "A X Y");
        EPOCHGUI_CHECK(state.caret == 5);

        options.accept_tab = true;
        result = update_text_control(
            state,
            options,
            TextControlInput{ .command = TextControlCommand::insert_text, .text = "\t" });
        EPOCHGUI_CHECK(result.text_changed);
        EPOCHGUI_CHECK(state.text == "A X Y\t");
        EPOCHGUI_CHECK(state.caret == 6);
        return 0;
    }

    int text_editor_document()
    {
        TextEditorState editor{};
        set_text_editor_text(editor, "alpha\x0a" "beta\x0a");
        normalize_text_editor(editor);

        TextEditorSnapshot snapshot = text_editor_snapshot(editor);
        EPOCHGUI_CHECK(snapshot.line_count == 3u);
        EPOCHGUI_CHECK(text_editor_line(editor, 0u) == "alpha");
        EPOCHGUI_CHECK(text_editor_line(editor, 1u) == "beta");
        EPOCHGUI_CHECK(text_editor_line(editor, 2u).empty());
        EPOCHGUI_CHECK(!snapshot.dirty);

        EPOCHGUI_CHECK(find_next_text_editor_match(editor, "beta"));
        EPOCHGUI_CHECK(editor.active_match.first == 6u);
        EPOCHGUI_CHECK(editor.active_match.past_last == 10u);

        TextControlOptions options{ .multiline = true };
        EPOCHGUI_CHECK(replace_text_editor_match(editor, options, "Epoch"));
        snapshot = text_editor_snapshot(editor);
        EPOCHGUI_CHECK(editor.text.text == "alpha\x0a" "Epoch\x0a");
        EPOCHGUI_CHECK(snapshot.revision > 1u);
        EPOCHGUI_CHECK(snapshot.dirty);

        mark_text_editor_saved(editor);
        EPOCHGUI_CHECK(!text_editor_snapshot(editor).dirty);
        return 0;
    }
    int segmented_control_geometry()
    {
        const std::array<float, 3> widths{ 80.0f, 120.0f, 60.0f };
        const SegmentedControlLayoutOptions options{
            .position = { 10.0f, 20.0f },
            .item_widths = widths,
            .height = 30.0f,
            .gap = 4.0f
        };

        const SegmentedControlLayout layout = make_segmented_control_layout(options);
        EPOCHGUI_CHECK(layout.valid);
        EPOCHGUI_CHECK(layout.item_count == 3);
        EPOCHGUI_CHECK(layout.bounds.position.x == 10.0f);
        EPOCHGUI_CHECK(layout.bounds.position.y == 20.0f);
        EPOCHGUI_CHECK(layout.bounds.size.x == 268.0f);
        EPOCHGUI_CHECK(layout.bounds.size.y == 30.0f);

        const Rect second = segmented_control_item_layout(options, 1);
        EPOCHGUI_CHECK(second.position.x == 94.0f);
        EPOCHGUI_CHECK(second.position.y == 20.0f);
        EPOCHGUI_CHECK(second.size.x == 120.0f);
        EPOCHGUI_CHECK(second.size.y == 30.0f);

        EPOCHGUI_CHECK(segmented_control_item_at(options, { 10.0f, 20.0f }) == 0);
        EPOCHGUI_CHECK(segmented_control_item_at(options, { 95.0f, 25.0f }) == 1);
        EPOCHGUI_CHECK(segmented_control_item_at(options, { 220.0f, 25.0f }) == 2);
        EPOCHGUI_CHECK(
            segmented_control_item_at(options, { 91.0f, 25.0f })
            == invalid_selectable_row_index);
        EPOCHGUI_CHECK(
            segmented_control_item_at(options, { 500.0f, 25.0f })
            == invalid_selectable_row_index);
        return 0;
    }

    int visual_control_geometry()
    {
        ImageBoxLayout image = make_image_box_layout({
            .bounds = { { 10.0f, 20.0f }, { 200.0f, 100.0f } },
            .source_extent = { 400.0f, 200.0f },
            .fit = ImageFitMode::contain,
            .padding = 10.0f,
            .caption_height = 20.0f
        });
        EPOCHGUI_CHECK(image.valid);
        EPOCHGUI_CHECK(image.viewport.position.x == 20.0f);
        EPOCHGUI_CHECK(image.viewport.position.y == 30.0f);
        EPOCHGUI_CHECK(image.viewport.size.x == 180.0f);
        EPOCHGUI_CHECK(image.viewport.size.y == 60.0f);
        EPOCHGUI_CHECK(approximately(image.content.position.x, 50.0f));
        EPOCHGUI_CHECK(approximately(image.content.position.y, 30.0f));
        EPOCHGUI_CHECK(approximately(image.content.size.x, 120.0f));
        EPOCHGUI_CHECK(approximately(image.content.size.y, 60.0f));
        EPOCHGUI_CHECK(image.caption.position.y == 90.0f);

        image = make_image_box_layout({
            .bounds = { { 0.0f, 0.0f }, { 64.0f, 32.0f } },
            .source_extent = { 2.0f, 1.0f },
            .fit = ImageFitMode::stretch,
            .padding = 0.0f
        });
        EPOCHGUI_CHECK(image.valid);
        EPOCHGUI_CHECK(image.content.size.x == 64.0f);
        EPOCHGUI_CHECK(image.content.size.y == 32.0f);

        const std::array<float, 2> tabWidths{ 100.0f, 120.0f };
        const TabButtonLayout tab = make_tab_button_layout({
            .strip = {
                .position = { 0.0f, 0.0f },
                .item_widths = tabWidths,
                .height = 30.0f,
                .gap = 2.0f },
            .indicator_height = 3.0f,
            .close_extent = 16.0f,
            .label_padding = 8.0f
        }, 1u, true, true);
        EPOCHGUI_CHECK(tab.valid);
        EPOCHGUI_CHECK(tab.active);
        EPOCHGUI_CHECK(tab.closable);
        EPOCHGUI_CHECK(tab.button.position.x == 102.0f);
        EPOCHGUI_CHECK(tab.button.size.x == 120.0f);
        EPOCHGUI_CHECK(tab.indicator.position.y == 27.0f);
        EPOCHGUI_CHECK(tab.indicator.size.y == 3.0f);
        EPOCHGUI_CHECK(tab.close_button.position.x == 206.0f);
        EPOCHGUI_CHECK(tab.close_button.position.y == 7.0f);
        EPOCHGUI_CHECK(tab.label.size.x == 88.0f);

        const SliderLayoutOptions sliderOptions{
            .bounds = {{10.0f, 20.0f}, {200.0f, 30.0f}},
            .minimum = -1.0f,
            .maximum = 1.0f,
            .value = 0.25f,
            .step = 0.25f
        };
        const SliderLayout slider = make_slider_layout(sliderOptions);
        EPOCHGUI_CHECK(slider.valid);
        EPOCHGUI_CHECK(slider.bounds.position.x == 10.0f);
        EPOCHGUI_CHECK(slider.track.position.x == 14.0f);
        EPOCHGUI_CHECK(slider.track.size.x == 192.0f);
        EPOCHGUI_CHECK(approximately(slider.fraction, 0.625f));
        EPOCHGUI_CHECK(approximately(slider.fill.size.x, 120.0f));
        EPOCHGUI_CHECK(approximately(slider.thumb.position.x, 130.875f));
        EPOCHGUI_CHECK(
            slider_value_from_position(sliderOptions, 14.0f) == -1.0f);
        EPOCHGUI_CHECK(
            slider_value_from_position(sliderOptions, 110.0f) == 0.0f);
        EPOCHGUI_CHECK(
            slider_value_from_position(sliderOptions, 206.0f) == 1.0f);
        EPOCHGUI_CHECK(!make_slider_layout({
            .bounds = {{0.0f, 0.0f}, {100.0f, 20.0f}},
            .minimum = 1.0f,
            .maximum = 1.0f
        }).valid);
        return 0;
    }

    int window_scoped_identity_and_tool_tabs()
    {
        const std::string worldScroll = scoped_control_key(
            "host-1", "World Outliner", "body");
        const std::string assetScroll = scoped_control_key(
            "host-1", "Asset Browser", "body");
        const std::string secondHostScroll = scoped_control_key(
            "host-2", "World Outliner", "body");

        EPOCHGUI_CHECK(worldScroll != assetScroll);
        EPOCHGUI_CHECK(worldScroll != secondHostScroll);
        EPOCHGUI_CHECK(
            scoped_control_key("a|b", "c", "d")
            != scoped_control_key("a", "b|c", "d"));

        const ToolTabSizingPolicy policy{};
        EPOCHGUI_CHECK(approximately(
            preferred_tool_tab_width(20.0f, false, false, policy),
            policy.minimum_width));
        EPOCHGUI_CHECK(approximately(
            preferred_tool_tab_width(1000.0f, true, true, policy),
            policy.maximum_width));
        EPOCHGUI_CHECK(
            preferred_tool_tab_width(84.0f, true, false, policy)
            > preferred_tool_tab_width(84.0f, false, false, policy));
        EPOCHGUI_CHECK(
            preferred_tool_tab_width(84.0f, true, true, policy)
            > preferred_tool_tab_width(84.0f, true, false, policy));
        return 0;
    }
}

int main()
{
    if (const int result = replacement_and_clipboard(); result != 0)
        return result;
    if (const int result = utf8_and_limits(); result != 0)
        return result;
    if (const int result = navigation_and_selection(); result != 0)
        return result;
    if (const int result = filtering_read_only_and_scroll(); result != 0)
        return result;
    if (const int result = whitespace_and_control_filtering(); result != 0)
        return result;
    if (const int result = text_editor_document(); result != 0)
        return result;
    if (const int result = segmented_control_geometry(); result != 0)
        return result;
    if (const int result = visual_control_geometry(); result != 0)
        return result;
    return window_scoped_identity_and_tool_tabs();
}
