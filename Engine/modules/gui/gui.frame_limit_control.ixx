// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Engine-owned pacing adapter; the portable EpochGui library has no perf dependency.
module;

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <span>
#include <string>
#include <string_view>

export module gui.frame_limit_control;

import gui.engine;
import perf.tier;

export namespace epochengine::gui
{
    struct FrameLimitChoice final
    {
        std::string_view label{};
        std::string_view argument{};
        double fps{};
    };

    struct FrameLimitEditState final
    {
        bool initialized{};
        bool manual{};
        std::string text{"1500"};
        std::string error{};
    };

    [[nodiscard]] inline std::span<const FrameLimitChoice> frame_limit_choices() noexcept
    {
        static constexpr std::array<FrameLimitChoice, 5> choices{{
            {"30 FPS", "30", 30.0}, {"60 FPS", "60", 60.0},
            {"120 FPS", "120", 120.0}, {"0 (Unlimited)", "0", 0.0},
            {"Manual", "1500", 1500.0}}};
        return choices;
    }

    [[nodiscard]] inline std::string_view frame_limit_label(double fps) noexcept
    {
        return perf::label_for_frame_limit(fps);
    }

    [[nodiscard]] inline std::string frame_limit_argument(double fps)
    {
        for (const auto& choice : frame_limit_choices())
            if (choice.fps == fps) return std::string{choice.argument};
        if (!std::isfinite(fps) || fps <= 0.0) return "0";
        std::array<char, 64> text{};
        const auto result = std::to_chars(text.data(), text.data() + text.size(), fps);
        return result.ec == std::errc{} ? std::string{text.data(), result.ptr} : "0";
    }

    [[nodiscard]] inline bool frame_limit_control(std::string_view id, double& fps,
        FrameLimitEditState& edit, float width, bool vsync = false, bool enabled = true)
    {
        const auto start = cursor_position();
        const auto choices = frame_limit_choices();
        if (!edit.initialized)
        {
            edit.initialized = true;
            edit.manual = fps != 0.0 && fps != 30.0 && fps != 60.0 && fps != 120.0;
            if (edit.manual) edit.text = frame_limit_argument(fps);
        }
        std::array<std::string_view, 5> labels{};
        for (std::size_t n = 0; n < choices.size(); ++n) labels[n] = choices[n].label;
        const float rowWidth = (std::max)(1.0f, width);
        const bool stacked = rowWidth < 280.0f;
        const float selectWidth = stacked ? rowWidth : (std::min)(250.0f, rowWidth - 160.0f);
        const auto selected = select_box(SelectBoxOptions{
            .id = id, .placeholder = "Frame limit",
            .selected = edit.manual ? "Manual" : vsync ? "VSync"
                : fps == 0.0 ? "0 (Unlimited)" : frame_limit_label(fps),
            .options = labels, .size = {selectWidth, 30.0f},
            .row_height = 28.0f, .max_visible_options = 5, .enabled = enabled});
        bool changed = false;
        if (enabled && selected.changed && selected.selected_index && *selected.selected_index < choices.size())
        {
            edit.manual = *selected.selected_index == choices.size() - 1u;
            edit.error.clear();
            if (edit.manual)
            {
                double parsed{};
                if (perf::parse_frame_limit_value(edit.text, parsed)) { fps = parsed; changed = true; }
                else edit.error = "Enter 0 or a positive FPS.";
            }
            else { fps = choices[*selected.selected_index].fps; changed = true; }
        }
        if (edit.manual)
        {
            const ScopedInputSuppression disabled{!enabled};
            const Vec2 manualPos{stacked ? start.x : start.x + selectWidth + 8.0f,
                start.y + (stacked ? 34.0f : 0.0f)};
            const float inputWidth = stacked ? (std::min)(88.0f, (std::max)(32.0f, rowWidth - 64.0f)) : 88.0f;
            set_cursor(manualPos);
            const auto input = edit_box(edit.text, {inputWidth, 30.0f}, 24u, false,
                TextInputKeyboardPolicy::focused, true, "1500");
            set_cursor({manualPos.x + inputWidth + 8.0f, manualPos.y});
            const bool apply = button("Apply", {56.0f, 30.0f});
            if (enabled && (input.submitted || apply))
            {
                double parsed{};
                if (perf::parse_frame_limit_value(edit.text, parsed))
                { fps = parsed; changed = true; edit.error.clear(); }
                else edit.error = "Enter 0 or a positive FPS.";
            }
        }
        set_cursor({start.x, start.y + (stacked && edit.manual ? 66.0f : 32.0f)});
        if (!edit.error.empty()) label(edit.error);
        return changed;
    }

    // Only this window's foreground selection changes. Activity caps continue
    // through perf.tier in both the session and renderer consumers. No persistence.
    [[nodiscard]] inline bool live_frame_limit_control(std::string_view id,
        perf::frame_pacing_policy startup, perf::frame_pacing_override* context,
        FrameLimitEditState& edit, float width)
    {
        const auto selected = context ? perf::with_frame_pacing_override(startup, *context) : startup;
        double fps = selected.target_hz;
        const bool changed = frame_limit_control(id, fps, edit, width,
            selected.mode == perf::frame_pacing_mode::vsync, context != nullptr);
        if (!context)
        {
            label("Frame limit unavailable: no runtime window.");
            return false;
        }
        if (changed)
            context->publish(fps > 0.0 ? perf::frame_pacing_mode::target_hz
                : perf::frame_pacing_mode::uncapped, fps);
        return changed;
    }
}
