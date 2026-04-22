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

#include <filesystem>

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#  include <vector>
#else
#  include <unistd.h>
#  include <vector>
#endif

module core.path;

namespace epoch::core::path
{
    namespace
    {
        [[nodiscard]] bool exists_noerr(const path& p) noexcept
        {
            std::error_code ec;
            return std::filesystem::exists(p, ec) && !ec;
        }
    }

    path executable_path()
    {
#if defined(_WIN32)
        std::wstring buf;
        buf.resize(32768);
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (n == 0) return {};
        buf.resize((size_t)n);
        return path{buf};
#elif defined(__APPLE__)
        std::uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::vector<char> tmp(size + 1u, '\0');
        if (_NSGetExecutablePath(tmp.data(), &size) != 0) return {};
        return path{tmp.data()};
#else
        std::vector<char> tmp(4096, '\0');
        for (;;)
        {
            const ssize_t n = ::readlink("/proc/self/exe", tmp.data(), tmp.size() - 1);
            if (n < 0) return {};
            if ((size_t)n < tmp.size() - 1)
            {
                tmp[(size_t)n] = '\0';
                return path{tmp.data()};
            }
            tmp.resize(tmp.size() * 2);
        }
#endif
    }

    path executable_dir()
    {
        const auto p = executable_path();
        if (p.empty()) return {};
        return p.parent_path();
    }

    bool is_epoch_repo_root(const path& candidate)
    {
        const path root = candidate.lexically_normal();
        return exists_noerr(root / "Engine" / "CMakeLists.txt")
            && exists_noerr(root / "Engine" / "include" / "aengine.hpp")
            && exists_noerr(root / "Engine" / "examples" / "ConsoleApplication1" / "main.cpp");
    }

    path find_epoch_repo_root(const path& start)
    {
        if (start.empty())
            return {};

        std::error_code ec;
        path probe = std::filesystem::absolute(start, ec).lexically_normal();
        if (ec)
            return {};

        if (std::filesystem::is_regular_file(probe, ec))
            probe = probe.parent_path();

        while (!probe.empty())
        {
            if (is_epoch_repo_root(probe))
                return probe;

            const path parent = probe.parent_path();
            if (parent == probe)
                break;
            probe = parent;
        }

        return {};
    }

    path example_console_workspace_dir()
    {
        if (const path repoRoot = find_epoch_repo_root(executable_path()); !repoRoot.empty())
            return normalize(repoRoot / "Engine" / "examples" / "ConsoleApplication1" / "workspace");

        if (const path exeDir = executable_dir(); !exeDir.empty())
            return normalize(exeDir / "workspace");

        return {};
    }

    path normalize(const path& p)
    {
        return p.lexically_normal();
    }

    path join(const path& p, const path& child)
    {
        return normalize(p / child);
    }
}
