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

#include <array>
#include <cstdio>
#include <cstdlib>
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
#  define EPOCH_VERSION_MINOR_VALUE 87
#endif

#if defined(EPOCH_OVERRIDE_VERSION_REVISION)
#  define EPOCH_VERSION_REVISION_VALUE EPOCH_OVERRIDE_VERSION_REVISION
#else
#  define EPOCH_VERSION_REVISION_VALUE 42
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
#  define EPOCH_WINDOWS_PACKAGED_VERSION_REVISION_VALUE 42
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
#  define EPOCH_LINUX_PACKAGED_VERSION_REVISION_VALUE 42
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
#  define EPOCH_MACOS_PACKAGED_VERSION_REVISION_VALUE 42
#endif

export module engine.version;

import engine.platform;

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
            "%d.%d.%02d",
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
            "%d.%d.%02d",
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

    export std::string GetRuntimePlatformDisplayString()
    {
#if defined(_WIN32)
        return "Windows";
#elif defined(__APPLE__)
        return "macOS";
#elif defined(__linux__)
        if (std::getenv("WSL_DISTRO_NAME") || std::getenv("WSL_INTEROP"))
            return "WSL Linux";
        return "Linux";
#else
        return "Unknown OS";
#endif
    }

    export constexpr std::string_view GetBuildCompilerString() noexcept
    {
#if defined(__clang__) && defined(_MSC_VER)
        return "clang-cl";
#elif defined(_MSC_VER)
        return "MSVC";
#elif defined(__clang__)
        return "Clang";
#elif defined(__GNUC__)
        return "GCC";
#else
        return "Unknown compiler";
#endif
    }

    export constexpr std::string_view GetBuildArchitectureString() noexcept
    {
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
        return "x64";
#elif defined(_M_ARM64) || defined(__aarch64__)
        return "arm64";
#elif defined(_M_IX86) || defined(__i386__)
        return "x86";
#else
        return "unknown-arch";
#endif
    }

    export constexpr std::string_view GetBuildConfigurationString() noexcept
    {
#if defined(NDEBUG)
        return "Release";
#else
        return "Debug";
#endif
    }

    export std::string GetEngineBuildTagString()
    {
        std::string result = GetRuntimePlatformDisplayString();
        result.push_back(' ');
        result += GetBuildArchitectureString();
        result.push_back(' ');
        result += GetBuildCompilerString();
        result.push_back(' ');
        result += GetBuildConfigurationString();
        return result;
    }

    export std::string GetEngineDisplayString()
    {
        std::string result{ kEngineName };
        result.push_back(' ');
        result += GetEngineVersion();
        result += " [";
        result += GetEngineBuildTagString();
        result += "]";
#if defined(EPOCH_UPDATER_SHELL_BUILD) && (EPOCH_UPDATER_SHELL_BUILD == 1)
        result += " (Updater Shell)";
#endif
        return result;
    }
}

export namespace epochengine = epochnamespace;
