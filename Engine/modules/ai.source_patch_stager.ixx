/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module ai.source_patch_stager;

export import ai.source_patch_proposal;
export import ai.source_patch_bundle;
export import ai.curated_context_bundle;
export import ai.iteration_supervisor_control;
export import epoch.build_validation;

export namespace epochengine::ai::source_patch_stager
{
    inline constexpr std::string_view plan_schema{
        "epoch-ai-source-patch-stage-plan/v1"};
    inline constexpr std::string_view staged_evidence_schema{
        "epoch-ai-source-patch-staged-evidence/v1"};
    inline constexpr std::string_view build_evidence_schema{
        "epoch-ai-source-patch-build-evidence/v1"};

    struct Limits final
    {
        std::uint32_t maximum_operations{16u};
        std::uint32_t maximum_identifier_bytes{256u};
        std::uint32_t maximum_path_bytes{1024u};
        std::uint64_t maximum_file_bytes{16u * 1024u * 1024u};
        std::uint64_t maximum_total_postimage_bytes{64u * 1024u * 1024u};
        std::uint32_t maximum_consumed_requests{512u};

        friend bool operator==(const Limits&, const Limits&) = default;
    };

    struct Configuration final
    {
        std::string stager_id{};
        std::string project_id{};
        std::string session_id{};
        std::uint64_t curated_session_id{};
        std::string campaign_id{};
        std::string objective_id{};
        std::string operation_id{};
        std::filesystem::path sandbox_root{};
        std::string sandbox_root_sha256{};
        std::string source_commit{};
        build_validation::SemanticVersion source_version{};
        build_validation::AdmissionPolicy build_policy{};
        std::uint64_t created_at_unix_seconds{};
        std::uint64_t expires_at_unix_seconds{};
        Limits limits{};
        bool sandbox_staging_permitted{true};
        bool live_source_write_permitted{};
        bool promotion_permitted{};
        bool commit_permitted{};
        bool release_permitted{};
        bool server_permitted{};
        bool network_listener_permitted{};

        friend bool operator==(const Configuration&, const Configuration&) = default;
    };

    enum class PathKind : std::uint8_t
    {
        missing,
        regular,
        symlink,
        reparse_point,
        other
    };

    struct HostPreimage final
    {
        std::string relative_path{};
        PathKind kind{PathKind::missing};
        std::string exact_bytes{};
        std::string content_sha256{};
        std::uint64_t byte_count{};
        std::string sandbox_root_sha256{};
        std::string destination_binding_sha256{};
        bool beneath_sandbox{};
        bool path_components_link_free{};
        bool path_components_reparse_free{};

        friend bool operator==(const HostPreimage&, const HostPreimage&) = default;
    };

    struct PrepareRequest final
    {
        std::string request_id{};
        std::uint64_t expected_generation{};
        std::string expected_state_sha256{};
        std::uint64_t expected_supervisor_generation{};
        std::string expected_supervisor_state_sha256{};
        std::string expected_proposal_sha256{};
        std::string expected_bundle_sha256{};
        std::string expected_approval_receipt_sha256{};
        std::uint64_t now_unix_seconds{};
        source_patch_proposal::SealedProposal proposal{};
        curated_context_bundle::Bundle curated{};
        iteration_supervisor_control::Snapshot supervisor{};
        iteration_supervisor_control::Receipt approval{};
        std::vector<HostPreimage> preimages{};
        bool operator_approved{};
        bool sandbox_staging_requested{true};
        bool live_source_write_requested{};
        bool arbitrary_file_read_requested{};
        bool compiler_invocation_requested{};
        bool model_launch_requested{};
        bool promotion_requested{};
        bool commit_requested{};
        bool release_requested{};
        bool server_requested{};
        bool network_listener_requested{};

        friend bool operator==(const PrepareRequest&, const PrepareRequest&) = default;
    };

    struct StagingOperation final
    {
        std::string operation_id{};
        source_patch_bundle::Operation kind{
            source_patch_bundle::Operation::update};
        std::string relative_path{};
        PathKind expected_preimage_kind{PathKind::regular};
        std::string before_sha256{};
        std::uint64_t before_byte_count{};
        std::string after_sha256{};
        std::uint64_t after_byte_count{};
        std::string postimage_utf8{};
        std::string destination_binding_sha256{};
        std::string operation_sha256{};

        friend bool operator==(const StagingOperation&, const StagingOperation&) = default;
    };

    struct TransitionReceipt final
    {
        std::string receipt_id{};
        std::string request_id{};
        std::string request_sha256{};
        std::string previous_receipt_sha256{};
        std::string state_before_sha256{};
        std::string state_after_sha256{};
        std::string subject_sha256{};
        std::uint64_t resulting_generation{};
        std::uint64_t completed_at_unix_seconds{};
        std::string receipt_sha256{};
        bool live_source_write_permitted{};
        bool promotion_permitted{};
        bool commit_permitted{};
        bool release_permitted{};

        friend bool operator==(const TransitionReceipt&, const TransitionReceipt&) = default;
    };

    struct StagePlan final
    {
        std::string schema{plan_schema};
        std::string plan_id{};
        std::string project_id{};
        std::string session_id{};
        std::string campaign_id{};
        std::string objective_id{};
        std::string operation_id{};
        std::string sandbox_root_sha256{};
        std::string proposal_sha256{};
        std::string bundle_sha256{};
        std::string approval_receipt_sha256{};
        std::uint64_t supervisor_generation{};
        std::string supervisor_state_sha256{};
        std::vector<StagingOperation> operations{};
        std::string staging_manifest_sha256{};
        std::string plan_sha256{};
        TransitionReceipt receipt{};
        bool host_execution_required{true};
        bool sandbox_only{true};
        bool live_source_write_permitted{};
        bool promotion_permitted{};
        bool commit_permitted{};
        bool release_permitted{};

        friend bool operator==(const StagePlan&, const StagePlan&) = default;
    };

    struct StagedObservation final
    {
        std::string operation_id{};
        std::string relative_path{};
        PathKind kind{PathKind::regular};
        std::string exact_bytes{};
        std::string content_sha256{};
        std::uint64_t byte_count{};
        std::string sandbox_root_sha256{};
        std::string destination_binding_sha256{};
        bool beneath_sandbox{};
        bool path_components_link_free{};
        bool path_components_reparse_free{};

        friend bool operator==(const StagedObservation&, const StagedObservation&) = default;
    };

    struct StagedReport final
    {
        std::string report_id{};
        std::uint64_t expected_generation{};
        std::string expected_state_sha256{};
        std::string plan_sha256{};
        std::string staging_manifest_sha256{};
        std::vector<StagedObservation> observations{};
        std::uint64_t completed_at_unix_seconds{};
        bool host_completed{};
        bool atomic_transaction{};
        bool rollback_available{};
        bool live_source_touched{};
        bool promotion_performed{};
        bool commit_performed{};
        bool release_performed{};

        friend bool operator==(const StagedReport&, const StagedReport&) = default;
    };

    struct StagedEvidence final
    {
        std::string schema{staged_evidence_schema};
        std::string report_id{};
        std::string plan_sha256{};
        std::string staging_manifest_sha256{};
        std::vector<std::string> observation_sha256s{};
        std::string evidence_sha256{};
        TransitionReceipt receipt{};
        bool verified{};
        bool live_source_write_permitted{};
        bool promotion_permitted{};
        bool commit_permitted{};
        bool release_permitted{};

        friend bool operator==(const StagedEvidence&, const StagedEvidence&) = default;
    };

    struct BuildReport final
    {
        std::string report_id{};
        std::uint64_t expected_generation{};
        std::string expected_state_sha256{};
        std::string staging_evidence_sha256{};
        std::string staging_manifest_sha256{};
        build_validation::ValidationReceipt receipt{};
        std::string receipt_sha256{};
        std::uint64_t completed_at_unix_seconds{};
        bool local_host{};
        bool trusted_host{};
        bool host_completed{};
        bool upload_performed{};
        bool promotion_performed{};
        bool commit_performed{};
        bool release_performed{};

        friend bool operator==(const BuildReport&, const BuildReport&) = default;
    };

    struct BuildEvidence final
    {
        std::string schema{build_evidence_schema};
        std::string report_id{};
        std::string staging_evidence_sha256{};
        std::string staging_manifest_sha256{};
        std::string validation_receipt_sha256{};
        std::string evidence_sha256{};
        TransitionReceipt receipt{};
        bool admitted{};
        bool upload_permitted{};
        bool promotion_permitted{};
        bool commit_permitted{};
        bool release_permitted{};

        friend bool operator==(const BuildEvidence&, const BuildEvidence&) = default;
    };

    enum class Phase : std::uint8_t
    {
        idle,
        ready,
        prepared,
        staged_verified,
        build_admitted
    };

    struct Snapshot final
    {
        Configuration configuration{};
        Phase phase{Phase::idle};
        std::uint64_t generation{};
        std::string previous_state_sha256{};
        std::string state_sha256{};
        std::string last_receipt_sha256{};
        std::string active_plan_sha256{};
        std::string active_manifest_sha256{};
        std::string active_staging_evidence_sha256{};
        std::vector<std::string> consumed_request_ids{};
        std::vector<std::string> consumed_request_sha256s{};
        bool live_source_write_permitted{};
        bool arbitrary_file_read_permitted{};
        bool compiler_invocation_permitted{};
        bool model_launch_permitted{};
        bool promotion_permitted{};
        bool commit_permitted{};
        bool release_permitted{};
        bool server_permitted{};
        bool network_listener_permitted{};

        friend bool operator==(const Snapshot&, const Snapshot&) = default;
    };

    enum class Code : std::uint8_t
    {
        ready,
        prepared,
        staged_verified,
        build_admitted,
        invalid_configuration,
        malformed_request,
        invalid_authority,
        approval_required,
        authority_broadening,
        stale_state,
        replay_refused,
        proposal_tampered,
        bundle_tampered,
        bundle_mismatch,
        invalid_sandbox,
        path_escape,
        link_ambiguity,
        preimage_mismatch,
        partial_operations,
        extra_operations,
        wrong_postimage,
        untrusted_build,
        failed_build,
        build_mismatch
    };

    struct Result final
    {
        Code code{Code::invalid_configuration};
        Snapshot snapshot{};
        std::optional<StagePlan> plan{};
        std::optional<StagedEvidence> staged{};
        std::optional<BuildEvidence> build{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready || code == Code::prepared
                || code == Code::staged_verified
                || code == Code::build_admitted;
        }
    };

    [[nodiscard]] std::string sha256_hex(std::string_view bytes);

    class Engine final
    {
    public:
        [[nodiscard]] Result begin(const Configuration& configuration);
        [[nodiscard]] Result prepare(const PrepareRequest& request);
        [[nodiscard]] Result verify_staged(
            const StagePlan& plan,
            const StagedReport& report);
        [[nodiscard]] Result admit_build(
            const StagedEvidence& staged,
            const BuildReport& report);
        [[nodiscard]] const Snapshot& snapshot() const noexcept;

    private:
        Snapshot snapshot_{};
        bool begun_{};
    };

    [[nodiscard]] bool run_contract();
}
