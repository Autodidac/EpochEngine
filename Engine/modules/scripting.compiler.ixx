/************************************************
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•—  â–ˆâ–ˆâ•—   *
 *  â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ•”â•â•â•  â–ˆâ–ˆâ•”â•â•â•â• â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘     â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â•šâ•â•â•â•â•â•â•â•šâ•â•      â•šâ•â•â•â•â•â•  â•šâ•â•â•â•â•â•â•šâ•â•  â•šâ•â•   *
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

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <source_location>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#include <errno.h>
#endif

export module scripting.compiler;

import core.logger;

namespace epochnamespace::compiler 
{
    namespace detail
    {
#ifdef _WIN32
        [[nodiscard]] inline std::wstring narrow_lossy(const std::wstring& value)
        {
            return std::wstring(value.begin(), value.end());
        }

        [[nodiscard]] inline std::string errno_message(int code)
        {
            char buffer[128]{};
            if (strerror_s(buffer, sizeof(buffer), code) != 0)
                return "unknown error";
            return std::string(buffer);
        }
#endif

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

        [[nodiscard]] inline std::string describe_command(
            const std::filesystem::path& compilerPath,
            const std::filesystem::path& input,
            const std::filesystem::path& output,
            const std::filesystem::path& includeRoot)
        {
            std::vector<std::string> clangArgs = {
                quote_arg(compilerPath.string()),
                "-std=c++20",
                "-shared",
                quote_arg(input.string()),
                "-o", quote_arg(output.string()),
                "-I" + quote_arg(includeRoot.string()),
                "-fno-rtti",
                "-fno-exceptions",
                "-O2"
            };

            std::string cmd;
            for (const auto& arg : clangArgs)
                cmd += arg + " ";
            return cmd;
        }

#ifdef _WIN32
        [[nodiscard]] inline bool spawn_compiler(
            const std::filesystem::path& compilerPath,
            const std::filesystem::path& input,
            const std::filesystem::path& output,
            const std::filesystem::path& includeRoot)
        {
            std::vector<std::wstring> args{
                compilerPath.wstring(),
                L"-std=c++20",
                L"-shared",
                input.wstring(),
                L"-o",
                output.wstring(),
                L"-I" + includeRoot.wstring(),
                L"-fno-rtti",
                L"-fno-exceptions",
                L"-O2"
            };

            std::vector<const wchar_t*> argv;
            argv.reserve(args.size() + 1);
            for (const auto& arg : args)
                argv.push_back(arg.c_str());
            argv.push_back(nullptr);

            const bool useDirectPath =
                compilerPath.has_parent_path() && std::filesystem::exists(compilerPath);

            errno = 0;
            const intptr_t result = useDirectPath
                ? _wspawnv(_P_WAIT, compilerPath.c_str(), argv.data())
                : _wspawnvp(_P_WAIT, compilerPath.c_str(), argv.data());
            if (result == -1)
            {
                logger::errorf_loc(
                    "Compiler",
                    std::source_location::current(),
                    "failed to launch clang++: {} (errno={}: {})",
                    compilerPath.string(),
                    errno,
                    errno_message(errno));
                return false;
            }

            if (result != 0)
            {
                logger::errorf_loc("Compiler", std::source_location::current(), "clang++ failed with code: {}", result);
                return false;
            }

            return true;
        }
#else
        [[nodiscard]] inline bool spawn_compiler(
            const std::filesystem::path& compilerPath,
            const std::filesystem::path& input,
            const std::filesystem::path& output,
            const std::filesystem::path& includeRoot)
        {
            const std::string cmd = describe_command(compilerPath, input, output, includeRoot);
            const int result = std::system(cmd.c_str());
            if (result != 0)
            {
                logger::errorf_loc("Compiler", std::source_location::current(), "clang++ failed with code: {}", result);
                return false;
            }
            return true;
        }
#endif
    }

    export bool compile_script_to_dll(const std::filesystem::path& input, const std::filesystem::path& output) {
        const auto compilerPath = detail::resolve_clangxx();
        const auto includeRoot = detail::resolve_engine_include_root(input);
        logger::infof_loc(
            "Compiler",
            std::source_location::current(),
            "running: {}",
            detail::describe_command(compilerPath, input, output, includeRoot));
        return detail::spawn_compiler(compilerPath, input, output, includeRoot);
    }

}
