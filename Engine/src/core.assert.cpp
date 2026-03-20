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

#include "../include/_epoch.stl_types.hpp"

#if defined(_WIN32)
#  include <windows.h>
#endif

module core.assert;

import core.log;

namespace epoch::core::asserts
{
    namespace
    {
#if defined(NDEBUG)
        inline constexpr bool kDebugEnabled = false;
#else
        inline constexpr bool kDebugEnabled = true;
#endif

        [[noreturn]] void fail(epoch::string_view kind,
            epoch::string_view message,
            std::source_location where)
        {
            const std::string_view k = epoch::to_std(kind);
            const std::string_view m = epoch::to_std(message);

            std::string line;
            line.reserve(256);

            line.append("[");
            line.append(k);
            line.append("] ");
            line.append(m.empty() ? std::string_view{ "<no message>" } : m);
            line.append(" @ ");
            line.append(where.file_name());
            line.push_back(':');
            line.append(std::to_string(where.line()));
            line.append(" (");
            line.append(where.function_name());
            line.push_back(')');

            // If core.log expects epoch::string_view, feed it epoch.
            epoch::core::log::error(epoch::to_view("assert"),
                epoch::to_view(std::string_view{ line }));

#if defined(_WIN32)
            // Only trigger a breakpoint when a debugger is actually attached.
            // Otherwise Windows may surface an interactive JIT/debug dialog,
            // which is the opposite of what we want for unattended updater runs.
            if (::IsDebuggerPresent() != FALSE)
                ::DebugBreak();
#endif
            std::terminate();
        }
    }

    void that(bool condition, epoch::string_view message, std::source_location where)
    {
        if (!condition)
            fail(epoch::to_view("assert"), message, where);
    }

    void debug(bool condition, epoch::string_view message, std::source_location where)
    {
        if constexpr (kDebugEnabled)
        {
            if (!condition)
                fail(epoch::to_view("debug_assert"), message, where);
        }
        else
        {
            (void)condition; (void)message; (void)where;
        }
    }
}
