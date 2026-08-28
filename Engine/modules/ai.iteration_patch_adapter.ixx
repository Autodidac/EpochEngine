/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module ai.iteration_patch_adapter;

export import ai.source_patch_bundle;
export import ai.self_iteration_orchestrator;
export import ai.mcp_orchestrator_bridge;

export namespace epochengine::ai::iteration_patch_adapter
{
    inline constexpr std::uint32_t schema_version = 1u;

    struct FileReview final
    {
        std::string relative_path{};
        source_patch_bundle::Operation operation{
            source_patch_bundle::Operation::update};
        source_patch_bundle::FileState before{};
        source_patch_bundle::FileState after{};
        std::vector<source_patch_bundle::HunkEvidence> hunks{};
        std::string unified_diff{};
        std::string evidence_sha256{};
    };

    struct ReviewArtifact final
    {
        std::string orchestrator_id{};
        std::string campaign_id{};
        std::uint64_t campaign_generation{};
        iteration_session::RequestIdentity session_identity{};
        iteration_session::IterationTargetKind target_kind{
            iteration_session::IterationTargetKind::engine_source};
        std::uint64_t orchestrator_generation{};
        std::string state_sha256{};
        std::string scope_sha256{};
        std::string proposal_sha256{};
        std::string candidate_sha256{};
        std::string authority_sha256{};
        std::string bundle_sha256{};
        std::string review_evidence_sha256{};
        std::vector<FileReview> files{};
        std::string canonical_diff{};
        bool operator_review_required{true};
        bool promotion_permitted{};
    };

    struct ApprovedReview final
    {
        ReviewArtifact artifact{};
        std::uint64_t orchestrator_generation{};
        std::string state_sha256{};
        std::string review_transition_id{};
    };

    struct ReceiptBinding final
    {
        std::string receipt_id{};
        mcp_orchestrator_bridge::MutationKind kind{
            mcp_orchestrator_bridge::MutationKind::decide_apply};
        std::string transport_session_id{};
        std::string connection_id{};
        std::uint64_t connection_generation{};
        std::string request_key{};
        std::string call_id{};
        std::string orchestrator_id{};
        std::string campaign_id{};
        std::uint64_t campaign_generation{};
        iteration_session::RequestIdentity session_identity{};
        std::uint64_t expected_orchestrator_generation{};
        std::string expected_state_sha256{};
        mcp_orchestrator_bridge::MutationIntent intent{};
    };

    struct ApplyOperation final
    {
        std::string operation_id{};
        std::string orchestrator_id{};
        std::string campaign_id{};
        iteration_session::RequestIdentity session_identity{};
        iteration_session::IterationTargetKind target_kind{
            iteration_session::IterationTargetKind::engine_source};
        std::uint64_t expected_generation{};
        std::string expected_state_sha256{};
        std::string transition_id{};
        std::string proposal_sha256{};
        std::string candidate_sha256{};
    };

    struct HostApprovalResult final
    {
        bool accepted{};
        self_iteration_orchestrator::Snapshot snapshot{};
        std::optional<ApplyOperation> operation{};
        std::string status{};
    };

    struct HostEvidenceResult final
    {
        bool accepted{};
        self_iteration_orchestrator::Snapshot snapshot{};
        std::string status{};
    };

    class HostTransitionPort
    {
    public:
        virtual ~HostTransitionPort() = default;
        [[nodiscard]] virtual HostApprovalResult approve_apply(
            const ReceiptBinding& receipt,
            const mcp_orchestrator_bridge::HostApproval& approval) = 0;
        [[nodiscard]] virtual HostEvidenceResult record_apply_evidence(
            const ApplyOperation& operation,
            std::string evidence_sha256,
            std::string summary,
            std::uint64_t now_unix_seconds) = 0;
    };

    class DirectHostTransitionPort final : public HostTransitionPort
    {
    public:
        DirectHostTransitionPort(
            mcp_orchestrator_bridge::Bridge& bridge,
            self_iteration_orchestrator::Orchestrator& orchestrator) noexcept;
        [[nodiscard]] HostApprovalResult approve_apply(
            const ReceiptBinding& receipt,
            const mcp_orchestrator_bridge::HostApproval& approval) override;
        [[nodiscard]] HostEvidenceResult record_apply_evidence(
            const ApplyOperation& operation,
            std::string evidence_sha256,
            std::string summary,
            std::uint64_t now_unix_seconds) override;

    private:
        mcp_orchestrator_bridge::Bridge* bridge_{};
        self_iteration_orchestrator::Orchestrator* orchestrator_{};
    };

    enum class JournalPhase : std::uint8_t
    {
        approved,
        committed,
        evidence_recorded,
        failed
    };

    struct JournalRecord final
    {
        std::uint32_t record_schema_version{schema_version};
        JournalPhase phase{JournalPhase::approved};
        std::string receipt_id{};
        std::string orchestrator_id{};
        std::string campaign_id{};
        std::uint64_t campaign_generation{};
        iteration_session::RequestIdentity session_identity{};
        std::string scope_sha256{};
        std::string authority_sha256{};
        std::string bundle_sha256{};
        std::string review_evidence_sha256{};
        std::string implementation_evidence_sha256{};
        ApplyOperation operation{};
        std::string record_sha256{};
    };

    class JournalPort
    {
    public:
        virtual ~JournalPort() = default;
        [[nodiscard]] virtual std::optional<JournalRecord> load(
            std::string_view receipt_id) = 0;
        [[nodiscard]] virtual bool store_atomic(const JournalRecord& record) = 0;
    };

    enum class Code : std::uint8_t
    {
        ready_for_review,
        review_bound,
        applied,
        evidence_pending,
        recovered,
        rejected,
        stale_state,
        cross_authority,
        replay_rejected,
        approval_required,
        partial_result,
        journal_failure,
        sandbox_failure,
        host_failure
    };

    struct Result final
    {
        Code code{Code::rejected};
        std::string status{};
        std::optional<ReviewArtifact> review{};
        std::optional<ApprovedReview> approved_review{};
        std::optional<source_patch_bundle::ApplyResult> apply_result{};
        std::optional<self_iteration_orchestrator::Snapshot> snapshot{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready_for_review
                || code == Code::review_bound || code == Code::applied
                || code == Code::evidence_pending || code == Code::recovered;
        }
    };

    struct ExecuteOptions final
    {
        bool defer_evidence_recording{};
    };

    [[nodiscard]] ReceiptBinding bind_receipt(
        const mcp_orchestrator_bridge::PendingReceipt& receipt);
    [[nodiscard]] std::string authority_sha256(
        const self_iteration_orchestrator::Snapshot& snapshot);
    [[nodiscard]] std::string scope_sha256(
        const self_iteration_orchestrator::Snapshot& snapshot);

    class Adapter final
    {
    public:
        [[nodiscard]] Result stage_review(
            const source_patch_bundle::Bundle& bundle,
            const self_iteration_orchestrator::Snapshot& snapshot) const;
        [[nodiscard]] Result bind_approved_review(
            ReviewArtifact review,
            const self_iteration_orchestrator::Snapshot& reviewed_snapshot,
            std::string review_transition_id) const;
        [[nodiscard]] Result execute(
            const ApprovedReview& review,
            const source_patch_bundle::Bundle& bundle,
            const ReceiptBinding& receipt,
            const mcp_orchestrator_bridge::HostApproval& approval,
            HostTransitionPort& host,
            source_patch_bundle::SandboxFilePort& sandbox,
            JournalPort& journal,
            std::uint64_t now_unix_seconds,
            ExecuteOptions options = {});
        [[nodiscard]] Result recover(
            const ApprovedReview& review,
            const source_patch_bundle::Bundle& bundle,
            std::string receipt_id,
            const self_iteration_orchestrator::Snapshot& snapshot,
            HostTransitionPort& host,
            source_patch_bundle::SandboxFilePort& sandbox,
            JournalPort& journal,
            std::uint64_t now_unix_seconds);

    private:
        source_patch_bundle::Executor executor_{};
        std::vector<std::string> completed_receipts_{};
    };

    [[nodiscard]] bool run_contract();
}
