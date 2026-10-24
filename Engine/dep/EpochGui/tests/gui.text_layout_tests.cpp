// SPDX-License-Identifier: MPL-2.0
#include <cstddef>
#include <limits>
#include <string_view>
import epoch.gui;
namespace gui = epochengine::gui_lib;

static bool invalid_metric_index = false;
static gui::TextGlyphMetrics measure(std::string_view text, std::size_t index, const void*)
{
    if (index >= text.size()) { invalid_metric_index = true; return {}; }
    const unsigned char value = static_cast<unsigned char>(text[index]);
    const auto count = value == 0xc3 ? 2u : value == 0xf0 ? 4u : 1u;
    // Deliberately nonuniform widths to prove consumer metrics govern layout.
    const float width = text[index] == 'W' ? 12.0f : text[index] == ' ' ? 4.0f : 8.0f;
    return {count, width, text[index] == ' '};
}
int main()
{
    const gui::TextLayoutMetrics metrics{measure, nullptr, 4.0f};
    auto lines = gui::text_visual_lines("ab cd", 24.0f, true, metrics);
    if (lines.size() != 2u || lines[0].past_last != 3u || lines[1].first != 3u) return 1;
    if (gui::text_visual_line_for_index(lines, 3u) != 1u) return 2;
    lines = gui::text_visual_lines("ab\ncd\n", 200.0f, true, metrics);
    if (lines.size() != 3u || lines[2].first != 6u || lines[2].past_last != 6u) return 3;
    if (gui::text_visual_line_for_index(lines, 2u) != 0u
        || gui::text_visual_line_for_index(lines, 3u) != 1u) return 4;
    const std::string_view unicode = "a\xc3\xa9\xf0\x9f\x98\x80";
    lines = gui::text_visual_lines(unicode, 9.0f, true, metrics);
    if (lines.size() != 3u || lines[1].first != 1u || lines[1].past_last != 3u
        || lines[2].first != 3u || lines[2].past_last != 7u) return 5;
    if (gui::text_index_from_point(unicode, lines, {5.0f, 12.0f}, 10.0f, true, metrics) != 3u
        || gui::text_index_from_point(unicode, lines, {3.0f, 12.0f}, 10.0f, true, metrics) != 1u) return 6;
    lines = gui::text_visual_lines("Wab", 1.0f, true, metrics);
    if (lines.size() != 3u || lines[0].past_last != 1u) return 7;
    lines = gui::text_visual_lines("", 0.0f, true, metrics);
    if (lines.size() != 1u || lines[0].first != 0u || lines[0].past_last != 0u) return 8;
    lines = gui::text_visual_lines("ab\ncd", 1.0f, false, metrics);
    if (lines.size() != 1u || lines[0].past_last != 5u) return 9;
    if (gui::text_index_from_point("ab\ncd", lines, {-1.0f, 500.0f}, 10.0f, false, metrics) != 0u
        || gui::text_index_from_point("ab\ncd", lines, {500.0f, 500.0f}, 10.0f, false, metrics) != 5u) return 10;
    lines = gui::text_visual_lines("a\n\nb", 100.0f, true, metrics);
    if (lines.size() != 3u || lines[1].first != lines[1].past_last) return 11;
    lines = {{100u, 200u}};
    if (gui::text_index_from_point("ab", lines, {500.0f, 0.0f}, 10.0f, true, metrics) != 2u
        || invalid_metric_index) return 12;
    lines = {{1u, 200u}};
    if (gui::text_index_from_point("ab", lines, {500.0f, 0.0f}, 10.0f, true, metrics) != 2u
        || invalid_metric_index) return 13;
    lines = {{1u, 0u}};
    if (gui::text_index_from_point("ab", lines, {500.0f, 0.0f}, 10.0f, true, metrics) != 1u) return 14;
    lines = gui::text_visual_lines("ab\ncd", 100.0f, true, metrics);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    if (gui::text_index_from_point("ab\ncd", lines, {nan, nan}, 10.0f, true, metrics) != 0u) return 15;
    if (gui::text_index_from_point("ab\ncd", lines, {0.0f, infinity}, 10.0f, true, metrics) != 3u
        || gui::text_index_from_point("ab\ncd", lines, {0.0f, -infinity}, 10.0f, true, metrics) != 0u) return 16;
    if (gui::text_index_from_point("ab\ncd", lines, {0.0f, 1.0f}, nan, true, metrics) != 3u) return 17;
    return 0;
}
