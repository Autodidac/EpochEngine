#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace epochengine::gui_lib::chat_context_popup
{
    struct Rect final
    {
        float x{};
        float y{};
        float width{};
        float height{};
    };

    struct Geometry final
    {
        Rect bounds{};
        float button_height{};
        float padding{};
        float gap{};
    };

    // Layout is measured with the same button height and spacing that the
    // renderer advances its cursor by. Never use the unclipped desktop or
    // parent-window bounds for a popup owned by a chat scroll viewport.
    [[nodiscard]] inline Geometry place(
        Rect viewport, float anchor_x, float anchor_y,
        float requested_width, float button_height, std::size_t rows,
        float padding, float gap) noexcept
    {
        if (!std::isfinite(viewport.x) || !std::isfinite(viewport.y)
            || !std::isfinite(viewport.width) || !std::isfinite(viewport.height)
            || !std::isfinite(anchor_x) || !std::isfinite(anchor_y))
            return {};
        viewport.width = std::max(0.0f, viewport.width);
        viewport.height = std::max(0.0f, viewport.height);
        padding = std::max(0.0f, padding);
        gap = std::max(0.0f, gap);
        button_height = std::max(0.0f, button_height);
        const float width = std::min(viewport.width, std::max(0.0f, requested_width));
        const float wanted_height = padding * 2.0f
            + static_cast<float>(rows) * button_height
            + static_cast<float>(rows > 0u ? rows - 1u : 0u) * gap;
        const float height = std::min(viewport.height, wanted_height);
        const float x = std::clamp(anchor_x, viewport.x,
            viewport.x + viewport.width - width);
        const float y = std::clamp(anchor_y, viewport.y,
            viewport.y + viewport.height - height);
        return {{x, y, width, height}, button_height, padding, gap};
    }
}
