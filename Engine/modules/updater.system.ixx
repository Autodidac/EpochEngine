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

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <regex>
#include <source_location>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

export module updater.system;

import core.logger;
import core.path;
import engine.cli;
import engine.platform;
import updater.tools;
import updater.config;

namespace epochnamespace::updater
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

#if !defined(_WIN32)
        [[nodiscard]] inline bool launch_detached_posix_script(
            const std::filesystem::path& script_path)
        {
            const std::string shell_command =
                "chmod +x " + quote_shell_arg(script_path.string())
                + " && nohup " + quote_shell_arg(script_path.string())
                + " >/dev/null 2>&1 </dev/null &";
            const std::string wrapped =
                "/bin/sh -lc " + quote_shell_arg(shell_command);
            return std::system(wrapped.c_str()) == 0;
        }
#endif

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

        [[nodiscard]] inline std::string read_text_file_tail(
            const std::filesystem::path& path,
            const std::uintmax_t max_bytes = 65536)
        {
            std::error_code size_ec;
            const auto size = std::filesystem::file_size(path, size_ec);
            if (size_ec || size == 0)
                return {};

            std::ifstream in(path, std::ios::binary);
            if (!in)
                return {};

            const std::uintmax_t start = size > max_bytes ? size - max_bytes : 0;
            in.seekg(static_cast<std::streamoff>(start), std::ios::beg);

            std::string text;
            text.resize(static_cast<std::size_t>(size - start));
            in.read(text.data(), static_cast<std::streamsize>(text.size()));
            text.resize(static_cast<std::size_t>(std::max<std::streamsize>(0, in.gcount())));

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

        [[nodiscard]] inline bool starts_with_ascii(
            const std::string& text,
            const std::string_view prefix) noexcept
        {
            return text.size() >= prefix.size()
                && std::equal(prefix.begin(), prefix.end(), text.begin());
        }

        [[nodiscard]] inline bool ends_with_ascii(
            const std::string& text,
            const std::string_view suffix) noexcept
        {
            return text.size() >= suffix.size()
                && std::equal(suffix.rbegin(), suffix.rend(), text.rbegin());
        }

        struct VersionCheckResult
        {
            bool ok{ false };
            std::string local;
            std::string remote;
            bool update_available{ false };
        };

        struct BuildStatusResult
        {
            bool checked{ false };
            bool ok{ false };
            bool pending{ false };
            std::string job_name;
            std::string status;
            std::string conclusion;
            std::string url;
            std::string reason;
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

        [[nodiscard]] inline std::filesystem::path find_or_prepare_git(
            const std::filesystem::path& log_path);

        [[nodiscard]] inline std::filesystem::path make_temp_download_path(
            const std::string_view stem);

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

        [[nodiscard]] inline std::filesystem::path ensure_directory(
            const std::filesystem::path& path)
        {
            std::error_code ec;
            std::filesystem::create_directories(path, ec);
            return path;
        }

        [[nodiscard]] inline std::filesystem::path runtime_cache_root()
        {
            std::error_code ec;
            auto root = env_path("EPOCH_UPDATER_CACHE_ROOT");
            if (!root.empty())
                return ensure_directory(root);

            if (root.empty())
                root = epoch::core::path::executable_dir();
            if (root.empty())
                root = std::filesystem::current_path(ec);
            if (root.empty() || ec)
                root = std::filesystem::path{ "." };

            return ensure_directory(root / CACHE_ROOT_SUBDIR());
        }

        [[nodiscard]] inline std::filesystem::path updater_cache_root()
        {
            return ensure_directory(runtime_cache_root() / UPDATER_CACHE_SUBDIR());
        }

        [[nodiscard]] inline std::filesystem::path package_cache_root()
        {
            return ensure_directory(runtime_cache_root() / PACKAGE_CACHE_SUBDIR());
        }

        [[nodiscard]] inline std::filesystem::path managed_tools_root()
        {
            return ensure_directory(updater_cache_root() / UPDATER_TOOLS_SUBDIR());
        }

        [[nodiscard]] inline std::filesystem::path managed_work_root()
        {
            return ensure_directory(updater_cache_root() / UPDATER_WORK_SUBDIR());
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

        [[nodiscard]] inline std::filesystem::path validated_vcpkg_executable_in_root(
            const std::filesystem::path& root)
        {
            if (root.empty())
                return {};

            std::error_code ec;
            const auto normalized_root = std::filesystem::absolute(root, ec).lexically_normal();
            if (ec)
                return {};

            const auto executable = normalized_root / VCPKG_EXECUTABLE_NAME();
            if (!std::filesystem::exists(executable, ec))
                return {};
            ec.clear();

            if (!std::filesystem::exists(normalized_root / "ports", ec))
                return {};
            ec.clear();

            if (!std::filesystem::exists(normalized_root / "versions", ec))
                return {};
            ec.clear();

            if (!std::filesystem::exists(
                normalized_root / "scripts" / "buildsystems" / "vcpkg.cmake",
                ec))
            {
                return {};
            }

            return executable;
        }

        [[nodiscard]] inline bool vcpkg_registry_supports_manifest(
            const std::filesystem::path& root,
            const std::filesystem::path& manifest_root,
            const std::filesystem::path& log_path)
        {
            const std::string baseline = read_manifest_builtin_baseline(manifest_root);
            if (baseline.empty())
                return true;

            std::error_code ec;
            if (!std::filesystem::exists(root / ".git", ec)
                || !std::filesystem::exists(root / "versions" / "baseline.json", ec))
            {
                append_log_line(
                    log_path,
                    "[WARN] Installed vcpkg cannot prove registry compatibility with manifest baseline "
                    + baseline + ": " + root.string());
                return false;
            }

            const auto git_exe = find_or_prepare_git(log_path);
            if (git_exe.empty())
            {
                append_log_line(
                    log_path,
                    "[WARN] Git is unavailable, so the installed vcpkg registry cannot be validated: "
                    + root.string());
                return false;
            }

            int ancestry_exit = -1;
            const bool launched = run_process_hidden(
                git_exe,
                {
                    "-C",
                    root.string(),
                    "merge-base",
                    "--is-ancestor",
                    baseline,
                    "HEAD"
                },
                root,
                {},
                true,
                &ancestry_exit);
            if (launched && ancestry_exit == 0)
                return true;

            append_log_line(
                log_path,
                "[WARN] Installed vcpkg registry is older than manifest baseline "
                + baseline + "; using managed vcpkg instead: " + root.string());
            return false;
        }

        inline void append_unique_vcpkg_candidate(
            std::vector<std::filesystem::path>& candidates,
            const std::filesystem::path& candidate)
        {
            if (candidate.empty())
                return;

            std::error_code ec;
            auto normalized = std::filesystem::absolute(candidate, ec).lexically_normal();
            if (ec)
                normalized = candidate.lexically_normal();

            for (const auto& existing : candidates)
            {
                if (lower_ascii(existing.string()) == lower_ascii(normalized.string()))
                    return;
            }

            candidates.push_back(std::move(normalized));
        }

        [[nodiscard]] inline std::filesystem::path find_installed_vcpkg(
            const std::filesystem::path& manifest_root,
            const std::filesystem::path& log_path)
        {
            std::vector<std::filesystem::path> candidates;

            const auto configured_exe = env_path("VCPKG_EXE_PATH");
            if (!configured_exe.empty())
                append_unique_vcpkg_candidate(candidates, configured_exe.parent_path());

            append_unique_vcpkg_candidate(candidates, env_path("EPOCH_UPDATER_VCPKG_ROOT"));
            append_unique_vcpkg_candidate(candidates, env_path("VCPKG_ROOT"));

            const auto from_path = find_executable_on_path(VCPKG_EXECUTABLE_NAME());
            if (!from_path.empty())
                append_unique_vcpkg_candidate(candidates, from_path.parent_path());

#if defined(_WIN32)
            const auto user_profile = env_path("USERPROFILE");
            if (!user_profile.empty())
            {
                append_unique_vcpkg_candidate(candidates, user_profile / "source" / "repos" / "vcpkg");
                append_unique_vcpkg_candidate(candidates, user_profile / "vcpkg");
            }
#else
            const auto home = env_path("HOME");
            if (!home.empty())
            {
                append_unique_vcpkg_candidate(candidates, home / "vcpkg");
                append_unique_vcpkg_candidate(candidates, home / "Documents" / "repos" / "vcpkg");
                append_unique_vcpkg_candidate(candidates, home / "source" / "repos" / "vcpkg");
            }
#endif

            for (const auto& candidate : candidates)
            {
                const auto executable = validated_vcpkg_executable_in_root(candidate);
                if (executable.empty())
                {
                    append_log_line(log_path, "[WARN] Ignoring unusable vcpkg root: " + candidate.string());
                    continue;
                }

                if (vcpkg_registry_supports_manifest(candidate, manifest_root, log_path))
                {
                    append_log_line(log_path, "[INFO] Using installed vcpkg: " + candidate.string());
                    log_info("Using installed vcpkg toolchain.");
                    return executable;
                }
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

        [[nodiscard]] inline std::vector<std::string> extract_json_objects(const std::string_view text)
        {
            std::vector<std::string> objects;
            bool in_string = false;
            bool escaping = false;
            int depth = 0;
            std::size_t start = std::string_view::npos;

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const char ch = text[i];

                if (in_string)
                {
                    if (escaping)
                    {
                        escaping = false;
                    }
                    else if (ch == '\\')
                    {
                        escaping = true;
                    }
                    else if (ch == '"')
                    {
                        in_string = false;
                    }

                    continue;
                }

                if (ch == '"')
                {
                    in_string = true;
                    continue;
                }

                if (ch == '{')
                {
                    if (depth == 0)
                        start = i;
                    ++depth;
                    continue;
                }

                if (ch == '}')
                {
                    if (depth <= 0)
                        continue;

                    --depth;
                    if (depth == 0 && start != std::string_view::npos)
                    {
                        objects.emplace_back(text.substr(start, i - start + 1));
                        start = std::string_view::npos;
                    }
                }
            }

            return objects;
        }

        [[nodiscard]] inline std::string extract_json_string_field(
            const std::string& object_text,
            const char* field_name)
        {
            const std::string field_pattern =
                std::string{ "\"" } + field_name + "\"\\s*:\\s*\"((?:\\\\.|[^\"])*)\"";

            const std::regex field_regex(
                field_pattern,
                std::regex::optimize);

            std::smatch match;
            if (!std::regex_search(object_text, match, field_regex))
                return {};

            return unescape_json_string_basic(match[1].str());
        }

        [[nodiscard]] inline std::string extract_json_array_text(
            const std::string& object_text,
            const char* field_name)
        {
            const auto key = std::string{ "\"" } + field_name + "\"";
            const auto key_pos = object_text.find(key);
            if (key_pos == std::string::npos)
                return {};

            const auto open_pos = object_text.find('[', key_pos + key.size());
            if (open_pos == std::string::npos)
                return {};

            bool in_string = false;
            bool escaping = false;
            int depth = 0;

            for (std::size_t i = open_pos; i < object_text.size(); ++i)
            {
                const char ch = object_text[i];

                if (in_string)
                {
                    if (escaping)
                    {
                        escaping = false;
                    }
                    else if (ch == '\\')
                    {
                        escaping = true;
                    }
                    else if (ch == '"')
                    {
                        in_string = false;
                    }

                    continue;
                }

                if (ch == '"')
                {
                    in_string = true;
                    continue;
                }

                if (ch == '[')
                {
                    ++depth;
                    continue;
                }

                if (ch == ']')
                {
                    --depth;
                    if (depth == 0)
                        return object_text.substr(open_pos, i - open_pos + 1);
                }
            }

            return {};
        }

        struct ReleaseAssetInfo
        {
            std::string name;
            std::string browser_download_url;
        };

        struct ReleaseInfo
        {
            std::string tag_name;
            std::vector<ReleaseAssetInfo> assets;
        };

        [[nodiscard]] inline std::vector<ReleaseInfo> parse_github_releases_json(const std::string& json)
        {
            std::vector<ReleaseInfo> releases;

            for (const auto& release_object : extract_json_objects(json))
            {
                ReleaseInfo release{};
                release.tag_name = extract_json_string_field(release_object, "tag_name");

                const auto assets_array = extract_json_array_text(release_object, "assets");
                if (!assets_array.empty())
                {
                    for (const auto& asset_object : extract_json_objects(assets_array))
                    {
                        ReleaseAssetInfo asset{};
                        asset.name = extract_json_string_field(asset_object, "name");
                        asset.browser_download_url =
                            extract_json_string_field(asset_object, "browser_download_url");

                        if (!asset.name.empty() && !asset.browser_download_url.empty())
                            release.assets.push_back(std::move(asset));
                    }
                }

                if (!release.tag_name.empty())
                    releases.push_back(std::move(release));
            }

            return releases;
        }

        struct ResolvedPackagedRelease
        {
            bool found{ false };
            std::string tag_name;
            std::string binary_name;
            std::string remote_version;
            std::string binary_url;
        };

        [[nodiscard]] inline ResolvedPackagedRelease resolve_packaged_release()
        {
            ResolvedPackagedRelease resolved{};
            const auto releases_json_path =
                make_temp_download_path("release_index").replace_extension(".json");

            if (!download_file(PROJECT_RELEASES_API_URL(), releases_json_path.string()))
                return resolved;

            const auto json = read_text_file(releases_json_path);
            std::error_code ec;
            std::filesystem::remove(releases_json_path, ec);
            if (json.empty())
                return resolved;

            const auto releases = parse_github_releases_json(json);
            const auto binary_prefix = PACKAGED_BINARY_ASSET_PREFIX();
            const auto binary_suffix = PACKAGED_BINARY_ASSET_SUFFIX();

            for (const auto& release : releases)
            {
                for (const auto& asset : release.assets)
                {
                    if (!starts_with_ascii(asset.name, binary_prefix)
                        || !ends_with_ascii(asset.name, binary_suffix))
                    {
                        continue;
                    }

                    const std::size_t version_offset = binary_prefix.size();
                    const std::size_t version_length =
                        asset.name.size() - binary_prefix.size() - binary_suffix.size();
                    const std::string remote_version =
                        extract_version_string(asset.name.substr(version_offset, version_length));
                    if (remote_version.empty())
                    {
                        continue;
                    }

                    resolved.found = true;
                    resolved.tag_name = release.tag_name;
                    resolved.binary_name = asset.name;
                    resolved.remote_version = remote_version;
                    resolved.binary_url = asset.browser_download_url;
                    return resolved;
                }
            }

            return resolved;
        }

        [[nodiscard]] inline bool text_matches_ci_job(
            const std::string& value,
            const std::string& job_name)
        {
            if (value.empty() || job_name.empty())
                return false;

            const auto normalized_value = lower_ascii(value);
            const auto normalized_job = lower_ascii(job_name);
            return normalized_value == normalized_job
                || normalized_value.find(normalized_job) != std::string::npos;
        }

        [[nodiscard]] inline BuildStatusResult check_platform_build_status(
            const std::string& runs_api_url,
            const std::string& job_name)
        {
            BuildStatusResult result{};
            result.job_name = job_name;

            if (runs_api_url.empty() || job_name.empty())
            {
                result.reason = "No platform build gate is configured.";
                return result;
            }

            const auto runs_json_path =
                make_temp_download_path("actions_runs").replace_extension(".json");
            if (!download_file(runs_api_url, runs_json_path.string()))
            {
                result.reason = "Could not download GitHub Actions run metadata.";
                return result;
            }

            const auto runs_json = read_text_file(runs_json_path);
            std::error_code ec;
            std::filesystem::remove(runs_json_path, ec);
            if (runs_json.empty())
            {
                result.reason = "GitHub Actions run metadata was empty.";
                return result;
            }

            const auto workflow_runs = extract_json_array_text(runs_json, "workflow_runs");
            if (workflow_runs.empty())
            {
                result.reason = "GitHub Actions response did not contain workflow runs.";
                return result;
            }

            bool attempted_job_metadata = false;
            bool parsed_job_metadata = false;
            for (const auto& run_object : extract_json_objects(workflow_runs))
            {
                const auto jobs_url = extract_json_string_field(run_object, "jobs_url");
                if (jobs_url.empty())
                    continue;

                attempted_job_metadata = true;
                const auto jobs_json_path =
                    make_temp_download_path("actions_jobs").replace_extension(".json");
                if (!download_file(jobs_url, jobs_json_path.string()))
                    continue;

                const auto jobs_json = read_text_file(jobs_json_path);
                std::filesystem::remove(jobs_json_path, ec);
                if (jobs_json.empty())
                    continue;

                const auto jobs_array = extract_json_array_text(jobs_json, "jobs");
                if (jobs_array.empty())
                    continue;

                parsed_job_metadata = true;
                for (const auto& job_object : extract_json_objects(jobs_array))
                {
                    const auto candidate_name = extract_json_string_field(job_object, "name");
                    if (!text_matches_ci_job(candidate_name, job_name))
                        continue;

                    result.checked = true;
                    result.job_name = candidate_name.empty() ? job_name : candidate_name;
                    result.status = extract_json_string_field(job_object, "status");
                    result.conclusion = extract_json_string_field(job_object, "conclusion");
                    result.url = extract_json_string_field(job_object, "html_url");

                    if (result.status != "completed")
                    {
                        result.pending = true;
                        result.reason = "Platform build is still " + result.status + ".";
                        return result;
                    }

                    result.ok = result.conclusion == "success";
                    if (result.ok)
                    {
                        result.reason = "Platform build succeeded.";
                    }
                    else if (!result.conclusion.empty())
                    {
                        result.reason = "Platform build concluded with " + result.conclusion + ".";
                    }
                    else
                    {
                        result.reason = "Platform build completed without a success conclusion.";
                    }

                    return result;
                }
            }

            if (attempted_job_metadata && !parsed_job_metadata)
            {
                result.reason = "Could not download GitHub Actions job metadata.";
                return result;
            }

            result.reason = "No matching platform build job was found for '" + job_name + "'.";
            return result;
        }

        [[nodiscard]] inline bool build_status_failure_is_transient(const BuildStatusResult& result)
        {
            if (result.checked || result.pending || result.ok)
                return false;

            return result.reason.find("Could not download GitHub Actions") != std::string::npos
                || result.reason.find("GitHub Actions run metadata was empty") != std::string::npos
                || result.reason.find("GitHub Actions response did not contain workflow runs") != std::string::npos;
        }

        [[nodiscard]] inline std::string capture_process_output(
            const std::filesystem::path& executable,
            const std::vector<std::string>& args,
            const std::filesystem::path& working_directory,
            int* const exit_code = nullptr)
        {
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
            std::string head;
            const auto local_git_dir = vcpkg_root / ".git";

            const auto resolve_local_head = [&]() -> std::string
            {
                if (!std::filesystem::exists(local_git_dir))
                    return {};

                const auto resolved_git_dir = trim_ascii(capture_process_output(
                    git_exe,
                    { "rev-parse", "--absolute-git-dir" },
                    vcpkg_root,
                    &git_exit));

                if (git_exit != 0 || resolved_git_dir.empty())
                {
                    append_log_line(
                        log_path,
                        "[WARN] Managed vcpkg git-dir probe failed with exit code "
                        + std::to_string(git_exit) + ".");
                    return {};
                }

                std::error_code local_ec;
                const auto normalized_local_git_dir =
                    std::filesystem::weakly_canonical(local_git_dir, local_ec);

                std::error_code resolved_ec;
                const auto normalized_resolved_git_dir =
                    std::filesystem::weakly_canonical(
                        std::filesystem::path{ resolved_git_dir },
                        resolved_ec);

                if (local_ec || resolved_ec
                    || normalized_local_git_dir != normalized_resolved_git_dir)
                {
                    append_log_line(
                        log_path,
                        "[WARN] Managed vcpkg git resolution escaped the sandboxed repo: "
                        + resolved_git_dir);

                    return {};
                }

                std::string local_head = capture_process_output(
                    git_exe,
                    {
                        "--git-dir=" + local_git_dir.string(),
                        "--work-tree=" + vcpkg_root.string(),
                        "rev-parse",
                        "--verify",
                        "HEAD"
                    },
                    vcpkg_root,
                    &git_exit);

                if (git_exit == 0 && !local_head.empty())
                    return lower_ascii(trim_ascii(std::move(local_head)));

                append_log_line(
                    log_path,
                    "[WARN] Managed vcpkg HEAD probe failed with exit code "
                    + std::to_string(git_exit) + ".");

                return {};
            };

            if (head = resolve_local_head(); !head.empty())
                return head;

            append_log_line(log_path, "[INFO] Initializing managed vcpkg git registry snapshot.");

            int init_exit = -1;
            if (!run_process_hidden(
                git_exe,
                { "-C", vcpkg_root.string(), "init" },
                vcpkg_root,
                log_path,
                true,
                &init_exit) || init_exit != 0)
            {
                append_log_line(log_path, "[ERROR] Failed to initialize the managed vcpkg git registry.");
                return {};
            }

            if (!std::filesystem::exists(local_git_dir))
            {
                append_log_line(log_path, "[ERROR] Managed vcpkg git init did not create a local .git directory.");
                return {};
            }

            const std::array<std::vector<std::string>, 4> setup_steps{
                std::vector<std::string>{
                    "--git-dir=" + local_git_dir.string(),
                    "--work-tree=" + vcpkg_root.string(),
                    "config",
                    "user.name",
                    "Epoch Updater"
                },
                std::vector<std::string>{
                    "--git-dir=" + local_git_dir.string(),
                    "--work-tree=" + vcpkg_root.string(),
                    "config",
                    "user.email",
                    "updater@epoch.local"
                },
                std::vector<std::string>{
                    "--git-dir=" + local_git_dir.string(),
                    "--work-tree=" + vcpkg_root.string(),
                    "config",
                    "core.longpaths",
                    "true"
                },
                std::vector<std::string>{
                    "--git-dir=" + local_git_dir.string(),
                    "--work-tree=" + vcpkg_root.string(),
                    "add",
                    "--",
                    "ports",
                    "versions",
                    "scripts",
                    "triplets",
                    ".vcpkg-root"
                }
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
            log_info("Committing managed vcpkg registry snapshot.");
            if (!run_process_hidden(
                git_exe,
                {
                    "--git-dir=" + local_git_dir.string(),
                    "--work-tree=" + vcpkg_root.string(),
                    "commit",
                    "--no-gpg-sign",
                    "-m",
                    "Managed vcpkg registry snapshot"
                },
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
                if (head = resolve_local_head(); !head.empty())
                    return head;

                append_log_line(log_path, "[ERROR] Managed vcpkg git registry did not produce a usable HEAD revision.");
                return {};
            }

            if (head = resolve_local_head(); !head.empty())
            {
                log_info("Managed vcpkg registry snapshot ready.");
                return head;
            }

            append_log_line(log_path, "[ERROR] Failed to resolve the managed vcpkg git HEAD revision.");
            return {};
        }

        [[nodiscard]] inline std::filesystem::path find_or_prepare_vcpkg(
            const std::filesystem::path& manifest_root,
            const std::filesystem::path& log_path)
        {
            if (const auto installed = find_installed_vcpkg(manifest_root, log_path);
                !installed.empty())
            {
                return installed;
            }

            const std::string baseline =
                read_manifest_builtin_baseline(manifest_root);
            const std::string resolved_ref =
                baseline.empty() ? std::string{ VCPKG_DEFAULT_REF } : baseline;
            const std::string safe_ref = shorten_token(resolved_ref);

            const auto tools_root = managed_tools_root();
            const auto managed_root = tools_root / ("v-" + safe_ref);
            const auto managed_exe = managed_root / VCPKG_EXECUTABLE_NAME();

            if (const auto prepared_vcpkg = validated_vcpkg_executable_in_root(managed_root);
                !prepared_vcpkg.empty())
            {
                log_info("Using managed vcpkg toolchain.");
                append_log_line(log_path, "[INFO] Using managed vcpkg: " + managed_root.string());
                return prepared_vcpkg;
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
                if (resolved_ref == VCPKG_DEFAULT_REF)
                {
                    append_log_line(log_path, "[ERROR] Failed to download managed vcpkg archive.");
                    return {};
                }

                append_log_line(
                    log_path,
                    "[WARN] Managed vcpkg ref " + resolved_ref
                    + " was not downloadable; falling back to " + std::string{ VCPKG_DEFAULT_REF } + ".");

                std::filesystem::remove(archive_path, ec);
                if (!download_file(VCPKG_ARCHIVE_URL(VCPKG_DEFAULT_REF), archive_path.string()))
                {
                    append_log_line(log_path, "[ERROR] Failed to download fallback managed vcpkg archive.");
                    return {};
                }
            }

            if (!extract_archive(archive_path.string(), staging_dir.string()))
            {
                append_log_line(log_path, "[ERROR] Failed to extract managed vcpkg archive.");
                std::filesystem::remove(archive_path, ec);
                return {};
            }

            std::filesystem::path bootstrap_candidate;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(staging_dir, ec))
            {
                if (ec)
                    break;

                if (entry.is_regular_file()
                    && entry.path().filename() == VCPKG_BOOTSTRAP_SCRIPT_NAME())
                {
                    bootstrap_candidate = entry.path();
                    break;
                }
            }

            if (bootstrap_candidate.empty())
            {
                append_log_line(log_path, "[ERROR] Managed vcpkg archive did not contain " + VCPKG_BOOTSTRAP_SCRIPT_NAME() + ".");
                std::filesystem::remove(archive_path, ec);
                std::filesystem::remove_all(staging_dir, ec);
                return {};
            }

            const std::filesystem::path extracted_root = bootstrap_candidate.parent_path();
            const auto normalized_for_compare = [](const std::filesystem::path& path)
                {
                    std::error_code path_ec;
                    auto absolute = std::filesystem::absolute(path, path_ec);
                    if (path_ec)
                    {
                        path_ec.clear();
                        absolute = path;
                    }

                    auto text = absolute.lexically_normal().string();
#if defined(_WIN32)
                    text = lower_ascii(std::move(text));
#endif
                    return text;
                };
            ec.clear();
            const bool direct_staging_root =
                normalized_for_compare(extracted_root)
                == normalized_for_compare(staging_dir);
            if (direct_staging_root)
            {
                std::filesystem::create_directories(managed_root, ec);
                if (ec)
                {
                    append_log_line(log_path, "[ERROR] Failed to prepare managed vcpkg destination directory.");
                    std::filesystem::remove(archive_path, ec);
                    std::filesystem::remove_all(staging_dir, ec);
                    return {};
                }

                std::filesystem::copy(
                    staging_dir,
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
            else
            {
                std::filesystem::create_directories(managed_root, ec);
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

#if defined(_WIN32)
            auto cmd = env_path("ComSpec");
            if (cmd.empty())
                cmd = std::filesystem::path{ "C:\\Windows\\System32\\cmd.exe" };
#endif

            int bootstrap_exit = -1;
            append_log_line(log_path, "[INFO] Bootstrapping managed vcpkg.");

#if defined(_WIN32)
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
#else
            if (!run_process_hidden(
                std::filesystem::path{ "/bin/bash" },
                {
                    bootstrap_script.string(),
                    "-disableMetrics"
                },
                managed_root,
                log_path,
                true,
                &bootstrap_exit) || bootstrap_exit != 0)
#endif
            {
                append_log_line(
                    log_path,
                    "[ERROR] Managed vcpkg bootstrap failed with exit code " + std::to_string(bootstrap_exit));
                log_error("Managed vcpkg bootstrap failed. See logs/epoch_source_update.log for details.");
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

        [[nodiscard]] inline bool patch_cmake_policy_overlay_portfile(
            const std::filesystem::path& portfile,
            const std::filesystem::path& log_path,
            std::string_view port_name)
        {
            auto portfile_text = read_text_file(portfile);
            if (portfile_text.empty())
            {
                append_log_line(log_path, std::string{ "[WARN] Failed to read the staged " }
                    + std::string{ port_name } + " portfile.");
                return false;
            }

            portfile_text = std::regex_replace(portfile_text, std::regex{ "\r\n?" }, "\n");
            if (portfile_text.find("CMAKE_POLICY_VERSION_MINIMUM=3.5") != std::string::npos)
            {
                append_log_line(log_path, std::string{ "[INFO] Managed vcpkg " }
                    + std::string{ port_name } + " port already carries modern CMake policy handling.");
                return true;
            }

            const std::string option_line = "        -DCMAKE_POLICY_VERSION_MINIMUM=3.5\n";
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
                    append_log_line(log_path, std::string{ "[WARN] Could not patch the staged " }
                        + std::string{ port_name } + " portfile.");
                    return false;
                }

                const auto line_break = portfile_text.find('\n', configure_pos);
                if (line_break == std::string::npos)
                {
                    append_log_line(log_path, std::string{ "[WARN] Could not patch the staged " }
                        + std::string{ port_name } + " portfile.");
                    return false;
                }

                portfile_text.insert(
                    line_break + 1,
                    "    OPTIONS\n"
                    "        -DCMAKE_POLICY_VERSION_MINIMUM=3.5\n");
            }

            std::ofstream out(portfile, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                append_log_line(log_path, std::string{ "[WARN] Failed to write the staged " }
                    + std::string{ port_name } + " overlay port.");
                return false;
            }

            out << portfile_text;
            append_log_line(log_path, std::string{ "[INFO] Prepared a " }
                + std::string{ port_name } + " overlay port for modern CMake policy handling.");
            return true;
        }

        [[nodiscard]] inline std::filesystem::path prepare_vcpkg_overlay_ports(
            const std::filesystem::path& vcpkg_root,
            const std::filesystem::path& log_path)
        {
            const auto overlay_root = managed_tools_root() / "ov";
            std::error_code ec;
            std::filesystem::create_directories(overlay_root, ec);
            if (ec)
            {
                append_log_line(log_path, "[WARN] Failed to create the updater overlay-ports directory.");
                return {};
            }

            bool staged_any = false;
            constexpr std::array<std::string_view, 11> k_policy_ports{
                "freetype",
                "glad",
                "glfw3",
                "libogg",
                "libvorbis",
                "raylib",
                "sdl3",
                "sfml",
                "shaderc",
                "spirv-tools",
                "zlib"
            };

            for (const auto port_name : k_policy_ports)
            {
                const auto source_port_dir = vcpkg_root / "ports" / std::string{ port_name };
                if (!std::filesystem::exists(source_port_dir))
                {
                    append_log_line(log_path, std::string{ "[WARN] Managed vcpkg " }
                        + std::string{ port_name } + " port was not found; skipping overlay patch.");
                    continue;
                }

                const auto overlay_port_dir = overlay_root / std::string{ port_name };
                ec.clear();
                std::filesystem::remove_all(overlay_port_dir, ec);
                ec.clear();
                std::filesystem::copy(
                    source_port_dir,
                    overlay_port_dir,
                    std::filesystem::copy_options::recursive
                    | std::filesystem::copy_options::overwrite_existing,
                    ec);
                if (ec)
                {
                    append_log_line(log_path, std::string{ "[WARN] Failed to stage the " }
                        + std::string{ port_name } + " overlay port.");
                    continue;
                }

                staged_any = patch_cmake_policy_overlay_portfile(
                    overlay_port_dir / "portfile.cmake",
                    log_path,
                    port_name)
                    || staged_any;
            }

            if (!staged_any)
                return {};

            append_log_line(log_path, "[INFO] Prepared updater overlay ports for modern CMake policy handling.");
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

            HANDLE output_handle = INVALID_HANDLE_VALUE;
            bool close_output_handle = false;
            if (!log_path.empty())
            {
                output_handle = CreateFileW(
                    log_path.wstring().c_str(),
                    FILE_APPEND_DATA,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    &sa,
                    OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    nullptr);

                if (output_handle == INVALID_HANDLE_VALUE)
                {
                    log_error(last_error_message("Failed to open updater process log"));
                    return false;
                }
                close_output_handle = true;
            }

            HANDLE input_handle = INVALID_HANDLE_VALUE;
            bool close_input_handle = false;
            const bool use_std_handles = hidden || output_handle != INVALID_HANDLE_VALUE;
            if (use_std_handles)
            {
                if (output_handle == INVALID_HANDLE_VALUE)
                {
                    output_handle = CreateFileW(
                        L"NUL",
                        GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        &sa,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        nullptr);
                    close_output_handle = output_handle != INVALID_HANDLE_VALUE;
                }

                input_handle = CreateFileW(
                    L"NUL",
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    &sa,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    nullptr);
                close_input_handle = input_handle != INVALID_HANDLE_VALUE;

                if (output_handle == INVALID_HANDLE_VALUE || input_handle == INVALID_HANDLE_VALUE)
                {
                    if (close_output_handle)
                        CloseHandle(output_handle);
                    if (close_input_handle)
                        CloseHandle(input_handle);
                    log_error("Failed to prepare updater child-process stdio handles.");
                    return false;
                }
            }

            STARTUPINFOW si{};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = hidden ? SW_HIDE : SW_SHOWNORMAL;

            if (use_std_handles)
            {
                si.dwFlags |= STARTF_USESTDHANDLES;
                si.hStdOutput = output_handle;
                si.hStdError = output_handle;
                si.hStdInput = input_handle;
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
                use_std_handles ? TRUE : FALSE,
                hidden ? CREATE_NO_WINDOW : 0,
                nullptr,
                working_directory.empty() ? nullptr : working_directory_wide.c_str(),
                &si,
                &pi);

            if (close_output_handle)
                CloseHandle(output_handle);
            if (close_input_handle)
                CloseHandle(input_handle);

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
#else
        inline void append_log_line(
            const std::filesystem::path& log_path,
            const std::string& line)
        {
            if (log_path.empty())
                return;

            std::ofstream out(log_path, std::ios::binary | std::ios::app);
            if (!out)
                return;

            out << line << '\n';
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

        [[nodiscard]] inline bool run_process_hidden(
            const std::filesystem::path& executable,
            const std::vector<std::string>& args,
            const std::filesystem::path& working_directory,
            const std::filesystem::path& log_path,
            const bool wait_for_exit,
            int* const exit_code = nullptr)
        {
            if (exit_code != nullptr)
                *exit_code = -1;

            std::string shell_command;
            if (!working_directory.empty())
                shell_command += "cd " + quote_shell_arg(working_directory.string()) + " && ";

            shell_command += build_command_line(executable, args);

            if (!log_path.empty())
                shell_command += " >> " + quote_shell_arg(log_path.string()) + " 2>&1";
            else
                shell_command += " >/dev/null 2>&1";

            if (!wait_for_exit)
                shell_command += " &";

            const std::string wrapped = "/bin/sh -lc " + quote_shell_arg(shell_command);
            const int rc = std::system(wrapped.c_str());

            if (!wait_for_exit)
            {
                if (exit_code != nullptr)
                    *exit_code = 0;
                return rc == 0;
            }

            if (rc == -1)
                return false;

            int normalized = rc;
            if (WIFEXITED(rc))
                normalized = WEXITSTATUS(rc);

            if (exit_code != nullptr)
                *exit_code = normalized;

            return true;
        }

        [[nodiscard]] inline bool launch_batch_hidden(const std::filesystem::path& script_path)
        {
            return run_process_hidden(
                std::filesystem::path{ "/bin/sh" },
                { script_path.string() },
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
            return std::filesystem::absolute("EpochEditor.exe", ec).lexically_normal();
#else
            return std::filesystem::absolute("epoch", ec).lexically_normal();
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
            const auto install_token =
                shorten_token(
                    target_binary.parent_path().filename().string()
                    + "_" + target_binary.stem().string(),
                    24);
            return ensure_directory(package_cache_root() / install_token) / "main.update.pkg";
        }

        [[nodiscard]] inline std::string sanitize_cache_file_name(std::string text)
        {
            for (char& ch : text)
            {
                const auto uch = static_cast<unsigned char>(ch);
                if (!std::isalnum(uch) && ch != '-' && ch != '_' && ch != '.')
                    ch = '_';
            }

            while (!text.empty() && (text.back() == '_' || text.back() == '.'))
                text.pop_back();

            if (text.empty() || text == "." || text == "..")
                text = "epoch-update-" + std::string{ PROJECT_PACKAGED_VERSION };

            return text;
        }

        [[nodiscard]] inline std::string update_package_cache_file_name(
            const std::string_view url,
            const std::string_view archive_extension)
        {
            const std::string stripped_url = strip_url_query_and_fragment(std::string{ url });
            const auto last_separator = stripped_url.find_last_of("/\\");
            std::string file_name = last_separator == std::string::npos
                ? stripped_url
                : stripped_url.substr(last_separator + 1);

            file_name = sanitize_cache_file_name(file_name);

            if (!archive_extension.empty())
            {
                const std::string lower_name = lower_ascii(file_name);
                const std::string lower_extension = lower_ascii(std::string{ archive_extension });
                if (!lower_name.ends_with(lower_extension))
                    file_name += archive_extension;
            }

            return file_name;
        }

        [[nodiscard]] inline std::filesystem::path replacement_package_path(
            const std::filesystem::path& target_binary,
            const std::string_view url,
            const std::string_view archive_extension)
        {
            const auto install_token =
                shorten_token(
                    target_binary.parent_path().filename().string()
                    + "_" + target_binary.stem().string(),
                    24);

            return ensure_directory(package_cache_root() / install_token)
                / update_package_cache_file_name(url, archive_extension);
        }

        [[nodiscard]] inline std::filesystem::path replacement_extract_dir(const std::filesystem::path& target_binary)
        {
            const auto install_root =
                updater_cache_root()
                / shorten_token(
                    target_binary.parent_path().filename().string()
                    + "_" + target_binary.stem().string(),
                    24)
                / "extract";
            return install_root;
        }

        [[nodiscard]] inline std::filesystem::path source_work_root(const std::filesystem::path& target_binary)
        {
            return ensure_directory(
                managed_work_root()
                / shorten_token(
                    target_binary.parent_path().filename().string()
                    + "_" + target_binary.stem().string(),
                    24));
        }

        [[nodiscard]] inline std::filesystem::path source_run_tools_root(
            const std::filesystem::path& target_binary,
            const std::string_view run_token)
        {
            return ensure_directory(
                target_binary.parent_path()
                / CACHE_ROOT_SUBDIR()
                / UPDATER_CACHE_SUBDIR()
                / "t"
                / shorten_token(std::string{ run_token }, 16));
        }

        [[nodiscard]] inline std::filesystem::path source_archive_path(const std::filesystem::path& target_binary)
        {
            return source_work_root(target_binary) / "source_snapshot";
        }

        [[nodiscard]] inline std::string archive_extension_from_url(const std::string_view url)
        {
            const auto normalized_url =
                lower_ascii(strip_url_query_and_fragment(std::string{ url }));

            if (normalized_url.ends_with(".tar.gz"))
                return ".tar.gz";
            if (normalized_url.ends_with(".tgz"))
                return ".tgz";
            if (normalized_url.ends_with(".zip"))
                return ".zip";
            return {};
        }

        [[nodiscard]] inline std::filesystem::path source_archive_path(
            const std::filesystem::path& target_binary,
            const std::string_view source_url)
        {
            auto archive_path = source_archive_path(target_binary);
            const auto extension = archive_extension_from_url(source_url);
            if (!extension.empty())
                archive_path += extension;
            return archive_path;
        }

        [[nodiscard]] inline std::string make_source_update_run_token()
        {
            static std::atomic<unsigned long long> s_source_run_counter{ 0 };
            const auto serial = s_source_run_counter.fetch_add(1, std::memory_order_relaxed);
            const auto tick = static_cast<unsigned long long>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count());
            return std::to_string(tick) + "_" + std::to_string(serial);
        }

        [[nodiscard]] inline std::filesystem::path source_run_dir(
            const std::filesystem::path& target_binary,
            const std::string_view run_token)
        {
            return ensure_directory(
                source_work_root(target_binary)
                / ("run_" + shorten_token(std::string{ run_token }, 40)));
        }

        [[nodiscard]] inline std::filesystem::path source_archive_path(
            const std::filesystem::path& target_binary,
            const std::string_view source_url,
            const std::string_view run_token)
        {
            auto archive_path = source_run_dir(target_binary, run_token) / "source_snapshot";
            const auto extension = archive_extension_from_url(source_url);
            if (!extension.empty())
                archive_path += extension;
            return archive_path;
        }

        [[nodiscard]] inline std::string describe_source_archive(const std::string_view /*source_url*/)
        {
            return PROJECT_SOURCE_ARCHIVE_LABEL();
        }

        [[nodiscard]] inline std::filesystem::path source_staging_dir(const std::filesystem::path& target_binary)
        {
            return source_work_root(target_binary) / "sx";
        }

        [[nodiscard]] inline std::filesystem::path source_final_dir(const std::filesystem::path& target_binary)
        {
            return source_work_root(target_binary) / "src";
        }

        [[nodiscard]] inline std::filesystem::path project_source_cache_root()
        {
            return ensure_directory(runtime_cache_root() / "project_sources" / "epoch_engine");
        }

        [[nodiscard]] inline std::filesystem::path project_source_downloads_dir()
        {
            return ensure_directory(project_source_cache_root() / "downloads");
        }

        [[nodiscard]] inline std::filesystem::path project_source_archive_path(
            const std::string_view source_url)
        {
            auto archive_path = project_source_downloads_dir() / "source_snapshot";
            const auto extension = archive_extension_from_url(source_url);
            if (!extension.empty())
                archive_path += extension;
            return archive_path;
        }

        [[nodiscard]] inline std::filesystem::path project_source_staging_dir()
        {
            return project_source_cache_root() / "staging";
        }

        [[nodiscard]] inline std::filesystem::path project_source_final_dir()
        {
            return project_source_cache_root() / "source";
        }

        [[nodiscard]] inline std::filesystem::path source_staging_dir(
            const std::filesystem::path& target_binary,
            const std::string_view run_token)
        {
            return source_run_dir(target_binary, run_token) / "sx";
        }

        [[nodiscard]] inline std::filesystem::path source_final_dir(
            const std::filesystem::path& target_binary,
            const std::string_view run_token)
        {
            return source_run_dir(target_binary, run_token) / "src";
        }

        [[nodiscard]] inline std::filesystem::path source_cancel_path(const std::filesystem::path& target_binary)
        {
            return target_binary.parent_path() / "epoch_source_update.cancel";
        }

        [[nodiscard]] inline std::filesystem::path source_active_run_path(const std::filesystem::path& target_binary)
        {
            return target_binary.parent_path() / "epoch_source_update.active";
        }

        [[nodiscard]] inline std::filesystem::path update_handoff_log_path_for(
            const std::filesystem::path& target_binary);

        struct SourceActiveMarker
        {
            std::filesystem::path run_dir{};
            unsigned long process_id{};
            bool has_process_id{ false };
        };

        [[nodiscard]] inline bool source_cancel_requested(const std::filesystem::path& target_binary)
        {
            std::error_code ec;
            return std::filesystem::exists(source_cancel_path(target_binary), ec) && !ec;
        }

        [[nodiscard]] inline std::atomic_bool& in_process_source_update_active() noexcept
        {
            static std::atomic_bool active{ false };
            return active;
        }

        struct ScopedInProcessSourceUpdate final
        {
            bool acquired{ false };

            ScopedInProcessSourceUpdate() noexcept
            {
                bool expected = false;
                acquired = in_process_source_update_active().compare_exchange_strong(
                    expected,
                    true,
                    std::memory_order_acq_rel);
            }

            ~ScopedInProcessSourceUpdate()
            {
                if (acquired)
                    in_process_source_update_active().store(false, std::memory_order_release);
            }
        };

        [[nodiscard]] inline bool stop_source_update_if_canceled(
            const std::filesystem::path& target_binary,
            const std::filesystem::path& log_path,
            const std::string_view checkpoint)
        {
            if (!source_cancel_requested(target_binary))
                return false;

            const std::string message =
                "[WARN] Source update canceled by operator at "
                + std::string{ checkpoint } + ".";
            append_log_line(log_path, message);
            append_log_line(update_handoff_log_path_for(target_binary), message);
            log_info("Source update cancel reached " + std::string{ checkpoint } + ".");
            return true;
        }

        [[nodiscard]] inline std::string normalized_absolute_path_string(
            const std::filesystem::path& path)
        {
            std::error_code ec;
            auto absolute = std::filesystem::absolute(path, ec);
            if (ec)
            {
                ec.clear();
                absolute = path;
            }

            auto normalized = absolute.lexically_normal().string();
#if defined(_WIN32)
            normalized = lower_ascii(std::move(normalized));
#endif
            return normalized;
        }

        [[nodiscard]] inline bool path_is_inside(
            const std::filesystem::path& root,
            const std::filesystem::path& candidate)
        {
            std::string root_text = normalized_absolute_path_string(root);
            std::string candidate_text = normalized_absolute_path_string(candidate);
            if (root_text.empty() || candidate_text.empty())
                return false;

            while (!root_text.empty()
                && (root_text.back() == '\\' || root_text.back() == '/'))
            {
                root_text.pop_back();
            }

            if (candidate_text == root_text)
                return true;
            if (candidate_text.size() <= root_text.size())
                return false;
            if (candidate_text.compare(0u, root_text.size(), root_text) != 0)
                return false;

            const char separator = candidate_text[root_text.size()];
            return separator == '\\' || separator == '/';
        }

        inline void remove_update_cache_path_best_effort(const std::filesystem::path& path)
        {
            if (path.empty())
                return;

            std::error_code ec;
            if (std::filesystem::is_directory(path, ec))
            {
                std::filesystem::remove_all(path, ec);
                return;
            }

            ec.clear();
            std::filesystem::remove(path, ec);
        }

        inline void cleanup_stale_source_update_runs(
            const std::filesystem::path& target_binary,
            const std::filesystem::path& keep_run_dir = {})
        {
            const auto work_root = source_work_root(target_binary);
            std::error_code ec;
            if (!std::filesystem::exists(work_root, ec))
                return;

            const std::string keep_text = keep_run_dir.empty()
                ? std::string{}
                : normalized_absolute_path_string(keep_run_dir);

            for (std::filesystem::directory_iterator it{ work_root, ec }, end; it != end; it.increment(ec))
            {
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                const auto path = it->path();
                if (!keep_text.empty()
                    && normalized_absolute_path_string(path) == keep_text)
                {
                    continue;
                }

                const std::string name = path.filename().string();
                const bool managed_source_cache =
                    name == "sx"
                    || name == "src"
                    || name.starts_with("run_")
                    || name.starts_with(".stale")
                    || name.starts_with("source_snapshot");

                if (managed_source_cache && path_is_inside(work_root, path))
                    remove_update_cache_path_best_effort(path);
            }
        }

        inline void cleanup_stale_source_tool_runs(
            const std::filesystem::path& target_binary,
            const std::filesystem::path& keep_tools_dir = {})
        {
            const auto tools_root =
                target_binary.parent_path()
                / CACHE_ROOT_SUBDIR()
                / UPDATER_CACHE_SUBDIR()
                / "t";
            std::error_code ec;
            if (!std::filesystem::exists(tools_root, ec))
                return;

            const std::string keep_text = keep_tools_dir.empty()
                ? std::string{}
                : normalized_absolute_path_string(keep_tools_dir);

            for (std::filesystem::directory_iterator it{ tools_root, ec }, end; it != end; it.increment(ec))
            {
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                const auto path = it->path();
                if (!keep_text.empty()
                    && normalized_absolute_path_string(path) == keep_text)
                {
                    continue;
                }

                if (path_is_inside(tools_root, path) && std::filesystem::is_directory(path, ec))
                    remove_update_cache_path_best_effort(path);
            }
        }

        [[nodiscard]] inline std::optional<SourceActiveMarker> read_source_active_marker(
            const std::filesystem::path& target_binary)
        {
            const auto active_run_path = source_active_run_path(target_binary);
            const auto active_run_marker = read_text_file(active_run_path);
            const auto line_end = active_run_marker.find_first_of("\r\n");
            const auto active_run_text = trim_ascii(active_run_marker.substr(0u, line_end));
            if (active_run_text.empty())
                return std::nullopt;

            const auto work_root = source_work_root(target_binary);
            const std::filesystem::path active_run{ active_run_text };
            if (!path_is_inside(work_root, active_run))
                return std::nullopt;

            SourceActiveMarker marker{};
            marker.run_dir = active_run;

            const auto pid_marker = active_run_marker.find("pid=");
            if (pid_marker != std::string::npos)
            {
                const auto digits_begin = pid_marker + 4u;
                auto digits_end = digits_begin;
                while (digits_end < active_run_marker.size()
                    && std::isdigit(static_cast<unsigned char>(active_run_marker[digits_end])) != 0)
                {
                    ++digits_end;
                }

                if (digits_end > digits_begin)
                {
                    unsigned long parsed_pid{};
                    const auto* begin = active_run_marker.data() + digits_begin;
                    const auto* end = active_run_marker.data() + digits_end;
                    const auto [ptr, ec] = std::from_chars(begin, end, parsed_pid);
                    if (ec == std::errc{} && ptr == end && parsed_pid != 0ul)
                    {
                        marker.process_id = parsed_pid;
                        marker.has_process_id = true;
                    }
                }
            }

            return marker;
        }

        [[nodiscard]] inline bool source_update_active_marker_present(
            const std::filesystem::path& target_binary,
            std::filesystem::path* active_run_out = nullptr)
        {
            const auto marker = read_source_active_marker(target_binary);
            if (!marker)
                return false;
            if (active_run_out)
                *active_run_out = marker->run_dir;
            return true;
        }

        [[nodiscard]] inline bool source_update_active_run_exists(
            const std::filesystem::path& target_binary,
            std::filesystem::path* active_run_out = nullptr)
        {
            std::filesystem::path active_run;
            if (!source_update_active_marker_present(target_binary, &active_run))
                return false;

            std::error_code ec;
            if (!std::filesystem::exists(active_run, ec) || ec)
                return false;

            if (active_run_out)
                *active_run_out = active_run;
            return true;
        }

        [[nodiscard]] inline bool process_is_running(const unsigned long process_id) noexcept
        {
            if (process_id == 0ul)
                return false;
#if defined(_WIN32)
            HANDLE process = OpenProcess(
                SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                FALSE,
                static_cast<DWORD>(process_id));
            if (process == nullptr)
                return false;

            DWORD exit_code = 0u;
            const bool running =
                GetExitCodeProcess(process, &exit_code) != 0 && exit_code == STILL_ACTIVE;
            CloseHandle(process);
            return running;
#else
            return false;
#endif
        }

        [[nodiscard]] inline bool file_recently_modified(
            const std::filesystem::path& path,
            const std::chrono::seconds max_age) noexcept
        {
            std::error_code ec;
            const auto write_time = std::filesystem::last_write_time(path, ec);
            if (ec)
                return false;

            const auto now = std::filesystem::file_time_type::clock::now();
            return write_time >= now || (now - write_time) <= max_age;
        }

        [[nodiscard]] inline std::filesystem::path source_update_log_path_for(
            const std::filesystem::path& target_binary)
        {
            return ensure_directory(target_binary.parent_path() / "logs") / "epoch_source_update.log";
        }

        [[nodiscard]] inline std::filesystem::path update_handoff_log_path_for(
            const std::filesystem::path& target_binary)
        {
            return ensure_directory(target_binary.parent_path() / "logs") / "epoch_update_handoff.log";
        }

        [[nodiscard]] inline bool contains_text(
            const std::string_view text,
            const std::string_view needle) noexcept
        {
            return !needle.empty() && text.find(needle) != std::string_view::npos;
        }

        [[nodiscard]] inline bool source_update_terminal_evidence(
            const std::string_view source_log,
            const std::string_view handoff_log) noexcept
        {
            const auto has = [&](const std::string_view needle) noexcept
                {
                    return contains_text(source_log, needle) || contains_text(handoff_log, needle);
                };

            return has("[ERROR]")
                || has("Source update cancel complete")
                || has("Source update canceled by operator")
                || has("Built runtime ready at")
                || has("Waiting for runtime handoff")
                || has("Source runtime files copied successfully")
                || has("Restarted updated runtime");
        }

        [[nodiscard]] inline bool source_update_pending_log_evidence(
            const std::filesystem::path& target_binary)
        {
            const std::string source_log = read_text_file_tail(source_update_log_path_for(target_binary));
            const std::string handoff_log = read_text_file_tail(update_handoff_log_path_for(target_binary));
            if (source_log.empty() && handoff_log.empty())
                return false;

            if (source_update_terminal_evidence(source_log, handoff_log))
                return false;

            const auto has = [&](const std::string_view needle) noexcept
                {
                    return contains_text(source_log, needle) || contains_text(handoff_log, needle);
                };

            return has("Source update worker started")
                || has("Source rebuild cancellation requested")
                || has("Source update cancel requested")
                || has("Source snapshot download")
                || has("Downloading latest")
                || has("Downloading managed vcpkg")
                || has("Restoring source dependencies")
                || has("MSBuild attempt")
                || has("Waiting for target runtime unlock");
        }

        [[nodiscard]] inline bool source_update_recent_pending_log_evidence(
            const std::filesystem::path& target_binary,
            const std::chrono::seconds max_age = std::chrono::minutes{ 3 })
        {
            if (!source_update_pending_log_evidence(target_binary))
                return false;

            return file_recently_modified(source_update_log_path_for(target_binary), max_age)
                || file_recently_modified(update_handoff_log_path_for(target_binary), max_age);
        }

        [[nodiscard]] inline std::optional<std::filesystem::file_time_type> file_write_time_or_none(
            const std::filesystem::path& path) noexcept
        {
            std::error_code ec;
            const auto write_time = std::filesystem::last_write_time(path, ec);
            if (ec)
                return std::nullopt;

            return write_time;
        }

        [[nodiscard]] inline int source_update_recent_cancel_seconds_remaining(
            const std::filesystem::path& target_binary,
            const std::chrono::seconds cooldown)
        {
            if (cooldown.count() <= 0)
                return 0;

            const auto source_log_path = source_update_log_path_for(target_binary);
            const auto handoff_log_path = update_handoff_log_path_for(target_binary);
            const std::string source_log = read_text_file_tail(source_log_path);
            const std::string handoff_log = read_text_file_tail(handoff_log_path);

            const auto latest_line_has_cancel_terminal = [](const std::string_view text) noexcept
                {
                    std::size_t end = text.find_last_not_of(" \t\r\n");
                    while (end != std::string_view::npos)
                    {
                        const std::size_t start = text.find_last_of("\r\n", end);
                        const std::string_view line =
                            start == std::string_view::npos
                                ? text.substr(0, end + 1u)
                                : text.substr(start + 1u, end - start);
                        if (!line.empty())
                        {
                            return contains_text(line, "Source update cancel complete")
                                || contains_text(line, "Source update canceled by operator");
                        }

                        if (start == std::string_view::npos || start == 0u)
                            break;
                        end = text.find_last_not_of(" \t\r\n", start - 1u);
                    }

                    return false;
                };

            const bool source_cancel_terminal = latest_line_has_cancel_terminal(source_log);
            const bool handoff_cancel_terminal = latest_line_has_cancel_terminal(handoff_log);
            if (!source_cancel_terminal && !handoff_cancel_terminal)
                return 0;

            std::optional<std::filesystem::file_time_type> newest_cancel_write{};
            const auto note_write_time = [&](const std::filesystem::path& path)
                {
                    if (const auto write_time = file_write_time_or_none(path))
                    {
                        if (!newest_cancel_write || *write_time > *newest_cancel_write)
                            newest_cancel_write = write_time;
                    }
                };

            if (source_cancel_terminal)
                note_write_time(source_log_path);
            if (handoff_cancel_terminal)
                note_write_time(handoff_log_path);
            if (!newest_cancel_write)
                return 0;

            const auto now = std::filesystem::file_time_type::clock::now();
            const auto elapsed = *newest_cancel_write >= now
                ? std::chrono::seconds{ 0 }
                : std::chrono::duration_cast<std::chrono::milliseconds>(now - *newest_cancel_write);
            const auto elapsed_seconds =
                std::chrono::duration_cast<std::chrono::seconds>(elapsed);
            const int remaining = static_cast<int>(cooldown.count() - elapsed_seconds.count());

            return (std::max)(0, remaining);
        }

        [[nodiscard]] inline bool source_update_active_marker_live(
            const std::filesystem::path& target_binary,
            std::filesystem::path* active_run_out = nullptr)
        {
            const auto marker = read_source_active_marker(target_binary);
            if (!marker)
                return false;

            std::error_code ec;
            const bool run_exists = std::filesystem::exists(marker->run_dir, ec) && !ec;

            if (marker->has_process_id)
            {
                if (process_is_running(marker->process_id))
                {
                    if (active_run_out)
                        *active_run_out = marker->run_dir;
                    return true;
                }

                remove_update_cache_path_best_effort(source_active_run_path(target_binary));
                if (!source_update_recent_pending_log_evidence(target_binary, std::chrono::seconds{ 20 }))
                    remove_update_cache_path_best_effort(source_cancel_path(target_binary));
                return false;
            }

            const auto active_marker_path = source_active_run_path(target_binary);
            if (!run_exists)
            {
                remove_update_cache_path_best_effort(active_marker_path);
                if (!source_update_recent_pending_log_evidence(target_binary, std::chrono::seconds{ 20 }))
                    remove_update_cache_path_best_effort(source_cancel_path(target_binary));
                return false;
            }

            const bool old_marker_recent =
                file_recently_modified(active_marker_path, std::chrono::seconds{ 15 })
                || source_update_recent_pending_log_evidence(target_binary, std::chrono::minutes{ 3 });
            if (!old_marker_recent)
            {
                remove_update_cache_path_best_effort(active_marker_path);
                if (!source_update_recent_pending_log_evidence(target_binary, std::chrono::seconds{ 20 }))
                    remove_update_cache_path_best_effort(source_cancel_path(target_binary));
                return false;
            }

            if (active_run_out)
                *active_run_out = marker->run_dir;
            return true;
        }

        [[nodiscard]] inline bool source_update_session_active(
            const std::filesystem::path& target_binary)
        {
            return source_update_active_marker_live(target_binary);
        }

        [[nodiscard]] inline bool source_update_cancellation_pending(
            const std::filesystem::path& target_binary)
        {
            const bool cancel_marker_present = source_cancel_requested(target_binary);
            const std::string source_log = read_text_file_tail(source_update_log_path_for(target_binary));
            const std::string handoff_log = read_text_file_tail(update_handoff_log_path_for(target_binary));
            if (source_update_terminal_evidence(source_log, handoff_log))
                return false;

            if (cancel_marker_present)
            {
                return source_update_active_marker_live(target_binary);
            }

            return (contains_text(source_log, "Source rebuild cancellation requested")
                    || contains_text(handoff_log, "Source rebuild cancellation requested"))
                && source_update_active_marker_live(target_binary);
        }

        [[nodiscard]] inline std::filesystem::path make_temp_download_path(const std::string_view stem)
        {
            static std::atomic<unsigned long long> s_counter{ 0 };

            auto temp_root = ensure_directory(updater_cache_root() / "tmp");

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

        [[nodiscard]] inline std::filesystem::path staged_update_handoff_script_path()
        {
#if defined(_WIN32)
            return updater_cache_root() / "epoch_staged_update_handoff.bat";
#else
            return updater_cache_root() / "epoch_staged_update_handoff.sh";
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
            const bool hidden,
            const std::filesystem::path& log_path = {})
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
                log_path,
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
#if defined(_WIN32)
            return source_root / SOURCE_BUILD_PLATFORM() / SOURCE_BUILD_CONFIGURATION();
#else
            const auto manifest_root = source_root / SOURCE_MANIFEST_ROOT_NAME();
            const std::array<std::filesystem::path, 2> candidates{
                manifest_root / "Bin" / "Clang-Release",
                manifest_root / "Bin" / "GCC-Release",
            };

            std::error_code ec;
            for (const auto& candidate : candidates)
            {
                if (std::filesystem::exists(candidate, ec))
                    return candidate;
                ec.clear();
            }

            return candidates.front();
#endif
        }

        [[nodiscard]] inline std::filesystem::path resolve_runtime_binary_path(
            const std::filesystem::path& runtime_dir,
            const std::filesystem::path& target_binary)
        {
#if defined(_WIN32)
            const std::array<std::filesystem::path, 4> names{
                target_binary.filename(),
                "EpochEditor.exe",
                "ConsoleApplication1.exe",
                "epoch.exe",
            };
#else
            const std::array<std::filesystem::path, 2> names{
                target_binary.filename(),
                "epoch",
            };
#endif

            std::error_code ec;
            for (const auto& name : names)
            {
                const auto candidate = runtime_dir / name;
                if (!candidate.empty() && std::filesystem::exists(candidate, ec))
                    return candidate;
                ec.clear();
            }

            if (std::filesystem::exists(runtime_dir, ec) && std::filesystem::is_directory(runtime_dir, ec))
            {
                for (std::filesystem::recursive_directory_iterator it{ runtime_dir, ec }, end; it != end; it.increment(ec))
                {
                    if (ec)
                    {
                        ec.clear();
                        continue;
                    }

                    if (!it->is_regular_file(ec))
                    {
                        ec.clear();
                        continue;
                    }

                    const auto filename = it->path().filename();
                    if (std::find(names.begin(), names.end(), filename) != names.end())
                        return it->path();
                }
            }

            return runtime_dir / target_binary.filename();
        }

        [[nodiscard]] inline std::filesystem::path source_runtime_binary_path(
            const std::filesystem::path& source_root,
            const std::filesystem::path& target_binary)
        {
            return resolve_runtime_binary_path(source_runtime_output_dir(source_root), target_binary);
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

            return extract_version_string(PROJECT_SOURCE_VERSION);
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
            const auto build_log = source_update_log_path_for(target_binary);

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
            append_log_line(
                build_log,
                "[INFO] Source updater build lane: "
                + SOURCE_BUILD_CONFIGURATION()
                + "|"
                + SOURCE_BUILD_PLATFORM()
                + " (runtime may currently be Debug).");

            const bool using_managed_vcpkg =
                path_is_inside(managed_tools_root(), vcpkg_root);
            if (using_managed_vcpkg
                && !prepare_manifest_for_managed_vcpkg_registry(
                    manifest_root,
                    vcpkg_root,
                    build_log))
            {
                log_error("Could not prepare the source snapshot for managed vcpkg.");
                return false;
            }
            if (!using_managed_vcpkg)
            {
                append_log_line(
                    build_log,
                    "[INFO] Using installed vcpkg registry; source manifest baseline left intact.");
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
                log_error("vcpkg dependency restore failed. See logs/epoch_source_update.log for details.");
                return false;
            }

            log_info("MSBuild: " + msbuild.string());
            append_log_line(build_log, "[INFO] Building updated runtime with MSBuild.");

            const auto vcpkg_install_root = manifest_root / "vcpkg_installed";

            const std::vector<std::string> msbuild_args{
                solution.string(),
                "/t:" + SOURCE_BUILD_TARGET(),
                "/p:Configuration=" + SOURCE_BUILD_CONFIGURATION(),
                "/p:Platform=" + SOURCE_BUILD_PLATFORM(),
                "/p:VcpkgRoot=" + vcpkg_root.string(),
                "/p:VcpkgManifestRoot=" + manifest_root.string(),
                "/p:VcpkgInstalledDir=" + vcpkg_install_root.string(),
                "/p:VcpkgManifestInstall=false",
                "/p:VcpkgTriplet=" + SOURCE_BUILD_PLATFORM() + "-windows",
                "/p:UseMultiToolTask=false",
                "/p:BuildInParallel=false",
                "/m:1",
                "/nr:false",
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
                log_error("Source build failed. See logs/epoch_source_update.log for details.");
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
            const auto manifest_root = source_manifest_root(source_root);
            const auto build_log = source_update_log_path_for(target_binary);
            const auto build_script = manifest_root / "build.sh";

            if (!std::filesystem::exists(build_script))
            {
                log_error("Extracted source snapshot does not contain the Linux build script.");
                return false;
            }

            append_log_line(build_log, "[INFO] Source update build started");
            append_log_line(build_log, "[INFO] Source root: " + source_root.string());
            append_log_line(build_log, "[INFO] Manifest root: " + manifest_root.string());
            append_log_line(build_log, "[INFO] Build script: " + build_script.string());

            if (stop_source_update_if_canceled(target_binary, build_log, "Linux dependency preparation"))
                return false;

            const auto vcpkg_exe = find_or_prepare_vcpkg(manifest_root, build_log);
            if (vcpkg_exe.empty())
            {
                append_log_line(build_log, "[ERROR] Could not locate or prepare vcpkg for the Linux source update.");
                log_error("Could not prepare vcpkg for the Linux source update.");
                return false;
            }

            const auto vcpkg_root = vcpkg_exe.parent_path();
            const bool using_managed_vcpkg = path_is_inside(managed_tools_root(), vcpkg_root);
            append_log_line(build_log, "[INFO] Linux source update vcpkg root: " + vcpkg_root.string());

            if (stop_source_update_if_canceled(target_binary, build_log, "managed vcpkg selection"))
                return false;

            if (using_managed_vcpkg
                && !prepare_manifest_for_managed_vcpkg_registry(
                    manifest_root,
                    vcpkg_root,
                    build_log))
            {
                append_log_line(build_log, "[ERROR] Could not prepare the Linux source snapshot for managed vcpkg.");
                log_error("Could not prepare the Linux source snapshot for managed vcpkg.");
                return false;
            }

            if (stop_source_update_if_canceled(target_binary, build_log, "managed vcpkg registry preparation"))
                return false;

            if (!using_managed_vcpkg)
            {
                append_log_line(
                    build_log,
                    "[INFO] Using installed vcpkg registry for the Linux source update.");
            }

            const auto overlay_ports = prepare_vcpkg_overlay_ports(vcpkg_root, build_log);
            if (stop_source_update_if_canceled(target_binary, build_log, "vcpkg overlay preparation"))
                return false;
            const auto run_linux_build =
                [&](const std::string& compiler_choice, const std::filesystem::path& expected_dir) -> bool
                {
                    append_log_line(
                        build_log,
                        "[INFO] Linux source build attempt started with compiler '" + compiler_choice + "'.");

                    std::vector<std::string> build_args{
                        build_script.string(),
                        "--bootstrap-current-toolchain",
                        "--tool-cache-root",
                        managed_tools_root().string(),
                        "--cancel-file",
                        source_cancel_path(target_binary).string(),
                        "--vcpkg-root",
                        vcpkg_root.string()
                    };
                    if (!overlay_ports.empty())
                    {
                        build_args.push_back("--vcpkg-overlay-ports");
                        build_args.push_back(overlay_ports.string());
                    }
                    build_args.push_back(compiler_choice);
                    build_args.push_back(SOURCE_BUILD_CONFIGURATION());

                    int build_exit = -1;
                    const bool launched = run_process_hidden(
                        std::filesystem::path{ "/bin/bash" },
                        build_args,
                        manifest_root,
                        build_log,
                        true,
                        &build_exit);

                    if (!launched)
                    {
                        append_log_line(
                            build_log,
                            "[WARN] Failed to launch Linux source build with compiler '" + compiler_choice + "'.");
                        return false;
                    }

                    if (build_exit != 0)
                    {
                        append_log_line(
                            build_log,
                            "[WARN] Linux source build with compiler '" + compiler_choice
                            + "' exited with code " + std::to_string(build_exit) + ".");
                        return false;
                    }

                    const auto built_binary = resolve_runtime_binary_path(expected_dir, target_binary);
                    if (!std::filesystem::exists(built_binary))
                    {
                        append_log_line(
                            build_log,
                            "[WARN] Linux source build with compiler '" + compiler_choice
                            + "' completed without producing the runtime binary.");
                        return false;
                    }

                    append_log_line(
                        build_log,
                        "[INFO] Linux source build completed successfully with compiler '" + compiler_choice + "'.");
                    return true;
                };

            const bool build_ok = run_linux_build(
                "clang",
                manifest_root / "Bin" / "Clang-Release");

            if (!build_ok)
            {
                append_log_line(build_log, "[WARN] Linux source updater does not fall back to GCC after a Clang full-engine build failure.");
            }

            if (!build_ok)
            {
                append_log_line(build_log, "[ERROR] Linux source build failed.");
                log_error("Linux source build failed. See logs/epoch_source_update.log for details.");
                return false;
            }

            const auto built_binary = source_runtime_binary_path(source_root, target_binary);
            if (!std::filesystem::exists(built_binary))
            {
                append_log_line(build_log, "[ERROR] Linux source build completed without a runtime binary.");
                log_error("Linux source build completed without producing the runtime binary.");
                return false;
            }

            append_log_line(build_log, "[INFO] Source build completed successfully.");
            return true;
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
        bool packaged_update_performed{ false };
        bool packaged_handoff_staged{ false };
        bool source_update_performed{ false };
        bool packaged_release_checked{ false };
        bool packaged_release_found{ false };
        bool packaged_release_missing{ false };
        bool source_fallback_attempted{ false };
        std::string local_version;
        std::string remote_version;
        std::string source_local_version;
        std::string source_remote_version;
        bool source_update_available{ false };
        bool platform_build_checked{ false };
        bool platform_build_ok{ false };
        bool platform_build_pending{ false };
        std::string platform_build_job;
        std::string platform_build_status;
        std::string platform_build_conclusion;
        std::string platform_build_url;
        std::string platform_build_reason;
        std::string packaged_release_reason;
        std::string status_message;
    };

    export struct ProjectSourceDownloadResult
    {
        bool ok{ false };
        std::filesystem::path project_root{};
        std::string status_message{};
    };

    export enum class UpdateHandoffMode
    {
        LaunchImmediately,
        StageForRestart
    };

    export struct UpdateChannel
    {
        std::string version_url;
        std::string binary_url;
        std::string source_url;
        std::string source_version_url;
        std::string platform_build_status_url;
        std::string platform_build_job_name;
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

        const auto run_token = system_detail::make_source_update_run_token();
        const auto work_root = system_detail::source_work_root(target_binary);
        const auto run_dir = system_detail::source_run_dir(target_binary, run_token);
        const auto archive_path = system_detail::source_archive_path(target_binary, channel.source_url, run_token);
        const auto staging_dir = system_detail::source_staging_dir(target_binary, run_token);
        const auto final_dir = system_detail::source_final_dir(target_binary, run_token);
        const auto target_dir = target_binary.parent_path();
        const auto build_log = system_detail::source_update_log_path_for(target_binary);
        const auto handoff_log = system_detail::update_handoff_log_path_for(target_binary);
        const auto cancel_path = system_detail::source_cancel_path(target_binary);
        const auto active_run_path = system_detail::source_active_run_path(target_binary);
        const auto worker_script = system_detail::make_temp_powershell_script_path("source_update_worker");
        const auto built_runtime_dir = system_detail::source_runtime_output_dir(final_dir);
        const auto built_binary = system_detail::source_runtime_binary_path(final_dir, target_binary);
        const auto manifest_root = system_detail::source_manifest_root(final_dir);
        const auto solution = system_detail::source_solution_path(final_dir);
        const auto target_assets_dir = target_dir / "assets";
        const auto built_assets_dir = built_runtime_dir / "assets";
        const auto source_repo_assets_dir = manifest_root / "assets";
        const auto managed_tools_root = system_detail::source_run_tools_root(target_binary, run_token);
        const auto source_fallback_urls = PROJECT_SOURCE_FALLBACK_URLS();

        std::filesystem::path existing_active_run;
        if (system_detail::source_update_session_active(target_binary))
        {
            const bool has_active_run =
                system_detail::source_update_active_run_exists(target_binary, &existing_active_run);
            system_detail::append_log_line(
                handoff_log,
                has_active_run
                    ? "[WARN] Existing source update worker is still active at: " + existing_active_run.string()
                    : "[WARN] Existing source update session is still settling; not launching a second worker.");
            system_detail::log_info("Source update worker launch skipped because an existing worker is still active.");
            return true;
        }

        {
            std::error_code cleanup_ec;
            system_detail::cleanup_stale_source_update_runs(target_binary, run_dir);
            system_detail::cleanup_stale_source_tool_runs(target_binary, managed_tools_root);
            cleanup_ec.clear();
            std::filesystem::remove(cancel_path, cleanup_ec);
            cleanup_ec.clear();
            std::filesystem::remove(build_log, cleanup_ec);
            cleanup_ec.clear();
            std::filesystem::remove(handoff_log, cleanup_ec);
            cleanup_ec.clear();
            std::filesystem::remove(system_detail::staged_update_handoff_script_path(), cleanup_ec);
        }

        {
            std::error_code active_ec;
            std::filesystem::create_directories(run_dir, active_ec);
            if (active_ec)
            {
                system_detail::log_error("Failed to prepare source update run directory: " + run_dir.string());
                return false;
            }

            active_ec.clear();
            std::filesystem::create_directories(active_run_path.parent_path(), active_ec);
            if (active_ec)
            {
                system_detail::remove_update_cache_path_best_effort(run_dir);
                system_detail::log_error("Failed to prepare source update marker directory: " + active_run_path.parent_path().string());
                return false;
            }

            std::ofstream active_run(active_run_path, std::ios::binary | std::ios::trunc);
            if (active_run)
                active_run << run_dir.string() << '\n';
            else
                system_detail::log_error("Failed to write source update active-run marker; Cancel will still signal the worker.");
        }

        std::ofstream ps(worker_script, std::ios::binary);
        if (!ps)
        {
            std::error_code marker_ec;
            std::filesystem::remove(active_run_path, marker_ec);
            system_detail::remove_update_cache_path_best_effort(run_dir);
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
            << "$sourceFallbackUrls = @(\n";

        for (const auto& fallback_url : source_fallback_urls)
        {
            if (!fallback_url.empty() && fallback_url != channel.source_url)
                ps << "  '" << esc(fallback_url) << "'\n";
        }

        ps
            << ")\n"
            << "$workRoot = '" << esc(work_root.string()) << "'\n"
            << "$runDir = '" << esc(run_dir.string()) << "'\n"
            << "$sourceArchive = '" << esc(archive_path.string()) << "'\n"
            << "$stagingDir = '" << esc(staging_dir.string()) << "'\n"
            << "$sourceRoot = '" << esc(final_dir.string()) << "'\n"
            << "$manifestRoot = '" << esc(manifest_root.string()) << "'\n"
            << "$solution = '" << esc(solution.string()) << "'\n"
            << "$buildLog = '" << esc(build_log.string()) << "'\n"
            << "$handoffLog = '" << esc(handoff_log.string()) << "'\n"
            << "$cancelPath = '" << esc(cancel_path.string()) << "'\n"
            << "$activeRunPath = '" << esc(active_run_path.string()) << "'\n"
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
            << "$workerHidden = $" << (silent_worker ? "true" : "false") << "\n"
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
            << "function Append-ToolTextToBuildLog([string]$Text) {\n"
            << "  if ([string]::IsNullOrEmpty($Text)) {\n"
            << "    return\n"
            << "  }\n"
            << "  if (-not $Text.EndsWith([Environment]::NewLine)) {\n"
            << "    $Text += [Environment]::NewLine\n"
            << "  }\n"
            << "  Append-Text $buildLog $Text\n"
            << "}\n"
            << "function Complete-ToolOutput($StdoutTask, $StderrTask) {\n"
            << "  try { Append-ToolTextToBuildLog ([string]$StdoutTask.Result) } catch { }\n"
            << "  try { Append-ToolTextToBuildLog ([string]$StderrTask.Result) } catch { }\n"
            << "}\n"
            << "function Write-Step([string]$Level, [string]$Message) {\n"
            << "  $line = \"$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') [$Level] $Message\"\n"
            << "  if (-not $workerHidden) { Write-Host $line }\n"
            << "  Append-Text $buildLog ($line + [Environment]::NewLine)\n"
            << "}\n"
            << "function Write-Handoff([string]$Level, [string]$Message) {\n"
            << "  $line = \"$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') [$Level] $Message\"\n"
            << "  Append-Text $handoffLog ($line + [Environment]::NewLine)\n"
            << "}\n"
            << "function Invoke-DownloadWithFallback([string]$PrimaryUrl, [string[]]$FallbackUrls, [string]$OutFile, [string]$Label) {\n"
            << "  $headers = @{ 'User-Agent' = 'EpochUpdater/1.0'; 'Accept' = 'application/octet-stream, application/vnd.github+json' }\n"
            << "  $urls = New-Object System.Collections.Generic.List[string]\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($PrimaryUrl)) { $urls.Add($PrimaryUrl) }\n"
            << "  foreach ($fallbackUrl in $FallbackUrls) {\n"
            << "    if ([string]::IsNullOrWhiteSpace($fallbackUrl)) { continue }\n"
            << "    $duplicate = $false\n"
            << "    foreach ($existingUrl in $urls) {\n"
            << "      if ([string]::Equals($existingUrl, $fallbackUrl, [System.StringComparison]::OrdinalIgnoreCase)) {\n"
            << "        $duplicate = $true\n"
            << "        break\n"
            << "      }\n"
            << "    }\n"
            << "    if (-not $duplicate) { $urls.Add($fallbackUrl) }\n"
            << "  }\n"
            << "  $lastError = ''\n"
            << "  foreach ($candidateUrl in $urls) {\n"
            << "    try {\n"
            << "      Remove-Item -LiteralPath $OutFile -Force -ErrorAction SilentlyContinue\n"
            << "      Write-Step 'INFO' ($Label + ' URL: ' + $candidateUrl)\n"
            << "      Invoke-WebRequest -UseBasicParsing -Headers $headers -Uri $candidateUrl -OutFile $OutFile\n"
            << "      if ((Test-Path -LiteralPath $OutFile) -and ((Get-Item -LiteralPath $OutFile).Length -gt 0)) {\n"
            << "        Write-Step 'INFO' ($Label + ' downloaded successfully.')\n"
            << "        return\n"
            << "      }\n"
            << "      $lastError = 'downloaded file was empty'\n"
            << "      Write-Step 'WARN' ($Label + ' produced an empty file from ' + $candidateUrl + '.')\n"
            << "    }\n"
            << "    catch {\n"
            << "      $lastError = $_.Exception.Message\n"
            << "      if ([string]::IsNullOrWhiteSpace($lastError)) { $lastError = $_.ToString() }\n"
            << "      Write-Step 'WARN' ($Label + ' failed from ' + $candidateUrl + ': ' + $lastError)\n"
            << "    }\n"
            << "  }\n"
            << "  throw ($Label + ' failed from all configured URLs. Last error: ' + $lastError)\n"
            << "}\n"
            << "function Test-Cancel {\n"
            << "  if (Test-Path -LiteralPath $cancelPath) {\n"
            << "    Write-Step 'WARN' 'Source update cancel requested by operator.'\n"
            << "    Write-Handoff 'WARN' 'Source update cancel requested before runtime replacement.'\n"
            << "    Remove-Item -LiteralPath $cancelPath -Force -ErrorAction SilentlyContinue\n"
            << "    Write-Step 'WARN' 'Source update cancel complete.'\n"
            << "    Write-Handoff 'WARN' 'Source update cancel complete.'\n"
            << "    Remove-Item -LiteralPath $activeRunPath -Force -ErrorAction SilentlyContinue\n"
            << "    Remove-Item -LiteralPath $workerPath -Force -ErrorAction SilentlyContinue\n"
            << "    exit 130\n"
            << "  }\n"
            << "}\n"
            << "function Clear-StaleSourceRuns {\n"
            << "  if (-not (Test-Path -LiteralPath $workRoot)) { return }\n"
            << "  $runFull = [System.IO.Path]::GetFullPath($runDir)\n"
            << "  Get-ChildItem -LiteralPath $workRoot -Force -ErrorAction SilentlyContinue | ForEach-Object {\n"
            << "    $name = $_.Name\n"
            << "    $managed = ($name -eq 'sx' -or $name -eq 'src' -or $name.StartsWith('run_') -or $name.StartsWith('.stale') -or $name.StartsWith('source_snapshot'))\n"
            << "    if ($managed) {\n"
            << "      $itemFull = [System.IO.Path]::GetFullPath($_.FullName)\n"
            << "      if (-not [string]::Equals($itemFull, $runFull, [System.StringComparison]::OrdinalIgnoreCase)) {\n"
            << "        Remove-Item -LiteralPath $_.FullName -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "      }\n"
            << "    }\n"
            << "  }\n"
            << "}\n"
            << "trap {\n"
            << "  $message = $_.Exception.Message\n"
            << "  if ([string]::IsNullOrWhiteSpace($message)) {\n"
            << "    $message = $_.ToString()\n"
            << "  }\n"
            << "  Write-Step 'ERROR' $message\n"
            << "  Write-Handoff 'ERROR' $message\n"
            << "  Remove-Item -LiteralPath $sourceArchive -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $stagingDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $sourceRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $activeRunPath -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $workerPath -Force -ErrorAction SilentlyContinue\n"
            << "  Start-Sleep -Milliseconds 100\n"
            << "  Remove-Item -LiteralPath $runDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  exit 1\n"
            << "}\n"
            << "function Join-ProcessArguments([string[]]$Arguments) {\n"
            << "  $parts = New-Object System.Collections.Generic.List[string]\n"
            << "  foreach ($argument in $Arguments) {\n"
            << "    if ($null -eq $argument) { continue }\n"
            << "    if ($argument.Length -eq 0) { $parts.Add('\"\"'); continue }\n"
            << "    if ($argument.IndexOfAny([char[]]@(' ', \"`t\", '\"')) -lt 0) { $parts.Add($argument); continue }\n"
            << "    $escaped = $argument.Replace('\\\\', '\\\\').Replace('\"', '\\\"')\n"
            << "    $parts.Add(('\"' + $escaped + '\"'))\n"
            << "  }\n"
            << "  return [string]::Join(' ', $parts)\n"
            << "}\n"
            << "function Stop-ProcessTree([int]$RootPid) {\n"
            << "  try {\n"
            << "    $all = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue\n"
            << "    $children = @{}\n"
            << "    foreach ($proc in $all) {\n"
            << "      if (-not $children.ContainsKey($proc.ParentProcessId)) { $children[$proc.ParentProcessId] = @() }\n"
            << "      $children[$proc.ParentProcessId] += $proc\n"
            << "    }\n"
            << "    $ids = New-Object System.Collections.Generic.List[int]\n"
            << "    function Add-ChildTree([int]$ProcessIdToAdd) {\n"
            << "      $ids.Add($ProcessIdToAdd) | Out-Null\n"
            << "      if ($children.ContainsKey($ProcessIdToAdd)) {\n"
            << "        foreach ($child in $children[$ProcessIdToAdd]) { Add-ChildTree $child.ProcessId }\n"
            << "      }\n"
            << "    }\n"
            << "    Add-ChildTree $RootPid\n"
            << "    foreach ($id in ($ids | Sort-Object -Descending)) {\n"
            << "      Stop-Process -Id $id -Force -ErrorAction SilentlyContinue\n"
            << "    }\n"
            << "  }\n"
            << "  catch {\n"
            << "    Stop-Process -Id $RootPid -Force -ErrorAction SilentlyContinue\n"
            << "  }\n"
            << "}\n"
            << "function Invoke-Tool([string]$FilePath, [string[]]$Arguments, [string]$WorkingDir, [string]$StepName, [string]$ExpectedOutputPath = '') {\n"
            << "  $toolProcess = $null\n"
            << "  $stdoutTask = $null\n"
            << "  $stderrTask = $null\n"
            << "  Push-Location $WorkingDir\n"
            << "  try {\n"
            << "    $argumentText = Join-ProcessArguments $Arguments\n"
            << "    $launchFilePath = $FilePath\n"
            << "    $launchArguments = $argumentText\n"
            << "    $extension = [System.IO.Path]::GetExtension($FilePath)\n"
            << "    if ([string]::Equals($extension, '.bat', [System.StringComparison]::OrdinalIgnoreCase) -or [string]::Equals($extension, '.cmd', [System.StringComparison]::OrdinalIgnoreCase)) {\n"
            << "      $comspec = [System.Environment]::GetEnvironmentVariable('ComSpec')\n"
            << "      if ([string]::IsNullOrWhiteSpace($comspec)) { $comspec = 'C:\\Windows\\System32\\cmd.exe' }\n"
            << "      $launchFilePath = $comspec\n"
            << "      $launchArguments = '/d /s /c \"\"' + $FilePath + '\"'\n"
            << "      if (-not [string]::IsNullOrWhiteSpace($argumentText)) { $launchArguments += ' ' + $argumentText }\n"
            << "      $launchArguments += '\"'\n"
            << "    }\n"
            << "    $startInfo = New-Object System.Diagnostics.ProcessStartInfo\n"
            << "    $startInfo.FileName = $launchFilePath\n"
            << "    $startInfo.Arguments = $launchArguments\n"
            << "    $startInfo.WorkingDirectory = $WorkingDir\n"
            << "    $startInfo.UseShellExecute = $false\n"
            << "    $startInfo.RedirectStandardOutput = $true\n"
            << "    $startInfo.RedirectStandardError = $true\n"
            << "    $startInfo.CreateNoWindow = $true\n"
            << "    $toolProcess = New-Object System.Diagnostics.Process\n"
            << "    $toolProcess.StartInfo = $startInfo\n"
            << "    if (-not $toolProcess.Start()) { throw ($StepName + ' failed to launch.') }\n"
            << "    $stdoutTask = $toolProcess.StandardOutput.ReadToEndAsync()\n"
            << "    $stderrTask = $toolProcess.StandardError.ReadToEndAsync()\n"
            << "    while (-not $toolProcess.WaitForExit(1000)) {\n"
            << "      if (Test-Path -LiteralPath $cancelPath) {\n"
            << "        Write-Step 'WARN' ($StepName + ' canceled; stopping child process tree.')\n"
            << "        Stop-ProcessTree $toolProcess.Id\n"
            << "        Start-Sleep -Milliseconds 250\n"
            << "        try { $toolProcess.WaitForExit(5000) | Out-Null } catch { }\n"
            << "        Complete-ToolOutput $stdoutTask $stderrTask\n"
            << "        Test-Cancel\n"
            << "      }\n"
            << "    }\n"
            << "    try { $toolProcess.WaitForExit() } catch { }\n"
            << "    Complete-ToolOutput $stdoutTask $stderrTask\n"
            << "    try { $toolProcess.Refresh() } catch { }\n"
            << "    $toolExitCode = $toolProcess.ExitCode\n"
            << "    $toolExitCodeText = [string]$toolExitCode\n"
            << "    if ([string]::IsNullOrWhiteSpace($toolExitCodeText)) {\n"
            << "      if (-not [string]::IsNullOrWhiteSpace($ExpectedOutputPath) -and (Test-Path -LiteralPath $ExpectedOutputPath)) {\n"
            << "        Write-Step 'WARN' ($StepName + ' did not report an exit code, but expected output exists; continuing.')\n"
            << "        Write-Handoff 'WARN' ($StepName + ' completed without an exit code; verified expected output exists.')\n"
            << "        return\n"
            << "      }\n"
            << "      throw ($StepName + ' did not report an exit code.')\n"
            << "    }\n"
            << "    if ($toolExitCode -ne 0) {\n"
            << "      throw ($StepName + ' failed with exit code ' + $toolExitCode + '.')\n"
            << "    }\n"
            << "  }\n"
            << "  finally {\n"
            << "    Pop-Location\n"
            << "    if ($null -ne $toolProcess) { try { $toolProcess.Dispose() } catch { } }\n"
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
            << "function Test-VcpkgRoot([string]$Root) {\n"
            << "  if ([string]::IsNullOrWhiteSpace($Root)) { return $false }\n"
            << "  try { $fullRoot = [System.IO.Path]::GetFullPath($Root) } catch { return $false }\n"
            << "  if (-not (Test-Path -LiteralPath (Join-Path $fullRoot $vcpkgExeName))) { return $false }\n"
            << "  if (-not (Test-Path -LiteralPath (Join-Path $fullRoot 'ports'))) { return $false }\n"
            << "  if (-not (Test-Path -LiteralPath (Join-Path $fullRoot 'versions'))) { return $false }\n"
            << "  if (-not (Test-Path -LiteralPath (Join-Path $fullRoot 'scripts\\buildsystems\\vcpkg.cmake'))) { return $false }\n"
            << "  return $true\n"
            << "}\n"
            << "function Resolve-InstalledVcpkgExe {\n"
            << "  $candidates = @()\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_EXE_PATH) -and (Test-Path -LiteralPath $env:VCPKG_EXE_PATH)) {\n"
            << "    $candidates += (Split-Path -Parent $env:VCPKG_EXE_PATH)\n"
            << "  }\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($env:EPOCH_UPDATER_VCPKG_ROOT)) { $candidates += $env:EPOCH_UPDATER_VCPKG_ROOT }\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) { $candidates += $env:VCPKG_ROOT }\n"
            << "  $vcpkgCommand = Get-Command $vcpkgExeName -ErrorAction SilentlyContinue\n"
            << "  if ($null -ne $vcpkgCommand -and -not [string]::IsNullOrWhiteSpace($vcpkgCommand.Source)) {\n"
            << "    $candidates += (Split-Path -Parent $vcpkgCommand.Source)\n"
            << "  }\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($env:USERPROFILE)) {\n"
            << "    $candidates += (Join-Path $env:USERPROFILE 'source\\repos\\vcpkg')\n"
            << "    $candidates += (Join-Path $env:USERPROFILE 'vcpkg')\n"
            << "  }\n"
            << "  $seen = @{}\n"
            << "  foreach ($candidate in $candidates) {\n"
            << "    if ([string]::IsNullOrWhiteSpace($candidate)) { continue }\n"
            << "    try { $fullCandidate = [System.IO.Path]::GetFullPath($candidate) } catch { continue }\n"
            << "    $key = $fullCandidate.ToLowerInvariant()\n"
            << "    if ($seen.ContainsKey($key)) { continue }\n"
            << "    $seen[$key] = $true\n"
            << "    if (Test-VcpkgRoot $fullCandidate) {\n"
            << "      Write-Step 'INFO' ('Using installed vcpkg: ' + $fullCandidate)\n"
            << "      Write-Handoff 'INFO' 'Using installed vcpkg toolchain.'\n"
            << "      return (Join-Path $fullCandidate $vcpkgExeName)\n"
            << "    }\n"
            << "    Write-Step 'WARN' ('Ignoring unusable vcpkg root: ' + $fullCandidate)\n"
            << "  }\n"
            << "  return ''\n"
            << "}\n"
            << "function Resolve-VcpkgExe {\n"
            << "  Test-Cancel\n"
            << "  $installedVcpkgExe = Resolve-InstalledVcpkgExe\n"
            << "  if (-not [string]::IsNullOrWhiteSpace($installedVcpkgExe) -and (Test-Path -LiteralPath $installedVcpkgExe)) {\n"
            << "    return $installedVcpkgExe\n"
            << "  }\n"
            << "  $vcpkgRef = Get-VcpkgRef\n"
            << "  $safeRef = Get-ShortToken $vcpkgRef\n"
            << "  $managedVcpkgRoot = Join-Path $managedToolsRoot ('v-' + $safeRef)\n"
            << "  $managedVcpkgExe = Join-Path $managedVcpkgRoot $vcpkgExeName\n"
            << "  if (Test-Path -LiteralPath $managedVcpkgExe) {\n"
            << "    Write-Step 'INFO' ('Using managed vcpkg: ' + $managedVcpkgRoot)\n"
            << "    Write-Handoff 'INFO' 'Using cached managed vcpkg toolchain.'\n"
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
            << "  Write-Handoff 'INFO' 'Downloading managed vcpkg toolchain.'\n"
            << "  $headers = @{ 'User-Agent' = 'EpochUpdater/1.0' }\n"
            << "  Test-Cancel\n"
            << "  try {\n"
            << "    Invoke-WebRequest -UseBasicParsing -Headers $headers -Uri (Get-VcpkgArchiveUrl $vcpkgRef) -OutFile $vcpkgArchive\n"
            << "    Test-Cancel\n"
            << "  }\n"
            << "  catch {\n"
            << "    if ($vcpkgRef -eq $vcpkgDefaultRef) {\n"
            << "      throw\n"
            << "    }\n"
            << "    Write-Step 'WARN' ('Managed vcpkg ref ' + $vcpkgRef + ' was not downloadable: ' + $_.Exception.Message + '. Falling back to ' + $vcpkgDefaultRef + '.')\n"
            << "    Write-Handoff 'WARN' 'Pinned vcpkg snapshot was unavailable; using the managed fallback toolchain.'\n"
            << "    $vcpkgRef = $vcpkgDefaultRef\n"
            << "    $safeRef = Get-ShortToken $vcpkgRef\n"
            << "    $managedVcpkgRoot = Join-Path $managedToolsRoot ('v-' + $safeRef)\n"
            << "    $managedVcpkgExe = Join-Path $managedVcpkgRoot $vcpkgExeName\n"
            << "    $vcpkgArchive = Join-Path $managedToolsRoot ('v-' + $safeRef + '.zip')\n"
            << "    $vcpkgStaging = Join-Path $managedToolsRoot ('vx-' + $safeRef)\n"
            << "    $bootstrapScript = Join-Path $managedVcpkgRoot $vcpkgBootstrapName\n"
            << "    Remove-Item -LiteralPath $vcpkgArchive -Force -ErrorAction SilentlyContinue\n"
            << "    Remove-Item -LiteralPath $vcpkgStaging -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "    Remove-Item -LiteralPath $managedVcpkgRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "    Write-Step 'INFO' ('Downloading managed vcpkg fallback (' + $vcpkgRef + ').')\n"
            << "    Write-Handoff 'INFO' 'Downloading managed vcpkg fallback toolchain.'\n"
            << "    Test-Cancel\n"
            << "    Invoke-WebRequest -UseBasicParsing -Headers $headers -Uri (Get-VcpkgArchiveUrl $vcpkgRef) -OutFile $vcpkgArchive\n"
            << "    Test-Cancel\n"
            << "  }\n"
            << "  Test-Cancel\n"
            << "  Expand-Archive -LiteralPath $vcpkgArchive -DestinationPath $vcpkgStaging -Force\n"
            << "  Test-Cancel\n"
            << "  $bootstrapCandidate = Get-ChildItem -LiteralPath $vcpkgStaging -Recurse -File -Filter $vcpkgBootstrapName -ErrorAction SilentlyContinue | Select-Object -First 1\n"
            << "  if ($null -eq $bootstrapCandidate) {\n"
            << "    Remove-Item -LiteralPath $managedVcpkgRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "    throw 'Managed vcpkg archive did not contain bootstrap-vcpkg.bat.'\n"
            << "  }\n"
            << "  $extractedRoot = Split-Path -Parent $bootstrapCandidate.FullName\n"
            << "  $stagingFull = [System.IO.Path]::GetFullPath($vcpkgStaging)\n"
            << "  $rootFull = [System.IO.Path]::GetFullPath($extractedRoot)\n"
            << "  if ([string]::Equals($stagingFull, $rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {\n"
            << "    New-Item -ItemType Directory -Path $managedVcpkgRoot -Force | Out-Null\n"
            << "    Copy-Item -Path (Join-Path $vcpkgStaging '*') -Destination $managedVcpkgRoot -Recurse -Force\n"
            << "  }\n"
            << "  else {\n"
            << "    New-Item -ItemType Directory -Path $managedVcpkgRoot -Force | Out-Null\n"
            << "    Copy-Item -Path (Join-Path $extractedRoot '*') -Destination $managedVcpkgRoot -Recurse -Force\n"
            << "  }\n"
            << "  Remove-Item -LiteralPath $vcpkgArchive -Force -ErrorAction SilentlyContinue\n"
            << "  Remove-Item -LiteralPath $vcpkgStaging -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  if (-not (Test-Path -LiteralPath $bootstrapScript)) {\n"
            << "    Remove-Item -LiteralPath $managedVcpkgRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "    throw 'Managed vcpkg bootstrap script is missing after extraction repair.'\n"
            << "  }\n"
            << "  Test-Cancel\n"
            << "  Write-Step 'INFO' 'Bootstrapping managed vcpkg.'\n"
            << "  Write-Handoff 'INFO' 'Bootstrapping managed vcpkg toolchain.'\n"
            << "  Invoke-Tool $bootstrapScript @('-disableMetrics') $managedVcpkgRoot 'vcpkg bootstrap' $managedVcpkgExe\n"
            << "  Test-Cancel\n"
            << "  if (-not (Test-Path -LiteralPath $managedVcpkgExe)) {\n"
            << "    throw 'Managed vcpkg bootstrap did not produce vcpkg.exe.'\n"
            << "  }\n"
            << "  Write-Step 'INFO' ('Managed vcpkg ready at: ' + $managedVcpkgExe)\n"
            << "  Write-Handoff 'INFO' 'Managed vcpkg toolchain is ready.'\n"
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
            << "  $gitDir = Join-Path $VcpkgRoot '.git'\n"
            << "  $head = ''\n"
            << "  if (Test-Path -LiteralPath $gitDir) {\n"
            << "    try {\n"
            << "      $resolvedGitDir = (& $gitExe '-C' $VcpkgRoot 'rev-parse' '--absolute-git-dir' 2>$null | Out-String).Trim()\n"
            << "      if (-not [string]::IsNullOrWhiteSpace($resolvedGitDir)) {\n"
            << "        $resolvedGitDir = [System.IO.Path]::GetFullPath($resolvedGitDir)\n"
            << "        $expectedGitDir = [System.IO.Path]::GetFullPath($gitDir)\n"
            << "        if ([string]::Equals($resolvedGitDir, $expectedGitDir, [System.StringComparison]::OrdinalIgnoreCase)) {\n"
            << "          $head = (& $gitExe ('--git-dir=' + $gitDir) ('--work-tree=' + $VcpkgRoot) 'rev-parse' '--verify' 'HEAD' 2>$null | Out-String).Trim()\n"
            << "        }\n"
            << "        else {\n"
            << "          Write-Step 'WARN' ('Managed vcpkg git resolution escaped the sandboxed repo: ' + $resolvedGitDir)\n"
            << "        }\n"
            << "      }\n"
            << "    }\n"
            << "    catch {\n"
            << "      $head = ''\n"
            << "    }\n"
            << "  }\n"
            << "  if ([string]::IsNullOrWhiteSpace($head)) {\n"
            << "    Write-Step 'INFO' 'Initializing managed vcpkg git registry snapshot.'\n"
            << "    Invoke-Tool $gitExe @('-C', $VcpkgRoot, 'init') $VcpkgRoot 'git init'\n"
            << "    if (-not (Test-Path -LiteralPath $gitDir)) {\n"
            << "      throw 'Managed vcpkg git init did not create a local .git directory.'\n"
            << "    }\n"
            << "    Invoke-Tool $gitExe @(('--git-dir=' + $gitDir), ('--work-tree=' + $VcpkgRoot), 'config', 'user.name', 'Epoch Updater') $VcpkgRoot 'git config user.name'\n"
            << "    Invoke-Tool $gitExe @(('--git-dir=' + $gitDir), ('--work-tree=' + $VcpkgRoot), 'config', 'user.email', 'updater@epoch.local') $VcpkgRoot 'git config user.email'\n"
            << "    Invoke-Tool $gitExe @(('--git-dir=' + $gitDir), ('--work-tree=' + $VcpkgRoot), 'config', 'core.longpaths', 'true') $VcpkgRoot 'git config core.longpaths'\n"
            << "    Invoke-Tool $gitExe @(('--git-dir=' + $gitDir), ('--work-tree=' + $VcpkgRoot), 'add', '--all') $VcpkgRoot 'git add'\n"
            << "    try {\n"
            << "      Invoke-Tool $gitExe @(('--git-dir=' + $gitDir), ('--work-tree=' + $VcpkgRoot), 'commit', '--no-gpg-sign', '-m', 'Managed vcpkg registry snapshot') $VcpkgRoot 'git commit'\n"
            << "    }\n"
            << "    catch {\n"
            << "    }\n"
            << "    try {\n"
            << "      $head = (& $gitExe ('--git-dir=' + $gitDir) ('--work-tree=' + $VcpkgRoot) 'rev-parse' '--verify' 'HEAD' 2>$null | Out-String).Trim()\n"
            << "    }\n"
            << "    catch {\n"
            << "      $head = ''\n"
            << "    }\n"
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
            << "function Stage-CMakePolicyOverlayPort([string]$VcpkgRoot, [string]$OverlayRoot, [string]$PortName) {\n"
            << "  $sourcePortDir = Join-Path $VcpkgRoot ('ports\\' + $PortName)\n"
            << "  if (-not (Test-Path -LiteralPath $sourcePortDir)) {\n"
            << "    Write-Step 'WARN' ('Managed vcpkg ' + $PortName + ' port was not found; skipping overlay patch.')\n"
            << "    return $false\n"
            << "  }\n"
            << "  $overlayPortDir = Join-Path $OverlayRoot $PortName\n"
            << "  Remove-Item -LiteralPath $overlayPortDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "  Copy-Item -LiteralPath $sourcePortDir -Destination $overlayPortDir -Recurse -Force\n"
            << "  $portfile = Join-Path $overlayPortDir 'portfile.cmake'\n"
            << "  if (-not (Test-Path -LiteralPath $portfile)) {\n"
            << "    Write-Step 'WARN' ('Staged ' + $PortName + ' overlay port has no portfile.cmake.')\n"
            << "    return $false\n"
            << "  }\n"
            << "  $portfileText = Get-Content -LiteralPath $portfile -Raw -ErrorAction Stop\n"
            << "  $portfileText = $portfileText.Replace(\"`r`n\", \"`n\").Replace(\"`r\", \"`n\")\n"
            << "  if (-not $portfileText.Contains('CMAKE_POLICY_VERSION_MINIMUM=3.5')) {\n"
            << "    $optionsMarker = \"`n    OPTIONS`n\"\n"
            << "    if ($portfileText.Contains($optionsMarker)) {\n"
            << "      $portfileText = $portfileText.Replace($optionsMarker, $optionsMarker + \"        -DCMAKE_POLICY_VERSION_MINIMUM=3.5`n\")\n"
            << "    }\n"
            << "    else {\n"
            << "      $configureMarker = 'vcpkg_cmake_configure('\n"
            << "      $configureIndex = $portfileText.IndexOf($configureMarker, [System.StringComparison]::Ordinal)\n"
            << "      if ($configureIndex -lt 0) {\n"
            << "        Write-Step 'WARN' ('Could not patch the ' + $PortName + ' overlay port; continuing without that policy overlay.')\n"
            << "        return $false\n"
            << "      }\n"
            << "      $lineBreak = $portfileText.IndexOf(\"`n\", $configureIndex)\n"
            << "      if ($lineBreak -lt 0) {\n"
            << "        Write-Step 'WARN' ('Could not patch the ' + $PortName + ' overlay port; continuing without that policy overlay.')\n"
            << "        return $false\n"
            << "      }\n"
            << "      $portfileText = $portfileText.Insert($lineBreak + 1, \"    OPTIONS`n        -DCMAKE_POLICY_VERSION_MINIMUM=3.5`n\")\n"
            << "    }\n"
            << "    [System.IO.File]::WriteAllText($portfile, $portfileText.Replace(\"`n\", [Environment]::NewLine), $utf8NoBom)\n"
            << "  }\n"
            << "  Write-Step 'INFO' ('Prepared a ' + $PortName + ' overlay port for modern CMake policy handling.')\n"
            << "  return $true\n"
            << "}\n"
            << "function Prepare-CMakePolicyOverlay([string]$VcpkgRoot) {\n"
            << "  $overlayRoot = Join-Path $managedToolsRoot 'ov'\n"
            << "  New-Item -ItemType Directory -Path $overlayRoot -Force | Out-Null\n"
            << "  $stagedAny = $false\n"
            << "  foreach ($portName in @('freetype', 'glad', 'glfw3', 'libogg', 'libvorbis', 'raylib', 'sdl3', 'sfml', 'shaderc', 'spirv-tools', 'zlib')) {\n"
            << "    $stagedPort = Stage-CMakePolicyOverlayPort $VcpkgRoot $overlayRoot $portName\n"
            << "    if ($stagedPort -eq $true) {\n"
            << "      $stagedAny = $true\n"
            << "    }\n"
            << "  }\n"
            << "  if (-not $stagedAny) {\n"
            << "    return ''\n"
            << "  }\n"
            << "  Write-Step 'INFO' 'Prepared updater overlay ports for modern CMake policy handling.'\n"
            << "  return $overlayRoot\n"
            << "}\n"
            << "New-Item -ItemType Directory -Path $workRoot -Force | Out-Null\n"
            << "Clear-StaleSourceRuns\n"
            << "Remove-Item -LiteralPath $runDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "New-Item -ItemType Directory -Path $runDir -Force | Out-Null\n"
            << "Set-Content -LiteralPath $activeRunPath -Value ($runDir + [Environment]::NewLine + 'pid=' + $PID) -NoNewline -Encoding UTF8\n"
            << "Remove-Item -LiteralPath $buildLog -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $handoffLog -Force -ErrorAction SilentlyContinue\n"
            << "Write-Step 'INFO' 'Source update worker started.'\n"
            << "Write-Handoff 'INFO' 'Source update worker started.'\n"
            << "Write-Handoff 'INFO' ('Build log: ' + $buildLog)\n"
            << "Write-Step 'INFO' ('Source root: ' + $sourceRoot)\n"
            << "Write-Step 'INFO' ('Manifest root: ' + $manifestRoot)\n"
            << "Write-Step 'INFO' ('MSBuild: ' + $msbuildExe)\n"
            << "Write-Step 'INFO' ('Source updater build lane: ' + $buildConfiguration + '|' + $buildPlatform + ' (runtime may currently be Debug).')\n"
            << "Write-Handoff 'INFO' ('Source updater build lane: ' + $buildConfiguration + '|' + $buildPlatform + '. Debug launches are updated through the Release output lane.')\n"
            << "$env:MSBUILDDISABLENODEREUSE = '1'\n"
            << "Remove-Item -LiteralPath $sourceArchive -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $stagingDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $sourceRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "New-Item -ItemType Directory -Path $runDir -Force | Out-Null\n"
            << "if (Test-Path -LiteralPath $sourceRoot) {\n"
            << "  $staleSourceRoot = $sourceRoot + '.stale.' + [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()\n"
            << "  Move-Item -LiteralPath $sourceRoot -Destination $staleSourceRoot -Force -ErrorAction SilentlyContinue\n"
            << "}\n"
            << "if (Test-Path -LiteralPath $sourceRoot) {\n"
            << "  throw ('Could not clear stale source update root: ' + $sourceRoot)\n"
            << "}\n"
            << "New-Item -ItemType Directory -Path (Split-Path -Parent $sourceArchive) -Force | Out-Null\n"
            << "Write-Step 'INFO' 'Downloading latest " << PROJECT_SOURCE_ARCHIVE_LABEL() << ".'\n"
            << "Test-Cancel\n"
            << "Invoke-DownloadWithFallback $sourceUrl $sourceFallbackUrls $sourceArchive 'Source snapshot download'\n"
            << "Test-Cancel\n"
            << "Expand-Archive -LiteralPath $sourceArchive -DestinationPath $stagingDir -Force\n"
            << "Test-Cancel\n"
            << "$extractedRoot = Get-ChildItem -LiteralPath $stagingDir -Directory | Select-Object -First 1\n"
            << "if ($null -ne $extractedRoot) {\n"
            << "  Move-Item -LiteralPath $extractedRoot.FullName -Destination $sourceRoot -Force\n"
            << "} else {\n"
            << "  New-Item -ItemType Directory -Path $sourceRoot -Force | Out-Null\n"
            << "  Copy-Item -Path (Join-Path $stagingDir '*') -Destination $sourceRoot -Recurse -Force\n"
            << "}\n"
            << "Remove-Item -LiteralPath $stagingDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "$manifestFile = Join-Path $manifestRoot 'vcpkg.json'\n"
            << "if (-not (Test-Path -LiteralPath $manifestFile)) {\n"
            << "  $nestedRoot = Get-ChildItem -LiteralPath $sourceRoot -Directory -ErrorAction SilentlyContinue | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'Engine\\vcpkg.json') } | Select-Object -First 1\n"
            << "  if ($null -ne $nestedRoot) {\n"
            << "    $repairRoot = $sourceRoot + '.nested.' + [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()\n"
            << "    $nestedName = $nestedRoot.Name\n"
            << "    Move-Item -LiteralPath $sourceRoot -Destination $repairRoot -Force\n"
            << "    Move-Item -LiteralPath (Join-Path $repairRoot $nestedName) -Destination $sourceRoot -Force\n"
            << "    Remove-Item -LiteralPath $repairRoot -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "    $manifestFile = Join-Path $manifestRoot 'vcpkg.json'\n"
            << "    Write-Step 'INFO' 'Repaired nested source snapshot root.'\n"
            << "  }\n"
            << "}\n"
            << "if (-not (Test-Path -LiteralPath $manifestFile)) {\n"
            << "  throw ('Downloaded source snapshot did not contain Engine\\vcpkg.json at: ' + $manifestFile)\n"
            << "}\n"
            << "Write-Step 'INFO' ('Source snapshot ready at: ' + $sourceRoot)\n"
            << "Write-Handoff 'INFO' 'Source snapshot downloaded and extracted.'\n"
            << "Test-Cancel\n"
            << "if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {\n"
            << "  Write-Step 'INFO' ('Checking inherited VCPKG_ROOT: ' + $env:VCPKG_ROOT)\n"
            << "}\n"
            << "$vcpkgExe = Resolve-VcpkgExe\n"
            << "Test-Cancel\n"
            << "$vcpkgRoot = Split-Path -Parent $vcpkgExe\n"
            << "$managedInstallRoot = Join-Path $manifestRoot 'vcpkg_installed'\n"
            << "Remove-Item Env:VCPKG_ROOT -Force -ErrorAction SilentlyContinue\n"
            << "$env:VCPKG_ROOT = $vcpkgRoot\n"
            << "Write-Step 'INFO' ('Pinned worker-local VCPKG_ROOT to selected toolchain: ' + $vcpkgRoot)\n"
            << "$managedToolsFull = [System.IO.Path]::GetFullPath($managedToolsRoot).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)\n"
            << "$vcpkgRootFull = [System.IO.Path]::GetFullPath($vcpkgRoot).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)\n"
            << "$usingManagedVcpkg = [string]::Equals($vcpkgRootFull, $managedToolsFull, [System.StringComparison]::OrdinalIgnoreCase) -or $vcpkgRootFull.StartsWith($managedToolsFull + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -or $vcpkgRootFull.StartsWith($managedToolsFull + [System.IO.Path]::AltDirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)\n"
            << "if ($usingManagedVcpkg) {\n"
            << "  Prepare-ManifestForManagedVcpkg $manifestRoot $vcpkgRoot\n"
            << "} else {\n"
            << "  Write-Step 'INFO' 'Using installed vcpkg registry; source manifest baseline left intact.'\n"
            << "}\n"
            << "Test-Cancel\n"
            << "$overlayRoot = Prepare-CMakePolicyOverlay $vcpkgRoot\n"
            << "Write-Step 'INFO' 'Restoring source dependencies with vcpkg.'\n"
            << "Write-Handoff 'INFO' 'Restoring source dependencies with vcpkg.'\n"
            << "$vcpkgArgs = @('install', '--triplet', $triplet, ('--x-manifest-root=' + $manifestRoot), ('--x-builtin-ports-root=' + (Join-Path $vcpkgRoot 'ports')), ('--x-builtin-registry-versions-dir=' + (Join-Path $vcpkgRoot 'versions')))\n"
            << "if (-not [string]::IsNullOrWhiteSpace($overlayRoot)) {\n"
            << "  $vcpkgArgs += ('--overlay-ports=' + $overlayRoot)\n"
            << "}\n"
            << "Invoke-Tool $vcpkgExe $vcpkgArgs $manifestRoot 'vcpkg restore'\n"
            << "Write-Handoff 'INFO' 'Source dependencies restored.'\n"
            << "Test-Cancel\n"
            << "$buildSucceeded = $false\n"
            << "for ($attempt = 1; $attempt -le 3 -and -not $buildSucceeded; ++$attempt) {\n"
            << "  try {\n"
            << "    Test-Cancel\n"
            << "    Write-Step 'INFO' ('MSBuild attempt ' + $attempt + ' started.')\n"
            << "    Write-Handoff 'INFO' ('MSBuild Release attempt ' + $attempt + ' started; node reuse is disabled and long compiles may stay on this line for several minutes.')\n"
            << "    Invoke-Tool $msbuildExe @($solution, ('/t:' + $buildTarget), ('/p:Configuration=' + $buildConfiguration), ('/p:Platform=' + $buildPlatform), ('/p:VcpkgRoot=' + $vcpkgRoot), ('/p:VcpkgManifestRoot=' + $manifestRoot), ('/p:VcpkgInstalledDir=' + $managedInstallRoot), '/p:VcpkgManifestInstall=false', ('/p:VcpkgTriplet=' + $triplet), '/p:UseMultiToolTask=false', '/p:BuildInParallel=false', '/m:1', '/nr:false', '/clp:ErrorsOnly') $sourceRoot ('MSBuild attempt ' + $attempt)\n"
            << "    Write-Handoff 'INFO' ('MSBuild Release attempt ' + $attempt + ' completed successfully.')\n"
            << "    $buildSucceeded = $true\n"
            << "  }\n"
            << "  catch {\n"
            << "    Write-Step 'WARN' $_.Exception.Message\n"
            << "    Write-Handoff 'WARN' $_.Exception.Message\n"
            << "    if ($attempt -lt 3) {\n"
            << "      Write-Step 'INFO' 'Retrying the source build after restore.'\n"
            << "      Start-Sleep -Seconds 5\n"
            << "      Test-Cancel\n"
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
            << "Write-Handoff 'INFO' ('Built runtime ready at: ' + $builtExe)\n"
            << "Write-Handoff 'INFO' 'Waiting for runtime handoff.'\n"
            << "for ($attempt = 1; $attempt -le 600; ++$attempt) {\n"
            << "  Test-Cancel\n"
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
            << "Test-Cancel\n"
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
            << "Remove-Item -LiteralPath $runDir -Recurse -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $cancelPath -Force -ErrorAction SilentlyContinue\n"
            << "Remove-Item -LiteralPath $activeRunPath -Force -ErrorAction SilentlyContinue\n"
            << "$env:EPOCH_POST_UPDATE_STARTUP_DELAY_MS = '3000'\n"
            << "$restartedProcess = Start-Process -FilePath $targetExe -WorkingDirectory $targetDir -PassThru\n"
            << "Remove-Item Env:EPOCH_POST_UPDATE_STARTUP_DELAY_MS -Force -ErrorAction SilentlyContinue\n"
            << "Start-Sleep -Milliseconds 900\n"
            << "try {\n"
            << "  $focusShell = New-Object -ComObject WScript.Shell\n"
            << "  [void]$focusShell.AppActivate($restartedProcess.Id)\n"
            << "} catch {\n"
            << "}\n"
            << "Write-Handoff 'INFO' 'Restarted updated runtime.'\n"
            << "Start-Sleep -Seconds 1\n"
            << "Remove-Item -LiteralPath $workerPath -Force -ErrorAction SilentlyContinue\n";

        ps.close();

        if (!system_detail::launch_powershell_script(worker_script, silent_worker, build_log))
        {
            std::error_code marker_ec;
            std::filesystem::remove(active_run_path, marker_ec);
            system_detail::remove_update_cache_path_best_effort(run_dir);
            system_detail::log_error("Failed to launch source update worker.");
            return false;
        }

        system_detail::log_info("Source update worker launched successfully.");
        return true;
    }
#endif

    export void cleanup_previous_update_artifacts()
    {
        if (!LEAVE_NO_FILES_ALWAYS_REDOWNLOAD)
            return;

        // Source updater runs now use executable-local, tokenized cache roots.
        // The old REPO-main cleanup was relative to the process working
        // directory, which can be a source checkout, launcher directory, or
        // packaged runtime folder depending on how Epoch was started.
    }

    bool replace_binary_from_script(
        const std::filesystem::path& target_binary,
        const std::filesystem::path& new_binary,
        const UpdateHandoffMode handoff_mode = UpdateHandoffMode::LaunchImmediately)
    {
#if defined(_WIN32)
        const auto script_path = handoff_mode == UpdateHandoffMode::StageForRestart
            ? system_detail::staged_update_handoff_script_path()
            : system_detail::make_temp_script_path("replace_binary");
        const auto handoff_log = system_detail::update_handoff_log_path_for(target_binary);

        std::error_code replacement_exists_ec;
        if (!std::filesystem::exists(new_binary, replacement_exists_ec))
        {
            const std::string message = "Binary replacement aborted because the downloaded replacement is missing: "
                + new_binary.string();
            system_detail::log_error(message);
            system_detail::append_log_line(handoff_log, "[ERROR] " + message);
            return false;
        }

        std::error_code script_ec;
        std::filesystem::create_directories(script_path.parent_path(), script_ec);
        script_ec.clear();
        std::filesystem::remove(script_path, script_ec);

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
            << "set \"EPOCH_POST_UPDATE_STARTUP_DELAY_MS=3000\"\r\n"
            << "start \"\" \"%TARGET%\"\r\n"
            << "powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command \"$target=$env:TARGET; Start-Sleep -Milliseconds 900; try { $name=[IO.Path]::GetFileNameWithoutExtension($target); $p=Get-Process -Name $name -ErrorAction SilentlyContinue ^| Sort-Object StartTime -Descending ^| Select-Object -First 1; if ($p) { $shell=New-Object -ComObject WScript.Shell; [void]$shell.AppActivate($p.Id) } } catch { }\" >nul 2>&1\r\n"
            << "set \"EPOCH_POST_UPDATE_STARTUP_DELAY_MS=\"\r\n"
            << ">> \"%LOG%\" echo [INFO] Restarted updated executable.\r\n"
            << "del /F /Q \"%NEWBIN%\" >nul 2>&1\r\n"
            << "del /F /Q \"%~f0\" >nul 2>&1\r\n";

        bat.close();

        if (handoff_mode == UpdateHandoffMode::StageForRestart)
        {
            system_detail::append_log_line(
                handoff_log,
                "[INFO] Binary replacement handoff staged; waiting for Restart.");
            return true;
        }

        if (!system_detail::launch_batch_hidden(script_path))
        {
            system_detail::log_error("Failed to launch binary replacement batch.");
            return false;
        }

        std::exit(0);
#else
        const auto script_path = handoff_mode == UpdateHandoffMode::StageForRestart
            ? system_detail::staged_update_handoff_script_path()
            : system_detail::make_temp_script_path("replace_binary");

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
            << "EPOCH_POST_UPDATE_STARTUP_DELAY_MS=3000 \"$TARGET\" &\n"
            << "rm -f \"$0\"\n";

        sh.close();

        if (handoff_mode == UpdateHandoffMode::StageForRestart)
            return true;

        if (!system_detail::launch_detached_posix_script(script_path))
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
        const std::filesystem::path& package_archive,
        const std::string_view restart_auto_command = {},
        const UpdateHandoffMode handoff_mode = UpdateHandoffMode::LaunchImmediately)
    {
#if defined(_WIN32)
        const auto target_dir = target_binary.parent_path();
        const auto script_path = handoff_mode == UpdateHandoffMode::StageForRestart
            ? system_detail::staged_update_handoff_script_path()
            : system_detail::make_temp_script_path("replace_runtime_zip");
        const auto extracted_binary = system_detail::resolve_runtime_binary_path(extracted_runtime_dir, target_binary);
        const auto runtime_payload_dir = extracted_binary.parent_path();
        const auto handoff_log = system_detail::update_handoff_log_path_for(target_binary);
        const bool chain_after_restart = !restart_auto_command.empty();

        std::error_code extracted_exists_ec;
        if (!std::filesystem::exists(extracted_binary, extracted_exists_ec))
        {
            const std::string message = "Packaged replacement aborted because the extracted runtime binary is missing: "
                + extracted_binary.string();
            system_detail::log_error(message);
            system_detail::append_log_line(handoff_log, "[ERROR] " + message);
            return false;
        }

        std::error_code script_ec;
        std::filesystem::create_directories(script_path.parent_path(), script_ec);
        script_ec.clear();
        std::filesystem::remove(script_path, script_ec);

        std::ofstream bat(script_path, std::ios::binary);
        if (!bat)
        {
            system_detail::log_error("Failed to create packaged runtime replacement batch.");
            return false;
        }

        bat
            << "@echo off\r\n"
            << "setlocal\r\n"
            << "set \"EXTRACTED=" << runtime_payload_dir.string() << "\"\r\n"
            << "set \"PACKAGE_ROOT=" << extracted_runtime_dir.string() << "\"\r\n"
            << "set \"TARGETDIR=" << target_dir.string() << "\"\r\n"
            << "set \"TARGETEXE=" << target_binary.string() << "\"\r\n"
            << "set \"NEWEXE=" << extracted_binary.string() << "\"\r\n"
            << "set \"ARCHIVE=" << package_archive.string() << "\"\r\n"
            << "set \"LOG=" << handoff_log.string() << "\"\r\n"
            << "> \"%LOG%\" echo [INFO] Packaged runtime replacement started\r\n"
            << ">> \"%LOG%\" echo [INFO] TARGETEXE=%TARGETEXE%\r\n"
            << ">> \"%LOG%\" echo [INFO] NEWEXE=%NEWEXE%\r\n"
            << ">> \"%LOG%\" echo [INFO] EXTRACTED=%EXTRACTED%\r\n"
            << "if not exist \"%NEWEXE%\" (\r\n"
            << "  >> \"%LOG%\" echo [ERROR] Extracted runtime binary is missing.\r\n"
            << "  exit /b 1\r\n"
            << ")\r\n"
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
            << "rmdir /S /Q \"%PACKAGE_ROOT%\" >nul 2>&1\r\n"
            << "del /F /Q \"%ARCHIVE%\" >nul 2>&1\r\n"
            << "set \"EPOCH_POST_UPDATE_STARTUP_DELAY_MS=3000\"\r\n"
            << (chain_after_restart
                ? ("set \"EPOCH_UPDATER_SHELL_AUTO_COMMAND="
                    + system_detail::powershell_escape_single_quoted(std::string{ restart_auto_command }) + "\"\r\n")
                : std::string{})
            << "start \"\" /D \"%TARGETDIR%\" \"%TARGETEXE%\"\r\n"
            << "if errorlevel 1 (\r\n"
            << "  >> \"%LOG%\" echo [ERROR] Failed to restart updated runtime.\r\n"
            << "  exit /b 1\r\n"
            << ")\r\n"
            << "powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command \"$target=$env:TARGETEXE; Start-Sleep -Milliseconds 900; try { $name=[IO.Path]::GetFileNameWithoutExtension($target); $p=Get-Process -Name $name -ErrorAction SilentlyContinue ^| Sort-Object StartTime -Descending ^| Select-Object -First 1; if ($p) { $shell=New-Object -ComObject WScript.Shell; [void]$shell.AppActivate($p.Id) } } catch { }\" >nul 2>&1\r\n"
            << "set \"EPOCH_POST_UPDATE_STARTUP_DELAY_MS=\"\r\n"
            << (chain_after_restart
                ? "set \"EPOCH_UPDATER_SHELL_AUTO_COMMAND=\"\r\n"
                : "")
            << ">> \"%LOG%\" echo [INFO] Restarted updated runtime.\r\n"
            << "del /F /Q \"%~f0\" >nul 2>&1\r\n";

        bat.close();

        if (handoff_mode == UpdateHandoffMode::StageForRestart)
        {
            system_detail::append_log_line(
                handoff_log,
                "[INFO] Packaged runtime handoff staged; waiting for Restart.");
            return true;
        }

        if (!system_detail::launch_batch_hidden(script_path))
        {
            system_detail::log_error("Failed to launch packaged runtime replacement batch.");
            return false;
        }

        std::exit(0);
#else
        const auto target_dir = target_binary.parent_path();
        const auto script_path = handoff_mode == UpdateHandoffMode::StageForRestart
            ? system_detail::staged_update_handoff_script_path()
            : system_detail::make_temp_script_path("replace_runtime_zip");
        const auto extracted_binary = system_detail::resolve_runtime_binary_path(extracted_runtime_dir, target_binary);
        const auto runtime_payload_dir = extracted_binary.parent_path();
        const auto handoff_log = system_detail::update_handoff_log_path_for(target_binary);
        const bool chain_after_restart = !restart_auto_command.empty();

        std::error_code extracted_exists_ec;
        if (!std::filesystem::exists(extracted_binary, extracted_exists_ec))
        {
            const std::string message = "Packaged replacement aborted because the extracted runtime binary is missing: "
                + extracted_binary.string();
            system_detail::log_error(message);
            system_detail::append_log_line(handoff_log, "[ERROR] " + message);
            return false;
        }

        std::ofstream sh(script_path, std::ios::binary);
        if (!sh)
        {
            system_detail::log_error("Failed to create packaged runtime replacement script.");
            return false;
        }

        sh
            << "#!/bin/sh\n"
            << "set -eu\n"
            << "EXTRACTED=" << system_detail::quote_shell_arg(runtime_payload_dir.string()) << "\n"
            << "PACKAGE_ROOT=" << system_detail::quote_shell_arg(extracted_runtime_dir.string()) << "\n"
            << "TARGETDIR=" << system_detail::quote_shell_arg(target_dir.string()) << "\n"
            << "ARCHIVE=" << system_detail::quote_shell_arg(package_archive.string()) << "\n"
            << "TARGETEXE=" << system_detail::quote_shell_arg(target_binary.string()) << "\n"
            << "NEWEXE=" << system_detail::quote_shell_arg(extracted_binary.string()) << "\n"
            << "LOG=" << system_detail::quote_shell_arg(handoff_log.string()) << "\n"
            << ": > \"$LOG\"\n"
            << "echo \"[INFO] Packaged runtime replacement started\" >> \"$LOG\"\n"
            << "echo \"[INFO] TARGETEXE=$TARGETEXE\" >> \"$LOG\"\n"
            << "echo \"[INFO] NEWEXE=$NEWEXE\" >> \"$LOG\"\n"
            << "if [ ! -f \"$NEWEXE\" ]; then\n"
            << "  echo \"[ERROR] Extracted runtime binary is missing.\" >> \"$LOG\"\n"
            << "  exit 1\n"
            << "fi\n"
            << "mkdir -p \"$TARGETDIR\"\n"
            << "i=0\n"
            << "while [ $i -lt 60 ]; do\n"
            << "  cp -f \"$NEWEXE\" \"$TARGETEXE\" 2>/dev/null || true\n"
            << "  if [ -f \"$TARGETEXE\" ]; then\n"
            << "    break\n"
            << "  fi\n"
            << "  i=$((i+1))\n"
            << "  if [ $i -eq 1 ] || [ $((i % 10)) -eq 0 ]; then\n"
            << "    echo \"[INFO] Waiting for packaged runtime handoff attempt $i.\" >> \"$LOG\"\n"
            << "  fi\n"
            << "  sleep 1\n"
            << "done\n"
            << "if [ ! -f \"$TARGETEXE\" ]; then\n"
            << "  echo \"[ERROR] Timed out waiting to copy the packaged runtime executable.\" >> \"$LOG\"\n"
            << "  exit 1\n"
            << "fi\n"
            << "chmod +x \"$TARGETEXE\" 2>/dev/null || true\n"
            << "find \"$EXTRACTED\" -maxdepth 1 -type f \\( -name '*.so' -o -name '*.so.*' -o -name '*.dll' -o -name '*.manifest' \\) -exec cp -f {} \"$TARGETDIR\" \\; 2>/dev/null || true\n"
            << "if [ -d \"$EXTRACTED/assets\" ]; then\n"
            << "  mkdir -p \"$TARGETDIR/assets\"\n"
            << "  cp -R \"$EXTRACTED/assets/.\" \"$TARGETDIR/assets/\" 2>/dev/null || true\n"
            << "fi\n"
            << "echo \"[INFO] Packaged runtime files copied successfully.\" >> \"$LOG\"\n"
            << "rm -rf \"$PACKAGE_ROOT\"\n"
            << "rm -f \"$ARCHIVE\"\n"
            << "cd \"$TARGETDIR\"\n"
            << (chain_after_restart
                ? ("EPOCH_UPDATER_SHELL_AUTO_COMMAND="
                    + system_detail::quote_shell_arg(std::string{ restart_auto_command })
                    + " EPOCH_POST_UPDATE_STARTUP_DELAY_MS=3000 \"$TARGETEXE\" >/dev/null 2>&1 &\n")
                : "EPOCH_POST_UPDATE_STARTUP_DELAY_MS=3000 \"$TARGETEXE\" >/dev/null 2>&1 &\n")
            << "if [ $? -ne 0 ]; then\n"
            << "  echo \"[ERROR] Failed to restart updated runtime.\" >> \"$LOG\"\n"
            << "  exit 1\n"
            << "fi\n"
            << "echo \"[INFO] Restarted updated runtime.\" >> \"$LOG\"\n"
            << "rm -f \"$0\"\n";

        sh.close();

        if (handoff_mode == UpdateHandoffMode::StageForRestart)
        {
            system_detail::append_log_line(
                handoff_log,
                "[INFO] Packaged runtime handoff staged; waiting for Restart.");
            return true;
        }

        if (!system_detail::launch_detached_posix_script(script_path))
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
        const auto built_binary = system_detail::resolve_runtime_binary_path(built_runtime_dir, target_binary);
        if (!std::filesystem::exists(built_binary))
        {
            system_detail::log_error("Source replacement aborted because the built runtime binary is missing.");
            return false;
        }

#if defined(_WIN32)
        const auto target_dir = target_binary.parent_path();
        const auto script_path = system_detail::make_temp_script_path("replace_runtime_source");
        const auto handoff_log = system_detail::update_handoff_log_path_for(target_binary);

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
            << "if errorlevel 1 (\r\n"
            << "  >> \"%LOG%\" echo [ERROR] Failed to restart updated runtime.\r\n"
            << "  exit /b 1\r\n"
            << ")\r\n"
            << "powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command \"$target=$env:TARGETEXE; Start-Sleep -Milliseconds 900; try { $name=[IO.Path]::GetFileNameWithoutExtension($target); $p=Get-Process -Name $name -ErrorAction SilentlyContinue ^| Sort-Object StartTime -Descending ^| Select-Object -First 1; if ($p) { $shell=New-Object -ComObject WScript.Shell; [void]$shell.AppActivate($p.Id) } } catch { }\" >nul 2>&1\r\n"
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
        const auto handoff_log = system_detail::update_handoff_log_path_for(target_binary);
        const auto source_assets_dir = built_runtime_dir / "assets";
        const auto source_repo_assets_dir = system_detail::source_manifest_root(source_root) / "assets";
        const auto target_assets_dir = target_dir / "assets";

        std::ofstream sh(script_path, std::ios::binary);
        if (!sh)
        {
            system_detail::log_error("Failed to create source runtime replacement script.");
            return false;
        }

        sh
            << "#!/bin/sh\n"
            << "set -eu\n"
            << "BUILTDIR=" << system_detail::quote_shell_arg(built_runtime_dir.string()) << "\n"
            << "TARGETDIR=" << system_detail::quote_shell_arg(target_dir.string()) << "\n"
            << "TARGETEXE=" << system_detail::quote_shell_arg(target_binary.string()) << "\n"
            << "BUILTEXE=" << system_detail::quote_shell_arg(built_binary.string()) << "\n"
            << "SRCROOT=" << system_detail::quote_shell_arg(source_root.string()) << "\n"
            << "ARCHIVE=" << system_detail::quote_shell_arg(package_archive.string()) << "\n"
            << "SRCASSETS=" << system_detail::quote_shell_arg(source_assets_dir.string()) << "\n"
            << "REPOASSETS=" << system_detail::quote_shell_arg(source_repo_assets_dir.string()) << "\n"
            << "DSTASSETS=" << system_detail::quote_shell_arg(target_assets_dir.string()) << "\n"
            << "LOG=" << system_detail::quote_shell_arg(handoff_log.string()) << "\n"
            << ": > \"$LOG\"\n"
            << "echo \"[INFO] Source runtime replacement started\" >> \"$LOG\"\n"
            << "echo \"[INFO] TARGETEXE=$TARGETEXE\" >> \"$LOG\"\n"
            << "echo \"[INFO] BUILTEXE=$BUILTEXE\" >> \"$LOG\"\n"
            << "if [ ! -f \"$BUILTEXE\" ]; then\n"
            << "  echo \"[ERROR] Built runtime binary is missing.\" >> \"$LOG\"\n"
            << "  exit 1\n"
            << "fi\n"
            << "i=0\n"
            << "while [ $i -lt 60 ]; do\n"
            << "  cp -f \"$BUILTEXE\" \"$TARGETEXE\" 2>/dev/null || true\n"
            << "  if [ -f \"$TARGETEXE\" ]; then\n"
            << "    break\n"
            << "  fi\n"
            << "  i=$((i+1))\n"
            << "  if [ $i -eq 1 ] || [ $((i % 10)) -eq 0 ]; then\n"
            << "    echo \"[INFO] Waiting for source runtime handoff attempt $i.\" >> \"$LOG\"\n"
            << "  fi\n"
            << "  sleep 1\n"
            << "done\n"
            << "if [ ! -f \"$TARGETEXE\" ]; then\n"
            << "  echo \"[ERROR] Timed out waiting to copy the rebuilt runtime executable.\" >> \"$LOG\"\n"
            << "  exit 1\n"
            << "fi\n"
            << "chmod +x \"$TARGETEXE\" 2>/dev/null || true\n"
            << "find \"$BUILTDIR\" -maxdepth 1 -type f \\( -name '*.so' -o -name '*.so.*' -o -name '*.dll' -o -name '*.manifest' \\) -exec cp -f {} \"$TARGETDIR\" \\; 2>/dev/null || true\n"
            << "if [ -d \"$SRCASSETS\" ]; then\n"
            << "  mkdir -p \"$DSTASSETS\"\n"
            << "  cp -R \"$SRCASSETS/.\" \"$DSTASSETS/\" 2>/dev/null || true\n"
            << "fi\n"
            << "if [ -d \"$REPOASSETS\" ]; then\n"
            << "  mkdir -p \"$DSTASSETS\"\n"
            << "  cp -R \"$REPOASSETS/.\" \"$DSTASSETS/\" 2>/dev/null || true\n"
            << "fi\n"
            << "echo \"[INFO] Source runtime files copied successfully.\" >> \"$LOG\"\n"
            << "rm -f \"$ARCHIVE\"\n"
            << "cd \"$TARGETDIR\"\n"
            << "\"$TARGETEXE\" >/dev/null 2>&1 &\n"
            << "if [ $? -ne 0 ]; then\n"
            << "  echo \"[ERROR] Failed to restart updated runtime.\" >> \"$LOG\"\n"
            << "  exit 1\n"
            << "fi\n"
            << "echo \"[INFO] Restarted updated runtime.\" >> \"$LOG\"\n"
            << "rm -rf \"$SRCROOT\"\n"
            << "rm -f \"$0\"\n";

        sh.close();

        if (!system_detail::launch_detached_posix_script(script_path))
        {
            system_detail::log_error("Failed to launch source runtime replacement script.");
            return false;
        }

        std::exit(0);
#endif
    }

    bool replace_binary(
        const std::filesystem::path& target_binary,
        const std::filesystem::path& new_binary,
        const UpdateHandoffMode handoff_mode = UpdateHandoffMode::LaunchImmediately)
    {
        clean_up_build_files();
        return replace_binary_from_script(target_binary, new_binary, handoff_mode);
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
            ? system_detail::extract_version_string(PROJECT_SOURCE_VERSION)
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

    namespace update_discovery_cache
    {
        struct RunCache
        {
            bool packaged_release_cached{ false };
            system_detail::ResolvedPackagedRelease packaged_release{};
            bool source_version_cached{ false };
            system_detail::VersionCheckResult source_version{};
            std::string source_version_url;
            std::string source_version_label;
            std::string source_local_version;
            bool platform_build_cached{ false };
            system_detail::BuildStatusResult platform_build{};
            std::string platform_build_url;
            std::string platform_build_job_name;
        };

        [[nodiscard]] inline std::mutex& mutex()
        {
            static std::mutex instance;
            return instance;
        }

        [[nodiscard]] inline RunCache& cache()
        {
            static RunCache instance;
            return instance;
        }
    }

    [[nodiscard]] inline system_detail::ResolvedPackagedRelease resolve_packaged_release_once_per_run()
    {
        std::lock_guard lock{ update_discovery_cache::mutex() };
        auto& cache = update_discovery_cache::cache();
        if (cache.packaged_release_cached)
        {
            system_detail::log_info("Using cached packaged release discovery for this run.");
            return cache.packaged_release;
        }

        cache.packaged_release = system_detail::resolve_packaged_release();
        cache.packaged_release_cached = true;
        return cache.packaged_release;
    }

    [[nodiscard]] inline system_detail::VersionCheckResult check_source_version_once_per_run(
        const std::string& url,
        const std::string_view label,
        const std::string& local_version_override)
    {
        std::lock_guard lock{ update_discovery_cache::mutex() };
        auto& cache = update_discovery_cache::cache();
        const std::string label_text{ label };
        const std::string normalized_local =
            local_version_override.empty()
            ? system_detail::extract_version_string(PROJECT_SOURCE_VERSION)
            : system_detail::extract_version_string(local_version_override);

        if (cache.source_version_cached
            && cache.source_version_url == url
            && cache.source_version_label == label_text
            && cache.source_local_version == normalized_local)
        {
            system_detail::log_info("Using cached source version discovery for this run.");
            return cache.source_version;
        }

        cache.source_version = check_for_updates(url, label, local_version_override);
        cache.source_version_cached = true;
        cache.source_version_url = url;
        cache.source_version_label = label_text;
        cache.source_local_version = normalized_local;
        return cache.source_version;
    }

    [[nodiscard]] inline system_detail::BuildStatusResult check_platform_build_status_once_per_run(
        const std::string& runs_api_url,
        const std::string& job_name)
    {
        std::lock_guard lock{ update_discovery_cache::mutex() };
        auto& cache = update_discovery_cache::cache();
        if (cache.platform_build_cached
            && cache.platform_build_url == runs_api_url
            && cache.platform_build_job_name == job_name)
        {
            system_detail::log_info("Using cached platform build status for this run.");
            return cache.platform_build;
        }

        auto fresh_status = system_detail::check_platform_build_status(runs_api_url, job_name);
        if (system_detail::build_status_failure_is_transient(fresh_status))
        {
            system_detail::log_info(
                "Platform build status check was transiently unavailable; not caching this failure for the run.");
            return fresh_status;
        }

        cache.platform_build = std::move(fresh_status);
        cache.platform_build_cached = true;
        cache.platform_build_url = runs_api_url;
        cache.platform_build_job_name = job_name;
        return cache.platform_build;
    }

    [[nodiscard]] bool move_download_into_place(
        const std::filesystem::path& downloaded_path,
        const std::filesystem::path& final_path)
    {
        std::error_code ec;
        std::filesystem::create_directories(final_path.parent_path(), ec);
        if (ec)
        {
            system_detail::log_error("Failed to create update cache directory: " + final_path.parent_path().string());
            return false;
        }

        std::filesystem::remove(final_path, ec);
        ec.clear();
        std::filesystem::rename(downloaded_path, final_path, ec);
        if (!ec)
            return true;

        ec.clear();
        std::filesystem::copy_file(
            downloaded_path,
            final_path,
            std::filesystem::copy_options::overwrite_existing,
            ec);
        if (ec)
        {
            system_detail::log_error("Failed to move downloaded update into cache: " + final_path.string());
            return false;
        }

        std::filesystem::remove(downloaded_path, ec);
        return true;
    }

    [[nodiscard]] bool download_update_file_atomic(
        const std::string& url,
        const std::filesystem::path& final_path)
    {
        std::error_code ec;
        auto temporary_path = final_path;
        temporary_path += ".download";
        std::filesystem::remove(temporary_path, ec);

        if (!download_file(url, temporary_path.string()))
        {
            system_detail::log_error("Update download failed from URL: " + url);
            return false;
        }

        if (!move_download_into_place(temporary_path, final_path))
        {
            std::filesystem::remove(temporary_path, ec);
            return false;
        }

        return true;
    }

    [[nodiscard]] bool download_source_archive_with_fallbacks(
        const std::string& primary_url,
        const std::filesystem::path& final_path)
    {
        std::vector<std::string> urls;
        if (!primary_url.empty())
            urls.push_back(primary_url);

        for (const auto& fallback_url : PROJECT_SOURCE_FALLBACK_URLS())
        {
            if (fallback_url.empty())
                continue;

            bool duplicate = false;
            for (const auto& existing_url : urls)
            {
                if (system_detail::lower_ascii(existing_url)
                    == system_detail::lower_ascii(fallback_url))
                {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate)
                urls.push_back(fallback_url);
        }

        for (const auto& url : urls)
        {
            system_detail::log_info("Source snapshot download URL: " + url);
            if (download_update_file_atomic(url, final_path))
                return true;

            system_detail::log_error("Source snapshot download failed from: " + url);
        }

        system_detail::log_error("Source snapshot download failed from all configured URLs.");
        return false;
    }

    export ProjectSourceDownloadResult download_project_source_code(
        const UpdateChannel& channel)
    {
        ProjectSourceDownloadResult result{};
        if (channel.source_url.empty())
        {
            result.status_message = "Project source code download failed: source URL is not configured.";
            system_detail::log_error(result.status_message);
            return result;
        }

        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path cache_root = system_detail::project_source_cache_root();
        const fs::path archive_path = system_detail::project_source_archive_path(channel.source_url);
        const fs::path staging_dir = system_detail::project_source_staging_dir();
        const fs::path final_dir = system_detail::project_source_final_dir();

        fs::remove_all(staging_dir, ec);
        ec.clear();
        fs::remove(archive_path, ec);
        ec.clear();
        fs::create_directories(archive_path.parent_path(), ec);
        if (ec)
        {
            result.status_message = "Project source code download failed: could not prepare cache downloads folder.";
            system_detail::log_error(result.status_message + " " + archive_path.parent_path().string());
            return result;
        }

        system_detail::log_info("Downloading project source code snapshot into cache: " + cache_root.string());
        if (!download_source_archive_with_fallbacks(channel.source_url, archive_path))
        {
            result.status_message = "Project source code download failed: source archive download did not complete.";
            return result;
        }

        fs::remove_all(staging_dir, ec);
        ec.clear();
        if (!extract_archive(archive_path.string(), staging_dir.string()))
        {
            fs::remove_all(staging_dir, ec);
            result.status_message = "Project source code download failed: downloaded source archive did not extract.";
            system_detail::log_error(result.status_message);
            return result;
        }

        fs::path extracted_root = system_detail::first_subdirectory(staging_dir);
        if (extracted_root.empty())
            extracted_root = staging_dir;

        fs::remove_all(final_dir, ec);
        ec.clear();
        fs::create_directories(final_dir.parent_path(), ec);
        if (ec)
        {
            fs::remove_all(staging_dir, ec);
            result.status_message = "Project source code download failed: could not prepare final source project folder.";
            system_detail::log_error(result.status_message + " " + final_dir.string());
            return result;
        }

        fs::rename(extracted_root, final_dir, ec);
        if (ec)
        {
            ec.clear();
            fs::copy(
                extracted_root,
                final_dir,
                fs::copy_options::recursive | fs::copy_options::overwrite_existing,
                ec);
            if (ec)
            {
                fs::remove_all(staging_dir, ec);
                result.status_message = "Project source code download failed: could not move source project into cache.";
                system_detail::log_error(result.status_message + " " + final_dir.string());
                return result;
            }
        }

        fs::remove_all(staging_dir, ec);
        result.ok = true;
        result.project_root = final_dir;
        result.status_message = "Project source code downloaded to cache: " + final_dir.string();
        system_detail::log_info(result.status_message);
        return result;
    }

    [[nodiscard]] bool prepare_cached_update_archive(
        const std::string& url,
        const std::filesystem::path& archive_path,
        const std::filesystem::path& extract_dir)
    {
        namespace fs = std::filesystem;

        std::error_code ec;
        fs::remove_all(extract_dir, ec);
        ec.clear();

        if (LEAVE_NO_FILES_ALWAYS_REDOWNLOAD)
        {
            fs::remove(archive_path, ec);
            ec.clear();
        }

        if (fs::exists(archive_path, ec) && !ec)
        {
            const auto cached_size = fs::file_size(archive_path, ec);
            if (!ec && cached_size > 0)
            {
                system_detail::log_info("Checking cached update package: " + archive_path.string());
                if (extract_archive(archive_path.string(), extract_dir.string()))
                {
                    system_detail::log_info("Using verified cached update package.");
                    return true;
                }

                system_detail::log_error("Cached update package failed validation; redownloading.");
            }

            ec.clear();
            fs::remove_all(extract_dir, ec);
            ec.clear();
            fs::remove(archive_path, ec);
        }

        system_detail::log_info("Downloading update package into cache: " + archive_path.string());
        if (!download_update_file_atomic(url, archive_path))
            return false;

        fs::remove_all(extract_dir, ec);
        ec.clear();
        if (!extract_archive(archive_path.string(), extract_dir.string()))
        {
            system_detail::log_error("Downloaded update package failed extraction; removing cached package.");
            fs::remove_all(extract_dir, ec);
            ec.clear();
            fs::remove(archive_path, ec);
            return false;
        }

        return true;
    }

    bool install_from_binary(
        const std::string& url,
        const std::string_view restart_auto_command = {},
        const UpdateHandoffMode handoff_mode = UpdateHandoffMode::LaunchImmediately)
    {
        const auto target_binary = system_detail::current_binary_path();
        const auto normalized_url =
            system_detail::lower_ascii(system_detail::strip_url_query_and_fragment(url));

        const auto archive_extension = [&]() -> std::string
            {
                if (normalized_url.ends_with(".tar.gz"))
                    return ".tar.gz";
                if (normalized_url.ends_with(".tgz"))
                    return ".tgz";
                if (normalized_url.ends_with(".zip"))
                    return ".zip";
                return {};
            }();

        if (!archive_extension.empty())
        {
            const auto archive_path =
                system_detail::replacement_package_path(target_binary, url, archive_extension);
            const auto extract_dir = system_detail::replacement_extract_dir(target_binary);

            std::error_code ec;
            std::filesystem::remove_all(extract_dir, ec);

            if (!prepare_cached_update_archive(url, archive_path, extract_dir))
                return false;

            return replace_runtime_from_script(
                target_binary,
                extract_dir,
                archive_path,
                restart_auto_command,
                handoff_mode);
        }

        const auto new_binary = system_detail::replacement_binary_path(target_binary);

        if (!download_update_file_atomic(url, new_binary))
            return false;

        return replace_binary(target_binary, new_binary, handoff_mode);
    }

    export std::filesystem::path source_update_log_path()
    {
        return system_detail::source_update_log_path_for(system_detail::current_binary_path());
    }

    export std::filesystem::path update_handoff_log_path()
    {
        return system_detail::update_handoff_log_path_for(system_detail::current_binary_path());
    }

    export bool source_update_worker_active()
    {
#if defined(_WIN32)
        try
        {
            return system_detail::source_update_session_active(system_detail::current_binary_path());
        }
        catch (const std::exception& e)
        {
            system_detail::log_error(std::string{ "Source update liveness check failed: " } + e.what());
            return false;
        }
        catch (...)
        {
            system_detail::log_error("Source update liveness check failed with an unknown exception.");
            return false;
        }
#else
        return system_detail::in_process_source_update_active().load(std::memory_order_acquire);
#endif
    }

    export int source_update_recent_cancel_seconds_remaining(const int cooldown_seconds)
    {
#if defined(_WIN32)
        if (cooldown_seconds <= 0)
            return 0;

        try
        {
            return system_detail::source_update_recent_cancel_seconds_remaining(
                system_detail::current_binary_path(),
                std::chrono::seconds{ cooldown_seconds });
        }
        catch (const std::exception& e)
        {
            system_detail::log_error(std::string{ "Source update cancel cooldown check failed: " } + e.what());
            return 0;
        }
        catch (...)
        {
            system_detail::log_error("Source update cancel cooldown check failed with an unknown exception.");
            return 0;
        }
#else
        (void)cooldown_seconds;
        return 0;
#endif
    }

    export bool source_update_cancel_requested()
    {
        try
        {
            return system_detail::source_update_cancellation_pending(system_detail::current_binary_path());
        }
        catch (const std::exception& e)
        {
            system_detail::log_error(std::string{ "Source update cancel-state check failed: " } + e.what());
            return false;
        }
        catch (...)
        {
            system_detail::log_error("Source update cancel-state check failed with an unknown exception.");
            return false;
        }
    }

    export bool launch_staged_update_handoff()
    {
        const auto script_path = system_detail::staged_update_handoff_script_path();
        const auto handoff_log = update_handoff_log_path();

        std::error_code ec;
        if (!std::filesystem::exists(script_path, ec) || ec)
        {
            system_detail::append_log_line(
                handoff_log,
                "[ERROR] Restart requested, but no staged update replacement script exists.");
            system_detail::log_error("No staged update replacement script exists.");
            return false;
        }

#if defined(_WIN32)
        if (!system_detail::launch_batch_hidden(script_path))
        {
            system_detail::append_log_line(
                handoff_log,
                "[ERROR] Failed to launch staged update replacement.");
            return false;
        }
#else
        if (!system_detail::launch_detached_posix_script(script_path))
        {
            system_detail::append_log_line(
                handoff_log,
                "[ERROR] Failed to launch staged update replacement.");
            return false;
        }
#endif

        system_detail::append_log_line(
            handoff_log,
            "[INFO] Staged update replacement launched from Restart.");
        return true;
    }

    export bool request_source_update_cancel()
    {
#if defined(_WIN32)
        try
        {
            const auto target_binary = system_detail::current_binary_path();
            const auto cancel_path = system_detail::source_cancel_path(target_binary);

            std::error_code dir_ec;
            std::filesystem::create_directories(cancel_path.parent_path(), dir_ec);
            if (dir_ec)
            {
                system_detail::log_error(
                    "Source update cancel failed to prepare marker directory: "
                    + cancel_path.parent_path().string());
                return false;
            }

            const std::wstring cancel_path_text = cancel_path.wstring();
            HANDLE file = ::CreateFileW(
                cancel_path_text.c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                system_detail::log_error("Source update cancel marker could not be opened.");
                return false;
            }

            constexpr char kCancelText[] = "cancel requested by runtime ui\r\n";
            DWORD written = 0u;
            const BOOL ok = ::WriteFile(
                file,
                kCancelText,
                static_cast<DWORD>(sizeof(kCancelText) - 1u),
                &written,
                nullptr);
            ::CloseHandle(file);

            const bool wrote_marker =
                ok != FALSE && written == static_cast<DWORD>(sizeof(kCancelText) - 1u);
            if (wrote_marker)
            {
                system_detail::append_log_line(
                    system_detail::source_update_log_path_for(target_binary),
                    "[WARN] Source rebuild cancellation requested from runtime UI.");
                system_detail::append_log_line(
                    system_detail::update_handoff_log_path_for(target_binary),
                    "[WARN] Source rebuild cancellation requested from runtime UI.");
            }
            else
            {
                system_detail::log_error("Source update cancel marker write failed.");
            }

            return wrote_marker;
        }
        catch (const std::exception& e)
        {
            system_detail::log_error(std::string{ "Source update cancel failed: " } + e.what());
            return false;
        }
        catch (...)
        {
            system_detail::log_error("Source update cancel failed with an unknown exception.");
            return false;
        }
#else
        try
        {
            const auto target_binary = system_detail::current_binary_path();
            const auto cancel_path = system_detail::source_cancel_path(target_binary);

            std::error_code dir_ec;
            std::filesystem::create_directories(cancel_path.parent_path(), dir_ec);
            if (dir_ec)
            {
                system_detail::log_error(
                    "Source update cancel failed to prepare marker directory: "
                    + cancel_path.parent_path().string());
                return false;
            }

            std::ofstream marker(cancel_path, std::ios::binary | std::ios::trunc);
            marker << "cancel requested by runtime ui\n";
            marker.flush();
            const bool wrote_marker = static_cast<bool>(marker);
            marker.close();

            if (wrote_marker)
            {
                system_detail::append_log_line(
                    system_detail::source_update_log_path_for(target_binary),
                    "[WARN] Source rebuild cancellation requested from runtime UI.");
                system_detail::append_log_line(
                    system_detail::update_handoff_log_path_for(target_binary),
                    "[WARN] Source rebuild cancellation requested from runtime UI.");
            }
            else
            {
                system_detail::log_error("Source update cancel marker write failed.");
            }

            return wrote_marker;
        }
        catch (const std::exception& e)
        {
            system_detail::log_error(std::string{ "Source update cancel failed: " } + e.what());
            return false;
        }
        catch (...)
        {
            system_detail::log_error("Source update cancel failed with an unknown exception.");
            return false;
        }
#endif
    }

    export bool run_source_update_command(
        const UpdateChannel& channel,
        const bool recheck_source_version = true,
        const bool silent_worker = false,
        const bool honor_env_silent = true)
    {
        if (channel.source_url.empty())
        {
            system_detail::log_error("Source update URL is not configured.");
            return false;
        }

        const auto target_binary = system_detail::current_binary_path();

#if defined(_WIN32)
        if (system_detail::source_update_session_active(target_binary))
        {
            system_detail::append_log_line(
                system_detail::update_handoff_log_path_for(target_binary),
                "[WARN] Source update request ignored because an existing source-update session is still active.");
            system_detail::log_info("Source update request is already covered by an active source-update session.");
            return true;
        }

        if (recheck_source_version && !channel.source_version_url.empty())
        {
            const auto source_status =
                check_source_version_once_per_run(channel.source_version_url, "Source", PROJECT_SOURCE_VERSION);

            if (source_status.ok && source_status.update_available)
                system_detail::log_info("A newer source snapshot is available on main.");
            else if (source_status.ok)
                system_detail::log_info("No newer source snapshot is currently available; continuing because source update was requested explicitly.");
        }

        const bool effective_silent_worker =
            silent_worker || (honor_env_silent && system_detail::env_flag_enabled("EPOCH_UPDATER_SILENT"));
        const bool hide_worker_window =
            effective_silent_worker
            || !system_detail::env_flag_enabled("EPOCH_UPDATER_SHOW_WORKER_CONSOLE");

        if (!launch_source_update_worker(channel, target_binary, hide_worker_window))
            return false;

        if (effective_silent_worker)
        {
            system_detail::log_info("Silent source update worker launched; caller owns runtime shutdown after replacement evidence.");
            return true;
        }

        system_detail::log_info("Source update worker launched. Keep using Epoch while it builds; close and restart after the worker reports replacement evidence.");
        return true;
#else
        system_detail::ScopedInProcessSourceUpdate active_update{};
        if (!active_update.acquired)
        {
            system_detail::log_info("Source update request is already covered by the active Linux worker.");
            return true;
        }

        const auto archive_path = system_detail::source_archive_path(target_binary, channel.source_url);
        const auto staging_dir = system_detail::source_staging_dir(target_binary);
        const auto final_dir = system_detail::source_final_dir(target_binary);

        if (recheck_source_version && !channel.source_version_url.empty())
        {
            const std::string local_source_version =
                system_detail::read_local_source_version(final_dir);

            const auto source_status =
                check_source_version_once_per_run(channel.source_version_url, "Source", local_source_version);

            if (source_status.ok && source_status.update_available)
                system_detail::log_info("A newer source snapshot is available on main.");
            else if (source_status.ok)
                system_detail::log_info("No newer source snapshot is currently available; continuing because source update was requested explicitly.");
        }

        std::error_code ec;
        std::filesystem::remove_all(staging_dir, ec);
        ec.clear();
        std::filesystem::remove_all(final_dir, ec);
        ec.clear();
        std::filesystem::remove(archive_path, ec);
        ec.clear();

        system_detail::log_info(
            "Downloading latest "
            + system_detail::describe_source_archive(channel.source_url)
            + ".");
        if (!download_source_archive_with_fallbacks(channel.source_url, archive_path))
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

        if (system_detail::stop_source_update_if_canceled(
                target_binary,
                system_detail::source_update_log_path_for(target_binary),
                "source extraction"))
        {
            return false;
        }

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
        const bool force,
        const bool honor_env_silent = true,
        const UpdateHandoffMode packaged_handoff_mode = UpdateHandoffMode::LaunchImmediately)
    {
        UpdateCommandResult result{};
        const auto target_binary = system_detail::current_binary_path();

#if defined(_WIN32)
        if (system_detail::source_update_session_active(target_binary))
        {
            result.update_available = true;
            result.source_update_available = true;
            result.source_update_performed = true;
            result.source_fallback_attempted = true;
            result.status_message =
                "Source rebuild is already active. Keep Epoch open until the existing worker reports restart-ready, cancel, or failure evidence.";
            system_detail::append_log_line(
                system_detail::update_handoff_log_path_for(target_binary),
                "[WARN] Update request ignored because a source rebuild is already active.");
            system_detail::log_info("Update request ignored because an existing source-update session is still active.");
            return result;
        }
#endif

        cleanup_previous_update_artifacts();

        const std::string local_packaged_version =
            system_detail::extract_version_string(PROJECT_PACKAGED_VERSION);
        const std::string local_source_version =
            system_detail::extract_version_string(PROJECT_SOURCE_VERSION);
        const std::string platform_key{ platform::current_platform_key() };

        auto packaged_release = resolve_packaged_release_once_per_run();
        system_detail::VersionCheckResult packaged_status{};
        packaged_status.local = local_packaged_version;
        result.local_version = local_packaged_version;
        result.packaged_release_checked = true;

        if (packaged_release.found && !packaged_release.remote_version.empty())
        {
            result.packaged_release_found = true;
            packaged_status.ok = true;
            packaged_status.local = local_packaged_version;
            packaged_status.remote = packaged_release.remote_version;
            packaged_status.update_available =
                system_detail::compare_versions(local_packaged_version, packaged_release.remote_version) < 0;

            system_detail::log_info("Packaged Local  : " + packaged_status.local);
            system_detail::log_info("Packaged Remote : " + packaged_status.remote);
            if (!packaged_release.binary_name.empty())
                system_detail::log_info("Packaged Asset  : " + packaged_release.binary_name);

            if (packaged_status.ok)
            {
                const bool current_source_is_newer =
                    !local_source_version.empty()
                    && system_detail::compare_versions(local_source_version, packaged_status.remote) > 0;

                if (current_source_is_newer && packaged_status.update_available)
                {
                    result.packaged_release_reason =
                        "Current source build is newer than the latest packaged runtime "
                        + packaged_status.remote + ".";
                    system_detail::log_info(
                        "Ignoring packaged release because the current source build is already newer.");
                    packaged_status.update_available = false;
                }

                result.local_version = packaged_status.local;
                result.remote_version = packaged_status.remote;
            }
        }
        else
        {
            result.packaged_release_missing = true;
            result.packaged_release_reason =
                "No " + platform_key + " packaged runtime asset was found in the latest release.";
            system_detail::log_info(result.packaged_release_reason);
        }

        system_detail::VersionCheckResult source_status{};
        if (!channel.source_version_url.empty())
        {
            source_status = check_source_version_once_per_run(
                channel.source_version_url,
                "Source",
                PROJECT_SOURCE_VERSION);

            if (source_status.ok)
            {
                result.source_local_version = source_status.local;
                result.source_remote_version = source_status.remote;
                result.source_update_available = source_status.update_available;
            }
        }

        const bool candidate_update_available =
            packaged_status.update_available
            || (source_status.ok && source_status.update_available);
        if (candidate_update_available)
        {
            const auto build_status = check_platform_build_status_once_per_run(
                channel.platform_build_status_url,
                channel.platform_build_job_name);

            result.platform_build_checked = build_status.checked;
            result.platform_build_ok = build_status.ok;
            result.platform_build_pending = build_status.pending;
            result.platform_build_job = build_status.job_name;
            result.platform_build_status = build_status.status;
            result.platform_build_conclusion = build_status.conclusion;
            result.platform_build_url = build_status.url;
            result.platform_build_reason = build_status.reason;

            if (!build_status.ok)
            {
                const std::string job =
                    build_status.job_name.empty() ? std::string{ "platform build" } : build_status.job_name;
                const std::string reason =
                    build_status.reason.empty() ? std::string{ "build status could not be proven." } : build_status.reason;
                const bool transient_build_status_failure =
                    system_detail::build_status_failure_is_transient(build_status);
                const bool source_only_update =
                    source_status.ok
                    && source_status.update_available
                    && !packaged_status.update_available;

                if (source_only_update && transient_build_status_failure)
                {
                    result.update_available = true;
                    result.source_update_available = true;
                    result.status_message =
                        "Source update is available, but the platform build status check is temporarily unavailable: "
                        + reason;
                    system_detail::log_info(
                        "Continuing source update path despite transient platform build status failure: " + reason);
                }
                else
                {
                    packaged_status.update_available = false;
                    source_status.update_available = false;
                    result.packaged_update_available = false;
                    result.source_update_available = false;
                    result.update_available = false;
                    result.force_required = false;

                    result.status_message = "Update withheld until " + job + " is green: " + reason;
                    system_detail::log_info("Update withheld until " + job + " is green: " + reason);
                    return result;
                }
            }

            if (build_status.ok && !build_status.job_name.empty())
                system_detail::log_info("Update platform build gate passed: " + build_status.job_name);
        }

        const bool source_should_drive_update =
            source_status.ok
            && source_status.update_available
            && (!packaged_status.update_available
                || packaged_status.remote.empty()
                || source_status.remote.empty()
                || system_detail::compare_versions(source_status.remote, packaged_status.remote) >= 0);

        if (source_should_drive_update)
        {
            result.update_available = true;
            result.source_update_available = true;

            const std::string source = source_status.remote.empty()
                ? std::string{ "main source" }
                : std::string{ "main source " } + source_status.remote;
            const std::string packaged = packaged_status.remote.empty()
                ? std::string{ "packaged runtime" }
                : std::string{ "packaged runtime " } + packaged_status.remote;

            if (packaged_status.update_available)
            {
                result.packaged_release_reason =
                    packaged + " is available, but " + source
                    + " is the selected update lane so Cancel/progress remain available while the local source rebuild runs.";
                system_detail::log_info(
                    "Source update selected ahead of packaged runtime because main source is current or newer.");
            }

            if (!force)
            {
                result.force_required = true;
                result.status_message = "Source update available: " + source
                    + ". Update will build current main source locally.";
                if (packaged_status.update_available)
                    result.status_message += " " + result.packaged_release_reason;
                return result;
            }

            result.source_fallback_attempted = true;
            const std::string source_message = packaged_status.update_available
                ? result.packaged_release_reason + " Starting source update from main."
                : "No newer packaged runtime is selected. Starting source update from main.";
            system_detail::log_info(source_message);
            const bool worker_launched = run_source_update_command(channel, false, false, honor_env_silent);
            result.update_performed = false;
            result.source_update_performed = worker_launched;
            result.status_message = worker_launched
                ? "Source rebuild worker started. Keep Epoch open until the worker reports restart-ready evidence, then restart from the update panel."
                : "Source update failed to start. Check logs/epoch_source_update.log and logs/epoch_update_handoff.log beside the executable.";
            return result;
        }

        if (packaged_status.update_available)
        {
            result.packaged_update_available = true;
            result.update_available = true;

            if (!force)
            {
                result.force_required = true;
                if (source_status.ok && source_status.update_available)
                {
                    const std::string packaged = packaged_status.remote.empty()
                        ? std::string{ "packaged runtime" }
                        : std::string{ "packaged runtime " } + packaged_status.remote;
                    const std::string source = source_status.remote.empty()
                        ? std::string{ "main source" }
                        : std::string{ "main source " } + source_status.remote;
                    result.status_message =
                        "Update available: " + packaged + " first; " + source + " is also available after restart.";
                }
                else
                {
                    result.status_message = packaged_status.remote.empty()
                        ? "Packaged runtime update available."
                        : "Packaged runtime update available: " + packaged_status.remote + ".";
                }
                return result;
            }

            if (source_status.ok && source_status.update_available)
            {
                system_detail::log_info(
                    "A newer source snapshot is still available after the packaged restart.");
                system_detail::log_info(
                    "Run update again after restart if you want to continue from the packaged build to main source.");
            }

            result.update_performed = install_from_binary(
                packaged_release.binary_url,
                {},
                packaged_handoff_mode);
            result.packaged_update_performed = result.update_performed;
            result.packaged_handoff_staged =
                result.packaged_update_performed
                && packaged_handoff_mode == UpdateHandoffMode::StageForRestart;
            if (result.update_performed)
            {
                result.status_message = result.packaged_handoff_staged
                    ? "Packaged update staged. Press Restart to close Epoch and finish the hidden runtime replacement."
                    : "Packaged update replacement started. Restart Epoch if this window remains open.";
                if (source_status.ok && source_status.update_available)
                {
                    result.status_message += source_status.remote.empty()
                        ? " Main source is also available after restart."
                        : " Main source " + source_status.remote + " is also available after restart.";
                }
            }
            else
            {
                result.status_message =
                    "Packaged update failed before replacement. The cached package or replacement executable was not verified.";
            }
            return result;
        }

        if (source_status.ok && source_status.update_available)
        {
            result.update_available = true;

            if (!force)
            {
                result.force_required = true;
                if (result.packaged_release_missing)
                {
                    result.status_message =
                        "No " + platform_key + " packaged runtime asset was found. Source update is available: "
                        + source_status.remote + ".";
                }
                else
                {
                    result.status_message = source_status.remote.empty()
                        ? "Source update is available."
                        : "Source update is available: " + source_status.remote + ".";
                }
                return result;
            }

            result.source_fallback_attempted = true;
            const std::string fallback_message = result.packaged_release_missing
                ? "No " + platform_key + " packaged runtime asset was found. Starting source update from main."
                : "No newer packaged runtime is available. Starting source update from main.";
            system_detail::log_info(fallback_message);
            const bool worker_launched = run_source_update_command(channel, false, false, honor_env_silent);
            result.update_performed = false;
            result.source_update_performed = worker_launched;
            result.status_message = worker_launched
                ? "Source rebuild worker started. Keep Epoch open until the worker reports restart-ready evidence, then restart from the update modal."
                : "Source update failed to start. Check logs/epoch_source_update.log and logs/epoch_update_handoff.log beside the executable.";
            return result;
        }

        if (!result.packaged_release_reason.empty() && !source_status.ok)
        {
            result.status_message =
                result.packaged_release_reason
                + " Source update availability could not be proven.";
        }
        else if (!result.packaged_release_reason.empty())
        {
            result.status_message = result.packaged_release_reason;
        }
        else
        {
            result.status_message = "Epoch is already current.";
        }

        return result;
    }
}
