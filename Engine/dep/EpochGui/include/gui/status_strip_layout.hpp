// SPDX-License-Identifier: MPL-2.0
// EpochGUI: bounded, responsive status-strip geometry. No renderer dependencies.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace epochgui::status_strip
{
    inline constexpr std::size_t max_items = 16u;

    struct Cell
    {
        float x{};
        float y{};
        float width{};
        float height{};
    };

    struct Layout
    {
        std::array<Cell, max_items> cells{};
        std::size_t count{};
        std::size_t rows{};
        float height{};
    };

    [[nodiscard]] inline Layout measure(std::span<const float> label_widths,
        float viewport_width, float item_height = 26.0f,
        float item_gap = 4.0f, float row_gap = 4.0f,
        float horizontal_padding = 12.0f,
        float text_padding = 20.0f,
        bool align_right = false) noexcept
    {
        Layout result{};
        if (label_widths.empty() || !std::isfinite(viewport_width)
            || viewport_width <= 0.0f)
            return result;

        const float inset = std::clamp(horizontal_padding, 0.0f,
            (std::max)(0.0f, (viewport_width - 1.0f) * 0.5f));
        const float usable = (std::max)(1.0f, viewport_width - inset * 2.0f);
        const float height = (std::max)(1.0f, item_height);
        const float gap = (std::max)(0.0f, item_gap);
        const float pitch = height + (std::max)(0.0f, row_gap);
        const float padding = (std::max)(0.0f, text_padding);
        float x = inset;
        std::size_t row = 0u;
        const std::size_t count = (std::min)(label_widths.size(), max_items);
        for (std::size_t i = 0u; i < count; ++i)
        {
            const float label = std::isfinite(label_widths[i])
                ? (std::max)(0.0f, label_widths[i]) : 0.0f;
            const float width = (std::min)(usable, label + padding);
            if (x + width > viewport_width - inset + 0.01f && x > inset + 0.01f)
            {
                ++row;
                x = inset;
            }
            result.cells[i] = Cell{x, static_cast<float>(row) * pitch,
                width, height};
            x += width + gap;
        }
        if (align_right)
        {
            // Anchor each independently wrapped row to the right edge.
            // Never move text outside the window or collapse the task chip.
            for (std::size_t first = 0u; first < count;)
            {
                std::size_t last = first + 1u;
                while (last < count
                    && result.cells[last].y == result.cells[first].y)
                    ++last;
                const Cell& tail = result.cells[last - 1u];
                const float shift = (std::max)(0.0f,
                    viewport_width - inset - tail.x - tail.width);
                for (std::size_t i = first; i < last; ++i)
                    result.cells[i].x += shift;
                first = last;
            }
        }
        result.count = count;
        result.rows = row + 1u;
        result.height = static_cast<float>(row) * pitch + height;
        return result;
    }
}
