#pragma once
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include "epoch.edition.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace epochengine::build_layout
{
    enum class Layout { Legacy, Shared, Invalid };
    inline constexpr std::string_view edition = EPOCH_EDITION_PUBLIC ? "public" : "development";
    inline constexpr std::string_view schema = "epoch.build-output-layout/v1";

    // Select from the source's declared build graph, never from whichever old
    // executable happens to exist. Retained pre-migration source stays usable.
    [[nodiscard]] inline Layout source_layout(const std::filesystem::path& root)
    {
        const auto marker = root / "Tools/MSVC/EpochOutputLayout.props";
        std::error_code error;
        const auto status = std::filesystem::symlink_status(marker, error);
        if (status.type() == std::filesystem::file_type::not_found) return Layout::Legacy;
        if (error || !std::filesystem::is_regular_file(status)
            || std::filesystem::is_symlink(status)) return Layout::Invalid;
        if (std::filesystem::canonical(marker, error) != marker.lexically_normal() || error)
            return Layout::Invalid;
        std::ifstream input{marker, std::ios::binary};
        std::array<char, 256> prefix{};
        input.read(prefix.data(), prefix.size());
        const std::string_view content{prefix.data(), static_cast<std::size_t>(input.gcount())};
        return content.find(schema) != std::string_view::npos ? Layout::Shared : Layout::Invalid;
    }

    [[nodiscard]] inline std::filesystem::path windows_relative_bin(
        const std::filesystem::path& root, std::string_view configuration,
        std::string_view platform = "x64")
    {
        switch (source_layout(root))
        {
        case Layout::Shared: return std::filesystem::path{"build/bin/windows"} / edition / platform / configuration;
        case Layout::Legacy: return std::filesystem::path{platform} / configuration;
        case Layout::Invalid: return {};
        }
        return {};
    }

    [[nodiscard]] inline std::filesystem::path windows_bin(
        const std::filesystem::path& root, std::string_view configuration,
        std::string_view platform = "x64")
    {
        const auto relative = windows_relative_bin(root, configuration, platform);
        return relative.empty() ? std::filesystem::path{} : root / relative;
    }

    [[nodiscard]] inline std::filesystem::path linux_cmake(
        const std::filesystem::path& root, std::string_view compiler_configuration)
    {
        switch (source_layout(root))
        {
        case Layout::Shared: return root / "build/cmake/linux" / edition / compiler_configuration;
        case Layout::Legacy: return root / "Engine/Bin" / compiler_configuration;
        case Layout::Invalid: return {};
        }
        return {};
    }
}
