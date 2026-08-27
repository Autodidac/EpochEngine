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
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <source_location>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <process.h>
#include <errno.h>
#else
#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

export module scripting.compiler;

import core.logger;
import core.path;
import platform.filesystem;
import platform.child_process;

namespace epochengine::compiler
{
    export inline constexpr std::uint64_t kDefaultMaximumScriptBytes = 8ull * 1024ull * 1024ull;
    export inline constexpr std::uint64_t kDefaultMaximumArtifactBytes = 256ull * 1024ull * 1024ull;

    export enum class CompileStatus : std::uint8_t
    {
        succeeded,
        refused,
        cancelled,
        busy,
        compiler_failed,
        verification_failed,
        publish_failed
    };

    export enum class CompileRefusal : std::uint8_t
    {
        none,
        cancelled,
        active_request,
        malformed_request,
        missing_workspace,
        missing_source,
        source_not_regular,
        source_too_large,
        invalid_source_name,
        invalid_output_name,
        invalid_output_parent,
        path_traversal,
        path_outside_workspace,
        source_output_alias,
        compiler_unavailable,
        temporary_path_occupied,
        source_evidence_mismatch,
        source_changed,
        output_changed,
        candidate_missing,
        candidate_invalid,
        atomic_replace_failed,
        published_evidence_mismatch,
        filesystem_error
    };

    export [[nodiscard]] constexpr std::string_view compile_status_name(
        CompileStatus status) noexcept
    {
        switch (status)
        {
        case CompileStatus::succeeded: return "succeeded";
        case CompileStatus::refused: return "refused";
        case CompileStatus::cancelled: return "cancelled";
        case CompileStatus::busy: return "busy";
        case CompileStatus::compiler_failed: return "compiler_failed";
        case CompileStatus::verification_failed: return "verification_failed";
        case CompileStatus::publish_failed: return "publish_failed";
        }
        return "unknown";
    }

    export [[nodiscard]] constexpr std::string_view compile_refusal_name(
        CompileRefusal refusal) noexcept
    {
        switch (refusal)
        {
        case CompileRefusal::none: return "none";
        case CompileRefusal::cancelled: return "cancelled";
        case CompileRefusal::active_request: return "active_request";
        case CompileRefusal::malformed_request: return "malformed_request";
        case CompileRefusal::missing_workspace: return "missing_workspace";
        case CompileRefusal::missing_source: return "missing_source";
        case CompileRefusal::source_not_regular: return "source_not_regular";
        case CompileRefusal::source_too_large: return "source_too_large";
        case CompileRefusal::invalid_source_name: return "invalid_source_name";
        case CompileRefusal::invalid_output_name: return "invalid_output_name";
        case CompileRefusal::invalid_output_parent: return "invalid_output_parent";
        case CompileRefusal::path_traversal: return "path_traversal";
        case CompileRefusal::path_outside_workspace: return "path_outside_workspace";
        case CompileRefusal::source_output_alias: return "source_output_alias";
        case CompileRefusal::compiler_unavailable: return "compiler_unavailable";
        case CompileRefusal::temporary_path_occupied: return "temporary_path_occupied";
        case CompileRefusal::source_evidence_mismatch: return "source_evidence_mismatch";
        case CompileRefusal::source_changed: return "source_changed";
        case CompileRefusal::output_changed: return "output_changed";
        case CompileRefusal::candidate_missing: return "candidate_missing";
        case CompileRefusal::candidate_invalid: return "candidate_invalid";
        case CompileRefusal::atomic_replace_failed: return "atomic_replace_failed";
        case CompileRefusal::published_evidence_mismatch: return "published_evidence_mismatch";
        case CompileRefusal::filesystem_error: return "filesystem_error";
        }
        return "unknown";
    }

    export struct ContentDigest final
    {
        std::array<std::uint64_t, 4> words{};

        [[nodiscard]] friend constexpr bool operator==(
            const ContentDigest& left,
            const ContentDigest& right) noexcept
        {
            for (std::size_t index = 0; index < left.words.size(); ++index)
            {
                if (left.words[index] != right.words[index])
                    return false;
            }
            return true;
        }
    };

    export struct FileEvidence final
    {
        bool exists{};
        bool regular_file{};
        std::uint64_t size_bytes{};
        std::int64_t write_stamp{};
        ContentDigest content{};

        [[nodiscard]] friend constexpr bool operator==(
            const FileEvidence& left,
            const FileEvidence& right) noexcept
        {
            return left.exists == right.exists
                && left.regular_file == right.regular_file
                && left.size_bytes == right.size_bytes
                && left.write_stamp == right.write_stamp
                && left.content == right.content;
        }
    };

    export enum class FileProbeStatus : std::uint8_t
    {
        available,
        missing,
        not_regular,
        too_large,
        read_failed
    };

    export struct FileProbeResult final
    {
        FileProbeStatus status{FileProbeStatus::missing};
        FileEvidence evidence{};
        std::string message{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == FileProbeStatus::available;
        }
    };

    export struct CompileRequest final
    {
        std::filesystem::path source_path{};
        std::filesystem::path output_path{};
        std::filesystem::path workspace_root{};
        std::filesystem::path compiler_path{};
        std::uint64_t request_generation{};
        std::uint64_t project_generation{};
        std::uint64_t maximum_source_bytes{kDefaultMaximumScriptBytes};
        std::uint64_t maximum_artifact_bytes{kDefaultMaximumArtifactBytes};
        bool require_selected_script_name{true};
        bool require_expected_source{};
        FileEvidence expected_source{};
        std::stop_token cancellation{};
    };

    export struct CompilePlan final
    {
        std::uint64_t request_generation{};
        std::uint64_t project_generation{};
        std::filesystem::path workspace_root{};
        std::filesystem::path source_path{};
        std::filesystem::path output_path{};
        std::filesystem::path temporary_output_path{};
        std::filesystem::path compiler_path{};
        std::vector<std::filesystem::path> include_roots{};
        std::vector<std::filesystem::path> library_roots{};
        std::string command_description{};
        FileEvidence source_before{};
        FileEvidence output_before{};
        std::uint64_t maximum_source_bytes{};
        std::uint64_t maximum_artifact_bytes{};
    };

    export struct CompilePlanningResult final
    {
        CompileRefusal refusal{CompileRefusal::malformed_request};
        CompilePlan plan{};
        std::string message{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return refusal == CompileRefusal::none;
        }
    };

    export struct CompileEvidence final
    {
        std::uint64_t request_generation{};
        std::uint64_t project_generation{};
        std::filesystem::path source_path{};
        std::filesystem::path output_path{};
        std::filesystem::path temporary_output_path{};
        std::string command_description{};
        FileEvidence source_before{};
        FileEvidence source_after{};
        FileEvidence output_before{};
        FileEvidence candidate{};
        FileEvidence published{};
        bool compiler_started{};
        bool scope_revalidated{};
        bool source_revalidated{};
        bool output_ownership_revalidated{};
        bool candidate_verified{};
        bool publication_scope_revalidated{};
        bool atomic_replace_committed{};
        bool published_artifact_verified{};
    };

    export struct CompileResult final
    {
        CompileStatus status{CompileStatus::refused};
        CompileRefusal refusal{CompileRefusal::malformed_request};
        std::string message{};
        CompileEvidence evidence{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == CompileStatus::succeeded;
        }
    };

    namespace detail
    {
        class DigestAccumulator final
        {
        public:
            void update(std::span<const std::byte> bytes) noexcept
            {
                static constexpr std::array<std::uint64_t, 4> primes{
                    1099511628211ull,
                    14029467366897019727ull,
                    1609587929392839161ull,
                    9650029242287828579ull};
                for (const std::byte value : bytes)
                {
                    const auto octet = static_cast<std::uint64_t>(value);
                    for (std::size_t lane = 0; lane < state_.size(); ++lane)
                    {
                        state_[lane] ^= octet + (lane * 0x9e3779b97f4a7c15ull);
                        state_[lane] *= primes[lane];
                        const unsigned shift = static_cast<unsigned>(11u + lane * 7u);
                        state_[lane] = (state_[lane] << shift)
                            | (state_[lane] >> (64u - shift));
                    }
                }
            }

            [[nodiscard]] ContentDigest finish(std::uint64_t size) const noexcept
            {
                ContentDigest digest{state_};
                for (std::size_t lane = 0; lane < digest.words.size(); ++lane)
                {
                    digest.words[lane] ^= size + lane * 0x517cc1b727220a95ull;
                    digest.words[lane] ^= digest.words[lane] >> 33u;
                    digest.words[lane] *= 0xff51afd7ed558ccdull;
                    digest.words[lane] ^= digest.words[lane] >> 33u;
                }
                return digest;
            }

        private:
            std::array<std::uint64_t, 4> state_{
                1469598103934665603ull,
                0x6a09e667f3bcc909ull,
                0xbb67ae8584caa73bull,
                0x3c6ef372fe94f82bull};
        };

        [[nodiscard]] inline bool contains_parent_reference(
            const std::filesystem::path& path) noexcept
        {
            for (const auto& component : path)
            {
                if (component == "..")
                    return true;
            }
            return false;
        }

        [[nodiscard]] inline bool path_is_within(
            const std::filesystem::path& root,
            const std::filesystem::path& candidate)
        {
            const auto relative = candidate.lexically_relative(root);
            if (relative.empty() || relative.is_absolute())
                return false;
            const auto first = relative.begin();
            return first == relative.end() || *first != "..";
        }

        [[nodiscard]] inline bool selected_script_name(
            const std::filesystem::path& path)
        {
            return std::string_view{path.filename().string()}.ends_with(".ascript.cpp");
        }

        [[nodiscard]] inline bool valid_output_name(
            const std::filesystem::path& path)
        {
#ifdef _WIN32
            return path.extension() == ".dll";
#else
            return path.extension() == ".so";
#endif
        }

        [[nodiscard]] inline std::filesystem::path temporary_output_path(
            const std::filesystem::path& output,
            std::uint64_t generation)
        {
            return output.parent_path()
                / (output.stem().string() + ".epoch_tmp_"
                    + std::to_string(generation) + output.extension().string());
        }

        [[nodiscard]] inline std::array<std::filesystem::path, 5>
        temporary_artifact_paths(const std::filesystem::path& output)
        {
            return {
                output,
                std::filesystem::path{output.string() + ".obj"},
                std::filesystem::path{output.string() + ".pdb"},
                std::filesystem::path{output.string() + ".lib"},
                std::filesystem::path{output.string() + ".exp"}};
        }

        enum class PathAvailability : std::uint8_t
        {
            missing,
            occupied,
            inspection_failed
        };

        [[nodiscard]] inline PathAvailability path_availability(
            const std::filesystem::path& path) noexcept
        {
            std::error_code error{};
            const auto status = std::filesystem::symlink_status(path, error);
            if (error == std::errc::no_such_file_or_directory)
                return PathAvailability::missing;
            if (error)
                return PathAvailability::inspection_failed;
            return std::filesystem::exists(status)
                ? PathAvailability::occupied
                : PathAvailability::missing;
        }

        [[nodiscard]] inline bool plan_paths_still_scoped(
            const CompilePlan& plan)
        {
            std::error_code error{};
            const auto root = std::filesystem::weakly_canonical(
                plan.workspace_root, error);
            if (error || root != plan.workspace_root
                || !std::filesystem::is_directory(root, error) || error)
            {
                return false;
            }

            const auto source = std::filesystem::weakly_canonical(
                plan.source_path, error);
            if (error || source != plan.source_path
                || !path_is_within(root, source))
            {
                return false;
            }

            const auto outputParent = std::filesystem::weakly_canonical(
                plan.output_path.parent_path(), error);
            if (error || outputParent != plan.output_path.parent_path()
                || !std::filesystem::is_directory(outputParent, error) || error
                || !path_is_within(root, plan.output_path))
            {
                return false;
            }

            const auto temporaryParent = std::filesystem::weakly_canonical(
                plan.temporary_output_path.parent_path(), error);
            return !error
                && temporaryParent == plan.temporary_output_path.parent_path()
                && std::filesystem::is_directory(temporaryParent, error)
                && !error
                && path_is_within(root, plan.temporary_output_path);
        }

        inline std::atomic_flag active_compile = ATOMIC_FLAG_INIT;
        inline std::atomic<std::uint64_t> compatibility_generation{0};

        class ActiveCompileGuard final
        {
        public:
            ActiveCompileGuard() noexcept
                : acquired_{!active_compile.test_and_set(std::memory_order_acquire)}
            {
            }

            ~ActiveCompileGuard()
            {
                if (acquired_)
                    active_compile.clear(std::memory_order_release);
            }

            [[nodiscard]] bool acquired() const noexcept { return acquired_; }

        private:
            bool acquired_{};
        };

        inline void remove_regular_file(const std::filesystem::path& path) noexcept
        {
            std::error_code error{};
            const auto status = std::filesystem::symlink_status(path, error);
            if (!error && std::filesystem::is_regular_file(status))
                std::filesystem::remove(path, error);
        }

        inline void cleanup_temporary_artifacts(
            const std::filesystem::path& output) noexcept
        {
            remove_regular_file(output);
            remove_regular_file(std::filesystem::path{output.string() + ".obj"});
            remove_regular_file(std::filesystem::path{output.string() + ".pdb"});
            remove_regular_file(std::filesystem::path{output.string() + ".lib"});
            remove_regular_file(std::filesystem::path{output.string() + ".exp"});
        }


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

        [[nodiscard]] inline std::filesystem::path latest_version_directory(
            const std::filesystem::path& root)
        {
            std::error_code error{};
            std::filesystem::path latest{};
            for (std::filesystem::directory_iterator iterator{root, error}, end;
                !error && iterator != end;
                iterator.increment(error))
            {
                if (!iterator->is_directory(error))
                {
                    error.clear();
                    continue;
                }
                const auto candidate = iterator->path();
                if (latest.empty()
                    || candidate.filename().string()
                        > latest.filename().string())
                {
                    latest = candidate;
                }
            }
            return latest;
        }

        [[nodiscard]] inline std::filesystem::path msvc_tool_root(
            const std::filesystem::path& compilerPath)
        {
            std::filesystem::path root = compilerPath;
            for (int depth = 0; depth < 4 && !root.empty(); ++depth)
                root = root.parent_path();
            return root;
        }

        [[nodiscard]] inline std::filesystem::path windows_sdk_root()
        {
            return "C:/Program Files (x86)/Windows Kits/10";
        }

        [[nodiscard]] inline std::filesystem::path resolve_clangxx()
        {
            const std::vector<std::filesystem::path> clangCandidates{
                "C:/Program Files/LLVM/bin/clang++.exe",
                "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang++.exe",
                "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin/clang++.exe"};
            std::error_code error{};
            for (const auto& candidate : clangCandidates)
            {
                if (std::filesystem::is_regular_file(candidate, error) && !error)
                    return candidate;
                error.clear();
            }

#ifdef _WIN32
            const auto msvcVersion = latest_version_directory(
                "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC");
            const auto msvcCompiler =
                msvcVersion / "bin" / "Hostx64" / "x64" / "cl.exe";
            if (std::filesystem::is_regular_file(msvcCompiler, error) && !error)
                return msvcCompiler;
#endif
            return std::filesystem::path{"clang++"};
        }

        [[nodiscard]] inline std::filesystem::path resolve_executable(
            const std::filesystem::path& selected) noexcept
        {
            const auto acceptable = [](const std::filesystem::path& candidate)
                -> std::filesystem::path
            {
                std::error_code error{};
                const auto absolute = std::filesystem::absolute(candidate, error);
                if (error)
                    return {};
                const auto canonical = std::filesystem::weakly_canonical(
                    absolute, error);
                if (error || !std::filesystem::is_regular_file(canonical, error)
                    || error)
                {
                    return {};
                }
#ifndef _WIN32
                if (::access(canonical.c_str(), X_OK) != 0)
                    return {};
#endif
                return canonical;
            };

            if (selected.empty())
                return {};
            if (selected.has_parent_path() || selected.is_absolute())
                return acceptable(selected);

            std::string environmentValue{};
#ifdef _WIN32
            char* environment{};
            std::size_t environmentSize{};
            if (_dupenv_s(
                    &environment, &environmentSize, "PATH") != 0
                || environment == nullptr)
            {
                return {};
            }
            environmentValue.assign(environment);
            std::free(environment);
#else
            const char* environment = std::getenv("PATH");
            if (environment == nullptr)
                return {};
            environmentValue.assign(environment);
#endif
            std::string_view remaining{environmentValue};
#ifdef _WIN32
            constexpr char delimiter = ';';
#else
            constexpr char delimiter = ':';
#endif
            while (true)
            {
                const std::size_t separator = remaining.find(delimiter);
                std::string_view component = remaining.substr(0u, separator);
                if (component.size() >= 2u && component.front() == '"'
                    && component.back() == '"')
                {
                    component.remove_prefix(1u);
                    component.remove_suffix(1u);
                }
                if (!component.empty())
                {
                    std::filesystem::path directory{std::string(component)};
                    if (directory.is_absolute())
                    {
                        std::filesystem::path candidate = directory / selected;
#ifdef _WIN32
                        if (candidate.extension().empty())
                            candidate += ".exe";
#endif
                        if (auto resolved = acceptable(candidate); !resolved.empty())
                            return resolved;
                    }
                }
                if (separator == std::string_view::npos)
                    break;
                remaining.remove_prefix(separator + 1u);
            }
            return {};
        }

        [[nodiscard]] inline bool compiler_uses_msvc_style(
            const std::filesystem::path& compilerPath)
        {
            std::string filename = compilerPath.filename().string();
            std::transform(filename.begin(), filename.end(), filename.begin(),
                [](unsigned char value)
                {
                    return static_cast<char>(std::tolower(value));
                });
            return filename == "cl.exe" || filename == "clang-cl.exe";
        }
        [[nodiscard]] inline std::filesystem::path resolve_engine_include_root(
            const std::filesystem::path& input)
        {
            std::error_code ec;
            std::vector<std::filesystem::path> candidates{};
            const auto push_candidate = [&](const std::filesystem::path& candidate) {
                if (candidate.empty())
                    return;

                const auto absolute = std::filesystem::absolute(candidate, ec).lexically_normal();
                if (!std::filesystem::exists(absolute, ec))
                    return;

                if (std::find(candidates.begin(), candidates.end(), absolute) == candidates.end())
                    candidates.push_back(absolute);
            };

            push_candidate(input.parent_path());
            push_candidate(input.parent_path().parent_path() / "include");
            push_candidate(input.parent_path().parent_path() / "source");

            if (const auto repoRoot = epochengine::core::path::find_epoch_repo_root(input); !repoRoot.empty())
            {
                push_candidate(repoRoot / "Engine" / "include");
                push_candidate(repoRoot / "Engine");
            }

            push_candidate(epochengine::core::path::engine_include_dir());
            if (const auto runtimeRoot = epochengine::core::path::runtime_root_dir(); !runtimeRoot.empty())
            {
                push_candidate(runtimeRoot / "Engine");
                push_candidate(runtimeRoot / "include");
            }

            for (const auto& candidate : candidates)
            {
                if (std::filesystem::exists(candidate, ec))
                    return candidate;
            }

            return epochengine::core::path::normalize(input.parent_path().parent_path().parent_path() / "include");
        }

        [[nodiscard]] inline std::vector<std::filesystem::path> resolve_include_roots(
            const std::filesystem::path& input,
            const std::filesystem::path& compilerPath)
        {
            std::error_code error{};
            std::vector<std::filesystem::path> roots{};
            const auto append = [&](const std::filesystem::path& candidate) {
                if (candidate.empty())
                    return;
                const auto normalized = std::filesystem::absolute(
                    candidate, error).lexically_normal();
                if (error || !std::filesystem::is_directory(normalized, error))
                {
                    error.clear();
                    return;
                }
                if (std::find(roots.begin(), roots.end(), normalized)
                    == roots.end())
                {
                    roots.push_back(normalized);
                }
            };

            append(input.parent_path());
            append(input.parent_path().parent_path() / "include");
            append(input.parent_path().parent_path() / "source");
            append(resolve_engine_include_root(input));

#ifdef _WIN32
            if (compiler_uses_msvc_style(compilerPath))
            {
                const auto tools = msvc_tool_root(compilerPath);
                append(tools / "include");
                append(tools / "atlmfc" / "include");
                const auto sdkInclude = latest_version_directory(
                    windows_sdk_root() / "Include");
                append(sdkInclude / "ucrt");
                append(sdkInclude / "shared");
                append(sdkInclude / "um");
                append(sdkInclude / "winrt");
                append(sdkInclude / "cppwinrt");
            }
#endif
            return roots;
        }

        [[nodiscard]] inline std::vector<std::filesystem::path> resolve_library_roots(
            const std::filesystem::path& compilerPath)
        {
            std::vector<std::filesystem::path> roots{};
#ifdef _WIN32
            if (compiler_uses_msvc_style(compilerPath))
            {
                std::error_code error{};
                const auto append = [&](const std::filesystem::path& candidate) {
                    if (std::filesystem::is_directory(candidate, error) && !error)
                        roots.push_back(candidate.lexically_normal());
                    error.clear();
                };
                const auto tools = msvc_tool_root(compilerPath);
                append(tools / "lib" / "x64");
                append(tools / "atlmfc" / "lib" / "x64");
                const auto sdkLib = latest_version_directory(
                    windows_sdk_root() / "Lib");
                append(sdkLib / "ucrt" / "x64");
                append(sdkLib / "um" / "x64");
            }
#else
            (void)compilerPath;
#endif
            return roots;
        }
        [[nodiscard]] inline std::string describe_command(
            const std::filesystem::path& compilerPath,
            const std::filesystem::path& input,
            const std::filesystem::path& output,
            std::span<const std::filesystem::path> includeRoots,
            std::span<const std::filesystem::path> libraryRoots)
        {
            std::vector<std::string> arguments{};
            if (compiler_uses_msvc_style(compilerPath))
            {
                arguments = {
                    quote_arg(compilerPath.string()),
                    "/nologo",
                    "/std:c++latest",
                    "/LD",
                    "/MD",
                    "/GR-",
                    "/O2",
                    quote_arg(input.string()),
                    "/Fe:" + quote_arg(output.string()),
                    "/Fo:" + quote_arg(output.string() + ".obj"),
                    "/Fd:" + quote_arg(output.string() + ".pdb")};
                for (const auto& root : includeRoots)
                    arguments.push_back("/I" + quote_arg(root.string()));
                arguments.push_back("/link");
                arguments.push_back("/INCREMENTAL:NO");
                arguments.push_back(
                    "/IMPLIB:" + quote_arg(output.string() + ".lib"));
                for (const auto& root : libraryRoots)
                    arguments.push_back("/LIBPATH:" + quote_arg(root.string()));
            }
            else
            {
                arguments = {
                    quote_arg(compilerPath.string()),
                    "-std=c++23",
                    "-shared",
                    quote_arg(input.string()),
                    "-o", quote_arg(output.string()),
                    "-fno-rtti",
                    "-fno-exceptions",
                    "-O2"};
                for (const auto& root : includeRoots)
                    arguments.push_back("-I" + quote_arg(root.string()));
            }

            std::string command{};
            for (const auto& argument : arguments)
            {
                if (!command.empty())
                    command.push_back(' ');
                command += argument;
            }
            return command;
        }
        struct SpawnResult final
        {
            bool launched{};
            bool succeeded{};
            bool cancelled{};
            std::int32_t exit_code{};
            bool exit_code_valid{};
        };

        [[nodiscard]] inline SpawnResult spawn_compiler(
            const std::filesystem::path& compilerPath,
            const std::filesystem::path& input,
            const std::filesystem::path& output,
            std::span<const std::filesystem::path> includeRoots,
            std::span<const std::filesystem::path> libraryRoots,
            std::stop_token cancellation)
        {
            std::vector<std::string> arguments{};
            if (compiler_uses_msvc_style(compilerPath))
            {
                arguments = {
                    "/nologo",
                    "/std:c++latest",
                    "/LD",
                    "/MD",
                    "/GR-",
                    "/O2",
                    input.string(),
                    "/Fe:" + output.string(),
                    "/Fo:" + output.string() + ".obj",
                    "/Fd:" + output.string() + ".pdb"};
                for (const auto& root : includeRoots)
                    arguments.push_back("/I" + root.string());
                arguments.push_back("/link");
                arguments.push_back("/INCREMENTAL:NO");
                arguments.push_back("/IMPLIB:" + output.string() + ".lib");
                for (const auto& root : libraryRoots)
                    arguments.push_back("/LIBPATH:" + root.string());
            }
            else
            {
                arguments = {
                    "-std=c++23",
                    "-shared",
                    input.string(),
                    "-o",
                    output.string(),
                    "-fno-rtti",
                    "-fno-exceptions",
                    "-O2"};
                for (const auto& root : includeRoots)
                {
                    arguments.emplace_back("-I");
                    arguments.push_back(root.string());
                }
            }

            platform::child_process::LaunchRequest request{};
            request.executable = compilerPath;
            request.working_directory = output.parent_path();
            request.arguments = std::move(arguments);
            request.correlation_key = "epoch.script.compile:"
                + output.filename().string();
            request.exclusive_group = "epoch.script.compile";
            request.display_name = "Epoch C++23 Script Compiler";
            request.window_mode = platform::child_process::WindowMode::hidden;
            const platform::child_process::LaunchResult launched =
                platform::child_process::launch_or_focus(request);
            if (launched.code != platform::child_process::LaunchCode::started)
            {
                logger::errorf_loc(
                    "Compiler",
                    std::source_location::current(),
                    "failed to launch script compiler: {} ({})",
                    compilerPath.string(),
                    launched.message);
                return {};
            }

            const platform::child_process::WaitResult waited =
                platform::child_process::wait(launched.handle, cancellation);
            (void)platform::child_process::release(launched.handle);
            if (waited.code == platform::child_process::WaitCode::cancelled)
            {
                return {
                    .launched = true,
                    .cancelled = true,
                    .exit_code = waited.process && waited.process->exit_code_valid
                        ? waited.process->exit_code
                        : 0,
                    .exit_code_valid = waited.process
                        && waited.process->exit_code_valid};
            }
            if (waited.code != platform::child_process::WaitCode::exited
                || !waited.process || !waited.process->exit_code_valid)
            {
                logger::errorf_loc(
                    "Compiler",
                    std::source_location::current(),
                    "script compiler process failed while waiting: {}",
                    waited.message);
                return {.launched = true};
            }

            const bool succeeded = waited.process->exit_code == 0;
            if (!succeeded)
            {
                logger::errorf_loc(
                    "Compiler",
                    std::source_location::current(),
                    "script compiler failed with code: {}",
                    waited.process->exit_code);
            }
            return {
                .launched = true,
                .succeeded = succeeded,
                .exit_code = waited.process->exit_code,
                .exit_code_valid = true};
        }
    }

    export [[nodiscard]] FileProbeResult inspect_file(
        const std::filesystem::path& path,
        std::uint64_t maximumBytes) noexcept
    {
        try
        {
            std::error_code error{};
            const auto status = std::filesystem::symlink_status(path, error);
            if (error == std::errc::no_such_file_or_directory)
                return {FileProbeStatus::missing, {}, "file does not exist"};
            if (error)
                return {FileProbeStatus::read_failed, {}, error.message()};
            if (!std::filesystem::exists(status))
                return {FileProbeStatus::missing, {}, "file does not exist"};
            if (!std::filesystem::is_regular_file(status))
            {
                return {
                    FileProbeStatus::not_regular,
                    {.exists = true},
                    "path is not a regular file"};
            }

            const std::uintmax_t nativeSize = std::filesystem::file_size(path, error);
            if (error)
                return {FileProbeStatus::read_failed, {}, error.message()};
            if (nativeSize > maximumBytes
                || nativeSize > (std::numeric_limits<std::uint64_t>::max)())
            {
                return {
                    FileProbeStatus::too_large,
                    {.exists = true, .regular_file = true},
                    "file exceeds the configured byte limit"};
            }

            std::ifstream input(path, std::ios::binary);
            if (!input)
                return {FileProbeStatus::read_failed, {}, "file could not be opened"};
            detail::DigestAccumulator digest{};
            std::array<char, 64u * 1024u> buffer{};
            std::uint64_t total{};
            while (input)
            {
                input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                const std::streamsize count = input.gcount();
                if (count > 0)
                {
                    const auto bytesRead = static_cast<std::uint64_t>(count);
                    if (bytesRead > maximumBytes - total)
                    {
                        return {
                            FileProbeStatus::too_large,
                            {.exists = true, .regular_file = true},
                            "file grew beyond the configured byte limit"};
                    }
                    total += bytesRead;
                    digest.update(std::as_bytes(std::span{
                        buffer.data(), static_cast<std::size_t>(count)}));
                }
            }
            if (!input.eof())
                return {FileProbeStatus::read_failed, {}, "file read failed"};
            if (total != static_cast<std::uint64_t>(nativeSize))
                return {FileProbeStatus::read_failed, {}, "file changed while being read"};

            const auto written = std::filesystem::last_write_time(path, error);
            if (error)
                return {FileProbeStatus::read_failed, {}, error.message()};
            return {
                FileProbeStatus::available,
                {
                    .exists = true,
                    .regular_file = true,
                    .size_bytes = total,
                    .write_stamp = static_cast<std::int64_t>(
                        written.time_since_epoch().count()),
                    .content = digest.finish(total)},
                {}};
        }
        catch (const std::exception& exception)
        {
            return {FileProbeStatus::read_failed, {}, exception.what()};
        }
        catch (...)
        {
            return {FileProbeStatus::read_failed, {}, "unknown filesystem failure"};
        }
    }

    export [[nodiscard]] std::string content_digest_hex(
        const ContentDigest& digest)
    {
        constexpr char digits[] = "0123456789abcdef";
        std::string result(64u, '0');
        for (std::size_t word = 0; word < digest.words.size(); ++word)
        {
            for (std::size_t byte = 0; byte < 8u; ++byte)
            {
                const unsigned shift = static_cast<unsigned>(56u - byte * 8u);
                const auto value = static_cast<unsigned char>(
                    (digest.words[word] >> shift) & 0xffu);
                const std::size_t offset = word * 16u + byte * 2u;
                result[offset] = digits[(value >> 4u) & 0x0fu];
                result[offset + 1u] = digits[value & 0x0fu];
            }
        }
        return result;
    }
    export [[nodiscard]] std::filesystem::path default_script_output_path(
        const std::filesystem::path& input)
    {
        const std::string filename = input.filename().string();
        constexpr std::string_view suffix = ".ascript.cpp";
        const std::string stem = std::string_view{filename}.ends_with(suffix)
            ? filename.substr(0u, filename.size() - suffix.size())
            : input.stem().string();
#ifdef _WIN32
        return input.parent_path() / (stem + ".dll");
#else
        return input.parent_path() / ("lib" + stem + ".so");
#endif
    }
    export [[nodiscard]] CompilePlanningResult plan_script_compile(
        const CompileRequest& request)
    {
        try
        {
            if (request.cancellation.stop_requested())
            {
                return {
                    CompileRefusal::cancelled,
                    {},
                    "script compilation was cancelled before preflight"};
            }
            if (request.request_generation == 0u
                || request.project_generation == 0u
                || request.source_path.empty()
                || request.output_path.empty()
                || request.maximum_source_bytes == 0u
                || request.maximum_artifact_bytes == 0u)
            {
                return {
                    CompileRefusal::malformed_request,
                    {},
                    "nonzero generations, source, output, and byte bounds are required"};
            }
            if (request.workspace_root.empty())
            {
                return {
                    CompileRefusal::missing_workspace,
                    {},
                    "an explicit script workspace root is required"};
            }
            if (detail::contains_parent_reference(request.workspace_root)
                || detail::contains_parent_reference(request.source_path)
                || detail::contains_parent_reference(request.output_path))
            {
                return {
                    CompileRefusal::path_traversal,
                    {},
                    "parent traversal is not accepted in script paths"};
            }
            if (request.require_selected_script_name
                && !detail::selected_script_name(request.source_path))
            {
                return {
                    CompileRefusal::invalid_source_name,
                    {},
                    "selected script source must end with .ascript.cpp"};
            }
            if (!detail::valid_output_name(request.output_path))
            {
                return {
                    CompileRefusal::invalid_output_name,
                    {},
                    "script output must use the platform shared-library extension"};
            }

            std::error_code error{};
            const auto rootAbsolute = std::filesystem::absolute(
                request.workspace_root, error);
            const auto root = std::filesystem::weakly_canonical(rootAbsolute, error);
            if (error || !std::filesystem::is_directory(root, error))
            {
                return {
                    CompileRefusal::missing_workspace,
                    {},
                    "script workspace does not resolve to a directory"};
            }
            const auto sourceAbsolute = std::filesystem::absolute(
                request.source_path, error);
            const auto source = std::filesystem::weakly_canonical(
                sourceAbsolute, error);
            if (error)
            {
                return {
                    CompileRefusal::missing_source,
                    {},
                    "script source could not be resolved"};
            }
            const auto outputAbsolute = std::filesystem::absolute(
                request.output_path, error);
            const auto outputParent = std::filesystem::weakly_canonical(
                outputAbsolute.parent_path(), error);
            if (error || !std::filesystem::is_directory(outputParent, error))
            {
                return {
                    CompileRefusal::invalid_output_parent,
                    {},
                    "script output parent does not resolve to a directory"};
            }
            const auto output = (
                outputParent / outputAbsolute.filename()).lexically_normal();
            if (!detail::path_is_within(root, source)
                || !detail::path_is_within(root, output))
            {
                return {
                    CompileRefusal::path_outside_workspace,
                    {},
                    "script source and output must remain inside the workspace"};
            }
            if (source == output)
            {
                return {
                    CompileRefusal::source_output_alias,
                    {},
                    "script source and output resolve to the same path"};
            }

            const FileProbeResult sourceProbe = inspect_file(
                source, request.maximum_source_bytes);
            if (!sourceProbe)
            {
                const CompileRefusal refusal = sourceProbe.status
                        == FileProbeStatus::missing
                    ? CompileRefusal::missing_source
                    : sourceProbe.status == FileProbeStatus::not_regular
                        ? CompileRefusal::source_not_regular
                        : sourceProbe.status == FileProbeStatus::too_large
                            ? CompileRefusal::source_too_large
                            : CompileRefusal::filesystem_error;
                return {refusal, {}, sourceProbe.message};
            }
            if (request.require_expected_source
                && !(sourceProbe.evidence == request.expected_source))
            {
                return {
                    CompileRefusal::source_evidence_mismatch,
                    {},
                    "script source no longer matches selected source evidence"};
            }

            FileEvidence outputBefore{};
            const FileProbeResult outputProbe = inspect_file(
                output, request.maximum_artifact_bytes);
            if (outputProbe.status == FileProbeStatus::available)
                outputBefore = outputProbe.evidence;
            else if (outputProbe.status != FileProbeStatus::missing)
            {
                return {
                    CompileRefusal::filesystem_error,
                    {},
                    "existing script output is not a replaceable regular file"};
            }

            CompilePlan plan{};
            plan.request_generation = request.request_generation;
            plan.project_generation = request.project_generation;
            plan.workspace_root = root;
            plan.source_path = source;
            plan.output_path = output;
            plan.temporary_output_path = detail::temporary_output_path(
                output, request.request_generation);
            const std::filesystem::path selectedCompiler =
                request.compiler_path.empty()
                    ? detail::resolve_clangxx()
                    : request.compiler_path;
            plan.compiler_path = detail::resolve_executable(selectedCompiler);
            if (plan.compiler_path.empty())
            {
                return {
                    CompileRefusal::compiler_unavailable,
                    {},
                    "selected script compiler is unavailable"};
            }
            plan.include_roots = detail::resolve_include_roots(
                source, plan.compiler_path);
            plan.library_roots = detail::resolve_library_roots(
                plan.compiler_path);
            plan.source_before = sourceProbe.evidence;
            plan.output_before = outputBefore;
            plan.maximum_source_bytes = request.maximum_source_bytes;
            plan.maximum_artifact_bytes = request.maximum_artifact_bytes;

            if (!detail::path_is_within(root, plan.temporary_output_path)
                || plan.temporary_output_path == source
                || plan.temporary_output_path == output)
            {
                return {
                    CompileRefusal::path_outside_workspace,
                    {},
                    "temporary script artifact escaped its workspace"};
            }
            for (const auto& temporary :
                detail::temporary_artifact_paths(plan.temporary_output_path))
            {
                const auto availability = detail::path_availability(temporary);
                if (availability == detail::PathAvailability::inspection_failed)
                {
                    return {
                        CompileRefusal::filesystem_error,
                        {},
                        "temporary script artifact path could not be inspected"};
                }
                if (availability == detail::PathAvailability::occupied)
                {
                    return {
                        CompileRefusal::temporary_path_occupied,
                        {},
                        "temporary script artifact path is already occupied"};
                }
            }
            plan.command_description = detail::describe_command(
                plan.compiler_path,
                plan.source_path,
                plan.temporary_output_path,
                plan.include_roots,
                plan.library_roots);
            return {CompileRefusal::none, std::move(plan), {}};
        }
        catch (const std::exception& exception)
        {
            return {CompileRefusal::filesystem_error, {}, exception.what()};
        }
        catch (...)
        {
            return {
                CompileRefusal::filesystem_error,
                {},
                "unknown script compilation planning failure"};
        }
    }
    export [[nodiscard]] CompileResult compile_script(
        const CompileRequest& request)
    {
        CompileResult result{};
        result.evidence.request_generation = request.request_generation;
        result.evidence.project_generation = request.project_generation;

        detail::ActiveCompileGuard active{};
        if (!active.acquired())
        {
            result.status = CompileStatus::busy;
            result.refusal = CompileRefusal::active_request;
            result.message = "another script compilation is already active";
            return result;
        }

        CompilePlanningResult planned = plan_script_compile(request);
        if (!planned)
        {
            result.status = planned.refusal == CompileRefusal::cancelled
                ? CompileStatus::cancelled
                : CompileStatus::refused;
            result.refusal = planned.refusal;
            result.message = std::move(planned.message);
            return result;
        }

        CompilePlan& plan = planned.plan;
        result.evidence.request_generation = plan.request_generation;
        result.evidence.project_generation = plan.project_generation;
        result.evidence.source_path = plan.source_path;
        result.evidence.output_path = plan.output_path;
        result.evidence.temporary_output_path = plan.temporary_output_path;
        result.evidence.command_description = plan.command_description;
        result.evidence.source_before = plan.source_before;
        result.evidence.output_before = plan.output_before;

        if (request.cancellation.stop_requested())
        {
            result.status = CompileStatus::cancelled;
            result.refusal = CompileRefusal::cancelled;
            result.message = "script compilation was cancelled after preflight";
            return result;
        }
        if (!detail::plan_paths_still_scoped(plan))
        {
            result.status = CompileStatus::verification_failed;
            result.refusal = CompileRefusal::path_outside_workspace;
            result.message = "script paths changed scope after preflight";
            return result;
        }
        result.evidence.scope_revalidated = true;

        for (const auto& temporary :
            detail::temporary_artifact_paths(plan.temporary_output_path))
        {
            const auto availability = detail::path_availability(temporary);
            if (availability == detail::PathAvailability::inspection_failed)
            {
                result.status = CompileStatus::refused;
                result.refusal = CompileRefusal::filesystem_error;
                result.message =
                    "temporary script artifact could not be inspected";
                return result;
            }
            if (availability == detail::PathAvailability::occupied)
            {
                result.status = CompileStatus::refused;
                result.refusal = CompileRefusal::temporary_path_occupied;
                result.message =
                    "temporary script artifact became occupied after preflight";
                return result;
            }
        }

        logger::infof_loc(
            "Compiler",
            std::source_location::current(),
            "script compile generation {}: {}",
            plan.request_generation,
            plan.command_description);
        const detail::SpawnResult spawned = detail::spawn_compiler(
            plan.compiler_path,
            plan.source_path,
            plan.temporary_output_path,
            plan.include_roots,
            plan.library_roots,
            request.cancellation);
        result.evidence.compiler_started = spawned.launched;
        if (spawned.cancelled)
        {
            detail::cleanup_temporary_artifacts(plan.temporary_output_path);
            result.status = CompileStatus::cancelled;
            result.refusal = CompileRefusal::cancelled;
            result.message = "script compilation was cancelled while the compiler process was running";
            return result;
        }        if (!spawned.succeeded)
        {
            if (spawned.launched)
                detail::cleanup_temporary_artifacts(plan.temporary_output_path);
            result.status = CompileStatus::compiler_failed;
            result.refusal = CompileRefusal::none;
            result.message =
                spawned.launched
                    ? "script compiler returned an unsuccessful process result"
                    : "script compiler process could not be started";
            return result;
        }
        if (request.cancellation.stop_requested())
        {
            detail::cleanup_temporary_artifacts(plan.temporary_output_path);
            result.status = CompileStatus::cancelled;
            result.refusal = CompileRefusal::cancelled;
            result.message = "script compilation was cancelled before verification";
            return result;
        }

        const FileProbeResult candidate = inspect_file(
            plan.temporary_output_path, plan.maximum_artifact_bytes);
        if (!candidate || candidate.evidence.size_bytes == 0u)
        {
            detail::cleanup_temporary_artifacts(plan.temporary_output_path);
            result.status = CompileStatus::verification_failed;
            result.refusal = candidate.status == FileProbeStatus::missing
                ? CompileRefusal::candidate_missing
                : CompileRefusal::candidate_invalid;
            result.message =
                "compiler did not produce a valid bounded script artifact";
            return result;
        }
        result.evidence.candidate = candidate.evidence;
        result.evidence.candidate_verified = true;

        if (!detail::plan_paths_still_scoped(plan))
        {
            detail::cleanup_temporary_artifacts(plan.temporary_output_path);
            result.status = CompileStatus::verification_failed;
            result.refusal = CompileRefusal::path_outside_workspace;
            result.message = "script paths changed scope before publication";
            return result;
        }
        result.evidence.publication_scope_revalidated = true;

        const FileProbeResult sourceAfter = inspect_file(
            plan.source_path, plan.maximum_source_bytes);
        if (!sourceAfter || !(sourceAfter.evidence == plan.source_before))
        {
            detail::cleanup_temporary_artifacts(plan.temporary_output_path);
            result.status = CompileStatus::verification_failed;
            result.refusal = CompileRefusal::source_changed;
            result.message = "script source changed while compilation was running";
            return result;
        }
        result.evidence.source_after = sourceAfter.evidence;
        result.evidence.source_revalidated = true;

        const FileProbeResult outputCurrent = inspect_file(
            plan.output_path, plan.maximum_artifact_bytes);
        const FileEvidence currentOutput = outputCurrent.status
                == FileProbeStatus::available
            ? outputCurrent.evidence
            : FileEvidence{};
        if ((outputCurrent.status != FileProbeStatus::available
                && outputCurrent.status != FileProbeStatus::missing)
            || !(currentOutput == plan.output_before))
        {
            detail::cleanup_temporary_artifacts(plan.temporary_output_path);
            result.status = CompileStatus::verification_failed;
            result.refusal = CompileRefusal::output_changed;
            result.message = "script output changed while compilation was running";
            return result;
        }
        result.evidence.output_ownership_revalidated = true;

        if (request.cancellation.stop_requested())
        {
            detail::cleanup_temporary_artifacts(plan.temporary_output_path);
            result.status = CompileStatus::cancelled;
            result.refusal = CompileRefusal::cancelled;
            result.message = "script compilation was cancelled before publication";
            return result;
        }

        std::error_code error{};
        if (!platform::filesystem::atomic_replace_same_filesystem(
                plan.temporary_output_path, plan.output_path, error))
        {
            detail::cleanup_temporary_artifacts(plan.temporary_output_path);
            result.status = CompileStatus::publish_failed;
            result.refusal = CompileRefusal::atomic_replace_failed;
            result.message = "atomic script artifact replacement failed: "
                + error.message();
            return result;
        }
        result.evidence.atomic_replace_committed = true;
        detail::cleanup_temporary_artifacts(plan.temporary_output_path);

        const FileProbeResult published = inspect_file(
            plan.output_path, plan.maximum_artifact_bytes);
        if (!published
            || published.evidence.size_bytes != candidate.evidence.size_bytes
            || !(published.evidence.content == candidate.evidence.content))
        {
            result.status = CompileStatus::verification_failed;
            result.refusal = CompileRefusal::published_evidence_mismatch;
            result.message =
                "published script artifact does not match its verified candidate";
            return result;
        }
        result.evidence.published = published.evidence;
        result.evidence.published_artifact_verified = true;
        result.status = CompileStatus::succeeded;
        result.refusal = CompileRefusal::none;
        result.message = "verified script artifact published atomically";
        return result;
    }

    export [[nodiscard]] CompileResult compile_script_to_dll_result(
        const std::filesystem::path& input,
        const std::filesystem::path& output,
        std::stop_token cancellation = {})
    {
        std::error_code error{};
        const auto source =
            std::filesystem::absolute(input, error).lexically_normal();
        const auto destination =
            std::filesystem::absolute(output, error).lexically_normal();
        std::filesystem::path workspace = source.parent_path();
        if (const auto repo = core::path::find_epoch_repo_root(source);
            !repo.empty()
            && detail::path_is_within(repo, source)
            && detail::path_is_within(repo, destination))
        {
            workspace = repo;
        }

        const std::uint64_t generation =
            detail::compatibility_generation.fetch_add(
                1u, std::memory_order_relaxed) + 1u;
        return compile_script({
            .source_path = source,
            .output_path = destination,
            .workspace_root = workspace,
            .request_generation = generation,
            .project_generation = 1u,
            .cancellation = cancellation});
    }

    export bool compile_script_to_dll(
        const std::filesystem::path& input,
        const std::filesystem::path& output,
        std::stop_token cancellation = {})
    {
        const CompileResult result =
            compile_script_to_dll_result(input, output, cancellation);
        if (!result)
        {
            logger::errorf_loc(
                "Compiler",
                std::source_location::current(),
                "script compile failed: status={} refusal={} message={}",
                compile_status_name(result.status),
                compile_refusal_name(result.refusal),
                result.message);
        }
        return static_cast<bool>(result);
    }

}
