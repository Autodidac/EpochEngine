module;

#include <cstdint>
#include <string_view>

#include "../include/_epoch.stl_types.hpp"

export module forest.factory;

import voxel.field;

export namespace epoch::forest
{
    inline constexpr std::string_view kForestFactoryPackageId = "engine_forest_factory";
    inline constexpr std::string_view kForestFactoryWorkspace = "Forest Factory";

    enum class ForestOutputKind : std::uint8_t
    {
        PreviewSkeleton,
        MeshLod,
        Impostor,
        VoxelOccupancy,
        SeedAsset
    };

    enum class ForestActivationMode : std::uint8_t
    {
        EditorPreviewOnly,
        ProjectSceneOptIn,
        RuntimePackageOptIn
    };

    struct ForestGenomeId
    {
        std::uint64_t stableHash{};
    };

    struct ForestSeed
    {
        std::uint64_t value{1};
    };

    struct ForestFactoryConfig
    {
        std::uint32_t maxBranchDepth{8};
        std::uint32_t maxPreviewSegments{4096};
        float trunkRadiusMeters{0.12F};
        float targetHeightMeters{6.0F};
        bool deterministicWindProfile{true};
        bool emitVoxelOccupancy{true};
    };

    struct ForestLodRequest
    {
        ForestOutputKind output{ForestOutputKind::PreviewSkeleton};
        std::uint8_t lod{};
        float viewDistanceMeters{};
    };

    struct ForestInstanceDesc
    {
        ForestGenomeId genome{};
        ForestSeed seed{};
        epoch::voxel::Float3 position{};
        float uniformScale{1.0F};
    };

    struct ForestActivationPolicy
    {
        ForestActivationMode mode{ForestActivationMode::EditorPreviewOnly};
        bool includeInGeneratedProject{};
        bool emitPackageManifest{};
        bool attachToMainScene{};
    };

    [[nodiscard]] constexpr ForestFactoryConfig default_config() noexcept
    {
        return {};
    }

    [[nodiscard]] constexpr bool valid(const ForestFactoryConfig& config) noexcept
    {
        return config.maxBranchDepth > 0u &&
               config.maxPreviewSegments > 0u &&
               config.trunkRadiusMeters > 0.0F &&
               config.targetHeightMeters > 0.0F;
    }

    [[nodiscard]] constexpr bool requires_project_activation(ForestOutputKind output) noexcept
    {
        return output == ForestOutputKind::MeshLod ||
               output == ForestOutputKind::Impostor ||
               output == ForestOutputKind::VoxelOccupancy ||
               output == ForestOutputKind::SeedAsset;
    }

    [[nodiscard]] constexpr ForestActivationPolicy activation_for_scene_use() noexcept
    {
        return {
            .mode = ForestActivationMode::ProjectSceneOptIn,
            .includeInGeneratedProject = true,
            .emitPackageManifest = true,
            .attachToMainScene = true};
    }

    [[nodiscard]] constexpr ForestActivationPolicy activation_for_editor_preview() noexcept
    {
        return {};
    }

    [[nodiscard]] constexpr std::uint64_t mix_seed(std::uint64_t state, std::uint64_t value) noexcept
    {
        state ^= value + 0x9e3779b97f4a7c15ull + (state << 6u) + (state >> 2u);
        return state;
    }

    [[nodiscard]] constexpr std::uint32_t encode_axis_millimeters(float meters) noexcept
    {
        constexpr float kMinMeters = -2147483.648F;
        constexpr float kMaxMeters = 2147483.647F;

        if (meters <= kMinMeters)
        {
            return 0x80000000u;
        }

        if (meters >= kMaxMeters)
        {
            return 0x7fffffffu;
        }

        const auto millimeters = static_cast<std::int32_t>(meters * 1000.0F);
        return static_cast<std::uint32_t>(millimeters);
    }

    [[nodiscard]] constexpr std::uint64_t stable_instance_key(const ForestInstanceDesc& instance) noexcept
    {
        auto key = mix_seed(instance.genome.stableHash, instance.seed.value);
        key = mix_seed(key, static_cast<std::uint64_t>(encode_axis_millimeters(instance.position.x)));
        key = mix_seed(key, static_cast<std::uint64_t>(encode_axis_millimeters(instance.position.y)));
        key = mix_seed(key, static_cast<std::uint64_t>(encode_axis_millimeters(instance.position.z)));
        return key;
    }
}
