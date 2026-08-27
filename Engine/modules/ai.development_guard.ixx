/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module ai.development_guard;

import core.sha256;

namespace epochengine::ai::development_guard_detail
{
    class DigestWriter final
    {
    public:
        void u8(std::uint8_t value) noexcept
        {
            sha_.update(std::span<const std::uint8_t>{&value, 1u});
        }

        void u32(std::uint32_t value) noexcept
        {
            std::array<std::uint8_t, 4u> bytes{};
            for (std::size_t index = 0u; index < bytes.size(); ++index)
                bytes[index] = static_cast<std::uint8_t>(value >> ((3u - index) * 8u));
            sha_.update(bytes);
        }

        void u64(std::uint64_t value) noexcept
        {
            std::array<std::uint8_t, 8u> bytes{};
            for (std::size_t index = 0u; index < bytes.size(); ++index)
                bytes[index] = static_cast<std::uint8_t>(value >> ((7u - index) * 8u));
            sha_.update(bytes);
        }

        void text(std::string_view value) noexcept
        {
            u64(static_cast<std::uint64_t>(value.size()));
            sha_.update(value);
        }

        [[nodiscard]] std::array<std::uint8_t, 32u> finish() noexcept
        {
            return sha_.finish().bytes;
        }

    private:
        core::sha256::Hasher sha_{};
    };
}

export namespace epochengine::ai::development_guard
{
    inline constexpr std::uint32_t contract_schema_version = 2u;
    inline constexpr std::uint32_t invalid_proposal_slot =
        (std::numeric_limits<std::uint32_t>::max)();

    struct SessionIdentity final
    {
        std::uint64_t value{};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return value != 0u && generation != 0u;
        }

        friend constexpr bool operator==(const SessionIdentity&, const SessionIdentity&) = default;
    };

    struct ProposalIdentity final
    {
        std::uint32_t slot{invalid_proposal_slot};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return slot != invalid_proposal_slot && generation != 0u;
        }

        friend constexpr bool operator==(const ProposalIdentity&, const ProposalIdentity&) = default;
    };

    struct ProposalDigest final
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

        friend constexpr bool operator==(
            const ProposalDigest& left,
            const ProposalDigest& right) noexcept
        {
            for (std::size_t index = 0u; index < left.bytes.size(); ++index)
            {
                if (left.bytes[index] != right.bytes[index])
                    return false;
            }
            return true;
        }
    };

    enum class OperationCategory : std::uint8_t
    {
        inspect,
        source_write,
        project_write,
        build,
        test,
        run,
        network_access,
        release
    };

    enum class RiskCategory : std::uint8_t
    {
        read_only,
        workspace_write,
        engine_source_write,
        child_process,
        runtime_execution,
        network_access,
        release
    };

    enum class WorkspaceArea : std::uint8_t
    {
        engine_source,
        project_source,
        build_output,
        evidence,
        dependency_cache
    };

    enum class WorkspacePermission : std::uint8_t
    {
        none = 0u,
        read = 1u << 0u,
        write = 1u << 1u,
        create = 1u << 2u,
        remove = 1u << 3u,
        execute = 1u << 4u
    };

    [[nodiscard]] constexpr WorkspacePermission operator|(
        WorkspacePermission left,
        WorkspacePermission right) noexcept
    {
        return static_cast<WorkspacePermission>(
            static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr WorkspacePermission operator&(
        WorkspacePermission left,
        WorkspacePermission right) noexcept
    {
        return static_cast<WorkspacePermission>(
            static_cast<std::uint8_t>(left) & static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr bool has_permission(
        WorkspacePermission granted,
        WorkspacePermission requested) noexcept
    {
        return (granted & requested) == requested;
    }

    struct WorkspaceRule final
    {
        WorkspaceArea area{WorkspaceArea::project_source};
        std::string relative_root{};
        WorkspacePermission permissions{WorkspacePermission::read};
    };

    struct WorkspaceIntent final
    {
        WorkspaceArea area{WorkspaceArea::project_source};
        std::string relative_path{};
        WorkspacePermission permissions{WorkspacePermission::read};
    };

    enum class ContentStateKind : std::uint8_t
    {
        absent,
        sha256
    };

    struct ContentState final
    {
        ContentStateKind kind{ContentStateKind::absent};
        ProposalDigest digest{};
        std::uint64_t byte_count{};
    };

    struct ContentTransition final
    {
        std::string relative_path{};
        ContentState before{};
        ContentState after{};
    };

    struct DevelopmentOperation final
    {
        std::uint64_t stable_id{};
        OperationCategory category{OperationCategory::inspect};
        RiskCategory risk{RiskCategory::read_only};
        std::string summary{};
        std::vector<WorkspaceIntent> workspace_intents{};
        std::vector<ContentTransition> content_transitions{};
    };

    struct GuardLimits final
    {
        std::size_t maximum_allowlist_rules{32u};
        std::size_t maximum_operators{8u};
        std::size_t maximum_proposals{64u};
        std::size_t maximum_operations_per_proposal{32u};
        std::size_t maximum_intents_per_operation{32u};
        std::size_t maximum_content_transitions_per_operation{32u};
        std::uint64_t maximum_content_bytes_per_transition{16u * 1024u * 1024u};
        std::size_t maximum_evidence_items{32u};
        std::size_t maximum_evidence_records{64u};
        std::size_t maximum_audit_events{512u};
        std::size_t maximum_string_bytes{4096u};
        std::size_t maximum_total_proposal_bytes{1024u * 1024u};
        std::size_t maximum_total_evidence_bytes{4u * 1024u * 1024u};
        std::uint64_t maximum_proposal_lifetime{24u * 60u * 60u};
        std::uint64_t maximum_approval_lifetime{60u * 60u};
        std::uint64_t maximum_permit_lifetime{5u * 60u};
    };

    struct SessionPolicy final
    {
        SessionIdentity identity{};
        std::string workspace_id{};
        std::uint64_t opened_at{};
        std::uint64_t expires_at{};
        GuardLimits limits{};
        std::vector<WorkspaceRule> workspace_allowlist{};
        std::vector<std::string> approved_operator_ids{};
    };

    struct ProposalDraft final
    {
        SessionIdentity session{};
        std::string title{};
        std::string rationale{};
        std::uint64_t created_at{};
        std::uint64_t expires_at{};
        std::vector<DevelopmentOperation> operations{};
    };

    enum class ProposalState : std::uint8_t
    {
        proposed,
        reviewed,
        approved,
        execution_eligible,
        executing,
        completed,
        failed,
        rejected,
        cancelled,
        expired
    };

    enum class ReviewDisposition : std::uint8_t
    {
        accept,
        reject
    };

    struct ReviewDecision final
    {
        SessionIdentity session{};
        ProposalIdentity proposal{};
        ProposalDigest proposal_digest{};
        std::string reviewer_id{};
        ReviewDisposition disposition{ReviewDisposition::reject};
        std::string note{};
    };

    enum class ApprovalDisposition : std::uint8_t
    {
        approve,
        reject
    };

    struct ApprovalDecision final
    {
        SessionIdentity session{};
        ProposalIdentity proposal{};
        ProposalDigest proposal_digest{};
        std::string operator_id{};
        ApprovalDisposition disposition{ApprovalDisposition::reject};
        std::uint64_t expires_at{};
        std::string note{};
    };

    enum class EvidenceCategory : std::uint8_t
    {
        inspection_record,
        patch,
        build_log,
        test_report,
        runtime_report,
        network_report,
        release_manifest,
        diagnostic
    };

    enum class EvidenceRequirement : std::uint32_t
    {
        none = 0u,
        inspection_record = 1u << 0u,
        patch = 1u << 1u,
        build_log = 1u << 2u,
        test_report = 1u << 3u,
        runtime_report = 1u << 4u,
        network_report = 1u << 5u,
        release_manifest = 1u << 6u,
        diagnostic = 1u << 7u
    };

    [[nodiscard]] constexpr EvidenceRequirement operator|(
        EvidenceRequirement left,
        EvidenceRequirement right) noexcept
    {
        return static_cast<EvidenceRequirement>(
            static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr EvidenceRequirement operator&(
        EvidenceRequirement left,
        EvidenceRequirement right) noexcept
    {
        return static_cast<EvidenceRequirement>(
            static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr bool has_evidence(
        EvidenceRequirement available,
        EvidenceRequirement required) noexcept
    {
        return (available & required) == required;
    }

    struct EvidenceItem final
    {
        EvidenceCategory category{EvidenceCategory::diagnostic};
        std::string locator{};
        ProposalDigest content_digest{};
        std::string summary{};
        bool verified{};
    };

    class DevelopmentGuard;

    class ExecutionPermit final
    {
    public:
        constexpr ExecutionPermit() noexcept = default;

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return serial != 0u && generation != 0u && session.valid()
                && proposal.valid() && proposal_digest.valid()
                && issued_at < expires_at;
        }

        [[nodiscard]] constexpr std::uint64_t serial_value() const noexcept
        {
            return serial;
        }

        [[nodiscard]] constexpr std::uint32_t generation_value() const noexcept
        {
            return generation;
        }

        [[nodiscard]] constexpr SessionIdentity session_identity() const noexcept
        {
            return session;
        }

        [[nodiscard]] constexpr ProposalIdentity proposal_identity() const noexcept
        {
            return proposal;
        }

        [[nodiscard]] constexpr ProposalDigest digest() const noexcept
        {
            return proposal_digest;
        }

        [[nodiscard]] constexpr std::uint64_t issued_time() const noexcept
        {
            return issued_at;
        }

        [[nodiscard]] constexpr std::uint64_t expiry() const noexcept
        {
            return expires_at;
        }

        friend constexpr bool operator==(
            const ExecutionPermit&,
            const ExecutionPermit&) = default;

    private:
        constexpr ExecutionPermit(
            std::uint64_t serial_value,
            std::uint32_t generation_value,
            SessionIdentity session_value,
            ProposalIdentity proposal_value,
            ProposalDigest digest_value,
            std::uint64_t issued_value,
            std::uint64_t expiry_value) noexcept
            : serial(serial_value),
              generation(generation_value),
              session(session_value),
              proposal(proposal_value),
              proposal_digest(digest_value),
              issued_at(issued_value),
              expires_at(expiry_value)
        {
        }

        std::uint64_t serial{};
        std::uint32_t generation{};
        SessionIdentity session{};
        ProposalIdentity proposal{};
        ProposalDigest proposal_digest{};
        std::uint64_t issued_at{};
        std::uint64_t expires_at{};

        friend class DevelopmentGuard;
    };
    enum class ExecutionOutcome : std::uint8_t
    {
        succeeded,
        failed
    };

    struct EvidenceSubmission final
    {
        ExecutionPermit permit{};
        std::string actor_id{};
        ExecutionOutcome outcome{ExecutionOutcome::failed};
        std::string summary{};
        std::vector<EvidenceItem> items{};
    };

    struct CancellationRequest final
    {
        SessionIdentity session{};
        ProposalIdentity proposal{};
        ProposalDigest proposal_digest{};
        std::string actor_id{};
        std::string reason{};
    };

    enum class GuardCode : std::uint8_t
    {
        none,
        invalid_policy,
        invalid_limits,
        invalid_session,
        session_expired,
        session_cancelled,
        invalid_identity,
        invalid_timestamp,
        invalid_text,
        invalid_operation,
        risk_mismatch,
        invalid_path,
        path_denied,
        storage_exhausted,
        proposal_not_found,
        digest_mismatch,
        state_conflict,
        review_required,
        approval_required,
        approval_not_required,
        operator_denied,
        approval_expired,
        proposal_expired,
        permit_invalid,
        permit_expired,
        evidence_missing,
        evidence_unverified,
        already_complete
    };

    [[nodiscard]] constexpr std::string_view to_string(GuardCode code) noexcept
    {
        switch (code)
        {
        case GuardCode::none: return "none";
        case GuardCode::invalid_policy: return "invalid_policy";
        case GuardCode::invalid_limits: return "invalid_limits";
        case GuardCode::invalid_session: return "invalid_session";
        case GuardCode::session_expired: return "session_expired";
        case GuardCode::session_cancelled: return "session_cancelled";
        case GuardCode::invalid_identity: return "invalid_identity";
        case GuardCode::invalid_timestamp: return "invalid_timestamp";
        case GuardCode::invalid_text: return "invalid_text";
        case GuardCode::invalid_operation: return "invalid_operation";
        case GuardCode::risk_mismatch: return "risk_mismatch";
        case GuardCode::invalid_path: return "invalid_path";
        case GuardCode::path_denied: return "path_denied";
        case GuardCode::storage_exhausted: return "storage_exhausted";
        case GuardCode::proposal_not_found: return "proposal_not_found";
        case GuardCode::digest_mismatch: return "digest_mismatch";
        case GuardCode::state_conflict: return "state_conflict";
        case GuardCode::review_required: return "review_required";
        case GuardCode::approval_required: return "approval_required";
        case GuardCode::approval_not_required: return "approval_not_required";
        case GuardCode::operator_denied: return "operator_denied";
        case GuardCode::approval_expired: return "approval_expired";
        case GuardCode::proposal_expired: return "proposal_expired";
        case GuardCode::permit_invalid: return "permit_invalid";
        case GuardCode::permit_expired: return "permit_expired";
        case GuardCode::evidence_missing: return "evidence_missing";
        case GuardCode::evidence_unverified: return "evidence_unverified";
        case GuardCode::already_complete: return "already_complete";
        }
        return "unknown";
    }

    struct GuardResult final
    {
        GuardCode code{GuardCode::none};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == GuardCode::none;
        }
    };

    struct ProposalReceipt final
    {
        GuardCode code{GuardCode::invalid_policy};
        ProposalIdentity proposal{};
        ProposalDigest digest{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == GuardCode::none && proposal.valid() && digest.valid();
        }
    };

    struct Eligibility final
    {
        GuardCode code{GuardCode::invalid_policy};
        EvidenceRequirement required_evidence{EvidenceRequirement::none};
        bool operator_approval_required{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == GuardCode::none;
        }
    };

    struct PermitReceipt final
    {
        GuardCode code{GuardCode::invalid_policy};
        ExecutionPermit permit{};
        EvidenceRequirement required_evidence{EvidenceRequirement::none};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == GuardCode::none && permit.valid();
        }
    };

    struct ReviewRecord final
    {
        ProposalDigest proposal_digest{};
        std::string reviewer_id{};
        ReviewDisposition disposition{ReviewDisposition::reject};
        std::uint64_t reviewed_at{};
        std::string note{};
    };

    struct ApprovalRecord final
    {
        ProposalDigest proposal_digest{};
        std::string operator_id{};
        ApprovalDisposition disposition{ApprovalDisposition::reject};
        std::uint64_t approved_at{};
        std::uint64_t expires_at{};
        std::string note{};
    };

    struct ProposalSnapshot final
    {
        SessionIdentity session{};
        ProposalIdentity identity{};
        ProposalDigest digest{};
        ProposalState state{ProposalState::proposed};
        std::string title{};
        std::string rationale{};
        std::uint64_t created_at{};
        std::uint64_t expires_at{};
        std::vector<DevelopmentOperation> operations{};
        std::optional<ReviewRecord> review{};
        std::optional<ApprovalRecord> approval{};
        EvidenceRequirement required_evidence{EvidenceRequirement::none};
        bool requires_operator_approval{};
    };

    struct EvidenceRecord final
    {
        SessionIdentity session{};
        ProposalIdentity proposal{};
        ProposalDigest proposal_digest{};
        ProposalDigest evidence_digest{};
        std::uint64_t permit_serial{};
        std::string actor_id{};
        ExecutionOutcome outcome{ExecutionOutcome::failed};
        std::uint64_t recorded_at{};
        std::string summary{};
        std::vector<EvidenceItem> items{};
    };

    enum class AuditKind : std::uint8_t
    {
        session_opened,
        proposal_registered,
        review_accepted,
        review_rejected,
        approval_granted,
        approval_rejected,
        permit_issued,
        execution_started,
        evidence_accepted,
        proposal_cancelled,
        proposal_expired,
        session_cancelled
    };

    struct AuditEvent final
    {
        std::uint64_t sequence{};
        std::uint64_t occurred_at{};
        SessionIdentity session{};
        ProposalIdentity proposal{};
        ProposalDigest proposal_digest{};
        ProposalDigest evidence_digest{};
        AuditKind kind{AuditKind::session_opened};
        ProposalState resulting_state{ProposalState::proposed};
        std::string actor_id{};
    };

    [[nodiscard]] constexpr RiskCategory expected_risk(
        OperationCategory category) noexcept
    {
        switch (category)
        {
        case OperationCategory::inspect: return RiskCategory::read_only;
        case OperationCategory::source_write: return RiskCategory::engine_source_write;
        case OperationCategory::project_write: return RiskCategory::workspace_write;
        case OperationCategory::build:
        case OperationCategory::test: return RiskCategory::child_process;
        case OperationCategory::run: return RiskCategory::runtime_execution;
        case OperationCategory::network_access: return RiskCategory::network_access;
        case OperationCategory::release: return RiskCategory::release;
        }
        return RiskCategory::release;
    }

    [[nodiscard]] constexpr bool requires_operator_approval(
        RiskCategory risk) noexcept
    {
        return risk != RiskCategory::read_only;
    }

    [[nodiscard]] constexpr EvidenceRequirement evidence_requirement(
        OperationCategory category) noexcept
    {
        switch (category)
        {
        case OperationCategory::inspect: return EvidenceRequirement::inspection_record;
        case OperationCategory::source_write:
        case OperationCategory::project_write: return EvidenceRequirement::patch;
        case OperationCategory::build: return EvidenceRequirement::build_log;
        case OperationCategory::test: return EvidenceRequirement::test_report;
        case OperationCategory::run: return EvidenceRequirement::runtime_report;
        case OperationCategory::network_access: return EvidenceRequirement::network_report;
        case OperationCategory::release: return EvidenceRequirement::release_manifest;
        }
        return EvidenceRequirement::none;
    }

    [[nodiscard]] inline std::optional<std::string> canonical_relative_path(
        std::string_view path)
    {
        if (path.empty())
            return std::nullopt;
        std::string normalized{};
        normalized.reserve(path.size());
        for (const char value : path)
        {
            const unsigned char byte = static_cast<unsigned char>(value);
            if (byte < 0x20u || value == ':')
                return std::nullopt;
            normalized.push_back(value == '\\' ? '/' : value);
        }
        if (normalized.front() == '/' || normalized.back() == '/')
            return std::nullopt;

        std::size_t start = 0u;
        while (start < normalized.size())
        {
            const std::size_t end = normalized.find('/', start);
            const std::size_t count = end == std::string::npos
                ? normalized.size() - start
                : end - start;
            const std::string_view segment{normalized.data() + start, count};
            if (segment.empty() || segment == "." || segment == ".."
                || segment.back() == '.' || segment.back() == ' ')
            {
                return std::nullopt;
            }

            const std::size_t extension = segment.find('.');
            const std::string_view device_stem = segment.substr(0u, extension);
            std::string upper_stem{};
            upper_stem.reserve(device_stem.size());
            for (const char character : device_stem)
            {
                upper_stem.push_back(
                    character >= 'a' && character <= 'z'
                        ? static_cast<char>(character - ('a' - 'A'))
                        : character);
            }
            const bool reserved_device = upper_stem == "CON"
                || upper_stem == "PRN" || upper_stem == "AUX"
                || upper_stem == "NUL"
                || (upper_stem.size() == 4u
                    && (upper_stem.starts_with("COM")
                        || upper_stem.starts_with("LPT"))
                    && upper_stem[3u] >= '1'
                    && upper_stem[3u] <= '9');
            if (reserved_device)
                return std::nullopt;
            if (end == std::string::npos)
                break;
            start = end + 1u;
        }
        return normalized;
    }


    [[nodiscard]] constexpr bool valid_content_state(
        const ContentState& state) noexcept
    {
        if (state.kind == ContentStateKind::absent)
            return !state.digest.valid() && state.byte_count == 0u;
        return state.kind == ContentStateKind::sha256
            && state.digest.valid();
    }

    [[nodiscard]] constexpr bool same_content_state(
        const ContentState& left,
        const ContentState& right) noexcept
    {
        return left.kind == right.kind
            && left.digest == right.digest
            && left.byte_count == right.byte_count;
    }

    class DevelopmentGuard final
    {
    public:
        explicit DevelopmentGuard(SessionPolicy policy)
            : policy_(std::move(policy))
        {
            for (WorkspaceRule& rule : policy_.workspace_allowlist)
            {
                if (const auto path = canonical_relative_path(rule.relative_root))
                    rule.relative_root = *path;
            }
            policy_validation_ = validate_policy(policy_);
            if (policy_validation_)
            {
                (void)append_audit(AuditEvent{
                    .occurred_at = policy_.opened_at,
                    .session = policy_.identity,
                    .kind = AuditKind::session_opened,
                    .resulting_state = ProposalState::proposed,
                    .actor_id = "epoch.host"});
            }
        }

        [[nodiscard]] static GuardResult validate_policy(
            const SessionPolicy& policy) noexcept
        {
            constexpr std::size_t hard_maximum_count = 4096u;
            constexpr std::size_t hard_maximum_bytes = 64u * 1024u * 1024u;
            if (!policy.identity.valid() || policy.workspace_id.empty()
                || policy.opened_at >= policy.expires_at)
            {
                return {GuardCode::invalid_policy};
            }
            const GuardLimits& limits = policy.limits;
            if (limits.maximum_allowlist_rules == 0u
                || limits.maximum_allowlist_rules > hard_maximum_count
                || limits.maximum_operators == 0u
                || limits.maximum_operators > hard_maximum_count
                || limits.maximum_proposals == 0u
                || limits.maximum_proposals > hard_maximum_count
                || limits.maximum_operations_per_proposal == 0u
                || limits.maximum_operations_per_proposal > hard_maximum_count
                || limits.maximum_intents_per_operation == 0u
                || limits.maximum_intents_per_operation > hard_maximum_count
                || limits.maximum_content_transitions_per_operation == 0u
                || limits.maximum_content_transitions_per_operation
                    > hard_maximum_count
                || limits.maximum_content_bytes_per_transition == 0u
                || limits.maximum_content_bytes_per_transition
                    > hard_maximum_bytes
                || limits.maximum_evidence_items == 0u
                || limits.maximum_evidence_items > hard_maximum_count
                || limits.maximum_evidence_records == 0u
                || limits.maximum_evidence_records > hard_maximum_count
                || limits.maximum_audit_events == 0u
                || limits.maximum_audit_events > hard_maximum_count * 8u
                || limits.maximum_string_bytes == 0u
                || limits.maximum_string_bytes > 64u * 1024u
                || limits.maximum_total_proposal_bytes == 0u
                || limits.maximum_total_proposal_bytes > hard_maximum_bytes
                || limits.maximum_total_evidence_bytes == 0u
                || limits.maximum_total_evidence_bytes > hard_maximum_bytes
                || limits.maximum_proposal_lifetime == 0u
                || limits.maximum_approval_lifetime == 0u
                || limits.maximum_permit_lifetime == 0u)
            {
                return {GuardCode::invalid_limits};
            }
            if (!valid_identifier(policy.workspace_id, limits.maximum_string_bytes)
                || policy.workspace_allowlist.empty()
                || policy.workspace_allowlist.size() > limits.maximum_allowlist_rules
                || policy.approved_operator_ids.empty()
                || policy.approved_operator_ids.size() > limits.maximum_operators)
            {
                return {GuardCode::invalid_policy};
            }
            for (std::size_t index = 0u; index < policy.approved_operator_ids.size(); ++index)
            {
                if (!valid_identifier(
                        policy.approved_operator_ids[index], limits.maximum_string_bytes))
                {
                    return {GuardCode::invalid_policy};
                }
                if (std::find(
                        policy.approved_operator_ids.begin() + static_cast<std::ptrdiff_t>(index + 1u),
                        policy.approved_operator_ids.end(),
                        policy.approved_operator_ids[index]) != policy.approved_operator_ids.end())
                {
                    return {GuardCode::invalid_policy};
                }
            }
            for (std::size_t index = 0u; index < policy.workspace_allowlist.size(); ++index)
            {
                const WorkspaceRule& rule = policy.workspace_allowlist[index];
                if (!canonical_relative_path(rule.relative_root)
                    || !valid_permissions(rule.permissions))
                {
                    return {GuardCode::invalid_policy};
                }
                for (std::size_t other = index + 1u;
                     other < policy.workspace_allowlist.size(); ++other)
                {
                    if (rule.area == policy.workspace_allowlist[other].area
                        && rule.relative_root == policy.workspace_allowlist[other].relative_root)
                    {
                        return {GuardCode::invalid_policy};
                    }
                }
            }
            return {};
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return static_cast<bool>(policy_validation_);
        }

        [[nodiscard]] GuardCode validation_code() const noexcept
        {
            return policy_validation_.code;
        }

        [[nodiscard]] const SessionPolicy& policy() const noexcept
        {
            return policy_;
        }

        [[nodiscard]] ProposalReceipt submit(
            ProposalDraft draft,
            std::uint64_t now)
        {
            if (const GuardResult active = validate_active_session(draft.session, now); !active)
                return {.code = active.code};
            if (records_.size() >= policy_.limits.maximum_proposals
                || audit_events_.size() >= policy_.limits.maximum_audit_events)
            {
                return {.code = GuardCode::storage_exhausted};
            }
            if (draft.created_at < policy_.opened_at || draft.created_at > now
                || draft.created_at >= draft.expires_at
                || draft.expires_at > policy_.expires_at
                || draft.expires_at - draft.created_at
                    > policy_.limits.maximum_proposal_lifetime)
            {
                return {.code = GuardCode::invalid_timestamp};
            }
            if (!valid_text(draft.title, false) || !valid_text(draft.rationale, false)
                || draft.operations.empty()
                || draft.operations.size() > policy_.limits.maximum_operations_per_proposal)
            {
                return {.code = GuardCode::invalid_text};
            }

            std::size_t stored_bytes = draft.title.size() + draft.rationale.size();
            EvidenceRequirement requirements = EvidenceRequirement::none;
            bool approval_required = false;
            for (std::size_t index = 0u; index < draft.operations.size(); ++index)
            {
                DevelopmentOperation& operation = draft.operations[index];
                const GuardCode operation_code = validate_operation(
                    operation, draft.operations, index, stored_bytes);
                if (operation_code != GuardCode::none)
                    return {.code = operation_code};
                requirements = requirements | evidence_requirement(operation.category);
                approval_required = approval_required
                    || requires_operator_approval(operation.risk);
            }
            if (stored_bytes > policy_.limits.maximum_total_proposal_bytes
                || proposal_bytes_ >
                    policy_.limits.maximum_total_proposal_bytes - stored_bytes)
                return {.code = GuardCode::storage_exhausted};

            ProposalRecord record{};
            record.snapshot.session = draft.session;
            record.snapshot.identity = {
                static_cast<std::uint32_t>(records_.size()), 1u};
            record.snapshot.state = ProposalState::proposed;
            record.snapshot.title = std::move(draft.title);
            record.snapshot.rationale = std::move(draft.rationale);
            record.snapshot.created_at = draft.created_at;
            record.snapshot.expires_at = draft.expires_at;
            record.snapshot.operations = std::move(draft.operations);
            record.snapshot.required_evidence = requirements;
            record.snapshot.requires_operator_approval = approval_required;
            record.snapshot.digest = proposal_digest(record.snapshot);
            if (!record.snapshot.digest.valid())
                return {.code = GuardCode::digest_mismatch};

            const ProposalReceipt receipt{
                .code = GuardCode::none,
                .proposal = record.snapshot.identity,
                .digest = record.snapshot.digest};
            records_.push_back(std::move(record));
            proposal_bytes_ += stored_bytes;
            (void)append_audit(AuditEvent{
                .occurred_at = now,
                .session = policy_.identity,
                .proposal = receipt.proposal,
                .proposal_digest = receipt.digest,
                .kind = AuditKind::proposal_registered,
                .resulting_state = ProposalState::proposed,
                .actor_id = "epoch.host"});
            return receipt;
        }

        [[nodiscard]] GuardResult review(
            ReviewDecision decision,
            std::uint64_t now)
        {
            if (const GuardResult active = validate_active_session(decision.session, now); !active)
                return active;
            ProposalRecord* record = find_record(decision.proposal);
            if (record == nullptr)
                return {GuardCode::proposal_not_found};
            if (const GuardResult current = validate_current(*record, decision.proposal_digest, now);
                !current)
            {
                return current;
            }
            if (record->snapshot.state != ProposalState::proposed)
                return {GuardCode::state_conflict};
            if (!valid_identifier(decision.reviewer_id, policy_.limits.maximum_string_bytes)
                || !valid_text(decision.note, true))
            {
                return {GuardCode::invalid_text};
            }
            if (audit_events_.size() >= policy_.limits.maximum_audit_events)
                return {GuardCode::storage_exhausted};

            record->snapshot.review = ReviewRecord{
                .proposal_digest = record->snapshot.digest,
                .reviewer_id = std::move(decision.reviewer_id),
                .disposition = decision.disposition,
                .reviewed_at = now,
                .note = std::move(decision.note)};
            record->snapshot.state = decision.disposition == ReviewDisposition::accept
                ? ProposalState::reviewed
                : ProposalState::rejected;
            (void)append_audit(AuditEvent{
                .occurred_at = now,
                .session = policy_.identity,
                .proposal = record->snapshot.identity,
                .proposal_digest = record->snapshot.digest,
                .kind = decision.disposition == ReviewDisposition::accept
                    ? AuditKind::review_accepted
                    : AuditKind::review_rejected,
                .resulting_state = record->snapshot.state,
                .actor_id = record->snapshot.review->reviewer_id});
            return {};
        }

        [[nodiscard]] GuardResult approve(
            ApprovalDecision decision,
            std::uint64_t now)
        {
            if (const GuardResult active = validate_active_session(decision.session, now); !active)
                return active;
            ProposalRecord* record = find_record(decision.proposal);
            if (record == nullptr)
                return {GuardCode::proposal_not_found};
            if (const GuardResult current = validate_current(*record, decision.proposal_digest, now);
                !current)
            {
                return current;
            }
            if (record->snapshot.state != ProposalState::reviewed)
                return {GuardCode::state_conflict};
            if (!record->snapshot.requires_operator_approval)
                return {GuardCode::approval_not_required};
            if (!operator_allowed(decision.operator_id))
                return {GuardCode::operator_denied};
            if (!valid_text(decision.note, true))
                return {GuardCode::invalid_text};
            if (decision.expires_at <= now
                || decision.expires_at > record->snapshot.expires_at
                || decision.expires_at > policy_.expires_at
                || decision.expires_at - now > policy_.limits.maximum_approval_lifetime)
            {
                return {GuardCode::invalid_timestamp};
            }
            if (audit_events_.size() >= policy_.limits.maximum_audit_events)
                return {GuardCode::storage_exhausted};

            record->snapshot.approval = ApprovalRecord{
                .proposal_digest = record->snapshot.digest,
                .operator_id = std::move(decision.operator_id),
                .disposition = decision.disposition,
                .approved_at = now,
                .expires_at = decision.expires_at,
                .note = std::move(decision.note)};
            record->snapshot.state = decision.disposition == ApprovalDisposition::approve
                ? ProposalState::approved
                : ProposalState::rejected;
            (void)append_audit(AuditEvent{
                .occurred_at = now,
                .session = policy_.identity,
                .proposal = record->snapshot.identity,
                .proposal_digest = record->snapshot.digest,
                .kind = decision.disposition == ApprovalDisposition::approve
                    ? AuditKind::approval_granted
                    : AuditKind::approval_rejected,
                .resulting_state = record->snapshot.state,
                .actor_id = record->snapshot.approval->operator_id});
            return {};
        }

        [[nodiscard]] Eligibility execution_eligibility(
            SessionIdentity session,
            ProposalIdentity proposal,
            ProposalDigest digest,
            std::uint64_t now) const noexcept
        {
            if (const GuardResult active = validate_active_session(session, now); !active)
                return {.code = active.code};
            const ProposalRecord* record = find_record(proposal);
            if (record == nullptr)
                return {.code = GuardCode::proposal_not_found};
            if (digest != record->snapshot.digest)
                return {.code = GuardCode::digest_mismatch};
            if (now > record->snapshot.expires_at)
                return {.code = GuardCode::proposal_expired};
            if (record->snapshot.state == ProposalState::completed
                || record->snapshot.state == ProposalState::failed)
            {
                return {.code = GuardCode::already_complete};
            }
            if (record->snapshot.state == ProposalState::cancelled)
                return {.code = GuardCode::session_cancelled};
            if (record->snapshot.state == ProposalState::rejected
                || record->snapshot.state == ProposalState::expired
                || record->snapshot.state == ProposalState::execution_eligible
                || record->snapshot.state == ProposalState::executing)
            {
                return {.code = GuardCode::state_conflict};
            }
            if (record->snapshot.state == ProposalState::proposed)
                return {.code = GuardCode::review_required};
            if (!record->snapshot.requires_operator_approval)
            {
                return {
                    .code = GuardCode::none,
                    .required_evidence = record->snapshot.required_evidence,
                    .operator_approval_required = false};
            }
            if (record->snapshot.state != ProposalState::approved
                || !record->snapshot.approval
                || record->snapshot.approval->disposition != ApprovalDisposition::approve)
            {
                return {
                    .code = GuardCode::approval_required,
                    .required_evidence = record->snapshot.required_evidence,
                    .operator_approval_required = true};
            }
            if (record->snapshot.approval->proposal_digest != digest)
                return {.code = GuardCode::digest_mismatch};
            if (now > record->snapshot.approval->expires_at)
                return {.code = GuardCode::approval_expired};
            return {
                .code = GuardCode::none,
                .required_evidence = record->snapshot.required_evidence,
                .operator_approval_required = true};
        }

        [[nodiscard]] PermitReceipt issue_execution_permit(
            SessionIdentity session,
            ProposalIdentity proposal,
            ProposalDigest digest,
            std::uint64_t now,
            std::uint64_t lifetime)
        {
            const Eligibility eligibility = execution_eligibility(
                session, proposal, digest, now);
            if (!eligibility)
            {
                if (eligibility.code == GuardCode::proposal_expired)
                    mark_expired(proposal, now);
                return {.code = eligibility.code};
            }
            if (lifetime == 0u || lifetime > policy_.limits.maximum_permit_lifetime
                || now > (std::numeric_limits<std::uint64_t>::max)() - lifetime)
            {
                return {.code = GuardCode::invalid_timestamp};
            }
            ProposalRecord* record = find_record(proposal);
            if (record == nullptr)
                return {.code = GuardCode::proposal_not_found};
            const std::uint64_t permit_expiry = now + lifetime;
            const std::uint64_t approval_expiry = record->snapshot.approval
                ? record->snapshot.approval->expires_at
                : record->snapshot.expires_at;
            if (permit_expiry > record->snapshot.expires_at
                || permit_expiry > policy_.expires_at)
            {
                return {.code = GuardCode::proposal_expired};
            }
            if (permit_expiry > approval_expiry)
                return {.code = GuardCode::approval_expired};
            if (next_permit_serial_ == (std::numeric_limits<std::uint64_t>::max)()
                || audit_events_.size() >= policy_.limits.maximum_audit_events)
            {
                return {.code = GuardCode::storage_exhausted};
            }

            ExecutionPermit permit{
                ++next_permit_serial_,
                1u,
                policy_.identity,
                proposal,
                digest,
                now,
                permit_expiry};
            record->permit = permit;
            record->permit_consumed = false;
            record->snapshot.state = ProposalState::execution_eligible;
            (void)append_audit(AuditEvent{
                .occurred_at = now,
                .session = policy_.identity,
                .proposal = proposal,
                .proposal_digest = digest,
                .kind = AuditKind::permit_issued,
                .resulting_state = ProposalState::execution_eligible,
                .actor_id = record->snapshot.approval
                    ? record->snapshot.approval->operator_id
                    : record->snapshot.review->reviewer_id});
            return {
                .code = GuardCode::none,
                .permit = permit,
                .required_evidence = eligibility.required_evidence};
        }

        [[nodiscard]] GuardResult claim_execution_permit(
            const ExecutionPermit& permit,
            std::string actor_id,
            std::uint64_t now)
        {
            if (!permit.valid())
                return {GuardCode::permit_invalid};
            if (const GuardResult active = validate_active_session(
                    permit.session, now); !active)
            {
                return active;
            }
            ProposalRecord* record = find_record(permit.proposal);
            if (record == nullptr)
                return {GuardCode::proposal_not_found};
            if (record->snapshot.state != ProposalState::execution_eligible
                || !record->permit || record->permit_consumed
                || *record->permit != permit
                || permit.proposal_digest != record->snapshot.digest)
            {
                return {GuardCode::permit_invalid};
            }
            if (now > permit.expires_at)
                return {GuardCode::permit_expired};
            if (!valid_identifier(
                    actor_id,
                    policy_.limits.maximum_string_bytes))
            {
                return {GuardCode::invalid_identity};
            }
            if (audit_events_.size() >= policy_.limits.maximum_audit_events)
                return {GuardCode::storage_exhausted};

            record->snapshot.state = ProposalState::executing;
            (void)append_audit(AuditEvent{
                .occurred_at = now,
                .session = policy_.identity,
                .proposal = record->snapshot.identity,
                .proposal_digest = record->snapshot.digest,
                .kind = AuditKind::execution_started,
                .resulting_state = ProposalState::executing,
                .actor_id = std::move(actor_id)});
            return {};
        }

        [[nodiscard]] GuardResult record_evidence(
            EvidenceSubmission submission,
            std::uint64_t now)
        {
            if (!submission.permit.valid())
                return {GuardCode::permit_invalid};
            if (const GuardResult active = validate_active_session(
                    submission.permit.session, now); !active)
            {
                return active;
            }
            ProposalRecord* record = find_record(submission.permit.proposal);
            if (record == nullptr)
                return {GuardCode::proposal_not_found};
            if (record->snapshot.state == ProposalState::completed
                || record->snapshot.state == ProposalState::failed)
            {
                return {GuardCode::already_complete};
            }
            if (record->snapshot.state != ProposalState::executing
                || !record->permit || record->permit_consumed
                || *record->permit != submission.permit
                || submission.permit.proposal_digest != record->snapshot.digest)
            {
                return {GuardCode::permit_invalid};
            }
            if (now > submission.permit.expires_at)
                return {GuardCode::permit_expired};
            if (!valid_identifier(submission.actor_id, policy_.limits.maximum_string_bytes)
                || !valid_text(submission.summary, false)
                || submission.items.empty()
                || submission.items.size() > policy_.limits.maximum_evidence_items)
            {
                return {GuardCode::invalid_text};
            }

            EvidenceRequirement supplied = EvidenceRequirement::none;
            std::size_t stored_bytes = submission.actor_id.size() + submission.summary.size();
            for (const EvidenceItem& item : submission.items)
            {
                if (!item.verified)
                    return {GuardCode::evidence_unverified};
                if (!item.content_digest.valid()
                    || !valid_text(item.locator, false)
                    || !valid_text(item.summary, false))
                {
                    return {GuardCode::invalid_text};
                }
                if (stored_bytes > (std::numeric_limits<std::size_t>::max)()
                    - item.locator.size() - item.summary.size())
                {
                    return {GuardCode::storage_exhausted};
                }
                stored_bytes += item.locator.size() + item.summary.size();
                supplied = supplied | requirement_for_evidence(item.category);
            }
            const EvidenceRequirement required = submission.outcome == ExecutionOutcome::succeeded
                ? record->snapshot.required_evidence
                : EvidenceRequirement::diagnostic;
            if (!has_evidence(supplied, required))
                return {GuardCode::evidence_missing};
            if (stored_bytes > policy_.limits.maximum_total_evidence_bytes
                || evidence_records_.size() >= policy_.limits.maximum_evidence_records
                || audit_events_.size() >= policy_.limits.maximum_audit_events
                || evidence_bytes_ > policy_.limits.maximum_total_evidence_bytes - stored_bytes)
            {
                return {GuardCode::storage_exhausted};
            }

            EvidenceRecord evidence{
                .session = policy_.identity,
                .proposal = record->snapshot.identity,
                .proposal_digest = record->snapshot.digest,
                .permit_serial = submission.permit.serial,
                .actor_id = std::move(submission.actor_id),
                .outcome = submission.outcome,
                .recorded_at = now,
                .summary = std::move(submission.summary),
                .items = std::move(submission.items)};
            evidence.evidence_digest = evidence_digest(evidence);
            if (!evidence.evidence_digest.valid())
                return {GuardCode::digest_mismatch};
            const ProposalDigest evidence_digest_value = evidence.evidence_digest;
            const std::string audit_actor = evidence.actor_id;
            evidence_records_.push_back(std::move(evidence));
            evidence_bytes_ += stored_bytes;
            record->permit_consumed = true;
            record->snapshot.state = submission.outcome == ExecutionOutcome::succeeded
                ? ProposalState::completed
                : ProposalState::failed;
            (void)append_audit(AuditEvent{
                .occurred_at = now,
                .session = policy_.identity,
                .proposal = record->snapshot.identity,
                .proposal_digest = record->snapshot.digest,
                .evidence_digest = evidence_digest_value,
                .kind = AuditKind::evidence_accepted,
                .resulting_state = record->snapshot.state,
                .actor_id = audit_actor});
            return {};
        }

        [[nodiscard]] GuardResult cancel(
            CancellationRequest request,
            std::uint64_t now)
        {
            if (const GuardResult active = validate_active_session(request.session, now); !active)
                return active;
            ProposalRecord* record = find_record(request.proposal);
            if (record == nullptr)
                return {GuardCode::proposal_not_found};
            if (const GuardResult current = validate_current(*record, request.proposal_digest, now);
                !current)
            {
                return current;
            }
            if (!valid_identifier(request.actor_id, policy_.limits.maximum_string_bytes)
                || !valid_text(request.reason, false))
            {
                return {GuardCode::invalid_text};
            }
            if (terminal(record->snapshot.state))
                return {GuardCode::already_complete};
            if (audit_events_.size() >= policy_.limits.maximum_audit_events)
                return {GuardCode::storage_exhausted};
            record->snapshot.state = ProposalState::cancelled;
            record->permit_consumed = true;
            (void)append_audit(AuditEvent{
                .occurred_at = now,
                .session = policy_.identity,
                .proposal = record->snapshot.identity,
                .proposal_digest = record->snapshot.digest,
                .kind = AuditKind::proposal_cancelled,
                .resulting_state = ProposalState::cancelled,
                .actor_id = std::move(request.actor_id)});
            return {};
        }

        [[nodiscard]] GuardResult cancel_session(
            SessionIdentity session,
            std::string operator_id,
            std::string reason,
            std::uint64_t now)
        {
            if (const GuardResult active = validate_active_session(session, now); !active)
                return active;
            if (!operator_allowed(operator_id))
                return {GuardCode::operator_denied};
            if (!valid_text(reason, false))
                return {GuardCode::invalid_text};
            if (audit_events_.size() >= policy_.limits.maximum_audit_events)
                return {GuardCode::storage_exhausted};
            session_cancelled_ = true;
            for (ProposalRecord& record : records_)
            {
                if (!terminal(record.snapshot.state))
                {
                    record.snapshot.state = ProposalState::cancelled;
                    record.permit_consumed = true;
                }
            }
            (void)append_audit(AuditEvent{
                .occurred_at = now,
                .session = policy_.identity,
                .kind = AuditKind::session_cancelled,
                .resulting_state = ProposalState::cancelled,
                .actor_id = std::move(operator_id)});
            return {};
        }

        [[nodiscard]] std::optional<ProposalSnapshot> snapshot(
            ProposalIdentity identity) const
        {
            const ProposalRecord* record = find_record(identity);
            return record == nullptr
                ? std::optional<ProposalSnapshot>{}
                : std::optional<ProposalSnapshot>{record->snapshot};
        }

        [[nodiscard]] std::span<const AuditEvent> audit() const noexcept
        {
            return audit_events_;
        }

        [[nodiscard]] std::span<const EvidenceRecord> evidence() const noexcept
        {
            return evidence_records_;
        }

        [[nodiscard]] std::size_t proposal_bytes() const noexcept
        {
            return proposal_bytes_;
        }

        [[nodiscard]] std::size_t evidence_bytes() const noexcept
        {
            return evidence_bytes_;
        }

    private:
        struct ProposalRecord final
        {
            ProposalSnapshot snapshot{};
            std::optional<ExecutionPermit> permit{};
            bool permit_consumed{};
        };

        [[nodiscard]] static bool valid_identifier(
            std::string_view value,
            std::size_t maximum_bytes) noexcept
        {
            if (value.empty() || value.size() > maximum_bytes)
                return false;
            for (const char character : value)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                const bool accepted = (byte >= 'a' && byte <= 'z')
                    || (byte >= 'A' && byte <= 'Z')
                    || (byte >= '0' && byte <= '9')
                    || character == '_' || character == '-' || character == '.'
                    || character == '@' || character == '/';
                if (!accepted)
                    return false;
            }
            return true;
        }

        [[nodiscard]] bool valid_text(
            std::string_view value,
            bool allow_empty) const noexcept
        {
            if ((!allow_empty && value.empty())
                || value.size() > policy_.limits.maximum_string_bytes)
            {
                return false;
            }
            for (const char character : value)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                if (byte == 0u || (byte < 0x20u && character != '\n'
                    && character != '\r' && character != '\t'))
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] static constexpr bool valid_permissions(
            WorkspacePermission permissions) noexcept
        {
            constexpr std::uint8_t known =
                static_cast<std::uint8_t>(WorkspacePermission::read)
                | static_cast<std::uint8_t>(WorkspacePermission::write)
                | static_cast<std::uint8_t>(WorkspacePermission::create)
                | static_cast<std::uint8_t>(WorkspacePermission::remove)
                | static_cast<std::uint8_t>(WorkspacePermission::execute);
            const std::uint8_t bits = static_cast<std::uint8_t>(permissions);
            return bits != 0u && (bits & static_cast<std::uint8_t>(~known)) == 0u;
        }

        [[nodiscard]] GuardResult validate_active_session(
            SessionIdentity session,
            std::uint64_t now) const noexcept
        {
            if (!policy_validation_)
                return policy_validation_;
            if (session != policy_.identity)
                return {GuardCode::invalid_session};
            if (session_cancelled_)
                return {GuardCode::session_cancelled};
            if (now < policy_.opened_at)
                return {GuardCode::invalid_timestamp};
            if (now > policy_.expires_at)
                return {GuardCode::session_expired};
            return {};
        }

        [[nodiscard]] GuardResult validate_current(
            const ProposalRecord& record,
            ProposalDigest digest,
            std::uint64_t now) const noexcept
        {
            if (digest != record.snapshot.digest)
                return {GuardCode::digest_mismatch};
            if (now > record.snapshot.expires_at)
                return {GuardCode::proposal_expired};
            return {};
        }

        [[nodiscard]] GuardCode validate_operation(
            DevelopmentOperation& operation,
            const std::vector<DevelopmentOperation>& operations,
            std::size_t operation_index,
            std::size_t& stored_bytes) const
        {
            if (operation.stable_id == 0u
                || operation.risk != expected_risk(operation.category)
                || !valid_text(operation.summary, false))
            {
                return operation.risk != expected_risk(operation.category)
                    ? GuardCode::risk_mismatch
                    : GuardCode::invalid_operation;
            }
            for (std::size_t index = 0u; index < operation_index; ++index)
            {
                if (operations[index].stable_id == operation.stable_id)
                    return GuardCode::invalid_identity;
            }
            if (operation.workspace_intents.size()
                    > policy_.limits.maximum_intents_per_operation
                || (operation.workspace_intents.empty()
                    && operation.category != OperationCategory::network_access))
            {
                return GuardCode::invalid_operation;
            }
            stored_bytes += operation.summary.size();
            bool has_write = false;
            bool has_execute = false;
            for (WorkspaceIntent& intent : operation.workspace_intents)
            {
                const auto path = canonical_relative_path(intent.relative_path);
                if (!path || !valid_permissions(intent.permissions))
                    return GuardCode::invalid_path;
                intent.relative_path = *path;
                if (!intent_allowed(intent))
                    return GuardCode::path_denied;
                if (operation.category == OperationCategory::inspect
                    && intent.permissions != WorkspacePermission::read)
                {
                    return GuardCode::risk_mismatch;
                }
                if (operation.category == OperationCategory::source_write
                    && intent.area != WorkspaceArea::engine_source)
                {
                    return GuardCode::path_denied;
                }
                if (operation.category == OperationCategory::project_write
                    && intent.area != WorkspaceArea::project_source)
                {
                    return GuardCode::path_denied;
                }
                const WorkspacePermission write_mask = WorkspacePermission::write
                    | WorkspacePermission::create | WorkspacePermission::remove;
                has_write = has_write
                    || static_cast<std::uint8_t>(intent.permissions & write_mask) != 0u;
                has_execute = has_execute
                    || has_permission(intent.permissions, WorkspacePermission::execute);
                if (stored_bytes > (std::numeric_limits<std::size_t>::max)()
                    - intent.relative_path.size())
                {
                    return GuardCode::storage_exhausted;
                }
                stored_bytes += intent.relative_path.size();
            }

            const bool writes_content =
                operation.category == OperationCategory::source_write
                || operation.category == OperationCategory::project_write;
            if (operation.content_transitions.size()
                    > policy_.limits.maximum_content_transitions_per_operation
                || (writes_content && operation.content_transitions.empty())
                || (!writes_content && !operation.content_transitions.empty()))
            {
                return GuardCode::invalid_operation;
            }

            for (std::size_t transition_index = 0u;
                 transition_index < operation.content_transitions.size();
                 ++transition_index)
            {
                ContentTransition& transition =
                    operation.content_transitions[transition_index];
                const auto path = canonical_relative_path(
                    transition.relative_path);
                if (!path
                    || !valid_content_state(transition.before)
                    || !valid_content_state(transition.after)
                    || same_content_state(transition.before, transition.after)
                    || transition.before.byte_count
                        > policy_.limits.maximum_content_bytes_per_transition
                    || transition.after.byte_count
                        > policy_.limits.maximum_content_bytes_per_transition)
                {
                    return GuardCode::invalid_operation;
                }
                transition.relative_path = *path;
                for (std::size_t previous = 0u;
                     previous < transition_index;
                     ++previous)
                {
                    if (operation.content_transitions[previous].relative_path
                        == transition.relative_path)
                    {
                        return GuardCode::invalid_path;
                    }
                }

                const WorkspacePermission required =
                    transition.before.kind == ContentStateKind::absent
                    ? WorkspacePermission::create
                    : transition.after.kind == ContentStateKind::absent
                        ? WorkspacePermission::remove
                        : WorkspacePermission::write;
                const bool intent_matches = std::any_of(
                    operation.workspace_intents.begin(),
                    operation.workspace_intents.end(),
                    [&](const WorkspaceIntent& intent)
                    {
                        return intent.relative_path == transition.relative_path
                            && has_permission(intent.permissions, required);
                    });
                if (!intent_matches)
                    return GuardCode::path_denied;

                if (stored_bytes > (std::numeric_limits<std::size_t>::max)()
                    - transition.relative_path.size())
                {
                    return GuardCode::storage_exhausted;
                }
                stored_bytes += transition.relative_path.size();
            }

            if ((operation.category == OperationCategory::source_write
                    || operation.category == OperationCategory::project_write)
                && !has_write)
            {
                return GuardCode::invalid_operation;
            }
            if (operation.category == OperationCategory::run && !has_execute)
                return GuardCode::invalid_operation;
            return GuardCode::none;
        }

        [[nodiscard]] bool intent_allowed(const WorkspaceIntent& intent) const noexcept
        {
            for (const WorkspaceRule& rule : policy_.workspace_allowlist)
            {
                if (rule.area != intent.area
                    || !has_permission(rule.permissions, intent.permissions))
                {
                    continue;
                }
                if (intent.relative_path == rule.relative_root)
                    return true;
                if (intent.relative_path.size() > rule.relative_root.size()
                    && intent.relative_path.starts_with(rule.relative_root)
                    && intent.relative_path[rule.relative_root.size()] == '/')
                {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] bool operator_allowed(std::string_view operator_id) const noexcept
        {
            return valid_identifier(operator_id, policy_.limits.maximum_string_bytes)
                && std::find(policy_.approved_operator_ids.begin(),
                    policy_.approved_operator_ids.end(), operator_id)
                    != policy_.approved_operator_ids.end();
        }

        [[nodiscard]] static constexpr bool terminal(ProposalState state) noexcept
        {
            return state == ProposalState::completed || state == ProposalState::failed
                || state == ProposalState::rejected || state == ProposalState::cancelled
                || state == ProposalState::expired;
        }

        [[nodiscard]] ProposalRecord* find_record(ProposalIdentity identity) noexcept
        {
            if (!identity.valid() || identity.slot >= records_.size())
                return nullptr;
            ProposalRecord& record = records_[identity.slot];
            return record.snapshot.identity == identity ? &record : nullptr;
        }

        [[nodiscard]] const ProposalRecord* find_record(
            ProposalIdentity identity) const noexcept
        {
            if (!identity.valid() || identity.slot >= records_.size())
                return nullptr;
            const ProposalRecord& record = records_[identity.slot];
            return record.snapshot.identity == identity ? &record : nullptr;
        }

        [[nodiscard]] bool append_audit(AuditEvent event)
        {
            if (audit_events_.size() >= policy_.limits.maximum_audit_events
                || next_audit_sequence_ == (std::numeric_limits<std::uint64_t>::max)())
            {
                return false;
            }
            event.sequence = ++next_audit_sequence_;
            audit_events_.push_back(std::move(event));
            return true;
        }

        void mark_expired(ProposalIdentity identity, std::uint64_t now)
        {
            ProposalRecord* record = find_record(identity);
            if (record == nullptr || terminal(record->snapshot.state)
                || audit_events_.size() >= policy_.limits.maximum_audit_events)
            {
                return;
            }
            record->snapshot.state = ProposalState::expired;
            record->permit_consumed = true;
            (void)append_audit(AuditEvent{
                .occurred_at = now,
                .session = policy_.identity,
                .proposal = record->snapshot.identity,
                .proposal_digest = record->snapshot.digest,
                .kind = AuditKind::proposal_expired,
                .resulting_state = ProposalState::expired,
                .actor_id = "epoch.host"});
        }

        [[nodiscard]] static ProposalDigest proposal_digest(
            const ProposalSnapshot& proposal) noexcept
        {
            development_guard_detail::DigestWriter writer{};
            writer.u32(contract_schema_version);
            writer.u64(proposal.session.value);
            writer.u32(proposal.session.generation);
            writer.u32(proposal.identity.slot);
            writer.u32(proposal.identity.generation);
            writer.text(proposal.title);
            writer.text(proposal.rationale);
            writer.u64(proposal.created_at);
            writer.u64(proposal.expires_at);
            writer.u64(static_cast<std::uint64_t>(proposal.operations.size()));
            for (const DevelopmentOperation& operation : proposal.operations)
            {
                writer.u64(operation.stable_id);
                writer.u8(static_cast<std::uint8_t>(operation.category));
                writer.u8(static_cast<std::uint8_t>(operation.risk));
                writer.text(operation.summary);
                writer.u64(static_cast<std::uint64_t>(operation.workspace_intents.size()));
                for (const WorkspaceIntent& intent : operation.workspace_intents)
                {
                    writer.u8(static_cast<std::uint8_t>(intent.area));
                    writer.text(intent.relative_path);
                    writer.u8(static_cast<std::uint8_t>(intent.permissions));
                }
                writer.u64(static_cast<std::uint64_t>(
                    operation.content_transitions.size()));
                for (const ContentTransition& transition :
                     operation.content_transitions)
                {
                    writer.text(transition.relative_path);
                    writer.u8(static_cast<std::uint8_t>(
                        transition.before.kind));
                    for (const std::uint8_t byte :
                         transition.before.digest.bytes)
                    {
                        writer.u8(byte);
                    }
                    writer.u64(transition.before.byte_count);
                    writer.u8(static_cast<std::uint8_t>(
                        transition.after.kind));
                    for (const std::uint8_t byte :
                         transition.after.digest.bytes)
                    {
                        writer.u8(byte);
                    }
                    writer.u64(transition.after.byte_count);
                }
            }
            return {writer.finish()};
        }

        [[nodiscard]] static ProposalDigest evidence_digest(
            const EvidenceRecord& evidence) noexcept
        {
            development_guard_detail::DigestWriter writer{};
            writer.u32(contract_schema_version);
            writer.u64(evidence.session.value);
            writer.u32(evidence.session.generation);
            writer.u32(evidence.proposal.slot);
            writer.u32(evidence.proposal.generation);
            for (const std::uint8_t byte : evidence.proposal_digest.bytes)
                writer.u8(byte);
            writer.u64(evidence.permit_serial);
            writer.text(evidence.actor_id);
            writer.u8(static_cast<std::uint8_t>(evidence.outcome));
            writer.u64(evidence.recorded_at);
            writer.text(evidence.summary);
            writer.u64(static_cast<std::uint64_t>(evidence.items.size()));
            for (const EvidenceItem& item : evidence.items)
            {
                writer.u8(static_cast<std::uint8_t>(item.category));
                writer.text(item.locator);
                for (const std::uint8_t byte : item.content_digest.bytes)
                    writer.u8(byte);
                writer.text(item.summary);
                writer.u8(item.verified ? 1u : 0u);
            }
            return {writer.finish()};
        }

        [[nodiscard]] static constexpr EvidenceRequirement requirement_for_evidence(
            EvidenceCategory category) noexcept
        {
            switch (category)
            {
            case EvidenceCategory::inspection_record:
                return EvidenceRequirement::inspection_record;
            case EvidenceCategory::patch: return EvidenceRequirement::patch;
            case EvidenceCategory::build_log: return EvidenceRequirement::build_log;
            case EvidenceCategory::test_report: return EvidenceRequirement::test_report;
            case EvidenceCategory::runtime_report: return EvidenceRequirement::runtime_report;
            case EvidenceCategory::network_report: return EvidenceRequirement::network_report;
            case EvidenceCategory::release_manifest:
                return EvidenceRequirement::release_manifest;
            case EvidenceCategory::diagnostic: return EvidenceRequirement::diagnostic;
            }
            return EvidenceRequirement::none;
        }

        SessionPolicy policy_{};
        GuardResult policy_validation_{GuardCode::invalid_policy};
        std::vector<ProposalRecord> records_{};
        std::vector<EvidenceRecord> evidence_records_{};
        std::vector<AuditEvent> audit_events_{};
        std::size_t proposal_bytes_{};
        std::size_t evidence_bytes_{};
        std::uint64_t next_permit_serial_{};
        std::uint64_t next_audit_sequence_{};
        bool session_cancelled_{};
    };
}
