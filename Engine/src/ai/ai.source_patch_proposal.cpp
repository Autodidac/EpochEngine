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
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.source_patch_proposal;

import core.sha256;

namespace epochengine::ai::source_patch_proposal
{
    namespace
    {
        using Digest = core::sha256::Digest;

        constexpr std::uint64_t maximum_configuration_lifetime{7u * 24u * 60u * 60u};
        constexpr std::size_t absolute_text_limit{64u * 1024u * 1024u};

        class Writer final
        {
        public:
            void u8(std::uint8_t value)
            {
                bytes_.push_back(static_cast<std::byte>(value));
            }

            void u32(std::uint32_t value)
            {
                for (std::uint32_t shift = 0u; shift != 32u; shift += 8u)
                    u8(static_cast<std::uint8_t>(value >> shift));
            }

            void u64(std::uint64_t value)
            {
                for (std::uint32_t shift = 0u; shift != 64u; shift += 8u)
                    u8(static_cast<std::uint8_t>(value >> shift));
            }

            void boolean(bool value)
            {
                u8(value ? 1u : 0u);
            }

            void text(std::string_view value)
            {
                u32(static_cast<std::uint32_t>(value.size()));
                bytes_.insert(bytes_.end(),
                    reinterpret_cast<const std::byte*>(value.data()),
                    reinterpret_cast<const std::byte*>(value.data() + value.size()));
            }

            void raw(std::span<const std::byte> value)
            {
                bytes_.insert(bytes_.end(), value.begin(), value.end());
            }

            [[nodiscard]] std::vector<std::byte> finish() &&
            {
                return std::move(bytes_);
            }

        private:
            std::vector<std::byte> bytes_{};
        };

        class Reader final
        {
        public:
            explicit Reader(std::span<const std::byte> bytes) noexcept
                : bytes_(bytes)
            {
            }

            [[nodiscard]] bool u8(std::uint8_t& value) noexcept
            {
                if (remaining() < 1u) return false;
                value = std::to_integer<std::uint8_t>(bytes_[offset_++]);
                return true;
            }

            [[nodiscard]] bool u32(std::uint32_t& value) noexcept
            {
                if (remaining() < 4u) return false;
                value = 0u;
                for (std::uint32_t shift = 0u; shift != 32u; shift += 8u)
                    value |= static_cast<std::uint32_t>(
                        std::to_integer<std::uint8_t>(bytes_[offset_++])) << shift;
                return true;
            }

            [[nodiscard]] bool u64(std::uint64_t& value) noexcept
            {
                if (remaining() < 8u) return false;
                value = 0u;
                for (std::uint32_t shift = 0u; shift != 64u; shift += 8u)
                    value |= static_cast<std::uint64_t>(
                        std::to_integer<std::uint8_t>(bytes_[offset_++])) << shift;
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
                std::uint32_t size{};
                if (!u32(size) || size > maximum || remaining() < size) return false;
                value.assign(reinterpret_cast<const char*>(bytes_.data() + offset_), size);
                offset_ += size;
                return true;
            }

            [[nodiscard]] bool raw(std::size_t size, std::span<const std::byte>& value)
            {
                if (remaining() < size) return false;
                value = bytes_.subspan(offset_, size);
                offset_ += size;
                return true;
            }

            [[nodiscard]] std::size_t remaining() const noexcept
            {
                return bytes_.size() - offset_;
            }

            [[nodiscard]] bool done() const noexcept
            {
                return offset_ == bytes_.size();
            }

        private:
            std::span<const std::byte> bytes_{};
            std::size_t offset_{};
        };

        [[nodiscard]] std::string hash(std::span<const std::byte> bytes)
        {
            return core::sha256::hex(core::sha256::hash(
                std::span<const std::uint8_t>{
                    reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()}));
        }

        [[nodiscard]] bool hex64(std::string_view value) noexcept
        {
            if (value.size() != 64u) return false;
            return std::ranges::all_of(value, [](char ch)
            {
                return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
            });
        }

        [[nodiscard]] bool identifier(
            std::string_view value,
            std::size_t maximum) noexcept
        {
            return !value.empty() && value.size() <= maximum
                && std::ranges::all_of(value, [](char ch)
                {
                    return (ch >= 'a' && ch <= 'z')
                        || (ch >= 'A' && ch <= 'Z')
                        || (ch >= '0' && ch <= '9')
                        || ch == '-' || ch == '_' || ch == '.' || ch == ':';
                });
        }

        [[nodiscard]] bool utf8(std::string_view value) noexcept
        {
            const auto* data = reinterpret_cast<const std::uint8_t*>(value.data());
            std::size_t at = 0u;
            while (at < value.size())
            {
                const std::uint8_t first = data[at++];
                if (first == 0u) return false;
                if (first <= 0x7fu) continue;
                std::uint32_t codepoint{};
                std::size_t continuation{};
                std::uint32_t minimum{};
                if (first >= 0xc2u && first <= 0xdfu)
                {
                    codepoint = first & 0x1fu;
                    continuation = 1u;
                    minimum = 0x80u;
                }
                else if (first >= 0xe0u && first <= 0xefu)
                {
                    codepoint = first & 0x0fu;
                    continuation = 2u;
                    minimum = 0x800u;
                }
                else if (first >= 0xf0u && first <= 0xf4u)
                {
                    codepoint = first & 0x07u;
                    continuation = 3u;
                    minimum = 0x10000u;
                }
                else return false;
                if (continuation > value.size() - at) return false;
                for (std::size_t index = 0u; index < continuation; ++index)
                {
                    const std::uint8_t byte = data[at++];
                    if ((byte & 0xc0u) != 0x80u) return false;
                    codepoint = (codepoint << 6u) | (byte & 0x3fu);
                }
                if (codepoint < minimum || codepoint > 0x10ffffu
                    || (codepoint >= 0xd800u && codepoint <= 0xdfffu)) return false;
            }
            return true;
        }

        [[nodiscard]] bool canonical_text(std::string_view value) noexcept
        {
            return utf8(value) && value.find('\r') == value.npos
                && !value.starts_with("\xef\xbb\xbf");
        }

        [[nodiscard]] bool canonical_path(
            std::string_view path,
            std::size_t maximum) noexcept
        {
            if (path.empty() || path.size() > maximum || path.front() == '/'
                || path.back() == '/' || path.find('\\') != path.npos
                || path.find(':') != path.npos || path.find("//") != path.npos
                || !canonical_text(path)) return false;
            std::size_t begin = 0u;
            while (begin < path.size())
            {
                const std::size_t end = path.find('/', begin);
                const std::string_view segment = path.substr(begin,
                    end == path.npos ? path.size() - begin : end - begin);
                if (segment.empty() || segment == "." || segment == ".."
                    || segment.front() == '.'
                    || std::ranges::any_of(segment, [](char ch)
                    {
                        const auto byte = static_cast<unsigned char>(ch);
                        return byte < 0x20u || ch == '<' || ch == '>'
                            || ch == '"' || ch == '|' || ch == '?' || ch == '*';
                    })) return false;
                if (end == path.npos) break;
                begin = end + 1u;
            }
            return true;
        }

        [[nodiscard]] bool path_under(
            std::string_view path,
            std::string_view prefix) noexcept
        {
            return path == prefix || (path.size() > prefix.size()
                && path.starts_with(prefix) && path[prefix.size()] == '/');
        }

        [[nodiscard]] bool hard_protected(std::string_view path) noexcept
        {
            static constexpr std::array<std::string_view, 8u> prefixes{
                ".git", ".codex", "build", "x64", "cache", "logs",
                "Engine/Bin", "Engine/built"};
            return std::ranges::any_of(prefixes,
                [path](std::string_view prefix) { return path_under(path, prefix); });
        }

        [[nodiscard]] bool limits_valid(const Limits& limits) noexcept
        {
            return limits.maximum_operations > 0u
                && limits.maximum_operations <= 64u
                && limits.maximum_hunks_per_file > 0u
                && limits.maximum_hunks_per_file <= 1024u
                && limits.maximum_path_bytes > 0u
                && limits.maximum_path_bytes <= 4096u
                && limits.maximum_identifier_bytes > 0u
                && limits.maximum_identifier_bytes <= 4096u
                && limits.maximum_metadata_bytes > 0u
                && limits.maximum_metadata_bytes <= 64u * 1024u
                && limits.maximum_source_bytes > 0u
                && limits.maximum_source_bytes <= 64u * 1024u * 1024u
                && limits.maximum_replacement_bytes > 0u
                && limits.maximum_replacement_bytes <= 32u * 1024u * 1024u
                && limits.maximum_total_postimage_bytes >= limits.maximum_source_bytes
                && limits.maximum_total_postimage_bytes <= 256u * 1024u * 1024u
                && limits.maximum_seen_requests > 0u
                && limits.maximum_seen_requests <= 4096u
                && limits.maximum_checkpoint_bytes >= 4096u
                && limits.maximum_checkpoint_bytes <= 16u * 1024u * 1024u;
        }

        [[nodiscard]] bool configuration_valid(const Configuration& configuration)
        {
            if (!limits_valid(configuration.limits)
                || !identifier(configuration.engine_id,
                    configuration.limits.maximum_identifier_bytes)
                || !identifier(configuration.project_id,
                    configuration.limits.maximum_identifier_bytes)
                || !hex64(configuration.workspace_root_sha256)
                || !hex64(configuration.human_authority_sha256)
                || configuration.source_revision == 0u
                || configuration.created_at_unix_seconds == 0u
                || configuration.expires_at_unix_seconds
                    <= configuration.created_at_unix_seconds
                || configuration.expires_at_unix_seconds
                    - configuration.created_at_unix_seconds
                    > maximum_configuration_lifetime
                || !std::ranges::is_sorted(configuration.protected_path_prefixes)
                || std::ranges::adjacent_find(configuration.protected_path_prefixes)
                    != configuration.protected_path_prefixes.end()) return false;
            return std::ranges::all_of(configuration.protected_path_prefixes,
                [&](const std::string& path)
                {
                    return canonical_path(path, configuration.limits.maximum_path_bytes);
                });
        }

        void write_limits(Writer& writer, const Limits& limits)
        {
            writer.u32(limits.maximum_operations);
            writer.u32(limits.maximum_hunks_per_file);
            writer.u32(limits.maximum_path_bytes);
            writer.u32(limits.maximum_identifier_bytes);
            writer.u32(limits.maximum_metadata_bytes);
            writer.u64(limits.maximum_source_bytes);
            writer.u64(limits.maximum_replacement_bytes);
            writer.u64(limits.maximum_total_postimage_bytes);
            writer.u32(limits.maximum_seen_requests);
            writer.u32(limits.maximum_checkpoint_bytes);
        }

        [[nodiscard]] bool read_limits(Reader& reader, Limits& limits)
        {
            return reader.u32(limits.maximum_operations)
                && reader.u32(limits.maximum_hunks_per_file)
                && reader.u32(limits.maximum_path_bytes)
                && reader.u32(limits.maximum_identifier_bytes)
                && reader.u32(limits.maximum_metadata_bytes)
                && reader.u64(limits.maximum_source_bytes)
                && reader.u64(limits.maximum_replacement_bytes)
                && reader.u64(limits.maximum_total_postimage_bytes)
                && reader.u32(limits.maximum_seen_requests)
                && reader.u32(limits.maximum_checkpoint_bytes)
                && limits_valid(limits);
        }

        void write_configuration(Writer& writer, const Configuration& value)
        {
            writer.text(value.engine_id);
            writer.text(value.project_id);
            writer.text(value.workspace_root_sha256);
            writer.text(value.human_authority_sha256);
            writer.u64(value.source_revision);
            writer.u64(value.created_at_unix_seconds);
            writer.u64(value.expires_at_unix_seconds);
            write_limits(writer, value.limits);
            writer.u32(static_cast<std::uint32_t>(value.protected_path_prefixes.size()));
            for (const auto& path : value.protected_path_prefixes) writer.text(path);
        }

        [[nodiscard]] bool read_configuration(Reader& reader, Configuration& value)
        {
            if (!reader.text(value.engine_id, 4096u)
                || !reader.text(value.project_id, 4096u)
                || !reader.text(value.workspace_root_sha256, 64u)
                || !reader.text(value.human_authority_sha256, 64u)
                || !reader.u64(value.source_revision)
                || !reader.u64(value.created_at_unix_seconds)
                || !reader.u64(value.expires_at_unix_seconds)
                || !read_limits(reader, value.limits)) return false;
            std::uint32_t count{};
            if (!reader.u32(count) || count > 256u) return false;
            value.protected_path_prefixes.resize(count);
            for (auto& path : value.protected_path_prefixes)
                if (!reader.text(path, value.limits.maximum_path_bytes)) return false;
            return configuration_valid(value);
        }

        void write_authority(Writer& writer, const Authority& value)
        {
            writer.text(value.authority_id);
            writer.text(value.session_id);
            writer.text(value.actor_sha256);
            writer.text(value.project_id);
            writer.text(value.workspace_root_sha256);
            writer.text(value.review_receipt_sha256);
            writer.boolean(value.source_snapshot_reviewed);
            writer.boolean(value.proposal_permitted);
            writer.boolean(value.source_apply_permitted);
            writer.boolean(value.arbitrary_file_read_permitted);
            writer.boolean(value.compiler_invocation_permitted);
            writer.boolean(value.model_launch_permitted);
            writer.boolean(value.network_permitted);
            writer.boolean(value.server_permitted);
            writer.boolean(value.listener_permitted);
            writer.boolean(value.promotion_permitted);
            writer.boolean(value.release_permitted);
        }

        [[nodiscard]] bool read_authority(Reader& reader, Authority& value)
        {
            return reader.text(value.authority_id, 4096u)
                && reader.text(value.session_id, 4096u)
                && reader.text(value.actor_sha256, 64u)
                && reader.text(value.project_id, 4096u)
                && reader.text(value.workspace_root_sha256, 64u)
                && reader.text(value.review_receipt_sha256, 64u)
                && reader.loose_boolean(value.source_snapshot_reviewed)
                && reader.loose_boolean(value.proposal_permitted)
                && reader.loose_boolean(value.source_apply_permitted)
                && reader.loose_boolean(value.arbitrary_file_read_permitted)
                && reader.loose_boolean(value.compiler_invocation_permitted)
                && reader.loose_boolean(value.model_launch_permitted)
                && reader.loose_boolean(value.network_permitted)
                && reader.loose_boolean(value.server_permitted)
                && reader.loose_boolean(value.listener_permitted)
                && reader.loose_boolean(value.promotion_permitted)
                && reader.loose_boolean(value.release_permitted);
        }

        [[nodiscard]] std::vector<std::byte> request_body(const Request& request)
        {
            Writer writer{};
            writer.text(request_schema);
            writer.text(request.request_id);
            writer.text(request.title);
            writer.text(request.rationale);
            writer.u64(request.expected_engine_generation);
            writer.text(request.expected_engine_state_sha256);
            writer.u64(request.now_unix_seconds);
            write_authority(writer, request.authority);
            writer.u32(static_cast<std::uint32_t>(request.operations.size()));
            for (const auto& operation : request.operations)
            {
                writer.text(operation.operation_id);
                writer.u8(static_cast<std::uint8_t>(operation.kind));
                writer.text(operation.relative_path);
                writer.u64(operation.base_source_revision);
                writer.text(operation.base_content_sha256);
                writer.u32(static_cast<std::uint32_t>(operation.edits.size()));
                for (const auto& edit : operation.edits)
                {
                    writer.text(edit.edit_id);
                    writer.u64(edit.start_line);
                    writer.u64(edit.line_count);
                    writer.text(edit.expected_range_sha256);
                    writer.text(edit.replacement_utf8);
                }
            }
            return std::move(writer).finish();
        }

        struct DecodedRequest final
        {
            Code code{Code::malformed_request};
            Request request{};
            std::string request_sha256{};
        };

        [[nodiscard]] DecodedRequest decode_request(
            std::span<const std::byte> packet,
            const Limits& limits)
        {
            Reader envelope{packet};
            std::string envelope_schema{};
            std::uint64_t body_size{};
            std::span<const std::byte> body{};
            std::string claimed_sha{};
            if (!envelope.text(envelope_schema, 128u)
                || envelope_schema != request_schema
                || !envelope.u64(body_size)
                || body_size > packet.size()
                || !envelope.raw(static_cast<std::size_t>(body_size), body)
                || !envelope.text(claimed_sha, 64u) || !envelope.done()
                || !hex64(claimed_sha)) return {};
            const std::string actual_sha = hash(body);
            if (actual_sha != claimed_sha)
                return {.code = Code::request_tampered};

            Reader reader{body};
            Request request{};
            std::string body_schema{};
            if (!reader.text(body_schema, 128u) || body_schema != request_schema
                || !reader.text(request.request_id, limits.maximum_identifier_bytes)
                || !reader.text(request.title, limits.maximum_metadata_bytes)
                || !reader.text(request.rationale, limits.maximum_metadata_bytes)
                || !reader.u64(request.expected_engine_generation)
                || !reader.text(request.expected_engine_state_sha256, 64u)
                || !reader.u64(request.now_unix_seconds)
                || !read_authority(reader, request.authority)) return {};
            std::uint32_t operation_count{};
            if (!reader.u32(operation_count)
                || operation_count > limits.maximum_operations) return {};
            request.operations.resize(operation_count);
            std::uint64_t replacements{};
            for (auto& operation : request.operations)
            {
                std::uint8_t kind{};
                std::uint32_t edit_count{};
                if (!reader.text(operation.operation_id,
                        limits.maximum_identifier_bytes)
                    || !reader.u8(kind)
                    || kind > static_cast<std::uint8_t>(OperationKind::remove)
                    || !reader.text(operation.relative_path, limits.maximum_path_bytes)
                    || !reader.u64(operation.base_source_revision)
                    || !reader.text(operation.base_content_sha256, 64u)
                    || !reader.u32(edit_count)
                    || edit_count > limits.maximum_hunks_per_file) return {};
                operation.kind = static_cast<OperationKind>(kind);
                operation.edits.resize(edit_count);
                for (auto& edit : operation.edits)
                {
                    if (!reader.text(edit.edit_id, limits.maximum_identifier_bytes)
                        || !reader.u64(edit.start_line)
                        || !reader.u64(edit.line_count)
                        || !reader.text(edit.expected_range_sha256, 64u)
                        || !reader.text(edit.replacement_utf8,
                            static_cast<std::size_t>(limits.maximum_replacement_bytes)))
                        return {};
                    if (replacements > limits.maximum_replacement_bytes
                            - edit.replacement_utf8.size())
                        return {.code = Code::budget_exceeded};
                    replacements += edit.replacement_utf8.size();
                }
            }
            if (!reader.done()) return {};
            const std::vector<std::byte> canonical = encode_request(request);
            if (!std::ranges::equal(canonical, packet))
                return {.code = Code::noncanonical_request};
            return {.code = Code::ready,
                .request = std::move(request),
                .request_sha256 = actual_sha};
        }

        [[nodiscard]] std::vector<std::byte> envelope(
            std::string_view schema,
            std::span<const std::byte> body)
        {
            Writer writer{};
            writer.text(schema);
            writer.u64(static_cast<std::uint64_t>(body.size()));
            writer.raw(body);
            writer.text(hash(body));
            return std::move(writer).finish();
        }
    }

    std::string sha256_hex(std::string_view bytes)
    {
        return core::sha256::hex(core::sha256::hash(bytes));
    }

    std::vector<std::byte> encode_request(const Request& request)
    {
        const std::vector<std::byte> body = request_body(request);
        return envelope(request_schema, body);
    }

    namespace
    {
        struct LineSpan final
        {
            std::size_t begin{};
            std::size_t end{};
        };

        [[nodiscard]] std::vector<LineSpan> line_spans(std::string_view bytes)
        {
            std::vector<LineSpan> result{};
            std::size_t begin = 0u;
            while (begin < bytes.size())
            {
                const std::size_t newline = bytes.find('\n', begin);
                const std::size_t end = newline == bytes.npos
                    ? bytes.size() : newline + 1u;
                result.push_back({begin, end});
                begin = end;
            }
            return result;
        }

        [[nodiscard]] std::uint64_t replacement_line_count(
            std::string_view bytes) noexcept
        {
            if (bytes.empty()) return 0u;
            const std::uint64_t newlines = static_cast<std::uint64_t>(
                std::ranges::count(bytes, '\n'));
            return newlines + (bytes.back() == '\n' ? 0u : 1u);
        }

        [[nodiscard]] bool path_protected(
            const Configuration& configuration,
            std::string_view path) noexcept
        {
            return hard_protected(path)
                || std::ranges::any_of(configuration.protected_path_prefixes,
                    [path](const std::string& prefix)
                    {
                        return path_under(path, prefix);
                    });
        }

        [[nodiscard]] Result failure(
            Code code,
            const Snapshot& snapshot,
            std::string status)
        {
            return {.code = code, .snapshot = snapshot, .status = std::move(status)};
        }

        [[nodiscard]] bool authority_broadens(const Authority& authority) noexcept
        {
            return authority.source_apply_permitted
                || authority.arbitrary_file_read_permitted
                || authority.compiler_invocation_permitted
                || authority.model_launch_permitted
                || authority.network_permitted || authority.server_permitted
                || authority.listener_permitted || authority.promotion_permitted
                || authority.release_permitted;
        }

        [[nodiscard]] Code authority_code(
            const Authority& authority,
            const Configuration& configuration)
        {
            if (!identifier(authority.authority_id,
                    configuration.limits.maximum_identifier_bytes)
                || !identifier(authority.session_id,
                    configuration.limits.maximum_identifier_bytes)
                || !hex64(authority.actor_sha256)
                || !hex64(authority.workspace_root_sha256)
                || !hex64(authority.review_receipt_sha256)
                || authority.project_id != configuration.project_id
                || authority.workspace_root_sha256
                    != configuration.workspace_root_sha256
                || authority.actor_sha256
                    != configuration.human_authority_sha256)
                return Code::invalid_authority;
            if (!authority.source_snapshot_reviewed)
                return Code::unreviewed_authority;
            if (!authority.proposal_permitted)
                return Code::invalid_authority;
            if (authority_broadens(authority))
                return Code::authority_broadening;
            return Code::ready;
        }

        [[nodiscard]] const ReviewedSource* find_source(
            std::span<const ReviewedSource> sources,
            std::string_view path)
        {
            const auto found = std::ranges::find_if(sources,
                [path](const ReviewedSource& source)
                {
                    return source.relative_path == path;
                });
            return found == sources.end() ? nullptr : &*found;
        }

        void write_file_identity(
            Writer& writer,
            const FileProposal& file,
            bool after)
        {
            writer.text(file.relative_path);
            writer.u8(static_cast<std::uint8_t>(file.kind));
            writer.boolean(after ? file.exists_after : file.existed_before);
            writer.text(after ? file.after_sha256 : file.before_sha256);
            writer.u64(after ? file.after_byte_count : file.before_byte_count);
        }

        [[nodiscard]] std::string hunk_digest(
            const RangeEdit& edit,
            std::string_view removed)
        {
            Writer writer{};
            writer.text("epoch-ai-source-patch-hunk/v1");
            writer.text(edit.edit_id);
            writer.u64(edit.start_line);
            writer.u64(edit.line_count);
            writer.text(sha256_hex(removed));
            writer.text(sha256_hex(edit.replacement_utf8));
            writer.u64(static_cast<std::uint64_t>(removed.size()));
            writer.u64(static_cast<std::uint64_t>(edit.replacement_utf8.size()));
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] std::string file_digest(const FileProposal& file)
        {
            Writer writer{};
            writer.text("epoch-ai-source-patch-file/v1");
            writer.text(file.operation_id);
            writer.u8(static_cast<std::uint8_t>(file.kind));
            writer.text(file.relative_path);
            writer.u64(file.base_source_revision);
            writer.boolean(file.existed_before);
            writer.boolean(file.exists_after);
            writer.text(file.before_sha256);
            writer.text(file.after_sha256);
            writer.u64(file.before_byte_count);
            writer.u64(file.after_byte_count);
            writer.text(file.postimage_utf8);
            writer.u32(static_cast<std::uint32_t>(file.hunks.size()));
            for (const auto& hunk : file.hunks)
            {
                writer.text(hunk.edit_id);
                writer.u64(hunk.start_line);
                writer.u64(hunk.removed_line_count);
                writer.u64(hunk.added_line_count);
                writer.u64(hunk.removed_byte_count);
                writer.u64(hunk.added_byte_count);
                writer.text(hunk.before_sha256);
                writer.text(hunk.after_sha256);
                writer.text(hunk.hunk_sha256);
            }
            return hash(std::move(writer).finish());
        }

        struct FileBuild final
        {
            Code code{Code::invalid_edit};
            FileProposal file{};
            std::string status{};
        };

        [[nodiscard]] FileBuild reject_file(Code code, std::string status)
        {
            return {.code = code, .status = std::move(status)};
        }

        [[nodiscard]] FileBuild build_file(
            const FileOperation& operation,
            const ReviewedSource& source,
            const Configuration& configuration)
        {
            if (source.project_id != configuration.project_id
                || source.source_revision != configuration.source_revision
                || operation.base_source_revision != configuration.source_revision
                || operation.base_source_revision != source.source_revision
                || operation.base_content_sha256 != source.content_sha256)
                return reject_file(Code::stale_source,
                    "The reviewed source revision or preimage digest is stale.");
            if (!source.human_reviewed)
                return reject_file(Code::source_not_reviewed,
                    "The exact source snapshot was not human reviewed.");
            if (source.utf8_bytes.size() > configuration.limits.maximum_source_bytes)
                return reject_file(Code::budget_exceeded,
                    "The reviewed source exceeds the configured file budget.");
            if (!canonical_text(source.utf8_bytes))
                return reject_file(source.utf8_bytes.find('\r') != source.utf8_bytes.npos
                        || source.utf8_bytes.starts_with("\xef\xbb\xbf")
                    ? Code::noncanonical_text : Code::binary_rejected,
                    "The reviewed source is not canonical LF UTF-8 text.");
            const std::string actual_source_sha = sha256_hex(source.utf8_bytes);
            if (!hex64(source.content_sha256)
                || actual_source_sha != source.content_sha256)
                return reject_file(Code::stale_source,
                    "The reviewed source bytes do not match their digest.");

            if (operation.kind == OperationKind::create)
            {
                if (source.exists) return reject_file(Code::source_unexpected,
                    "Create requires a reviewed absent source entry.");
                if (!source.create_permitted)
                    return reject_file(Code::operation_not_permitted,
                        "Create was not permitted for the reviewed path.");
                if (!source.utf8_bytes.empty()
                    || source.content_sha256 != sha256_hex({}))
                    return reject_file(Code::stale_source,
                        "An absent source must carry the canonical empty digest.");
                if (operation.edits.size() != 1u
                    || operation.edits.front().start_line != 1u
                    || operation.edits.front().line_count != 0u)
                    return reject_file(Code::invalid_edit,
                        "Create requires one insertion at line one.");
            }
            else if (operation.kind == OperationKind::update)
            {
                if (!source.exists) return reject_file(Code::source_missing,
                    "Update requires a reviewed existing source entry.");
                if (!source.update_permitted)
                    return reject_file(Code::operation_not_permitted,
                        "Update was not permitted for the reviewed path.");
                if (operation.edits.empty())
                    return reject_file(Code::invalid_edit,
                        "Update requires at least one exact range edit.");
            }
            else
            {
                if (!source.exists) return reject_file(Code::source_missing,
                    "Delete requires a reviewed existing source entry.");
                if (!source.delete_permitted)
                    return reject_file(Code::operation_not_permitted,
                        "Delete was not permitted for the reviewed path.");
                if (!operation.edits.empty())
                    return reject_file(Code::invalid_edit,
                        "Delete cannot carry range edits.");
            }

            FileProposal file{};
            file.operation_id = operation.operation_id;
            file.kind = operation.kind;
            file.relative_path = operation.relative_path;
            file.base_source_revision = operation.base_source_revision;
            file.existed_before = source.exists;
            file.exists_after = operation.kind != OperationKind::remove;
            file.before_sha256 = source.content_sha256;
            file.before_byte_count = static_cast<std::uint64_t>(source.utf8_bytes.size());

            if (operation.kind == OperationKind::remove)
            {
                file.after_sha256 = sha256_hex({});
                file.file_receipt_sha256 = file_digest(file);
                return {.code = Code::ready, .file = std::move(file)};
            }

            const std::vector<LineSpan> lines = line_spans(source.utf8_bytes);
            std::size_t cursor = 0u;
            std::uint64_t previous_start{};
            std::uint64_t previous_end{};
            std::vector<std::string> edit_ids{};
            std::string postimage{};
            postimage.reserve(source.utf8_bytes.size());
            std::uint64_t replacement_total{};
            for (const RangeEdit& edit : operation.edits)
            {
                if (!identifier(edit.edit_id,
                        configuration.limits.maximum_identifier_bytes)
                    || std::ranges::find(edit_ids, edit.edit_id) != edit_ids.end()
                    || edit.start_line == 0u || !hex64(edit.expected_range_sha256))
                    return reject_file(Code::invalid_edit,
                        "Each edit requires one unique identifier and exact range digest.");
                edit_ids.push_back(edit.edit_id);
                if (!canonical_text(edit.replacement_utf8))
                    return reject_file(edit.replacement_utf8.find('\r')
                            != edit.replacement_utf8.npos
                            || edit.replacement_utf8.starts_with("\xef\xbb\xbf")
                        ? Code::noncanonical_text : Code::binary_rejected,
                        "Replacement bytes are not canonical LF UTF-8 text.");
                if (edit.replacement_utf8.size()
                        > configuration.limits.maximum_replacement_bytes
                    || replacement_total
                        > configuration.limits.maximum_replacement_bytes
                            - edit.replacement_utf8.size())
                    return reject_file(Code::budget_exceeded,
                        "Replacement bytes exceed the configured budget.");
                replacement_total += edit.replacement_utf8.size();

                if (previous_start != 0u)
                {
                    if (edit.start_line < previous_start)
                        return reject_file(Code::unsorted_hunks,
                            "Range edits must be sorted by starting line.");
                    if (edit.start_line == previous_start
                        || edit.start_line < previous_end)
                        return reject_file(Code::overlapping_hunks,
                            "Range edits overlap or share one insertion point.");
                }

                const std::uint64_t line_total = static_cast<std::uint64_t>(lines.size());
                if (edit.start_line > line_total + 1u
                    || (edit.line_count > 0u
                        && (edit.start_line > line_total
                            || edit.line_count > line_total - edit.start_line + 1u)))
                    return reject_file(Code::invalid_edit,
                        "A range edit falls outside the reviewed source lines.");
                const std::size_t begin = edit.start_line == line_total + 1u
                    ? source.utf8_bytes.size()
                    : lines[static_cast<std::size_t>(edit.start_line - 1u)].begin;
                const std::size_t end = edit.line_count == 0u ? begin
                    : lines[static_cast<std::size_t>(
                        edit.start_line + edit.line_count - 2u)].end;
                if (begin < cursor)
                    return reject_file(Code::overlapping_hunks,
                        "A range edit overlaps an earlier range.");
                const std::string_view removed = std::string_view{source.utf8_bytes}
                    .substr(begin, end - begin);
                if (sha256_hex(removed) != edit.expected_range_sha256)
                    return reject_file(Code::fuzzy_context_rejected,
                        "The exact reviewed range digest does not match.");
                postimage.append(source.utf8_bytes, cursor, begin - cursor);
                postimage.append(edit.replacement_utf8);
                cursor = end;

                HunkSummary summary{};
                summary.edit_id = edit.edit_id;
                summary.start_line = edit.start_line;
                summary.removed_line_count = edit.line_count;
                summary.added_line_count = replacement_line_count(
                    edit.replacement_utf8);
                summary.removed_byte_count = static_cast<std::uint64_t>(removed.size());
                summary.added_byte_count = static_cast<std::uint64_t>(
                    edit.replacement_utf8.size());
                summary.before_sha256 = sha256_hex(removed);
                summary.after_sha256 = sha256_hex(edit.replacement_utf8);
                summary.hunk_sha256 = hunk_digest(edit, removed);
                file.hunks.push_back(std::move(summary));
                previous_start = edit.start_line;
                previous_end = edit.start_line + edit.line_count;
            }
            postimage.append(source.utf8_bytes, cursor, source.utf8_bytes.size() - cursor);
            if (postimage.size() > configuration.limits.maximum_source_bytes)
                return reject_file(Code::budget_exceeded,
                    "The simulated postimage exceeds the configured file budget.");
            if (postimage == source.utf8_bytes)
                return reject_file(Code::no_change,
                    "The exact range edits produce no source change.");
            file.postimage_utf8 = std::move(postimage);
            file.after_byte_count = static_cast<std::uint64_t>(
                file.postimage_utf8.size());
            file.after_sha256 = sha256_hex(file.postimage_utf8);
            file.file_receipt_sha256 = file_digest(file);
            return {.code = Code::ready, .file = std::move(file)};
        }

        [[nodiscard]] AuthorityEvidence authority_evidence(const Authority& value)
        {
            return {.authority_id = value.authority_id,
                .actor_sha256 = value.actor_sha256,
                .review_receipt_sha256 = value.review_receipt_sha256,
                .workspace_root_sha256 = value.workspace_root_sha256,
                .project_id = value.project_id};
        }

        [[nodiscard]] std::string proposal_digest(const SealedProposal& proposal)
        {
            Writer writer{};
            writer.text(proposal.schema);
            writer.text(proposal.proposal_id);
            writer.text(proposal.request_sha256);
            writer.text(proposal.project_id);
            writer.u64(proposal.source_revision);
            writer.text(proposal.title);
            writer.text(proposal.rationale);
            writer.u32(static_cast<std::uint32_t>(proposal.files.size()));
            for (const auto& file : proposal.files)
            {
                writer.text(file.file_receipt_sha256);
                writer.text(file.postimage_utf8);
            }
            writer.text(proposal.authority.authority_id);
            writer.text(proposal.authority.actor_sha256);
            writer.text(proposal.authority.review_receipt_sha256);
            writer.text(proposal.authority.workspace_root_sha256);
            writer.text(proposal.authority.project_id);
            writer.boolean(proposal.authority.human_review_required);
            writer.boolean(proposal.authority.proposal_only);
            writer.boolean(proposal.authority.source_apply_permitted);
            writer.boolean(proposal.authority.arbitrary_file_read_permitted);
            writer.boolean(proposal.authority.compiler_invocation_permitted);
            writer.boolean(proposal.authority.model_launch_permitted);
            writer.boolean(proposal.authority.network_permitted);
            writer.boolean(proposal.authority.server_permitted);
            writer.boolean(proposal.authority.listener_permitted);
            writer.boolean(proposal.authority.promotion_permitted);
            writer.boolean(proposal.authority.release_permitted);
            writer.text(proposal.aggregate_before_sha256);
            writer.text(proposal.aggregate_after_sha256);
            writer.boolean(proposal.simulated_in_memory);
            writer.boolean(proposal.human_review_required);
            writer.boolean(proposal.applied);
            writer.boolean(proposal.compiled);
            writer.boolean(proposal.tested);
            writer.boolean(proposal.promoted);
            writer.boolean(proposal.released);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] std::string snapshot_digest(const Snapshot& snapshot)
        {
            Writer writer{};
            writer.text("epoch-ai-source-patch-state/v1");
            write_configuration(writer, snapshot.configuration);
            writer.u64(snapshot.generation);
            writer.text(snapshot.previous_state_sha256);
            writer.u32(static_cast<std::uint32_t>(snapshot.seen_request_ids.size()));
            for (std::size_t index = 0u; index < snapshot.seen_request_ids.size(); ++index)
            {
                writer.text(snapshot.seen_request_ids[index]);
                writer.text(snapshot.seen_request_sha256[index]);
            }
            writer.boolean(snapshot.source_apply_permitted);
            writer.boolean(snapshot.arbitrary_file_read_permitted);
            writer.boolean(snapshot.compiler_invocation_permitted);
            writer.boolean(snapshot.model_launch_permitted);
            writer.boolean(snapshot.network_permitted);
            writer.boolean(snapshot.server_permitted);
            writer.boolean(snapshot.listener_permitted);
            writer.boolean(snapshot.promotion_permitted);
            writer.boolean(snapshot.release_permitted);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] std::string receipt_digest(const Receipt& receipt)
        {
            Writer writer{};
            writer.text("epoch-ai-source-patch-receipt/v1");
            writer.text(receipt.receipt_id);
            writer.text(receipt.request_id);
            writer.text(receipt.request_sha256);
            writer.text(receipt.previous_receipt_sha256);
            writer.text(receipt.engine_state_before_sha256);
            writer.text(receipt.engine_state_after_sha256);
            writer.text(receipt.proposal_sha256);
            writer.u64(receipt.sealed_at_unix_seconds);
            writer.u64(receipt.resulting_generation);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] bool safe_snapshot(const Snapshot& snapshot) noexcept
        {
            return !snapshot.source_apply_permitted
                && !snapshot.arbitrary_file_read_permitted
                && !snapshot.compiler_invocation_permitted
                && !snapshot.model_launch_permitted && !snapshot.network_permitted
                && !snapshot.server_permitted && !snapshot.listener_permitted
                && !snapshot.promotion_permitted && !snapshot.release_permitted;
        }

        void write_snapshot(Writer& writer, const Snapshot& snapshot)
        {
            write_configuration(writer, snapshot.configuration);
            writer.u64(snapshot.generation);
            writer.text(snapshot.previous_state_sha256);
            writer.text(snapshot.state_sha256);
            writer.text(snapshot.last_receipt_sha256);
            writer.u32(static_cast<std::uint32_t>(snapshot.seen_request_ids.size()));
            for (std::size_t index = 0u; index < snapshot.seen_request_ids.size(); ++index)
            {
                writer.text(snapshot.seen_request_ids[index]);
                writer.text(snapshot.seen_request_sha256[index]);
            }
            writer.boolean(snapshot.source_apply_permitted);
            writer.boolean(snapshot.arbitrary_file_read_permitted);
            writer.boolean(snapshot.compiler_invocation_permitted);
            writer.boolean(snapshot.model_launch_permitted);
            writer.boolean(snapshot.network_permitted);
            writer.boolean(snapshot.server_permitted);
            writer.boolean(snapshot.listener_permitted);
            writer.boolean(snapshot.promotion_permitted);
            writer.boolean(snapshot.release_permitted);
        }

        [[nodiscard]] bool read_snapshot(Reader& reader, Snapshot& snapshot)
        {
            if (!read_configuration(reader, snapshot.configuration)
                || !reader.u64(snapshot.generation)
                || !reader.text(snapshot.previous_state_sha256, 64u)
                || !reader.text(snapshot.state_sha256, 64u)
                || !reader.text(snapshot.last_receipt_sha256, 64u)) return false;
            std::uint32_t count{};
            if (!reader.u32(count)
                || count > snapshot.configuration.limits.maximum_seen_requests)
                return false;
            snapshot.seen_request_ids.resize(count);
            snapshot.seen_request_sha256.resize(count);
            for (std::uint32_t index = 0u; index < count; ++index)
            {
                if (!reader.text(snapshot.seen_request_ids[index],
                        snapshot.configuration.limits.maximum_identifier_bytes)
                    || !reader.text(snapshot.seen_request_sha256[index], 64u))
                    return false;
            }
            return reader.loose_boolean(snapshot.source_apply_permitted)
                && reader.loose_boolean(snapshot.arbitrary_file_read_permitted)
                && reader.loose_boolean(snapshot.compiler_invocation_permitted)
                && reader.loose_boolean(snapshot.model_launch_permitted)
                && reader.loose_boolean(snapshot.network_permitted)
                && reader.loose_boolean(snapshot.server_permitted)
                && reader.loose_boolean(snapshot.listener_permitted)
                && reader.loose_boolean(snapshot.promotion_permitted)
                && reader.loose_boolean(snapshot.release_permitted);
        }
    }

    Result Engine::begin(const Configuration& configuration)
    {
        if (!configuration_valid(configuration))
            return failure(Code::invalid_configuration, {},
                "Source patch proposal configuration is invalid.");
        snapshot_ = {};
        snapshot_.configuration = configuration;
        snapshot_.generation = 1u;
        snapshot_.state_sha256 = snapshot_digest(snapshot_);
        begun_ = true;
        return {.code = Code::ready,
            .snapshot = snapshot_,
            .status = "No-I/O source patch proposal engine is ready."};
    }

    Result Engine::propose(
        std::span<const std::byte> canonical_request,
        std::span<const ReviewedSource> reviewed_sources)
    {
        if (!begun_ || !configuration_valid(snapshot_.configuration)
            || !safe_snapshot(snapshot_))
            return failure(Code::invalid_configuration, snapshot_,
                "Source patch proposal engine is not initialized safely.");
        const auto& limits = snapshot_.configuration.limits;
        const std::uint64_t maximum_packet = limits.maximum_replacement_bytes
            + static_cast<std::uint64_t>(limits.maximum_metadata_bytes)
                * (static_cast<std::uint64_t>(limits.maximum_operations) + 2u)
            + static_cast<std::uint64_t>(limits.maximum_operations)
                * static_cast<std::uint64_t>(limits.maximum_hunks_per_file) * 256u;
        if (canonical_request.empty() || canonical_request.size() > maximum_packet)
            return failure(Code::budget_exceeded, snapshot_,
                "Canonical proposal request exceeds its bounded packet budget.");

        DecodedRequest decoded = decode_request(canonical_request, limits);
        if (decoded.code != Code::ready)
            return failure(decoded.code, snapshot_,
                decoded.code == Code::request_tampered
                    ? "Canonical request body digest verification failed."
                    : decoded.code == Code::noncanonical_request
                        ? "Request encoding is not canonical."
                        : decoded.code == Code::budget_exceeded
                            ? "Request replacement budget was exceeded."
                            : "Source patch request is malformed.");
        Request& request = decoded.request;

        if (!identifier(request.request_id, limits.maximum_identifier_bytes)
            || request.title.empty() || request.title.size() > limits.maximum_metadata_bytes
            || request.rationale.empty()
            || request.rationale.size() > limits.maximum_metadata_bytes
            || !canonical_text(request.title) || !canonical_text(request.rationale))
            return failure(Code::malformed_request, snapshot_,
                "Request identity or review metadata is invalid.");
        if (std::ranges::find(snapshot_.seen_request_ids, request.request_id)
                != snapshot_.seen_request_ids.end()
            || std::ranges::find(snapshot_.seen_request_sha256,
                    decoded.request_sha256) != snapshot_.seen_request_sha256.end())
            return failure(Code::replay_refused, snapshot_,
                "Request identity or digest was already consumed.");
        if (request.expected_engine_generation != snapshot_.generation
            || request.expected_engine_state_sha256 != snapshot_.state_sha256)
            return failure(Code::stale_state, snapshot_,
                "Request does not bind the current engine generation and digest.");
        if (request.now_unix_seconds < snapshot_.configuration.created_at_unix_seconds
            || request.now_unix_seconds >= snapshot_.configuration.expires_at_unix_seconds)
            return failure(Code::stale_state, snapshot_,
                "Request falls outside the configured proposal lifetime.");
        const Code authority = authority_code(request.authority,
            snapshot_.configuration);
        if (authority != Code::ready)
            return failure(authority, snapshot_,
                authority == Code::authority_broadening
                    ? "Proposal request attempts to broaden no-I/O authority."
                    : authority == Code::unreviewed_authority
                        ? "Proposal authority does not attest reviewed source."
                        : "Proposal authority does not match the configured human boundary.");
        if (request.operations.empty()
            || request.operations.size() > limits.maximum_operations)
            return failure(Code::budget_exceeded, snapshot_,
                "Request must contain a bounded non-empty operation set.");
        if (snapshot_.seen_request_ids.size() >= limits.maximum_seen_requests)
            return failure(Code::budget_exceeded, snapshot_,
                "Replay history is full; checkpoint into a new bounded session.");

        for (std::size_t left = 0u; left < reviewed_sources.size(); ++left)
        {
            for (std::size_t right = left + 1u; right < reviewed_sources.size(); ++right)
            {
                if (reviewed_sources[left].relative_path
                    == reviewed_sources[right].relative_path)
                    return failure(Code::duplicate_operation, snapshot_,
                        "Reviewed source contains duplicate canonical paths.");
            }
        }

        SealedProposal proposal{};
        proposal.proposal_id = request.request_id;
        proposal.request_sha256 = decoded.request_sha256;
        proposal.project_id = snapshot_.configuration.project_id;
        proposal.source_revision = snapshot_.configuration.source_revision;
        proposal.title = request.title;
        proposal.rationale = request.rationale;
        proposal.authority = authority_evidence(request.authority);
        std::vector<std::string> operation_ids{};
        std::vector<std::string> paths{};
        std::uint64_t total_postimage_bytes{};
        for (const FileOperation& operation : request.operations)
        {
            if (!identifier(operation.operation_id, limits.maximum_identifier_bytes)
                || !hex64(operation.base_content_sha256))
                return failure(Code::malformed_request, snapshot_,
                    "File operation identity or base digest is invalid.");
            if (!canonical_path(operation.relative_path, limits.maximum_path_bytes))
                return failure(Code::invalid_path, snapshot_,
                    "File operation path is not canonical project-relative text.");
            if (path_protected(snapshot_.configuration, operation.relative_path))
                return failure(Code::protected_path, snapshot_,
                    "File operation targets a protected path.");
            if (std::ranges::find(operation_ids, operation.operation_id)
                    != operation_ids.end()
                || std::ranges::find(paths, operation.relative_path) != paths.end())
                return failure(Code::duplicate_operation, snapshot_,
                    "Operation identifiers and canonical paths must be unique.");
            operation_ids.push_back(operation.operation_id);
            paths.push_back(operation.relative_path);
            const ReviewedSource* source = find_source(
                reviewed_sources, operation.relative_path);
            if (source == nullptr)
                return failure(Code::source_missing, snapshot_,
                    "Operation path is absent from host-supplied reviewed source.");
            FileBuild built = build_file(operation, *source,
                snapshot_.configuration);
            if (built.code != Code::ready)
                return failure(built.code, snapshot_, std::move(built.status));
            if (total_postimage_bytes
                    > limits.maximum_total_postimage_bytes
                        - built.file.after_byte_count)
                return failure(Code::budget_exceeded, snapshot_,
                    "Aggregate simulated postimages exceed the configured budget.");
            total_postimage_bytes += built.file.after_byte_count;
            proposal.files.push_back(std::move(built.file));
        }

        Writer before{};
        Writer after{};
        before.text("epoch-ai-source-patch-aggregate-before/v1");
        after.text("epoch-ai-source-patch-aggregate-after/v1");
        for (const auto& file : proposal.files)
        {
            write_file_identity(before, file, false);
            write_file_identity(after, file, true);
        }
        proposal.aggregate_before_sha256 = hash(std::move(before).finish());
        proposal.aggregate_after_sha256 = hash(std::move(after).finish());
        proposal.canonical_proposal_sha256 = proposal_digest(proposal);

        const std::string state_before = snapshot_.state_sha256;
        const std::string previous_receipt = snapshot_.last_receipt_sha256;
        snapshot_.previous_state_sha256 = state_before;
        ++snapshot_.generation;
        snapshot_.seen_request_ids.push_back(request.request_id);
        snapshot_.seen_request_sha256.push_back(decoded.request_sha256);
        snapshot_.state_sha256 = snapshot_digest(snapshot_);

        Receipt receipt{};
        receipt.receipt_id = request.request_id + ":sealed:"
            + std::to_string(snapshot_.generation);
        receipt.request_id = request.request_id;
        receipt.request_sha256 = decoded.request_sha256;
        receipt.previous_receipt_sha256 = previous_receipt;
        receipt.engine_state_before_sha256 = state_before;
        receipt.engine_state_after_sha256 = snapshot_.state_sha256;
        receipt.proposal_sha256 = proposal.canonical_proposal_sha256;
        receipt.sealed_at_unix_seconds = request.now_unix_seconds;
        receipt.resulting_generation = snapshot_.generation;
        receipt.receipt_sha256 = receipt_digest(receipt);
        snapshot_.last_receipt_sha256 = receipt.receipt_sha256;
        proposal.receipt = receipt;

        return {.code = Code::sealed,
            .snapshot = snapshot_,
            .proposal = std::move(proposal),
            .status = "Exact source changes were simulated and sealed for human review; no I/O occurred."};
    }

    Checkpoint Engine::checkpoint() const
    {
        if (!begun_ || !configuration_valid(snapshot_.configuration)
            || !safe_snapshot(snapshot_)
            || snapshot_.state_sha256 != snapshot_digest(snapshot_)) return {};
        Writer body_writer{};
        body_writer.text(checkpoint_schema);
        write_snapshot(body_writer, snapshot_);
        const std::vector<std::byte> body = std::move(body_writer).finish();
        std::vector<std::byte> packet = envelope(checkpoint_schema, body);
        if (packet.size() > snapshot_.configuration.limits.maximum_checkpoint_bytes)
            return {};
        return {.bytes = std::move(packet), .sha256 = hash(body)};
    }

    Result Engine::restore(
        std::span<const std::byte> checkpoint_bytes,
        std::uint64_t now_unix_seconds)
    {
        Reader envelope_reader{checkpoint_bytes};
        std::string schema{};
        std::uint64_t body_size{};
        std::span<const std::byte> body{};
        std::string claimed_sha{};
        if (!envelope_reader.text(schema, 128u) || schema != checkpoint_schema
            || !envelope_reader.u64(body_size) || body_size > checkpoint_bytes.size()
            || !envelope_reader.raw(static_cast<std::size_t>(body_size), body)
            || !envelope_reader.text(claimed_sha, 64u) || !envelope_reader.done()
            || !hex64(claimed_sha) || claimed_sha != hash(body))
            return failure(Code::invalid_checkpoint, snapshot_,
                "Checkpoint envelope or digest is invalid.");
        Reader reader{body};
        Snapshot restored{};
        std::string body_schema{};
        if (!reader.text(body_schema, 128u) || body_schema != checkpoint_schema
            || !read_snapshot(reader, restored) || !reader.done()
            || checkpoint_bytes.size()
                > restored.configuration.limits.maximum_checkpoint_bytes
            || restored.generation == 0u || !safe_snapshot(restored)
            || restored.seen_request_ids.size()
                != restored.seen_request_sha256.size()
            || !std::ranges::all_of(restored.seen_request_sha256,
                [](const std::string& digest) { return hex64(digest); })
            || restored.state_sha256 != snapshot_digest(restored)
            || now_unix_seconds < restored.configuration.created_at_unix_seconds
            || now_unix_seconds >= restored.configuration.expires_at_unix_seconds)
            return failure(Code::invalid_checkpoint, snapshot_,
                "Checkpoint state, authority, or lifetime is invalid.");
        Writer canonical_body_writer{};
        canonical_body_writer.text(checkpoint_schema);
        write_snapshot(canonical_body_writer, restored);
        const std::vector<std::byte> canonical_body =
            std::move(canonical_body_writer).finish();
        const std::vector<std::byte> canonical_packet =
            envelope(checkpoint_schema, canonical_body);
        if (!std::ranges::equal(canonical_packet, checkpoint_bytes))
            return failure(Code::invalid_checkpoint, snapshot_,
                "Checkpoint encoding is not canonical.");
        snapshot_ = std::move(restored);
        begun_ = true;
        return {.code = Code::ready,
            .snapshot = snapshot_,
            .status = "Source patch proposal checkpoint restored exactly."};
    }

    const Snapshot& Engine::snapshot() const noexcept
    {
        return snapshot_;
    }
}
