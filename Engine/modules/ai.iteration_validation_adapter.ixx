/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module ai.iteration_validation_adapter;

export import ai.self_iteration_orchestrator;
export import ai.source_patch_bundle;
export import epoch.build_validation;

export namespace epochengine::ai::iteration_validation_adapter
{
    inline constexpr std::uint32_t schema_version = 1u;
    inline constexpr std::size_t engine_stage_count = 7u;
    inline constexpr std::size_t maximum_failure_diagnostic_bytes =
        16u * 1024u;

    struct Policy final
    {
        std::uint32_t maximum_retries_per_stage{1u};
        std::uint32_t maximum_parallel_tasks{1u};
        bool accept_parallel_results{};
        build_validation::AdmissionPolicy admission{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_retries_per_stage <= 3u
                && maximum_parallel_tasks > 0u
                && maximum_parallel_tasks <= engine_stage_count
                && (accept_parallel_results || maximum_parallel_tasks == 1u);
        }
    };

    struct Authority final
    {
        std::string authority_sha256{};
        std::string bundle_sha256{};
        std::string candidate_sha256{};
        std::string source_commit{};
        std::string source_tree_sha256{};
        build_validation::SemanticVersion source_version{};
        build_validation::SemanticVersion packaged_version{};
        build_validation::Platform platform{build_validation::Platform::invalid};
        build_validation::Compiler compiler{build_validation::Compiler::invalid};
        std::string toolchain{};
        std::string toolchain_sha256{};
        std::string configuration_sha256{};
    };

    enum class TaskState : std::uint8_t
    {
        ready,
        dispatched,
        result_ready,
        evidence_recorded,
        retry_ready,
        repair_ready,
        repair_evidence_recorded,
        failed,
        cancelled
    };

    struct Task final
    {
        std::string task_id{};
        std::uint32_t stage_index{};
        std::uint32_t attempt{};
        iteration_session::ValidationActor actor{
            iteration_session::ValidationActor::debug_compiler};
        build_validation::CheckLane required_lane{
            build_validation::CheckLane::invalid};
        build_validation::Configuration configuration{
            build_validation::Configuration::invalid};
        std::string target{};
        std::string campaign_id{};
        std::uint64_t campaign_generation{};
        iteration_session::RequestIdentity session_identity{};
        std::uint64_t base_orchestrator_generation{};
        std::string base_state_sha256{};
        std::uint64_t adapter_generation{};
        std::string adapter_state_sha256{};
        Authority authority{};
        std::string task_sha256{};
    };

    struct TaskResult final
    {
        std::string task_id{};
        std::uint32_t stage_index{};
        std::uint32_t attempt{};
        std::string campaign_id{};
        std::uint64_t campaign_generation{};
        iteration_session::RequestIdentity session_identity{};
        std::uint64_t adapter_generation{};
        std::string adapter_state_sha256{};
        std::string task_sha256{};
        std::string authority_sha256{};
        std::string bundle_sha256{};
        std::string candidate_sha256{};
        std::string toolchain_sha256{};
        std::string configuration_sha256{};
        build_validation::ValidationReceipt receipt{};
        std::string receipt_sha256{};
        std::string diagnostic{};
        bool host_completed{};
    };

    class TaskHost
    {
    public:
        virtual ~TaskHost() = default;
        [[nodiscard]] virtual bool enqueue(const Task& task) = 0;
        virtual void cancel(std::string_view task_id) noexcept = 0;
    };

    enum class FailureKind : std::uint8_t
    {
        none,
        required_check,
        admission_policy
    };

    struct StageRecord final
    {
        Task task{};
        TaskState state{TaskState::ready};
        std::string receipt_sha256{};
        std::string evidence_sha256{};
        std::string diagnostic{};
        std::optional<build_validation::ValidationReceipt> receipt{};
        FailureKind failure_kind{FailureKind::none};
    };

    struct AdmissionEvidence final
    {
        std::string schema{"epoch.iteration-validation-admission/v1"};
        std::string campaign_id{};
        iteration_session::RequestIdentity session_identity{};
        std::string candidate_sha256{};
        std::string bundle_sha256{};
        std::string authority_sha256{};
        std::string source_commit{};
        std::string source_tree_sha256{};
        std::string toolchain_sha256{};
        std::string configuration_sha256{};
        std::vector<std::string> receipt_sha256s{};
        std::string canonical_json{};
        std::string evidence_sha256{};
        bool admitted{};
        bool upload_permitted{};
        bool release_permitted{};
    };

    struct Snapshot final
    {
        std::uint64_t generation{};
        std::string previous_state_sha256{};
        std::string state_sha256{};
        std::string orchestrator_id{};
        std::string campaign_id{};
        std::uint64_t campaign_generation{};
        iteration_session::RequestIdentity session_identity{};
        std::uint64_t base_orchestrator_generation{};
        std::string base_state_sha256{};
        Authority authority{};
        Policy policy{};
        std::vector<StageRecord> stages{};
        std::uint32_t next_evidence_index{};
        std::optional<AdmissionEvidence> admission{};
        std::string status{};
        bool cancelled{};
        bool upload_permitted{};
        bool release_permitted{};
    };

    class JournalPort
    {
    public:
        virtual ~JournalPort() = default;
        [[nodiscard]] virtual std::optional<Snapshot> load(
            std::string_view campaign_id) = 0;
        [[nodiscard]] virtual bool store_atomic(const Snapshot& snapshot) = 0;
    };

    struct HostEvidenceResult final
    {
        bool accepted{};
        self_iteration_orchestrator::Snapshot snapshot{};
        std::string status{};
    };

    class OrchestratorPort
    {
    public:
        virtual ~OrchestratorPort() = default;
        [[nodiscard]] virtual HostEvidenceResult record_validation(
            iteration_session::ValidationActor actor,
            std::string evidence_sha256,
            std::string summary,
            bool passed,
            self_iteration_orchestrator::ActionToken action) = 0;
    };

    class DirectOrchestratorPort final : public OrchestratorPort
    {
    public:
        explicit DirectOrchestratorPort(
            self_iteration_orchestrator::Orchestrator& orchestrator) noexcept;
        [[nodiscard]] HostEvidenceResult record_validation(
            iteration_session::ValidationActor actor,
            std::string evidence_sha256,
            std::string summary,
            bool passed,
            self_iteration_orchestrator::ActionToken action) override;

    private:
        self_iteration_orchestrator::Orchestrator* orchestrator_{};
    };

    enum class Code : std::uint8_t
    {
        ready,
        dispatched,
        result_accepted,
        retry_scheduled,
        evidence_recorded,
        admitted,
        repair_ready,
        repair_required,
        cancelled,
        invalid_policy,
        stale_state,
        replay_rejected,
        out_of_order,
        forged_result,
        failed_validation,
        budget_exhausted,
        journal_failure,
        host_failure
    };

    struct Result final
    {
        Code code{Code::stale_state};
        Snapshot snapshot{};
        std::vector<Task> tasks{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready || code == Code::dispatched
                || code == Code::result_accepted
                || code == Code::retry_scheduled
                || code == Code::evidence_recorded
                || code == Code::admitted || code == Code::repair_ready
                || code == Code::repair_required || code == Code::cancelled;
        }
    };

    class Adapter final
    {
    public:
        [[nodiscard]] Result begin(
            const self_iteration_orchestrator::Snapshot& orchestrator,
            Authority authority,
            Policy policy,
            JournalPort& journal);
        [[nodiscard]] Result resume(
            const self_iteration_orchestrator::Snapshot& orchestrator,
            Authority authority,
            Policy policy,
            JournalPort& journal);
        [[nodiscard]] Result dispatch(TaskHost& host, JournalPort& journal);
        [[nodiscard]] Result record_result(
            TaskResult result,
            JournalPort& journal);
        [[nodiscard]] Result advance_orchestrator(
            const self_iteration_orchestrator::Snapshot& orchestrator,
            self_iteration_orchestrator::ActionToken action,
            OrchestratorPort& port,
            JournalPort& journal);
        [[nodiscard]] Result cancel(
            std::string reason,
            TaskHost& host,
            JournalPort& journal);
        [[nodiscard]] Snapshot snapshot() const;

    private:
        [[nodiscard]] Result persist(Code code, std::string status, JournalPort& journal);
        [[nodiscard]] bool binding_matches(
            const self_iteration_orchestrator::Snapshot& orchestrator,
            const Authority& authority,
            const Policy& policy) const;

        Snapshot snapshot_{};
        bool configured_{};
    };

    [[nodiscard]] bool run_contract();
}
