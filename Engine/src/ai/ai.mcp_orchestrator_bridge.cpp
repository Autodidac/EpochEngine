/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

module ai.mcp_orchestrator_bridge;

import core.sha256;

namespace epochengine::ai::mcp_orchestrator_bridge
{
    namespace
    {
        [[nodiscard]] std::string digest_text(const std::string_view text)
        {
            return core::sha256::hex(core::sha256::hash(text));
        }

        [[nodiscard]] bool lowercase_hex(
            const std::string_view text,
            const std::size_t size)
        {
            return text.size() == size
                && std::all_of(text.begin(), text.end(), [](const char value)
                {
                    return (value >= '0' && value <= '9')
                        || (value >= 'a' && value <= 'f');
                });
        }

        [[nodiscard]] bool identifier(const std::string_view text)
        {
            return !text.empty() && text.size() <= 96u
                && std::all_of(text.begin(), text.end(), [](const char value)
                {
                    return (value >= 'a' && value <= 'z')
                        || (value >= 'A' && value <= 'Z')
                        || (value >= '0' && value <= '9')
                        || value == '.' || value == '_' || value == '-';
                });
        }

        [[nodiscard]] std::string json_string(const std::string_view text)
        {
            std::string result{"\""};
            for (const unsigned char value : text)
            {
                switch (value)
                {
                case '\\': result += "\\\\"; break;
                case '"': result += "\\\""; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:
                    if (value >= 0x20u) result.push_back(static_cast<char>(value));
                    break;
                }
            }
            result.push_back('"');
            return result;
        }

        [[nodiscard]] const McpArgument* find_argument(
            const McpToolCall& call,
            const std::string_view name)
        {
            const auto found = std::find_if(call.arguments.begin(), call.arguments.end(),
                [&](const McpArgument& argument) { return argument.name == name; });
            return found == call.arguments.end() ? nullptr : &*found;
        }

        [[nodiscard]] bool exact_arguments(
            const McpToolCall& call,
            const std::initializer_list<std::string_view> expected)
        {
            if (call.arguments.size() != expected.size()) return false;
            for (const auto name : expected)
            {
                std::size_t count{};
                for (const auto& argument : call.arguments)
                    if (argument.name == name) ++count;
                if (count != 1u) return false;
            }
            return true;
        }

        [[nodiscard]] std::string_view argument(
            const McpToolCall& call,
            const std::string_view name)
        {
            const auto* value = find_argument(call, name);
            return value ? std::string_view{value->value} : std::string_view{};
        }

        [[nodiscard]] McpErrorCode error_for(const Code code)
        {
            switch (code)
            {
            case Code::capability_denied: return McpErrorCode::capability_denied;
            case Code::approval_required: return McpErrorCode::approval_required;
            case Code::cancelled: return McpErrorCode::cancelled;
            case Code::budget_exhausted: return McpErrorCode::budget_exhausted;
            case Code::stale_state:
            case Code::out_of_order: return McpErrorCode::evidence_missing;
            case Code::ready:
            case Code::pending_host_approval: return McpErrorCode::none;
            default: return McpErrorCode::invalid_request;
            }
        }

        [[nodiscard]] std::string phase_name(
            const self_iteration_orchestrator::Phase phase)
        {
            using self_iteration_orchestrator::Phase;
            switch (phase)
            {
            case Phase::idle: return "idle";
            case Phase::awaiting_plan_request: return "awaiting_plan_request";
            case Phase::awaiting_plan_result: return "awaiting_plan_result";
            case Phase::awaiting_curated_evidence: return "awaiting_curated_evidence";
            case Phase::awaiting_proposal_request: return "awaiting_proposal_request";
            case Phase::awaiting_proposal_result: return "awaiting_proposal_result";
            case Phase::awaiting_manual_review: return "awaiting_manual_review";
            case Phase::awaiting_apply_decision: return "awaiting_apply_decision";
            case Phase::awaiting_apply_result: return "awaiting_apply_result";
            case Phase::awaiting_validation_request: return "awaiting_validation_request";
            case Phase::awaiting_validation_result: return "awaiting_validation_result";
            case Phase::checkpoint_ready: return "checkpoint_ready";
            case Phase::checkpointed: return "checkpointed";
            case Phase::rejected: return "rejected";
            case Phase::cancelled: return "cancelled";
            case Phase::blocked: return "blocked";
            }
            return "blocked";
        }

        [[nodiscard]] McpToolResult tool_result(
            const McpToolCall* call,
            const McpCallState state,
            const McpErrorCode error,
            std::string output,
            std::vector<McpEvidenceAttachment> evidence = {})
        {
            return McpToolResult{
                .session_id = call ? call->session_id : std::string{},
                .call_id = call ? call->call_id : std::string{},
                .state = state,
                .error = error,
                .output = std::move(output),
                .evidence = std::move(evidence)};
        }

        [[nodiscard]] std::string action_transition(const std::string& receipt_id)
        {
            return "bridge-" + receipt_id.substr(0u, 32u);
        }
    }

    const std::string& PendingReceipt::receipt_id() const noexcept { return receipt_id_; }
    MutationKind PendingReceipt::kind() const noexcept { return kind_; }
    const std::string& PendingReceipt::transport_session_id() const noexcept { return transport_session_id_; }
    const std::string& PendingReceipt::connection_id() const noexcept { return connection_id_; }
    std::uint64_t PendingReceipt::connection_generation() const noexcept { return connection_generation_; }
    const std::string& PendingReceipt::request_key() const noexcept { return request_key_; }
    const std::string& PendingReceipt::call_id() const noexcept { return call_id_; }
    const std::string& PendingReceipt::orchestrator_id() const noexcept { return orchestrator_id_; }
    const std::string& PendingReceipt::campaign_id() const noexcept { return campaign_id_; }
    std::uint64_t PendingReceipt::campaign_generation() const noexcept { return campaign_generation_; }
    iteration_session::RequestIdentity PendingReceipt::session_identity() const noexcept { return session_identity_; }
    std::uint64_t PendingReceipt::expected_orchestrator_generation() const noexcept { return expected_orchestrator_generation_; }
    const std::string& PendingReceipt::expected_state_sha256() const noexcept { return expected_state_sha256_; }
    const MutationIntent& PendingReceipt::intent() const noexcept { return intent_; }

    Bridge::Bridge(std::string transport_session_id, const Limits limits)
        : transport_session_id_(std::move(transport_session_id)), limits_(limits)
    {
        if (!identifier(transport_session_id_) || !limits_.valid())
        {
            connection_state_ = ConnectionState::cancelled;
            transport_session_id_.clear();
        }
    }

    void Bridge::notify(
        const NotificationKind kind,
        std::string request_key,
        std::string receipt_id,
        std::string operation_id,
        const self_iteration_orchestrator::Snapshot& snapshot,
        std::string evidence_sha256,
        std::string message)
    {
        if (notifications_.size() >= limits_.maximum_notifications)
            notifications_.erase(notifications_.begin());
        notifications_.push_back(Notification{
            .sequence = ++next_notification_sequence_,
            .kind = kind,
            .request_key = std::move(request_key),
            .receipt_id = std::move(receipt_id),
            .operation_id = std::move(operation_id),
            .campaign_id = snapshot.campaign.campaign_id,
            .campaign_generation = snapshot.campaign.record_generation,
            .state_sha256 = snapshot.state_sha256,
            .evidence_sha256 = std::move(evidence_sha256),
            .message = std::move(message)});
    }

    Result Bridge::reject(
        const Code code,
        const McpToolCall* call,
        self_iteration_orchestrator::Snapshot snapshot,
        std::string status)
    {
        notify(code == Code::cancelled ? NotificationKind::cancelled
                : NotificationKind::rejected,
            call ? call->call_id : std::string{}, {}, {}, snapshot,
            digest_text(status), status);
        return Result{
            .code = code,
            .tool_result = tool_result(call, code == Code::cancelled
                    ? McpCallState::cancelled : McpCallState::rejected,
                error_for(code),
                "{\"schema\":\"epoch.ai.mcp_orchestrator_bridge.v1\",\"status\":"
                    + json_string(status) + "}"),
            .snapshot = std::move(snapshot),
            .status = std::move(status)};
    }

    Result Bridge::connect(
        std::string connection_id,
        const std::uint64_t generation,
        const self_iteration_orchestrator::Snapshot& snapshot)
    {
        if (transport_session_id_.empty() || connection_state_ == ConnectionState::cancelled
            || connection_state_ == ConnectionState::connected
            || !identifier(connection_id) || generation == 0u
            || !lowercase_hex(snapshot.state_sha256, 64u)
            || !lowercase_hex(snapshot.orchestrator_id, 64u)
            || (connection_generation_ != 0u && generation != connection_generation_ + 1u)
            || (!bound_orchestrator_id_.empty()
                && bound_orchestrator_id_ != snapshot.orchestrator_id))
            return reject(Code::stale_state, nullptr, snapshot,
                "Connection or reconnect proof is stale, malformed, or crosses orchestrator authority.");
        connection_id_ = std::move(connection_id);
        connection_generation_ = generation;
        connection_state_ = ConnectionState::connected;
        bound_orchestrator_id_ = snapshot.orchestrator_id;
        last_state_sha256_ = snapshot.state_sha256;
        notify(NotificationKind::connected, {}, {}, {}, snapshot,
            snapshot.state_sha256,
            generation == 1u ? "Bridge connected to one exact orchestrator state."
                : "Bridge reconnected; request replay history and pending receipt bindings were preserved.");
        return Result{.code = Code::ready,
            .snapshot = snapshot,
            .status = "Bridge connection accepted."};
    }

    Result Bridge::disconnect(
        std::string reason,
        const self_iteration_orchestrator::Snapshot& snapshot)
    {
        if (connection_state_ != ConnectionState::connected
            || reason.empty() || reason.size() > limits_.maximum_summary_bytes
            || snapshot.orchestrator_id != bound_orchestrator_id_)
            return reject(Code::stale_state, nullptr, snapshot,
                "Disconnect rejected invalid connection or authority state.");
        connection_state_ = ConnectionState::disconnected;
        last_state_sha256_ = snapshot.state_sha256;
        notify(NotificationKind::disconnected, {}, {}, {}, snapshot,
            digest_text(reason), std::move(reason));
        return Result{.code = Code::ready, .snapshot = snapshot,
            .status = "Bridge disconnected without consuming pending work."};
    }

    Result Bridge::make_pending(
        const mcp_stdio::RequestId& request_id,
        std::string call_id,
        MutationIntent intent,
        self_iteration_orchestrator::Orchestrator& orchestrator)
    {
        const auto snapshot = orchestrator.snapshot();
        if (connection_state_ != ConnectionState::connected)
            return reject(Code::disconnected, nullptr, snapshot,
                "Mutation proposal requires one connected bridge.");
        if (snapshot.orchestrator_id != bound_orchestrator_id_)
            return reject(Code::stale_state, nullptr, snapshot,
                "Mutation proposal crossed the bridge's bound orchestrator authority.");
        if (!request_id.valid() || request_id.kind == mcp_stdio::RequestIdKind::absent
            || call_id != "rpc:" + request_id.key()
            || intent.summary.empty()
            || intent.summary.size() > limits_.maximum_summary_bytes)
            return reject(Code::invalid_request, nullptr, snapshot,
                "Mutation identity or bounded intent is malformed.");
        const std::string request_key = transport_session_id_ + "\n" + request_id.key();
        if (std::find(seen_request_keys_.begin(), seen_request_keys_.end(), request_key)
            != seen_request_keys_.end())
            return reject(Code::replay_denied, nullptr, snapshot,
                "This exact transport request identity was already consumed.");
        if (seen_request_keys_.size() >= limits_.maximum_seen_requests
            || pending_.size() >= limits_.maximum_pending_receipts)
            return reject(Code::budget_exhausted, nullptr, snapshot,
                "Bridge request or pending-receipt budget is exhausted.");
        seen_request_keys_.push_back(request_key);
        PendingReceipt receipt{};
        receipt.kind_ = intent.kind;
        receipt.transport_session_id_ = transport_session_id_;
        receipt.connection_id_ = connection_id_;
        receipt.connection_generation_ = connection_generation_;
        receipt.request_key_ = request_id.key();
        receipt.call_id_ = std::move(call_id);
        receipt.orchestrator_id_ = snapshot.orchestrator_id;
        receipt.campaign_id_ = snapshot.campaign.campaign_id;
        receipt.campaign_generation_ = snapshot.campaign.record_generation;
        receipt.session_identity_ = snapshot.campaign.session.identity;
        receipt.expected_orchestrator_generation_ = snapshot.generation;
        receipt.expected_state_sha256_ = snapshot.state_sha256;
        receipt.intent_ = std::move(intent);
        receipt.receipt_id_ = digest_text(receipt.transport_session_id_ + "\n"
            + receipt.connection_id_ + "\n" + std::to_string(receipt.connection_generation_)
            + "\n" + receipt.request_key_ + "\n" + receipt.call_id_ + "\n"
            + receipt.orchestrator_id_ + "\n" + receipt.campaign_id_ + "\n"
            + std::to_string(receipt.expected_orchestrator_generation_) + "\n"
            + receipt.expected_state_sha256_ + "\n"
            + std::to_string(static_cast<unsigned>(receipt.kind_)));
        pending_.push_back(PendingState{.receipt = receipt});
        notify(NotificationKind::pending, receipt.request_key_, receipt.receipt_id_, {},
            snapshot, digest_text(receipt.receipt_id_ + "\n" + receipt.expected_state_sha256_),
            "Mutation remains pending exact host/operator approval; bridge executed nothing.");
        const std::string output =
            "{\"schema\":\"epoch.ai.mcp_orchestrator_bridge.v1\",\"state\":\"pending_host_approval\",\"receipt_id\":"
            + json_string(receipt.receipt_id_) + ",\"campaign_id\":"
            + json_string(receipt.campaign_id_) + ",\"campaign_generation\":"
            + std::to_string(receipt.campaign_generation_) + "}";
        McpToolCall call{.session_id = transport_session_id_,
            .call_id = receipt.call_id_};
        return Result{
            .code = Code::pending_host_approval,
            .tool_result = tool_result(&call, McpCallState::awaiting_approval,
                McpErrorCode::none, output),
            .snapshot = snapshot,
            .pending_receipt = std::move(receipt),
            .status = "Mutation awaits exact host approval receipt."};
    }

    Result Bridge::propose_mutation(
        const mcp_stdio::RequestId& request_id,
        std::string call_id,
        MutationIntent intent,
        self_iteration_orchestrator::Orchestrator& orchestrator)
    {
        return make_pending(request_id, std::move(call_id),
            std::move(intent), orchestrator);
    }

    Result Bridge::dispatch(
        const mcp_stdio::RequestId& request_id,
        const McpToolCall& call,
        const McpSessionAuthority& authority,
        self_iteration_orchestrator::Orchestrator& orchestrator,
        const std::uint64_t now)
    {
        const auto snapshot = orchestrator.snapshot();
        if (connection_state_ != ConnectionState::connected)
            return reject(Code::disconnected, &call, snapshot,
                "MCP dispatch requires one connected bridge.");
        if (snapshot.orchestrator_id != bound_orchestrator_id_)
            return reject(Code::stale_state, &call, snapshot,
                "MCP dispatch crossed the bridge's bound orchestrator authority.");
        if (!request_id.valid() || request_id.kind == mcp_stdio::RequestIdKind::absent
            || call.session_id != transport_session_id_
            || call.call_id != "rpc:" + request_id.key())
            return reject(Code::invalid_request, &call, snapshot,
                "Request, call, and transport session identities are not exactly bound.");
        if (call.cancellation_requested || authority.cancellation_requested)
            return reject(Code::cancelled, &call, snapshot,
                "Transport or call cancellation was already requested.");
        McpToolRegistry registry = mcp_campaign::make_tool_registry();
        const auto validation = registry.validate(call, authority);
        if (!validation)
        {
            const Code code = validation.error == McpErrorCode::capability_denied
                ? Code::capability_denied
                : validation.error == McpErrorCode::approval_required
                    ? Code::approval_required : Code::invalid_request;
            return reject(code, &call, snapshot, validation.message);
        }
        if (snapshot.campaign.session.source.target_kind
                == iteration_session::IterationTargetKind::engine_source
            && !has_capability(authority.granted_capabilities,
                McpToolCapability::engine_source))
            return reject(Code::capability_denied, &call, snapshot,
                "Engine-source orchestration requires explicit engine-source capability.");
        const std::string seen_key = transport_session_id_ + "\n" + request_id.key();
        const auto already_seen = [&]
        {
            return std::find(seen_request_keys_.begin(), seen_request_keys_.end(), seen_key)
                != seen_request_keys_.end();
        };
        if (call.tool == mcp_campaign::kInspectTargetTool)
        {
            if (!exact_arguments(call, {"target_handle"})
                || argument(call, "target_handle") != snapshot.orchestrator_id)
                return reject(Code::stale_state, &call, snapshot,
                    "Inspection target handle does not match this orchestrator authority.");
            if (already_seen()) return reject(Code::replay_denied, &call, snapshot,
                "Read-only request identity was already consumed.");
            seen_request_keys_.push_back(seen_key);
            const std::string output =
                "{\"schema\":\"epoch.ai.mcp_orchestrator_bridge.v1\",\"orchestrator_id\":"
                + json_string(snapshot.orchestrator_id) + ",\"target_key\":"
                + json_string(snapshot.target_key) + ",\"target_kind\":"
                + json_string(snapshot.campaign.session.source.target_kind
                    == iteration_session::IterationTargetKind::engine_source
                        ? "engine_source" : "project_source")
                + ",\"state_sha256\":" + json_string(snapshot.state_sha256) + "}";
            return Result{.code = Code::ready,
                .tool_result = tool_result(&call, McpCallState::succeeded,
                    McpErrorCode::none, output,
                    {McpEvidenceAttachment{.kind = "orchestrator_authority",
                        .content_hash = snapshot.state_sha256,
                        .summary = "Opaque orchestrator authority and state digest; no paths or source bytes."}}),
                .snapshot = snapshot, .status = "Read-only authority snapshot returned."};
        }
        if (call.tool == mcp_campaign::kStatusTool)
        {
            if (!exact_arguments(call, {"campaign_id"})
                || argument(call, "campaign_id") != snapshot.campaign.campaign_id)
                return reject(Code::stale_state, &call, snapshot,
                    "Status campaign identity is stale or cross-authority.");
            if (already_seen()) return reject(Code::replay_denied, &call, snapshot,
                "Read-only request identity was already consumed.");
            seen_request_keys_.push_back(seen_key);
            const std::string output =
                "{\"schema\":\"epoch.ai.mcp_orchestrator_bridge.v1\",\"campaign_id\":"
                + json_string(snapshot.campaign.campaign_id)
                + ",\"campaign_generation\":"
                + std::to_string(snapshot.campaign.record_generation)
                + ",\"orchestrator_generation\":" + std::to_string(snapshot.generation)
                + ",\"state_sha256\":" + json_string(snapshot.state_sha256)
                + ",\"phase\":" + json_string(phase_name(snapshot.phase)) + "}";
            return Result{.code = Code::ready,
                .tool_result = tool_result(&call, McpCallState::succeeded,
                    McpErrorCode::none, output,
                    {McpEvidenceAttachment{.kind = "orchestrator_state",
                        .content_hash = snapshot.state_sha256,
                        .summary = "Bounded orchestrator progress and digest evidence."}}),
                .snapshot = snapshot, .status = "Read-only orchestrator status returned."};
        }

        MutationIntent intent{};
        if (call.tool == mcp_campaign::kStartTool)
        {
            if (!exact_arguments(call, {"target_handle", "objective", "model"})
                || argument(call, "target_handle") != snapshot.orchestrator_id
                || argument(call, "objective") != snapshot.campaign.session.objective
                || argument(call, "model") != snapshot.campaign.session.model_name
                || snapshot.phase != self_iteration_orchestrator::Phase::awaiting_plan_request)
                return reject(Code::stale_state, &call, snapshot,
                    "Start request does not match the exact configured objective, model, target, or phase.");
            intent.kind = MutationKind::request_plan;
            intent.summary = "MCP requested the configured campaign plan.";
        }
        else
        {
            const bool cancel = call.tool == mcp_campaign::kCancelTool;
            if (!exact_arguments(call, cancel
                    ? std::initializer_list<std::string_view>{"campaign_id", "reason"}
                    : std::initializer_list<std::string_view>{"campaign_id"})
                || argument(call, "campaign_id") != snapshot.campaign.campaign_id)
                return reject(Code::stale_state, &call, snapshot,
                    "Mutation campaign identity or argument set is stale.");
            if (call.tool == mcp_campaign::kResumeTool)
            {
                if (snapshot.phase != self_iteration_orchestrator::Phase::awaiting_plan_request)
                    return reject(Code::out_of_order, &call, snapshot,
                        "Resume may request only a fresh plan from a fail-closed resumed state.");
                intent.kind = MutationKind::request_plan;
                intent.summary = "MCP requested a fresh post-resume plan.";
            }
            else if (cancel)
            {
                intent.kind = MutationKind::cancel;
                intent.summary = std::string{argument(call, "reason")};
            }
            else if (call.tool == mcp_campaign::kValidateCandidateTool)
            {
                if (snapshot.phase
                    != self_iteration_orchestrator::Phase::awaiting_validation_request)
                    return reject(Code::out_of_order, &call, snapshot,
                        "Candidate validation is not requestable in the current phase.");
                intent.kind = MutationKind::request_validation;
                intent.summary = "MCP requested the next fixed trusted validation actor.";
            }
            else
                return reject(Code::invalid_request, &call, snapshot,
                    "Unsupported campaign tool for this bridge state.");
        }
        (void)now;
        return make_pending(request_id, call.call_id, std::move(intent), orchestrator);
    }

    Result Bridge::approve_receipt(
        const HostApproval& approval,
        self_iteration_orchestrator::Orchestrator& orchestrator)
    {
        auto snapshot = orchestrator.snapshot();
        if (connection_state_ != ConnectionState::connected)
            return reject(Code::disconnected, nullptr, snapshot,
                "Host receipt cannot complete while the bridge is disconnected.");
        const auto found = std::find_if(pending_.begin(), pending_.end(),
            [&](const PendingState& value)
            { return value.receipt.receipt_id() == approval.receipt_id; });
        if (found == pending_.end())
            return reject(Code::stale_state, nullptr, snapshot,
                "Unknown host approval receipt.");
        if (found->consumed)
            return reject(Code::replay_denied, nullptr, snapshot,
                "Host approval receipt was already consumed.");
        const auto& receipt = found->receipt;
        if (!approval.operator_approved
            || approval.connection_generation != connection_generation_
            || receipt.connection_generation() > approval.connection_generation
            || approval.expected_orchestrator_generation
                != receipt.expected_orchestrator_generation()
            || approval.expected_state_sha256 != receipt.expected_state_sha256()
            || snapshot.orchestrator_id != receipt.orchestrator_id()
            || snapshot.campaign.campaign_id != receipt.campaign_id()
            || snapshot.campaign.record_generation != receipt.campaign_generation()
            || snapshot.campaign.session.identity != receipt.session_identity()
            || snapshot.generation != receipt.expected_orchestrator_generation()
            || snapshot.state_sha256 != receipt.expected_state_sha256())
            return reject(Code::stale_state, nullptr, snapshot,
                "Host approval does not bind the exact connection, campaign, session, generation, and state digest.");
        const auto action = self_iteration_orchestrator::ActionToken{
            .expected_generation = snapshot.generation,
            .expected_state_sha256 = snapshot.state_sha256,
            .transition_id = action_transition(receipt.receipt_id()),
            .now_unix_seconds = approval.now_unix_seconds,
            .operator_approved = true};
        self_iteration_orchestrator::Result applied{};
        switch (receipt.kind())
        {
        case MutationKind::request_plan: applied = orchestrator.request_plan(action); break;
        case MutationKind::share_curated_evidence:
            applied = orchestrator.share_curated_evidence(action,
                receipt.intent().scope_sha256, receipt.intent().evidence_sha256,
                receipt.intent().summary); break;
        case MutationKind::request_proposal: applied = orchestrator.request_proposal(action); break;
        case MutationKind::review_proposal:
            applied = orchestrator.review_proposal(action,
                receipt.intent().approve, receipt.intent().summary); break;
        case MutationKind::decide_apply:
            applied = orchestrator.decide_apply(action,
                receipt.intent().approve, receipt.intent().summary); break;
        case MutationKind::request_validation:
            applied = orchestrator.request_validation(action); break;
        case MutationKind::checkpoint:
            applied = orchestrator.checkpoint(action, receipt.intent().summary); break;
        case MutationKind::cancel:
            applied = orchestrator.cancel(action, receipt.intent().summary); break;
        }
        if (!applied)
            return reject(Code::out_of_order, nullptr, snapshot, applied.status);
        found->consumed = true;
        last_state_sha256_ = applied.snapshot.state_sha256;
        notify(applied.snapshot.phase == self_iteration_orchestrator::Phase::cancelled
                ? NotificationKind::cancelled : NotificationKind::completed,
            receipt.request_key(), receipt.receipt_id(),
            applied.pending_operation ? applied.pending_operation->operation_id() : std::string{},
            applied.snapshot, applied.snapshot.state_sha256, applied.status);
        McpToolCall call{.session_id = transport_session_id_, .call_id = receipt.call_id()};
        return Result{.code = Code::ready,
            .tool_result = tool_result(&call, McpCallState::succeeded,
                McpErrorCode::none,
                "{\"schema\":\"epoch.ai.mcp_orchestrator_bridge.v1\",\"state\":\"host_approved\",\"state_sha256\":"
                    + json_string(applied.snapshot.state_sha256) + "}"),
            .snapshot = applied.snapshot,
            .pending_operation = std::move(applied.pending_operation),
            .status = applied.status};
    }

    Result Bridge::complete_operation(
        const self_iteration_orchestrator::PendingOperation& operation,
        HostOperationResult result,
        self_iteration_orchestrator::Orchestrator& orchestrator)
    {
        const auto snapshot = orchestrator.snapshot();
        if (connection_state_ != ConnectionState::connected)
            return reject(Code::disconnected, nullptr, snapshot,
                "Host operation result cannot complete while disconnected.");
        if (!result.operator_approved || result.operation_id != operation.operation_id()
            || result.expected_generation != operation.expected_generation()
            || result.expected_state_sha256 != operation.expected_state_sha256()
            || result.transition_id != operation.transition_id()
            || snapshot.orchestrator_id != operation.orchestrator_id()
            || snapshot.campaign.campaign_id != operation.campaign_id()
            || snapshot.campaign.session.identity != operation.session_identity()
            || snapshot.generation != operation.expected_generation()
            || snapshot.state_sha256 != operation.expected_state_sha256())
            return reject(Code::stale_state, nullptr, snapshot,
                "Host operation result is stale, out of order, or cross-authority.");
        if (std::find(completed_operation_ids_.begin(), completed_operation_ids_.end(),
                operation.operation_id()) != completed_operation_ids_.end())
            return reject(Code::replay_denied, nullptr, snapshot,
                "Host operation result was already consumed.");
        if (result.content.size() > limits_.maximum_content_bytes
            || result.summary.empty()
            || result.summary.size() > limits_.maximum_summary_bytes)
            return reject(Code::budget_exhausted, nullptr, snapshot,
                "Host operation result exceeds bounded content or summary limits.");
        const auto receipt = self_iteration_orchestrator::OperationReceipt{
            .operation_id = result.operation_id,
            .expected_generation = result.expected_generation,
            .expected_state_sha256 = result.expected_state_sha256,
            .transition_id = result.transition_id,
            .now_unix_seconds = result.now_unix_seconds};
        self_iteration_orchestrator::Result applied{};
        switch (operation.kind())
        {
        case self_iteration_orchestrator::OperationKind::model_plan:
            applied = orchestrator.record_plan(receipt, result.content, result.summary);
            break;
        case self_iteration_orchestrator::OperationKind::model_proposal:
            applied = orchestrator.record_proposal(receipt, result.content, result.summary);
            break;
        case self_iteration_orchestrator::OperationKind::sandbox_apply:
            applied = orchestrator.record_apply(receipt,
                result.evidence_sha256, result.summary);
            break;
        case self_iteration_orchestrator::OperationKind::trusted_validation:
            applied = orchestrator.record_validation(receipt,
                result.evidence_sha256, result.summary, result.passed);
            break;
        }
        if (!applied)
            return reject(Code::out_of_order, nullptr, snapshot, applied.status);
        completed_operation_ids_.push_back(operation.operation_id());
        last_state_sha256_ = applied.snapshot.state_sha256;
        notify(NotificationKind::progress, operation.transition_id(), {},
            operation.operation_id(), applied.snapshot,
            !result.evidence_sha256.empty() ? result.evidence_sha256
                : digest_text(result.content), applied.status);
        return Result{.code = Code::ready,
            .snapshot = applied.snapshot,
            .pending_operation = std::move(applied.pending_operation),
            .status = applied.status};
    }

    std::vector<Notification> Bridge::drain_notifications()
    {
        std::vector<Notification> result{};
        result.swap(notifications_);
        return result;
    }

    ConnectionState Bridge::connection_state() const noexcept { return connection_state_; }
    std::uint64_t Bridge::connection_generation() const noexcept { return connection_generation_; }
}
