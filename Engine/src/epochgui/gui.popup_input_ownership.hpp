// Epoch system notes (auto-checked):
// EpochGUI: use consistent draw/hit-test geometry and preserve child input ownership.
#pragma once
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Stateless input-admission policy for deferred editor dropdowns.
// The visual draw order does not grant input ownership to a popup: ownership
// must be latched before regular controls handle the current input frame.
namespace epochengine::gui::popup_input {

struct Capture final {
    bool selection_open_this_frame{};
    bool current_selection_owner{};
    bool application_menu_open_this_frame{};
    bool application_menu_owner{};
};

[[nodiscard]] constexpr bool allows_pointer(Capture capture) noexcept {
    if (capture.selection_open_this_frame && !capture.current_selection_owner)
        return false;
    if (capture.application_menu_open_this_frame && !capture.application_menu_owner)
        return false;
    return true;
}

} // namespace epochengine::gui::popup_input
