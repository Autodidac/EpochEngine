module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../include/_epoch.stl_types.hpp"

export module forest.factory;

import voxel.field;

export namespace epoch::forest
{
    inline constexpr std::string_view kForestFactoryPackageId = "engine_forest_factory";
    inline constexpr std::string_view kForestFactoryWorkspace = "Forest Factory";
    inline constexpr std::string_view kForestFactoryTechnique = "Temporal graph / parametric L-system";
    inline constexpr std::string_view kForestFactoryReferenceRepo =
        "https://github.com/Autodidac/Temporal_Parametric_Graph_Lindenmayer_System_Plant_Lab";

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

    enum class ForestPreset : std::uint8_t
    {
        Cannabis,
        Tree,
        Bush,
        Fern
    };

    enum class ForestPreviewMode : std::uint8_t
    {
        Mode2D,
        Mode3D
    };

    enum class ForestEditStage : std::uint8_t
    {
        Stage,
        Structure,
        Branch,
        Foliage
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

    struct ForestTemporalProfile
    {
        float timeSeconds{28.0F};
        float speed{8.0F};
        float durationSeconds{28.0F};
        bool playing{};
        bool reverse{};
    };

    struct ForestBranchProfile
    {
        float branchesPerNode{2.0F};
        float nodeStepMeters{1.0F};
        float startHeightMeters{0.16F};
        float branchLengthMeters{0.64F};
        std::uint32_t levels{2};
        std::uint32_t childrenPerNode{2};
        float angleDegrees{48.0F};
        float spreadDegrees{360.0F};
        float twistDegrees{137.5F};
        float jitterDegrees{18.0F};
        float upwardBend{0.26F};
        float outwardBias{0.72F};
        float curve{0.66F};
        float sag{0.03F};
    };

    struct ForestDisplayToggles
    {
        bool veins{};
        bool nodes{};
        bool wire{};
        bool texture{true};
    };

    struct ForestFactoryProfile
    {
        ForestPreset preset{ForestPreset::Tree};
        ForestPreviewMode previewMode{ForestPreviewMode::Mode3D};
        ForestEditStage editStage{ForestEditStage::Stage};
        ForestSeed seed{1337};
        ForestTemporalProfile temporal{};
        ForestBranchProfile branch{};
        ForestDisplayToggles display{};
        ForestFactoryConfig config{};
        std::uint32_t atlasIndex{};
    };

    struct ForestPreviewStats
    {
        std::uint32_t nodes{};
        std::uint32_t branches{};
        std::uint32_t leaves{};
        std::uint32_t vertices{};
        std::uint32_t triangles{};
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

    inline constexpr std::array<std::string_view, 4> kForestPresetNames{{
        "Cannabis",
        "Tree",
        "Bush",
        "Fern"
    }};

    inline constexpr std::array<std::string_view, 4> kForestStageNames{{
        "Stage",
        "Structure",
        "Branch",
        "Foliage"
    }};

    [[nodiscard]] constexpr ForestFactoryConfig default_config() noexcept
    {
        return {};
    }

    [[nodiscard]] constexpr std::string_view preset_name(ForestPreset preset) noexcept
    {
        const auto index = static_cast<std::size_t>(preset);
        return index < kForestPresetNames.size() ? kForestPresetNames[index] : std::string_view{ "Unknown" };
    }

    [[nodiscard]] constexpr std::string_view stage_name(ForestEditStage stage) noexcept
    {
        const auto index = static_cast<std::size_t>(stage);
        return index < kForestStageNames.size() ? kForestStageNames[index] : std::string_view{ "Unknown" };
    }

    [[nodiscard]] constexpr ForestFactoryProfile default_profile(ForestPreset preset = ForestPreset::Tree) noexcept
    {
        ForestFactoryProfile profile{};
        profile.preset = preset;

        switch (preset)
        {
        case ForestPreset::Cannabis:
            profile.branch.levels = 2;
            profile.branch.childrenPerNode = 2;
            profile.branch.angleDegrees = 48.0F;
            profile.branch.spreadDegrees = 360.0F;
            profile.branch.twistDegrees = 137.5F;
            profile.config.targetHeightMeters = 2.1F;
            break;
        case ForestPreset::Bush:
            profile.branch.levels = 3;
            profile.branch.childrenPerNode = 3;
            profile.branch.angleDegrees = 34.0F;
            profile.branch.spreadDegrees = 270.0F;
            profile.branch.outwardBias = 0.82F;
            profile.config.targetHeightMeters = 1.6F;
            break;
        case ForestPreset::Fern:
            profile.branch.levels = 2;
            profile.branch.childrenPerNode = 5;
            profile.branch.angleDegrees = 22.0F;
            profile.branch.spreadDegrees = 180.0F;
            profile.branch.sag = 0.12F;
            profile.config.targetHeightMeters = 1.2F;
            break;
        case ForestPreset::Tree:
        default:
            profile.branch.levels = 5;
            profile.branch.childrenPerNode = 2;
            profile.branch.branchLengthMeters = 1.4F;
            profile.branch.startHeightMeters = 0.35F;
            profile.config.targetHeightMeters = 7.5F;
            break;
        }

        return profile;
    }

    [[nodiscard]] constexpr bool valid(const ForestFactoryConfig& config) noexcept
    {
        return config.maxBranchDepth > 0u &&
               config.maxPreviewSegments > 0u &&
               config.trunkRadiusMeters > 0.0F &&
               config.targetHeightMeters > 0.0F;
    }

    [[nodiscard]] constexpr ForestPreviewStats estimate_preview_stats(const ForestFactoryProfile& profile) noexcept
    {
        std::uint32_t nodes = 1;
        std::uint32_t frontier = 1;
        const auto levels = profile.branch.levels > profile.config.maxBranchDepth
            ? profile.config.maxBranchDepth
            : profile.branch.levels;
        const auto children = profile.branch.childrenPerNode == 0u ? 1u : profile.branch.childrenPerNode;

        for (std::uint32_t level = 0; level < levels; ++level)
        {
            frontier *= children;
            nodes += frontier;
            if (nodes > profile.config.maxPreviewSegments)
            {
                nodes = profile.config.maxPreviewSegments;
                break;
            }
        }

        const std::uint32_t branches = nodes > 0u ? nodes - 1u : 0u;
        const std::uint32_t leaves = (std::max)(frontier, 1u) * (profile.preset == ForestPreset::Fern ? 3u : 1u);
        const std::uint32_t vertices = nodes * 12u + branches * 8u + leaves * 6u;
        const std::uint32_t triangles = branches * 8u + leaves * 2u;

        return {
            .nodes = nodes,
            .branches = branches,
            .leaves = leaves,
            .vertices = vertices,
            .triangles = triangles
        };
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
