#include <array>
#include <cstddef>
#include <string>

import epoch.gui;

namespace
{
    using namespace epochengine::gui_lib;

    int check(bool condition, int line)
    {
        return condition ? 0 : line;
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
    return segmented_control_geometry();
}
