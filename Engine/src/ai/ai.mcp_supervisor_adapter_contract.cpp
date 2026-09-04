/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
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
        [[nodiscard]] std::string digest(std::string_view value)
        {
            return core::sha256::hex(core::sha256::hash(value));
        }

        [[nodiscard]] std::string digest(std::span<const std::uint8_t> value)
        {
            return core::sha256::hex(core::sha256::hash(value));
        }

        [[nodiscard]] Configuration configuration()
        {
            return {.adapter_id = "adapter-contract",
                .control_id = "control-contract",
                .session_id = "session-contract",
                .actor_sha256 = digest("operator-contract"),
                .campaign_id = "campaign-contract",
                .project_id = "project-contract",
                .curated_session_id = 17u,
                .created_at_unix_seconds = 1'800'000'000u,
                .expires_at_unix_seconds = 1'800'003'600u};
        }

        [[nodiscard]] supervisor::QuerySnapshot query_snapshot()
        {
            return {.control_id = "control-contract",
                .session_id = "session-contract",
                .campaign_id = "campaign-contract",
                .control_generation = 3u,
                .control_state_sha256 = digest("control-state"),
                .control_phase = supervisor::ControlPhase::active,
                .queue_generation = 5u,
                .queue_state_sha256 = digest("queue-state"),
                .scheduler_generation = 7u,
                .scheduler_state_sha256 = digest("scheduler-state"),
                .scheduler_phase = iteration_campaign_scheduler::Phase::idle,
                .active_objective_id = "objective-contract",
                .active_operation_id = "operation-contract"};
        }

        struct Host final
        {
            supervisor::QuerySnapshot current{query_snapshot()};
            supervisor::QuerySnapshot after{query_snapshot()};
            supervisor::Code submit_code{supervisor::Code::paused};
            bool reject{};
            bool command_seen{};

            [[nodiscard]] static supervisor::QueryResult query(void* raw)
            {
                auto& self = *static_cast<Host*>(raw);
                return {supervisor::Code::ready, self.current, "host snapshot"};
            }

            [[nodiscard]] static supervisor::Result submit(
                void* raw, std::span<const std::byte> bytes)
            {
                auto& self = *static_cast<Host*>(raw);
                self.command_seen = !bytes.empty();
                supervisor::Result result{};
                result.code = self.reject
                    ? supervisor::Code::invalid_transition : self.submit_code;
                result.snapshot.configuration.control_id = self.current.control_id;
                result.snapshot.configuration.session_id = self.current.session_id;
                result.snapshot.configuration.campaign_id = self.current.campaign_id;
                result.snapshot.generation = self.after.control_generation;
                result.snapshot.state_sha256 = self.after.control_state_sha256;
                result.snapshot.queue_generation = self.after.queue_generation;
                result.snapshot.queue_state_sha256 = self.after.queue_state_sha256;
                result.snapshot.scheduler_generation = self.after.scheduler_generation;
                result.snapshot.scheduler_state_sha256 = self.after.scheduler_state_sha256;
                result.receipt.receipt_sha256 = digest("host-receipt-"
                    + std::to_string(self.after.control_generation));
                result.status = self.reject ? "rejected" : "accepted";
                if (!self.reject) self.current = self.after;
                return result;
            }
        };

        [[nodiscard]] HostGateway gateway(
            Host& host, const curated_context_bundle::Bundle* bundle = nullptr)
        {
            return {&host, &Host::query, &Host::submit, bundle};
        }

        [[nodiscard]] Request request(
            const Adapter& adapter, const Host& host, Method method,
            std::string id,
            const curated_context_bundle::Bundle* bundle = nullptr)
        {
            const auto& state = adapter.snapshot();
            return {.id = std::move(id), .method = method,
                .session_id = state.configuration.session_id,
                .actor_sha256 = state.configuration.actor_sha256,
                .campaign_id = state.configuration.campaign_id,
                .project_id = state.configuration.project_id,
                .objective_id = "objective-contract",
                .operation_id = "operation-contract",
                .curated_session_id = state.configuration.curated_session_id,
                .expected_adapter_generation = state.generation,
                .expected_adapter_state_sha256 = state.state_sha256,
                .expected_control_generation = host.current.control_generation,
                .expected_control_state_sha256 = host.current.control_state_sha256,
                .expected_queue_generation = host.current.queue_generation,
                .expected_queue_state_sha256 = host.current.queue_state_sha256,
                .expected_scheduler_generation = host.current.scheduler_generation,
                .expected_scheduler_state_sha256 = host.current.scheduler_state_sha256,
                .curated_bundle_sha256 = bundle ? bundle->bundle_sha256 : "",
                .now_unix_seconds = 1'800'000'100u,
                .operator_approved = true};
        }

        [[nodiscard]] curated_context_bundle::Bundle curated()
        {
#if defined(_WIN32)
            const std::filesystem::path root{"C:/epoch/contracts/project"};
#else
            const std::filesystem::path root{"/epoch/contracts/project"};
#endif
            curated_context_bundle::ReviewedEntry entry{
                .project_relative_path = "Scripts/reviewed.cpp",
                .declared_symbol = "reviewed_symbol",
                .first_line = 4u, .last_line = 4u,
                .source_revision = 11u,
                .exact_bytes = "private reviewed source bytes",
                .provenance = {.review_id = "review-contract",
                    .reviewer_binding = "operator-reviewed",
                    .reviewed_at_unix_seconds = 1'800'000'050u}};
            entry.provenance.selection_sha256 = digest(
                entry.project_relative_path + "\n" + entry.declared_symbol + "\n"
                + std::to_string(entry.first_line) + "\n"
                + std::to_string(entry.last_line) + "\n"
                + std::to_string(entry.source_revision) + "\n"
                + digest(entry.exact_bytes));
            curated_context_bundle::Request input{
                .binding = {.project_id = "project-contract",
                    .reviewed_root = root,
                    .project_manifest_sha256 = digest("manifest"),
                    .project_profile_sha256 = digest("profile"),
                    .reviewed_revision = 11u,
                    .audience = curated_context_bundle::Audience::local_model,
                    .provider = project_profile::Provider::epoch_local_qwen38,
                    .model_binding = "qwen3.8-contract",
                    .endpoint_binding = "epoch-local-runtime",
                    .session_id = 17u, .request_id = 23u,
                    .campaign_id = "campaign-contract",
                    .campaign_generation = 7u,
                    .operator_shared = true},
                .reviewed_entries = {entry}};
            const auto result = curated_context_bundle::build(input);
            return result ? result.bundle : curated_context_bundle::Bundle{};
        }

        [[nodiscard]] bool valid_success(const Result& result)
        {
            return result && !result.canonical_response.empty()
                && result.response_sha256 == digest(result.canonical_response)
                && result.receipt_sha256.size() == 64u
                && result.adapter_state_sha256.size() == 64u;
        }

        [[nodiscard]] Result invoke(
            Adapter& adapter, Host& host, Method method, std::string id,
            const curated_context_bundle::Bundle* bundle = nullptr)
        {
            return adapter.dispatch(
                canonical_request(request(adapter, host, method,
                    std::move(id), bundle)), gateway(host, bundle));
        }

        [[nodiscard]] bool snapshot_safe(const Snapshot& s) noexcept
        {
            return !s.source_bytes_exposed && !s.filesystem_read_permitted
                && !s.transport_permitted && !s.model_launch_permitted
                && !s.source_apply_permitted && !s.promotion_permitted
                && !s.release_permitted && !s.server_permitted
                && !s.network_listener_permitted;
        }
    }

    ContractFailure run_contract() noexcept
    {
        if (!protocol::run_contract()) return ContractFailure::malformed;
        Adapter adapter{}; Host host{};
        if (!adapter.begin(configuration())) return ContractFailure::begin;
        if (!valid_success(invoke(adapter, host, Method::campaign_snapshot,
                "request-snapshot"))) return ContractFailure::campaign_snapshot;
        if (!valid_success(invoke(adapter, host, Method::campaign_query,
                "request-query"))) return ContractFailure::campaign_query;

        const struct Mutation { Method method; supervisor::Code code;
            ContractFailure failure; } mutations[]{
            {Method::supervisor_pause, supervisor::Code::paused,
                ContractFailure::pause},
            {Method::supervisor_resume, supervisor::Code::resumed,
                ContractFailure::resume},
            {Method::supervisor_cancel, supervisor::Code::cancelled,
                ContractFailure::cancel},
            {Method::supervisor_retry, supervisor::Code::retry_authorized,
                ContractFailure::retry},
            {Method::supervisor_approve, supervisor::Code::approved,
                ContractFailure::approve},
            {Method::supervisor_reject, supervisor::Code::rejected,
                ContractFailure::reject}};
        unsigned sequence{};
        for (const auto& mutation : mutations)
        {
            host.submit_code = mutation.code; host.command_seen = false;
            if (!valid_success(invoke(adapter, host, mutation.method,
                    "request-mutation-" + std::to_string(++sequence)))
                || !host.command_seen) return mutation.failure;
        }

        const auto bundle = curated();
        if (bundle.bundle_sha256.size() != 64u) return ContractFailure::curated_query;
        if (!valid_success(invoke(adapter, host, Method::curated_context_query,
                "request-curated-query", &bundle)))
            return ContractFailure::curated_query;
        const auto evidence = invoke(adapter, host,
            Method::curated_context_evidence, "request-curated-evidence", &bundle);
        if (!valid_success(evidence)) return ContractFailure::curated_evidence;
        const std::string evidence_text{
            evidence.canonical_response.begin(), evidence.canonical_response.end()};
        if (evidence_text.find("private reviewed source bytes") != std::string::npos
            || evidence_text.find("reviewed.cpp") == std::string::npos)
            return ContractFailure::source_boundary;

        if (adapter.dispatch({}, gateway(host)).code != Code::parse_error)
            return ContractFailure::malformed;
        auto noncanonical = canonical_request(request(adapter, host,
            Method::campaign_query, "request-noncanonical"));
        noncanonical.insert(noncanonical.begin() + 1u, ' ');
        if (adapter.dispatch(noncanonical, gateway(host)).code
            != Code::noncanonical_request) return ContractFailure::noncanonical;

        auto unknown_wire = protocol::WireRequest{
            "request-unknown", "epoch.unknown", configuration().session_id,
            configuration().actor_sha256, configuration().campaign_id,
            configuration().project_id, "objective-contract", "operation-contract",
            configuration().curated_session_id, adapter.snapshot().generation,
            adapter.snapshot().state_sha256, host.current.control_generation,
            host.current.control_state_sha256, host.current.queue_generation,
            host.current.queue_state_sha256, host.current.scheduler_generation,
            host.current.scheduler_state_sha256, {}, 1'800'000'100u, true};
        if (adapter.dispatch(protocol::canonical_bytes(unknown_wire), gateway(host)).code
            != Code::method_not_found) return ContractFailure::unknown_method;

        auto invalid = request(adapter, host, Method::campaign_query,
            "request-invalid-authority");
        invalid.actor_sha256 = digest("another-actor");
        if (adapter.dispatch(canonical_request(invalid), gateway(host)).code
            != Code::invalid_authority) return ContractFailure::invalid_authority;
        auto stale_adapter = request(adapter, host, Method::campaign_query,
            "request-stale-adapter");
        --stale_adapter.expected_adapter_generation;
        if (adapter.dispatch(canonical_request(stale_adapter), gateway(host)).code
            != Code::stale_adapter_state) return ContractFailure::stale_adapter;
        auto stale_host = request(adapter, host, Method::campaign_query,
            "request-stale-host");
        ++stale_host.expected_control_generation;
        if (adapter.dispatch(canonical_request(stale_host), gateway(host)).code
            != Code::stale_host_state) return ContractFailure::stale_host;

        auto replay_request = request(adapter, host, Method::campaign_query,
            "request-replay");
        const auto replay_bytes = canonical_request(replay_request);
        if (!adapter.dispatch(replay_bytes, gateway(host))
            || adapter.dispatch(replay_bytes, gateway(host)).code
                != Code::replay_refused) return ContractFailure::replay;
        std::vector<std::uint8_t> oversized(
            adapter.snapshot().configuration.limits.maximum_request_bytes + 1u, 'x');
        if (adapter.dispatch(oversized, gateway(host)).code
            != Code::budget_exceeded) return ContractFailure::request_budget;

        Adapter limited{}; Host verbose{};
        auto limited_config = configuration();
        limited_config.adapter_id = "adapter-response-budget";
        limited_config.limits.maximum_response_bytes = 4096u;
        if (!limited.begin(limited_config)) return ContractFailure::response_budget;
        for (unsigned index = 0u; index < 32u; ++index)
        {
            supervisor::ItemSummary item{};
            item.objective_id = "objective-" + std::to_string(index)
                + std::string(100u, 'o');
            item.objective_sha256 = digest(item.objective_id);
            item.campaign_generation = index;
            item.request_id = "request-" + std::to_string(index);
            item.request_sha256 = digest(item.request_id);
            item.response_receipt_id = "receipt-" + std::to_string(index);
            item.response_sha256 = digest(item.response_receipt_id);
            verbose.current.items.push_back(item);
        }
        if (invoke(limited, verbose, Method::campaign_snapshot,
                "request-response-budget").code != Code::budget_exceeded)
            return ContractFailure::response_budget;

        host.reject = true;
        if (invoke(adapter, host, Method::supervisor_pause,
                "request-host-reject").code != Code::host_rejected)
            return ContractFailure::host_rejection;
        host.reject = false;
        auto missing = request(adapter, host, Method::curated_context_query,
            "request-curated-missing", &bundle);
        if (adapter.dispatch(canonical_request(missing), gateway(host)).code
            != Code::curated_context_unavailable)
            return ContractFailure::curated_missing;
        auto tampered = bundle;
        tampered.canonical_bytes.front() ^= 0x01u;
        auto tamper_request = request(adapter, host,
            Method::curated_context_evidence, "request-curated-tamper", &tampered);
        if (adapter.dispatch(canonical_request(tamper_request),
                gateway(host, &tampered)).code
            != Code::curated_context_unavailable)
            return ContractFailure::curated_tamper;
        if (!snapshot_safe(adapter.snapshot())) return ContractFailure::source_boundary;

        const auto saved = adapter.checkpoint();
        if (!saved) return ContractFailure::checkpoint_roundtrip;
        Adapter restored{};
        RestoreExpectation expected{adapter.snapshot().configuration,
            adapter.snapshot().generation, saved.sha256};
        const auto restored_result = restored.restore(saved.canonical_bytes, expected);
        if (!restored_result || restored.snapshot() != adapter.snapshot())
            return ContractFailure::checkpoint_roundtrip;
        if (restored.dispatch(replay_bytes, gateway(host)).code
            != Code::replay_refused) return ContractFailure::checkpoint_roundtrip;

        auto tampered_checkpoint = saved.canonical_bytes;
        tampered_checkpoint[tampered_checkpoint.size() / 2u] ^= 0x01u;
        if (Adapter{}.restore(tampered_checkpoint, expected).code
            != Code::checkpoint_integrity) return ContractFailure::checkpoint_tamper;
        auto truncated = saved.canonical_bytes;
        truncated.pop_back();
        auto truncated_expected = expected;
        truncated_expected.checkpoint_sha256 = digest(truncated);
        if (Adapter{}.restore(truncated, truncated_expected).code
            != Code::checkpoint_malformed) return ContractFailure::checkpoint_truncation;
        auto trailing = saved.canonical_bytes;
        trailing.push_back(0u);
        auto trailing_expected = expected;
        trailing_expected.checkpoint_sha256 = digest(trailing);
        if (Adapter{}.restore(trailing, trailing_expected).code
            != Code::checkpoint_noncanonical) return ContractFailure::checkpoint_trailing;
        auto cross = expected;
        cross.configuration.project_id = "another-project";
        if (Adapter{}.restore(saved.canonical_bytes, cross).code
            != Code::checkpoint_cross_session)
            return ContractFailure::checkpoint_cross_session;
        auto stale = expected; ++stale.current_generation;
        if (Adapter{}.restore(saved.canonical_bytes, stale).code
            != Code::checkpoint_stale) return ContractFailure::checkpoint_stale;
        auto noncanonical_checkpoint = saved.canonical_bytes;
        noncanonical_checkpoint.push_back(1u);
        noncanonical_checkpoint.push_back(2u);
        auto noncanonical_expected = expected;
        noncanonical_expected.checkpoint_sha256 = digest(noncanonical_checkpoint);
        if (Adapter{}.restore(noncanonical_checkpoint, noncanonical_expected).code
            != Code::checkpoint_noncanonical)
            return ContractFailure::checkpoint_noncanonical;
        return ContractFailure::none;
    }
}

#if defined(EPOCH_AI_MCP_SUPERVISOR_ADAPTER_CONTRACT_MAIN)
int main()
{
    const auto failure =
        epochengine::ai::mcp_supervisor_adapter::run_contract();
    return failure
            == epochengine::ai::mcp_supervisor_adapter::ContractFailure::none
        ? 0 : static_cast<int>(failure);
}
#endif
