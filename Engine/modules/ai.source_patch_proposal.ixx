/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module ai.source_patch_proposal;

export namespace epochengine::ai::source_patch_proposal
{
    inline constexpr std::string_view request_schema{
        "epoch-ai-source-patch-request/v1"};
    inline constexpr std::string_view proposal_schema{
        "epoch-ai-source-patch-proposal/v1"};
    inline constexpr std::string_view checkpoint_schema{
        "epoch-ai-source-patch-checkpoint/v1"};

    struct Limits final
    {
        std::uint32_t maximum_operations{16u};
        std::uint32_t maximum_hunks_per_file{64u};
        std::uint32_t maximum_path_bytes{1024u};
        std::uint32_t maximum_identifier_bytes{256u};
        std::uint32_t maximum_metadata_bytes{4096u};
        std::uint64_t maximum_source_bytes{16u * 1024u * 1024u};
        std::uint64_t maximum_replacement_bytes{4u * 1024u * 1024u};
        std::uint64_t maximum_total_postimage_bytes{64u * 1024u * 1024u};
        std::uint32_t maximum_seen_requests{512u};
        std::uint32_t maximum_checkpoint_bytes{1024u * 1024u};

        friend bool operator==(const Limits&, const Limits&) = default;
    };

    struct Configuration final
    {
        std::string engine_id{};
        std::string project_id{};
        std::string workspace_root_sha256{};
        std::string human_authority_sha256{};
        std::uint64_t source_revision{};
        std::uint64_t created_at_unix_seconds{};
        std::uint64_t expires_at_unix_seconds{};
        Limits limits{};
        std::vector<std::string> protected_path_prefixes{};

        friend bool operator==(const Configuration&, const Configuration&) = default;
    };

    struct ReviewedSource final
    {
        std::string project_id{};
        std::uint64_t source_revision{};
        std::string relative_path{};
        bool exists{};
        std::string content_sha256{};
        std::string utf8_bytes{};
        bool human_reviewed{};
        bool create_permitted{};
        bool update_permitted{};
        bool delete_permitted{};

        friend bool operator==(const ReviewedSource&, const ReviewedSource&) = default;
    };

    enum class OperationKind : std::uint8_t
    {
        create,
        update,
        remove
    };

    struct RangeEdit final
    {
        std::string edit_id{};
        std::uint64_t start_line{};
        std::uint64_t line_count{};
        std::string expected_range_sha256{};
        std::string replacement_utf8{};

        friend bool operator==(const RangeEdit&, const RangeEdit&) = default;
    };

    struct FileOperation final
    {
        std::string operation_id{};
        OperationKind kind{OperationKind::update};
        std::string relative_path{};
        std::uint64_t base_source_revision{};
        std::string base_content_sha256{};
        std::vector<RangeEdit> edits{};

        friend bool operator==(const FileOperation&, const FileOperation&) = default;
    };

    struct Authority final
    {
        std::string authority_id{};
        std::string session_id{};
        std::string actor_sha256{};
        std::string project_id{};
        std::string workspace_root_sha256{};
        std::string review_receipt_sha256{};
        bool source_snapshot_reviewed{};
        bool proposal_permitted{};
        bool source_apply_permitted{};
        bool arbitrary_file_read_permitted{};
        bool compiler_invocation_permitted{};
        bool model_launch_permitted{};
        bool network_permitted{};
        bool server_permitted{};
        bool listener_permitted{};
        bool promotion_permitted{};
        bool release_permitted{};

        friend bool operator==(const Authority&, const Authority&) = default;
    };

    struct Request final
    {
        std::string request_id{};
        std::string title{};
        std::string rationale{};
        std::uint64_t expected_engine_generation{};
        std::string expected_engine_state_sha256{};
        std::uint64_t now_unix_seconds{};
        Authority authority{};
        std::vector<FileOperation> operations{};

        friend bool operator==(const Request&, const Request&) = default;
    };

    struct HunkSummary final
    {
        std::string edit_id{};
        std::uint64_t start_line{};
        std::uint64_t removed_line_count{};
        std::uint64_t added_line_count{};
        std::uint64_t removed_byte_count{};
        std::uint64_t added_byte_count{};
        std::string before_sha256{};
        std::string after_sha256{};
        std::string hunk_sha256{};

        friend bool operator==(const HunkSummary&, const HunkSummary&) = default;
    };

    struct FileProposal final
    {
        std::string operation_id{};
        OperationKind kind{OperationKind::update};
        std::string relative_path{};
        std::uint64_t base_source_revision{};
        bool existed_before{};
        bool exists_after{};
        std::string before_sha256{};
        std::string after_sha256{};
        std::uint64_t before_byte_count{};
        std::uint64_t after_byte_count{};
        std::string postimage_utf8{};
        std::vector<HunkSummary> hunks{};
        std::string file_receipt_sha256{};

        friend bool operator==(const FileProposal&, const FileProposal&) = default;
    };

    struct AuthorityEvidence final
    {
        std::string authority_id{};
        std::string actor_sha256{};
        std::string review_receipt_sha256{};
        std::string workspace_root_sha256{};
        std::string project_id{};
        bool human_review_required{true};
        bool proposal_only{true};
        bool source_apply_permitted{};
        bool arbitrary_file_read_permitted{};
        bool compiler_invocation_permitted{};
        bool model_launch_permitted{};
        bool network_permitted{};
        bool server_permitted{};
        bool listener_permitted{};
        bool promotion_permitted{};
        bool release_permitted{};

        friend bool operator==(const AuthorityEvidence&, const AuthorityEvidence&) = default;
    };

    struct Receipt final
    {
        std::string receipt_id{};
        std::string request_id{};
        std::string request_sha256{};
        std::string previous_receipt_sha256{};
        std::string engine_state_before_sha256{};
        std::string engine_state_after_sha256{};
        std::string proposal_sha256{};
        std::uint64_t sealed_at_unix_seconds{};
        std::uint64_t resulting_generation{};
        std::string receipt_sha256{};

        friend bool operator==(const Receipt&, const Receipt&) = default;
    };

    struct SealedProposal final
    {
        std::string schema{proposal_schema};
        std::string proposal_id{};
        std::string request_sha256{};
        std::string project_id{};
        std::uint64_t source_revision{};
        std::string title{};
        std::string rationale{};
        std::vector<FileProposal> files{};
        AuthorityEvidence authority{};
        std::string aggregate_before_sha256{};
        std::string aggregate_after_sha256{};
        std::string canonical_proposal_sha256{};
        Receipt receipt{};
        bool simulated_in_memory{true};
        bool human_review_required{true};
        bool applied{};
        bool compiled{};
        bool tested{};
        bool promoted{};
        bool released{};

        friend bool operator==(const SealedProposal&, const SealedProposal&) = default;
    };

    struct Snapshot final
    {
        Configuration configuration{};
        std::uint64_t generation{};
        std::string previous_state_sha256{};
        std::string state_sha256{};
        std::string last_receipt_sha256{};
        std::vector<std::string> seen_request_ids{};
        std::vector<std::string> seen_request_sha256{};
        bool source_apply_permitted{};
        bool arbitrary_file_read_permitted{};
        bool compiler_invocation_permitted{};
        bool model_launch_permitted{};
        bool network_permitted{};
        bool server_permitted{};
        bool listener_permitted{};
        bool promotion_permitted{};
        bool release_permitted{};

        friend bool operator==(const Snapshot&, const Snapshot&) = default;
    };

    enum class Code : std::uint8_t
    {
        ready,
        sealed,
        invalid_configuration,
        malformed_request,
        noncanonical_request,
        request_tampered,
        invalid_authority,
        unreviewed_authority,
        authority_broadening,
        stale_state,
        stale_source,
        replay_refused,
        invalid_path,
        protected_path,
        duplicate_operation,
        source_missing,
        source_unexpected,
        source_not_reviewed,
        operation_not_permitted,
        binary_rejected,
        noncanonical_text,
        budget_exceeded,
        invalid_edit,
        unsorted_hunks,
        overlapping_hunks,
        fuzzy_context_rejected,
        no_change,
        invalid_checkpoint
    };

    struct Result final
    {
        Code code{Code::invalid_configuration};
        Snapshot snapshot{};
        std::optional<SealedProposal> proposal{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready || code == Code::sealed;
        }
    };

    struct Checkpoint final
    {
        std::vector<std::byte> bytes{};
        std::string sha256{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return !bytes.empty() && sha256.size() == 64u;
        }
    };

    [[nodiscard]] std::string sha256_hex(std::string_view bytes);
    [[nodiscard]] std::vector<std::byte> encode_request(const Request& request);

    class Engine final
    {
    public:
        [[nodiscard]] Result begin(const Configuration& configuration);
        [[nodiscard]] Result propose(
            std::span<const std::byte> canonical_request,
            std::span<const ReviewedSource> reviewed_sources);
        [[nodiscard]] Checkpoint checkpoint() const;
        [[nodiscard]] Result restore(
            std::span<const std::byte> checkpoint_bytes,
            std::uint64_t now_unix_seconds);
        [[nodiscard]] const Snapshot& snapshot() const noexcept;

    private:
        Snapshot snapshot_{};
        bool begun_{};
    };

    [[nodiscard]] bool run_contract();
}
