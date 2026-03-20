/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/

module;

#if defined(EPOCH_OVERRIDE_VERSION_MAJOR)
#  define EPOCH_VERSION_MAJOR_VALUE EPOCH_OVERRIDE_VERSION_MAJOR
#else
#  define EPOCH_VERSION_MAJOR_VALUE 0
#endif

#if defined(EPOCH_OVERRIDE_VERSION_MINOR)
#  define EPOCH_VERSION_MINOR_VALUE EPOCH_OVERRIDE_VERSION_MINOR
#else
#  define EPOCH_VERSION_MINOR_VALUE 83
#endif

#if defined(EPOCH_OVERRIDE_VERSION_REVISION)
#  define EPOCH_VERSION_REVISION_VALUE EPOCH_OVERRIDE_VERSION_REVISION
#else
#  define EPOCH_VERSION_REVISION_VALUE 13
#endif

export module aengine.version;

import <array>;
import <cstdio>;
import <string>;
import <string_view>;

export namespace epochnamespace
{
    export constexpr int major = EPOCH_VERSION_MAJOR_VALUE;
    export constexpr int minor = EPOCH_VERSION_MINOR_VALUE;
    export constexpr int revision = EPOCH_VERSION_REVISION_VALUE;

    export constexpr std::string_view kEngineName = "Epoch";

    export constexpr int GetMajor() noexcept
    {
        return major;
    }

    export constexpr int GetMinor() noexcept
    {
        return minor;
    }

    export constexpr int GetRevision() noexcept
    {
        return revision;
    }

    export constexpr std::string_view GetEngineNameView() noexcept
    {
        return kEngineName;
    }

    export const char* GetEngineName() noexcept
    {
        return kEngineName.data();
    }

    export const char* GetEngineVersion() noexcept
    {
        thread_local std::array<char, 32> buffer{};
        std::snprintf(
            buffer.data(),
            buffer.size(),
            "%d.%d.%d",
            major,
            minor,
            revision
        );
        return buffer.data();
    }

    export std::string GetEngineVersionString()
    {
        return std::string{ GetEngineVersion() };
    }

    export std::string GetEngineDisplayString()
    {
        std::string result{ kEngineName };
        result.push_back(' ');
        result += GetEngineVersion();
#if defined(EPOCH_UPDATER_SHELL_BUILD) && (EPOCH_UPDATER_SHELL_BUILD == 1)
        result += " (Updater Shell)";
#endif
        return result;
    }
}

export namespace epochengine = epochnamespace;
