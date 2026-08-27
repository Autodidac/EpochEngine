/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cctype>
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
        [[nodiscard]] std::string lowercase(std::string_view value)
        {
            std::string result{value};
            std::transform(result.begin(), result.end(), result.begin(),
                [](char character)
                {
                    return static_cast<char>(std::tolower(
                        static_cast<unsigned char>(character)));
                });
            return result;
        }

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
        if (model_name.empty())
            return result;

        const std::string normalized = lowercase(model_name);
        const bool qwen38 = normalized.find("qwen3.8") != std::string::npos
            || normalized.find("qwen-3.8") != std::string::npos
            || normalized.find("qwen_3.8") != std::string::npos;
        const bool heavy_qwen = normalized.find("qwen") != std::string::npos
            && (qwen38
                || normalized.find("27b") != std::string::npos
                || normalized.find("30b") != std::string::npos
                || normalized.find("32b") != std::string::npos
                || normalized.find("70b") != std::string::npos
                || normalized.find("72b") != std::string::npos);
        const bool coding_model = heavy_qwen
            || normalized.find("coder") != std::string::npos
            || normalized.find("devstral") != std::string::npos
            || normalized.find("codestral") != std::string::npos;

        result.context_tokens = heavy_qwen ? 65'536u
            : coding_model ? 32'768u : 16'384u;
        result.code_generation = true;
        result.tool_use = true;
        result.repository_reasoning = coding_model;
        result.research = coding_model;
        result.build_repair = coding_model;
        result.self_review = coding_model;
        return result;
    }

    CapabilityAssessment assess_capabilities(
        const ModelCapabilities& model,
        const LoopPolicy& policy)
    {
        CapabilityAssessment result{
            .disposition = CapabilityDisposition::eligible,
            .summary = "The selected model fits the bounded writer lane.",
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

        const bool common = model.code_generation && model.tool_use
            && model.context_tokens >= minimum_context(policy.risk);
        const bool repository = policy.risk == RiskClass::contained_feature
            || (model.repository_reasoning && model.build_repair);
        const bool planning = !policy.require_research || model.research;
        const bool review = model.self_review;
        if (!common || !repository || !planning || !review)
        {
            result.disposition =
                CapabilityDisposition::recommend_stronger_model;
            result.summary =
                "The selected model may assist, but it is not admitted to the "
                "requested automated writer lane.";
            result.recommended_model_class = policy.risk
                    == RiskClass::contained_feature
                ? "Use a coding model with tool use, self-review, and at least "
                  "8K context."
                : policy.risk == RiskClass::related_files
                    ? "Use a 27B-class coding model with repository reasoning, "
                      "build repair, and at least 16K context."
                    : "Use a strong 27B-70B coding model with repository "
                      "reasoning, research, build repair, self-review, and at "
                      "least 64K context.";
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
        if (capability_.disposition != CapabilityDisposition::eligible)
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
            return Milestone::local_self_review;
        case Milestone::static_analysis:
            return policy_.require_sanitizer
                ? Milestone::sanitizer
                : Milestone::local_self_review;
        case Milestone::sanitizer:
            return Milestone::local_self_review;
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
}
