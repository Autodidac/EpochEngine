// Epoch system notes (auto-checked):
// EpochGUI: use consistent draw/hit-test geometry and preserve child input ownership.
#pragma once
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Stateless input-admission policy for deferred editor dropdowns.
// The visual draw order does not grant input ownership to a popup: ownership
// must be latched before regular controls handle the current input frame.
namespace epochengine::gui::popup_input {

// Included after import epoch.gui.input; the library owns the decision.
using Capture = gui_lib::input::PopupCapture;

[[nodiscard]] inline bool allows_pointer(Capture capture) noexcept {
    return gui_lib::input::popup_allows_pointer(capture);
}

} // namespace epochengine::gui::popup_input
