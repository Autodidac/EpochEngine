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
        ModelAsset,
        ResearchPrototype,
        DownloadableSource
    };

    enum class ActivationMode : std::uint8_t
    {
        ProjectOptIn,
        MainSceneUse,
        HeadlessServerOptIn,
        ClientListenServerOptIn,
        ModelDownloadOptIn,
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
    inline constexpr std::string_view kForestFactoryReferenceRepo = epoch::forest::kForestFactoryReferenceRepo;
    inline constexpr std::string_view kForestFactoryPackageSourceRepo = kEpochEngineExtensionsRepo;
    inline constexpr std::string_view kEngineArcadePackageId = "engine_arcade";
    inline constexpr std::string_view kEngineArcadeSceneId = "engine_arcade_scene";
    inline constexpr std::string_view kEngineForestFactoryPackageId = epoch::forest::kForestFactoryPackageId;
    inline constexpr std::string_view kEngineNetworkRuntimePackageId = "engine_network_runtime";
    inline constexpr std::string_view kEngineAuthoritativeServerPackageId = "engine_authoritative_dedicated_server";
    inline constexpr std::string_view kEngineListenServerPackageId = "engine_client_listen_server";
    inline constexpr std::string_view kNemotronNanoPackageId = "os_model_nemotron_3_nano_4b_bf16";
    inline constexpr std::string_view kQwenCoderPackageId = "os_model_qwen_27b";
    inline constexpr std::string_view kBonsaiImageTernaryPackageId = "os_model_bonsai_image_ternary_4b_mlx_2bit";
    inline constexpr std::string_view kBonsaiImageBinaryPackageId = "os_model_bonsai_image_binary_4b_mlx_1bit";
    inline constexpr std::string_view kFluxKleinImagePackageId = "os_model_flux_2_klein_4b";

    inline constexpr std::array<PackageDescriptor, 15> kKnownPackages{{
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
            .summary = "Core temporal graph/L-system vegetation lab. Editor preview is built in; project assets are emitted only after explicit activation.",
            .kind = PackageKind::CoreOptIn,
            .activation = ActivationMode::MainSceneUse,
            .shipsInCore = true,
            .externalSourceRepo = kForestFactoryPackageSourceRepo,
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
            .id = kNemotronNanoPackageId,
            .displayName = "NVIDIA Nemotron 3 Nano 4B BF16",
            .summary = "Fast OS coding/review model lane. Weights download on demand into cache/models and may be included in projects only by explicit package opt-in.",
            .kind = PackageKind::ModelAsset,
            .activation = ActivationMode::ModelDownloadOptIn,
            .requiresHumanBuildGate = true,
            .externalSourceRepo = "https://huggingface.co/nvidia/NVIDIA-Nemotron-3-Nano-4B-BF16",
        },
        {
            .id = kQwenCoderPackageId,
            .displayName = "Qwen 3.6 27B",
            .summary = "Heavy OS coding/planning model lane. Weights download on demand into cache/models and may be included in projects only by explicit package opt-in.",
            .kind = PackageKind::ModelAsset,
            .activation = ActivationMode::ModelDownloadOptIn,
            .requiresHumanBuildGate = true,
            .externalSourceRepo = "https://huggingface.co/Qwen/Qwen3.6-27B",
        },
        {
            .id = kBonsaiImageTernaryPackageId,
            .displayName = "Bonsai Image Ternary 4B",
            .summary = "Preferred local image generation/editing model lane for the best quality/footprint balance. Weights download on demand into cache/models.",
            .kind = PackageKind::ModelAsset,
            .activation = ActivationMode::ModelDownloadOptIn,
            .requiresHumanBuildGate = true,
            .externalSourceRepo = "https://huggingface.co/prism-ml/bonsai-image-ternary-4B-mlx-2bit",
        },
        {
            .id = kBonsaiImageBinaryPackageId,
            .displayName = "Bonsai Image Binary 4B",
            .summary = "Optional low-memory local image generation/editing model lane for the smallest local footprint. Weights download on demand into cache/models.",
            .kind = PackageKind::ModelAsset,
            .activation = ActivationMode::ModelDownloadOptIn,
            .requiresHumanBuildGate = true,
            .externalSourceRepo = "https://huggingface.co/prism-ml/bonsai-image-binary-4B-mlx-1bit",
        },
        {
            .id = kFluxKleinImagePackageId,
            .displayName = "FLUX.2 Klein 4B",
            .summary = "Optional higher-memory fallback image generation/editing model lane. Bonsai Ternary 4B remains the recommended local default.",
            .kind = PackageKind::ModelAsset,
            .activation = ActivationMode::ModelDownloadOptIn,
            .requiresHumanBuildGate = true,
            .externalSourceRepo = "https://huggingface.co/black-forest-labs/FLUX.2-klein-4B",
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
            .summary = "Plant Lab source/prototype lineage used as Forest Factory reference material before production promotion.",
            .kind = PackageKind::ResearchPrototype,
            .activation = ActivationMode::ManualResearchImport,
            .externalSourceRepo = kForestFactoryReferenceRepo,
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
