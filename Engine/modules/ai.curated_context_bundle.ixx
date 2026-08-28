/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module ai.curated_context_bundle;

export import ai.project_profile;

export namespace epochengine::ai::curated_context_bundle
{
    inline constexpr std::uint32_t format_version{1u};

    enum class Audience : std::uint8_t
    {
        local_model,
        outbound_mcp
    };

    enum class Code : std::uint8_t
    {
        ready,
        invalid_binding,
        provider_unresolved,
        path_rejected,
        stale_revision,
        binary_rejected,
        duplicate_conflict,
        overlapping_range,
        budget_exceeded,
        replay_rejected,
        cross_project_checkpoint,
        stale_generation,
        checkpoint_malformed,
        checkpoint_integrity,
        checkpoint_noncanonical
    };

    [[nodiscard]] constexpr std::string_view code_name(Code code) noexcept
    {
        switch (code)
        {
        case Code::ready: return "ready";
        case Code::invalid_binding: return "invalid_binding";
        case Code::provider_unresolved: return "provider_unresolved";
        case Code::path_rejected: return "path_rejected";
        case Code::stale_revision: return "stale_revision";
        case Code::binary_rejected: return "binary_rejected";
        case Code::duplicate_conflict: return "duplicate_conflict";
        case Code::overlapping_range: return "overlapping_range";
        case Code::budget_exceeded: return "budget_exceeded";
        case Code::replay_rejected: return "replay_rejected";
        case Code::cross_project_checkpoint:
            return "cross_project_checkpoint";
        case Code::stale_generation: return "stale_generation";
        case Code::checkpoint_malformed: return "checkpoint_malformed";
        case Code::checkpoint_integrity: return "checkpoint_integrity";
        case Code::checkpoint_noncanonical: return "checkpoint_noncanonical";
        }
        return "unknown";
    }

    struct Provenance final
    {
        std::string review_id{};
        std::string reviewer_binding{};
        std::string selection_sha256{};
        std::uint64_t reviewed_at_unix_seconds{};

        friend bool operator==(const Provenance&, const Provenance&) = default;
    };

    struct ReviewedEntry final
    {
        std::string project_relative_path{};
        std::string declared_symbol{};
        std::uint32_t first_line{};
        std::uint32_t last_line{};
        std::uint64_t source_revision{};
        std::string exact_bytes{};
        Provenance provenance{};

        friend bool operator==(const ReviewedEntry&, const ReviewedEntry&) = default;
    };

    struct Limits final
    {
        std::uint32_t maximum_entries{32u};
        std::uint64_t maximum_entry_bytes{128u * 1024u};
        std::uint64_t maximum_total_bytes{512u * 1024u};
        std::uint64_t maximum_chunk_bytes{64u * 1024u};
        std::uint32_t maximum_chunks{16u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_entries > 0u && maximum_entries <= 256u
                && maximum_entry_bytes >= 256u
                && maximum_entry_bytes <= 1024u * 1024u
                && maximum_total_bytes >= maximum_entry_bytes
                && maximum_total_bytes <= 8u * 1024u * 1024u
                && maximum_chunk_bytes >= 1024u
                && maximum_chunk_bytes <= maximum_total_bytes
                && maximum_chunks > 0u && maximum_chunks <= 256u;
        }

        friend constexpr bool operator==(const Limits&, const Limits&) = default;
    };

    struct Binding final
    {
        std::string project_id{};
        std::filesystem::path reviewed_root{};
        std::string project_manifest_sha256{};
        std::string project_profile_sha256{};
        std::uint64_t reviewed_revision{};
        Audience audience{Audience::local_model};
        project_profile::Provider provider{
            project_profile::Provider::disabled};
        std::string model_binding{};
        std::string endpoint_binding{};
        std::uint64_t session_id{};
        std::uint64_t request_id{};
        std::string campaign_id{};
        std::uint64_t campaign_generation{};
        bool operator_shared{};

        friend bool operator==(const Binding&, const Binding&) = default;
    };

    struct Request final
    {
        Binding binding{};
        Limits limits{};
        std::vector<ReviewedEntry> reviewed_entries{};
        std::uint64_t expected_generation{};
    };

    struct EntryEvidence final
    {
        std::string project_relative_path{};
        std::string declared_symbol{};
        std::uint32_t first_line{};
        std::uint32_t last_line{};
        std::uint64_t source_revision{};
        std::uint64_t byte_count{};
        std::string content_sha256{};
        std::string entry_sha256{};
        Provenance provenance{};

        friend bool operator==(const EntryEvidence&, const EntryEvidence&) = default;
    };

    struct Chunk final
    {
        std::uint32_t index{};
        std::uint64_t offset{};
        std::vector<std::uint8_t> bytes{};
        std::string sha256{};

        friend bool operator==(const Chunk&, const Chunk&) = default;
    };

    struct ResumeState final
    {
        std::string project_id{};
        std::string reviewed_root{};
        std::string campaign_id{};
        std::uint64_t session_id{};
        std::uint64_t request_id{};
        std::uint64_t generation{};
        std::string request_sha256{};
        std::string bundle_sha256{};

        friend bool operator==(const ResumeState&, const ResumeState&) = default;
    };

    struct Bundle final
    {
        Binding binding{};
        Limits limits{};
        std::vector<ReviewedEntry> entries{};
        std::vector<EntryEvidence> evidence{};
        std::vector<Chunk> chunks{};
        std::vector<std::uint8_t> canonical_bytes{};
        std::string bundle_sha256{};
        std::string request_sha256{};
        std::string evidence_summary{};
        ResumeState resume{};
    };

    struct RefusalReceipt final
    {
        Code code{Code::invalid_binding};
        std::string project_id{};
        std::string campaign_id{};
        std::uint64_t session_id{};
        std::uint64_t request_id{};
        std::uint64_t expected_generation{};
        std::string request_sha256{};
        std::string receipt_sha256{};
        std::string status{};
    };

    struct Result final
    {
        Code code{Code::invalid_binding};
        Bundle bundle{};
        RefusalReceipt refusal{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready;
        }
    };

    struct CheckpointRecord final
    {
        Code code{Code::checkpoint_malformed};
        std::vector<std::uint8_t> canonical_bytes{};
        std::string sha256{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready;
        }
    };

    struct RestoreExpectation final
    {
        std::string project_id{};
        std::filesystem::path reviewed_root{};
        std::string campaign_id{};
        std::uint64_t session_id{};
        std::uint64_t current_generation{};
        std::string checkpoint_sha256{};
    };

    struct RestoreResult final
    {
        Code code{Code::checkpoint_malformed};
        Bundle bundle{};
        std::string canonical_sha256{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready;
        }
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        local_roundtrip,
        external_roundtrip,
        deterministic_order,
        duplicate_deduplication,
        duplicate_conflict,
        overlap,
        traversal,
        absolute_path,
        cross_project_path,
        stale_revision,
        entry_budget,
        total_budget,
        binary_data,
        provider_binding,
        replay,
        checkpoint_roundtrip,
        checkpoint_tamper,
        checkpoint_truncation,
        checkpoint_trailing,
        checkpoint_cross_project,
        checkpoint_stale,
        checkpoint_noncanonical
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::local_roundtrip: return "local_roundtrip";
        case ContractFailure::external_roundtrip: return "external_roundtrip";
        case ContractFailure::deterministic_order: return "deterministic_order";
        case ContractFailure::duplicate_deduplication:
            return "duplicate_deduplication";
        case ContractFailure::duplicate_conflict: return "duplicate_conflict";
        case ContractFailure::overlap: return "overlap";
        case ContractFailure::traversal: return "traversal";
        case ContractFailure::absolute_path: return "absolute_path";
        case ContractFailure::cross_project_path: return "cross_project_path";
        case ContractFailure::stale_revision: return "stale_revision";
        case ContractFailure::entry_budget: return "entry_budget";
        case ContractFailure::total_budget: return "total_budget";
        case ContractFailure::binary_data: return "binary_data";
        case ContractFailure::provider_binding: return "provider_binding";
        case ContractFailure::replay: return "replay";
        case ContractFailure::checkpoint_roundtrip:
            return "checkpoint_roundtrip";
        case ContractFailure::checkpoint_tamper: return "checkpoint_tamper";
        case ContractFailure::checkpoint_truncation:
            return "checkpoint_truncation";
        case ContractFailure::checkpoint_trailing:
            return "checkpoint_trailing";
        case ContractFailure::checkpoint_cross_project:
            return "checkpoint_cross_project";
        case ContractFailure::checkpoint_stale: return "checkpoint_stale";
        case ContractFailure::checkpoint_noncanonical:
            return "checkpoint_noncanonical";
        }
        return "unknown";
    }

    [[nodiscard]] Result build(
        const Request& request,
        const ResumeState* prior = nullptr) noexcept;
    [[nodiscard]] CheckpointRecord checkpoint(const Bundle& bundle) noexcept;
    [[nodiscard]] RestoreResult restore(
        std::span<const std::uint8_t> bytes,
        const RestoreExpectation& expectation) noexcept;
    [[nodiscard]] ContractFailure run_contract() noexcept;
}
