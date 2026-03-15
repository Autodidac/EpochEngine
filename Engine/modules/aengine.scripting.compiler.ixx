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

export module aengine.scripting.compiler;

import <cstdlib>;
import <filesystem>;
import <iostream>;
import <string>;
import <vector>;

export namespace epochnamespace::compiler 
{
    namespace detail
    {
        [[nodiscard]] inline std::string quote_arg(std::string value)
        {
            std::string escaped;
            escaped.reserve(value.size() + 2);
            escaped.push_back('"');
            for (const char ch : value)
            {
                if (ch == '"')
                    escaped.push_back('\\');
                escaped.push_back(ch);
            }
            escaped.push_back('"');
            return escaped;
        }

        [[nodiscard]] inline std::filesystem::path resolve_clangxx()
        {
            const std::vector<std::filesystem::path> candidates{
                std::filesystem::path{ "C:/Program Files/LLVM/bin/clang++.exe" },
                std::filesystem::path{ "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang++.exe" },
                std::filesystem::path{ "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin/clang++.exe" }
            };

            for (const auto& candidate : candidates)
            {
                if (std::filesystem::exists(candidate))
                    return candidate;
            }

            return std::filesystem::path{ "clang++" };
        }

        [[nodiscard]] inline std::filesystem::path resolve_engine_include_root(
            const std::filesystem::path& input) noexcept
        {
            std::error_code ec;
            std::vector<std::filesystem::path> candidates{
                input.parent_path().parent_path().parent_path() / "include",
                std::filesystem::current_path(ec) / "Engine" / "include",
                std::filesystem::current_path(ec) / "include"
            };

            for (const auto& candidate : candidates)
            {
                if (!candidate.empty() && std::filesystem::exists(candidate))
                    return std::filesystem::absolute(candidate, ec).lexically_normal();
            }

            return (input.parent_path().parent_path().parent_path() / "include").lexically_normal();
        }
    }

    export bool compile_script_to_dll(const std::filesystem::path& input, const std::filesystem::path& output) {
        const auto compilerPath = detail::resolve_clangxx();
        const auto includeRoot = detail::resolve_engine_include_root(input);
        std::vector<std::string> clangArgs = {
            detail::quote_arg(compilerPath.string()),
            "-std=c++20",
            "-shared",
            detail::quote_arg(input.string()),
            "-o", detail::quote_arg(output.string()),
            "-I" + detail::quote_arg(includeRoot.string()),
            "-fno-rtti",
            "-fno-exceptions",
            "-O2"
        };

        std::string cmd;
        for (const auto& arg : clangArgs) {
            cmd += arg + " ";
        }

        std::cout << "[compiler] running: " << cmd << std::endl;
        int result = std::system(cmd.c_str());
        if (result != 0) {
            std::cerr << "[compiler] clang++ failed with code: " << result << std::endl;
            return false;
        }

        return true;
    }

}
