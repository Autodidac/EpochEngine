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

export module aengine.updater.system;

import <filesystem>;
import <regex>;
import <system_error>;
import <vector>;
import <array>;
import <fstream>;
import <iostream>;
import <string>;
import <iterator>;
import <source_location>;
import <cctype>;
import <cstdlib>;
import <atomic>;
import <chrono>;

import aengine.cli;
import aengine.updater.tools;
import aengine.updater.config;

export namespace epochnamespace::updater
{
    namespace system_detail
    {
        inline void log_info(const std::string& message)
        {
            std::cout << message << std::endl;
        }

        inline void log_error(const std::string& message)
        {
            std::cerr << message << std::endl;
        }

        [[nodiscard]] inline std::string strip_utf8_bom(std::string text)
        {
            if (text.size() >= 3
                && static_cast<unsigned char>(text[0]) == 0xEF
                && static_cast<unsigned char>(text[1]) == 0xBB
                && static_cast<unsigned char>(text[2]) == 0xBF)
            {
                text.erase(0, 3);
            }
            return text;
        }

        [[nodiscard]] inline std::string trim_ascii(std::string text)
        {
            text = strip_utf8_bom(std::move(text));

            const auto is_trim = [](unsigned char c) noexcept
            {
                return c <= 0x20 || c == 0x7F;
            };

            while (!text.empty() && is_trim(static_cast<unsigned char>(text.front())))
                text.erase(text.begin());

            while (!text.empty() && is_trim(static_cast<unsigned char>(text.back())))
                text.pop_back();

            return text;
        }

        [[nodiscard]] inline std::string read_text_file(const std::filesystem::path& path)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return {};

            return std::string(
                (std::istreambuf_iterator<char>(in)),
                std::istreambuf_iterator<char>());
        }

        [[nodiscard]] inline std::string extract_version_string(std::string text)
        {
            text = trim_ascii(std::move(text));
            if (text.empty())
                return {};

            {
                static const std::regex kSemver(
                    R"((\d+)\s*\.\s*(\d+)\s*\.\s*(\d+))",
                    std::regex::optimize);
                std::smatch match;
                if (std::regex_search(text, match, kSemver))
                    return match[1].str() + "." + match[2].str() + "." + match[3].str();
            }

            const auto parse_named_component = [&](const char* name) -> std::string
            {
                const std::regex componentRegex(
                    std::string{ R"(\b)" } + name + R"(\s*=\s*(\d+))",
                    std::regex::optimize);
                std::smatch match;
                if (!std::regex_search(text, match, componentRegex))
                    return {};
                return match[1].str();
            };

            const std::string major = parse_named_component("major");
            const std::string minor = parse_named_component("minor");
            const std::string revision = parse_named_component("revision");

            if (!major.empty() && !minor.empty() && !revision.empty())
                return major + "." + minor + "." + revision;

            return {};
        }

        [[nodiscard]] inline std::array<int, 3> parse_version_triplet(const std::string& text)
        {
            std::array<int, 3> parts{ 0, 0, 0 };

            std::size_t start = 0;
            for (std::size_t i = 0; i < parts.size(); ++i)
            {
                const std::size_t end = text.find('.', start);
                const std::string token = text.substr(start, end == std::string::npos ? std::string::npos : (end - start));
                parts[i] = token.empty() ? 0 : std::stoi(token);

                if (end == std::string::npos)
                    break;
                start = end + 1;
            }

            return parts;
        }

        [[nodiscard]] inline int compare_versions(
            const std::string& lhs,
            const std::string& rhs)
        {
            const auto left = parse_version_triplet(lhs);
            const auto right = parse_version_triplet(rhs);

            for (std::size_t i = 0; i < left.size(); ++i)
            {
                if (left[i] < right[i])
                    return -1;
                if (left[i] > right[i])
                    return 1;
            }

            return 0;
        }

        struct VersionCheckResult
        {
            bool ok{ false };
            std::string local;
            std::string remote;
            bool update_available{ false };
        };

        [[nodiscard]] inline std::filesystem::path current_binary_path()
        {
            std::error_code ec;
            if (!epochnamespace::core::cli::exe_path.empty())
                return std::filesystem::absolute(epochnamespace::core::cli::exe_path, ec).lexically_normal();

#if defined(_WIN32)
            return std::filesystem::absolute("ConsoleApplication1.exe", ec).lexically_normal();
#else
            return std::filesystem::absolute("ConsoleApplication1", ec).lexically_normal();
#endif
        }

        [[nodiscard]] inline std::filesystem::path replacement_binary_path(
            const std::filesystem::path& target_binary)
        {
            const std::filesystem::path parent = target_binary.parent_path();
            const std::filesystem::path stem = target_binary.stem();
            const std::filesystem::path ext = target_binary.extension();
            return parent / (stem.string() + ".update" + ext.string());
        }

        [[nodiscard]] inline std::filesystem::path replacement_package_path(
            const std::filesystem::path& target_binary)
        {
            return target_binary.parent_path() / "main.update.zip";
        }

        [[nodiscard]] inline std::filesystem::path replacement_extract_dir(
            const std::filesystem::path& target_binary)
        {
            return target_binary.parent_path() / "__epoch_update";
        }

        [[nodiscard]] inline std::filesystem::path source_archive_path(
            const std::filesystem::path& target_binary)
        {
            return target_binary.parent_path() / "EpochEngine-source-main.zip";
        }

        [[nodiscard]] inline std::filesystem::path source_staging_dir(
            const std::filesystem::path& target_binary)
        {
            return target_binary.parent_path() / "__epoch_source_update";
        }

        [[nodiscard]] inline std::filesystem::path source_final_dir(
            const std::filesystem::path& target_binary)
        {
            return target_binary.parent_path() / "EpochEngine-source-main";
        }

        [[nodiscard]] inline std::filesystem::path make_temp_download_path(std::string_view stem)
        {
            static std::atomic<unsigned long long> s_counter{ 0 };

            std::error_code ec;
            auto temp_root = std::filesystem::temp_directory_path(ec);
            if (ec || temp_root.empty())
            {
                ec.clear();
                temp_root = std::filesystem::current_path(ec);
            }

            std::string safe_stem;
            safe_stem.reserve(stem.size());
            for (const char ch : stem)
            {
                const auto uch = static_cast<unsigned char>(ch);
                safe_stem.push_back(std::isalnum(uch) ? ch : '_');
            }

            if (safe_stem.empty())
                safe_stem = "download";

            const auto serial = s_counter.fetch_add(1, std::memory_order_relaxed);
            const auto tick =
                static_cast<unsigned long long>(
                    std::chrono::high_resolution_clock::now().time_since_epoch().count());
            return temp_root / ("epoch_" + safe_stem + "_" + std::to_string(tick) + "_" + std::to_string(serial) + ".tmp");
        }

        [[nodiscard]] inline std::filesystem::path source_solution_path(
            const std::filesystem::path& source_root)
        {
            return source_root / "Engine.sln";
        }

        [[nodiscard]] inline std::filesystem::path source_runtime_output_dir(
            const std::filesystem::path& source_root)
        {
            return source_root / "x64" / "Debug";
        }

        [[nodiscard]] inline std::filesystem::path source_runtime_binary_path(
            const std::filesystem::path& source_root,
            const std::filesystem::path& target_binary)
        {
            return source_runtime_output_dir(source_root) / target_binary.filename();
        }

        [[nodiscard]] inline std::filesystem::path source_manifest_root(
            const std::filesystem::path& source_root)
        {
            return source_root / "Engine";
        }

        [[nodiscard]] inline std::filesystem::path env_path(const char* name)
        {
#if defined(_WIN32)
            char* value = nullptr;
            std::size_t length = 0;
            if (_dupenv_s(&value, &length, name) == 0 && value != nullptr)
            {
                const std::filesystem::path result{ value };
                std::free(value);
                return result;
            }
#else
            if (const char* value = std::getenv(name))
                return std::filesystem::path{ value };
#endif
            return {};
        }

        [[nodiscard]] inline std::filesystem::path find_msbuild_path()
        {
#if defined(_WIN32)
            const auto from_env = env_path("MSBUILD_EXE_PATH");
            if (!from_env.empty() && std::filesystem::exists(from_env))
                return from_env;

            const std::filesystem::path program_files = env_path("ProgramFiles");
            const std::filesystem::path program_files_x86 = env_path("ProgramFiles(x86)");

            const std::array<std::filesystem::path, 10> candidates{
                program_files / "Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2022/Enterprise/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2022/Preview/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2022/Enterprise/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2022/Preview/MSBuild/Current/Bin/MSBuild.exe",
            };

            for (const auto& candidate : candidates)
            {
                if (!candidate.empty() && std::filesystem::exists(candidate))
                    return candidate;
            }
#endif
            return {};
        }

        [[nodiscard]] inline bool build_runtime_from_source(
            const std::filesystem::path& source_root,
            const std::filesystem::path& target_binary)
        {
#if defined(_WIN32)
            const auto manifest_root = source_manifest_root(source_root);
            const auto vcpkg_root = env_path("VCPKG_ROOT");
            const auto vcpkg_exe = vcpkg_root / "vcpkg.exe";
            if (vcpkg_root.empty() || !std::filesystem::exists(vcpkg_exe))
            {
                log_error("[ERROR] Could not locate vcpkg. Set VCPKG_ROOT before running a source update.");
                return false;
            }

            const auto msbuild = find_msbuild_path();
            if (msbuild.empty())
            {
                log_error("[ERROR] Could not locate MSBuild. Install Visual Studio 2022 or Build Tools to use source updates.");
                return false;
            }

            const auto solution = source_solution_path(source_root);
            if (!std::filesystem::exists(solution))
            {
                log_error("[ERROR] Extracted source snapshot does not contain Engine.sln.");
                return false;
            }

            log_info("[INFO] Building updated runtime from source.");
            log_info("[INFO] Restoring source dependencies with vcpkg.");
            const std::string vcpkg_command =
                "\"" + vcpkg_exe.string() + "\" install --triplet x64-windows --x-manifest-root=\""
                + manifest_root.string() + "\"";
            if (std::system(vcpkg_command.c_str()) != 0)
            {
                log_error("[ERROR] vcpkg dependency restore failed.");
                return false;
            }

            log_info("[INFO] MSBuild: " + msbuild.string());

            const std::string command =
                "\"" + msbuild.string() + "\" \"" + solution.string() + "\""
                " /t:ConsoleApplication1"
                " /p:Configuration=Debug"
                " /p:Platform=x64"
                " /m:1 /clp:ErrorsOnly";

            if (std::system(command.c_str()) != 0)
            {
                log_error("[ERROR] Source build failed.");
                return false;
            }

            const auto built_binary = source_runtime_binary_path(source_root, target_binary);
            if (!std::filesystem::exists(built_binary))
            {
                log_error("[ERROR] Source build completed without producing the runtime binary.");
                return false;
            }

            return true;
#else
            (void)source_root;
            (void)target_binary;
            log_error("[ERROR] Source rebuild updates are currently implemented for Windows MSBuild builds only.");
            return false;
#endif
        }
    }

    // ─────────────────────────────────────────────
    // Results / channels
    // ─────────────────────────────────────────────
    export struct UpdateCommandResult
    {
        bool update_available{ false };
        bool packaged_update_available{ false };
        bool force_required{ false };
        bool update_performed{ false };
        std::string local_version;
        std::string remote_version;
        std::string source_local_version;
        std::string source_remote_version;
        bool source_update_available{ false };
    };

    export struct UpdateChannel
    {
        std::string version_url;
        std::string binary_url;
        std::string source_url;
        std::string source_version_url;
    };

    // ─────────────────────────────────────────────
    // Cleanup
    // ─────────────────────────────────────────────
    export void cleanup_previous_update_artifacts()
    {
        namespace fs = std::filesystem;

        if (!epochnamespace::updater::LEAVE_NO_FILES_ALWAYS_REDOWNLOAD)
            return;

        std::vector<fs::path> targets;

#if defined(_WIN32)
        targets.emplace_back("replace_updater.bat");
#else
        targets.emplace_back("replace_and_restart.sh");
#endif
        targets.emplace_back(
            epochnamespace::updater::REPO + "-main");

        for (const auto& t : targets)
        {
            std::error_code ec;
            fs::remove_all(t, ec);
        }
    }

    // ─────────────────────────────────────────────
    // Binary replacement
    // ─────────────────────────────────────────────
    bool replace_binary_from_script(
        const std::filesystem::path& target_binary,
        const std::filesystem::path& new_binary)
    {
#if defined(_WIN32)
        const std::filesystem::path script_path =
            std::filesystem::absolute(epochnamespace::updater::REPLACE_RUNNING_EXE_SCRIPT_NAME());
        std::ofstream bat(script_path);
        if (!bat)
            return false;

        bat <<
            "@echo off\n"
            "timeout /t 2 >nul\n"
            "del /F /Q \"" << target_binary.string() << "\" >nul 2>&1\n"
            "move /Y \"" << new_binary.string() << "\" \"" << target_binary.string() << "\" >nul\n"
            "if errorlevel 1 exit /b 1\n"
            "start \"\" \"" << target_binary.string() << "\"\n";
        bat.close();

        const std::string command =
            "cmd.exe /C start \"\" /min \"" + script_path.string() + "\"";
        if (std::system(command.c_str()) != 0)
            return false;

        std::exit(0);
#else
        const std::filesystem::path script_path =
            std::filesystem::absolute(epochnamespace::updater::REPLACE_RUNNING_EXE_SCRIPT_NAME());
        std::ofstream sh(script_path);
        if (!sh)
            return false;

        sh <<
            "#!/bin/sh\n"
            "sleep 2\n"
            "rm -f \"" << target_binary.string() << "\"\n"
            "mv \"" << new_binary.string() << "\" \"" << target_binary.string() << "\"\n"
            "chmod +x \"" << target_binary.string() << "\"\n"
            "\"" << target_binary.string() << "\" &\n";
        sh.close();

        const std::string command =
            "chmod +x \"" + script_path.string() + "\" && \"" + script_path.string() + "\" &";
        if (std::system(command.c_str()) != 0)
            return false;

        std::exit(0);
#endif
    }

    bool replace_runtime_from_script(
        const std::filesystem::path& target_binary,
        const std::filesystem::path& extracted_runtime_dir,
        const std::filesystem::path& package_archive)
    {
#if defined(_WIN32)
        const std::filesystem::path target_dir = target_binary.parent_path();
        const std::filesystem::path script_path =
            std::filesystem::absolute(epochnamespace::updater::REPLACE_RUNNING_EXE_SCRIPT_NAME());
        std::ofstream bat(script_path);
        if (!bat)
            return false;

        bat <<
            "@echo off\n"
            "timeout /t 2 >nul\n"
            "robocopy \"" << extracted_runtime_dir.string() << "\" \"" << target_dir.string()
            << "\" /E /NFL /NDL /NJH /NJS /NC /NS >nul\n"
            "if errorlevel 8 exit /b 1\n"
            "rmdir /S /Q \"" << extracted_runtime_dir.string() << "\" >nul 2>&1\n"
            "del /F /Q \"" << package_archive.string() << "\" >nul 2>&1\n"
            "start \"\" /D \"" << target_dir.string() << "\" \"" << target_binary.string() << "\"\n";
        bat.close();

        const std::string command =
            "cmd.exe /C start \"\" /min \"" + script_path.string() + "\"";
        if (std::system(command.c_str()) != 0)
            return false;

        std::exit(0);
#else
        const std::filesystem::path target_dir = target_binary.parent_path();
        const std::filesystem::path script_path =
            std::filesystem::absolute(epochnamespace::updater::REPLACE_RUNNING_EXE_SCRIPT_NAME());
        std::ofstream sh(script_path);
        if (!sh)
            return false;

        sh <<
            "#!/bin/sh\n"
            "sleep 2\n"
            "cp -R \"" << extracted_runtime_dir.string() << "/.\" \"" << target_dir.string() << "\"\n"
            "rm -rf \"" << extracted_runtime_dir.string() << "\"\n"
            "rm -f \"" << package_archive.string() << "\"\n"
            "cd \"" << target_dir.string() << "\"\n"
            "\"" << target_binary.string() << "\" &\n";
        sh.close();

        const std::string command =
            "chmod +x \"" + script_path.string() + "\" && \"" + script_path.string() + "\" &";
        if (std::system(command.c_str()) != 0)
            return false;

        std::exit(0);
#endif
    }

    bool replace_runtime_from_source_script(
        const std::filesystem::path& target_binary,
        const std::filesystem::path& built_runtime_dir,
        const std::filesystem::path& source_root,
        const std::filesystem::path& package_archive)
    {
        (void)source_root;
#if defined(_WIN32)
        const std::filesystem::path target_dir = target_binary.parent_path();
        const std::filesystem::path script_path =
            std::filesystem::absolute(epochnamespace::updater::REPLACE_RUNNING_EXE_SCRIPT_NAME());
        std::ofstream bat(script_path);
        if (!bat)
            return false;

        const std::filesystem::path source_assets_dir = built_runtime_dir / "assets";
        const std::filesystem::path target_assets_dir = target_dir / "assets";

        bat <<
            "@echo off\n"
            "timeout /t 2 >nul\n"
            "robocopy \"" << built_runtime_dir.string() << "\" \"" << target_dir.string() << "\" "
            << "\"" << target_binary.filename().string() << "\" *.dll *.manifest /NFL /NDL /NJH /NJS /NC /NS >nul\n"
            "if errorlevel 8 exit /b 1\n"
            "if exist \"" << source_assets_dir.string() << "\" robocopy \"" << source_assets_dir.string()
            << "\" \"" << target_assets_dir.string() << "\" /E /NFL /NDL /NJH /NJS /NC /NS >nul\n"
            "if errorlevel 8 exit /b 1\n"
            "del /F /Q \"" << package_archive.string() << "\" >nul 2>&1\n"
            "start \"\" /D \"" << target_dir.string() << "\" \"" << target_binary.string() << "\"\n";
        bat.close();

        const std::string command =
            "cmd.exe /C start \"\" /min \"" + script_path.string() + "\"";
        if (std::system(command.c_str()) != 0)
            return false;

        std::exit(0);
#else
        const std::filesystem::path target_dir = target_binary.parent_path();
        const std::filesystem::path script_path =
            std::filesystem::absolute(epochnamespace::updater::REPLACE_RUNNING_EXE_SCRIPT_NAME());
        std::ofstream sh(script_path);
        if (!sh)
            return false;

        sh <<
            "#!/bin/sh\n"
            "sleep 2\n"
            "cp \"" << (built_runtime_dir / target_binary.filename()).string() << "\" \"" << target_binary.string() << "\"\n"
            "cp -R \"" << (built_runtime_dir / "assets").string() << "/.\" \"" << (target_dir / "assets").string() << "\"\n"
            "rm -f \"" << package_archive.string() << "\"\n"
            "cd \"" << target_dir.string() << "\"\n"
            "\"" << target_binary.string() << "\" &\n";
        sh.close();

        const std::string command =
            "chmod +x \"" + script_path.string() + "\" && \"" + script_path.string() + "\" &";
        if (std::system(command.c_str()) != 0)
            return false;

        std::exit(0);
#endif
    }

    bool replace_binary(
        const std::filesystem::path& target_binary,
        const std::filesystem::path& new_binary)
    {
        epochnamespace::updater::clean_up_build_files();
        return replace_binary_from_script(target_binary, new_binary);
    }

    // ─────────────────────────────────────────────
    // Version check (MODULE-SAFE)
    // ─────────────────────────────────────────────
    system_detail::VersionCheckResult check_for_updates(
        const std::string& url,
        std::string_view label = {})
    {
        system_detail::VersionCheckResult result{};
        const auto tmp = system_detail::make_temp_download_path(
            label.empty() ? std::string_view{ "remote_version" } : label);
        std::error_code ec;

        std::filesystem::remove(tmp, ec);

        if (!epochnamespace::updater::download_file(url, tmp.string()))
            return result;

        const std::string downloaded = system_detail::read_text_file(tmp);
        std::filesystem::remove(tmp, ec);

        const std::string localVersion =
            system_detail::extract_version_string(epochnamespace::updater::PROJECT_VERSION);
        const std::string remoteVersion =
            system_detail::extract_version_string(downloaded);

        if (remoteVersion.empty())
        {
            system_detail::log_error("Could not parse remote version payload.");
            return result;
        }

        const std::string normalizedLocal =
            localVersion.empty()
            ? system_detail::trim_ascii(epochnamespace::updater::PROJECT_VERSION)
            : localVersion;

        const std::string prefix =
            label.empty()
            ? std::string{}
            : (std::string{ label } + " ");

        system_detail::log_info("[INFO] " + prefix + "Local  : " + normalizedLocal);
        system_detail::log_info("[INFO] " + prefix + "Remote : " + remoteVersion);

        result.ok = true;
        result.local = normalizedLocal;
        result.remote = remoteVersion;
        result.update_available = system_detail::compare_versions(normalizedLocal, remoteVersion) < 0;
        return result;
    }

    // ─────────────────────────────────────────────
    // Installation paths
    // ─────────────────────────────────────────────
    bool install_from_binary(const std::string& url)
    {
        const auto target_binary = system_detail::current_binary_path();
        const auto lower_url = [&]()
        {
            std::string s = url;
            for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return s;
        }();

        if (lower_url.ends_with(".zip"))
        {
            const auto archive_path = system_detail::replacement_package_path(target_binary);
            const auto extract_dir = system_detail::replacement_extract_dir(target_binary);

            std::error_code ec;
            std::filesystem::remove_all(extract_dir, ec);

            if (!epochnamespace::updater::download_file(url, archive_path.string()))
                return false;

            if (!epochnamespace::updater::extract_archive(archive_path.string(), extract_dir.string()))
            {
                system_detail::log_error("Failed to extract update package.");
                return false;
            }

            return replace_runtime_from_script(target_binary, extract_dir, archive_path);
        }

        const auto bin = system_detail::replacement_binary_path(target_binary);

        if (!epochnamespace::updater::download_file(url, bin.string()))
            return false;

        return replace_binary(target_binary, bin);
    }

    export bool run_source_update_command(const UpdateChannel& channel, bool recheck_source_version = true)
    {
        if (channel.source_url.empty())
        {
            system_detail::log_error("[ERROR] Source update URL is not configured.");
            return false;
        }

        if (recheck_source_version && !channel.source_version_url.empty())
        {
            const auto source_status = check_for_updates(channel.source_version_url, "Source");
            if (source_status.ok && source_status.update_available)
            {
                system_detail::log_info("[INFO] A newer source snapshot is available on main.");
            }
            else if (source_status.ok)
            {
                system_detail::log_info("[INFO] No newer source snapshot is currently available; continuing because source update was requested explicitly.");
            }
        }

        const auto target_binary = system_detail::current_binary_path();
        const auto archive_path = system_detail::source_archive_path(target_binary);
        const auto staging_dir = system_detail::source_staging_dir(target_binary);
        const auto final_dir = system_detail::source_final_dir(target_binary);

        std::error_code ec;
        std::filesystem::remove_all(staging_dir, ec);
        std::filesystem::remove_all(final_dir, ec);

        system_detail::log_info("[INFO] Downloading latest source snapshot from main.");
        if (!epochnamespace::updater::download_file(channel.source_url, archive_path.string()))
            return false;

        if (!epochnamespace::updater::extract_archive(archive_path.string(), staging_dir.string()))
        {
            system_detail::log_error("[ERROR] Failed to extract source snapshot.");
            return false;
        }

        std::filesystem::path extracted_root{};
        for (const auto& entry : std::filesystem::directory_iterator(staging_dir, ec))
        {
            if (entry.is_directory())
            {
                extracted_root = entry.path();
                break;
            }
        }

        if (ec)
        {
            system_detail::log_error("[ERROR] Failed to inspect extracted source snapshot.");
            return false;
        }

        if (!extracted_root.empty())
        {
            std::filesystem::rename(extracted_root, final_dir, ec);
            if (ec)
            {
                ec.clear();
                std::filesystem::copy(
                    extracted_root,
                    final_dir,
                    std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
                    ec);
                if (ec)
                {
                    system_detail::log_error("[ERROR] Failed to move extracted source snapshot into place.");
                    return false;
                }
            }
        }
        else
        {
            std::filesystem::rename(staging_dir, final_dir, ec);
            if (ec)
            {
                system_detail::log_error("[ERROR] Extracted source snapshot did not contain a project root.");
                return false;
            }
        }

        std::filesystem::remove_all(staging_dir, ec);
        std::filesystem::remove(archive_path, ec);

        system_detail::log_info("[INFO] Source snapshot ready at: " + final_dir.string());
        if (!system_detail::build_runtime_from_source(final_dir, target_binary))
            return false;

        const auto built_runtime_dir = system_detail::source_runtime_output_dir(final_dir);
        const auto built_binary = system_detail::source_runtime_binary_path(final_dir, target_binary);
        if (!std::filesystem::exists(built_binary))
        {
            system_detail::log_error("[ERROR] Built runtime output is missing after source update.");
            return false;
        }

        system_detail::log_info("[INFO] Replacing current runtime from rebuilt source output.");
        return replace_runtime_from_source_script(target_binary, built_runtime_dir, final_dir, archive_path);
    }

    // ─────────────────────────────────────────────
    // Entry point
    // ─────────────────────────────────────────────
    export UpdateCommandResult run_update_command(
        const UpdateChannel& channel,
        bool force)
    {
        cleanup_previous_update_artifacts();

        UpdateCommandResult r{};

        const auto packaged_status = check_for_updates(channel.version_url);
        if (!packaged_status.ok)
            return r;

        r.local_version = packaged_status.local;
        r.remote_version = packaged_status.remote;

        if (packaged_status.update_available)
        {
            r.packaged_update_available = true;
            r.update_available = true;

            if (!force)
            {
                r.force_required = true;
                return r;
            }

            r.update_performed = install_from_binary(channel.binary_url);
            return r;
        }

        if (!channel.source_version_url.empty())
        {
            const auto source_status = check_for_updates(channel.source_version_url, "Source");
            if (source_status.ok)
            {
                r.source_local_version = source_status.local;
                r.source_remote_version = source_status.remote;
                r.source_update_available = source_status.update_available;
            }

            if (source_status.ok && source_status.update_available)
            {
                r.update_available = true;

                if (!force)
                {
                    r.force_required = true;
                    return r;
                }

                system_detail::log_info("[INFO] No newer packaged runtime is available. Falling back to source update from main.");
                r.update_performed = run_source_update_command(channel, false);
                return r;
            }
        }

        return r;
    }
}
