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
import <iostream>;
import <system_error>;
import <vector>;
import <array>;
import <fstream>;
import <string>;
import <iterator>;
import <source_location>;
import <cctype>;

import aengine.cli;
import aengine.updater.tools;
import aengine.updater.config;
import aengine.core.logger;

export namespace epochnamespace::updater
{
    namespace system_detail
    {
        constexpr std::string_view kUpdaterLog = "Updater";

        inline void log_info(const std::string& message)
        {
            logger::get(kUpdaterLog).log(
                logger::LogLevel::INFO,
                message,
                std::source_location::current());
            std::cout << message << std::endl;
        }

        inline void log_error(const std::string& message)
        {
            logger::get(kUpdaterLog).log(
                logger::LogLevel::Error,
                message,
                std::source_location::current());
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
    }

    // ─────────────────────────────────────────────
    // Results / channels
    // ─────────────────────────────────────────────
    export struct UpdateCommandResult
    {
        bool update_available{ false };
        bool force_required{ false };
        bool update_performed{ false };
    };

    export struct UpdateChannel
    {
        std::string version_url;
        std::string binary_url;
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
            "start \"\" \"" << target_binary.string() << "\"\n";
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
    bool check_for_updates(const std::string& url)
    {
        constexpr char tmp[] = "remote_version.txt";

        if (!epochnamespace::updater::download_file(url, tmp))
            return false;

        const std::string downloaded = system_detail::read_text_file(tmp);
        std::filesystem::remove(tmp);

        const std::string localVersion =
            system_detail::extract_version_string(epochnamespace::updater::PROJECT_VERSION);
        const std::string remoteVersion =
            system_detail::extract_version_string(downloaded);

        if (remoteVersion.empty())
        {
            system_detail::log_error("Could not parse remote version payload.");
            return false;
        }

        const std::string normalizedLocal =
            localVersion.empty()
            ? system_detail::trim_ascii(epochnamespace::updater::PROJECT_VERSION)
            : localVersion;

        system_detail::log_info("[INFO] Local  : " + normalizedLocal);
        system_detail::log_info("[INFO] Remote : " + remoteVersion);

        return remoteVersion != normalizedLocal;
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

    // ─────────────────────────────────────────────
    // Entry point
    // ─────────────────────────────────────────────
    export UpdateCommandResult run_update_command(
        const UpdateChannel& channel,
        bool force)
    {
        cleanup_previous_update_artifacts();

        UpdateCommandResult r{};

        if (!check_for_updates(channel.version_url))
        {
            system_detail::log_info("[INFO] No packaged update is currently available.");
            return r;
        }

        r.update_available = true;

        if (!force)
        {
            r.force_required = true;
            return r;
        }

        r.update_performed = install_from_binary(channel.binary_url);
        return r;
    }
}
