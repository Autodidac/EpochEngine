/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.iteration_campaign_queue;

import core.sha256;

namespace epochengine::ai::iteration_campaign_queue
{
    namespace
    {
        constexpr std::uint32_t checkpoint_version = 1u;
        constexpr std::uint64_t maximum_duration_seconds = 7u * 24u * 60u * 60u;
        constexpr std::uint32_t absolute_maximum_items = 256u;
        constexpr std::uint32_t absolute_maximum_seen = 4096u;
        constexpr std::uint32_t maximum_checkpoint_bytes = 4u * 1024u * 1024u;

        class ByteWriter final
        {
        public:
            void u8(std::uint8_t value)
            {
                bytes_.push_back(value);
            }

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

            void boolean(bool value)
            {
                u8(value ? 1u : 0u);
            }

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

        class ByteReader final
        {
        public:
            explicit ByteReader(std::span<const std::uint8_t> bytes) noexcept
                : bytes_{bytes}
            {
            }

            [[nodiscard]] bool u8(std::uint8_t& value) noexcept
            {
                if (offset_ >= bytes_.size())
                    return false;
                value = bytes_[offset_++];
                return true;
            }

            [[nodiscard]] bool u32(std::uint32_t& value) noexcept
            {
                if (remaining() < 4u)
                    return false;
                value = 0u;
                for (std::uint32_t shift = 0u; shift != 32u; shift += 8u)
                    value |= static_cast<std::uint32_t>(bytes_[offset_++]) << shift;
                return true;
            }

            [[nodiscard]] bool u64(std::uint64_t& value) noexcept
            {
                if (remaining() < 8u)
                    return false;
                value = 0u;
                for (std::uint32_t shift = 0u; shift != 64u; shift += 8u)
                    value |= static_cast<std::uint64_t>(bytes_[offset_++]) << shift;
                return true;
            }

            [[nodiscard]] bool boolean(bool& value) noexcept
            {
                std::uint8_t encoded{};
                if (!u8(encoded) || encoded > 1u)
                    return false;
                value = encoded != 0u;
                return true;
            }

            [[nodiscard]] bool text(
                std::string& value,
                std::size_t maximum_bytes)
            {
                std::uint32_t length{};
                if (!u32(length) || length > maximum_bytes || remaining() < length)
                    return false;
                value.assign(
                    reinterpret_cast<const char*>(bytes_.data() + offset_),
                    length);
                offset_ += length;
                return true;
            }

            [[nodiscard]] bool raw(
                std::span<const std::uint8_t>& value,
                std::size_t length) noexcept
            {
                if (remaining() < length)
                    return false;
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

        [[nodiscard]] std::string digest_bytes(
            std::span<const std::uint8_t> bytes)
        {
            return core::sha256::hex(core::sha256::hash(bytes));
        }

        [[nodiscard]] bool lowercase_hex(std::string_view value) noexcept
        {
            if (value.size() != 64u)
                return false;
            return std::ranges::all_of(value, [](char character)
            {
                return (character >= '0' && character <= '9')
                    || (character >= 'a' && character <= 'f');
            });
        }

        [[nodiscard]] bool safe_identifier(std::string_view value) noexcept
        {
            if (value.empty() || value.size() > 128u)
                return false;
            return std::ranges::all_of(value, [](char character)
            {
                return (character >= 'a' && character <= 'z')
                    || (character >= 'A' && character <= 'Z')
                    || (character >= '0' && character <= '9')
                    || character == '.' || character == '_' || character == '-';
            });
        }

        [[nodiscard]] bool safe_text(
            std::string_view value,
            std::size_t maximum_bytes) noexcept
        {
            if (value.empty() || value.size() > maximum_bytes)
                return false;
            return std::ranges::none_of(value, [](char character)
            {
                const auto byte = static_cast<unsigned char>(character);
                return byte == 0u || (byte < 0x20u && character != '\n'
                    && character != '\r' && character != '\t');
            });
        }

        [[nodiscard]] bool valid_limits(const QueueLimits& limits) noexcept
        {
            return limits.maximum_items > 0u
                && limits.maximum_items <= absolute_maximum_items
                && limits.maximum_seen_requests >= limits.maximum_items
                && limits.maximum_seen_requests <= absolute_maximum_seen
                && limits.maximum_seen_responses >= limits.maximum_items
                && limits.maximum_seen_responses <= absolute_maximum_seen
                && limits.maximum_objective_bytes > 0u
                && limits.maximum_objective_bytes <= 64u * 1024u
                && limits.maximum_locator_bytes > 0u
                && limits.maximum_locator_bytes <= 16u * 1024u;
        }

        [[nodiscard]] bool valid_configuration(
            const QueueConfiguration& configuration) noexcept
        {
            return safe_identifier(configuration.queue_id)
                && lowercase_hex(configuration.human_authority_sha256)
                && configuration.created_at_unix_seconds > 0u
                && configuration.expires_at_unix_seconds
                    > configuration.created_at_unix_seconds
                && configuration.expires_at_unix_seconds
                    - configuration.created_at_unix_seconds
                    <= maximum_duration_seconds
                && valid_limits(configuration.limits);
        }

        [[nodiscard]] bool valid_campaign(
            const CampaignReference& campaign,
            std::size_t maximum_locator_bytes) noexcept
        {
            return lowercase_hex(campaign.campaign_id)
                && lowercase_hex(campaign.target_key)
                && safe_text(campaign.state_locator, maximum_locator_bytes)
                && lowercase_hex(campaign.state_sha256)
                && campaign.record_generation > 0u;
        }

        [[nodiscard]] bool terminal(ItemPhase phase) noexcept
        {
            return phase == ItemPhase::completed || phase == ItemPhase::rejected
                || phase == ItemPhase::cancelled;
        }

        void write_limits(ByteWriter& writer, const QueueLimits& limits)
        {
            writer.u32(limits.maximum_items);
            writer.u32(limits.maximum_seen_requests);
            writer.u32(limits.maximum_seen_responses);
            writer.u32(limits.maximum_objective_bytes);
            writer.u32(limits.maximum_locator_bytes);
        }

        [[nodiscard]] bool read_limits(ByteReader& reader, QueueLimits& limits)
        {
            return reader.u32(limits.maximum_items)
                && reader.u32(limits.maximum_seen_requests)
                && reader.u32(limits.maximum_seen_responses)
                && reader.u32(limits.maximum_objective_bytes)
                && reader.u32(limits.maximum_locator_bytes)
                && valid_limits(limits);
        }

        void write_configuration(
            ByteWriter& writer,
            const QueueConfiguration& configuration)
        {
            writer.text(configuration.queue_id);
            writer.text(configuration.human_authority_sha256);
            writer.u64(configuration.created_at_unix_seconds);
            writer.u64(configuration.expires_at_unix_seconds);
            write_limits(writer, configuration.limits);
        }

        [[nodiscard]] bool read_configuration(
            ByteReader& reader,
            QueueConfiguration& configuration)
        {
            return reader.text(configuration.queue_id, 128u)
                && reader.text(configuration.human_authority_sha256, 64u)
                && reader.u64(configuration.created_at_unix_seconds)
                && reader.u64(configuration.expires_at_unix_seconds)
                && read_limits(reader, configuration.limits)
                && valid_configuration(configuration);
        }

        void write_campaign(ByteWriter& writer, const CampaignReference& campaign)
        {
            writer.text(campaign.campaign_id);
            writer.text(campaign.target_key);
            writer.text(campaign.state_locator);
            writer.text(campaign.state_sha256);
            writer.u64(campaign.record_generation);
        }

        [[nodiscard]] bool read_campaign(
            ByteReader& reader,
            CampaignReference& campaign,
            std::size_t maximum_locator_bytes)
        {
            return reader.text(campaign.campaign_id, 64u)
                && reader.text(campaign.target_key, 64u)
                && reader.text(campaign.state_locator, maximum_locator_bytes)
                && reader.text(campaign.state_sha256, 64u)
                && reader.u64(campaign.record_generation)
                && valid_campaign(campaign, maximum_locator_bytes);
        }

        void write_objective(ByteWriter& writer, const ObjectiveRequest& request)
        {
            writer.text(request.objective_id);
            writer.text(request.objective);
            writer.u8(request.priority);
            write_campaign(writer, request.campaign);
        }

        void write_request(ByteWriter& writer, const RequestReceipt& request)
        {
            writer.text(request.request_id);
            writer.text(request.request_sha256);
            writer.text(request.binding_sha256);
            writer.text(request.payload_sha256);
            writer.u64(request.issued_at_unix_seconds);
        }

        [[nodiscard]] bool read_request(ByteReader& reader, RequestReceipt& request)
        {
            return reader.text(request.request_id, 128u)
                && reader.text(request.request_sha256, 64u)
                && reader.text(request.binding_sha256, 64u)
                && reader.text(request.payload_sha256, 64u)
                && reader.u64(request.issued_at_unix_seconds)
                && (request.request_id.empty() || safe_identifier(request.request_id))
                && (request.request_sha256.empty()
                    || lowercase_hex(request.request_sha256))
                && (request.binding_sha256.empty()
                    || lowercase_hex(request.binding_sha256))
                && (request.payload_sha256.empty()
                    || lowercase_hex(request.payload_sha256));
        }

        void write_response(ByteWriter& writer, const ResponseReceipt& response)
        {
            writer.text(response.receipt_id);
            writer.text(response.request_id);
            writer.text(response.request_sha256);
            writer.text(response.binding_sha256);
            writer.text(response.payload_sha256);
            writer.text(response.response_sha256);
            writer.u64(response.received_at_unix_seconds);
        }

        [[nodiscard]] bool read_response(
            ByteReader& reader,
            ResponseReceipt& response)
        {
            return reader.text(response.receipt_id, 128u)
                && reader.text(response.request_id, 128u)
                && reader.text(response.request_sha256, 64u)
                && reader.text(response.binding_sha256, 64u)
                && reader.text(response.payload_sha256, 64u)
                && reader.text(response.response_sha256, 64u)
                && reader.u64(response.received_at_unix_seconds)
                && (response.receipt_id.empty()
                    || safe_identifier(response.receipt_id))
                && (response.request_id.empty()
                    || safe_identifier(response.request_id))
                && (response.request_sha256.empty()
                    || lowercase_hex(response.request_sha256))
                && (response.binding_sha256.empty()
                    || lowercase_hex(response.binding_sha256))
                && (response.payload_sha256.empty()
                    || lowercase_hex(response.payload_sha256))
                && (response.response_sha256.empty()
                    || lowercase_hex(response.response_sha256));
        }

        [[nodiscard]] std::string calculate_request_digest(
            const QueueSnapshot& snapshot,
            const WorkItem& item,
            const RequestReceipt& request)
        {
            ByteWriter writer{};
            writer.text("epoch-ai-mcp-request/v1");
            writer.text(snapshot.configuration.queue_id);
            writer.text(item.request.objective_id);
            writer.text(item.objective_sha256);
            writer.text(item.request.campaign.state_sha256);
            writer.text(request.request_id);
            writer.text(request.binding_sha256);
            writer.text(request.payload_sha256);
            writer.u64(request.issued_at_unix_seconds);
            return digest_bytes(writer.bytes());
        }

        [[nodiscard]] std::string calculate_response_digest(
            const ResponseReceipt& response)
        {
            ByteWriter writer{};
            writer.text("epoch-ai-mcp-response/v1");
            writer.text(response.receipt_id);
            writer.text(response.request_id);
            writer.text(response.request_sha256);
            writer.text(response.binding_sha256);
            writer.text(response.payload_sha256);
            writer.u64(response.received_at_unix_seconds);
            return digest_bytes(writer.bytes());
        }

        void write_string_list(
            ByteWriter& writer,
            const std::vector<std::string>& values)
        {
            writer.u32(static_cast<std::uint32_t>(values.size()));
            for (const auto& value : values)
                writer.text(value);
        }

        [[nodiscard]] bool read_string_list(
            ByteReader& reader,
            std::vector<std::string>& values,
            std::uint32_t maximum,
            bool identifiers)
        {
            std::uint32_t count{};
            if (!reader.u32(count) || count > maximum)
                return false;
            values.reserve(count);
            for (std::uint32_t index = 0u; index < count; ++index)
            {
                std::string value{};
                if (!reader.text(value, identifiers ? 128u : 64u)
                    || (identifiers ? !safe_identifier(value)
                                    : !lowercase_hex(value)))
                    return false;
                values.push_back(std::move(value));
            }
            return true;
        }

        [[nodiscard]] std::vector<std::uint8_t> canonical_state(
            const QueueSnapshot& snapshot)
        {
            ByteWriter writer{};
            writer.text(checkpoint_schema);
            write_configuration(writer, snapshot.configuration);
            writer.u64(snapshot.generation);
            writer.u64(snapshot.next_insertion_sequence);
            writer.text(snapshot.previous_state_sha256);
            writer.u32(static_cast<std::uint32_t>(snapshot.items.size()));
            for (const auto& item : snapshot.items)
            {
                write_objective(writer, item.request);
                writer.text(item.objective_sha256);
                writer.u8(static_cast<std::uint8_t>(item.phase));
                writer.u64(item.insertion_sequence);
                write_request(writer, item.model_request);
                write_response(writer, item.model_response);
            }
            write_string_list(writer, snapshot.seen_request_ids);
            write_string_list(writer, snapshot.seen_request_sha256);
            write_string_list(writer, snapshot.seen_response_ids);
            write_string_list(writer, snapshot.seen_response_sha256);
            return writer.take();
        }

        [[nodiscard]] bool contains(
            const std::vector<std::string>& values,
            std::string_view value) noexcept
        {
            return std::ranges::find(values, value) != values.end();
        }

        [[nodiscard]] bool unique_values(
            const std::vector<std::string>& values)
        {
            for (std::size_t index = 0u; index < values.size(); ++index)
            {
                if (std::ranges::find(
                        values.begin() + static_cast<std::ptrdiff_t>(index + 1u),
                        values.end(), values[index]) != values.end())
                    return false;
            }
            return true;
        }

        [[nodiscard]] bool valid_snapshot(const QueueSnapshot& snapshot)
        {
            const auto& limits = snapshot.configuration.limits;
            if (!valid_configuration(snapshot.configuration)
                || snapshot.generation == 0u
                || snapshot.next_insertion_sequence == 0u
                || !lowercase_hex(snapshot.state_sha256)
                || (!snapshot.previous_state_sha256.empty()
                    && !lowercase_hex(snapshot.previous_state_sha256))
                || snapshot.items.size() > limits.maximum_items
                || snapshot.seen_request_ids.size() > limits.maximum_seen_requests
                || snapshot.seen_request_sha256.size() > limits.maximum_seen_requests
                || snapshot.seen_response_ids.size() > limits.maximum_seen_responses
                || snapshot.seen_response_sha256.size() > limits.maximum_seen_responses
                || snapshot.source_write_permitted || snapshot.promotion_permitted
                || snapshot.release_permitted || snapshot.server_permitted
                || snapshot.network_listener_permitted
                || !unique_values(snapshot.seen_request_ids)
                || !unique_values(snapshot.seen_request_sha256)
                || !unique_values(snapshot.seen_response_ids)
                || !unique_values(snapshot.seen_response_sha256))
                return false;

            std::vector<std::string> objective_ids{};
            std::uint64_t largest_sequence{};
            for (const auto& item : snapshot.items)
            {
                if (!safe_identifier(item.request.objective_id)
                    || !safe_text(item.request.objective,
                        limits.maximum_objective_bytes)
                    || item.request.priority > 7u
                    || !valid_campaign(item.request.campaign,
                        limits.maximum_locator_bytes)
                    || item.objective_sha256 != objective_digest(item.request)
                    || item.insertion_sequence == 0u
                    || item.insertion_sequence >= snapshot.next_insertion_sequence
                    || static_cast<std::uint8_t>(item.phase)
                        > static_cast<std::uint8_t>(ItemPhase::cancelled)
                    || contains(objective_ids, item.request.objective_id))
                    return false;
                objective_ids.push_back(item.request.objective_id);
                largest_sequence = std::max(largest_sequence, item.insertion_sequence);

                const bool has_request = !item.model_request.request_id.empty();
                const bool has_response = !item.model_response.receipt_id.empty();
                if (has_request
                    && (item.model_request.request_sha256
                            != calculate_request_digest(snapshot, item,
                                item.model_request)
                        || !contains(snapshot.seen_request_ids,
                            item.model_request.request_id)
                        || !contains(snapshot.seen_request_sha256,
                            item.model_request.request_sha256)))
                    return false;
                if (has_response
                    && (item.model_response.response_sha256
                            != calculate_response_digest(item.model_response)
                        || item.model_response.request_id
                            != item.model_request.request_id
                        || item.model_response.request_sha256
                            != item.model_request.request_sha256
                        || item.model_response.binding_sha256
                            != item.model_request.binding_sha256
                        || !contains(snapshot.seen_response_ids,
                            item.model_response.receipt_id)
                        || !contains(snapshot.seen_response_sha256,
                            item.model_response.response_sha256)))
                    return false;
                if ((item.phase == ItemPhase::queued && (has_request || has_response))
                    || (item.phase == ItemPhase::awaiting_response
                        && (!has_request || has_response))
                    || ((item.phase == ItemPhase::awaiting_human_review
                            || item.phase == ItemPhase::approved_for_campaign)
                        && (!has_request || !has_response)))
                    return false;
            }
            return largest_sequence < snapshot.next_insertion_sequence;
        }

        [[nodiscard]] bool read_snapshot(
            ByteReader& reader,
            QueueSnapshot& snapshot)
        {
            std::string schema{};
            std::uint32_t item_count{};
            if (!reader.text(schema, 64u) || schema != checkpoint_schema
                || !read_configuration(reader, snapshot.configuration)
                || !reader.u64(snapshot.generation)
                || !reader.u64(snapshot.next_insertion_sequence)
                || !reader.text(snapshot.previous_state_sha256, 64u)
                || !reader.u32(item_count)
                || item_count > snapshot.configuration.limits.maximum_items)
                return false;

            snapshot.items.reserve(item_count);
            for (std::uint32_t index = 0u; index < item_count; ++index)
            {
                WorkItem item{};
                std::uint8_t phase{};
                if (!reader.text(item.request.objective_id, 128u)
                    || !reader.text(item.request.objective,
                        snapshot.configuration.limits.maximum_objective_bytes)
                    || !reader.u8(item.request.priority)
                    || !read_campaign(reader, item.request.campaign,
                        snapshot.configuration.limits.maximum_locator_bytes)
                    || !reader.text(item.objective_sha256, 64u)
                    || !reader.u8(phase)
                    || phase > static_cast<std::uint8_t>(ItemPhase::cancelled)
                    || !reader.u64(item.insertion_sequence)
                    || !read_request(reader, item.model_request)
                    || !read_response(reader, item.model_response))
                    return false;
                item.phase = static_cast<ItemPhase>(phase);
                snapshot.items.push_back(std::move(item));
            }

            if (!read_string_list(reader, snapshot.seen_request_ids,
                    snapshot.configuration.limits.maximum_seen_requests, true)
                || !read_string_list(reader, snapshot.seen_request_sha256,
                    snapshot.configuration.limits.maximum_seen_requests, false)
                || !read_string_list(reader, snapshot.seen_response_ids,
                    snapshot.configuration.limits.maximum_seen_responses, true)
                || !read_string_list(reader, snapshot.seen_response_sha256,
                    snapshot.configuration.limits.maximum_seen_responses, false))
                return false;
            snapshot.source_write_permitted = false;
            snapshot.promotion_permitted = false;
            snapshot.release_permitted = false;
            snapshot.server_permitted = false;
            snapshot.network_listener_permitted = false;
            // The checkpoint body appends exactly one length-prefixed hex state
            // digest after the canonical state. Reject any other trailing shape.
            return reader.remaining() == 4u + 64u;
        }

        [[nodiscard]] Result status_result(
            Status status,
            const QueueSnapshot& snapshot,
            std::string objective_id = {})
        {
            return Result{status, std::move(objective_id),
                snapshot.state_sha256, {}};
        }
    }

    std::string objective_digest(const ObjectiveRequest& request)
    {
        ByteWriter writer{};
        writer.text("epoch-ai-bounded-objective/v1");
        write_objective(writer, request);
        return digest_bytes(writer.bytes());
    }

    ResponseReceipt make_response_receipt(
        const RequestReceipt& request,
        std::string receipt_id,
        std::string payload_sha256,
        std::uint64_t received_at_unix_seconds)
    {
        ResponseReceipt response{};
        response.receipt_id = std::move(receipt_id);
        response.request_id = request.request_id;
        response.request_sha256 = request.request_sha256;
        response.binding_sha256 = request.binding_sha256;
        response.payload_sha256 = std::move(payload_sha256);
        response.received_at_unix_seconds = received_at_unix_seconds;
        response.response_sha256 = calculate_response_digest(response);
        return response;
    }

    Result Queue::begin(const QueueConfiguration& configuration)
    {
        if (!valid_configuration(configuration))
            return {Status::invalid_configuration};
        snapshot_ = {};
        snapshot_.configuration = configuration;
        snapshot_.generation = 0u;
        snapshot_.next_insertion_sequence = 1u;
        initialized_ = true;
        commit_mutation();
        return status_result(Status::ready, snapshot_);
    }

    Result Queue::restore(
        std::span<const std::byte> checkpoint_bytes,
        std::uint64_t now_unix_seconds)
    {
        if (checkpoint_bytes.empty()
            || checkpoint_bytes.size() > maximum_checkpoint_bytes)
            return {Status::invalid_checkpoint};
        const auto bytes = std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(checkpoint_bytes.data()),
            checkpoint_bytes.size()};
        ByteReader outer{bytes};
        std::string schema{};
        std::uint32_t version{};
        std::uint32_t body_size{};
        if (!outer.text(schema, 64u) || schema != checkpoint_schema
            || !outer.u32(version) || version != checkpoint_version
            || !outer.u32(body_size) || body_size > maximum_checkpoint_bytes)
            return {Status::invalid_checkpoint};
        std::span<const std::uint8_t> body{};
        std::string body_sha256{};
        if (!outer.raw(body, body_size)
            || !outer.text(body_sha256, 64u)
            || outer.remaining() != 0u
            || !lowercase_hex(body_sha256)
            || digest_bytes(body) != body_sha256)
            return {Status::invalid_checkpoint};

        ByteReader body_reader{body};
        QueueSnapshot restored{};
        if (!read_snapshot(body_reader, restored)
            || !body_reader.text(restored.state_sha256, 64u)
            || body_reader.remaining() != 0u
            || !valid_snapshot(restored)
            || digest_bytes(canonical_state(restored)) != restored.state_sha256)
            return {Status::invalid_checkpoint};
        if (now_unix_seconds < restored.configuration.created_at_unix_seconds
            || now_unix_seconds > restored.configuration.expires_at_unix_seconds)
            return {Status::expired};
        snapshot_ = std::move(restored);
        initialized_ = true;
        return status_result(Status::ready, snapshot_);
    }

    Result Queue::validate_human(const HumanActionToken& authority) const
    {
        if (!initialized_ || !authority.operator_approved
            || authority.actor_sha256
                != snapshot_.configuration.human_authority_sha256)
            return status_result(Status::invalid_authority, snapshot_);
        if (authority.expected_generation != snapshot_.generation
            || authority.expected_state_sha256 != snapshot_.state_sha256)
            return status_result(Status::stale_state, snapshot_);
        if (authority.now_unix_seconds
                < snapshot_.configuration.created_at_unix_seconds
            || authority.now_unix_seconds
                > snapshot_.configuration.expires_at_unix_seconds)
            return status_result(Status::expired, snapshot_);
        return status_result(Status::ready, snapshot_);
    }

    Result Queue::validate_model(const ModelActionToken& authority) const
    {
        if (!initialized_ || !authority.model_call_permitted
            || authority.actor_sha256
                != snapshot_.configuration.human_authority_sha256)
            return status_result(Status::invalid_authority, snapshot_);
        if (authority.expected_generation != snapshot_.generation
            || authority.expected_state_sha256 != snapshot_.state_sha256)
            return status_result(Status::stale_state, snapshot_);
        if (authority.now_unix_seconds
                < snapshot_.configuration.created_at_unix_seconds
            || authority.now_unix_seconds
                > snapshot_.configuration.expires_at_unix_seconds)
            return status_result(Status::expired, snapshot_);
        return status_result(Status::ready, snapshot_);
    }

    Result Queue::enqueue(
        const HumanActionToken& authority,
        const ObjectiveRequest& request)
    {
        if (const Result validation = validate_human(authority); !validation)
            return validation;
        if (!safe_identifier(request.objective_id)
            || !safe_text(request.objective,
                snapshot_.configuration.limits.maximum_objective_bytes)
            || request.priority > 7u
            || !valid_campaign(request.campaign,
                snapshot_.configuration.limits.maximum_locator_bytes))
            return status_result(Status::invalid_objective, snapshot_);
        if (find_item(request.objective_id))
            return status_result(Status::duplicate_objective, snapshot_,
                request.objective_id);
        if (snapshot_.items.size()
            >= snapshot_.configuration.limits.maximum_items)
            return status_result(Status::limit_reached, snapshot_);
        WorkItem item{};
        item.request = request;
        item.objective_sha256 = objective_digest(request);
        item.insertion_sequence = snapshot_.next_insertion_sequence++;
        snapshot_.items.push_back(std::move(item));
        commit_mutation();
        return status_result(Status::ready, snapshot_, request.objective_id);
    }

    Result Queue::issue_next(
        const ModelActionToken& authority,
        std::string request_id,
        std::string transport_binding_sha256,
        std::string payload_sha256)
    {
        if (const Result validation = validate_model(authority); !validation)
            return validation;
        if (!safe_identifier(request_id)
            || !lowercase_hex(transport_binding_sha256)
            || !lowercase_hex(payload_sha256))
            return status_result(Status::invalid_receipt, snapshot_);
        if (contains(snapshot_.seen_request_ids, request_id))
            return status_result(Status::replay_refused, snapshot_);
        if (snapshot_.seen_request_ids.size()
                >= snapshot_.configuration.limits.maximum_seen_requests
            || snapshot_.seen_request_sha256.size()
                >= snapshot_.configuration.limits.maximum_seen_requests)
            return status_result(Status::limit_reached, snapshot_);

        WorkItem* selected{};
        for (auto& item : snapshot_.items)
        {
            if (item.phase != ItemPhase::queued)
                continue;
            if (!selected || item.request.priority > selected->request.priority
                || (item.request.priority == selected->request.priority
                    && item.insertion_sequence < selected->insertion_sequence))
                selected = &item;
        }
        if (!selected)
            return status_result(Status::no_work, snapshot_);

        RequestReceipt request{};
        request.request_id = std::move(request_id);
        request.binding_sha256 = std::move(transport_binding_sha256);
        request.payload_sha256 = std::move(payload_sha256);
        request.issued_at_unix_seconds = authority.now_unix_seconds;
        request.request_sha256 = calculate_request_digest(
            snapshot_, *selected, request);
        if (contains(snapshot_.seen_request_sha256, request.request_sha256))
            return status_result(Status::replay_refused, snapshot_);
        selected->model_request = request;
        selected->phase = ItemPhase::awaiting_response;
        snapshot_.seen_request_ids.push_back(request.request_id);
        snapshot_.seen_request_sha256.push_back(request.request_sha256);
        const std::string objective_id = selected->request.objective_id;
        commit_mutation();
        Result result = status_result(Status::ready, snapshot_, objective_id);
        result.request = std::move(request);
        return result;
    }

    Result Queue::record_response(const ResponseReceipt& response)
    {
        if (!initialized_ || !safe_identifier(response.receipt_id)
            || !safe_identifier(response.request_id)
            || !lowercase_hex(response.request_sha256)
            || !lowercase_hex(response.binding_sha256)
            || !lowercase_hex(response.payload_sha256)
            || !lowercase_hex(response.response_sha256))
            return status_result(Status::invalid_receipt, snapshot_);
        if (response.received_at_unix_seconds
                < snapshot_.configuration.created_at_unix_seconds
            || response.received_at_unix_seconds
                > snapshot_.configuration.expires_at_unix_seconds)
            return status_result(Status::expired, snapshot_);
        if (contains(snapshot_.seen_response_ids, response.receipt_id)
            || contains(snapshot_.seen_response_sha256, response.response_sha256))
            return status_result(Status::replay_refused, snapshot_);
        if (snapshot_.seen_response_ids.size()
                >= snapshot_.configuration.limits.maximum_seen_responses
            || snapshot_.seen_response_sha256.size()
                >= snapshot_.configuration.limits.maximum_seen_responses)
            return status_result(Status::limit_reached, snapshot_);

        WorkItem* selected{};
        for (auto& item : snapshot_.items)
        {
            if (item.phase == ItemPhase::awaiting_response
                && item.model_request.request_id == response.request_id)
            {
                selected = &item;
                break;
            }
        }
        if (!selected)
            return status_result(Status::invalid_transition, snapshot_);
        if (response.request_sha256 != selected->model_request.request_sha256
            || response.binding_sha256 != selected->model_request.binding_sha256
            || response.received_at_unix_seconds
                < selected->model_request.issued_at_unix_seconds
            || response.response_sha256 != calculate_response_digest(response))
            return status_result(Status::invalid_receipt, snapshot_);
        selected->model_response = response;
        selected->phase = ItemPhase::awaiting_human_review;
        snapshot_.seen_response_ids.push_back(response.receipt_id);
        snapshot_.seen_response_sha256.push_back(response.response_sha256);
        const std::string objective_id = selected->request.objective_id;
        commit_mutation();
        return status_result(Status::ready, snapshot_, objective_id);
    }

    Result Queue::review_response(
        const HumanActionToken& authority,
        std::string_view objective_id,
        HumanDisposition disposition)
    {
        if (const Result validation = validate_human(authority); !validation)
            return validation;
        WorkItem* item = find_item(objective_id);
        if (!item || item->phase != ItemPhase::awaiting_human_review)
            return status_result(Status::invalid_transition, snapshot_,
                std::string{objective_id});
        switch (disposition)
        {
        case HumanDisposition::approve_for_campaign:
            item->phase = ItemPhase::approved_for_campaign;
            break;
        case HumanDisposition::reject:
            item->phase = ItemPhase::rejected;
            break;
        case HumanDisposition::cancel:
            item->phase = ItemPhase::cancelled;
            break;
        }
        commit_mutation();
        return status_result(Status::ready, snapshot_, std::string{objective_id});
    }

    Result Queue::advance_campaign_checkpoint(
        const HumanActionToken& authority,
        std::string_view objective_id,
        const CampaignReference& campaign,
        bool completed)
    {
        if (const Result validation = validate_human(authority); !validation)
            return validation;
        WorkItem* item = find_item(objective_id);
        if (!item || item->phase != ItemPhase::approved_for_campaign)
            return status_result(Status::invalid_transition, snapshot_,
                std::string{objective_id});
        const CampaignReference& old = item->request.campaign;
        if (!valid_campaign(campaign,
                snapshot_.configuration.limits.maximum_locator_bytes)
            || campaign.campaign_id != old.campaign_id
            || campaign.target_key != old.target_key
            || campaign.record_generation <= old.record_generation
            || campaign.state_sha256 == old.state_sha256)
            return status_result(Status::invalid_checkpoint, snapshot_,
                std::string{objective_id});
        item->request.campaign = campaign;
        item->objective_sha256 = objective_digest(item->request);
        item->phase = completed ? ItemPhase::completed : ItemPhase::queued;
        item->model_request = {};
        item->model_response = {};
        commit_mutation();
        return status_result(Status::ready, snapshot_, std::string{objective_id});
    }

    Result Queue::cancel(
        const HumanActionToken& authority,
        std::string_view objective_id)
    {
        if (const Result validation = validate_human(authority); !validation)
            return validation;
        WorkItem* item = find_item(objective_id);
        if (!item || terminal(item->phase))
            return status_result(Status::invalid_transition, snapshot_,
                std::string{objective_id});
        item->phase = ItemPhase::cancelled;
        commit_mutation();
        return status_result(Status::ready, snapshot_, std::string{objective_id});
    }

    Checkpoint Queue::checkpoint() const
    {
        if (!initialized_ || !valid_snapshot(snapshot_)
            || digest_bytes(canonical_state(snapshot_)) != snapshot_.state_sha256)
            return {};
        ByteWriter body{};
        body.raw(canonical_state(snapshot_));
        body.text(snapshot_.state_sha256);
        const auto body_bytes = body.take();
        ByteWriter outer{};
        outer.text(checkpoint_schema);
        outer.u32(checkpoint_version);
        outer.u32(static_cast<std::uint32_t>(body_bytes.size()));
        outer.raw(body_bytes);
        outer.text(digest_bytes(body_bytes));
        const auto encoded = outer.take();
        Checkpoint result{};
        result.sha256 = digest_bytes(encoded);
        result.bytes.reserve(encoded.size());
        for (const auto byte : encoded)
            result.bytes.push_back(static_cast<std::byte>(byte));
        return result;
    }

    const QueueSnapshot& Queue::snapshot() const noexcept
    {
        return snapshot_;
    }

    WorkItem* Queue::find_item(std::string_view objective_id)
    {
        const auto found = std::ranges::find_if(snapshot_.items,
            [objective_id](const WorkItem& item)
            {
                return item.request.objective_id == objective_id;
            });
        return found == snapshot_.items.end() ? nullptr : &*found;
    }

    const WorkItem* Queue::find_item(std::string_view objective_id) const
    {
        const auto found = std::ranges::find_if(snapshot_.items,
            [objective_id](const WorkItem& item)
            {
                return item.request.objective_id == objective_id;
            });
        return found == snapshot_.items.end() ? nullptr : &*found;
    }

    void Queue::commit_mutation()
    {
        snapshot_.previous_state_sha256 = snapshot_.state_sha256;
        ++snapshot_.generation;
        snapshot_.state_sha256 = digest_bytes(canonical_state(snapshot_));
    }
}
