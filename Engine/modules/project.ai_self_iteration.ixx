/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <string>
#include <string_view>

export module project.ai_self_iteration;

export namespace epochengine::project_ai_iteration
{
    inline constexpr std::string_view source_review_path{
        "Assets/AI/SelfIteration/source_review.epochreview"};
    inline constexpr std::string_view iteration_config_path{
        "Assets/AI/SelfIteration/iteration.epochiteration"};
    inline constexpr std::string_view mcp_bridge_path{
        "Assets/AI/SelfIteration/mcp_bridge.epochmcp"};

    enum class ProviderPolicy : std::uint8_t
    {
        engine_selected,
        external_mcp_client
    };

    enum class Provision : std::uint8_t
    {
        disabled = 0,
        engine_selected,
        external_mcp_client
    };

    [[nodiscard]] constexpr bool enabled(Provision provision) noexcept
    {
        return provision != Provision::disabled;
    }

    [[nodiscard]] constexpr ProviderPolicy provider_policy(
        Provision provision) noexcept
    {
        return provision == Provision::external_mcp_client
            ? ProviderPolicy::external_mcp_client
            : ProviderPolicy::engine_selected;
    }

    struct Limits final
    {
        std::uint32_t maximum_iterations{4u};
        std::uint32_t maximum_source_files{24u};
        std::uint32_t maximum_tool_calls{32u};
        std::uint32_t maximum_patch_bytes{512u * 1024u};
        std::uint32_t maximum_context_bytes{1024u * 1024u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_iterations != 0u && maximum_iterations <= 64u
                && maximum_source_files != 0u
                && maximum_source_files <= 1024u
                && maximum_tool_calls != 0u && maximum_tool_calls <= 4096u
                && maximum_patch_bytes >= 4096u
                && maximum_patch_bytes <= 16u * 1024u * 1024u
                && maximum_context_bytes >= maximum_patch_bytes
                && maximum_context_bytes <= 64u * 1024u * 1024u;
        }

        friend constexpr bool operator==(const Limits&, const Limits&) = default;
    };

    struct Kit final
    {
        ProviderPolicy provider{ProviderPolicy::engine_selected};
        Limits limits{};
        bool require_source_review{true};
        bool require_operator_approval{true};
        bool project_source_write{true};
        bool engine_source_write{};
        bool auto_start{};
        bool server_or_listener{};

        friend constexpr bool operator==(const Kit&, const Kit&) = default;
    };

    struct ProvisionedFiles final
    {
        bool accepted{};
        Kit kit{};
        std::string source_review_bytes{};
        std::string iteration_config_bytes{};
        std::string mcp_bridge_bytes{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return accepted && !source_review_bytes.empty()
                && !iteration_config_bytes.empty()
                && !mcp_bridge_bytes.empty();
        }
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        engine_selected_roundtrip,
        external_mcp_roundtrip,
        bounded_policy,
        client_only_bridge,
        tamper_rejection
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::engine_selected_roundtrip:
            return "engine_selected_roundtrip";
        case ContractFailure::external_mcp_roundtrip:
            return "external_mcp_roundtrip";
        case ContractFailure::bounded_policy: return "bounded_policy";
        case ContractFailure::client_only_bridge: return "client_only_bridge";
        case ContractFailure::tamper_rejection: return "tamper_rejection";
        }
        return "unknown";
    }

    [[nodiscard]] Kit make_kit(
        ProviderPolicy provider,
        Limits limits = {}) noexcept;
    [[nodiscard]] ProvisionedFiles serialize_kit(const Kit& kit) noexcept;
    [[nodiscard]] ProvisionedFiles parse_kit(
        std::string_view sourceReviewBytes,
        std::string_view iterationConfigBytes,
        std::string_view mcpBridgeBytes) noexcept;
    [[nodiscard]] ContractFailure run_contract() noexcept;
}
