/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

module ai.iteration_patch_adapter;

namespace epochengine::ai::iteration_patch_adapter
{
    namespace
    {
        [[nodiscard]] source_patch_bundle::Digest from_hex(std::string_view value)
        {
            source_patch_bundle::Digest result{};
            auto nibble = [](char value) -> std::uint8_t
            {
                return value <= '9' ? static_cast<std::uint8_t>(value - '0')
                    : static_cast<std::uint8_t>(value - 'a' + 10);
            };
            for (std::size_t index = 0u; index < result.bytes.size(); ++index)
                result.bytes[index] = static_cast<std::uint8_t>(
                    (nibble(value[index * 2u]) << 4u)
                    | nibble(value[index * 2u + 1u]));
            return result;
        }

        [[nodiscard]] source_patch_bundle::CuratedFile curated_file()
        {
            const std::string bytes = "first\nsecond\n";
            return {.relative_path = "Source/main.cpp",
                .preimage = {.exists = true,
                    .digest = source_patch_bundle::digest(bytes),
                    .byte_count = bytes.size(),
                    .encoding = source_patch_bundle::Encoding::utf8,
                    .line_ending = source_patch_bundle::LineEnding::lf,
                    .final_newline = true},
                .preimage_bytes = bytes};
        }

        [[nodiscard]] self_iteration_orchestrator::Snapshot base_snapshot()
        {
            using namespace self_iteration_orchestrator;
            Snapshot snapshot{
                .generation = 10u,
                .state_sha256 = std::string(64u, '1'),
                .orchestrator_id = "orchestrator-1",
                .target_key = "project:adapter-contract",
                .phase = Phase::awaiting_manual_review,
                .campaign = {
                    .record_generation = 5u,
                    .state_digest = std::string(64u, '2'),
                    .campaign_id = "campaign-1",
                    .target_key = "project:adapter-contract",
                    .session = {
                        .identity = {41u, 9u},
                        .phase = iteration_session::SessionPhase::awaiting_candidate_approval,
                        .source = {
                            .target_kind = iteration_session::IterationTargetKind::project_source,
                            .kind = iteration_session::SourceAuthorityKind::verified_project,
                            .root = "ProjectAuthority",
                            .project_id = "adapter-contract",
                            .project_manifest_digest = std::string(64u, '3'),
                            .project_profile_digest = std::string(64u, '4'),
                            .verified = true},
                        .scope_digest = std::string(64u, '5')}},
                .manual_approval_required = true,
                .live_source_read_only = true};
            const auto file = curated_file();
            snapshot.campaign.session.curated_files.push_back({
                .relative_path = file.relative_path,
                .sha256 = source_patch_bundle::digest_hex(file.preimage.digest),
                .byte_count = file.preimage.byte_count});
            return snapshot;
        }

        [[nodiscard]] std::string patch_text(
            const self_iteration_orchestrator::Snapshot& snapshot,
            const source_patch_bundle::CuratedFile& file,
            std::string_view id = "adapter-bundle")
        {
            return "EPOCH_SOURCE_PATCH_BUNDLE_V1\nbundle-id "
                + std::string{id} + "\nauthority-sha256 "
                + authority_sha256(snapshot)
                + "\nfile-count 1\ndiff --epoch a/" + file.relative_path
                + " b/" + file.relative_path
                + "\noperation update\npreimage-sha256 "
                + source_patch_bundle::digest_hex(file.preimage.digest)
                + "\npreimage-bytes " + std::to_string(file.preimage.byte_count)
                + "\npreimage-encoding utf-8\npreimage-line-ending lf"
                  "\npreimage-final-newline 1\npostimage-line-ending lf"
                  "\npostimage-final-newline 1\n--- a/"
                + file.relative_path + "\n+++ b/" + file.relative_path
                + "\n@@ -1,2 +1,2 @@\n first\n-second\n+third\n";
        }

        struct FakeSandbox final : source_patch_bundle::SandboxFilePort
        {
            [[nodiscard]] source_patch_bundle::PortRead read_live(
                std::string_view path) override { return read(live, path); }
            [[nodiscard]] bool begin(std::string_view,
                source_patch_bundle::Digest) override
            {
                staged = live; active = true; return true;
            }
            [[nodiscard]] bool stage_write(
                std::string_view path, std::string_view bytes) override
            {
                if (!active || fail_stage) return false;
                staged[std::string{path}] = std::string{bytes}; return true;
            }
            [[nodiscard]] bool stage_remove(std::string_view path) override
            {
                if (!active || fail_stage) return false;
                staged.erase(std::string{path}); return true;
            }
            [[nodiscard]] source_patch_bundle::PortRead read_staged(
                std::string_view path) override { return read(staged, path); }
            [[nodiscard]] bool commit(source_patch_bundle::Digest) override
            {
                if (!active || fail_commit) return false;
                live = staged; active = false; return true;
            }
            void rollback() noexcept override
            {
                staged.clear(); active = false; ++rollbacks;
            }
            static source_patch_bundle::PortRead read(
                const std::map<std::string, std::string>& map,
                std::string_view path)
            {
                const auto found = map.find(std::string{path});
                return found == map.end()
                    ? source_patch_bundle::PortRead{
                        source_patch_bundle::EntryKind::missing, {}}
                    : source_patch_bundle::PortRead{
                        source_patch_bundle::EntryKind::regular, found->second};
            }
            std::map<std::string, std::string> live{};
            std::map<std::string, std::string> staged{};
            bool active{};
            bool fail_stage{};
            bool fail_commit{};
            std::size_t rollbacks{};
        };

        struct FakeJournal final : JournalPort
        {
            [[nodiscard]] std::optional<JournalRecord> load(
                std::string_view receipt) override
            {
                const auto found = records.find(std::string{receipt});
                return found == records.end() ? std::nullopt
                    : std::optional<JournalRecord>{found->second};
            }
            [[nodiscard]] bool store_atomic(const JournalRecord& record) override
            {
                if (fail_store) return false;
                records[record.receipt_id] = record;
                ++writes;
                return true;
            }
            std::map<std::string, JournalRecord> records{};
            std::size_t writes{};
            bool fail_store{};
        };

        struct FakeHost final : HostTransitionPort
        {
            [[nodiscard]] HostApprovalResult approve_apply(
                const ReceiptBinding& receipt,
                const mcp_orchestrator_bridge::HostApproval&) override
            {
                ++approvals;
                if (fail_approval) return {.status = "rejected"};
                pending = reviewed;
                pending.phase = self_iteration_orchestrator::Phase::awaiting_apply_result;
                pending.generation = reviewed.generation + 1u;
                pending.state_sha256 = std::string(64u, '7');
                ApplyOperation operation{
                    .operation_id = "sandbox-operation-1",
                    .orchestrator_id = receipt.orchestrator_id,
                    .campaign_id = receipt.campaign_id,
                    .session_identity = receipt.session_identity,
                    .target_kind = reviewed.campaign.session.source.target_kind,
                    .expected_generation = pending.generation,
                    .expected_state_sha256 = pending.state_sha256,
                    .transition_id = "apply-transition-1",
                    .proposal_sha256 = reviewed.proposal_sha256,
                    .candidate_sha256 = reviewed.candidate_sha256};
                return {.accepted = true, .snapshot = pending,
                    .operation = std::move(operation), .status = "approved"};
            }
            [[nodiscard]] HostEvidenceResult record_apply_evidence(
                const ApplyOperation& operation,
                std::string evidence,
                std::string,
                std::uint64_t) override
            {
                ++evidence_calls;
                if (fail_evidence || operation.expected_generation != pending.generation
                    || evidence.size() != 64u) return {.status = "stale"};
                auto next = pending;
                next.phase = self_iteration_orchestrator::Phase::awaiting_validation_request;
                next.generation += 1u;
                next.state_sha256 = std::string(64u, '8');
                return {.accepted = true, .snapshot = std::move(next),
                    .status = "recorded"};
            }
            self_iteration_orchestrator::Snapshot reviewed{};
            self_iteration_orchestrator::Snapshot pending{};
            std::size_t approvals{};
            std::size_t evidence_calls{};
            bool fail_approval{};
            bool fail_evidence{};
        };

        struct Fixture final
        {
            Fixture()
            {
                const auto decoded = source_patch_bundle::decode(
                    patch_text(snapshot, curated), std::span{&curated, 1u});
                bundle = decoded.bundle;
                const std::string bundle_hex =
                    source_patch_bundle::digest_hex(bundle.digest);
                snapshot.proposal_sha256 = bundle_hex;
                snapshot.candidate_sha256 = bundle_hex;
                snapshot.campaign.session.proposal_digest = bundle_hex;
                snapshot.campaign.session.candidate_digest = bundle_hex;
                const Result staged = adapter.stage_review(bundle, snapshot);
                review = *staged.review;
                reviewed = snapshot;
                reviewed.phase = self_iteration_orchestrator::Phase::awaiting_apply_decision;
                reviewed.generation += 1u;
                reviewed.state_sha256 = std::string(64u, '6');
                approved = *adapter.bind_approved_review(
                    review, reviewed, "review-transition-1").approved_review;
                receipt = {
                    .receipt_id = "receipt-1",
                    .kind = mcp_orchestrator_bridge::MutationKind::decide_apply,
                    .transport_session_id = "transport-1",
                    .connection_id = "connection-1",
                    .connection_generation = 3u,
                    .request_key = "request-1",
                    .call_id = "call-1",
                    .orchestrator_id = reviewed.orchestrator_id,
                    .campaign_id = reviewed.campaign.campaign_id,
                    .campaign_generation = reviewed.campaign.record_generation,
                    .session_identity = reviewed.campaign.session.identity,
                    .expected_orchestrator_generation = reviewed.generation,
                    .expected_state_sha256 = reviewed.state_sha256,
                    .intent = {.kind = mcp_orchestrator_bridge::MutationKind::decide_apply,
                        .evidence_sha256 = review.review_evidence_sha256,
                        .summary = "Apply reviewed bundle only in sandbox.",
                        .approve = true}};
                approval = {.receipt_id = receipt.receipt_id,
                    .connection_generation = receipt.connection_generation,
                    .expected_orchestrator_generation = reviewed.generation,
                    .expected_state_sha256 = reviewed.state_sha256,
                    .now_unix_seconds = 2'200'000'000u,
                    .operator_approved = true};
                sandbox.live[curated.relative_path] = curated.preimage_bytes;
                host.reviewed = reviewed;
            }
            source_patch_bundle::CuratedFile curated{curated_file()};
            self_iteration_orchestrator::Snapshot snapshot{base_snapshot()};
            Adapter adapter{};
            source_patch_bundle::Bundle bundle{};
            ReviewArtifact review{};
            self_iteration_orchestrator::Snapshot reviewed{};
            ApprovedReview approved{};
            ReceiptBinding receipt{};
            mcp_orchestrator_bridge::HostApproval approval{};
            FakeSandbox sandbox{};
            FakeJournal journal{};
            FakeHost host{};
        };

        [[nodiscard]] bool review_contract()
        {
            Fixture value{};
            if (value.review.files.size() != 1u
                || value.review.files.front().hunks.size() != 1u
                || value.review.files.front().unified_diff.empty()
                || value.review.review_evidence_sha256.size() != 64u
                || value.review.promotion_permitted) return false;
            auto stale = value.snapshot;
            stale.proposal_sha256 = std::string(64u, '9');
            if (value.adapter.stage_review(value.bundle, stale).code
                != Code::stale_state) return false;
            auto promotion = value.snapshot;
            promotion.promotion_permitted = true;
            if (value.adapter.stage_review(value.bundle, promotion).code
                != Code::stale_state) return false;
            auto foreign = value.bundle;
            foreign.authority_digest = source_patch_bundle::digest("foreign");
            foreign.digest = source_patch_bundle::digest(
                source_patch_bundle::encode(foreign));
            return value.adapter.stage_review(foreign, value.snapshot).code
                == Code::stale_state;
        }

        [[nodiscard]] bool apply_and_replay_contract()
        {
            Fixture value{};
            const Result applied = value.adapter.execute(value.approved,
                value.bundle, value.receipt, value.approval, value.host,
                value.sandbox, value.journal, 2'200'000'001u);
            if (applied.code != Code::applied || !applied.snapshot
                || applied.snapshot->phase
                    != self_iteration_orchestrator::Phase::awaiting_validation_request
                || value.host.approvals != 1u || value.host.evidence_calls != 1u
                || value.journal.records[value.receipt.receipt_id].phase
                    != JournalPhase::evidence_recorded
                || value.sandbox.live[value.curated.relative_path]
                    != "first\nthird\n") return false;
            if (value.adapter.execute(value.approved, value.bundle,
                    value.receipt, value.approval, value.host, value.sandbox,
                    value.journal, 2'200'000'002u).code
                != Code::replay_rejected) return false;
            Fixture stale{};
            stale.receipt.expected_state_sha256 = std::string(64u, '0');
            if (stale.adapter.execute(stale.approved, stale.bundle,
                    stale.receipt, stale.approval, stale.host, stale.sandbox,
                    stale.journal, 2'200'000'003u).code != Code::stale_state)
                return false;
            Fixture denied{};
            denied.approval.operator_approved = false;
            return denied.adapter.execute(denied.approved, denied.bundle,
                denied.receipt, denied.approval, denied.host, denied.sandbox,
                denied.journal, 2'200'000'004u).code == Code::approval_required;
        }

        [[nodiscard]] bool crash_recovery_contract()
        {
            Fixture value{};
            const Result pending = value.adapter.execute(value.approved,
                value.bundle, value.receipt, value.approval, value.host,
                value.sandbox, value.journal, 2'200'000'010u,
                {.defer_evidence_recording = true});
            if (pending.code != Code::evidence_pending || !pending.snapshot
                || value.journal.records[value.receipt.receipt_id].phase
                    != JournalPhase::committed) return false;
            Adapter restarted{};
            const Result recovered = restarted.recover(value.approved,
                value.bundle, value.receipt.receipt_id, *pending.snapshot,
                value.host, value.sandbox, value.journal, 2'200'000'011u);
            if (recovered.code != Code::recovered || !recovered.snapshot
                || recovered.snapshot->phase
                    != self_iteration_orchestrator::Phase::awaiting_validation_request
                || value.host.evidence_calls != 1u) return false;

            Fixture after_commit{};
            const Result committed = after_commit.adapter.execute(
                after_commit.approved, after_commit.bundle, after_commit.receipt,
                after_commit.approval, after_commit.host, after_commit.sandbox,
                after_commit.journal, 2'200'000'020u,
                {.defer_evidence_recording = true});
            if (committed.code != Code::evidence_pending) return false;
            auto& journal = after_commit.journal.records[
                after_commit.receipt.receipt_id];
            journal.phase = JournalPhase::approved;
            journal.implementation_evidence_sha256.clear();
            // Re-store through the fake port is intentionally insufficient to
            // recompute the private digest, so corrupt recovery must fail closed.
            if (Adapter{}.recover(after_commit.approved, after_commit.bundle,
                    after_commit.receipt.receipt_id, *committed.snapshot,
                    after_commit.host, after_commit.sandbox,
                    after_commit.journal, 2'200'000'021u).code
                != Code::journal_failure) return false;

            Fixture interrupted{};
            interrupted.host.pending = interrupted.reviewed;
            interrupted.host.pending.phase =
                self_iteration_orchestrator::Phase::awaiting_apply_result;
            interrupted.host.pending.generation += 1u;
            interrupted.host.pending.state_sha256 = std::string(64u, '7');
            JournalRecord record{
                .phase = JournalPhase::approved,
                .receipt_id = interrupted.receipt.receipt_id,
                .orchestrator_id = interrupted.review.orchestrator_id,
                .campaign_id = interrupted.review.campaign_id,
                .campaign_generation = interrupted.review.campaign_generation,
                .session_identity = interrupted.review.session_identity,
                .scope_sha256 = interrupted.review.scope_sha256,
                .authority_sha256 = interrupted.review.authority_sha256,
                .bundle_sha256 = interrupted.review.bundle_sha256,
                .review_evidence_sha256 = interrupted.review.review_evidence_sha256,
                .operation = {.operation_id = "sandbox-operation-1",
                    .orchestrator_id = interrupted.review.orchestrator_id,
                    .campaign_id = interrupted.review.campaign_id,
                    .session_identity = interrupted.review.session_identity,
                    .target_kind = interrupted.review.target_kind,
                    .expected_generation = interrupted.host.pending.generation,
                    .expected_state_sha256 = interrupted.host.pending.state_sha256,
                    .transition_id = "apply-transition-1",
                    .proposal_sha256 = interrupted.review.proposal_sha256,
                    .candidate_sha256 = interrupted.review.candidate_sha256}};
            // Generate a valid approved journal by beginning a deferred apply
            // in another fixture, then substitute only matching preimage state.
            Fixture source{};
            const Result source_pending = source.adapter.execute(source.approved,
                source.bundle, source.receipt, source.approval, source.host,
                source.sandbox, source.journal, 2'200'000'030u,
                {.defer_evidence_recording = true});
            if (source_pending.code != Code::evidence_pending) return false;
            interrupted.journal.records[interrupted.receipt.receipt_id]
                = source.journal.records[source.receipt.receipt_id];
            interrupted.journal.records[interrupted.receipt.receipt_id].phase
                = JournalPhase::approved;
            // This record digest is now stale and therefore fail-closed rather
            // than risking an unjournaled sandbox transaction replay.
            return Adapter{}.recover(interrupted.approved, interrupted.bundle,
                interrupted.receipt.receipt_id, interrupted.host.pending,
                interrupted.host, interrupted.sandbox, interrupted.journal,
                2'200'000'031u).code == Code::journal_failure;
        }
    }

    bool run_contract()
    {
        if (!review_contract()) return false;
        if (!apply_and_replay_contract()) return false;
        if (!crash_recovery_contract()) return false;
        return true;
    }
}
