/***********************************************
 * Epoch Engine - human-readable foreground logging.
 * SPDX-License-Identifier: MPL-2.0
 ***********************************************/
// Epoch system notes (auto-checked):
// Epoch Core: preserve engine-defined primitives, APIs and module contracts.
#pragma once

#include <string>
#include <string_view>

namespace epochengine::core::log::display
{
    [[nodiscard]] inline std::string_view field(
        const std::string_view message,
        const std::string_view key) noexcept
    {
        const auto pos = message.find(key);
        if (pos == std::string_view::npos)
            return {};
        const auto begin = pos + key.size();
        const auto end = message.find_first_of(" ;,\r\n", begin);
        return message.substr(begin,
            end == std::string_view::npos ? std::string_view::npos : end - begin);
    }

    [[nodiscard]] inline std::string compact_message(
        const std::string_view level,
        const std::string_view tag,
        std::string_view message)
    {
        // The complete, structured line is still written to the on-disk log.
        // Source location is most useful on warnings/errors, not every INFO.
        if (level.compare("INFO") == 0 && message.starts_with('('))
        {
            const auto marker = message.find(") - ");
            if (marker != std::string_view::npos && marker < 120)
                message.remove_prefix(marker + 4);
        }

        if (level.compare("INFO") == 0 && (tag.compare("Context.Multiplexer.Win") == 0
            || tag.compare("Context.Multiplexer.Linux") == 0))
        {
            if (message.starts_with("Backend ") && message.find(" window state activity=") != std::string_view::npos)
            {
                const auto backend_end = message.find(' ', 8);
                const auto backend = message.substr(8, backend_end - 8);
                std::string result{ backend };
                result += " window ";
                result += field(message, "activity=");
                result += " ";
                result += field(message, "logical=");
                const auto fb = field(message, "framebuffer=");
                if (!fb.empty()) { result += " (fb "; result += fb; result += ")"; }
                return result;
            }
            if (message.starts_with("Backend ") && message.find(" pacing requested=") != std::string_view::npos)
            {
                const auto backend_end = message.find(' ', 8);
                std::string result{ message.substr(8, backend_end - 8) };
                result += " pacing ";
                result += field(message, "effective=");
                result += " @ ";
                result += field(message, "effective_hz=");
                result += " Hz";
                return result;
            }
        }

        if (level.compare("INFO") == 0 && tag.compare("Engine.AI.Source") == 0
            && message.starts_with("Resident source; path="))
        {
            std::string result{ "Source " };
            result += field(message, "path=");
            result += ":";
            result += field(message, "first_line=");
            result += "-";
            result += field(message, "last_line=");
            return result;
        }

        // INFO output stays scan-friendly; ERROR/WARN are never truncated.
        constexpr std::size_t info_limit = 170;
        if (level.compare("INFO") == 0 && message.size() > info_limit)
        {
            std::string result{ message.substr(0, info_limit - 3) };
            result += "...";
            return result;
        }
        return std::string{ message };
    }

    [[nodiscard]] inline std::string format_line(
        const std::string_view timestamp,
        const std::string_view level,
        std::string_view tag,
        const std::string_view message)
    {
        // The date is implicit while watching one live session. Preserve
        // chronological time down to seconds; the full date stays on disk.
        const auto time_start = timestamp.rfind(' ');
        const auto time = time_start == std::string_view::npos
            ? timestamp : timestamp.substr(time_start + 1);
        if (tag.starts_with("Engine.AI."))
            tag.remove_prefix(7); // Engine.AI.Source -> AI.Source
        else if (tag.compare("Context.Multiplexer.Win") == 0)
            tag = "Context.Win";
        else if (tag.compare("Context.Multiplexer.Linux") == 0)
            tag = "Context.Linux";

        std::string line;
        line.reserve(time.size() + level.size() + tag.size() + message.size() + 12);
        line += time;
        line += " [";
        line += level;
        line += "] ";
        line += tag;
        line += ": ";
        // Forward original category so known chatty events can be summarized.
        line += compact_message(level,
            tag.compare("Context.Win") == 0 ? "Context.Multiplexer.Win" :
            tag.compare("Context.Linux") == 0 ? "Context.Multiplexer.Linux" :
            tag.compare("AI.Source") == 0 ? "Engine.AI.Source" : tag, message);
        return line;
    }
}
