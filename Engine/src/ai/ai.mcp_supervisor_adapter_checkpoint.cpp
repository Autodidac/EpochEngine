/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.mcp_supervisor_adapter;

import core.sha256;

namespace epochengine::ai::mcp_supervisor_adapter
{
    namespace
    {
        inline constexpr std::string_view checkpoint_domain{
            "epoch-ai-mcp-supervisor-adapter-checkpoint-v1"};

        [[nodiscard]] std::string sha(std::span<const std::uint8_t> bytes)
        {
            return core::sha256::hex(core::sha256::hash(bytes));
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

        class Writer final
        {
        public:
            void u8(std::uint8_t value) { bytes_.push_back(value); }
            void u32(std::uint32_t value)
            {
                for (unsigned shift = 0u; shift < 32u; shift += 8u)
                    bytes_.push_back(static_cast<std::uint8_t>(value >> shift));
            }
            void u64(std::uint64_t value)
            {
                for (unsigned shift = 0u; shift < 64u; shift += 8u)
                    bytes_.push_back(static_cast<std::uint8_t>(value >> shift));
            }
            void string(std::string_view value)
            {
                u32(static_cast<std::uint32_t>(value.size()));
                bytes_.insert(bytes_.end(), value.begin(), value.end());
            }
            [[nodiscard]] const std::vector<std::uint8_t>& bytes() const
            { return bytes_; }
            [[nodiscard]] std::vector<std::uint8_t> take()
            { return std::move(bytes_); }
        private:
            std::vector<std::uint8_t> bytes_{};
        };

        class Reader final
        {
        public:
            explicit Reader(std::span<const std::uint8_t> bytes) : bytes_{bytes} {}
            [[nodiscard]] bool u8(std::uint8_t& value)
            {
                if (at_ >= bytes_.size()) return false;
                value = bytes_[at_++]; return true;
            }
            [[nodiscard]] bool u32(std::uint32_t& value)
            {
                value = 0u;
                if (bytes_.size() - at_ < 4u) return false;
                for (unsigned shift = 0u; shift < 32u; shift += 8u)
                    value |= static_cast<std::uint32_t>(bytes_[at_++]) << shift;
                return true;
            }
            [[nodiscard]] bool u64(std::uint64_t& value)
            {
                value = 0u;
                if (bytes_.size() - at_ < 8u) return false;
                for (unsigned shift = 0u; shift < 64u; shift += 8u)
                    value |= static_cast<std::uint64_t>(bytes_[at_++]) << shift;
                return true;
            }
            [[nodiscard]] bool string(std::string& value, std::size_t maximum)
            {
                std::uint32_t count{};
                if (!u32(count) || count > maximum
                    || bytes_.size() - at_ < count) return false;
                value.assign(reinterpret_cast<const char*>(bytes_.data() + at_), count);
                at_ += count; return true;
            }
            [[nodiscard]] std::size_t consumed() const noexcept { return at_; }
        private:
            std::span<const std::uint8_t> bytes_{};
            std::size_t at_{};
        };

        void write_configuration(Writer& w, const Configuration& c)
        {
            w.string(c.adapter_id); w.string(c.control_id); w.string(c.session_id);
            w.string(c.actor_sha256); w.string(c.campaign_id); w.string(c.project_id);
            w.u64(c.curated_session_id); w.u64(c.created_at_unix_seconds);
            w.u64(c.expires_at_unix_seconds);
            w.u32(c.limits.maximum_request_bytes);
            w.u32(c.limits.maximum_response_bytes);
            w.u32(c.limits.maximum_seen_requests);
            w.u32(c.limits.maximum_summary_bytes);
            w.u32(c.limits.maximum_checkpoint_bytes);
        }

        [[nodiscard]] bool read_configuration(Reader& r, Configuration& c)
        {
            return r.string(c.adapter_id, 128u) && r.string(c.control_id, 128u)
                && r.string(c.session_id, 128u) && r.string(c.actor_sha256, 64u)
                && r.string(c.campaign_id, 128u) && r.string(c.project_id, 128u)
                && r.u64(c.curated_session_id)
                && r.u64(c.created_at_unix_seconds)
                && r.u64(c.expires_at_unix_seconds)
                && r.u32(c.limits.maximum_request_bytes)
                && r.u32(c.limits.maximum_response_bytes)
                && r.u32(c.limits.maximum_seen_requests)
                && r.u32(c.limits.maximum_summary_bytes)
                && r.u32(c.limits.maximum_checkpoint_bytes);
        }

        [[nodiscard]] bool unsafe(const Snapshot& s) noexcept
        {
            return s.source_bytes_exposed || s.filesystem_read_permitted
                || s.transport_permitted || s.model_launch_permitted
                || s.source_apply_permitted || s.promotion_permitted
                || s.release_permitted || s.server_permitted
                || s.network_listener_permitted;
        }

        [[nodiscard]] std::vector<std::uint8_t> encode(const Snapshot& s)
        {
            Writer w{};
            w.string(checkpoint_domain); w.u32(format_version);
            write_configuration(w, s.configuration);
            w.u64(s.generation); w.string(s.previous_state_sha256);
            w.string(s.state_sha256); w.string(s.last_receipt_sha256);
            w.u32(static_cast<std::uint32_t>(s.seen_request_ids.size()));
            for (const auto& value : s.seen_request_ids) w.string(value);
            w.u32(static_cast<std::uint32_t>(s.seen_request_sha256.size()));
            for (const auto& value : s.seen_request_sha256) w.string(value);
            w.u8(s.source_bytes_exposed); w.u8(s.filesystem_read_permitted);
            w.u8(s.transport_permitted); w.u8(s.model_launch_permitted);
            w.u8(s.source_apply_permitted); w.u8(s.promotion_permitted);
            w.u8(s.release_permitted); w.u8(s.server_permitted);
            w.u8(s.network_listener_permitted);
            w.string(sha(w.bytes()));
            return w.take();
        }

        [[nodiscard]] bool read_bool(Reader& r, bool& value)
        {
            std::uint8_t raw{};
            if (!r.u8(raw) || raw > 1u) return false;
            value = raw != 0u; return true;
        }

        struct Decoded final
        {
            Snapshot snapshot{};
            std::string seal{};
            std::size_t seal_offset{};
            std::size_t consumed{};
        };

        [[nodiscard]] bool decode(
            std::span<const std::uint8_t> bytes, Decoded& out)
        {
            Reader r{bytes};
            std::string domain{}; std::uint32_t version{}, count{};
            if (!r.string(domain, 128u) || domain != checkpoint_domain
                || !r.u32(version) || version != format_version
                || !read_configuration(r, out.snapshot.configuration)
                || !r.u64(out.snapshot.generation)
                || !r.string(out.snapshot.previous_state_sha256, 64u)
                || !r.string(out.snapshot.state_sha256, 64u)
                || !r.string(out.snapshot.last_receipt_sha256, 64u)
                || !r.u32(count)
                || count > out.snapshot.configuration.limits.maximum_seen_requests)
                return false;
            out.snapshot.seen_request_ids.reserve(count);
            for (std::uint32_t i = 0u; i < count; ++i)
            {
                std::string value{};
                if (!r.string(value, 128u)) return false;
                out.snapshot.seen_request_ids.push_back(std::move(value));
            }
            if (!r.u32(count)
                || count != out.snapshot.seen_request_ids.size()) return false;
            out.snapshot.seen_request_sha256.reserve(count);
            for (std::uint32_t i = 0u; i < count; ++i)
            {
                std::string value{};
                if (!r.string(value, 64u)) return false;
                out.snapshot.seen_request_sha256.push_back(std::move(value));
            }
            if (!read_bool(r, out.snapshot.source_bytes_exposed)
                || !read_bool(r, out.snapshot.filesystem_read_permitted)
                || !read_bool(r, out.snapshot.transport_permitted)
                || !read_bool(r, out.snapshot.model_launch_permitted)
                || !read_bool(r, out.snapshot.source_apply_permitted)
                || !read_bool(r, out.snapshot.promotion_permitted)
                || !read_bool(r, out.snapshot.release_permitted)
                || !read_bool(r, out.snapshot.server_permitted)
                || !read_bool(r, out.snapshot.network_listener_permitted))
                return false;
            out.seal_offset = r.consumed();
            if (!r.string(out.seal, 64u)) return false;
            out.consumed = r.consumed();
            return true;
        }
    }

    Checkpoint Adapter::checkpoint() const noexcept
    {
        if (!initialized_)
            return {Code::checkpoint_malformed, {}, {},
                "adapter is not initialized"};
        auto bytes = encode(snapshot_);
        if (bytes.size() > snapshot_.configuration.limits.maximum_checkpoint_bytes)
            return {Code::budget_exceeded, {}, {},
                "checkpoint exceeds the configured byte budget"};
        return {Code::ready, bytes, sha(bytes), "canonical checkpoint ready"};
    }

    RestoreResult Adapter::restore(
        std::span<const std::uint8_t> bytes,
        const RestoreExpectation& expected) noexcept
    {
        if (!expected.configuration.limits.valid()
            || bytes.size() > expected.configuration.limits.maximum_checkpoint_bytes)
            return {Code::checkpoint_malformed, {},
                "restore expectation or checkpoint budget is invalid"};
        if (sha(bytes) != expected.checkpoint_sha256)
            return {Code::checkpoint_integrity, {},
                "checkpoint digest does not match the expected digest"};
        Decoded decoded{};
        if (!decode(bytes, decoded))
            return {Code::checkpoint_malformed, {},
                "checkpoint is truncated or structurally malformed"};
        if (decoded.snapshot.configuration != expected.configuration)
            return {Code::checkpoint_cross_session, {},
                "checkpoint authority does not match this adapter session"};
        if (decoded.snapshot.generation != expected.current_generation)
            return {Code::checkpoint_stale, {},
                "checkpoint generation is stale"};
        if (unsafe(decoded.snapshot)
            || decoded.snapshot.seen_request_ids.size()
                != decoded.snapshot.seen_request_sha256.size()
            || decoded.snapshot.state_sha256 != state_digest(decoded.snapshot)
            || sha(bytes.first(decoded.seal_offset)) != decoded.seal)
            return {Code::checkpoint_integrity, {},
                "checkpoint state or authority seal is invalid"};
        const auto canonical = encode(decoded.snapshot);
        if (canonical.size() != bytes.size()
            || !std::equal(canonical.begin(), canonical.end(), bytes.begin()))
            return {Code::checkpoint_noncanonical, {},
                "checkpoint has trailing or noncanonical bytes"};
        snapshot_ = std::move(decoded.snapshot);
        initialized_ = true;
        return {Code::ready, snapshot_, "checkpoint restored"};
    }
}
