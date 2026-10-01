/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <locale>
#include <string>
#include <string_view>

namespace epochengine::ai::self_iteration_analytics
{
    struct Record final
    {
        std::uint64_t unix_seconds{};
        std::string event{};
        std::string outcome{"pending"};
        std::string label{"pending"};
        std::string session_key{};
        std::string objective_sha256{};
        std::string status_sha256{};
        std::uint64_t candidate_iteration{};
        std::uint64_t pass_iteration{};
        std::uint64_t total_coding_passes{};
        std::uint32_t controller_generation{};
        std::size_t reviewed_file_count{};
        std::size_t materialized_file_count{};
        std::uint64_t materialized_bytes{};
        std::size_t context_expansions{};
        std::size_t navigation_fallbacks{};
        std::size_t reasoning_only_recoveries{};
        std::size_t provider_recoveries{};
        std::size_t source_corrections{};
        std::size_t plan_corrections{};
        std::size_t repair_attempts{};
        int system1_top_source_score{};
        int system1_source_score_margin{};
        double system1_source_confidence{};
        bool workspace_ready{};
        bool debug_build_verified{};
        bool debug_test_verified{};
        bool release_build_verified{};
        bool release_test_verified{};
        bool headless_build_verified{};
        bool headless_test_verified{};
        bool full_validation_verified{};
        bool candidate_preview_ready{};
        // Reserved now so a later EngCoder/JEV-style predictor can add image
        // features without changing the base analytics schema.
        std::size_t visual_evidence_count{};
        std::string visual_feature_schema{"none"};
    };

    [[nodiscard]] inline std::string escape_json(const std::string_view value)
    {
        std::string output{};
        output.reserve(value.size() + 8u);
        constexpr char hex[] = "0123456789abcdef";
        for (const unsigned char c : value)
        {
            switch (c)
            {
            case '\\': output += "\\\\"; break;
            case '"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (c < 0x20u)
                {
                    output += "\\u00";
                    output.push_back(hex[(c >> 4u) & 0x0fu]);
                    output.push_back(hex[c & 0x0fu]);
                }
                else
                {
                    output.push_back(static_cast<char>(c));
                }
                break;
            }
        }
        return output;
    }

    [[nodiscard]] inline bool append_jsonl(
        const std::filesystem::path& workspace_root,
        const Record& record) noexcept
    {
        if (workspace_root.empty() || !workspace_root.is_absolute())
            return false;

        try
        {
            const auto log_root = workspace_root / "logs";
            std::error_code error{};
            std::filesystem::create_directories(log_root, error);
            if (error) return false;

            std::ofstream stream{
                log_root / "self_iteration_analytics.jsonl",
                std::ios::binary | std::ios::app};
            if (!stream) return false;
            stream.imbue(std::locale::classic());

            const auto q = [](const std::string_view value)
            {
                return std::string{"\""} + escape_json(value) + "\"";
            };
            const auto b = [](const bool value) -> std::string_view
            {
                return value ? "true" : "false";
            };

            stream
                << "{\"schema\":\"epoch.self_iteration.analytics.v1\""
                << ",\"unix_seconds\":" << record.unix_seconds
                << ",\"event\":" << q(record.event)
                << ",\"outcome\":" << q(record.outcome)
                << ",\"label\":" << q(record.label)
                << ",\"session_key\":" << q(record.session_key)
                << ",\"objective_sha256\":" << q(record.objective_sha256)
                << ",\"status_sha256\":" << q(record.status_sha256)
                << ",\"candidate_iteration\":" << record.candidate_iteration
                << ",\"pass_iteration\":" << record.pass_iteration
                << ",\"total_coding_passes\":" << record.total_coding_passes
                << ",\"controller_generation\":" << record.controller_generation
                << ",\"reviewed_file_count\":" << record.reviewed_file_count
                << ",\"materialized_file_count\":" << record.materialized_file_count
                << ",\"materialized_bytes\":" << record.materialized_bytes
                << ",\"context_expansions\":" << record.context_expansions
                << ",\"navigation_fallbacks\":" << record.navigation_fallbacks
                << ",\"reasoning_only_recoveries\":" << record.reasoning_only_recoveries
                << ",\"provider_recoveries\":" << record.provider_recoveries
                << ",\"source_corrections\":" << record.source_corrections
                << ",\"plan_corrections\":" << record.plan_corrections
                << ",\"repair_attempts\":" << record.repair_attempts
                << ",\"system1_top_source_score\":" << record.system1_top_source_score
                << ",\"system1_source_score_margin\":" << record.system1_source_score_margin
                << ",\"system1_source_confidence\":" << record.system1_source_confidence
                << ",\"workspace_ready\":" << b(record.workspace_ready)
                << ",\"debug_build_verified\":" << b(record.debug_build_verified)
                << ",\"debug_test_verified\":" << b(record.debug_test_verified)
                << ",\"release_build_verified\":" << b(record.release_build_verified)
                << ",\"release_test_verified\":" << b(record.release_test_verified)
                << ",\"headless_build_verified\":" << b(record.headless_build_verified)
                << ",\"headless_test_verified\":" << b(record.headless_test_verified)
                << ",\"full_validation_verified\":" << b(record.full_validation_verified)
                << ",\"candidate_preview_ready\":" << b(record.candidate_preview_ready)
                << ",\"visual_evidence_count\":" << record.visual_evidence_count
                << ",\"visual_feature_schema\":" << q(record.visual_feature_schema)
                << ",\"predictor_score\":null}\n";
            return static_cast<bool>(stream);
        }
        catch (...)
        {
            return false;
        }
    }
}
