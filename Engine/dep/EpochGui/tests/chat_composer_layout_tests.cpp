// SPDX-License-Identifier: MPL-2.0
#include <gui/chat_composer_layout.hpp>
#include <cassert>
#include <cmath>
#include <cstddef>
using namespace epochengine::gui_lib::chat_composer;

constexpr bool disjoint(Rect a, Rect b) noexcept
{
    return a.x + a.width <= b.x || b.x + b.width <= a.x
        || a.y + a.height <= b.y || b.y + b.height <= a.y;
}
constexpr bool inside(Rect r, Rect parent) noexcept
{
    return r.x >= parent.x && r.y >= parent.y
        && r.x + r.width <= parent.x + parent.width
        && r.y + r.height <= parent.y + parent.height;
}
constexpr bool valid_layout(float width, bool expanded, bool mic, std::size_t lines)
{
    auto layout = measure(width, 19.f, 29.f, lines, expanded, mic);
    if (!(layout.background.width == width
        && layout.background.height > 0.f
        && layout.input.width > 0.f
        && layout.input.height > 0.f
        && layout.visible_lines >= 1u && layout.visible_lines <= 6u
        && inside(layout.add, layout.background)
        && inside(layout.send, layout.background)
        && inside(layout.input, layout.background)
        && disjoint(layout.add, layout.input)
        && disjoint(layout.send, layout.input)
        && disjoint(layout.add, layout.send)))
        return false;
    if (mic && (!inside(layout.microphone, layout.background)
        || !disjoint(layout.microphone, layout.input)
        || !disjoint(layout.microphone, layout.add)
        || !disjoint(layout.microphone, layout.send)))
        return false;
    return true;
}
static_assert(valid_layout(240.f, false, true, 1u));
static_assert(valid_layout(800.f, false, true, 1u));
static_assert(valid_layout(160.f, true, true, 1u));
static_assert(valid_layout(116.f, true, true, 1u));
static_assert(valid_layout(100.f, true, false, 1u));
static_assert(valid_layout(240.f, true, true, 6u));
static_assert(valid_layout(800.f, true, true, 10u));
static_assert(single_line_text_width(240.f, true) == 120.f);
static_assert(expanded_text_width(240.f) == 224.f);
static_assert(microphone_fits(200.f));
static_assert(microphone_fits(180.f));
static_assert(!microphone_fits(110.f));
int main()
{
    for (float width : {100.f, 116.f, 160.f, 200.f, 240.f, 300.f, 600.f, 1400.f})
    {
        for (std::size_t lines = 1; lines <= 12; ++lines)
        {
            assert(valid_layout(width, false, microphone_fits(width), lines));
            assert(valid_layout(width, true, microphone_fits(width), lines));
            const auto one = measure(width, 19.f, 29.f, 1u, false,
                microphone_fits(width));
            assert(one.background.height >= 42.f);
            assert(one.background.height < 49.f);
        }
    }
    const auto two = measure(240.f, 19.f, 29.f, 2u, true, true);
    const auto six = measure(240.f, 19.f, 29.f, 6u, true, true);
    const auto eight = measure(240.f, 19.f, 29.f, 8u, true, true);
    assert(two.background.height < six.background.height);
    assert(six.background.height == eight.background.height);
}
