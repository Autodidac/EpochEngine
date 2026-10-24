// Epoch system notes (auto-checked):
// EpochGUI: use consistent draw/hit-test geometry and preserve child input ownership.
#pragma once
// EpochGUI: child widgets receive floating-window body input first.
// The frame owns chrome input and absorbs unused clicks only after children.
namespace epochengine::gui::floating_input
{
    [[nodiscard]] constexpr bool chrome_owns_press(
        bool inside_window, bool movable, bool title_hovered,
        bool close_hovered, bool resize_hovered) noexcept
    {
        return inside_window && (close_hovered || resize_hovered
            || (movable && title_hovered));
    }

    [[nodiscard]] constexpr bool absorb_unhandled_pointer(
        bool inside_window, bool already_consumed) noexcept
    {
        return inside_window && !already_consumed;
    }
}
