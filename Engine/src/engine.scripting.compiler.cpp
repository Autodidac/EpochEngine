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
// Prefer standard library module imports in the module purview.
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

import core.log;

namespace epochengine::compiler
{
    // Builds a single TU into a shared library (DLL/.so).
    // Returns true on success (exit code == 0).
    inline bool compile_script_to_dll(
        const std::filesystem::path& input,
        const std::filesystem::path& output
    )
    {
        std::vector<std::string> args;
        args.reserve(32);

#if defined(_WIN32)
        // If you're using MSVC-style link, you likely want clang-cl instead.
        // But keep clang++ since that's what your code already used.
        args.emplace_back("clang++");
#else
        args.emplace_back("clang++");
        args.emplace_back("-fPIC");
#endif

        args.emplace_back("-std=c++23");
        args.emplace_back("-shared");

        args.emplace_back(input.string());

        args.emplace_back("-o");
        args.emplace_back(output.string());

        // Legacy CMake fallback: prefer project-local scripts, then engine include roots.
        if (!input.parent_path().empty())
            args.emplace_back("-I" + input.parent_path().string());
        args.emplace_back("-Iinclude");
        args.emplace_back("-IEngine/include");

        // Match your engine defaults (no RTTI/exceptions) as requested.
        args.emplace_back("-fno-rtti");
        args.emplace_back("-fno-exceptions");

        args.emplace_back("-O2");

        // Build command line (simple quoting for spaces).
        std::string cmd;
        cmd.reserve(4096);

        for (const auto& a : args)
        {
            const bool needs_quotes = (a.find(' ') != std::string::npos) || (a.find('\t') != std::string::npos);
            if (needs_quotes)
            {
                cmd.push_back('"');
                cmd += a;
                cmd.push_back('"');
            }
            else
            {
                cmd += a;
            }
            cmd.push_back(' ');
        }

        epochengine::core::log::core_log_write(
            static_cast<std::uint32_t>(epochengine::core::log::level::info),
            "Compiler",
            cmd.c_str());

        const int result = std::system(cmd.c_str());
        if (result != 0)
        {
            const std::string errorMessage = "[compiler] clang failed with code: " + std::to_string(result);
            epochengine::core::log::core_log_write(
                static_cast<std::uint32_t>(epochengine::core::log::level::error),
                "Compiler",
                errorMessage.c_str());
            return false;
        }

        return true;
    }
} // namespace epochengine::compiler
