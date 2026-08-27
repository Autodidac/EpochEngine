/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

export module editor.ai_development_controller;

export namespace epochengine::editor_ai_development
{
    struct LogicalTime final
    {
        std::uint64_t value{};

        friend constexpr bool operator==(const LogicalTime&, const LogicalTime&) = default;
    };

    struct Duration final
    {
        std::uint64_t value{};

        friend constexpr bool operator==(const Duration&, const Duration&) = default;
    };

    struct EvidenceDigest final
    {
        std::array<std::uint8_t, 32u> bytes{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            for (const std::uint8_t byte : bytes)
            {
                if (byte != 0u)
                    return true;
            }
            return false;
        }

        friend constexpr bool operator==(const EvidenceDigest&, const EvidenceDigest&) = default;
    };

    [[nodiscard]] EvidenceDigest evidence_digest(
        std::string_view content) noexcept;

    struct ContentState final
    {
        bool exists{};
        EvidenceDigest digest{};
        std::uint64_t byte_count{};

        friend constexpr bool operator==(
            const ContentState&, const ContentState&) = default;
    };

    enum class OperationKind : std::uint8_t
    {
        build,
        run,
        engine_source_edit,
        project_source_edit
    };

    enum class EvidenceKind : std::uint8_t
    {
        patch,
        build_log,
        runtime_report,
        diagnostic
    };

    enum class CompletionOutcome : std::uint8_t
    {
        succeeded,
        failed
    };

    enum class ControllerPhase : std::uint8_t
    {
        unavailable,
        ready,
        proposed,
        reviewed,
        approved,
        authorized,
        completed,
        failed,
        cancelled
    };

    enum class ControllerCode : std::uint8_t
    {
        none,
        invalid_configuration,
        invalid_request,
        invalid_phase,
        guard_rejected
    };

    enum class ClockPolicy : std::uint8_t
    {
        trusted_monotonic,
        externally_driven_contract
    };

    struct SessionConfiguration final
    {
        std::uint64_t session_value{};
        std::uint32_t session_generation{1u};
        std::string workspace_id{};
        std::string workspace_root{};
        std::string source_snapshot_root{};
        std::string engine_source_root{"Engine"};
        std::string project_source_root{"Projects"};
        std::string build_output_root{"x64"};
        std::string evidence_root{"logs"};
        std::string operator_id{};
        LogicalTime opened_at{};
        LogicalTime expires_at{};
        ClockPolicy clock_policy{ClockPolicy::trusted_monotonic};
    };

    struct OperationRequest final
    {
        OperationKind kind{OperationKind::build};
        std::string summary{};
        std::string relative_path{};
        ContentState before{};
        ContentState after{};
    };

    struct ProposalRequest final
    {
        std::string title{};
        std::string rationale{};
        std::vector<OperationRequest> operations{};
        LogicalTime expires_at{};
        ClockPolicy clock_policy{ClockPolicy::trusted_monotonic};
    };

    struct EvidenceItem final
    {
        EvidenceKind kind{EvidenceKind::diagnostic};
        std::string locator{};
        EvidenceDigest content_digest{};
        std::string summary{};
        bool verified{};
    };

    struct CompletionRequest final
    {
        std::string actor_id{};
        CompletionOutcome outcome{CompletionOutcome::failed};
        std::string summary{};
        std::vector<EvidenceItem> evidence{};
    };

    struct ControllerResult final
    {
        ControllerCode code{ControllerCode::none};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ControllerCode::none;
        }
    };

    struct OperationSummary final
    {
        std::uint64_t stable_id{};
        OperationKind kind{OperationKind::build};
        std::string summary{};
        std::string relative_path{};
        ContentState before{};
        ContentState after{};
        bool writes_source{};
        bool starts_process{};

        friend constexpr bool operator==(
            const OperationSummary&, const OperationSummary&) = default;
    };

    struct SourceExecutionPayload final
    {
        std::uint64_t stable_operation_id{};
        std::string relative_path{};
        std::string replacement_bytes{};
    };

    struct SourceExecutionReport final
    {
        ControllerResult controller_result{};
        std::string transaction_code{};
        std::string transaction_status{};
        std::string evidence_manifest{};
        EvidenceDigest evidence_digest{};
        bool attempted{};
        bool committed{};
        bool rollback_attempted{};
        bool rollback_complete{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return controller_result
                && attempted
                && committed;
        }
    };

    enum class PromotionCandidateCode : std::uint8_t
    {
        ready,
        invalid_root,
        invalid_operations,
        unsafe_path,
        live_preimage_mismatch,
        sandbox_postimage_mismatch
    };

    struct PromotionCandidateResult final
    {
        PromotionCandidateCode code{PromotionCandidateCode::invalid_operations};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == PromotionCandidateCode::ready;
        }
    };

    [[nodiscard]] constexpr std::string_view to_string(
        PromotionCandidateCode code) noexcept
    {
        switch (code)
        {
        case PromotionCandidateCode::ready: return "ready";
        case PromotionCandidateCode::invalid_root: return "invalid_root";
        case PromotionCandidateCode::invalid_operations: return "invalid_operations";
        case PromotionCandidateCode::unsafe_path: return "unsafe_path";
        case PromotionCandidateCode::live_preimage_mismatch: return "live_preimage_mismatch";
        case PromotionCandidateCode::sandbox_postimage_mismatch: return "sandbox_postimage_mismatch";
        }
        return "unknown";
    }

    [[nodiscard]] PromotionCandidateResult verify_source_promotion_candidate(
        std::string_view live_source_root,
        std::string_view sandbox_root,
        const std::vector<OperationSummary>& operations);

    struct ControllerSnapshot final
    {
        ControllerPhase phase{ControllerPhase::unavailable};
        ControllerCode last_code{ControllerCode::invalid_configuration};
        std::string status{};
        std::string workspace_id{};
        std::string proposal_title{};
        std::string digest_hex{};
        std::vector<OperationSummary> operations{};
        std::vector<EvidenceKind> required_evidence{};
        bool operator_approval_required{};
        bool exact_operator_approval_recorded{};
        bool single_use_permit_issued{};
        bool permit_consumed{};
        bool model_source_payloads_ready{};
        std::size_t audit_event_count{};
        std::size_t evidence_record_count{};
    };

    [[nodiscard]] constexpr std::string_view to_string(
        OperationKind kind) noexcept
    {
        switch (kind)
        {
        case OperationKind::build: return "build";
        case OperationKind::run: return "run";
        case OperationKind::engine_source_edit: return "engine_source_edit";
        case OperationKind::project_source_edit: return "project_source_edit";
        }
        return "unknown";
    }

    [[nodiscard]] constexpr std::string_view to_string(
        ControllerPhase phase) noexcept
    {
        switch (phase)
        {
        case ControllerPhase::unavailable: return "unavailable";
        case ControllerPhase::ready: return "ready";
        case ControllerPhase::proposed: return "proposed";
        case ControllerPhase::reviewed: return "reviewed";
        case ControllerPhase::approved: return "approved";
        case ControllerPhase::authorized: return "authorized";
        case ControllerPhase::completed: return "completed";
        case ControllerPhase::failed: return "failed";
        case ControllerPhase::cancelled: return "cancelled";
        }
        return "unknown";
    }

    [[nodiscard]] bool run_contract();

    class DevelopmentController final
    {
    public:
        explicit DevelopmentController(SessionConfiguration configuration);
        ~DevelopmentController();

        DevelopmentController(DevelopmentController&&) noexcept;
        DevelopmentController& operator=(DevelopmentController&&) noexcept;

        DevelopmentController(const DevelopmentController&) = delete;
        DevelopmentController& operator=(const DevelopmentController&) = delete;

        [[nodiscard]] ControllerResult propose(
            ProposalRequest request,
            LogicalTime now);

        [[nodiscard]] ControllerResult propose_model_reply(
            std::string_view raw_reply,
            OperationKind expected_source_kind,
            LogicalTime now);

        [[nodiscard]] ControllerResult review(
            std::string reviewer_id,
            std::string note,
            LogicalTime now);

        [[nodiscard]] ControllerResult approve(
            std::string operator_id,
            std::string note,
            LogicalTime now,
            LogicalTime expires_at);

        [[nodiscard]] ControllerResult authorize(
            LogicalTime now,
            Duration permit_lifetime);

        [[nodiscard]] ControllerResult complete(
            CompletionRequest completion,
            LogicalTime now);

        [[nodiscard]] SourceExecutionReport
            execute_authorized_source_changes(
                std::vector<SourceExecutionPayload> payloads,
                LogicalTime now);

        [[nodiscard]] SourceExecutionReport
            execute_authorized_model_source_changes(LogicalTime now);

        [[nodiscard]] ControllerResult cancel(
            std::string actor_id,
            std::string reason,
            LogicalTime now);

        [[nodiscard]] ControllerSnapshot snapshot() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_{};
    };
}
