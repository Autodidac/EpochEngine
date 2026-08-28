/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

module project.ai_self_iteration;

namespace epochengine::project_ai_iteration
{
    namespace
    {
        [[nodiscard]] constexpr std::string_view provider_name(
            ProviderPolicy provider) noexcept
        {
            return provider == ProviderPolicy::external_mcp_client
                ? "external_mcp_client" : "engine_selected";
        }

        [[nodiscard]] constexpr std::string_view endpoint_binding(
            ProviderPolicy provider) noexcept
        {
            return provider == ProviderPolicy::external_mcp_client
                ? "operator_selected_runtime_endpoint"
                : "engine_selected_ai_runtime";
        }

        [[nodiscard]] constexpr std::string_view transport_name(
            ProviderPolicy provider) noexcept
        {
            return provider == ProviderPolicy::external_mcp_client
                ? "operator_managed_mcp_transport"
                : "epoch_internal_ai_dispatch";
        }

        [[nodiscard]] ProvisionedFiles reject(std::string status) noexcept
        {
            ProvisionedFiles result{};
            result.status = std::move(status);
            return result;
        }

        [[nodiscard]] std::string review_bytes()
        {
            return
                "schema=epoch.project.source_review.v1\n"
                "scope=project_root\n"
                "path_admission=existing_project_paths_only\n"
                "symbol_admission=host_evidence_required\n"
                "proposal_format=unified_diff\n"
                "live_source_read_only=true\n"
                "staging=disposable_workspace\n"
                "apply=operator_approved_only\n"
                "engine_source_write=false\n";
        }

        [[nodiscard]] std::string iteration_bytes(const Kit& kit)
        {
            return
                "schema=epoch.project.self_iteration.v1\n"
                "provider_policy=" + std::string{provider_name(kit.provider)} + "\n"
                "source_review=Assets/AI/SelfIteration/source_review.epochreview\n"
                "mcp_bridge=Assets/AI/SelfIteration/mcp_bridge.epochmcp\n"
                "maximum_iterations=" + std::to_string(kit.limits.maximum_iterations) + "\n"
                "maximum_source_files=" + std::to_string(kit.limits.maximum_source_files) + "\n"
                "maximum_tool_calls=" + std::to_string(kit.limits.maximum_tool_calls) + "\n"
                "maximum_patch_bytes=" + std::to_string(kit.limits.maximum_patch_bytes) + "\n"
                "maximum_context_bytes=" + std::to_string(kit.limits.maximum_context_bytes) + "\n"
                "require_source_review=true\n"
                "require_operator_approval=true\n"
                "project_source_write=true\n"
                "engine_source_write=false\n"
                "auto_start=false\n"
                "server_or_listener=false\n";
        }

        [[nodiscard]] std::string bridge_bytes(const Kit& kit)
        {
            return
                "schema=epoch.project.mcp_bridge.v1\n"
                "role=client\n"
                "direction=outbound_only\n"
                "protocol=epoch_mcp_v1\n"
                "provider_policy=" + std::string{provider_name(kit.provider)} + "\n"
                "transport=" + std::string{transport_name(kit.provider)} + "\n"
                "endpoint_binding=" + std::string{endpoint_binding(kit.provider)} + "\n"
                "credentials=operator_runtime_only\n"
                "tool_scope=reviewed_project_source\n"
                "listen=false\n"
                "bind=false\n"
                "serve=false\n"
                "auto_connect=false\n";
        }

        [[nodiscard]] bool parse_unsigned_line(
            std::string_view bytes,
            std::string_view key,
            std::uint32_t& value) noexcept
        {
            const std::string marker = std::string{key} + "=";
            const auto offset = bytes.find(marker);
            if (offset == std::string_view::npos
                || (offset != 0u && bytes[offset - 1u] != '\n'))
                return false;
            const auto start = offset + marker.size();
            const auto end = bytes.find('\n', start);
            if (end == std::string_view::npos || end == start)
                return false;
            const auto token = bytes.substr(start, end - start);
            std::uint64_t parsed{};
            const auto conversion = std::from_chars(
                token.data(), token.data() + token.size(), parsed);
            if (conversion.ec != std::errc{}
                || conversion.ptr != token.data() + token.size()
                || parsed > std::numeric_limits<std::uint32_t>::max())
                return false;
            value = static_cast<std::uint32_t>(parsed);
            return true;
        }
    }

    Kit make_kit(const ProviderPolicy provider, const Limits limits) noexcept
    {
        return Kit{.provider = provider, .limits = limits};
    }

    ProvisionedFiles serialize_kit(const Kit& kit) noexcept
    {
        if (!kit.limits.valid() || !kit.require_source_review
            || !kit.require_operator_approval || !kit.project_source_write
            || kit.engine_source_write || kit.auto_start
            || kit.server_or_listener)
        {
            return reject(
                "Project self-iteration kit conflicts with bounded source authority.");
        }
        try
        {
            return ProvisionedFiles{
                .accepted = true,
                .kit = kit,
                .source_review_bytes = review_bytes(),
                .iteration_config_bytes = iteration_bytes(kit),
                .mcp_bridge_bytes = bridge_bytes(kit),
                .status = "Project self-iteration kit is deterministic and client-only."};
        }
        catch (...)
        {
            return reject("Project self-iteration kit allocation failed.");
        }
    }

    ProvisionedFiles parse_kit(
        const std::string_view sourceReviewBytes,
        const std::string_view iterationConfigBytes,
        const std::string_view mcpBridgeBytes) noexcept
    {
        if (sourceReviewBytes.empty() || iterationConfigBytes.empty()
            || mcpBridgeBytes.empty() || sourceReviewBytes.size() > 16u * 1024u
            || iterationConfigBytes.size() > 16u * 1024u
            || mcpBridgeBytes.size() > 16u * 1024u)
            return reject("Project self-iteration descriptors are absent or oversized.");

        ProviderPolicy provider{};
        if (iterationConfigBytes.find("provider_policy=engine_selected\n")
            != std::string_view::npos)
            provider = ProviderPolicy::engine_selected;
        else if (iterationConfigBytes.find(
                     "provider_policy=external_mcp_client\n")
            != std::string_view::npos)
            provider = ProviderPolicy::external_mcp_client;
        else
            return reject("Project self-iteration provider policy is unsupported.");

        Limits limits{};
        if (!parse_unsigned_line(
                iterationConfigBytes, "maximum_iterations",
                limits.maximum_iterations)
            || !parse_unsigned_line(
                iterationConfigBytes, "maximum_source_files",
                limits.maximum_source_files)
            || !parse_unsigned_line(
                iterationConfigBytes, "maximum_tool_calls",
                limits.maximum_tool_calls)
            || !parse_unsigned_line(
                iterationConfigBytes, "maximum_patch_bytes",
                limits.maximum_patch_bytes)
            || !parse_unsigned_line(
                iterationConfigBytes, "maximum_context_bytes",
                limits.maximum_context_bytes))
            return reject("Project self-iteration limits are malformed.");

        const Kit kit = make_kit(provider, limits);
        const ProvisionedFiles canonical = serialize_kit(kit);
        if (!canonical || canonical.source_review_bytes != sourceReviewBytes
            || canonical.iteration_config_bytes != iterationConfigBytes
            || canonical.mcp_bridge_bytes != mcpBridgeBytes)
            return reject("Project self-iteration descriptors are not canonical or safe.");
        return canonical;
    }

    ContractFailure run_contract() noexcept
    {
        for (const auto [provider, failure] : {
                std::pair{ProviderPolicy::engine_selected,
                    ContractFailure::engine_selected_roundtrip},
                std::pair{ProviderPolicy::external_mcp_client,
                    ContractFailure::external_mcp_roundtrip}})
        {
            const ProvisionedFiles encoded = serialize_kit(make_kit(provider));
            const ProvisionedFiles decoded = parse_kit(
                encoded.source_review_bytes,
                encoded.iteration_config_bytes,
                encoded.mcp_bridge_bytes);
            if (!encoded || !decoded || decoded.kit != encoded.kit
                || decoded.source_review_bytes != encoded.source_review_bytes
                || decoded.iteration_config_bytes
                    != encoded.iteration_config_bytes
                || decoded.mcp_bridge_bytes != encoded.mcp_bridge_bytes)
                return failure;
        }

        Limits excessive{};
        excessive.maximum_iterations = 65u;
        if (serialize_kit(make_kit(
                ProviderPolicy::engine_selected, excessive)))
            return ContractFailure::bounded_policy;

        const ProvisionedFiles external = serialize_kit(
            make_kit(ProviderPolicy::external_mcp_client));
        if (!external
            || external.mcp_bridge_bytes.find("role=client\n")
                == std::string::npos
            || external.mcp_bridge_bytes.find("direction=outbound_only\n")
                == std::string::npos
            || external.mcp_bridge_bytes.find("listen=false\n")
                == std::string::npos
            || external.mcp_bridge_bytes.find("bind=false\n")
                == std::string::npos
            || external.mcp_bridge_bytes.find("serve=false\n")
                == std::string::npos)
            return ContractFailure::client_only_bridge;

        std::string tampered = external.mcp_bridge_bytes;
        tampered.replace(
            tampered.find("listen=false"),
            std::string{"listen=false"}.size(), "listen=true");
        if (parse_kit(
                external.source_review_bytes,
                external.iteration_config_bytes, tampered))
            return ContractFailure::tamper_rejection;
        return ContractFailure::none;
    }
}
