#pragma once
#include "epoch.edition.hpp"
#include <string_view>

// Historical channel flags cannot change the edition's repository authority.
#if defined(EPOCH_UPDATE_DISTRIBUTION_SITE)
#undef EPOCH_UPDATE_DISTRIBUTION_SITE
#endif
#define EPOCH_UPDATE_DISTRIBUTION_SITE (1 - EPOCH_SOURCE_EDITION_PUBLIC)

namespace epochengine::updater::distribution
{
    inline constexpr bool public_edition = EPOCH_EDITION_PUBLIC != 0;
    inline constexpr std::string_view edition_id = public_edition
        ? "epoch.public.github.v1" : "epoch.development.site.v1";
    inline constexpr std::string_view source_marker = public_edition
        ? "#define EPOCH_SOURCE_EDITION_PUBLIC 1"
        : "#define EPOCH_SOURCE_EDITION_PUBLIC 0";

    [[nodiscard]] inline constexpr bool plain_url(std::string_view url) noexcept
    {
        for (const unsigned char ch : url)
            if (ch < 0x21u || ch > 0x7eu) return false;
        return url.starts_with("https://") && url.find_first_of("\\ #%?@") == url.npos
            && url.find("/../") == url.npos && url.find("/./") == url.npos;
    }

    [[nodiscard]] inline constexpr bool release_download_allowed(
        std::string_view url) noexcept
    {
        return plain_url(url) && url.starts_with(public_edition
            ? "https://github.com/Autodidac/EpochEngine/releases/download/"
            : "https://epoch.adamrushford.chatgpt.site/downloads/releases/");
    }

    [[nodiscard]] inline constexpr bool marker_matches(std::string_view bytes) noexcept
    {
        // One exact, line-bounded identity; reject duplicate or conflicting defines.
        constexpr std::string_view key = "#define EPOCH_SOURCE_EDITION_PUBLIC ";
        const auto pos = bytes.find(key);
        if (pos == bytes.npos || (pos != 0u && bytes[pos - 1u] != '\n')
            || bytes.find(key, pos + key.size()) != bytes.npos
            || !bytes.substr(pos).starts_with(source_marker)) return false;
        const auto end = pos + source_marker.size();
        return end == bytes.size() || bytes[end] == '\r' || bytes[end] == '\n';
    }
}
