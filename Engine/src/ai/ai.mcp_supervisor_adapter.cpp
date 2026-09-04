/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.mcp_supervisor_adapter;

import ai.mcp_supervisor_protocol;
import core.sha256;

namespace epochengine::ai::mcp_supervisor_adapter
{
    namespace protocol = mcp_supervisor_protocol;
    namespace supervisor = iteration_supervisor_control;

    namespace
    {
        [[nodiscard]] bool identifier(std::string_view value) noexcept
        {
            if (value.empty() || value.size() > 128u) return false;
            return std::all_of(value.begin(), value.end(), [](unsigned char c)
            {
                return std::isalnum(c) != 0 || c == '-' || c == '_'
                    || c == '.' || c == ':';
            });
        }

        [[nodiscard]] bool digest(std::string_view value) noexcept
        {
            return value.size() == 64u
                && std::all_of(value.begin(), value.end(), [](char c)
                {
                    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
                });
        }

        [[nodiscard]] std::string sha(std::span<const std::uint8_t> bytes)
        {
            return core::sha256::hex(core::sha256::hash(bytes));
        }

        [[nodiscard]] std::string sha(std::string_view text)
        {
            return core::sha256::hex(core::sha256::hash(text));
        }

        void hash_field(core::sha256::Hasher& h, std::string_view value)
        {
            h.update(std::to_string(value.size())); h.update(":");
            h.update(value); h.update(";");
        }

        void hash_field(core::sha256::Hasher& h, std::uint64_t value)
        {
            hash_field(h, std::to_string(value));
        }

        [[nodiscard]] std::string state_digest(const Snapshot& value)
        {
            core::sha256::Hasher h{};
            h.update("epoch-ai-mcp-supervisor-adapter-state-v1;");
            const auto& c = value.configuration;
            hash_field(h, c.adapter_id); hash_field(h, c.control_id);
            hash_field(h, c.session_id); hash_field(h, c.actor_sha256);
            hash_field(h, c.campaign_id); hash_field(h, c.project_id);
            hash_field(h, c.curated_session_id);
            hash_field(h, c.created_at_unix_seconds);
            hash_field(h, c.expires_at_unix_seconds);
            hash_field(h, c.limits.maximum_request_bytes);
            hash_field(h, c.limits.maximum_response_bytes);
            hash_field(h, c.limits.maximum_seen_requests);
            hash_field(h, c.limits.maximum_summary_bytes);
            hash_field(h, c.limits.maximum_checkpoint_bytes);
            hash_field(h, value.generation);
            hash_field(h, value.previous_state_sha256);
            hash_field(h, value.last_receipt_sha256);
            for (const auto& id : value.seen_request_ids) hash_field(h, id);
            for (const auto& item : value.seen_request_sha256)
                hash_field(h, item);
            return core::sha256::hex(h.finish());
        }

        [[nodiscard]] bool configuration_valid(const Configuration& c) noexcept
        {
            return identifier(c.adapter_id) && identifier(c.control_id)
                && identifier(c.session_id) && digest(c.actor_sha256)
                && identifier(c.campaign_id) && identifier(c.project_id)
                && c.curated_session_id > 0u && c.created_at_unix_seconds > 0u
                && c.expires_at_unix_seconds > c.created_at_unix_seconds
                && c.limits.valid();
        }

        [[nodiscard]] bool safe(const supervisor::QuerySnapshot& q) noexcept
        {
            return !q.source_write_permitted
                && !q.arbitrary_file_read_permitted
                && !q.model_launch_permitted && !q.promotion_permitted
                && !q.release_permitted && !q.server_permitted
                && !q.network_listener_permitted;
        }

        [[nodiscard]] bool safe(const supervisor::Snapshot& q) noexcept
        {
            return !q.source_write_permitted
                && !q.arbitrary_file_read_permitted
                && !q.model_launch_permitted && !q.promotion_permitted
                && !q.release_permitted && !q.server_permitted
                && !q.network_listener_permitted;
        }

        [[nodiscard]] bool query_bound(
            const supervisor::QuerySnapshot& q,
            const Configuration& c,
            const Request& r) noexcept
        {
            return q.control_id == c.control_id && q.session_id == c.session_id
                && q.campaign_id == c.campaign_id
                && q.control_generation == r.expected_control_generation
                && q.control_state_sha256 == r.expected_control_state_sha256
                && q.queue_generation == r.expected_queue_generation
                && q.queue_state_sha256 == r.expected_queue_state_sha256
                && q.scheduler_generation == r.expected_scheduler_generation
                && q.scheduler_state_sha256 == r.expected_scheduler_state_sha256
                && safe(q);
        }

        [[nodiscard]] Method parse_method(std::string_view value, bool& found)
        {
            for (std::uint8_t index = 0u;
                index <= static_cast<std::uint8_t>(Method::curated_context_evidence);
                ++index)
            {
                const auto method = static_cast<Method>(index);
                if (value == method_name(method)) { found = true; return method; }
            }
            found = false;
            return Method::campaign_query;
        }

        [[nodiscard]] bool mutating(Method value) noexcept
        {
            return value >= Method::supervisor_pause
                && value <= Method::supervisor_reject;
        }

        [[nodiscard]] supervisor::CommandKind command_kind(Method value)
        {
            switch (value)
            {
            case Method::supervisor_pause: return supervisor::CommandKind::pause;
            case Method::supervisor_resume: return supervisor::CommandKind::resume;
            case Method::supervisor_cancel: return supervisor::CommandKind::cancel;
            case Method::supervisor_retry: return supervisor::CommandKind::retry;
            case Method::supervisor_approve: return supervisor::CommandKind::approve;
            case Method::supervisor_reject: return supervisor::CommandKind::reject;
            default: return supervisor::CommandKind::synchronize;
            }
        }

        [[nodiscard]] std::string error_number(Code value)
        {
            switch (value)
            {
            case Code::parse_error: return "-32700";
            case Code::invalid_request:
            case Code::noncanonical_request: return "-32600";
            case Code::method_not_found: return "-32601";
            case Code::invalid_params:
            case Code::invalid_authority: return "-32602";
            case Code::stale_adapter_state:
            case Code::stale_host_state: return "-32001";
            case Code::replay_refused: return "-32002";
            case Code::budget_exceeded: return "-32003";
            case Code::host_rejected: return "-32004";
            case Code::curated_context_unavailable: return "-32005";
            default: return "-32603";
            }
        }

        [[nodiscard]] std::string error_text(
            std::string_view id, Code code, std::string_view status)
        {
            return "{\"jsonrpc\":\"2.0\",\"id\":" + protocol::json_string(id)
                + ",\"error\":{\"code\":" + error_number(code)
                + ",\"message\":" + protocol::json_string(code_name(code))
                + ",\"data\":{\"schema\":"
                + protocol::json_string(response_schema) + ",\"status\":"
                + protocol::json_string(status) + "}}}";
        }

        [[nodiscard]] std::string item_json(const supervisor::ItemSummary& item)
        {
            return "{\"objective_id\":" + protocol::json_string(item.objective_id)
                + ",\"objective_sha256\":" + protocol::json_string(item.objective_sha256)
                + ",\"campaign_generation\":" + std::to_string(item.campaign_generation)
                + ",\"phase\":" + std::to_string(static_cast<unsigned>(item.phase))
                + ",\"request_id\":" + protocol::json_string(item.request_id)
                + ",\"request_sha256\":" + protocol::json_string(item.request_sha256)
                + ",\"response_receipt_id\":" + protocol::json_string(item.response_receipt_id)
                + ",\"response_sha256\":" + protocol::json_string(item.response_sha256) + "}";
        }

        [[nodiscard]] std::string query_json(const supervisor::QuerySnapshot& q)
        {
            std::string text{"{\"control_generation\":"
                + std::to_string(q.control_generation)
                + ",\"control_state_sha256\":" + protocol::json_string(q.control_state_sha256)
                + ",\"control_phase\":" + std::to_string(static_cast<unsigned>(q.control_phase))
                + ",\"queue_generation\":" + std::to_string(q.queue_generation)
                + ",\"queue_state_sha256\":" + protocol::json_string(q.queue_state_sha256)
                + ",\"scheduler_generation\":" + std::to_string(q.scheduler_generation)
                + ",\"scheduler_state_sha256\":" + protocol::json_string(q.scheduler_state_sha256)
                + ",\"scheduler_phase\":" + std::to_string(static_cast<unsigned>(q.scheduler_phase))
                + ",\"active_objective_id\":" + protocol::json_string(q.active_objective_id)
                + ",\"active_operation_id\":" + protocol::json_string(q.active_operation_id)
                + ",\"next_retry_at_unix_seconds\":" + std::to_string(q.next_retry_at_unix_seconds)
                + ",\"items\":["};
            for (std::size_t index = 0u; index < q.items.size(); ++index)
            {
                if (index) text.push_back(',');
                text += item_json(q.items[index]);
            }
            return text + "],\"truncated\":" + (q.truncated ? "true}" : "false}");
        }

        [[nodiscard]] std::string curated_json(
            Method method, const curated_context_bundle::Bundle& bundle)
        {
            std::string text{"{\"bundle_sha256\":"
                + protocol::json_string(bundle.bundle_sha256)
                + ",\"request_sha256\":" + protocol::json_string(bundle.request_sha256)
                + ",\"entry_count\":" + std::to_string(bundle.evidence.size())
                + ",\"chunk_count\":" + std::to_string(bundle.chunks.size())
                + ",\"generation\":" + std::to_string(bundle.resume.generation)};
            if (method == Method::curated_context_evidence)
            {
                text += ",\"evidence\":[";
                for (std::size_t index = 0u; index < bundle.evidence.size(); ++index)
                {
                    const auto& e = bundle.evidence[index];
                    if (index) text.push_back(',');
                    text += "{\"path\":" + protocol::json_string(e.project_relative_path)
                        + ",\"symbol\":" + protocol::json_string(e.declared_symbol)
                        + ",\"first_line\":" + std::to_string(e.first_line)
                        + ",\"last_line\":" + std::to_string(e.last_line)
                        + ",\"source_revision\":" + std::to_string(e.source_revision)
                        + ",\"byte_count\":" + std::to_string(e.byte_count)
                        + ",\"content_sha256\":" + protocol::json_string(e.content_sha256)
                        + ",\"entry_sha256\":" + protocol::json_string(e.entry_sha256)
                        + ",\"review_id\":" + protocol::json_string(e.provenance.review_id) + "}";
                }
                text.push_back(']');
            }
            text.push_back('}');
            return text;
        }

        [[nodiscard]] Result response(
            const Snapshot& state, Code code, std::string_view id,
            std::string request_hash, std::string status)
        {
            const std::string text = error_text(id, code, status);
            const std::vector<std::uint8_t> bytes{text.begin(), text.end()};
            return {code, bytes, std::move(request_hash), sha(bytes), {},
                state.generation, state.state_sha256, {}, std::move(status)};
        }

        [[nodiscard]] bool curated_bound(
            const curated_context_bundle::Bundle& b,
            const Configuration& c,
            const Request& r) noexcept
        {
            return b.binding.project_id == c.project_id
                && b.binding.campaign_id == c.campaign_id
                && b.binding.session_id == c.curated_session_id
                && b.resume.project_id == c.project_id
                && b.resume.campaign_id == c.campaign_id
                && b.resume.session_id == c.curated_session_id
                && b.bundle_sha256 == r.curated_bundle_sha256
                && digest(b.bundle_sha256) && !b.canonical_bytes.empty()
                && sha(b.canonical_bytes) == b.bundle_sha256;
        }

        [[nodiscard]] protocol::WireRequest wire(const Request& r)
        {
            return {r.id, std::string{method_name(r.method)}, r.session_id,
                r.actor_sha256, r.campaign_id, r.project_id, r.objective_id,
                r.operation_id, r.curated_session_id,
                r.expected_adapter_generation, r.expected_adapter_state_sha256,
                r.expected_control_generation, r.expected_control_state_sha256,
                r.expected_queue_generation, r.expected_queue_state_sha256,
                r.expected_scheduler_generation, r.expected_scheduler_state_sha256,
                r.curated_bundle_sha256, r.now_unix_seconds,
                r.operator_approved};
        }
    }

    std::vector<std::uint8_t> canonical_request(const Request& request)
    {
        return protocol::canonical_bytes(wire(request));
    }

    Result Adapter::begin(const Configuration& configuration) noexcept
    {
        if (!configuration_valid(configuration))
            return response(snapshot_, Code::invalid_params, {}, {},
                "adapter configuration was rejected");
        snapshot_ = {};
        snapshot_.configuration = configuration;
        snapshot_.generation = 1u;
        snapshot_.state_sha256 = state_digest(snapshot_);
        initialized_ = true;
        return {Code::ready, {}, {}, {}, {}, snapshot_.generation,
            snapshot_.state_sha256, {}, "adapter ready"};
    }

    Result Adapter::dispatch(
        std::span<const std::uint8_t> bytes, const HostGateway& gateway) noexcept
    {
        if (!initialized_)
            return response(snapshot_, Code::invalid_request, {}, {},
                "adapter is not initialized");
        if (bytes.size() > snapshot_.configuration.limits.maximum_request_bytes)
            return response(snapshot_, Code::budget_exceeded, {}, sha(bytes),
                "request exceeds the configured byte budget");
        const std::string request_hash = sha(bytes);
        const auto parsed = protocol::parse(bytes);
        if (parsed.code == protocol::ParseCode::malformed)
            return response(snapshot_, Code::parse_error, {}, request_hash,
                "request is not the exact JSON-RPC schema");
        if (parsed.code == protocol::ParseCode::noncanonical)
            return response(snapshot_, Code::noncanonical_request,
                parsed.request.id, request_hash,
                "request must use canonical JSON encoding");

        bool found{};
        const Method method = parse_method(parsed.request.method, found);
        Request request{parsed.request.id, method, parsed.request.session_id,
            parsed.request.actor_sha256, parsed.request.campaign_id,
            parsed.request.project_id, parsed.request.objective_id,
            parsed.request.operation_id, parsed.request.curated_session_id,
            parsed.request.expected_adapter_generation,
            parsed.request.expected_adapter_state_sha256,
            parsed.request.expected_control_generation,
            parsed.request.expected_control_state_sha256,
            parsed.request.expected_queue_generation,
            parsed.request.expected_queue_state_sha256,
            parsed.request.expected_scheduler_generation,
            parsed.request.expected_scheduler_state_sha256,
            parsed.request.curated_bundle_sha256,
            parsed.request.now_unix_seconds, parsed.request.operator_approved};
        if (!found)
            return response(snapshot_, Code::method_not_found, request.id,
                request_hash, "method is not on the supervisor allowlist");
        if (!identifier(request.id) || !identifier(request.objective_id)
            || !identifier(request.operation_id))
            return response(snapshot_, Code::invalid_params, request.id,
                request_hash, "request identifiers are invalid");
        const auto& c = snapshot_.configuration;
        if (request.session_id != c.session_id
            || request.actor_sha256 != c.actor_sha256
            || request.campaign_id != c.campaign_id
            || request.project_id != c.project_id
            || request.curated_session_id != c.curated_session_id
            || request.now_unix_seconds < c.created_at_unix_seconds
            || request.now_unix_seconds > c.expires_at_unix_seconds
            || !request.operator_approved)
            return response(snapshot_, Code::invalid_authority, request.id,
                request_hash, "session or operator authority is not bound");
        for (std::size_t i = 0u; i < snapshot_.seen_request_ids.size(); ++i)
            if (snapshot_.seen_request_ids[i] == request.id
                || snapshot_.seen_request_sha256[i] == request_hash)
                return response(snapshot_, Code::replay_refused, request.id,
                    request_hash, "request id or digest was already consumed");
        if (snapshot_.seen_request_ids.size()
            >= c.limits.maximum_seen_requests)
            return response(snapshot_, Code::budget_exceeded, request.id,
                request_hash,
                "replay ledger is full; checkpoint and rotate the session");
        if (request.expected_adapter_generation != snapshot_.generation
            || request.expected_adapter_state_sha256 != snapshot_.state_sha256)
            return response(snapshot_, Code::stale_adapter_state, request.id,
                request_hash, "adapter generation or digest is stale");
        if (!gateway.query)
            return response(snapshot_, Code::invalid_authority, request.id,
                request_hash, "host query gateway is absent");
        const auto before = gateway.query(gateway.context);
        if (!before || !query_bound(before.snapshot, c, request))
            return response(snapshot_, Code::stale_host_state, request.id,
                request_hash, "host state does not match the admitted request");

        if ((method == Method::curated_context_query
                || method == Method::curated_context_evidence)
            && (!gateway.curated_context
                || !curated_bound(*gateway.curated_context, c, request)))
            return response(snapshot_, Code::curated_context_unavailable,
                request.id, request_hash,
                "reviewed curated evidence is absent or does not match");

        supervisor::QuerySnapshot after = before.snapshot;
        std::string host_receipt{};
        if (mutating(method))
        {
            if (!gateway.submit)
                return response(snapshot_, Code::invalid_authority, request.id,
                    request_hash, "host submit gateway is absent");
            supervisor::Command command{request.id, command_kind(method),
                c.control_id, c.session_id, c.actor_sha256, c.campaign_id,
                request.objective_id, request.operation_id,
                request.expected_control_generation,
                request.expected_control_state_sha256,
                request.expected_queue_generation, request.expected_queue_state_sha256,
                request.expected_scheduler_generation,
                request.expected_scheduler_state_sha256,
                request.now_unix_seconds, request.operator_approved,
                false, false, false, false, false, false, false};
            const auto command_bytes = supervisor::canonical_command(command);
            const auto submitted = gateway.submit(gateway.context, command_bytes);
            if (!submitted || !safe(submitted.snapshot))
                return response(snapshot_, Code::host_rejected, request.id,
                    request_hash, "supervisor rejected the bounded command");
            const auto refreshed = gateway.query(gateway.context);
            if (!refreshed || !safe(refreshed.snapshot)
                || refreshed.snapshot.control_id != c.control_id
                || refreshed.snapshot.session_id != c.session_id
                || refreshed.snapshot.campaign_id != c.campaign_id)
                return response(snapshot_, Code::host_rejected, request.id,
                    request_hash, "host did not return a safe committed state");
            after = refreshed.snapshot;
            host_receipt = submitted.receipt.receipt_sha256;
        }

        const std::string payload = method == Method::curated_context_query
                || method == Method::curated_context_evidence
            ? curated_json(method, *gateway.curated_context) : query_json(after);
        const std::string result_text = "{\"jsonrpc\":\"2.0\",\"id\":"
            + protocol::json_string(request.id) + ",\"result\":{\"schema\":"
            + protocol::json_string(response_schema) + ",\"method\":"
            + protocol::json_string(method_name(method)) + ",\"payload\":"
            + payload + ",\"host_receipt_sha256\":"
            + protocol::json_string(host_receipt) + "}}";
        if (result_text.size() > c.limits.maximum_response_bytes)
            return response(snapshot_, Code::budget_exceeded, request.id,
                request_hash, "response exceeds the configured byte budget");

        const std::string response_hash = sha(result_text);
        core::sha256::Hasher receipt_hasher{};
        receipt_hasher.update("epoch-ai-mcp-supervisor-adapter-receipt-v1;");
        hash_field(receipt_hasher, snapshot_.last_receipt_sha256);
        hash_field(receipt_hasher, request_hash);
        hash_field(receipt_hasher, response_hash);
        hash_field(receipt_hasher, host_receipt);
        hash_field(receipt_hasher, snapshot_.generation + 1u);
        const std::string receipt_hash = core::sha256::hex(receipt_hasher.finish());
        snapshot_.previous_state_sha256 = snapshot_.state_sha256;
        ++snapshot_.generation;
        snapshot_.last_receipt_sha256 = receipt_hash;
        snapshot_.seen_request_ids.push_back(request.id);
        snapshot_.seen_request_sha256.push_back(request_hash);
        snapshot_.state_sha256 = state_digest(snapshot_);
        const std::vector<std::uint8_t> result_bytes{
            result_text.begin(), result_text.end()};
        return {Code::ready, result_bytes, request_hash, response_hash,
            receipt_hash, snapshot_.generation, snapshot_.state_sha256,
            after.control_state_sha256, "canonical response ready"};
    }

    const Snapshot& Adapter::snapshot() const noexcept { return snapshot_; }
}
