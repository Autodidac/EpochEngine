/************************************************
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•—  â–ˆâ–ˆâ•—   *
 *  â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ•”â•â•â•  â–ˆâ–ˆâ•”â•â•â•â• â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘     â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â•šâ•â•â•â•â•â•â•â•šâ•â•      â•šâ•â•â•â•â•â•  â•šâ•â•â•â•â•â•â•šâ•â•  â•šâ•â•   *
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

#include <array>
#include <cstdio>
#include <string>
#include <string_view>

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
#  define EPOCH_VERSION_REVISION_VALUE 70
#endif

#if defined(EPOCH_OVERRIDE_WINDOWS_PACKAGED_VERSION_MAJOR)
#  define EPOCH_WINDOWS_PACKAGED_VERSION_MAJOR_VALUE EPOCH_OVERRIDE_WINDOWS_PACKAGED_VERSION_MAJOR
#else
#  define EPOCH_WINDOWS_PACKAGED_VERSION_MAJOR_VALUE EPOCH_VERSION_MAJOR_VALUE
#endif

#if defined(EPOCH_OVERRIDE_WINDOWS_PACKAGED_VERSION_MINOR)
#  define EPOCH_WINDOWS_PACKAGED_VERSION_MINOR_VALUE EPOCH_OVERRIDE_WINDOWS_PACKAGED_VERSION_MINOR
#else
#  define EPOCH_WINDOWS_PACKAGED_VERSION_MINOR_VALUE EPOCH_VERSION_MINOR_VALUE
#endif

#if defined(EPOCH_OVERRIDE_WINDOWS_PACKAGED_VERSION_REVISION)
#  define EPOCH_WINDOWS_PACKAGED_VERSION_REVISION_VALUE EPOCH_OVERRIDE_WINDOWS_PACKAGED_VERSION_REVISION
#else
#  define EPOCH_WINDOWS_PACKAGED_VERSION_REVISION_VALUE EPOCH_VERSION_REVISION_VALUE
#endif

#if defined(EPOCH_OVERRIDE_LINUX_PACKAGED_VERSION_MAJOR)
#  define EPOCH_LINUX_PACKAGED_VERSION_MAJOR_VALUE EPOCH_OVERRIDE_LINUX_PACKAGED_VERSION_MAJOR
#else
#  define EPOCH_LINUX_PACKAGED_VERSION_MAJOR_VALUE EPOCH_VERSION_MAJOR_VALUE
#endif

#if defined(EPOCH_OVERRIDE_LINUX_PACKAGED_VERSION_MINOR)
#  define EPOCH_LINUX_PACKAGED_VERSION_MINOR_VALUE EPOCH_OVERRIDE_LINUX_PACKAGED_VERSION_MINOR
#else
#  define EPOCH_LINUX_PACKAGED_VERSION_MINOR_VALUE EPOCH_VERSION_MINOR_VALUE
#endif

#if defined(EPOCH_OVERRIDE_LINUX_PACKAGED_VERSION_REVISION)
#  define EPOCH_LINUX_PACKAGED_VERSION_REVISION_VALUE EPOCH_OVERRIDE_LINUX_PACKAGED_VERSION_REVISION
#else
#  define EPOCH_LINUX_PACKAGED_VERSION_REVISION_VALUE EPOCH_VERSION_REVISION_VALUE
#endif

#if defined(EPOCH_OVERRIDE_MACOS_PACKAGED_VERSION_MAJOR)
#  define EPOCH_MACOS_PACKAGED_VERSION_MAJOR_VALUE EPOCH_OVERRIDE_MACOS_PACKAGED_VERSION_MAJOR
#else
#  define EPOCH_MACOS_PACKAGED_VERSION_MAJOR_VALUE EPOCH_VERSION_MAJOR_VALUE
#endif

#if defined(EPOCH_OVERRIDE_MACOS_PACKAGED_VERSION_MINOR)
#  define EPOCH_MACOS_PACKAGED_VERSION_MINOR_VALUE EPOCH_OVERRIDE_MACOS_PACKAGED_VERSION_MINOR
#else
#  define EPOCH_MACOS_PACKAGED_VERSION_MINOR_VALUE EPOCH_VERSION_MINOR_VALUE
#endif

#if defined(EPOCH_OVERRIDE_MACOS_PACKAGED_VERSION_REVISION)
#  define EPOCH_MACOS_PACKAGED_VERSION_REVISION_VALUE EPOCH_OVERRIDE_MACOS_PACKAGED_VERSION_REVISION
#else
#  define EPOCH_MACOS_PACKAGED_VERSION_REVISION_VALUE EPOCH_VERSION_REVISION_VALUE
#endif

export module aengine.version;

import aengine.platform;

namespace epochnamespace
{
    export constexpr int major = EPOCH_VERSION_MAJOR_VALUE;
    export constexpr int minor = EPOCH_VERSION_MINOR_VALUE;
    export constexpr int revision = EPOCH_VERSION_REVISION_VALUE;
    export constexpr int windows_packaged_major = EPOCH_WINDOWS_PACKAGED_VERSION_MAJOR_VALUE;
    export constexpr int windows_packaged_minor = EPOCH_WINDOWS_PACKAGED_VERSION_MINOR_VALUE;
    export constexpr int windows_packaged_revision = EPOCH_WINDOWS_PACKAGED_VERSION_REVISION_VALUE;
    export constexpr int linux_packaged_major = EPOCH_LINUX_PACKAGED_VERSION_MAJOR_VALUE;
    export constexpr int linux_packaged_minor = EPOCH_LINUX_PACKAGED_VERSION_MINOR_VALUE;
    export constexpr int linux_packaged_revision = EPOCH_LINUX_PACKAGED_VERSION_REVISION_VALUE;
    export constexpr int macos_packaged_major = EPOCH_MACOS_PACKAGED_VERSION_MAJOR_VALUE;
    export constexpr int macos_packaged_minor = EPOCH_MACOS_PACKAGED_VERSION_MINOR_VALUE;
    export constexpr int macos_packaged_revision = EPOCH_MACOS_PACKAGED_VERSION_REVISION_VALUE;

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

    export constexpr int GetPackagedMajor() noexcept
    {
        switch (platform::current_platform())
        {
        case platform::RuntimePlatform::Windows:
            return windows_packaged_major;
        case platform::RuntimePlatform::Linux:
            return linux_packaged_major;
        case platform::RuntimePlatform::MacOS:
            return macos_packaged_major;
        default:
            return major;
        }
    }

    export constexpr int GetPackagedMinor() noexcept
    {
        switch (platform::current_platform())
        {
        case platform::RuntimePlatform::Windows:
            return windows_packaged_minor;
        case platform::RuntimePlatform::Linux:
            return linux_packaged_minor;
        case platform::RuntimePlatform::MacOS:
            return macos_packaged_minor;
        default:
            return minor;
        }
    }

    export constexpr int GetPackagedRevision() noexcept
    {
        switch (platform::current_platform())
        {
        case platform::RuntimePlatform::Windows:
            return windows_packaged_revision;
        case platform::RuntimePlatform::Linux:
            return linux_packaged_revision;
        case platform::RuntimePlatform::MacOS:
            return macos_packaged_revision;
        default:
            return revision;
        }
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

    export const char* GetPackagedVersion() noexcept
    {
        thread_local std::array<char, 32> buffer{};
        std::snprintf(
            buffer.data(),
            buffer.size(),
            "%d.%d.%d",
            GetPackagedMajor(),
            GetPackagedMinor(),
            GetPackagedRevision()
        );
        return buffer.data();
    }

    export std::string GetPackagedVersionString()
    {
        return std::string{ GetPackagedVersion() };
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
