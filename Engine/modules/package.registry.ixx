module;

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "../include/_epoch.stl_types.hpp"

export module package.registry;

import forest.factory;

export namespace epoch::package_registry
{
    enum class PackageKind : std::uint8_t
    {
        RuntimeMini,
        CoreOptIn,
        NetworkRuntime,
        HeadlessServer,
        ResearchPrototype,
        DownloadableSource
    };

    enum class ActivationMode : std::uint8_t
    {
        ProjectOptIn,
        MainSceneUse,
        HeadlessServerOptIn,
        ClientListenServerOptIn,
        ManualResearchImport,
        DownloadedOptIn
    };

    struct PackageDescriptor
    {
        std::string_view id{};
        std::string_view displayName{};
        std::string_view summary{};
        PackageKind kind{PackageKind::RuntimeMini};
        ActivationMode activation{ActivationMode::ProjectOptIn};
        bool shipsInCore{};
        bool includeInGeneratedProject{};
        bool requiresHumanBuildGate{true};
        bool serverOrListenerAllowed{};
        bool headlessCapable{};
        bool requiresExplicitNetworkApproval{};
        std::string_view externalSourceRepo{};
    };

    inline constexpr std::string_view kEpochEngineExtensionsRepo = "https://github.com/Autodidac/EpochEngineExtensions";
    inline constexpr std::string_view kEngineArcadePackageId = "engine_arcade";
    inline constexpr std::string_view kEngineArcadeSceneId = "engine_arcade_scene";
    inline constexpr std::string_view kEngineForestFactoryPackageId = epoch::forest::kForestFactoryPackageId;
    inline constexpr std::string_view kEngineNetworkRuntimePackageId = "engine_network_runtime";
    inline constexpr std::string_view kEngineAuthoritativeServerPackageId = "engine_authoritative_dedicated_server";
    inline constexpr std::string_view kEngineListenServerPackageId = "engine_client_listen_server";

    inline constexpr std::array<PackageDescriptor, 10> kKnownPackages{{
        {
            .id = kEngineArcadePackageId,
            .displayName = "Engine Arcade",
            .summary = "Kernel mini-runtime scenes exposed as opt-in project package/script assets.",
            .kind = PackageKind::RuntimeMini,
            .activation = ActivationMode::ProjectOptIn,
            .shipsInCore = true,
        },
        {
            .id = kEngineForestFactoryPackageId,
            .displayName = "Forest Factory",
            .summary = "Deterministic procedural forest descriptors with project-local assets only when used.",
            .kind = PackageKind::CoreOptIn,
            .activation = ActivationMode::MainSceneUse,
            .shipsInCore = true,
        },
        {
            .id = kEngineNetworkRuntimePackageId,
            .displayName = "Engine Network Runtime",
            .summary = "Shared client/session replication contracts; inert unless a network package is explicitly enabled.",
            .kind = PackageKind::NetworkRuntime,
            .activation = ActivationMode::ProjectOptIn,
            .shipsInCore = true,
        },
        {
            .id = kEngineAuthoritativeServerPackageId,
            .displayName = "Optional Authoritative Dedicated Server",
            .summary = "Optional headless authoritative server package for multiplayer projects that explicitly choose that model.",
            .kind = PackageKind::HeadlessServer,
            .activation = ActivationMode::HeadlessServerOptIn,
            .serverOrListenerAllowed = true,
            .headlessCapable = true,
            .requiresExplicitNetworkApproval = true,
            .externalSourceRepo = kEpochEngineExtensionsRepo,
        },
        {
            .id = kEngineListenServerPackageId,
            .displayName = "Client Listen Server",
            .summary = "Client-hosted nondedicated or competitive-friendly session option for multiplayer projects that explicitly opt in.",
            .kind = PackageKind::NetworkRuntime,
            .activation = ActivationMode::ClientListenServerOptIn,
            .shipsInCore = true,
            .serverOrListenerAllowed = true,
            .requiresExplicitNetworkApproval = true,
            .externalSourceRepo = kEpochEngineExtensionsRepo,
        },
        {
            .id = "research_voxel_planetoid",
            .displayName = "Research Voxel Planetoid",
            .summary = "Renderer-neutral voxel/chunk prototype snapshot staged for reviewed package import.",
            .kind = PackageKind::ResearchPrototype,
            .activation = ActivationMode::ManualResearchImport,
            .externalSourceRepo = kEpochEngineExtensionsRepo,
        },
        {
            .id = "research_voxel_authoring",
            .displayName = "Research Voxel Authoring",
            .summary = "External voxel authoring/toolkit prototype staged for API-boundary review.",
            .kind = PackageKind::ResearchPrototype,
            .activation = ActivationMode::ManualResearchImport,
            .externalSourceRepo = kEpochEngineExtensionsRepo,
        },
        {
            .id = "research_forest_temporal_graph",
            .displayName = "Research Forest Factory Temporal Graph",
            .summary = "Temporal graph and parametric L-system forest lab staged as Forest Factory reference material only.",
            .kind = PackageKind::ResearchPrototype,
            .activation = ActivationMode::ManualResearchImport,
            .externalSourceRepo = kEpochEngineExtensionsRepo,
        },
        {
            .id = "research_planetary_terrain",
            .displayName = "Research Planetary Terrain",
            .summary = "NMS-like planetary and multi-terrain stacks remain package-gated.",
            .kind = PackageKind::DownloadableSource,
            .activation = ActivationMode::DownloadedOptIn,
            .externalSourceRepo = kEpochEngineExtensionsRepo,
        },
        {
            .id = "research_fft_ocean",
            .displayName = "Research FFT Ocean",
            .summary = "Ocean simulation package candidate; not part of the minimal engine clone.",
            .kind = PackageKind::DownloadableSource,
            .activation = ActivationMode::DownloadedOptIn,
            .externalSourceRepo = kEpochEngineExtensionsRepo,
        },
    }};

    [[nodiscard]] constexpr std::span<const PackageDescriptor> known_packages() noexcept
    {
        return {kKnownPackages.data(), kKnownPackages.size()};
    }

    [[nodiscard]] constexpr const PackageDescriptor* find(std::string_view id) noexcept
    {
        for (const auto& package : kKnownPackages)
        {
            if (package.id == id)
            {
                return &package;
            }
        }

        return nullptr;
    }

    [[nodiscard]] constexpr bool is_core_opt_in(std::string_view id) noexcept
    {
        const auto* package = find(id);
        return package != nullptr && package->kind == PackageKind::CoreOptIn;
    }

    [[nodiscard]] constexpr bool must_use_human_build_gate(std::string_view id) noexcept
    {
        const auto* package = find(id);
        return package == nullptr || package->requiresHumanBuildGate;
    }

    [[nodiscard]] constexpr bool requires_explicit_network_approval(std::string_view id) noexcept
    {
        const auto* package = find(id);
        return package != nullptr && package->requiresExplicitNetworkApproval;
    }

    [[nodiscard]] constexpr bool can_create_server_or_listener_after_approval(std::string_view id) noexcept
    {
        const auto* package = find(id);
        return package != nullptr &&
               package->serverOrListenerAllowed &&
               package->requiresExplicitNetworkApproval;
    }

    [[nodiscard]] constexpr std::string_view external_source_repo(std::string_view id) noexcept
    {
        const auto* package = find(id);
        return package != nullptr ? package->externalSourceRepo : std::string_view{};
    }
}
