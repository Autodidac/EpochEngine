/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

module ai.iteration_patch_adapter;

namespace epochengine::ai::iteration_patch_adapter
{
    namespace
    {
        [[nodiscard]] bool lowercase_hex(
            std::string_view value,
            std::size_t size = 64u) noexcept
        {
            if (value.size() != size) return false;
            for (const char character : value)
                if (!((character >= '0' && character <= '9')
                    || (character >= 'a' && character <= 'f'))) return false;
            return true;
        }

        [[nodiscard]] std::string digest_text(std::string_view value)
        {
            return source_patch_bundle::digest_hex(
                source_patch_bundle::digest(value));
        }

        [[nodiscard]] std::string authority_material(
            const self_iteration_orchestrator::Snapshot& snapshot)
        {
            const auto& source = snapshot.campaign.session.source;
            return "EPOCH_ITERATION_PATCH_AUTHORITY_V1\n"
                + std::to_string(static_cast<std::uint8_t>(source.target_kind))
                + "\n" + std::to_string(static_cast<std::uint8_t>(source.kind))
                + "\n" + source.root.generic_string()
                + "\n" + source.source_version + "\n" + source.commit
                + "\n" + source.receipt_digest + "\n" + source.project_id
                + "\n" + source.project_manifest_digest
                + "\n" + source.project_profile_digest
                + "\n" + (source.verified ? "1" : "0");
        }

        [[nodiscard]] bool same_session(
            const iteration_session::RequestIdentity& left,
            const iteration_session::RequestIdentity& right) noexcept
        {
            return left == right && left.valid();
        }

        [[nodiscard]] std::string file_diff(
            const source_patch_bundle::Bundle& bundle,
            std::string_view path)
        {
            const std::string add_marker = "diff --epoch /dev/null b/"
                + std::string{path} + "\n";
            const std::string edit_marker = "diff --epoch a/"
                + std::string{path} + " ";
            std::size_t begin = bundle.canonical_diff.find(add_marker);
            if (begin == bundle.canonical_diff.npos)
                begin = bundle.canonical_diff.find(edit_marker);
            if (begin == bundle.canonical_diff.npos) return {};
            const std::size_t end = bundle.canonical_diff.find(
                "diff --epoch ", begin + 1u);
            return bundle.canonical_diff.substr(begin,
                end == std::string::npos ? end : end - begin);
        }

        [[nodiscard]] std::string review_digest(
            const ReviewArtifact& review)
        {
            std::string material = "EPOCH_ITERATION_PATCH_REVIEW_V1\n"
                + review.orchestrator_id + "\n" + review.campaign_id + "\n"
                + std::to_string(review.campaign_generation) + "\n"
                + std::to_string(review.session_identity.session_id) + "\n"
                + std::to_string(review.session_identity.request_id) + "\n"
                + std::to_string(review.orchestrator_generation) + "\n"
                + review.state_sha256 + "\n" + review.scope_sha256 + "\n"
                + review.proposal_sha256 + "\n" + review.candidate_sha256
                + "\n" + review.authority_sha256 + "\n" + review.bundle_sha256;
            for (const FileReview& file : review.files)
            {
                material += "\n" + file.relative_path + "\n"
                    + source_patch_bundle::digest_hex(file.before.digest) + "\n"
                    + source_patch_bundle::digest_hex(file.after.digest) + "\n"
                    + file.evidence_sha256;
                for (const auto& hunk : file.hunks)
                    material += "\n" + source_patch_bundle::digest_hex(hunk.digest);
            }
            return digest_text(material);
        }

        [[nodiscard]] std::string journal_digest(const JournalRecord& record)
        {
            const ApplyOperation& operation = record.operation;
            return digest_text("EPOCH_ITERATION_PATCH_JOURNAL_V1\n"
                + std::to_string(record.record_schema_version) + "\n"
                + std::to_string(static_cast<std::uint8_t>(record.phase)) + "\n"
                + record.receipt_id + "\n" + record.orchestrator_id + "\n"
                + record.campaign_id + "\n"
                + std::to_string(record.campaign_generation) + "\n"
                + std::to_string(record.session_identity.session_id) + "\n"
                + std::to_string(record.session_identity.request_id) + "\n"
                + record.scope_sha256 + "\n" + record.authority_sha256 + "\n"
                + record.bundle_sha256 + "\n" + record.review_evidence_sha256
                + "\n" + record.implementation_evidence_sha256 + "\n"
                + operation.operation_id + "\n" + operation.orchestrator_id
                + "\n" + operation.campaign_id + "\n"
                + std::to_string(operation.session_identity.session_id) + "\n"
                + std::to_string(operation.session_identity.request_id) + "\n"
                + std::to_string(operation.expected_generation) + "\n"
                + operation.expected_state_sha256 + "\n"
                + operation.transition_id + "\n" + operation.proposal_sha256
                + "\n" + operation.candidate_sha256);
        }

        [[nodiscard]] bool store(JournalPort& port, JournalRecord& record)
        {
            record.record_sha256.clear();
            record.record_sha256 = journal_digest(record);
            return port.store_atomic(record);
        }

        [[nodiscard]] bool valid_journal(const JournalRecord& record)
        {
            return record.record_schema_version == schema_version
                && lowercase_hex(record.record_sha256)
                && journal_digest(JournalRecord{
                    .record_schema_version = record.record_schema_version,
                    .phase = record.phase,
                    .receipt_id = record.receipt_id,
                    .orchestrator_id = record.orchestrator_id,
                    .campaign_id = record.campaign_id,
                    .campaign_generation = record.campaign_generation,
                    .session_identity = record.session_identity,
                    .scope_sha256 = record.scope_sha256,
                    .authority_sha256 = record.authority_sha256,
                    .bundle_sha256 = record.bundle_sha256,
                    .review_evidence_sha256 = record.review_evidence_sha256,
                    .implementation_evidence_sha256 = record.implementation_evidence_sha256,
                    .operation = record.operation}) == record.record_sha256;
        }

        [[nodiscard]] bool operation_matches(
            const ApplyOperation& operation,
            const ApprovedReview& review) noexcept
        {
            return operation.orchestrator_id == review.artifact.orchestrator_id
                && operation.campaign_id == review.artifact.campaign_id
                && same_session(operation.session_identity,
                    review.artifact.session_identity)
                && operation.target_kind == review.artifact.target_kind
                && operation.proposal_sha256 == review.artifact.proposal_sha256
                && operation.candidate_sha256 == review.artifact.candidate_sha256
                && !operation.operation_id.empty()
                && lowercase_hex(operation.expected_state_sha256)
                && !operation.transition_id.empty();
        }

        enum class SandboxImage : std::uint8_t { preimage, postimage, mixed };

        [[nodiscard]] SandboxImage inspect_sandbox(
            const source_patch_bundle::Bundle& bundle,
            source_patch_bundle::SandboxFilePort& sandbox)
        {
            bool all_before = true;
            bool all_after = true;
            for (const auto& file : bundle.files)
            {
                const auto current = sandbox.read_live(file.relative_path);
                const bool before = file.preimage.exists
                    ? current.kind == source_patch_bundle::EntryKind::regular
                        && source_patch_bundle::digest(current.bytes)
                            == file.preimage.digest
                        && current.bytes.size() == file.preimage.byte_count
                    : current.kind == source_patch_bundle::EntryKind::missing;
                const bool after = file.postimage.exists
                    ? current.kind == source_patch_bundle::EntryKind::regular
                        && source_patch_bundle::digest(current.bytes)
                            == file.postimage.digest
                        && current.bytes.size() == file.postimage.byte_count
                    : current.kind == source_patch_bundle::EntryKind::missing;
                all_before = all_before && before;
                all_after = all_after && after;
            }
            if (all_before && !all_after) return SandboxImage::preimage;
            if (all_after && !all_before) return SandboxImage::postimage;
            return SandboxImage::mixed;
        }

        [[nodiscard]] Result failure(Code code, std::string status)
        {
            return {.code = code, .status = std::move(status)};
        }
    }

    ReceiptBinding bind_receipt(
        const mcp_orchestrator_bridge::PendingReceipt& receipt)
    {
        return {.receipt_id = receipt.receipt_id(),
            .kind = receipt.kind(),
            .transport_session_id = receipt.transport_session_id(),
            .connection_id = receipt.connection_id(),
            .connection_generation = receipt.connection_generation(),
            .request_key = receipt.request_key(),
            .call_id = receipt.call_id(),
            .orchestrator_id = receipt.orchestrator_id(),
            .campaign_id = receipt.campaign_id(),
            .campaign_generation = receipt.campaign_generation(),
            .session_identity = receipt.session_identity(),
            .expected_orchestrator_generation =
                receipt.expected_orchestrator_generation(),
            .expected_state_sha256 = receipt.expected_state_sha256(),
            .intent = receipt.intent()};
    }

    std::string authority_sha256(
        const self_iteration_orchestrator::Snapshot& snapshot)
    {
        return digest_text(authority_material(snapshot));
    }

    std::string scope_sha256(
        const self_iteration_orchestrator::Snapshot& snapshot)
    {
        return snapshot.campaign.session.scope_digest;
    }

    DirectHostTransitionPort::DirectHostTransitionPort(
        mcp_orchestrator_bridge::Bridge& bridge,
        self_iteration_orchestrator::Orchestrator& orchestrator) noexcept
        : bridge_(&bridge), orchestrator_(&orchestrator)
    {
    }

    HostApprovalResult DirectHostTransitionPort::approve_apply(
        const ReceiptBinding& receipt,
        const mcp_orchestrator_bridge::HostApproval& approval)
    {
        if (bridge_ == nullptr || orchestrator_ == nullptr
            || receipt.receipt_id != approval.receipt_id)
            return {.status = "Direct host receipt binding is invalid."};
        const auto result = bridge_->approve_receipt(approval, *orchestrator_);
        HostApprovalResult normalized{
            .accepted = static_cast<bool>(result),
            .snapshot = result.snapshot,
            .status = result.status};
        if (result.pending_operation)
        {
            const auto& operation = *result.pending_operation;
            normalized.operation = ApplyOperation{
                .operation_id = operation.operation_id(),
                .orchestrator_id = operation.orchestrator_id(),
                .campaign_id = operation.campaign_id(),
                .session_identity = operation.session_identity(),
                .target_kind = operation.target_kind(),
                .expected_generation = operation.expected_generation(),
                .expected_state_sha256 = operation.expected_state_sha256(),
                .transition_id = operation.transition_id(),
                .proposal_sha256 = operation.proposal_sha256(),
                .candidate_sha256 = operation.candidate_sha256()};
        }
        return normalized;
    }

    HostEvidenceResult DirectHostTransitionPort::record_apply_evidence(
        const ApplyOperation& operation,
        std::string evidence_sha256,
        std::string summary,
        const std::uint64_t now_unix_seconds)
    {
        if (orchestrator_ == nullptr) return {.status = "Orchestrator is unavailable."};
        const auto result = orchestrator_->record_apply({
            .operation_id = operation.operation_id,
            .expected_generation = operation.expected_generation,
            .expected_state_sha256 = operation.expected_state_sha256,
            .transition_id = operation.transition_id,
            .now_unix_seconds = now_unix_seconds},
            std::move(evidence_sha256), std::move(summary));
        return {.accepted = static_cast<bool>(result),
            .snapshot = result.snapshot, .status = result.status};
    }

    Result Adapter::stage_review(
        const source_patch_bundle::Bundle& bundle,
        const self_iteration_orchestrator::Snapshot& snapshot) const
    {
        const std::string bundle_hex = source_patch_bundle::digest_hex(bundle.digest);
        const std::string authority_hex = authority_sha256(snapshot);
        if (snapshot.phase != self_iteration_orchestrator::Phase::awaiting_manual_review
            || !lowercase_hex(snapshot.state_sha256)
            || !lowercase_hex(snapshot.proposal_sha256)
            || !lowercase_hex(snapshot.candidate_sha256)
            || snapshot.proposal_sha256 != bundle_hex
            || snapshot.candidate_sha256 != bundle_hex
            || snapshot.promotion_permitted || snapshot.git_permitted
            || snapshot.release_permitted || snapshot.network_or_server_permitted)
            return failure(Code::stale_state, "Patch does not bind the current review-only orchestrator state.");
        if (source_patch_bundle::digest_hex(bundle.authority_digest) != authority_hex)
            return failure(Code::cross_authority, "Patch authority does not match the campaign source authority.");
        if (!lowercase_hex(scope_sha256(snapshot))
            || !same_session(snapshot.campaign.session.identity,
                snapshot.campaign.session.identity))
            return failure(Code::cross_authority, "Campaign scope or session identity is invalid.");

        ReviewArtifact review{
            .orchestrator_id = snapshot.orchestrator_id,
            .campaign_id = snapshot.campaign.campaign_id,
            .campaign_generation = snapshot.campaign.record_generation,
            .session_identity = snapshot.campaign.session.identity,
            .target_kind = snapshot.campaign.session.source.target_kind,
            .orchestrator_generation = snapshot.generation,
            .state_sha256 = snapshot.state_sha256,
            .scope_sha256 = scope_sha256(snapshot),
            .proposal_sha256 = snapshot.proposal_sha256,
            .candidate_sha256 = snapshot.candidate_sha256,
            .authority_sha256 = authority_hex,
            .bundle_sha256 = bundle_hex,
            .canonical_diff = bundle.canonical_diff};
        for (const auto& file : bundle.files)
        {
            const auto curated = std::find_if(
                snapshot.campaign.session.curated_files.begin(),
                snapshot.campaign.session.curated_files.end(),
                [&](const iteration_session::CuratedFile& value)
                {
                    return value.relative_path == file.relative_path;
                });
            if (curated == snapshot.campaign.session.curated_files.end()
                || curated->sha256
                    != source_patch_bundle::digest_hex(file.preimage.digest)
                || curated->byte_count != file.preimage.byte_count)
                return failure(Code::cross_authority, "Patch file is outside the exact curated scope.");
            FileReview item{
                .relative_path = file.relative_path,
                .operation = file.operation,
                .before = file.preimage,
                .after = file.postimage,
                .unified_diff = file_diff(bundle, file.relative_path),
                .evidence_sha256 = source_patch_bundle::digest_hex(
                    file.evidence_digest)};
            for (const auto& hunk : file.hunks)
                item.hunks.push_back({hunk.old_start, hunk.old_count,
                    hunk.new_start, hunk.new_count, hunk.digest});
            if (item.unified_diff.empty())
                return failure(Code::rejected, "Review diff is incomplete.");
            review.files.push_back(std::move(item));
        }
        review.review_evidence_sha256 = review_digest(review);
        return {.code = Code::ready_for_review,
            .status = "Exact curated patch diff is ready for explicit operator review.",
            .review = std::move(review)};
    }

    Result Adapter::bind_approved_review(
        ReviewArtifact review,
        const self_iteration_orchestrator::Snapshot& snapshot,
        std::string transition_id) const
    {
        if (transition_id.empty() || review.review_evidence_sha256 != review_digest(review)
            || snapshot.phase
                != self_iteration_orchestrator::Phase::awaiting_apply_decision
            || snapshot.orchestrator_id != review.orchestrator_id
            || snapshot.campaign.campaign_id != review.campaign_id
            || !same_session(snapshot.campaign.session.identity,
                review.session_identity)
            || snapshot.campaign.session.scope_digest != review.scope_sha256
            || snapshot.proposal_sha256 != review.proposal_sha256
            || snapshot.candidate_sha256 != review.candidate_sha256
            || snapshot.generation <= review.orchestrator_generation
            || snapshot.state_sha256 == review.state_sha256
            || snapshot.promotion_permitted || snapshot.git_permitted
            || snapshot.release_permitted || snapshot.network_or_server_permitted)
            return failure(Code::stale_state, "Approved review does not bind the next orchestrator state.");
        ApprovedReview approved{
            .artifact = std::move(review),
            .orchestrator_generation = snapshot.generation,
            .state_sha256 = snapshot.state_sha256,
            .review_transition_id = std::move(transition_id)};
        return {.code = Code::review_bound,
            .status = "Reviewed patch is bound to the current apply-decision state.",
            .approved_review = std::move(approved)};
    }

    Result Adapter::execute(
        const ApprovedReview& review,
        const source_patch_bundle::Bundle& bundle,
        const ReceiptBinding& receipt,
        const mcp_orchestrator_bridge::HostApproval& approval,
        HostTransitionPort& host,
        source_patch_bundle::SandboxFilePort& sandbox,
        JournalPort& journal,
        const std::uint64_t now,
        ExecuteOptions options)
    {
        if (review.artifact.review_evidence_sha256 != review_digest(review.artifact)
            || review.artifact.bundle_sha256
                != source_patch_bundle::digest_hex(bundle.digest)
            || review.artifact.authority_sha256
                != source_patch_bundle::digest_hex(bundle.authority_digest))
            return failure(Code::cross_authority, "Bundle changed after operator review.");
        if (receipt.kind != mcp_orchestrator_bridge::MutationKind::decide_apply
            || !receipt.intent.approve
            || receipt.intent.evidence_sha256
                != review.artifact.review_evidence_sha256
            || receipt.orchestrator_id != review.artifact.orchestrator_id
            || receipt.campaign_id != review.artifact.campaign_id
            || receipt.campaign_generation < review.artifact.campaign_generation
            || !same_session(receipt.session_identity,
                review.artifact.session_identity)
            || receipt.expected_orchestrator_generation
                != review.orchestrator_generation
            || receipt.expected_state_sha256 != review.state_sha256)
            return failure(Code::stale_state, "MCP receipt is stale or cross-bound.");
        if (!approval.operator_approved || approval.receipt_id != receipt.receipt_id
            || approval.connection_generation != receipt.connection_generation
            || approval.expected_orchestrator_generation
                != receipt.expected_orchestrator_generation
            || approval.expected_state_sha256 != receipt.expected_state_sha256)
            return failure(Code::approval_required, "One exact current host approval is required.");
        if (std::find(completed_receipts_.begin(), completed_receipts_.end(),
                receipt.receipt_id) != completed_receipts_.end()
            || journal.load(receipt.receipt_id).has_value())
            return failure(Code::replay_rejected, "Patch receipt already has durable state.");

        HostApprovalResult approved = host.approve_apply(receipt, approval);
        if (!approved.accepted || !approved.operation
            || approved.snapshot.phase
                != self_iteration_orchestrator::Phase::awaiting_apply_result
            || !operation_matches(*approved.operation, review))
            return failure(Code::host_failure, "Host did not create the exact sandbox apply operation.");
        JournalRecord record{
            .phase = JournalPhase::approved,
            .receipt_id = receipt.receipt_id,
            .orchestrator_id = review.artifact.orchestrator_id,
            .campaign_id = review.artifact.campaign_id,
            .campaign_generation = approved.snapshot.campaign.record_generation,
            .session_identity = review.artifact.session_identity,
            .scope_sha256 = review.artifact.scope_sha256,
            .authority_sha256 = review.artifact.authority_sha256,
            .bundle_sha256 = review.artifact.bundle_sha256,
            .review_evidence_sha256 = review.artifact.review_evidence_sha256,
            .operation = *approved.operation};
        if (!store(journal, record))
            return failure(Code::journal_failure, "Could not persist approved patch operation.");

        const auto applied = executor_.apply(bundle, {
            .receipt_id = receipt.receipt_id,
            .bundle_digest = bundle.digest,
            .authority_digest = bundle.authority_digest,
            .operator_approved = true}, sandbox);
        if (!applied)
        {
            record.phase = JournalPhase::failed;
            (void)store(journal, record);
            Result result = failure(Code::sandbox_failure, applied.status);
            result.apply_result = applied;
            return result;
        }
        record.phase = JournalPhase::committed;
        record.implementation_evidence_sha256 =
            source_patch_bundle::digest_hex(applied.evidence_digest);
        if (!store(journal, record))
            return failure(Code::journal_failure,
                "Sandbox committed; durable evidence marker needs recovery.");
        if (options.defer_evidence_recording)
            return {.code = Code::evidence_pending,
                .status = "Sandbox committed atomically; orchestrator evidence is pending recovery.",
                .apply_result = applied,
                .snapshot = approved.snapshot};

        HostEvidenceResult evidence = host.record_apply_evidence(
            record.operation, record.implementation_evidence_sha256,
            "Verified exact source patch postimages in the disposable sandbox.", now);
        if (!evidence.accepted || evidence.snapshot.phase
            != self_iteration_orchestrator::Phase::awaiting_validation_request)
            return failure(Code::host_failure,
                "Committed sandbox evidence was not accepted by the orchestrator.");
        record.phase = JournalPhase::evidence_recorded;
        if (!store(journal, record))
            return failure(Code::journal_failure,
                "Orchestrator accepted evidence but final journal marker failed.");
        completed_receipts_.push_back(receipt.receipt_id);
        return {.code = Code::applied,
            .status = "One approved patch receipt committed and recorded verified postimage evidence.",
            .apply_result = applied, .snapshot = evidence.snapshot};
    }

    Result Adapter::recover(
        const ApprovedReview& review,
        const source_patch_bundle::Bundle& bundle,
        std::string receipt_id,
        const self_iteration_orchestrator::Snapshot& snapshot,
        HostTransitionPort& host,
        source_patch_bundle::SandboxFilePort& sandbox,
        JournalPort& journal,
        const std::uint64_t now)
    {
        const auto loaded = journal.load(receipt_id);
        if (!loaded || !valid_journal(*loaded))
            return failure(Code::journal_failure, "Recovery journal is missing or corrupt.");
        JournalRecord record = *loaded;
        if (record.receipt_id != receipt_id
            || record.orchestrator_id != review.artifact.orchestrator_id
            || record.campaign_id != review.artifact.campaign_id
            || !same_session(record.session_identity, review.artifact.session_identity)
            || record.scope_sha256 != review.artifact.scope_sha256
            || record.authority_sha256 != review.artifact.authority_sha256
            || record.bundle_sha256 != review.artifact.bundle_sha256
            || record.review_evidence_sha256
                != review.artifact.review_evidence_sha256
            || record.bundle_sha256
                != source_patch_bundle::digest_hex(bundle.digest))
            return failure(Code::cross_authority, "Recovery record crosses reviewed authority.");
        if (record.phase == JournalPhase::failed)
            return failure(Code::replay_rejected, "Failed patch receipt requires a new approval.");
        if (record.phase == JournalPhase::evidence_recorded)
        {
            if (snapshot.orchestrator_id != record.orchestrator_id
                || snapshot.campaign.campaign_id != record.campaign_id
                || snapshot.phase
                    != self_iteration_orchestrator::Phase::awaiting_validation_request)
                return failure(Code::stale_state, "Recorded evidence does not match resumed state.");
            completed_receipts_.push_back(receipt_id);
            return {.code = Code::recovered,
                .status = "Previously recorded patch evidence restored without replay.",
                .snapshot = snapshot};
        }

        const SandboxImage image = inspect_sandbox(bundle, sandbox);
        if (record.phase == JournalPhase::approved
            && image == SandboxImage::preimage)
        {
            sandbox.rollback();
            record.phase = JournalPhase::failed;
            if (!store(journal, record))
                return failure(Code::journal_failure, "Interrupted staging rollback could not be journaled.");
            return {.code = Code::recovered,
                .status = "Interrupted uncommitted staging was rolled back; new approval is required."};
        }
        if (image == SandboxImage::mixed)
            return failure(Code::partial_result,
                "Sandbox is neither the complete preimage nor complete postimage set.");
        if (image != SandboxImage::postimage)
            return failure(Code::stale_state, "Committed journal does not match sandbox postimages.");
        if (record.implementation_evidence_sha256.empty())
        {
            std::string material = "EPOCH_ITERATION_PATCH_RECOVERED_EVIDENCE_V1\n"
                + record.receipt_id + "\n" + record.bundle_sha256;
            for (const auto& file : bundle.files)
                material += "\n" + file.relative_path + "\n"
                    + source_patch_bundle::digest_hex(file.postimage.digest);
            record.implementation_evidence_sha256 = digest_text(material);
        }
        record.phase = JournalPhase::committed;
        if (!store(journal, record))
            return failure(Code::journal_failure, "Recovered commit marker could not be persisted.");
        if (snapshot.phase != self_iteration_orchestrator::Phase::awaiting_apply_result
            || snapshot.orchestrator_id != record.orchestrator_id
            || snapshot.campaign.campaign_id != record.campaign_id
            || snapshot.generation != record.operation.expected_generation
            || snapshot.state_sha256 != record.operation.expected_state_sha256)
            return failure(Code::stale_state, "Pending apply operation changed before evidence recovery.");
        HostEvidenceResult evidence = host.record_apply_evidence(
            record.operation, record.implementation_evidence_sha256,
            "Recovered atomic sandbox postimages after process interruption.", now);
        if (!evidence.accepted || evidence.snapshot.phase
            != self_iteration_orchestrator::Phase::awaiting_validation_request)
            return failure(Code::host_failure, "Recovered evidence was rejected by host state.");
        record.phase = JournalPhase::evidence_recorded;
        if (!store(journal, record))
            return failure(Code::journal_failure, "Recovered evidence final marker failed.");
        completed_receipts_.push_back(receipt_id);
        return {.code = Code::recovered,
            .status = "Atomic postimages and orchestrator evidence recovered without patch replay.",
            .snapshot = evidence.snapshot};
    }
}
