// SPDX-License-Identifier: MPL-2.0
// Shared, renderer-independent geometry for the EpochGUI compact chat composer.
#pragma once

#include <algorithm>
#include <cstddef>

namespace epochengine::gui_lib::chat_composer
{
    struct Rect
    {
        float x{};
        float y{};
        float width{};
        float height{};
    };

    struct Layout
    {
        Rect background{};
        Rect input{};
        Rect add{};
        Rect microphone{};
        Rect pause{};
        Rect send{};
        std::size_t visible_lines{1u};
        bool expanded{};
        bool has_microphone{};
        bool has_pause{};
    };

    inline constexpr float button_size = 30.0f;
    inline constexpr float control_gap = 6.0f;
    inline constexpr float edge_inset = 6.0f;
    inline constexpr float input_inset = 8.0f;
    inline constexpr std::size_t maximum_visible_lines = 6u;

    // Width available to *edit_box* (which applies its own internal 5px inset).
    // Callers must use input.width minus those two insets when measuring wraps.
    [[nodiscard]] constexpr float single_line_text_width(float width,
        bool microphone, bool pause = false) noexcept
    {
        const float left = edge_inset + button_size + control_gap;
        const float right = edge_inset + button_size + control_gap
            + (microphone ? button_size + control_gap : 0.0f)
            + (pause ? button_size + control_gap : 0.0f);
        return (std::max)(1.0f, width - left - right);
    }

    [[nodiscard]] constexpr float expanded_text_width(float width) noexcept
    {
        return (std::max)(1.0f, width - 2.0f * input_inset);
    }

    [[nodiscard]] constexpr bool microphone_fits(float width,
        bool pause = false) noexcept
    {
        // Every visible circular control has its complete 30 px hitbox.
        // The pause button consumes the same width in both layout modes.
        return width >= 2.0f * edge_inset
            + (pause ? 4.0f : 3.0f) * button_size
            + (pause ? 3.0f : 2.0f) * control_gap;
    }

    [[nodiscard]] constexpr Layout measure(float width, float line_advance,
        float minimum_text_height, std::size_t visual_lines, bool expanded,
        bool microphone, bool pause = false) noexcept
    {
        Layout result{};
        const float w = (std::max)(1.0f, width);
        const float line = (std::max)(1.0f, line_advance);
        const float text_min = (std::max)(minimum_text_height, line + 10.0f);
        result.expanded = expanded;
        result.has_microphone = microphone;
        result.has_pause = pause;
        result.visible_lines = (std::clamp)(visual_lines,
            std::size_t{1u}, maximum_visible_lines);
        if (expanded)
        {
            const float text_h = (std::max)(text_min,
                static_cast<float>(result.visible_lines) * line + 10.0f);
            result.input = {input_inset, 4.0f,
                expanded_text_width(w), text_h};
            const float controls_y = result.input.y + text_h + 4.0f;
            const float send_x = w - edge_inset - button_size;
            result.add = {edge_inset, controls_y, button_size, button_size};
            result.send = {send_x, controls_y, button_size, button_size};
            if (pause)
                result.pause = {send_x - control_gap - button_size,
                    controls_y, button_size, button_size};
            if (microphone)
                result.microphone = {send_x - (pause ? 2.0f : 1.0f)
                    * (control_gap + button_size),
                    controls_y, button_size, button_size};
            result.background = {0.0f, 0.0f, w,
                controls_y + button_size + edge_inset};
        }
        else
        {
            const float h = (std::max)(42.0f, text_min + 8.0f);
            const float controls_y = (h - button_size) * 0.5f;
            const float send_x = w - edge_inset - button_size;
            result.add = {edge_inset, controls_y, button_size, button_size};
            result.send = {send_x, controls_y, button_size, button_size};
            if (pause)
                result.pause = {send_x - control_gap - button_size,
                    controls_y, button_size, button_size};
            if (microphone)
                result.microphone = {send_x - (pause ? 2.0f : 1.0f)
                    * (control_gap + button_size),
                    controls_y, button_size, button_size};
            result.input = {
                edge_inset + button_size + control_gap, 4.0f,
                single_line_text_width(w, microphone, pause), h - 8.0f};
            result.background = {0.0f, 0.0f, w, h};
        }
        return result;
    }
} // namespace epochengine::gui_lib::chat_composer
