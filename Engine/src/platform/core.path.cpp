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
#include <cstdlib>

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

namespace epochengine::core::path
{
    namespace
    {
        [[nodiscard]] bool exists_noerr(const path& p) noexcept
        {
            std::error_code ec;
            return std::filesystem::exists(p, ec) && !ec;
        }

        [[nodiscard]] path env_override_path(const char* key)
        {
#if defined(_WIN32)
            char* envBuf = nullptr;
            std::size_t len = 0;
            if (_dupenv_s(&envBuf, &len, key) == 0 && envBuf && *envBuf)
            {
                const path envPath{ envBuf };
                std::free(envBuf);
                return normalize(envPath);
            }
            std::free(envBuf);
            return {};
#else
            if (const char* envValue = std::getenv(key); envValue && *envValue)
                return normalize(path{ envValue });
            return {};
#endif
        }

        [[nodiscard]] path resolve_runtime_root_from(const path& start)
        {
            std::error_code ec;
            path probe = std::filesystem::absolute(start, ec).lexically_normal();
            if (ec)
                return {};

            if (std::filesystem::is_regular_file(probe, ec))
                probe = probe.parent_path();

            while (!probe.empty())
            {
                if (exists_noerr(probe / "assets"))
                    return probe;

                const path parent = probe.parent_path();
                if (parent == probe)
                    break;
                probe = parent;
            }

            return {};
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
            && exists_noerr(root / "Engine" / "include" / "epoch.engine.hpp")
            && exists_noerr(root / "Engine" / "src" / "epoch.main.cpp")
            && exists_noerr(root / "Engine" / "examples" / "StaticLib1" / "StaticLib1.vcxproj");
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

    path runtime_root_dir()
    {
        if (const path overridePath = env_override_path("EPOCH_RUNTIME_ROOT"); !overridePath.empty())
            return overridePath;

        if (const path repoRoot = find_epoch_repo_root(executable_path()); !repoRoot.empty())
            return repoRoot;

        if (const path exeDir = executable_dir(); !exeDir.empty())
        {
            if (const path runtimeRoot = resolve_runtime_root_from(exeDir); !runtimeRoot.empty())
                return runtimeRoot;

            return normalize(exeDir);
        }

        return {};
    }

    path engine_asset_dir()
    {
        if (const path overridePath = env_override_path("EPOCH_ASSET_ROOT"); !overridePath.empty())
        {
            if (exists_noerr(overridePath / "Engine" / "assets"))
                return normalize(overridePath / "Engine" / "assets");
            if (exists_noerr(overridePath / "assets"))
                return normalize(overridePath / "assets");
            if (exists_noerr(overridePath))
                return overridePath;
        }

        if (const path repoRoot = runtime_root_dir(); is_epoch_repo_root(repoRoot))
            return normalize(repoRoot / "Engine" / "assets");

        if (const path runtimeRoot = runtime_root_dir(); !runtimeRoot.empty())
            return normalize(runtimeRoot / "assets");

        return {};
    }

    path example_asset_dir()
    {
        if (const path overridePath = env_override_path("EPOCH_EXAMPLE_ASSET_ROOT"); !overridePath.empty())
            return overridePath;

        if (const path assetOverride = env_override_path("EPOCH_ASSET_ROOT"); !assetOverride.empty())
        {
            if (exists_noerr(assetOverride / "Engine" / "examples" / "ConsoleApplication1" / "assets"))
                return normalize(assetOverride / "Engine" / "examples" / "ConsoleApplication1" / "assets");
            if (exists_noerr(assetOverride / "examples" / "ConsoleApplication1" / "assets"))
                return normalize(assetOverride / "examples" / "ConsoleApplication1" / "assets");
            if (exists_noerr(assetOverride / "assets"))
                return normalize(assetOverride / "assets");
            if (exists_noerr(assetOverride))
                return assetOverride;
        }

        if (const path repoRoot = runtime_root_dir(); is_epoch_repo_root(repoRoot))
            return normalize(repoRoot / "Engine" / "examples" / "ConsoleApplication1" / "assets");

        if (const path runtimeRoot = runtime_root_dir(); !runtimeRoot.empty())
            return normalize(runtimeRoot / "assets");

        return {};
    }

    path log_output_dir()
    {
        if (const path overridePath = env_override_path("EPOCH_LOG_DIR"); !overridePath.empty())
            return overridePath;

        if (const path runtimeRoot = runtime_root_dir(); !runtimeRoot.empty())
            return normalize(runtimeRoot / "logs");

        if (const path exeDir = executable_dir(); !exeDir.empty())
            return normalize(exeDir / "logs");

        return {};
    }

    path capture_output_dir()
    {
        if (const path overridePath = env_override_path("EPOCH_CAPTURE_DIR"); !overridePath.empty())
            return overridePath;

        if (const path logRoot = log_output_dir(); !logRoot.empty())
            return normalize(logRoot / "captures");

        return {};
    }

    path engine_include_dir()
    {
        if (const path repoRoot = runtime_root_dir(); is_epoch_repo_root(repoRoot))
            return normalize(repoRoot / "Engine" / "include");

        if (const path runtimeRoot = runtime_root_dir(); !runtimeRoot.empty() && exists_noerr(runtimeRoot / "include"))
            return normalize(runtimeRoot / "include");

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
