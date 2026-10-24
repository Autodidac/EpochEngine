// SPDX-License-Identifier: MPL-2.0
// Renderer-independent UTF-8 clipping. Do not split codepoints at length limits.
#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace epochengine::gui_lib::utf8_text_layout
{
    [[nodiscard]] constexpr std::string_view prefix(std::string_view text,
        std::size_t maximum_bytes) noexcept
    {
        std::size_t end = (std::min)(text.size(), maximum_bytes);
        if (end >= text.size()) return text;
        while (end > 0u && (static_cast<unsigned char>(text[end]) & 0xc0u) == 0x80u)
            --end;
        return text.substr(0u, end);
    }
}
