/************************************************
 * Epoch Engine - bounded agent role and escalation policy
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
// Epoch Editor: keep editor state and source changes within their owning workflows.
#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace epochengine::editor_ai_agent_routing
{
    enum class Role : std::uint8_t
    {
        local,
        codex,
        disabled
    };

    enum class Stage : std::uint8_t
    {
        planning,
        implementation,
        build_diagnosis,
        documentation,
        visual_review,
        final_review,
        count
    };

    enum class Escalation : std::uint8_t
    {
        manual,
        after_failures,
        every_docked_candidate
    };

    enum class CandidateExecution : std::uint8_t
    {
        require_isolation,
        local_testing
    };

    inline constexpr std::array<std::string_view, static_cast<std::size_t>(Stage::count)>
        stage_names{{"Planning", "Source implementation", "Failed build / tests",
            "Documentation", "Visual inspection", "Final candidate review"}};

    struct Settings final
    {
        // Only review tasks can be delegated by the app-server integration.
        // All implementation and build authority remains with the existing
        // guarded Epoch pipeline regardless of saved UI preferences.
        std::array<Role, static_cast<std::size_t>(Stage::count)> stages{
            Role::local, Role::local, Role::local,
            Role::local, Role::local, Role::codex};
        Escalation escalation{Escalation::after_failures};
        std::uint32_t failure_threshold{3u};
        std::uint32_t max_codex_reviews_per_candidate{1u};
        bool enabled{true};
        // Explicit opt-in: even subscription-authenticated Codex consumes
        // limited provider quota; API-key authentication may incur charges.
        // New installations and migrated settings never initiate paid calls
        // without the operator enabling automatic escalation.
        bool automatic_codex_approved{false};
        // Consent applies to this exact model. Changing the reviewer cannot
        // silently transfer a previous authorization to another paid model.
        std::string automatic_codex_model{};
        bool use_source_recipes{true};
        // Host preference, never supplied by a model or inherited from a
        // candidate. Local testing uses supervised processes, not a VM.
        CandidateExecution candidate_execution{CandidateExecution::local_testing};
        // Codex is a bounded escalation agent. The selected model must also
        // be present in app-server's model/list response before inference.
        std::string codex_model{"gpt-6.1-sol"};
        std::string codex_effort{"low"};
        std::string codex_summary{"concise"};
    };

    [[nodiscard]] inline bool safe_model_id(std::string_view model) noexcept
    {
        if (model.empty() || model.size() > 128u) return false;
        for (const unsigned char value : model)
            if (!((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z')
                || (value >= '0' && value <= '9') || value == '-'
                || value == '_' || value == '.' || value == ':' || value == '/'))
                return false;
        return true;
    }

    [[nodiscard]] inline bool valid_effort(std::string_view effort) noexcept
    {
        return effort == "none" || effort == "low" || effort == "medium"
            || effort == "high" || effort == "xhigh" || effort == "max";
    }

    [[nodiscard]] inline bool valid_summary(std::string_view summary) noexcept
    {
        return summary == "auto" || summary == "concise"
            || summary == "detailed" || summary == "none";
    }

    // One actionable failure per edited generation, not one per Debug,
    // Release, headless and contract lane. Environment/toolchain failures
    // do not spend the escalation budget.
    [[nodiscard]] inline bool record_local_failure(
        std::uint32_t& count, std::uint32_t& previousGeneration,
        bool& hasPreviousGeneration, std::uint32_t generation,
        bool environmentFailure) noexcept
    {
        if (environmentFailure
            || (hasPreviousGeneration && previousGeneration == generation))
            return false;
        previousGeneration = generation;
        hasPreviousGeneration = true;
        count = (std::min)(count + 1u, 10u);
        return true;
    }

    [[nodiscard]] inline bool automatic_audit_enabled(
        const Settings& settings, std::uint32_t localFailures,
        bool alreadyAudited, std::uint32_t reviews = 0u) noexcept
    {
        return settings.enabled && settings.automatic_codex_approved
            && settings.automatic_codex_model == settings.codex_model
            && settings.escalation == Escalation::after_failures
            && settings.stages[static_cast<std::size_t>(Stage::final_review)]
                == Role::codex
            && settings.max_codex_reviews_per_candidate > 0u
            && reviews < settings.max_codex_reviews_per_candidate
            && !alreadyAudited
            && localFailures >= (std::max)(settings.failure_threshold, 1u);
    }

    [[nodiscard]] inline bool final_review_enabled(const Settings& settings,
        std::uint32_t localFailures, std::uint32_t reviews,
        bool candidateDocked, bool pendingHumanDecision) noexcept
    {
        if (!settings.enabled || !settings.automatic_codex_approved
            || settings.automatic_codex_model != settings.codex_model
            || !candidateDocked || !pendingHumanDecision
            || settings.stages[static_cast<std::size_t>(Stage::final_review)]
                != Role::codex
            || settings.max_codex_reviews_per_candidate == 0u
            || reviews >= settings.max_codex_reviews_per_candidate)
            return false;
        switch (settings.escalation)
        {
        case Escalation::after_failures:
            return localFailures >= (std::max)(settings.failure_threshold, 1u);
        case Escalation::every_docked_candidate:
            return true;
        case Escalation::manual:
        default:
            return false;
        }
    }

    [[nodiscard]] inline bool manual_review_enabled(
        const Settings& settings, std::uint32_t reviews) noexcept
    {
        return settings.enabled
            && settings.stages[static_cast<std::size_t>(Stage::final_review)]
                == Role::codex
            && reviews < settings.max_codex_reviews_per_candidate;
    }

    [[nodiscard]] inline bool save(const Settings& settings,
        const std::filesystem::path& target)
    {
        if (target.empty()
            || static_cast<unsigned>(settings.candidate_execution)
                > static_cast<unsigned>(CandidateExecution::local_testing)) return false;
        std::error_code error{};
        std::filesystem::create_directories(target.parent_path(), error);
        if (error) return false;
        const auto temp = target.parent_path() /
            (target.filename().string() + ".pending");
        {
            std::ofstream out{temp, std::ios::binary | std::ios::trunc};
            if (!out) return false;
            out << "epoch-ai-roles-v6\n";
            out << static_cast<unsigned>(settings.escalation) << '\n';
            out << (std::clamp)(settings.failure_threshold, 1u, 10u) << '\n';
            out << (std::min)(settings.max_codex_reviews_per_candidate, 3u) << '\n';
            out << (settings.enabled ? 1 : 0) << '\n';
            for (const auto stage : settings.stages)
                out << static_cast<unsigned>(stage) << '\n';
            if (!safe_model_id(settings.codex_model)
                || !valid_effort(settings.codex_effort)
                || !valid_summary(settings.codex_summary)) return false;
            out << settings.codex_model << '\n' << settings.codex_effort
                << '\n' << settings.codex_summary << '\n';
            out << (settings.automatic_codex_approved ? 1 : 0) << '\n';
            out << static_cast<unsigned>(settings.candidate_execution) << '\n';
            out << (settings.use_source_recipes ? 1 : 0) << '\n';
            if (!settings.automatic_codex_model.empty()
                && !safe_model_id(settings.automatic_codex_model)) return false;
            out << settings.automatic_codex_model << '\n';
            out.flush();
            if (!out) return false;
        }
        // Existing user settings may be replaced, never source files. The
        // temporary file is local to the same directory for atomic rename.
#if defined(_WIN32)
        const bool replaced = ::MoveFileExW(temp.c_str(), target.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (!replaced) std::filesystem::remove(temp, error);
        return replaced;
#else
        std::filesystem::rename(temp, target, error);
        if (error) std::filesystem::remove(temp, error);
        return !error;
#endif
    }

    [[nodiscard]] inline Settings load(const std::filesystem::path& source)
    {
        Settings output{};
        std::ifstream in{source, std::ios::binary};
        if (!in) return output;
        std::string line{};
        if (!std::getline(in, line)) return output;
        const bool version6 = line == "epoch-ai-roles-v6";
        const bool version5 = version6 || line == "epoch-ai-roles-v5";
        const bool version4 = version5 || line == "epoch-ai-roles-v4";
        const bool version3 = version4 || line == "epoch-ai-roles-v3";
        const bool extended = version3 || line == "epoch-ai-roles-v2";
        if (!extended && line != "epoch-ai-roles-v1") return output;
        const auto read_number = [&]() -> std::uint32_t
        {
            if (!std::getline(in, line)) { in.setstate(std::ios::failbit); return 0u; }
            std::uint32_t number{};
            const auto [ptr, error] = std::from_chars(line.data(), line.data() + line.size(), number);
            if (error != std::errc{} || ptr != line.data() + line.size())
                in.setstate(std::ios::failbit);
            return number;
        };
        const auto escalation = read_number();
        const auto threshold = read_number();
        const auto maxReviews = read_number();
        const auto enabled = read_number();
        for (auto& stage : output.stages)
        {
            const auto value = read_number();
            if (value > static_cast<unsigned>(Role::disabled))
                in.setstate(std::ios::failbit);
            else stage = static_cast<Role>(value);
        }
        if (in && extended)
        {
            if (!std::getline(in, output.codex_model)
                || !std::getline(in, output.codex_effort)
                || !std::getline(in, output.codex_summary)
                || !safe_model_id(output.codex_model)
                || !valid_effort(output.codex_effort)
                || !valid_summary(output.codex_summary)) return Settings{};
        }
        if (in && version3)
        {
            const auto approved = read_number();
            if (approved > 1u) return Settings{};
            output.automatic_codex_approved = approved == 1u;
        }
        if (in && version4)
        {
            const auto execution = read_number();
            if (execution > static_cast<unsigned>(CandidateExecution::local_testing))
                return Settings{};
            output.candidate_execution = static_cast<CandidateExecution>(execution);
        }
        if (!in || escalation > static_cast<unsigned>(Escalation::every_docked_candidate)
            || enabled > 1u)
            return Settings{};
        if (version5)
        {
            const auto recipes = read_number();
            if (!in || recipes > 1u) return Settings{};
            output.use_source_recipes = recipes == 1u;
        }
        if (version6)
        {
            if (!std::getline(in, output.automatic_codex_model)
                || (!output.automatic_codex_model.empty()
                    && !safe_model_id(output.automatic_codex_model))) return Settings{};
        }
        else if (output.automatic_codex_approved)
            output.automatic_codex_model = output.codex_model;
        output.escalation = static_cast<Escalation>(escalation);
        output.failure_threshold = (std::clamp)(threshold, 1u, 10u);
        output.max_codex_reviews_per_candidate = (std::min)(maxReviews, 3u);
        output.enabled = enabled == 1u;
        // Codex may not be assigned to implementation, builds or tooling in
        // this version; the UI controls supported *execution* roles, not
        // unsupported pretend actions.
        for (std::size_t n{}; n < static_cast<std::size_t>(Stage::final_review); ++n)
            output.stages[n] = Role::local;
        return output;
    }
}
