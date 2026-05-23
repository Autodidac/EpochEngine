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
        ResearchPrototype,
        DownloadableSource
    };

    enum class ActivationMode : std::uint8_t
    {
        ProjectOptIn,
        MainSceneUse,
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
    };

    inline constexpr std::string_view kEngineArcadePackageId = "engine_arcade";
    inline constexpr std::string_view kEngineArcadeSceneId = "engine_arcade_scene";
    inline constexpr std::string_view kEngineForestFactoryPackageId = epoch::forest::kForestFactoryPackageId;

    inline constexpr std::array<PackageDescriptor, 7> kKnownPackages{{
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
            .id = "research_voxel_planetoid_vulkan",
            .displayName = "Research Voxel Planetoid Vulkan",
            .summary = "Local Vulkan voxel/chunk prototype snapshot staged for reviewed package import.",
            .kind = PackageKind::ResearchPrototype,
            .activation = ActivationMode::ManualResearchImport,
        },
        {
            .id = "research_almond_voxel_authoring",
            .displayName = "Research Almond Voxel Authoring",
            .summary = "External voxel authoring/toolkit prototype staged for API-boundary review.",
            .kind = PackageKind::ResearchPrototype,
            .activation = ActivationMode::ManualResearchImport,
        },
        {
            .id = "research_plant_lsystem_wicked_2ol",
            .displayName = "Research Plant L-System Wicked 2OL",
            .summary = "WickedEngine plant/L-system lab staged as reference material only.",
            .kind = PackageKind::ResearchPrototype,
            .activation = ActivationMode::ManualResearchImport,
        },
        {
            .id = "research_planetary_terrain",
            .displayName = "Research Planetary Terrain",
            .summary = "NMS-like planetary and multi-terrain stacks remain package-gated.",
            .kind = PackageKind::DownloadableSource,
            .activation = ActivationMode::DownloadedOptIn,
        },
        {
            .id = "research_fft_ocean",
            .displayName = "Research FFT Ocean",
            .summary = "Ocean simulation package candidate; not part of the minimal engine clone.",
            .kind = PackageKind::DownloadableSource,
            .activation = ActivationMode::DownloadedOptIn,
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
}
