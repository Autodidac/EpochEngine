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
export module aengine.platform;

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

#if !defined(__linux__)
    inline bool pump_events() { return true; }
#endif
}
