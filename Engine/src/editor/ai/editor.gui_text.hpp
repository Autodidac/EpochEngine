// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch editor presentation boundary: preserve valid UTF-8 model text while
// removing control sequences; translate legacy host terminology for display.
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
// Epoch Editor: keep editor state and source changes within their owning workflows.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace epochengine::editor_gui_text
{
    [[nodiscard]] inline std::string sanitize(std::string_view text)
    {
        std::string out;
        out.reserve(text.size());
        for (std::size_t i = 0; i < text.size();)
        {
            const auto c = static_cast<unsigned char>(text[i]);
            if (c == '\r') { ++i; continue; }
            if (c == '\t' || c == '\n' || (c >= 0x20u && c < 0x7fu))
            {
                out.push_back(static_cast<char>(c)); ++i; continue;
            }
            // ANSI CSI (including colored local-model terminal output) is
            // untrusted text, never a GUI styling instruction.
            if (c == 0x1bu)
            {
                ++i;
                if (i < text.size() && text[i] == '[')
                {
                    ++i;
                    while (i < text.size())
                    {
                        const unsigned char part = static_cast<unsigned char>(text[i++]);
                        if (part >= 0x40u && part <= 0x7eu) break;
                    }
                }
                continue;
            }
            if (c < 0x20u || c == 0x7fu || (c >= 0x80u && c < 0xc2u))
            {
                if (c >= 0x80u) out.push_back('?');
                ++i; continue;
            }
            const unsigned n = c < 0xe0u ? 2u : c < 0xf0u ? 3u : c < 0xf5u ? 4u : 0u;
            if (n == 0u || i + n > text.size())
            {
                out.push_back('?'); ++i; continue;
            }
            std::uint32_t cp = static_cast<std::uint32_t>(c & (n == 2 ? 0x1fu : n == 3 ? 0x0fu : 0x07u));
            bool valid = true;
            for (unsigned j = 1u; j < n; ++j)
            {
                const auto cc = static_cast<unsigned char>(text[i+j]);
                if ((cc & 0xc0u) != 0x80u) { valid = false; break; }
                cp = (cp << 6u) | (cc & 0x3fu);
            }
            valid = valid && cp >= (n == 2 ? 0x80u : n == 3 ? 0x800u : 0x10000u)
                && cp <= 0x10ffffu && !(cp >= 0xd800u && cp <= 0xdfffu)
                && !(cp >= 0x7fu && cp <= 0x9fu)
                && cp != 0x200bu && cp != 0xfeffu;
            if (valid) out.append(text.substr(i, n));
            else out.push_back('?');
            i += valid ? n : 1u;
        }
        return out;
    }

    [[nodiscard]] inline std::string host_status(std::string_view text)
    {
        const std::string safe = sanitize(text);
        std::string out;
        out.reserve(safe.size());
        const auto lower = [](unsigned char c) noexcept {
            return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
        };
        const auto matches = [&](std::size_t at, std::string_view value) noexcept {
            if (value.size() > safe.size() - at) return false;
            for (std::size_t j = 0; j < value.size(); ++j)
                if (lower(static_cast<unsigned char>(safe[at+j])) != value[j]) return false;
            return true;
        };
        for (std::size_t i = 0; i < safe.size();)
        {
            // Display-only transformation. Never change saved wire-format
            // identifiers, command parsing, or raw model/code output.
            std::size_t count{};
            if (matches(i, "engine self-coding")) count = sizeof("engine self-coding") - 1u;
            else if (matches(i, "self-coding")) count = 11u;
            else if (matches(i, "self coding")) count = 11u;
            else if (matches(i, "engine work")) count = sizeof("engine work") - 1u;
            else if (matches(i, "ai engine dev")) count = sizeof("ai engine dev") - 1u;
            else if (matches(i, "ai development")) count = sizeof("ai development") - 1u;
            else if (matches(i, "ai dev")) count = sizeof("ai dev") - 1u;
            if (count && (i == 0 || !((safe[i-1] >= 'A' && safe[i-1] <= 'Z')
                || (safe[i-1] >= 'a' && safe[i-1] <= 'z'))))
            {
                out += "Assistant";
                i += count;
            }
            else out.push_back(safe[i++]);
        }
        return out;
    }
}
