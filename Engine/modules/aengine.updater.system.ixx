/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 ***********************************************/
module;

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

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
                const auto uch = static_cast<unsigned char>(ch);
                if (uch < 0x20 || uch == 0x7F)
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

            const auto parse_defined_component = [&](const char* macro_name) -> std::string
                {
                    const std::regex component_regex(
                        std::string{ R"(\b)" } + macro_name + R"(\b(?:\s*=\s*|\s+)(\d+))",
                        std::regex::optimize);

                    std::smatch match;
                    if (!std::regex_search(text, match, component_regex))
                        return {};

                    return match[1].str();
                };

            const std::string defined_major = parse_defined_component("EPOCH_VERSION_MAJOR_VALUE");
            const std::string defined_minor = parse_defined_component("EPOCH_VERSION_MINOR_VALUE");
            const std::string defined_revision = parse_defined_component("EPOCH_VERSION_REVISION_VALUE");

            if (!defined_major.empty() && !defined_minor.empty() && !defined_revision.empty())
            {
                return std::to_string(parse_int_or_zero(defined_major))
                    + "."
                    + std::to_string(parse_int_or_zero(defined_minor))
                    + "."
                    + std::to_string(parse_int_or_zero(defined_revision));
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

#if defined(_WIN32)
        inline void append_log_line(
            const std::filesystem::path& log_path,
            const std::string& line);

        [[nodiscard]] inline bool run_process_hidden(
            const std::filesystem::path& executable,
            const std::vector<std::string>& args,
            const std::filesystem::path& working_directory,
            const std::filesystem::path& log_path,
            const bool wait_for_exit,
            int* const exit_code);

        [[nodiscard]] inline std::filesystem::path make_temp_download_path(
            const std::string_view stem);
#endif

        [[nodiscard]] inline std::string sanitize_path_component(std::string text)
        {
            for (char& ch : text)
            {
                const auto uch = static_cast<unsigned char>(ch);
                if (!std::isalnum(uch) && ch != '-' && ch != '_')
                    ch = '_';
            }

            while (!text.empty() && text.back() == '_')
                text.pop_back();

            if (text.empty())
                text = "default";

            return text;
        }

        [[nodiscard]] inline std::filesystem::path managed_tools_root()
        {
#if defined(_WIN32)
            const auto local_app_data = env_path("LOCALAPPDATA");
            if (!local_app_data.empty())
                return local_app_data / UPDATER_TOOLS_SUBDIR();
#endif

            std::error_code ec;
            auto temp_root = std::filesystem::temp_directory_path(ec);
            if (!ec && !temp_root.empty())
                return temp_root / UPDATER_TOOLS_SUBDIR();

            ec.clear();
            const auto cwd = std::filesystem::current_path(ec);
            if (!ec && !cwd.empty())
                return cwd / UPDATER_TOOLS_SUBDIR();

            return std::filesystem::path{ UPDATER_TOOLS_SUBDIR() };
        }

        [[nodiscard]] inline std::filesystem::path managed_work_root()
        {
#if defined(_WIN32)
            const auto local_app_data = env_path("LOCALAPPDATA");
            if (!local_app_data.empty())
                return local_app_data / UPDATER_WORK_SUBDIR();
#endif

            std::error_code ec;
            auto temp_root = std::filesystem::temp_directory_path(ec);
            if (!ec && !temp_root.empty())
                return temp_root / UPDATER_WORK_SUBDIR();

            ec.clear();
            const auto cwd = std::filesystem::current_path(ec);
            if (!ec && !cwd.empty())
                return cwd / UPDATER_WORK_SUBDIR();

            return std::filesystem::path{ UPDATER_WORK_SUBDIR() };
        }

        [[nodiscard]] inline std::string shorten_token(
            std::string text,
            const std::size_t max_length = 12)
        {
            text = sanitize_path_component(std::move(text));
            if (text.size() > max_length)
                text.resize(max_length);

            while (!text.empty() && text.back() == '_')
                text.pop_back();

            if (text.empty())
                text = "default";

            return text;
        }

        [[nodiscard]] inline std::string read_manifest_builtin_baseline(
            const std::filesystem::path& manifest_root)
        {
            const auto manifest_file = manifest_root / "vcpkg.json";
            const auto text = read_text_file(manifest_file);
            if (text.empty())
                return {};

            static const std::regex k_baseline_regex(
                R"REGEX("builtin-baseline"\s*:\s*"([0-9A-Fa-f]+)")REGEX",
                std::regex::optimize);

            std::smatch match;
            if (!std::regex_search(text, match, k_baseline_regex))
                return {};

            return lower_ascii(trim_ascii(match[1].str()));
        }

        [[nodiscard]] inline std::filesystem::path find_executable_on_path(
            const std::string_view executable_name)
        {
            std::string path_value;
#if defined(_WIN32)
            char* path_env = nullptr;
            std::size_t path_size = 0;
            if (_dupenv_s(&path_env, &path_size, "PATH") != 0
                || path_env == nullptr
                || *path_env == '\0')
            {
                if (path_env != nullptr)
                    std::free(path_env);
                return {};
            }

            path_value.assign(path_env);
            std::free(path_env);
#else
            const char* const path_env = std::getenv("PATH");
            if (path_env == nullptr || *path_env == '\0')
                return {};

            path_value.assign(path_env);
#endif

#if defined(_WIN32)
            constexpr char separator = ';';
#else
            constexpr char separator = ':';
#endif

            std::string current;
            for (const char ch : path_value)
            {
                if (ch == separator)
                {
                    if (!current.empty())
                    {
                        std::filesystem::path candidate =
                            std::filesystem::path{ current } / executable_name;
                        if (std::filesystem::exists(candidate))
                            return candidate;
                    }

                    current.clear();
                    continue;
                }

                current.push_back(ch);
            }

            if (!current.empty())
            {
                std::filesystem::path candidate =
                    std::filesystem::path{ current } / executable_name;
                if (std::filesystem::exists(candidate))
                    return candidate;
            }

            return {};
        }

        [[nodiscard]] inline std::string unescape_json_string_basic(std::string text)
        {
            std::string out;
            out.reserve(text.size());

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                if (text[i] != '\\' || (i + 1) >= text.size())
                {
                    out.push_back(text[i]);
                    continue;
                }

                const char next = text[++i];
                switch (next)
                {
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/'); break;
                case '"':  out.push_back('"'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                default:
                    out.push_back(next);
                    break;
                }
            }

            return out;
        }

        [[nodiscard]] inline std::string capture_process_output(
            const std::filesystem::path& executable,
            const std::vector<std::string>& args,
            const std::filesystem::path& working_directory,
            int* const exit_code = nullptr)
        {
#if defined(_WIN32)
            const auto temp_output =
                make_temp_download_path("proc_capture").replace_extension(".log");

            int local_exit_code = -1;
            const bool launched = run_process_hidden(
                executable,
                args,
                working_directory,
                temp_output,
                true,
                &local_exit_code);

            auto output = trim_ascii(read_text_file(temp_output));
            std::error_code ec;
            std::filesystem::remove(temp_output, ec);

            if (exit_code != nullptr)
                *exit_code = launched ? local_exit_code : -1;

            return launched ? output : std::string{};
#else
            (void)executable;
            (void)args;
            (void)working_directory;
            if (exit_code != nullptr)
                *exit_code = -1;
            return {};
#endif
        }

        [[nodiscard]] inline std::string query_latest_github_asset_url(
            const std::string& api_url,
            const std::string_view include_fragment,
            const std::string_view include_suffix,
            const std::string_view excluded_fragment,
            const std::filesystem::path& log_path)
        {
            const auto json_path =
                make_temp_download_path("release_asset").replace_extension(".json");

            const bool downloaded = download_file(api_url, json_path.string());
            const auto json = downloaded ? read_text_file(json_path) : std::string{};

            std::error_code ec;
            std::filesystem::remove(json_path, ec);

            if (json.empty())
            {
                append_log_line(log_path, "[ERROR] Failed to query GitHub release metadata: " + api_url);
                return {};
            }

            static const std::regex k_asset_regex(
                R"REGEX("browser_download_url"\s*:\s*"([^"]+)")REGEX",
                std::regex::optimize);

            std::string fallback_match;
            for (auto it = std::sregex_iterator(json.begin(), json.end(), k_asset_regex);
                it != std::sregex_iterator();
                ++it)
            {
                std::string candidate = unescape_json_string_basic((*it)[1].str());
                if (!include_fragment.empty()
                    && candidate.find(include_fragment) == std::string::npos)
                {
                    continue;
                }

                if (!include_suffix.empty()
                    && !candidate.ends_with(std::string{ include_suffix }))
                {
                    continue;
                }

                if (excluded_fragment.empty()
                    || candidate.find(excluded_fragment) == std::string::npos)
                {
                    return candidate;
                }

                if (fallback_match.empty())
                    fallback_match = std::move(candidate);
            }

            if (fallback_match.empty())
            {
                append_log_line(log_path, "[ERROR] No matching GitHub release asset was found.");
                return {};
            }

            return fallback_match;
        }

        [[nodiscard]] inline std::filesystem::path git_executable_in_root(
            const std::filesystem::path& root)
        {
#if defined(_WIN32)
            const std::array<std::filesystem::path, 4> candidates{
                root / "cmd/git.exe",
                root / "bin/git.exe",
                root / "mingw64/bin/git.exe",
                root / "usr/bin/git.exe"
            };

            for (const auto& candidate : candidates)
            {
                if (std::filesystem::exists(candidate))
                    return candidate;
            }
#else
            const auto candidate = root / "bin/git";
            if (std::filesystem::exists(candidate))
                return candidate;
#endif

            return {};
        }

        [[nodiscard]] inline std::filesystem::path find_or_prepare_git(
            const std::filesystem::path& log_path)
        {
#if defined(_WIN32)
            const auto configured = env_path("GIT_EXE_PATH");
            if (!configured.empty() && std::filesystem::exists(configured))
            {
                append_log_line(log_path, "[INFO] Using configured git: " + configured.string());
                return configured;
            }

            if (const auto from_path = find_executable_on_path("git.exe");
                !from_path.empty())
            {
                append_log_line(log_path, "[INFO] Using git from PATH: " + from_path.string());
                return from_path;
            }

            const auto program_files = env_path("ProgramFiles");
            const auto program_files_x86 = env_path("ProgramFiles(x86)");
            const std::array<std::filesystem::path, 6> common_candidates{
                program_files / "Git/cmd/git.exe",
                program_files / "Git/bin/git.exe",
                program_files / "Git/mingw64/bin/git.exe",
                program_files_x86 / "Git/cmd/git.exe",
                program_files_x86 / "Git/bin/git.exe",
                program_files_x86 / "Git/mingw64/bin/git.exe"
            };

            for (const auto& candidate : common_candidates)
            {
                if (!candidate.empty() && std::filesystem::exists(candidate))
                {
                    append_log_line(log_path, "[INFO] Using installed git: " + candidate.string());
                    return candidate;
                }
            }

            const auto tools_root = managed_tools_root();
            const auto managed_root = tools_root / "g";
            if (const auto managed_git = git_executable_in_root(managed_root);
                !managed_git.empty())
            {
                append_log_line(log_path, "[INFO] Using managed git: " + managed_git.string());
                return managed_git;
            }

            std::error_code ec;
            std::filesystem::create_directories(tools_root, ec);
            if (ec)
            {
                append_log_line(log_path, "[ERROR] Failed to create updater tools directory for git.");
                return {};
            }

            const auto archive_url = query_latest_github_asset_url(
                GIT_WINDOWS_RELEASE_API_URL(),
                "MinGit-",
                "-64-bit.zip",
                "busybox",
                log_path);
            if (archive_url.empty())
                return {};

            const auto archive_path =
                make_temp_download_path("git_mingit").replace_extension(".zip");
            const auto staging_dir = tools_root / "gx";

            std::filesystem::remove_all(staging_dir, ec);
            ec.clear();
            std::filesystem::remove_all(managed_root, ec);

            append_log_line(log_path, "[INFO] Downloading managed git.");
            if (!download_file(archive_url, archive_path.string()))
            {
                append_log_line(log_path, "[ERROR] Failed to download managed git archive.");
                return {};
            }

            if (!extract_archive(archive_path.string(), staging_dir.string()))
            {
                append_log_line(log_path, "[ERROR] Failed to extract managed git archive.");
                std::filesystem::remove(archive_path, ec);
                return {};
            }

            std::filesystem::create_directories(managed_root.parent_path(), ec);
            if (!ec)
                std::filesystem::rename(staging_dir, managed_root, ec);

            if (ec)
            {
                ec.clear();
                std::filesystem::copy(
                    staging_dir,
                    managed_root,
                    std::filesystem::copy_options::recursive
                    | std::filesystem::copy_options::overwrite_existing,
                    ec);
            }

            std::filesystem::remove(archive_path, ec);
            std::filesystem::remove_all(staging_dir, ec);

            if (ec)
            {
                append_log_line(log_path, "[ERROR] Failed to stage managed git.");
                return {};
            }

            if (const auto managed_git = git_executable_in_root(managed_root);
                !managed_git.empty())
            {
                append_log_line(log_path, "[INFO] Managed git ready at: " + managed_git.string());
                return managed_git;
            }

            append_log_line(log_path, "[ERROR] Managed git executable is missing after extraction.");
            return {};
#else
            return find_executable_on_path("git");
#endif
        }

        [[nodiscard]] inline std::string ensure_vcpkg_git_head(
            const std::filesystem::path& git_exe,
            const std::filesystem::path& vcpkg_root,
            const std::filesystem::path& log_path)
        {
            int git_exit = -1;
            auto head = capture_process_output(
                git_exe,
                { "rev-parse", "--verify", "HEAD" },
                vcpkg_root,
                &git_exit);

            if (git_exit == 0 && !head.empty())
                return lower_ascii(trim_ascii(std::move(head)));

            append_log_line(log_path, "[INFO] Initializing managed vcpkg git registry snapshot.");

            const std::array<std::vector<std::string>, 4> setup_steps{
                std::vector<std::string>{ "init" },
                std::vector<std::string>{ "config", "user.name", "Epoch Updater" },
                std::vector<std::string>{ "config", "user.email", "updater@epoch.local" },
                std::vector<std::string>{ "add", "--all" }
            };

            for (const auto& args : setup_steps)
            {
                int step_exit = -1;
                if (!run_process_hidden(
                    git_exe,
                    args,
                    vcpkg_root,
                    log_path,
                    true,
                    &step_exit) || step_exit != 0)
                {
                    append_log_line(log_path, "[ERROR] Failed to prepare the managed vcpkg git registry.");
                    return {};
                }
            }

            int commit_exit = -1;
            if (!run_process_hidden(
                git_exe,
                { "commit", "--no-gpg-sign", "-m", "Managed vcpkg registry snapshot" },
                vcpkg_root,
                log_path,
                true,
                &commit_exit))
            {
                append_log_line(log_path, "[ERROR] Failed to launch git commit for the managed vcpkg registry.");
                return {};
            }

            if (commit_exit != 0)
            {
                head = capture_process_output(
                    git_exe,
                    { "rev-parse", "--verify", "HEAD" },
                    vcpkg_root,
                    &git_exit);

                if (git_exit == 0 && !head.empty())
                    return lower_ascii(trim_ascii(std::move(head)));

                append_log_line(log_path, "[ERROR] Managed vcpkg git registry did not produce a usable HEAD revision.");
                return {};
            }

            head = capture_process_output(
                git_exe,
                { "rev-parse", "--verify", "HEAD" },
                vcpkg_root,
                &git_exit);

            if (git_exit == 0 && !head.empty())
                return lower_ascii(trim_ascii(std::move(head)));

            append_log_line(log_path, "[ERROR] Failed to resolve the managed vcpkg git HEAD revision.");
            return {};
        }

        [[nodiscard]] inline std::filesystem::path find_or_prepare_vcpkg(
            const std::filesystem::path& manifest_root,
            const std::filesystem::path& log_path)
        {
#if defined(_WIN32)
            const std::string baseline =
                read_manifest_builtin_baseline(manifest_root);
            const std::string resolved_ref =
                baseline.empty() ? std::string{ VCPKG_DEFAULT_REF } : baseline;
            const std::string safe_ref = shorten_token(resolved_ref);

            const auto tools_root = managed_tools_root();
            const auto managed_root = tools_root / ("v-" + safe_ref);
            const auto managed_exe = managed_root / VCPKG_EXECUTABLE_NAME();

            if (std::filesystem::exists(managed_exe))
            {
                log_info("Using managed vcpkg toolchain.");
                append_log_line(log_path, "[INFO] Using managed vcpkg: " + managed_root.string());
                return managed_exe;
            }

            std::error_code ec;
            std::filesystem::create_directories(tools_root, ec);
            if (ec)
            {
                log_error("Failed to create updater tools directory.");
                append_log_line(log_path, "[ERROR] Failed to create updater tools directory: " + tools_root.string());
                return {};
            }

            const auto archive_path =
                make_temp_download_path("vcpkg_" + safe_ref).replace_extension(".zip");
            const auto staging_dir = tools_root / ("vx-" + safe_ref);
            const auto bootstrap_script = managed_root / VCPKG_BOOTSTRAP_SCRIPT_NAME();

            log_info("Downloading managed vcpkg toolchain.");
            append_log_line(log_path, "[INFO] Preparing managed vcpkg ref: " + resolved_ref);
            append_log_line(log_path, "[INFO] Managed vcpkg target: " + managed_root.string());

            std::filesystem::remove_all(staging_dir, ec);
            ec.clear();
            std::filesystem::remove_all(managed_root, ec);

            if (!download_file(VCPKG_ARCHIVE_URL(resolved_ref), archive_path.string()))
            {
                append_log_line(log_path, "[ERROR] Failed to download managed vcpkg archive.");
                return {};
            }

            if (!extract_archive(archive_path.string(), staging_dir.string()))
            {
                append_log_line(log_path, "[ERROR] Failed to extract managed vcpkg archive.");
                std::filesystem::remove(archive_path, ec);
                return {};
            }

            std::filesystem::path extracted_root;
            for (const auto& entry : std::filesystem::directory_iterator(staging_dir, ec))
            {
                if (ec)
                    break;

                if (entry.is_directory())
                {
                    extracted_root = entry.path();
                    break;
                }
            }

            if (extracted_root.empty())
            {
                append_log_line(log_path, "[ERROR] Managed vcpkg archive did not contain an extracted root directory.");
                std::filesystem::remove(archive_path, ec);
                std::filesystem::remove_all(staging_dir, ec);
                return {};
            }

            ec.clear();
            std::filesystem::rename(extracted_root, managed_root, ec);
            if (ec)
            {
                ec.clear();
                std::filesystem::create_directories(managed_root.parent_path(), ec);
                if (ec)
                {
                    append_log_line(log_path, "[ERROR] Failed to prepare managed vcpkg destination directory.");
                    std::filesystem::remove(archive_path, ec);
                    std::filesystem::remove_all(staging_dir, ec);
                    return {};
                }

                std::filesystem::copy(
                    extracted_root,
                    managed_root,
                    std::filesystem::copy_options::recursive
                    | std::filesystem::copy_options::overwrite_existing,
                    ec);

                if (ec)
                {
                    append_log_line(log_path, "[ERROR] Failed to copy managed vcpkg files into place.");
                    std::filesystem::remove(archive_path, ec);
                    std::filesystem::remove_all(staging_dir, ec);
                    return {};
                }
            }

            std::filesystem::remove(archive_path, ec);
            std::filesystem::remove_all(staging_dir, ec);

            if (!std::filesystem::exists(bootstrap_script))
            {
                append_log_line(log_path, "[ERROR] Managed vcpkg bootstrap script is missing.");
                return {};
            }

            auto cmd = env_path("ComSpec");
            if (cmd.empty())
                cmd = std::filesystem::path{ "C:\\Windows\\System32\\cmd.exe" };

            int bootstrap_exit = -1;
            append_log_line(log_path, "[INFO] Bootstrapping managed vcpkg.");

            if (!run_process_hidden(
                cmd,
                {
                    "/C",
                    bootstrap_script.string(),
                    "-disableMetrics"
                },
                managed_root,
                log_path,
                true,
                &bootstrap_exit) || bootstrap_exit != 0)
            {
                append_log_line(
                    log_path,
                    "[ERROR] Managed vcpkg bootstrap failed with exit code " + std::to_string(bootstrap_exit));
                log_error("Managed vcpkg bootstrap failed. See epoch_source_update.log for details.");
                return {};
            }

            if (!std::filesystem::exists(managed_exe))
            {
                append_log_line(log_path, "[ERROR] Managed vcpkg executable is missing after bootstrap.");
                return {};
            }

            append_log_line(log_path, "[INFO] Managed vcpkg ready at: " + managed_exe.string());
            log_info("Managed vcpkg ready.");
            return managed_exe;
#else
            (void)manifest_root;
            (void)log_path;
            return {};
#endif
        }

        [[nodiscard]] inline bool prepare_manifest_for_managed_vcpkg_registry(
            const std::filesystem::path& manifest_root,
            const std::filesystem::path& vcpkg_root,
            const std::filesystem::path& log_path)
        {
            const auto manifest_file = manifest_root / "vcpkg.json";
            if (!std::filesystem::exists(manifest_file))
            {
                append_log_line(log_path, "[ERROR] Source manifest file is missing for vcpkg preparation.");
                return false;
            }

            const auto config_file = manifest_root / "vcpkg-configuration.json";
            std::error_code ec;
            std::filesystem::remove(config_file, ec);

            const auto git_exe = find_or_prepare_git(log_path);
            if (git_exe.empty())
            {
                append_log_line(log_path, "[ERROR] Could not locate or prepare git for the managed vcpkg registry.");
                return false;
            }

            auto manifest_text = read_text_file(manifest_file);
            if (manifest_text.empty())
            {
                append_log_line(log_path, "[ERROR] Failed to read the source manifest before vcpkg preparation.");
                return false;
            }

            const auto registry_head = ensure_vcpkg_git_head(git_exe, vcpkg_root, log_path);
            if (registry_head.empty())
            {
                append_log_line(log_path, "[ERROR] Failed to prepare the managed vcpkg registry revision.");
                return false;
            }

            static const std::regex k_baseline_regex(
                R"REGEX("builtin-baseline"\s*:\s*"[0-9A-Fa-f]+")REGEX",
                std::regex::optimize);
            const std::string baseline_line =
                "\"builtin-baseline\": \"" + registry_head + "\"";

            if (std::regex_search(manifest_text, k_baseline_regex))
            {
                manifest_text = std::regex_replace(
                    manifest_text,
                    k_baseline_regex,
                    baseline_line,
                    std::regex_constants::format_first_only);
            }
            else
            {
                const auto brace = manifest_text.find('{');
                if (brace == std::string::npos)
                {
                    append_log_line(log_path, "[ERROR] Source manifest is malformed and could not be updated.");
                    return false;
                }

                manifest_text.insert(brace + 1, "\n  " + baseline_line + ",");
            }

            {
                std::ofstream manifest_out(manifest_file, std::ios::binary | std::ios::trunc);
                if (!manifest_out)
                {
                    append_log_line(log_path, "[ERROR] Failed to rewrite the source manifest for managed vcpkg.");
                    return false;
                }

                manifest_out << manifest_text;
            }

            append_log_line(log_path, "[INFO] Reconfigured the source snapshot to use the managed vcpkg git registry.");
            return true;
        }

        [[nodiscard]] inline std::filesystem::path prepare_vcpkg_overlay_ports(
            const std::filesystem::path& vcpkg_root,
            const std::filesystem::path& log_path)
        {
            const auto source_glad_dir = vcpkg_root / "ports" / "glad";
            if (!std::filesystem::exists(source_glad_dir))
            {
                append_log_line(log_path, "[WARN] Managed vcpkg glad port was not found; skipping overlay patch.");
                return {};
            }

            const auto overlay_root = managed_tools_root() / "ov";
            const auto glad_overlay_dir = overlay_root / "glad";
            std::error_code ec;

            std::filesystem::remove_all(glad_overlay_dir, ec);
            ec.clear();
            std::filesystem::create_directories(overlay_root, ec);
            if (ec)
            {
                append_log_line(log_path, "[WARN] Failed to create the updater overlay-ports directory.");
                return {};
            }

            std::filesystem::copy(
                source_glad_dir,
                glad_overlay_dir,
                std::filesystem::copy_options::recursive
                | std::filesystem::copy_options::overwrite_existing,
                ec);
            if (ec)
            {
                append_log_line(log_path, "[WARN] Failed to stage the glad overlay port.");
                return {};
            }

            const auto portfile = glad_overlay_dir / "portfile.cmake";
            auto portfile_text = read_text_file(portfile);
            if (portfile_text.empty())
            {
                append_log_line(log_path, "[WARN] Failed to read the staged glad portfile.");
                return {};
            }

            if (portfile_text.find("CMAKE_POLICY_VERSION_MINIMUM=3.5") == std::string::npos)
            {
                const std::string option_line =
                    "        -DCMAKE_POLICY_VERSION_MINIMUM=3.5\n";
                const auto options_pos = portfile_text.find("\n    OPTIONS\n");
                if (options_pos != std::string::npos)
                {
                    portfile_text.insert(options_pos + std::string{ "\n    OPTIONS\n" }.size(), option_line);
                }
                else
                {
                    const auto configure_pos = portfile_text.find("vcpkg_cmake_configure(");
                    if (configure_pos == std::string::npos)
                    {
                        append_log_line(log_path, "[WARN] Could not patch the staged glad portfile.");
                        return {};
                    }

                    const auto line_break = portfile_text.find('\n', configure_pos);
                    if (line_break == std::string::npos)
                    {
                        append_log_line(log_path, "[WARN] Could not patch the staged glad portfile.");
                        return {};
                    }

                    portfile_text.insert(
                        line_break + 1,
                        "    OPTIONS\n"
                        "        -DCMAKE_POLICY_VERSION_MINIMUM=3.5\n");
                }

                std::ofstream out(portfile, std::ios::binary | std::ios::trunc);
                if (!out)
                {
                    append_log_line(log_path, "[WARN] Failed to write the staged glad overlay port.");
                    return {};
                }

                out << portfile_text;
            }

            append_log_line(log_path, "[INFO] Prepared a glad overlay port for modern CMake policy handling.");
            return overlay_root;
        }

#if defined(_WIN32)
        [[nodiscard]] inline std::wstring to_wide(const std::string& value)
        {
            if (value.empty())
                return {};

            const int required = MultiByteToWideChar(
                CP_UTF8,
                0,
                value.c_str(),
                -1,
                nullptr,
                0);

            if (required <= 0)
                return std::wstring(value.begin(), value.end());

            std::wstring wide(static_cast<std::size_t>(required - 1), L'\0');

            MultiByteToWideChar(
                CP_UTF8,
                0,
                value.c_str(),
                -1,
                wide.data(),
                required);

            return wide;
        }

        [[nodiscard]] inline std::string last_error_message(const char* prefix)
        {
            const DWORD error = GetLastError();
            std::error_code ec(static_cast<int>(error), std::system_category());
            return std::string{ prefix } + ": " + ec.message();
        }

        inline void append_log_line(
            const std::filesystem::path& log_path,
            const std::string& line)
        {
            if (log_path.empty())
                return;

            std::ofstream out(log_path, std::ios::binary | std::ios::app);
            if (!out)
                return;

            out << line << "\r\n";
        }

        [[nodiscard]] inline std::string build_command_line(
            const std::filesystem::path& executable,
            const std::vector<std::string>& args)
        {
            std::string command = quote_shell_arg(executable.string());

            for (const auto& arg : args)
            {
                command.push_back(' ');
                command += quote_shell_arg(arg);
            }

            return command;
        }

        [[nodiscard]] inline bool run_process_with_visibility(
            const std::filesystem::path& executable,
            const std::vector<std::string>& args,
            const std::filesystem::path& working_directory,
            const std::filesystem::path& log_path,
            const bool wait_for_exit,
            const bool hidden,
            int* const exit_code = nullptr)
        {
            if (exit_code != nullptr)
                *exit_code = -1;

            SECURITY_ATTRIBUTES sa{};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;

            HANDLE log_handle = INVALID_HANDLE_VALUE;
            if (!log_path.empty())
            {
                log_handle = CreateFileW(
                    log_path.wstring().c_str(),
                    FILE_APPEND_DATA,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    &sa,
                    OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    nullptr);

                if (log_handle == INVALID_HANDLE_VALUE)
                {
                    log_error(last_error_message("Failed to open updater process log"));
                    return false;
                }
            }

            STARTUPINFOW si{};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = hidden ? SW_HIDE : SW_SHOWNORMAL;

            if (log_handle != INVALID_HANDLE_VALUE)
            {
                si.dwFlags |= STARTF_USESTDHANDLES;
                si.hStdOutput = log_handle;
                si.hStdError = log_handle;
                si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            }

            PROCESS_INFORMATION pi{};

            std::string command_line = build_command_line(executable, args);
            std::wstring command_line_wide = to_wide(command_line);
            std::wstring working_directory_wide =
                working_directory.empty() ? std::wstring{} : working_directory.wstring();

            const BOOL created = CreateProcessW(
                executable.wstring().c_str(),
                command_line_wide.data(),
                nullptr,
                nullptr,
                log_handle != INVALID_HANDLE_VALUE ? TRUE : FALSE,
                hidden ? CREATE_NO_WINDOW : 0,
                nullptr,
                working_directory.empty() ? nullptr : working_directory_wide.c_str(),
                &si,
                &pi);

            if (log_handle != INVALID_HANDLE_VALUE)
                CloseHandle(log_handle);

            if (!created)
            {
                log_error(last_error_message("Failed to launch updater process"));
                return false;
            }

            if (wait_for_exit)
            {
                WaitForSingleObject(pi.hProcess, INFINITE);

                DWORD process_exit_code = 0;
                if (!GetExitCodeProcess(pi.hProcess, &process_exit_code))
                    process_exit_code = static_cast<DWORD>(-1);

                if (exit_code != nullptr)
                    *exit_code = static_cast<int>(process_exit_code);
            }

            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return true;
        }

        [[nodiscard]] inline bool run_process_hidden(
            const std::filesystem::path& executable,
            const std::vector<std::string>& args,
            const std::filesystem::path& working_directory,
            const std::filesystem::path& log_path,
            const bool wait_for_exit,
            int* const exit_code = nullptr)
        {
            return run_process_with_visibility(
                executable,
                args,
                working_directory,
                log_path,
                wait_for_exit,
                true,
                exit_code);
        }

        [[nodiscard]] inline bool launch_batch_hidden(const std::filesystem::path& script_path)
        {
            auto cmd = env_path("ComSpec");
            if (cmd.empty())
                cmd = std::filesystem::path{ "C:\\Windows\\System32\\cmd.exe" };

            return run_process_hidden(
                cmd,
                { "/C", script_path.string() },
                script_path.parent_path(),
                {},
                false,
                nullptr);
        }
#endif

        [[nodiscard]] inline std::filesystem::path current_binary_path()
        {
            std::error_code ec;

#if defined(_WIN32)
            std::wstring module_path(MAX_PATH, L'\0');
            for (;;)
            {
                const DWORD copied = GetModuleFileNameW(
                    nullptr,
                    module_path.data(),
                    static_cast<DWORD>(module_path.size()));

                if (copied == 0)
                    break;

                if (copied < module_path.size() - 1)
                {
                    module_path.resize(copied);
                    return std::filesystem::path{ module_path }.lexically_normal();
                }

                module_path.resize(module_path.size() * 2);
            }
#endif

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
            return (managed_work_root() / shorten_token(target_binary.stem().string(), 10)) / "src.zip";
        }

        [[nodiscard]] inline std::filesystem::path source_staging_dir(const std::filesystem::path& target_binary)
        {
            return (managed_work_root() / shorten_token(target_binary.stem().string(), 10)) / "sx";
        }

        [[nodiscard]] inline std::filesystem::path source_final_dir(const std::filesystem::path& target_binary)
        {
            return (managed_work_root() / shorten_token(target_binary.stem().string(), 10)) / "src";
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

#if defined(_WIN32)
        [[nodiscard]] inline std::filesystem::path make_temp_powershell_script_path(const std::string_view stem)
        {
            return make_temp_download_path(stem).replace_extension(".ps1");
        }

        [[nodiscard]] inline std::string powershell_escape_single_quoted(const std::string_view text)
        {
            std::string out;
            out.reserve(text.size() + 8);
            for (const char ch : text)
            {
                if (ch == '\'')
                    out += "''";
                else
                    out.push_back(ch);
            }
            return out;
        }

        [[nodiscard]] inline bool launch_powershell_script(
            const std::filesystem::path& script_path,
            const bool hidden)
        {
            auto powershell = env_path("SystemRoot") / "System32/WindowsPowerShell/v1.0/powershell.exe";
            if (powershell.empty() || !std::filesystem::exists(powershell))
                powershell = std::filesystem::path{ "powershell.exe" };

            return run_process_with_visibility(
                powershell,
                {
                    "-NoLogo",
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    script_path.string()
                },
                script_path.parent_path(),
                {},
                false,
                hidden,
                nullptr);
        }
#endif

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
                int query_exit = -1;
                const auto text = capture_process_output(
                    vswhere,
                    {
                        "-latest",
                        "-requires",
                        "Microsoft.Component.MSBuild",
                        "-find",
                        "MSBuild\\**\\Bin\\MSBuild.exe"
                    },
                    vswhere.parent_path(),
                    &query_exit);

                if (query_exit == 0)
                {
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
            const auto build_log = target_binary.parent_path() / "epoch_source_update.log";

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

            const auto vcpkg_exe = find_or_prepare_vcpkg(manifest_root, build_log);
            if (vcpkg_exe.empty())
            {
                log_error("Could not prepare vcpkg for the source update.");
                return false;
            }

            const auto vcpkg_root = vcpkg_exe.parent_path();
            append_log_line(build_log, "[INFO] Source update build started");
            append_log_line(build_log, "[INFO] Source root: " + source_root.string());
            append_log_line(build_log, "[INFO] Manifest root: " + manifest_root.string());
            append_log_line(build_log, "[INFO] vcpkg root: " + vcpkg_root.string());
            append_log_line(build_log, "[INFO] MSBuild: " + msbuild.string());

            if (!prepare_manifest_for_managed_vcpkg_registry(
                manifest_root,
                vcpkg_root,
                build_log))
            {
                log_error("Could not prepare the source snapshot for managed vcpkg.");
                return false;
            }

            const auto overlay_ports = prepare_vcpkg_overlay_ports(vcpkg_root, build_log);

            log_info("Building updated runtime from source.");
            log_info("Restoring source dependencies with vcpkg.");
            append_log_line(build_log, "[INFO] Restoring source dependencies with vcpkg.");

            std::vector<std::string> vcpkg_args{
                "install",
                "--triplet",
                SOURCE_BUILD_PLATFORM() + "-windows",
                "--x-manifest-root=" + manifest_root.string(),
                "--x-builtin-ports-root=" + (vcpkg_root / "ports").string(),
                "--x-builtin-registry-versions-dir=" + (vcpkg_root / "versions").string()
            };
            if (!overlay_ports.empty())
                vcpkg_args.push_back("--overlay-ports=" + overlay_ports.string());

            int vcpkg_exit = -1;
            if (!run_process_hidden(
                vcpkg_exe,
                vcpkg_args,
                manifest_root,
                build_log,
                true,
                &vcpkg_exit) || vcpkg_exit != 0)
            {
                append_log_line(
                    build_log,
                    "[ERROR] vcpkg dependency restore failed with exit code " + std::to_string(vcpkg_exit));
                log_error("vcpkg dependency restore failed. See epoch_source_update.log for details.");
                return false;
            }

            log_info("MSBuild: " + msbuild.string());
            append_log_line(build_log, "[INFO] Building updated runtime with MSBuild.");

            const std::vector<std::string> msbuild_args{
                solution.string(),
                "/t:" + SOURCE_BUILD_TARGET(),
                "/p:Configuration=" + SOURCE_BUILD_CONFIGURATION(),
                "/p:Platform=" + SOURCE_BUILD_PLATFORM(),
                "/p:UseMultiToolTask=false",
                "/m:1",
                "/clp:ErrorsOnly"
            };

            bool build_ok = false;
            for (int attempt = 1; attempt <= 2; ++attempt)
            {
                append_log_line(
                    build_log,
                    "[INFO] MSBuild attempt " + std::to_string(attempt) + " started.");

                int msbuild_exit = -1;
                const bool launched = run_process_hidden(
                    msbuild,
                    msbuild_args,
                    source_root,
                    build_log,
                    true,
                    &msbuild_exit);

                if (launched && msbuild_exit == 0)
                {
                    build_ok = true;
                    append_log_line(
                        build_log,
                        "[INFO] MSBuild attempt " + std::to_string(attempt) + " completed successfully.");
                    break;
                }

                append_log_line(
                    build_log,
                    "[WARN] MSBuild attempt " + std::to_string(attempt)
                    + " failed with exit code " + std::to_string(msbuild_exit) + ".");

                if (attempt == 1)
                {
                    append_log_line(
                        build_log,
                        "[INFO] Retrying the source build once after dependency restore.");
                }
            }

            if (!build_ok)
            {
                append_log_line(build_log, "[ERROR] Source build failed after two attempts.");
                log_error("Source build failed. See epoch_source_update.log for details.");
                return false;
            }

            const auto built_binary = source_runtime_binary_path(source_root, target_binary);
            if (!std::filesystem::exists(built_binary))
            {
                append_log_line(build_log, "[ERROR] Source build completed without a runtime binary.");
                log_error("Source build completed without producing the runtime binary.");
                return false;
            }

            append_log_line(build_log, "[INFO] Source build completed successfully.");
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

        [[nodiscard]] inline bool env_flag_enabled(const char* name)
        {
            const auto value = lower_ascii(trim_ascii(env_path(name).string()));
            return value == "1"
                || value == "true"
                || value == "yes"
                || value == "on";
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

#if defined(_WIN32)
    [[nodiscard]] inline bool launch_source_update_worker(
        const UpdateChannel& channel,
        const std::filesystem::path& target_binary,
        const bool silent_worker)
    {
        if (channel.source_url.empty())
        {
            system_detail::log_error("Source update URL is not configured.");
            return false;
        }

        const auto msbuild = system_detail::find_msbuild_path();
        if (msbuild.empty())
        {
            system_detail::log_error("Could not locate MSBuild. Install Visual Studio Build Tools or set MSBUILD_EXE_PATH.");
            return false;
        }

        const auto archive_path = system_detail::source_archive_path(target_binary);
        const auto staging_dir = system_detail::source_staging_dir(target_binary);
        const auto final_dir = system_detail::source_final_dir(target_binary);
        const auto target_dir = target_binary.parent_path();
        const auto build_log = target_dir / "epoch_source_update.log";
        const auto handoff_log = target_dir / "epoch_update_handoff.log";
        const auto worker_script = system_detail::make_temp_powershell_script_path("source_update_worker");
        const auto built_runtime_dir = system_detail::source_runtime_output_dir(final_dir);
        const auto built_binary = system_detail::source_runtime_binary_path(final_dir, target_binary);
        const auto manifest_root = system_detail::source_manifest_root(final_dir);
        const auto solution = system_detail::source_solution_path(final_dir);
        const auto target_assets_dir = target_dir / "assets";
        const auto built_assets_dir = built_runtime_dir / "assets";
        const auto source_repo_assets_dir = manifest_root / "assets";
        const auto managed_tools_root = system_detail::managed_tools_root();

        std::ofstream ps(worker_script, std::ios::binary);
        if (!ps)
        {
            system_detail::log_error("Failed to create source update worker script.");
            return false;
        }

        const auto triplet = SOURCE_BUILD_PLATFORM() + std::string{ "-windows" };
        const auto esc = [](const std::string& value)
            {
                return system_detail::powershell_escape_single_quoted(value);
            };

        ps
            << "$ErrorActionPreference = 'Stop'\n"
            << "$ProgressPreference = 'SilentlyContinue'\n"
            << "$sourceUrl = '" << esc(channel.source_url) << "'\n"
            << "$sourceArchive = '" << esc(archive_path.string()) << "'\n"
            << "$stagingDir = '" << esc(staging_dir.string()) << "'\n"
            << "$sourceRoot = '" << esc(final_dir.string()) << "'\n"
            << "$manifestRoot = '" << esc(manifest_root.string()) << "'\n"
            << "$solution = '" << esc(solution.string()) << "'\n"
            << "$buildLog = '" << esc(build_log.string()) << "'\n"
            << "$handoffLog = '" << esc(handoff_log.string()) << "'\n"
            << "$targetExe = '" << esc(target_binary.string()) << "'\n"
            << "$targetDir = '" << esc(target_dir.string()) << "'\n"
            << "$targetAssetsDir = '" << esc(target_assets_dir.string()) << "'\n"
            << "$builtDir = '" << esc(built_runtime_dir.string()) << "'\n"
            << "$builtExe = '" << esc(built_binary.string()) << "'\n"
            << "$builtAssetsDir = '" << esc(built_assets_dir.string()) << "'\n"
            << "$sourceRepoAssetsDir = '" << esc(source_repo_assets_dir.string()) << "'\n"
            << "$managedToolsRoot = '" << esc(managed_tools_root.string()) << "'\n"
            << "$vcpkgDefaultRef = '" << esc(std::string{ VCPKG_DEFAULT_REF }) << "'\n"
            << "$vcpkgDefaultArchiveUrl = '" << esc(VCPKG_ARCHIVE_URL(VCPKG_DEFAULT_REF)) << "'\n"
            << "$vcpkgArchiveBaseUrl = '" << esc(VCPKG_ARCHIVE_BASE_URL()) << "'\n"
            << "$vcpkgExeName = '" << esc(VCPKG_EXECUTABLE_NAME()) << "'\n"
            << "$vcpkgBootstrapName = '" << esc(VCPKG_BOOTSTRAP_SCRIPT_NAME()) << "'\n"
            << "$gitReleaseApiUrl = '" << esc(GIT_WINDOWS_RELEASE_API_URL()) << "'\n"
            << "$msbuildExe = '" << esc(msbuild.string()) << "'\n"
            << "$triplet = '" << esc(triplet) << "'\n"
            << "$buildTarget = '" << esc(SOURCE_BUILD_TARGET()) << "'\n"
            << "$buildConfiguration = '" << esc(SOURCE_BUILD_CONFIGURATION()) << "'\n"
            << "$buildPlatform = '" << esc(SOURCE_BUILD_PLATFORM()) << "'\n"
            << "$workerPath = $MyInvocation.MyCommand.Path\n"
            << "$utf8NoBom = New-Object System.Text.UTF8Encoding($false)\n"
            << "function Append-Text([string]$Path, [string]$Text) {\n"
            << "  if ([string]::IsNullOrEmpty($Path) -or [string]::IsNullOrEmpty($Text)) {\n"
            << "    return\n"
            << "  }\n"
            << "  [System.IO.File]::AppendAllText($Path, $Text, $utf8NoBom)\n"
            << "}\n"
            << "function Get-SafeToken([string]$Value) {\n"
            << "  if ([string]::IsNullOrWhiteSpace($Value)) {\n"
            << "    return 'default'\n"
            << "  }\n"
            << "  $chars = $Value.ToCharArray() | ForEach-Object {\n"
            << "    if ([char]::IsLetterOrDigit($_) -or $_ -eq '-' -or $_ -eq '_') { $_ } else { '_' }\n"
            << "  }\n"
            << "  $safe = -join $chars\n"
            << "  $safe = $safe.TrimEnd('_')\n"
            << "  if ([string]::IsNullOrWhiteSpace($safe)) {\n"
            << "    return 'default'\n"
            << "  }\n"
            << "  return $safe\n"
            << "}\n"
            << "function Get-ShortToken([string]$Value, [int]$MaxLength = 12) {\n"
            << "  $safe = Get-SafeToken $Value\n"
            << "  if ($safe.Length -gt $MaxLength) {\n"
            << "    $safe = $safe.Substring(0, $MaxLength)\n"
            << "  }\n"
            << "  return $safe.TrimEnd('_')\n"
            << "}\n"
            << "function Get-VcpkgRef {\n"
            << "  $manifestFile = Join-Path $manifestRoot 'vcpkg.json'\n"
            << "  if (-not (Test-Path -LiteralPath $manifestFile)) {\n"
            << "    return $vcpkgDefaultRef\n"
            << "  }\n"
            << "  $text = Get-Content -LiteralPath $manifestFile -Raw -ErrorAction SilentlyContinue\n"
            << "  if ([string]::IsNullOrWhiteSpace($text)) {\n"
            << "    return $vcpkgDefaultRef\n"
            << "  }\n"
            << "  $match = [regex]::Match($text, '\"builtin-baseline\"\\s*:\\s*\"([0-9A-Fa-f]+)\"')\n"
            << "  if ($match.Success) {\n"
            << "    return $match.Groups[1].Value.ToLowerInvariant()\n"
            << "  }\n"
            << "  return $vcpkgDefaultRef\n"
            << "}\n"
            << "function Get-VcpkgArchiveUrl([string]$Ref) {\n"
            << "  if ($Ref -eq $vcpkgDefaultRef) {\n"
            << "    return $vcpkgDefaultArchiveUrl\n"
            << "  }\n"
            << "  return ($vcpkgArchiveBaseUrl + $Ref + '.zip')\n"
            << "}\n"
            << "function Get-ReleaseAssetUrl([string]$ApiUrl, [string]$IncludeFragment, [string]$IncludeSuffix, [string]$ExcludeFragment) {\n"
            << "  $headers = @{ 'User-Agent' = 'EpochUpdater/1.0'; 'Accept' = 'application/vnd.github+json' }\n"
            << "  $release = Invoke-RestMethod -Headers $headers -Uri $ApiUrl -UseBasicParsing\n"
            << "  $fallback = $null\n"
            << "  foreach ($asset in $release.assets) {\n"
            << "    if ($null -eq $asset) { continue }\n"
            << "    $url = [string]$asset.browser_download_url\n"
            << "    if ([string]::IsNullOrWhiteSpace($url)) { continue }\n"
            << "    if (-not [string]::IsNullOrWhiteSpace($IncludeFragment) -and -not $url.Contains($IncludeFragment)) { continue }\n"
            << "    if (-not [string]::IsNullOrWhiteSpace($IncludeSuffix) -and -not $url.EndsWith($IncludeSuffix)) { continue }\n"
            << "    if ([string]::IsNullOrWhiteSpace($ExcludeFragment) -or -not $url.Contains($ExcludeFragment)) {\n"
            << "      return $url\n"
            << "    }\n"
            << "    if ($null -eq $fallback) { $fallback = $url }\n"
            << "  }\n"
            << "  return $fallback\n"
            << "}\n"
            << "function Append-FileToBuildLog([string]$Path) {\n"
            << "  if (-not (Test-Path -LiteralPath $Path)) {\n"
            << "    return\n"
            << "  }\n"
            << "  $text = Get-Content -LiteralPath $Path -Raw -ErrorAction SilentlyContinue\n"
            << "  if ([string]::IsNullOrEmpty($text)) {\n"
            << "    return\n"
            << "  }\n"
            << "  if (-not $text.EndsWith([Environment]::NewLine)) {\n"
            << "    $text += [Environment]::NewLine\n"
            << "  }\n"
            << "  Append-Text $buildLog $text\n"
            << "}\n"
            << "function Write-Step([string]$Level, [string]$Message) {\n"
            << "  $line = \"$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') [$Level] $Message\"\n"
            << "  Write-Host $line\n"
            << "  Append-Text $buildLog ($line + [Environment]::NewLine)\n"
            << "}\n"
            << "function Write-Handoff([string]$Level, [string]$Message) {\n"
            << "  $line = \"$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') [$Level] $Message\"\n"
            << "  Write-Host $line\n"
            << "  Append-Text $handoffLog ($line + [Environment]::NewLine)\n"
            << "}\n"
            << "function Invoke-Tool([string]$FilePath, [string[]]$Arguments, [string]$WorkingDir, [string]$StepName) {\n"
            << "  $toolLog = [System.IO.Path]::GetTempFileName()\n"
            << "  Push-Location $WorkingDir\n"
            << "  try {\n"
            << "    & $FilePath @Arguments *> $toolLog\n"
            << "    $toolExitCode = $LASTEXITCODE\n"
            << "    Append-FileToBuildLog $toolLog\n"
            << "    if ($toolExitCode -ne 0) {\n"
            << "      throw ($StepName + ' failed with exit code ' + $toolExitCode + '.')\n"
            << "    }\n"
            << "  }\n"
            << "  finally {\n"
            << "    Pop-Location\n"
            << "    Remove-Item -LiteralPath $toolLog -Force -ErrorAction SilentlyContinue\n"
            << "  }\n"
            << "}\n"
            << "function Resolve-GitExe {\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($env:GIT_EXE_PATH) -and (Test-Path -LiteralPath $env:GIT_EXE_PATH)) {\n"
            << "    Write-Step 'INFO' ('Using configured git: ' + $env:GIT_EXE_PATH)\n"
            << "    return $env:GIT_EXE_PATH\n"
            << "  }\n"
            << "  $gitCommand = Get-Command git.exe -ErrorAction SilentlyContinue\n"
            << "  if ($null -ne $gitCommand -and -not [string]::IsNullOrWhiteSpace($gitCommand.Source) -and (Test-Path -LiteralPath $gitCommand.Source)) {\n"
            << "    Write-Step 'INFO' ('Using git from PATH: ' + $gitCommand.Source)\n"
            << "    return $gitCommand.Source\n"
            << "  }\n"
            << "  $commonCandidates = @(\n"
            << "    (Join-Path $env:ProgramFiles 'Git\\cmd\\git.exe'),\n"
            << "    (Join-Path $env:ProgramFiles 'Git\\bin\\git.exe'),\n"
            << "    (Join-Path $env:ProgramFiles 'Git\\mingw64\\bin\\git.exe'),\n"
            << "    (Join-Path ${env:ProgramFiles(x86)} 'Git\\cmd\\git.exe'),\n"
            << "    (Join-Path ${env:ProgramFiles(x86)} 'Git\\bin\\git.exe'),\n"
            << "    (Join-Path ${env:ProgramFiles(x86)} 'Git\\mingw64\\bin\\git.exe')\n"
            << "  )\n"
            << "  foreach ($candidate in $commonCandidates) {\n"
            << "    if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate)) {\n"
            << "      Write-Step 'INFO' ('Using installed git: ' + $candidate)\n"
            << "      return $candidate\n"
            << "    }\n"
            << "  }\n"
            << "  $managedGitRoot = Join-Path $managedToolsRoot 'g'\n"
            << "  $managedGitCandidates = @(\n"
            << "    (Join-Path $managedGitRoot 'cmd\\git.exe'),\n"
            << "    (Join-Path $managedGitRoot 'bin\\git.exe'),\n"
            << "    (Join-Path $managedGitRoot 'mingw64\\bin\\git.exe'),\n"
            << "    (Join-Path $managedGitRoot 'usr\\bin\\git.exe')\n"
            << "  )\n"
            << "  foreach ($candidate in $managedGitCandidates) {\n"
            << "    if (Test-Path -LiteralPath $candidate) {\n"
            << "      Write-Step 'INFO' ('Using managed git: ' + $candidate)\n"
            << "      return $candidate\n"
            << "    }\n"
            << "  }\n"
            << "  New-Item -ItemType Directory -Path $managedToolsRoot -Force | Out-Null\n"
            << "  $gitArchiveUrl = Get-ReleaseAssetUrl $gitReleaseApiUrl 'MinGit-' '-64-bit.zip' 'busybox'\n"
            << "  if ([string]::IsNullOrWhiteSpace($gitArchiveUrl)) {\n"
            << "    throw 'Could not locate a managed Git release asset.'\n"
            << "  }\n"
            << "  $gitArchive = Join-Path $managedToolsRoot 'git.zip'\n"
            << "  $gitStaging = Join-Path $managedToolsRoot 'gx'\n"
            << "  Remove-Item -LiteralPath $gitArchive -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $gitStaging -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $managedGitRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Write-Step 'INFO' 'Downloading managed git.'\n"
            << "  $headers = @{ 'User-Agent' = 'EpochUpdater/1.0' }\n"
            << "  Invoke-WebRequest -UseBasicParsing -Headers $headers -Uri $gitArchiveUrl -OutFile $gitArchive\n"
            << "  Expand-Archive -LiteralPath $gitArchive -DestinationPath $gitStaging -Force\n"
            << "  Move-Item -LiteralPath $gitStaging -Destination $managedGitRoot -Force\n"
            << "  Remove-Item -LiteralPath $gitArchive -Force -ErrorAction SilentlyContinue\n"
            << "  foreach ($candidate in $managedGitCandidates) {\n"
            << "    if (Test-Path -LiteralPath $candidate) {\n"
            << "      Write-Step 'INFO' ('Managed git ready at: ' + $candidate)\n"
            << "      return $candidate\n"
            << "    }\n"
            << "  }\n"
            << "  throw 'Managed git extraction did not produce git.exe.'\n"
            << "}\n"
            << "function Resolve-VcpkgExe {\n"
            << "  $vcpkgRef = Get-VcpkgRef\n"
            << "  $safeRef = Get-ShortToken $vcpkgRef\n"
            << "  $managedVcpkgRoot = Join-Path $managedToolsRoot ('v-' + $safeRef)\n"
            << "  $managedVcpkgExe = Join-Path $managedVcpkgRoot $vcpkgExeName\n"
            << "  if (Test-Path -LiteralPath $managedVcpkgExe) {\n"
            << "    Write-Step 'INFO' ('Using managed vcpkg: ' + $managedVcpkgRoot)\n"
            << "    return $managedVcpkgExe\n"
            << "  }\n"
            << "  $vcpkgArchive = Join-Path $managedToolsRoot ('v-' + $safeRef + '.zip')\n"
            << "  $vcpkgStaging = Join-Path $managedToolsRoot ('vx-' + $safeRef)\n"
            << "  $bootstrapScript = Join-Path $managedVcpkgRoot $vcpkgBootstrapName\n"
            << "  New-Item -ItemType Directory -Path $managedToolsRoot -Force | Out-Null\n"
            << "  Remove-Item -LiteralPath $vcpkgArchive -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $vcpkgStaging -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $managedVcpkgRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Write-Step 'INFO' ('Downloading managed vcpkg (' + $vcpkgRef + ').')\n"
            << "  $headers = @{ 'User-Agent' = 'EpochUpdater/1.0' }\n"
            << "  Invoke-WebRequest -UseBasicParsing -Headers $headers -Uri (Get-VcpkgArchiveUrl $vcpkgRef) -OutFile $vcpkgArchive\n"
            << "  Expand-Archive -LiteralPath $vcpkgArchive -DestinationPath $vcpkgStaging -Force\n"
            << "  $extractedRoot = Get-ChildItem -LiteralPath $vcpkgStaging -Directory | Select-Object -First 1\n"
            << "  if ($null -eq $extractedRoot) {\n"
            << "    throw 'Managed vcpkg archive did not extract correctly.'\n"
            << "  }\n"
            << "  Move-Item -LiteralPath $extractedRoot.FullName -Destination $managedVcpkgRoot -Force\n"
            << "  Remove-Item -LiteralPath $vcpkgArchive -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $vcpkgStaging -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Invoke-Tool $bootstrapScript @('-disableMetrics') $managedVcpkgRoot 'vcpkg bootstrap'\n"
            << "  if (-not (Test-Path -LiteralPath $managedVcpkgExe)) {\n"
            << "    throw 'Managed vcpkg bootstrap did not produce vcpkg.exe.'\n"
            << "  }\n"
            << "  Write-Step 'INFO' ('Managed vcpkg ready at: ' + $managedVcpkgExe)\n"
            << "  return $managedVcpkgExe\n"
            << "}\n"
            << "function Prepare-ManifestForManagedVcpkg([string]$ManifestRoot, [string]$VcpkgRoot) {\n"
            << "  $manifestFile = Join-Path $ManifestRoot 'vcpkg.json'\n"
            << "  if (-not (Test-Path -LiteralPath $manifestFile)) {\n"
            << "    throw 'Source manifest file is missing for managed vcpkg preparation.'\n"
            << "  }\n"
            << "  $configFile = Join-Path $ManifestRoot 'vcpkg-configuration.json'\n"
            << "  Remove-Item -LiteralPath $configFile -Force -ErrorAction SilentlyContinue\n"
            << "  $gitExe = Resolve-GitExe\n"
            << "  Push-Location $VcpkgRoot\n"
            << "  try {\n"
            << "    $head = ''\n"
            << "    try {\n"
            << "      $head = (& $gitExe 'rev-parse' '--verify' 'HEAD' 2>$null | Out-String).Trim()\n"
            << "    }\n"
            << "    catch {\n"
            << "      $head = ''\n"
            << "    }\n"
            << "    if ([string]::IsNullOrWhiteSpace($head)) {\n"
            << "      Write-Step 'INFO' 'Initializing managed vcpkg git registry snapshot.'\n"
            << "      Invoke-Tool $gitExe @('init') $VcpkgRoot 'git init'\n"
            << "      Invoke-Tool $gitExe @('config', 'user.name', 'Epoch Updater') $VcpkgRoot 'git config user.name'\n"
            << "      Invoke-Tool $gitExe @('config', 'user.email', 'updater@epoch.local') $VcpkgRoot 'git config user.email'\n"
            << "      Invoke-Tool $gitExe @('add', '--all') $VcpkgRoot 'git add'\n"
            << "      try {\n"
            << "        Invoke-Tool $gitExe @('commit', '--no-gpg-sign', '-m', 'Managed vcpkg registry snapshot') $VcpkgRoot 'git commit'\n"
            << "      }\n"
            << "      catch {\n"
            << "      }\n"
            << "      $head = (& $gitExe 'rev-parse' '--verify' 'HEAD' 2>$null | Out-String).Trim()\n"
            << "    }\n"
            << "  }\n"
            << "  finally {\n"
            << "    Pop-Location\n"
            << "  }\n"
            << "  if ([string]::IsNullOrWhiteSpace($head)) {\n"
            << "    throw 'Managed vcpkg git registry did not produce a usable HEAD revision.'\n"
            << "  }\n"
            << "  $head = $head.Trim().ToLowerInvariant()\n"
            << "  $manifestText = Get-Content -LiteralPath $manifestFile -Raw -ErrorAction Stop\n"
            << "  $baselineRegex = [regex]'\"builtin-baseline\"\\s*:\\s*\"[0-9A-Fa-f]+\"'\n"
            << "  if ($baselineRegex.IsMatch($manifestText)) {\n"
            << "    $manifestText = $baselineRegex.Replace($manifestText, ('\"builtin-baseline\": \"' + $head + '\"'), 1)\n"
            << "  }\n"
            << "  else {\n"
            << "    $braceIndex = $manifestText.IndexOf('{')\n"
            << "    if ($braceIndex -lt 0) {\n"
            << "      throw 'Source manifest is malformed and could not be updated.'\n"
            << "    }\n"
            << "    $manifestText = $manifestText.Insert($braceIndex + 1, [Environment]::NewLine + '  \"builtin-baseline\": \"' + $head + '\",')\n"
            << "  }\n"
            << "  [System.IO.File]::WriteAllText($manifestFile, $manifestText, $utf8NoBom)\n"
            << "  Write-Step 'INFO' 'Reconfigured the source snapshot to use the managed vcpkg git registry.'\n"
            << "}\n"
            << "function Prepare-GladOverlay([string]$VcpkgRoot) {\n"
            << "  $sourceGladDir = Join-Path $VcpkgRoot 'ports\\glad'\n"
            << "  if (-not (Test-Path -LiteralPath $sourceGladDir)) {\n"
            << "    return ''\n"
            << "  }\n"
            << "  $overlayRoot = Join-Path $managedToolsRoot 'ov'\n"
            << "  $gladOverlayDir = Join-Path $overlayRoot 'glad'\n"
            << "  Remove-Item -LiteralPath $gladOverlayDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  New-Item -ItemType Directory -Path $overlayRoot -Force | Out-Null\n"
            << "  Copy-Item -LiteralPath $sourceGladDir -Destination $gladOverlayDir -Recurse -Force\n"
            << "  $portfile = Join-Path $gladOverlayDir 'portfile.cmake'\n"
            << "  $portfileText = Get-Content -LiteralPath $portfile -Raw -ErrorAction Stop\n"
            << "  if (-not $portfileText.Contains('CMAKE_POLICY_VERSION_MINIMUM=3.5')) {\n"
            << "    $optionsMarker = [Environment]::NewLine + '    OPTIONS' + [Environment]::NewLine\n"
            << "    if ($portfileText.Contains($optionsMarker)) {\n"
            << "      $portfileText = $portfileText.Replace($optionsMarker, $optionsMarker + '        -DCMAKE_POLICY_VERSION_MINIMUM=3.5' + [Environment]::NewLine)\n"
            << "    }\n"
            << "    else {\n"
            << "      $configureMarker = 'vcpkg_cmake_configure(' + [Environment]::NewLine\n"
            << "      if (-not $portfileText.Contains($configureMarker)) {\n"
            << "        throw 'Could not patch the glad overlay port.'\n"
            << "      }\n"
            << "      $portfileText = $portfileText.Replace($configureMarker, $configureMarker + '    OPTIONS' + [Environment]::NewLine + '        -DCMAKE_POLICY_VERSION_MINIMUM=3.5' + [Environment]::NewLine)\n"
            << "    }\n"
            << "    [System.IO.File]::WriteAllText($portfile, $portfileText, $utf8NoBom)\n"
            << "  }\n"
            << "  Write-Step 'INFO' 'Prepared a glad overlay port for modern CMake policy handling.'\n"
            << "  return $overlayRoot\n"
            << "}\n"
            << "Remove-Item -LiteralPath $buildLog -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $handoffLog -Force -ErrorAction SilentlyContinue\n"
            << "Write-Step 'INFO' 'Source update worker started.'\n"
            << "Write-Step 'INFO' ('Source root: ' + $sourceRoot)\n"
            << "Write-Step 'INFO' ('Manifest root: ' + $manifestRoot)\n"
            << "Write-Step 'INFO' ('MSBuild: ' + $msbuildExe)\n"
            << "Remove-Item -LiteralPath $sourceArchive -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $stagingDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $sourceRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "New-Item -ItemType Directory -Path (Split-Path -Parent $sourceArchive) -Force | Out-Null\n"
            << "Write-Step 'INFO' 'Downloading latest source snapshot from main.'\n"
            << "$headers = @{ 'User-Agent' = 'EpochUpdater/1.0' }\n"
            << "Invoke-WebRequest -UseBasicParsing -Headers $headers -Uri $sourceUrl -OutFile $sourceArchive\n"
            << "Expand-Archive -LiteralPath $sourceArchive -DestinationPath $stagingDir -Force\n"
            << "$extractedRoot = Get-ChildItem -LiteralPath $stagingDir -Directory | Select-Object -First 1\n"
            << "if ($null -ne $extractedRoot) {\n"
            << "  Move-Item -LiteralPath $extractedRoot.FullName -Destination $sourceRoot -Force\n"
            << "} else {\n"
            << "  New-Item -ItemType Directory -Path $sourceRoot -Force | Out-Null\n"
            << "  Copy-Item -Path (Join-Path $stagingDir '*') -Destination $sourceRoot -Recurse -Force\n"
            << "}\n"
            << "Remove-Item -LiteralPath $stagingDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "Write-Step 'INFO' ('Source snapshot ready at: ' + $sourceRoot)\n"
            << "$vcpkgExe = Resolve-VcpkgExe\n"
            << "$vcpkgRoot = Split-Path -Parent $vcpkgExe\n"
            << "$env:VCPKG_ROOT = $vcpkgRoot\n"
            << "Prepare-ManifestForManagedVcpkg $manifestRoot $vcpkgRoot\n"
            << "$overlayRoot = Prepare-GladOverlay $vcpkgRoot\n"
            << "Write-Step 'INFO' 'Restoring source dependencies with vcpkg.'\n"
            << "$vcpkgArgs = @('install', '--triplet', $triplet, ('--x-manifest-root=' + $manifestRoot), ('--x-builtin-ports-root=' + (Join-Path $vcpkgRoot 'ports')), ('--x-builtin-registry-versions-dir=' + (Join-Path $vcpkgRoot 'versions')))\n"
            << "if (-not [string]::IsNullOrWhiteSpace($overlayRoot)) {\n"
            << "  $vcpkgArgs += ('--overlay-ports=' + $overlayRoot)\n"
            << "}\n"
            << "Invoke-Tool $vcpkgExe $vcpkgArgs $manifestRoot 'vcpkg restore'\n"
            << "$buildSucceeded = $false\n"
            << "for ($attempt = 1; $attempt -le 3 -and -not $buildSucceeded; ++$attempt) {\n"
            << "  try {\n"
            << "    Write-Step 'INFO' ('MSBuild attempt ' + $attempt + ' started.')\n"
            << "    Invoke-Tool $msbuildExe @($solution, ('/t:' + $buildTarget), ('/p:Configuration=' + $buildConfiguration), ('/p:Platform=' + $buildPlatform), '/p:UseMultiToolTask=false', '/m:1', '/clp:ErrorsOnly') $sourceRoot ('MSBuild attempt ' + $attempt)\n"
            << "    $buildSucceeded = $true\n"
            << "  }\n"
            << "  catch {\n"
            << "    Write-Step 'WARN' $_.Exception.Message\n"
            << "    if ($attempt -lt 3) {\n"
            << "      Write-Step 'INFO' 'Retrying the source build after restore.'\n"
            << "      Start-Sleep -Seconds 5\n"
            << "    }\n"
            << "  }\n"
            << "}\n"
            << "if (-not $buildSucceeded) {\n"
            << "  throw 'Source build failed after three attempts.'\n"
            << "}\n"
            << "if (-not (Test-Path -LiteralPath $builtExe)) {\n"
            << "  throw 'Built runtime output is missing after source update.'\n"
            << "}\n"
            << "Write-Step 'INFO' ('Built runtime ready at: ' + $builtExe)\n"
            << "Write-Handoff 'INFO' 'Waiting for runtime handoff.'\n"
            << "for ($attempt = 1; $attempt -le 600; ++$attempt) {\n"
            << "  try {\n"
            << "    if (Test-Path -LiteralPath $targetExe) {\n"
            << "      Remove-Item -LiteralPath $targetExe -Force -ErrorAction Stop\n"
            << "    }\n"
            << "  }\n"
            << "  catch {\n"
            << "  }\n"
            << "  if (-not (Test-Path -LiteralPath $targetExe)) {\n"
            << "    break\n"
            << "  }\n"
            << "  if ($attempt -eq 1 -or ($attempt % 15) -eq 0) {\n"
            << "    Write-Handoff 'INFO' ('Waiting for target runtime unlock attempt ' + $attempt + '.')\n"
            << "  }\n"
            << "  Start-Sleep -Seconds 1\n"
            << "}\n"
            << "if (Test-Path -LiteralPath $targetExe) {\n"
            << "  throw 'Timed out waiting for target runtime executable to unlock.'\n"
            << "}\n"
            << "Copy-Item -LiteralPath $builtExe -Destination $targetExe -Force\n"
            << "Get-ChildItem -LiteralPath $builtDir -File | Where-Object { $_.Extension -in '.dll', '.manifest' } | ForEach-Object {\n"
            << "  Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $targetDir $_.Name) -Force\n"
            << "}\n"
            << "if (Test-Path -LiteralPath $builtAssetsDir) {\n"
            << "  New-Item -ItemType Directory -Path $targetAssetsDir -Force | Out-Null\n"
            << "  Copy-Item -Path (Join-Path $builtAssetsDir '*') -Destination $targetAssetsDir -Recurse -Force\n"
            << "}\n"
            << "if (Test-Path -LiteralPath $sourceRepoAssetsDir) {\n"
            << "  New-Item -ItemType Directory -Path $targetAssetsDir -Force | Out-Null\n"
            << "  Copy-Item -Path (Join-Path $sourceRepoAssetsDir '*') -Destination $targetAssetsDir -Recurse -Force\n"
            << "}\n"
            << "Write-Handoff 'INFO' 'Source runtime files copied successfully.'\n"
            << "Remove-Item -LiteralPath $sourceArchive -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $sourceRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "Start-Process -FilePath $targetExe -WorkingDirectory $targetDir\n"
            << "Write-Handoff 'INFO' 'Restarted updated runtime.'\n"
            << "Start-Sleep -Seconds 1\n"
            << "Remove-Item -LiteralPath $workerPath -Force -ErrorAction SilentlyContinue\n";

        ps.close();

        if (!system_detail::launch_powershell_script(worker_script, silent_worker))
        {
            system_detail::log_error("Failed to launch source update worker.");
            return false;
        }

        system_detail::log_info("Source update worker launched successfully.");
        return true;
    }
#endif

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

        (void)command;
        if (!system_detail::launch_batch_hidden(script_path))
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

        if (!system_detail::launch_batch_hidden(script_path))
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
        const auto source_repo_assets_dir = system_detail::source_manifest_root(source_root) / "assets";
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
            << "set \"REPOASSETS=" << source_repo_assets_dir.string() << "\"\r\n"
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
            << "if exist \"%REPOASSETS%\" robocopy \"%REPOASSETS%\" \"%DSTASSETS%\" /E /NFL /NDL /NJH /NJS /NC /NS >nul\r\n"
            << "if errorlevel 8 (\r\n"
            << "  >> \"%LOG%\" echo [ERROR] Source repository asset robocopy failed with errorlevel %%ERRORLEVEL%%.\r\n"
            << "  exit /b 1\r\n"
            << ")\r\n"
            << ">> \"%LOG%\" echo [INFO] Source runtime files copied successfully.\r\n"
            << "del /F /Q \"%ARCHIVE%\" >nul 2>&1\r\n"
            << "start \"\" /D \"%TARGETDIR%\" \"%TARGETEXE%\"\r\n"
            << ">> \"%LOG%\" echo [INFO] Restarted updated runtime.\r\n"
            << "rmdir /S /Q \"%SRCROOT%\" >nul 2>&1\r\n"
            << "del /F /Q \"%~f0\" >nul 2>&1\r\n";

        bat.close();

        if (!system_detail::launch_batch_hidden(script_path))
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

    export bool run_source_update_command(
        const UpdateChannel& channel,
        const bool recheck_source_version = true,
        const bool silent_worker = false)
    {
        if (channel.source_url.empty())
        {
            system_detail::log_error("Source update URL is not configured.");
            return false;
        }

        const auto target_binary = system_detail::current_binary_path();

#if defined(_WIN32)
        if (recheck_source_version && !channel.source_version_url.empty())
        {
            const auto final_dir = system_detail::source_final_dir(target_binary);
            const std::string local_source_version =
                system_detail::read_local_source_version(final_dir);

            const auto source_status =
                check_for_updates(channel.source_version_url, "Source", local_source_version);

            if (source_status.ok && source_status.update_available)
                system_detail::log_info("A newer source snapshot is available on main.");
            else if (source_status.ok)
                system_detail::log_info("No newer source snapshot is currently available; continuing because source update was requested explicitly.");
        }

        const bool effective_silent_worker =
            silent_worker || system_detail::env_flag_enabled("EPOCH_UPDATER_SILENT");

        if (!launch_source_update_worker(channel, target_binary, effective_silent_worker))
            return false;

        system_detail::log_info("Closing the current runtime so the source update worker can finish the replacement.");
        std::exit(0);
        return true;
#else
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
#endif
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
