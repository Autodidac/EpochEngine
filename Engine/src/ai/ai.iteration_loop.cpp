/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.iteration_loop;

namespace epochengine::ai::iteration
{
    namespace
    {
        [[nodiscard]] constexpr bool architecture_review_required(
            RiskClass risk) noexcept
        {
            return risk != RiskClass::contained_feature
                && risk != RiskClass::related_files;
        }

        [[nodiscard]] constexpr bool frontier_review_required(
            RiskClass risk) noexcept
        {
            return risk == RiskClass::cross_subsystem
                || risk == RiskClass::architecture_campaign
                || risk == RiskClass::lifetime_critical
                || risk == RiskClass::concurrency_critical
                || risk == RiskClass::vulkan_synchronization;
        }

        [[nodiscard]] constexpr std::uint32_t minimum_context(
            RiskClass risk) noexcept
        {
            switch (risk)
            {
            case RiskClass::contained_feature: return 8'192u;
            case RiskClass::related_files: return 16'384u;
            case RiskClass::subsystem: return 32'768u;
            case RiskClass::cross_subsystem:
            case RiskClass::architecture_campaign:
            case RiskClass::lifetime_critical:
            case RiskClass::concurrency_critical:
            case RiskClass::vulkan_synchronization:
                return 65'536u;
            }
            return 65'536u;
        }

        [[nodiscard]] bool valid_summary(std::string_view summary) noexcept
        {
            return !summary.empty() && summary.size() <= 4'096u
                && std::none_of(summary.begin(), summary.end(), [](char value)
                {
                    return value == '\0';
                });
        }
    }

    ModelCapabilities infer_model_capabilities(std::string_view model_name)
    {
        ModelCapabilities result{};
        result.model_name = std::string{model_name};
        return result;
    }

    CapabilityAssessment assess_capabilities(
        const ModelCapabilities& model,
        const LoopPolicy& policy)
    {
        CapabilityAssessment result{
            .disposition = CapabilityDisposition::eligible,
            .summary = "Reported model properties fit the bounded writer lane; actual output remains unverified.",
            .recommended_model_class =
                "A tool-capable coding model with repository reasoning.",
            .operator_approval_required = true,
            .architecture_review_required =
                architecture_review_required(policy.risk),
            .frontier_review_required =
                frontier_review_required(policy.risk)};

        if (model.model_name.empty())
        {
            result.disposition = CapabilityDisposition::blocked;
            result.summary = "No operator-selected local model is available.";
            result.recommended_model_class =
                "Select a local coding model before starting the loop.";
            return result;
        }

        if (policy.host_context_budget_tokens < minimum_context(policy.risk))
        {
            result.disposition = CapabilityDisposition::blocked;
            result.summary = "The host source-request context budget is below the required bounded lane.";
            result.recommended_model_class = "Configure a sufficient host request budget; changing a model name cannot enlarge it.";
            return result;
        }

        const bool common = model.code_generation && model.tool_use
            && model.context_tokens >= minimum_context(policy.risk);
        const bool repository = policy.risk == RiskClass::contained_feature
            || (model.repository_reasoning && model.build_repair);
        const bool planning = !policy.require_research || model.research;
        const bool review = !policy.require_local_self_review || model.self_review;
        if (!common || !repository || !planning || !review)
        {
            result.disposition =
                CapabilityDisposition::recommend_stronger_model;
            result.summary =
                "Model skill and context support are not established by its name. "
                "The selected model may attempt bounded sandbox work; its output "
                "must still pass exact source, compiler and validation checks.";
            result.recommended_model_class =
                "Choose an agentic coding model with enough context for the task. "
                "Qwen 3.5+ is the requested coding tier, with 3.8 preferred; "
                "newer operator-selected models are not rejected by name.";
            return result;
        }

        if (result.frontier_review_required)
        {
            result.summary =
                "The local model may write bounded candidates, but this risk "
                "class requires operator supervision and frontier review before "
                "completion.";
        }
        else if (result.architecture_review_required)
        {
            result.summary =
                "The local model may iterate with compiler, test, analyzer, and "
                "architecture-review evidence.";
        }
        return result;
    }

    EvidenceActor required_actor(Milestone milestone) noexcept
    {
        switch (milestone)
        {
        case Milestone::inspect_architecture:
        case Milestone::state_invariants:
        case Milestone::research_and_plan:
        case Milestone::source_proposal:
        case Milestone::local_self_review:
            return EvidenceActor::local_model;
        case Milestone::implement:
            return EvidenceActor::source_executor;
        case Milestone::operator_approval:
            return EvidenceActor::operator_user;
        case Milestone::compile: return EvidenceActor::compiler;
        case Milestone::test: return EvidenceActor::test_runner;
        case Milestone::static_analysis:
            return EvidenceActor::static_analyzer;
        case Milestone::sanitizer:
            return EvidenceActor::sanitizer_runtime;
        case Milestone::architecture_review:
            return EvidenceActor::architecture_reviewer;
        case Milestone::visual_validation:
            return EvidenceActor::visual_harness;
        case Milestone::frontier_review:
            return EvidenceActor::frontier_reviewer;
        case Milestone::idle:
        case Milestone::complete:
        case Milestone::blocked:
        case Milestone::cancelled:
            return EvidenceActor::operator_user;
        }
        return EvidenceActor::operator_user;
    }

    LoopResult BoundedIterationLoop::configure(
        LoopPolicy policy,
        ModelCapabilities model)
    {
        if (policy.maximum_repair_attempts == 0u
            || policy.maximum_repair_attempts > 10u
            || !policy.scope_digest.valid())
        {
            return remember(LoopCode::invalid_policy,
                "Repair attempts must be bounded between one and ten and the "
                "campaign requires a non-zero host-owned scope digest.");
        }
        policy_ = policy;
        model_ = std::move(model);
        capability_ = assess_capabilities(model_, policy_);
        milestone_ = Milestone::idle;
        evidence_.clear();
        pending_authority_digest_ = {};
        approved_authority_digest_ = {};
        dispatch_id_ = 0u;
        repair_attempts_ = 0u;
        configured_ = true;
        operator_approval_recorded_ = false;
        repair_cycle_ = false;
        return remember(LoopCode::none, capability_.summary);
    }

    LoopResult BoundedIterationLoop::begin()
    {
        if (!configured_ || milestone_ != Milestone::idle)
            return remember(LoopCode::invalid_state,
                "The loop must be configured and idle before it starts.");
        if (!capability_.may_attempt())
        {
            milestone_ = Milestone::blocked;
            return remember(LoopCode::capability_blocked,
                capability_.summary + " "
                + capability_.recommended_model_class);
        }
        milestone_ = Milestone::inspect_architecture;
        begin_dispatch();
        return remember(LoopCode::none,
            "Inspect existing ownership, build lanes, and subsystem boundaries.");
    }

    LoopResult BoundedIterationLoop::record(MilestoneEvidence evidence)
    {
        if (milestone_ == Milestone::idle
            || milestone_ == Milestone::complete
            || milestone_ == Milestone::blocked
            || milestone_ == Milestone::cancelled)
        {
            return remember(LoopCode::invalid_state,
                "The loop is not accepting milestone evidence.");
        }
        if (evidence.dispatch_id != dispatch_id_)
        {
            return remember(LoopCode::stale_completion,
                "Stale or duplicate milestone completion rejected.");
        }
        if (evidence.milestone != milestone_
            || evidence.actor != required_actor(milestone_)
            || !evidence.digest.valid()
            || !evidence.authority_digest.valid()
            || !evidence.verified
            || (evidence.actor == EvidenceActor::local_model
                && !evidence.operator_reviewed)
            || !valid_summary(evidence.summary))
        {
            return remember(LoopCode::evidence_rejected,
                "Evidence must match the active dispatch, milestone, required "
                "actor, verified digest, bounded summary, and operator review "
                "for model-authored evidence.");
        }
        if (!authority_matches(evidence)
            || (milestone_ == Milestone::source_proposal
                && evidence.digest != evidence.authority_digest))
        {
            return remember(LoopCode::authority_mismatch,
                "Evidence is not bound to the active scope or exact approved "
                "source-proposal digest.");
        }

        evidence_.push_back(std::move(evidence));
        const MilestoneEvidence& recorded = evidence_.back();
        if (!recorded.passed)
        {
            if (milestone_ == Milestone::operator_approval)
            {
                milestone_ = Milestone::blocked;
                begin_dispatch();
                return remember(LoopCode::capability_blocked,
                    "The operator did not approve the immutable proposal.");
            }
            if (!repairable(milestone_))
            {
                begin_dispatch();
                return remember(LoopCode::none,
                    "The milestone remains active until new verified evidence passes.");
            }
            ++repair_attempts_;
            if (repair_attempts_ > policy_.maximum_repair_attempts)
            {
                milestone_ = Milestone::blocked;
                begin_dispatch();
                return remember(LoopCode::retry_exhausted,
                    "The bounded repair budget is exhausted; human diagnosis is required.");
            }
            pending_authority_digest_ = {};
            approved_authority_digest_ = {};
            operator_approval_recorded_ = false;
            repair_cycle_ = true;
            milestone_ = Milestone::source_proposal;
            begin_dispatch();
            return remember(LoopCode::none,
                "Validation failed. Request one bounded repair proposal, obtain "
                "approval for its new exact digest, and repeat every downstream gate.");
        }

        if (milestone_ == Milestone::source_proposal)
            pending_authority_digest_ = recorded.authority_digest;
        if (milestone_ == Milestone::operator_approval)
        {
            approved_authority_digest_ = pending_authority_digest_;
            operator_approval_recorded_ = true;
        }
        const Milestone completedMilestone = milestone_;
        milestone_ = milestone_after(milestone_);
        if (completedMilestone == Milestone::implement)
            repair_cycle_ = false;
        begin_dispatch();
        if (milestone_ == Milestone::complete)
        {
            return remember(LoopCode::none,
                "All required evidence gates passed; the bounded iteration is complete.");
        }
        return remember(LoopCode::none,
            "Awaiting verified " + std::string{name(milestone_)}
            + " evidence from the "
            + std::string{name(required_actor(milestone_))} + ".");
    }

    LoopResult BoundedIterationLoop::cancel(std::string reason)
    {
        if (milestone_ == Milestone::complete
            || milestone_ == Milestone::cancelled)
        {
            return remember(LoopCode::invalid_state,
                "A terminal loop cannot be cancelled again.");
        }
        if (!valid_summary(reason))
            return remember(LoopCode::evidence_rejected,
                "Cancellation requires a bounded reason.");
        milestone_ = Milestone::cancelled;
        begin_dispatch();
        status_ = std::move(reason);
        last_code_ = LoopCode::cancelled;
        return {last_code_, status_};
    }

    MilestoneAction BoundedIterationLoop::next_action() const
    {
        MilestoneAction action{
            .milestone = milestone_,
            .actor = required_actor(milestone_),
            .dispatch_id = dispatch_id_,
            .repair_attempt = repair_attempts_};

        switch (milestone_)
        {
        case Milestone::inspect_architecture:
            action.kind = DispatchKind::request_local_model;
            action.authority_digest = policy_.scope_digest;
            action.instruction =
                "Inspect only the host-curated architecture and ownership evidence.";
            action.operator_review_required = true;
            break;
        case Milestone::state_invariants:
            action.kind = DispatchKind::request_local_model;
            action.authority_digest = policy_.scope_digest;
            action.instruction =
                "State the non-negotiable authority, lifetime, and build invariants.";
            action.operator_review_required = true;
            break;
        case Milestone::research_and_plan:
            action.kind = DispatchKind::request_local_model;
            action.authority_digest = policy_.scope_digest;
            action.instruction =
                "Produce one bounded implementation and validation plan.";
            action.operator_review_required = true;
            break;
        case Milestone::source_proposal:
            action.kind = DispatchKind::request_local_model;
            action.instruction = repair_cycle_
                ? "Produce one repaired exact-content source proposal from the "
                  "verified failure evidence without expanding approved paths."
                : "Produce one exact-content source proposal for the approved scope.";
            action.operator_review_required = true;
            action.exact_source_proposal_required = true;
            break;
        case Milestone::operator_approval:
            action.kind = DispatchKind::await_operator;
            action.authority_digest = pending_authority_digest_;
            action.instruction =
                "Review and approve or reject the exact immutable proposal digest.";
            break;
        case Milestone::implement:
            action.kind = DispatchKind::execute_source_transaction;
            action.authority_digest = approved_authority_digest_;
            action.instruction =
                "Apply the approved exact bytes through the guarded sandbox executor.";
            break;
        case Milestone::compile:
            action.kind = DispatchKind::run_compiler;
            action.authority_digest = approved_authority_digest_;
            action.instruction =
                "Run the host-owned compiler adapter in the disposable sandbox.";
            break;
        case Milestone::test:
            action.kind = DispatchKind::run_tests;
            action.authority_digest = approved_authority_digest_;
            action.instruction =
                "Run the host-owned build-safe test adapter.";
            break;
        case Milestone::static_analysis:
            action.kind = DispatchKind::run_static_analyzer;
            action.authority_digest = approved_authority_digest_;
            action.instruction =
                "Run the configured host-owned static analyzer.";
            break;
        case Milestone::sanitizer:
            action.kind = DispatchKind::run_sanitizer;
            action.authority_digest = approved_authority_digest_;
            action.instruction =
                "Run the applicable host-owned sanitizer lane.";
            break;
        case Milestone::local_self_review:
            action.kind = DispatchKind::request_local_model;
            action.authority_digest = approved_authority_digest_;
            action.instruction =
                "Review the exact sandbox diff and verified host evidence once.";
            action.operator_review_required = true;
            break;
        case Milestone::architecture_review:
            action.kind = DispatchKind::await_architecture_review;
            action.authority_digest = approved_authority_digest_;
            action.instruction = "Await independent architecture-review evidence.";
            break;
        case Milestone::visual_validation:
            action.kind = DispatchKind::run_visual_harness;
            action.authority_digest = approved_authority_digest_;
            action.instruction = "Run the approved visual evidence harness.";
            break;
        case Milestone::frontier_review:
            action.kind = DispatchKind::await_frontier_review;
            action.authority_digest = approved_authority_digest_;
            action.instruction = "Await independent frontier-review evidence.";
            break;
        case Milestone::complete:
            action.kind = DispatchKind::complete;
            action.instruction = "The bounded iteration is complete.";
            break;
        case Milestone::blocked:
            action.kind = DispatchKind::blocked;
            action.instruction = status_;
            break;
        case Milestone::cancelled:
            action.kind = DispatchKind::cancelled;
            action.instruction = status_;
            break;
        case Milestone::idle:
            action.kind = DispatchKind::none;
            action.instruction = status_;
            break;
        }
        return action;
    }

    LoopSnapshot BoundedIterationLoop::snapshot() const
    {
        return {
            .milestone = milestone_,
            .last_code = last_code_,
            .risk = policy_.risk,
            .capability = capability_,
            .model_name = model_.model_name,
            .status = status_,
            .repair_attempts = repair_attempts_,
            .maximum_repair_attempts = policy_.maximum_repair_attempts,
            .evidence_count = evidence_.size(),
            .dispatch_id = dispatch_id_,
            .active_authority_digest = approved_authority_digest_.valid()
                ? approved_authority_digest_
                : pending_authority_digest_.valid()
                    ? pending_authority_digest_
                    : policy_.scope_digest,
            .operator_approval_recorded = operator_approval_recorded_,
            .repair_cycle = repair_cycle_,
            .complete = milestone_ == Milestone::complete};
    }

    const std::vector<MilestoneEvidence>&
    BoundedIterationLoop::evidence() const noexcept
    {
        return evidence_;
    }

    Milestone BoundedIterationLoop::milestone_after(
        Milestone current) const noexcept
    {
        switch (current)
        {
        case Milestone::inspect_architecture:
            return Milestone::state_invariants;
        case Milestone::state_invariants:
            return policy_.require_research
                ? Milestone::research_and_plan
                : Milestone::source_proposal;
        case Milestone::research_and_plan:
            return Milestone::source_proposal;
        case Milestone::source_proposal:
            return Milestone::operator_approval;
        case Milestone::operator_approval: return Milestone::implement;
        case Milestone::implement: return Milestone::compile;
        case Milestone::compile: return Milestone::test;
        case Milestone::test:
            if (policy_.require_static_analysis)
                return Milestone::static_analysis;
            if (policy_.require_sanitizer)
                return Milestone::sanitizer;
            if (policy_.require_local_self_review)
                return Milestone::local_self_review;
            if (capability_.architecture_review_required)
                return Milestone::architecture_review;
            if (policy_.require_visual_validation)
                return Milestone::visual_validation;
            return capability_.frontier_review_required
                ? Milestone::frontier_review : Milestone::complete;
        case Milestone::static_analysis:
            if (policy_.require_sanitizer)
                return Milestone::sanitizer;
            if (policy_.require_local_self_review)
                return Milestone::local_self_review;
            if (capability_.architecture_review_required)
                return Milestone::architecture_review;
            if (policy_.require_visual_validation)
                return Milestone::visual_validation;
            return capability_.frontier_review_required
                ? Milestone::frontier_review : Milestone::complete;
        case Milestone::sanitizer:
            if (policy_.require_local_self_review)
                return Milestone::local_self_review;
            if (capability_.architecture_review_required)
                return Milestone::architecture_review;
            if (policy_.require_visual_validation)
                return Milestone::visual_validation;
            return capability_.frontier_review_required
                ? Milestone::frontier_review : Milestone::complete;
        case Milestone::local_self_review:
            if (capability_.architecture_review_required)
                return Milestone::architecture_review;
            if (policy_.require_visual_validation)
                return Milestone::visual_validation;
            if (capability_.frontier_review_required)
                return Milestone::frontier_review;
            return Milestone::complete;
        case Milestone::architecture_review:
            if (policy_.require_visual_validation)
                return Milestone::visual_validation;
            if (capability_.frontier_review_required)
                return Milestone::frontier_review;
            return Milestone::complete;
        case Milestone::visual_validation:
            return capability_.frontier_review_required
                ? Milestone::frontier_review
                : Milestone::complete;
        case Milestone::frontier_review: return Milestone::complete;
        case Milestone::idle:
        case Milestone::complete:
        case Milestone::blocked:
        case Milestone::cancelled:
            return current;
        }
        return Milestone::blocked;
    }

    LoopResult BoundedIterationLoop::remember(
        LoopCode code,
        std::string status)
    {
        last_code_ = code;
        status_ = std::move(status);
        return {last_code_, status_};
    }

    bool BoundedIterationLoop::repairable(Milestone milestone) const noexcept
    {
        return milestone == Milestone::source_proposal
            || milestone == Milestone::implement
            || milestone == Milestone::compile
            || milestone == Milestone::test
            || milestone == Milestone::static_analysis
            || milestone == Milestone::sanitizer
            || milestone == Milestone::local_self_review
            || milestone == Milestone::architecture_review
            || milestone == Milestone::visual_validation
            || milestone == Milestone::frontier_review;
    }

    bool BoundedIterationLoop::authority_matches(
        const MilestoneEvidence& evidence) const noexcept
    {
        switch (milestone_)
        {
        case Milestone::inspect_architecture:
        case Milestone::state_invariants:
        case Milestone::research_and_plan:
            return evidence.authority_digest == policy_.scope_digest;
        case Milestone::source_proposal:
            return evidence.authority_digest.valid();
        case Milestone::operator_approval:
            return pending_authority_digest_.valid()
                && evidence.authority_digest == pending_authority_digest_;
        case Milestone::implement:
        case Milestone::compile:
        case Milestone::test:
        case Milestone::static_analysis:
        case Milestone::sanitizer:
        case Milestone::local_self_review:
        case Milestone::architecture_review:
        case Milestone::visual_validation:
        case Milestone::frontier_review:
            return approved_authority_digest_.valid()
                && evidence.authority_digest == approved_authority_digest_;
        case Milestone::idle:
        case Milestone::complete:
        case Milestone::blocked:
        case Milestone::cancelled:
            return false;
        }
        return false;
    }

    void BoundedIterationLoop::begin_dispatch() noexcept
    {
        ++dispatch_id_;
        if (dispatch_id_ == 0u)
            ++dispatch_id_;
    }

    CandidateLineageResult SandboxCandidateLineage::configure(
        const EvidenceDigest initialSourceDigest,
        std::string initialSandboxIdentity)
    {
        if (!initialSourceDigest.valid()
            || !valid_summary(initialSandboxIdentity))
        {
            return remember(
                CandidateLineageCode::invalid_candidate,
                "A candidate lineage requires a valid source digest and bounded sandbox identity.");
        }
        generation_ = 1u;
        head_source_digest_ = initialSourceDigest;
        head_sandbox_identity_ = std::move(initialSandboxIdentity);
        candidates_.clear();
        selections_.clear();
        configured_ = true;
        return remember(
            CandidateLineageCode::none,
            "Sandbox lineage configured; live source is not a writable lineage member.");
    }

    CandidateLineageResult SandboxCandidateLineage::admit(
        SandboxCandidateEvidence candidate)
    {
        if (!configured_)
        {
            return remember(
                CandidateLineageCode::invalid_state,
                "Configure the sandbox lineage before admitting a preview candidate.");
        }
        if (candidate.generation != generation_)
        {
            return remember(
                CandidateLineageCode::stale_generation,
                "A stale candidate generation cannot enter the current comparison.");
        }
        if (candidate.parent_source_digest != head_source_digest_)
        {
            return remember(
                CandidateLineageCode::parent_mismatch,
                "The candidate was not built from the selected sandbox head.");
        }
        if (!candidate.source_tree_digest.valid()
            || candidate.source_tree_digest == head_source_digest_
            || !candidate.executable_digest.valid()
            || !candidate.validation_digest.valid()
            || !candidate.capture_digest.valid()
            || !valid_summary(candidate.sandbox_identity)
            || candidate.sandbox_identity == head_sandbox_identity_
            || candidate.platform_process_id == 0u
            || candidate.platform_window_id == 0u)
        {
            return remember(
                CandidateLineageCode::invalid_candidate,
                "A preview candidate requires distinct sandbox source, executable, validation, capture, PID, and visible-window evidence.");
        }
        const bool duplicate = std::ranges::any_of(
            candidates_,
            [&candidate](const SandboxCandidateEvidence& existing)
            {
                return existing.slot == candidate.slot
                    || existing.sandbox_identity == candidate.sandbox_identity
                    || existing.platform_process_id
                        == candidate.platform_process_id
                    || existing.platform_window_id
                        == candidate.platform_window_id;
            });
        if (duplicate)
        {
            return remember(
                CandidateLineageCode::duplicate_candidate,
                "Candidate slots, sandbox roots, processes, and preview windows must be distinct.");
        }
        candidates_.push_back(std::move(candidate));
        return remember(
            CandidateLineageCode::none,
            candidates_.size() == 2u
                ? "Candidate A and B are ready for operator comparison."
                : "The first candidate preview is ready; admit its peer before selection.");
    }

    CandidateLineageResult SandboxCandidateLineage::select(
        const CandidateChoice choice,
        const EvidenceDigest operatorReviewDigest)
    {
        if (!configured_ || candidates_.size() != 2u)
        {
            return remember(
                CandidateLineageCode::comparison_incomplete,
                "Candidate A and B must both be admitted before choosing a sandbox head.");
        }
        if (!operatorReviewDigest.valid())
        {
            return remember(
                CandidateLineageCode::invalid_candidate,
                "Candidate selection requires a non-zero operator review digest.");
        }

        const SandboxCandidateEvidence* selected{};
        if (choice != CandidateChoice::reject_both)
        {
            const CandidateSlot slot = choice == CandidateChoice::candidate_a
                ? CandidateSlot::candidate_a
                : CandidateSlot::candidate_b;
            const auto found = std::ranges::find(
                candidates_, slot, &SandboxCandidateEvidence::slot);
            if (found == candidates_.end())
            {
                return remember(
                    CandidateLineageCode::comparison_incomplete,
                    "The selected candidate slot is not available.");
            }
            selected = &*found;
        }

        CandidateLineageResult result{};
        result.retire_process_ids.reserve(candidates_.size());
        for (const auto& candidate : candidates_)
            result.retire_process_ids.push_back(candidate.platform_process_id);

        const EvidenceDigest parentDigest = head_source_digest_;
        if (selected != nullptr)
        {
            head_source_digest_ = selected->source_tree_digest;
            head_sandbox_identity_ = selected->sandbox_identity;
        }
        selections_.push_back(CandidateSelectionEvidence{
            .generation = generation_,
            .choice = choice,
            .parent_source_digest = parentDigest,
            .selected_source_digest = head_source_digest_,
            .operator_review_digest = operatorReviewDigest});
        ++generation_;
        candidates_.clear();
        last_code_ = CandidateLineageCode::none;
        status_ = selected != nullptr
            ? "The selected candidate is the next sandbox head; both preview children must now retire."
            : "Both candidates were rejected; the sandbox head is unchanged and both preview children must now retire.";
        result.status = status_;
        return result;
    }

    CandidateLineageSnapshot SandboxCandidateLineage::snapshot() const
    {
        return {
            .generation = generation_,
            .head_source_digest = head_source_digest_,
            .head_sandbox_identity = head_sandbox_identity_,
            .candidates = candidates_,
            .selection_count = selections_.size(),
            .comparison_ready = candidates_.size() == 2u};
    }

    const std::vector<CandidateSelectionEvidence>&
    SandboxCandidateLineage::selections() const noexcept
    {
        return selections_;
    }

    CandidateLineageResult SandboxCandidateLineage::remember(
        const CandidateLineageCode code,
        std::string status)
    {
        last_code_ = code;
        status_ = std::move(status);
        return {.code = last_code_, .status = status_};
    }
}
