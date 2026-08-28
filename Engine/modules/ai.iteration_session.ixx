/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

export module ai.iteration_session;

export import ai.iteration_loop;

export namespace epochengine::ai::iteration_session
{
    enum class SourceAuthorityKind : std::uint8_t
    {
        unavailable,
        explicit_checkout,
        verified_cache
    };

    enum class CandidatePolicy : std::uint8_t
    {
        manual_each_candidate,
        auto_validate_within_approved_scope
    };

    enum class SessionPhase : std::uint8_t
    {
        idle,
        selection_required,
        awaiting_context_share,
        awaiting_candidate,
        awaiting_candidate_approval,
        executing_candidate,
        validating_candidate,
        awaiting_repair,
        candidate_verified,
        cancelled,
        blocked
    };

    enum class ValidationActor : std::uint8_t
    {
        debug_compiler,
        debug_contract,
        release_compiler,
        release_contract,
        headless_compiler,
        headless_contract,
        full_validation
    };

    struct RequestIdentity final
    {
        std::uint64_t session_id{};
        std::uint64_t request_id{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return session_id != 0u && request_id != 0u;
        }

        friend constexpr bool operator==(
            const RequestIdentity&,
            const RequestIdentity&) = default;
    };

    struct SourceAuthority final
    {
        SourceAuthorityKind kind{SourceAuthorityKind::unavailable};
        std::filesystem::path root{};
        std::string source_version{};
        std::string commit{};
        std::string receipt_digest{};
        bool verified{};
    };

    struct CuratedFile final
    {
        std::string relative_path{};
        std::string sha256{};
        std::uint64_t byte_count{};

        friend bool operator==(const CuratedFile&, const CuratedFile&) = default;
    };

    struct ValidationEvidence final
    {
        ValidationActor actor{ValidationActor::debug_compiler};
        std::string candidate_digest{};
        std::string evidence_digest{};
        std::string summary{};
        bool passed{};
    };

    struct SessionConfiguration final
    {
        std::string objective{};
        std::string model_name{};
        SourceAuthority source{};
        std::vector<CuratedFile> curated_files{};
        CandidatePolicy policy{CandidatePolicy::manual_each_candidate};
        std::uint32_t maximum_repair_attempts{3u};
    };

    struct CandidateReport final
    {
        RequestIdentity identity{};
        SessionPhase phase{SessionPhase::idle};
        CandidatePolicy policy{CandidatePolicy::manual_each_candidate};
        SourceAuthority source{};
        std::string objective_digest{};
        std::string scope_digest{};
        std::string proposal_digest{};
        std::string candidate_digest{};
        std::string model_name{};
        std::vector<CuratedFile> curated_files{};
        std::vector<ValidationEvidence> validation{};
        std::uint32_t repair_attempt{};
        std::string status{};
        bool candidate_approved{};
        bool resume_requires_revalidation{true};
    };

    struct SessionResult final
    {
        bool accepted{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return accepted;
        }
    };

    struct CuratedInspection final
    {
        bool accepted{};
        std::vector<CuratedFile> files{};
        std::string status{};
    };

    [[nodiscard]] CuratedInspection inspect_curated_files(
        const SourceAuthority& authority,
        const std::vector<std::string>& relative_paths);

    class IterationSession final
    {
    public:
        [[nodiscard]] SessionResult configure(SessionConfiguration configuration);
        [[nodiscard]] SessionResult require_selection(std::string reason);
        [[nodiscard]] SessionResult context_shared();
        [[nodiscard]] SessionResult stage_candidate(
            RequestIdentity identity,
            std::string_view proposal_bytes);
        [[nodiscard]] SessionResult approve_candidate(RequestIdentity identity);
        [[nodiscard]] SessionResult record_implementation(
            RequestIdentity identity,
            std::string evidence_digest,
            std::string summary);
        [[nodiscard]] SessionResult record_validation(
            RequestIdentity identity,
            ValidationEvidence evidence,
            bool validation_sequence_complete);
        [[nodiscard]] SessionResult cancel(
            RequestIdentity identity,
            std::string reason);
        [[nodiscard]] SessionResult resume_scope_fail_closed(
            const CandidateReport& report,
            const SourceAuthority& current_source,
            std::vector<CuratedFile> current_files);
        [[nodiscard]] RequestIdentity identity() const noexcept;
        [[nodiscard]] CandidateReport report() const;
        [[nodiscard]] std::string serialize_report() const;

    private:
        [[nodiscard]] bool identity_matches(RequestIdentity identity) const noexcept;
        [[nodiscard]] SessionResult reject(std::string status);
        [[nodiscard]] SessionResult accept(std::string status);
        [[nodiscard]] SessionResult record_loop_model_milestone(
            iteration::Milestone milestone,
            iteration::EvidenceDigest digest,
            iteration::EvidenceDigest authority,
            std::string summary);
        [[nodiscard]] SessionResult record_loop_host_milestone(
            iteration::Milestone milestone,
            iteration::EvidenceActor actor,
            iteration::EvidenceDigest digest,
            std::string summary,
            bool passed);

        CandidateReport report_{};
        iteration::BoundedIterationLoop loop_{};
        iteration::EvidenceDigest scope_digest_{};
        iteration::EvidenceDigest proposal_digest_{};
        std::uint64_t next_request_id_{};
        bool configured_{};
        bool compiler_milestone_recorded_{};
    };

    [[nodiscard]] bool run_contract();
}
