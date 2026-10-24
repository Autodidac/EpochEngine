// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Strict packaged-runtime identity, shared by native admission and CPU checks.
#pragma once
#include <cstddef>
#include <regex>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace epochengine::updater::runtime_identity
{
    [[nodiscard]] inline bool matches(
        const std::string_view marker, const std::string_view expected_version,
        const std::string_view expected_edition)
    {
        static const std::regex version_pattern{R"((0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*))", std::regex::optimize};
        if (marker.empty() || marker.size() > 4096u || expected_version.size() > 32u
            || !std::regex_match(expected_version.begin(), expected_version.end(), version_pattern))
            return false;
        std::size_t root_keys{};
        const auto document = nlohmann::json::parse(marker,
            [&](int depth, nlohmann::json::parse_event_t event, nlohmann::json&)
            {
                if (depth == 1 && event == nlohmann::json::parse_event_t::key)
                    ++root_keys;
                return true;
            }, false);
        if (!document.is_object() || document.size() != 3u || root_keys != 3u)
            return false;
        const auto schema = document.find("schema");
        const auto edition = document.find("edition");
        const auto version = document.find("version");
        return schema != document.end() && schema->is_string()
            && *schema == "epoch.distribution.v1"
            && edition != document.end() && edition->is_string()
            && edition->get_ref<const std::string&>() == expected_edition
            && version != document.end() && version->is_string()
            && version->get_ref<const std::string&>() == expected_version;
    }

    [[nodiscard]] inline constexpr bool markerless_allowed(
        bool public_edition, std::string_view expected_version) noexcept
    {
        return !public_edition && expected_version == "0.90.55";
    }
}
