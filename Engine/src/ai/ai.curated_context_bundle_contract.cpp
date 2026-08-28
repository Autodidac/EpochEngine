/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.curated_context_bundle;
import core.sha256;

namespace epochengine::ai::curated_context_bundle
{
    namespace
    {
        [[nodiscard]] std::string digest(std::string_view value)
        {
            return core::sha256::hex(core::sha256::hash(value));
        }

        [[nodiscard]] std::string selection_digest(const ReviewedEntry& entry)
        {
            return digest(entry.project_relative_path + "\n"
                + entry.declared_symbol + "\n"
                + std::to_string(entry.first_line) + "\n"
                + std::to_string(entry.last_line) + "\n"
                + std::to_string(entry.source_revision) + "\n"
                + digest(entry.exact_bytes));
        }

        [[nodiscard]] ReviewedEntry entry(
            std::string path,
            std::string symbol,
            std::uint32_t first,
            std::uint32_t last,
            std::string bytes,
            std::uint64_t revision = 41u)
        {
            ReviewedEntry value{
                .project_relative_path = std::move(path),
                .declared_symbol = std::move(symbol),
                .first_line = first,
                .last_line = last,
                .source_revision = revision,
                .exact_bytes = std::move(bytes),
                .provenance = {
                    .review_id = "review-41",
                    .reviewer_binding = "editor-reviewed-source",
                    .reviewed_at_unix_seconds = 1'800'000'000u}};
            value.provenance.selection_sha256 = selection_digest(value);
            return value;
        }

        [[nodiscard]] Request request()
        {
#if defined(_WIN32)
            const std::filesystem::path root{"C:/epoch/contracts/project"};
#else
            const std::filesystem::path root{"/epoch/contracts/project"};
#endif
            return {
                .binding = {
                    .project_id = "curated-contract",
                    .reviewed_root = root,
                    .project_manifest_sha256 = std::string(64u, 'a'),
                    .project_profile_sha256 = std::string(64u, 'b'),
                    .reviewed_revision = 41u,
                    .audience = Audience::local_model,
                    .provider =
                        project_profile::Provider::epoch_local_qwen38,
                    .model_binding = "qwen3.8-project-review",
                    .endpoint_binding = "epoch-local-runtime",
                    .session_id = 17u,
                    .request_id = 29u,
                    .campaign_id = "campaign-9",
                    .campaign_generation = 3u,
                    .operator_shared = true},
                .reviewed_entries = {
                    entry("Scripts/zeta.cpp", "zeta", 20u, 21u,
                        "int zeta = 1;\nreturn zeta;"),
                    entry("Scripts/alpha.cpp", "alpha", 2u, 3u,
                        "int alpha = 2;\nreturn alpha;")}};
        }

        [[nodiscard]] std::string bytes_sha(
            std::span<const std::uint8_t> bytes)
        {
            return core::sha256::hex(core::sha256::hash(bytes));
        }

        [[nodiscard]] std::vector<std::uint8_t> reseal(
            std::vector<std::uint8_t> payload)
        {
            const auto seal = core::sha256::hash(payload);
            payload.insert(payload.end(), seal.bytes.begin(), seal.bytes.end());
            return payload;
        }

        [[nodiscard]] std::uint32_t read_u32(
            const std::vector<std::uint8_t>& bytes,
            std::size_t offset)
        {
            return std::uint32_t(bytes[offset])
                | std::uint32_t(bytes[offset + 1u]) << 8u
                | std::uint32_t(bytes[offset + 2u]) << 16u
                | std::uint32_t(bytes[offset + 3u]) << 24u;
        }

        void write_u32(
            std::vector<std::uint8_t>& bytes,
            std::size_t offset,
            std::uint32_t value)
        {
            for (std::size_t index = 0u; index < 4u; ++index)
                bytes[offset + index] =
                    static_cast<std::uint8_t>(value >> (index * 8u));
        }
    }

    ContractFailure run_contract() noexcept
    {
        Request localRequest = request();
        const Result local = build(localRequest);
        if (!local || local.bundle.entries.size() != 2u
            || local.bundle.entries.front().project_relative_path
                != "Scripts/alpha.cpp"
            || local.bundle.chunks.empty()
            || local.bundle.evidence.size() != 2u
            || local.bundle.evidence_summary.find("No path was read")
                == std::string::npos)
            return ContractFailure::local_roundtrip;

        Request externalRequest = localRequest;
        externalRequest.binding.audience = Audience::outbound_mcp;
        externalRequest.binding.provider =
            project_profile::Provider::external_mcp;
        externalRequest.binding.model_binding = "operator-mcp-model";
        externalRequest.binding.endpoint_binding =
            "https://operator.example/mcp";
        ++externalRequest.binding.request_id;
        const Result external = build(externalRequest);
        if (!external
            || external.bundle.binding.audience != Audience::outbound_mcp
            || external.bundle.binding.provider
                != project_profile::Provider::external_mcp)
            return ContractFailure::external_roundtrip;

        Request orderedRequest = localRequest;
        std::reverse(
            orderedRequest.reviewed_entries.begin(),
            orderedRequest.reviewed_entries.end());
        if (build(orderedRequest).bundle.canonical_bytes
            != local.bundle.canonical_bytes)
            return ContractFailure::deterministic_order;

        Request duplicate = localRequest;
        duplicate.reviewed_entries.push_back(
            duplicate.reviewed_entries.front());
        const Result deduplicated = build(duplicate);
        if (!deduplicated || deduplicated.bundle.entries.size() != 2u)
            return ContractFailure::duplicate_deduplication;

        Request conflict = localRequest;
        ReviewedEntry conflicting = conflict.reviewed_entries.front();
        conflicting.exact_bytes = "int zeta = 3;\nreturn zeta;";
        conflicting.provenance.selection_sha256 =
            selection_digest(conflicting);
        conflict.reviewed_entries.push_back(std::move(conflicting));
        if (build(conflict).code != Code::duplicate_conflict)
            return ContractFailure::duplicate_conflict;

        Request overlapRequest = localRequest;
        overlapRequest.reviewed_entries = {
            entry("Scripts/ranges.cpp", "first", 1u, 2u, "a\nb"),
            entry("Scripts/ranges.cpp", "second", 2u, 3u, "b\nc")};
        if (build(overlapRequest).code != Code::overlapping_range)
            return ContractFailure::overlap;

        Request traversal = localRequest;
        traversal.reviewed_entries.front().project_relative_path =
            "../Engine/private.cpp";
        traversal.reviewed_entries.front().provenance.selection_sha256 =
            selection_digest(traversal.reviewed_entries.front());
        const Result traversalResult = build(traversal);
        if (traversalResult.code != Code::path_rejected
            || traversalResult.refusal.receipt_sha256.size() != 64u
            || build(traversal).refusal.receipt_sha256
                != traversalResult.refusal.receipt_sha256)
            return ContractFailure::traversal;

        Request absolute = localRequest;
        absolute.reviewed_entries.front().project_relative_path =
            "C:/outside.cpp";
        absolute.reviewed_entries.front().provenance.selection_sha256 =
            selection_digest(absolute.reviewed_entries.front());
        if (build(absolute).code != Code::path_rejected)
            return ContractFailure::absolute_path;

        Request crossProject = localRequest;
        crossProject.reviewed_entries.front().project_relative_path =
            "Projects/Other/Scripts/file.cpp";
        crossProject.reviewed_entries.front().provenance.selection_sha256 =
            selection_digest(crossProject.reviewed_entries.front());
        if (build(crossProject).code != Code::path_rejected)
            return ContractFailure::cross_project_path;

        Request stale = localRequest;
        stale.reviewed_entries.front().source_revision = 40u;
        stale.reviewed_entries.front().provenance.selection_sha256 =
            selection_digest(stale.reviewed_entries.front());
        if (build(stale).code != Code::stale_revision)
            return ContractFailure::stale_revision;

        Request entryBudget = localRequest;
        entryBudget.limits.maximum_entry_bytes = 256u;
        entryBudget.reviewed_entries = {
            entry("Scripts/large.cpp", "large", 1u, 1u,
                std::string(257u, 'x'))};
        if (build(entryBudget).code != Code::budget_exceeded)
            return ContractFailure::entry_budget;

        Request totalBudget = localRequest;
        totalBudget.limits.maximum_entry_bytes = 700u;
        totalBudget.limits.maximum_total_bytes = 1024u;
        totalBudget.limits.maximum_chunk_bytes = 1024u;
        totalBudget.reviewed_entries = {
            entry("Scripts/a.cpp", "a", 1u, 1u, std::string(600u, 'a')),
            entry("Scripts/b.cpp", "b", 1u, 1u, std::string(600u, 'b'))};
        if (build(totalBudget).code != Code::budget_exceeded)
            return ContractFailure::total_budget;

        Request binary = localRequest;
        binary.reviewed_entries.front().exact_bytes =
            std::string{"text\0binary", 11u};
        binary.reviewed_entries.front().provenance.selection_sha256 =
            selection_digest(binary.reviewed_entries.front());
        if (build(binary).code != Code::binary_rejected)
            return ContractFailure::binary_data;
        binary = localRequest;
        binary.reviewed_entries.front().exact_bytes =
            std::string{"\xc3\x28", 2u};
        binary.reviewed_entries.front().provenance.selection_sha256 =
            selection_digest(binary.reviewed_entries.front());
        if (build(binary).code != Code::binary_rejected)
            return ContractFailure::binary_data;

        Request provider = localRequest;
        provider.binding.provider =
            project_profile::Provider::external_mcp;
        if (build(provider).code != Code::provider_unresolved)
            return ContractFailure::provider_binding;

        Request replay = localRequest;
        replay.expected_generation = local.bundle.resume.generation;
        if (build(replay, &local.bundle.resume).code
            != Code::replay_rejected)
            return ContractFailure::replay;

        const CheckpointRecord record = checkpoint(local.bundle);
        RestoreExpectation expected{
            .project_id = local.bundle.binding.project_id,
            .reviewed_root = local.bundle.binding.reviewed_root,
            .campaign_id = local.bundle.binding.campaign_id,
            .session_id = local.bundle.binding.session_id,
            .current_generation = local.bundle.resume.generation,
            .checkpoint_sha256 = record.sha256};
        const RestoreResult restored =
            restore(record.canonical_bytes, expected);
        if (!record || !restored
            || restored.bundle.canonical_bytes
                != local.bundle.canonical_bytes
            || restored.bundle.resume != local.bundle.resume)
            return ContractFailure::checkpoint_roundtrip;

        auto tampered = record.canonical_bytes;
        tampered[20u] ^= 1u;
        if (restore(tampered, expected).code != Code::checkpoint_integrity)
            return ContractFailure::checkpoint_tamper;

        std::vector<std::uint8_t> truncated(
            record.canonical_bytes.begin(),
            record.canonical_bytes.end() - 32u);
        truncated.pop_back();
        truncated = reseal(std::move(truncated));
        auto truncatedExpected = expected;
        truncatedExpected.checkpoint_sha256 = bytes_sha(truncated);
        if (restore(truncated, truncatedExpected).code
            != Code::checkpoint_malformed)
            return ContractFailure::checkpoint_truncation;

        std::vector<std::uint8_t> trailing(
            record.canonical_bytes.begin(),
            record.canonical_bytes.end() - 32u);
        trailing.push_back(0u);
        trailing = reseal(std::move(trailing));
        auto trailingExpected = expected;
        trailingExpected.checkpoint_sha256 = bytes_sha(trailing);
        if (restore(trailing, trailingExpected).code
            != Code::checkpoint_malformed)
            return ContractFailure::checkpoint_trailing;

        auto otherExpected = expected;
        otherExpected.project_id = "other-project";
        if (restore(record.canonical_bytes, otherExpected).code
            != Code::cross_project_checkpoint)
            return ContractFailure::checkpoint_cross_project;
        auto staleExpected = expected;
        ++staleExpected.current_generation;
        if (restore(record.canonical_bytes, staleExpected).code
            != Code::stale_generation)
            return ContractFailure::checkpoint_stale;

        std::vector<std::uint8_t> noncanonical(
            record.canonical_bytes.begin(),
            record.canonical_bytes.end() - 32u);
        const std::size_t projectLengthOffset = 20u;
        const std::uint32_t projectLength =
            read_u32(noncanonical, projectLengthOffset);
        const std::size_t rootLengthOffset =
            projectLengthOffset + 4u + projectLength;
        const std::uint32_t rootLength =
            read_u32(noncanonical, rootLengthOffset);
        const std::size_t rootEnd =
            rootLengthOffset + 4u + rootLength;
        const std::string suffix{"/segment/.."};
        noncanonical.insert(
            noncanonical.begin() + rootEnd,
            suffix.begin(), suffix.end());
        write_u32(noncanonical, rootLengthOffset,
            rootLength + static_cast<std::uint32_t>(suffix.size()));
        noncanonical = reseal(std::move(noncanonical));
        auto noncanonicalExpected = expected;
        noncanonicalExpected.checkpoint_sha256 =
            bytes_sha(noncanonical);
        if (restore(noncanonical, noncanonicalExpected).code
            != Code::checkpoint_noncanonical)
            return ContractFailure::checkpoint_noncanonical;

        return ContractFailure::none;
    }
}

#if defined(EPOCH_AI_CURATED_CONTEXT_BUNDLE_CONTRACT_MAIN)
int main()
{
    const auto failure =
        epochengine::ai::curated_context_bundle::run_contract();
    return failure
            == epochengine::ai::curated_context_bundle::ContractFailure::none
        ? 0 : static_cast<int>(failure);
}
#endif
