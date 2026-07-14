#include <cstddef>
#include <string>

import epoch.gui;

namespace
{
    using namespace epochnamespace::gui_lib;

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
}

int main()
{
    if (const int result = replacement_and_clipboard(); result != 0)
        return result;
    if (const int result = utf8_and_limits(); result != 0)
        return result;
    if (const int result = navigation_and_selection(); result != 0)
        return result;
    return filtering_read_only_and_scroll();
}
