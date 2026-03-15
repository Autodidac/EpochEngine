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

export module aengine.version;

import <array>;
import <cstdio>;
import <string>;
import <string_view>;

export namespace epochnamespace
{
    export constexpr int major = 0;
    export constexpr int minor = 82;
    export constexpr int revision = 13;

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
}

export namespace epochengine = epochnamespace;
