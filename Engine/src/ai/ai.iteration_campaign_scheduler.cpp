/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.iteration_campaign_scheduler;

import core.sha256;

namespace epochengine::ai::iteration_campaign_scheduler
{
    namespace
    {
        constexpr std::uint32_t checkpoint_version = 1u;
        constexpr std::size_t maximum_checkpoint_bytes = 256u * 1024u;
        constexpr std::size_t maximum_seen_receipts = 128u;
        constexpr std::uint64_t maximum_duration_seconds = 7u * 24u * 60u * 60u;

        class Writer final
        {
        public:
            void u8(std::uint8_t value) { bytes_.push_back(value); }
            void u32(std::uint32_t value)
            {
                for (std::uint32_t shift = 0u; shift < 32u; shift += 8u)
                    bytes_.push_back(static_cast<std::uint8_t>(value >> shift));
            }
            void u64(std::uint64_t value)
            {
                for (std::uint32_t shift = 0u; shift < 64u; shift += 8u)
                    bytes_.push_back(static_cast<std::uint8_t>(value >> shift));
            }
            void boolean(bool value) { u8(value ? 1u : 0u); }
            void text(std::string_view value)
            {
                u32(static_cast<std::uint32_t>(value.size()));
                bytes_.insert(bytes_.end(), value.begin(), value.end());
            }
            void raw(std::span<const std::uint8_t> bytes)
            {
                bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
            }
            [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept
            {
                return bytes_;
            }
            [[nodiscard]] std::vector<std::uint8_t> take() noexcept
            {
                return std::move(bytes_);
            }
        private:
            std::vector<std::uint8_t> bytes_{};
        };

        class Reader final
        {
        public:
            explicit Reader(std::span<const std::uint8_t> bytes) : bytes_{bytes} {}
            [[nodiscard]] bool u8(std::uint8_t& value)
            {
                if (remaining() < 1u) return false;
                value = bytes_[offset_++];
                return true;
            }
            [[nodiscard]] bool u32(std::uint32_t& value)
            {
                if (remaining() < 4u) return false;
                value = 0u;
                for (std::uint32_t shift = 0u; shift < 32u; shift += 8u)
                    value |= static_cast<std::uint32_t>(bytes_[offset_++]) << shift;
                return true;
            }
            [[nodiscard]] bool u64(std::uint64_t& value)
            {
                if (remaining() < 8u) return false;
                value = 0u;
                for (std::uint32_t shift = 0u; shift < 64u; shift += 8u)
                    value |= static_cast<std::uint64_t>(bytes_[offset_++]) << shift;
                return true;
            }
            [[nodiscard]] bool boolean(bool& value)
            {
                std::uint8_t encoded{};
                if (!u8(encoded) || encoded > 1u) return false;
                value = encoded != 0u;
                return true;
            }
            [[nodiscard]] bool text(std::string& value, std::size_t maximum)
            {
                std::uint32_t length{};
                if (!u32(length) || length > maximum || remaining() < length)
                    return false;
                value.assign(reinterpret_cast<const char*>(bytes_.data() + offset_),
                    length);
                offset_ += length;
                return true;
            }
            [[nodiscard]] bool raw(
                std::span<const std::uint8_t>& bytes,
                std::size_t length)
            {
                if (remaining() < length) return false;
                bytes = bytes_.subspan(offset_, length);
                offset_ += length;
                return true;
            }
            [[nodiscard]] std::size_t remaining() const noexcept
            {
                return bytes_.size() - offset_;
            }
        private:
            std::span<const std::uint8_t> bytes_{};
            std::size_t offset_{};
        };

        [[nodiscard]] std::string digest(std::span<const std::uint8_t> bytes)
        {
            return core::sha256::hex(core::sha256::hash(bytes));
        }

        [[nodiscard]] std::string digest(std::string_view text)
        {
            return core::sha256::hex(core::sha256::hash(text));
        }

        [[nodiscard]] bool hex64(std::string_view value)
        {
            return value.size() == 64u && std::ranges::all_of(value, [](char ch)
            {
                return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
            });
        }

        [[nodiscard]] bool identifier(std::string_view value)
        {
            return !value.empty() && value.size() <= 128u
                && std::ranges::all_of(value, [](char ch)
                {
                    return (ch >= 'a' && ch <= 'z')
                        || (ch >= 'A' && ch <= 'Z')
                        || (ch >= '0' && ch <= '9')
                        || ch == '-' || ch == '_' || ch == '.' || ch == ':';
                });
        }

        [[nodiscard]] bool retry_valid(const RetryPolicy& retry)
        {
            return retry.maximum_attempts > 0u && retry.maximum_attempts <= 16u
                && retry.initial_backoff_seconds > 0u
                && retry.initial_backoff_seconds <= 60u
                && retry.maximum_backoff_seconds >= retry.initial_backoff_seconds
                && retry.maximum_backoff_seconds <= 15u * 60u;
        }

        [[nodiscard]] bool configuration_valid(const Configuration& config)
        {
            return identifier(config.scheduler_id)
                && hex64(config.human_authority_sha256)
                && identifier(config.queue_id)
                && hex64(config.orchestrator_id)
                && hex64(config.host_configuration_sha256)
                && config.created_at_unix_seconds > 0u
                && config.expires_at_unix_seconds > config.created_at_unix_seconds
                && config.expires_at_unix_seconds - config.created_at_unix_seconds
                    <= maximum_duration_seconds
                && retry_valid(config.retry);
        }

        [[nodiscard]] bool host_safe(
            const self_iteration_orchestrator::HostBinding& host)
        {
            return identifier(host.binding_id) && hex64(host.configuration_sha256)
                && host.generation > 0u && host.stdio_only
                && !host.automatic_launch && !host.network_enabled
                && !host.listener_enabled && !host.server_enabled;
        }

        [[nodiscard]] const iteration_campaign_queue::WorkItem* next_item(
            const iteration_campaign_queue::QueueSnapshot& queue)
        {
            const iteration_campaign_queue::WorkItem* selected{};
            for (const auto& item : queue.items)
            {
                if (item.phase != iteration_campaign_queue::ItemPhase::queued)
                    continue;
                if (!selected || item.request.priority > selected->request.priority
                    || (item.request.priority == selected->request.priority
                        && item.insertion_sequence < selected->insertion_sequence))
                    selected = &item;
            }
            return selected;
        }

        [[nodiscard]] const iteration_campaign_queue::WorkItem* find_item(
            const iteration_campaign_queue::QueueSnapshot& queue,
            std::string_view objective_id)
        {
            const auto found = std::ranges::find_if(queue.items,
                [objective_id](const auto& item)
                {
                    return item.request.objective_id == objective_id;
                });
            return found == queue.items.end() ? nullptr : &*found;
        }

        [[nodiscard]] bool campaign_matches(
            const iteration_campaign_queue::WorkItem& item,
            const self_iteration_orchestrator::Snapshot& orchestrator)
        {
            const auto& campaign = item.request.campaign;
            return campaign.campaign_id == orchestrator.campaign.campaign_id
                && campaign.target_key == orchestrator.target_key
                && campaign.state_sha256 == orchestrator.campaign.state_digest
                && campaign.record_generation
                    == orchestrator.campaign.record_generation;
        }

        [[nodiscard]] bool binding_matches(
            const Configuration& config,
            const iteration_campaign_queue::QueueSnapshot& queue,
            const self_iteration_orchestrator::Snapshot& orchestrator)
        {
            return queue.configuration.queue_id == config.queue_id
                && queue.configuration.human_authority_sha256
                    == config.human_authority_sha256
                && orchestrator.orchestrator_id == config.orchestrator_id
                && orchestrator.transport == config.transport
                && orchestrator.host.configuration_sha256
                    == config.host_configuration_sha256
                && host_safe(orchestrator.host)
                && orchestrator.manual_approval_required
                && orchestrator.live_source_read_only
                && !orchestrator.promotion_permitted && !orchestrator.git_permitted
                && !orchestrator.release_permitted
                && !orchestrator.network_or_server_permitted;
        }

        [[nodiscard]] std::string transport_binding_digest(
            const Configuration& config,
            const iteration_campaign_queue::QueueSnapshot& queue,
            const self_iteration_orchestrator::Snapshot& orchestrator,
            std::uint64_t connection_generation,
            const iteration_campaign_queue::WorkItem& item)
        {
            Writer writer{};
            writer.text("epoch-ai-campaign-transport-binding/v1");
            writer.text(config.scheduler_id);
            writer.u8(static_cast<std::uint8_t>(config.transport));
            writer.text(orchestrator.host.binding_id);
            writer.text(orchestrator.host.configuration_sha256);
            writer.u64(orchestrator.host.generation);
            writer.u64(connection_generation);
            writer.text(queue.configuration.queue_id);
            writer.u64(queue.generation);
            writer.text(queue.state_sha256);
            writer.text(orchestrator.orchestrator_id);
            writer.u64(orchestrator.generation);
            writer.text(orchestrator.state_sha256);
            writer.text(item.request.objective_id);
            writer.text(item.objective_sha256);
            writer.text(item.request.campaign.state_sha256);
            return digest(writer.bytes());
        }

        [[nodiscard]] std::string payload_digest(
            const mcp_orchestrator_bridge::PendingReceipt& receipt,
            const mcp_orchestrator_bridge::MutationIntent& intent,
            const iteration_campaign_queue::WorkItem& item)
        {
            Writer writer{};
            writer.text("epoch-ai-campaign-transport-request/v1");
            writer.text(receipt.receipt_id());
            writer.text(receipt.transport_session_id());
            writer.text(receipt.connection_id());
            writer.u64(receipt.connection_generation());
            writer.text(receipt.request_key());
            writer.text(receipt.call_id());
            writer.text(receipt.orchestrator_id());
            writer.text(receipt.campaign_id());
            writer.u64(receipt.campaign_generation());
            writer.u8(static_cast<std::uint8_t>(intent.kind));
            writer.text(intent.scope_sha256);
            writer.text(intent.evidence_sha256);
            writer.text(intent.summary);
            writer.text(item.objective_sha256);
            return digest(writer.bytes());
        }

        [[nodiscard]] std::string response_payload_digest(
            const mcp_orchestrator_bridge::HostOperationResult& response,
            const mcp_orchestrator_bridge::Result& completed)
        {
            Writer writer{};
            writer.text("epoch-ai-campaign-transport-response/v1");
            writer.text(response.operation_id);
            writer.u64(response.expected_generation);
            writer.text(response.expected_state_sha256);
            writer.text(response.transition_id);
            writer.u64(response.now_unix_seconds);
            writer.text(response.content);
            writer.text(response.evidence_sha256);
            writer.text(response.summary);
            writer.boolean(response.passed);
            writer.text(completed.snapshot.state_sha256);
            writer.u64(completed.snapshot.generation);
            return digest(writer.bytes());
        }

        void write_configuration(Writer& writer, const Configuration& config)
        {
            writer.text(config.scheduler_id);
            writer.text(config.human_authority_sha256);
            writer.text(config.queue_id);
            writer.text(config.orchestrator_id);
            writer.text(config.host_configuration_sha256);
            writer.u8(static_cast<std::uint8_t>(config.transport));
            writer.u64(config.created_at_unix_seconds);
            writer.u64(config.expires_at_unix_seconds);
            writer.u32(config.retry.maximum_attempts);
            writer.u64(config.retry.initial_backoff_seconds);
            writer.u64(config.retry.maximum_backoff_seconds);
        }

        [[nodiscard]] bool read_configuration(Reader& reader, Configuration& config)
        {
            std::uint8_t transport{};
            return reader.text(config.scheduler_id, 128u)
                && reader.text(config.human_authority_sha256, 64u)
                && reader.text(config.queue_id, 128u)
                && reader.text(config.orchestrator_id, 64u)
                && reader.text(config.host_configuration_sha256, 64u)
                && reader.u8(transport)
                && transport <= static_cast<std::uint8_t>(
                    self_iteration_orchestrator::TransportKind::external_mcp)
                && (config.transport = static_cast<
                    self_iteration_orchestrator::TransportKind>(transport), true)
                && reader.u64(config.created_at_unix_seconds)
                && reader.u64(config.expires_at_unix_seconds)
                && reader.u32(config.retry.maximum_attempts)
                && reader.u64(config.retry.initial_backoff_seconds)
                && reader.u64(config.retry.maximum_backoff_seconds)
                && configuration_valid(config);
        }

        void write_request(
            Writer& writer,
            const iteration_campaign_queue::RequestReceipt& request)
        {
            writer.text(request.request_id);
            writer.text(request.request_sha256);
            writer.text(request.binding_sha256);
            writer.text(request.payload_sha256);
            writer.u64(request.issued_at_unix_seconds);
        }

        [[nodiscard]] bool read_request(
            Reader& reader,
            iteration_campaign_queue::RequestReceipt& request)
        {
            return reader.text(request.request_id, 128u)
                && reader.text(request.request_sha256, 64u)
                && reader.text(request.binding_sha256, 64u)
                && reader.text(request.payload_sha256, 64u)
                && reader.u64(request.issued_at_unix_seconds)
                && (request.request_id.empty() || identifier(request.request_id))
                && (request.request_sha256.empty() || hex64(request.request_sha256))
                && (request.binding_sha256.empty() || hex64(request.binding_sha256))
                && (request.payload_sha256.empty() || hex64(request.payload_sha256));
        }

        void write_list(Writer& writer, const std::vector<std::string>& values)
        {
            writer.u32(static_cast<std::uint32_t>(values.size()));
            for (const auto& value : values) writer.text(value);
        }

        [[nodiscard]] bool read_list(Reader& reader, std::vector<std::string>& values)
        {
            std::uint32_t count{};
            if (!reader.u32(count) || count > maximum_seen_receipts) return false;
            values.reserve(count);
            for (std::uint32_t index = 0u; index < count; ++index)
            {
                std::string value{};
                if (!reader.text(value, 128u) || !identifier(value)
                    || std::ranges::find(values, value) != values.end())
                    return false;
                values.push_back(std::move(value));
            }
            return true;
        }

        [[nodiscard]] std::vector<std::uint8_t> canonical_state(
            const Snapshot& snapshot)
        {
            Writer writer{};
            writer.text(checkpoint_schema);
            write_configuration(writer, snapshot.configuration);
            writer.u64(snapshot.generation);
            writer.text(snapshot.previous_state_sha256);
            writer.u8(static_cast<std::uint8_t>(snapshot.phase));
            writer.u64(snapshot.queue_generation);
            writer.text(snapshot.queue_state_sha256);
            writer.u64(snapshot.orchestrator_generation);
            writer.text(snapshot.orchestrator_state_sha256);
            writer.text(snapshot.objective_id);
            writer.text(snapshot.objective_sha256);
            writer.text(snapshot.campaign_state_sha256);
            write_request(writer, snapshot.request);
            writer.text(snapshot.pending_bridge_receipt_id);
            writer.text(snapshot.pending_operation_id);
            writer.text(snapshot.transport_binding_sha256);
            writer.u32(snapshot.attempts);
            writer.u64(snapshot.next_retry_at_unix_seconds);
            writer.text(snapshot.last_failure_sha256);
            write_list(writer, snapshot.seen_bridge_receipt_ids);
            write_list(writer, snapshot.seen_operation_ids);
            return writer.take();
        }

        [[nodiscard]] bool read_snapshot(Reader& reader, Snapshot& snapshot)
        {
            std::string schema{};
            std::uint8_t phase{};
            if (!reader.text(schema, 64u) || schema != checkpoint_schema
                || !read_configuration(reader, snapshot.configuration)
                || !reader.u64(snapshot.generation)
                || !reader.text(snapshot.previous_state_sha256, 64u)
                || !reader.u8(phase)
                || phase > static_cast<std::uint8_t>(Phase::cancelled)
                || !reader.u64(snapshot.queue_generation)
                || !reader.text(snapshot.queue_state_sha256, 64u)
                || !reader.u64(snapshot.orchestrator_generation)
                || !reader.text(snapshot.orchestrator_state_sha256, 64u)
                || !reader.text(snapshot.objective_id, 128u)
                || !reader.text(snapshot.objective_sha256, 64u)
                || !reader.text(snapshot.campaign_state_sha256, 64u)
                || !read_request(reader, snapshot.request)
                || !reader.text(snapshot.pending_bridge_receipt_id, 128u)
                || !reader.text(snapshot.pending_operation_id, 128u)
                || !reader.text(snapshot.transport_binding_sha256, 64u)
                || !reader.u32(snapshot.attempts)
                || !reader.u64(snapshot.next_retry_at_unix_seconds)
                || !reader.text(snapshot.last_failure_sha256, 64u)
                || !read_list(reader, snapshot.seen_bridge_receipt_ids)
                || !read_list(reader, snapshot.seen_operation_ids))
                return false;
            snapshot.phase = static_cast<Phase>(phase);
            snapshot.source_write_permitted = false;
            snapshot.promotion_permitted = false;
            snapshot.release_permitted = false;
            snapshot.server_permitted = false;
            snapshot.network_listener_permitted = false;
            return reader.remaining() == 68u;
        }

        [[nodiscard]] bool snapshot_valid(const Snapshot& snapshot)
        {
            const bool idle_shape = snapshot.phase == Phase::idle
                || snapshot.phase == Phase::backoff
                || snapshot.phase == Phase::blocked
                || snapshot.phase == Phase::cancelled;
            const bool pending_shape = snapshot.phase == Phase::awaiting_transport_approval
                || snapshot.phase == Phase::awaiting_transport_response
                || snapshot.phase == Phase::awaiting_human_review;
            return configuration_valid(snapshot.configuration)
                && snapshot.generation > 0u
                && hex64(snapshot.state_sha256)
                && (snapshot.previous_state_sha256.empty()
                    || hex64(snapshot.previous_state_sha256))
                && snapshot.queue_generation > 0u
                && hex64(snapshot.queue_state_sha256)
                && snapshot.orchestrator_generation > 0u
                && hex64(snapshot.orchestrator_state_sha256)
                && snapshot.attempts <= snapshot.configuration.retry.maximum_attempts
                && snapshot.seen_bridge_receipt_ids.size() <= maximum_seen_receipts
                && snapshot.seen_operation_ids.size() <= maximum_seen_receipts
                && ((idle_shape && (snapshot.request.request_id.empty()
                        || snapshot.phase == Phase::cancelled))
                    || (pending_shape && identifier(snapshot.objective_id)
                        && hex64(snapshot.objective_sha256)
                        && hex64(snapshot.campaign_state_sha256)
                        && identifier(snapshot.request.request_id)
                        && hex64(snapshot.request.request_sha256)
                        && hex64(snapshot.transport_binding_sha256)))
                && !snapshot.source_write_permitted
                && !snapshot.promotion_permitted && !snapshot.release_permitted
                && !snapshot.server_permitted
                && !snapshot.network_listener_permitted;
        }

        [[nodiscard]] Result result(
            Code code,
            const Snapshot& snapshot,
            std::string status)
        {
            return Result{.code = code, .snapshot = snapshot,
                .status = std::move(status)};
        }
    }

    Result Scheduler::begin(
        const Configuration& configuration,
        const iteration_campaign_queue::QueueSnapshot& queue,
        const self_iteration_orchestrator::Snapshot& orchestrator)
    {
        if (!configuration_valid(configuration)
            || !binding_matches(configuration, queue, orchestrator))
            return result(Code::invalid_configuration, snapshot_,
                "Scheduler configuration does not bind the exact safe queue and orchestrator host.");
        snapshot_ = {};
        snapshot_.configuration = configuration;
        snapshot_.phase = Phase::idle;
        initialized_ = true;
        synchronize(queue, orchestrator);
        commit_mutation();
        return result(Code::ready, snapshot_,
            "Scheduler is bound to one queue, campaign orchestrator, and guarded MCP transport.");
    }

    Result Scheduler::validate(
        const Authority& authority,
        const iteration_campaign_queue::QueueSnapshot& queue,
        const self_iteration_orchestrator::Snapshot& orchestrator) const
    {
        if (!initialized_ || !authority.operator_permitted
            || authority.actor_sha256 != snapshot_.configuration.human_authority_sha256)
            return result(Code::invalid_authority, snapshot_,
                "An exact operator authority is required.");
        if (authority.expected_generation != snapshot_.generation
            || authority.expected_state_sha256 != snapshot_.state_sha256
            || authority.expected_queue_state_sha256 != queue.state_sha256
            || authority.expected_orchestrator_state_sha256 != orchestrator.state_sha256
            || snapshot_.queue_generation != queue.generation
            || snapshot_.queue_state_sha256 != queue.state_sha256
            || snapshot_.orchestrator_generation != orchestrator.generation
            || snapshot_.orchestrator_state_sha256 != orchestrator.state_sha256
            || !binding_matches(snapshot_.configuration, queue, orchestrator))
            return result(Code::stale_state, snapshot_,
                "Scheduler, queue, orchestrator, or host binding changed.");
        if (authority.now_unix_seconds < snapshot_.configuration.created_at_unix_seconds
            || authority.now_unix_seconds > snapshot_.configuration.expires_at_unix_seconds)
            return result(Code::invalid_authority, snapshot_,
                "Scheduler authority is outside its bounded lifetime.");
        return result(Code::ready, snapshot_, "Exact scheduler authority accepted.");
    }

    Result Scheduler::dispatch_next(
        const Authority& authority,
        iteration_campaign_queue::Queue& queue,
        mcp_orchestrator_bridge::Bridge& bridge,
        self_iteration_orchestrator::Orchestrator& orchestrator)
    {
        const auto orchestrator_snapshot = orchestrator.snapshot();
        if (const Result checked = validate(authority, queue.snapshot(),
                orchestrator_snapshot); !checked)
            return checked;
        if (snapshot_.phase == Phase::cancelled)
            return result(Code::cancelled, snapshot_, "Scheduler is cancelled.");
        if (snapshot_.phase == Phase::backoff
            && authority.now_unix_seconds < snapshot_.next_retry_at_unix_seconds)
            return result(Code::transport_backoff, snapshot_,
                "Transport retry remains inside deterministic backoff.");
        if (snapshot_.phase != Phase::idle && snapshot_.phase != Phase::backoff)
            return result(Code::invalid_transition, snapshot_,
                "A transport request is already active.");
        const auto* selected = next_item(queue.snapshot());
        if (!selected || !campaign_matches(*selected, orchestrator_snapshot))
            return result(Code::stale_state, snapshot_,
                "Next objective does not bind the exact orchestrator campaign checkpoint.");

        const std::string attempt_seed = snapshot_.configuration.scheduler_id + "\n"
            + selected->request.objective_id + "\n" + queue.snapshot().state_sha256
            + "\n" + std::to_string(snapshot_.attempts + 1u);
        const std::string attempt_digest = digest(attempt_seed);
        mcp_stdio::RequestId request_id{
            .kind = mcp_stdio::RequestIdKind::string,
            .string_value = "sched-" + attempt_digest.substr(0u, 24u)};
        const std::string call_id = "rpc:" + request_id.key();
        mcp_orchestrator_bridge::MutationIntent intent{
            .kind = mcp_orchestrator_bridge::MutationKind::request_plan,
            .scope_sha256 = selected->request.campaign.state_sha256,
            .evidence_sha256 = queue.snapshot().state_sha256,
            .summary = selected->request.objective};
        auto bridge_result = bridge.propose_mutation(
            request_id, call_id, intent, orchestrator);
        if (!bridge_result || !bridge_result.pending_receipt
            || bridge_result.code
                != mcp_orchestrator_bridge::Code::pending_host_approval)
            return failure(Code::transport_backoff, bridge_result.status,
                authority.now_unix_seconds, queue.snapshot(), orchestrator.snapshot());
        if (std::ranges::find(snapshot_.seen_bridge_receipt_ids,
                bridge_result.pending_receipt->receipt_id())
            != snapshot_.seen_bridge_receipt_ids.end())
            return result(Code::replay_refused, snapshot_,
                "Bridge receipt was already bound to this scheduler.");

        const std::string binding = transport_binding_digest(
            snapshot_.configuration, queue.snapshot(), orchestrator_snapshot,
            bridge.connection_generation(), *selected);
        const std::string payload = payload_digest(
            *bridge_result.pending_receipt, intent, *selected);
        const auto queue_result = queue.issue_next(
            iteration_campaign_queue::ModelActionToken{
                queue.snapshot().state_sha256,
                snapshot_.configuration.human_authority_sha256,
                queue.snapshot().generation,
                authority.now_unix_seconds,
                true},
            bridge_result.pending_receipt->receipt_id(), binding, payload);
        if (!queue_result || queue_result.objective_id != selected->request.objective_id)
            return failure(Code::transport_blocked,
                "Queue changed after bridge receipt creation; orphan receipt is blocked.",
                authority.now_unix_seconds, queue.snapshot(), orchestrator.snapshot());

        snapshot_.phase = Phase::awaiting_transport_approval;
        snapshot_.objective_id = queue_result.objective_id;
        snapshot_.objective_sha256 = selected->objective_sha256;
        snapshot_.campaign_state_sha256 = selected->request.campaign.state_sha256;
        snapshot_.request = queue_result.request;
        snapshot_.pending_bridge_receipt_id =
            bridge_result.pending_receipt->receipt_id();
        snapshot_.pending_operation_id.clear();
        snapshot_.transport_binding_sha256 = binding;
        snapshot_.attempts = 0u;
        snapshot_.next_retry_at_unix_seconds = 0u;
        snapshot_.last_failure_sha256.clear();
        snapshot_.seen_bridge_receipt_ids.push_back(
            bridge_result.pending_receipt->receipt_id());
        synchronize(queue.snapshot(), orchestrator.snapshot());
        commit_mutation();
        Result dispatched = result(Code::pending_human_approval, snapshot_,
            "Guarded MCP transport request awaits explicit bridge approval.");
        dispatched.bridge = std::move(bridge_result);
        dispatched.request = queue_result.request;
        return dispatched;
    }

    Result Scheduler::approve_dispatch(
        const Authority& authority,
        const mcp_orchestrator_bridge::HostApproval& approval,
        iteration_campaign_queue::Queue& queue,
        mcp_orchestrator_bridge::Bridge& bridge,
        self_iteration_orchestrator::Orchestrator& orchestrator)
    {
        if (const Result checked = validate(authority, queue.snapshot(),
                orchestrator.snapshot()); !checked)
            return checked;
        if (snapshot_.phase != Phase::awaiting_transport_approval
            || approval.receipt_id != snapshot_.pending_bridge_receipt_id
            || !approval.operator_approved)
            return result(Code::invalid_transition, snapshot_,
                "Approval does not bind the active bridge receipt.");
        auto bridge_result = bridge.approve_receipt(approval, orchestrator);
        if (!bridge_result || !bridge_result.pending_operation)
            return failure(Code::transport_backoff, bridge_result.status,
                authority.now_unix_seconds, queue.snapshot(), orchestrator.snapshot());
        if (bridge_result.pending_operation->transport()
                != snapshot_.configuration.transport
            || bridge_result.pending_operation->host_binding().configuration_sha256
                != snapshot_.configuration.host_configuration_sha256)
            return failure(Code::transport_blocked,
                "Approved operation crossed its configured transport binding.",
                authority.now_unix_seconds, queue.snapshot(), orchestrator.snapshot());
        snapshot_.phase = Phase::awaiting_transport_response;
        snapshot_.pending_operation_id =
            bridge_result.pending_operation->operation_id();
        snapshot_.attempts = 0u;
        snapshot_.next_retry_at_unix_seconds = 0u;
        synchronize(queue.snapshot(), orchestrator.snapshot());
        commit_mutation();
        Result approved = result(Code::awaiting_response, snapshot_,
            "Human-approved guarded MCP operation awaits one exact response.");
        approved.bridge = std::move(bridge_result);
        return approved;
    }

    Result Scheduler::complete_response(
        const Authority& authority,
        const self_iteration_orchestrator::PendingOperation& operation,
        mcp_orchestrator_bridge::HostOperationResult response,
        iteration_campaign_queue::Queue& queue,
        mcp_orchestrator_bridge::Bridge& bridge,
        self_iteration_orchestrator::Orchestrator& orchestrator)
    {
        if (const Result checked = validate(authority, queue.snapshot(),
                orchestrator.snapshot()); !checked)
            return checked;
        if (std::ranges::find(snapshot_.seen_operation_ids,
                operation.operation_id()) != snapshot_.seen_operation_ids.end())
            return result(Code::replay_refused, snapshot_,
                "MCP operation response was already consumed.");
        if (snapshot_.phase != Phase::awaiting_transport_response
            || operation.operation_id() != snapshot_.pending_operation_id)
            return result(Code::invalid_transition, snapshot_,
                "Response does not bind the active MCP operation.");
        if (snapshot_.attempts > 0u
            && authority.now_unix_seconds < snapshot_.next_retry_at_unix_seconds)
            return result(Code::transport_backoff, snapshot_,
                "Transport completion retry remains inside deterministic backoff.");
        const auto response_copy = response;
        auto bridge_result = bridge.complete_operation(
            operation, std::move(response), orchestrator);
        if (!bridge_result)
            return failure(Code::transport_backoff, bridge_result.status,
                authority.now_unix_seconds, queue.snapshot(), orchestrator.snapshot());
        const std::string payload = response_payload_digest(
            response_copy, bridge_result);
        const std::string receipt_id = digest(
            "epoch-ai-campaign-completion/v1\n" + operation.operation_id()
            + "\n" + snapshot_.request.request_sha256 + "\n" + payload);
        const auto queue_response = iteration_campaign_queue::make_response_receipt(
            snapshot_.request, receipt_id, payload,
            response_copy.now_unix_seconds);
        const auto recorded = queue.record_response(queue_response);
        if (!recorded)
            return failure(Code::transport_blocked,
                "Queue refused the exact bridge completion receipt.",
                authority.now_unix_seconds, queue.snapshot(), orchestrator.snapshot());
        snapshot_.seen_operation_ids.push_back(operation.operation_id());
        snapshot_.phase = Phase::awaiting_human_review;
        snapshot_.attempts = 0u;
        snapshot_.next_retry_at_unix_seconds = 0u;
        snapshot_.last_failure_sha256.clear();
        synchronize(queue.snapshot(), orchestrator.snapshot());
        commit_mutation();
        Result completed = result(Code::awaiting_human_review, snapshot_,
            "Transport response is digest-bound and awaits explicit queue review.");
        completed.bridge = std::move(bridge_result);
        return completed;
    }

    Result Scheduler::cancel(
        const Authority& authority,
        iteration_campaign_queue::Queue& queue)
    {
        // Cancellation is intentionally queue-local. It never auto-approves a
        // bridge mutation or expands transport authority.
        if (!initialized_ || !authority.operator_permitted
            || authority.actor_sha256 != snapshot_.configuration.human_authority_sha256
            || authority.expected_generation != snapshot_.generation
            || authority.expected_state_sha256 != snapshot_.state_sha256
            || authority.expected_queue_state_sha256 != queue.snapshot().state_sha256
            || snapshot_.objective_id.empty())
            return result(Code::invalid_authority, snapshot_,
                "Cancellation requires exact scheduler and queue authority.");
        const auto cancelled = queue.cancel(
            iteration_campaign_queue::HumanActionToken{
                queue.snapshot().state_sha256,
                snapshot_.configuration.human_authority_sha256,
                queue.snapshot().generation,
                authority.now_unix_seconds,
                true}, snapshot_.objective_id);
        if (!cancelled)
            return result(Code::invalid_transition, snapshot_,
                "Queue objective cannot be cancelled in its current state.");
        snapshot_.phase = Phase::cancelled;
        snapshot_.queue_generation = queue.snapshot().generation;
        snapshot_.queue_state_sha256 = queue.snapshot().state_sha256;
        commit_mutation();
        return result(Code::cancelled, snapshot_,
            "Operator cancelled the exact queued objective; late transport data is refused.");
    }

    Result Scheduler::failure(
        Code code,
        std::string status,
        std::uint64_t now_unix_seconds,
        const iteration_campaign_queue::QueueSnapshot& queue,
        const self_iteration_orchestrator::Snapshot& orchestrator)
    {
        ++snapshot_.attempts;
        snapshot_.last_failure_sha256 = digest(status);
        if (snapshot_.attempts >= snapshot_.configuration.retry.maximum_attempts
            || code == Code::transport_blocked)
        {
            snapshot_.phase = Phase::blocked;
            code = Code::transport_blocked;
        }
        else
        {
            std::uint64_t delay = snapshot_.configuration.retry.initial_backoff_seconds;
            for (std::uint32_t index = 1u; index < snapshot_.attempts; ++index)
                delay = std::min(delay * 2u,
                    snapshot_.configuration.retry.maximum_backoff_seconds);
            snapshot_.next_retry_at_unix_seconds = now_unix_seconds + delay;
            if (snapshot_.phase != Phase::awaiting_transport_response)
                snapshot_.phase = Phase::backoff;
            code = Code::transport_backoff;
        }
        synchronize(queue, orchestrator);
        commit_mutation();
        return result(code, snapshot_, std::move(status));
    }

    void Scheduler::synchronize(
        const iteration_campaign_queue::QueueSnapshot& queue,
        const self_iteration_orchestrator::Snapshot& orchestrator)
    {
        snapshot_.queue_generation = queue.generation;
        snapshot_.queue_state_sha256 = queue.state_sha256;
        snapshot_.orchestrator_generation = orchestrator.generation;
        snapshot_.orchestrator_state_sha256 = orchestrator.state_sha256;
    }

    void Scheduler::commit_mutation()
    {
        snapshot_.previous_state_sha256 = snapshot_.state_sha256;
        ++snapshot_.generation;
        snapshot_.state_sha256 = digest(canonical_state(snapshot_));
    }

    Checkpoint Scheduler::checkpoint() const
    {
        if (!initialized_ || !snapshot_valid(snapshot_)
            || digest(canonical_state(snapshot_)) != snapshot_.state_sha256)
            return {};
        Writer body{};
        body.raw(canonical_state(snapshot_));
        body.text(snapshot_.state_sha256);
        const auto body_bytes = body.take();
        Writer outer{};
        outer.text(checkpoint_schema);
        outer.u32(checkpoint_version);
        outer.u32(static_cast<std::uint32_t>(body_bytes.size()));
        outer.raw(body_bytes);
        outer.text(digest(body_bytes));
        const auto encoded = outer.take();
        Checkpoint checkpoint{};
        checkpoint.sha256 = digest(encoded);
        checkpoint.bytes.reserve(encoded.size());
        for (const auto byte : encoded)
            checkpoint.bytes.push_back(static_cast<std::byte>(byte));
        return checkpoint;
    }

    Result Scheduler::restore(
        std::span<const std::byte> bytes,
        std::uint64_t now_unix_seconds,
        const iteration_campaign_queue::QueueSnapshot& queue,
        const self_iteration_orchestrator::Snapshot& orchestrator)
    {
        if (bytes.empty() || bytes.size() > maximum_checkpoint_bytes)
            return result(Code::invalid_checkpoint, snapshot_,
                "Scheduler checkpoint size is invalid.");
        Reader outer{std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()}};
        std::string schema{};
        std::uint32_t version{};
        std::uint32_t body_size{};
        std::span<const std::uint8_t> body{};
        std::string body_sha{};
        if (!outer.text(schema, 64u) || schema != checkpoint_schema
            || !outer.u32(version) || version != checkpoint_version
            || !outer.u32(body_size) || body_size > maximum_checkpoint_bytes
            || !outer.raw(body, body_size)
            || !outer.text(body_sha, 64u) || outer.remaining() != 0u
            || !hex64(body_sha) || digest(body) != body_sha)
            return result(Code::invalid_checkpoint, snapshot_,
                "Scheduler checkpoint envelope failed exact digest validation.");
        Reader reader{body};
        Snapshot restored{};
        if (!read_snapshot(reader, restored)
            || !reader.text(restored.state_sha256, 64u)
            || reader.remaining() != 0u || !snapshot_valid(restored)
            || digest(canonical_state(restored)) != restored.state_sha256
            || now_unix_seconds < restored.configuration.created_at_unix_seconds
            || now_unix_seconds > restored.configuration.expires_at_unix_seconds
            || !binding_matches(restored.configuration, queue, orchestrator)
            || restored.queue_generation != queue.generation
            || restored.queue_state_sha256 != queue.state_sha256
            || restored.orchestrator_generation != orchestrator.generation
            || restored.orchestrator_state_sha256 != orchestrator.state_sha256)
            return result(Code::invalid_checkpoint, snapshot_,
                "Scheduler checkpoint is tampered, expired, or stale against live authorities.");
        snapshot_ = std::move(restored);
        initialized_ = true;
        return result(Code::ready, snapshot_,
            "Scheduler resumed with exact queue, orchestrator, retry, and replay state.");
    }

    const Snapshot& Scheduler::snapshot() const noexcept
    {
        return snapshot_;
    }
}
