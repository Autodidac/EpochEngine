/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

module ai.iteration_campaign_scheduler;

namespace epochengine::ai::iteration_campaign_scheduler
{
    namespace
    {
        [[nodiscard]] Authority authority(
            const Scheduler& scheduler,
            const iteration_campaign_queue::Queue& queue,
            const self_iteration_orchestrator::Orchestrator& orchestrator,
            std::uint64_t now)
        {
            return Authority{
                scheduler.snapshot().configuration.human_authority_sha256,
                scheduler.snapshot().state_sha256,
                queue.snapshot().state_sha256,
                orchestrator.snapshot().state_sha256,
                scheduler.snapshot().generation,
                now,
                true};
        }

        [[nodiscard]] iteration_campaign_queue::HumanActionToken queue_authority(
            const iteration_campaign_queue::Queue& queue,
            std::uint64_t now)
        {
            return iteration_campaign_queue::HumanActionToken{
                queue.snapshot().state_sha256,
                queue.snapshot().configuration.human_authority_sha256,
                queue.snapshot().generation,
                now,
                true};
        }

        [[nodiscard]] mcp_orchestrator_bridge::HostApproval approval(
            const mcp_orchestrator_bridge::PendingReceipt& receipt,
            std::uint64_t now)
        {
            return mcp_orchestrator_bridge::HostApproval{
                .receipt_id = receipt.receipt_id(),
                .connection_generation = receipt.connection_generation(),
                .expected_orchestrator_generation =
                    receipt.expected_orchestrator_generation(),
                .expected_state_sha256 = receipt.expected_state_sha256(),
                .now_unix_seconds = now,
                .operator_approved = true};
        }

        [[nodiscard]] mcp_orchestrator_bridge::HostOperationResult response(
            const self_iteration_orchestrator::PendingOperation& operation,
            std::uint64_t now,
            std::string content)
        {
            return mcp_orchestrator_bridge::HostOperationResult{
                .operation_id = operation.operation_id(),
                .expected_generation = operation.expected_generation(),
                .expected_state_sha256 = operation.expected_state_sha256(),
                .transition_id = operation.transition_id(),
                .now_unix_seconds = now,
                .content = std::move(content),
                .summary = "Guarded transport returned one bounded plan.",
                .passed = true,
                .operator_approved = true};
        }

        [[nodiscard]] self_iteration_orchestrator::Configuration orchestration(
            const iteration_session::SourceAuthority& source,
            const std::vector<iteration_session::CuratedFile>& files,
            const std::filesystem::path& cache,
            project_profile::Provider provider,
            std::string profile,
            std::string host_digest,
            std::uint64_t now)
        {
            return self_iteration_orchestrator::Configuration{
                .authority = source,
                .curated_files = files,
                .cache_root = cache,
                .objective = "Produce one bounded digest-bound campaign plan.",
                .provider = provider,
                .project_profile_bytes = std::move(profile),
                .operator_model_binding = provider == project_profile::Provider::external_mcp
                    ? "Qwen3.8-27B-external-mcp" : std::string{},
                .host = self_iteration_orchestrator::HostBinding{
                    .binding_id = provider == project_profile::Provider::external_mcp
                        ? "external-mcp-binding" : "local-child-binding",
                    .configuration_sha256 = std::move(host_digest),
                    .generation = 1u,
                    .stdio_only = true},
                .budgets = iteration_campaign::default_budgets(
                    iteration_session::IterationTargetKind::project_source),
                .created_at_unix_seconds = now,
                .project_source_campaign_permitted = true,
                .sandbox_apply_permitted = true};
        }

        [[nodiscard]] bool prepare_queue(
            iteration_campaign_queue::Queue& queue,
            const self_iteration_orchestrator::Snapshot& orchestrator,
            std::string queue_id,
            std::uint64_t now)
        {
            const iteration_campaign_queue::QueueConfiguration configuration{
                std::move(queue_id), std::string(64u, '1'), now,
                now + 4u * 60u * 60u, {4u, 16u, 16u, 2048u, 512u}};
            if (!queue.begin(configuration))
                return false;
            const iteration_campaign_queue::ObjectiveRequest objective{
                "bounded-plan",
                orchestrator.campaign.session.objective,
                4u,
                {orchestrator.campaign.campaign_id,
                    orchestrator.target_key,
                    "cache/ai/iterations/active.epochai",
                    orchestrator.campaign.state_digest,
                    orchestrator.campaign.record_generation}};
            return static_cast<bool>(queue.enqueue(
                queue_authority(queue, now + 1u), objective));
        }

        [[nodiscard]] bool awaiting_review(
            const iteration_campaign_queue::Queue& queue)
        {
            return queue.snapshot().items.size() == 1u
                && queue.snapshot().items.front().phase
                    == iteration_campaign_queue::ItemPhase::awaiting_human_review;
        }
    }

    bool run_contract()
    {
        namespace fs = std::filesystem;
        using iteration_session::IterationTargetKind;
        using iteration_session::SourceAuthority;
        using iteration_session::SourceAuthorityKind;
        constexpr std::uint64_t now = 2'300'000'000u;
        const auto sequence = std::chrono::steady_clock::now()
            .time_since_epoch().count();
        const fs::path root = fs::temp_directory_path()
            / ("epoch_scheduler_" + std::to_string(sequence));
        const fs::path project = root / "Project";
        std::error_code ec{};
        fs::create_directories(project / "Source", ec);
        std::ofstream{project / "Source/main.cpp", std::ios::binary}
            << "int scheduler_contract() { return 41; }\n";

        const auto local_profile = project_profile::serialize_profile(
            project_profile::make_profile(
                project_profile::Provider::epoch_local_qwen38));
        SourceAuthority source{
            .target_kind = IterationTargetKind::project_source,
            .kind = SourceAuthorityKind::verified_project,
            .root = fs::weakly_canonical(project, ec),
            .project_id = "scheduler_contract",
            .project_manifest_digest = std::string(64u, 'a'),
            .project_profile_digest = local_profile.sha256,
            .verified = !ec};
        const auto inspected = iteration_session::inspect_curated_files(
            source, {"Source/main.cpp"});
        if (!inspected.accepted)
        {
            fs::remove_all(root, ec);
            return false;
        }

        // Local child path: persist a failed disconnected attempt, restore its
        // exact backoff state, reconnect, approve, and bind the response.
        self_iteration_orchestrator::Orchestrator local_orchestrator{};
        const std::string local_host_digest(64u, 'b');
        auto local_begun = local_orchestrator.begin(orchestration(
            source, inspected.files, root / "local-cache",
            project_profile::Provider::epoch_local_qwen38,
            local_profile.canonical_bytes, local_host_digest, now));
        iteration_campaign_queue::Queue local_queue{};
        if (!local_begun || !prepare_queue(
                local_queue, local_begun.snapshot, "local-queue", now))
        {
            fs::remove_all(root, ec);
            return false;
        }
        Scheduler local_scheduler{};
        Configuration local_configuration{
            "local-scheduler", std::string(64u, '1'), "local-queue",
            local_begun.snapshot.orchestrator_id, local_host_digest,
            self_iteration_orchestrator::TransportKind::guarded_local_mcp_child,
            now, now + 4u * 60u * 60u, {3u, 2u, 8u}};
        mcp_orchestrator_bridge::Bridge local_bridge{"scheduler-local-session"};
        if (!local_scheduler.begin(local_configuration,
                local_queue.snapshot(), local_orchestrator.snapshot())
            || !local_bridge.connect("local-connection", 1u,
                local_orchestrator.snapshot())
            || !local_bridge.disconnect("contract backoff",
                local_orchestrator.snapshot()))
        {
            fs::remove_all(root, ec);
            return false;
        }
        Result failed_dispatch = local_scheduler.dispatch_next(
            authority(local_scheduler, local_queue, local_orchestrator, now + 2u),
            local_queue, local_bridge, local_orchestrator);
        const Checkpoint backoff_checkpoint = local_scheduler.checkpoint();
        Scheduler restored_local{};
        if (failed_dispatch.code != Code::transport_backoff
            || failed_dispatch.snapshot.attempts != 1u
            || failed_dispatch.snapshot.next_retry_at_unix_seconds != now + 4u
            || !backoff_checkpoint
            || !restored_local.restore(backoff_checkpoint.bytes, now + 3u,
                local_queue.snapshot(), local_orchestrator.snapshot())
            || restored_local.dispatch_next(
                authority(restored_local, local_queue, local_orchestrator, now + 3u),
                local_queue, local_bridge, local_orchestrator).code
                != Code::transport_backoff
            || !local_bridge.connect("local-reconnect", 2u,
                local_orchestrator.snapshot()))
        {
            fs::remove_all(root, ec);
            return false;
        }
        Result local_dispatch = restored_local.dispatch_next(
            authority(restored_local, local_queue, local_orchestrator, now + 4u),
            local_queue, local_bridge, local_orchestrator);
        if (!local_dispatch || !local_dispatch.bridge.pending_receipt
            || local_dispatch.code != Code::pending_human_approval)
        {
            fs::remove_all(root, ec);
            return false;
        }
        Result local_approved = restored_local.approve_dispatch(
            authority(restored_local, local_queue, local_orchestrator, now + 5u),
            approval(*local_dispatch.bridge.pending_receipt, now + 5u),
            local_queue, local_bridge, local_orchestrator);
        if (!local_approved || !local_approved.bridge.pending_operation
            || local_approved.bridge.pending_operation->transport()
                != self_iteration_orchestrator::TransportKind::guarded_local_mcp_child)
        {
            fs::remove_all(root, ec);
            return false;
        }
        const auto local_operation = *local_approved.bridge.pending_operation;
        Result local_completed = restored_local.complete_response(
            authority(restored_local, local_queue, local_orchestrator, now + 6u),
            local_operation,
            response(local_operation, now + 6u,
                "Inspect the admitted source, propose one bounded change, validate."),
            local_queue, local_bridge, local_orchestrator);
        if (local_completed.code != Code::awaiting_human_review
            || !awaiting_review(local_queue)
            || restored_local.complete_response(
                authority(restored_local, local_queue, local_orchestrator, now + 7u),
                local_operation,
                response(local_operation, now + 7u, "replay"),
                local_queue, local_bridge, local_orchestrator).code
                != Code::replay_refused)
        {
            fs::remove_all(root, ec);
            return false;
        }

        // External MCP uses the same queue receipts but retains an independent
        // host/profile/connection binding and never inherits local receipts.
        const auto external_profile = project_profile::serialize_profile(
            project_profile::make_profile(project_profile::Provider::external_mcp));
        SourceAuthority external_source = source;
        external_source.project_id = "scheduler_external_contract";
        external_source.project_profile_digest = external_profile.sha256;
        self_iteration_orchestrator::Orchestrator external_orchestrator{};
        const std::string external_host_digest(64u, 'c');
        auto external_begun = external_orchestrator.begin(orchestration(
            external_source, inspected.files, root / "external-cache",
            project_profile::Provider::external_mcp,
            external_profile.canonical_bytes, external_host_digest, now + 20u));
        if (!external_begun)
        {
            fs::remove_all(root, ec);
            return false;
        }
        iteration_campaign_queue::Queue external_queue{};
        if (!prepare_queue(external_queue,
                external_begun.snapshot, "external-queue", now + 20u))
        {
            fs::remove_all(root, ec);
            return false;
        }
        Scheduler external_scheduler{};
        Configuration external_configuration{
            "external-scheduler", std::string(64u, '1'), "external-queue",
            external_begun.snapshot.orchestrator_id, external_host_digest,
            self_iteration_orchestrator::TransportKind::external_mcp,
            now + 20u, now + 4u * 60u * 60u, {3u, 2u, 8u}};
        mcp_orchestrator_bridge::Bridge external_bridge{
            "scheduler-external-session"};
        if (!external_scheduler.begin(external_configuration,
                external_queue.snapshot(), external_orchestrator.snapshot())
            || !external_bridge.connect("external-connection", 1u,
                external_orchestrator.snapshot()))
        {
            fs::remove_all(root, ec);
            return false;
        }
        Result external_dispatch = external_scheduler.dispatch_next(
            authority(external_scheduler, external_queue, external_orchestrator,
                now + 22u), external_queue, external_bridge, external_orchestrator);
        Result external_approved = external_dispatch.bridge.pending_receipt
            ? external_scheduler.approve_dispatch(
                authority(external_scheduler, external_queue,
                    external_orchestrator, now + 23u),
                approval(*external_dispatch.bridge.pending_receipt, now + 23u),
                external_queue, external_bridge, external_orchestrator)
            : Result{};
        if (!external_approved || !external_approved.bridge.pending_operation
            || external_approved.bridge.pending_operation->transport()
                != self_iteration_orchestrator::TransportKind::external_mcp)
        {
            fs::remove_all(root, ec);
            return false;
        }
        const auto external_operation = *external_approved.bridge.pending_operation;
        Result external_completed = external_scheduler.complete_response(
            authority(external_scheduler, external_queue, external_orchestrator,
                now + 24u), external_operation,
            response(external_operation, now + 24u,
                "External MCP returned one exact bounded plan."),
            external_queue, external_bridge, external_orchestrator);
        const Checkpoint completed_checkpoint = external_scheduler.checkpoint();
        Scheduler restored_external{};
        std::vector<std::byte> tampered = completed_checkpoint.bytes;
        if (!tampered.empty()) tampered[tampered.size() / 2u] ^= std::byte{1u};
        const bool restored_ok = external_completed.code == Code::awaiting_human_review
            && awaiting_review(external_queue)
            && completed_checkpoint
            && restored_external.restore(completed_checkpoint.bytes, now + 25u,
                external_queue.snapshot(), external_orchestrator.snapshot())
            && restored_external.snapshot().seen_operation_ids.size() == 1u
            && Scheduler{}.restore(tampered, now + 25u,
                external_queue.snapshot(), external_orchestrator.snapshot()).code
                == Code::invalid_checkpoint;
        const Result cancelled = restored_ok
            ? restored_external.cancel(
                authority(restored_external, external_queue,
                    external_orchestrator, now + 26u),
                external_queue)
            : Result{};
        const bool ok = restored_ok
            && cancelled.code == Code::cancelled
            && cancelled.snapshot.phase == Phase::cancelled
            && external_queue.snapshot().items.front().phase
                == iteration_campaign_queue::ItemPhase::cancelled
            && !external_scheduler.snapshot().source_write_permitted
            && !external_scheduler.snapshot().promotion_permitted
            && !external_scheduler.snapshot().release_permitted
            && !external_scheduler.snapshot().server_permitted
            && !external_scheduler.snapshot().network_listener_permitted;
        fs::remove_all(root, ec);
        return ok;
    }
}

#if defined(EPOCH_AI_ITERATION_CAMPAIGN_SCHEDULER_CONTRACT_MAIN)
int main()
{
    return epochengine::ai::iteration_campaign_scheduler::run_contract() ? 0 : 1;
}
#endif
