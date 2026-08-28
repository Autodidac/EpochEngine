/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module project.ai_self_iteration_session;

export import ai.project_profile;
export import ai.self_iteration_orchestrator;
export import project.ai_self_iteration;

export namespace epochengine::project_ai_iteration_session
{
    enum class Code : std::uint8_t
    {
        ready,
        invalid_request,
        descriptors_rejected,
        profile_rejected,
        disabled,
        provider_unresolved,
        provider_conflict,
        authority_rejected,
        stale_generation,
        cross_project_checkpoint,
        checkpoint_malformed,
        checkpoint_integrity,
        checkpoint_noncanonical
    };

    [[nodiscard]] constexpr std::string_view code_name(Code code) noexcept
    {
        switch (code)
        {
        case Code::ready: return "ready";
        case Code::invalid_request: return "invalid_request";
        case Code::descriptors_rejected: return "descriptors_rejected";
        case Code::profile_rejected: return "profile_rejected";
        case Code::disabled: return "disabled";
        case Code::provider_unresolved: return "provider_unresolved";
        case Code::provider_conflict: return "provider_conflict";
        case Code::authority_rejected: return "authority_rejected";
        case Code::stale_generation: return "stale_generation";
        case Code::cross_project_checkpoint:
            return "cross_project_checkpoint";
        case Code::checkpoint_malformed: return "checkpoint_malformed";
        case Code::checkpoint_integrity: return "checkpoint_integrity";
        case Code::checkpoint_noncanonical: return "checkpoint_noncanonical";
        }
        return "unknown";
    }

    struct DescriptorBytes final
    {
        std::string source_review{};
        std::string iteration_config{};
        std::string mcp_bridge{};
    };

    struct SessionBinding final
    {
        ai::project_profile::Provider concrete_provider{
            ai::project_profile::Provider::disabled};
        std::string model_binding{};
        std::string endpoint_binding{};
        std::uint64_t generation{};
        bool operator_confirmed{};

        friend bool operator==(const SessionBinding&, const SessionBinding&) = default;
    };

    struct Request final
    {
        std::string project_id{};
        std::filesystem::path project_root{};
        std::string manifest_sha256{};
        DescriptorBytes descriptors{};
        std::string project_profile_bytes{};
        SessionBinding binding{};
        ai::self_iteration_orchestrator::HostBinding host{};
        std::filesystem::path disposable_cache_root{};
        std::string objective{};
        std::uint64_t expected_checkpoint_generation{};
        std::uint64_t created_at_unix_seconds{};
    };

    struct Checkpoint final
    {
        std::string project_id{};
        std::string project_root{};
        std::string manifest_sha256{};
        std::string descriptor_set_sha256{};
        std::string project_profile_sha256{};
        std::string binding_sha256{};
        std::string orchestrator_configuration_sha256{};
        std::string bridge_configuration_sha256{};
        ai::project_profile::Provider concrete_provider{
            ai::project_profile::Provider::disabled};
        std::uint64_t generation{};
        std::string status{};

        friend bool operator==(const Checkpoint&, const Checkpoint&) = default;
    };

    struct BridgeConfiguration final
    {
        std::string transport_session_id{};
        std::string endpoint_binding{};
        std::string model_binding{};
        std::uint64_t binding_generation{};
        bool outbound_client_only{true};
        bool auto_connect{};
        bool listener{};
        bool server{};

        friend bool operator==(const BridgeConfiguration&, const BridgeConfiguration&) = default;
    };

    struct Plan final
    {
        ai::self_iteration_orchestrator::Configuration orchestrator{};
        BridgeConfiguration bridge{};
        Checkpoint checkpoint{};
    };

    struct Result final
    {
        Code code{Code::invalid_request};
        Plan plan{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready;
        }
    };

    inline constexpr std::uint32_t checkpoint_format_version{1u};

    struct SerializedCheckpoint final
    {
        Code code{Code::invalid_request};
        std::vector<std::uint8_t> canonical_bytes{};
        std::string sha256{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready;
        }
    };

    struct RestoreExpectation final
    {
        std::string project_id{};
        std::filesystem::path project_root{};
        std::uint64_t current_generation{};
        std::string checkpoint_sha256{};
    };

    struct RestoredCheckpoint final
    {
        Code code{Code::invalid_request};
        Plan plan{};
        std::string canonical_sha256{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready;
        }
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        disabled_refusal,
        selected_local,
        selected_external,
        direct_external,
        unresolved_refusal,
        descriptor_tamper,
        cross_project,
        stale_generation,
        authority_boundary,
        checkpoint_roundtrip,
        checkpoint_truncated,
        checkpoint_trailing,
        checkpoint_tamper,
        checkpoint_noncanonical,
        checkpoint_cross_project,
        checkpoint_stale,
        checkpoint_unresolved,
        checkpoint_authority
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::disabled_refusal: return "disabled_refusal";
        case ContractFailure::selected_local: return "selected_local";
        case ContractFailure::selected_external: return "selected_external";
        case ContractFailure::direct_external: return "direct_external";
        case ContractFailure::unresolved_refusal: return "unresolved_refusal";
        case ContractFailure::descriptor_tamper: return "descriptor_tamper";
        case ContractFailure::cross_project: return "cross_project";
        case ContractFailure::stale_generation: return "stale_generation";
        case ContractFailure::authority_boundary: return "authority_boundary";
        case ContractFailure::checkpoint_roundtrip: return "checkpoint_roundtrip";
        case ContractFailure::checkpoint_truncated: return "checkpoint_truncated";
        case ContractFailure::checkpoint_trailing: return "checkpoint_trailing";
        case ContractFailure::checkpoint_tamper: return "checkpoint_tamper";
        case ContractFailure::checkpoint_noncanonical:
            return "checkpoint_noncanonical";
        case ContractFailure::checkpoint_cross_project:
            return "checkpoint_cross_project";
        case ContractFailure::checkpoint_stale: return "checkpoint_stale";
        case ContractFailure::checkpoint_unresolved:
            return "checkpoint_unresolved";
        case ContractFailure::checkpoint_authority:
            return "checkpoint_authority";
        }
        return "unknown";
    }

    [[nodiscard]] Result admit(
        const Request& request,
        const Checkpoint* prior = nullptr) noexcept;
    [[nodiscard]] SerializedCheckpoint serialize_checkpoint(
        const Plan& plan) noexcept;
    [[nodiscard]] RestoredCheckpoint restore_checkpoint(
        std::span<const std::uint8_t> bytes,
        const RestoreExpectation& expectation) noexcept;
    [[nodiscard]] ContractFailure run_contract() noexcept;
}
