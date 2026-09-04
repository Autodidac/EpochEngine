/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

export module ai.iteration_campaign;

export import ai.iteration_session;
export import ai.iteration_campaign_queue;

export namespace epochengine::ai::iteration_campaign
{
    struct CampaignBudgets final
    {
        std::uint32_t maximum_candidates{8u};
        std::uint32_t maximum_model_calls{32u};
        std::uint32_t maximum_source_operations{32u};
        std::uint32_t maximum_repairs_per_candidate{3u};
        std::uint32_t maximum_changes_per_candidate{4u};
        std::uint32_t maximum_curated_files{
            static_cast<std::uint32_t>(iteration_session::kMaximumCuratedFiles)};
        std::uint64_t maximum_context_bytes{184u * 1024u};
        std::uint64_t maximum_candidate_bytes{1024u * 1024u};
        std::uint64_t maximum_file_bytes{16u * 1024u * 1024u};
        std::uint64_t maximum_transaction_bytes{64u * 1024u * 1024u};
        std::uint32_t maximum_workspace_files{4096u};
        std::uint64_t maximum_workspace_bytes{512u * 1024u * 1024u};
        std::uint32_t maximum_validation_records{56u};

        friend bool operator==(const CampaignBudgets&, const CampaignBudgets&) = default;
    };

    struct CampaignCounters final
    {
        std::uint32_t candidates{};
        std::uint32_t model_calls{};
        std::uint32_t source_operations{};
        std::uint32_t validation_records{};

        friend bool operator==(const CampaignCounters&, const CampaignCounters&) = default;
    };

    enum class BudgetKind : std::uint8_t
    {
        candidate,
        model_call,
        source_operation,
        validation_record
    };

    enum class ValidationStep : std::uint8_t
    {
        debug_compiler,
        debug_contract,
        release_compiler,
        release_contract,
        headless_compiler,
        headless_contract,
        full_validation,
        project_compiler,
        project_contract
    };

    struct CampaignConfiguration final
    {
        iteration_session::SourceAuthority authority{};
        std::string objective{};
        std::string model_name{};
        std::vector<iteration_session::CuratedFile> curated_files{};
        iteration_session::CandidatePolicy policy{
            iteration_session::CandidatePolicy::manual_each_candidate};
        CampaignBudgets budgets{};
        std::uint64_t created_at_unix_seconds{};
        std::uint64_t duration_seconds{8u * 60u * 60u};
        bool auto_validation_permit{};
    };

    struct CampaignReport final
    {
        std::uint64_t record_generation{1u};
        std::string previous_state_digest{};
        std::string state_digest{};
        std::string campaign_id{};
        std::string target_key{};
        std::uint64_t created_at_unix_seconds{};
        std::uint64_t expires_at_unix_seconds{};
        CampaignBudgets budgets{};
        CampaignCounters counters{};
        iteration_session::CandidateReport session{};
        std::string status{};
        bool cancelled{};
    };

    struct CampaignResult final
    {
        bool accepted{};
        CampaignReport report{};
        std::filesystem::path state_path{};
        std::string state_digest{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return accepted;
        }
    };

    [[nodiscard]] CampaignBudgets default_budgets(
        iteration_session::IterationTargetKind target_kind) noexcept;
    [[nodiscard]] std::vector<ValidationStep> validation_plan(
        iteration_session::IterationTargetKind target_kind);
    [[nodiscard]] CampaignResult begin_campaign(
        CampaignConfiguration configuration,
        iteration_session::IterationSession& session);
    [[nodiscard]] CampaignResult consume_budget(
        CampaignReport report,
        BudgetKind kind,
        std::uint32_t amount = 1u);
    [[nodiscard]] std::filesystem::path state_path(
        const std::filesystem::path& cache_root,
        const CampaignReport& report);
    [[nodiscard]] CampaignResult save_report(
        const std::filesystem::path& cache_root,
        const CampaignReport& report);
    [[nodiscard]] CampaignResult load_report(
        const std::filesystem::path& report_path);
    [[nodiscard]] CampaignResult resume_campaign(
        CampaignReport report,
        const iteration_session::SourceAuthority& current_authority,
        std::vector<iteration_session::CuratedFile> current_files,
        std::uint64_t now_unix_seconds,
        iteration_session::IterationSession& session,
        bool auto_validation_permit = false);
    [[nodiscard]] bool run_contract();
}
