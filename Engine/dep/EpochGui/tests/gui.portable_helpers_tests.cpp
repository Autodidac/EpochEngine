// SPDX-License-Identifier: MPL-2.0
#include <gui/status_strip_layout.hpp>
#include <gui/utf8_text_layout.hpp>
#include <array>
#include <limits>
#include <string_view>
import epoch.gui;

namespace
{
    [[nodiscard]] bool translucent_surface_contract() noexcept
    {
        using epochengine::gui_lib::translucent_surface;
        // White on black needs at least 209/255 opacity over linearly blended white.
        if (translucent_surface({0, 0, 0}, {255, 255, 255}, 0).a != 209) return false;
        // Black on white needs at least 117/255 opacity over sRGB-blended black.
        if (translucent_surface({255, 255, 255}, {0, 0, 0}, 0).a != 117) return false;
        if (translucent_surface({255, 255, 255}, {0, 0, 0}, 190).a != 190) return false;

        const auto tinted = translucent_surface({26, 35, 42}, {240, 244, 248}, 160);
        if (tinted.r != 26 || tinted.g != 35 || tinted.b != 42 || tinted.a <= 160 || tinted.a >= 255) return false;
        if (translucent_surface({26, 35, 42}, {240, 244, 248}, 255).a != 255) return false;
        if (translucent_surface({100, 100, 100}, {100, 100, 100}, 128).a != 255) return false;
        return translucent_surface({26, 35, 42}, {240, 244, 248, 160}, 128).a == 255;
    }
}

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
    if (!translucent_surface_contract()) return 5;
    return 0;
}
