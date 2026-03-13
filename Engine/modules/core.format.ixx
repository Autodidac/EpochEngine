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
 // ============================================================================
// modules/core.format.ixx
// Tiny formatting helper around std::format / std::vformat.
// ============================================================================
module;

#include "../include/_epoch.stl_types.hpp"
#include <format>

export module core.format;

export namespace epoch::core::format
{
    // Backend: takes pre-built format_args.
    [[nodiscard]] inline epoch::string vstr(epoch::string_view fmt, std::format_args args)
    {
        return epoch::string{ std::vformat(epoch::to_std(fmt), args) };
    }

    // Convenience: build args safely (lvalues) then call vstr.
    template <class... Args>
    [[nodiscard]] inline epoch::string str(epoch::string_view fmt, const Args&... args)
    {
        return vstr(fmt, std::make_format_args(args...));
    }
}
