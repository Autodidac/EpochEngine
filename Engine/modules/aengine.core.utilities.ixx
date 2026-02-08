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
module;

#ifdef _WIN32
// MSVC doesn't always predefine the architecture macros inside module
// interface units, which makes <winnt.h> bail out with "No Target
// Architecture". Mirror the mappings that <windows.h> normally performs so
// Windows headers have the information they expect.
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
#else
#include <unistd.h>
#endif

export module aengine.core.utilities;

import <cstdio>;
import <iostream>;
import <type_traits>;
import <utility>;

export namespace almondnamespace::utilities
{
#ifdef _WIN32
    [[nodiscard]] inline bool isConsoleApplication() noexcept
    {
        return GetConsoleWindow() != nullptr;
    }
#else
    [[nodiscard]] inline bool isConsoleApplication() noexcept
    {
        return isatty(fileno(stdout)) != 0;
    }
#endif

#define NO_RETURN_RETRY_ONCE(call)                  \
    do {                                            \
        try {                                       \
            call;                                   \
        } catch (...) {                             \
            std::cerr << "[Retry] Retrying: " #call "\n"; \
            call;                                   \
        }                                           \
    } while (0)

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
