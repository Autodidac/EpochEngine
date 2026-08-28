/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

module ai.mcp_orchestrator_bridge;

import ai.iteration_campaign_scheduler;
import ai.iteration_supervisor_control;

namespace epochengine::ai::mcp_orchestrator_bridge
{
    namespace
    {
        [[nodiscard]] mcp_stdio::RequestId request(std::string value)
        {
            return mcp_stdio::RequestId{
                .kind = mcp_stdio::RequestIdKind::string,
                .string_value = std::move(value)};
        }

        [[nodiscard]] McpToolCall call(
            const mcp_stdio::RequestId& id,
            std::string tool,
            std::vector<McpArgument> arguments = {})
        {
            return McpToolCall{
                .session_id = "bridge-contract-session",
                .call_id = "rpc:" + id.key(),
                .tool = std::move(tool),
                .arguments = std::move(arguments)};
        }

        [[nodiscard]] McpSessionAuthority authority(
            const McpToolCall& call,
            const bool engine = false)
        {
            McpToolCapability capabilities = McpToolCapability::inspect
                | McpToolCapability::author | McpToolCapability::build
                | McpToolCapability::execute | McpToolCapability::diagnose
                | McpToolCapability::capture;
            if (engine) capabilities = capabilities | McpToolCapability::engine_source;
            return McpSessionAuthority{
                .session_id = call.session_id,
                .granted_capabilities = capabilities,
                .approved_call_ids = {call.call_id}};
        }

        [[nodiscard]] HostApproval approval(
            const PendingReceipt& receipt,
            const std::uint64_t now)
        {
            return HostApproval{
                .receipt_id = receipt.receipt_id(),
                .connection_generation = receipt.connection_generation(),
                .expected_orchestrator_generation =
                    receipt.expected_orchestrator_generation(),
                .expected_state_sha256 = receipt.expected_state_sha256(),
                .now_unix_seconds = now,
                .operator_approved = true};
        }

        [[nodiscard]] HostOperationResult operation_result(
            const self_iteration_orchestrator::PendingOperation& operation,
            const std::uint64_t now,
            std::string content,
            std::string evidence,
            std::string summary,
            const bool passed = true)
        {
            return HostOperationResult{
                .operation_id = operation.operation_id(),
                .expected_generation = operation.expected_generation(),
                .expected_state_sha256 = operation.expected_state_sha256(),
                .transition_id = operation.transition_id(),
                .now_unix_seconds = now,
                .content = std::move(content),
                .evidence_sha256 = std::move(evidence),
                .summary = std::move(summary),
                .passed = passed,
                .operator_approved = true};
        }

        [[nodiscard]] Result propose_and_approve(
            Bridge& bridge,
            self_iteration_orchestrator::Orchestrator& orchestrator,
            std::string id,
            MutationIntent intent,
            const std::uint64_t now)
        {
            const auto request_id = request(std::move(id));
            Result proposed = bridge.propose_mutation(
                request_id, "rpc:" + request_id.key(), std::move(intent), orchestrator);
            return proposed && proposed.pending_receipt
                ? bridge.approve_receipt(approval(*proposed.pending_receipt, now), orchestrator)
                : Result{};
        }
    }

    bool run_contract()
    {
        namespace fs = std::filesystem;
        using namespace iteration_session;
        using self_iteration_orchestrator::HostBinding;
        constexpr std::uint64_t now = 2'200'000'000u;
        const auto sequence = std::chrono::steady_clock::now()
            .time_since_epoch().count();
        const fs::path root = fs::temp_directory_path()
            / ("eai_bridge_" + std::to_string(sequence));
        const fs::path project = root / "Project";
        const fs::path cache = root / "cache/ai";
        std::error_code ec{};
        fs::create_directories(project / "Source", ec);
        std::ofstream{project / "Source/main.cpp", std::ios::binary}
            << "int bridge_contract() { return 38; }\n";
        const auto profile = project_profile::serialize_profile(
            project_profile::make_profile(
                project_profile::Provider::epoch_local_qwen38));
        const SourceAuthority source{
            .target_kind = IterationTargetKind::project_source,
            .kind = SourceAuthorityKind::verified_project,
            .root = fs::weakly_canonical(project, ec),
            .project_id = "mcp_bridge_contract",
            .project_manifest_digest = std::string(64u, 'a'),
            .project_profile_digest = profile.sha256,
            .verified = !ec};
        const auto files = inspect_curated_files(source, {"Source/main.cpp"});
        const HostBinding local_host{
            .binding_id = "local-child-binding",
            .configuration_sha256 = std::string(64u, 'b'),
            .generation = 4u,
            .stdio_only = true};
        self_iteration_orchestrator::Configuration configuration{
            .authority = source,
            .curated_files = files.files,
            .cache_root = cache,
            .objective = "Improve only the admitted generated-project source.",
            .provider = project_profile::Provider::epoch_local_qwen38,
            .project_profile_bytes = profile.canonical_bytes,
            .host = local_host,
            .budgets = iteration_campaign::default_budgets(
                IterationTargetKind::project_source),
            .created_at_unix_seconds = now,
            .project_source_campaign_permitted = true,
            .sandbox_apply_permitted = true};
        self_iteration_orchestrator::Orchestrator orchestrator{};
        auto begun = files.accepted ? orchestrator.begin(configuration)
            : self_iteration_orchestrator::Result{};
        Bridge bridge{"bridge-contract-session"};
        Result connected = begun
            ? bridge.connect("local-connection", 1u, begun.snapshot) : Result{};
        if (!connected || bridge.connection_state() != ConnectionState::connected)
        {
            fs::remove_all(root, ec);
            return false;
        }

        const auto inspect_id = request("inspect-1");
        const auto inspect_call = call(inspect_id, std::string{mcp_campaign::kInspectTargetTool},
            {McpArgument{"target_handle", begun.snapshot.orchestrator_id}});
        Result inspected = bridge.dispatch(inspect_id, inspect_call,
            authority(inspect_call), orchestrator, now + 1u);
        Result duplicate_inspect = bridge.dispatch(inspect_id, inspect_call,
            authority(inspect_call), orchestrator, now + 1u);
        if (!inspected || inspected.tool_result.state != McpCallState::succeeded
            || duplicate_inspect.code != Code::replay_denied)
        {
            fs::remove_all(root, ec);
            return false;
        }

        const auto start_id = request("start-1");
        const auto start_call = call(start_id, std::string{mcp_campaign::kStartTool},
            {McpArgument{"target_handle", begun.snapshot.orchestrator_id},
                McpArgument{"objective", begun.snapshot.campaign.session.objective},
                McpArgument{"model", begun.snapshot.campaign.session.model_name}});
        Result start = bridge.dispatch(start_id, start_call,
            authority(start_call), orchestrator, now + 2u);
        if (!start || !start.pending_receipt
            || start.code != Code::pending_host_approval)
        {
            fs::remove_all(root, ec);
            return false;
        }
        HostApproval stale_approval = approval(*start.pending_receipt, now + 3u);
        stale_approval.expected_state_sha256 = std::string(64u, '0');
        if (bridge.approve_receipt(stale_approval, orchestrator).code
            != Code::stale_state)
        {
            fs::remove_all(root, ec);
            return false;
        }
        Result plan_pending = bridge.approve_receipt(
            approval(*start.pending_receipt, now + 3u), orchestrator);
        if (!plan_pending || !plan_pending.pending_operation
            || plan_pending.pending_operation->transport()
                != self_iteration_orchestrator::TransportKind::guarded_local_mcp_child)
        {
            fs::remove_all(root, ec);
            return false;
        }

        // Disconnect preserves replay/pending bindings but blocks out-of-band
        // completion until a generation-bound reconnect.
        if (!bridge.disconnect("Simulated MCP transport disconnect.",
                orchestrator.snapshot())
            || bridge.complete_operation(*plan_pending.pending_operation,
                operation_result(*plan_pending.pending_operation, now + 4u,
                    "plan", {}, "Disconnected plan result."), orchestrator).code
                != Code::disconnected)
        {
            fs::remove_all(root, ec);
            return false;
        }
        if (!bridge.connect("local-reconnect", 2u, orchestrator.snapshot()))
        {
            fs::remove_all(root, ec);
            return false;
        }
        Result planned = bridge.complete_operation(*plan_pending.pending_operation,
            operation_result(*plan_pending.pending_operation, now + 5u,
                "Inspect exact scope, propose one patch, validate, checkpoint.", {},
                "Local child returned a bounded plan."), orchestrator);
        if (!planned
            || bridge.complete_operation(*plan_pending.pending_operation,
                operation_result(*plan_pending.pending_operation, now + 5u,
                    "replay", {}, "Replay."), orchestrator).code
                != Code::stale_state)
        {
            fs::remove_all(root, ec);
            return false;
        }

        Result shared = propose_and_approve(bridge, orchestrator, "share-1",
            MutationIntent{
                .kind = MutationKind::share_curated_evidence,
                .scope_sha256 = planned.snapshot.campaign.session.scope_digest,
                .evidence_sha256 = std::string(64u, 'c'),
                .summary = "Operator shared the exact curated scope."}, now + 6u);
        Result proposal_pending = shared ? propose_and_approve(
            bridge, orchestrator, "proposal-1",
            MutationIntent{.kind = MutationKind::request_proposal,
                .summary = "Request one exact proposal."}, now + 7u) : Result{};
        if (!proposal_pending || !proposal_pending.pending_operation)
        {
            fs::remove_all(root, ec);
            return false;
        }
        HostOperationResult wrong_order = operation_result(
            *proposal_pending.pending_operation, now + 8u,
            "candidate", {}, "Wrong result identity.");
        wrong_order.operation_id = std::string(64u, 'f');
        if (bridge.complete_operation(*proposal_pending.pending_operation,
                std::move(wrong_order), orchestrator).code != Code::stale_state)
        {
            fs::remove_all(root, ec);
            return false;
        }
        Result proposed = bridge.complete_operation(*proposal_pending.pending_operation,
            operation_result(*proposal_pending.pending_operation, now + 8u,
                "candidate: replace exact admitted function body",
                {}, "Local child returned one bounded proposal."), orchestrator);
        Result reviewed = proposed ? propose_and_approve(
            bridge, orchestrator, "review-1",
            MutationIntent{.kind = MutationKind::review_proposal,
                .summary = "Operator reviewed the exact proposal digest.",
                .approve = true}, now + 9u) : Result{};
        Result apply_pending = reviewed ? propose_and_approve(
            bridge, orchestrator, "apply-1",
            MutationIntent{.kind = MutationKind::decide_apply,
                .summary = "Operator approved disposable-workspace apply only.",
                .approve = true}, now + 10u) : Result{};
        Result applied = apply_pending && apply_pending.pending_operation
            ? bridge.complete_operation(*apply_pending.pending_operation,
                operation_result(*apply_pending.pending_operation, now + 11u,
                    {}, std::string(64u, 'd'),
                    "Fake host recorded sandbox implementation evidence."),
                orchestrator) : Result{};
        if (!applied)
        {
            fs::remove_all(root, ec);
            return false;
        }

        Result current = applied;
        constexpr char validation_hex[] = {'1', '2', '3', '4', '5', '6', '7'};
        for (std::uint32_t index = 0u; index < 7u; ++index)
        {
            const auto validation_id = request("validate-" + std::to_string(index));
            const auto validation_call = call(validation_id,
                std::string{mcp_campaign::kValidateCandidateTool},
                {McpArgument{"campaign_id", current.snapshot.campaign.campaign_id}});
            Result validation_receipt = bridge.dispatch(validation_id,
                validation_call, authority(validation_call), orchestrator,
                now + 12u + index * 3u);
            Result validation_pending = validation_receipt.pending_receipt
                ? bridge.approve_receipt(approval(*validation_receipt.pending_receipt,
                    now + 13u + index * 3u), orchestrator) : Result{};
            if (!validation_pending || !validation_pending.pending_operation
                || validation_pending.pending_operation->validation_index() != index)
            {
                fs::remove_all(root, ec);
                return false;
            }
            current = bridge.complete_operation(*validation_pending.pending_operation,
                operation_result(*validation_pending.pending_operation,
                    now + 14u + index * 3u, {},
                    std::string(64u, validation_hex[index]),
                    "Fake trusted host validated the exact candidate.", true),
                orchestrator);
            if (!current)
            {
                fs::remove_all(root, ec);
                return false;
            }
        }
        Result checkpointed = propose_and_approve(bridge, orchestrator,
            "checkpoint-1", MutationIntent{
                .kind = MutationKind::checkpoint,
                .summary = "Operator recorded a verified rollback checkpoint."}, now + 40u);
        const auto status_id = request("status-1");
        const auto status_call = call(status_id, std::string{mcp_campaign::kStatusTool},
            {McpArgument{"campaign_id", checkpointed.snapshot.campaign.campaign_id}});
        Result status = checkpointed ? bridge.dispatch(status_id, status_call,
            authority(status_call), orchestrator, now + 41u) : Result{};
        auto notifications = bridge.drain_notifications();
        if (!checkpointed
            || checkpointed.snapshot.phase
                != self_iteration_orchestrator::Phase::checkpointed
            || !status || notifications.empty())
        {
            fs::remove_all(root, ec);
            return false;
        }
        for (std::size_t index = 1u; index < notifications.size(); ++index)
            if (notifications[index].sequence != notifications[index - 1u].sequence + 1u)
            {
                fs::remove_all(root, ec);
                return false;
            }

        // An external-MCP profile remains independently bound and cannot be
        // driven through the local bridge or inherit its receipts.
        const auto external_profile = project_profile::serialize_profile(
            project_profile::make_profile(project_profile::Provider::external_mcp));
        SourceAuthority external_source = source;
        external_source.project_id = "mcp_bridge_external";
        external_source.project_profile_digest = external_profile.sha256;
        auto external_configuration = configuration;
        external_configuration.authority = external_source;
        external_configuration.provider = project_profile::Provider::external_mcp;
        external_configuration.project_profile_bytes = external_profile.canonical_bytes;
        external_configuration.operator_model_binding = "Qwen3.8-27B-external-mcp";
        external_configuration.host = HostBinding{
            .binding_id = "external-mcp-binding",
            .configuration_sha256 = std::string(64u, 'e'),
            .generation = 2u,
            .stdio_only = true};
        external_configuration.created_at_unix_seconds = now + 50u;
        self_iteration_orchestrator::Orchestrator external_orchestrator{};
        auto external_begun = external_orchestrator.begin(external_configuration);
        if (!external_begun
            || bridge.propose_mutation(request("cross-authority"),
                "rpc:s:cross-authority",
                MutationIntent{.kind = MutationKind::cancel,
                    .summary = "Cross authority must fail."},
                external_orchestrator).code != Code::stale_state)
        {
            fs::remove_all(root, ec);
            return false;
        }
        Bridge external_bridge{"bridge-contract-session"};
        Result external_connected = external_bridge.connect(
            "external-connection", 1u, external_begun.snapshot);
        const auto external_start_id = request("external-start");
        const auto external_start_call = call(external_start_id,
            std::string{mcp_campaign::kStartTool},
            {McpArgument{"target_handle", external_begun.snapshot.orchestrator_id},
                McpArgument{"objective", external_begun.snapshot.campaign.session.objective},
                McpArgument{"model", external_begun.snapshot.campaign.session.model_name}});
        Result external_receipt = external_connected ? external_bridge.dispatch(
            external_start_id, external_start_call, authority(external_start_call),
            external_orchestrator, now + 51u) : Result{};
        Result external_plan = external_receipt.pending_receipt
            ? external_bridge.approve_receipt(
                approval(*external_receipt.pending_receipt, now + 52u),
                external_orchestrator) : Result{};
        Result cancelled = external_plan ? propose_and_approve(
            external_bridge, external_orchestrator, "external-cancel",
            MutationIntent{.kind = MutationKind::cancel,
                .summary = "Operator cancelled the external campaign."}, now + 53u)
            : Result{};
        const bool ok = external_plan && external_plan.pending_operation
            && external_plan.pending_operation->transport()
                == self_iteration_orchestrator::TransportKind::external_mcp
            && cancelled
            && cancelled.snapshot.phase == self_iteration_orchestrator::Phase::cancelled
            && external_bridge.complete_operation(*external_plan.pending_operation,
                operation_result(*external_plan.pending_operation, now + 54u,
                    "late plan", {}, "Late result."), external_orchestrator).code
                == Code::stale_state;
        fs::remove_all(root, ec);
        return ok && iteration_campaign_scheduler::run_contract()
            && iteration_supervisor_control::run_contract();
    }
}
