/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.iteration_supervisor_control;

import core.sha256;

namespace epochengine::ai::iteration_supervisor_control
{
    namespace
    {
        constexpr std::uint32_t checkpoint_version = 1u;
        constexpr std::uint64_t maximum_duration_seconds = 7u * 24u * 60u * 60u;
        constexpr std::uint32_t absolute_maximum_query_items = 256u;
        constexpr std::uint32_t absolute_maximum_receipts = 4096u;
        constexpr std::uint32_t absolute_maximum_checkpoint_bytes = 4u * 1024u * 1024u;

        class Writer final
        {
        public:
            void u8(std::uint8_t value) { bytes_.push_back(value); }
            void u32(std::uint32_t value)
            {
                for (std::uint32_t shift = 0u; shift != 32u; shift += 8u)
                    bytes_.push_back(static_cast<std::uint8_t>(value >> shift));
            }
            void u64(std::uint64_t value)
            {
                for (std::uint32_t shift = 0u; shift != 64u; shift += 8u)
                    bytes_.push_back(static_cast<std::uint8_t>(value >> shift));
            }
            void boolean(bool value) { u8(value ? 1u : 0u); }
            void text(std::string_view value)
            {
                u32(static_cast<std::uint32_t>(value.size()));
                bytes_.insert(bytes_.end(), value.begin(), value.end());
            }
            void raw(std::span<const std::uint8_t> value)
            {
                bytes_.insert(bytes_.end(), value.begin(), value.end());
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
            explicit Reader(std::span<const std::uint8_t> bytes) noexcept
                : bytes_{bytes}
            {
            }
            [[nodiscard]] bool u8(std::uint8_t& value) noexcept
            {
                if (offset_ >= bytes_.size()) return false;
                value = bytes_[offset_++];
                return true;
            }
            [[nodiscard]] bool u32(std::uint32_t& value) noexcept
            {
                if (remaining() < 4u) return false;
                value = 0u;
                for (std::uint32_t shift = 0u; shift != 32u; shift += 8u)
                    value |= static_cast<std::uint32_t>(bytes_[offset_++]) << shift;
                return true;
            }
            [[nodiscard]] bool u64(std::uint64_t& value) noexcept
            {
                if (remaining() < 8u) return false;
                value = 0u;
                for (std::uint32_t shift = 0u; shift != 64u; shift += 8u)
                    value |= static_cast<std::uint64_t>(bytes_[offset_++]) << shift;
                return true;
            }
            [[nodiscard]] bool loose_boolean(bool& value) noexcept
            {
                std::uint8_t encoded{};
                if (!u8(encoded)) return false;
                value = encoded != 0u;
                return true;
            }
            [[nodiscard]] bool text(std::string& value, std::size_t maximum)
            {
                std::uint32_t length{};
                if (!u32(length) || length > maximum || remaining() < length)
                    return false;
                value.assign(reinterpret_cast<const char*>(bytes_.data() + offset_), length);
                offset_ += length;
                return true;
            }
            [[nodiscard]] bool raw(
                std::span<const std::uint8_t>& value,
                std::size_t length) noexcept
            {
                if (remaining() < length) return false;
                value = bytes_.subspan(offset_, length);
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

        [[nodiscard]] bool hex64(std::string_view value) noexcept
        {
            return value.size() == 64u && std::ranges::all_of(value, [](char ch)
            {
                return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
            });
        }

        [[nodiscard]] bool identifier(std::string_view value) noexcept
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

        [[nodiscard]] bool optional_identifier(std::string_view value) noexcept
        {
            return value.empty() || identifier(value);
        }

        [[nodiscard]] bool limits_valid(const Limits& limits) noexcept
        {
            return limits.maximum_query_items > 0u
                && limits.maximum_query_items <= absolute_maximum_query_items
                && limits.maximum_receipts > 0u
                && limits.maximum_receipts <= absolute_maximum_receipts
                && limits.maximum_checkpoint_bytes >= 4096u
                && limits.maximum_checkpoint_bytes <= absolute_maximum_checkpoint_bytes;
        }

        [[nodiscard]] bool configuration_valid(const Configuration& configuration)
        {
            return identifier(configuration.control_id)
                && identifier(configuration.session_id)
                && hex64(configuration.actor_sha256)
                && identifier(configuration.campaign_id)
                && configuration.created_at_unix_seconds > 0u
                && configuration.expires_at_unix_seconds
                    > configuration.created_at_unix_seconds
                && configuration.expires_at_unix_seconds
                    - configuration.created_at_unix_seconds
                    <= maximum_duration_seconds
                && limits_valid(configuration.limits);
        }

        [[nodiscard]] const iteration_campaign_queue::WorkItem* campaign_item(
            const iteration_campaign_queue::QueueSnapshot& queue,
            std::string_view campaign_id,
            std::string_view objective_id = {})
        {
            const auto found = std::ranges::find_if(queue.items,
                [campaign_id, objective_id](const auto& item)
                {
                    return item.request.campaign.campaign_id == campaign_id
                        && (objective_id.empty()
                            || item.request.objective_id == objective_id);
                });
            return found == queue.items.end() ? nullptr : &*found;
        }

        [[nodiscard]] bool safe_dependencies(
            const iteration_campaign_queue::QueueSnapshot& queue,
            const iteration_campaign_scheduler::Snapshot& scheduler)
        {
            return !queue.source_write_permitted && !queue.promotion_permitted
                && !queue.release_permitted && !queue.server_permitted
                && !queue.network_listener_permitted
                && !scheduler.source_write_permitted
                && !scheduler.promotion_permitted && !scheduler.release_permitted
                && !scheduler.server_permitted
                && !scheduler.network_listener_permitted;
        }

        [[nodiscard]] bool binding_valid(
            const Configuration& configuration,
            const iteration_campaign_queue::QueueSnapshot& queue,
            const iteration_campaign_scheduler::Snapshot& scheduler)
        {
            return queue.configuration.queue_id
                    == scheduler.configuration.queue_id
                && queue.configuration.human_authority_sha256
                    == configuration.actor_sha256
                && scheduler.configuration.human_authority_sha256
                    == configuration.actor_sha256
                && campaign_item(queue, configuration.campaign_id) != nullptr
                && (scheduler.objective_id.empty()
                    || campaign_item(queue, configuration.campaign_id,
                        scheduler.objective_id) != nullptr)
                && safe_dependencies(queue, scheduler);
        }

        void write_limits(Writer& writer, const Limits& limits)
        {
            writer.u32(limits.maximum_query_items);
            writer.u32(limits.maximum_receipts);
            writer.u32(limits.maximum_checkpoint_bytes);
        }

        [[nodiscard]] bool read_limits(Reader& reader, Limits& limits)
        {
            return reader.u32(limits.maximum_query_items)
                && reader.u32(limits.maximum_receipts)
                && reader.u32(limits.maximum_checkpoint_bytes)
                && limits_valid(limits);
        }

        void write_configuration(Writer& writer, const Configuration& configuration)
        {
            writer.text(configuration.control_id);
            writer.text(configuration.session_id);
            writer.text(configuration.actor_sha256);
            writer.text(configuration.campaign_id);
            writer.u64(configuration.created_at_unix_seconds);
            writer.u64(configuration.expires_at_unix_seconds);
            write_limits(writer, configuration.limits);
        }

        [[nodiscard]] bool read_configuration(
            Reader& reader,
            Configuration& configuration)
        {
            return reader.text(configuration.control_id, 128u)
                && reader.text(configuration.session_id, 128u)
                && reader.text(configuration.actor_sha256, 64u)
                && reader.text(configuration.campaign_id, 128u)
                && reader.u64(configuration.created_at_unix_seconds)
                && reader.u64(configuration.expires_at_unix_seconds)
                && read_limits(reader, configuration.limits)
                && configuration_valid(configuration);
        }

        void write_string_list(
            Writer& writer,
            const std::vector<std::string>& values)
        {
            writer.u32(static_cast<std::uint32_t>(values.size()));
            for (const auto& value : values) writer.text(value);
        }

        [[nodiscard]] bool read_string_list(
            Reader& reader,
            std::vector<std::string>& values,
            std::uint32_t maximum,
            bool identifiers)
        {
            std::uint32_t count{};
            if (!reader.u32(count) || count > maximum) return false;
            values.reserve(count);
            for (std::uint32_t index = 0u; index < count; ++index)
            {
                std::string value{};
                if (!reader.text(value, identifiers ? 128u : 64u)
                    || (identifiers ? !identifier(value) : !hex64(value))
                    || std::ranges::find(values, value) != values.end())
                    return false;
                values.push_back(std::move(value));
            }
            return true;
        }

        [[nodiscard]] std::vector<std::uint8_t> state_bytes(
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
            writer.u64(snapshot.scheduler_generation);
            writer.text(snapshot.scheduler_state_sha256);
            writer.text(snapshot.active_objective_id);
            writer.text(snapshot.active_operation_id);
            writer.text(snapshot.last_receipt_sha256);
            write_string_list(writer, snapshot.seen_command_ids);
            write_string_list(writer, snapshot.seen_command_sha256);
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
                || phase > static_cast<std::uint8_t>(ControlPhase::cancelled)
                || !reader.u64(snapshot.queue_generation)
                || !reader.text(snapshot.queue_state_sha256, 64u)
                || !reader.u64(snapshot.scheduler_generation)
                || !reader.text(snapshot.scheduler_state_sha256, 64u)
                || !reader.text(snapshot.active_objective_id, 128u)
                || !reader.text(snapshot.active_operation_id, 128u)
                || !reader.text(snapshot.last_receipt_sha256, 64u)
                || !read_string_list(reader, snapshot.seen_command_ids,
                    snapshot.configuration.limits.maximum_receipts, true)
                || !read_string_list(reader, snapshot.seen_command_sha256,
                    snapshot.configuration.limits.maximum_receipts, false))
                return false;
            snapshot.phase = static_cast<ControlPhase>(phase);
            snapshot.source_write_permitted = false;
            snapshot.arbitrary_file_read_permitted = false;
            snapshot.model_launch_permitted = false;
            snapshot.promotion_permitted = false;
            snapshot.release_permitted = false;
            snapshot.server_permitted = false;
            snapshot.network_listener_permitted = false;
            return true;
        }

        [[nodiscard]] bool snapshot_valid(const Snapshot& snapshot)
        {
            return configuration_valid(snapshot.configuration)
                && snapshot.generation > 0u
                && hex64(snapshot.state_sha256)
                && (snapshot.previous_state_sha256.empty()
                    || hex64(snapshot.previous_state_sha256))
                && snapshot.queue_generation > 0u
                && hex64(snapshot.queue_state_sha256)
                && snapshot.scheduler_generation > 0u
                && hex64(snapshot.scheduler_state_sha256)
                && optional_identifier(snapshot.active_objective_id)
                && optional_identifier(snapshot.active_operation_id)
                && (snapshot.last_receipt_sha256.empty()
                    || hex64(snapshot.last_receipt_sha256))
                && snapshot.seen_command_ids.size()
                    == snapshot.seen_command_sha256.size()
                && snapshot.seen_command_ids.size()
                    <= snapshot.configuration.limits.maximum_receipts
                && !snapshot.source_write_permitted
                && !snapshot.arbitrary_file_read_permitted
                && !snapshot.model_launch_permitted
                && !snapshot.promotion_permitted && !snapshot.release_permitted
                && !snapshot.server_permitted
                && !snapshot.network_listener_permitted;
        }

        void write_command(Writer& writer, const Command& command)
        {
            writer.text(command_schema);
            writer.text(command.command_id);
            writer.u8(static_cast<std::uint8_t>(command.kind));
            writer.text(command.control_id);
            writer.text(command.session_id);
            writer.text(command.actor_sha256);
            writer.text(command.campaign_id);
            writer.text(command.objective_id);
            writer.text(command.operation_id);
            writer.u64(command.expected_control_generation);
            writer.text(command.expected_control_state_sha256);
            writer.u64(command.expected_queue_generation);
            writer.text(command.expected_queue_state_sha256);
            writer.u64(command.expected_scheduler_generation);
            writer.text(command.expected_scheduler_state_sha256);
            writer.u64(command.now_unix_seconds);
            writer.boolean(command.operator_approved);
            writer.boolean(command.source_write_requested);
            writer.boolean(command.arbitrary_file_read_requested);
            writer.boolean(command.model_launch_requested);
            writer.boolean(command.promotion_requested);
            writer.boolean(command.release_requested);
            writer.boolean(command.server_requested);
            writer.boolean(command.network_listener_requested);
        }

        enum class ParseCode : std::uint8_t { valid, malformed, noncanonical };

        [[nodiscard]] ParseCode parse_command(
            std::span<const std::byte> encoded,
            Command& command,
            std::string& command_sha256)
        {
            if (encoded.empty() || encoded.size() > 16u * 1024u)
                return ParseCode::malformed;
            const auto bytes = std::span<const std::uint8_t>{
                reinterpret_cast<const std::uint8_t*>(encoded.data()), encoded.size()};
            Reader reader{bytes};
            std::string schema{};
            std::uint8_t kind{};
            if (!reader.text(schema, 64u) || schema != command_schema
                || !reader.text(command.command_id, 128u)
                || !reader.u8(kind)
                || kind > static_cast<std::uint8_t>(CommandKind::synchronize)
                || !reader.text(command.control_id, 128u)
                || !reader.text(command.session_id, 128u)
                || !reader.text(command.actor_sha256, 64u)
                || !reader.text(command.campaign_id, 128u)
                || !reader.text(command.objective_id, 128u)
                || !reader.text(command.operation_id, 128u)
                || !reader.u64(command.expected_control_generation)
                || !reader.text(command.expected_control_state_sha256, 64u)
                || !reader.u64(command.expected_queue_generation)
                || !reader.text(command.expected_queue_state_sha256, 64u)
                || !reader.u64(command.expected_scheduler_generation)
                || !reader.text(command.expected_scheduler_state_sha256, 64u)
                || !reader.u64(command.now_unix_seconds)
                || !reader.loose_boolean(command.operator_approved)
                || !reader.loose_boolean(command.source_write_requested)
                || !reader.loose_boolean(command.arbitrary_file_read_requested)
                || !reader.loose_boolean(command.model_launch_requested)
                || !reader.loose_boolean(command.promotion_requested)
                || !reader.loose_boolean(command.release_requested)
                || !reader.loose_boolean(command.server_requested)
                || !reader.loose_boolean(command.network_listener_requested)
                || reader.remaining() != 0u)
                return ParseCode::malformed;
            command.kind = static_cast<CommandKind>(kind);
            Writer canonical{};
            write_command(canonical, command);
            if (!std::ranges::equal(canonical.bytes(), bytes))
                return ParseCode::noncanonical;
            command_sha256 = digest(bytes);
            return ParseCode::valid;
        }

        [[nodiscard]] bool command_shape_valid(const Command& command)
        {
            return identifier(command.command_id)
                && identifier(command.control_id)
                && identifier(command.session_id)
                && hex64(command.actor_sha256)
                && identifier(command.campaign_id)
                && optional_identifier(command.objective_id)
                && optional_identifier(command.operation_id)
                && command.expected_control_generation > 0u
                && hex64(command.expected_control_state_sha256)
                && command.expected_queue_generation > 0u
                && hex64(command.expected_queue_state_sha256)
                && command.expected_scheduler_generation > 0u
                && hex64(command.expected_scheduler_state_sha256)
                && command.now_unix_seconds > 0u;
        }

        [[nodiscard]] bool broadens_authority(const Command& command) noexcept
        {
            return command.source_write_requested
                || command.arbitrary_file_read_requested
                || command.model_launch_requested
                || command.promotion_requested || command.release_requested
                || command.server_requested || command.network_listener_requested;
        }

        [[nodiscard]] bool contains(
            const std::vector<std::string>& values,
            std::string_view value)
        {
            return std::ranges::find(values, value) != values.end();
        }

        [[nodiscard]] bool exact_observation(
            const Snapshot& snapshot,
            const iteration_campaign_queue::QueueSnapshot& queue,
            const iteration_campaign_scheduler::Snapshot& scheduler)
        {
            return snapshot.queue_generation == queue.generation
                && snapshot.queue_state_sha256 == queue.state_sha256
                && snapshot.scheduler_generation == scheduler.generation
                && snapshot.scheduler_state_sha256 == scheduler.state_sha256;
        }

        [[nodiscard]] bool next_observation(
            const Snapshot& snapshot,
            const iteration_campaign_queue::QueueSnapshot& queue,
            const iteration_campaign_scheduler::Snapshot& scheduler)
        {
            const bool queue_next = (snapshot.queue_generation == queue.generation
                    && snapshot.queue_state_sha256 == queue.state_sha256)
                || (queue.generation == snapshot.queue_generation + 1u
                    && queue.previous_state_sha256 == snapshot.queue_state_sha256);
            const bool scheduler_next =
                (snapshot.scheduler_generation == scheduler.generation
                    && snapshot.scheduler_state_sha256 == scheduler.state_sha256)
                || (scheduler.generation == snapshot.scheduler_generation + 1u
                    && scheduler.previous_state_sha256
                        == snapshot.scheduler_state_sha256);
            return queue_next && scheduler_next
                && (!exact_observation(snapshot, queue, scheduler));
        }

        [[nodiscard]] QuerySnapshot make_query(
            const Snapshot& control,
            const iteration_campaign_queue::QueueSnapshot& queue,
            const iteration_campaign_scheduler::Snapshot& scheduler)
        {
            QuerySnapshot query{};
            query.control_id = control.configuration.control_id;
            query.session_id = control.configuration.session_id;
            query.campaign_id = control.configuration.campaign_id;
            query.control_generation = control.generation;
            query.control_state_sha256 = control.state_sha256;
            query.control_phase = control.phase;
            query.queue_generation = queue.generation;
            query.queue_state_sha256 = queue.state_sha256;
            query.scheduler_generation = scheduler.generation;
            query.scheduler_state_sha256 = scheduler.state_sha256;
            query.scheduler_phase = scheduler.phase;
            query.active_objective_id = scheduler.objective_id;
            query.active_operation_id = scheduler.pending_operation_id;
            query.next_retry_at_unix_seconds = scheduler.next_retry_at_unix_seconds;
            for (const auto& item : queue.items)
            {
                if (item.request.campaign.campaign_id
                        != control.configuration.campaign_id)
                    continue;
                if (query.items.size()
                    >= control.configuration.limits.maximum_query_items)
                {
                    query.truncated = true;
                    break;
                }
                query.items.push_back(ItemSummary{
                    item.request.objective_id,
                    item.objective_sha256,
                    item.request.campaign.campaign_id,
                    item.request.campaign.state_sha256,
                    item.request.campaign.record_generation,
                    item.phase,
                    item.model_request.request_id,
                    item.model_request.request_sha256,
                    item.model_response.receipt_id,
                    item.model_response.response_sha256});
            }
            return query;
        }

        [[nodiscard]] Result result(
            Code code,
            const Snapshot& snapshot,
            std::string status)
        {
            return Result{.code = code, .snapshot = snapshot,
                .status = std::move(status)};
        }

        [[nodiscard]] std::string transition_digest(
            const Snapshot& snapshot,
            const Command& command,
            std::string_view command_sha256,
            Outcome outcome,
            std::string_view queue_before,
            std::string_view scheduler_before)
        {
            Writer writer{};
            writer.text("epoch-ai-supervisor-transition/v1");
            writer.text(snapshot.configuration.control_id);
            writer.text(snapshot.configuration.session_id);
            writer.text(snapshot.configuration.campaign_id);
            writer.text(command.command_id);
            writer.text(command_sha256);
            writer.u8(static_cast<std::uint8_t>(outcome));
            writer.text(snapshot.state_sha256);
            writer.text(queue_before);
            writer.text(snapshot.queue_state_sha256);
            writer.text(scheduler_before);
            writer.text(snapshot.scheduler_state_sha256);
            writer.text(command.objective_id);
            writer.text(command.operation_id);
            writer.u64(command.now_unix_seconds);
            return digest(writer.bytes());
        }

        [[nodiscard]] std::string receipt_digest(const Receipt& receipt)
        {
            Writer writer{};
            writer.text("epoch-ai-supervisor-receipt/v1");
            writer.text(receipt.receipt_id);
            writer.text(receipt.command_id);
            writer.text(receipt.command_sha256);
            writer.text(receipt.previous_receipt_sha256);
            writer.text(receipt.control_state_before_sha256);
            writer.text(receipt.queue_state_before_sha256);
            writer.text(receipt.queue_state_after_sha256);
            writer.text(receipt.scheduler_state_before_sha256);
            writer.text(receipt.scheduler_state_after_sha256);
            writer.text(receipt.campaign_id);
            writer.text(receipt.objective_id);
            writer.text(receipt.operation_id);
            writer.u8(static_cast<std::uint8_t>(receipt.outcome));
            writer.u64(receipt.completed_at_unix_seconds);
            writer.text(receipt.transition_sha256);
            return digest(writer.bytes());
        }
    }

    std::vector<std::byte> canonical_command(const Command& command)
    {
        Writer writer{};
        write_command(writer, command);
        std::vector<std::byte> encoded{};
        encoded.reserve(writer.bytes().size());
        for (const std::uint8_t byte : writer.bytes())
            encoded.push_back(static_cast<std::byte>(byte));
        return encoded;
    }

    Result Surface::begin(
        const Configuration& configuration,
        const iteration_campaign_queue::QueueSnapshot& queue,
        const iteration_campaign_scheduler::Snapshot& scheduler)
    {
        if (!configuration_valid(configuration)
            || !binding_valid(configuration, queue, scheduler))
            return result(Code::invalid_configuration, snapshot_,
                "Supervisor configuration does not bind one safe campaign queue and scheduler.");
        snapshot_ = {};
        snapshot_.configuration = configuration;
        snapshot_.phase = ControlPhase::active;
        observe(queue, scheduler);
        snapshot_.generation = 1u;
        snapshot_.state_sha256 = digest(state_bytes(snapshot_));
        initialized_ = true;
        return result(Code::ready, snapshot_,
            "Supervisor is bound to one in-process campaign control session.");
    }

    QueryResult Surface::query(
        const iteration_campaign_queue::QueueSnapshot& queue,
        const iteration_campaign_scheduler::Snapshot& scheduler) const
    {
        if (!initialized_ || !snapshot_valid(snapshot_)
            || !binding_valid(snapshot_.configuration, queue, scheduler))
            return {Code::invalid_configuration, {},
                "Supervisor is not initialized against safe dependencies."};
        if (!exact_observation(snapshot_, queue, scheduler))
            return {Code::stale_state, {},
                "Live queue or scheduler state requires an exact synchronize command."};
        return {Code::ready, make_query(snapshot_, queue, scheduler),
            "Bounded campaign control snapshot is current."};
    }

    Result Surface::submit(
        std::span<const std::byte> encoded,
        iteration_campaign_queue::Queue& queue,
        iteration_campaign_scheduler::Scheduler& scheduler)
    {
        Command command{};
        std::string command_sha256{};
        const ParseCode parsed = parse_command(encoded, command, command_sha256);
        if (parsed == ParseCode::malformed)
            return result(Code::malformed_command, snapshot_,
                "Control command failed bounded binary decoding.");
        if (parsed == ParseCode::noncanonical)
            return result(Code::noncanonical_command, snapshot_,
                "Control command bytes are semantically decodable but noncanonical.");
        if (initialized_ && (contains(snapshot_.seen_command_ids, command.command_id)
                || contains(snapshot_.seen_command_sha256, command_sha256)))
            return result(Code::replay_refused, snapshot_,
                "Control command identifier or digest was already consumed.");
        if (!initialized_ || !snapshot_valid(snapshot_)
            || !command_shape_valid(command))
            return result(Code::malformed_command, snapshot_,
                "Control command fields are malformed or the surface is unavailable.");
        if (broadens_authority(command))
            return result(Code::authority_broadening, snapshot_,
                "Control commands cannot request source, model, release, or server authority.");
        if (!command.operator_approved
            || command.actor_sha256 != snapshot_.configuration.actor_sha256)
            return result(Code::invalid_authority, snapshot_,
                "Exact human actor approval is required.");
        if (command.control_id != snapshot_.configuration.control_id
            || command.session_id != snapshot_.configuration.session_id
            || command.campaign_id != snapshot_.configuration.campaign_id)
            return result(Code::unknown_identifier, snapshot_,
                "Control, session, or campaign identifier is unknown.");
        if (command.now_unix_seconds < snapshot_.configuration.created_at_unix_seconds
            || command.now_unix_seconds > snapshot_.configuration.expires_at_unix_seconds)
            return result(Code::invalid_authority, snapshot_,
                "Human control authority is outside its bounded lifetime.");
        if (command.expected_control_generation != snapshot_.generation
            || command.expected_control_state_sha256 != snapshot_.state_sha256)
            return result(Code::stale_state, snapshot_,
                "Control generation or digest is stale.");

        const auto& queue_live = queue.snapshot();
        const auto& scheduler_live = scheduler.snapshot();
        if (!binding_valid(snapshot_.configuration, queue_live, scheduler_live))
            return result(Code::invalid_configuration, snapshot_,
                "Live dependencies crossed the configured safe campaign binding.");
        if (command.expected_queue_generation != queue_live.generation
            || command.expected_queue_state_sha256 != queue_live.state_sha256
            || command.expected_scheduler_generation != scheduler_live.generation
            || command.expected_scheduler_state_sha256 != scheduler_live.state_sha256)
            return result(Code::stale_state, snapshot_,
                "Command does not bind the exact live queue and scheduler state.");
        if (snapshot_.seen_command_ids.size()
                >= snapshot_.configuration.limits.maximum_receipts)
            return result(Code::receipt_limit_reached, snapshot_,
                "Supervisor receipt capacity is exhausted.");

        if (command.kind == CommandKind::synchronize)
        {
            if (snapshot_.phase != ControlPhase::active
                || !next_observation(snapshot_, queue_live, scheduler_live)
                || command.objective_id != scheduler_live.objective_id
                || command.operation_id != scheduler_live.pending_operation_id)
                return result(Code::invalid_transition, snapshot_,
                    "Synchronize accepts exactly one adjacent live transition while active.");
            const std::string queue_before = snapshot_.queue_state_sha256;
            const std::string scheduler_before = snapshot_.scheduler_state_sha256;
            observe(queue_live, scheduler_live);
            Receipt receipt = commit(command, std::move(command_sha256),
                Outcome::synchronized, queue_before, scheduler_before);
            Result synchronized = result(Code::synchronized, snapshot_,
                "Supervisor adopted one exact adjacent queue/scheduler transition.");
            synchronized.receipt = std::move(receipt);
            return synchronized;
        }

        if (!exact_observation(snapshot_, queue_live, scheduler_live))
            return result(Code::stale_state, snapshot_,
                "Supervisor must synchronize before accepting another command.");
        if (command.objective_id != snapshot_.active_objective_id
            || command.operation_id != snapshot_.active_operation_id)
            return result(Code::unknown_identifier, snapshot_,
                "Objective or operation identifier does not bind the active scheduler state.");

        const std::string queue_before = snapshot_.queue_state_sha256;
        const std::string scheduler_before = snapshot_.scheduler_state_sha256;
        Outcome outcome{};
        Code code{};
        switch (command.kind)
        {
        case CommandKind::pause:
            if (snapshot_.phase != ControlPhase::active)
                return result(Code::invalid_transition, snapshot_,
                    "Pause requires an active control session.");
            snapshot_.phase = ControlPhase::paused;
            outcome = Outcome::paused;
            code = Code::paused;
            break;
        case CommandKind::resume:
            if (snapshot_.phase != ControlPhase::paused)
                return result(Code::invalid_transition, snapshot_,
                    "Resume requires a paused control session.");
            snapshot_.phase = ControlPhase::active;
            outcome = Outcome::resumed;
            code = Code::resumed;
            break;
        case CommandKind::cancel:
        {
            if ((snapshot_.phase != ControlPhase::active
                    && snapshot_.phase != ControlPhase::paused)
                || snapshot_.active_objective_id.empty())
                return result(Code::invalid_transition, snapshot_,
                    "Cancel requires one active scheduled objective.");
            auto cancelled = scheduler.cancel(
                iteration_campaign_scheduler::Authority{
                    command.actor_sha256,
                    scheduler_live.state_sha256,
                    queue_live.state_sha256,
                    scheduler_live.orchestrator_state_sha256,
                    scheduler_live.generation,
                    command.now_unix_seconds,
                    true}, queue);
            if (cancelled.code != iteration_campaign_scheduler::Code::cancelled)
                return result(Code::invalid_transition, snapshot_,
                    "Scheduler or queue refused cancellation in its current phase.");
            observe(queue.snapshot(), scheduler.snapshot());
            snapshot_.phase = ControlPhase::cancelled;
            outcome = Outcome::cancelled;
            code = Code::cancelled;
            Result accepted = result(code, snapshot_, {});
            accepted.scheduler = std::move(cancelled);
            Receipt receipt = commit(command, std::move(command_sha256), outcome,
                queue_before, scheduler_before);
            accepted.snapshot = snapshot_;
            accepted.receipt = std::move(receipt);
            accepted.status = "Exact campaign objective was cancelled; late data remains refused.";
            return accepted;
        }
        case CommandKind::retry:
            if (snapshot_.phase != ControlPhase::active
                || scheduler_live.phase
                    != iteration_campaign_scheduler::Phase::backoff
                || command.now_unix_seconds
                    < scheduler_live.next_retry_at_unix_seconds)
                return result(Code::invalid_transition, snapshot_,
                    "Retry requires active control after deterministic backoff expires.");
            outcome = Outcome::retry_authorized;
            code = Code::retry_authorized;
            break;
        case CommandKind::approve:
        case CommandKind::reject:
        {
            const auto* item = campaign_item(queue_live,
                snapshot_.configuration.campaign_id,
                snapshot_.active_objective_id);
            if (snapshot_.phase != ControlPhase::active || !item
                || scheduler_live.phase
                    != iteration_campaign_scheduler::Phase::awaiting_human_review
                || item->phase
                    != iteration_campaign_queue::ItemPhase::awaiting_human_review)
                return result(Code::invalid_transition, snapshot_,
                    "Human review requires one digest-bound response awaiting review.");
            const auto disposition = command.kind == CommandKind::approve
                ? iteration_campaign_queue::HumanDisposition::approve_for_campaign
                : iteration_campaign_queue::HumanDisposition::reject;
            auto reviewed = queue.review_response(
                iteration_campaign_queue::HumanActionToken{
                    queue_live.state_sha256,
                    command.actor_sha256,
                    queue_live.generation,
                    command.now_unix_seconds,
                    true}, snapshot_.active_objective_id, disposition);
            if (!reviewed)
                return result(Code::invalid_transition, snapshot_,
                    "Queue refused human review in its current phase.");
            observe(queue.snapshot(), scheduler.snapshot());
            snapshot_.phase = ControlPhase::reviewed;
            outcome = command.kind == CommandKind::approve
                ? Outcome::approved : Outcome::rejected;
            code = command.kind == CommandKind::approve
                ? Code::approved : Code::rejected;
            Result accepted = result(code, snapshot_, {});
            accepted.queue = std::move(reviewed);
            Receipt receipt = commit(command, std::move(command_sha256), outcome,
                queue_before, scheduler_before);
            accepted.snapshot = snapshot_;
            accepted.receipt = std::move(receipt);
            accepted.status = code == Code::approved
                ? "Human approved the exact response for campaign consideration only."
                : "Human rejected the exact response; no campaign step was promoted.";
            return accepted;
        }
        case CommandKind::synchronize:
            return result(Code::invalid_transition, snapshot_,
                "Synchronize was not accepted.");
        }

        Receipt receipt = commit(command, std::move(command_sha256), outcome,
            queue_before, scheduler_before);
        Result accepted = result(code, snapshot_,
            code == Code::retry_authorized
                ? "One exact retry authority is ready for the host scheduler call."
                : (code == Code::paused
                    ? "Campaign control is paused without touching transport state."
                    : "Campaign control resumed under the same exact authority."));
        accepted.receipt = std::move(receipt);
        if (code == Code::retry_authorized)
        {
            accepted.retry_authority = iteration_campaign_scheduler::Authority{
                command.actor_sha256,
                scheduler_live.state_sha256,
                queue_live.state_sha256,
                scheduler_live.orchestrator_state_sha256,
                scheduler_live.generation,
                command.now_unix_seconds,
                true};
        }
        return accepted;
    }

    void Surface::observe(
        const iteration_campaign_queue::QueueSnapshot& queue,
        const iteration_campaign_scheduler::Snapshot& scheduler)
    {
        snapshot_.queue_generation = queue.generation;
        snapshot_.queue_state_sha256 = queue.state_sha256;
        snapshot_.scheduler_generation = scheduler.generation;
        snapshot_.scheduler_state_sha256 = scheduler.state_sha256;
        snapshot_.active_objective_id = scheduler.objective_id;
        snapshot_.active_operation_id = scheduler.pending_operation_id;
    }

    Receipt Surface::commit(
        const Command& command,
        std::string command_sha256,
        Outcome outcome,
        std::string queue_before,
        std::string scheduler_before)
    {
        Receipt receipt{};
        receipt.command_id = command.command_id;
        receipt.command_sha256 = std::move(command_sha256);
        receipt.previous_receipt_sha256 = snapshot_.last_receipt_sha256;
        receipt.control_state_before_sha256 = snapshot_.state_sha256;
        receipt.queue_state_before_sha256 = std::move(queue_before);
        receipt.queue_state_after_sha256 = snapshot_.queue_state_sha256;
        receipt.scheduler_state_before_sha256 = std::move(scheduler_before);
        receipt.scheduler_state_after_sha256 = snapshot_.scheduler_state_sha256;
        receipt.campaign_id = snapshot_.configuration.campaign_id;
        receipt.objective_id = command.objective_id;
        receipt.operation_id = command.operation_id;
        receipt.outcome = outcome;
        receipt.completed_at_unix_seconds = command.now_unix_seconds;
        receipt.transition_sha256 = transition_digest(snapshot_, command,
            receipt.command_sha256, outcome,
            receipt.queue_state_before_sha256,
            receipt.scheduler_state_before_sha256);
        receipt.receipt_id = "control-" + receipt.transition_sha256.substr(0u, 24u);
        receipt.receipt_sha256 = receipt_digest(receipt);
        snapshot_.seen_command_ids.push_back(command.command_id);
        snapshot_.seen_command_sha256.push_back(receipt.command_sha256);
        snapshot_.last_receipt_sha256 = receipt.receipt_sha256;
        snapshot_.previous_state_sha256 = snapshot_.state_sha256;
        ++snapshot_.generation;
        snapshot_.state_sha256 = digest(state_bytes(snapshot_));
        return receipt;
    }

    Checkpoint Surface::checkpoint() const
    {
        if (!initialized_ || !snapshot_valid(snapshot_)
            || digest(state_bytes(snapshot_)) != snapshot_.state_sha256)
            return {};
        Writer body{};
        body.raw(state_bytes(snapshot_));
        body.text(snapshot_.state_sha256);
        const auto body_bytes = body.take();
        if (body_bytes.size() > snapshot_.configuration.limits.maximum_checkpoint_bytes)
            return {};
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
        for (const std::uint8_t byte : encoded)
            checkpoint.bytes.push_back(static_cast<std::byte>(byte));
        return checkpoint;
    }

    Result Surface::restore(
        std::span<const std::byte> checkpoint,
        std::uint64_t now_unix_seconds,
        const iteration_campaign_queue::QueueSnapshot& queue,
        const iteration_campaign_scheduler::Snapshot& scheduler)
    {
        if (checkpoint.empty()
            || checkpoint.size() > absolute_maximum_checkpoint_bytes)
            return result(Code::invalid_checkpoint, snapshot_,
                "Supervisor checkpoint size is invalid.");
        const auto bytes = std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(checkpoint.data()), checkpoint.size()};
        Reader outer{bytes};
        std::string schema{};
        std::uint32_t version{};
        std::uint32_t body_size{};
        std::span<const std::uint8_t> body{};
        std::string body_sha256{};
        if (!outer.text(schema, 64u) || schema != checkpoint_schema
            || !outer.u32(version) || version != checkpoint_version
            || !outer.u32(body_size) || body_size > absolute_maximum_checkpoint_bytes
            || !outer.raw(body, body_size)
            || !outer.text(body_sha256, 64u) || outer.remaining() != 0u
            || !hex64(body_sha256) || digest(body) != body_sha256)
            return result(Code::invalid_checkpoint, snapshot_,
                "Supervisor checkpoint envelope failed exact digest validation.");
        Reader reader{body};
        Snapshot restored{};
        if (!read_snapshot(reader, restored)
            || !reader.text(restored.state_sha256, 64u)
            || reader.remaining() != 0u || !snapshot_valid(restored)
            || digest(state_bytes(restored)) != restored.state_sha256
            || body_size > restored.configuration.limits.maximum_checkpoint_bytes
            || now_unix_seconds < restored.configuration.created_at_unix_seconds
            || now_unix_seconds > restored.configuration.expires_at_unix_seconds
            || !binding_valid(restored.configuration, queue, scheduler)
            || restored.queue_generation != queue.generation
            || restored.queue_state_sha256 != queue.state_sha256
            || restored.scheduler_generation != scheduler.generation
            || restored.scheduler_state_sha256 != scheduler.state_sha256
            || restored.active_objective_id != scheduler.objective_id
            || restored.active_operation_id != scheduler.pending_operation_id)
            return result(Code::invalid_checkpoint, snapshot_,
                "Supervisor checkpoint is tampered, expired, or stale.");
        snapshot_ = std::move(restored);
        initialized_ = true;
        return result(Code::ready, snapshot_,
            "Supervisor restored exact receipt, queue, and scheduler bindings.");
    }

    const Snapshot& Surface::snapshot() const noexcept
    {
        return snapshot_;
    }
}
