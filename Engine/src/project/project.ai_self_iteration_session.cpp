/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module project.ai_self_iteration_session;

import core.sha256;

namespace epochengine::project_ai_iteration_session
{
    namespace
    {
        [[nodiscard]] bool safe_id(std::string_view value) noexcept
        {
            return !value.empty() && value.size() <= 128u
                && std::all_of(value.begin(), value.end(), [](unsigned char c) {
                    return std::isalnum(c) || c == '-' || c == '_' || c == ':';
                });
        }

        [[nodiscard]] bool hex_digest(std::string_view value) noexcept
        {
            return value.size() == 64u
                && std::all_of(value.begin(), value.end(), [](unsigned char c) {
                    return std::isdigit(c) || (c >= 'a' && c <= 'f');
                });
        }

        [[nodiscard]] std::string digest(std::string_view value)
        {
            return core::sha256::hex(core::sha256::hash(value));
        }

        [[nodiscard]] Result reject(Code code, std::string status) noexcept
        {
            Result result{};
            result.code = code;
            result.status = std::move(status);
            return result;
        }

        [[nodiscard]] bool concrete_provider(
            ai::project_profile::Provider provider) noexcept
        {
            return provider == ai::project_profile::Provider::epoch_local_qwen38
                || provider == ai::project_profile::Provider::external_mcp;
        }

        constexpr std::array<std::uint8_t, 8u> checkpoint_magic{
            'E', 'P', 'S', 'I', 'C', 'K', 'P', 'T'};
        constexpr std::size_t checkpoint_seal_size{32u};
        constexpr std::size_t maximum_checkpoint_size{2u * 1024u * 1024u};

        [[nodiscard]] std::string canonical_path_text(
            const std::filesystem::path& value)
        {
            std::string result = value.lexically_normal().generic_string();
            while (result.size() > 3u && result.back() == '/')
                result.pop_back();
            return result;
        }

        class Writer final
        {
        public:
            void raw(std::span<const std::uint8_t> bytes)
            {
                bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
            }

            void u8(std::uint8_t value)
            {
                bytes_.push_back(value);
            }

            void u32(std::uint32_t value)
            {
                for (std::size_t index = 0u; index < 4u; ++index)
                    u8(static_cast<std::uint8_t>(value >> (index * 8u)));
            }

            void u64(std::uint64_t value)
            {
                for (std::size_t index = 0u; index < 8u; ++index)
                    u8(static_cast<std::uint8_t>(value >> (index * 8u)));
            }

            void boolean(bool value)
            {
                u8(value ? 1u : 0u);
            }

            void text(std::string_view value)
            {
                if (value.size() > (std::numeric_limits<std::uint32_t>::max)())
                    throw std::length_error("checkpoint string exceeds uint32");
                u32(static_cast<std::uint32_t>(value.size()));
                raw(std::span<const std::uint8_t>{
                    reinterpret_cast<const std::uint8_t*>(value.data()),
                    value.size()});
            }

            void path(const std::filesystem::path& value, bool canonical)
            {
                text(canonical
                    ? canonical_path_text(value)
                    : value.generic_string());
            }

            [[nodiscard]] std::vector<std::uint8_t> take()
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
                : bytes_(bytes)
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
                value = 0u;
                for (std::size_t index = 0u; index < 4u; ++index)
                {
                    std::uint8_t byte{};
                    if (!u8(byte))
                        return false;
                    value |= static_cast<std::uint32_t>(byte) << (index * 8u);
                }
                return true;
            }

            [[nodiscard]] bool u64(std::uint64_t& value) noexcept
            {
                value = 0u;
                for (std::size_t index = 0u; index < 8u; ++index)
                {
                    std::uint8_t byte{};
                    if (!u8(byte))
                        return false;
                    value |= static_cast<std::uint64_t>(byte) << (index * 8u);
                }
                return true;
            }

            [[nodiscard]] bool boolean(bool& value) noexcept
            {
                std::uint8_t encoded{};
                if (!u8(encoded) || encoded > 1u)
                    return false;
                value = encoded == 1u;
                return true;
            }

            [[nodiscard]] bool text(
                std::string& value,
                std::size_t maximum) noexcept
            {
                std::uint32_t length{};
                if (!u32(length) || length > maximum
                    || length > bytes_.size() - offset_)
                    return false;
                value.assign(
                    reinterpret_cast<const char*>(bytes_.data() + offset_),
                    length);
                offset_ += length;
                return true;
            }

            [[nodiscard]] bool magic() noexcept
            {
                for (const std::uint8_t expected : checkpoint_magic)
                {
                    std::uint8_t actual{};
                    if (!u8(actual) || actual != expected)
                        return false;
                }
                return true;
            }

            [[nodiscard]] bool finished() const noexcept
            {
                return offset_ == bytes_.size();
            }

        private:
            std::span<const std::uint8_t> bytes_{};
            std::size_t offset_{};
        };

        void encode_authority(
            Writer& writer,
            const ai::iteration_session::SourceAuthority& authority,
            bool canonicalPaths)
        {
            writer.u8(static_cast<std::uint8_t>(authority.target_kind));
            writer.u8(static_cast<std::uint8_t>(authority.kind));
            writer.path(authority.root, canonicalPaths);
            writer.text(authority.source_version);
            writer.text(authority.commit);
            writer.text(authority.receipt_digest);
            writer.text(authority.project_id);
            writer.text(authority.project_manifest_digest);
            writer.text(authority.project_profile_digest);
            writer.boolean(authority.verified);
        }

        void encode_budgets(
            Writer& writer,
            const ai::iteration_campaign::CampaignBudgets& budgets)
        {
            writer.u32(budgets.maximum_candidates);
            writer.u32(budgets.maximum_model_calls);
            writer.u32(budgets.maximum_source_operations);
            writer.u32(budgets.maximum_repairs_per_candidate);
            writer.u32(budgets.maximum_changes_per_candidate);
            writer.u32(budgets.maximum_curated_files);
            writer.u64(budgets.maximum_context_bytes);
            writer.u64(budgets.maximum_candidate_bytes);
            writer.u64(budgets.maximum_file_bytes);
            writer.u64(budgets.maximum_transaction_bytes);
            writer.u32(budgets.maximum_workspace_files);
            writer.u64(budgets.maximum_workspace_bytes);
            writer.u32(budgets.maximum_validation_records);
        }

        void encode_configuration(
            Writer& writer,
            const ai::self_iteration_orchestrator::Configuration& configuration,
            bool canonicalPaths)
        {
            encode_authority(writer, configuration.authority, canonicalPaths);
            if (configuration.curated_files.size()
                > (std::numeric_limits<std::uint32_t>::max)())
                throw std::length_error("too many curated files");
            writer.u32(static_cast<std::uint32_t>(
                configuration.curated_files.size()));
            for (const auto& file : configuration.curated_files)
            {
                writer.text(file.relative_path);
                writer.text(file.sha256);
                writer.u64(file.byte_count);
            }
            writer.path(configuration.cache_root, canonicalPaths);
            writer.text(configuration.objective);
            writer.u8(static_cast<std::uint8_t>(configuration.provider));
            writer.text(configuration.project_profile_bytes);
            writer.text(configuration.engine_model_binding);
            writer.text(configuration.operator_model_binding);
            writer.text(configuration.host.binding_id);
            writer.text(configuration.host.configuration_sha256);
            writer.u64(configuration.host.generation);
            writer.boolean(configuration.host.stdio_only);
            writer.boolean(configuration.host.automatic_launch);
            writer.boolean(configuration.host.network_enabled);
            writer.boolean(configuration.host.listener_enabled);
            writer.boolean(configuration.host.server_enabled);
            encode_budgets(writer, configuration.budgets);
            writer.u64(configuration.created_at_unix_seconds);
            writer.u64(configuration.duration_seconds);
            writer.boolean(configuration.engine_source_campaign_permitted);
            writer.boolean(configuration.project_source_campaign_permitted);
            writer.boolean(configuration.sandbox_apply_permitted);
        }

        void encode_bridge(
            Writer& writer,
            const BridgeConfiguration& bridge)
        {
            writer.text(bridge.transport_session_id);
            writer.text(bridge.endpoint_binding);
            writer.text(bridge.model_binding);
            writer.u64(bridge.binding_generation);
            writer.boolean(bridge.outbound_client_only);
            writer.boolean(bridge.auto_connect);
            writer.boolean(bridge.listener);
            writer.boolean(bridge.server);
        }

        void encode_checkpoint_fields(
            Writer& writer,
            const Checkpoint& checkpoint)
        {
            writer.text(checkpoint.project_id);
            writer.text(checkpoint.project_root);
            writer.text(checkpoint.manifest_sha256);
            writer.text(checkpoint.descriptor_set_sha256);
            writer.text(checkpoint.project_profile_sha256);
            writer.text(checkpoint.binding_sha256);
            writer.text(checkpoint.orchestrator_configuration_sha256);
            writer.text(checkpoint.bridge_configuration_sha256);
            writer.u8(static_cast<std::uint8_t>(checkpoint.concrete_provider));
            writer.u64(checkpoint.generation);
            writer.text(checkpoint.status);
        }

        [[nodiscard]] std::string configuration_digest(
            const ai::self_iteration_orchestrator::Configuration& configuration)
        {
            Writer writer{};
            writer.text("epoch-project-iteration-configuration/v1");
            encode_configuration(writer, configuration, true);
            const auto bytes = writer.take();
            return core::sha256::hex(core::sha256::hash(bytes));
        }

        [[nodiscard]] std::string bridge_digest(
            const BridgeConfiguration& bridge)
        {
            Writer writer{};
            writer.text("epoch-project-iteration-bridge/v1");
            encode_bridge(writer, bridge);
            const auto bytes = writer.take();
            return core::sha256::hex(core::sha256::hash(bytes));
        }

        [[nodiscard]] std::vector<std::uint8_t> encode_plan(
            const Plan& plan,
            bool canonicalPaths)
        {
            Writer writer{};
            writer.raw(checkpoint_magic);
            writer.u32(checkpoint_format_version);
            encode_checkpoint_fields(writer, plan.checkpoint);
            encode_configuration(
                writer, plan.orchestrator, canonicalPaths);
            encode_bridge(writer, plan.bridge);
            return writer.take();
        }

        [[nodiscard]] std::vector<std::uint8_t> seal_payload(
            std::vector<std::uint8_t> payload)
        {
            const auto seal = core::sha256::hash(payload);
            payload.insert(payload.end(), seal.bytes.begin(), seal.bytes.end());
            return payload;
        }

        [[nodiscard]] bool decode_provider(
            Reader& reader,
            ai::project_profile::Provider& provider) noexcept
        {
            std::uint8_t encoded{};
            if (!reader.u8(encoded)
                || encoded > static_cast<std::uint8_t>(
                    ai::project_profile::Provider::external_mcp))
                return false;
            provider = static_cast<ai::project_profile::Provider>(encoded);
            return true;
        }

        [[nodiscard]] bool decode_authority(
            Reader& reader,
            ai::iteration_session::SourceAuthority& authority) noexcept
        {
            std::uint8_t target{};
            std::uint8_t kind{};
            std::string root{};
            if (!reader.u8(target) || target > 1u
                || !reader.u8(kind) || kind > 3u
                || !reader.text(root, 4096u)
                || !reader.text(authority.source_version, 128u)
                || !reader.text(authority.commit, 128u)
                || !reader.text(authority.receipt_digest, 128u)
                || !reader.text(authority.project_id, 128u)
                || !reader.text(authority.project_manifest_digest, 64u)
                || !reader.text(authority.project_profile_digest, 64u)
                || !reader.boolean(authority.verified))
                return false;
            authority.target_kind =
                static_cast<ai::iteration_session::IterationTargetKind>(target);
            authority.kind =
                static_cast<ai::iteration_session::SourceAuthorityKind>(kind);
            authority.root = std::filesystem::path{root};
            return true;
        }

        [[nodiscard]] bool decode_budgets(
            Reader& reader,
            ai::iteration_campaign::CampaignBudgets& budgets) noexcept
        {
            return reader.u32(budgets.maximum_candidates)
                && reader.u32(budgets.maximum_model_calls)
                && reader.u32(budgets.maximum_source_operations)
                && reader.u32(budgets.maximum_repairs_per_candidate)
                && reader.u32(budgets.maximum_changes_per_candidate)
                && reader.u32(budgets.maximum_curated_files)
                && reader.u64(budgets.maximum_context_bytes)
                && reader.u64(budgets.maximum_candidate_bytes)
                && reader.u64(budgets.maximum_file_bytes)
                && reader.u64(budgets.maximum_transaction_bytes)
                && reader.u32(budgets.maximum_workspace_files)
                && reader.u64(budgets.maximum_workspace_bytes)
                && reader.u32(budgets.maximum_validation_records);
        }

        [[nodiscard]] bool decode_configuration(
            Reader& reader,
            ai::self_iteration_orchestrator::Configuration& configuration)
            noexcept
        {
            if (!decode_authority(reader, configuration.authority))
                return false;
            std::uint32_t fileCount{};
            if (!reader.u32(fileCount) || fileCount > 4096u)
                return false;
            configuration.curated_files.clear();
            configuration.curated_files.reserve(fileCount);
            for (std::uint32_t index = 0u; index < fileCount; ++index)
            {
                ai::iteration_session::CuratedFile file{};
                if (!reader.text(file.relative_path, 4096u)
                    || !reader.text(file.sha256, 64u)
                    || !reader.u64(file.byte_count))
                    return false;
                configuration.curated_files.push_back(std::move(file));
            }
            std::string cacheRoot{};
            if (!reader.text(cacheRoot, 4096u)
                || !reader.text(configuration.objective, 4096u)
                || !decode_provider(reader, configuration.provider)
                || !reader.text(configuration.project_profile_bytes, 1024u * 1024u)
                || !reader.text(configuration.engine_model_binding, 256u)
                || !reader.text(configuration.operator_model_binding, 256u)
                || !reader.text(configuration.host.binding_id, 256u)
                || !reader.text(configuration.host.configuration_sha256, 64u)
                || !reader.u64(configuration.host.generation)
                || !reader.boolean(configuration.host.stdio_only)
                || !reader.boolean(configuration.host.automatic_launch)
                || !reader.boolean(configuration.host.network_enabled)
                || !reader.boolean(configuration.host.listener_enabled)
                || !reader.boolean(configuration.host.server_enabled)
                || !decode_budgets(reader, configuration.budgets)
                || !reader.u64(configuration.created_at_unix_seconds)
                || !reader.u64(configuration.duration_seconds)
                || !reader.boolean(
                    configuration.engine_source_campaign_permitted)
                || !reader.boolean(
                    configuration.project_source_campaign_permitted)
                || !reader.boolean(configuration.sandbox_apply_permitted))
                return false;
            configuration.cache_root = std::filesystem::path{cacheRoot};
            return true;
        }

        [[nodiscard]] bool decode_bridge(
            Reader& reader,
            BridgeConfiguration& bridge) noexcept
        {
            return reader.text(bridge.transport_session_id, 256u)
                && reader.text(bridge.endpoint_binding, 1024u)
                && reader.text(bridge.model_binding, 256u)
                && reader.u64(bridge.binding_generation)
                && reader.boolean(bridge.outbound_client_only)
                && reader.boolean(bridge.auto_connect)
                && reader.boolean(bridge.listener)
                && reader.boolean(bridge.server);
        }

        [[nodiscard]] bool decode_checkpoint_fields(
            Reader& reader,
            Checkpoint& checkpoint) noexcept
        {
            return reader.text(checkpoint.project_id, 128u)
                && reader.text(checkpoint.project_root, 4096u)
                && reader.text(checkpoint.manifest_sha256, 64u)
                && reader.text(checkpoint.descriptor_set_sha256, 64u)
                && reader.text(checkpoint.project_profile_sha256, 64u)
                && reader.text(checkpoint.binding_sha256, 64u)
                && reader.text(
                    checkpoint.orchestrator_configuration_sha256, 64u)
                && reader.text(checkpoint.bridge_configuration_sha256, 64u)
                && decode_provider(reader, checkpoint.concrete_provider)
                && reader.u64(checkpoint.generation)
                && reader.text(checkpoint.status, 256u);
        }

        [[nodiscard]] bool decode_plan(
            std::span<const std::uint8_t> payload,
            Plan& plan) noexcept
        {
            Reader reader{payload};
            std::uint32_t version{};
            return reader.magic() && reader.u32(version)
                && version == checkpoint_format_version
                && decode_checkpoint_fields(reader, plan.checkpoint)
                && decode_configuration(reader, plan.orchestrator)
                && decode_bridge(reader, plan.bridge)
                && reader.finished();
        }

        [[nodiscard]] bool budgets_bounded(
            const ai::iteration_campaign::CampaignBudgets& value) noexcept
        {
            const ai::iteration_campaign::CampaignBudgets maximum{};
            return value.maximum_candidates > 0u
                && value.maximum_candidates <= maximum.maximum_candidates
                && value.maximum_model_calls > 0u
                && value.maximum_model_calls <= maximum.maximum_model_calls
                && value.maximum_source_operations > 0u
                && value.maximum_source_operations
                    <= maximum.maximum_source_operations
                && value.maximum_repairs_per_candidate > 0u
                && value.maximum_repairs_per_candidate
                    <= maximum.maximum_repairs_per_candidate
                && value.maximum_changes_per_candidate > 0u
                && value.maximum_changes_per_candidate
                    <= maximum.maximum_changes_per_candidate
                && value.maximum_curated_files > 0u
                && value.maximum_curated_files
                    <= maximum.maximum_curated_files
                && value.maximum_context_bytes > 0u
                && value.maximum_context_bytes <= maximum.maximum_context_bytes
                && value.maximum_candidate_bytes > 0u
                && value.maximum_candidate_bytes
                    <= maximum.maximum_candidate_bytes
                && value.maximum_file_bytes > 0u
                && value.maximum_file_bytes <= maximum.maximum_file_bytes
                && value.maximum_transaction_bytes > 0u
                && value.maximum_transaction_bytes
                    <= maximum.maximum_transaction_bytes
                && value.maximum_workspace_files > 0u
                && value.maximum_workspace_files
                    <= maximum.maximum_workspace_files
                && value.maximum_workspace_bytes > 0u
                && value.maximum_workspace_bytes
                    <= maximum.maximum_workspace_bytes
                && value.maximum_validation_records > 0u
                && value.maximum_validation_records
                    <= maximum.maximum_validation_records;
        }

        [[nodiscard]] Code validate_plan(
            const Plan& plan,
            std::string& status) noexcept
        {
            namespace profile = ai::project_profile;
            namespace session = ai::iteration_session;
            const auto& checkpoint = plan.checkpoint;
            const auto& configuration = plan.orchestrator;
            const auto& authority = configuration.authority;
            const auto& bridge = plan.bridge;
            const std::filesystem::path checkpointRoot{
                checkpoint.project_root};
            const std::string normalizedCheckpointRoot =
                canonical_path_text(checkpointRoot);
            const std::string normalizedAuthorityRoot =
                canonical_path_text(authority.root);

            if (!safe_id(checkpoint.project_id)
                || !checkpointRoot.is_absolute()
                || normalizedCheckpointRoot != checkpoint.project_root
                || !hex_digest(checkpoint.manifest_sha256)
                || !hex_digest(checkpoint.descriptor_set_sha256)
                || !hex_digest(checkpoint.project_profile_sha256)
                || !hex_digest(checkpoint.binding_sha256)
                || checkpoint.generation == 0u)
            {
                status = "Checkpoint identity fields are malformed.";
                return Code::checkpoint_malformed;
            }
            if (!concrete_provider(checkpoint.concrete_provider)
                || !concrete_provider(configuration.provider))
            {
                status = "Checkpoint provider is unresolved.";
                return Code::provider_unresolved;
            }
            if (checkpoint.concrete_provider != configuration.provider)
            {
                status = "Checkpoint provider conflicts with its admitted configuration.";
                return Code::provider_conflict;
            }
            if (authority.target_kind
                    != session::IterationTargetKind::project_source
                || authority.kind != session::SourceAuthorityKind::verified_project
                || !authority.verified
                || authority.project_id != checkpoint.project_id
                || normalizedAuthorityRoot != checkpoint.project_root
                || authority.project_manifest_digest
                    != checkpoint.manifest_sha256
                || authority.project_profile_digest
                    != checkpoint.project_profile_sha256
                || !authority.source_version.empty()
                || !authority.commit.empty()
                || !authority.receipt_digest.empty()
                || configuration.engine_source_campaign_permitted
                || !configuration.project_source_campaign_permitted
                || !configuration.sandbox_apply_permitted)
            {
                status = "Checkpoint attempts to broaden project-source authority.";
                return Code::authority_rejected;
            }
            if (!configuration.curated_files.empty()
                || !configuration.cache_root.is_absolute()
                || configuration.objective.empty()
                || configuration.objective.size() > 4096u
                || !configuration.engine_model_binding.empty()
                || configuration.operator_model_binding.empty()
                || configuration.operator_model_binding.size() > 256u
                || configuration.created_at_unix_seconds == 0u
                || configuration.duration_seconds != 8u * 60u * 60u
                || !budgets_bounded(configuration.budgets)
                || configuration.host.binding_id.empty()
                || configuration.host.binding_id.size() > 256u
                || !hex_digest(configuration.host.configuration_sha256)
                || configuration.host.generation == 0u
                || !configuration.host.stdio_only
                || configuration.host.automatic_launch
                || configuration.host.network_enabled
                || configuration.host.listener_enabled
                || configuration.host.server_enabled)
            {
                status = "Checkpoint configuration violates bounded project-session policy.";
                return Code::authority_rejected;
            }

            const auto decoded = profile::parse_profile(
                configuration.project_profile_bytes);
            if (!decoded || !decoded.profile.enabled
                || decoded.sha256 != checkpoint.project_profile_sha256)
            {
                status = "Checkpoint project profile failed canonical verification.";
                return Code::profile_rejected;
            }
            if (decoded.profile.provider != profile::Provider::engine_selected
                && decoded.profile.provider != configuration.provider)
            {
                status = "Checkpoint provider conflicts with the project profile.";
                return Code::provider_conflict;
            }
            if (bridge.transport_session_id
                    != "project-iteration-" + checkpoint.binding_sha256
                || bridge.endpoint_binding.empty()
                || bridge.endpoint_binding.size() > 1024u
                || bridge.model_binding
                    != configuration.operator_model_binding
                || bridge.binding_generation == 0u
                || !bridge.outbound_client_only || bridge.auto_connect
                || bridge.listener || bridge.server)
            {
                status = "Checkpoint bridge attempts to broaden transport authority.";
                return Code::authority_rejected;
            }
            const std::string expectedBinding = digest(
                checkpoint.project_id + "\n" + checkpoint.project_root + "\n"
                + std::to_string(
                    static_cast<unsigned>(checkpoint.concrete_provider))
                + "\n" + bridge.model_binding + "\n"
                + bridge.endpoint_binding + "\n"
                + std::to_string(bridge.binding_generation));
            if (expectedBinding != checkpoint.binding_sha256)
            {
                status = "Checkpoint session binding digest is stale.";
                return Code::checkpoint_integrity;
            }
            if (!hex_digest(checkpoint.orchestrator_configuration_sha256)
                || configuration_digest(configuration)
                    != checkpoint.orchestrator_configuration_sha256
                || !hex_digest(checkpoint.bridge_configuration_sha256)
                || bridge_digest(bridge)
                    != checkpoint.bridge_configuration_sha256)
            {
                status = "Checkpoint configuration digest verification failed.";
                return Code::checkpoint_integrity;
            }
            if (checkpoint.status
                != "ready_for_operator_directed_project_source_plan")
            {
                status = "Checkpoint status is not resumable.";
                return Code::checkpoint_malformed;
            }
            status = "Checkpoint authority and configuration are bounded.";
            return Code::ready;
        }

        [[nodiscard]] SerializedCheckpoint make_serialized(
            const Plan& plan,
            bool canonicalPaths)
        {
            SerializedCheckpoint result{};
            result.code = Code::ready;
            result.canonical_bytes = seal_payload(
                encode_plan(plan, canonicalPaths));
            result.sha256 = core::sha256::hex(
                core::sha256::hash(result.canonical_bytes));
            result.status = "Canonical project iteration checkpoint serialized.";
            return result;
        }
    }

    Result admit(const Request& request, const Checkpoint* prior) noexcept
    {
        namespace profile = ai::project_profile;
        namespace orchestrator = ai::self_iteration_orchestrator;
        try
        {
            if (!safe_id(request.project_id) || !request.project_root.is_absolute()
                || !request.disposable_cache_root.is_absolute()
                || !hex_digest(request.manifest_sha256)
                || request.objective.empty() || request.objective.size() > 4096u
                || request.created_at_unix_seconds == 0u
                || !request.binding.operator_confirmed
                || request.binding.generation == 0u
                || request.binding.model_binding.empty()
                || request.binding.model_binding.size() > 256u
                || request.binding.endpoint_binding.empty()
                || request.binding.endpoint_binding.size() > 1024u)
                return reject(Code::invalid_request,
                    "Project iteration admission requires one explicit confirmed session binding and bounded project identity.");

            const auto kit = project_ai_iteration::parse_kit(
                request.descriptors.source_review,
                request.descriptors.iteration_config,
                request.descriptors.mcp_bridge);
            if (!kit)
                return reject(Code::descriptors_rejected, kit.status);
            const auto decoded = profile::parse_profile(
                request.project_profile_bytes);
            if (!decoded)
                return reject(Code::profile_rejected, decoded.status);
            if (!decoded.profile.enabled
                || decoded.profile.provider == profile::Provider::disabled)
                return reject(Code::disabled,
                    "Project AI profile is disabled; no project iteration session was admitted.");

            const auto configuredProvider = decoded.profile.provider;
            const auto selectedProvider = request.binding.concrete_provider;
            if (configuredProvider == profile::Provider::engine_selected)
            {
                if (!concrete_provider(selectedProvider))
                    return reject(Code::provider_unresolved,
                        "Engine-selected project AI requires a confirmed concrete current provider.");
            }
            else if (configuredProvider != selectedProvider)
                return reject(Code::provider_conflict,
                    "Concrete session provider conflicts with the canonical project AI profile.");

            if (kit.kit.provider
                    == project_ai_iteration::ProviderPolicy::external_mcp_client
                && selectedProvider != profile::Provider::external_mcp)
                return reject(Code::provider_conflict,
                    "External MCP kit requires the confirmed external MCP provider.");
            if (selectedProvider == profile::Provider::external_mcp
                && request.binding.endpoint_binding
                    == "operator_selected_runtime_endpoint")
                return reject(Code::provider_unresolved,
                    "External MCP endpoint must be resolved for this confirmed session.");

            if (!request.host.stdio_only || request.host.automatic_launch
                || request.host.network_enabled || request.host.listener_enabled
                || request.host.server_enabled)
                return reject(Code::authority_rejected,
                    "Project iteration host must remain inert, stdio-only, and non-network-serving.");

            const std::string root = canonical_path_text(request.project_root);
            const std::string descriptorDigest = digest(
                request.descriptors.source_review + "\n--iteration--\n"
                + request.descriptors.iteration_config + "\n--bridge--\n"
                + request.descriptors.mcp_bridge);
            const std::string bindingDigest = digest(
                request.project_id + "\n" + root + "\n"
                + std::to_string(static_cast<unsigned>(selectedProvider)) + "\n"
                + request.binding.model_binding + "\n"
                + request.binding.endpoint_binding + "\n"
                + std::to_string(request.binding.generation));

            std::uint64_t generation = 1u;
            if (prior)
            {
                if (prior->project_id != request.project_id
                    || prior->project_root != root
                    || prior->manifest_sha256 != request.manifest_sha256)
                    return reject(Code::cross_project_checkpoint,
                        "Checkpoint belongs to another project identity or manifest.");
                if (prior->generation != request.expected_checkpoint_generation
                    || prior->descriptor_set_sha256 != descriptorDigest
                    || prior->project_profile_sha256 != decoded.sha256)
                    return reject(Code::stale_generation,
                        "Checkpoint generation or canonical descriptor evidence is stale.");
                generation = prior->generation + 1u;
            }
            else if (request.expected_checkpoint_generation != 0u)
                return reject(Code::stale_generation,
                    "Initial project iteration admission expects generation zero.");

            orchestrator::Configuration configuration{};
            configuration.authority = {
                .target_kind = ai::iteration_session::IterationTargetKind::project_source,
                .kind = ai::iteration_session::SourceAuthorityKind::verified_project,
                .root = request.project_root,
                .project_id = request.project_id,
                .project_manifest_digest = request.manifest_sha256,
                .project_profile_digest = decoded.sha256,
                .verified = true};
            configuration.cache_root = request.disposable_cache_root;
            configuration.objective = request.objective;
            configuration.provider = selectedProvider;
            configuration.project_profile_bytes = request.project_profile_bytes;
            configuration.operator_model_binding =
                request.binding.model_binding;
            configuration.host = request.host;
            configuration.budgets.maximum_candidates =
                kit.kit.limits.maximum_iterations;
            configuration.budgets.maximum_model_calls =
                kit.kit.limits.maximum_tool_calls;
            configuration.budgets.maximum_source_operations =
                kit.kit.limits.maximum_tool_calls;
            configuration.budgets.maximum_curated_files = (std::min)(
                kit.kit.limits.maximum_source_files,
                configuration.budgets.maximum_curated_files);
            configuration.budgets.maximum_context_bytes = (std::min)(
                static_cast<std::uint64_t>(
                    kit.kit.limits.maximum_context_bytes),
                configuration.budgets.maximum_context_bytes);
            configuration.budgets.maximum_candidate_bytes = (std::min)(
                static_cast<std::uint64_t>(
                    kit.kit.limits.maximum_patch_bytes),
                configuration.budgets.maximum_candidate_bytes);
            configuration.created_at_unix_seconds =
                request.created_at_unix_seconds;
            configuration.engine_source_campaign_permitted = false;
            configuration.project_source_campaign_permitted = true;
            configuration.sandbox_apply_permitted = true;

            Result result{};
            result.code = Code::ready;
            result.plan.orchestrator = std::move(configuration);
            result.plan.bridge = {
                .transport_session_id = "project-iteration-" + bindingDigest,
                .endpoint_binding = request.binding.endpoint_binding,
                .model_binding = request.binding.model_binding,
                .binding_generation = request.binding.generation};
            result.plan.checkpoint = {
                .project_id = request.project_id,
                .project_root = root,
                .manifest_sha256 = request.manifest_sha256,
                .descriptor_set_sha256 = descriptorDigest,
                .project_profile_sha256 = decoded.sha256,
                .binding_sha256 = bindingDigest,
                .concrete_provider = selectedProvider,
                .generation = generation,
                .status = "ready_for_operator_directed_project_source_plan"};
            result.plan.checkpoint.orchestrator_configuration_sha256 =
                configuration_digest(result.plan.orchestrator);
            result.plan.checkpoint.bridge_configuration_sha256 =
                bridge_digest(result.plan.bridge);
            result.status =
                "Project iteration session admitted without starting transport or granting approval.";
            return result;
        }
        catch (...)
        {
            return reject(Code::invalid_request,
                "Project iteration admission allocation failed.");
        }
    }

    SerializedCheckpoint serialize_checkpoint(const Plan& plan) noexcept
    {
        try
        {
            std::string status{};
            const Code validation = validate_plan(plan, status);
            if (validation != Code::ready)
            {
                SerializedCheckpoint result{};
                result.code = validation;
                result.status = std::move(status);
                return result;
            }
            return make_serialized(plan, true);
        }
        catch (...)
        {
            SerializedCheckpoint result{};
            result.code = Code::checkpoint_malformed;
            result.status = "Checkpoint serialization failed without producing bytes.";
            return result;
        }
    }

    RestoredCheckpoint restore_checkpoint(
        std::span<const std::uint8_t> bytes,
        const RestoreExpectation& expectation) noexcept
    {
        try
        {
            RestoredCheckpoint result{};
            if (!safe_id(expectation.project_id)
                || !expectation.project_root.is_absolute()
                || expectation.current_generation == 0u
                || !hex_digest(expectation.checkpoint_sha256))
            {
                result.code = Code::invalid_request;
                result.status =
                    "Checkpoint restore requires exact project, root, generation, and record digest expectations.";
                return result;
            }
            if (bytes.size() <= checkpoint_magic.size() + 4u
                    + checkpoint_seal_size
                || bytes.size() > maximum_checkpoint_size)
            {
                result.code = Code::checkpoint_malformed;
                result.status = "Checkpoint record length is invalid.";
                return result;
            }

            const std::string actualRecordDigest =
                core::sha256::hex(core::sha256::hash(bytes));
            if (actualRecordDigest != expectation.checkpoint_sha256)
            {
                result.code = Code::checkpoint_integrity;
                result.status =
                    "Checkpoint record does not match the caller-held digest.";
                return result;
            }

            const auto payload = bytes.first(
                bytes.size() - checkpoint_seal_size);
            const auto suppliedSeal = bytes.last(checkpoint_seal_size);
            const auto computedSeal = core::sha256::hash(payload);
            std::uint8_t difference{};
            for (std::size_t index = 0u;
                index < checkpoint_seal_size; ++index)
            {
                difference |= static_cast<std::uint8_t>(
                    computedSeal.bytes[index] ^ suppliedSeal[index]);
            }
            if (difference != 0u)
            {
                result.code = Code::checkpoint_integrity;
                result.status = "Checkpoint payload seal verification failed.";
                return result;
            }

            Plan decoded{};
            if (!decode_plan(payload, decoded))
            {
                result.code = Code::checkpoint_malformed;
                result.status =
                    "Checkpoint payload is truncated, trailing, or structurally invalid.";
                return result;
            }
            std::string validationStatus{};
            const Code validation = validate_plan(decoded, validationStatus);
            if (validation != Code::ready)
            {
                result.code = validation;
                result.status = std::move(validationStatus);
                return result;
            }

            const std::string expectedRoot =
                canonical_path_text(expectation.project_root);
            if (decoded.checkpoint.project_id != expectation.project_id
                || decoded.checkpoint.project_root != expectedRoot)
            {
                result.code = Code::cross_project_checkpoint;
                result.status =
                    "Checkpoint restore expectation belongs to another project.";
                return result;
            }
            if (decoded.checkpoint.generation
                != expectation.current_generation)
            {
                result.code = Code::stale_generation;
                result.status =
                    "Checkpoint generation is stale for the current project session.";
                return result;
            }

            const SerializedCheckpoint canonical =
                serialize_checkpoint(decoded);
            if (!canonical
                || canonical.canonical_bytes.size() != bytes.size()
                || !std::equal(
                    canonical.canonical_bytes.begin(),
                    canonical.canonical_bytes.end(),
                    bytes.begin()))
            {
                result.code = Code::checkpoint_noncanonical;
                result.status =
                    "Checkpoint is validly framed but not canonically encoded.";
                return result;
            }

            result.code = Code::ready;
            result.plan = std::move(decoded);
            result.canonical_sha256 = actualRecordDigest;
            result.status =
                "Canonical project iteration checkpoint restored without external effects.";
            return result;
        }
        catch (...)
        {
            RestoredCheckpoint result{};
            result.code = Code::checkpoint_malformed;
            result.status =
                "Checkpoint restore failed without admitting session authority.";
            return result;
        }
    }

    ContractFailure run_contract() noexcept
    {
        namespace profile = ai::project_profile;
        const auto make_request = [](
            profile::Provider configured,
            profile::Provider selected,
            project_ai_iteration::ProviderPolicy kitPolicy)
        {
            const auto kit = project_ai_iteration::serialize_kit(
                project_ai_iteration::make_kit(kitPolicy));
            const auto projectProfile = profile::serialize_profile(
                profile::make_profile(configured));
#if defined(_WIN32)
            const std::filesystem::path projectRoot{"C:/epoch/contracts/project"};
            const std::filesystem::path cacheRoot{"C:/epoch/contracts/cache"};
#else
            const std::filesystem::path projectRoot{"/epoch/contracts/project"};
            const std::filesystem::path cacheRoot{"/epoch/contracts/cache"};
#endif
            Request request{
                .project_id = "contract-project",
                .project_root = projectRoot,
                .manifest_sha256 = std::string(64u, 'a'),
                .descriptors = {
                    .source_review = kit.source_review_bytes,
                    .iteration_config = kit.iteration_config_bytes,
                    .mcp_bridge = kit.mcp_bridge_bytes},
                .project_profile_bytes = projectProfile.canonical_bytes,
                .binding = {
                    .concrete_provider = selected,
                    .model_binding = selected == profile::Provider::external_mcp
                        ? "operator-external-model" : "epoch-local-qwen",
                    .endpoint_binding =
                        selected == profile::Provider::external_mcp
                        ? "https://operator.example/mcp"
                        : "epoch-local-runtime",
                    .generation = 7u,
                    .operator_confirmed = true},
                .host = {
                    .binding_id = "contract-host",
                    .configuration_sha256 = std::string(64u, 'b'),
                    .generation = 3u,
                    .stdio_only = true},
                .disposable_cache_root = cacheRoot,
                .objective = "Review and propose one bounded project-source repair.",
                .created_at_unix_seconds = 1'800'000'000u};
            return request;
        };

        Request disabled = make_request(
            profile::Provider::disabled,
            profile::Provider::epoch_local_qwen38,
            project_ai_iteration::ProviderPolicy::engine_selected);
        if (admit(disabled).code != Code::disabled)
            return ContractFailure::disabled_refusal;

        Request selectedLocal = make_request(
            profile::Provider::engine_selected,
            profile::Provider::epoch_local_qwen38,
            project_ai_iteration::ProviderPolicy::engine_selected);
        const Result local = admit(selectedLocal);
        if (!local
            || local.plan.orchestrator.provider
                != profile::Provider::epoch_local_qwen38
            || local.plan.orchestrator.project_profile_bytes
                != selectedLocal.project_profile_bytes)
            return ContractFailure::selected_local;

        Request selectedExternal = make_request(
            profile::Provider::engine_selected,
            profile::Provider::external_mcp,
            project_ai_iteration::ProviderPolicy::engine_selected);
        const Result external = admit(selectedExternal);
        if (!external
            || external.plan.orchestrator.provider
                != profile::Provider::external_mcp
            || !external.plan.bridge.outbound_client_only
            || external.plan.bridge.auto_connect
            || external.plan.bridge.listener || external.plan.bridge.server)
            return ContractFailure::selected_external;

        Request directExternal = make_request(
            profile::Provider::external_mcp,
            profile::Provider::external_mcp,
            project_ai_iteration::ProviderPolicy::external_mcp_client);
        if (!admit(directExternal))
            return ContractFailure::direct_external;

        Request unresolved = selectedLocal;
        unresolved.binding.concrete_provider = profile::Provider::disabled;
        if (admit(unresolved).code != Code::provider_unresolved)
            return ContractFailure::unresolved_refusal;

        Request tampered = selectedLocal;
        tampered.descriptors.mcp_bridge.replace(
            tampered.descriptors.mcp_bridge.find("listen=false"),
            std::string{"listen=false"}.size(), "listen=true");
        if (admit(tampered).code != Code::descriptors_rejected)
            return ContractFailure::descriptor_tamper;

        Request resumed = selectedLocal;
        resumed.expected_checkpoint_generation =
            local.plan.checkpoint.generation;
        Request otherProject = resumed;
        otherProject.project_id = "other-project";
        if (admit(otherProject, &local.plan.checkpoint).code
                != Code::cross_project_checkpoint)
            return ContractFailure::cross_project;

        Request stale = resumed;
        stale.expected_checkpoint_generation = 0u;
        if (admit(stale, &local.plan.checkpoint).code
                != Code::stale_generation)
            return ContractFailure::stale_generation;

        Request serving = selectedLocal;
        serving.host.network_enabled = true;
        serving.host.listener_enabled = true;
        if (admit(serving).code != Code::authority_rejected
            || !local.plan.orchestrator.project_source_campaign_permitted
            || local.plan.orchestrator.engine_source_campaign_permitted
            || local.plan.orchestrator.authority.target_kind
                != ai::iteration_session::IterationTargetKind::project_source
            || local.plan.orchestrator.authority.root
                != selectedLocal.project_root)
            return ContractFailure::authority_boundary;

        const SerializedCheckpoint serialized =
            serialize_checkpoint(local.plan);
        if (!serialized || serialized.canonical_bytes.empty()
            || !hex_digest(serialized.sha256))
            return ContractFailure::checkpoint_roundtrip;
        RestoreExpectation expectation{
            .project_id = local.plan.checkpoint.project_id,
            .project_root = selectedLocal.project_root,
            .current_generation = local.plan.checkpoint.generation,
            .checkpoint_sha256 = serialized.sha256};
        const RestoredCheckpoint restored = restore_checkpoint(
            serialized.canonical_bytes, expectation);
        if (!restored
            || restored.canonical_sha256 != serialized.sha256
            || restored.plan.checkpoint != local.plan.checkpoint
            || restored.plan.bridge != local.plan.bridge
            || restored.plan.orchestrator.authority
                != local.plan.orchestrator.authority
            || restored.plan.orchestrator.host
                != local.plan.orchestrator.host
            || restored.plan.orchestrator.budgets
                != local.plan.orchestrator.budgets
            || restored.plan.orchestrator.project_profile_bytes
                != local.plan.orchestrator.project_profile_bytes
            || restored.plan.orchestrator.objective
                != local.plan.orchestrator.objective)
            return ContractFailure::checkpoint_roundtrip;

        std::vector<std::uint8_t> truncatedPayload{
            serialized.canonical_bytes.begin(),
            serialized.canonical_bytes.end() - checkpoint_seal_size};
        truncatedPayload.pop_back();
        const auto truncated = seal_payload(std::move(truncatedPayload));
        RestoreExpectation truncatedExpectation = expectation;
        truncatedExpectation.checkpoint_sha256 =
            core::sha256::hex(core::sha256::hash(truncated));
        if (restore_checkpoint(truncated, truncatedExpectation).code
            != Code::checkpoint_malformed)
            return ContractFailure::checkpoint_truncated;

        std::vector<std::uint8_t> trailingPayload{
            serialized.canonical_bytes.begin(),
            serialized.canonical_bytes.end() - checkpoint_seal_size};
        trailingPayload.push_back(0u);
        const auto trailing = seal_payload(std::move(trailingPayload));
        RestoreExpectation trailingExpectation = expectation;
        trailingExpectation.checkpoint_sha256 =
            core::sha256::hex(core::sha256::hash(trailing));
        if (restore_checkpoint(trailing, trailingExpectation).code
            != Code::checkpoint_malformed)
            return ContractFailure::checkpoint_trailing;

        std::vector<std::uint8_t> tamperedCheckpoint =
            serialized.canonical_bytes;
        tamperedCheckpoint[checkpoint_magic.size() + 8u] ^= 0x01u;
        if (restore_checkpoint(tamperedCheckpoint, expectation).code
            != Code::checkpoint_integrity)
            return ContractFailure::checkpoint_tamper;

        Plan noncanonicalPlan = local.plan;
        noncanonicalPlan.orchestrator.authority.root /=
            std::filesystem::path{"resume-segment/.."};
        noncanonicalPlan.orchestrator.cache_root /=
            std::filesystem::path{"resume-segment/.."};
        const SerializedCheckpoint noncanonical =
            make_serialized(noncanonicalPlan, false);
        RestoreExpectation noncanonicalExpectation = expectation;
        noncanonicalExpectation.checkpoint_sha256 = noncanonical.sha256;
        if (restore_checkpoint(
                noncanonical.canonical_bytes,
                noncanonicalExpectation).code
            != Code::checkpoint_noncanonical)
            return ContractFailure::checkpoint_noncanonical;

        RestoreExpectation otherExpectation = expectation;
        otherExpectation.project_id = "different-project";
        if (restore_checkpoint(
                serialized.canonical_bytes,
                otherExpectation).code != Code::cross_project_checkpoint)
            return ContractFailure::checkpoint_cross_project;

        RestoreExpectation staleExpectation = expectation;
        ++staleExpectation.current_generation;
        if (restore_checkpoint(
                serialized.canonical_bytes,
                staleExpectation).code != Code::stale_generation)
            return ContractFailure::checkpoint_stale;

        Plan unresolvedPlan = local.plan;
        unresolvedPlan.orchestrator.provider = profile::Provider::disabled;
        const SerializedCheckpoint unresolvedCheckpoint =
            make_serialized(unresolvedPlan, true);
        RestoreExpectation unresolvedExpectation = expectation;
        unresolvedExpectation.checkpoint_sha256 =
            unresolvedCheckpoint.sha256;
        if (restore_checkpoint(
                unresolvedCheckpoint.canonical_bytes,
                unresolvedExpectation).code != Code::provider_unresolved)
            return ContractFailure::checkpoint_unresolved;

        Plan broadenedPlan = local.plan;
        broadenedPlan.orchestrator.engine_source_campaign_permitted = true;
        const SerializedCheckpoint broadenedCheckpoint =
            make_serialized(broadenedPlan, true);
        RestoreExpectation broadenedExpectation = expectation;
        broadenedExpectation.checkpoint_sha256 =
            broadenedCheckpoint.sha256;
        if (restore_checkpoint(
                broadenedCheckpoint.canonical_bytes,
                broadenedExpectation).code != Code::authority_rejected)
            return ContractFailure::checkpoint_authority;

        return ContractFailure::none;
    }
}
