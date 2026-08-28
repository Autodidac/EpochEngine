/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module ai.self_iteration_orchestrator;

export import ai.project_profile;
export import ai.iteration_campaign;
export import ai.mcp_child_host;

export namespace epochengine::ai::self_iteration_orchestrator
{
    enum class TransportKind : std::uint8_t
    {
        guarded_local_mcp_child,
        external_mcp
    };

    enum class Phase : std::uint8_t
    {
        idle,
        awaiting_plan_request,
        awaiting_plan_result,
        awaiting_curated_evidence,
        awaiting_proposal_request,
        awaiting_proposal_result,
        awaiting_manual_review,
        awaiting_apply_decision,
        awaiting_apply_result,
        awaiting_validation_request,
        awaiting_validation_result,
        checkpoint_ready,
        checkpointed,
        rejected,
        cancelled,
        blocked
    };

    enum class OperationKind : std::uint8_t
    {
        model_plan,
        model_proposal,
        sandbox_apply,
        trusted_validation
    };

    enum class EvidenceKind : std::uint8_t
    {
        configured,
        plan_requested,
        plan_received,
        curated_context,
        proposal_requested,
        proposal_received,
        manual_review,
        apply_decision,
        implementation,
        validation,
        checkpoint,
        resumed,
        cancelled
    };

    struct HostBinding final
    {
        std::string binding_id{};
        std::string configuration_sha256{};
        std::uint64_t generation{};
        bool stdio_only{true};
        bool automatic_launch{};
        bool network_enabled{};
        bool listener_enabled{};
        bool server_enabled{};

        friend bool operator==(const HostBinding&, const HostBinding&) = default;
    };

    struct Limits final
    {
        std::size_t maximum_evidence_records{128u};
        std::size_t maximum_summary_bytes{2048u};
        std::size_t maximum_plan_bytes{128u * 1024u};
        std::size_t maximum_state_bytes{512u * 1024u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_evidence_records >= 16u
                && maximum_evidence_records <= 256u
                && maximum_summary_bytes >= 64u
                && maximum_summary_bytes <= 4096u
                && maximum_plan_bytes >= 4096u
                && maximum_plan_bytes <= 256u * 1024u
                && maximum_state_bytes >= 64u * 1024u
                && maximum_state_bytes <= 1024u * 1024u;
        }
    };

    struct Configuration final
    {
        iteration_session::SourceAuthority authority{};
        std::vector<iteration_session::CuratedFile> curated_files{};
        std::filesystem::path cache_root{};
        std::string objective{};
        project_profile::Provider provider{project_profile::Provider::disabled};
        std::string project_profile_bytes{};
        std::string engine_model_binding{};
        std::string operator_model_binding{};
        HostBinding host{};
        iteration_campaign::CampaignBudgets budgets{};
        std::uint64_t created_at_unix_seconds{};
        std::uint64_t duration_seconds{8u * 60u * 60u};
        bool engine_source_campaign_permitted{};
        bool project_source_campaign_permitted{};
        bool sandbox_apply_permitted{};
    };

    struct ActionToken final
    {
        std::uint64_t expected_generation{};
        std::string expected_state_sha256{};
        std::string transition_id{};
        std::uint64_t now_unix_seconds{};
        bool operator_approved{};
    };

    struct OperationReceipt final
    {
        std::string operation_id{};
        std::uint64_t expected_generation{};
        std::string expected_state_sha256{};
        std::string transition_id{};
        std::uint64_t now_unix_seconds{};
    };

    struct EvidenceRecord final
    {
        std::uint64_t sequence{};
        EvidenceKind kind{EvidenceKind::configured};
        std::string transition_id{};
        std::string evidence_sha256{};
        std::string summary{};
        bool passed{};
    };

    class PendingOperation final
    {
    public:
        [[nodiscard]] OperationKind kind() const noexcept;
        [[nodiscard]] const std::string& operation_id() const noexcept;
        [[nodiscard]] const std::string& orchestrator_id() const noexcept;
        [[nodiscard]] const std::string& campaign_id() const noexcept;
        [[nodiscard]] iteration_session::RequestIdentity session_identity() const noexcept;
        [[nodiscard]] iteration_session::IterationTargetKind target_kind() const noexcept;
        [[nodiscard]] TransportKind transport() const noexcept;
        [[nodiscard]] const HostBinding& host_binding() const noexcept;
        [[nodiscard]] std::uint64_t expected_generation() const noexcept;
        [[nodiscard]] const std::string& expected_state_sha256() const noexcept;
        [[nodiscard]] const std::string& transition_id() const noexcept;
        [[nodiscard]] const std::string& objective_sha256() const noexcept;
        [[nodiscard]] const std::string& scope_sha256() const noexcept;
        [[nodiscard]] const std::string& plan_sha256() const noexcept;
        [[nodiscard]] const std::string& proposal_sha256() const noexcept;
        [[nodiscard]] const std::string& candidate_sha256() const noexcept;
        [[nodiscard]] std::uint32_t validation_index() const noexcept;
        [[nodiscard]] iteration_session::ValidationActor validation_actor() const noexcept;

    private:
        friend class Orchestrator;
        OperationKind kind_{OperationKind::model_plan};
        std::string operation_id_{};
        std::string orchestrator_id_{};
        std::string campaign_id_{};
        iteration_session::RequestIdentity session_identity_{};
        iteration_session::IterationTargetKind target_kind_{
            iteration_session::IterationTargetKind::engine_source};
        TransportKind transport_{TransportKind::guarded_local_mcp_child};
        HostBinding host_binding_{};
        std::uint64_t expected_generation_{};
        std::string expected_state_sha256_{};
        std::string transition_id_{};
        std::string objective_sha256_{};
        std::string scope_sha256_{};
        std::string plan_sha256_{};
        std::string proposal_sha256_{};
        std::string candidate_sha256_{};
        std::uint32_t validation_index_{};
        iteration_session::ValidationActor validation_actor_{
            iteration_session::ValidationActor::debug_compiler};
    };

    struct Snapshot final
    {
        std::uint64_t generation{};
        std::string previous_state_sha256{};
        std::string state_sha256{};
        std::string orchestrator_id{};
        std::string target_key{};
        Phase phase{Phase::idle};
        TransportKind transport{TransportKind::guarded_local_mcp_child};
        project_profile::Provider provider{project_profile::Provider::disabled};
        HostBinding host{};
        iteration_campaign::CampaignReport campaign{};
        std::string profile_sha256{};
        std::string plan_sha256{};
        std::string proposal_sha256{};
        std::string candidate_sha256{};
        std::string pending_operation_id{};
        OperationKind pending_operation_kind{OperationKind::model_plan};
        std::string last_transition_id{};
        std::uint32_t validation_index{};
        std::vector<EvidenceRecord> evidence{};
        std::string status{};
        bool manual_approval_required{true};
        bool live_source_read_only{true};
        bool promotion_permitted{};
        bool git_permitted{};
        bool release_permitted{};
        bool network_or_server_permitted{};
    };

    struct Result final
    {
        bool accepted{};
        Snapshot snapshot{};
        std::filesystem::path state_path{};
        std::optional<PendingOperation> pending_operation{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return accepted;
        }
    };

    class Orchestrator final
    {
    public:
        [[nodiscard]] Result begin(Configuration configuration, Limits limits = {});
        [[nodiscard]] Result resume(
            Configuration configuration,
            const std::filesystem::path& state_path,
            std::uint64_t now_unix_seconds,
            Limits limits = {});
        [[nodiscard]] Result request_plan(ActionToken action);
        [[nodiscard]] Result record_plan(
            OperationReceipt receipt,
            std::string_view plan_bytes,
            std::string summary);
        [[nodiscard]] Result share_curated_evidence(
            ActionToken action,
            std::string scope_sha256,
            std::string evidence_sha256,
            std::string summary);
        [[nodiscard]] Result request_proposal(ActionToken action);
        [[nodiscard]] Result record_proposal(
            OperationReceipt receipt,
            std::string_view proposal_bytes,
            std::string summary);
        [[nodiscard]] Result review_proposal(
            ActionToken action,
            bool approve,
            std::string reason);
        [[nodiscard]] Result decide_apply(
            ActionToken action,
            bool approve,
            std::string reason);
        [[nodiscard]] Result record_apply(
            OperationReceipt receipt,
            std::string implementation_evidence_sha256,
            std::string summary);
        [[nodiscard]] Result request_validation(ActionToken action);
        [[nodiscard]] Result record_validation(
            OperationReceipt receipt,
            std::string evidence_sha256,
            std::string summary,
            bool passed);
        [[nodiscard]] Result checkpoint(ActionToken action, std::string summary);
        [[nodiscard]] Result cancel(ActionToken action, std::string reason);
        [[nodiscard]] Snapshot snapshot() const;
        [[nodiscard]] std::filesystem::path state_path() const;

    private:
        [[nodiscard]] Result reject(std::string status) const;
        [[nodiscard]] Result commit(
            Phase phase,
            EvidenceKind evidence_kind,
            std::string transition_id,
            std::string evidence_sha256,
            std::string summary,
            bool passed,
            bool campaign_already_advanced = false);
        [[nodiscard]] Result create_operation(
            ActionToken action,
            OperationKind kind,
            Phase waiting_phase,
            EvidenceKind evidence_kind,
            iteration_campaign::BudgetKind budget_kind,
            std::string summary);
        [[nodiscard]] Result persist(std::string status);
        [[nodiscard]] bool action_matches(const ActionToken& action) const;
        [[nodiscard]] bool receipt_matches(const OperationReceipt& receipt) const;
        [[nodiscard]] PendingOperation make_pending_operation() const;
        [[nodiscard]] bool validate_configuration(
            const Configuration& configuration,
            std::string& model_name,
            std::string& profile_sha256,
            TransportKind& transport,
            std::string& status) const;

        Snapshot snapshot_{};
        std::filesystem::path cache_root_{};
        std::filesystem::path state_path_{};
        Limits limits_{};
        iteration_session::IterationSession session_{};
        bool configured_{};
    };

    [[nodiscard]] bool run_contract();
}
