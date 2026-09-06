/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <cstdint>
#include <string>
#include <vector>

module ai.iteration_loop;

namespace epochengine::ai::iteration
{
    namespace
    {
        [[nodiscard]] EvidenceDigest digest(std::uint64_t value) noexcept
        {
            return {0x45504f43484c4f4full, value == 0u ? 1u : value};
        }

        [[nodiscard]] bool scope_milestone(Milestone milestone) noexcept
        {
            return milestone == Milestone::inspect_architecture
                || milestone == Milestone::state_invariants
                || milestone == Milestone::research_and_plan;
        }

        [[nodiscard]] MilestoneEvidence passing(
            const BoundedIterationLoop& loop,
            Milestone milestone,
            std::uint64_t ordinal,
            EvidenceDigest scope,
            EvidenceDigest proposal)
        {
            const EvidenceDigest authority = scope_milestone(milestone)
                ? scope : proposal;
            return {
                .milestone = milestone,
                .actor = required_actor(milestone),
                .digest = milestone == Milestone::source_proposal
                    ? authority : digest(ordinal),
                .authority_digest = authority,
                .dispatch_id = loop.next_action().dispatch_id,
                .summary = "Verified contract evidence.",
                .verified = true,
                .passed = true,
                .operator_reviewed = true};
        }

        [[nodiscard]] LoopPolicy policy_with_scope(
            EvidenceDigest scope,
            RiskClass risk = RiskClass::contained_feature)
        {
            LoopPolicy policy{};
            policy.risk = risk;
            policy.scope_digest = scope;
            return policy;
        }

        [[nodiscard]] ModelCapabilities strong_model()
        {
            return {
                .model_name = "Qwen3.8",
                .context_tokens = 65'536u,
                .code_generation = true,
                .repository_reasoning = true,
                .tool_use = true,
                .research = true,
                .build_repair = true,
                .self_review = true};
        }

        [[nodiscard]] bool contained_flow_contract(bool unknownModel = false)
        {
            const EvidenceDigest scope = digest(1'000u);
            const EvidenceDigest proposal = digest(1'001u);
            BoundedIterationLoop loop{};
            if (!loop.configure(policy_with_scope(scope), unknownModel
                    ? infer_model_capabilities("operator/future-agentic-model")
                    : strong_model())
                || !loop.begin())
            {
                return false;
            }
            constexpr std::array sequence{
                Milestone::inspect_architecture,
                Milestone::state_invariants,
                Milestone::research_and_plan,
                Milestone::source_proposal,
                Milestone::operator_approval,
                Milestone::implement,
                Milestone::compile,
                Milestone::test,
                Milestone::static_analysis,
                Milestone::sanitizer,
                Milestone::local_self_review};
            std::uint64_t ordinal = 1u;
            for (const Milestone milestone : sequence)
            {
                if (loop.snapshot().milestone != milestone
                    || !loop.record(passing(
                        loop, milestone, ordinal++, scope, proposal)))
                {
                    return false;
                }
            }
            const LoopSnapshot result = loop.snapshot();
            return result.complete
                && result.operator_approval_recorded
                && result.active_authority_digest == proposal
                && result.evidence_count == sequence.size()
                && loop.next_action().kind == DispatchKind::complete;
        }

        [[nodiscard]] bool actor_and_order_contract()
        {
            const EvidenceDigest scope = digest(1'100u);
            const EvidenceDigest proposal = digest(1'101u);
            BoundedIterationLoop loop{};
            if (!loop.configure(policy_with_scope(scope), strong_model())
                || !loop.begin())
            {
                return false;
            }
            MilestoneEvidence wrong = passing(
                loop, Milestone::compile, 1u, scope, proposal);
            if (loop.record(std::move(wrong)).code
                    != LoopCode::evidence_rejected
                || loop.snapshot().milestone
                    != Milestone::inspect_architecture)
            {
                return false;
            }
            wrong = passing(
                loop, Milestone::inspect_architecture, 2u, scope, proposal);
            wrong.actor = EvidenceActor::compiler;
            return loop.record(std::move(wrong)).code
                    == LoopCode::evidence_rejected
                && loop.snapshot().evidence_count == 0u;
        }

        [[nodiscard]] bool repair_budget_contract()
        {
            const EvidenceDigest scope = digest(1'200u);
            const EvidenceDigest firstProposal = digest(1'201u);
            const EvidenceDigest secondProposal = digest(1'202u);
            const EvidenceDigest thirdProposal = digest(1'203u);
            BoundedIterationLoop loop{};
            LoopPolicy policy = policy_with_scope(scope);
            policy.maximum_repair_attempts = 2u;
            policy.require_research = false;
            policy.require_static_analysis = false;
            policy.require_sanitizer = false;
            if (!loop.configure(policy, strong_model()) || !loop.begin())
                return false;
            for (const Milestone milestone : {
                    Milestone::inspect_architecture,
                    Milestone::state_invariants,
                    Milestone::source_proposal,
                    Milestone::operator_approval,
                    Milestone::implement})
            {
                if (!loop.record(passing(
                    loop,
                    milestone,
                    static_cast<std::uint64_t>(milestone) + 1u,
                    scope,
                    firstProposal)))
                {
                    return false;
                }
            }

            auto failure = passing(
                loop, Milestone::compile, 20u, scope, firstProposal);
            failure.passed = false;
            const std::uint64_t staleDispatch = failure.dispatch_id;
            if (!loop.record(failure)
                || loop.snapshot().milestone != Milestone::source_proposal
                || loop.snapshot().repair_attempts != 1u
                || !loop.snapshot().repair_cycle)
            {
                return false;
            }
            failure.dispatch_id = staleDispatch;
            if (loop.record(failure).code != LoopCode::stale_completion)
                return false;

            for (const Milestone milestone : {
                    Milestone::source_proposal,
                    Milestone::operator_approval,
                    Milestone::implement})
            {
                if (!loop.record(passing(
                    loop, milestone, 30u
                        + static_cast<std::uint64_t>(milestone),
                    scope, secondProposal)))
                {
                    return false;
                }
            }
            failure = passing(
                loop, Milestone::compile, 40u, scope, secondProposal);
            failure.passed = false;
            if (!loop.record(failure)
                || loop.snapshot().milestone != Milestone::source_proposal
                || loop.snapshot().repair_attempts != 2u)
            {
                return false;
            }

            for (const Milestone milestone : {
                    Milestone::source_proposal,
                    Milestone::operator_approval,
                    Milestone::implement})
            {
                if (!loop.record(passing(
                    loop, milestone, 50u
                        + static_cast<std::uint64_t>(milestone),
                    scope, thirdProposal)))
                {
                    return false;
                }
            }
            failure = passing(
                loop, Milestone::compile, 60u, scope, thirdProposal);
            failure.passed = false;
            return loop.record(failure).code == LoopCode::retry_exhausted
                && loop.snapshot().milestone == Milestone::blocked
                && loop.next_action().kind == DispatchKind::blocked;
        }

        [[nodiscard]] bool high_risk_review_contract()
        {
            const EvidenceDigest scope = digest(1'300u);
            const EvidenceDigest proposal = digest(1'301u);
            LoopPolicy policy =
                policy_with_scope(scope, RiskClass::vulkan_synchronization);
            policy.require_research = false;
            policy.require_static_analysis = false;
            policy.require_sanitizer = false;
            BoundedIterationLoop loop{};
            if (!loop.configure(policy, strong_model()) || !loop.begin())
                return false;
            constexpr std::array sequence{
                Milestone::inspect_architecture,
                Milestone::state_invariants,
                Milestone::source_proposal,
                Milestone::operator_approval,
                Milestone::implement,
                Milestone::compile,
                Milestone::test,
                Milestone::local_self_review,
                Milestone::architecture_review,
                Milestone::frontier_review};
            std::uint64_t ordinal = 70u;
            for (const Milestone milestone : sequence)
            {
                if (loop.snapshot().milestone != milestone
                    || !loop.record(passing(
                        loop, milestone, ordinal++, scope, proposal)))
                {
                    return false;
                }
            }
            return loop.snapshot().complete
                && loop.snapshot().capability.frontier_review_required;
        }

        [[nodiscard]] bool model_guidance_contract()
        {
            LoopPolicy policy =
                policy_with_scope(digest(1'400u), RiskClass::subsystem);
            ModelCapabilities weak = infer_model_capabilities("generic-7b");
            BoundedIterationLoop loop{};
            if (!loop.configure(policy, std::move(weak)))
                return false;
            return loop.begin().code == LoopCode::none
                && loop.snapshot().milestone == Milestone::inspect_architecture
                && loop.snapshot().capability.may_attempt()
                && loop.snapshot().capability.disposition
                    == CapabilityDisposition::recommend_stronger_model
                && !loop.snapshot().complete;
        }

        [[nodiscard]] bool visual_gate_contract()
        {
            const EvidenceDigest scope = digest(1'500u);
            const EvidenceDigest proposal = digest(1'501u);
            LoopPolicy policy = policy_with_scope(scope);
            policy.require_research = false;
            policy.require_static_analysis = false;
            policy.require_sanitizer = false;
            policy.require_visual_validation = true;
            BoundedIterationLoop loop{};
            if (!loop.configure(policy, strong_model()) || !loop.begin())
                return false;
            constexpr std::array before_visual{
                Milestone::inspect_architecture,
                Milestone::state_invariants,
                Milestone::source_proposal,
                Milestone::operator_approval,
                Milestone::implement,
                Milestone::compile,
                Milestone::test,
                Milestone::local_self_review};
            std::uint64_t ordinal = 90u;
            for (const Milestone milestone : before_visual)
            {
                if (!loop.record(passing(
                    loop, milestone, ordinal++, scope, proposal)))
                {
                    return false;
                }
            }
            return loop.snapshot().milestone == Milestone::visual_validation
                && loop.next_action().kind == DispatchKind::run_visual_harness
                && !loop.snapshot().complete;
        }

        [[nodiscard]] bool authority_binding_contract()
        {
            const EvidenceDigest scope = digest(1'600u);
            const EvidenceDigest proposal = digest(1'601u);
            const EvidenceDigest wrongAuthority = digest(1'602u);
            BoundedIterationLoop loop{};
            if (!loop.configure(policy_with_scope(scope), strong_model())
                || !loop.begin())
            {
                return false;
            }

            auto wrong = passing(
                loop, Milestone::inspect_architecture, 100u, scope, proposal);
            wrong.authority_digest = wrongAuthority;
            if (loop.record(wrong).code != LoopCode::authority_mismatch
                || loop.snapshot().evidence_count != 0u)
            {
                return false;
            }

            for (const Milestone milestone : {
                    Milestone::inspect_architecture,
                    Milestone::state_invariants,
                    Milestone::research_and_plan,
                    Milestone::source_proposal})
            {
                if (!loop.record(passing(
                    loop, milestone, 110u
                        + static_cast<std::uint64_t>(milestone),
                    scope, proposal)))
                {
                    return false;
                }
            }
            wrong = passing(
                loop, Milestone::operator_approval, 120u, scope, proposal);
            wrong.authority_digest = wrongAuthority;
            return loop.record(wrong).code == LoopCode::authority_mismatch
                && loop.snapshot().milestone == Milestone::operator_approval;
        }

        [[nodiscard]] bool selected_identity_admission_contract()
        {
            const LoopPolicy policy =
                policy_with_scope(digest(1'700u), RiskClass::subsystem);
            for (const auto name : {"Qwen3.8", "qwen/qwen3.5",
                    "operator/future-agentic-model", "nvidia/nemotron-3-nano-4b"})
            {
                const ModelCapabilities model = infer_model_capabilities(name);
                const auto assessed = assess_capabilities(model, policy);
                if (model.model_name != name || model.context_tokens != 0u
                    || model.code_generation || model.repository_reasoning
                    || model.tool_use || model.research || model.build_repair
                    || model.self_review || !assessed.may_attempt()
                    || assessed.disposition != CapabilityDisposition::recommend_stronger_model)
                    return false;
            }
            return true;
        }

        [[nodiscard]] bool host_budget_admission_contract()
        {
            auto policy = policy_with_scope(digest(1'710u), RiskClass::related_files);
            BoundedIterationLoop missingModel{};
            if (!missingModel.configure(policy, infer_model_capabilities({}))
                || missingModel.begin().code != LoopCode::capability_blocked
                || missingModel.snapshot().capability.may_attempt())
                return false;
            for (const auto budget : {0u, 8'192u})
            {
                policy.host_context_budget_tokens = budget;
                BoundedIterationLoop insufficient{};
                if (!insufficient.configure(policy, strong_model())
                    || insufficient.begin().code != LoopCode::capability_blocked
                    || insufficient.snapshot().capability.may_attempt())
                    return false;
            }
            return contained_flow_contract(true);
        }

        [[nodiscard]] SandboxCandidateEvidence candidate(
            const CandidateSlot slot,
            const std::uint64_t generation,
            const EvidenceDigest parent,
            const std::uint64_t ordinal)
        {
            return {
                .slot = slot,
                .generation = generation,
                .parent_source_digest = parent,
                .source_tree_digest = digest(2'000u + ordinal),
                .executable_digest = digest(3'000u + ordinal),
                .validation_digest = digest(4'000u + ordinal),
                .capture_digest = digest(5'000u + ordinal),
                .sandbox_identity = slot == CandidateSlot::candidate_a
                    ? "session_1/candidate_a"
                    : "session_1/candidate_b",
                .platform_process_id = 10'000u + ordinal,
                .platform_window_id = 20'000u + ordinal};
        }

        [[nodiscard]] bool sandbox_candidate_lineage_contract()
        {
            const EvidenceDigest initial = digest(1'900u);
            SandboxCandidateLineage lineage{};
            if (!lineage.configure(initial, "session_0/head"))
                return false;

            SandboxCandidateEvidence candidateA = candidate(
                CandidateSlot::candidate_a, 1u, initial, 1u);
            SandboxCandidateEvidence candidateB = candidate(
                CandidateSlot::candidate_b, 1u, initial, 2u);
            const EvidenceDigest selectedA = candidateA.source_tree_digest;
            if (!lineage.admit(candidateA)
                || lineage.snapshot().comparison_ready
                || !lineage.admit(candidateB)
                || !lineage.snapshot().comparison_ready)
            {
                return false;
            }

            SandboxCandidateEvidence duplicate = candidateB;
            duplicate.slot = CandidateSlot::candidate_a;
            if (lineage.admit(std::move(duplicate)).code
                != CandidateLineageCode::duplicate_candidate)
            {
                return false;
            }

            const CandidateLineageResult selected = lineage.select(
                CandidateChoice::candidate_a,
                digest(6'001u));
            const CandidateLineageSnapshot advanced = lineage.snapshot();
            if (!selected
                || selected.retire_process_ids
                    != std::vector<std::uint64_t>{10'001u, 10'002u}
                || advanced.generation != 2u
                || advanced.head_source_digest != selectedA
                || advanced.head_sandbox_identity != "session_1/candidate_a"
                || !advanced.candidates.empty()
                || advanced.selection_count != 1u)
            {
                return false;
            }

            SandboxCandidateEvidence stale = candidate(
                CandidateSlot::candidate_a, 1u, initial, 3u);
            if (lineage.admit(std::move(stale)).code
                != CandidateLineageCode::stale_generation)
            {
                return false;
            }
            SandboxCandidateEvidence wrongParent = candidate(
                CandidateSlot::candidate_a, 2u, initial, 4u);
            if (lineage.admit(std::move(wrongParent)).code
                != CandidateLineageCode::parent_mismatch)
            {
                return false;
            }

            SandboxCandidateEvidence nextA = candidate(
                CandidateSlot::candidate_a, 2u, selectedA, 5u);
            nextA.sandbox_identity = "session_2/candidate_a";
            SandboxCandidateEvidence nextB = candidate(
                CandidateSlot::candidate_b, 2u, selectedA, 6u);
            nextB.sandbox_identity = "session_2/candidate_b";
            if (!lineage.admit(std::move(nextA))
                || !lineage.admit(std::move(nextB))
                || !lineage.select(
                    CandidateChoice::reject_both,
                    digest(6'002u)))
            {
                return false;
            }
            const CandidateLineageSnapshot rejected = lineage.snapshot();
            return rejected.generation == 3u
                && rejected.head_source_digest == selectedA
                && rejected.head_sandbox_identity == "session_1/candidate_a"
                && rejected.selection_count == 2u
                && lineage.selections().back().choice
                    == CandidateChoice::reject_both;
        }

        [[nodiscard]] int failure_code()
        {
            if (!contained_flow_contract()) return 1;
            if (!actor_and_order_contract()) return 2;
            if (!repair_budget_contract()) return 3;
            if (!high_risk_review_contract()) return 4;
            if (!model_guidance_contract()) return 5;
            if (!visual_gate_contract()) return 6;
            if (!authority_binding_contract()) return 7;
            if (!selected_identity_admission_contract()) return 8;
            if (!sandbox_candidate_lineage_contract()) return 9;
            if (!host_budget_admission_contract()) return 10;
            return 0;
        }
    }

    bool run_contract()
    {
        return failure_code() == 0;
    }
}

#if defined(EPOCH_AI_ITERATION_LOOP_CONTRACT_MAIN)
int main()
{
    return epochengine::ai::iteration::failure_code();
}
#endif
