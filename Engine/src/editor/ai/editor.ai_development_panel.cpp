/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include "core.format_text.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module editor.ai_development_panel;

import ai.development_proposal_codec;
import ai.iteration_loop;
import ai.iteration_session;
import ai.self_iteration_orchestrator;
import editor.ai_development_controller;
import gui.engine;
import core.sha256;

namespace epochengine::editor_ai_development_panel
{
    namespace
    {
        [[nodiscard]] editor_ai_development::LogicalTime logical_time_now() noexcept
        {
            const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            return editor_ai_development::LogicalTime{
                static_cast<std::uint64_t>((std::max)(
                    std::int64_t{1}, seconds))};
        }
    }


        [[nodiscard]] std::string content_state_summary(
            const editor_ai_development::ContentState& state)
        {
            if (!state.exists)
                return "absent";
            static constexpr char digits[] = "0123456789abcdef";
            std::string result{};
            result.reserve(48u);
            for (std::size_t index = 0u; index < 8u; ++index)
            {
                const std::uint8_t byte = state.digest.bytes[index];
                result.push_back(digits[byte >> 4u]);
                result.push_back(digits[byte & 0x0fu]);
            }
            result += "... (";
            result += std::to_string(state.byte_count);
            result += " bytes)";
            return result;
        }

    namespace
    {
        constexpr std::size_t maximum_source_context_evidence_bytes =
            184u * 1024u;
        constexpr std::size_t maximum_full_source_context_bytes =
            48u * 1024u;
        constexpr std::size_t maximum_source_excerpt_bytes =
            16u * 1024u;

        struct SourceContextLoadResult final
        {
            bool accepted{};
            std::size_t file_count{};
            std::size_t source_bytes{};
            std::string evidence{};
            std::string status{};
        };

        [[nodiscard]] bool path_is_within(
            const std::filesystem::path& root,
            const std::filesystem::path& candidate) noexcept
        {
            const std::filesystem::path relative =
                candidate.lexically_relative(root);
            if (relative.empty() || relative == "." || relative.is_absolute())
                return false;
            return std::none_of(relative.begin(), relative.end(),
                [](const std::filesystem::path& component)
                {
                    return component == "..";
                });
        }

        struct HostSourceContextSelection final
        {
            bool accepted{};
            std::vector<std::string> paths{};
            std::string status{};
        };

        struct SourceContextCandidate final
        {
            int score{};
            std::uintmax_t byte_count{};
            std::string relative_path{};
        };

        [[nodiscard]] std::string lower_ascii(std::string_view text)
        {
            std::string result{};
            result.reserve(text.size());
            for (const char value : text)
            {
                result.push_back(value >= 'A' && value <= 'Z'
                    ? static_cast<char>(value - 'A' + 'a')
                    : value);
            }
            return result;
        }

        void add_source_context_term(
            std::vector<std::string>& terms,
            std::string term)
        {
            static constexpr std::array<std::string_view, 44> stopWords{
                "a", "add", "an", "and", "be", "build", "by", "cannot",
                "change", "code", "confused", "cpp", "create", "currently",
                "cxx", "does", "doesn", "engine", "feature", "file", "fix",
                "for", "from", "hpp", "inside", "it", "ixx", "just",
                "line", "make", "not", "now", "only", "please", "proving",
                "should", "shouldn", "source", "that", "the", "this", "user",
                "with", "work"};
            if (term.size() < 2u
                || std::ranges::find(stopWords, term) != stopWords.end()
                || std::ranges::find(terms, term) != terms.end())
            {
                return;
            }
            terms.push_back(std::move(term));
        }

        [[nodiscard]] std::vector<std::string> source_context_terms(
            std::string_view objective)
        {
            const std::string lowered = lower_ascii(objective);
            std::vector<std::string> terms{};
            std::string token{};
            for (const char value : lowered)
            {
                if ((value >= 'a' && value <= 'z')
                    || (value >= '0' && value <= '9')
                    || value == '_')
                {
                    token.push_back(value);
                }
                else if (!token.empty())
                {
                    add_source_context_term(terms, std::move(token));
                    token.clear();
                }
            }
            if (!token.empty())
                add_source_context_term(terms, std::move(token));

            const auto addDelimitedPhrases =
                [&](const char delimiter)
                {
                    std::size_t cursor{};
                    while (cursor < lowered.size())
                    {
                        const std::size_t begin =
                            lowered.find(delimiter, cursor);
                        if (begin == std::string::npos)
                            break;
                        const std::size_t end =
                            lowered.find(delimiter, begin + 1u);
                        if (end == std::string::npos)
                            break;
                        std::string phrase =
                            lowered.substr(begin + 1u, end - begin - 1u);
                        const std::size_t first =
                            phrase.find_first_not_of(" \t\r\n");
                        const std::size_t last =
                            phrase.find_last_not_of(" \t\r\n");
                        if (first != std::string::npos)
                        {
                            phrase = phrase.substr(first, last - first + 1u);
                            if (phrase.size() >= 3u)
                            {
                                add_source_context_term(
                                    terms, std::move(phrase));
                            }
                        }
                        cursor = end + 1u;
                    }
                };
            addDelimitedPhrases('"');
            addDelimitedPhrases('`');

            const auto addAlias = [&](std::string_view needle,
                                      std::string alias)
            {
                if (lowered.find(needle) != std::string::npos)
                    add_source_context_term(terms, std::move(alias));
            };
            addAlias("logger", "log");
            addAlias("logging", "log");
            addAlias("widget", "gui");
            addAlias("dock", "gui");
            addAlias("editbox", "text_control");
            addAlias("edit box", "text_control");
            addAlias("textbox", "text_control");
            addAlias("text box", "text_control");
            addAlias("text input", "text_control");
            addAlias("textinput", "text_control");
            addAlias("input field", "text_control");
            addAlias("backspace", "text_control");
            addAlias("caret", "text_control");
            addAlias("text cursor", "text_control");
            addAlias("editbox", "epochgui");
            addAlias("edit box", "epochgui");
            addAlias("textbox", "epochgui");
            addAlias("text input", "epochgui");
            addAlias("backspace", "epochgui");
            addAlias("playhead", "timeline");
            addAlias("camera", "preview_grid");
            addAlias("middle mouse", "preview_grid");
            addAlias("mouse", "input");
            addAlias("pan", "preview_grid");
            addAlias("orbit", "preview_grid");
            addAlias("dolly", "preview_grid");
            addAlias("navigation", "preview_grid");
            addAlias("alt", "input");
            addAlias("selection", "interaction");
            addAlias("model", "ai");
            addAlias("iteration", "ai");
            addAlias("proposal", "ai");
            addAlias("context", "render");
            addAlias("renderer", "render");
            addAlias("asset", "project");
            addAlias("tree", "forest");
            addAlias("plant", "forest");
            const bool directAiObjective =
                lowered.find("llama") != std::string::npos
                || lowered.find("qwen") != std::string::npos
                || lowered.find("inference") != std::string::npos
                || lowered.find("transcript") != std::string::npos;
            if (directAiObjective)
            {
                add_source_context_term(terms, "ai.engine");
                if (lowered.find("contract") != std::string::npos
                    || lowered.find("regression") != std::string::npos
                    || lowered.find("test") != std::string::npos)
                {
                    add_source_context_term(terms, "epoch.engine_legacy");
                }
            }
            return terms;
        }

        [[nodiscard]] std::optional<std::string>
            unique_explicit_replacement_target(
                std::string_view objective,
                std::string_view evidence)
        {
            const std::string lowered = lower_ascii(objective);
            if (lowered.find("replace") == std::string::npos
                && lowered.find("change") == std::string::npos)
            {
                return std::nullopt;
            }

            const std::size_t doubleQuote = objective.find('"');
            const std::size_t backtick = objective.find('`');
            std::size_t begin = doubleQuote;
            char delimiter = '"';
            if (begin == std::string_view::npos
                || (backtick != std::string_view::npos && backtick < begin))
            {
                begin = backtick;
                delimiter = '`';
            }
            if (begin == std::string_view::npos)
                return std::nullopt;
            const std::size_t end = objective.find(delimiter, begin + 1u);
            if (end == std::string_view::npos)
                return std::nullopt;

            std::string target{objective.substr(begin + 1u, end - begin - 1u)};
            const std::size_t firstText =
                target.find_first_not_of(" \t\r\n");
            const std::size_t lastText =
                target.find_last_not_of(" \t\r\n");
            if (firstText == std::string::npos)
                return std::nullopt;
            target = target.substr(firstText, lastText - firstText + 1u);
            if (target.size() < 3u)
                return std::nullopt;

            const std::size_t occurrence = evidence.find(target);
            if (occurrence == std::string_view::npos
                || evidence.find(target, occurrence + target.size())
                    != std::string_view::npos)
            {
                return std::nullopt;
            }
            return target;
        }

        [[nodiscard]] bool bounded_diagnostic_objective(
            std::string_view objective)
        {
            static constexpr std::array<std::string_view, 8> diagnostics{
                "bug", "bugs", "crash", "defect", "failure", "issue",
                "regression", "wrong"};
            const std::string lowered = lower_ascii(objective);
            const std::vector<std::string> terms =
                source_context_terms(objective);
            const bool requestsDiagnosis = std::ranges::any_of(
                diagnostics,
                [&lowered](std::string_view term)
                {
                    return lowered.find(term) != std::string::npos;
                });
            const bool namesOwner = std::ranges::any_of(
                terms,
                [](const std::string& term)
                {
                    return std::ranges::find(diagnostics, term)
                        == diagnostics.end();
                });
            return requestsDiagnosis && namesOwner;
        }

        [[nodiscard]] bool ignored_source_context_component(
            const std::filesystem::path& component)
        {
            static constexpr std::array<std::string_view, 12> ignored{
                ".git", ".vs", "bin", "build", "built", "cache",
                "generated", "logs", "out", "packages", "vcpkg_installed",
                "x64"};
            const std::string name = lower_ascii(component.generic_string());
            return std::ranges::find(ignored, name) != ignored.end();
        }

        [[nodiscard]] bool source_context_extension(
            const std::filesystem::path& path)
        {
            const std::string extension = lower_ascii(path.extension().string());
            return extension == ".cpp" || extension == ".h"
                || extension == ".hpp" || extension == ".ixx";
        }

        [[nodiscard]] HostSourceContextSelection curate_source_context(
            std::string_view sourceRoot,
            Domain domain,
            std::string_view objective)
        {
            HostSourceContextSelection result{};
            std::string normalizedObjective = lower_ascii(objective);
            std::ranges::replace(normalizedObjective, '\\', '/');

            const std::vector<std::string> terms = source_context_terms(objective);
            if (terms.empty())
            {
                result.status =
                    "The objective does not identify a source subsystem. Name the affected behavior or owner before requesting a proposal.";
                return result;
            }

            std::error_code error{};
            const std::filesystem::path root = std::filesystem::weakly_canonical(
                std::filesystem::path{sourceRoot}, error);
            if (error || !root.is_absolute()
                || !std::filesystem::is_directory(root, error) || error)
            {
                result.status =
                    "The source checkout is unavailable; host-curated context cannot be prepared. No source bytes were read or sent.";
                return result;
            }

            std::vector<std::filesystem::path> scanRoots{};
            if (domain == Domain::engine_source)
            {
                scanRoots = {
                    root / "Engine/src",
                    root / "Engine/modules",
                    root / "Engine/include",
                    root / "Engine/dep/EpochGui/src",
                    root / "Engine/dep/EpochGui/modules",
                    root / "Engine/dep/EpochGui/include",
                    root / "Engine/dep/EpochGui/tests"};
            }
            else
            {
                scanRoots = {root / "Projects"};
            }

            std::vector<SourceContextCandidate> candidates{};
            std::size_t scannedFiles{};
            constexpr std::size_t maximumScannedFiles = 8'192u;
            constexpr std::uintmax_t maximumCandidateBytes =
                maximum_source_context_evidence_bytes;
            constexpr int exactObjectivePathScore = 1'000'000;
            for (const auto& scanRoot : scanRoots)
            {
                error.clear();
                if (!std::filesystem::is_directory(scanRoot, error) || error)
                    continue;

                std::filesystem::recursive_directory_iterator cursor{
                    scanRoot,
                    std::filesystem::directory_options::skip_permission_denied,
                    error};
                const std::filesystem::recursive_directory_iterator end{};
                error.clear();
                while (cursor != end && scannedFiles < maximumScannedFiles)
                {
                    const std::filesystem::directory_entry entry = *cursor;
                    error.clear();
                    if (entry.is_directory(error))
                    {
                        if (!error && ignored_source_context_component(
                                entry.path().filename()))
                        {
                            cursor.disable_recursion_pending();
                        }
                    }
                    else if (!error && entry.is_regular_file(error)
                        && !error && source_context_extension(entry.path()))
                    {
                        ++scannedFiles;
                        error.clear();
                        const std::uintmax_t fileBytes = entry.file_size(error);
                        if (!error && fileBytes <= maximumCandidateBytes)
                        {
                            const std::filesystem::path relative =
                                entry.path().lexically_relative(root);
                            const std::string relativeText =
                                relative.generic_string();
                            const std::string searchable = lower_ascii(relativeText);
                            const bool exactObjectivePath =
                                normalizedObjective.find(searchable) != std::string::npos;
                            const std::string filename = lower_ascii(
                                entry.path().filename().string());
                            int score = exactObjectivePath ? exactObjectivePathScore : 0;
                            for (const auto& term : terms)
                            {
                                const bool explicitOwner =
                                    term.find('_') != std::string::npos
                                    || term.find('.') != std::string::npos;
                                if (filename.find(term) != std::string::npos)
                                    score += explicitOwner ? 48 : 12;
                                if (searchable.find(term) != std::string::npos)
                                    score += explicitOwner ? 16 : 4;
                            }
                            if (score > 0)
                            {
                                if (searchable.find("/src/") != std::string::npos)
                                    score += 3;
                                else if (searchable.find("/modules/") != std::string::npos
                                    || searchable.find("/tests/") != std::string::npos)
                                    score += 2;
                                else if (searchable.find("/include/") != std::string::npos)
                                    score += 1;
                            }
                            if (score > 0)
                            {
                                candidates.push_back(SourceContextCandidate{
                                    score, fileBytes, relativeText});
                            }
                        }
                    }

                    error.clear();
                    cursor.increment(error);
                    if (error)
                        error.clear();
                }
            }

            std::ranges::sort(candidates,
                [](const SourceContextCandidate& left,
                   const SourceContextCandidate& right)
                {
                    if (left.score != right.score)
                        return left.score > right.score;
                    if (left.byte_count != right.byte_count)
                        return left.byte_count < right.byte_count;
                    return left.relative_path < right.relative_path;
                });

            const bool matchedExactPath = !candidates.empty()
                && candidates.front().score >= exactObjectivePathScore;
            constexpr std::size_t maximumCandidateCount = 6u;
            constexpr std::uintmax_t maximumAggregateSelectedBytes =
                128u * 1024u;
            const int minimumSelectedScore = candidates.empty()
                ? 0
                : (std::max)(1, candidates.front().score / 2);
            std::uintmax_t selectedBytes{};
            for (const auto& candidate : candidates)
            {
                if (result.paths.size() >= maximumCandidateCount)
                    break;
                if (candidate.score < minimumSelectedScore)
                    break;
                if (!result.paths.empty()
                    && selectedBytes + candidate.byte_count
                        > maximumAggregateSelectedBytes)
                    continue;
                result.paths.push_back(candidate.relative_path);
                selectedBytes += candidate.byte_count;
            }

            if (result.paths.empty())
            {
                result.status =
                    "Epoch found no unambiguous host-ranked source match for the objective.";
                return result;
            }

            result.accepted = true;
            if (matchedExactPath)
            {
                result.status = epochengine::format_text(
                    "Epoch matched {} exact existing source path(s) from the objective ({} KiB total). Review this host-owned selection before sharing any bytes.",
                    result.paths.size(),
                    (selectedBytes + 1023u) / 1024u);
            }
            else
            {
                result.status = epochengine::format_text(
                    "Epoch ranked {} existing source file(s) from the objective ({} KiB total). Review this host-owned selection before sharing any bytes.",
                    result.paths.size(),
                    (selectedBytes + 1023u) / 1024u);
            }
            return result;
        }

        [[nodiscard]] SourceContextLoadResult load_reviewed_source_context(
            std::string_view sourceRoot,
            const std::vector<std::string>& relativePaths,
            std::string_view baseEvidence,
            std::string_view objective)
        {
            SourceContextLoadResult result{};
            if (sourceRoot.empty() || relativePaths.empty())
            {
                result.status =
                    "The reviewed source root or path list is unavailable.";
                return result;
            }
            if (baseEvidence.size() > maximum_source_context_evidence_bytes)
            {
                result.status =
                    "The host architecture evidence already exceeds the bounded context budget.";
                return result;
            }

            std::error_code error{};
            const std::filesystem::path root = std::filesystem::weakly_canonical(
                std::filesystem::path{sourceRoot}, error);
            if (error || !root.is_absolute()
                || !std::filesystem::is_directory(root, error) || error)
            {
                result.status =
                    "The reviewed source root could not be resolved as a directory.";
                return result;
            }

            result.evidence.assign(baseEvidence);
            if (!result.evidence.empty() && !result.evidence.ends_with('\n'))
                result.evidence.push_back('\n');

            for (const auto& relativeText : relativePaths)
            {
                const std::filesystem::path requested =
                    root / std::filesystem::path{relativeText};
                error.clear();
                const bool exists = std::filesystem::exists(requested, error);
                if (error)
                {
                    result.status =
                        "The reviewed path could not be inspected: " + relativeText + ".";
                    return result;
                }

                if (!exists)
                {
                    error.clear();
                    const std::filesystem::path parent =
                        std::filesystem::weakly_canonical(
                            requested.parent_path(), error);
                    if (error || !path_is_within(root, parent))
                    {
                        result.status =
                            "The absent reviewed path escapes the selected source root: "
                            + relativeText + ".";
                        return result;
                    }
                    const std::string marker =
                        "FILE_ABSENT " + relativeText + "\n";
                    if (result.evidence.size() + marker.size()
                        > maximum_source_context_evidence_bytes)
                    {
                        result.status =
                            "The reviewed source context exceeds its bounded evidence budget.";
                        return result;
                    }
                    result.evidence += marker;
                    continue;
                }

                error.clear();
                const std::filesystem::path resolved =
                    std::filesystem::canonical(requested, error);
                if (error || !path_is_within(root, resolved)
                    || !std::filesystem::is_regular_file(resolved, error) || error)
                {
                    result.status =
                        "The reviewed path is not a regular file inside the selected source root: "
                        + relativeText + ".";
                    return result;
                }

                error.clear();
                const auto fileSize = std::filesystem::file_size(resolved, error);
                if (error || fileSize > maximum_source_context_evidence_bytes)
                {
                    result.status =
                        "The reviewed source file exceeds the bounded evidence budget: "
                        + relativeText + ".";
                    return result;
                }
                std::string bytes(static_cast<std::size_t>(fileSize), '\0');
                std::ifstream stream{resolved, std::ios::binary};
                if (!stream
                    || (!bytes.empty() && !stream.read(
                        bytes.data(), static_cast<std::streamsize>(bytes.size()))))
                {
                    result.status =
                        "The reviewed source file could not be read exactly: "
                        + relativeText + ".";
                    return result;
                }
                if (!ai::development_proposal_codec::valid_context_text(bytes))
                {
                    result.status =
                        "The reviewed source file is not valid UTF-8 text: "
                        + relativeText + ".";
                    return result;
                }

                std::string_view sharedBytes{bytes};
                std::size_t excerptOffset{};
                bool excerpted{};
                if (bytes.size() > maximum_full_source_context_bytes)
                {
                    const std::string loweredBytes = lower_ascii(bytes);
                    const std::vector<std::string> terms =
                        source_context_terms(objective);
                    std::size_t anchor{};
                    std::size_t strongestTermBytes{};
                    for (const auto& term : terms)
                    {
                        if (term.size() <= strongestTermBytes)
                            continue;
                        const std::size_t occurrence = loweredBytes.find(term);
                        if (occurrence == std::string::npos)
                            continue;
                        anchor = occurrence;
                        strongestTermBytes = term.size();
                    }

                    excerptOffset = anchor > maximum_source_excerpt_bytes / 2u
                        ? anchor - maximum_source_excerpt_bytes / 2u
                        : 0u;
                    if (excerptOffset > 0u)
                    {
                        const std::size_t precedingNewline =
                            bytes.rfind('\n', excerptOffset);
                        if (precedingNewline != std::string::npos
                            && excerptOffset - (precedingNewline + 1u)
                                <= 1024u)
                        {
                            excerptOffset = precedingNewline + 1u;
                        }
                        else
                        {
                            while (excerptOffset > 0u
                                && (static_cast<unsigned char>(
                                        bytes[excerptOffset]) & 0xc0u) == 0x80u)
                            {
                                --excerptOffset;
                            }
                        }
                    }
                    std::size_t excerptEnd = (std::min)(
                        bytes.size(),
                        excerptOffset + maximum_source_excerpt_bytes);
                    if (excerptEnd < bytes.size())
                    {
                        const std::size_t endingNewline =
                            bytes.rfind('\n', excerptEnd);
                        if (endingNewline != std::string::npos
                            && endingNewline >= excerptOffset
                            && excerptEnd - (endingNewline + 1u) <= 1024u)
                        {
                            excerptEnd = endingNewline + 1u;
                        }
                        else
                        {
                            while (excerptEnd > excerptOffset
                                && (static_cast<unsigned char>(
                                        bytes[excerptEnd]) & 0xc0u) == 0x80u)
                            {
                                --excerptEnd;
                            }
                        }
                    }
                    sharedBytes = std::string_view{bytes}.substr(
                        excerptOffset,
                        excerptEnd - excerptOffset);
                    excerpted = true;
                }

                std::string block{};
                if (excerpted)
                {
                    block =
                        "FILE_SOURCE_SIZE " + relativeText + " "
                        + std::to_string(bytes.size()) + "\n"
                        + "FILE_EXCERPT_OFFSET " + relativeText + " "
                        + std::to_string(excerptOffset) + "\n"
                        + "FILE_EXCERPT_SIZE " + relativeText + " "
                        + std::to_string(sharedBytes.size()) + "\n"
                        + "FILE_EXCERPT_BEGIN " + relativeText + "\n";
                    block.append(sharedBytes);
                    if (!block.ends_with('\n'))
                        block.push_back('\n');
                    block += "FILE_EXCERPT_END " + relativeText + "\n";
                }
                else
                {
                    block =
                        "FILE_CONTENT_SIZE " + relativeText + " "
                        + std::to_string(sharedBytes.size()) + "\n"
                        + "FILE_CONTENT_BEGIN " + relativeText + "\n";
                    block.append(sharedBytes);
                    if (!block.ends_with('\n'))
                        block.push_back('\n');
                    block += "FILE_CONTENT_END " + relativeText + "\n";
                }
                if (result.evidence.size() + block.size()
                    > maximum_source_context_evidence_bytes)
                {
                    result.status =
                        "The reviewed source context exceeds its bounded evidence budget.";
                    return result;
                }
                result.source_bytes += sharedBytes.size();
                ++result.file_count;
                result.evidence += block;
            }

            result.accepted = true;
            result.status = epochengine::format_text(
                "Prepared {} reviewed source file(s), {} source byte(s), and exact absent-path evidence for this request. Primary evidence: {}.",
                result.file_count,
                result.source_bytes,
                relativePaths.front());
            return result;
        }
        struct GroundingResult final
        {
            bool accepted{};
            std::string status{};
        };

        [[nodiscard]] GroundingResult validate_grounded_proposal(
            const ai::development_proposal_codec::Proposal& proposal,
            std::string_view objective,
            std::string_view evidence,
            std::string_view sourceRoot)
        {
            if (objective.empty())
            {
                return {
                    false,
                    "Enter a bounded development objective before staging source."};
            }

            for (const auto& change : proposal.changes)
            {
                const std::string sizeMarker =
                    "FILE_CONTENT_SIZE " + change.relative_path + " ";
                const std::string contentMarker =
                    "FILE_CONTENT_BEGIN " + change.relative_path + "\n";
                const std::string excerptSizeMarker =
                    "FILE_EXCERPT_SIZE " + change.relative_path + " ";
                const std::string excerptMarker =
                    "FILE_EXCERPT_BEGIN " + change.relative_path + "\n";
                const std::string absentMarker =
                    "FILE_ABSENT " + change.relative_path + "\n";
                const bool hasFullEvidence =
                    evidence.find(sizeMarker) != std::string_view::npos
                    && evidence.find(contentMarker) != std::string_view::npos;
                const bool hasExcerptEvidence =
                    evidence.find(excerptSizeMarker) != std::string_view::npos
                    && evidence.find(excerptMarker) != std::string_view::npos;
                std::error_code error{};
                const bool existing = std::filesystem::is_regular_file(
                    std::filesystem::path{sourceRoot}
                        / std::filesystem::path{change.relative_path},
                    error) && !error;
                if (existing && !hasFullEvidence && !hasExcerptEvidence)
                {
                    return {
                        false,
                        "Proposal rejected: exact counted current contents or an "
                        "objective-centered excerpt were not loaded for "
                        + change.relative_path + "."};
                }
                if (existing && hasExcerptEvidence
                    && change.edit_kind
                        != ai::development_proposal_codec::SourceEditKind::
                            replace_exact_block)
                {
                    return {
                        false,
                        "Proposal rejected: excerpt evidence permits an exact-block "
                        "edit only for " + change.relative_path + "."};
                }
                if (!existing
                    && evidence.find(absentMarker) == std::string_view::npos)
                {
                    return {
                        false,
                        "Proposal rejected: the proposed new path was not "
                        "reviewed and proven absent: " + change.relative_path
                        + "."};
                }
                if (!existing
                    && objective.find(change.relative_path)
                        == std::string_view::npos)
                {
                    return {
                        false,
                        "Proposal rejected: the model invented new path "
                        + change.relative_path
                        + "; name an exact new path in the objective first."};
                }
            }
            return {
                true,
                "Source proposal is grounded in the selected objective and "
                "exact source evidence."};
        }
    }

    struct Panel::Implementation final
    {
        static constexpr std::size_t maximum_source_repair_attempts = 3u;
        static constexpr std::size_t maximum_model_reply_corrections = 2u;

        std::unique_ptr<editor_ai_development::DevelopmentController> controller{};
        std::unique_ptr<editor_ai_development::DevelopmentController> promotion_controller{};
        std::unique_ptr<ai::iteration_session::IterationSession> iteration_session{};
        std::unique_ptr<ai::self_iteration_orchestrator::Orchestrator>
            campaign_orchestrator{};
        std::optional<ai::self_iteration_orchestrator::PendingOperation>
            campaign_pending_operation{};
        ai::self_iteration_orchestrator::Configuration campaign_configuration{};
        std::filesystem::path campaign_state_path{};
        ai::project_profile::Provider campaign_provider{
            ai::project_profile::Provider::epoch_local_qwen38};
        std::string campaign_scope_digest{};
        std::uint64_t campaign_transition_generation{};
        std::uint32_t generation{};
        std::string workspace_id{};
        std::string source_root{};
        std::string sandbox_base_root{};
        std::string workspace_root{};
        std::string development_objective{};
        std::string source_context_evidence{};
        std::string source_baseline_evidence{};
        std::string source_context_evidence_objective{};
        std::vector<std::string> pending_source_context_paths{};
        std::string pending_source_context_reason{};
        std::string pending_source_context_objective{};
        std::string source_candidate_raw_reply{};
        std::vector<editor_ai_development::OperationSummary>
            source_candidate_operations{};
        editor_ai_development::OperationKind source_candidate_kind{
            editor_ai_development::OperationKind::engine_source_edit};
        bool advanced_controls{};
        bool source_workspace_ready{};
        bool source_workspace_pending{};
        bool source_build_pending{};
        bool source_test_pending{};
        bool source_build_verified{};
        bool source_test_verified{};
        bool source_release_build_pending{};
        bool source_release_test_pending{};
        bool source_release_build_verified{};
        bool source_release_test_verified{};
        bool source_headless_build_pending{};
        bool source_headless_test_pending{};
        bool source_headless_build_verified{};
        bool source_headless_test_verified{};
        bool source_full_validation_pending{};
        bool source_full_validation_verified{};
        bool source_promotion_staged{};
        bool source_promotion_completed{};
        std::size_t source_workspace_file_count{};
        std::uint64_t source_workspace_total_bytes{};
        std::size_t source_repair_attempts{};
        std::size_t model_reply_corrections{};
        bool source_diagnostic_recheck_queued{};
        std::string model_reply_correction_diagnostic{};
        std::string last_implementation_evidence_digest{};
        std::string status_message{
            "No guarded AI development proposal is active."};
        Domain active_domain{Domain::tooling};

        void reset_controller(
            std::string requestedWorkspace,
            std::string requestedSourceRoot,
            std::string requestedSandboxBaseRoot)
        {
            promotion_controller.reset();
            source_candidate_raw_reply.clear();
            source_candidate_operations.clear();
            source_build_verified = false;
            source_test_verified = false;
            source_release_build_pending = false;
            source_release_test_pending = false;
            source_release_build_verified = false;
            source_release_test_verified = false;
            source_headless_build_pending = false;
            source_headless_test_pending = false;
            source_headless_build_verified = false;
            source_headless_test_verified = false;
            source_full_validation_pending = false;
            source_full_validation_verified = false;
            source_promotion_staged = false;
            source_promotion_completed = false;
            const auto now = logical_time_now();
            ++generation;
            if (generation == 0u)
                generation = 1u;
            const std::uint64_t sessionValue = (now.value << 16u)
                ^ static_cast<std::uint64_t>(generation);
            workspace_id = requestedWorkspace.empty()
                ? std::string{"epoch.engine"}
                : std::move(requestedWorkspace);
            source_root = std::move(requestedSourceRoot);
            sandbox_base_root = std::move(requestedSandboxBaseRoot);
            workspace_root.clear();
            source_workspace_ready = false;
            source_workspace_pending = false;
            source_build_pending = false;
            source_test_pending = false;
            source_release_build_pending = false;
            source_release_test_pending = false;
            source_headless_build_pending = false;
            source_headless_test_pending = false;
            source_workspace_file_count = 0u;
            source_workspace_total_bytes = 0u;
            source_context_evidence.clear();
            source_baseline_evidence.clear();
            source_repair_attempts = 0u;
            source_context_evidence_objective.clear();
            model_reply_corrections = 0u;
            source_diagnostic_recheck_queued = false;
            model_reply_correction_diagnostic.clear();
            pending_source_context_paths.clear();
            pending_source_context_reason.clear();
            pending_source_context_objective.clear();
            if (!sandbox_base_root.empty())
            {
                const std::filesystem::path basePath =
                    std::filesystem::path{sandbox_base_root}.lexically_normal();
                if (basePath.is_absolute())
                {
                    const std::filesystem::path sandboxPath = basePath
                        / "cache/ai/iterations"
                        / ("session_" + std::to_string(
                            sessionValue == 0u ? 1u : sessionValue));
                    std::error_code error{};
                    std::filesystem::create_directories(
                        sandboxPath / "Engine", error);
                    if (!error)
                        std::filesystem::create_directories(
                            sandboxPath / "Projects", error);
                    if (!error)
                        std::filesystem::create_directories(
                            sandboxPath / "logs", error);
                    if (!error)
                        workspace_root = sandboxPath.generic_string();
                }
            }
            controller = std::make_unique<
                editor_ai_development::DevelopmentController>(
                    editor_ai_development::SessionConfiguration{
                        .session_value = sessionValue == 0u ? 1u : sessionValue,
                        .session_generation = generation,
                        .workspace_id = workspace_id,
                        .workspace_root = workspace_root,
                        .source_snapshot_root = source_root,
                        .engine_source_root = "Engine",
                        .project_source_root = "Projects",
                        .build_output_root = "Projects",
                        .evidence_root = "logs",
                        .operator_id = "epoch.operator",
                        .opened_at = now,
                        .expires_at = {now.value + 8u * 60u * 60u}
                    });
            status_message = controller->snapshot().status;
        }

        [[nodiscard]] editor_ai_development::DevelopmentController& ensure(
            std::string requestedWorkspace,
            std::string requestedSourceRoot,
            std::string requestedSandboxBaseRoot)
        {
            if (!controller || (!requestedWorkspace.empty()
                    && workspace_id != requestedWorkspace)
                || source_root != requestedSourceRoot
                || sandbox_base_root != requestedSandboxBaseRoot)
            {
                iteration_session.reset();
                reset_controller(
                    std::move(requestedWorkspace),
                    std::move(requestedSourceRoot),
                    std::move(requestedSandboxBaseRoot));
            }
            return *controller;
        }

        void clear_verified_source_candidate()
        {
            promotion_controller.reset();
            source_candidate_raw_reply.clear();
            source_candidate_operations.clear();
            source_build_verified = false;
            source_test_verified = false;
            source_release_build_pending = false;
            source_release_test_pending = false;
            source_release_build_verified = false;
            source_release_test_verified = false;
            source_promotion_staged = false;
            source_headless_build_pending = false;
            source_headless_test_pending = false;
            source_headless_build_verified = false;
            source_headless_test_verified = false;
            source_full_validation_pending = false;
            source_full_validation_verified = false;
            source_promotion_completed = false;
        }

        [[nodiscard]] editor_ai_development::PromotionCandidateResult
            verify_source_candidate() const
        {
            if (!source_build_verified || !source_test_verified
                || !source_release_build_verified
                || !source_release_test_verified
                || !source_headless_build_verified
                || !source_headless_test_verified
                || !source_full_validation_verified
                || source_candidate_raw_reply.empty()
                || source_candidate_operations.empty())
            {
                return {
                    editor_ai_development::PromotionCandidateCode::
                        invalid_operations,
                    "Debug/Release compiler contracts, HeadlessCI build/run, and explicit full-validation evidence are required before live promotion."};
            }
            return editor_ai_development::verify_source_promotion_candidate(
                source_root,
                workspace_root,
                source_candidate_operations);
        }

        [[nodiscard]] static std::string digest_text(
            const std::string_view text)
        {
            return core::sha256::hex(core::sha256::hash(text));
        }

        [[nodiscard]] static std::string_view campaign_phase_name(
            const ai::self_iteration_orchestrator::Phase phase) noexcept
        {
            using Phase = ai::self_iteration_orchestrator::Phase;
            switch (phase)
            {
            case Phase::idle: return "Idle";
            case Phase::awaiting_plan_request: return "Awaiting plan request";
            case Phase::awaiting_plan_result: return "Awaiting plan result";
            case Phase::awaiting_curated_evidence: return "Awaiting curated evidence";
            case Phase::awaiting_proposal_request: return "Awaiting proposal request";
            case Phase::awaiting_proposal_result: return "Awaiting proposal result";
            case Phase::awaiting_manual_review: return "Awaiting manual review";
            case Phase::awaiting_apply_decision: return "Awaiting sandbox apply decision";
            case Phase::awaiting_apply_result: return "Awaiting sandbox apply result";
            case Phase::awaiting_validation_request: return "Awaiting validation request";
            case Phase::awaiting_validation_result: return "Awaiting validation result";
            case Phase::checkpoint_ready: return "Checkpoint ready";
            case Phase::checkpointed: return "Checkpointed";
            case Phase::rejected: return "Rejected";
            case Phase::cancelled: return "Cancelled";
            case Phase::blocked: return "Blocked";
            }
            return "Unknown";
        }

        [[nodiscard]] static std::string_view campaign_provider_name(
            const ai::project_profile::Provider provider) noexcept
        {
            return provider == ai::project_profile::Provider::external_mcp
                ? "External MCP" : provider
                    == ai::project_profile::Provider::epoch_local_qwen38
                    ? "Epoch-local Qwen3.8" : "Disabled";
        }

        [[nodiscard]] ai::self_iteration_orchestrator::ActionToken
            campaign_action(
                const ai::self_iteration_orchestrator::Snapshot& snapshot,
                std::string_view verb,
                const std::uint64_t now,
                const bool approved = true)
        {
            ++campaign_transition_generation;
            if (campaign_transition_generation == 0u)
                campaign_transition_generation = 1u;
            return ai::self_iteration_orchestrator::ActionToken{
                .expected_generation = snapshot.generation,
                .expected_state_sha256 = snapshot.state_sha256,
                .transition_id = std::string{"editor-"} + std::string{verb}
                    + "-" + std::to_string(campaign_transition_generation),
                .now_unix_seconds = now,
                .operator_approved = approved};
        }

        [[nodiscard]] static ai::self_iteration_orchestrator::OperationReceipt
            campaign_receipt(
                const ai::self_iteration_orchestrator::PendingOperation& operation,
                const std::uint64_t now)
        {
            return {
                .operation_id = operation.operation_id(),
                .expected_generation = operation.expected_generation(),
                .expected_state_sha256 = operation.expected_state_sha256(),
                .transition_id = operation.transition_id(),
                .now_unix_seconds = now};
        }

        void capture_campaign_result(
            RenderResult& output,
            ai::self_iteration_orchestrator::Result result)
        {
            status_message = result.status;
            if (result.pending_operation)
                campaign_pending_operation = result.pending_operation;
            else if (result.snapshot.phase
                    != ai::self_iteration_orchestrator::Phase::awaiting_plan_result
                && result.snapshot.phase
                    != ai::self_iteration_orchestrator::Phase::awaiting_proposal_result
                && result.snapshot.phase
                    != ai::self_iteration_orchestrator::Phase::awaiting_apply_result
                && result.snapshot.phase
                    != ai::self_iteration_orchestrator::Phase::awaiting_validation_result)
            {
                campaign_pending_operation.reset();
            }
            if (!result.state_path.empty())
                campaign_state_path = result.state_path;
            const std::string digest = result.snapshot.state_sha256.empty()
                ? std::string{"unavailable"}
                : result.snapshot.state_sha256.substr(0u, 16u) + "...";
            output.campaign_evidence.push_back(epochengine::format_text(
                "Self-iteration | {} | generation {} | state {} | {}",
                campaign_phase_name(result.snapshot.phase),
                result.snapshot.generation,
                digest,
                result.status));
            output.status = status_message;
        }

        [[nodiscard]] std::optional<
            ai::self_iteration_orchestrator::Configuration>
            prepare_campaign_configuration(
                const Input& input,
                const std::uint64_t now,
                std::string& refusal)
        {
            using namespace ai::iteration_session;
            if (input.domain != Domain::engine_source
                || development_objective.empty()
                || !input.source_authority_verified
                || input.curated_source_paths.empty()
                || input.curated_scope_digest.size() != 64u)
            {
                refusal = "Start requires one bounded objective, verified Engine authority, and an exact shared curated scope digest.";
                return std::nullopt;
            }
            std::error_code error{};
            const auto root = std::filesystem::weakly_canonical(
                std::filesystem::path{input.source_snapshot_root}, error);
            if (error || !root.is_absolute())
            {
                refusal = "The verified Engine source root is unavailable.";
                return std::nullopt;
            }
            SourceAuthority authority{
                .target_kind = IterationTargetKind::engine_source,
                .kind = input.source_authority_kind == "explicit_checkout"
                    ? SourceAuthorityKind::explicit_checkout
                    : input.source_authority_kind == "verified_cache"
                        ? SourceAuthorityKind::verified_cache
                        : SourceAuthorityKind::unavailable,
                .root = root,
                .source_version = input.source_authority_version,
                .commit = input.source_authority_commit,
                .receipt_digest = input.source_authority_receipt_digest,
                .verified = input.source_authority_verified};
            const auto inspected = inspect_curated_files(
                authority, input.curated_source_paths);
            if (!inspected.accepted)
            {
                refusal = inspected.status;
                return std::nullopt;
            }
            std::filesystem::path cacheRoot{input.source_cache_root};
            if (cacheRoot.empty())
                cacheRoot = std::filesystem::path{input.workspace_root}
                    / "cache/ai";
            cacheRoot = std::filesystem::absolute(cacheRoot, error)
                .lexically_normal();
            if (error || !cacheRoot.is_absolute())
            {
                refusal = "The self-iteration cache root is unavailable.";
                return std::nullopt;
            }
            const std::string selectedBinding = input.selected_model.empty()
                ? (campaign_provider == ai::project_profile::Provider::external_mcp
                    ? std::string{"operator-external-mcp-model"}
                    : std::string{"qwen3.8"})
                : input.selected_model;
            const std::string hostFingerprint = epochengine::format_text(
                "{}\n{}\n{}",
                static_cast<unsigned>(campaign_provider),
                selectedBinding,
                input.selected_endpoint);
            campaign_scope_digest = input.curated_scope_digest;
            return ai::self_iteration_orchestrator::Configuration{
                .authority = std::move(authority),
                .curated_files = inspected.files,
                .cache_root = std::move(cacheRoot),
                .objective = development_objective,
                .provider = campaign_provider,
                .engine_model_binding = selectedBinding,
                .operator_model_binding = selectedBinding,
                .host = {
                    .binding_id = campaign_provider
                            == ai::project_profile::Provider::external_mcp
                        ? "operator-external-mcp" : "epoch-local-qwen38",
                    .configuration_sha256 = digest_text(hostFingerprint),
                    .generation = 1u,
                    .stdio_only = true},
                .budgets = ai::iteration_campaign::default_budgets(
                    IterationTargetKind::engine_source),
                .created_at_unix_seconds = now,
                .engine_source_campaign_permitted = true,
                .sandbox_apply_permitted = true};
        }

        [[nodiscard]] std::string campaign_model_prompt(
            const ai::self_iteration_orchestrator::OperationKind kind) const
        {
            const auto snapshot = campaign_orchestrator
                ? campaign_orchestrator->snapshot()
                : ai::self_iteration_orchestrator::Snapshot{};
            if (kind == ai::self_iteration_orchestrator::OperationKind::model_plan)
            {
                return "EPOCH_SELF_ITERATION_PLAN_V1\nOBJECTIVE\n"
                    + development_objective
                    + "\nCURATED_SCOPE_SHA256\n" + campaign_scope_digest
                    + "\nReturn one bounded evidence-driven plan only. Do not claim edits, builds, approval, Git, release, or live-source authority.";
            }
            return "EPOCH_SELF_ITERATION_PROPOSAL_V1\nOBJECTIVE\n"
                + development_objective
                + "\nCAMPAIGN_SCOPE_SHA256\n"
                + snapshot.campaign.session.scope_digest
                + "\nCURATED_SCOPE_SHA256\n" + campaign_scope_digest
                + "\nReturn one exact EPOCH_SOURCE_PATCH_PROPOSAL_V1 packet for the reviewed files only. No approval, promotion, Git, release, or unrestricted execution.";
        }

        [[nodiscard]] bool request_next_campaign_validation(
            RenderResult& output,
            const std::uint64_t now)
        {
            if (!campaign_orchestrator) return false;
            const auto snapshot = campaign_orchestrator->snapshot();
            if (snapshot.phase
                != ai::self_iteration_orchestrator::Phase::awaiting_validation_request)
                return false;
            auto requested = campaign_orchestrator->request_validation(
                campaign_action(snapshot, "validation-request", now));
            const bool accepted = static_cast<bool>(requested);
            capture_campaign_result(output, std::move(requested));
            return accepted;
        }

        void render_typed_campaign(
            const Input& input,
            const float width,
            RenderResult& output)
        {
            using namespace ai::self_iteration_orchestrator;
            const std::uint64_t now = logical_time_now().value;
            Snapshot snapshot = campaign_orchestrator
                ? campaign_orchestrator->snapshot() : Snapshot{};
            const bool terminal = snapshot.phase == Phase::checkpointed
                || snapshot.phase == Phase::rejected
                || snapshot.phase == Phase::cancelled
                || snapshot.phase == Phase::blocked;

            gui::label("Typed Self-Iteration Campaign");
            gui::property_row(
                "Provider", campaign_provider_name(campaign_provider));
            const std::array providerActions{
                gui::InlineButtonSpec{
                    .label = "Local Qwen3.8",
                    .width = 132.0f,
                    .enabled = !campaign_orchestrator || terminal},
                gui::InlineButtonSpec{
                    .label = "External MCP",
                    .width = 120.0f,
                    .enabled = !campaign_orchestrator || terminal}}
            ;
            if (const auto provider = gui::inline_button_row(
                    providerActions, 29.0f, 5.0f))
            {
                campaign_provider = *provider == 0u
                    ? ai::project_profile::Provider::epoch_local_qwen38
                    : ai::project_profile::Provider::external_mcp;
                status_message = std::string{"Selected "}
                    + std::string{campaign_provider_name(campaign_provider)}
                    + "; no campaign started and no source bytes sent.";
                output.campaign_evidence.push_back(status_message);
            }

            const bool canStart = (!campaign_orchestrator || terminal)
                && !development_objective.empty()
                && input.source_authority_verified
                && !input.curated_source_paths.empty()
                && input.curated_scope_digest.size() == 64u;
            const bool canResume = !campaign_state_path.empty();
            const bool canCancel = campaign_orchestrator && !terminal
                && snapshot.phase != Phase::idle;
            const std::array lifecycleActions{
                gui::InlineButtonSpec{
                    .label = "Start", .width = 76.0f, .enabled = canStart},
                gui::InlineButtonSpec{
                    .label = "Resume", .width = 82.0f, .enabled = canResume},
                gui::InlineButtonSpec{
                    .label = "Cancel", .width = 82.0f, .enabled = canCancel}}
            ;
            if (const auto action = gui::inline_button_row(
                    lifecycleActions, 30.0f, 5.0f))
            {
                if (*action == 0u)
                {
                    std::string refusal{};
                    const auto configuration = prepare_campaign_configuration(
                        input, now, refusal);
                    if (!configuration)
                    {
                        status_message = std::move(refusal);
                        output.campaign_evidence.push_back(status_message);
                    }
                    else
                    {
                        campaign_configuration = *configuration;
                        campaign_orchestrator = std::make_unique<Orchestrator>();
                        campaign_pending_operation.reset();
                        capture_campaign_result(
                            output,
                            campaign_orchestrator->begin(
                                campaign_configuration));
                    }
                }
                else if (*action == 1u)
                {
                    std::string refusal{};
                    const auto configuration = prepare_campaign_configuration(
                        input, now, refusal);
                    if (!configuration || campaign_state_path.empty())
                    {
                        status_message = configuration
                            ? "No saved typed campaign state is available."
                            : std::move(refusal);
                        output.campaign_evidence.push_back(status_message);
                    }
                    else
                    {
                        campaign_configuration = *configuration;
                        campaign_orchestrator = std::make_unique<Orchestrator>();
                        campaign_pending_operation.reset();
                        capture_campaign_result(
                            output,
                            campaign_orchestrator->resume(
                                campaign_configuration,
                                campaign_state_path,
                                now));
                    }
                }
                else if (campaign_orchestrator)
                {
                    capture_campaign_result(
                        output,
                        campaign_orchestrator->cancel(
                            campaign_action(snapshot, "cancel", now),
                            "Operator cancelled the typed self-iteration campaign."));
                }
                snapshot = campaign_orchestrator
                    ? campaign_orchestrator->snapshot() : Snapshot{};
            }

            gui::property_row("Phase", campaign_phase_name(snapshot.phase));
            gui::property_row(
                "Objective",
                development_objective.empty()
                    ? std::string{"Enter a bounded objective above"}
                    : development_objective);
            gui::property_row(
                "Curated scope",
                campaign_scope_digest.empty()
                    ? std::string{"Not admitted"}
                    : campaign_scope_digest);
            gui::property_row(
                "Profile",
                snapshot.profile_sha256.empty()
                    ? std::string{"Not configured"}
                    : snapshot.profile_sha256);
            gui::property_row(
                "Checkpoint",
                campaign_state_path.empty()
                    ? std::string{"Not written"}
                    : campaign_state_path.generic_string());
            gui::wrapped_label(
                "Every model, apply, validation, and checkpoint transition is digest-bound. Live source stays read-only; this controller cannot approve promotion, Git, release, listeners, servers, or unrestricted execution.",
                width);

            if (!campaign_orchestrator)
            {
                gui::wrapped_label(
                    "Share exact curated context, choose a provider, then Start. Authoring and project build/output remain separate.",
                    width);
                return;
            }

            const auto requestModel = [&](const OperationKind kind,
                                          Result requested)
            {
                const bool accepted = static_cast<bool>(requested);
                capture_campaign_result(output, std::move(requested));
                if (accepted)
                {
                    output.action = HostAction::request_model_source_proposal;
                    output.model_prompt = campaign_model_prompt(kind);
                }
            };

            if (snapshot.phase == Phase::awaiting_plan_request
                && gui::button("Request Bounded Plan", {width, 30.0f}))
            {
                requestModel(OperationKind::model_plan,
                    campaign_orchestrator->request_plan(
                        campaign_action(snapshot, "plan-request", now)));
            }
            else if (snapshot.phase == Phase::awaiting_curated_evidence
                && gui::button("Share Exact Curated Evidence", {width, 30.0f}))
            {
                capture_campaign_result(output,
                    campaign_orchestrator->share_curated_evidence(
                        campaign_action(snapshot, "share-curated", now),
                        snapshot.campaign.session.scope_digest,
                        campaign_scope_digest,
                        "Operator shared only the exact reviewed scope digest."));
            }
            else if (snapshot.phase == Phase::awaiting_proposal_request
                && gui::button("Request Digest-Bound Proposal", {width, 30.0f}))
            {
                requestModel(OperationKind::model_proposal,
                    campaign_orchestrator->request_proposal(
                        campaign_action(snapshot, "proposal-request", now)));
            }
            else if (snapshot.phase == Phase::awaiting_manual_review)
            {
                const std::array decisions{
                    gui::InlineButtonSpec{.label = "Approve Proposal", .width = 152.0f},
                    gui::InlineButtonSpec{.label = "Reject", .width = 82.0f}}
                ;
                if (const auto decision = gui::inline_button_row(
                        decisions, 30.0f, 5.0f))
                {
                    capture_campaign_result(output,
                        campaign_orchestrator->review_proposal(
                            campaign_action(snapshot, "proposal-review", now),
                            *decision == 0u,
                            *decision == 0u
                                ? "Operator reviewed and approved the exact proposal digest."
                                : "Operator rejected the proposal without applying it."));
                }
            }
            else if (snapshot.phase == Phase::awaiting_apply_decision)
            {
                const std::array decisions{
                    gui::InlineButtonSpec{.label = "Apply In Sandbox", .width = 154.0f},
                    gui::InlineButtonSpec{.label = "Reject", .width = 82.0f}}
                ;
                if (const auto decision = gui::inline_button_row(
                        decisions, 30.0f, 5.0f))
                {
                    auto decided = campaign_orchestrator->decide_apply(
                        campaign_action(snapshot, "sandbox-apply", now),
                        *decision == 0u,
                        *decision == 0u
                            ? "Operator approved disposable-workspace application only."
                            : "Operator rejected sandbox application.");
                    const bool accepted = static_cast<bool>(decided);
                    capture_campaign_result(output, std::move(decided));
                    if (accepted && *decision == 0u && controller)
                    {
                        auto phase = controller->snapshot().phase;
                        if (phase == editor_ai_development::ControllerPhase::proposed)
                        {
                            (void)controller->review(
                                "epoch.operator",
                                "Operator reviewed the exact typed campaign proposal.",
                                logical_time_now());
                            phase = controller->snapshot().phase;
                        }
                        if (phase == editor_ai_development::ControllerPhase::reviewed)
                        {
                            (void)controller->approve(
                                "epoch.operator",
                                "Operator approved sandbox execution only.",
                                logical_time_now(),
                                {now + 10u * 60u});
                            phase = controller->snapshot().phase;
                        }
                        if (phase == editor_ai_development::ControllerPhase::approved)
                        {
                            (void)controller->authorize(
                                logical_time_now(),
                                editor_ai_development::Duration{5u * 60u});
                            phase = controller->snapshot().phase;
                        }
                        if (phase == editor_ai_development::ControllerPhase::authorized)
                        {
                            RenderResult host = execute_source_and_queue_build(
                                logical_time_now());
                            output.action = host.action;
                            output.source_root = std::move(host.source_root);
                            output.workspace_root = std::move(host.workspace_root);
                            output.workspace_generation = host.workspace_generation;
                            if (host.action == HostAction::compile_source_workspace
                                && campaign_pending_operation)
                            {
                                auto applied = campaign_orchestrator->record_apply(
                                    campaign_receipt(
                                        *campaign_pending_operation, now),
                                    last_implementation_evidence_digest,
                                    "Disposable workspace transaction committed with host evidence.");
                                if (applied)
                                {
                                    capture_campaign_result(output, std::move(applied));
                                    (void)request_next_campaign_validation(
                                        output, now);
                                }
                                else
                                {
                                    capture_campaign_result(output, std::move(applied));
                                }
                            }
                        }
                    }
                }
            }
            else if (snapshot.phase == Phase::checkpoint_ready
                && gui::button("Record Rollback Checkpoint", {width, 30.0f}))
            {
                capture_campaign_result(output,
                    campaign_orchestrator->checkpoint(
                        campaign_action(snapshot, "checkpoint", now),
                        "Operator recorded the fully validated candidate as a rollback checkpoint; live promotion remains separate."));
            }

            static constexpr std::array<std::string_view, 7u> stages{
                "Debug compiler", "Debug contract", "Release compiler",
                "Release contract", "Headless compiler", "Headless contract",
                "Full validation"};
            const auto current = campaign_orchestrator->snapshot();
            gui::label("Validation Evidence");
            for (std::size_t index = 0u; index < stages.size(); ++index)
            {
                gui::property_row(
                    stages[index],
                    index < current.validation_index ? "Passed"
                    : index == current.validation_index
                        && (current.phase == Phase::awaiting_validation_request
                            || current.phase == Phase::awaiting_validation_result)
                        ? "Active" : "Pending",
                    126.0f);
            }
            if (!current.evidence.empty())
            {
                gui::label("Checkpoint / Error Evidence");
                const std::size_t begin = current.evidence.size() > 8u
                    ? current.evidence.size() - 8u : 0u;
                for (std::size_t index = begin;
                     index < current.evidence.size(); ++index)
                {
                    const auto& record = current.evidence[index];
                    gui::wrapped_label(
                        epochengine::format_text(
                            "#{} {} | {} | {}",
                            record.sequence,
                            record.passed ? "pass" : "fail",
                            record.evidence_sha256,
                            record.summary),
                        width);
                }
            }
            gui::wrapped_label(status_message, width);
        }

        [[nodiscard]] bool record_iteration_validation(
            const ai::iteration_session::ValidationActor actor,
            const bool succeeded,
            const std::string_view summary,
            const bool sequenceComplete)
        {
            if (!iteration_session)
                return true;
            const auto candidate = iteration_session->report().candidate_digest;
            const auto recorded = iteration_session->record_validation(
                iteration_session->identity(),
                ai::iteration_session::ValidationEvidence{
                    .actor = actor,
                    .candidate_digest = candidate,
                    .evidence_digest = digest_text(summary),
                    .summary = std::string{summary},
                    .passed = succeeded},
                sequenceComplete);
            if (!recorded)
                status_message = recorded.status;
            return static_cast<bool>(recorded);
        }

        [[nodiscard]] RenderResult source_context_model_request() const
        {
            RenderResult output{};
            const auto area = active_domain == Domain::engine_source
                ? ai::development_proposal_codec::SourceArea::engine
                : ai::development_proposal_codec::SourceArea::project;
            output.action = HostAction::request_model_source_proposal;
            output.model_prompt =
                ai::development_proposal_codec::context_request_prompt(
                    area,
                    development_objective,
                    source_context_evidence);
            output.status = status_message;
            return output;
        }

        [[nodiscard]] RenderResult source_model_request() const
        {
            RenderResult output{};
            const auto area = active_domain == Domain::engine_source
                ? ai::development_proposal_codec::SourceArea::engine
                : ai::development_proposal_codec::SourceArea::project;
            output.action = HostAction::request_model_source_proposal;
            output.model_prompt =
                ai::development_proposal_codec::protocol_prompt(
                    area,
                    development_objective,
                    source_context_evidence);
            if (!model_reply_correction_diagnostic.empty())
            {
                output.model_prompt +=
                    "\n\nEPOCH_SOURCE_PROTOCOL_CORRECTION_V1\n"
                    "The previous reply was rejected before any source bytes "
                    "were staged. Return a fresh complete proposal, not a patch "
                    "to the previous reply.\nCORRECTION_ATTEMPT ";
                output.model_prompt +=
                    std::to_string(model_reply_corrections);
                output.model_prompt += " OF ";
                output.model_prompt +=
                    std::to_string(maximum_model_reply_corrections);
                output.model_prompt += "\nDIAGNOSTIC ";
                output.model_prompt += model_reply_correction_diagnostic;
                output.model_prompt += "\nEND_EPOCH_SOURCE_PROTOCOL_CORRECTION_V1";
            }
            output.status = status_message;
            return output;
        }

        [[nodiscard]] RenderResult execute_source_and_queue_build(
            editor_ai_development::LogicalTime now)
        {
            RenderResult output{};
            if (!controller)
            {
                status_message =
                    "The guarded source controller is unavailable.";
                output.status = status_message;
                return output;
            }
            if (source_build_pending)
            {
                status_message =
                    "The host compiler is already evaluating this sandbox generation.";
                output.status = status_message;
                return output;
            }

            source_build_verified = false;
            source_test_verified = false;
            source_release_build_pending = false;
            source_release_test_pending = false;
            source_release_build_verified = false;
            source_release_test_verified = false;
            source_promotion_staged = false;
            source_headless_build_pending = false;
            source_headless_test_pending = false;
            source_headless_build_verified = false;
            source_headless_test_verified = false;
            source_full_validation_pending = false;
            source_full_validation_verified = false;
            source_promotion_completed = false;
            promotion_controller.reset();
            const auto report =
                controller->execute_authorized_model_source_changes(now);
            status_message = report.transaction_status.empty()
                ? report.controller_result.status
                : report.controller_result.status + " "
                    + report.transaction_status;
            if (report)
            {
                last_implementation_evidence_digest =
                    digest_text(report.evidence_manifest);
                if (iteration_session)
                {
                    const auto recorded = iteration_session->record_implementation(
                        iteration_session->identity(),
                        digest_text(report.evidence_manifest),
                        report.transaction_status.empty()
                            ? std::string{"Sandbox source transaction committed."}
                            : report.transaction_status);
                    if (!recorded)
                    {
                        status_message = recorded.status;
                        output.status = status_message;
                        return output;
                    }
                }
                source_build_pending = true;
                output.action = HostAction::compile_source_workspace;
                output.source_root = source_root;
                output.workspace_root = workspace_root;
                output.workspace_generation = generation;
                status_message +=
                    " Exact proposal bytes are committed only in the disposable workspace; the host compiler is queued.";
            }
            output.status = status_message;
            return output;
        }

        [[nodiscard]] RenderResult queue_repair_after_verified_failure(
            std::string failureEvidence,
            std::string_view gateName)
        {
            RenderResult output{};
            status_message = failureEvidence
                + " The failed candidate cannot advance or modify live source.";
            if (active_domain == Domain::tooling
                || source_baseline_evidence.empty())
            {
                output.status = status_message;
                return output;
            }
            if (source_repair_attempts
                >= maximum_source_repair_attempts)
            {
                status_message +=
                    " The bounded repair limit is exhausted; operator refinement is required.";
                output.status = status_message;
                return output;
            }

            const std::size_t nextAttempt = source_repair_attempts + 1u;
            const std::string preservedWorkspaceId = workspace_id;
            const std::string preservedSourceRoot = source_root;
            const std::string preservedSandboxBase = sandbox_base_root;
            const std::string preservedObjective = development_objective;
            const std::string preservedBaseline = source_baseline_evidence;
            const std::string preservedEvidenceObjective =
                source_context_evidence_objective;
            const Domain preservedDomain = active_domain;

            reset_controller(
                preservedWorkspaceId,
                preservedSourceRoot,
                preservedSandboxBase);
            development_objective = preservedObjective;
            source_baseline_evidence = preservedBaseline;
            source_context_evidence = preservedBaseline;
            source_context_evidence_objective =
                preservedEvidenceObjective;
            active_domain = preservedDomain;
            source_repair_attempts = nextAttempt;

            constexpr std::size_t maximumFailureEvidenceBytes =
                32u * 1024u;
            if (failureEvidence.size() > maximumFailureEvidenceBytes)
            {
                failureEvidence.erase(
                    0u,
                    failureEvidence.size() - maximumFailureEvidenceBytes);
            }
            source_context_evidence +=
                "\nVERIFIED_HOST_REPAIR_CONTEXT_V1\nGATE ";
            source_context_evidence += gateName;
            source_context_evidence += "\nREPAIR_ATTEMPT ";
            source_context_evidence += std::to_string(nextAttempt);
            source_context_evidence += " OF ";
            source_context_evidence +=
                std::to_string(maximum_source_repair_attempts);
            source_context_evidence += "\n";
            source_context_evidence += failureEvidence;
            source_context_evidence +=
                "\nEND_VERIFIED_HOST_REPAIR_CONTEXT_V1\n";

            if (workspace_root.empty())
            {
                status_message =
                    "A fresh disposable workspace could not be allocated for the bounded repair attempt.";
                output.status = status_message;
                return output;
            }

            source_workspace_pending = true;
            output.action = HostAction::materialize_source_workspace;
            output.source_root = source_root;
            output.workspace_root = workspace_root;
            output.workspace_generation = generation;
            output.include_paths =
                active_domain == Domain::engine_source
                ? std::vector<std::string>{"Engine.sln", "Engine"}
                : std::vector<std::string>{"Projects"};
            output.excluded_components = {
                ".git",
                ".vs",
                "Bin",
                "Debug",
                "Release",
                "build",
                "built",
                "cache",
                "logs",
                "vcpkg_installed",
                "x64"};
            status_message = epochengine::format_text(
                "Verified {} failure opened bounded repair attempt {}/{}; a fresh disposable workspace is queued before the selected local model receives diagnostics.",
                gateName,
                nextAttempt,
                maximum_source_repair_attempts);
            output.status = status_message;
            return output;
        }

        [[nodiscard]] RenderResult queue_model_reply_correction(
            std::string rejection)
        {
            RenderResult output{};
            status_message = std::move(rejection);
            if (active_domain == Domain::tooling
                || source_context_evidence.empty())
            {
                output.status = status_message;
                return output;
            }
            if (model_reply_corrections
                >= maximum_model_reply_corrections)
            {
                status_message +=
                    " The bounded model-packet correction limit is exhausted; "
                    "operator refinement is required.";
                output.status = status_message;
                return output;
            }

            constexpr std::size_t maximumDiagnosticBytes = 2u * 1024u;
            if (status_message.size() > maximumDiagnosticBytes)
                status_message.resize(maximumDiagnosticBytes);
            ++model_reply_corrections;
            model_reply_correction_diagnostic = status_message;
            status_message = epochengine::format_text(
                "Qwen source packet rejected before staging; correction "
                "attempt {}/{} is queued with the deterministic host diagnostic.",
                model_reply_corrections,
                maximum_model_reply_corrections);
            output = source_model_request();
            output.status = status_message;
            return output;
        }

        [[nodiscard]] RenderResult stage_source_reply(
            const Input& input,
            std::string_view reply,
            editor_ai_development::LogicalTime now)
        {
            RenderResult output{};
            if (input.domain == Domain::tooling)
            {
                status_message =
                    "Tool-harness replies do not use the source proposal codec.";
                output.status = status_message;
                return output;
            }
            if (reply.empty())
            {
                status_message =
                    "No local-model source reply is available to validate.";
                output.status = status_message;
                return output;
            }
            if (development_objective.empty())
                development_objective = input.development_objective;
            if (development_objective.empty())
            {
                status_message =
                    "Enter one bounded development objective before requesting source work.";
                output.status = status_message;
                return output;
            }

            const bool engineSource = input.domain == Domain::engine_source;
            const auto area = engineSource
                ? ai::development_proposal_codec::SourceArea::engine
                : ai::development_proposal_codec::SourceArea::project;
            const auto contextRequest =
                ai::development_proposal_codec::decode_context_request(
                    reply, area);
            if (contextRequest.recognized)
            {
                pending_source_context_paths.clear();
                pending_source_context_reason.clear();
                pending_source_context_objective.clear();
                if (!contextRequest)
                {
                    status_message =
                        "Model source-context request rejected: "
                        + contextRequest.status
                        + " No source bytes were read or shared.";
                    output.status = status_message;
                    return output;
                }
                pending_source_context_paths = contextRequest.request.paths;
                pending_source_context_reason =
                    "Model request: " + contextRequest.request.reason;
                pending_source_context_objective = development_objective;
                source_context_evidence = input.architecture_evidence;
                source_context_evidence_objective = development_objective;
                active_domain = input.domain;
                status_message =
                    epochengine::format_text(
                        "Qwen requested {} bounded source path(s). Review the "
                        "validated list before Share reads or sends any file bytes.",
                        pending_source_context_paths.size());
                output.status = status_message;
                return output;
            }
            if (reply == "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1")
            {
                if (const auto target =
                        unique_explicit_replacement_target(
                            development_objective,
                            source_context_evidence))
                {
                    return queue_model_reply_correction(
                        "The model declared insufficient evidence, but the "
                        "first quoted replacement target occurs exactly once "
                        "in the shared source evidence: " + *target
                        + ". Re-evaluate that exact block.");
                }
                if (!source_diagnostic_recheck_queued
                    && bounded_diagnostic_objective(development_objective))
                {
                    source_diagnostic_recheck_queued = true;
                    return queue_model_reply_correction(
                        "The operator requested one source-proven defect in a "
                        "named subsystem. Re-inspect the supplied exact source "
                        "and select one concrete bug only when its cause, the "
                        "violated nearby invariant, and a bounded repair are "
                        "all proven by those bytes. A pre-named symbol is not "
                        "required for this diagnostic objective; return the "
                        "insufficient-evidence marker again if no such defect "
                        "is present.");
                }
                status_message =
                    !source_diagnostic_recheck_queued
                    ? "Qwen could not justify a bounded source step from the supplied evidence. No source was staged; refine the objective or request again after architecture evidence improves."
                    : "Qwen could not prove a concrete defect in the shared reviewed source after one diagnostic recheck. No source was staged; name a behavior or symbol and request again.";
                output.status = status_message;
                return output;
            }

            const std::string_view evidence = source_context_evidence.empty()
                ? std::string_view{input.architecture_evidence}
                : std::string_view{source_context_evidence};
            const auto decoded =
                ai::development_proposal_codec::decode(reply);
            if (!decoded)
            {
                return queue_model_reply_correction(decoded.status);
            }
            const GroundingResult grounding = validate_grounded_proposal(
                decoded.proposal,
                development_objective,
                evidence,
                input.source_snapshot_root);
            if (!grounding.accepted)
            {
                return queue_model_reply_correction(grounding.status);
            }
            const auto quality =
                ai::development_proposal_codec::validate_quality(
                    decoded.proposal,
                    development_objective,
                    evidence);
            if (!quality)
            {
                return queue_model_reply_correction(quality.status);
            }

            const auto result = controller->propose_model_reply(
                reply,
                engineSource
                    ? editor_ai_development::OperationKind::engine_source_edit
                    : editor_ai_development::OperationKind::project_source_edit,
                now);
            status_message = result.status;
            if (result)
            {
                if (iteration_session)
                {
                    const auto staged = iteration_session->stage_candidate(
                        iteration_session->identity(), reply);
                    if (!staged)
                    {
                        status_message = staged.status;
                        output.status = status_message;
                        return output;
                    }
                }
                active_domain = input.domain;
                const auto snapshot = controller->snapshot();
                source_candidate_raw_reply.assign(reply);
                source_candidate_operations = snapshot.operations;
                source_candidate_kind = engineSource
                    ? editor_ai_development::OperationKind::engine_source_edit
                    : editor_ai_development::OperationKind::project_source_edit;
                source_build_verified = false;
                source_test_verified = false;
                source_release_build_pending = false;
                model_reply_corrections = 0u;
                source_diagnostic_recheck_queued = false;
                model_reply_correction_diagnostic.clear();
                source_release_test_pending = false;
                source_release_build_verified = false;
                source_release_test_verified = false;
                source_promotion_staged = false;
                source_headless_build_pending = false;
                source_headless_test_pending = false;
                source_headless_build_verified = false;
                source_headless_test_verified = false;
                source_full_validation_pending = false;
                source_full_validation_verified = false;
                source_promotion_completed = false;
                promotion_controller.reset();
            }
            output.status = status_message;
            return output;
        }
    };
    Panel::Panel()
        : implementation_(std::make_unique<Implementation>())
    {
    }

    Panel::~Panel() = default;
    Panel::Panel(Panel&&) noexcept = default;
    Panel& Panel::operator=(Panel&&) noexcept = default;

    bool Panel::run_contract()
    {
        Panel accepted{};
        Panel contextSelection{};
        auto& contextState = *contextSelection.implementation_;
        contextState.development_objective = "fix bugs";
        contextState.source_context_evidence =
            "PATH Engine/src/editor/editor.application.cpp\n"
            "PATH Engine/src/editor/ai/editor.ai_development_panel.cpp\n";
        contextState.active_domain = Domain::engine_source;
        const RenderResult contextRequest =
            contextState.source_context_model_request();
        if (contextRequest.action
                != HostAction::request_model_source_proposal
            || contextRequest.model_prompt.find(
                "EPOCH_SOURCE_CONTEXT_REQUEST_V1") == std::string::npos
            || contextRequest.model_prompt.find("fix bugs")
                == std::string::npos)
        {
            return false;
        }

        Input contextInput{};
        contextInput.domain = Domain::engine_source;
        contextInput.development_objective = "fix bugs";
        contextInput.architecture_evidence =
            contextState.source_context_evidence;
        const RenderResult stagedContext = contextState.stage_source_reply(
            contextInput,
            "EPOCH_SOURCE_CONTEXT_REQUEST_V1\n"
            "reason: Inspect the editor AI request handoff\n"
            "path_count: 2\n"
            "path: Engine/src/editor/editor.application.cpp\n"
            "path: Engine/src/editor/ai/editor.ai_development_panel.cpp\n"
            "end_request\n",
            editor_ai_development::LogicalTime{1u});
        if (stagedContext.action != HostAction::none
            || contextState.pending_source_context_paths.size() != 2u
            || contextState.pending_source_context_objective != "fix bugs")
        {
            return false;
        }
        const RenderResult rejectedContext = contextState.stage_source_reply(
            contextInput,
            "EPOCH_SOURCE_CONTEXT_REQUEST_V1\n"
            "reason: Escape the source root\n"
            "path_count: 1\n"
            "path: Engine/../secret.cpp\n"
            "end_request\n",
            editor_ai_development::LogicalTime{2u});
        if (rejectedContext.action != HostAction::none
            || !contextState.pending_source_context_paths.empty()
            || rejectedContext.status.find("rejected") == std::string::npos)
        {
            return false;
        }

        {
            struct ScopedContractRoot final
            {
                std::filesystem::path path{};
                ~ScopedContractRoot()
                {
                    std::error_code cleanupError{};
                    std::filesystem::remove_all(path, cleanupError);
                }
            } fixture{
                std::filesystem::temp_directory_path()
                    / ("epoch_ai_share_context_"
                        + std::to_string(
                            std::chrono::steady_clock::now()
                                .time_since_epoch().count()))};

            const std::string reviewedPath =
                "Engine/src/editor/reviewed.cpp";
            std::error_code fixtureError{};
            std::filesystem::create_directories(
                fixture.path / "Engine/src/editor",
                fixtureError);
            if (fixtureError)
                return false;

            {
                std::ofstream reviewedSource{
                    fixture.path / reviewedPath,
                    std::ios::binary | std::ios::trunc};
                reviewedSource
                    << "namespace epochengine::reviewed { int value = 1; }\n";
                if (!reviewedSource.good())
                    return false;
            }

            Panel localOpen{};
            Input localOpenInput{};
            localOpenInput.domain = Domain::engine_source;
            localOpenInput.workspace_id = "epoch.contract.local-open";

            const auto writeFixtureSource = [&](
                const std::string_view relativePath,
                const std::string_view contents)
            {
                const std::filesystem::path path =
                    fixture.path / std::filesystem::path{relativePath};
                std::error_code directoryError{};
                std::filesystem::create_directories(
                    path.parent_path(), directoryError);
                if (directoryError)
                    return false;
                std::ofstream output{
                    path, std::ios::binary | std::ios::trunc};
                output.write(
                    contents.data(),
                    static_cast<std::streamsize>(contents.size()));
                return output.good();
            };
            std::string aiImplementation{
                "namespace epochengine::ai { int ai_development_panel = 1; "
                "int direct_runtime = 1; }\n"};
            aiImplementation.resize(132u * 1024u, ' ');
            constexpr std::string_view transcriptOwner{
                "bool normalize_direct_llama_cpp_transcript = true;"};
            aiImplementation.replace(
                60u * 1024u, transcriptOwner.size(), transcriptOwner);
            constexpr std::string_view literalOwner{
                "operator refinement is required."};
            aiImplementation.replace(
                92u * 1024u, literalOwner.size(), literalOwner);
            std::string aiModule{
                "export module ai.engine;\n"};
            aiModule.resize(10u * 1024u, ' ');
            std::string legacyContract{
                "namespace epochengine { int contract = 1; }\n"};
            legacyContract.resize(160u * 1024u, ' ');
            if (!writeFixtureSource(
                    "Engine/src/ai/ai.engine.cpp",
                    aiImplementation)
                || !writeFixtureSource(
                    "Engine/modules/ai.engine.ixx",
                    aiModule)
                || !writeFixtureSource(
                    "Engine/src/epoch.engine_legacy.cpp",
                    legacyContract)
                || !writeFixtureSource(
                    "Engine/src/physics/physics.manager_contract.cpp",
                    "namespace epochengine::physics { int contract = 1; }\n"))
            {
                return false;
            }

            const HostSourceContextSelection exactPathSelection =
                curate_source_context(
                    fixture.path.generic_string(),
                    Domain::engine_source,
                    "Fix raw source parsing in "
                    "Engine\\src\\ai\\ai.engine.cpp.");
            if (!exactPathSelection.accepted
                || exactPathSelection.paths
                    != std::vector<std::string>{
                        "Engine/src/ai/ai.engine.cpp"}
                || exactPathSelection.status.find("exact existing source path")
                    == std::string::npos)
            {
                return false;
            }

            const HostSourceContextSelection focusedSelection =
                curate_source_context(
                    fixture.path.generic_string(),
                    Domain::engine_source,
                    "Add a contract proving direct llama.cpp transcript "
                    "parsing cannot be confused by an Assistant: line inside "
                    "the user prompt.");
            if (!focusedSelection.accepted
                || focusedSelection.paths.size() != 1u
                || focusedSelection.paths.front()
                    != "Engine/src/ai/ai.engine.cpp")
            {
                return false;
            }
            const std::string focusedObjective =
                "Add a contract proving direct llama.cpp transcript parsing "
                "cannot be confused by an Assistant: line inside the user prompt.";
            const SourceContextLoadResult excerptLoaded =
                load_reviewed_source_context(
                    fixture.path.generic_string(),
                    focusedSelection.paths,
                    "PATH Engine/src/ai/ai.engine.cpp\n",
                    focusedObjective);
            if (!excerptLoaded.accepted
                || excerptLoaded.file_count != 1u
                || excerptLoaded.source_bytes > maximum_source_excerpt_bytes
                || excerptLoaded.evidence.find(
                    "FILE_SOURCE_SIZE Engine/src/ai/ai.engine.cpp ")
                    == std::string::npos
                || excerptLoaded.evidence.find(
                    "FILE_EXCERPT_BEGIN Engine/src/ai/ai.engine.cpp\n")
                    == std::string::npos
                || excerptLoaded.evidence.find(
                    "FILE_CONTENT_BEGIN Engine/src/ai/ai.engine.cpp\n")
                    != std::string::npos
                || excerptLoaded.evidence.find(transcriptOwner)
                    == std::string::npos)
            {
                return false;
            }

            const std::string literalObjective =
                "replace \"operator refinement is required.\" with "
                "\"No source was staged; operator refinement is required.\" "
                "in Engine/src/ai/ai.engine.cpp owned by ai_development_panel";
            const SourceContextLoadResult literalExcerptLoaded =
                load_reviewed_source_context(
                    fixture.path.generic_string(),
                    exactPathSelection.paths,
                    "PATH Engine/src/ai/ai.engine.cpp\n",
                    literalObjective);
            if (!literalExcerptLoaded.accepted
                || literalExcerptLoaded.file_count != 1u
                || literalExcerptLoaded.source_bytes
                    > maximum_source_excerpt_bytes
                || literalExcerptLoaded.evidence.find(
                    "FILE_EXCERPT_BEGIN Engine/src/ai/ai.engine.cpp\n")
                    == std::string::npos
                || literalExcerptLoaded.evidence.find(literalOwner)
                    == std::string::npos)
            {
                return false;
            }

            localOpenInput.source_snapshot_root =
                fixture.path.generic_string();
            localOpenInput.source_authority_kind = "explicit_checkout";
            localOpenInput.source_authority_version = "0.89.31";
            localOpenInput.source_authority_commit =
                "0123456789abcdef0123456789abcdef01234567";
            localOpenInput.source_authority_receipt_digest =
                "0123456789abcdef0123456789abcdef"
                "0123456789abcdef0123456789abcdef";
            localOpenInput.source_authority_verified = true;
            localOpenInput.development_objective =
                "inspect reviewed editor source";
            localOpenInput.architecture_evidence =
                "PATH " + reviewedPath + "\n";

            auto& localOpenState = *localOpen.implementation_;
            (void)localOpenState.ensure(
                localOpenInput.workspace_id,
                localOpenInput.source_snapshot_root,
                localOpenInput.workspace_root);
            localOpenState.development_objective =
                localOpenInput.development_objective;
            localOpenState.pending_source_context_paths = {reviewedPath};
            localOpenState.pending_source_context_objective =
                localOpenInput.development_objective;

            const RenderResult opened =
                localOpen.share_requested_source_context(localOpenInput);
            if (opened.action != HostAction::none
                || opened.source_root
                    != localOpenInput.source_snapshot_root
                || opened.source_paths
                    != std::vector<std::string>{reviewedPath}
                || opened.status.find("open locally")
                    == std::string::npos
                || localOpenState.pending_source_context_paths
                    != std::vector<std::string>{reviewedPath}
                || !localOpenState.source_context_evidence.empty())
            {
                return false;
            }
        }

        auto& acceptedState = *accepted.implementation_;
        acceptedState.generation = 17u;
        acceptedState.source_root = "C:/epoch/live";
        acceptedState.workspace_root = "C:/epoch/sandbox";
        acceptedState.source_build_verified = true;
        acceptedState.source_test_pending = true;
        acceptedState.source_candidate_raw_reply = "candidate";
        acceptedState.source_candidate_operations.push_back({});

        const RenderResult debugTest = accepted.complete_source_test(
            17u, true, "Debug contract evidence passed.");
        if (debugTest.action
                != HostAction::compile_source_release_workspace
            || !acceptedState.source_test_verified
            || !acceptedState.source_release_build_pending
            || acceptedState.source_release_build_verified
            || acceptedState.source_release_test_verified)
        {
            return false;
        }

        const RenderResult staleReleaseBuild =
            accepted.complete_source_release_build(
                16u, true, "Stale Release compiler evidence.");
        if (staleReleaseBuild.action != HostAction::none
            || !acceptedState.source_release_build_pending
            || acceptedState.source_release_build_verified)
        {
            return false;
        }

        const RenderResult releaseBuild =
            accepted.complete_source_release_build(
                17u, true, "Release compiler evidence passed.");
        if (releaseBuild.action
                != HostAction::test_source_release_workspace
            || acceptedState.source_release_build_pending
            || !acceptedState.source_release_build_verified
            || !acceptedState.source_release_test_pending)
        {
            return false;
        }

        const RenderResult releaseTest =
            accepted.complete_source_release_test(
                17u, true, "Release contract evidence passed.");
        if (releaseTest.action != HostAction::compile_source_headless_workspace
            || acceptedState.source_release_test_pending
            || !acceptedState.source_release_test_verified
            || !acceptedState.source_headless_build_pending
            || acceptedState.source_headless_build_verified
            || acceptedState.source_headless_test_verified)
        {
            return false;
        }

        const RenderResult staleHeadlessBuild =
            accepted.complete_source_headless_build(
                16u, true, "Stale HeadlessCI compiler evidence.");
        if (staleHeadlessBuild.action != HostAction::none
            || !acceptedState.source_headless_build_pending
            || acceptedState.source_headless_build_verified)
        {
            return false;
        }

        const RenderResult headlessBuild =
            accepted.complete_source_headless_build(
                17u, true, "HeadlessCI compiler evidence passed.");
        if (headlessBuild.action != HostAction::test_source_headless_workspace
            || acceptedState.source_headless_build_pending
            || !acceptedState.source_headless_build_verified
            || !acceptedState.source_headless_test_pending)
        {
            return false;
        }

        const RenderResult headlessTest =
            accepted.complete_source_headless_test(
                17u, true, "HeadlessCI runtime evidence passed.");
        if (headlessTest.action != HostAction::none
            || acceptedState.source_headless_test_pending
            || !acceptedState.source_headless_test_verified
            || acceptedState.source_full_validation_pending
            || acceptedState.source_full_validation_verified
            || !accepted.has_source_full_validation_candidate()
            || accepted.has_verified_source_candidate())
        {
            return false;
        }

        const RenderResult fullValidation =
            accepted.approve_source_full_validation();
        if (fullValidation.action
                != HostAction::test_source_full_validation_workspace
            || !acceptedState.source_full_validation_pending
            || accepted.has_source_full_validation_candidate()
            || accepted.has_verified_source_candidate())
        {
            return false;
        }

        const RenderResult staleFullValidation =
            accepted.complete_source_full_validation(
                16u, true, "Stale full-validation evidence.");
        if (staleFullValidation.action != HostAction::none
            || !acceptedState.source_full_validation_pending
            || acceptedState.source_full_validation_verified)
        {
            return false;
        }

        const RenderResult acceptedFullValidation =
            accepted.complete_source_full_validation(
                17u, true, "Full-validation evidence passed.");
        if (acceptedFullValidation.action != HostAction::none
            || acceptedState.source_full_validation_pending
            || !acceptedState.source_full_validation_verified
            || accepted.has_source_full_validation_candidate()
            || !accepted.has_verified_source_candidate())
        {
            return false;
        }

        Panel compilerFailure{};
        auto& compilerFailureState = *compilerFailure.implementation_;
        compilerFailureState.generation = 31u;
        compilerFailureState.source_root = "C:/epoch/live";
        compilerFailureState.workspace_root = "C:/epoch/sandbox";
        compilerFailureState.source_build_verified = true;
        compilerFailureState.source_test_pending = true;
        if (compilerFailure.complete_source_test(
                31u, true, "Debug contract evidence passed.").action
            != HostAction::compile_source_release_workspace)
        {
            return false;
        }
        const RenderResult rejectedReleaseBuild =
            compilerFailure.complete_source_release_build(
                31u, false, "Release compiler evidence failed.");
        if (rejectedReleaseBuild.action != HostAction::none
            || compilerFailureState.source_release_build_pending
            || compilerFailureState.source_release_build_verified
            || compilerFailureState.source_release_test_pending
            || compilerFailureState.source_release_test_verified)
        {
            return false;
        }

        Panel contractFailure{};
        auto& contractFailureState = *contractFailure.implementation_;
        contractFailureState.generation = 47u;
        contractFailureState.source_root = "C:/epoch/live";
        contractFailureState.workspace_root = "C:/epoch/sandbox";
        contractFailureState.source_build_verified = true;
        contractFailureState.source_test_pending = true;
        if (contractFailure.complete_source_test(
                47u, true, "Debug contract evidence passed.").action
                != HostAction::compile_source_release_workspace
            || contractFailure.complete_source_release_build(
                47u, true, "Release compiler evidence passed.").action
                != HostAction::test_source_release_workspace)
        {
            return false;
        }
        const RenderResult rejectedReleaseTest =
            contractFailure.complete_source_release_test(
                47u, false, "Release contract evidence failed.");
        if (rejectedReleaseTest.action != HostAction::none
            || !contractFailureState.source_release_build_verified
            || contractFailureState.source_release_test_pending
            || contractFailureState.source_release_test_verified
            || contractFailureState.source_headless_build_pending
            || contractFailureState.source_headless_test_pending
            || contractFailureState.source_headless_build_verified
            || contractFailureState.source_headless_test_verified)
        {
            return false;
        }

        Panel headlessFailure{};
        auto& headlessFailureState = *headlessFailure.implementation_;
        headlessFailureState.generation = 59u;
        headlessFailureState.source_root = "C:/epoch/live";
        headlessFailureState.workspace_root = "C:/epoch/sandbox";
        headlessFailureState.source_build_verified = true;
        headlessFailureState.source_test_verified = true;
        headlessFailureState.source_release_build_verified = true;
        headlessFailureState.source_release_test_verified = true;
        headlessFailureState.source_headless_build_pending = true;
        if (headlessFailure.complete_source_headless_build(
                59u, true, "HeadlessCI compiler evidence passed.").action
            != HostAction::test_source_headless_workspace)
        {
            return false;
        }
        const RenderResult rejectedHeadlessTest =
            headlessFailure.complete_source_headless_test(
                59u, false, "HeadlessCI runtime evidence failed.");
        if (rejectedHeadlessTest.action != HostAction::none
            || !headlessFailureState.source_headless_build_verified
            || headlessFailureState.source_headless_test_pending
            || headlessFailureState.source_headless_test_verified
            || headlessFailureState.source_full_validation_pending
            || headlessFailureState.source_full_validation_verified)
        {
            return false;
        }

        Panel validationFailure{};
        auto& validationFailureState = *validationFailure.implementation_;
        validationFailureState.generation = 71u;
        validationFailureState.source_root = "C:/epoch/live";
        validationFailureState.workspace_root = "C:/epoch/sandbox";
        validationFailureState.source_build_verified = true;
        validationFailureState.source_test_verified = true;
        validationFailureState.source_release_build_verified = true;
        validationFailureState.source_release_test_verified = true;
        validationFailureState.source_headless_build_verified = true;
        validationFailureState.source_headless_test_verified = true;
        validationFailureState.source_candidate_raw_reply = "candidate";
        validationFailureState.source_candidate_operations.push_back({});
        if (!validationFailure.has_source_full_validation_candidate()
            || validationFailure.approve_source_full_validation().action
                != HostAction::test_source_full_validation_workspace)
        {
            return false;
        }
        const RenderResult rejectedFullValidation =
            validationFailure.complete_source_full_validation(
                71u, false, "Full-validation evidence failed.");
        if (rejectedFullValidation.action != HostAction::none
            || validationFailureState.source_full_validation_pending
            || validationFailureState.source_full_validation_verified
            || validationFailure.has_verified_source_candidate())
        {
            return false;
        }

        Panel falseInsufficient{};
        auto& falseInsufficientState =
            *falseInsufficient.implementation_;
        falseInsufficientState.development_objective =
            "replace \"operator refinement is required.\" with "
            "\"No source was staged; operator refinement is required.\" "
            "in Engine/src/editor/ai/editor.ai_development_panel.cpp";
        falseInsufficientState.source_context_evidence =
            "FILE_EXCERPT_SIZE "
            "Engine/src/editor/ai/editor.ai_development_panel.cpp 43\n"
            "FILE_EXCERPT_BEGIN "
            "Engine/src/editor/ai/editor.ai_development_panel.cpp\n"
            "operator refinement is required.\n"
            "FILE_EXCERPT_END "
            "Engine/src/editor/ai/editor.ai_development_panel.cpp\n";
        falseInsufficientState.active_domain = Domain::engine_source;
        const Input falseInsufficientInput{
            .domain = Domain::engine_source,
            .source_snapshot_root = "C:/epoch/live",
            .development_objective =
                falseInsufficientState.development_objective,
            .architecture_evidence =
                falseInsufficientState.source_context_evidence};
        const RenderResult correctedFalseInsufficient =
            falseInsufficientState.stage_source_reply(
                falseInsufficientInput,
                "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1",
                logical_time_now());
        if (correctedFalseInsufficient.action
                != HostAction::request_model_source_proposal
            || correctedFalseInsufficient.model_prompt.find(
                "first quoted replacement target occurs exactly once")
                == std::string::npos)
        {
            return false;
        }

        Panel diagnosticInsufficient{};
        auto& diagnosticInsufficientState =
            *diagnosticInsufficient.implementation_;
        diagnosticInsufficientState.development_objective =
            "fix a bug in opengl";
        diagnosticInsufficientState.source_context_evidence =
            "FILE_CONTENT_BEGIN Engine/src/renderers/opengl/opengl.context_init.cpp\n"
            "void initialize_context() {}\n"
            "FILE_CONTENT_END Engine/src/renderers/opengl/opengl.context_init.cpp\n";
        diagnosticInsufficientState.active_domain = Domain::engine_source;
        const Input diagnosticInsufficientInput{
            .domain = Domain::engine_source,
            .source_snapshot_root = "C:/epoch/live",
            .development_objective =
                diagnosticInsufficientState.development_objective,
            .architecture_evidence =
                diagnosticInsufficientState.source_context_evidence};
        const RenderResult diagnosticRecheck =
            diagnosticInsufficientState.stage_source_reply(
                diagnosticInsufficientInput,
                "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1",
                logical_time_now());
        const RenderResult diagnosticExhausted =
            diagnosticInsufficientState.stage_source_reply(
                diagnosticInsufficientInput,
                "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1",
                logical_time_now());
        if (diagnosticRecheck.action
                != HostAction::request_model_source_proposal
            || diagnosticRecheck.model_prompt.find(
                "A pre-named symbol is not required") == std::string::npos
            || diagnosticExhausted.action != HostAction::none
            || diagnosticExhausted.status.find(
                "after one diagnostic recheck") == std::string::npos)
        {
            return false;
        }

        Panel packetCorrection{};
        auto& packetCorrectionState = *packetCorrection.implementation_;
        packetCorrectionState.development_objective =
            "Repair the exact parser implementation.";
        packetCorrectionState.source_context_evidence =
            "FILE_CONTENT_BEGIN Engine/src/ai/parser.cpp\n"
            "known source\n"
            "FILE_CONTENT_END Engine/src/ai/parser.cpp\n";
        packetCorrectionState.active_domain = Domain::engine_source;
        const Input packetInput{
            .domain = Domain::engine_source,
            .source_snapshot_root = "C:/epoch/live",
            .development_objective =
                "Repair the exact parser implementation.",
            .architecture_evidence =
                packetCorrectionState.source_context_evidence};
        const RenderResult correctionOne =
            packetCorrectionState.stage_source_reply(
                packetInput,
                "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1\nunexpected suffix",
                logical_time_now());
        const RenderResult correctionTwo =
            packetCorrectionState.stage_source_reply(
                packetInput, "still malformed", logical_time_now());
        const RenderResult correctionExhausted =
            packetCorrectionState.stage_source_reply(
                packetInput, "malformed again", logical_time_now());
        return correctionOne.action
                == HostAction::request_model_source_proposal
            && correctionTwo.action
                == HostAction::request_model_source_proposal
            && correctionExhausted.action == HostAction::none
            && correctionOne.model_prompt.find(
                "EPOCH_SOURCE_PROTOCOL_CORRECTION_V1")
                != std::string::npos
            && correctionTwo.model_prompt.find("CORRECTION_ATTEMPT 2 OF 2")
                != std::string::npos
            && correctionExhausted.status.find("limit is exhausted")
                != std::string::npos;
    }

    void Panel::reset(std::string workspaceId)
    {
        if (implementation_)
            implementation_->reset_controller(
                std::move(workspaceId),
                implementation_->source_root,
                implementation_->sandbox_base_root);
    }

    std::string Panel::status() const
    {
        return implementation_
            ? implementation_->status_message
            : std::string{"Guarded development panel is unavailable."};
    }

    RenderResult Panel::render(const Input& input)
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }
        auto& state = *implementation_;
        auto& controller = state.ensure(
            input.workspace_id,
            input.source_snapshot_root,
            input.workspace_root);
        if (state.development_objective.empty()
            && !input.development_objective.empty())
        {
            state.development_objective = input.development_objective;
        }
        if (state.source_context_evidence.empty()
            && !input.architecture_evidence.empty())
        {
            state.source_context_evidence = input.architecture_evidence;
        }
        if (!state.source_context_evidence_objective.empty()
            && state.source_context_evidence_objective
                != state.development_objective)
        {
            state.source_context_evidence = input.architecture_evidence;
            state.source_context_evidence_objective.clear();
            state.pending_source_context_paths.clear();
            state.pending_source_context_reason.clear();
            state.pending_source_context_objective.clear();
            state.status_message =
                "The objective changed; stale reviewed source context was discarded.";
        }
        auto snapshot = controller.snapshot();
        const float width = (std::max)(180.0f, input.available_width);

        gui::label("Engine Development Sandbox");
        gui::property_row("Phase",
            editor_ai_development::to_string(snapshot.phase));
        gui::property_row("Live source",
            state.source_root.empty()
                ? std::string{"Unavailable"}
                : state.source_root);
        gui::property_row("Disposable sandbox",
            state.workspace_root.empty()
                ? std::string{"Unavailable"}
                : state.workspace_root);
        if (input.domain != Domain::tooling)
        {
            gui::label("Development Objective");
            (void)gui::edit_box(
                state.development_objective,
                {width, 82.0f},
                4'096u,
                true);
            gui::wrapped_label(
                "Name one bounded source outcome, or ask for one source-proven "
                "defect in a named subsystem. Epoch accepts only existing paths "
                "and evidence-backed edits. Invented source operations are "
                "rejected before staging.",
                width);
            if (!state.pending_source_context_paths.empty())
            {
                gui::label("Host-Curated Source Context");
                gui::property_row("Target model",
                    input.selected_model.empty()
                        ? std::string{"Unavailable"}
                        : input.selected_model);
                gui::property_row("Endpoint",
                    input.selected_endpoint.empty()
                        ? std::string{"Unavailable"}
                        : input.selected_endpoint);
                gui::property_row("Selection",
                    state.pending_source_context_reason.empty()
                        ? std::string{"Ranked from the approved objective"}
                        : state.pending_source_context_reason);
                for (const auto& path : state.pending_source_context_paths)
                    gui::wrapped_label(path, width);
                const bool staleObjective =
                    state.pending_source_context_objective
                        != state.development_objective;
                gui::wrapped_label(
                    staleObjective
                        ? "The objective changed after selection. Reject it and request fresh context."
                        : "Epoch validated these requested paths without reading them. Share reads and sends only their reviewed UTF-8 bytes to the selected endpoint; Reject sends nothing.",
                    width);
                if (!staleObjective
                    && gui::button("Share Curated Context", {width, 30.0f}))
                {
                    return share_requested_source_context(input);
                }
                if (gui::button("Reject Selection", {width, 30.0f}))
                    return reject_requested_source_context();
            }
        }
        if (input.domain == Domain::engine_source)
        {
            state.render_typed_campaign(input, width, output);
        }
        (void)gui::toggle_switch(
            "Advanced evidence details",
            state.advanced_controls,
            {width, 28.0f});
        if (state.advanced_controls)
        {
            gui::label("Sandbox Evidence");
            gui::property_row("Phase",
                editor_ai_development::to_string(snapshot.phase));
            gui::property_row("Digest",
                snapshot.digest_hex.empty()
                    ? std::string{"(proposal not staged)"}
                    : snapshot.digest_hex.substr(0u, 24u) + "...");
            gui::property_row("Approval",
                snapshot.exact_operator_approval_recorded
                    ? "Exact operator approval recorded"
                    : "Required before execution");
            gui::property_row("Permit",
                snapshot.single_use_permit_issued
                    ? "Single-use sandbox permit issued"
                    : "Not issued");
            gui::property_row("Evidence",
                std::to_string(snapshot.evidence_record_count)
                    + " verified record(s)");
            for (const auto& operation : snapshot.operations)
            {
                gui::wrapped_label(
                    epochengine::format_text("{} | {} | {}",
                        editor_ai_development::to_string(operation.kind),
                        operation.summary,
                        operation.relative_path),
                    width);
                if (operation.writes_source)
                {
                    gui::property_row("Before",
                        content_state_summary(operation.before));
                    gui::property_row("After",
                        content_state_summary(operation.after));
                }
            }
            gui::wrapped_label(
                "Evidence details are read-only. The compact workflow below is "
                "the only place that can request, approve, cancel, or execute a "
                "sandbox transaction.",
                width);
        }

        {
            gui::property_row("Status", state.status_message);
            gui::property_row("Isolation", "Live source is read-only");
            if (state.source_headless_test_verified
                && !state.source_candidate_operations.empty())
            {
                gui::property_row(
                    state.source_full_validation_verified
                        ? "Live promotion" : "Full validation",
                    state.source_promotion_staged
                        ? "Exact digest staged for separate approval"
                        : (state.source_full_validation_verified
                            ? "All required evidence verified; approval required"
                            : "Explicit operator approval required"));
            }
            for (const auto& operation : snapshot.operations)
            {
                gui::wrapped_label(
                    epochengine::format_text("{} | {}",
                        editor_ai_development::to_string(operation.kind),
                        operation.relative_path),
                    width);
            }

            const auto compactNow = logical_time_now();
            const auto approveAndRunInSandbox = [&]()
            {
                auto phase = controller.snapshot().phase;
                if (phase == editor_ai_development::ControllerPhase::proposed)
                {
                    const auto reviewed = controller.review(
                        "epoch.operator",
                        "Operator reviewed the exact sandbox operation packet.",
                        compactNow);
                    state.status_message = reviewed.status;
                    if (!reviewed)
                        return;
                    phase = controller.snapshot().phase;
                }
                if (phase == editor_ai_development::ControllerPhase::reviewed)
                {
                    const auto approved = controller.approve(
                        "epoch.operator",
                        "Operator approves this immutable packet for sandbox execution only.",
                        compactNow,
                        {compactNow.value + 10u * 60u});
                    state.status_message = approved.status;
                    if (!approved)
                        return;
                    phase = controller.snapshot().phase;
                }
                if (phase == editor_ai_development::ControllerPhase::approved)
                {
                    const auto authorized = controller.authorize(
                        compactNow,
                        editor_ai_development::Duration{5u * 60u});
                    state.status_message = authorized.status;
                    if (!authorized)
                        return;
                    phase = controller.snapshot().phase;
                }
                if (phase != editor_ai_development::ControllerPhase::authorized)
                    return;

                if (state.active_domain == Domain::tooling)
                {
                    output.action = HostAction::execute_tool_harness;
                    state.status_message =
                        "Approved tool harness execution requested.";
                    return;
                }

                output = state.execute_source_and_queue_build(compactNow);
            };

            if (input.execution_pending)
            {
                gui::wrapped_label(
                    "A cancellable host task is running in the disposable source workspace.",
                    width);
                if (gui::button(
                        "Cancel Running Sandbox Task", {width, 30.0f}))
                {
                    output.action = HostAction::cancel_source_task;
                    state.status_message =
                        "Cancellation was requested for the running guarded source task.";
                }
            }
            else if (snapshot.phase
                == editor_ai_development::ControllerPhase::ready)
            {
                if (input.domain == Domain::tooling)
                {
                    if (gui::button("Prepare Tool Harness", {width, 30.0f}))
                    {
                        if (!input.tool_source_ready
                            || input.tool_output_relative_path.empty())
                        {
                            state.status_message =
                                "Save a valid tool script before preparing its harness.";
                        }
                        else
                        {
                            editor_ai_development::ProposalRequest request{};
                            request.title = "Build and run selected editor tool script";
                            request.rationale =
                                "Execute one reviewed tool through the bounded host.";
                            request.expires_at = {compactNow.value + 15u * 60u};
                            request.operations = {
                                {editor_ai_development::OperationKind::build,
                                    "Compile the selected C++23 project tool script.",
                                    input.tool_output_relative_path},
                                {editor_ai_development::OperationKind::run,
                                    "Run the script through the bounded editor host API.",
                                    input.tool_output_relative_path}};
                            const auto proposed = controller.propose(
                                std::move(request), compactNow);
                            state.status_message = proposed.status;
                            if (proposed)
                                state.active_domain = Domain::tooling;
                        }
                    }
                }
                else
                {
                    if (gui::button("Request Proposal", {width, 30.0f}))
                    {
                        if (state.development_objective.empty())
                        {
                            state.status_message =
                                "Enter one bounded development objective first.";
                        }
                        else
                        {
                            const HostSourceContextSelection selection =
                                curate_source_context(
                                    input.source_snapshot_root,
                                    input.domain,
                                    state.development_objective);
                            state.pending_source_context_paths = selection.paths;
                            state.pending_source_context_reason = selection.status;
                            state.pending_source_context_objective =
                                state.development_objective;
                            state.source_context_evidence =
                                input.architecture_evidence;
                            state.source_context_evidence_objective =
                                state.development_objective;
                            state.active_domain = input.domain;
                            if (selection.accepted)
                            {
                                state.status_message = selection.status;
                            }
                            else
                            {
                                state.status_message =
                                    "selection_required: " + selection.status
                                    + " Name an existing path or symbol; no source bytes were read or sent.";
                                if (!state.iteration_session)
                                    state.iteration_session = std::make_unique<
                                        ai::iteration_session::IterationSession>();
                                (void)state.iteration_session->require_selection(
                                    state.status_message);
                                output.status = state.status_message;
                            }
                        }
                    }
                }
            }
            else if (snapshot.phase
                    == editor_ai_development::ControllerPhase::proposed
                || snapshot.phase
                    == editor_ai_development::ControllerPhase::reviewed
                || snapshot.phase
                    == editor_ai_development::ControllerPhase::approved
                || snapshot.phase
                    == editor_ai_development::ControllerPhase::authorized)
            {
                if (state.active_domain == Domain::tooling)
                {
                    if (!input.execution_pending
                        && gui::button(
                            "Approve And Run Harness", {width, 30.0f}))
                    {
                        approveAndRunInSandbox();
                    }
                    if (!input.execution_pending
                        && gui::button("Cancel", {width, 30.0f}))
                    {
                        const auto cancelled = controller.cancel(
                            "epoch.operator",
                            "Operator cancelled the guarded sandbox proposal.",
                            compactNow);
                        state.status_message = cancelled.status;
                    }
                }
                else
                {
                    gui::wrapped_label(
                        "Review, approve, or cancel the guarded source reply "
                        "from its AI Chat message.",
                        width);
                }
            }
            else if (gui::button("New Sandbox Iteration", {width, 30.0f}))
            {
                state.reset_controller(
                    input.workspace_id,
                    input.source_snapshot_root,
                    input.workspace_root);
            }
        }

        output.status = state.status_message;
        return output;
    }

    RenderResult Panel::stage_latest_model_proposal(const Input& input)
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }

        auto& state = *implementation_;
        auto& controller = state.ensure(
            input.workspace_id,
            input.source_snapshot_root,
            input.workspace_root);
        if (input.latest_raw_model_reply.empty())
        {
            state.status_message =
                "No local-model source proposal is available to validate.";
            output.status = state.status_message;
            return output;
        }
        if (state.campaign_orchestrator
            && state.campaign_pending_operation)
        {
            using OperationKind =
                ai::self_iteration_orchestrator::OperationKind;
            const auto pending = *state.campaign_pending_operation;
            const std::uint64_t now = logical_time_now().value;
            if (pending.kind() == OperationKind::model_plan)
            {
                state.capture_campaign_result(
                    output,
                    state.campaign_orchestrator->record_plan(
                        state.campaign_receipt(pending, now),
                        input.latest_raw_model_reply,
                        "Selected provider returned the bounded campaign plan."));
                return output;
            }
            if (pending.kind() == OperationKind::model_proposal)
            {
                if (controller.snapshot().phase
                    != editor_ai_development::ControllerPhase::ready)
                {
                    state.status_message =
                        "Finish or cancel the current guarded proposal before staging another.";
                    output.status = state.status_message;
                    return output;
                }
                RenderResult staged = state.stage_source_reply(
                    input,
                    input.latest_raw_model_reply,
                    logical_time_now());
                if (controller.snapshot().phase
                    != editor_ai_development::ControllerPhase::proposed)
                {
                    return staged;
                }
                state.capture_campaign_result(
                    staged,
                    state.campaign_orchestrator->record_proposal(
                        state.campaign_receipt(pending, now),
                        input.latest_raw_model_reply,
                        "Guarded proposal codec admitted the exact reviewed source packet."));
                return staged;
            }
        }
        if (controller.snapshot().phase
            != editor_ai_development::ControllerPhase::ready)
        {
            state.status_message =
                "Finish or cancel the current guarded proposal before staging another.";
            output.status = state.status_message;
            return output;
        }

        output = state.stage_source_reply(
            input,
            input.latest_raw_model_reply,
            logical_time_now());
        output.status = state.status_message;
        return output;
    }

    RenderResult Panel::share_requested_source_context(const Input& input)
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }

        auto& state = *implementation_;
        (void)state.ensure(
            input.workspace_id,
            input.source_snapshot_root,
            input.workspace_root);
        if (state.pending_source_context_paths.empty())
        {
            state.status_message =
                "No reviewed source selection is waiting to be shared.";
            output.status = state.status_message;
            return output;
        }
        if (state.pending_source_context_objective
            != state.development_objective)
        {
            state.status_message =
                "The development objective changed after context selection. Reject "
                "this selection and request fresh context.";
            output.status = state.status_message;
            return output;
        }
        const std::vector<std::string> sharedSourcePaths =
            state.pending_source_context_paths;
        ai::iteration_session::SourceAuthority authority{
            .kind = input.source_authority_kind == "explicit_checkout"
                ? ai::iteration_session::SourceAuthorityKind::explicit_checkout
                : input.source_authority_kind == "verified_cache"
                    ? ai::iteration_session::SourceAuthorityKind::verified_cache
                    : ai::iteration_session::SourceAuthorityKind::unavailable,
            .root = std::filesystem::path{input.source_snapshot_root},
            .source_version = input.source_authority_version,
            .commit = input.source_authority_commit,
            .receipt_digest = input.source_authority_receipt_digest,
            .verified = input.source_authority_verified};
        const auto inspected = ai::iteration_session::inspect_curated_files(
            authority, sharedSourcePaths);
        if (!inspected.accepted)
        {
            state.status_message = "Verified source authority refused context sharing: "
                + inspected.status + " No source bytes were read or sent.";
            output.status = state.status_message;
            return output;
        }
        const auto loaded = load_reviewed_source_context(
            input.source_snapshot_root,
            sharedSourcePaths,
            input.architecture_evidence,
            state.development_objective);
        state.status_message = loaded.status;
        if (!loaded.accepted)
        {
            output.status = state.status_message;
            return output;
        }
        output.source_root = input.source_snapshot_root;
        output.source_paths = sharedSourcePaths;

        if (input.selected_model.empty() || input.selected_endpoint.empty())
        {
            state.status_message =
                "The reviewed files are open locally. Select and approve a "
                "model endpoint before sharing their bytes; nothing was sent.";
            output.status = state.status_message;
            return output;
        }

        const auto modelCapabilities =
            ai::iteration::infer_model_capabilities(
                input.selected_model);
        const auto capability =
            ai::iteration::assess_capabilities(
                modelCapabilities,
                ai::iteration::LoopPolicy{
                    .risk = ai::iteration::RiskClass::related_files,
                    .maximum_repair_attempts =
                        static_cast<std::uint32_t>(
                            Implementation::maximum_source_repair_attempts)});
        if (capability.disposition
            != ai::iteration::CapabilityDisposition::eligible)
        {
            state.status_message = capability.summary + " "
                + capability.recommended_model_class;
            output.status = state.status_message;
            return output;
        }

        state.iteration_session = std::make_unique<
            ai::iteration_session::IterationSession>();
        const auto configured = state.iteration_session->configure(
            ai::iteration_session::SessionConfiguration{
                .objective = state.development_objective,
                .model_name = input.selected_model,
                .source = authority,
                .curated_files = inspected.files,
                .policy = ai::iteration_session::CandidatePolicy::manual_each_candidate,
                .maximum_repair_attempts = static_cast<std::uint32_t>(
                    Implementation::maximum_source_repair_attempts)});
        if (!configured)
        {
            state.status_message = configured.status;
            output.status = state.status_message;
            return output;
        }
        const auto shared = state.iteration_session->context_shared();
        if (!shared)
        {
            state.status_message = shared.status;
            output.status = state.status_message;
            return output;
        }
        state.source_context_evidence = loaded.evidence;
        state.source_baseline_evidence = loaded.evidence;
        state.source_repair_attempts = 0u;
        state.model_reply_corrections = 0u;
        state.source_diagnostic_recheck_queued = false;
        state.model_reply_correction_diagnostic.clear();
        state.source_context_evidence_objective =
            state.development_objective;
        state.pending_source_context_paths.clear();
        state.pending_source_context_reason.clear();
        state.pending_source_context_objective.clear();
        state.active_domain = input.domain;
        if (!state.source_workspace_ready)
        {
            if (state.source_workspace_pending)
            {
                state.status_message =
                    "The disposable source workspace is already being materialized.";
                output.status = state.status_message;
                output.source_root = input.source_snapshot_root;
                output.source_paths = sharedSourcePaths;
                return output;
            }
            state.source_workspace_pending = true;
            state.status_message =
                "Preparing a buildable disposable source workspace before any model-authored bytes are staged.";
            output.action = HostAction::materialize_source_workspace;
            output.source_root = input.source_snapshot_root;
            output.source_paths = sharedSourcePaths;
            output.workspace_root = state.workspace_root;
            output.workspace_generation = state.generation;
            output.include_paths = input.domain == Domain::engine_source
                ? std::vector<std::string>{"Engine.sln", "Engine"}
                : std::vector<std::string>{"Projects"};
            output.excluded_components = {
                ".git",
                ".vs",
                "Bin",
                "Debug",
                "Release",
                "build",
                "built",
                "cache",
                "logs",
                "vcpkg_installed",
                "x64"};
            output.status = state.status_message;
            return output;
        }

        state.status_message =
            "Buildable disposable workspace is verified; requesting one exact source proposal.";
        output = state.source_model_request();
        output.source_root = input.source_snapshot_root;
        output.source_paths = sharedSourcePaths;
        return output;
    }

    RenderResult Panel::complete_source_workspace(
        std::uint32_t generation,
        bool succeeded,
        std::string status,
        std::size_t file_count,
        std::uint64_t total_bytes)
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }
        auto& state = *implementation_;
        if (generation != state.generation
            || !state.source_workspace_pending)
        {
            state.status_message =
                "A stale disposable-workspace completion was ignored.";
            output.status = state.status_message;
            return output;
        }

        state.source_workspace_pending = false;
        state.source_workspace_ready = succeeded;
        state.source_workspace_file_count = succeeded ? file_count : 0u;
        state.source_workspace_total_bytes = succeeded ? total_bytes : 0u;
        if (!succeeded)
        {
            state.status_message = status.empty()
                ? std::string{
                    "Disposable source workspace materialization failed closed."}
                : std::move(status);
            output.status = state.status_message;
            return output;
        }

        state.status_message = epochengine::format_text(
            "Verified disposable build workspace: {} source file(s), {} KiB copied locally. Only the explicitly reviewed context was sent to the selected model; live source remained read-only.",
            file_count,
            (total_bytes + 1023u) / 1024u);
        return state.source_model_request();
    }

    RenderResult Panel::reject_requested_source_context()
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }
        auto& state = *implementation_;
        state.pending_source_context_paths.clear();
        state.pending_source_context_reason.clear();
        state.pending_source_context_objective.clear();
        state.status_message =
            "The host-curated source selection was rejected; no file bytes were read "
            "or sent.";
        output.status = state.status_message;
        return output;
    }

    bool Panel::has_pending_source_context() const
    {
        return implementation_
            && !implementation_->pending_source_context_paths.empty();
    }

    bool Panel::has_staged_proposal() const
    {
        if (!implementation_ || !implementation_->controller)
            return false;
        const auto phase = implementation_->controller->snapshot().phase;
        return phase == editor_ai_development::ControllerPhase::proposed
            || phase == editor_ai_development::ControllerPhase::reviewed
            || phase == editor_ai_development::ControllerPhase::approved
            || phase == editor_ai_development::ControllerPhase::authorized;
    }

    bool Panel::has_source_full_validation_candidate() const
    {
        return implementation_
            && implementation_->source_build_verified
            && implementation_->source_test_verified
            && implementation_->source_release_build_verified
            && implementation_->source_release_test_verified
            && implementation_->source_headless_build_verified
            && implementation_->source_headless_test_verified
            && !implementation_->source_full_validation_pending
            && !implementation_->source_full_validation_verified
            && !implementation_->source_candidate_raw_reply.empty()
            && !implementation_->source_candidate_operations.empty()
            && !implementation_->source_promotion_completed;
    }

    bool Panel::has_verified_source_candidate() const
    {
        return implementation_
            && implementation_->source_build_verified
            && implementation_->source_test_verified
            && implementation_->source_release_build_verified
            && implementation_->source_release_test_verified
            && implementation_->source_headless_build_verified
            && implementation_->source_headless_test_verified
            && implementation_->source_full_validation_verified
            && !implementation_->source_candidate_raw_reply.empty()
            && !implementation_->source_candidate_operations.empty()
            && !implementation_->source_promotion_completed;
    }

    bool Panel::has_staged_source_promotion() const
    {
        return implementation_
            && implementation_->source_promotion_staged
            && implementation_->promotion_controller;
    }

    RenderResult Panel::approve_and_run_staged_proposal(const Input& input)
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }
        auto& state = *implementation_;
        auto& controller = state.ensure(
            input.workspace_id,
            input.source_snapshot_root,
            input.workspace_root);
        if (input.execution_pending)
        {
            state.status_message =
                "The guarded sandbox is already executing approved work.";
            output.status = state.status_message;
            return output;
        }

        const auto now = logical_time_now();
        auto phase = controller.snapshot().phase;
        if (phase == editor_ai_development::ControllerPhase::proposed)
        {
            const auto reviewed = controller.review(
                "epoch.operator",
                "Operator reviewed the exact source proposal beside its AI Chat response.",
                now);
            state.status_message = reviewed.status;
            if (!reviewed)
            {
                output.status = state.status_message;
                return output;
            }
            phase = controller.snapshot().phase;
        }
        if (phase == editor_ai_development::ControllerPhase::reviewed)
        {
            const auto approved = controller.approve(
                "epoch.operator",
                "Operator approves this immutable packet for sandbox execution only.",
                now,
                {now.value + 10u * 60u});
            state.status_message = approved.status;
            if (!approved)
            {
                output.status = state.status_message;
                return output;
            }
            phase = controller.snapshot().phase;
        }
        if (phase == editor_ai_development::ControllerPhase::approved)
        {
            const auto authorized = controller.authorize(
                now,
                editor_ai_development::Duration{5u * 60u});
            state.status_message = authorized.status;
            if (!authorized)
            {
                output.status = state.status_message;
                return output;
            }
            phase = controller.snapshot().phase;
        }
        if (phase != editor_ai_development::ControllerPhase::authorized)
        {
            state.status_message =
                "No reviewed source proposal is ready for sandbox execution.";
            output.status = state.status_message;
            return output;
        }
        if (state.active_domain != Domain::tooling && state.iteration_session)
        {
            const auto approved = state.iteration_session->approve_candidate(
                state.iteration_session->identity());
            if (!approved)
            {
                state.status_message = approved.status;
                output.status = state.status_message;
                return output;
            }
        }

        if (state.active_domain == Domain::tooling)
        {
            output.action = HostAction::execute_tool_harness;
            state.status_message =
                "Approved tool harness execution requested.";
        }
        else
        {
            output = state.execute_source_and_queue_build(now);
        }
        output.status = state.status_message;
        return output;
    }

    RenderResult Panel::complete_source_build(
        std::uint32_t generation,
        bool succeeded,
        std::string status)
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }
        auto& state = *implementation_;
        if (generation != state.generation || !state.source_build_pending)
        {
            state.status_message =
                "A stale sandbox-compiler completion was ignored.";
            output.status = state.status_message;
            return output;
        }

        const std::string iterationEvidence = status.empty()
            ? std::string{"The Debug compiler completion contained no diagnostic text."}
            : status;
        if (!state.record_iteration_validation(
                ai::iteration_session::ValidationActor::debug_compiler,
                succeeded,
                iterationEvidence,
                false))
        {
            output.status = state.status_message;
            return output;
        }
        state.source_build_pending = false;
        if (succeeded)
        {
            state.source_build_verified = true;
            state.source_test_verified = false;
            state.status_message = status.empty()
                ? std::string{
                    "The host compiler accepted the disposable source workspace."}
                : std::move(status);
            state.source_test_pending = true;
            output.action = HostAction::test_source_workspace;
            output.source_root = state.source_root;
            output.workspace_root = state.workspace_root;
            output.workspace_generation = state.generation;
            state.status_message +=
                " Compiler evidence passed; the build-safe engine contract test is queued as a distinct actor.";
        }
        else
        {
            state.source_build_verified = false;
            state.source_test_verified = false;
            output = state.queue_repair_after_verified_failure(
                status.empty()
                ? std::string{
                    "The host compiler rejected the disposable source workspace."}
                : std::move(status),
                "compiler");
        }
        output.status = state.status_message;
        return output;
    }

    RenderResult Panel::complete_source_test(
        std::uint32_t generation,
        bool succeeded,
        std::string status)
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }
        auto& state = *implementation_;
        if (generation != state.generation || !state.source_test_pending)
        {
            state.status_message =
                "A stale sandbox-test completion was ignored.";
            output.status = state.status_message;
            return output;
        }
        const std::string iterationEvidence = status.empty()
            ? std::string{"The Debug contract completion contained no diagnostic text."}
            : status;
        if (!state.record_iteration_validation(
                ai::iteration_session::ValidationActor::debug_contract,
                succeeded,
                iterationEvidence,
                false))
        {
            output.status = state.status_message;
            return output;
        }
        state.source_test_pending = false;
        if (succeeded)
        {
            state.source_test_verified = true;
            state.source_release_build_verified = false;
            state.source_release_test_verified = false;
            state.source_headless_build_pending = false;
            state.source_headless_test_pending = false;
            state.source_headless_build_verified = false;
            state.source_headless_test_verified = false;
            state.source_full_validation_pending = false;
            state.source_full_validation_verified = false;
            state.status_message = status.empty()
                ? std::string{
                    "The Debug build-safe engine contract test accepted the disposable source workspace."}
                : std::move(status);
            state.source_release_build_pending = true;
            output.action = HostAction::compile_source_release_workspace;
            output.source_root = state.source_root;
            output.workspace_root = state.workspace_root;
            output.workspace_generation = state.generation;
            state.status_message +=
                " Debug compiler and contract evidence passed; the independent Release compiler is queued.";
        }
        else
        {
            state.source_test_verified = false;
            output = state.queue_repair_after_verified_failure(
                status.empty() ? std::string{
                    "The build-safe engine contract test rejected the disposable source workspace."}
                    : std::move(status),
                "test");
        }
        output.status = state.status_message;
        return output;
    }

    RenderResult Panel::complete_source_release_build(
        std::uint32_t generation,
        bool succeeded,
        std::string status)
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }
        auto& state = *implementation_;
        if (generation != state.generation
            || !state.source_release_build_pending)
        {
            state.status_message =
                "A stale sandbox Release-compiler completion was ignored.";
            output.status = state.status_message;
            return output;
        }

        const std::string iterationEvidence = status.empty()
            ? std::string{"The Release compiler completion contained no diagnostic text."}
            : status;
        if (!state.record_iteration_validation(
                ai::iteration_session::ValidationActor::release_compiler,
                succeeded,
                iterationEvidence,
                false))
        {
            output.status = state.status_message;
            return output;
        }
        state.source_release_build_pending = false;
        if (succeeded)
        {
            state.source_release_build_verified = true;
            state.source_release_test_verified = false;
            state.status_message = status.empty()
                ? std::string{
                    "The host Release compiler accepted the disposable source workspace."}
                : std::move(status);
            state.source_release_test_pending = true;
            output.action = HostAction::test_source_release_workspace;
            output.source_root = state.source_root;
            output.workspace_root = state.workspace_root;
            output.workspace_generation = state.generation;
            state.status_message +=
                " Release compiler evidence passed; the Release build-safe engine contract test is queued as a distinct actor.";
        }
        else
        {
            state.source_release_build_verified = false;
            state.source_release_test_verified = false;
            output = state.queue_repair_after_verified_failure(
                status.empty()
                ? std::string{
                    "The host Release compiler rejected the disposable source workspace."}
                : std::move(status),
                "release compiler");
        }
        output.status = state.status_message;
        return output;
    }

    RenderResult Panel::complete_source_release_test(
        std::uint32_t generation,
        bool succeeded,
        std::string status)
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }
        auto& state = *implementation_;
        if (generation != state.generation
            || !state.source_release_test_pending)
        {
            state.status_message =
                "A stale sandbox Release-test completion was ignored.";
            output.status = state.status_message;
            return output;
        }

        const std::string iterationEvidence = status.empty()
            ? std::string{"The Release contract completion contained no diagnostic text."}
            : status;
        if (!state.record_iteration_validation(
                ai::iteration_session::ValidationActor::release_contract,
                succeeded,
                iterationEvidence,
                false))
        {
            output.status = state.status_message;
            return output;
        }
        state.source_release_test_pending = false;
        if (succeeded)
        {
            state.source_release_test_verified = true;
            state.source_headless_build_verified = false;
            state.source_headless_test_verified = false;
            state.source_full_validation_pending = false;
            state.source_full_validation_verified = false;
            state.status_message = status.empty()
                ? std::string{
                    "The Release build-safe engine contract test accepted the disposable source workspace."}
                : std::move(status);
            state.source_headless_build_pending = true;
            output.action = HostAction::compile_source_headless_workspace;
            output.source_root = state.source_root;
            output.workspace_root = state.workspace_root;
            output.workspace_generation = state.generation;
            state.status_message +=
                " Debug and Release compiler/contract evidence passed; the independent HeadlessCI build is queued.";
        }
        else
        {
            state.source_release_test_verified = false;
            state.source_headless_build_pending = false;
            state.source_headless_test_pending = false;
            state.source_headless_build_verified = false;
            state.source_headless_test_verified = false;
            state.source_full_validation_pending = false;
            state.source_full_validation_verified = false;
            output = state.queue_repair_after_verified_failure(
                status.empty() ? std::string{
                    "The Release build-safe engine contract test rejected the disposable source workspace."}
                    : std::move(status),
                "release test");
        }
        output.status = state.status_message;
        return output;
    }

    RenderResult Panel::complete_source_headless_build(
        std::uint32_t generation,
        bool succeeded,
        std::string status)
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }
        auto& state = *implementation_;
        if (generation != state.generation
            || !state.source_headless_build_pending)
        {
            state.status_message =
                "A stale sandbox HeadlessCI-compiler completion was ignored.";
            output.status = state.status_message;
            return output;
        }

        const std::string iterationEvidence = status.empty()
            ? std::string{"The HeadlessCI compiler completion contained no diagnostic text."}
            : status;
        if (!state.record_iteration_validation(
                ai::iteration_session::ValidationActor::headless_compiler,
                succeeded,
                iterationEvidence,
                false))
        {
            output.status = state.status_message;
            return output;
        }
        state.source_headless_build_pending = false;
        if (succeeded)
        {
            state.source_headless_build_verified = true;
            state.source_headless_test_verified = false;
            state.source_full_validation_pending = false;
            state.source_full_validation_verified = false;
            state.status_message = status.empty()
                ? std::string{
                    "The host HeadlessCI compiler accepted the disposable source workspace."}
                : std::move(status);
            state.source_headless_test_pending = true;
            output.action = HostAction::test_source_headless_workspace;
            output.source_root = state.source_root;
            output.workspace_root = state.workspace_root;
            output.workspace_generation = state.generation;
            state.status_message +=
                " HeadlessCI compiler evidence passed; its asset-light contract executable is queued as a distinct actor.";
        }
        else
        {
            state.source_headless_build_verified = false;
            state.source_headless_test_verified = false;
            state.source_full_validation_pending = false;
            state.source_full_validation_verified = false;
            output = state.queue_repair_after_verified_failure(
                status.empty()
                ? std::string{
                    "The host HeadlessCI compiler rejected the disposable source workspace."}
                : std::move(status),
                "HeadlessCI compiler");
        }
        output.status = state.status_message;
        return output;
    }

    RenderResult Panel::complete_source_headless_test(
        std::uint32_t generation,
        bool succeeded,
        std::string status)
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }
        auto& state = *implementation_;
        if (generation != state.generation
            || !state.source_headless_test_pending)
        {
            state.status_message =
                "A stale sandbox HeadlessCI-test completion was ignored.";
            output.status = state.status_message;
            return output;
        }

        const std::string iterationEvidence = status.empty()
            ? std::string{"The HeadlessCI contract completion contained no diagnostic text."}
            : status;
        if (!state.record_iteration_validation(
                ai::iteration_session::ValidationActor::headless_contract,
                succeeded,
                iterationEvidence,
                false))
        {
            output.status = state.status_message;
            return output;
        }
        state.source_headless_test_pending = false;
        if (succeeded)
        {
            state.source_headless_test_verified = true;
            state.source_full_validation_pending = false;
            state.source_full_validation_verified = false;
            state.status_message = status.empty()
                ? std::string{
                    "The HeadlessCI executable accepted the disposable source workspace."}
                : std::move(status);
            state.status_message +=
                " Debug/Release compiler contracts and HeadlessCI build/run passed. Full engine validation is available for explicit operator approval; live promotion remains locked.";
        }
        else
        {
            state.source_headless_test_verified = false;
            state.source_full_validation_pending = false;
            state.source_full_validation_verified = false;
            output = state.queue_repair_after_verified_failure(
                status.empty() ? std::string{
                    "The HeadlessCI executable rejected the disposable source workspace."}
                    : std::move(status),
                "HeadlessCI test");
        }
        output.status = state.status_message;
        return output;
    }

    RenderResult Panel::approve_source_full_validation()
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }

        auto& state = *implementation_;
        if (!state.source_build_verified
            || !state.source_test_verified
            || !state.source_release_build_verified
            || !state.source_release_test_verified
            || !state.source_headless_build_verified
            || !state.source_headless_test_verified
            || state.source_candidate_raw_reply.empty()
            || state.source_candidate_operations.empty())
        {
            state.status_message =
                "Debug/Release compiler contracts and HeadlessCI build/run must pass before full validation.";
            output.status = state.status_message;
            return output;
        }
        if (state.source_full_validation_verified)
        {
            state.status_message =
                "Full engine validation already passed for this candidate.";
            output.status = state.status_message;
            return output;
        }
        if (state.source_full_validation_pending)
        {
            state.status_message =
                "Full engine validation is already running for this candidate.";
            output.status = state.status_message;
            return output;
        }

        state.source_full_validation_pending = true;
        output.action = HostAction::test_source_full_validation_workspace;
        output.source_root = state.source_root;
        output.workspace_root = state.workspace_root;
        output.workspace_generation = state.generation;
        state.status_message =
            "Operator approved full engine validation in the disposable workspace. Live promotion remains locked until trusted completion evidence returns.";
        output.status = state.status_message;
        return output;
    }

    RenderResult Panel::complete_source_full_validation(
        std::uint32_t generation,
        bool succeeded,
        std::string status)
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }

        auto& state = *implementation_;
        if (generation != state.generation
            || !state.source_full_validation_pending)
        {
            state.status_message =
                "A stale sandbox full-validation completion was ignored.";
            output.status = state.status_message;
            return output;
        }

        const std::string iterationEvidence = status.empty()
            ? std::string{"The full-validation completion contained no diagnostic text."}
            : status;
        if (!state.record_iteration_validation(
                ai::iteration_session::ValidationActor::full_validation,
                succeeded,
                iterationEvidence,
                true))
        {
            output.status = state.status_message;
            return output;
        }
        state.source_full_validation_pending = false;
        if (succeeded)
        {
            state.source_full_validation_verified = true;
            state.status_message = status.empty()
                ? std::string{
                    "Full engine validation accepted the disposable source workspace."}
                : std::move(status);
            state.status_message +=
                " All required compiler, contract, HeadlessCI, project-profile, generated-child, and AI-gate evidence passed. The exact candidate may now be staged for separate live-source approval; analyzer, sanitizer, architecture, visual, and frontier adapters remain distinct.";
        }
        else
        {
            state.source_full_validation_verified = false;
            output = state.queue_repair_after_verified_failure(
                status.empty() ? std::string{
                    "Full engine validation rejected the disposable source workspace."}
                    : std::move(status),
                "full validation");
        }
        output.status = state.status_message;
        return output;
    }

    RenderResult Panel::stage_verified_source_promotion(const Input& input)
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }
        auto& state = *implementation_;
        (void)state.ensure(
            input.workspace_id,
            input.source_snapshot_root,
            input.workspace_root);
        if (input.execution_pending)
        {
            state.status_message =
                "Wait for the running guarded source task before staging live promotion.";
            output.status = state.status_message;
            return output;
        }
        if (state.source_promotion_staged && state.promotion_controller)
        {
            state.status_message =
                "The exact live-source promotion digest is already staged.";
            output.status = state.status_message;
            return output;
        }

        const auto verification = state.verify_source_candidate();
        if (!verification)
        {
            state.clear_verified_source_candidate();
            state.status_message = verification.status;
            output.status = state.status_message;
            return output;
        }

        const auto now = logical_time_now();
        std::uint64_t sessionValue =
            (now.value << 16u)
            ^ static_cast<std::uint64_t>(state.generation)
            ^ 0xa11e5u;
        if (sessionValue == 0u)
            sessionValue = 1u;
        state.promotion_controller = std::make_unique<
            editor_ai_development::DevelopmentController>(
                editor_ai_development::SessionConfiguration{
                    .session_value = sessionValue,
                    .session_generation = state.generation == 0u
                        ? 1u
                        : state.generation,
                    .workspace_id =
                        state.workspace_id + ".live-promotion",
                    .workspace_root = state.source_root,
                    .source_snapshot_root = state.source_root,
                    .engine_source_root = "Engine",
                    .project_source_root = "Projects",
                    .build_output_root = "Projects",
                    .evidence_root = "logs",
                    .operator_id = "epoch.operator",
                    .opened_at = now,
                    .expires_at = {now.value + 60u * 60u}});
        const auto proposed =
            state.promotion_controller->propose_model_reply(
                state.source_candidate_raw_reply,
                state.source_candidate_kind,
                now);
        if (!proposed)
        {
            state.promotion_controller.reset();
            state.status_message =
                "Live promotion staging failed closed: " + proposed.status;
            output.status = state.status_message;
            return output;
        }

        const auto snapshot = state.promotion_controller->snapshot();
        if (snapshot.operations != state.source_candidate_operations
            || snapshot.digest_hex.empty())
        {
            state.promotion_controller.reset();
            state.clear_verified_source_candidate();
            state.status_message =
                "Live promotion staging rejected a changed operation set or missing digest.";
            output.status = state.status_message;
            return output;
        }
        state.source_promotion_staged = true;
        state.status_message = epochengine::format_text(
            "Live promotion staged for {} exact file(s). Review digest {}... and use the separate Approve Live Promotion action.",
            snapshot.operations.size(),
            snapshot.digest_hex.substr(0u, 24u));
        output.status = state.status_message;
        return output;
    }

    RenderResult Panel::approve_and_promote_verified_source(
        const Input& input)
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }
        auto& state = *implementation_;
        if (input.execution_pending)
        {
            state.status_message =
                "Wait for the running guarded source task before live promotion.";
            output.status = state.status_message;
            return output;
        }
        if (!state.source_promotion_staged
            || !state.promotion_controller)
        {
            state.status_message =
                "Stage and review the verified live-source digest before approval.";
            output.status = state.status_message;
            return output;
        }

        const auto verification = state.verify_source_candidate();
        const auto staged = state.promotion_controller->snapshot();
        if (!verification
            || staged.operations != state.source_candidate_operations)
        {
            state.clear_verified_source_candidate();
            state.status_message = verification
                ? std::string{
                    "The staged live promotion no longer matches the reviewed operation set."}
                : verification.status;
            output.status = state.status_message;
            return output;
        }

        const auto now = logical_time_now();
        auto phase = staged.phase;
        if (phase == editor_ai_development::ControllerPhase::proposed)
        {
            const auto reviewed = state.promotion_controller->review(
                "epoch.operator",
                "Operator reviewed live preimages, compiler-tested sandbox postimages, and the exact promotion digest.",
                now);
            state.status_message = reviewed.status;
            if (!reviewed)
            {
                output.status = state.status_message;
                return output;
            }
            phase = state.promotion_controller->snapshot().phase;
        }
        if (phase == editor_ai_development::ControllerPhase::reviewed)
        {
            const auto approved = state.promotion_controller->approve(
                "epoch.operator",
                "Operator explicitly approves this exact verified candidate for live-source promotion.",
                now,
                {now.value + 10u * 60u});
            state.status_message = approved.status;
            if (!approved)
            {
                output.status = state.status_message;
                return output;
            }
            phase = state.promotion_controller->snapshot().phase;
        }
        if (phase == editor_ai_development::ControllerPhase::approved)
        {
            const auto authorized = state.promotion_controller->authorize(
                now,
                editor_ai_development::Duration{5u * 60u});
            state.status_message = authorized.status;
            if (!authorized)
            {
                output.status = state.status_message;
                return output;
            }
            phase = state.promotion_controller->snapshot().phase;
        }
        if (phase != editor_ai_development::ControllerPhase::authorized)
        {
            state.status_message =
                "The verified live promotion is not in an executable approval phase.";
            output.status = state.status_message;
            return output;
        }

        const auto report =
            state.promotion_controller
                ->execute_authorized_model_source_changes(now);
        state.source_promotion_staged = false;
        if (report)
        {
            const std::size_t promotedFiles =
                state.source_candidate_operations.size();
            state.source_candidate_raw_reply.clear();
            state.source_candidate_operations.clear();
            state.source_build_verified = false;
            state.source_test_verified = false;
            state.source_promotion_completed = true;
            state.status_message = epochengine::format_text(
                "Promoted {} compiler-and-contract-verified file(s) into live source through the guarded atomic transaction. No Git, release, updater, or automatic follow-on authority was granted.",
                promotedFiles);
        }
        else
        {
            const std::string failure = report.transaction_status.empty()
                ? report.controller_result.status
                : report.controller_result.status + " "
                    + report.transaction_status;
            state.clear_verified_source_candidate();
            state.status_message =
                "Live promotion failed closed: " + failure;
        }
        output.status = state.status_message;
        return output;
    }

    RenderResult Panel::discard_verified_source_candidate()
    {
        RenderResult output{};
        if (!implementation_)
        {
            output.status = "Guarded development panel is unavailable.";
            return output;
        }
        auto& state = *implementation_;
        if (state.promotion_controller)
        {
            const auto phase = state.promotion_controller->snapshot().phase;
            if (phase == editor_ai_development::ControllerPhase::proposed
                || phase == editor_ai_development::ControllerPhase::reviewed
                || phase == editor_ai_development::ControllerPhase::approved
                || phase == editor_ai_development::ControllerPhase::authorized)
            {
                (void)state.promotion_controller->cancel(
                    "epoch.operator",
                    "Operator discarded the verified live-source candidate.",
                    logical_time_now());
            }
        }
        state.clear_verified_source_candidate();
        state.status_message =
            "The verified sandbox candidate was discarded; live source was not changed.";
        output.status = state.status_message;
        return output;
    }

    RenderResult Panel::cancel_staged_proposal()
    {
        RenderResult output{};
        if (!implementation_ || !implementation_->controller)
        {
            output.status = "No guarded source proposal is active.";
            return output;
        }
        auto& state = *implementation_;
        const auto cancelled = state.controller->cancel(
            "epoch.operator",
            "Operator cancelled the source proposal beside its AI Chat response.",
            logical_time_now());
        state.status_message = cancelled.status;
        output.status = state.status_message;
        return output;
    }

    void Panel::complete_tool_harness(
        bool succeeded,
        std::string buildSummary,
        std::string runtimeSummary)
    {
        if (!implementation_ || !implementation_->controller)
            return;
        auto& state = *implementation_;
        const auto result = state.controller->complete(
            editor_ai_development::CompletionRequest{
                .actor_id = "epoch.operator",
                .outcome = succeeded
                    ? editor_ai_development::CompletionOutcome::succeeded
                    : editor_ai_development::CompletionOutcome::failed,
                .summary = succeeded
                    ? "Approved tool harness built and ran successfully."
                    : "Approved tool harness failed; inspect verified evidence.",
                .evidence = {
                    editor_ai_development::EvidenceItem{
                        .kind = editor_ai_development::EvidenceKind::build_log,
                        .locator = "logs/guarded_ai_harness.build",
                        .content_digest = editor_ai_development::evidence_digest(
                            buildSummary),
                        .summary = std::move(buildSummary),
                        .verified = true
                    },
                    editor_ai_development::EvidenceItem{
                        .kind = editor_ai_development::EvidenceKind::runtime_report,
                        .locator = "logs/guarded_ai_harness.runtime",
                        .content_digest = editor_ai_development::evidence_digest(
                            runtimeSummary),
                        .summary = std::move(runtimeSummary),
                        .verified = true
                    }
                }
            },
            logical_time_now());
        state.status_message = result.status;
    }
}
