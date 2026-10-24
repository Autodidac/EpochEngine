// SPDX-License-Identifier: MPL-2.0
module;
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>
#include <span>
#include <string_view>
#include <vector>
module epoch.gui;
namespace epochengine::gui_lib
{
    std::vector<TextVisualLine> text_visual_lines(
        std::string_view text,
        float wrapWidth,
        bool multiline,
        const TextLayoutMetrics& metrics)
    {
        std::vector<TextVisualLine> lines{};
        if (!multiline || !metrics.measure)
        {
            lines.push_back({0u, text.size()});
            return lines;
        }

        const float effectiveWidth = (std::max)(metrics.minimum_width, wrapWidth);
        std::size_t hardStart = 0u;
        while (hardStart <= text.size())
        {
            const std::size_t hardEnd = [&] { const auto end = text.find('\n', hardStart); return end == std::string_view::npos ? text.size() : end; }();
            if (hardStart == hardEnd)
            {
                lines.push_back({hardStart, hardEnd});
            }
            else
            {
                std::size_t segmentStart = hardStart;
                while (segmentStart < hardEnd)
                {
                    float penX = 0.0f;
                    std::size_t cursor = segmentStart;
                    std::size_t lastBreak = segmentStart;
                    std::size_t segmentEnd = hardEnd;
                    bool wrapped = false;

                    while (cursor < hardEnd)
                    {
                        const auto glyph = metrics.measure(text, cursor, metrics.context);
                        const std::size_t byteCount = (std::max)(std::size_t{1u}, glyph.byte_count);
                        const std::size_t next = cursor + (std::min)(hardEnd - cursor, byteCount);
                        const float advance = glyph.advance;

                        if (penX > 0.0f && penX + advance > effectiveWidth + 0.001f)
                        {
                            segmentEnd = lastBreak > segmentStart
                                ? lastBreak : cursor;
                            if (segmentEnd <= segmentStart)
                                segmentEnd = next;
                            wrapped = true;
                            break;
                        }

                        penX += advance;
                        if (glyph.wrap_space)
                            lastBreak = next;
                        cursor = next;
                    }

                    if (!wrapped)
                        segmentEnd = hardEnd;
                    lines.push_back({segmentStart, segmentEnd});
                    segmentStart = segmentEnd;
                }
            }

            if (hardEnd >= text.size())
                break;
            hardStart = hardEnd + 1u;
        }

        if (lines.empty())
            lines.push_back({0u, 0u});
        return lines;
    }

    std::size_t text_visual_line_for_index(
        std::span<const TextVisualLine> lines,
        std::size_t index) noexcept
    {
        if (lines.empty()) return 0u;
        for (std::size_t line = 0u; line < lines.size(); ++line)
        {
            const auto& current = lines[line];
            const bool last = line + 1u >= lines.size();
            const std::size_t nextStart = last
                ? (std::numeric_limits<std::size_t>::max)()
                : lines[line + 1u].first;
            if (index < current.past_last
                || (index == current.past_last && (last || index < nextStart)))
                return line;
        }
        return lines.size() - 1u;
    }

    std::size_t text_index_from_point(std::string_view text,
        std::span<const TextVisualLine> lines, Vec2 local_point, float line_advance,
        bool multiline, const TextLayoutMetrics& metrics) noexcept
    {
        if (lines.empty() || !metrics.measure) return 0u;
        // Avoid converting non-finite or rounded-out-of-range row values to integers.
        const float pitch = std::isfinite(line_advance) ? (std::max)(1.0f, line_advance) : 1.0f;
        const float row = local_point.y / pitch;
        std::size_t requestedLine = 0u;
        if (multiline && row > 0.0f)
            requestedLine = row >= static_cast<float>(lines.size() - 1u)
                ? lines.size() - 1u : static_cast<std::size_t>(row);
        auto line = lines[requestedLine];
        line.first = (std::min)(line.first, text.size());
        line.past_last = (std::clamp)(line.past_last, line.first, text.size());
        if (!(local_point.x > 0.0f)) return line.first;
        float penX = 0.0f;
        std::size_t index = line.first;
        while (index < line.past_last)
        {
            const auto glyph = metrics.measure(text, index, metrics.context);
            if (local_point.x <= penX + glyph.advance * 0.5f) return index;
            penX += glyph.advance;
            index += (std::min)(line.past_last - index, (std::max)(std::size_t{1u}, glyph.byte_count));
        }
        return line.past_last;
    }
}
