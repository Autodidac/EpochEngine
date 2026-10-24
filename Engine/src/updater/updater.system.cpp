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
#include "epoch.build_layout.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
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

#include <openssl/evp.h>
#include "../../include/updater.runtime_identity.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

module updater.system;

import core.logger;
import core.path;
import core.sha256;
import epoch.cli;
import epoch.version;
import platform.engine;
import updater.tools;
import updater.config;
import updater.source_access;

namespace epochengine::updater
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

        void log_info(const std::string& message)
        {
            logger::get("Updater").log(
                logger::LogLevel::INFO,
                logger_safe(message),
                std::source_location::current());
        }

        void log_error(const std::string& message)
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

        [[nodiscard]] inline bool source_tree_matches_edition(const std::filesystem::path& root)
        {
            std::error_code ec;
            const auto marker = root / "Engine/include/epoch.edition.hpp";
            if (!std::filesystem::is_regular_file(marker, ec) || ec
                || std::filesystem::is_symlink(marker, ec) || ec
                || std::filesystem::file_size(marker, ec) > 4096u || ec) return false;
            return PROJECT_SOURCE_EDITION_MATCHES(read_text_file(marker));
        }

        [[nodiscard]] inline bool local_source_edition_allowed(const std::filesystem::path& root)
        {
            std::error_code ec;
            const bool has_marker = std::filesystem::exists(root / "Engine/include/epoch.edition.hpp", ec);
            // Retain read admission for historical owner snapshots. A present
            // marker can never be ignored; public has no marker-less legacy.
            return !ec && (has_marker ? source_tree_matches_edition(root) : !PUBLIC_DISTRIBUTION);
        }

        [[nodiscard]] inline bool markerless_runtime_allowed(
            const std::string_view expected_version) noexcept
        {
            // Only the already verified historical Site runtime is grandfathered.
            // A new or public runtime always needs an explicit payload identity.
            return runtime_identity::markerless_allowed(PUBLIC_DISTRIBUTION, expected_version);
        }

        [[nodiscard]] inline bool runtime_payload_matches_edition(
            const std::filesystem::path& payload_root,
            const std::string_view expected_version)
        {
            const auto marker = payload_root / "epoch-distribution.json";
            std::error_code ec;
            if (std::filesystem::is_symlink(marker, ec)) return false;
            ec.clear();
            const bool present = std::filesystem::exists(marker, ec);
            if (ec) return false;
            if (!present) return markerless_runtime_allowed(expected_version);
            if (!std::filesystem::is_regular_file(marker, ec) || ec
                || std::filesystem::file_size(marker, ec) > 4096u || ec)
                return false;
            return runtime_identity::matches(read_text_file(marker), expected_version, EDITION_ID);
        }

        [[nodiscard]] inline std::optional<std::string> sha256_file_hex(
            const std::filesystem::path& path)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return std::nullopt;

            core::sha256::Hasher hasher{};
            std::array<char, 64u * 1024u> buffer{};
            while (in)
            {
                in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                const auto count = in.gcount();
                if (count > 0)
                {
                    hasher.update(std::string_view{
                        buffer.data(),
                        static_cast<std::size_t>(count) });
                }
            }

            if (!in.eof())
                return std::nullopt;

            return core::sha256::hex(hasher.finish());
        }

        [[nodiscard]] inline std::optional<std::string> parse_sha256_text(
            std::string text)
        {
            text = trim_ascii(std::move(text));
            if (text.size() < 64u)
                return std::nullopt;

            const auto digest =
                core::sha256::from_hex(text.substr(0u, 64u));
            if (!digest)
                return std::nullopt;
            return core::sha256::hex(*digest);
        }

        [[nodiscard]] inline bool verify_sha256(
            const std::filesystem::path& path,
            const std::string_view expected_hex)
        {
            const auto expected = core::sha256::from_hex(expected_hex);
            const auto observed = sha256_file_hex(path);
            return expected.has_value()
                && observed.has_value()
                && *observed == core::sha256::hex(*expected);
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

        // Install a verified compatible binary first. Source is the authenticated
        // fallback only when no eligible newer packaged runtime exists.
        [[nodiscard]] inline bool prefer_source_release(
            const VersionCheckResult& source, const VersionCheckResult& packaged)
        {
            return source.ok && source.update_available && !source.remote.empty()
                && !packaged.update_available;
        }

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

        void append_log_line(
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
            // The explicit candidate data root outranks ambient updater paths.
            // This routes storage only; it does not authorize an update.
            if (const auto candidateData = epochengine::core::path::candidate_data_root();
                !candidateData.empty())
                return ensure_directory(candidateData / CACHE_ROOT_SUBDIR());

            std::error_code ec;
            auto root = env_path("EPOCH_UPDATER_CACHE_ROOT");
            if (!root.empty())
                return ensure_directory(root);

            if (root.empty())
                root = epochengine::core::path::executable_dir();
            if (root.empty())
                root = std::filesystem::current_path(ec);
            if (root.empty() || ec)
                root = std::filesystem::path{ "." };

            return ensure_directory(root / CACHE_ROOT_SUBDIR());
        }

        [[nodiscard]] inline std::filesystem::path updater_cache_root()
        {
            return ensure_directory(runtime_cache_root() / UPDATER_CACHE_SUBDIR()
                / (PUBLIC_DISTRIBUTION ? "public" : "development"));
        }

        [[nodiscard]] inline std::filesystem::path package_cache_root()
        {
            return ensure_directory(runtime_cache_root() / PACKAGE_CACHE_SUBDIR()
                / (PUBLIC_DISTRIBUTION ? "public" : "development"));
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

#include "implementation/updater.release_integrity.inl"
#include "implementation/updater.toolchain_bootstrap.inl"
#include "implementation/updater.source_worker.inl"
#if defined(_WIN32)
    [[nodiscard]] bool launch_source_update_worker(
        const UpdateChannel& channel,
        const std::filesystem::path& target_binary,
        bool silent_worker,
        const PreparedSourceArchive& prepared_source);
#endif

    void cleanup_previous_update_artifacts()
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
        const UpdateHandoffMode handoff_mode = UpdateHandoffMode::LaunchImmediately,
        const std::string_view expected_version = {})
    {
        const auto extracted_binary = system_detail::resolve_runtime_binary_path(extracted_runtime_dir, target_binary);
        const auto runtime_payload_dir = extracted_binary.parent_path();
        if (!system_detail::runtime_payload_matches_edition(runtime_payload_dir, expected_version))
        {
            system_detail::log_error("Packaged replacement refused a missing, malformed, foreign-edition or wrong-version runtime identity.");
            return false;
        }
#if defined(_WIN32)
        const auto target_dir = target_binary.parent_path();
        const auto script_path = handoff_mode == UpdateHandoffMode::StageForRestart
            ? system_detail::staged_update_handoff_script_path()
            : system_detail::make_temp_script_path("replace_runtime_zip");
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
            << "if exist \"%EXTRACTED%\\epoch-distribution.json\" (\r\n"
            << "  copy /Y \"%EXTRACTED%\\epoch-distribution.json\" \"%TARGETDIR%\\epoch-distribution.json\" >nul\r\n"
            << "  if errorlevel 1 exit /b 1\r\n"
            << ") else (\r\n"
            << "  del /F /Q \"%TARGETDIR%\\epoch-distribution.json\" >nul 2>&1\r\n"
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
            << "if [ -f \"$EXTRACTED/epoch-distribution.json\" ]; then\n"
            << "  cp -f \"$EXTRACTED/epoch-distribution.json\" \"$TARGETDIR/epoch-distribution.json\" || exit 1\n"
            << "else\n"
            << "  rm -f \"$TARGETDIR/epoch-distribution.json\" || exit 1\n"
            << "fi\n"
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
            << "set \"EPOCH_UPDATER_SHELL_AUTO_COMMAND=\"\r\n"
            << "set \"EPOCH_EDITOR_AUTO_COMMAND=\"\r\n"
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
        const auto active_marker_path = system_detail::source_active_run_path(target_binary);
        const auto cancel_marker_path = system_detail::source_cancel_path(target_binary);

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
            << "ACTIVE=" << system_detail::quote_shell_arg(active_marker_path.string()) << "\n"
            << "CANCEL=" << system_detail::quote_shell_arg(cancel_marker_path.string()) << "\n"
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
            << "if [ -d \"$BUILTDIR/lib\" ]; then\n"
            << "  mkdir -p \"$TARGETDIR/lib\"\n"
            << "  if ! cp -R \"$BUILTDIR/lib/.\" \"$TARGETDIR/lib/\"; then\n"
            << "    echo \"[ERROR] Failed to copy rebuilt Linux runtime libraries.\" >> \"$LOG\"\n"
            << "    exit 1\n"
            << "  fi\n"
            << "fi\n"
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
            << "rm -f \"$ACTIVE\" \"$CANCEL\"\n"
            << "cd \"$TARGETDIR\"\n"
            << "unset EPOCH_UPDATER_SHELL_AUTO_COMMAND EPOCH_EDITOR_AUTO_COMMAND || true\n"
            << "env -u EPOCH_UPDATER_SHELL_AUTO_COMMAND -u EPOCH_EDITOR_AUTO_COMMAND \"$TARGETEXE\" >/dev/null 2>&1 &\n"
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

        std::string request_url = url;
        if (request_url.find("api.github.com/repos/") != std::string::npos
            && request_url.find("/contents/") != std::string::npos)
        {
            request_url += request_url.find('?') == std::string::npos ? '?' : '&';
            request_url += "epoch_cache_bust="
                + std::to_string(static_cast<unsigned long long>(
                    std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        }

        if (!download_file(request_url, tmp.string()))
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
        if (!PUBLIC_DISTRIBUTION || primary_url != PROJECT_SOURCE_URL())
        {
            system_detail::log_error("Source download refused a different edition or repository.");
            return false;
        }
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
            if (url != PROJECT_SOURCE_URL())
            {
                system_detail::log_error("Source fallback refused a different edition or repository.");
                return false;
            }
            system_detail::log_info("Source snapshot download URL: " + url);
            if (download_update_file_atomic(url, final_path))
            {
                const auto expected =
                    system_detail::download_expected_sha256(url == PROJECT_SOURCE_URL()
                        ? PROJECT_SOURCE_CHECKSUM_URL() : url + ".sha256");
                if (expected
                    && system_detail::verify_sha256(final_path, *expected))
                {
                    system_detail::log_info(
                        "Source snapshot SHA-256 verified.");
                    return true;
                }

                std::error_code remove_ec;
                std::filesystem::remove(final_path, remove_ec);
                system_detail::log_error(
                    "Source snapshot SHA-256 verification failed for: " + url);
            }

            system_detail::log_error("Source snapshot download failed from: " + url);
        }

        system_detail::log_error("Source snapshot download failed from all configured URLs.");
        return false;
    }

    namespace
    {
        [[nodiscard]] bool source_authority_shape(
            const std::filesystem::path& root,
            const bool require_checkout)
        {
            std::error_code ec{};
            if (root.empty() || !root.is_absolute()
                || !std::filesystem::is_directory(root, ec)
                || std::filesystem::is_symlink(root, ec))
            {
                return false;
            }
            const std::array required{
                root / "Engine.sln",
                root / "Engine/modules/epoch.version.ixx",
                root / "Engine/modules/engine.version.ixx"};
            for (const auto& path : required)
            {
                ec.clear();
                if (!std::filesystem::is_regular_file(path, ec)
                    || std::filesystem::is_symlink(path, ec))
                    return false;
            }
            if (require_checkout)
            {
                ec.clear();
                const auto git = root / ".git";
                if (!std::filesystem::exists(git, ec)
                    || std::filesystem::is_symlink(git, ec))
                    return false;
            }
            return true;
        }

        [[nodiscard]] std::optional<std::string> receipt_value(
            const std::string_view receipt,
            const std::string_view key)
        {
            const std::string prefix = std::string{key} + "=";
            std::optional<std::string> result{};
            std::size_t offset{};
            while (offset <= receipt.size())
            {
                const std::size_t end = receipt.find('\n', offset);
                const std::string_view line = receipt.substr(
                    offset, end == std::string_view::npos
                        ? receipt.size() - offset : end - offset);
                if (line.starts_with(prefix))
                {
                    if (result)
                        return std::nullopt;
                    result = std::string{line.substr(prefix.size())};
                }
                if (end == std::string_view::npos)
                    break;
                offset = end + 1u;
            }
            return result;
        }

        [[nodiscard]] bool lowercase_hex_commit(const std::string_view commit)
        {
            return commit.size() == 40u
                && std::all_of(commit.begin(), commit.end(), [](const char ch)
                    { return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'); });
        }

        [[nodiscard]] std::optional<std::string> checkout_commit_identity(
            const std::filesystem::path& checkout)
        {
            namespace fs = std::filesystem;
            std::error_code ec{};
            fs::path git_dir = checkout / ".git";
            if (fs::is_regular_file(git_dir, ec))
            {
                const std::string marker = system_detail::trim_ascii(
                    system_detail::read_text_file(git_dir));
                constexpr std::string_view prefix = "gitdir:";
                if (!marker.starts_with(prefix))
                    return std::nullopt;
                git_dir = fs::path{system_detail::trim_ascii(
                    marker.substr(prefix.size()))};
                if (git_dir.is_relative())
                    git_dir = checkout / git_dir;
            }
            ec.clear();
            git_dir = fs::weakly_canonical(git_dir, ec);
            if (ec || !fs::is_directory(git_dir, ec)
                || fs::is_symlink(git_dir, ec))
                return std::nullopt;

            std::vector<fs::path> ref_roots{git_dir};
            const std::string common_marker = system_detail::trim_ascii(
                system_detail::read_text_file(git_dir / "commondir"));
            if (!common_marker.empty())
            {
                fs::path common_dir{common_marker};
                if (common_dir.is_relative())
                    common_dir = git_dir / common_dir;
                ec.clear();
                common_dir = fs::weakly_canonical(common_dir, ec);
                if (ec || !fs::is_directory(common_dir, ec)
                    || fs::is_symlink(common_dir, ec))
                    return std::nullopt;
                if (common_dir != git_dir)
                    ref_roots.push_back(std::move(common_dir));
            }

            const std::string head = system_detail::trim_ascii(
                system_detail::read_text_file(git_dir / "HEAD"));
            if (lowercase_hex_commit(head))
                return head;
            constexpr std::string_view ref_prefix = "ref:";
            if (!head.starts_with(ref_prefix))
                return std::nullopt;
            const std::string ref = system_detail::trim_ascii(
                head.substr(ref_prefix.size()));
            if (ref.empty() || ref.starts_with('/') || ref.find("..") != std::string::npos)
                return std::nullopt;
            for (const auto& ref_root : ref_roots)
            {
                const std::string loose = system_detail::trim_ascii(
                    system_detail::read_text_file(ref_root / fs::path{ref}));
                if (lowercase_hex_commit(loose))
                    return loose;

                const std::string packed = system_detail::read_text_file(
                    ref_root / "packed-refs");
                std::size_t offset{};
                while (offset <= packed.size())
                {
                    const std::size_t end = packed.find('\n', offset);
                    const std::string_view line = std::string_view{packed}.substr(
                        offset, end == std::string::npos
                            ? packed.size() - offset : end - offset);
                    const std::size_t separator = line.find(' ');
                    if (separator == 40u && line.substr(separator + 1u) == ref)
                    {
                        const std::string commit{line.substr(0u, separator)};
                        if (lowercase_hex_commit(commit))
                            return commit;
                    }
                    if (end == std::string::npos)
                        break;
                    offset = end + 1u;
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] VerifiedSourceAuthority resolve_source_authority_at(
            const std::filesystem::path& explicit_checkout,
            const std::filesystem::path& cached_root)
        {
            namespace fs = std::filesystem;
            std::error_code ec{};
            if (!explicit_checkout.empty())
            {
                const auto checkout = fs::weakly_canonical(explicit_checkout, ec);
                if (!ec && source_authority_shape(checkout, false)
                    && system_detail::local_source_edition_allowed(checkout))
                {
                    const std::string version = system_detail::read_local_source_version(checkout);
                    const auto version_digest = system_detail::sha256_file_hex(
                        checkout / "Engine/modules/engine.version.ixx");
                    const auto commit = checkout_commit_identity(checkout);
                    if (!version.empty() && version_digest)
                    {
                        if (commit)
                        {
                            return VerifiedSourceAuthority{
                                .kind = SourceAuthorityKind::explicit_checkout,
                                .root = checkout,
                                .source_version = version,
                                .commit = *commit,
                                .receipt_digest = *version_digest,
                                .status_message = "Using the explicit verified Epoch development checkout.",
                                .verified = true};
                        }

                        // A distributed/extracted Epoch source tree may not carry
                        // Git metadata. Self-iteration still needs a stable, local
                        // read authority for exactly those bytes. Bind that
                        // authority to the version-file digest and rehash every
                        // curated source file before it is shared or edited.
                        return VerifiedSourceAuthority{
                            .kind = SourceAuthorityKind::local_snapshot,
                            .root = checkout,
                            .source_version = version,
                            .commit = version_digest->substr(0u, 40u),
                            .receipt_digest = *version_digest,
                            .status_message = "Using the local Epoch source snapshot; Git metadata is absent, so authority is digest-bound to current local source bytes.",
                            .verified = true};
                    }
                }
            }

            ec.clear();
            const auto cached = fs::weakly_canonical(cached_root, ec);
            const auto receipt_path = cached / ".epoch-source-authority";
            if (ec || !source_authority_shape(cached, false)
                || !system_detail::local_source_edition_allowed(cached)
                || !fs::is_regular_file(receipt_path, ec)
                || fs::is_symlink(receipt_path, ec))
            {
                return VerifiedSourceAuthority{
                    .status_message = "No explicit Epoch checkout or verified cached source receipt is available. Download Project Source explicitly, then retry."};
            }
            const std::string receipt = system_detail::read_text_file(receipt_path);
            const auto schema = receipt_value(receipt, "schema");
            const auto version = receipt_value(receipt, "source_version");
            const auto commit = receipt_value(receipt, "commit");
            const auto format = receipt_value(receipt, "archive_format");
            const auto archive_digest = receipt_value(receipt, "archive_sha256");
            const auto receipt_digest = system_detail::sha256_file_hex(receipt_path);
            if (!schema || *schema != "epoch.source.authority.v1"
                || !version || version->empty() || !commit
                || !lowercase_hex_commit(*commit) || !format
                || (*format != "zip" && *format != "tar.gz")
                || !archive_digest
                || !core::sha256::from_hex(*archive_digest)
                || !receipt_digest
                || system_detail::read_local_source_version(cached) != *version)
            {
                return VerifiedSourceAuthority{
                    .status_message = "The cached source authority receipt is missing, malformed, or disagrees with the extracted source version."};
            }
            return VerifiedSourceAuthority{
                .kind = SourceAuthorityKind::verified_cache,
                .root = cached,
                .source_version = *version,
                .commit = *commit,
                .receipt_digest = *receipt_digest,
                .status_message = "Using an authenticated extracted source cache; each curated file is rehashed from current bytes before sharing.",
                .verified = true};
        }

        [[nodiscard]] bool write_source_authority_receipt(
            const std::filesystem::path& root,
            const PreparedSourceArchive& source)
        {
            if (!source.ok || source.source_version.empty()
                || !lowercase_hex_commit(source.commit)
                || (source.archive_format != "zip" && source.archive_format != "tar.gz"))
                return false;
            const auto archive_digest = system_detail::sha256_file_hex(
                source.archive_path);
            if (!archive_digest)
                return false;
            const std::string receipt =
                "schema=epoch.source.authority.v1\nsource_version="
                + source.source_version + "\ncommit=" + source.commit
                + "\narchive_format=" + source.archive_format
                + "\narchive_sha256=" + *archive_digest + "\n";
            const auto path = root / ".epoch-source-authority";
            const auto temporary = root / ".epoch-source-authority.tmp";
            std::ofstream out{temporary, std::ios::binary | std::ios::trunc};
            out.write(receipt.data(), static_cast<std::streamsize>(receipt.size()));
            out.close();
            if (!out.good())
                return false;
            std::error_code ec{};
            std::filesystem::rename(temporary, path, ec);
            if (ec)
            {
                std::filesystem::remove(path, ec);
                ec.clear();
                std::filesystem::rename(temporary, path, ec);
            }
            return !ec;
        }
    }

    VerifiedSourceAuthority resolve_verified_source_authority(
        const std::filesystem::path& explicit_checkout)
    {
        return resolve_source_authority_at(
            explicit_checkout,
            system_detail::project_source_final_dir());
    }

    bool verified_source_authority_contract_self_test()
    {
        namespace fs = std::filesystem;
        const auto root = fs::temp_directory_path()
            / ("epoch_source_authority_" + system_detail::make_source_update_run_token());
        const auto checkout = root / "checkout";
        const auto linked_checkout = root / "linked-checkout";
        const auto local_snapshot = root / "local-snapshot";
        const auto common_git = root / "common.git";
        const auto linked_git = common_git / "worktrees" / "linked";
        const auto cached = root / "cached";
        const auto write_shape = [](const fs::path& path)
            {
                std::error_code ec{};
                fs::create_directories(path / "Engine/modules", ec);
                std::ofstream{path / "Engine.sln"} << "solution\n";
                std::ofstream{path / "Engine/modules/epoch.version.ixx"}
                    << "#define EPOCH_VERSION_MAJOR_VALUE 0\n#define EPOCH_VERSION_MINOR_VALUE 89\n#define EPOCH_VERSION_REVISION_VALUE 32\n";
                std::ofstream{path / "Engine/modules/engine.version.ixx"}
                    << "#define EPOCH_VERSION_MAJOR_VALUE 0\n#define EPOCH_VERSION_MINOR_VALUE 89\n#define EPOCH_VERSION_REVISION_VALUE 32\n";
                fs::create_directories(path / "Engine/include", ec);
                std::ofstream{path / "Engine/include/epoch.edition.hpp"}
                    << "#define EPOCH_SOURCE_EDITION_PUBLIC " << (PUBLIC_DISTRIBUTION ? "1\n" : "0\n");
                return !ec;
            };
        const bool shaped = write_shape(checkout) && write_shape(linked_checkout)
            && write_shape(local_snapshot) && write_shape(cached);
        std::error_code git_ec{};
        fs::create_directories(checkout / ".git", git_ec);
        std::ofstream{checkout / ".git/HEAD"}
            << "fedcbafedcbafedcbafedcbafedcbafedcbafedc\n";
        std::ofstream{cached / ".epoch-source-authority"}
            << "schema=epoch.source.authority.v1\nsource_version=0.89.32\n"
               "commit=0123456789012345678901234567890123456789\narchive_format=zip\n"
               "archive_sha256=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
        fs::create_directories(linked_git, git_ec);
        fs::create_directories(common_git / "refs/heads", git_ec);
        std::ofstream{linked_checkout / ".git"}
            << "gitdir: " << linked_git.generic_string() << "\n";
        std::ofstream{linked_git / "HEAD"}
            << "ref: refs/heads/linked-authority\n";
        std::ofstream{linked_git / "commondir"} << "../..\n";
        std::ofstream{common_git / "refs/heads/linked-authority"}
            << "abcdefabcdefabcdefabcdefabcdefabcdefabcd\n";
        const auto explicit_result = shaped
            ? resolve_source_authority_at(checkout, cached)
            : VerifiedSourceAuthority{};
        const auto linked_result = shaped && !git_ec
            ? resolve_source_authority_at(linked_checkout, cached)
            : VerifiedSourceAuthority{};
        const auto local_snapshot_result = shaped
            ? resolve_source_authority_at(local_snapshot, cached)
            : VerifiedSourceAuthority{};
        const auto cached_result = resolve_source_authority_at({}, cached);
        std::error_code cleanup_ec{};
        fs::remove_all(root, cleanup_ec);
        return explicit_result.verified
            && explicit_result.kind == SourceAuthorityKind::explicit_checkout
            && explicit_result.commit == "fedcbafedcbafedcbafedcbafedcbafedcbafedc"
            && linked_result.verified
            && linked_result.kind == SourceAuthorityKind::explicit_checkout
            && linked_result.commit == "abcdefabcdefabcdefabcdefabcdefabcdefabcd"
            && local_snapshot_result.verified
            && local_snapshot_result.kind == SourceAuthorityKind::local_snapshot
            && local_snapshot_result.commit.size() == 40u
            && local_snapshot_result.receipt_digest.size() == 64u
            && cached_result.verified
            && cached_result.kind == SourceAuthorityKind::verified_cache
            && cached_result.commit == "0123456789012345678901234567890123456789";
    }

    ProjectSourceDownloadResult download_project_source_code(
        const UpdateChannel& channel)
    {
        ProjectSourceDownloadResult result{};
        if (!PROJECT_SOURCE_CHANNEL_ALLOWED(channel.source_url, channel.source_version_url))
        {
            result.status_message = "Source download refused: this channel belongs to another edition or repository.";
            system_detail::log_error(result.status_message);
            return result;
        }
        if (!SOURCE_PROJECT_DOWNLOAD_ENABLED)
        {
            result.status_message =
                "Project source code download is disabled by this build's distribution policy.";
            system_detail::log_error(result.status_message);
            return result;
        }

        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path cache_root = system_detail::project_source_cache_root();
        PreparedSourceArchive authorized_source{};
        fs::path archive_path{};
        if (channel.source_url.empty())
        {
            authorized_source = acquire_private_source_archive(cache_root);
            if (!authorized_source.ok)
            {
                result.status_message = authorized_source.message;
                system_detail::log_error(result.status_message);
                return result;
            }
            archive_path = authorized_source.archive_path;
        }
        else
        {
            archive_path = system_detail::project_source_archive_path(channel.source_url);
        }
        const auto cleanup_authorized_source = [&]()
            {
                if (!authorized_source.cleanup_root.empty())
                {
                    std::error_code cleanup_ec;
                    fs::remove_all(authorized_source.cleanup_root, cleanup_ec);
                }
            };
        const fs::path staging_dir = system_detail::project_source_staging_dir();
        const fs::path final_dir = system_detail::project_source_final_dir();

        fs::remove_all(staging_dir, ec);
        ec.clear();
        if (!channel.source_url.empty())
            fs::remove(archive_path, ec);
        ec.clear();
        fs::create_directories(archive_path.parent_path(), ec);
        if (ec)
        {
            cleanup_authorized_source();
            result.status_message = "Project source code download failed: could not prepare cache downloads folder.";
            system_detail::log_error(result.status_message + " " + archive_path.parent_path().string());
            return result;
        }

        if (!channel.source_url.empty())
        {
            system_detail::log_info("Downloading project source code snapshot into cache: " + cache_root.string());
            if (!download_source_archive_with_fallbacks(channel.source_url, archive_path))
            {
                cleanup_authorized_source();
                result.status_message = "Project source code download failed: source archive download did not complete.";
                return result;
            }
        }

        fs::remove_all(staging_dir, ec);
        ec.clear();
        if (!extract_archive(archive_path.string(), staging_dir.string()))
        {
            fs::remove_all(staging_dir, ec);
            cleanup_authorized_source();
            result.status_message = "Project source code download failed: downloaded source archive did not extract.";
            system_detail::log_error(result.status_message);
            return result;
        }

        fs::path extracted_root = system_detail::first_subdirectory(staging_dir);
        if (extracted_root.empty())
            extracted_root = staging_dir;

        if (!system_detail::source_tree_matches_edition(extracted_root))
        {
            fs::remove_all(staging_dir, ec);
            cleanup_authorized_source();
            result.status_message = "Source download refused: the archive edition does not match this installed engine.";
            system_detail::log_error(result.status_message);
            return result;
        }

        fs::remove_all(final_dir, ec);
        ec.clear();
        fs::create_directories(final_dir.parent_path(), ec);
        if (ec)
        {
            fs::remove_all(staging_dir, ec);
            cleanup_authorized_source();
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
                cleanup_authorized_source();
                result.status_message = "Project source code download failed: could not move source project into cache.";
                system_detail::log_error(result.status_message + " " + final_dir.string());
                return result;
            }
        }

        fs::remove_all(staging_dir, ec);
        if (authorized_source.ok
            && !write_source_authority_receipt(final_dir, authorized_source))
        {
            cleanup_authorized_source();
            result.status_message = "Project source code was extracted, but its verified source-authority receipt could not be published.";
            system_detail::log_error(result.status_message);
            return result;
        }
        cleanup_authorized_source();
        result.ok = true;
        result.project_root = final_dir;
        result.status_message = "Project source code downloaded to cache: " + final_dir.string();
        system_detail::log_info(result.status_message);
        return result;
    }

    [[nodiscard]] bool prepare_cached_update_archive(
        const std::string& url,
        const std::string& checksum_url,
        const std::string_view signed_sha256,
        const std::filesystem::path& archive_path,
        const std::filesystem::path& extract_dir)
    {
        namespace fs = std::filesystem;

        std::error_code ec;
        const auto expected_sha256 =
            system_detail::resolve_expected_sha256(checksum_url, signed_sha256);
        if (!expected_sha256)
        {
            system_detail::log_error(
                "Update package checksum metadata is missing or invalid.");
            return false;
        }

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
                if (system_detail::verify_sha256(archive_path, *expected_sha256)
                    && extract_archive(archive_path.string(), extract_dir.string()))
                {
                    system_detail::log_info(
                        "Using SHA-256 verified cached update package.");
                    return true;
                }

                system_detail::log_error(
                    "Cached update package failed checksum or extraction validation; redownloading.");
            }

            ec.clear();
            fs::remove_all(extract_dir, ec);
            ec.clear();
            fs::remove(archive_path, ec);
        }

        system_detail::log_info("Downloading update package into cache: " + archive_path.string());
        if (!download_update_file_atomic(url, archive_path))
            return false;

        if (!system_detail::verify_sha256(archive_path, *expected_sha256))
        {
            system_detail::log_error(
                "Downloaded update package failed SHA-256 verification; removing cached package.");
            fs::remove(archive_path, ec);
            return false;
        }

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
        const std::string& checksum_url,
        const std::string_view restart_auto_command = {},
        const UpdateHandoffMode handoff_mode = UpdateHandoffMode::LaunchImmediately,
        const std::string_view signed_sha256 = {},
        const std::string_view expected_version = {})
    {
        if (!PROJECT_RELEASE_DOWNLOAD_ALLOWED(url)
            || (!checksum_url.empty() && !PROJECT_RELEASE_DOWNLOAD_ALLOWED(checksum_url)))
        {
            system_detail::log_error("Packaged update refused a different edition or repository.");
            return false;
        }
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

            if (!prepare_cached_update_archive(
                    url,
                    checksum_url,
                    signed_sha256,
                    archive_path,
                    extract_dir))
                return false;

            return replace_runtime_from_script(
                target_binary,
                extract_dir,
                archive_path,
                restart_auto_command,
                handoff_mode,
                expected_version);
        }

        if (!system_detail::markerless_runtime_allowed(expected_version))
        {
            system_detail::log_error("Raw executable update refused: no independently verified embedded edition identity is available. Use an edition-marked runtime archive.");
            return false;
        }
        const auto new_binary = system_detail::replacement_binary_path(target_binary);

        if (!download_update_file_atomic(url, new_binary))
            return false;

        const auto expected_sha256 =
            system_detail::resolve_expected_sha256(checksum_url, signed_sha256);
        if (!expected_sha256 || !system_detail::verify_sha256(new_binary, *expected_sha256))
        {
            system_detail::log_error("Downloaded update binary failed SHA-256 verification.");
            std::error_code ec;
            std::filesystem::remove(new_binary, ec);
            return false;
        }

        return replace_binary(target_binary, new_binary, handoff_mode);
    }

    bool update_discovery_contract_self_test()
    {
        // Source and packaged releases are independent platform authorities.
        // Exercise historical spelling without pinning this contract to the
        // previous development version or requiring all platforms to advance.
#if defined(EPOCH_UPDATER_SHELL_BUILD) && EPOCH_UPDATER_SHELL_BUILD == 1
        const std::string sourceVersion{"0.0.0"}; // No source runtime installed yet.
#else
        const auto sourceVersion = epochengine::FormatVersionString(
            epochengine::GetMajor(), epochengine::GetMinor(), epochengine::GetRevision());
#endif
#if defined(EPOCH_UPDATER_SHELL_BUILD) && EPOCH_UPDATER_SHELL_BUILD == 1
        const std::string packagedVersion{"0.0.0"};
#else
        const auto packagedVersion = epochengine::FormatVersionString(
            epochengine::GetPackagedMajor(), epochengine::GetPackagedMinor(),
            epochengine::GetPackagedRevision());
#endif
        const std::string padding(128u * 1024u, 'x');
        const std::string document =
            "{\"canonical_payload\":\"{\\\"schema\\\":\\\"epoch-test/v1\\\","
            "\\\"padding\\\":\\\"" +
            padding +
            "\\\"}\","
            "\"signature\":{\"algorithm\":\"Ed25519\","
            "\"key_id\":\"test-key\",\"value\":\"test-signature\"}}";
        const std::string payload =
            system_detail::extract_json_string_field(document, "canonical_payload");
        const std::string signature =
            system_detail::extract_json_object_text(document, "signature");
        std::string too_deep;
        for (std::size_t depth = 0u; depth <= system_detail::k_release_json_max_depth; ++depth)
        {
            too_deep.push_back('[');
        }
        too_deep += "0";
        for (std::size_t depth = 0u; depth <= system_detail::k_release_json_max_depth; ++depth)
        {
            too_deep.push_back(']');
        }
        std::string too_many_items = "[";
        std::string too_many_fields = "{";
        std::string too_long_string = "{\"value\":\"";
        for (std::size_t index = 0u; index <= system_detail::k_release_json_max_array_items;
             ++index)
        {
            if (index != 0u)
            {
                too_many_items.push_back(',');
            }
            too_many_items += "0";
        }
        for (std::size_t index = 0u; index <= system_detail::k_release_json_max_object_fields;
             ++index)
        {
            if (index != 0u)
                too_many_fields.push_back(',');
            too_many_fields += "\"f" + std::to_string(index) + "\":0";
        }
        too_many_items.push_back(']');
        too_many_fields.push_back('}');
        too_long_string.append(system_detail::k_release_json_max_string_bytes + 1u, 'x');
        too_long_string += "\"}";
        const std::string too_large(system_detail::k_release_json_max_input_bytes + 1u, ' ');
        return system_detail::json_document_within_release_limits(document) &&
               payload.starts_with("{\"schema\":\"epoch-test/v1\"") && payload.ends_with("\"}") &&
               payload.size() > padding.size() &&
               system_detail::json_document_within_release_limits(payload) &&
               system_detail::extract_json_string_field(signature, "algorithm") == "Ed25519" &&
               system_detail::extract_json_string_field(signature, "key_id") == "test-key" &&
               !system_detail::json_document_within_release_limits(too_deep) &&
               !system_detail::json_document_within_release_limits(too_many_items) &&
               !system_detail::json_document_within_release_limits(too_many_fields) &&
               !system_detail::json_document_within_release_limits(too_long_string) &&
               !system_detail::json_document_within_release_limits(too_large) &&
               PROJECT_SOURCE_VERSION == sourceVersion &&
               PROJECT_PACKAGED_VERSION == packagedVersion &&
               epochengine::FormatVersionString(0, 89, 6) == "0.89.06" &&
               epochengine::FormatVersionString(0, 89, 30) == "0.89.30" &&
               epochengine::FormatVersionString(0, 90, 2) == "0.90.02" &&
               epochengine::FormatVersionString(1, 0, 0) == "1.0.0" &&
               system_detail::compare_versions("0.90.02", "0.89.35") > 0 &&
               system_detail::compare_versions("0.89.30", "0.90.02") < 0 &&
               system_detail::compare_versions("0.89.06", "0.89.6") == 0 &&
               SOURCE_VERSION_FILE_NAME() == "Engine/modules/epoch.version.ixx" &&
               (!SOURCE_UPDATE_FALLBACK_ENABLED ||
                   (PUBLIC_DISTRIBUTION
                       ? PROJECT_SOURCE_URL().ends_with(PROJECT_SOURCE_ARCHIVE_EXTENSION())
                       : AUTHORIZED_SOURCE_DISTRIBUTION_ENABLED && PROJECT_SOURCE_URL().empty())) &&
               !system_detail::prefer_source_release(
                   {.ok=true, .remote="0.90.177", .update_available=true},
                   {.ok=true, .remote="0.90.55", .update_available=true}) &&
               system_detail::prefer_source_release(
                   {.ok=true, .remote="0.90.181", .update_available=true},
                   {.ok=true, .remote="0.90.55", .update_available=false}) &&
               !system_detail::prefer_source_release(
                   {.ok=true, .remote="0.90.55", .update_available=true},
                   {.ok=true, .remote="0.90.55", .update_available=true}) &&
               !system_detail::prefer_source_release(
                   {.ok=false, .remote="0.90.177", .update_available=true},
                   {.ok=true, .remote="0.90.55", .update_available=true}) &&
               (PUBLIC_DISTRIBUTION
                   ? (!SITE_DISTRIBUTION && !AUTHORIZED_SOURCE_DISTRIBUTION_ENABLED
                       && PROJECT_SOURCE_VERSION_URL().starts_with("https://raw.githubusercontent.com/Autodidac/EpochEngine/")
                       && PROJECT_SOURCE_URL().starts_with("https://github.com/Autodidac/EpochEngine/"))
                   : (SITE_DISTRIBUTION && PROJECT_SOURCE_URL().empty()
                       && PROJECT_SOURCE_CHECKSUM_URL().empty()
                       && PROJECT_SOURCE_VERSION_URL() == std::string{EPOCH_SITE_BASE} + "/api/epoch/source-version")) &&
               PROJECT_SOURCE_CHANNEL_ALLOWED(PROJECT_SOURCE_URL(), PROJECT_SOURCE_VERSION_URL()) &&
               !PROJECT_SOURCE_CHANNEL_ALLOWED("https://example.com/source.zip", PROJECT_SOURCE_VERSION_URL()) &&
               !PROJECT_SOURCE_CHANNEL_ALLOWED(PROJECT_SOURCE_URL(), "https://example.com/version") &&
               !PROJECT_RELEASE_DOWNLOAD_ALLOWED(PUBLIC_DISTRIBUTION
                   ? "https://epoch.adamrushford.chatgpt.site/downloads/releases/wrong.zip"
                   : "https://github.com/Autodidac/EpochEngine/releases/download/v1/wrong.zip") &&
               PROJECT_SOURCE_EDITION_MATCHES(PUBLIC_DISTRIBUTION
                   ? "#define EPOCH_SOURCE_EDITION_PUBLIC 1\n" : "#define EPOCH_SOURCE_EDITION_PUBLIC 0\n") &&
               !PROJECT_SOURCE_EDITION_MATCHES(PUBLIC_DISTRIBUTION
                   ? "#define EPOCH_SOURCE_EDITION_PUBLIC 0\n" : "#define EPOCH_SOURCE_EDITION_PUBLIC 1\n") &&
               PROJECT_SOURCE_FALLBACK_URLS().empty() &&
               PROJECT_ACTION_RUNS_API_URL().starts_with(PUBLIC_DISTRIBUTION ? std::string{GITHUB_API_BASE}
                   : std::string{EPOCH_SITE_BASE}) &&
               PROJECT_RELEASE_DOWNLOAD_ALLOWED(PROJECT_RELEASE_DOWNLOAD_PREFIX() + "v1/runtime.zip");
    }

    std::filesystem::path source_update_log_path()
    {
        return system_detail::source_update_log_path_for(system_detail::current_binary_path());
    }

    std::filesystem::path update_handoff_log_path()
    {
        return system_detail::update_handoff_log_path_for(system_detail::current_binary_path());
    }

    bool source_update_worker_active()
    {
#if defined(_WIN32)
        try
        {
            return system_detail::source_update_session_active(system_detail::current_binary_path());
        }
        catch (const std::exception &e)
        {
            system_detail::log_error(std::string{"Source update liveness check failed: "} + e.what());
            return false;
        }
        catch (...)
        {
            system_detail::log_error("Source update liveness check failed with an unknown exception.");
            return false;
        }
#else
        return system_detail::in_process_source_update_active().load(std::memory_order_acquire)
            || system_detail::source_update_session_active(system_detail::current_binary_path());
#endif
    }

    int source_update_recent_cancel_seconds_remaining(const int cooldown_seconds)
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

    bool source_update_cancel_requested()
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

    bool launch_staged_update_handoff()
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

    bool request_source_update_cancel()
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

    bool run_source_update_command(
        const UpdateChannel& channel,
        const bool recheck_source_version,
        const bool silent_worker,
        const bool honor_env_silent)
    {
        if (!PROJECT_SOURCE_CHANNEL_ALLOWED(channel.source_url, channel.source_version_url))
        {
            system_detail::log_error("Source update refused a different edition or repository.");
            return false;
        }
        if (!SOURCE_UPDATE_FALLBACK_ENABLED)
        {
            system_detail::log_error("Source rebuild updates are disabled by this build's distribution policy.");
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

        PreparedSourceArchive prepared_source{};
        if (channel.source_url.empty())
        {
            prepared_source = acquire_private_source_archive(
                system_detail::source_work_root(target_binary));
            if (!prepared_source.ok)
            {
                system_detail::log_error(prepared_source.message);
                return false;
            }
            system_detail::log_info(
                "Authorized source snapshot " + prepared_source.source_version
                + " (" + prepared_source.commit + ") is ready for the Windows build worker.");
        }

        const bool effective_silent_worker =
            silent_worker || (honor_env_silent && system_detail::env_flag_enabled("EPOCH_UPDATER_SILENT"));
        const bool hide_worker_window =
            effective_silent_worker
            || !system_detail::env_flag_enabled("EPOCH_UPDATER_SHOW_WORKER_CONSOLE");

        if (!launch_source_update_worker(
                channel,
                target_binary,
                hide_worker_window,
                prepared_source))
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

        system_detail::ScopedLinuxSourceUpdateLock source_lock{
            system_detail::linux_source_update_lock_path()
        };
        if (!source_lock.acquired)
        {
            if (source_lock.already_locked)
            {
                system_detail::log_info(
                    "Source update request is already covered by another Epoch process.");
                return true;
            }

            system_detail::log_error(
                "Linux source updater lock could not be acquired: "
                + system_detail::linux_source_update_lock_path().string());
            return false;
        }

        PreparedSourceArchive prepared_source{};
        if (channel.source_url.empty())
        {
            prepared_source = acquire_private_source_archive(
                system_detail::source_work_root(target_binary));
            if (!prepared_source.ok)
            {
                system_detail::log_error(prepared_source.message);
                return false;
            }
            system_detail::log_info(
                "Authorized source snapshot " + prepared_source.source_version
                + " (" + prepared_source.commit + ") is ready for the Linux build worker.");
        }
        const auto cleanup_prepared_source = [&]()
            {
                if (!prepared_source.cleanup_root.empty())
                    system_detail::remove_update_cache_path_best_effort(prepared_source.cleanup_root);
            };

        const auto run_token = system_detail::make_source_update_run_token();
        const auto run_dir = system_detail::source_run_dir(target_binary, run_token);
        const std::string archive_identity = prepared_source.ok
            ? prepared_source.archive_path.string()
            : channel.source_url;
        const auto archive_path = system_detail::source_archive_path(
            target_binary,
            archive_identity,
            run_token);
        const auto staging_dir = system_detail::source_staging_dir(target_binary, run_token);
        const auto final_dir = system_detail::source_final_dir(target_binary, run_token);
        const auto active_marker_path = system_detail::source_active_run_path(target_binary);
        system_detail::ScopedSourceActiveMarkerCleanup active_marker{
            .marker_path = active_marker_path
        };

        std::error_code marker_ec;
        std::filesystem::create_directories(run_dir, marker_ec);
        if (marker_ec)
        {
            cleanup_prepared_source();
            system_detail::log_error("Failed to prepare Linux source update run directory: " + run_dir.string());
            return false;
        }
        std::filesystem::create_directories(active_marker_path.parent_path(), marker_ec);
        if (marker_ec
            || !system_detail::write_linux_source_active_marker(active_marker_path, run_dir))
        {
            cleanup_prepared_source();
            system_detail::log_error(
                "The Linux source updater status marker could not be written: "
                + active_marker_path.string());
            return false;
        }
        active_marker.owns_marker = true;

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
        system_detail::cleanup_stale_source_update_runs(target_binary, run_dir);

        if (prepared_source.ok)
        {
            std::filesystem::create_directories(archive_path.parent_path(), ec);
            ec.clear();
            std::filesystem::copy_file(
                prepared_source.archive_path,
                archive_path,
                std::filesystem::copy_options::overwrite_existing,
                ec);
            cleanup_prepared_source();
            if (ec)
            {
                system_detail::log_error(
                    "The Linux source updater could not claim the authorized source archive.");
                return false;
            }
            system_detail::log_info("Using process-authorized AES-GCM verified source snapshot.");
        }
        else
        {
            system_detail::log_info(
                "Downloading latest "
                + system_detail::describe_source_archive(channel.source_url)
                + ".");
            if (!download_source_archive_with_fallbacks(channel.source_url, archive_path))
                return false;
        }

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

    UpdateCommandResult run_update_command(
        const UpdateChannel& channel,
        const bool force,
        const bool honor_env_silent,
        const UpdateHandoffMode packaged_handoff_mode)
    {
        UpdateCommandResult result{};
        if (!PROJECT_UPDATE_CHANNEL_ALLOWED(channel.version_url, channel.binary_url,
                channel.source_url, channel.source_version_url, channel.platform_build_status_url))
        {
            result.operation_failed = true;
            result.status_message = "Update refused: this channel belongs to another edition or repository.";
            system_detail::log_error(result.status_message);
            return result;
        }
        const auto target_binary = system_detail::current_binary_path();

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
        result.packaged_release_checked = PROJECT_PACKAGED_UPDATES_ENABLED;

        if (!PROJECT_PACKAGED_UPDATES_ENABLED)
        {
            result.packaged_release_reason =
                "Development updates use your private source. Packaged downloads are available from the owner-only site.";
            system_detail::log_info(result.packaged_release_reason);
        }
        else if (packaged_release.found && !packaged_release.remote_version.empty())
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
        else if (packaged_release.discovery_ok)
        {
            packaged_status.ok = true;
            packaged_status.remote = packaged_release.latest_version;
            result.remote_version = packaged_release.latest_version;
            result.packaged_release_missing =
                !packaged_release.latest_version.empty()
                && system_detail::compare_versions(
                    local_packaged_version,
                    packaged_release.latest_version) < 0;
            result.packaged_release_reason = result.packaged_release_missing
                ? "No " + platform_key + " packaged runtime asset was found for newer release "
                    + packaged_release.latest_version + "."
                : "No newer compatible " + platform_key + " packaged runtime is available.";
            system_detail::log_info(result.packaged_release_reason);
        }
        else
        {
            result.packaged_release_reason =
                "Packaged release discovery or signature verification did not complete.";
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

        // Signed packaged artifacts have their own release-bound verification.
        // A newer source-head build must not veto an already verified binary.
        const bool candidate_update_available =
            system_detail::prefer_source_release(source_status, packaged_status);
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
            && !packaged_status.update_available;

        if (source_should_drive_update)
        {
            result.update_available = true;
            result.source_update_available = true;

            const std::string source = source_status.remote.empty()
                ? std::string{ "main source" }
                : std::string{ "main source " } + source_status.remote;
            if (!force)
            {
                result.force_required = true;
                result.status_message = "Source update available: " + source
                    + ". Update will build current main source locally.";
                return result;
            }

            result.source_fallback_attempted = true;
            system_detail::log_info(
                "No newer packaged runtime is selected. Starting the authorized source fallback.");
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
                packaged_release.checksum_url,
                {},
                packaged_handoff_mode,
                packaged_release.expected_sha256,
                packaged_release.remote_version);
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
                if (SOURCE_UPDATE_FALLBACK_ENABLED && private_source_access_enabled())
                {
                    result.source_fallback_attempted = true;
                    system_detail::log_info(
                        "Packaged update failed verification or staging. Starting the authorized source fallback.");
                    const bool worker_launched = run_source_update_command(
                        channel,
                        false,
                        false,
                        honor_env_silent);
                    result.source_update_performed = worker_launched;
                    result.status_message = worker_launched
                        ? "Packaged update failed safely; the authorized source rebuild worker started as backup."
                        : "Packaged update failed safely, and the authorized source fallback did not start.";
                }
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

        if (result.packaged_release_missing
            && SOURCE_UPDATE_FALLBACK_ENABLED
            && private_source_access_enabled())
        {
            result.update_available = true;
            result.source_update_available = true;
            if (!force)
            {
                result.force_required = true;
                result.status_message =
                    "No compatible packaged runtime is available for " + platform_key
                    + ". Authorized source build is available as the backup lane.";
                return result;
            }

            result.source_fallback_attempted = true;
            system_detail::log_info(
                "No compatible packaged runtime is available. Starting the authorized source fallback.");
            const bool worker_launched = run_source_update_command(
                channel,
                false,
                false,
                honor_env_silent);
            result.source_update_performed = worker_launched;
            result.status_message = worker_launched
                ? "Authorized source rebuild worker started because no compatible packaged runtime was available."
                : "No compatible packaged runtime was available, and the authorized source fallback did not start.";
            return result;
        }

        const bool packaged_discovery_complete =
            !PROJECT_PACKAGED_UPDATES_ENABLED || packaged_status.ok;
        const bool source_discovery_complete =
            channel.source_version_url.empty() || source_status.ok;
        if (!packaged_discovery_complete || !source_discovery_complete)
        {
            result.operation_failed = true;
            result.status_message = "Update check incomplete.";
            if (!packaged_discovery_complete)
            {
                result.status_message += " " + (result.packaged_release_reason.empty()
                    ? std::string{ "Packaged release discovery could not be verified." }
                    : result.packaged_release_reason);
            }
            if (!source_discovery_complete)
                result.status_message += " Source update availability could not be verified.";
            result.status_message += " The installed runtime was not changed; retry the check.";
        }
        else if (!result.packaged_release_reason.empty())
        {
            result.status_message = result.packaged_release_reason;
        }
        else
        {
            result.status_message = "Epoch packaged runtime and main source are already current.";
        }

        return result;
    }
}
