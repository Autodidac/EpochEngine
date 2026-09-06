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
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#  include <windows.h>
#  include <shellapi.h>
#else
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#if defined(__APPLE__)
#  include <mach-o/dyld.h>
#endif
#endif

module core.path;

namespace epochengine::core::path
{
    namespace
    {
        constexpr std::size_t maximum_candidate_depth = 128u;
        constexpr std::size_t maximum_candidate_path = 32760u;

        struct CandidateRoot final
        {
            std::mutex mutex{};
            path root{};
#if defined(_WIN32)
            std::vector<HANDLE> directories{};
#else
            std::vector<int> directories{};
#endif

            ~CandidateRoot()
            {
                for (const auto directory : directories)
#if defined(_WIN32)
                    CloseHandle(directory);
#else
                    ::close(directory);
#endif
            }
        };

        CandidateRoot& candidate_state()
        {
            static CandidateRoot state;
            return state;
        }

        // Keep every admitted ancestor pinned while this process uses the
        // binding. This does not constrain arbitrary candidate code or writes
        // through other APIs, and Unix directory handles do not prevent rename.
        bool validate_candidate_root(const path& root, CandidateRoot& pending)
        {
            if (root.empty() || !root.is_absolute() || root == root.root_path()
                || root.filename().empty() || root.native().size() > maximum_candidate_path
                || root.native().find(path::value_type{}) != path::string_type::npos)
                return false;

            const auto native = root.native();
#if defined(_WIN32)
            const auto drive = root.root_name().native();
            if (drive.size() != 2u || drive[1] != L':'
                || !((drive[0] >= L'A' && drive[0] <= L'Z')
                    || (drive[0] >= L'a' && drive[0] <= L'z')))
                return false;
            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, native.data(),
                    static_cast<int>(native.size()), nullptr, 0, nullptr, nullptr) == 0)
                return false;
            const auto separator = [](wchar_t ch) { return ch == L'\\' || ch == L'/'; };
#else
            const auto separator = [](char ch) { return ch == '/'; };
#endif
            for (std::size_t index = 1u; index < native.size(); ++index)
                if (separator(native[index - 1u]) && separator(native[index]))
                    return false;

            std::size_t depth{};
            for (const auto& component : root.relative_path())
            {
                if (++depth > maximum_candidate_depth || component.empty()
                    || component == "." || component == "..")
                    return false;
#if defined(_WIN32)
                const auto name = component.native();
                if (name.back() == L'.' || name.back() == L' '
                    || name.find_first_of(L":*?\"<>|") != std::wstring::npos)
                    return false;
                for (const auto ch : name)
                    if (ch < 32)
                        return false;
                auto stem = name.substr(0u, name.find(L'.'));
                for (auto& ch : stem)
                    if (ch >= L'a' && ch <= L'z')
                        ch = static_cast<wchar_t>(ch - L'a' + L'A');
                if (stem == L"CON" || stem == L"PRN" || stem == L"AUX"
                    || stem == L"NUL" || stem == L"CONIN$" || stem == L"CONOUT$"
                    || (stem.size() == 4u
                        && (stem.starts_with(L"COM") || stem.starts_with(L"LPT"))
                        && ((stem[3] >= L'1' && stem[3] <= L'9')
                            || stem[3] == L'\u00b9' || stem[3] == L'\u00b2'
                            || stem[3] == L'\u00b3')))
                    return false;
#endif
            }
            if (depth == 0u)
                return false;

            pending.directories.reserve(depth + 1u);
            path current = root.root_path();
#if defined(_WIN32)
            const auto pin = [&](const path& directory)
            {
                auto extended = directory;
                extended.make_preferred();
                const auto name = L"\\\\?\\" + extended.native();
                const HANDLE handle = CreateFileW(name.c_str(), FILE_READ_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
                if (handle == INVALID_HANDLE_VALUE)
                    return false;
                pending.directories.push_back(handle);
                FILE_ATTRIBUTE_TAG_INFO attributes{};
                if (!GetFileInformationByHandleEx(handle, FileAttributeTagInfo,
                        &attributes, sizeof(attributes))
                    || !(attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    || (attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                    return false;
                std::wstring final_name(maximum_candidate_path + 4u, L'\0');
                const DWORD length = GetFinalPathNameByHandleW(handle, final_name.data(),
                    static_cast<DWORD>(final_name.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
                if (length == 0u || length >= final_name.size())
                    return false;
                final_name.resize(length);
                // Reject DOS short-name or other namespace aliases, not just
                // symlinks. Case differences are ordinary on Windows.
                return CompareStringOrdinal(name.data(), static_cast<int>(name.size()),
                    final_name.data(), static_cast<int>(final_name.size()), TRUE) == CSTR_EQUAL;
            };
            if (!pin(current))
                return false;
            for (const auto& component : root.relative_path())
            {
                current /= component;
                if (!pin(current))
                    return false;
            }
#else
            const int first = ::open(current.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (first < 0)
                return false;
            pending.directories.push_back(first);
            for (const auto& component : root.relative_path())
            {
                const int descriptor = ::openat(pending.directories.back(), component.c_str(),
                    O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
                if (descriptor < 0)
                    return false;
                pending.directories.push_back(descriptor);
                struct stat status{};
                if (::fstat(descriptor, &status) != 0 || !S_ISDIR(status.st_mode))
                    return false;
            }
#endif
            pending.root = root;
            pending.root.make_preferred();
            return true;
        }

        template<class Character>
        CandidateDataArguments scan_candidate_arguments(
            int argc, Character** argv, std::basic_string_view<Character> option)
        {
            int found{};
            for (int index = 1; index < argc; ++index)
            {
                if (!argv[index])
                    return { false, 0 };
                const std::basic_string_view<Character> argument{ argv[index] };
                if (!argument.starts_with(option))
                    continue;
                if (argument != option || found != 0 || index + 1 >= argc
                    || !argv[index + 1] || argv[index + 1][0] == Character{})
                    return { false, 0 };
                found = index;
                ++index;
            }
            if (found == 0)
                return { true, 0 };
            return { configure_candidate_data_root(path{ argv[found + 1] }), found };
        }

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

    path candidate_data_root()
    {
        auto& state = candidate_state();
        const std::lock_guard lock{ state.mutex };
        return state.root;
    }

    bool configure_candidate_data_root(const path& root) noexcept
    {
        try
        {
            auto& state = candidate_state();
            const std::lock_guard lock{ state.mutex };
            if (!state.root.empty())
                return false;
            CandidateRoot pending;
            if (!validate_candidate_root(root, pending))
                return false;
            state.root.swap(pending.root);
            state.directories.swap(pending.directories);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    CandidateDataArguments preflight_candidate_data_arguments(int argc, char** argv) noexcept
    {
        try
        {
            if (argc < 1 || argc > 4096 || !argv || !argv[0])
                return { false, 0 };
            for (int index = 1; index < argc; ++index)
                if (!argv[index])
                    return { false, 0 };
#if defined(_WIN32)
            bool requested{};
            for (int index = 1; index < argc; ++index)
                requested = requested || std::string_view{argv[index]}.starts_with("--candidate-data-root");
            // Ordinary command lines keep CRT semantics, including quoting
            // forms whose token count differs from CommandLineToArgvW.
            if (!requested) return {true, 0};
            struct NativeArguments final
            {
                wchar_t** value{};
                ~NativeArguments() { if (value) LocalFree(value); }
            };
            int native_count{};
            const NativeArguments native{ CommandLineToArgvW(GetCommandLineW(), &native_count) };
            if (!native.value || native_count != argc)
                return { false, 0 };
            return scan_candidate_arguments(argc, native.value,
                std::wstring_view{ L"--candidate-data-root" });
#else
            return scan_candidate_arguments(argc, argv,
                std::string_view{ "--candidate-data-root" });
#endif
        }
        catch (...)
        {
            return { false, 0 };
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
            && exists_noerr(root / "Engine" / "examples" / "EpochEngine" / "EpochEngine.vcxproj");
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
        if (const auto candidate = candidate_data_root(); !candidate.empty())
            return candidate / "workspace";

        return example_console_workspace_dir(executable_path());
    }

    path example_console_workspace_dir(const path& host_executable)
    {
        if (host_executable.empty() || !host_executable.is_absolute())
            return {};

        // GetModuleFileNameW preserves the operator's launch spelling, which
        // may pass through a renamed-checkout junction. Resolve only that
        // trusted host identity, before any sandbox directory is created or
        // any candidate path is admitted. Candidate/output redirects still
        // belong to their strict no-redirect checks, not this host resolver.
        std::error_code error;
        const path executable = std::filesystem::canonical(host_executable, error);
        if (error || !std::filesystem::is_regular_file(executable, error) || error)
            return {};

        if (const path repoRoot = find_epoch_repo_root(executable); !repoRoot.empty())
            return normalize(repoRoot / "Engine" / "examples" / "EpochEditor" / "workspace");

        return normalize(executable.parent_path() / "workspace");
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
            if (exists_noerr(assetOverride / "Engine" / "examples" / "EpochEditor" / "assets"))
                return normalize(assetOverride / "Engine" / "examples" / "EpochEditor" / "assets");
            if (exists_noerr(assetOverride / "examples" / "EpochEditor" / "assets"))
                return normalize(assetOverride / "examples" / "EpochEditor" / "assets");
            if (exists_noerr(assetOverride / "assets"))
                return normalize(assetOverride / "assets");
            if (exists_noerr(assetOverride))
                return assetOverride;
        }

        if (const path repoRoot = runtime_root_dir(); is_epoch_repo_root(repoRoot))
            return normalize(repoRoot / "Engine" / "examples" / "EpochEditor" / "assets");

        if (const path runtimeRoot = runtime_root_dir(); !runtimeRoot.empty())
            return normalize(runtimeRoot / "assets");

        return {};
    }

    path log_output_dir()
    {
        if (const auto candidate = candidate_data_root(); !candidate.empty())
            return candidate / "logs";

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
        if (const auto candidate = candidate_data_root(); !candidate.empty())
            return candidate / "logs" / "captures";

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
