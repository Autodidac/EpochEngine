/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

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

            const std::string root = request.project_root.lexically_normal().generic_string();
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

        return ContractFailure::none;
    }
}
