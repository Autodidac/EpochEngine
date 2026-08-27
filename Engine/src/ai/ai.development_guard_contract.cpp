/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

import ai.development_guard;

namespace epochengine::ai::development_guard
{
    namespace
    {
        constexpr std::uint64_t opened_at = 1'000u;
        constexpr std::uint64_t session_expiry = 20'000u;

        [[nodiscard]] ProposalDigest digest(std::uint8_t seed) noexcept
        {
            ProposalDigest value{};
            for (std::size_t index = 0u; index < value.bytes.size(); ++index)
                value.bytes[index] = static_cast<std::uint8_t>(seed + index);
            return value;
        }

        [[nodiscard]] SessionPolicy policy(GuardLimits limits = {})
        {
            const WorkspacePermission readable_writable = WorkspacePermission::read
                | WorkspacePermission::write | WorkspacePermission::create
                | WorkspacePermission::remove;
            return {
                .identity = {0x45504f4348u, 7u},
                .workspace_id = "epoch.contract.workspace",
                .opened_at = opened_at,
                .expires_at = session_expiry,
                .limits = limits,
                .workspace_allowlist = {
                    {WorkspaceArea::engine_source, "Engine", readable_writable},
                    {WorkspaceArea::project_source, "Projects", readable_writable},
                    {WorkspaceArea::build_output, "x64",
                        readable_writable | WorkspacePermission::execute},
                    {WorkspaceArea::evidence, "logs", readable_writable},
                    {WorkspaceArea::dependency_cache, "cache", readable_writable}},
                .approved_operator_ids = {"operator.primary", "operator.release"}};
        }

        [[nodiscard]] DevelopmentOperation inspect_operation(
            std::uint64_t stable_id = 1u)
        {
            return {
                .stable_id = stable_id,
                .category = OperationCategory::inspect,
                .risk = RiskCategory::read_only,
                .summary = "Inspect the guarded source surface.",
                .workspace_intents = {{
                    WorkspaceArea::engine_source,
                    "Engine/modules/ai.mcp.ixx",
                    WorkspacePermission::read}}};
        }

        [[nodiscard]] DevelopmentOperation source_write_operation(
            std::uint64_t stable_id = 2u)
        {
            return {
                .stable_id = stable_id,
                .category = OperationCategory::source_write,
                .risk = RiskCategory::engine_source_write,
                .summary = "Apply one reviewed engine-source patch.",
                .workspace_intents = {{
                    WorkspaceArea::engine_source,
                    "Engine/src/ai/ai.service.cpp",
                    WorkspacePermission::write}},
                .content_transitions = {{
                    .relative_path = "Engine/src/ai/ai.service.cpp",
                    .before = {
                        .kind = ContentStateKind::sha256,
                        .digest = digest(40u),
                        .byte_count = 120u},
                    .after = {
                        .kind = ContentStateKind::sha256,
                        .digest = digest(41u),
                        .byte_count = 144u}}}};
        }

        [[nodiscard]] DevelopmentOperation build_operation(
            std::uint64_t stable_id = 3u)
        {
            return {
                .stable_id = stable_id,
                .category = OperationCategory::build,
                .risk = RiskCategory::child_process,
                .summary = "Build the reviewed source revision.",
                .workspace_intents = {
                    {WorkspaceArea::engine_source, "Engine", WorkspacePermission::read},
                    {WorkspaceArea::build_output, "x64/Debug",
                        WorkspacePermission::write | WorkspacePermission::create}}};
        }

        [[nodiscard]] DevelopmentOperation test_operation(
            std::uint64_t stable_id = 4u)
        {
            return {
                .stable_id = stable_id,
                .category = OperationCategory::test,
                .risk = RiskCategory::child_process,
                .summary = "Run bounded contract tests.",
                .workspace_intents = {{
                    WorkspaceArea::build_output,
                    "x64/Debug/EpochEditor.exe",
                    WorkspacePermission::read | WorkspacePermission::execute}}};
        }

        [[nodiscard]] DevelopmentOperation run_operation(
            std::uint64_t stable_id = 5u)
        {
            return {
                .stable_id = stable_id,
                .category = OperationCategory::run,
                .risk = RiskCategory::runtime_execution,
                .summary = "Run one explicitly approved evidence probe.",
                .workspace_intents = {{
                    WorkspaceArea::build_output,
                    "x64/Debug/EpochEditor.exe",
                    WorkspacePermission::read | WorkspacePermission::execute}}};
        }

        [[nodiscard]] ProposalDraft draft(
            const SessionPolicy& session,
            std::vector<DevelopmentOperation> operations,
            std::string title = "Guarded development proposal",
            std::uint64_t expires_at = 8'000u)
        {
            return {
                .session = session.identity,
                .title = std::move(title),
                .rationale = "Exercise an exact, reviewable development intent.",
                .created_at = 1'100u,
                .expires_at = expires_at,
                .operations = std::move(operations)};
        }

        [[nodiscard]] ReviewDecision accepted_review(
            const SessionPolicy& session,
            const ProposalReceipt& receipt)
        {
            return {
                .session = session.identity,
                .proposal = receipt.proposal,
                .proposal_digest = receipt.digest,
                .reviewer_id = "review.host",
                .disposition = ReviewDisposition::accept,
                .note = "Paths, risks, and evidence requirements reviewed."};
        }

        [[nodiscard]] ApprovalDecision approval(
            const SessionPolicy& session,
            const ProposalReceipt& receipt,
            std::uint64_t expires_at = 4'000u)
        {
            return {
                .session = session.identity,
                .proposal = receipt.proposal,
                .proposal_digest = receipt.digest,
                .operator_id = "operator.primary",
                .disposition = ApprovalDisposition::approve,
                .expires_at = expires_at,
                .note = "Approved for this exact immutable digest."};
        }

        [[nodiscard]] EvidenceItem evidence_item(
            EvidenceCategory category,
            std::uint8_t seed,
            bool verified = true)
        {
            return {
                .category = category,
                .locator = "logs/evidence/item.json",
                .content_digest = digest(seed),
                .summary = "Host-verified bounded evidence.",
                .verified = verified};
        }

        [[nodiscard]] bool policy_validation_contract()
        {
            SessionPolicy valid = policy();
            if (!DevelopmentGuard::validate_policy(valid))
                return false;

            SessionPolicy invalid = valid;
            invalid.identity = {};
            if (DevelopmentGuard::validate_policy(invalid).code != GuardCode::invalid_policy)
                return false;
            invalid = valid;
            invalid.limits.maximum_proposals = 0u;
            if (DevelopmentGuard::validate_policy(invalid).code != GuardCode::invalid_limits)
                return false;
            invalid = valid;
            invalid.approved_operator_ids.push_back("operator.primary");
            if (DevelopmentGuard::validate_policy(invalid).code != GuardCode::invalid_policy)
                return false;
            invalid = valid;
            invalid.workspace_allowlist.front().relative_root = "Engine/../outside";
            if (DevelopmentGuard::validate_policy(invalid).code != GuardCode::invalid_policy)
                return false;

            const auto normalized = canonical_relative_path("Engine\\src\\ai");
            return normalized && *normalized == "Engine/src/ai"
                && !canonical_relative_path("C:/Engine")
                && !canonical_relative_path("../Engine")
                && !canonical_relative_path("Engine//src")
                && !canonical_relative_path("/Engine/src");
        }

        [[nodiscard]] bool inspect_flow_contract()
        {
            const SessionPolicy session = policy();
            DevelopmentGuard guard{session};
            ProposalDraft proposal = draft(session, {inspect_operation()});
            ProposalDraft immutable_probe = proposal;
            const ProposalReceipt receipt = guard.submit(std::move(proposal), 1'200u);
            if (!receipt || guard.audit().size() != 2u)
                return false;

            immutable_probe.title = "Mutated after submission";
            immutable_probe.operations.front().summary = "Mutated operation";
            const auto stored = guard.snapshot(receipt.proposal);
            if (!stored || stored->title == immutable_probe.title
                || stored->operations.front().summary
                    == immutable_probe.operations.front().summary
                || stored->digest != receipt.digest)
            {
                return false;
            }

            if (guard.execution_eligibility(
                    session.identity, receipt.proposal, receipt.digest, 1'250u).code
                != GuardCode::review_required)
            {
                return false;
            }
            ReviewDecision foreign_review = accepted_review(session, receipt);
            ++foreign_review.session.generation;
            if (guard.review(std::move(foreign_review), 1'300u).code
                != GuardCode::invalid_session)
            {
                return false;
            }
            ReviewDecision wrong_digest = accepted_review(session, receipt);
            wrong_digest.proposal_digest = digest(99u);
            if (guard.review(std::move(wrong_digest), 1'300u).code
                != GuardCode::digest_mismatch)
            {
                return false;
            }
            if (!guard.review(accepted_review(session, receipt), 1'300u))
                return false;
            if (guard.approve(approval(session, receipt), 1'350u).code
                != GuardCode::approval_not_required)
            {
                return false;
            }

            const Eligibility eligible = guard.execution_eligibility(
                session.identity, receipt.proposal, receipt.digest, 1'400u);
            if (!eligible || eligible.operator_approval_required
                || !has_evidence(eligible.required_evidence,
                    EvidenceRequirement::inspection_record))
            {
                return false;
            }
            const PermitReceipt permit = guard.issue_execution_permit(
                session.identity, receipt.proposal, receipt.digest, 1'400u, 100u);
            if (!permit
                || !guard.claim_execution_permit(
                    permit.permit, "tool.inspect", 1'401u))
            {
                return false;
            }
            if (guard.claim_execution_permit(
                    permit.permit, "tool.inspect", 1'402u).code
                != GuardCode::permit_invalid)
            {
                return false;
            }
            if (guard.issue_execution_permit(
                    session.identity, receipt.proposal, receipt.digest, 1'401u, 50u).code
                != GuardCode::state_conflict)
            {
                return false;
            }

            EvidenceSubmission unverified{
                .permit = permit.permit,
                .actor_id = "tool.inspect",
                .outcome = ExecutionOutcome::succeeded,
                .summary = "Inspection completed.",
                .items = {evidence_item(
                    EvidenceCategory::inspection_record, 1u, false)}};
            if (guard.record_evidence(std::move(unverified), 1'450u).code
                != GuardCode::evidence_unverified)
            {
                return false;
            }
            EvidenceSubmission missing{
                .permit = permit.permit,
                .actor_id = "tool.inspect",
                .outcome = ExecutionOutcome::succeeded,
                .summary = "Wrong evidence category.",
                .items = {evidence_item(EvidenceCategory::diagnostic, 2u)}};
            if (guard.record_evidence(std::move(missing), 1'450u).code
                != GuardCode::evidence_missing)
            {
                return false;
            }
            EvidenceSubmission complete{
                .permit = permit.permit,
                .actor_id = "tool.inspect",
                .outcome = ExecutionOutcome::succeeded,
                .summary = "Inspection completed.",
                .items = {evidence_item(EvidenceCategory::inspection_record, 3u)}};
            if (!guard.record_evidence(std::move(complete), 1'450u))
                return false;
            if (guard.record_evidence(EvidenceSubmission{
                    .permit = permit.permit,
                    .actor_id = "tool.inspect",
                    .outcome = ExecutionOutcome::succeeded,
                    .summary = "Duplicate.",
                    .items = {evidence_item(EvidenceCategory::inspection_record, 4u)}},
                    1'451u).code != GuardCode::already_complete)
            {
                return false;
            }
            const auto finished = guard.snapshot(receipt.proposal);
            if (!finished || finished->state != ProposalState::completed
                || guard.evidence().size() != 1u
                || !guard.evidence().front().evidence_digest.valid())
            {
                return false;
            }
            std::uint64_t expected_sequence = 1u;
            for (const AuditEvent& event : guard.audit())
            {
                if (event.sequence != expected_sequence++)
                    return false;
            }
            return guard.audit().back().kind == AuditKind::evidence_accepted
                && guard.audit().back().evidence_digest
                    == guard.evidence().front().evidence_digest;
        }

        [[nodiscard]] bool guarded_execution_contract()
        {
            const SessionPolicy session = policy();
            DevelopmentGuard guard{session};
            const ProposalReceipt receipt = guard.submit(draft(session, {
                source_write_operation(), build_operation(), test_operation(), run_operation()}),
                1'200u);
            if (!receipt || !guard.review(accepted_review(session, receipt), 1'300u))
                return false;

            const Eligibility blocked = guard.execution_eligibility(
                session.identity, receipt.proposal, receipt.digest, 1'350u);
            if (blocked.code != GuardCode::approval_required
                || !blocked.operator_approval_required)
            {
                return false;
            }
            ApprovalDecision wrong_digest = approval(session, receipt);
            wrong_digest.proposal_digest = digest(75u);
            if (guard.approve(std::move(wrong_digest), 1'400u).code
                != GuardCode::digest_mismatch)
            {
                return false;
            }
            ApprovalDecision intruder = approval(session, receipt);
            intruder.operator_id = "model.self";
            if (guard.approve(std::move(intruder), 1'400u).code
                != GuardCode::operator_denied)
            {
                return false;
            }
            ApprovalDecision too_long = approval(session, receipt, 10'000u);
            if (guard.approve(std::move(too_long), 1'400u).code
                != GuardCode::invalid_timestamp)
            {
                return false;
            }
            if (!guard.approve(approval(session, receipt), 1'400u))
                return false;
            const auto approved = guard.snapshot(receipt.proposal);
            if (!approved || !approved->approval
                || approved->approval->operator_id != "operator.primary"
                || approved->approval->proposal_digest != receipt.digest)
            {
                return false;
            }
            if (guard.issue_execution_permit(
                    session.identity, receipt.proposal, receipt.digest,
                    1'500u, session.limits.maximum_permit_lifetime + 1u).code
                != GuardCode::invalid_timestamp)
            {
                return false;
            }
            const PermitReceipt permit = guard.issue_execution_permit(
                session.identity, receipt.proposal, receipt.digest, 1'500u, 100u);
            if (!permit
                || !guard.claim_execution_permit(
                    permit.permit, "harness", 1'501u))
            {
                return false;
            }

            const ExecutionPermit forged{};
            if (guard.record_evidence(EvidenceSubmission{
                    .permit = forged,
                    .actor_id = "harness",
                    .outcome = ExecutionOutcome::failed,
                    .summary = "Forged permit.",
                    .items = {evidence_item(EvidenceCategory::diagnostic, 10u)}},
                    1'550u).code != GuardCode::permit_invalid)
            {
                return false;
            }
            if (guard.record_evidence(EvidenceSubmission{
                    .permit = permit.permit,
                    .actor_id = "harness",
                    .outcome = ExecutionOutcome::succeeded,
                    .summary = "Incomplete evidence.",
                    .items = {evidence_item(EvidenceCategory::patch, 11u)}},
                    1'550u).code != GuardCode::evidence_missing)
            {
                return false;
            }
            EvidenceSubmission complete{
                .permit = permit.permit,
                .actor_id = "harness",
                .outcome = ExecutionOutcome::succeeded,
                .summary = "Approved source, build, test, and run completed.",
                .items = {
                    evidence_item(EvidenceCategory::patch, 12u),
                    evidence_item(EvidenceCategory::build_log, 13u),
                    evidence_item(EvidenceCategory::test_report, 14u),
                    evidence_item(EvidenceCategory::runtime_report, 15u)}};
            if (!guard.record_evidence(std::move(complete), 1'550u))
                return false;
            return guard.snapshot(receipt.proposal)->state == ProposalState::completed;
        }

        [[nodiscard]] bool path_and_risk_contract()
        {
            const SessionPolicy session = policy();
            DevelopmentGuard guard{session};

            DevelopmentOperation traversal = source_write_operation();
            traversal.workspace_intents.front().relative_path = "Engine/../outside.cpp";
            if (guard.submit(draft(session, {traversal}), 1'200u).code
                != GuardCode::invalid_path)
            {
                return false;
            }
            DevelopmentOperation prefix_escape = source_write_operation();
            prefix_escape.workspace_intents.front().relative_path = "Engine2/outside.cpp";
            if (guard.submit(draft(session, {prefix_escape}), 1'200u).code
                != GuardCode::path_denied)
            {
                return false;
            }
            DevelopmentOperation wrong_area = source_write_operation();
            wrong_area.workspace_intents.front().area = WorkspaceArea::project_source;
            wrong_area.workspace_intents.front().relative_path = "Projects/game.cpp";
            if (guard.submit(draft(session, {wrong_area}), 1'200u).code
                != GuardCode::path_denied)
            {
                return false;
            }
            DevelopmentOperation disguised = source_write_operation();
            disguised.risk = RiskCategory::read_only;
            if (guard.submit(draft(session, {disguised}), 1'200u).code
                != GuardCode::risk_mismatch)
            {
                return false;
            }
            DevelopmentOperation inspecting_write = inspect_operation();
            inspecting_write.workspace_intents.front().permissions = WorkspacePermission::write;
            if (guard.submit(draft(session, {inspecting_write}), 1'200u).code
                != GuardCode::risk_mismatch)
            {
                return false;
            }
            DevelopmentOperation non_executable_run = run_operation();
            non_executable_run.workspace_intents.front().permissions = WorkspacePermission::read;
            if (guard.submit(draft(session, {non_executable_run}), 1'200u).code
                != GuardCode::invalid_operation)
            {
                return false;
            }
            DevelopmentOperation transition_mismatch =
                source_write_operation(51u);
            transition_mismatch.content_transitions.front().relative_path =
                "Engine/src/ai/other.cpp";
            if (guard.submit(draft(
                    session, {transition_mismatch}), 1'200u).code
                != GuardCode::path_denied)
            {
                return false;
            }

            DevelopmentOperation no_op = source_write_operation(52u);
            no_op.content_transitions.front().after =
                no_op.content_transitions.front().before;
            if (guard.submit(draft(session, {no_op}), 1'200u).code
                != GuardCode::invalid_operation)
            {
                return false;
            }

            DevelopmentOperation inspect_with_content =
                inspect_operation(53u);
            inspect_with_content.content_transitions =
                source_write_operation().content_transitions;
            if (guard.submit(draft(
                    session, {inspect_with_content}), 1'200u).code
                != GuardCode::invalid_operation)
            {
                return false;
            }

            DevelopmentOperation oversized = source_write_operation(54u);
            oversized.content_transitions.front().after.byte_count =
                session.limits.maximum_content_bytes_per_transition + 1u;
            if (guard.submit(draft(
                    session, {oversized}), 1'200u).code
                != GuardCode::invalid_operation)
            {
                return false;
            }
            if (guard.submit(draft(session, {
                    inspect_operation(50u), source_write_operation(50u)}), 1'200u).code
                != GuardCode::invalid_identity)
            {
                return false;
            }
            ProposalDraft foreign = draft(session, {inspect_operation()});
            ++foreign.session.generation;
            return guard.submit(std::move(foreign), 1'200u).code
                == GuardCode::invalid_session;
        }

        [[nodiscard]] bool expiry_and_cancellation_contract()
        {
            const SessionPolicy session = policy();
            DevelopmentGuard proposal_expiry_guard{session};
            const ProposalReceipt expiring = proposal_expiry_guard.submit(
                draft(session, {inspect_operation()}, "Expiring proposal", 1'500u), 1'200u);
            if (!expiring
                || !proposal_expiry_guard.review(
                    accepted_review(session, expiring), 1'300u))
            {
                return false;
            }
            if (proposal_expiry_guard.issue_execution_permit(
                    session.identity, expiring.proposal, expiring.digest,
                    1'501u, 10u).code != GuardCode::proposal_expired)
            {
                return false;
            }
            const auto expired = proposal_expiry_guard.snapshot(expiring.proposal);
            if (!expired || expired->state != ProposalState::expired)
                return false;

            DevelopmentGuard approval_expiry_guard{session};
            const ProposalReceipt approval_receipt = approval_expiry_guard.submit(
                draft(session, {source_write_operation()}), 1'200u);
            if (!approval_expiry_guard.review(
                    accepted_review(session, approval_receipt), 1'300u)
                || !approval_expiry_guard.approve(
                    approval(session, approval_receipt, 1'450u), 1'350u))
            {
                return false;
            }
            if (approval_expiry_guard.execution_eligibility(
                    session.identity, approval_receipt.proposal,
                    approval_receipt.digest, 1'451u).code
                != GuardCode::approval_expired)
            {
                return false;
            }

            DevelopmentGuard permit_expiry_guard{session};
            const ProposalReceipt permit_receipt = permit_expiry_guard.submit(
                draft(session, {inspect_operation()}), 1'200u);
            if (!permit_expiry_guard.review(
                    accepted_review(session, permit_receipt), 1'300u))
            {
                return false;
            }
            const PermitReceipt short_permit = permit_expiry_guard.issue_execution_permit(
                session.identity, permit_receipt.proposal, permit_receipt.digest,
                1'400u, 10u);
            if (!short_permit
                || !permit_expiry_guard.claim_execution_permit(
                    short_permit.permit, "tool.inspect", 1'401u))
            {
                return false;
            }
            if (permit_expiry_guard.record_evidence(EvidenceSubmission{
                    .permit = short_permit.permit,
                    .actor_id = "tool.inspect",
                    .outcome = ExecutionOutcome::succeeded,
                    .summary = "Late evidence.",
                    .items = {evidence_item(EvidenceCategory::inspection_record, 20u)}},
                    1'411u).code != GuardCode::permit_expired)
            {
                return false;
            }

            DevelopmentGuard cancellation_guard{session};
            const ProposalReceipt cancelled = cancellation_guard.submit(
                draft(session, {inspect_operation()}), 1'200u);
            if (!cancellation_guard.review(accepted_review(session, cancelled), 1'300u))
                return false;
            const PermitReceipt cancelled_permit = cancellation_guard.issue_execution_permit(
                session.identity, cancelled.proposal, cancelled.digest, 1'400u, 100u);
            if (!cancelled_permit
                || !cancellation_guard.cancel(CancellationRequest{
                    .session = session.identity,
                    .proposal = cancelled.proposal,
                    .proposal_digest = cancelled.digest,
                    .actor_id = "operator.primary",
                    .reason = "Operator cancelled before execution."}, 1'410u))
            {
                return false;
            }
            if (cancellation_guard.record_evidence(EvidenceSubmission{
                    .permit = cancelled_permit.permit,
                    .actor_id = "tool.inspect",
                    .outcome = ExecutionOutcome::succeeded,
                    .summary = "Cancelled evidence.",
                    .items = {evidence_item(EvidenceCategory::inspection_record, 21u)}},
                    1'420u).code != GuardCode::permit_invalid)
            {
                return false;
            }

            DevelopmentGuard session_cancel_guard{session};
            const ProposalReceipt session_cancelled = session_cancel_guard.submit(
                draft(session, {inspect_operation()}), 1'200u);
            if (!session_cancelled
                || !session_cancel_guard.cancel_session(
                    session.identity, "operator.primary", "Stop all AI work.", 1'250u))
            {
                return false;
            }
            return session_cancel_guard.execution_eligibility(
                session.identity, session_cancelled.proposal,
                session_cancelled.digest, 1'260u).code == GuardCode::session_cancelled;
        }

        [[nodiscard]] bool bounded_storage_contract()
        {
            GuardLimits limits{};
            limits.maximum_proposals = 1u;
            SessionPolicy bounded_policy = policy(limits);
            DevelopmentGuard proposal_guard{bounded_policy};
            if (!proposal_guard.submit(
                    draft(bounded_policy, {inspect_operation()}), 1'200u))
            {
                return false;
            }
            if (proposal_guard.submit(draft(
                    bounded_policy, {inspect_operation(2u)}, "Second proposal"), 1'200u).code
                != GuardCode::storage_exhausted)
            {
                return false;
            }

            limits = {};
            limits.maximum_operations_per_proposal = 1u;
            bounded_policy = policy(limits);
            DevelopmentGuard operation_guard{bounded_policy};
            if (operation_guard.submit(draft(bounded_policy, {
                    inspect_operation(), source_write_operation()}), 1'200u))
            {
                return false;
            }

            limits = {};
            limits.maximum_audit_events = 2u;
            bounded_policy = policy(limits);
            DevelopmentGuard audit_guard{bounded_policy};
            const ProposalReceipt audit_receipt = audit_guard.submit(
                draft(bounded_policy, {inspect_operation()}), 1'200u);
            if (!audit_receipt
                || audit_guard.review(
                    accepted_review(bounded_policy, audit_receipt), 1'300u).code
                    != GuardCode::storage_exhausted)
            {
                return false;
            }

            limits = {};
            limits.maximum_total_proposal_bytes = 32u;
            bounded_policy = policy(limits);
            DevelopmentGuard byte_guard{bounded_policy};
            if (byte_guard.submit(draft(
                    bounded_policy, {inspect_operation()},
                    "A proposal title larger than the complete byte budget"), 1'200u).code
                != GuardCode::storage_exhausted)
            {
                return false;
            }

            limits = {};
            limits.maximum_evidence_records = 1u;
            bounded_policy = policy(limits);
            DevelopmentGuard evidence_guard{bounded_policy};
            const ProposalReceipt first = evidence_guard.submit(
                draft(bounded_policy, {inspect_operation()}, "First evidence"), 1'200u);
            if (!evidence_guard.review(accepted_review(bounded_policy, first), 1'300u))
                return false;
            const PermitReceipt first_permit = evidence_guard.issue_execution_permit(
                bounded_policy.identity, first.proposal, first.digest, 1'400u, 100u);
            if (!first_permit
                || !evidence_guard.claim_execution_permit(
                    first_permit.permit, "tool.inspect", 1'401u))
            {
                return false;
            }
            if (!evidence_guard.record_evidence(EvidenceSubmission{
                    .permit = first_permit.permit,
                    .actor_id = "tool.inspect",
                    .outcome = ExecutionOutcome::succeeded,
                    .summary = "First evidence.",
                    .items = {evidence_item(EvidenceCategory::inspection_record, 30u)}},
                    1'450u))
            {
                return false;
            }
            const ProposalReceipt second = evidence_guard.submit(draft(
                bounded_policy, {inspect_operation(2u)}, "Second evidence"), 1'500u);
            if (!evidence_guard.review(accepted_review(bounded_policy, second), 1'550u))
                return false;
            const PermitReceipt second_permit = evidence_guard.issue_execution_permit(
                bounded_policy.identity, second.proposal, second.digest, 1'600u, 100u);
            if (!second_permit
                || !evidence_guard.claim_execution_permit(
                    second_permit.permit, "tool.inspect", 1'601u))
            {
                return false;
            }
            return evidence_guard.record_evidence(EvidenceSubmission{
                    .permit = second_permit.permit,
                    .actor_id = "tool.inspect",
                    .outcome = ExecutionOutcome::succeeded,
                    .summary = "Second evidence.",
                    .items = {evidence_item(EvidenceCategory::inspection_record, 31u)}},
                    1'650u).code == GuardCode::storage_exhausted;
        }

        [[nodiscard]] bool deterministic_digest_contract()
        {
            const SessionPolicy session = policy();
            DevelopmentGuard first_guard{session};
            DevelopmentGuard second_guard{session};
            ProposalDraft first_draft = draft(session, {source_write_operation()});
            ProposalDraft second_draft = first_draft;
            second_draft.operations.front().workspace_intents.front().relative_path =
                "Engine\\src\\ai\\ai.service.cpp";
            const ProposalReceipt first = first_guard.submit(std::move(first_draft), 1'200u);
            const ProposalReceipt second = second_guard.submit(std::move(second_draft), 1'200u);
            if (!first || !second || first.digest != second.digest)
                return false;

            DevelopmentGuard changed_guard{session};
            ProposalDraft changed = draft(session, {source_write_operation()});
            changed.rationale = "A semantically different proposal.";
            const ProposalReceipt different = changed_guard.submit(std::move(changed), 1'200u);
            if (!different || different.digest == first.digest)
                return false;

            DevelopmentGuard content_changed_guard{session};
            ProposalDraft content_changed = draft(
                session, {source_write_operation()});
            content_changed.operations.front()
                .content_transitions.front().after.digest = digest(99u);
            const ProposalReceipt content_different =
                content_changed_guard.submit(
                    std::move(content_changed), 1'200u);
            if (!content_different
                || content_different.digest == first.digest)
            {
                return false;
            }

            ProposalIdentity stale = first.proposal;
            ++stale.generation;
            return first_guard.execution_eligibility(
                session.identity, stale, first.digest, 1'300u).code
                == GuardCode::proposal_not_found;
        }

        [[nodiscard]] bool failed_execution_evidence_contract()
        {
            const SessionPolicy session = policy();
            DevelopmentGuard guard{session};
            const ProposalReceipt receipt = guard.submit(
                draft(session, {build_operation()}), 1'200u);
            if (!guard.review(accepted_review(session, receipt), 1'300u)
                || !guard.approve(approval(session, receipt), 1'400u))
            {
                return false;
            }
            const PermitReceipt permit = guard.issue_execution_permit(
                session.identity, receipt.proposal, receipt.digest, 1'500u, 100u);
            if (!permit
                || !guard.claim_execution_permit(
                    permit.permit, "build.harness", 1'501u))
            {
                return false;
            }
            if (guard.record_evidence(EvidenceSubmission{
                    .permit = permit.permit,
                    .actor_id = "build.harness",
                    .outcome = ExecutionOutcome::failed,
                    .summary = "Build failed without diagnostics.",
                    .items = {evidence_item(EvidenceCategory::build_log, 40u)}},
                    1'550u).code != GuardCode::evidence_missing)
            {
                return false;
            }
            if (!guard.record_evidence(EvidenceSubmission{
                    .permit = permit.permit,
                    .actor_id = "build.harness",
                    .outcome = ExecutionOutcome::failed,
                    .summary = "Build failed with diagnostics.",
                    .items = {evidence_item(EvidenceCategory::diagnostic, 41u)}},
                    1'550u))
            {
                return false;
            }
            const auto failed = guard.snapshot(receipt.proposal);
            return failed && failed->state == ProposalState::failed
                && guard.evidence().front().outcome == ExecutionOutcome::failed;
        }
    }

    [[nodiscard]] int run_development_guard_contract_tests()
    {
        if (!policy_validation_contract()) return 1;
        if (!inspect_flow_contract()) return 2;
        if (!guarded_execution_contract()) return 3;
        if (!path_and_risk_contract()) return 4;
        if (!expiry_and_cancellation_contract()) return 5;
        if (!bounded_storage_contract()) return 6;
        if (!deterministic_digest_contract()) return 7;
        if (!failed_execution_evidence_contract()) return 8;
        return 0;
    }
}

int main()
{
    return epochengine::ai::development_guard::run_development_guard_contract_tests();
}
