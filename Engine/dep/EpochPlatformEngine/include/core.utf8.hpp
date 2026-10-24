// SPDX-License-Identifier: MPL-2.0
// Shared text admission, extracted from the existing AI packet UTF-8 validator.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace epochengine::core::utf8
{
    [[nodiscard]] inline bool valid(std::string_view text) noexcept
    {
        std::size_t index{};
        while (index < text.size())
        {
            const std::uint8_t lead =
                static_cast<std::uint8_t>(text[index]);
            if (lead <= 0x7fu)
            {
                ++index;
                continue;
            }

            std::size_t continuationCount{};
            std::uint32_t codePoint{};
            std::uint32_t minimum{};
            if ((lead & 0xe0u) == 0xc0u)
            {
                continuationCount = 1u;
                codePoint = lead & 0x1fu;
                minimum = 0x80u;
            }
            else if ((lead & 0xf0u) == 0xe0u)
            {
                continuationCount = 2u;
                codePoint = lead & 0x0fu;
                minimum = 0x800u;
            }
            else if ((lead & 0xf8u) == 0xf0u)
            {
                continuationCount = 3u;
                codePoint = lead & 0x07u;
                minimum = 0x10000u;
            }
            else
            {
                return false;
            }
            if (continuationCount > text.size() - index - 1u)
                return false;
            for (std::size_t offset = 1u;
                 offset <= continuationCount; ++offset)
            {
                const std::uint8_t byte =
                    static_cast<std::uint8_t>(text[index + offset]);
                if ((byte & 0xc0u) != 0x80u)
                    return false;
                codePoint = (codePoint << 6u) | (byte & 0x3fu);
            }
            if (codePoint < minimum || codePoint > 0x10ffffu
                || (codePoint >= 0xd800u && codePoint <= 0xdfffu))
            {
                return false;
            }
            index += continuationCount + 1u;
        }
        return true;
    }

}
