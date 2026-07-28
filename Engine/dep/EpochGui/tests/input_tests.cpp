#include <cmath>

import epoch.gui.input;

namespace
{
    namespace gui = epochengine::gui_lib;
    namespace input = epochengine::gui_lib::input;

    [[nodiscard]] bool near(float left, float right) noexcept
    {
        return std::fabs(left - right) < 0.001f;
    }
}

int main()
{
    input::InputTracker tracker{};
    tracker.set_pointer_position({ 40.0f, 50.0f });
    tracker.set_pointer_button(input::PointerButton::left, true);

    const input::InputFrame& pressed = tracker.frame();
    if (!pressed.pointer(input::PointerButton::left).down
        || !pressed.pointer(input::PointerButton::left).pressed
        || !near(pressed.pointer_position.x, 40.0f)
        || !near(pressed.pointer_delta.y, 50.0f))
    {
        return 1;
    }

    const gui::FloatingWindowInput floating = input::floating_window_input(pressed);
    if (!floating.mouse_down || !floating.mouse_pressed || floating.mouse_released)
        return 2;

    tracker.finish_frame();
    if (tracker.frame().pointer(input::PointerButton::left).pressed
        || !tracker.frame().pointer(input::PointerButton::left).down)
    {
        return 3;
    }

    tracker.set_pointer_button(input::PointerButton::left, false);
    if (!tracker.frame().pointer(input::PointerButton::left).released
        || tracker.frame().pointer(input::PointerButton::left).down)
    {
        return 4;
    }

    tracker.finish_frame();
    tracker.set_pointer_position({ 120.0f, 90.0f });
    tracker.set_pointer_button(input::PointerButton::right, true);
    tracker.finish_frame();
    tracker.set_pointer_button(input::PointerButton::right, false);

    const input::ContextMenuRequest context = input::context_menu_request(
        tracker.frame(),
        { { 100.0f, 80.0f }, { 100.0f, 80.0f } });
    if (!context.requested || !near(context.position.x, 120.0f))
        return 5;

    const input::BorderlessWindowChromeLayout chrome =
        input::make_borderless_window_chrome_layout({
            .bounds = { { 0.0f, 0.0f }, { 800.0f, 600.0f } },
            .title_bar_height = 44.0f,
            .resize_border = 8.0f,
            .caption_padding_left = 16.0f,
            .button_width = 48.0f
        });

    if (!chrome.valid
        || input::hit_test_borderless_window_chrome(chrome, { 2.0f, 2.0f })
            != input::WindowChromeRegion::resize_top_left
        || input::hit_test_borderless_window_chrome(chrome, { 200.0f, 20.0f })
            != input::WindowChromeRegion::caption
        || input::hit_test_borderless_window_chrome(chrome, { 780.0f, 20.0f })
            != input::WindowChromeRegion::close_button
        || input::hit_test_borderless_window_chrome(chrome, { 300.0f, 200.0f })
            != input::WindowChromeRegion::client)
    {
        return 6;
    }

    tracker.reset();
    tracker.set_pointer_position({ 780.0f, 20.0f });
    tracker.set_pointer_button(input::PointerButton::left, true);
    tracker.finish_frame();
    tracker.set_pointer_button(input::PointerButton::left, false);
    if (input::borderless_window_command(chrome, tracker.frame()) != input::WindowCommand::close)
        return 7;

    return 0;
}
