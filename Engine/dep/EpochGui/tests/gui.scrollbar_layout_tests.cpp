// SPDX-License-Identifier: MPL-2.0
#include <cmath>
#include <limits>
import epoch.gui;

int main()
{
    namespace gui = epochengine::gui_lib;
    const auto near = [](float a, float b) { return std::abs(a - b) < 0.001f; };
    const auto middle = gui::make_scrollbar_layout({
        .track = {{200.0f, 30.0f}, {10.0f, 100.0f}},
        .content_extent = 500.0f, .scroll_offset = 200.0f });
    if (!near(middle.thumb.position.y, 70.0f) || !near(middle.thumb.size.y, 20.0f)
        || !near(middle.maximum_scroll_offset, 400.0f)) return 1;
    const float grab = gui::scrollbar_grab_offset(middle, 76.0f, true);
    if (!near(grab, 6.0f) || !near(gui::scrollbar_offset_from_pointer(middle, 76.0f, grab), 200.0f)) return 2;
    if (!near(gui::scrollbar_offset_from_pointer(middle, -100.0f, grab), 0.0f)
        || !near(gui::scrollbar_offset_from_pointer(middle, 900.0f, grab), 400.0f)) return 3;
    if (!near(gui::scrollbar_grab_offset(middle, 120.0f, false), 10.0f)) return 4;
    // Translated, scaled native viewports retain their supplied coordinate space.
    const auto scaled = gui::make_scrollbar_layout({
        .track = {{400.0f, 60.0f}, {20.0f, 200.0f}},
        .content_extent = 1000.0f, .scroll_offset = 400.0f, .minimum_thumb_extent = 36.0f });
    if (!near(scaled.thumb.position.y, middle.thumb.position.y * 2.0f)
        || !near(scaled.thumb.size.y, middle.thumb.size.y * 2.0f)) return 5;
    const auto tiny = gui::make_scrollbar_layout({
        .track = {{5.0f, 7.0f}, {10.0f, 8.0f}}, .content_extent = 500.0f, .scroll_offset = 492.0f });
    if (!near(tiny.thumb.size.y, 8.0f) || !near(tiny.thumb.position.y, 7.0f)) return 6;
    const auto fits = gui::make_scrollbar_layout({
        .track = {{0.0f, 30.0f}, {10.0f, 100.0f}}, .content_extent = 20.0f, .scroll_offset = 90.0f });
    if (fits.maximum_scroll_offset != 0.0f || fits.scroll_offset != 0.0f || fits.thumb.size.y != 100.0f) return 7;
    const auto empty = gui::make_scrollbar_layout({});
    if (empty.thumb.size.y != 0.0f || gui::scrollbar_offset_from_pointer(empty, 50.0f, 0.0f) != 0.0f) return 8;
    const auto invalid = gui::make_scrollbar_layout({
        .track = {{0.0f, 0.0f}, {10.0f, 100.0f}},
        .content_extent = std::numeric_limits<float>::infinity(),
        .scroll_offset = std::numeric_limits<float>::quiet_NaN() });
    if (!std::isfinite(invalid.thumb.position.y) || invalid.maximum_scroll_offset != 0.0f) return 9;
    // Clipped consumers retain ownership of hit admission; geometry remains the same.
    if (middle.thumb.position.x != middle.track.position.x || middle.thumb.size.x != 10.0f) return 10;
    return 0;
}
