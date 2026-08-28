/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstdint>
#include <map>
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
        using OrchestratorSnapshot = self_iteration_orchestrator::Snapshot;
        using Actor = iteration_session::ValidationActor;

        [[nodiscard]] std::string repeated(const char byte, const std::size_t count)
        {
            return std::string(count, byte);
        }

        [[nodiscard]] std::string digest(std::string_view bytes)
        {
            return source_patch_bundle::digest_hex(
                source_patch_bundle::digest(bytes));
        }

        class FakeJournal final : public JournalPort
        {
        public:
            [[nodiscard]] std::optional<Snapshot> load(
                std::string_view campaign_id) override
            {
                const auto found = states.find(std::string{campaign_id});
                return found == states.end() ? std::nullopt
                    : std::optional<Snapshot>{found->second};
            }

            [[nodiscard]] bool store_atomic(const Snapshot& snapshot) override
            {
                if (fail_next_store)
                {
                    fail_next_store = false;
                    return false;
                }
                states[snapshot.campaign_id] = snapshot;
                ++stores;
                return true;
            }

            std::map<std::string, Snapshot> states{};
            std::uint32_t stores{};
            bool fail_next_store{};
        };

        class FakeHost final : public TaskHost
        {
        public:
            [[nodiscard]] bool enqueue(const Task& task) override
            {
                if (!accept) return false;
                queued.push_back(task);
                return true;
            }

            void cancel(std::string_view task_id) noexcept override
            {
                cancelled.emplace_back(task_id);
            }

            std::vector<Task> queued{};
            std::vector<std::string> cancelled{};
            bool accept{true};
        };

        class FakeOrchestrator final : public OrchestratorPort
        {
        public:
            explicit FakeOrchestrator(OrchestratorSnapshot initial)
                : current(std::move(initial))
            {
            }

            [[nodiscard]] HostEvidenceResult record_validation(
                const Actor actor,
                std::string evidence_sha256,
                std::string summary,
                const bool passed,
                self_iteration_orchestrator::ActionToken action) override
            {
                constexpr Actor order[]{Actor::debug_compiler,
                    Actor::debug_contract, Actor::release_compiler,
                    Actor::release_contract, Actor::headless_compiler,
                    Actor::headless_contract, Actor::full_validation};
                if (index >= std::size(order) || actor != order[index]
                    || !build_validation::valid_lower_hex(evidence_sha256, 32u)
                    || summary.empty() || !passed
                    || action.expected_generation != current.generation
                    || action.expected_state_sha256 != current.state_sha256
                    || action.transition_id.empty() || action.now_unix_seconds == 0u)
                    return {.snapshot = current, .status = "forged fake-host evidence"};
                ++index;
                ++current.generation;
                current.previous_state_sha256 = current.state_sha256;
                current.state_sha256 = digest("orchestrator.validation."
                    + std::to_string(current.generation));
                current.validation_index = static_cast<std::uint32_t>(index);
                current.phase = index == std::size(order)
                    ? self_iteration_orchestrator::Phase::checkpoint_ready
                    : self_iteration_orchestrator::Phase::awaiting_validation_request;
                return {.accepted = true, .snapshot = current,
                    .status = "fake host accepted exact validation evidence"};
            }

            OrchestratorSnapshot current{};
            std::size_t index{};
        };

        [[nodiscard]] OrchestratorSnapshot orchestrator_snapshot()
        {
            OrchestratorSnapshot value{};
            value.generation = 41u;
            value.state_sha256 = repeated('1', 64u);
            value.orchestrator_id = "orchestrator.contract.1";
            value.phase = self_iteration_orchestrator::Phase::awaiting_validation_request;
            value.campaign.record_generation = 9u;
            value.campaign.campaign_id = "campaign.contract.1";
            value.campaign.session.identity = {701u, 37u};
            value.campaign.session.source.target_kind
                = iteration_session::IterationTargetKind::engine_source;
            value.campaign.session.source.commit = repeated('a', 40u);
            value.candidate_sha256 = repeated('b', 64u);
            return value;
        }

        [[nodiscard]] Authority authority()
        {
            return {.authority_sha256 = repeated('c', 64u),
                .bundle_sha256 = repeated('d', 64u),
                .candidate_sha256 = repeated('b', 64u),
                .source_commit = repeated('a', 40u),
                .source_tree_sha256 = repeated('e', 64u),
                .source_version = {0u, 89u, 31u},
                .packaged_version = {0u, 89u, 30u},
                .platform = build_validation::Platform::windows_x64,
                .compiler = build_validation::Compiler::msvc,
                .toolchain = "msvc-19.44-vcpkg-static",
                .toolchain_sha256 = repeated('f', 64u),
                .configuration_sha256 = repeated('9', 64u)};
        }

        [[nodiscard]] build_validation::CheckEvidence check(
            const build_validation::CheckLane lane,
            const char digest_byte)
        {
            return {.lane = lane,
                .status = build_validation::CheckStatus::passed,
                .duration_milliseconds = 17u,
                .evidence_sha256 = repeated(digest_byte, 64u),
                .diagnostic = "bounded fake-host proof"};
        }

        [[nodiscard]] build_validation::ValidationReceipt receipt_for(
            const Task& task)
        {
            build_validation::ValidationReceipt receipt{
                .source_version = task.authority.source_version,
                .packaged_version = task.authority.packaged_version,
                .source_commit = task.authority.source_commit,
                .source_tree_sha256 = task.authority.source_tree_sha256,
                .platform = task.authority.platform,
                .compiler = task.authority.compiler,
                .configuration = task.configuration,
                .target = task.target,
                .toolchain = task.authority.toolchain,
                .artifact = {.name = task.target + ".exe",
                    .sha256 = repeated('8', 64u), .size_bytes = 4096u},
                .checks = {check(task.required_lane, '7')}};
            if (task.actor == Actor::full_validation)
            {
                receipt.checks = {
                    check(build_validation::CheckLane::source_names, '1'),
                    check(build_validation::CheckLane::compile, '2'),
                    check(build_validation::CheckLane::engine_contract, '3'),
                    check(build_validation::CheckLane::headless_ci, '4'),
                    check(build_validation::CheckLane::package_inventory, '5'),
                    check(build_validation::CheckLane::dependency_resolution, '6')};
            }
            return receipt;
        }

        [[nodiscard]] TaskResult result_for(const Task& task)
        {
            TaskResult result{
                .task_id = task.task_id,
                .stage_index = task.stage_index,
                .attempt = task.attempt,
                .campaign_id = task.campaign_id,
                .campaign_generation = task.campaign_generation,
                .session_identity = task.session_identity,
                .adapter_generation = task.adapter_generation,
                .adapter_state_sha256 = task.adapter_state_sha256,
                .task_sha256 = task.task_sha256,
                .authority_sha256 = task.authority.authority_sha256,
                .bundle_sha256 = task.authority.bundle_sha256,
                .candidate_sha256 = task.authority.candidate_sha256,
                .toolchain_sha256 = task.authority.toolchain_sha256,
                .configuration_sha256 = task.authority.configuration_sha256,
                .receipt = receipt_for(task),
                .diagnostic = "host task exited normally",
                .host_completed = true};
            result.receipt_sha256 = digest(
                build_validation::canonical_json(result.receipt));
            return result;
        }

        [[nodiscard]] self_iteration_orchestrator::ActionToken action_for(
            const OrchestratorSnapshot& snapshot,
            const std::uint64_t sequence)
        {
            return {.expected_generation = snapshot.generation,
                .expected_state_sha256 = snapshot.state_sha256,
                .transition_id = "validation.contract."
                    + std::to_string(sequence),
                .now_unix_seconds = 1000u + sequence};
        }

        [[nodiscard]] bool happy_path()
        {
            FakeJournal journal{};
            FakeHost host{};
            auto current = orchestrator_snapshot();
            FakeOrchestrator orchestrator{current};
            Adapter adapter{};
            const Policy policy{};
            const auto started = adapter.begin(current, authority(), policy, journal);
            if (!started || started.code != Code::ready
                || started.snapshot.stages.size() != engine_stage_count)
                return false;

            for (std::uint32_t index = 0u; index < engine_stage_count; ++index)
            {
                const auto dispatched = adapter.dispatch(host, journal);
                if (!dispatched || dispatched.tasks.size() != 1u
                    || dispatched.tasks.front().stage_index != index)
                    return false;
                TaskResult result = result_for(dispatched.tasks.front());
                if (index == 0u)
                {
                    TaskResult forged = result;
                    forged.task_sha256 = repeated('0', 64u);
                    if (adapter.record_result(std::move(forged), journal).code
                        != Code::forged_result)
                        return false;
                }
                const auto recorded = adapter.record_result(std::move(result), journal);
                if (!recorded || recorded.code != Code::result_accepted)
                    return false;
                const auto advanced = adapter.advance_orchestrator(current,
                    action_for(current, index + 1u), orchestrator, journal);
                if (!advanced) return false;
                current = orchestrator.current;
                if (index + 1u == engine_stage_count)
                {
                    if (advanced.code != Code::admitted || !advanced.snapshot.admission
                        || !advanced.snapshot.admission->admitted
                        || advanced.snapshot.admission->upload_permitted
                        || advanced.snapshot.admission->release_permitted
                        || advanced.snapshot.admission->receipt_sha256s.size()
                            != engine_stage_count
                        || advanced.snapshot.admission->canonical_json.find(
                            "\"upload_permitted\":false") == std::string::npos)
                        return false;
                }
                else if (advanced.code != Code::evidence_recorded)
                    return false;
            }
            return orchestrator.index == engine_stage_count && journal.stores >= 22u;
        }

        [[nodiscard]] bool resume_cancel_and_retry()
        {
            auto current = orchestrator_snapshot();
            const Authority bound_authority = authority();
            const Policy policy{};
            FakeJournal journal{};
            FakeHost host{};
            Adapter interrupted{};
            if (!interrupted.begin(current, bound_authority, policy, journal)
                || !interrupted.dispatch(host, journal))
                return false;
            const Task stale_task = host.queued.back();
            Adapter resumed{};
            const auto recovered = resumed.resume(
                current, bound_authority, policy, journal);
            if (!recovered || recovered.snapshot.stages.front().state
                    != TaskState::retry_ready
                || recovered.snapshot.stages.front().task.attempt != 2u
                || resumed.record_result(result_for(stale_task), journal).code
                    != Code::replay_rejected)
                return false;
            const auto redelivered = resumed.dispatch(host, journal);
            if (!redelivered || redelivered.tasks.front().attempt != 2u)
                return false;
            const auto cancelled = resumed.cancel("operator cancelled validation",
                host, journal);
            if (!cancelled || !cancelled.snapshot.cancelled
                || host.cancelled.size() != 1u)
                return false;

            FakeJournal retry_journal{};
            FakeHost retry_host{};
            Adapter exhausted{};
            Policy no_retries{};
            no_retries.maximum_retries_per_stage = 0u;
            if (!exhausted.begin(current, bound_authority, no_retries, retry_journal))
                return false;
            const auto issued = exhausted.dispatch(retry_host, retry_journal);
            if (!issued) return false;
            TaskResult bad = result_for(issued.tasks.front());
            bad.receipt.artifact.sha256 = "not-a-digest";
            bad.receipt_sha256 = digest(build_validation::canonical_json(bad.receipt));
            return exhausted.record_result(std::move(bad), retry_journal).code
                == Code::failed_validation;
        }

        [[nodiscard]] bool parallel_policy()
        {
            auto current = orchestrator_snapshot();
            FakeJournal journal{};
            FakeHost host{};
            Adapter adapter{};
            Policy policy{};
            policy.maximum_parallel_tasks = 2u;
            policy.accept_parallel_results = true;
            if (!adapter.begin(current, authority(), policy, journal)) return false;
            const auto issued = adapter.dispatch(host, journal);
            if (!issued || issued.tasks.size() != 2u) return false;
            if (!adapter.record_result(result_for(issued.tasks[1]), journal)) return false;
            FakeOrchestrator orchestrator{current};
            if (adapter.advance_orchestrator(current, action_for(current, 1u),
                    orchestrator, journal).code != Code::out_of_order)
                return false;
            return static_cast<bool>(adapter.record_result(
                result_for(issued.tasks[0]), journal));
        }
    }

    bool run_contract()
    {
        return happy_path() && resume_cancel_and_retry() && parallel_policy();
    }
}
