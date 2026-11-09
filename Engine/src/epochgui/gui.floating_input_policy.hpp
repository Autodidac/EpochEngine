// Epoch system notes (auto-checked):
// EpochGUI: use consistent draw/hit-test geometry and preserve child input ownership.
#pragma once
// EpochGUI: child widgets receive floating-window body input first.
// The frame owns chrome input and absorbs unused clicks only after children.
namespace epochengine::gui::floating_input
{
    // Included after import epoch.gui.input; keep the adapter call sites stable.
    [[nodiscard]] inline bool chrome_owns_press(
        bool inside_window, bool movable, bool title_hovered,
        bool close_hovered, bool resize_hovered) noexcept
    {
        return gui_lib::input::floating_chrome_owns_press(inside_window,
            movable, title_hovered, close_hovered, resize_hovered);
    }

    [[nodiscard]] inline bool absorb_unhandled_pointer(
        bool inside_window, bool already_consumed) noexcept
    {
        return gui_lib::input::floating_absorb_unhandled_pointer(inside_window, already_consumed);
    }
}
