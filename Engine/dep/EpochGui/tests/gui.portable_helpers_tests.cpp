// SPDX-License-Identifier: MPL-2.0
#include <gui/status_strip_layout.hpp>
#include <gui/utf8_text_layout.hpp>
#include <array>
#include <limits>
#include <string_view>
int main()
{
    const std::string_view text = "a\xc3\xa9\xf0\x9f\x98\x80";
    using epochengine::gui_lib::utf8_text_layout::prefix;
    if (prefix(text, 2) != "a" || prefix(text, 6).size() != 3 || prefix(text, 7) != text) return 1;
    const std::array widths{80.0f, 80.0f, 20.0f};
    const auto narrow = epochgui::status_strip::measure(widths, 130.0f);
    if (narrow.count != 3 || narrow.rows != 3) return 2;
    const auto aligned = epochgui::status_strip::measure(widths, 400.0f, 26.0f, 4.0f, 4.0f, 12.0f, 20.0f, true);
    if (aligned.rows != 1 || aligned.cells[2].x + aligned.cells[2].width != 388.0f) return 3;
    if (epochgui::status_strip::measure(widths, std::numeric_limits<float>::quiet_NaN()).count != 0) return 4;
    return 0;
}
