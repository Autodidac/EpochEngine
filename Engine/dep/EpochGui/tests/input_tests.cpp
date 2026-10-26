// SPDX-License-Identifier: MPL-2.0
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
    // Native pumps may deliver a complete tap before the next consumer frame.
    input::InputTracker edges;
    edges.set_key(input::Key::tab, true, false);
    edges.set_key(input::Key::tab, false, false);
    if (!edges.frame().key(input::Key::tab).pressed
        || !edges.frame().key(input::Key::tab).released
        || edges.frame().key(input::Key::tab).down) return 20;
    edges.finish_frame();
    if (edges.frame().key(input::Key::tab).pressed || edges.frame().key(input::Key::tab).released) return 21;
    edges.set_key(input::Key::arrow_left, true, false);
    edges.set_key(input::Key::arrow_left, true, true);
    edges.set_key(input::Key::arrow_left, false, false);
    if (!edges.frame().key(input::Key::arrow_left).repeated
        || !edges.frame().key(input::Key::arrow_left).pressed
        || !edges.frame().key(input::Key::arrow_left).released
        || edges.frame().key(input::Key::arrow_left).down) return 22;
    edges.finish_frame();
    if (edges.frame().key(input::Key::arrow_left).repeated) return 23;
    edges.set_key(input::Key::enter, false, true); // A release is never a repeat.
    if (edges.frame().key(input::Key::enter).repeated) return 24;
    edges.set_pointer_button(input::PointerButton::left, true, 2);
    edges.set_pointer_button(input::PointerButton::left, false);
    edges.set_pointer_button(input::PointerButton::left, true);
    const auto& button = edges.frame().pointer(input::PointerButton::left);
    if (!button.pressed || !button.released || !button.down || button.click_count != 2) return 25;
    edges.finish_frame();
    if (!button.down || button.pressed || button.released || button.click_count != 0) return 26;
    edges.set_key(input::Key::space, true, false);
    edges.finish_frame();
    edges.set_key(input::Key::space, false, false);
    edges.set_key(input::Key::space, true, false);
    if (!edges.frame().key(input::Key::space).down
        || !edges.frame().key(input::Key::space).pressed
        || !edges.frame().key(input::Key::space).released) return 28;
    edges.set_key(input::Key::escape, true, true);
    edges.set_key(static_cast<input::Key>(255), false, false);
    edges.set_pointer_button(static_cast<input::PointerButton>(255), false);
    if (!edges.frame().key(input::Key::escape).down || !edges.frame().key(input::Key::escape).repeated
        || !button.down) return 29;
    edges.reset();
    edges.set_pointer_button(static_cast<input::PointerButton>(255), true);
    edges.set_key(static_cast<input::Key>(255), true, true);
    if (edges.frame().pointer(input::PointerButton::left).down
        || edges.frame().key(input::Key::escape).down || edges.frame().sequence != 0) return 27;
    input::InputTracker ordered;
    input::InputFrame event;
    if (edges.next_event_frame(event)) return 40; // Snapshot users opt in explicitly.
    ordered.enable_ordered_events();
    ordered.set_modifiers({.shift = true});
    ordered.set_pointer_position({10.0f, 20.0f});
    ordered.set_pointer_button(input::PointerButton::left, true);
    ordered.set_pointer_position({40.0f, 50.0f});
    ordered.set_pointer_position({60.0f, 70.0f}); // Adjacent moves may coalesce.
    ordered.set_key(input::Key::arrow_left, true);
    ordered.set_modifiers({});
    ordered.set_pointer_position({80.0f, 90.0f}); // Cannot merge across the key/modifier edge.
    ordered.set_pointer_button(input::PointerButton::left, false);
    unsigned event_count = 0;
    bool press_seen = false, move_seen = false, key_seen = false, release_seen = false;
    while (ordered.next_event_frame(event))
    {
        ++event_count;
        const auto& left = event.pointer(input::PointerButton::left);
        if (left.pressed)
            press_seen = left.down && !left.released && near(event.pointer_position.x, 10.0f)
                && near(event.pointer_position.y, 20.0f) && event.modifiers.shift;
        if (near(event.pointer_position.x, 60.0f) && event.pointer_delta.x != 0.0f)
            move_seen = left.down && !left.pressed && event.modifiers.shift;
        if (event.key(input::Key::arrow_left).pressed)
            key_seen = left.down && event.modifiers.shift && near(event.pointer_position.x, 60.0f);
        if (left.released)
            release_seen = !left.down && !left.pressed && !event.modifiers.shift
                && event.key(input::Key::arrow_left).down && near(event.pointer_position.x, 80.0f);
    }
    if (event_count != 8 || !press_seen || !move_seen || !key_seen || !release_seen) return 41;
    if (ordered.frame().key(input::Key::arrow_left).pressed
        || !ordered.frame().key(input::Key::arrow_left).down) return 42;
    ordered.finish_frame();
    ordered.reset(); // Reset clears pending events but preserves ordered mode.
    for (unsigned index = 0; index < 10000; ++index)
        ordered.set_pointer_position({static_cast<float>(index), 25.0f});
    if (ordered.events_overflowed() || !ordered.next_event_frame(event)
        || !near(event.pointer_position.x, 9999.0f) || ordered.next_event_frame(event)) return 43;
    ordered.reset();
    for (std::size_t index = 0; index <= input::InputTracker::maximum_pending_events; ++index)
        ordered.set_key(input::Key::space, (index % 2) == 0);
    if (!ordered.events_overflowed() || ordered.next_event_frame(event)
        || ordered.frame().key(input::Key::space).down || ordered.frame().key(input::Key::space).released) return 44;
    ordered.set_pointer_button(input::PointerButton::left, true); // Ignored until cancellation/reset.
    if (ordered.frame().pointer(input::PointerButton::left).down) return 45;
    ordered.reset();
    ordered.set_pointer_button(input::PointerButton::left, false);
    if (!ordered.next_event_frame(event) || event.pointer(input::PointerButton::left).released) return 46;
    ordered.set_pointer_button(input::PointerButton::left, true);
    ordered.reset();
    if (ordered.next_event_frame(event) || ordered.events_overflowed()
        || ordered.frame().pointer(input::PointerButton::left).down) return 47;
    ordered.reset();
    ordered.set_input_source(7);
    ordered.set_pointer_position({10.0f, 15.0f});
    ordered.set_pointer_button(input::PointerButton::left, true);
    ordered.set_input_source(9);
    ordered.set_pointer_position({90.0f, 95.0f});
    ordered.set_pointer_button(input::PointerButton::left, false);
    event_count = 0;
    press_seen = release_seen = false;
    while (ordered.next_event_frame(event))
    {
        ++event_count;
        if (event.pointer(input::PointerButton::left).pressed)
            press_seen = event.input_source == 7 && near(event.pointer_position.x, 10.0f);
        if (event.pointer(input::PointerButton::left).released)
            release_seen = event.input_source == 9 && near(event.pointer_position.x, 90.0f);
    }
    if (event_count != 6 || !press_seen || !release_seen) return 48;
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

    input::ModalInputArbiter modal{};
    if (modal.active()
        || modal.keyboard_captured()
        || !modal.background_input_allowed()
        || !modal.pointer_allowed({ -100.0f, -100.0f }))
    {
        return 8;
    }

    modal.begin({ { 100.0f, 80.0f }, { 300.0f, 220.0f } });
    if (!modal.active()
        || !modal.keyboard_captured()
        || modal.background_input_allowed()
        || !modal.pointer_allowed({ 120.0f, 100.0f })
        || modal.pointer_allowed({ 20.0f, 20.0f }))
    {
        return 9;
    }

    // A model popup rendered after editor panels must own input even when an
    // underlying text field or button occupies the same screen coordinates.
    modal.require_owner_scope();
    if (modal.pointer_allowed({ 120.0f, 100.0f })
        || modal.owner_input_allowed()
        || !modal.keyboard_captured())
        return 12;
    modal.enter_owner_scope();
    if (!modal.pointer_allowed({ 120.0f, 100.0f })
        || modal.pointer_allowed({ 20.0f, 20.0f })
        || !modal.owner_input_allowed())
        return 13;
    modal.leave_owner_scope();
    if (modal.pointer_allowed({ 120.0f, 100.0f })
        || modal.owner_input_allowed())
        return 14;

    modal.block_until_clear();
    if (modal.mode() != input::ModalInputMode::blocked_until_clear
        || modal.pointer_allowed({ 120.0f, 100.0f })
        || !modal.keyboard_captured())
    {
        return 10;
    }

    modal.clear();
    if (modal.active() || !modal.pointer_allowed({ 20.0f, 20.0f }))
        return 11;

    return 0;
}
