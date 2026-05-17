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

#include <string>
#include <string_view>

// Keep platform / ABI macros in the global fragment.
// They must remain macros for ABI + build-system compatibility.

#ifdef _WIN32
#ifndef ENGINE_STATICLIB
#ifdef ENGINE_DLL_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
#else
#define ENGINE_API
#endif
#else
#if (__GNUC__ >= 4) && !defined(ENGINE_STATICLIB) && defined(ENGINE_DLL_EXPORTS)
#define ENGINE_API __attribute__((visibility("default")))
#else
#define ENGINE_API
#endif
#endif

#ifndef _STDCALL_SUPPORTED
#define ALECALLCONV __cdecl
#else
#define ALECALLCONV __stdcall
#endif

// -----------------------------------------------------------------------------
// Module declaration
// -----------------------------------------------------------------------------
export module engine.platform;

// -----------------------------------------------------------------------------
// Namespace selection
// -----------------------------------------------------------------------------
// You were previously redefining this via macro.
// For modules, make this explicit and stable.

export namespace epochnamespace
{
    // This namespace intentionally left minimal.
    // Platform-specific helpers live in other modules.
}

export namespace epochnamespace::platform
{
    enum class RuntimePlatform
    {
        Windows,
        Linux,
        MacOS,
        Unknown
    };

    constexpr RuntimePlatform current_platform() noexcept
    {
#if defined(_WIN32)
        return RuntimePlatform::Windows;
#elif defined(__APPLE__)
        return RuntimePlatform::MacOS;
#elif defined(__linux__)
        return RuntimePlatform::Linux;
#else
        return RuntimePlatform::Unknown;
#endif
    }

    constexpr bool is_windows() noexcept
    {
        return current_platform() == RuntimePlatform::Windows;
    }

    constexpr bool is_linux() noexcept
    {
        return current_platform() == RuntimePlatform::Linux;
    }

    constexpr bool is_macos() noexcept
    {
        return current_platform() == RuntimePlatform::MacOS;
    }

    constexpr std::string_view current_platform_key() noexcept
    {
        switch (current_platform())
        {
        case RuntimePlatform::Windows:
            return "windows";
        case RuntimePlatform::Linux:
            return "linux";
        case RuntimePlatform::MacOS:
            return "macos";
        default:
            return "unknown";
        }
    }

    enum class WindowTopology
    {
        SingleParent,
        StandaloneOnly
    };

    enum class SourceSnapshotArchive
    {
        Zip,
        TarGz
    };

    struct RuntimePolicy
    {
        RuntimePlatform platform = RuntimePlatform::Unknown;
        std::string_view platform_key = "unknown";
        WindowTopology window_topology = WindowTopology::StandaloneOnly;
        bool prefer_single_context_runtime = false;
        std::string_view updater_shell_backend = "software";
        SourceSnapshotArchive source_snapshot_archive = SourceSnapshotArchive::Zip;
    };

    [[nodiscard]] constexpr std::string_view source_snapshot_archive_extension_for(
        const SourceSnapshotArchive archive) noexcept
    {
        switch (archive)
        {
        case SourceSnapshotArchive::TarGz:
            return ".tar.gz";
        case SourceSnapshotArchive::Zip:
        default:
            return ".zip";
        }
    }

    [[nodiscard]] constexpr std::string_view source_snapshot_archive_label_for(
        const SourceSnapshotArchive archive) noexcept
    {
        switch (archive)
        {
        case SourceSnapshotArchive::TarGz:
            return "GitHub source snapshot tarball from main";
        case SourceSnapshotArchive::Zip:
        default:
            return "GitHub source snapshot zip archive from main";
        }
    }

    [[nodiscard]] constexpr RuntimePolicy current_runtime_policy() noexcept
    {
        switch (current_platform())
        {
        case RuntimePlatform::Windows:
            return {
                .platform = RuntimePlatform::Windows,
                .platform_key = "windows",
                .window_topology = WindowTopology::SingleParent,
                .prefer_single_context_runtime = false,
                .updater_shell_backend = "software",
                .source_snapshot_archive = SourceSnapshotArchive::Zip
            };
        case RuntimePlatform::Linux:
            return {
                .platform = RuntimePlatform::Linux,
                .platform_key = "linux",
                .window_topology = WindowTopology::StandaloneOnly,
                .prefer_single_context_runtime = true,
                .updater_shell_backend = "opengl",
                .source_snapshot_archive = SourceSnapshotArchive::TarGz
            };
        case RuntimePlatform::MacOS:
            return {
                .platform = RuntimePlatform::MacOS,
                .platform_key = "macos",
                .window_topology = WindowTopology::StandaloneOnly,
                .prefer_single_context_runtime = true,
                .updater_shell_backend = "software",
                .source_snapshot_archive = SourceSnapshotArchive::TarGz
            };
        case RuntimePlatform::Unknown:
        default:
            return {};
        }
    }

    [[nodiscard]] constexpr bool supports_parented_multiwindow() noexcept
    {
        return current_runtime_policy().window_topology == WindowTopology::SingleParent;
    }

    [[nodiscard]] constexpr bool default_parented_multiwindow() noexcept
    {
        return supports_parented_multiwindow();
    }

    [[nodiscard]] constexpr bool prefer_single_context_runtime() noexcept
    {
        return current_runtime_policy().prefer_single_context_runtime;
    }

    [[nodiscard]] constexpr std::string_view updater_shell_backend_name() noexcept
    {
        return current_runtime_policy().updater_shell_backend;
    }

    [[nodiscard]] constexpr std::string_view source_snapshot_archive_extension() noexcept
    {
        return source_snapshot_archive_extension_for(current_runtime_policy().source_snapshot_archive);
    }

    [[nodiscard]] constexpr std::string_view source_snapshot_archive_label() noexcept
    {
        return source_snapshot_archive_label_for(current_runtime_policy().source_snapshot_archive);
    }

#if !defined(__linux__)
    inline bool pump_events() { return true; }
#endif
}

export namespace epoch::platform::policy
{
    using RuntimePlatform = epochnamespace::platform::RuntimePlatform;
    using WindowTopology = epochnamespace::platform::WindowTopology;
    using SourceSnapshotArchive = epochnamespace::platform::SourceSnapshotArchive;
    using RuntimePolicy = epochnamespace::platform::RuntimePolicy;

    [[nodiscard]] constexpr RuntimePolicy current_runtime_policy() noexcept
    {
        return epochnamespace::platform::current_runtime_policy();
    }

    [[nodiscard]] constexpr bool supports_parented_multiwindow() noexcept
    {
        return epochnamespace::platform::supports_parented_multiwindow();
    }

    [[nodiscard]] constexpr bool default_parented_multiwindow() noexcept
    {
        return epochnamespace::platform::default_parented_multiwindow();
    }

    [[nodiscard]] constexpr bool prefer_single_context_runtime() noexcept
    {
        return epochnamespace::platform::prefer_single_context_runtime();
    }

    [[nodiscard]] constexpr std::string_view updater_shell_backend_name() noexcept
    {
        return epochnamespace::platform::updater_shell_backend_name();
    }

    [[nodiscard]] constexpr std::string_view source_snapshot_archive_extension() noexcept
    {
        return epochnamespace::platform::source_snapshot_archive_extension();
    }

    [[nodiscard]] constexpr std::string_view source_snapshot_archive_label() noexcept
    {
        return epochnamespace::platform::source_snapshot_archive_label();
    }
}
