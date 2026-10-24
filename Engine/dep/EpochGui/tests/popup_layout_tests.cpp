// SPDX-License-Identifier: MPL-2.0
// EpochGUI portable popup placement contract; no native window or renderer.
#if defined(EPOCHGUI_TEST_MODULE_IMPORT)
import epoch.gui;
#else
#include "gui/popup_layout.hpp"
#endif

int popup_layout_contract()
{
    namespace gui = epochengine::gui_lib;
    gui::PopupPlacement used{};
    bool flipped{}, clamped{};
    const auto below = gui::place_popup({
        .owner = {{20.0f, 30.0f}, {250.0f, 30.0f}},
        .preferred_size = {250.0f, 88.0f},
        .viewport_size = {600.0f, 400.0f}}, {}, &used, &flipped, &clamped);
    if (flipped || clamped || used != gui::PopupPlacement::below
        || below.position.y != 64.0f || below.size.y != 88.0f)
        return 1;
    const auto above = gui::place_popup({
        .owner = {{20.0f, 260.0f}, {250.0f, 30.0f}},
        .preferred_size = {250.0f, 88.0f},
        .viewport_size = {600.0f, 360.0f}}, {}, &used, &flipped, &clamped);
    if (!flipped || clamped || used != gui::PopupPlacement::above
        || above.position.y != 168.0f || above.size.y != 88.0f)
        return 2;
    const auto compressed = gui::place_popup({
        .owner = {{100.0f, 70.0f}, {100.0f, 30.0f}},
        .preferred_size = {250.0f, 200.0f},
        .viewport_size = {160.0f, 100.0f}}, {}, &used, &flipped, &clamped);
    if (compressed.size.x != 152.0f || compressed.size.y != 92.0f
        || compressed.position.x < 4.0f || compressed.position.y < 4.0f
        || compressed.position.x + compressed.size.x > 156.0f
        || compressed.position.y + compressed.size.y > 96.0f)
        return 3;
    return 0;
}

#if !defined(EPOCHGUI_TEST_NO_MAIN)
int main()
{
    return popup_layout_contract();
}
#endif
