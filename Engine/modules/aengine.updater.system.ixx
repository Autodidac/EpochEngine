/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 ***********************************************/
module;

export module aengine.updater.system;

import <array>;
import <atomic>;
import <cctype>;
import <charconv>;
import <chrono>;
import <cstdlib>;
import <filesystem>;
import <fstream>;
import <iterator>;
import <regex>;
import <source_location>;
import <string>;
import <string_view>;
import <system_error>;
import <vector>;

import aengine.core.logger;
import aengine.cli;
import aengine.updater.tools;
import aengine.updater.config;

export namespace epochnamespace::updater
{
    namespace system_detail
    {
        [[nodiscard]] inline std::string logger_safe(std::string text)
        {
            if (text.size() >= 3
                && static_cast<unsigned char>(text[0]) == 0xEF
                && static_cast<unsigned char>(text[1]) == 0xBB
                && static_cast<unsigned char>(text[2]) == 0xBF)
            {
                text.erase(0, 3);
            }

            for (char& ch : text)
            {
                if (ch == '\r' || ch == '\n')
                    ch = ' ';
            }

            while (!text.empty() && text.back() == ' ')
                text.pop_back();

            return text;
        }

        inline void log_info(const std::string& message)
        {
            logger::get("Updater").log(
                logger::LogLevel::INFO,
                logger_safe(message),
                std::source_location::current());
        }

        inline void log_error(const std::string& message)
        {
            logger::get("Updater").log(
                logger::LogLevel::Error,
                logger_safe(message),
                std::source_location::current());
        }

        [[nodiscard]] inline std::string quote_shell_arg(const std::string& value)
        {
#if defined(_WIN32)
            std::string out;
            out.reserve(value.size() + 2);
            out.push_back('"');
            for (const char c : value)
            {
                if (c == '"')
                    out += "\\\"";
                else
                    out.push_back(c);
            }
            out.push_back('"');
            return out;
#else
            std::string out;
            out.reserve(value.size() + 2);
            out.push_back('"');
            for (const char c : value)
            {
                if (c == '"' || c == '\\' || c == '$' || c == '`')
                    out.push_back('\\');
                out.push_back(c);
            }
            out.push_back('"');
            return out;
#endif
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

        [[nodiscard]] inline std::string normalize_line_endings(std::string text)
        {
            std::string out;
            out.reserve(text.size());

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const char c = text[i];

                if (c == '\r')
                {
                    if ((i + 1) < text.size() && text[i + 1] == '\n')
                        continue;

                    out.push_back('\n');
                    continue;
                }

                out.push_back(c);
            }

            return out;
        }

        [[nodiscard]] inline std::string trim_ascii(std::string text)
        {
            text = strip_utf8_bom(std::move(text));
            text = normalize_line_endings(std::move(text));

            const auto is_trim = [](const unsigned char c) noexcept
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

            std::string text{
                std::istreambuf_iterator<char>{ in },
                std::istreambuf_iterator<char>{}
            };

            text = strip_utf8_bom(std::move(text));
            text = normalize_line_endings(std::move(text));
            return text;
        }

        [[nodiscard]] inline int parse_int_or_zero(const std::string_view text) noexcept
        {
            int value = 0;
            const auto* const first = text.data();
            const auto* const last = text.data() + text.size();
            const auto [ptr, ec] = std::from_chars(first, last, value);

            if (ec != std::errc{} || ptr != last)
                return 0;

            return value;
        }

        [[nodiscard]] inline std::string canonicalize_semver(const std::string_view text)
        {
            std::array<int, 3> parts{ 0, 0, 0 };

            int part_index = 0;
            bool in_number = false;
            std::string current;

            for (const char c : text)
            {
                if (c >= '0' && c <= '9')
                {
                    current.push_back(c);
                    in_number = true;
                    continue;
                }

                if (c == '.' && in_number)
                {
                    if (part_index >= 3)
                        break;

                    parts[part_index++] = parse_int_or_zero(current);
                    current.clear();
                    in_number = false;
                    continue;
                }

                if (in_number)
                    break;
            }

            if (!current.empty() && part_index < 3)
                parts[part_index++] = parse_int_or_zero(current);

            return std::to_string(parts[0]) + "."
                + std::to_string(parts[1]) + "."
                + std::to_string(parts[2]);
        }

        [[nodiscard]] inline std::string extract_version_string(std::string text)
        {
            text = trim_ascii(std::move(text));
            if (text.empty())
                return {};

            {
                static const std::regex k_semver(
                    R"((\d+)\s*\.\s*(\d+)\s*\.\s*(\d+))",
                    std::regex::optimize);

                std::smatch match;
                if (std::regex_search(text, match, k_semver))
                {
                    return std::to_string(parse_int_or_zero(match[1].str()))
                        + "."
                        + std::to_string(parse_int_or_zero(match[2].str()))
                        + "."
                        + std::to_string(parse_int_or_zero(match[3].str()));
                }
            }

            const auto parse_named_component = [&](const char* name) -> std::string
                {
                    const std::regex component_regex(
                        std::string{ R"(\b)" } + name + R"(\s*=\s*(\d+))",
                        std::regex::optimize);

                    std::smatch match;
                    if (!std::regex_search(text, match, component_regex))
                        return {};

                    return match[1].str();
                };

            const std::string major = parse_named_component("major");
            const std::string minor = parse_named_component("minor");
            const std::string revision = parse_named_component("revision");

            if (!major.empty() && !minor.empty() && !revision.empty())
            {
                return std::to_string(parse_int_or_zero(major))
                    + "."
                    + std::to_string(parse_int_or_zero(minor))
                    + "."
                    + std::to_string(parse_int_or_zero(revision));
            }

            return canonicalize_semver(text);
        }

        [[nodiscard]] inline std::array<int, 3> parse_version_triplet(const std::string& text)
        {
            std::array<int, 3> parts{ 0, 0, 0 };

            std::size_t start = 0;
            for (std::size_t i = 0; i < parts.size(); ++i)
            {
                const std::size_t end = text.find('.', start);
                const std::size_t token_end = (end == std::string::npos ? text.size() : end);
                const std::string_view token{ text.data() + start, token_end - start };

                parts[i] = parse_int_or_zero(token);

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

        [[nodiscard]] inline std::string strip_url_query_and_fragment(std::string url)
        {
            const auto q = url.find('?');
            const auto h = url.find('#');

            const auto cut_q = (q == std::string::npos) ? url.size() : q;
            const auto cut_h = (h == std::string::npos) ? url.size() : h;
            const auto cut = (cut_q < cut_h) ? cut_q : cut_h;

            url.resize(cut);
            return url;
        }

        [[nodiscard]] inline std::string lower_ascii(std::string text)
        {
            for (char& ch : text)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return text;
        }

        struct VersionCheckResult
        {
            bool ok{ false };
            std::string local;
            std::string remote;
            bool update_available{ false };
        };

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

        [[nodiscard]] inline std::filesystem::path current_binary_path()
        {
            std::error_code ec;

            if (!epochnamespace::core::cli::exe_path.empty())
                return std::filesystem::absolute(epochnamespace::core::cli::exe_path, ec).lexically_normal();

            const auto configured_binary = std::filesystem::path{ RUNTIME_BINARY_NAME() };
            if (!configured_binary.empty())
                return std::filesystem::absolute(configured_binary, ec).lexically_normal();

#if defined(_WIN32)
            return std::filesystem::absolute("ConsoleApplication1.exe", ec).lexically_normal();
#else
            return std::filesystem::absolute("ConsoleApplication1", ec).lexically_normal();
#endif
        }

        [[nodiscard]] inline std::filesystem::path replacement_binary_path(const std::filesystem::path& target_binary)
        {
            const auto parent = target_binary.parent_path();
            const auto stem = target_binary.stem();
            const auto ext = target_binary.extension();
            return parent / (stem.string() + ".update" + ext.string());
        }

        [[nodiscard]] inline std::filesystem::path replacement_package_path(const std::filesystem::path& target_binary)
        {
            return target_binary.parent_path() / "main.update.zip";
        }

        [[nodiscard]] inline std::filesystem::path replacement_extract_dir(const std::filesystem::path& target_binary)
        {
            return target_binary.parent_path() / "__epoch_update";
        }

        [[nodiscard]] inline std::filesystem::path source_archive_path(const std::filesystem::path& target_binary)
        {
            return target_binary.parent_path() / "EpochEngine-source-main.zip";
        }

        [[nodiscard]] inline std::filesystem::path source_staging_dir(const std::filesystem::path& target_binary)
        {
            return target_binary.parent_path() / "__epoch_source_update";
        }

        [[nodiscard]] inline std::filesystem::path source_final_dir(const std::filesystem::path& target_binary)
        {
            return target_binary.parent_path() / "EpochEngine-source-main";
        }

        [[nodiscard]] inline std::filesystem::path make_temp_download_path(const std::string_view stem)
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
            const auto tick = static_cast<unsigned long long>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count());

            return temp_root / ("epoch_" + safe_stem + "_" + std::to_string(tick) + "_" + std::to_string(serial) + ".tmp");
        }

        [[nodiscard]] inline std::filesystem::path make_temp_script_path(const std::string_view stem)
        {
#if defined(_WIN32)
            return make_temp_download_path(stem).replace_extension(".bat");
#else
            return make_temp_download_path(stem).replace_extension(".sh");
#endif
        }

        [[nodiscard]] inline std::filesystem::path source_solution_path(const std::filesystem::path& source_root)
        {
            return source_root / SOURCE_SOLUTION_NAME();
        }

        [[nodiscard]] inline std::filesystem::path source_runtime_output_dir(const std::filesystem::path& source_root)
        {
            return source_root / SOURCE_BUILD_PLATFORM() / SOURCE_BUILD_CONFIGURATION();
        }

        [[nodiscard]] inline std::filesystem::path source_runtime_binary_path(
            const std::filesystem::path& source_root,
            const std::filesystem::path& target_binary)
        {
            return source_runtime_output_dir(source_root) / target_binary.filename();
        }

        [[nodiscard]] inline std::filesystem::path source_manifest_root(const std::filesystem::path& source_root)
        {
            return source_root / SOURCE_MANIFEST_ROOT_NAME();
        }

        [[nodiscard]] inline std::filesystem::path source_version_file_path(const std::filesystem::path& source_root)
        {
            return source_root / SOURCE_VERSION_FILE_NAME();
        }

        [[nodiscard]] inline std::string read_local_source_version(const std::filesystem::path& source_root)
        {
            const auto version_file = source_version_file_path(source_root);
            if (!version_file.empty())
            {
                const auto text = read_text_file(version_file);
                const auto version = extract_version_string(text);
                if (!version.empty())
                    return version;
            }

            return extract_version_string(PROJECT_VERSION);
        }

        [[nodiscard]] inline std::filesystem::path find_msbuild_path()
        {
#if defined(_WIN32)
            const auto from_env = env_path("MSBUILD_EXE_PATH");
            if (!from_env.empty() && std::filesystem::exists(from_env))
                return from_env;

            const auto program_files_x86 = env_path("ProgramFiles(x86)");
            const auto vswhere =
                program_files_x86 / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";

            if (!vswhere.empty() && std::filesystem::exists(vswhere))
            {
                const auto out = make_temp_download_path("msbuild_path");

                const std::string command =
                    quote_shell_arg(vswhere.string())
                    + " -latest -requires Microsoft.Component.MSBuild "
                    "-find MSBuild\\**\\Bin\\MSBuild.exe > "
                    + quote_shell_arg(out.string());

                if (std::system(command.c_str()) == 0)
                {
                    const auto text = trim_ascii(read_text_file(out));
                    std::error_code ec;
                    std::filesystem::remove(out, ec);

                    if (!text.empty() && std::filesystem::exists(text))
                        return std::filesystem::path{ text };
                }
            }

            const auto program_files = env_path("ProgramFiles");

            const std::array<std::filesystem::path, 20> candidates{
                program_files / "Microsoft Visual Studio/2026/Community/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2026/Professional/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2026/Enterprise/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2026/BuildTools/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2026/Preview/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2022/Enterprise/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2022/Preview/MSBuild/Current/Bin/MSBuild.exe",

                program_files_x86 / "Microsoft Visual Studio/2026/Community/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2026/Professional/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2026/Enterprise/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2026/BuildTools/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2026/Preview/MSBuild/Current/Bin/MSBuild.exe",
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
                log_error("Could not locate vcpkg. Set VCPKG_ROOT before running a source update.");
                return false;
            }

            const auto msbuild = find_msbuild_path();
            if (msbuild.empty())
            {
                log_error("Could not locate MSBuild. Install Visual Studio Build Tools or set MSBUILD_EXE_PATH.");
                return false;
            }

            const auto solution = source_solution_path(source_root);
            if (!std::filesystem::exists(solution))
            {
                log_error("Extracted source snapshot does not contain the configured solution file.");
                return false;
            }

            log_info(" Building updated runtime from source.");
            log_info("Restoring source dependencies with vcpkg.");

            const std::string vcpkg_command =
                quote_shell_arg(vcpkg_exe.string())
                + " install --triplet "
                + SOURCE_BUILD_PLATFORM()
                + "-windows --x-manifest-root="
                + quote_shell_arg(manifest_root.string());

            if (std::system(vcpkg_command.c_str()) != 0)
            {
                log_error("vcpkg dependency restore failed.");
                return false;
            }

            log_info("MSBuild: " + msbuild.string());

            const std::string command =
                quote_shell_arg(msbuild.string())
                + " " + quote_shell_arg(solution.string())
                + " /t:" + SOURCE_BUILD_TARGET()
                + " /p:Configuration=" + SOURCE_BUILD_CONFIGURATION()
                + " /p:Platform=" + SOURCE_BUILD_PLATFORM()
                + " /m:1 /clp:ErrorsOnly";

            if (std::system(command.c_str()) != 0)
            {
                log_error("Source build failed.");
                return false;
            }

            const auto built_binary = source_runtime_binary_path(source_root, target_binary);
            if (!std::filesystem::exists(built_binary))
            {
                log_error("Source build completed without producing the runtime binary.");
                return false;
            }

            return true;
#else
            (void)source_root;
            (void)target_binary;
            log_error("Source rebuild updates are currently implemented for Windows MSBuild builds only.");
            return false;
#endif
        }

        [[nodiscard]] inline std::filesystem::path first_subdirectory(const std::filesystem::path& root)
        {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(root, ec))
            {
                if (entry.is_directory())
                    return entry.path();
            }

            return {};
        }
    }

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

    export void cleanup_previous_update_artifacts()
    {
        namespace fs = std::filesystem;

        if (!LEAVE_NO_FILES_ALWAYS_REDOWNLOAD)
            return;

        std::vector<fs::path> targets;
        targets.emplace_back(std::string{ REPO } + "-main");

        for (const auto& target : targets)
        {
            std::error_code ec;
            fs::remove_all(target, ec);
        }
    }

    bool replace_binary_from_script(
        const std::filesystem::path& target_binary,
        const std::filesystem::path& new_binary)
    {
#if defined(_WIN32)
        const auto script_path = system_detail::make_temp_script_path("replace_binary");
        const auto handoff_log = target_binary.parent_path() / "epoch_update_handoff.log";

        std::ofstream bat(script_path, std::ios::binary);
        if (!bat)
        {
            system_detail::log_error("Failed to create binary replacement batch.");
            return false;
        }

        bat
            << "@echo off\r\n"
            << "setlocal\r\n"
            << "set \"TARGET=" << target_binary.string() << "\"\r\n"
            << "set \"NEWBIN=" << new_binary.string() << "\"\r\n"
            << "set \"LOG=" << handoff_log.string() << "\"\r\n"
            << "> \"%LOG%\" echo [INFO] Binary replacement started\r\n"
            << ">> \"%LOG%\" echo [INFO] TARGET=%TARGET%\r\n"
            << ">> \"%LOG%\" echo [INFO] NEWBIN=%NEWBIN%\r\n"
            << "for /L %%I in (1,1,60) do (\r\n"
            << "  del /F /Q \"%TARGET%\" >nul 2>&1\r\n"
            << "  if exist \"%TARGET%\" (\r\n"
            << "    >> \"%LOG%\" echo [INFO] Waiting for target unlock attempt %%I\r\n"
            << "    timeout /t 1 >nul\r\n"
            << "  ) else (\r\n"
            << "    goto target_ready\r\n"
            << "  )\r\n"
            << ")\r\n"
            << ">> \"%LOG%\" echo [ERROR] Timed out waiting for target executable to unlock.\r\n"
            << "exit /b 1\r\n"
            << ":target_ready\r\n"
            << "copy /Y \"%NEWBIN%\" \"%TARGET%\" >nul\r\n"
            << "if errorlevel 1 (\r\n"
            << "  >> \"%LOG%\" echo [ERROR] Failed to copy replacement executable into place.\r\n"
            << "  exit /b 1\r\n"
            << ")\r\n"
            << "if not exist \"%TARGET%\" (\r\n"
            << "  >> \"%LOG%\" echo [ERROR] Replacement executable is still missing after copy.\r\n"
            << "  exit /b 1\r\n"
            << ")\r\n"
            << ">> \"%LOG%\" echo [INFO] Replacement executable copied successfully.\r\n"
            << "start \"\" \"%TARGET%\"\r\n"
            << ">> \"%LOG%\" echo [INFO] Restarted updated executable.\r\n"
            << "del /F /Q \"%NEWBIN%\" >nul 2>&1\r\n"
            << "del /F /Q \"%~f0\" >nul 2>&1\r\n";

        bat.close();

        const std::string command =
            "cmd.exe /C start \"\" /min " + system_detail::quote_shell_arg(script_path.string());

        if (std::system(command.c_str()) != 0)
        {
            system_detail::log_error("Failed to launch binary replacement batch.");
            return false;
        }

        std::exit(0);
#else
        const auto script_path = system_detail::make_temp_script_path("replace_binary");

        std::ofstream sh(script_path, std::ios::binary);
        if (!sh)
        {
            system_detail::log_error("Failed to create binary replacement script.");
            return false;
        }

        sh
            << "#!/bin/sh\n"
            << "TARGET=" << system_detail::quote_shell_arg(target_binary.string()) << "\n"
            << "NEWBIN=" << system_detail::quote_shell_arg(new_binary.string()) << "\n"
            << "i=0\n"
            << "while [ $i -lt 20 ]; do\n"
            << "  rm -f \"$TARGET\"\n"
            << "  mv \"$NEWBIN\" \"$TARGET\" 2>/dev/null && break\n"
            << "  i=$((i+1))\n"
            << "  sleep 1\n"
            << "done\n"
            << "chmod +x \"$TARGET\"\n"
            << "\"$TARGET\" &\n"
            << "rm -f \"$0\"\n";

        sh.close();

        const std::string command =
            "chmod +x " + system_detail::quote_shell_arg(script_path.string())
            + " && " + system_detail::quote_shell_arg(script_path.string()) + " &";

        if (std::system(command.c_str()) != 0)
        {
            system_detail::log_error("Failed to launch binary replacement script.");
            return false;
        }

        std::exit(0);
#endif
    }

    bool replace_runtime_from_script(
        const std::filesystem::path& target_binary,
        const std::filesystem::path& extracted_runtime_dir,
        const std::filesystem::path& package_archive)
    {
#if defined(_WIN32)
        const auto target_dir = target_binary.parent_path();
        const auto script_path = system_detail::make_temp_script_path("replace_runtime_zip");
        const auto extracted_binary = extracted_runtime_dir / target_binary.filename();
        const auto handoff_log = target_dir / "epoch_update_handoff.log";

        std::ofstream bat(script_path, std::ios::binary);
        if (!bat)
        {
            system_detail::log_error("Failed to create packaged runtime replacement batch.");
            return false;
        }

        bat
            << "@echo off\r\n"
            << "setlocal\r\n"
            << "set \"EXTRACTED=" << extracted_runtime_dir.string() << "\"\r\n"
            << "set \"TARGETDIR=" << target_dir.string() << "\"\r\n"
            << "set \"TARGETEXE=" << target_binary.string() << "\"\r\n"
            << "set \"NEWEXE=" << extracted_binary.string() << "\"\r\n"
            << "set \"ARCHIVE=" << package_archive.string() << "\"\r\n"
            << "set \"LOG=" << handoff_log.string() << "\"\r\n"
            << "> \"%LOG%\" echo [INFO] Packaged runtime replacement started\r\n"
            << ">> \"%LOG%\" echo [INFO] TARGETEXE=%TARGETEXE%\r\n"
            << ">> \"%LOG%\" echo [INFO] NEWEXE=%NEWEXE%\r\n"
            << ">> \"%LOG%\" echo [INFO] EXTRACTED=%EXTRACTED%\r\n"
            << "for /L %%I in (1,1,60) do (\r\n"
            << "  del /F /Q \"%TARGETEXE%\" >nul 2>&1\r\n"
            << "  if exist \"%TARGETEXE%\" (\r\n"
            << "    >> \"%LOG%\" echo [INFO] Waiting for runtime unlock attempt %%I\r\n"
            << "    timeout /t 1 >nul\r\n"
            << "  ) else (\r\n"
            << "    goto exe_ready\r\n"
            << "  )\r\n"
            << ")\r\n"
            << ">> \"%LOG%\" echo [ERROR] Timed out waiting for target runtime executable to unlock.\r\n"
            << "exit /b 1\r\n"
            << ":exe_ready\r\n"
            << "copy /Y \"%NEWEXE%\" \"%TARGETEXE%\" >nul\r\n"
            << "if errorlevel 1 (\r\n"
            << "  >> \"%LOG%\" echo [ERROR] Failed to copy packaged runtime executable into place.\r\n"
            << "  exit /b 1\r\n"
            << ")\r\n"
            << "robocopy \"%EXTRACTED%\" \"%TARGETDIR%\" *.dll *.manifest /NFL /NDL /NJH /NJS /NC /NS >nul\r\n"
            << "if errorlevel 8 (\r\n"
            << "  >> \"%LOG%\" echo [ERROR] DLL/runtime payload robocopy failed with errorlevel %%ERRORLEVEL%%.\r\n"
            << "  exit /b 1\r\n"
            << ")\r\n"
            << "if exist \"%EXTRACTED%\\assets\" robocopy \"%EXTRACTED%\\assets\" \"%TARGETDIR%\\assets\" /E /NFL /NDL /NJH /NJS /NC /NS >nul\r\n"
            << "if errorlevel 8 (\r\n"
            << "  >> \"%LOG%\" echo [ERROR] Asset robocopy failed with errorlevel %%ERRORLEVEL%%.\r\n"
            << "  exit /b 1\r\n"
            << ")\r\n"
            << ">> \"%LOG%\" echo [INFO] Packaged runtime files copied successfully.\r\n"
            << "rmdir /S /Q \"%EXTRACTED%\" >nul 2>&1\r\n"
            << "del /F /Q \"%ARCHIVE%\" >nul 2>&1\r\n"
            << "start \"\" /D \"%TARGETDIR%\" \"%TARGETEXE%\"\r\n"
            << ">> \"%LOG%\" echo [INFO] Restarted updated runtime.\r\n"
            << "del /F /Q \"%~f0\" >nul 2>&1\r\n";

        bat.close();

        const std::string command =
            "cmd.exe /C start \"\" /min " + system_detail::quote_shell_arg(script_path.string());

        if (std::system(command.c_str()) != 0)
        {
            system_detail::log_error("Failed to launch packaged runtime replacement batch.");
            return false;
        }

        std::exit(0);
#else
        const auto target_dir = target_binary.parent_path();
        const auto script_path = system_detail::make_temp_script_path("replace_runtime_zip");

        std::ofstream sh(script_path, std::ios::binary);
        if (!sh)
        {
            system_detail::log_error("Failed to create packaged runtime replacement script.");
            return false;
        }

        sh
            << "#!/bin/sh\n"
            << "EXTRACTED=" << system_detail::quote_shell_arg(extracted_runtime_dir.string()) << "\n"
            << "TARGETDIR=" << system_detail::quote_shell_arg(target_dir.string()) << "\n"
            << "ARCHIVE=" << system_detail::quote_shell_arg(package_archive.string()) << "\n"
            << "TARGETEXE=" << system_detail::quote_shell_arg(target_binary.string()) << "\n"
            << "i=0\n"
            << "while [ $i -lt 20 ]; do\n"
            << "  cp -R \"$EXTRACTED/.\" \"$TARGETDIR\" 2>/dev/null && break\n"
            << "  i=$((i+1))\n"
            << "  sleep 1\n"
            << "done\n"
            << "rm -rf \"$EXTRACTED\"\n"
            << "rm -f \"$ARCHIVE\"\n"
            << "cd \"$TARGETDIR\"\n"
            << "\"$TARGETEXE\" &\n"
            << "rm -f \"$0\"\n";

        sh.close();

        const std::string command =
            "chmod +x " + system_detail::quote_shell_arg(script_path.string())
            + " && " + system_detail::quote_shell_arg(script_path.string()) + " &";

        if (std::system(command.c_str()) != 0)
        {
            system_detail::log_error("Failed to launch packaged runtime replacement script.");
            return false;
        }

        std::exit(0);
#endif
    }

    bool replace_runtime_from_source_script(
        const std::filesystem::path& target_binary,
        const std::filesystem::path& built_runtime_dir,
        const std::filesystem::path& source_root,
        const std::filesystem::path& package_archive)
    {
        const auto built_binary = built_runtime_dir / target_binary.filename();
        if (!std::filesystem::exists(built_binary))
        {
            system_detail::log_error("Source replacement aborted because the built runtime binary is missing.");
            return false;
        }

#if defined(_WIN32)
        const auto target_dir = target_binary.parent_path();
        const auto script_path = system_detail::make_temp_script_path("replace_runtime_source");
        const auto handoff_log = target_dir / "epoch_update_handoff.log";

        std::ofstream bat(script_path, std::ios::binary);
        if (!bat)
        {
            system_detail::log_error("Failed to create source runtime replacement batch.");
            return false;
        }

        const auto source_assets_dir = built_runtime_dir / "assets";
        const auto target_assets_dir = target_dir / "assets";

        bat
            << "@echo off\r\n"
            << "setlocal\r\n"
            << "set \"BUILTDIR=" << built_runtime_dir.string() << "\"\r\n"
            << "set \"TARGETDIR=" << target_dir.string() << "\"\r\n"
            << "set \"TARGETEXE=" << target_binary.string() << "\"\r\n"
            << "set \"EXENAME=" << target_binary.filename().string() << "\"\r\n"
            << "set \"BUILTEXE=" << built_binary.string() << "\"\r\n"
            << "set \"SRCASSETS=" << source_assets_dir.string() << "\"\r\n"
            << "set \"DSTASSETS=" << target_assets_dir.string() << "\"\r\n"
            << "set \"SRCROOT=" << source_root.string() << "\"\r\n"
            << "set \"ARCHIVE=" << package_archive.string() << "\"\r\n"
            << "set \"LOG=" << handoff_log.string() << "\"\r\n"
            << "> \"%LOG%\" echo [INFO] Source runtime replacement started\r\n"
            << ">> \"%LOG%\" echo [INFO] TARGETEXE=%TARGETEXE%\r\n"
            << ">> \"%LOG%\" echo [INFO] BUILTEXE=%BUILTEXE%\r\n"
            << ">> \"%LOG%\" echo [INFO] BUILTDIR=%BUILTDIR%\r\n"
            << "for /L %%I in (1,1,60) do (\r\n"
            << "  del /F /Q \"%TARGETEXE%\" >nul 2>&1\r\n"
            << "  if exist \"%TARGETEXE%\" (\r\n"
            << "    >> \"%LOG%\" echo [INFO] Waiting for runtime unlock attempt %%I\r\n"
            << "    timeout /t 1 >nul\r\n"
            << "  ) else (\r\n"
            << "    goto exe_ready\r\n"
            << "  )\r\n"
            << ")\r\n"
            << ">> \"%LOG%\" echo [ERROR] Timed out waiting for target runtime executable to unlock.\r\n"
            << "exit /b 1\r\n"
            << ":exe_ready\r\n"
            << "copy /Y \"%BUILTEXE%\" \"%TARGETEXE%\" >nul\r\n"
            << "if errorlevel 1 (\r\n"
            << "  >> \"%LOG%\" echo [ERROR] Failed to copy source-built runtime executable into place.\r\n"
            << "  exit /b 1\r\n"
            << ")\r\n"
            << "robocopy \"%BUILTDIR%\" \"%TARGETDIR%\" *.dll *.manifest /NFL /NDL /NJH /NJS /NC /NS >nul\r\n"
            << "if errorlevel 8 (\r\n"
            << "  >> \"%LOG%\" echo [ERROR] Source DLL/runtime payload robocopy failed with errorlevel %%ERRORLEVEL%%.\r\n"
            << "  exit /b 1\r\n"
            << ")\r\n"
            << "if exist \"%SRCASSETS%\" robocopy \"%SRCASSETS%\" \"%DSTASSETS%\" /E /NFL /NDL /NJH /NJS /NC /NS >nul\r\n"
            << "if errorlevel 8 (\r\n"
            << "  >> \"%LOG%\" echo [ERROR] Source asset robocopy failed with errorlevel %%ERRORLEVEL%%.\r\n"
            << "  exit /b 1\r\n"
            << ")\r\n"
            << ">> \"%LOG%\" echo [INFO] Source runtime files copied successfully.\r\n"
            << "del /F /Q \"%ARCHIVE%\" >nul 2>&1\r\n"
            << "start \"\" /D \"%TARGETDIR%\" \"%TARGETEXE%\"\r\n"
            << ">> \"%LOG%\" echo [INFO] Restarted updated runtime.\r\n"
            << "rmdir /S /Q \"%SRCROOT%\" >nul 2>&1\r\n"
            << "del /F /Q \"%~f0\" >nul 2>&1\r\n";

        bat.close();

        const std::string command =
            "cmd.exe /C start \"\" /min " + system_detail::quote_shell_arg(script_path.string());

        if (std::system(command.c_str()) != 0)
        {
            system_detail::log_error("Failed to launch source runtime replacement batch.");
            return false;
        }

        std::exit(0);
#else
        const auto target_dir = target_binary.parent_path();
        const auto script_path = system_detail::make_temp_script_path("replace_runtime_source");

        std::ofstream sh(script_path, std::ios::binary);
        if (!sh)
        {
            system_detail::log_error("Failed to create source runtime replacement script.");
            return false;
        }

        sh
            << "#!/bin/sh\n"
            << "BUILTDIR=" << system_detail::quote_shell_arg(built_runtime_dir.string()) << "\n"
            << "TARGETDIR=" << system_detail::quote_shell_arg(target_dir.string()) << "\n"
            << "TARGETEXE=" << system_detail::quote_shell_arg(target_binary.string()) << "\n"
            << "BUILTEXE=" << system_detail::quote_shell_arg((built_runtime_dir / target_binary.filename()).string()) << "\n"
            << "SRCROOT=" << system_detail::quote_shell_arg(source_root.string()) << "\n"
            << "ARCHIVE=" << system_detail::quote_shell_arg(package_archive.string()) << "\n"
            << "i=0\n"
            << "while [ $i -lt 20 ]; do\n"
            << "  cp \"$BUILTEXE\" \"$TARGETEXE\" 2>/dev/null && break\n"
            << "  i=$((i+1))\n"
            << "  sleep 1\n"
            << "done\n"
            << "if [ -d "
            << system_detail::quote_shell_arg((built_runtime_dir / "assets").string())
            << " ]; then cp -R "
            << system_detail::quote_shell_arg(((built_runtime_dir / "assets") / ".").string()) << " "
            << system_detail::quote_shell_arg((target_dir / "assets").string()) << "; fi\n"
            << "rm -f \"$ARCHIVE\"\n"
            << "cd \"$TARGETDIR\"\n"
            << "\"$TARGETEXE\" &\n"
            << "rm -rf \"$SRCROOT\"\n"
            << "rm -f \"$0\"\n";

        sh.close();

        const std::string command =
            "chmod +x " + system_detail::quote_shell_arg(script_path.string())
            + " && " + system_detail::quote_shell_arg(script_path.string()) + " &";

        if (std::system(command.c_str()) != 0)
        {
            system_detail::log_error("Failed to launch source runtime replacement script.");
            return false;
        }

        std::exit(0);
#endif
    }

    bool replace_binary(
        const std::filesystem::path& target_binary,
        const std::filesystem::path& new_binary)
    {
        clean_up_build_files();
        return replace_binary_from_script(target_binary, new_binary);
    }

    system_detail::VersionCheckResult check_for_updates(
        const std::string& url,
        const std::string_view label = {},
        const std::string& local_version_override = {})
    {
        system_detail::VersionCheckResult result{};

        const auto tmp = system_detail::make_temp_download_path(
            label.empty() ? std::string_view{ "remote_version" } : label);

        std::error_code ec;
        std::filesystem::remove(tmp, ec);

        if (!download_file(url, tmp.string()))
            return result;

        const std::string downloaded = system_detail::read_text_file(tmp);
        std::filesystem::remove(tmp, ec);

        const std::string remote_version =
            system_detail::extract_version_string(downloaded);

        if (remote_version.empty())
        {
            system_detail::log_error("Could not parse remote version payload.");
            return result;
        }

        const std::string normalized_local =
            local_version_override.empty()
            ? system_detail::extract_version_string(PROJECT_VERSION)
            : system_detail::extract_version_string(local_version_override);

        if (normalized_local.empty())
        {
            system_detail::log_error("Could not parse local version payload.");
            return result;
        }

        const std::string prefix =
            label.empty()
            ? std::string{}
            : (std::string{ label } + " ");

        system_detail::log_info("" + prefix + "Local  : " + normalized_local);
        system_detail::log_info("" + prefix + "Remote : " + remote_version);

        result.ok = true;
        result.local = normalized_local;
        result.remote = remote_version;
        result.update_available =
            system_detail::compare_versions(normalized_local, remote_version) < 0;

        return result;
    }

    bool install_from_binary(const std::string& url)
    {
        const auto target_binary = system_detail::current_binary_path();
        const auto normalized_url =
            system_detail::lower_ascii(system_detail::strip_url_query_and_fragment(url));

        if (normalized_url.ends_with(".zip"))
        {
            const auto archive_path = system_detail::replacement_package_path(target_binary);
            const auto extract_dir = system_detail::replacement_extract_dir(target_binary);

            std::error_code ec;
            std::filesystem::remove_all(extract_dir, ec);

            if (!download_file(url, archive_path.string()))
                return false;

            if (!extract_archive(archive_path.string(), extract_dir.string()))
            {
                system_detail::log_error("Failed to extract update package.");
                return false;
            }

            return replace_runtime_from_script(target_binary, extract_dir, archive_path);
        }

        const auto new_binary = system_detail::replacement_binary_path(target_binary);

        if (!download_file(url, new_binary.string()))
            return false;

        return replace_binary(target_binary, new_binary);
    }

    export bool run_source_update_command(const UpdateChannel& channel, const bool recheck_source_version = true)
    {
        if (channel.source_url.empty())
        {
            system_detail::log_error("Source update URL is not configured.");
            return false;
        }

        const auto target_binary = system_detail::current_binary_path();
        const auto archive_path = system_detail::source_archive_path(target_binary);
        const auto staging_dir = system_detail::source_staging_dir(target_binary);
        const auto final_dir = system_detail::source_final_dir(target_binary);

        if (recheck_source_version && !channel.source_version_url.empty())
        {
            const std::string local_source_version =
                system_detail::read_local_source_version(final_dir);

            const auto source_status =
                check_for_updates(channel.source_version_url, "Source", local_source_version);

            if (source_status.ok && source_status.update_available)
                system_detail::log_info("A newer source snapshot is available on main.");
            else if (source_status.ok)
                system_detail::log_info("No newer source snapshot is currently available; continuing because source update was requested explicitly.");
        }

        std::error_code ec;
        std::filesystem::remove_all(staging_dir, ec);
        std::filesystem::remove_all(final_dir, ec);

        system_detail::log_info("Downloading latest source snapshot from main.");
        if (!download_file(channel.source_url, archive_path.string()))
            return false;

        if (!extract_archive(archive_path.string(), staging_dir.string()))
        {
            system_detail::log_error("Failed to extract source snapshot.");
            return false;
        }

        const std::filesystem::path extracted_root =
            system_detail::first_subdirectory(staging_dir);

        if (!extracted_root.empty())
        {
            std::filesystem::rename(extracted_root, final_dir, ec);
            if (ec)
            {
                ec.clear();

                std::filesystem::copy(
                    extracted_root,
                    final_dir,
                    std::filesystem::copy_options::recursive |
                    std::filesystem::copy_options::overwrite_existing,
                    ec);

                if (ec)
                {
                    system_detail::log_error("Failed to move extracted source snapshot into place.");
                    return false;
                }
            }
        }
        else
        {
            std::filesystem::rename(staging_dir, final_dir, ec);
            if (ec)
            {
                system_detail::log_error("Extracted source snapshot did not contain a project root.");
                return false;
            }
        }

        std::filesystem::remove_all(staging_dir, ec);

        system_detail::log_info("Source snapshot ready at: " + final_dir.string());

        if (!system_detail::build_runtime_from_source(final_dir, target_binary))
            return false;

        const auto built_runtime_dir = system_detail::source_runtime_output_dir(final_dir);
        const auto built_binary = system_detail::source_runtime_binary_path(final_dir, target_binary);

        if (!std::filesystem::exists(built_binary))
        {
            system_detail::log_error("Built runtime output is missing after source update.");
            return false;
        }

        system_detail::log_info("Replacing current runtime from rebuilt source output.");
        return replace_runtime_from_source_script(target_binary, built_runtime_dir, final_dir, archive_path);
    }

    export UpdateCommandResult run_update_command(
        const UpdateChannel& channel,
        const bool force)
    {
        cleanup_previous_update_artifacts();

        UpdateCommandResult result{};

        const auto packaged_status = check_for_updates(channel.version_url);
        if (!packaged_status.ok)
            return result;

        result.local_version = packaged_status.local;
        result.remote_version = packaged_status.remote;

        if (packaged_status.update_available)
        {
            result.packaged_update_available = true;
            result.update_available = true;

            if (!force)
            {
                result.force_required = true;
                return result;
            }

            result.update_performed = install_from_binary(channel.binary_url);
            return result;
        }

        if (!channel.source_version_url.empty())
        {
            const auto target_binary = system_detail::current_binary_path();
            const auto final_dir = system_detail::source_final_dir(target_binary);
            const auto local_source_version =
                system_detail::read_local_source_version(final_dir);

            const auto source_status =
                check_for_updates(channel.source_version_url, "Source", local_source_version);

            if (source_status.ok)
            {
                result.source_local_version = source_status.local;
                result.source_remote_version = source_status.remote;
                result.source_update_available = source_status.update_available;
            }

            if (source_status.ok && source_status.update_available)
            {
                result.update_available = true;

                if (!force)
                {
                    result.force_required = true;
                    return result;
                }

                system_detail::log_info("No newer packaged runtime is available. Falling back to source update from main.");
                result.update_performed = run_source_update_command(channel, false);
                return result;
            }
        }

        return result;
    }
}
