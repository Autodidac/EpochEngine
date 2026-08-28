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

module ai.iteration_supervisor_control;

namespace epochengine::ai::iteration_supervisor_control
{
    namespace
    {
        struct Fixture final
        {
            std::filesystem::path root{};
            iteration_session::SourceAuthority source{};
            std::vector<iteration_session::CuratedFile> files{};
            self_iteration_orchestrator::Orchestrator orchestrator{};
            iteration_campaign_queue::Queue queue{};
            iteration_campaign_scheduler::Scheduler scheduler{};
            mcp_orchestrator_bridge::Bridge bridge;
            std::string actor_sha256{};
            std::uint64_t now{};

            Fixture(std::filesystem::path path, std::string session)
                : root{std::move(path)}, bridge{std::move(session)}
            {
            }
        };

        [[nodiscard]] iteration_campaign_queue::HumanActionToken queue_authority(
            const Fixture& fixture,
            std::uint64_t now)
        {
            return {
                fixture.queue.snapshot().state_sha256,
                fixture.actor_sha256,
                fixture.queue.snapshot().generation,
                now,
                true};
        }

        [[nodiscard]] iteration_campaign_scheduler::Authority scheduler_authority(
            const Fixture& fixture,
            std::uint64_t now)
        {
            return {
                fixture.actor_sha256,
                fixture.scheduler.snapshot().state_sha256,
                fixture.queue.snapshot().state_sha256,
                fixture.orchestrator.snapshot().state_sha256,
                fixture.scheduler.snapshot().generation,
                now,
                true};
        }

        [[nodiscard]] bool initialize(
            Fixture& fixture,
            project_profile::Provider provider,
            std::uint32_t objective_count)
        {
            namespace fs = std::filesystem;
            std::error_code ec{};
            fs::create_directories(fixture.root / "Project/Source", ec);
            std::ofstream{fixture.root / "Project/Source/main.cpp", std::ios::binary}
                << "int supervisor_contract() { return 73; }\n";
            const auto profile = project_profile::serialize_profile(
                project_profile::make_profile(provider));
            fixture.actor_sha256 = std::string(64u,
                provider == project_profile::Provider::external_mcp ? 'e' : 'd');
            fixture.source = {
                .target_kind = iteration_session::IterationTargetKind::project_source,
                .kind = iteration_session::SourceAuthorityKind::verified_project,
                .root = fs::weakly_canonical(fixture.root / "Project", ec),
                .project_id = provider == project_profile::Provider::external_mcp
                    ? "supervisor_external" : "supervisor_local",
                .project_manifest_digest = std::string(64u, 'a'),
                .project_profile_digest = profile.sha256,
                .verified = !ec};
            const auto inspected = iteration_session::inspect_curated_files(
                fixture.source, {"Source/main.cpp"});
            if (!inspected.accepted) return false;
            fixture.files = inspected.files;
            const std::string host_digest(64u,
                provider == project_profile::Provider::external_mcp ? 'c' : 'b');
            auto begun = fixture.orchestrator.begin(
                self_iteration_orchestrator::Configuration{
                    .authority = fixture.source,
                    .curated_files = fixture.files,
                    .cache_root = fixture.root / "c",
                    .objective = "Return one bounded, evidence-backed plan.",
                    .provider = provider,
                    .project_profile_bytes = profile.canonical_bytes,
                    .operator_model_binding =
                        provider == project_profile::Provider::external_mcp
                            ? "Qwen3.8-27B-external-mcp" : std::string{},
                    .host = self_iteration_orchestrator::HostBinding{
                        .binding_id = provider
                            == project_profile::Provider::external_mcp
                                ? "external-control-binding"
                                : "local-control-binding",
                        .configuration_sha256 = host_digest,
                        .generation = 1u,
                        .stdio_only = true},
                    .budgets = iteration_campaign::default_budgets(
                        iteration_session::IterationTargetKind::project_source),
                    .created_at_unix_seconds = fixture.now,
                    .project_source_campaign_permitted = true,
                    .sandbox_apply_permitted = true});
            if (!begun) return false;
            if (!fixture.queue.begin(iteration_campaign_queue::QueueConfiguration{
                    "supervisor-queue", fixture.actor_sha256, fixture.now,
                    fixture.now + 4u * 60u * 60u,
                    {8u, 32u, 32u, 2048u, 512u}}))
                return false;
            for (std::uint32_t index = 0u; index < objective_count; ++index)
            {
                if (!fixture.queue.enqueue(queue_authority(
                        fixture, fixture.now + 1u + index),
                    iteration_campaign_queue::ObjectiveRequest{
                        "objective-" + std::to_string(index + 1u),
                        "Prepare bounded plan " + std::to_string(index + 1u),
                        static_cast<std::uint8_t>(7u - index),
                        {begun.snapshot.campaign.campaign_id,
                            begun.snapshot.target_key,
                            "cache/ai/iterations/active.epochai",
                            begun.snapshot.campaign.state_digest,
                            begun.snapshot.campaign.record_generation}}))
                    return false;
            }
            const auto transport = provider == project_profile::Provider::external_mcp
                ? self_iteration_orchestrator::TransportKind::external_mcp
                : self_iteration_orchestrator::TransportKind::guarded_local_mcp_child;
            if (!fixture.scheduler.begin(
                    iteration_campaign_scheduler::Configuration{
                        "supervisor-scheduler", fixture.actor_sha256,
                        "supervisor-queue", begun.snapshot.orchestrator_id,
                        host_digest, transport, fixture.now,
                        fixture.now + 4u * 60u * 60u, {3u, 2u, 8u}},
                    fixture.queue.snapshot(), fixture.orchestrator.snapshot()))
                return false;
            return static_cast<bool>(fixture.bridge.connect(
                "supervisor-connection", 1u,
                fixture.orchestrator.snapshot()));
        }

        [[nodiscard]] Configuration control_configuration(
            const Fixture& fixture,
            std::uint32_t maximum_query_items = 8u)
        {
            return {
                "supervisor-control",
                "supervisor-session",
                fixture.actor_sha256,
                fixture.orchestrator.snapshot().campaign.campaign_id,
                fixture.now,
                fixture.now + 4u * 60u * 60u,
                {maximum_query_items, 64u, 256u * 1024u}};
        }

        [[nodiscard]] Command command(
            CommandKind kind,
            std::string command_id,
            const Surface& surface,
            const Fixture& fixture,
            std::uint64_t now)
        {
            return {
                std::move(command_id),
                kind,
                surface.snapshot().configuration.control_id,
                surface.snapshot().configuration.session_id,
                fixture.actor_sha256,
                surface.snapshot().configuration.campaign_id,
                fixture.scheduler.snapshot().objective_id,
                fixture.scheduler.snapshot().pending_operation_id,
                surface.snapshot().generation,
                surface.snapshot().state_sha256,
                fixture.queue.snapshot().generation,
                fixture.queue.snapshot().state_sha256,
                fixture.scheduler.snapshot().generation,
                fixture.scheduler.snapshot().state_sha256,
                now,
                true};
        }

        [[nodiscard]] Result submit(
            Surface& surface,
            const Command& value,
            Fixture& fixture)
        {
            const auto encoded = canonical_command(value);
            return surface.submit(encoded, fixture.queue, fixture.scheduler);
        }

        [[nodiscard]] mcp_orchestrator_bridge::HostApproval bridge_approval(
            const mcp_orchestrator_bridge::PendingReceipt& receipt,
            std::uint64_t now)
        {
            return {
                .receipt_id = receipt.receipt_id(),
                .connection_generation = receipt.connection_generation(),
                .expected_orchestrator_generation =
                    receipt.expected_orchestrator_generation(),
                .expected_state_sha256 = receipt.expected_state_sha256(),
                .now_unix_seconds = now,
                .operator_approved = true};
        }

        [[nodiscard]] bool advance_to_review(Fixture& fixture)
        {
            auto dispatched = fixture.scheduler.dispatch_next(
                scheduler_authority(fixture, fixture.now + 10u), fixture.queue,
                fixture.bridge, fixture.orchestrator);
            if (!dispatched.bridge.pending_receipt) return false;
            auto approved = fixture.scheduler.approve_dispatch(
                scheduler_authority(fixture, fixture.now + 11u),
                bridge_approval(*dispatched.bridge.pending_receipt,
                    fixture.now + 11u),
                fixture.queue, fixture.bridge, fixture.orchestrator);
            if (!approved.bridge.pending_operation) return false;
            const auto operation = *approved.bridge.pending_operation;
            auto completed = fixture.scheduler.complete_response(
                scheduler_authority(fixture, fixture.now + 12u), operation,
                mcp_orchestrator_bridge::HostOperationResult{
                    .operation_id = operation.operation_id(),
                    .expected_generation = operation.expected_generation(),
                    .expected_state_sha256 = operation.expected_state_sha256(),
                    .transition_id = operation.transition_id(),
                    .now_unix_seconds = fixture.now + 12u,
                    .content = "One bounded plan with exact evidence.",
                    .summary = "Guarded response for human review.",
                    .passed = true,
                    .operator_approved = true},
                fixture.queue, fixture.bridge, fixture.orchestrator);
            return completed.code
                == iteration_campaign_scheduler::Code::awaiting_human_review;
        }

        [[nodiscard]] bool hard_false(const Snapshot& snapshot)
        {
            return !snapshot.source_write_permitted
                && !snapshot.arbitrary_file_read_permitted
                && !snapshot.model_launch_permitted
                && !snapshot.promotion_permitted && !snapshot.release_permitted
                && !snapshot.server_permitted
                && !snapshot.network_listener_permitted;
        }
    }

    bool run_contract()
    {
        namespace fs = std::filesystem;
        const auto sequence = std::chrono::steady_clock::now()
            .time_since_epoch().count();
        const fs::path root = fs::temp_directory_path()
            / ("esc_" + std::to_string(sequence));
        std::error_code ec{};

        // Local transport: bounded query, strict command decoding, deterministic
        // retry permit, adjacent-state synchronization, and cancellation.
        Fixture local{root / "l", "supervisor-local-transport"};
        local.now = 2'400'000'000u;
        if (!initialize(local, project_profile::Provider::epoch_local_qwen38, 3u))
        {
            fs::remove_all(root, ec);
            return false;
        }
        if (!local.bridge.disconnect("force deterministic backoff",
                local.orchestrator.snapshot()))
        {
            fs::remove_all(root, ec);
            return false;
        }
        const auto failed = local.scheduler.dispatch_next(
            scheduler_authority(local, local.now + 5u), local.queue,
            local.bridge, local.orchestrator);
        if (failed.code != iteration_campaign_scheduler::Code::transport_backoff)
        {
            fs::remove_all(root, ec);
            return false;
        }
        Surface control{};
        if (!control.begin(control_configuration(local, 1u),
                local.queue.snapshot(), local.scheduler.snapshot()))
        {
            fs::remove_all(root, ec);
            return false;
        }
        const auto initial_query = control.query(
            local.queue.snapshot(), local.scheduler.snapshot());
        if (!initial_query || initial_query.snapshot.items.size() != 1u
            || !initial_query.snapshot.truncated
            || initial_query.snapshot.source_write_permitted
            || initial_query.snapshot.model_launch_permitted)
        {
            fs::remove_all(root, ec);
            return false;
        }

        Command malformed_value = command(CommandKind::pause, "malformed-1",
            control, local, local.now + 6u);
        auto malformed = canonical_command(malformed_value);
        malformed.push_back(std::byte{0u});
        if (control.submit(malformed, local.queue, local.scheduler).code
                != Code::malformed_command)
        {
            fs::remove_all(root, ec);
            return false;
        }
        Command noncanonical_value = command(CommandKind::pause, "noncanonical-1",
            control, local, local.now + 6u);
        auto noncanonical = canonical_command(noncanonical_value);
        noncanonical[noncanonical.size() - 8u] = std::byte{2u};
        if (control.submit(noncanonical, local.queue, local.scheduler).code
                != Code::noncanonical_command)
        {
            fs::remove_all(root, ec);
            return false;
        }
        Command broaden = command(CommandKind::pause, "broaden-1",
            control, local, local.now + 6u);
        broaden.source_write_requested = true;
        if (submit(control, broaden, local).code != Code::authority_broadening)
        {
            fs::remove_all(root, ec);
            return false;
        }
        Command unknown = command(CommandKind::pause, "unknown-1",
            control, local, local.now + 6u);
        unknown.campaign_id = "unknown-campaign";
        if (submit(control, unknown, local).code != Code::unknown_identifier)
        {
            fs::remove_all(root, ec);
            return false;
        }
        Command stale = command(CommandKind::pause, "stale-1",
            control, local, local.now + 6u);
        stale.expected_control_state_sha256 = std::string(64u, '0');
        if (submit(control, stale, local).code != Code::stale_state)
        {
            fs::remove_all(root, ec);
            return false;
        }

        const auto pause_result = submit(control,
            command(CommandKind::pause, "pause-1", control, local,
                local.now + 6u), local);
        const auto resume_result = submit(control,
            command(CommandKind::resume, "resume-1", control, local,
                local.now + 7u), local);
        const auto early_retry = submit(control,
            command(CommandKind::retry, "retry-early", control, local,
                local.now + 6u), local);
        if (pause_result.code != Code::paused
            || resume_result.code != Code::resumed
            || early_retry.code != Code::invalid_transition
            || resume_result.receipt.previous_receipt_sha256
                != pause_result.receipt.receipt_sha256)
        {
            fs::remove_all(root, ec);
            return false;
        }
        const std::uint64_t retry_time = local.scheduler.snapshot()
            .next_retry_at_unix_seconds;
        const Command retry_command = command(CommandKind::retry, "retry-1",
            control, local, retry_time);
        const auto retry_result = submit(control, retry_command, local);
        if (retry_result.code != Code::retry_authorized
            || !retry_result.retry_authority
            || retry_result.retry_authority->expected_state_sha256
                != local.scheduler.snapshot().state_sha256
            || submit(control, retry_command, local).code != Code::replay_refused)
        {
            fs::remove_all(root, ec);
            return false;
        }
        const Checkpoint retry_checkpoint = control.checkpoint();
        const Checkpoint retry_checkpoint_again = control.checkpoint();
        Surface restored{};
        std::vector<std::byte> tampered = retry_checkpoint.bytes;
        if (!tampered.empty()) tampered[tampered.size() / 2u] ^= std::byte{1u};
        if (!retry_checkpoint || retry_checkpoint.sha256
                != retry_checkpoint_again.sha256
            || retry_checkpoint.bytes != retry_checkpoint_again.bytes
            || !restored.restore(retry_checkpoint.bytes, retry_time,
                local.queue.snapshot(), local.scheduler.snapshot())
            || Surface{}.restore(tampered, retry_time,
                local.queue.snapshot(), local.scheduler.snapshot()).code
                != Code::invalid_checkpoint)
        {
            fs::remove_all(root, ec);
            return false;
        }

        if (!local.bridge.connect("supervisor-retry-connection", 2u,
                local.orchestrator.snapshot()))
        {
            fs::remove_all(root, ec);
            return false;
        }
        const auto dispatched = local.scheduler.dispatch_next(
            *retry_result.retry_authority, local.queue,
            local.bridge, local.orchestrator);
        if (dispatched.code
                != iteration_campaign_scheduler::Code::pending_human_approval
            || control.query(local.queue.snapshot(), local.scheduler.snapshot()).code
                != Code::stale_state)
        {
            fs::remove_all(root, ec);
            return false;
        }
        const auto synchronized = submit(control,
            command(CommandKind::synchronize, "sync-1", control, local,
                retry_time + 1u), local);
        const auto cancelled = submit(control,
            command(CommandKind::cancel, "cancel-1", control, local,
                retry_time + 2u), local);
        if (synchronized.code != Code::synchronized
            || cancelled.code != Code::cancelled
            || cancelled.receipt.previous_receipt_sha256
                != synchronized.receipt.receipt_sha256
            || local.queue.snapshot().items.front().phase
                != iteration_campaign_queue::ItemPhase::cancelled
            || !hard_false(control.snapshot()))
        {
            fs::remove_all(root, ec);
            return false;
        }

        // Local response review: approval affects only the queue's explicit
        // campaign admission state and never grants source/promotion authority.
        Fixture approval{root / "a", "supervisor-approval-transport"};
        approval.now = local.now + 100u;
        Surface approval_control{};
        if (!initialize(approval,
                project_profile::Provider::epoch_local_qwen38, 1u)
            || !advance_to_review(approval)
            || !approval_control.begin(control_configuration(approval),
                approval.queue.snapshot(), approval.scheduler.snapshot()))
        {
            fs::remove_all(root, ec);
            return false;
        }
        Command wrong_operation = command(CommandKind::approve, "approve-wrong",
            approval_control, approval, approval.now + 13u);
        wrong_operation.operation_id = "unknown-operation";
        if (submit(approval_control, wrong_operation, approval).code
                != Code::unknown_identifier)
        {
            fs::remove_all(root, ec);
            return false;
        }
        const Command approve_command = command(CommandKind::approve, "approve-1",
            approval_control, approval, approval.now + 13u);
        const auto approved = submit(approval_control, approve_command, approval);
        const auto approval_checkpoint = approval_control.checkpoint();
        Surface approval_restored{};
        if (approved.code != Code::approved
            || approval.queue.snapshot().items.front().phase
                != iteration_campaign_queue::ItemPhase::approved_for_campaign
            || submit(approval_control, approve_command, approval).code
                != Code::replay_refused
            || !approval_checkpoint
            || !approval_restored.restore(approval_checkpoint.bytes,
                approval.now + 14u, approval.queue.snapshot(),
                approval.scheduler.snapshot())
            || !hard_false(approval_restored.snapshot()))
        {
            fs::remove_all(root, ec);
            return false;
        }

        // External MCP follows the identical control contract and rejection is
        // terminal for the response without any transport or source action.
        Fixture rejection{root / "r", "supervisor-rejection-transport"};
        rejection.now = local.now + 200u;
        Surface rejection_control{};
        if (!initialize(rejection, project_profile::Provider::external_mcp, 1u)
            || !advance_to_review(rejection)
            || !rejection_control.begin(control_configuration(rejection),
                rejection.queue.snapshot(), rejection.scheduler.snapshot()))
        {
            fs::remove_all(root, ec);
            return false;
        }
        const auto rejected = submit(rejection_control,
            command(CommandKind::reject, "reject-1", rejection_control,
                rejection, rejection.now + 13u), rejection);
        const bool ok = rejected.code == Code::rejected
            && rejection.queue.snapshot().items.front().phase
                == iteration_campaign_queue::ItemPhase::rejected
            && rejected.receipt.campaign_id
                == rejection.orchestrator.snapshot().campaign.campaign_id
            && rejected.receipt.operation_id
                == rejection.scheduler.snapshot().pending_operation_id
            && rejected.receipt.receipt_sha256.size() == 64u
            && hard_false(rejection_control.snapshot());
        fs::remove_all(root, ec);
        return ok;
    }
}

#if defined(EPOCH_AI_ITERATION_SUPERVISOR_CONTROL_CONTRACT_MAIN)
int main()
{
    return epochengine::ai::iteration_supervisor_control::run_contract() ? 0 : 1;
}
#endif
