/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <sstream>

import epoch.gui;

namespace
{
    namespace layout = epochengine::gui_lib;

    bool near(float left, float right) noexcept
    {
        return std::abs(left - right) <= 0.0005f;
    }

    bool geometry_contract() noexcept
    {
        constexpr std::array widths{ 0.0f, 1.0f, 7.0f, 48.0f, 320.0f, 800.0f, 1919.0f, 3840.0f };
        for (const float width : widths)
        {
            const auto centered = layout::make_bottom_dock_column_layout({ .viewport_width = width });
            if (!centered.split || !near(centered.left_width, centered.right_width)
                || !near(centered.left_width + centered.splitter_width * 0.5f, width * 0.5f)
                || !near(centered.right_offset + centered.right_width, width))
                return false;
            for (const float saved : { 0.25f, 0.37f, 0.50f, 0.55f, 0.68f, 0.75f })
            {
                const layout::BottomDockColumnOptions options{
                    .viewport_width = width, .requested_fraction = saved };
                const auto observed = layout::make_bottom_dock_column_layout(options);
                if (observed.normalized_fraction != saved || options.requested_fraction != saved
                    || observed.left_width < 0.0f || observed.right_width < 0.0f
                    || !near(observed.left_width + observed.splitter_width + observed.right_width, width))
                    return false;
                if (width > 7.0f && !near(observed.left_width / (width - 7.0f), saved))
                    return false;
            }
        }
        return true;
    }

    bool dragging_contract() noexcept
    {
        for (const float width : { 320.0f, 800.0f, 1920.0f })
            for (const float saved : { 0.25f, 0.37f, 0.50f, 0.55f, 0.68f, 0.75f })
            {
                const layout::BottomDockColumnOptions options{
                    .viewport_width = width, .requested_fraction = saved };
                const auto before = layout::make_bottom_dock_column_layout(options);
                for (const float grab : { 0.0f, 3.5f, 7.0f })
                    if (!near(layout::bottom_dock_column_fraction_from_pointer(
                            options, before.left_width + grab, grab), saved)
                        || !near(layout::bottom_dock_column_fraction_from_pointer(
                            options, (width - 7.0f) * 0.40f + grab, grab), 0.40f))
                        return false;
                if (layout::bottom_dock_column_fraction_from_pointer(options, -100.0f, 0.0f) != 0.25f
                    || layout::bottom_dock_column_fraction_from_pointer(options, width + 100.0f, 7.0f) != 0.75f)
                    return false;
            }
        return true;
    }

    bool persistence_and_context_resize_contract()
    {
        // The file format has no "user adjusted" bit. Keep old 0.55 and 0.68
        // values as faithfully as any other manual ratio, across context copies.
        for (const float original : { 0.25f, 0.37f, 0.50f, 0.55f, 0.68f, 0.75f })
        {
            std::stringstream persisted;
            persisted << original;
            float decoded{};
            if (!(persisted >> decoded)
                || layout::normalize_bottom_dock_column_fraction(decoded) != original)
                return false;
            const std::array contextFractions{ decoded, decoded };
            constexpr std::array mainWidths{ 1200.0f, 600.0f, 1.0f, 1800.0f };
            constexpr std::array secondaryWidths{ 800.0f, 1500.0f, 7.0f, 400.0f };
            for (std::size_t index = 0u; index < mainWidths.size(); ++index)
            {
                const auto main = layout::make_bottom_dock_column_layout({
                    .viewport_width = mainWidths[index], .requested_fraction = contextFractions[0] });
                const auto secondary = layout::make_bottom_dock_column_layout({
                    .viewport_width = secondaryWidths[index], .requested_fraction = contextFractions[1] });
                if (main.normalized_fraction != original || secondary.normalized_fraction != original)
                    return false;
            }
        }
        return true;
    }

    bool visibility_and_invalid_contract() noexcept
    {
        const auto onlyLeft = layout::make_bottom_dock_column_layout({
            .viewport_width = 800.0f, .right_visible = false });
        const auto onlyRight = layout::make_bottom_dock_column_layout({
            .viewport_width = 800.0f, .left_visible = false });
        const auto hidden = layout::make_bottom_dock_column_layout({
            .viewport_width = 800.0f, .left_visible = false, .right_visible = false });
        if (onlyLeft.split || onlyLeft.left_width != 800.0f || onlyLeft.right_width != 0.0f
            || onlyRight.split || onlyRight.right_width != 800.0f || onlyRight.right_offset != 0.0f
            || hidden.split || hidden.left_width != 0.0f || hidden.right_width != 0.0f)
            return false;
        const auto invalid = layout::make_bottom_dock_column_layout({
            .viewport_width = std::numeric_limits<float>::infinity(),
            .requested_fraction = std::numeric_limits<float>::quiet_NaN() });
        return invalid.normalized_fraction == 0.50f && invalid.left_width == 0.0f
            && invalid.right_width == 0.0f
            && layout::bottom_dock_column_fraction_from_pointer({
                .viewport_width = 0.0f, .requested_fraction = 0.68f }, 100.0f, 3.5f) == 0.68f;
    }
}

int main()
{
    if (!geometry_contract()) return 1;
    if (!dragging_contract()) return 2;
    if (!persistence_and_context_resize_contract()) return 3;
    if (!visibility_and_invalid_contract()) return 4;
    return 0;
}
