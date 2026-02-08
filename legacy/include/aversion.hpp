/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondShell - Modular C++ Framework                      *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 *   Use permitted for Non-Commercial Purposes ONLY,          *
 *   without prior commercial licensing agreement.            *
 *                                                            *
 *   Redistribution Allowed with This Notice and              *
 *   LICENSE file. No obligation to disclose modifications.   *
 *                                                            *
 *   See LICENSE file for full terms.                         *
 *                                                            *
 **************************************************************/
 // aversion.hpp
#pragma once

#include <array>
#include <cstdio>
#include <string>
#include <string_view>

namespace almondnamespace
{
    // Version information as constexpr for compile-time evaluation
    inline constexpr int major = 0;
    inline constexpr int minor = 81;
    inline constexpr int revision = 44;

    inline constexpr std::string_view kEngineName = "Almond Shell";

    // Use inline to ensure that each definition is treated uniquely in different translation units
    inline int GetMajor() { return major; }
    inline int GetMinor() { return minor; }
    inline int GetRevision() { return revision; }

    inline const char* GetEngineName()
    {
        return kEngineName.data();
    }

    inline std::string_view GetEngineNameView()
    {
        return kEngineName;
    }

    inline const char* GetEngineVersion()
    {
        thread_local std::array<char, 32> version_string{};
        std::snprintf(version_string.data(), version_string.size(), "%d.%d.%d", major, minor, revision);
        return version_string.data();
    }

    inline std::string GetEngineVersionString()
    {
        return std::string{ GetEngineVersion() };
    }

} // namespace almondnamespace

// Keep the engine name accessible even when builds expect the legacy
// almondshell namespace alias instead of almondnamespace.
namespace almondshell = almondnamespace;
