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
#pragma once

#include "aplatform.hpp"

#if defined(__cpp_modules) && __cpp_modules >= 201907L && !defined(ALMOND_FORCE_LEGACY_HEADERS)
import almond.core.utilities;
#else
#include <iostream>
#include <type_traits>
#include <utility>

namespace almondnamespace::utilities
{
#ifdef _WIN32 // Windows-specific

// MSVC may skip defining architecture macros when compiling module
// interfaces or when certain switches are used. Recreate the mappings that
// <windows.h> usually sets up so Windows headers don't abort with "No Target
// Architecture".
#if !defined(_X86_) && !defined(_AMD64_) && !defined(_ARM_) && !defined(_ARM64_)
#if defined(_M_IX86)
#define _X86_
#elif defined(_M_AMD64) || defined(_M_X64)
#define _AMD64_
#elif defined(_M_ARM)
#define _ARM_
#elif defined(_M_ARM64)
#define _ARM64_
#endif
#endif

#include <consoleapi3.h>
#include "aframework.hpp"

    [[nodiscard]] inline constexpr bool isConsoleApplication() noexcept
    {
        return GetConsoleWindow() != nullptr;
    }

#else // Unix-like

#include <unistd.h>
#include <stdio.h>

    [[nodiscard]] inline constexpr bool isConsoleApplication() noexcept
    {
        return isatty(fileno(stdout)) != 0;
    }

#endif

#ifndef NO_RETURN_RETRY_ONCE
#define NO_RETURN_RETRY_ONCE(call)                  \
    do {                                            \
        try {                                       \
            call;                                   \
        } catch (...) {                             \
            std::cerr << "[Retry] Retrying: " #call "\n"; \
            call;                                   \
        }                                           \
    } while (0)
#endif

    template<typename Func>
    auto make_retry_once(Func&& f)
    {
        return [f = std::forward<Func>(f)](auto&&... args) -> decltype(auto)
            {
                try
                {
                    return f(std::forward<decltype(args)>(args)...);
                }
                catch (...)
                {
                    std::cerr << "[Retry] First attempt failed, retrying once...\n";
                    return f(std::forward<decltype(args)>(args)...);
                }
            };
    }
} // namespace almondnamespace::utilities
#endif

#ifndef NO_RETURN_RETRY_ONCE
#define NO_RETURN_RETRY_ONCE(call)                  \
    do {                                            \
        try {                                       \
            call;                                   \
        } catch (...) {                             \
            std::cerr << "[Retry] Retrying: " #call "\n"; \
            call;                                   \
        }                                           \
    } while (0)
#endif
