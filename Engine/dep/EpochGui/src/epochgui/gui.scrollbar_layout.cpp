// SPDX-License-Identifier: MPL-2.0
module;
#include <algorithm>
#include <cmath>
module epoch.gui;

namespace epochengine::gui_lib
{
    ScrollbarLayout make_scrollbar_layout(const ScrollbarLayoutOptions& options) noexcept
    {
        ScrollbarLayout result{};
        result.track = options.track;
        const float extent = std::isfinite(options.track.size.y)
            ? (std::max)(0.0f, options.track.size.y) : 0.0f;
        result.track.size.y = extent;
        const float content = std::isfinite(options.content_extent)
            ? (std::max)(extent, options.content_extent) : extent;
        result.maximum_scroll_offset = content - extent;
        result.scroll_offset = std::isfinite(options.scroll_offset)
            ? (std::clamp)(options.scroll_offset, 0.0f, result.maximum_scroll_offset) : 0.0f;
        const float minimum = std::isfinite(options.minimum_thumb_extent)
            ? (std::max)(0.0f, options.minimum_thumb_extent) : 18.0f;
        const float visibleRatio = content > 0.0f ? extent / content : 1.0f;
        const float thumbExtent = (std::min)(extent, (std::max)(minimum, extent * visibleRatio));
        const float ratio = result.maximum_scroll_offset > 0.0f
            ? result.scroll_offset / result.maximum_scroll_offset : 0.0f;
        result.thumb = { {result.track.position.x, result.track.position.y + (extent - thumbExtent) * ratio},
            {result.track.size.x, thumbExtent} };
        return result;
    }

    float scrollbar_grab_offset(const ScrollbarLayout& layout, float pointer_y, bool thumb_hovered) noexcept
    {
        return thumb_hovered ? pointer_y - layout.thumb.position.y : layout.thumb.size.y * 0.5f;
    }

    float scrollbar_offset_from_pointer(const ScrollbarLayout& layout, float pointer_y, float grab_offset) noexcept
    {
        // Preserve the adapter's one-pixel minimum travel for tiny tracks.
        const float travel = (std::max)(1.0f, layout.track.size.y - layout.thumb.size.y);
        const float requested = (pointer_y - layout.track.position.y - grab_offset) / travel;
        return std::isfinite(requested)
            ? (std::clamp)(requested, 0.0f, 1.0f) * layout.maximum_scroll_offset : layout.scroll_offset;
    }
}
