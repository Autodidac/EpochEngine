/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.source_patch_stager;

import core.sha256;

namespace epochengine::ai::source_patch_stager
{
    namespace
    {
        constexpr std::uint64_t maximum_lifetime{7u * 24u * 60u * 60u};

        class Writer final
        {
        public:
            void u8(const std::uint8_t value)
            {
                bytes_.push_back(static_cast<std::byte>(value));
            }
            void u32(const std::uint32_t value)
            {
                for (std::uint32_t shift{}; shift != 32u; shift += 8u)
                    u8(static_cast<std::uint8_t>(value >> shift));
            }
            void u64(const std::uint64_t value)
            {
                for (std::uint32_t shift{}; shift != 64u; shift += 8u)
                    u8(static_cast<std::uint8_t>(value >> shift));
            }
            void boolean(const bool value) { u8(value ? 1u : 0u); }
            void text(const std::string_view value)
            {
                u32(static_cast<std::uint32_t>(value.size()));
                bytes_.insert(bytes_.end(),
                    reinterpret_cast<const std::byte*>(value.data()),
                    reinterpret_cast<const std::byte*>(value.data() + value.size()));
            }
            [[nodiscard]] std::vector<std::byte> finish() &&
            {
                return std::move(bytes_);
            }
        private:
            std::vector<std::byte> bytes_{};
        };

        [[nodiscard]] std::string hash(const std::span<const std::byte> bytes)
        {
            return core::sha256::hex(core::sha256::hash(
                std::span<const std::uint8_t>{
                    reinterpret_cast<const std::uint8_t*>(bytes.data()),
                    bytes.size()}));
        }
        [[nodiscard]] std::string hash(std::vector<std::byte> bytes)
        {
            return hash(std::span<const std::byte>{bytes});
        }
        [[nodiscard]] bool hex64(const std::string_view value) noexcept
        {
            return value.size() == 64u && std::ranges::all_of(value,
                [](const char byte)
                {
                    return (byte >= '0' && byte <= '9')
                        || (byte >= 'a' && byte <= 'f');
                });
        }
        [[nodiscard]] bool identifier(
            const std::string_view value, const std::size_t maximum) noexcept
        {
            return !value.empty() && value.size() <= maximum
                && std::ranges::all_of(value,
                    [](const unsigned char byte)
                    {
                        return std::isalnum(byte) != 0 || byte == '-'
                            || byte == '_' || byte == '.' || byte == ':';
                    });
        }
        [[nodiscard]] bool valid_utf8(const std::string_view bytes) noexcept
        {
            std::size_t index{};
            while (index < bytes.size())
            {
                const auto lead = static_cast<std::uint8_t>(bytes[index++]);
                if (lead == 0u) return false;
                if (lead < 0x80u) continue;
                std::uint32_t code{};
                std::size_t trailing{};
                if (lead >= 0xc2u && lead <= 0xdfu)
                {
                    code = lead & 0x1fu; trailing = 1u;
                }
                else if (lead >= 0xe0u && lead <= 0xefu)
                {
                    code = lead & 0x0fu; trailing = 2u;
                }
                else if (lead >= 0xf0u && lead <= 0xf4u)
                {
                    code = lead & 0x07u; trailing = 3u;
                }
                else return false;
                if (index + trailing > bytes.size()) return false;
                for (std::size_t offset{}; offset < trailing; ++offset)
                {
                    const auto next = static_cast<std::uint8_t>(bytes[index++]);
                    if ((next & 0xc0u) != 0x80u) return false;
                    code = (code << 6u) | (next & 0x3fu);
                }
                if ((trailing == 1u && code < 0x80u)
                    || (trailing == 2u && code < 0x800u)
                    || (trailing == 3u && code < 0x10000u)
                    || code > 0x10ffffu
                    || (code >= 0xd800u && code <= 0xdfffu)) return false;
            }
            return true;
        }
        [[nodiscard]] bool canonical_text(const std::string_view bytes) noexcept
        {
            return !bytes.starts_with("\xef\xbb\xbf")
                && bytes.find('\r') == bytes.npos && valid_utf8(bytes);
        }
        [[nodiscard]] bool canonical_relative_path(
            const std::string_view value, const std::size_t maximum)
        {
            if (value.empty() || value.size() > maximum
                || value.find('\\') != value.npos || value.starts_with('/'))
                return false;
            const std::filesystem::path path{value};
            if (path.is_absolute() || path.has_root_name()
                || path.has_root_directory() || path.lexically_normal() != path
                || path.generic_string() != value) return false;
            for (const auto& component : path)
            {
                const std::string part = component.generic_string();
                if (part.empty() || part == "." || part == "..") return false;
            }
            return true;
        }
        [[nodiscard]] std::string root_text(const std::filesystem::path& root)
        {
            return root.lexically_normal().generic_string();
        }
        [[nodiscard]] bool limits_valid(const Limits& value) noexcept
        {
            return value.maximum_operations > 0u
                && value.maximum_operations <= 256u
                && value.maximum_identifier_bytes >= 32u
                && value.maximum_identifier_bytes <= 1024u
                && value.maximum_path_bytes >= 32u
                && value.maximum_path_bytes <= 4096u
                && value.maximum_file_bytes >= 1024u
                && value.maximum_file_bytes <= 64u * 1024u * 1024u
                && value.maximum_total_postimage_bytes >= value.maximum_file_bytes
                && value.maximum_total_postimage_bytes <= 512u * 1024u * 1024u
                && value.maximum_consumed_requests >= 16u
                && value.maximum_consumed_requests <= 4096u;
        }
        [[nodiscard]] bool safe_configuration(const Configuration& value)
        {
            const auto root = value.sandbox_root.lexically_normal();
            return identifier(value.stager_id, value.limits.maximum_identifier_bytes)
                && identifier(value.project_id, value.limits.maximum_identifier_bytes)
                && identifier(value.session_id, value.limits.maximum_identifier_bytes)
                && value.curated_session_id != 0u
                && identifier(value.campaign_id, value.limits.maximum_identifier_bytes)
                && identifier(value.objective_id, value.limits.maximum_identifier_bytes)
                && identifier(value.operation_id, value.limits.maximum_identifier_bytes)
                && value.sandbox_root.is_absolute() && root == value.sandbox_root
                && !root_text(root).empty() && hex64(value.sandbox_root_sha256)
                && sha256_hex(root_text(root)) == value.sandbox_root_sha256
                && build_validation::valid_lower_hex(value.source_commit, 20u)
                && value.source_version.valid()
                && value.created_at_unix_seconds > 0u
                && value.expires_at_unix_seconds > value.created_at_unix_seconds
                && value.expires_at_unix_seconds - value.created_at_unix_seconds
                    <= maximum_lifetime
                && limits_valid(value.limits) && value.sandbox_staging_permitted
                && !value.live_source_write_permitted
                && !value.promotion_permitted && !value.commit_permitted
                && !value.release_permitted && !value.server_permitted
                && !value.network_listener_permitted;
        }

        void write_configuration(Writer& writer, const Configuration& value)
        {
            writer.text(value.stager_id); writer.text(value.project_id);
            writer.text(value.session_id); writer.u64(value.curated_session_id);
            writer.text(value.campaign_id); writer.text(value.objective_id);
            writer.text(value.operation_id); writer.text(root_text(value.sandbox_root));
            writer.text(value.sandbox_root_sha256); writer.text(value.source_commit);
            writer.u32(value.source_version.major); writer.u32(value.source_version.minor);
            writer.u32(value.source_version.revision);
            writer.boolean(value.build_policy.require_release_configuration);
            writer.boolean(value.build_policy.require_headless_ci);
            writer.boolean(value.build_policy.require_package_inventory);
            writer.boolean(value.build_policy.require_dependency_resolution);
            writer.boolean(value.build_policy.require_renderer_smoke);
            writer.boolean(value.build_policy.permit_explicit_renderer_skip);
            writer.u64(value.created_at_unix_seconds);
            writer.u64(value.expires_at_unix_seconds);
            writer.u32(value.limits.maximum_operations);
            writer.u32(value.limits.maximum_identifier_bytes);
            writer.u32(value.limits.maximum_path_bytes);
            writer.u64(value.limits.maximum_file_bytes);
            writer.u64(value.limits.maximum_total_postimage_bytes);
            writer.u32(value.limits.maximum_consumed_requests);
            writer.boolean(value.sandbox_staging_permitted);
            writer.boolean(value.live_source_write_permitted);
            writer.boolean(value.promotion_permitted); writer.boolean(value.commit_permitted);
            writer.boolean(value.release_permitted); writer.boolean(value.server_permitted);
            writer.boolean(value.network_listener_permitted);
        }

        void write_file_identity(
            Writer& writer,
            const source_patch_proposal::FileProposal& file,
            const bool after)
        {
            writer.text(file.relative_path);
            writer.u8(static_cast<std::uint8_t>(file.kind));
            writer.boolean(after ? file.exists_after : file.existed_before);
            writer.text(after ? file.after_sha256 : file.before_sha256);
            writer.u64(after ? file.after_byte_count : file.before_byte_count);
        }

        [[nodiscard]] std::string file_digest(
            const source_patch_proposal::FileProposal& file)
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

        [[nodiscard]] std::string proposal_digest(
            const source_patch_proposal::SealedProposal& proposal)
        {
            Writer writer{};
            writer.text(proposal.schema); writer.text(proposal.proposal_id);
            writer.text(proposal.request_sha256); writer.text(proposal.project_id);
            writer.u64(proposal.source_revision); writer.text(proposal.title);
            writer.text(proposal.rationale);
            writer.u32(static_cast<std::uint32_t>(proposal.files.size()));
            for (const auto& file : proposal.files)
            {
                writer.text(file.file_receipt_sha256);
                writer.text(file.postimage_utf8);
            }
            const auto& authority = proposal.authority;
            writer.text(authority.authority_id); writer.text(authority.actor_sha256);
            writer.text(authority.review_receipt_sha256);
            writer.text(authority.workspace_root_sha256);
            writer.text(authority.project_id);
            writer.boolean(authority.human_review_required);
            writer.boolean(authority.proposal_only);
            writer.boolean(authority.source_apply_permitted);
            writer.boolean(authority.arbitrary_file_read_permitted);
            writer.boolean(authority.compiler_invocation_permitted);
            writer.boolean(authority.model_launch_permitted);
            writer.boolean(authority.network_permitted);
            writer.boolean(authority.server_permitted);
            writer.boolean(authority.listener_permitted);
            writer.boolean(authority.promotion_permitted);
            writer.boolean(authority.release_permitted);
            writer.text(proposal.aggregate_before_sha256);
            writer.text(proposal.aggregate_after_sha256);
            writer.boolean(proposal.simulated_in_memory);
            writer.boolean(proposal.human_review_required);
            writer.boolean(proposal.applied); writer.boolean(proposal.compiled);
            writer.boolean(proposal.tested); writer.boolean(proposal.promoted);
            writer.boolean(proposal.released);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] std::string proposal_receipt_digest(
            const source_patch_proposal::Receipt& receipt)
        {
            Writer writer{};
            writer.text("epoch-ai-source-patch-receipt/v1");
            writer.text(receipt.receipt_id); writer.text(receipt.request_id);
            writer.text(receipt.request_sha256);
            writer.text(receipt.previous_receipt_sha256);
            writer.text(receipt.engine_state_before_sha256);
            writer.text(receipt.engine_state_after_sha256);
            writer.text(receipt.proposal_sha256);
            writer.u64(receipt.sealed_at_unix_seconds);
            writer.u64(receipt.resulting_generation);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] bool sealed_proposal_valid(
            const source_patch_proposal::SealedProposal& proposal,
            const Configuration& configuration)
        {
            if (proposal.schema != source_patch_proposal::proposal_schema
                || proposal.project_id != configuration.project_id
                || proposal.files.empty()
                || proposal.files.size() > configuration.limits.maximum_operations
                || !hex64(proposal.request_sha256)
                || !hex64(proposal.aggregate_before_sha256)
                || !hex64(proposal.aggregate_after_sha256)
                || !hex64(proposal.canonical_proposal_sha256)
                || !proposal.simulated_in_memory || !proposal.human_review_required
                || proposal.applied || proposal.compiled || proposal.tested
                || proposal.promoted || proposal.released)
                return false;

            const auto& authority = proposal.authority;
            if (authority.project_id != configuration.project_id
                || !authority.human_review_required || !authority.proposal_only
                || authority.source_apply_permitted
                || authority.arbitrary_file_read_permitted
                || authority.compiler_invocation_permitted
                || authority.model_launch_permitted || authority.network_permitted
                || authority.server_permitted || authority.listener_permitted
                || authority.promotion_permitted || authority.release_permitted
                || !hex64(authority.actor_sha256)
                || !hex64(authority.review_receipt_sha256)
                || !hex64(authority.workspace_root_sha256))
                return false;

            Writer before{}; Writer after{};
            before.text("epoch-ai-source-patch-aggregate-before/v1");
            after.text("epoch-ai-source-patch-aggregate-after/v1");
            std::uint64_t total{};
            std::string previous_path{};
            for (const auto& file : proposal.files)
            {
                if (!canonical_relative_path(file.relative_path,
                        configuration.limits.maximum_path_bytes)
                    || (!previous_path.empty() && file.relative_path <= previous_path)
                    || !hex64(file.before_sha256) || !hex64(file.after_sha256)
                    || file.before_byte_count > configuration.limits.maximum_file_bytes
                    || file.after_byte_count > configuration.limits.maximum_file_bytes
                    || file.after_byte_count != file.postimage_utf8.size()
                    || (!file.exists_after && !file.postimage_utf8.empty())
                    || (file.exists_after
                        && (!canonical_text(file.postimage_utf8)
                            || sha256_hex(file.postimage_utf8) != file.after_sha256))
                    || file.file_receipt_sha256 != file_digest(file))
                    return false;
                if (total > configuration.limits.maximum_total_postimage_bytes
                        - file.after_byte_count)
                    return false;
                total += file.after_byte_count;
                previous_path = file.relative_path;
                write_file_identity(before, file, false);
                write_file_identity(after, file, true);
            }
            if (hash(std::move(before).finish()) != proposal.aggregate_before_sha256
                || hash(std::move(after).finish()) != proposal.aggregate_after_sha256
                || proposal_digest(proposal) != proposal.canonical_proposal_sha256)
                return false;
            const auto& receipt = proposal.receipt;
            return receipt.request_sha256 == proposal.request_sha256
                && receipt.proposal_sha256 == proposal.canonical_proposal_sha256
                && receipt.receipt_sha256 == proposal_receipt_digest(receipt);
        }

        [[nodiscard]] std::string supervisor_receipt_digest(
            const iteration_supervisor_control::Receipt& receipt)
        {
            Writer writer{};
            writer.text("epoch-ai-supervisor-receipt/v1");
            writer.text(receipt.receipt_id); writer.text(receipt.command_id);
            writer.text(receipt.command_sha256);
            writer.text(receipt.previous_receipt_sha256);
            writer.text(receipt.control_state_before_sha256);
            writer.text(receipt.queue_state_before_sha256);
            writer.text(receipt.queue_state_after_sha256);
            writer.text(receipt.scheduler_state_before_sha256);
            writer.text(receipt.scheduler_state_after_sha256);
            writer.text(receipt.campaign_id); writer.text(receipt.objective_id);
            writer.text(receipt.operation_id);
            writer.u8(static_cast<std::uint8_t>(receipt.outcome));
            writer.u64(receipt.completed_at_unix_seconds);
            writer.text(receipt.transition_sha256);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] bool approval_valid(
            const PrepareRequest& request,
            const Configuration& configuration)
        {
            const auto& snapshot = request.supervisor;
            const auto& approval = request.approval;
            return request.operator_approved
                && approval.outcome == iteration_supervisor_control::Outcome::approved
                && approval.campaign_id == configuration.campaign_id
                && approval.objective_id == configuration.objective_id
                && approval.operation_id == configuration.operation_id
                && approval.receipt_sha256 == request.expected_approval_receipt_sha256
                && approval.receipt_sha256 == supervisor_receipt_digest(approval)
                && snapshot.configuration.session_id == configuration.session_id
                && snapshot.configuration.campaign_id == configuration.campaign_id
                && snapshot.generation == request.expected_supervisor_generation
                && snapshot.state_sha256 == request.expected_supervisor_state_sha256
                && snapshot.last_receipt_sha256 == approval.receipt_sha256
                && snapshot.active_objective_id == configuration.objective_id
                && snapshot.active_operation_id == configuration.operation_id
                && snapshot.phase == iteration_supervisor_control::ControlPhase::reviewed
                && !snapshot.source_write_permitted
                && !snapshot.arbitrary_file_read_permitted
                && !snapshot.model_launch_permitted
                && !snapshot.promotion_permitted && !snapshot.release_permitted
                && !snapshot.server_permitted
                && !snapshot.network_listener_permitted
                && !approval.source_write_permitted
                && !approval.promotion_permitted && !approval.release_permitted
                && !approval.server_permitted
                && !approval.network_listener_permitted;
        }

        [[nodiscard]] std::uint32_t line_count(
            const std::string_view bytes) noexcept
        {
            if (bytes.empty()) return 0u;
            return static_cast<std::uint32_t>(
                std::ranges::count(bytes, '\n')
                + (bytes.back() == '\n' ? 0u : 1u));
        }

        [[nodiscard]] const curated_context_bundle::ReviewedEntry* curated_entry(
            const curated_context_bundle::Bundle& bundle,
            const std::string_view path)
        {
            const auto found = std::ranges::find_if(bundle.entries,
                [path](const curated_context_bundle::ReviewedEntry& entry)
                {
                    return entry.project_relative_path == path;
                });
            return found == bundle.entries.end() ? nullptr : &*found;
        }

        [[nodiscard]] bool curated_valid(
            const PrepareRequest& request,
            const Configuration& configuration)
        {
            const auto checkpoint =
                curated_context_bundle::checkpoint(request.curated);
            if (!checkpoint
                || request.curated.bundle_sha256 != request.expected_bundle_sha256
                || !hex64(request.curated.bundle_sha256))
                return false;
            const auto& binding = request.curated.binding;
            if (binding.project_id != configuration.project_id
                || binding.session_id != configuration.curated_session_id
                || binding.campaign_id != configuration.campaign_id
                || binding.reviewed_revision != request.proposal.source_revision
                || !binding.operator_shared)
                return false;

            for (const auto& file : request.proposal.files)
            {
                if (!file.existed_before) continue;
                const auto* entry = curated_entry(request.curated, file.relative_path);
                if (entry == nullptr
                    || entry->source_revision != file.base_source_revision
                    || entry->first_line != 1u
                    || entry->last_line != line_count(entry->exact_bytes)
                    || entry->exact_bytes.size() != file.before_byte_count
                    || sha256_hex(entry->exact_bytes) != file.before_sha256)
                    return false;
            }
            return true;
        }

        [[nodiscard]] std::string destination_binding(
            const Configuration& configuration,
            const std::string_view relative_path)
        {
            Writer writer{};
            writer.text("epoch-ai-source-patch-destination/v1");
            writer.text(configuration.sandbox_root_sha256);
            writer.text(relative_path);
            writer.text(configuration.project_id);
            writer.text(configuration.session_id);
            writer.text(configuration.operation_id);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] const HostPreimage* preimage(
            const PrepareRequest& request,
            const std::string_view path)
        {
            const auto found = std::ranges::find_if(request.preimages,
                [path](const HostPreimage& item)
                {
                    return item.relative_path == path;
                });
            return found == request.preimages.end() ? nullptr : &*found;
        }

        [[nodiscard]] bool preimage_valid(
            const HostPreimage& value,
            const source_patch_proposal::FileProposal& file,
            const Configuration& configuration)
        {
            const auto expected_kind = file.existed_before
                ? PathKind::regular : PathKind::missing;
            if (value.relative_path != file.relative_path
                || value.kind != expected_kind
                || value.sandbox_root_sha256 != configuration.sandbox_root_sha256
                || value.destination_binding_sha256
                    != destination_binding(configuration, file.relative_path)
                || !value.beneath_sandbox || !value.path_components_link_free
                || !value.path_components_reparse_free)
                return false;
            if (!file.existed_before)
                return value.exact_bytes.empty()
                    && value.byte_count == 0u
                    && value.content_sha256 == sha256_hex(std::string_view{});
            return value.byte_count == file.before_byte_count
                && value.byte_count == value.exact_bytes.size()
                && value.content_sha256 == file.before_sha256
                && sha256_hex(value.exact_bytes) == value.content_sha256
                && canonical_text(value.exact_bytes);
        }

        [[nodiscard]] source_patch_bundle::Operation operation_kind(
            const source_patch_proposal::FileProposal& file) noexcept
        {
            if (!file.existed_before)
                return source_patch_bundle::Operation::add;
            if (!file.exists_after)
                return source_patch_bundle::Operation::remove;
            return source_patch_bundle::Operation::update;
        }

        [[nodiscard]] std::string operation_digest(
            const StagingOperation& operation)
        {
            Writer writer{};
            writer.text("epoch-ai-source-patch-stage-operation/v1");
            writer.text(operation.operation_id);
            writer.u8(static_cast<std::uint8_t>(operation.kind));
            writer.text(operation.relative_path);
            writer.u8(static_cast<std::uint8_t>(
                operation.expected_preimage_kind));
            writer.text(operation.before_sha256);
            writer.u64(operation.before_byte_count);
            writer.text(operation.after_sha256);
            writer.u64(operation.after_byte_count);
            writer.text(operation.postimage_utf8);
            writer.text(operation.destination_binding_sha256);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] std::string manifest_digest(
            const Configuration& configuration,
            const std::string_view proposal_sha256,
            const std::string_view bundle_sha256,
            const std::string_view approval_sha256,
            const std::span<const StagingOperation> operations)
        {
            Writer writer{};
            writer.text("epoch-ai-source-patch-staging-manifest/v1");
            writer.text(configuration.project_id);
            writer.text(configuration.session_id);
            writer.text(configuration.campaign_id);
            writer.text(configuration.objective_id);
            writer.text(configuration.operation_id);
            writer.text(configuration.sandbox_root_sha256);
            writer.text(proposal_sha256); writer.text(bundle_sha256);
            writer.text(approval_sha256);
            writer.u32(static_cast<std::uint32_t>(operations.size()));
            for (const auto& operation : operations)
                writer.text(operation.operation_sha256);
            writer.boolean(true);
            writer.boolean(false); writer.boolean(false);
            writer.boolean(false); writer.boolean(false);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] std::string plan_digest(const StagePlan& plan)
        {
            Writer writer{};
            writer.text(plan.schema); writer.text(plan.plan_id);
            writer.text(plan.project_id); writer.text(plan.session_id);
            writer.text(plan.campaign_id); writer.text(plan.objective_id);
            writer.text(plan.operation_id); writer.text(plan.sandbox_root_sha256);
            writer.text(plan.proposal_sha256); writer.text(plan.bundle_sha256);
            writer.text(plan.approval_receipt_sha256);
            writer.u64(plan.supervisor_generation);
            writer.text(plan.supervisor_state_sha256);
            writer.u32(static_cast<std::uint32_t>(plan.operations.size()));
            for (const auto& operation : plan.operations)
                writer.text(operation.operation_sha256);
            writer.text(plan.staging_manifest_sha256);
            writer.boolean(plan.host_execution_required);
            writer.boolean(plan.sandbox_only);
            writer.boolean(plan.live_source_write_permitted);
            writer.boolean(plan.promotion_permitted);
            writer.boolean(plan.commit_permitted);
            writer.boolean(plan.release_permitted);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] bool authority_broadening(
            const PrepareRequest& request) noexcept
        {
            return !request.sandbox_staging_requested
                || request.live_source_write_requested
                || request.arbitrary_file_read_requested
                || request.compiler_invocation_requested
                || request.model_launch_requested || request.promotion_requested
                || request.commit_requested || request.release_requested
                || request.server_requested || request.network_listener_requested;
        }

        [[nodiscard]] bool seen(
            const std::span<const std::string> values,
            const std::string_view value)
        {
            return std::ranges::find(values, value) != values.end();
        }

        [[nodiscard]] std::string request_digest(const PrepareRequest& request)
        {
            Writer writer{};
            writer.text("epoch-ai-source-patch-prepare-request/v1");
            writer.text(request.request_id); writer.u64(request.expected_generation);
            writer.text(request.expected_state_sha256);
            writer.u64(request.expected_supervisor_generation);
            writer.text(request.expected_supervisor_state_sha256);
            writer.text(request.expected_proposal_sha256);
            writer.text(request.expected_bundle_sha256);
            writer.text(request.expected_approval_receipt_sha256);
            writer.u64(request.now_unix_seconds);
            writer.u32(static_cast<std::uint32_t>(request.preimages.size()));
            for (const auto& item : request.preimages)
            {
                writer.text(item.relative_path);
                writer.u8(static_cast<std::uint8_t>(item.kind));
                writer.text(item.exact_bytes); writer.text(item.content_sha256);
                writer.u64(item.byte_count); writer.text(item.sandbox_root_sha256);
                writer.text(item.destination_binding_sha256);
                writer.boolean(item.beneath_sandbox);
                writer.boolean(item.path_components_link_free);
                writer.boolean(item.path_components_reparse_free);
            }
            writer.boolean(request.operator_approved);
            writer.boolean(request.sandbox_staging_requested);
            writer.boolean(request.live_source_write_requested);
            writer.boolean(request.arbitrary_file_read_requested);
            writer.boolean(request.compiler_invocation_requested);
            writer.boolean(request.model_launch_requested);
            writer.boolean(request.promotion_requested);
            writer.boolean(request.commit_requested);
            writer.boolean(request.release_requested);
            writer.boolean(request.server_requested);
            writer.boolean(request.network_listener_requested);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] std::string snapshot_digest(const Snapshot& snapshot)
        {
            Writer writer{};
            writer.text("epoch-ai-source-patch-stager-state/v1");
            write_configuration(writer, snapshot.configuration);
            writer.u8(static_cast<std::uint8_t>(snapshot.phase));
            writer.u64(snapshot.generation);
            writer.text(snapshot.previous_state_sha256);
            writer.text(snapshot.active_plan_sha256);
            writer.text(snapshot.active_manifest_sha256);
            writer.text(snapshot.active_staging_evidence_sha256);
            writer.u32(static_cast<std::uint32_t>(
                snapshot.consumed_request_ids.size()));
            for (std::size_t index{}; index < snapshot.consumed_request_ids.size();
                ++index)
            {
                writer.text(snapshot.consumed_request_ids[index]);
                writer.text(snapshot.consumed_request_sha256s[index]);
            }
            writer.boolean(snapshot.live_source_write_permitted);
            writer.boolean(snapshot.arbitrary_file_read_permitted);
            writer.boolean(snapshot.compiler_invocation_permitted);
            writer.boolean(snapshot.model_launch_permitted);
            writer.boolean(snapshot.promotion_permitted);
            writer.boolean(snapshot.commit_permitted);
            writer.boolean(snapshot.release_permitted);
            writer.boolean(snapshot.server_permitted);
            writer.boolean(snapshot.network_listener_permitted);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] std::string transition_digest(
            const TransitionReceipt& receipt)
        {
            Writer writer{};
            writer.text("epoch-ai-source-patch-stage-transition/v1");
            writer.text(receipt.receipt_id); writer.text(receipt.request_id);
            writer.text(receipt.request_sha256);
            writer.text(receipt.previous_receipt_sha256);
            writer.text(receipt.state_before_sha256);
            writer.text(receipt.state_after_sha256);
            writer.text(receipt.subject_sha256);
            writer.u64(receipt.resulting_generation);
            writer.u64(receipt.completed_at_unix_seconds);
            writer.boolean(receipt.live_source_write_permitted);
            writer.boolean(receipt.promotion_permitted);
            writer.boolean(receipt.commit_permitted);
            writer.boolean(receipt.release_permitted);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] TransitionReceipt make_transition(
            const Snapshot& snapshot,
            std::string request_id,
            std::string request_sha256,
            std::string previous_receipt,
            std::string state_before,
            std::string subject_sha256,
            const std::uint64_t completed_at)
        {
            TransitionReceipt receipt{};
            receipt.receipt_id = request_id + ":stage:"
                + std::to_string(snapshot.generation);
            receipt.request_id = std::move(request_id);
            receipt.request_sha256 = std::move(request_sha256);
            receipt.previous_receipt_sha256 = std::move(previous_receipt);
            receipt.state_before_sha256 = std::move(state_before);
            receipt.state_after_sha256 = snapshot.state_sha256;
            receipt.subject_sha256 = std::move(subject_sha256);
            receipt.resulting_generation = snapshot.generation;
            receipt.completed_at_unix_seconds = completed_at;
            receipt.receipt_sha256 = transition_digest(receipt);
            return receipt;
        }

        [[nodiscard]] bool transition_valid(
            const TransitionReceipt& receipt) noexcept
        {
            return !receipt.receipt_id.empty() && !receipt.request_id.empty()
                && hex64(receipt.request_sha256)
                && hex64(receipt.state_before_sha256)
                && hex64(receipt.state_after_sha256)
                && hex64(receipt.subject_sha256)
                && receipt.resulting_generation > 0u
                && receipt.completed_at_unix_seconds > 0u
                && !receipt.live_source_write_permitted
                && !receipt.promotion_permitted && !receipt.commit_permitted
                && !receipt.release_permitted
                && receipt.receipt_sha256 == transition_digest(receipt);
        }

        [[nodiscard]] bool plan_valid(const StagePlan& plan)
        {
            if (plan.schema != plan_schema || plan.operations.empty()
                || !plan.host_execution_required || !plan.sandbox_only
                || plan.live_source_write_permitted || plan.promotion_permitted
                || plan.commit_permitted || plan.release_permitted
                || !hex64(plan.staging_manifest_sha256)
                || !hex64(plan.plan_sha256)
                || plan.plan_sha256 != plan_digest(plan)
                || !transition_valid(plan.receipt)
                || plan.receipt.subject_sha256 != plan.plan_sha256)
                return false;
            for (const auto& operation : plan.operations)
            {
                if (operation.operation_sha256 != operation_digest(operation)
                    || !hex64(operation.destination_binding_sha256))
                    return false;
            }
            return true;
        }

        [[nodiscard]] std::string observation_digest(
            const StagedObservation& observation)
        {
            Writer writer{};
            writer.text("epoch-ai-source-patch-staged-observation/v1");
            writer.text(observation.operation_id);
            writer.text(observation.relative_path);
            writer.u8(static_cast<std::uint8_t>(observation.kind));
            writer.text(observation.exact_bytes);
            writer.text(observation.content_sha256);
            writer.u64(observation.byte_count);
            writer.text(observation.sandbox_root_sha256);
            writer.text(observation.destination_binding_sha256);
            writer.boolean(observation.beneath_sandbox);
            writer.boolean(observation.path_components_link_free);
            writer.boolean(observation.path_components_reparse_free);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] std::string staged_evidence_digest(
            const StagedEvidence& evidence)
        {
            Writer writer{};
            writer.text(evidence.schema); writer.text(evidence.report_id);
            writer.text(evidence.plan_sha256);
            writer.text(evidence.staging_manifest_sha256);
            writer.u32(static_cast<std::uint32_t>(
                evidence.observation_sha256s.size()));
            for (const auto& digest : evidence.observation_sha256s)
                writer.text(digest);
            writer.boolean(evidence.verified);
            writer.boolean(evidence.live_source_write_permitted);
            writer.boolean(evidence.promotion_permitted);
            writer.boolean(evidence.commit_permitted);
            writer.boolean(evidence.release_permitted);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] bool staged_evidence_valid(
            const StagedEvidence& evidence)
        {
            return evidence.schema == staged_evidence_schema
                && !evidence.report_id.empty() && hex64(evidence.plan_sha256)
                && hex64(evidence.staging_manifest_sha256)
                && !evidence.observation_sha256s.empty()
                && std::ranges::all_of(evidence.observation_sha256s,
                    [](const std::string& digest) { return hex64(digest); })
                && evidence.verified && !evidence.live_source_write_permitted
                && !evidence.promotion_permitted && !evidence.commit_permitted
                && !evidence.release_permitted
                && evidence.evidence_sha256 == staged_evidence_digest(evidence)
                && transition_valid(evidence.receipt)
                && evidence.receipt.subject_sha256 == evidence.evidence_sha256;
        }

        [[nodiscard]] std::string build_evidence_digest(
            const BuildEvidence& evidence)
        {
            Writer writer{};
            writer.text(evidence.schema); writer.text(evidence.report_id);
            writer.text(evidence.staging_evidence_sha256);
            writer.text(evidence.staging_manifest_sha256);
            writer.text(evidence.validation_receipt_sha256);
            writer.boolean(evidence.admitted);
            writer.boolean(evidence.upload_permitted);
            writer.boolean(evidence.promotion_permitted);
            writer.boolean(evidence.commit_permitted);
            writer.boolean(evidence.release_permitted);
            return hash(std::move(writer).finish());
        }

        [[nodiscard]] Result failure(
            const Code code,
            const Snapshot& snapshot,
            std::string status)
        {
            return {.code = code, .snapshot = snapshot,
                .status = std::move(status)};
        }
    }

    std::string sha256_hex(const std::string_view bytes)
    {
        return hash(std::as_bytes(std::span{bytes.data(), bytes.size()}));
    }

    Result Engine::begin(const Configuration& configuration)
    {
        if (begun_ || !safe_configuration(configuration))
            return failure(Code::invalid_configuration, snapshot_,
                "Source patch stager configuration is invalid or already active.");

        snapshot_ = {};
        snapshot_.configuration = configuration;
        snapshot_.phase = Phase::ready;
        snapshot_.generation = 1u;
        snapshot_.state_sha256 = snapshot_digest(snapshot_);
        begun_ = true;
        return {.code = Code::ready, .snapshot = snapshot_,
            .status = "Source patch stager is ready; no host operation was performed."};
    }

    Result Engine::prepare(const PrepareRequest& request)
    {
        if (!begun_)
            return failure(Code::invalid_configuration, snapshot_,
                "Source patch stager is not active.");
        const auto& configuration = snapshot_.configuration;
        const std::string request_sha256 = request_digest(request);
        if (seen(snapshot_.consumed_request_ids, request.request_id)
            || seen(snapshot_.consumed_request_sha256s, request_sha256))
            return failure(Code::replay_refused, snapshot_,
                "Prepare request replay was refused.");
        if (snapshot_.phase != Phase::ready
            || request.expected_generation != snapshot_.generation
            || request.expected_state_sha256 != snapshot_.state_sha256)
            return failure(Code::stale_state, snapshot_,
                "Prepare request does not bind the current stager generation.");
        if (!identifier(request.request_id,
                configuration.limits.maximum_identifier_bytes)
            || !hex64(request.expected_proposal_sha256)
            || !hex64(request.expected_bundle_sha256)
            || !hex64(request.expected_approval_receipt_sha256)
            || !hex64(request.expected_supervisor_state_sha256)
            || request.now_unix_seconds < configuration.created_at_unix_seconds
            || request.now_unix_seconds > configuration.expires_at_unix_seconds)
            return failure(Code::malformed_request, snapshot_,
                "Prepare request contains malformed or expired bindings.");

        if (authority_broadening(request))
            return failure(Code::authority_broadening, snapshot_,
                "Prepare request asks for authority beyond sandbox staging.");
        if (request.proposal.canonical_proposal_sha256
                != request.expected_proposal_sha256
            || !sealed_proposal_valid(request.proposal, configuration))
            return failure(Code::proposal_tampered, snapshot_,
                "Sealed source proposal failed exact digest verification.");
        if (request.curated.bundle_sha256 != request.expected_bundle_sha256)
            return failure(Code::bundle_tampered, snapshot_,
                "Curated source bundle digest does not match the request.");
        if (!curated_valid(request, configuration))
            return failure(Code::bundle_mismatch, snapshot_,
                "Curated source evidence does not cover every existing preimage.");
        if (!request.operator_approved)
            return failure(Code::approval_required, snapshot_,
                "Explicit human approval is required before sandbox staging.");
        if (!approval_valid(request, configuration))
            return failure(Code::invalid_authority, snapshot_,
                "Human approval receipt or supervisor binding is invalid.");
        if (request.preimages.size() < request.proposal.files.size())
            return failure(Code::partial_operations, snapshot_,
                "Host preimage report omits a proposed operation.");
        if (request.preimages.size() > request.proposal.files.size())
            return failure(Code::extra_operations, snapshot_,
                "Host preimage report contains an unrequested operation.");

        std::vector<StagingOperation> operations{};
        operations.reserve(request.proposal.files.size());
        for (const auto& file : request.proposal.files)
        {
            const auto* observed = preimage(request, file.relative_path);
            if (observed == nullptr)
                return failure(Code::partial_operations, snapshot_,
                    "Host preimage report omits a proposal path.");
            if (!canonical_relative_path(observed->relative_path,
                    configuration.limits.maximum_path_bytes)
                || !observed->beneath_sandbox)
                return failure(Code::path_escape, snapshot_,
                    "Host preimage path is not safely beneath the sandbox.");
            if (observed->kind == PathKind::symlink
                || observed->kind == PathKind::reparse_point
                || !observed->path_components_link_free
                || !observed->path_components_reparse_free)
                return failure(Code::link_ambiguity, snapshot_,
                    "Host preimage has a symlink or reparse ambiguity.");
            if (!preimage_valid(*observed, file, configuration))
                return failure(Code::preimage_mismatch, snapshot_,
                    "Host preimage bytes or destination binding are stale.");

            StagingOperation operation{};
            operation.operation_id = file.operation_id;
            operation.kind = operation_kind(file);
            operation.relative_path = file.relative_path;
            operation.expected_preimage_kind = observed->kind;
            operation.before_sha256 = file.before_sha256;
            operation.before_byte_count = file.before_byte_count;
            operation.after_sha256 = file.after_sha256;
            operation.after_byte_count = file.after_byte_count;
            operation.postimage_utf8 = file.postimage_utf8;
            operation.destination_binding_sha256 =
                observed->destination_binding_sha256;
            operation.operation_sha256 = operation_digest(operation);
            operations.push_back(std::move(operation));
        }
        for (std::size_t index{}; index < request.preimages.size(); ++index)
        {
            if (std::ranges::count(request.preimages,
                    request.preimages[index].relative_path,
                    &HostPreimage::relative_path) != 1)
                return failure(Code::extra_operations, snapshot_,
                    "Host preimage report contains duplicate paths.");
        }

        StagePlan plan{};
        plan.plan_id = request.request_id + ":plan";
        plan.project_id = configuration.project_id;
        plan.session_id = configuration.session_id;
        plan.campaign_id = configuration.campaign_id;
        plan.objective_id = configuration.objective_id;
        plan.operation_id = configuration.operation_id;
        plan.sandbox_root_sha256 = configuration.sandbox_root_sha256;
        plan.proposal_sha256 = request.expected_proposal_sha256;
        plan.bundle_sha256 = request.expected_bundle_sha256;
        plan.approval_receipt_sha256 =
            request.expected_approval_receipt_sha256;
        plan.supervisor_generation = request.expected_supervisor_generation;
        plan.supervisor_state_sha256 =
            request.expected_supervisor_state_sha256;
        plan.operations = std::move(operations);
        plan.staging_manifest_sha256 = manifest_digest(configuration,
            plan.proposal_sha256, plan.bundle_sha256,
            plan.approval_receipt_sha256, plan.operations);
        plan.plan_sha256 = plan_digest(plan);

        const std::string state_before = snapshot_.state_sha256;
        const std::string previous_receipt = snapshot_.last_receipt_sha256;
        snapshot_.previous_state_sha256 = state_before;
        ++snapshot_.generation;
        snapshot_.phase = Phase::prepared;
        snapshot_.active_plan_sha256 = plan.plan_sha256;
        snapshot_.active_manifest_sha256 = plan.staging_manifest_sha256;
        snapshot_.consumed_request_ids.push_back(request.request_id);
        snapshot_.consumed_request_sha256s.push_back(request_sha256);
        snapshot_.state_sha256 = snapshot_digest(snapshot_);
        plan.receipt = make_transition(snapshot_, request.request_id,
            request_sha256, previous_receipt, state_before,
            plan.plan_sha256, request.now_unix_seconds);
        snapshot_.last_receipt_sha256 = plan.receipt.receipt_sha256;

        return {.code = Code::prepared, .snapshot = snapshot_,
            .plan = std::move(plan),
            .status = "Sandbox staging plan is sealed for host execution; live source remains untouched."};
    }

    Result Engine::verify_staged(
        const StagePlan& plan,
        const StagedReport& report)
    {
        if (begun_ && seen(snapshot_.consumed_request_ids, report.report_id))
            return failure(Code::replay_refused, snapshot_,
                "Staged report replay was refused.");
        if (!begun_ || snapshot_.phase != Phase::prepared)
            return failure(Code::stale_state, snapshot_,
                "No prepared sandbox staging plan is active.");
        if (!plan_valid(plan)
            || plan.plan_sha256 != snapshot_.active_plan_sha256
            || plan.staging_manifest_sha256
                != snapshot_.active_manifest_sha256)
            return failure(Code::proposal_tampered, snapshot_,
                "Staging plan failed exact digest verification.");
        if (report.expected_generation != snapshot_.generation
            || report.expected_state_sha256 != snapshot_.state_sha256
            || report.plan_sha256 != plan.plan_sha256
            || report.staging_manifest_sha256
                != plan.staging_manifest_sha256)
            return failure(Code::stale_state, snapshot_,
                "Staged report does not bind the current plan generation.");
        if (!identifier(report.report_id,
                snapshot_.configuration.limits.maximum_identifier_bytes)
            || report.completed_at_unix_seconds
                < snapshot_.configuration.created_at_unix_seconds
            || report.completed_at_unix_seconds
                > snapshot_.configuration.expires_at_unix_seconds)
            return failure(Code::malformed_request, snapshot_,
                "Staged report contains malformed or expired fields.");
        if (!report.host_completed || !report.atomic_transaction
            || !report.rollback_available || report.live_source_touched
            || report.promotion_performed || report.commit_performed
            || report.release_performed)
            return failure(Code::authority_broadening, snapshot_,
                "Host report does not prove atomic sandbox-only staging.");
        if (report.observations.size() < plan.operations.size())
            return failure(Code::partial_operations, snapshot_,
                "Staged report omits a planned operation.");
        if (report.observations.size() > plan.operations.size())
            return failure(Code::extra_operations, snapshot_,
                "Staged report contains an unplanned operation.");

        std::vector<std::string> observation_sha256s{};
        observation_sha256s.reserve(report.observations.size());
        for (std::size_t index{}; index < plan.operations.size(); ++index)
        {
            const auto& operation = plan.operations[index];
            const auto& observation = report.observations[index];
            const bool removed =
                operation.kind == source_patch_bundle::Operation::remove;
            const PathKind expected_kind =
                removed ? PathKind::missing : PathKind::regular;
            if (observation.operation_id != operation.operation_id
                || observation.relative_path != operation.relative_path)
                return failure(Code::partial_operations, snapshot_,
                    "Staged observation ordering or identity is incomplete.");
            if (!canonical_relative_path(observation.relative_path,
                    snapshot_.configuration.limits.maximum_path_bytes)
                || !observation.beneath_sandbox)
                return failure(Code::path_escape, snapshot_,
                    "Staged observation is not safely beneath the sandbox.");
            if (observation.kind == PathKind::symlink
                || observation.kind == PathKind::reparse_point
                || !observation.path_components_link_free
                || !observation.path_components_reparse_free)
                return failure(Code::link_ambiguity, snapshot_,
                    "Staged observation contains link or reparse ambiguity.");
            if (observation.kind != expected_kind
                || observation.sandbox_root_sha256
                    != snapshot_.configuration.sandbox_root_sha256
                || observation.destination_binding_sha256
                    != operation.destination_binding_sha256
                || observation.byte_count != operation.after_byte_count
                || observation.byte_count != observation.exact_bytes.size()
                || observation.content_sha256 != operation.after_sha256
                || sha256_hex(observation.exact_bytes)
                    != observation.content_sha256
                || (!removed && (!canonical_text(observation.exact_bytes)
                    || observation.exact_bytes != operation.postimage_utf8))
                || (removed && !observation.exact_bytes.empty()))
                return failure(Code::wrong_postimage, snapshot_,
                    "Staged observation does not match the sealed postimage.");
            observation_sha256s.push_back(observation_digest(observation));
        }

        Writer report_writer{};
        report_writer.text("epoch-ai-source-patch-staged-report/v1");
        report_writer.text(report.report_id); report_writer.text(report.plan_sha256);
        report_writer.text(report.staging_manifest_sha256);
        report_writer.u32(static_cast<std::uint32_t>(
            observation_sha256s.size()));
        for (const auto& digest : observation_sha256s)
            report_writer.text(digest);
        report_writer.u64(report.completed_at_unix_seconds);
        report_writer.boolean(report.host_completed);
        report_writer.boolean(report.atomic_transaction);
        report_writer.boolean(report.rollback_available);
        report_writer.boolean(report.live_source_touched);
        report_writer.boolean(report.promotion_performed);
        report_writer.boolean(report.commit_performed);
        report_writer.boolean(report.release_performed);
        const std::string report_sha256 =
            hash(std::move(report_writer).finish());
        if (seen(snapshot_.consumed_request_ids, report.report_id)
            || seen(snapshot_.consumed_request_sha256s, report_sha256))
            return failure(Code::replay_refused, snapshot_,
                "Staged report replay was refused.");

        StagedEvidence evidence{};
        evidence.report_id = report.report_id;
        evidence.plan_sha256 = plan.plan_sha256;
        evidence.staging_manifest_sha256 =
            plan.staging_manifest_sha256;
        evidence.observation_sha256s = std::move(observation_sha256s);
        evidence.verified = true;
        evidence.evidence_sha256 = staged_evidence_digest(evidence);

        const std::string state_before = snapshot_.state_sha256;
        const std::string previous_receipt = snapshot_.last_receipt_sha256;
        snapshot_.previous_state_sha256 = state_before;
        ++snapshot_.generation;
        snapshot_.phase = Phase::staged_verified;
        snapshot_.active_staging_evidence_sha256 =
            evidence.evidence_sha256;
        snapshot_.consumed_request_ids.push_back(report.report_id);
        snapshot_.consumed_request_sha256s.push_back(report_sha256);
        snapshot_.state_sha256 = snapshot_digest(snapshot_);
        evidence.receipt = make_transition(snapshot_, report.report_id,
            report_sha256, previous_receipt, state_before,
            evidence.evidence_sha256, report.completed_at_unix_seconds);
        snapshot_.last_receipt_sha256 = evidence.receipt.receipt_sha256;

        return {.code = Code::staged_verified, .snapshot = snapshot_,
            .staged = std::move(evidence),
            .status = "Host-supplied sandbox postimages exactly match the sealed staging manifest."};
    }

    Result Engine::admit_build(
        const StagedEvidence& staged,
        const BuildReport& report)
    {
        if (begun_ && seen(snapshot_.consumed_request_ids, report.report_id))
            return failure(Code::replay_refused, snapshot_,
                "Build report replay was refused.");
        if (!begun_ || snapshot_.phase != Phase::staged_verified)
            return failure(Code::stale_state, snapshot_,
                "No verified staged evidence is awaiting build admission.");
        if (!staged_evidence_valid(staged)
            || staged.evidence_sha256
                != snapshot_.active_staging_evidence_sha256
            || staged.staging_manifest_sha256
                != snapshot_.active_manifest_sha256)
            return failure(Code::build_mismatch, snapshot_,
                "Staged evidence failed exact digest verification.");
        if (report.expected_generation != snapshot_.generation
            || report.expected_state_sha256 != snapshot_.state_sha256
            || report.staging_evidence_sha256 != staged.evidence_sha256
            || report.staging_manifest_sha256
                != staged.staging_manifest_sha256)
            return failure(Code::stale_state, snapshot_,
                "Build report does not bind the current staged generation.");
        if (!identifier(report.report_id,
                snapshot_.configuration.limits.maximum_identifier_bytes)
            || !hex64(report.receipt_sha256)
            || report.completed_at_unix_seconds
                < snapshot_.configuration.created_at_unix_seconds
            || report.completed_at_unix_seconds
                > snapshot_.configuration.expires_at_unix_seconds)
            return failure(Code::malformed_request, snapshot_,
                "Build report contains malformed or expired fields.");
        if (!report.local_host || !report.trusted_host || !report.host_completed)
            return failure(Code::untrusted_build, snapshot_,
                "Only a completed trusted local-host build can be admitted.");
        if (report.upload_performed || report.promotion_performed
            || report.commit_performed || report.release_performed)
            return failure(Code::authority_broadening, snapshot_,
                "Build report claims a forbidden upload, promotion, commit, or release.");

        const auto& validation = report.receipt;
        const std::string canonical =
            build_validation::canonical_json(validation);
        if (canonical.empty()
            || sha256_hex(canonical) != report.receipt_sha256
            || validation.source_commit
                != snapshot_.configuration.source_commit
            || validation.source_version
                != snapshot_.configuration.source_version
            || validation.source_tree_sha256
                != staged.staging_manifest_sha256)
            return failure(Code::build_mismatch, snapshot_,
                "Build validation receipt does not bind the staged source tree.");
        if (validation.checks.empty()
            || std::ranges::any_of(validation.checks,
                [](const build_validation::CheckEvidence& check)
                {
                    return check.status
                        != build_validation::CheckStatus::passed;
                }))
            return failure(Code::failed_build, snapshot_,
                "Build admission refuses failed, skipped, or invalid checks.");
        if (!build_validation::admit(validation,
                snapshot_.configuration.source_version,
                snapshot_.configuration.build_policy))
            return failure(Code::failed_build, snapshot_,
                "Build validation policy did not admit the local receipt.");

        Writer report_writer{};
        report_writer.text("epoch-ai-source-patch-build-report/v1");
        report_writer.text(report.report_id);
        report_writer.text(report.staging_evidence_sha256);
        report_writer.text(report.staging_manifest_sha256);
        report_writer.text(report.receipt_sha256);
        report_writer.u64(report.completed_at_unix_seconds);
        report_writer.boolean(report.local_host);
        report_writer.boolean(report.trusted_host);
        report_writer.boolean(report.host_completed);
        report_writer.boolean(report.upload_performed);
        report_writer.boolean(report.promotion_performed);
        report_writer.boolean(report.commit_performed);
        report_writer.boolean(report.release_performed);
        const std::string report_sha256 =
            hash(std::move(report_writer).finish());
        if (seen(snapshot_.consumed_request_ids, report.report_id)
            || seen(snapshot_.consumed_request_sha256s, report_sha256))
            return failure(Code::replay_refused, snapshot_,
                "Build report replay was refused.");

        BuildEvidence evidence{};
        evidence.report_id = report.report_id;
        evidence.staging_evidence_sha256 = staged.evidence_sha256;
        evidence.staging_manifest_sha256 =
            staged.staging_manifest_sha256;
        evidence.validation_receipt_sha256 = report.receipt_sha256;
        evidence.admitted = true;
        evidence.evidence_sha256 = build_evidence_digest(evidence);

        const std::string state_before = snapshot_.state_sha256;
        const std::string previous_receipt = snapshot_.last_receipt_sha256;
        snapshot_.previous_state_sha256 = state_before;
        ++snapshot_.generation;
        snapshot_.phase = Phase::build_admitted;
        snapshot_.consumed_request_ids.push_back(report.report_id);
        snapshot_.consumed_request_sha256s.push_back(report_sha256);
        snapshot_.state_sha256 = snapshot_digest(snapshot_);
        evidence.receipt = make_transition(snapshot_, report.report_id,
            report_sha256, previous_receipt, state_before,
            evidence.evidence_sha256, report.completed_at_unix_seconds);
        snapshot_.last_receipt_sha256 = evidence.receipt.receipt_sha256;

        return {.code = Code::build_admitted, .snapshot = snapshot_,
            .build = std::move(evidence),
            .status = "Trusted local build evidence is admitted for later human review; no promotion authority was granted."};
    }

    const Snapshot& Engine::snapshot() const noexcept
    {
        return snapshot_;
    }
}
