/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

export module ai.iteration_loop;

export namespace epochengine::ai::iteration
{
    enum class RiskClass : std::uint8_t
    {
        contained_feature,
        related_files,
        subsystem,
        cross_subsystem,
        architecture_campaign,
        lifetime_critical,
        concurrency_critical,
        vulkan_synchronization
    };

    enum class Milestone : std::uint8_t
    {
        idle,
        inspect_architecture,
        state_invariants,
        research_and_plan,
        source_proposal,
        operator_approval,
        implement,
        compile,
        test,
        static_analysis,
        sanitizer,
        local_self_review,
        architecture_review,
        visual_validation,
        frontier_review,
        complete,
        blocked,
        cancelled
    };

    enum class EvidenceActor : std::uint8_t
    {
        local_model,
        source_executor,
        operator_user,
        compiler,
        test_runner,
        static_analyzer,
        sanitizer_runtime,
        architecture_reviewer,
        visual_harness,
        frontier_reviewer
    };

    enum class CapabilityDisposition : std::uint8_t
    {
        eligible,
        recommend_stronger_model,
        blocked
    };

    enum class LoopCode : std::uint8_t
    {
        none,
        invalid_policy,
        invalid_state,
        capability_blocked,
        evidence_rejected,
        stale_completion,
        authority_mismatch,
        retry_exhausted,
        cancelled
    };

    enum class DispatchKind : std::uint8_t
    {
        none,
        request_local_model,
        await_operator,
        execute_source_transaction,
        run_compiler,
        run_tests,
        run_static_analyzer,
        run_sanitizer,
        await_architecture_review,
        run_visual_harness,
        await_frontier_review,
        complete,
        blocked,
        cancelled
    };

    struct EvidenceDigest final
    {
        std::uint64_t high{};
        std::uint64_t low{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return high != 0u || low != 0u;
        }

        friend constexpr bool operator==(
            const EvidenceDigest&,
            const EvidenceDigest&) = default;
    };

    struct ModelCapabilities final
    {
        std::string model_name{};
        std::uint32_t context_tokens{};
        bool code_generation{};
        bool repository_reasoning{};
        bool tool_use{};
        bool research{};
        bool build_repair{};
        bool self_review{};
    };

    struct LoopPolicy final
    {
        RiskClass risk{RiskClass::contained_feature};
        EvidenceDigest scope_digest{};
        std::uint32_t maximum_repair_attempts{3u};
        bool require_research{true};
        bool require_static_analysis{true};
        bool require_sanitizer{true};
        bool require_visual_validation{};
    };

    struct CapabilityAssessment final
    {
        CapabilityDisposition disposition{CapabilityDisposition::blocked};
        std::string summary{};
        std::string recommended_model_class{};
        bool operator_approval_required{true};
        bool architecture_review_required{};
        bool frontier_review_required{};
    };

    struct MilestoneEvidence final
    {
        Milestone milestone{Milestone::idle};
        EvidenceActor actor{EvidenceActor::local_model};
        EvidenceDigest digest{};
        EvidenceDigest authority_digest{};
        std::uint64_t dispatch_id{};
        std::string summary{};
        bool verified{};
        bool passed{};
        bool operator_reviewed{};
    };

    struct MilestoneAction final
    {
        DispatchKind kind{DispatchKind::none};
        Milestone milestone{Milestone::idle};
        EvidenceActor actor{EvidenceActor::operator_user};
        EvidenceDigest authority_digest{};
        std::uint64_t dispatch_id{};
        std::uint32_t repair_attempt{};
        std::string instruction{};
        bool operator_review_required{};
        bool exact_source_proposal_required{};
    };

    struct LoopResult final
    {
        LoopCode code{LoopCode::none};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == LoopCode::none;
        }
    };

    struct LoopSnapshot final
    {
        Milestone milestone{Milestone::idle};
        LoopCode last_code{LoopCode::none};
        RiskClass risk{RiskClass::contained_feature};
        CapabilityAssessment capability{};
        std::string model_name{};
        std::string status{};
        std::uint32_t repair_attempts{};
        std::uint32_t maximum_repair_attempts{};
        std::size_t evidence_count{};
        std::uint64_t dispatch_id{};
        EvidenceDigest active_authority_digest{};
        bool operator_approval_recorded{};
        bool repair_cycle{};
        bool complete{};
    };

    [[nodiscard]] constexpr std::string_view name(RiskClass risk) noexcept
    {
        switch (risk)
        {
        case RiskClass::contained_feature: return "contained feature";
        case RiskClass::related_files: return "related files";
        case RiskClass::subsystem: return "large subsystem";
        case RiskClass::cross_subsystem: return "cross-subsystem";
        case RiskClass::architecture_campaign: return "architecture campaign";
        case RiskClass::lifetime_critical: return "lifetime critical";
        case RiskClass::concurrency_critical: return "concurrency critical";
        case RiskClass::vulkan_synchronization: return "Vulkan synchronization";
        }
        return "unknown";
    }

    [[nodiscard]] constexpr std::string_view name(Milestone milestone) noexcept
    {
        switch (milestone)
        {
        case Milestone::idle: return "Idle";
        case Milestone::inspect_architecture: return "Inspect Architecture";
        case Milestone::state_invariants: return "State Invariants";
        case Milestone::research_and_plan: return "Research And Plan";
        case Milestone::source_proposal: return "Source Proposal";
        case Milestone::operator_approval: return "Operator Approval";
        case Milestone::implement: return "Implement";
        case Milestone::compile: return "Compile";
        case Milestone::test: return "Test";
        case Milestone::static_analysis: return "Static Analysis";
        case Milestone::sanitizer: return "Sanitizer";
        case Milestone::local_self_review: return "Local Self Review";
        case Milestone::architecture_review: return "Architecture Review";
        case Milestone::visual_validation: return "Visual Validation";
        case Milestone::frontier_review: return "Frontier Review";
        case Milestone::complete: return "Complete";
        case Milestone::blocked: return "Blocked";
        case Milestone::cancelled: return "Cancelled";
        }
        return "Unknown";
    }

    [[nodiscard]] constexpr std::string_view name(
        EvidenceActor actor) noexcept
    {
        switch (actor)
        {
        case EvidenceActor::local_model: return "local model";
        case EvidenceActor::source_executor: return "source executor";
        case EvidenceActor::operator_user: return "operator";
        case EvidenceActor::compiler: return "compiler";
        case EvidenceActor::test_runner: return "test runner";
        case EvidenceActor::static_analyzer: return "static analyzer";
        case EvidenceActor::sanitizer_runtime: return "sanitizer runtime";
        case EvidenceActor::architecture_reviewer:
            return "architecture reviewer";
        case EvidenceActor::visual_harness: return "visual harness";
        case EvidenceActor::frontier_reviewer: return "frontier reviewer";
        }
        return "unknown";
    }

    [[nodiscard]] ModelCapabilities infer_model_capabilities(
        std::string_view model_name);

    [[nodiscard]] CapabilityAssessment assess_capabilities(
        const ModelCapabilities& model,
        const LoopPolicy& policy);

    [[nodiscard]] EvidenceActor required_actor(Milestone milestone) noexcept;
    [[nodiscard]] constexpr std::string_view name(
        DispatchKind kind) noexcept
    {
        switch (kind)
        {
        case DispatchKind::none: return "none";
        case DispatchKind::request_local_model: return "request local model";
        case DispatchKind::await_operator: return "await operator";
        case DispatchKind::execute_source_transaction:
            return "execute source transaction";
        case DispatchKind::run_compiler: return "run compiler";
        case DispatchKind::run_tests: return "run tests";
        case DispatchKind::run_static_analyzer: return "run static analyzer";
        case DispatchKind::run_sanitizer: return "run sanitizer";
        case DispatchKind::await_architecture_review:
            return "await architecture review";
        case DispatchKind::run_visual_harness: return "run visual harness";
        case DispatchKind::await_frontier_review:
            return "await frontier review";
        case DispatchKind::complete: return "complete";
        case DispatchKind::blocked: return "blocked";
        case DispatchKind::cancelled: return "cancelled";
        }
        return "unknown";
    }

    class BoundedIterationLoop final
    {
    public:
        [[nodiscard]] LoopResult configure(
            LoopPolicy policy,
            ModelCapabilities model);
        [[nodiscard]] LoopResult begin();
        [[nodiscard]] LoopResult record(MilestoneEvidence evidence);
        [[nodiscard]] LoopResult cancel(std::string reason);
        [[nodiscard]] MilestoneAction next_action() const;
        [[nodiscard]] LoopSnapshot snapshot() const;
        [[nodiscard]] const std::vector<MilestoneEvidence>& evidence() const noexcept;

    private:
        [[nodiscard]] Milestone milestone_after(Milestone current) const noexcept;
        [[nodiscard]] LoopResult remember(LoopCode code, std::string status);
        [[nodiscard]] bool repairable(Milestone milestone) const noexcept;
        [[nodiscard]] bool authority_matches(
            const MilestoneEvidence& evidence) const noexcept;
        void begin_dispatch() noexcept;

        LoopPolicy policy_{};
        ModelCapabilities model_{};
        CapabilityAssessment capability_{};
        Milestone milestone_{Milestone::idle};
        LoopCode last_code_{LoopCode::none};
        std::string status_{"Configure a bounded iteration before starting."};
        std::vector<MilestoneEvidence> evidence_{};
        EvidenceDigest pending_authority_digest_{};
        EvidenceDigest approved_authority_digest_{};
        std::uint64_t dispatch_id_{};
        std::uint32_t repair_attempts_{};
        bool configured_{};
        bool operator_approval_recorded_{};
        bool repair_cycle_{};
    };

    [[nodiscard]] bool run_contract();
}
