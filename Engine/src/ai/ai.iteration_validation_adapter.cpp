/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.iteration_validation_adapter;

namespace epochengine::ai::iteration_validation_adapter
{
    namespace
    {
        using Actor = iteration_session::ValidationActor;
        using Lane = build_validation::CheckLane;
        using Configuration = build_validation::Configuration;

        constexpr std::array<Actor, engine_stage_count> actors{
            Actor::debug_compiler, Actor::debug_contract,
            Actor::release_compiler, Actor::release_contract,
            Actor::headless_compiler, Actor::headless_contract,
            Actor::full_validation};

        [[nodiscard]] bool lower_hex(std::string_view value, std::size_t size) noexcept
        {
            return build_validation::valid_lower_hex(value, size);
        }

        [[nodiscard]] std::string hash(std::string_view value)
        {
            return source_patch_bundle::digest_hex(
                source_patch_bundle::digest(value));
        }

        [[nodiscard]] Lane lane_for(Actor actor) noexcept
        {
            switch (actor)
            {
            case Actor::debug_compiler:
            case Actor::release_compiler:
            case Actor::headless_compiler: return Lane::compile;
            case Actor::debug_contract:
            case Actor::release_contract: return Lane::engine_contract;
            case Actor::headless_contract: return Lane::headless_ci;
            case Actor::full_validation: return Lane::package_inventory;
            }
            return Lane::invalid;
        }

        [[nodiscard]] Configuration configuration_for(Actor actor) noexcept
        {
            return actor == Actor::release_compiler
                || actor == Actor::release_contract
                || actor == Actor::full_validation
                ? Configuration::release : Configuration::debug;
        }

        [[nodiscard]] std::string target_for(Actor actor)
        {
            return actor == Actor::headless_compiler
                || actor == Actor::headless_contract
                ? "HeadlessCI" : "EpochEditor";
        }

        [[nodiscard]] std::string authority_material(const Authority& authority)
        {
            return authority.authority_sha256 + "\n" + authority.bundle_sha256
                + "\n" + authority.candidate_sha256 + "\n"
                + authority.source_commit + "\n" + authority.source_tree_sha256
                + "\n" + std::to_string(authority.source_version.major) + "."
                + std::to_string(authority.source_version.minor) + "."
                + std::to_string(authority.source_version.revision) + "\n"
                + std::to_string(authority.packaged_version.major) + "."
                + std::to_string(authority.packaged_version.minor) + "."
                + std::to_string(authority.packaged_version.revision) + "\n"
                + std::to_string(static_cast<std::uint8_t>(authority.platform))
                + "\n" + std::to_string(static_cast<std::uint8_t>(authority.compiler))
                + "\n" + authority.toolchain + "\n"
                + authority.toolchain_sha256 + "\n"
                + authority.configuration_sha256;
        }

        [[nodiscard]] bool valid_authority(const Authority& value)
        {
            return lower_hex(value.authority_sha256, 32u)
                && lower_hex(value.bundle_sha256, 32u)
                && lower_hex(value.candidate_sha256, 32u)
                && lower_hex(value.source_commit, 20u)
                && lower_hex(value.source_tree_sha256, 32u)
                && value.source_version.valid() && value.packaged_version.valid()
                && value.platform != build_validation::Platform::invalid
                && value.compiler != build_validation::Compiler::invalid
                && build_validation::valid_identifier(value.toolchain)
                && lower_hex(value.toolchain_sha256, 32u)
                && lower_hex(value.configuration_sha256, 32u);
        }

        [[nodiscard]] std::string task_material(const Task& task)
        {
            return "EPOCH_ITERATION_VALIDATION_TASK_V1\n" + task.task_id + "\n"
                + std::to_string(task.stage_index) + "\n"
                + std::to_string(task.attempt) + "\n"
                + std::to_string(static_cast<std::uint8_t>(task.actor)) + "\n"
                + std::to_string(static_cast<std::uint8_t>(task.required_lane)) + "\n"
                + std::to_string(static_cast<std::uint8_t>(task.configuration)) + "\n"
                + task.target + "\n" + task.campaign_id + "\n"
                + std::to_string(task.campaign_generation) + "\n"
                + std::to_string(task.session_identity.session_id) + "\n"
                + std::to_string(task.session_identity.request_id) + "\n"
                + std::to_string(task.base_orchestrator_generation) + "\n"
                + task.base_state_sha256 + "\n"
                + std::to_string(task.adapter_generation) + "\n"
                + task.adapter_state_sha256 + "\n"
                + authority_material(task.authority);
        }

        [[nodiscard]] std::string snapshot_material(const Snapshot& value)
        {
            std::string material = "EPOCH_ITERATION_VALIDATION_STATE_V1\n"
                + std::to_string(value.generation) + "\n"
                + value.previous_state_sha256 + "\n" + value.orchestrator_id
                + "\n" + value.campaign_id + "\n"
                + std::to_string(value.campaign_generation) + "\n"
                + std::to_string(value.session_identity.session_id) + "\n"
                + std::to_string(value.session_identity.request_id) + "\n"
                + std::to_string(value.base_orchestrator_generation) + "\n"
                + value.base_state_sha256 + "\n" + authority_material(value.authority)
                + "\n" + std::to_string(value.policy.maximum_retries_per_stage)
                + "\n" + std::to_string(value.policy.maximum_parallel_tasks)
                + "\n" + (value.policy.accept_parallel_results ? "1" : "0")
                + "\n" + std::to_string(value.next_evidence_index)
                + "\n" + (value.cancelled ? "1" : "0");
            for (const StageRecord& stage : value.stages)
                material += "\n" + stage.task.task_sha256 + "\n"
                    + std::to_string(static_cast<std::uint8_t>(stage.state))
                    + "\n" + stage.receipt_sha256 + "\n"
                    + stage.evidence_sha256;
            if (value.admission) material += "\n" + value.admission->evidence_sha256;
            return material;
        }

        [[nodiscard]] Task make_task(
            const Snapshot& snapshot,
            std::uint32_t index,
            std::uint32_t attempt)
        {
            const Actor actor = actors[index];
            Task task{
                .task_id = snapshot.campaign_id + ".validation."
                    + std::to_string(index) + "." + std::to_string(attempt),
                .stage_index = index,
                .attempt = attempt,
                .actor = actor,
                .required_lane = lane_for(actor),
                .configuration = configuration_for(actor),
                .target = target_for(actor),
                .campaign_id = snapshot.campaign_id,
                .campaign_generation = snapshot.campaign_generation,
                .session_identity = snapshot.session_identity,
                .base_orchestrator_generation = snapshot.base_orchestrator_generation,
                .base_state_sha256 = snapshot.base_state_sha256,
                .adapter_generation = snapshot.generation,
                .adapter_state_sha256 = snapshot.state_sha256,
                .authority = snapshot.authority};
            task.task_sha256 = hash(task_material(task));
            return task;
        }

        [[nodiscard]] bool task_valid(const Task& task)
        {
            return task.stage_index < actors.size()
                && task.actor == actors[task.stage_index]
                && task.required_lane == lane_for(task.actor)
                && task.configuration == configuration_for(task.actor)
                && task.target == target_for(task.actor)
                && task.session_identity.valid()
                && lower_hex(task.task_sha256, 32u)
                && hash(task_material(task)) == task.task_sha256;
        }

        [[nodiscard]] bool receipt_matches(
            const Task& task,
            const TaskResult& result,
            std::string& diagnostic)
        {
            if (result.task_id != task.task_id
                || result.stage_index != task.stage_index
                || result.attempt != task.attempt
                || result.campaign_id != task.campaign_id
                || result.campaign_generation != task.campaign_generation
                || result.session_identity != task.session_identity
                || result.adapter_generation != task.adapter_generation
                || result.adapter_state_sha256 != task.adapter_state_sha256
                || result.task_sha256 != task.task_sha256
                || result.authority_sha256 != task.authority.authority_sha256
                || result.bundle_sha256 != task.authority.bundle_sha256
                || result.candidate_sha256 != task.authority.candidate_sha256
                || result.toolchain_sha256 != task.authority.toolchain_sha256
                || result.configuration_sha256
                    != task.authority.configuration_sha256
                || !result.host_completed)
            {
                diagnostic = "Validation result binding is stale or forged.";
                return false;
            }
            const auto& receipt = result.receipt;
            const std::string canonical = build_validation::canonical_json(receipt);
            if (hash(canonical) != result.receipt_sha256
                || receipt.source_version != task.authority.source_version
                || receipt.packaged_version != task.authority.packaged_version
                || receipt.source_commit != task.authority.source_commit
                || receipt.source_tree_sha256 != task.authority.source_tree_sha256
                || receipt.platform != task.authority.platform
                || receipt.compiler != task.authority.compiler
                || receipt.configuration != task.configuration
                || receipt.target != task.target
                || receipt.toolchain != task.authority.toolchain
                || !lower_hex(receipt.artifact.sha256, 32u)
                || receipt.artifact.size_bytes == 0u)
            {
                diagnostic = "Build-validation receipt does not match the exact task authority.";
                return false;
            }
            const auto* check = build_validation::find_check(
                receipt, task.required_lane);
            if (check == nullptr || check->status != build_validation::CheckStatus::passed
                || !lower_hex(check->evidence_sha256, 32u))
            {
                diagnostic = "Required validation check did not pass.";
                return false;
            }
            return true;
        }

        [[nodiscard]] bool result_binding_matches(
            const Task& task,
            const TaskResult& result) noexcept
        {
            return result.task_id == task.task_id
                && result.stage_index == task.stage_index
                && result.attempt == task.attempt
                && result.campaign_id == task.campaign_id
                && result.campaign_generation == task.campaign_generation
                && result.session_identity == task.session_identity
                && result.adapter_generation == task.adapter_generation
                && result.adapter_state_sha256 == task.adapter_state_sha256
                && result.task_sha256 == task.task_sha256
                && result.authority_sha256 == task.authority.authority_sha256
                && result.bundle_sha256 == task.authority.bundle_sha256
                && result.candidate_sha256 == task.authority.candidate_sha256
                && result.toolchain_sha256 == task.authority.toolchain_sha256
                && result.configuration_sha256
                    == task.authority.configuration_sha256
                && result.host_completed;
        }

        [[nodiscard]] AdmissionEvidence aggregate(const Snapshot& snapshot)
        {
            AdmissionEvidence evidence{
                .campaign_id = snapshot.campaign_id,
                .session_identity = snapshot.session_identity,
                .candidate_sha256 = snapshot.authority.candidate_sha256,
                .bundle_sha256 = snapshot.authority.bundle_sha256,
                .authority_sha256 = snapshot.authority.authority_sha256,
                .source_commit = snapshot.authority.source_commit,
                .source_tree_sha256 = snapshot.authority.source_tree_sha256,
                .toolchain_sha256 = snapshot.authority.toolchain_sha256,
                .configuration_sha256 = snapshot.authority.configuration_sha256};
            bool all = snapshot.stages.size() == engine_stage_count;
            for (const StageRecord& stage : snapshot.stages)
            {
                all = all && stage.state == TaskState::evidence_recorded
                    && stage.receipt.has_value();
                evidence.receipt_sha256s.push_back(stage.receipt_sha256);
            }
            if (all)
            {
                const auto& final_receipt = *snapshot.stages.back().receipt;
                all = static_cast<bool>(build_validation::admit(
                    final_receipt, snapshot.authority.source_version,
                    snapshot.policy.admission));
            }
            std::string json = "{\"schema\":\"" + evidence.schema
                + "\",\"campaign_id\":\"" + evidence.campaign_id
                + "\",\"candidate_sha256\":\"" + evidence.candidate_sha256
                + "\",\"bundle_sha256\":\"" + evidence.bundle_sha256
                + "\",\"authority_sha256\":\"" + evidence.authority_sha256
                + "\",\"source_commit\":\"" + evidence.source_commit
                + "\",\"source_tree_sha256\":\"" + evidence.source_tree_sha256
                + "\",\"toolchain_sha256\":\"" + evidence.toolchain_sha256
                + "\",\"configuration_sha256\":\""
                + evidence.configuration_sha256 + "\",\"receipts\":[";
            for (std::size_t index = 0u; index < evidence.receipt_sha256s.size(); ++index)
            {
                if (index != 0u) json += ',';
                json += "\"" + evidence.receipt_sha256s[index] + "\"";
            }
            json += "],\"admitted\":" + std::string{all ? "true" : "false"}
                + ",\"upload_permitted\":false,\"release_permitted\":false}";
            evidence.canonical_json = std::move(json);
            evidence.evidence_sha256 = hash(evidence.canonical_json);
            evidence.admitted = all;
            return evidence;
        }

        [[nodiscard]] Result rejected(Code code, const Snapshot& snapshot, std::string status)
        {
            return {.code = code, .snapshot = snapshot, .status = std::move(status)};
        }
    }

    DirectOrchestratorPort::DirectOrchestratorPort(
        self_iteration_orchestrator::Orchestrator& orchestrator) noexcept
        : orchestrator_(&orchestrator)
    {
    }

    HostEvidenceResult DirectOrchestratorPort::record_validation(
        const Actor actor,
        std::string evidence_sha256,
        std::string summary,
        const bool passed,
        self_iteration_orchestrator::ActionToken action)
    {
        if (orchestrator_ == nullptr) return {.status = "Orchestrator is unavailable."};
        const std::uint64_t now_unix_seconds = action.now_unix_seconds;
        const auto requested = orchestrator_->request_validation(std::move(action));
        if (!requested || !requested.pending_operation
            || requested.pending_operation->validation_actor() != actor)
            return {.status = requested.status};
        const auto& operation = *requested.pending_operation;
        const auto recorded = orchestrator_->record_validation({
            .operation_id = operation.operation_id(),
            .expected_generation = operation.expected_generation(),
            .expected_state_sha256 = operation.expected_state_sha256(),
            .transition_id = operation.transition_id(),
            .now_unix_seconds = now_unix_seconds},
            std::move(evidence_sha256), std::move(summary), passed);
        return {.accepted = static_cast<bool>(recorded),
            .snapshot = recorded.snapshot, .status = recorded.status};
    }

    Result Adapter::persist(Code code, std::string status, JournalPort& journal)
    {
        snapshot_.previous_state_sha256 = snapshot_.state_sha256;
        ++snapshot_.generation;
        snapshot_.status = status;
        snapshot_.state_sha256.clear();
        snapshot_.state_sha256 = hash(snapshot_material(snapshot_));
        if (!journal.store_atomic(snapshot_))
            return rejected(Code::journal_failure, snapshot_, "Validation state could not be persisted atomically.");
        return {.code = code, .snapshot = snapshot_, .status = std::move(status)};
    }

    bool Adapter::binding_matches(
        const self_iteration_orchestrator::Snapshot& orchestrator,
        const Authority& authority,
        const Policy& policy) const
    {
        return configured_ && authority_material(authority)
                == authority_material(snapshot_.authority)
            && policy.maximum_retries_per_stage
                == snapshot_.policy.maximum_retries_per_stage
            && policy.maximum_parallel_tasks == snapshot_.policy.maximum_parallel_tasks
            && policy.accept_parallel_results == snapshot_.policy.accept_parallel_results
            && orchestrator.orchestrator_id == snapshot_.orchestrator_id
            && orchestrator.campaign.campaign_id == snapshot_.campaign_id
            && orchestrator.campaign.session.identity == snapshot_.session_identity
            && orchestrator.candidate_sha256 == snapshot_.authority.candidate_sha256;
    }

    Result Adapter::begin(
        const self_iteration_orchestrator::Snapshot& orchestrator,
        Authority authority,
        Policy policy,
        JournalPort& journal)
    {
        if (!policy.valid() || !valid_authority(authority))
            return rejected(Code::invalid_policy, {}, "Validation policy or authority is invalid.");
        if (orchestrator.phase
                != self_iteration_orchestrator::Phase::awaiting_validation_request
            || orchestrator.campaign.session.source.target_kind
                != iteration_session::IterationTargetKind::engine_source
            || orchestrator.candidate_sha256 != authority.candidate_sha256
            || orchestrator.campaign.session.source.commit != authority.source_commit
            || orchestrator.promotion_permitted || orchestrator.git_permitted
            || orchestrator.release_permitted
            || orchestrator.network_or_server_permitted)
            return rejected(Code::stale_state, {}, "Orchestrator is not at the exact engine validation boundary.");
        snapshot_ = {.generation = 1u,
            .orchestrator_id = orchestrator.orchestrator_id,
            .campaign_id = orchestrator.campaign.campaign_id,
            .campaign_generation = orchestrator.campaign.record_generation,
            .session_identity = orchestrator.campaign.session.identity,
            .base_orchestrator_generation = orchestrator.generation,
            .base_state_sha256 = orchestrator.state_sha256,
            .authority = std::move(authority),
            .policy = policy,
            .status = "Seven trusted validation stages are ready for host dispatch."};
        snapshot_.state_sha256 = hash(snapshot_material(snapshot_));
        for (std::uint32_t index = 0u; index < actors.size(); ++index)
            snapshot_.stages.push_back({.task = make_task(snapshot_, index, 1u)});
        snapshot_.state_sha256 = hash(snapshot_material(snapshot_));
        configured_ = true;
        if (!journal.store_atomic(snapshot_))
            return rejected(Code::journal_failure, snapshot_, "Initial validation state could not be stored.");
        return {.code = Code::ready, .snapshot = snapshot_,
            .status = snapshot_.status};
    }

    Result Adapter::resume(
        const self_iteration_orchestrator::Snapshot& orchestrator,
        Authority authority,
        Policy policy,
        JournalPort& journal)
    {
        const auto loaded = journal.load(orchestrator.campaign.campaign_id);
        if (!loaded) return rejected(Code::journal_failure, {}, "Validation state is missing.");
        snapshot_ = *loaded;
        configured_ = true;
        const std::string stored = snapshot_.state_sha256;
        snapshot_.state_sha256.clear();
        const bool digest_ok = hash(snapshot_material(snapshot_)) == stored;
        snapshot_.state_sha256 = stored;
        if (!digest_ok || !binding_matches(orchestrator, authority, policy)
            || snapshot_.stages.size() != engine_stage_count)
            return rejected(Code::stale_state, snapshot_, "Validation resume binding is stale or corrupt.");
        bool changed = false;
        for (StageRecord& stage : snapshot_.stages)
        {
            if (!task_valid(stage.task))
                return rejected(Code::forged_result, snapshot_, "Stored validation task digest is invalid.");
            if (stage.state == TaskState::dispatched)
            {
                if (stage.task.attempt > policy.maximum_retries_per_stage)
                    stage.state = TaskState::failed;
                else
                {
                    stage.task = make_task(snapshot_, stage.task.stage_index,
                        stage.task.attempt + 1u);
                    stage.state = TaskState::retry_ready;
                }
                changed = true;
            }
        }
        return changed ? persist(Code::ready,
            "Interrupted host tasks were invalidated and rebound for explicit retry.", journal)
            : Result{.code = Code::ready, .snapshot = snapshot_,
                .status = "Validation state resumed without replay."};
    }

    Result Adapter::dispatch(TaskHost& host, JournalPort& journal)
    {
        if (!configured_ || snapshot_.cancelled)
            return rejected(Code::stale_state, snapshot_, "Validation adapter is not dispatchable.");
        std::size_t active{};
        for (const StageRecord& stage : snapshot_.stages)
            if (stage.state == TaskState::dispatched) ++active;
        std::vector<Task> issued{};
        for (StageRecord& stage : snapshot_.stages)
        {
            if (active >= snapshot_.policy.maximum_parallel_tasks) break;
            if (stage.state != TaskState::ready && stage.state != TaskState::retry_ready)
                continue;
            stage.task.adapter_generation = snapshot_.generation;
            stage.task.adapter_state_sha256 = snapshot_.state_sha256;
            stage.task.task_sha256 = hash(task_material(stage.task));
            if (!host.enqueue(stage.task))
                return rejected(Code::host_failure, snapshot_, "Host rejected typed validation task.");
            stage.state = TaskState::dispatched;
            issued.push_back(stage.task);
            ++active;
            if (!snapshot_.policy.accept_parallel_results) break;
        }
        if (issued.empty()) return rejected(Code::out_of_order, snapshot_, "No validation task is dispatchable.");
        Result result = persist(Code::dispatched,
            "Bound validation tasks were queued to the injected host.", journal);
        result.tasks = std::move(issued);
        return result;
    }

    Result Adapter::record_result(TaskResult result, JournalPort& journal)
    {
        if (!configured_ || result.stage_index >= snapshot_.stages.size())
            return rejected(Code::stale_state, snapshot_, "Validation result stage is invalid.");
        StageRecord& stage = snapshot_.stages[result.stage_index];
        if (stage.state != TaskState::dispatched)
            return rejected(Code::replay_rejected, snapshot_, "Validation task is not awaiting a result.");
        if (!snapshot_.policy.accept_parallel_results)
        {
            for (std::size_t index = 0u; index < result.stage_index; ++index)
                if (snapshot_.stages[index].state != TaskState::evidence_recorded)
                    return rejected(Code::out_of_order, snapshot_, "Validation result arrived out of policy order.");
        }
        std::string diagnostic{};
        if (!result_binding_matches(stage.task, result))
            return rejected(Code::forged_result, snapshot_,
                "Validation result binding is stale or forged.");
        if (!receipt_matches(stage.task, result, diagnostic))
        {
            if (stage.task.attempt <= snapshot_.policy.maximum_retries_per_stage)
            {
                stage.task = make_task(snapshot_, stage.task.stage_index,
                    stage.task.attempt + 1u);
                stage.state = TaskState::retry_ready;
                stage.diagnostic = diagnostic;
                return persist(Code::retry_scheduled,
                    "Invalid validation result was rejected and one bounded retry scheduled.", journal);
            }
            stage.state = TaskState::failed;
            stage.diagnostic = diagnostic;
            return persist(Code::failed_validation, diagnostic, journal);
        }
        if (stage.task.actor == Actor::full_validation
            && !build_validation::admit(result.receipt,
                snapshot_.authority.source_version, snapshot_.policy.admission))
        {
            stage.state = TaskState::failed;
            return persist(Code::failed_validation,
                "Full build-validation receipt did not satisfy admission policy.", journal);
        }
        stage.state = TaskState::result_ready;
        stage.receipt_sha256 = result.receipt_sha256;
        stage.evidence_sha256 = hash("EPOCH_ITERATION_VALIDATION_RESULT_V1\n"
            + stage.task.task_sha256 + "\n" + result.receipt_sha256);
        stage.diagnostic = std::move(result.diagnostic);
        stage.receipt = std::move(result.receipt);
        return persist(Code::result_accepted,
            "Exact host validation result accepted for ordered orchestrator evidence.", journal);
    }

    Result Adapter::advance_orchestrator(
        const self_iteration_orchestrator::Snapshot& orchestrator,
        self_iteration_orchestrator::ActionToken action,
        OrchestratorPort& port,
        JournalPort& journal)
    {
        if (!configured_ || snapshot_.cancelled
            || snapshot_.next_evidence_index >= snapshot_.stages.size())
            return rejected(Code::stale_state, snapshot_, "No validation evidence is advanceable.");
        StageRecord& stage = snapshot_.stages[snapshot_.next_evidence_index];
        if (stage.state != TaskState::result_ready)
            return rejected(Code::out_of_order, snapshot_, "Next validation result is not ready.");
        if (orchestrator.phase
                != self_iteration_orchestrator::Phase::awaiting_validation_request
            || orchestrator.orchestrator_id != snapshot_.orchestrator_id
            || orchestrator.campaign.campaign_id != snapshot_.campaign_id
            || orchestrator.campaign.session.identity != snapshot_.session_identity
            || orchestrator.candidate_sha256 != snapshot_.authority.candidate_sha256
            || action.expected_generation != orchestrator.generation
            || action.expected_state_sha256 != orchestrator.state_sha256)
            return rejected(Code::stale_state, snapshot_, "Orchestrator evidence action is stale.");
        const auto host = port.record_validation(stage.task.actor,
            stage.evidence_sha256, "Trusted host validated exact candidate and build authority.",
            true, std::move(action));
        if (!host.accepted)
            return rejected(Code::host_failure, snapshot_, host.status);
        stage.state = TaskState::evidence_recorded;
        ++snapshot_.next_evidence_index;
        if (snapshot_.next_evidence_index == snapshot_.stages.size())
        {
            snapshot_.admission = aggregate(snapshot_);
            if (!snapshot_.admission->admitted)
                return persist(Code::failed_validation,
                    "Aggregate build-validation evidence was not admitted.", journal);
            return persist(Code::admitted,
                "Seven trusted stages admitted; Site-readable evidence grants no upload or release authority.", journal);
        }
        return persist(Code::evidence_recorded,
            "Trusted validation evidence recorded in orchestrator order.", journal);
    }

    Result Adapter::cancel(
        std::string reason,
        TaskHost& host,
        JournalPort& journal)
    {
        if (!configured_ || reason.empty() || snapshot_.cancelled)
            return rejected(Code::stale_state, snapshot_, "Validation cancellation is invalid.");
        for (StageRecord& stage : snapshot_.stages)
        {
            if (stage.state == TaskState::dispatched) host.cancel(stage.task.task_id);
            if (stage.state != TaskState::evidence_recorded)
                stage.state = TaskState::cancelled;
        }
        snapshot_.cancelled = true;
        return persist(Code::cancelled, std::move(reason), journal);
    }

    Snapshot Adapter::snapshot() const { return snapshot_; }
}
