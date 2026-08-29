/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include "../include/core.stl_types.hpp"

export module forest.factory;

import authoring.morphology;
import voxel.field;

export namespace epochengine::forest
{
    inline constexpr std::string_view kForestFactoryPackageId = "engine_forest_factory";
    inline constexpr std::string_view kPlantLabWorkspace = "Plant Lab";
    inline constexpr std::string_view kForestFactoryWorkspace = "Forest Factory Placement Portal";
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

    enum class ForestProfileProperty : std::uint8_t
    {
        TargetHeightMeters,
        TrunkRadiusMeters,
        BranchLevels,
        ChildrenPerNode,
        BranchStartHeightMeters,
        BranchLengthMeters,
        BranchAngleDegrees,
        BranchSpreadDegrees,
        GrowthDurationSeconds,
        GrowthTimeSeconds
    };

    struct ForestProfileEdit
    {
        std::uint64_t sequence{};
        ForestProfileProperty property{ForestProfileProperty::TargetHeightMeters};
        float beforeValue{};
        float afterValue{};
    };

    struct ForestAssetDocument
    {
        ForestGenomeId genome{};
        std::string name{"Untitled Plant"};
        ForestFactoryProfile profile{};
        std::uint64_t revision{1u};
        std::uint64_t contentHash{};
        std::uint64_t nextSequence{1u};
        std::vector<ForestProfileEdit> journal{};
        std::size_t historyCursor{};
    };

    struct ForestPreviewStats
    {
        std::uint32_t nodes{};
        std::uint32_t branches{};
        std::uint32_t leaves{};
        std::uint32_t vertices{};
        std::uint32_t triangles{};
    };

    inline constexpr std::size_t kForestPreviewMaxSegments = 256u;
    inline constexpr std::size_t kForestPreviewMaxLeaves = 384u;

    struct ForestPreviewSegment
    {
        epochengine::voxel::Float3 start{};
        epochengine::voxel::Float3 end{};
        float radius{0.08F};
        std::uint32_t depth{};
    };

    struct ForestPreviewLeaf
    {
        epochengine::voxel::Float3 position{};
        float size{0.24F};
        std::uint32_t sourceSegment{};
    };

    struct ForestPreviewGeometry
    {
        std::array<ForestPreviewSegment, kForestPreviewMaxSegments> segments{};
        std::size_t segmentCount{};
        std::array<ForestPreviewLeaf, kForestPreviewMaxLeaves> leaves{};
        std::size_t leafCount{};
        ForestPreviewStats stats{};
    };

    struct ForestVoxelOccupancySummary
    {
        epochengine::voxel::ChunkDesc chunk{};
        epochengine::voxel::CellSemantic semantics{
            epochengine::voxel::CellSemantic::Geometry |
            epochengine::voxel::CellSemantic::Lighting |
            epochengine::voxel::CellSemantic::Navigation |
            epochengine::voxel::CellSemantic::Visibility |
            epochengine::voxel::CellSemantic::ProceduralVegetation};
        std::array<float, 3> boundsMinMeters{};
        std::array<float, 3> boundsMaxMeters{};
        std::uint64_t trunkCells{};
        std::uint64_t branchCells{};
        std::uint64_t foliageCells{};
        std::uint64_t activeCells{};
        std::uint64_t denseBytes{};
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
        epochengine::voxel::Float3 position{};
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
            profile.branch.branchesPerNode = 3.0f;
            profile.branch.angleDegrees = 34.0F;
            profile.branch.spreadDegrees = 270.0F;
            profile.branch.outwardBias = 0.82F;
            profile.config.targetHeightMeters = 1.6F;
            break;
        case ForestPreset::Fern:
            profile.branch.levels = 2;
            profile.branch.childrenPerNode = 5;
            profile.branch.branchesPerNode = 5.0f;
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

    using MorphologyGraph = epochengine::authoring::morphology::Graph;
    using MorphologySample = epochengine::authoring::morphology::Sample;
    using MorphologyVoxelLodPlan = epochengine::authoring::morphology::VoxelLodPlan;

    struct CompiledForestAsset
    {
        ForestGenomeId genome{};
        std::uint64_t sourceRevision{};
        std::uint64_t sourceContentHash{};
        MorphologyGraph graph{};
        MorphologySample sample{};
        MorphologyVoxelLodPlan voxelLods{};
        ForestPreviewGeometry preview{};
        ForestVoxelOccupancySummary voxelOccupancy{};
        bool valid{};
    };

    inline constexpr std::size_t kForestMaximumProfileEdits = 512u;

    [[nodiscard]] constexpr std::uint64_t mix_seed(
        std::uint64_t state,
        std::uint64_t value) noexcept
    {
        state ^= value + 0x9e3779b97f4a7c15ull + (state << 6u) + (state >> 2u);
        return state;
    }

    [[nodiscard]] constexpr std::uint64_t forest_name_hash(
        std::string_view name) noexcept
    {
        std::uint64_t hash = 1469598103934665603ull;
        for (const char character : name)
        {
            hash ^= static_cast<std::uint8_t>(character);
            hash *= 1099511628211ull;
        }
        return hash == 0u ? 1u : hash;
    }

    [[nodiscard]] inline std::uint64_t forest_float_bits(float value) noexcept
    {
        return static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(value));
    }

    [[nodiscard]] inline std::uint64_t forest_profile_content_hash(
        const ForestFactoryProfile& profile) noexcept
    {
        std::uint64_t hash = 0x45504f4348464f52ull;
        const auto add_float = [&](float value) noexcept
        {
            hash = mix_seed(hash, forest_float_bits(value));
        };
        hash = mix_seed(hash, static_cast<std::uint64_t>(profile.preset));
        hash = mix_seed(hash, static_cast<std::uint64_t>(profile.previewMode));
        hash = mix_seed(hash, static_cast<std::uint64_t>(profile.editStage));
        hash = mix_seed(hash, profile.seed.value);
        add_float(profile.temporal.timeSeconds);
        add_float(profile.temporal.speed);
        add_float(profile.temporal.durationSeconds);
        hash = mix_seed(hash, profile.temporal.playing ? 1u : 0u);
        hash = mix_seed(hash, profile.temporal.reverse ? 1u : 0u);
        add_float(profile.branch.branchesPerNode);
        add_float(profile.branch.nodeStepMeters);
        add_float(profile.branch.startHeightMeters);
        add_float(profile.branch.branchLengthMeters);
        hash = mix_seed(hash, profile.branch.levels);
        hash = mix_seed(hash, profile.branch.childrenPerNode);
        add_float(profile.branch.angleDegrees);
        add_float(profile.branch.spreadDegrees);
        add_float(profile.branch.twistDegrees);
        add_float(profile.branch.jitterDegrees);
        add_float(profile.branch.upwardBend);
        add_float(profile.branch.outwardBias);
        add_float(profile.branch.curve);
        add_float(profile.branch.sag);
        hash = mix_seed(hash, profile.config.maxBranchDepth);
        hash = mix_seed(hash, profile.config.maxPreviewSegments);
        add_float(profile.config.trunkRadiusMeters);
        add_float(profile.config.targetHeightMeters);
        hash = mix_seed(hash, profile.config.deterministicWindProfile ? 1u : 0u);
        hash = mix_seed(hash, profile.config.emitVoxelOccupancy ? 1u : 0u);
        hash = mix_seed(hash, profile.atlasIndex);
        return hash == 0u ? 1u : hash;
    }

    [[nodiscard]] inline float forest_profile_value(
        const ForestFactoryProfile& profile,
        ForestProfileProperty property) noexcept
    {
        switch (property)
        {
        case ForestProfileProperty::TargetHeightMeters: return profile.config.targetHeightMeters;
        case ForestProfileProperty::TrunkRadiusMeters: return profile.config.trunkRadiusMeters;
        case ForestProfileProperty::BranchLevels: return static_cast<float>(profile.branch.levels);
        case ForestProfileProperty::ChildrenPerNode: return static_cast<float>(profile.branch.childrenPerNode);
        case ForestProfileProperty::BranchStartHeightMeters: return profile.branch.startHeightMeters;
        case ForestProfileProperty::BranchLengthMeters: return profile.branch.branchLengthMeters;
        case ForestProfileProperty::BranchAngleDegrees: return profile.branch.angleDegrees;
        case ForestProfileProperty::BranchSpreadDegrees: return profile.branch.spreadDegrees;
        case ForestProfileProperty::GrowthDurationSeconds: return profile.temporal.durationSeconds;
        case ForestProfileProperty::GrowthTimeSeconds: return profile.temporal.timeSeconds;
        }
        return 0.0f;
    }

    inline void set_forest_profile_value(
        ForestFactoryProfile& profile,
        ForestProfileProperty property,
        float value) noexcept
    {
        switch (property)
        {
        case ForestProfileProperty::TargetHeightMeters:
            profile.config.targetHeightMeters = (std::clamp)(value, 0.1f, 256.0f);
            break;
        case ForestProfileProperty::TrunkRadiusMeters:
            profile.config.trunkRadiusMeters = (std::clamp)(value, 0.005f, 8.0f);
            break;
        case ForestProfileProperty::BranchLevels:
            profile.branch.levels = static_cast<std::uint32_t>((std::clamp)(
                std::round(value), 1.0f, 32.0f));
            break;
        case ForestProfileProperty::ChildrenPerNode:
            profile.branch.childrenPerNode = static_cast<std::uint32_t>((std::clamp)(
                std::round(value), 1.0f, 8.0f));
            break;
        case ForestProfileProperty::BranchStartHeightMeters:
            profile.branch.startHeightMeters = (std::clamp)(value, 0.0f, 128.0f);
            break;
        case ForestProfileProperty::BranchLengthMeters:
            profile.branch.branchLengthMeters = (std::clamp)(value, 0.01f, 128.0f);
            break;
        case ForestProfileProperty::BranchAngleDegrees:
            profile.branch.angleDegrees = (std::clamp)(value, 0.0f, 180.0f);
            break;
        case ForestProfileProperty::BranchSpreadDegrees:
            profile.branch.spreadDegrees = (std::clamp)(value, 0.0f, 360.0f);
            break;
        case ForestProfileProperty::GrowthDurationSeconds:
            profile.temporal.durationSeconds = (std::clamp)(value, 0.001f, 86'400.0f);
            profile.temporal.timeSeconds = (std::clamp)(
                profile.temporal.timeSeconds, 0.0f, profile.temporal.durationSeconds);
            break;
        case ForestProfileProperty::GrowthTimeSeconds:
            profile.temporal.timeSeconds = (std::clamp)(
                value, 0.0f, (std::max)(profile.temporal.durationSeconds, 0.001f));
            break;
        }
    }

    [[nodiscard]] inline ForestAssetDocument make_forest_asset_document(
        std::string_view name,
        ForestPreset preset = ForestPreset::Tree)
    {
        ForestAssetDocument document{};
        document.name.assign(name.empty() ? std::string_view{"Untitled Plant"} : name);
        document.profile = default_profile(preset);
        document.genome.stableHash = mix_seed(
            forest_name_hash(document.name),
            mix_seed(static_cast<std::uint64_t>(preset), document.profile.seed.value));
        if (document.genome.stableHash == 0u)
            document.genome.stableHash = 1u;
        document.contentHash = forest_profile_content_hash(document.profile);
        return document;
    }

    [[nodiscard]] inline bool apply_forest_profile_edit(
        ForestAssetDocument& document,
        ForestProfileProperty property,
        float requestedValue)
    {
        if (!std::isfinite(requestedValue))
            return false;

        ForestFactoryProfile candidate = document.profile;
        const float before = forest_profile_value(candidate, property);
        set_forest_profile_value(candidate, property, requestedValue);
        const float after = forest_profile_value(candidate, property);
        if (before == after)
            return false;

        if (document.historyCursor < document.journal.size())
        {
            document.journal.erase(
                document.journal.begin() + static_cast<std::ptrdiff_t>(document.historyCursor),
                document.journal.end());
        }
        if (document.journal.size() >= kForestMaximumProfileEdits)
        {
            document.journal.erase(document.journal.begin());
            if (document.historyCursor > 0u)
                --document.historyCursor;
        }

        document.profile = candidate;
        document.journal.push_back(ForestProfileEdit{
            .sequence = document.nextSequence++,
            .property = property,
            .beforeValue = before,
            .afterValue = after});
        document.historyCursor = document.journal.size();
        ++document.revision;
        document.contentHash = forest_profile_content_hash(document.profile);
        return true;
    }

    [[nodiscard]] inline bool undo_forest_profile_edit(ForestAssetDocument& document)
    {
        if (document.historyCursor == 0u)
            return false;
        const ForestProfileEdit& edit = document.journal[document.historyCursor - 1u];
        set_forest_profile_value(document.profile, edit.property, edit.beforeValue);
        --document.historyCursor;
        ++document.revision;
        document.contentHash = forest_profile_content_hash(document.profile);
        return true;
    }

    [[nodiscard]] inline bool redo_forest_profile_edit(ForestAssetDocument& document)
    {
        if (document.historyCursor >= document.journal.size())
            return false;
        const ForestProfileEdit& edit = document.journal[document.historyCursor];
        set_forest_profile_value(document.profile, edit.property, edit.afterValue);
        ++document.historyCursor;
        ++document.revision;
        document.contentHash = forest_profile_content_hash(document.profile);
        return true;
    }

    [[nodiscard]] inline ForestAssetDocument make_default_plant_lab_document()
    {
        auto document = make_forest_asset_document(
            "Plant Lab Default Tree", ForestPreset::Tree);
        (void)apply_forest_profile_edit(
            document, ForestProfileProperty::TargetHeightMeters, 4.2f);
        (void)apply_forest_profile_edit(
            document, ForestProfileProperty::TrunkRadiusMeters, 0.09f);
        (void)apply_forest_profile_edit(
            document, ForestProfileProperty::BranchLevels, 4.0f);
        (void)apply_forest_profile_edit(
            document, ForestProfileProperty::BranchStartHeightMeters, 0.24f);
        (void)apply_forest_profile_edit(
            document, ForestProfileProperty::BranchLengthMeters, 0.92f);
        return document;
    }

    [[nodiscard]] inline epochengine::authoring::morphology::Recipe morphology_recipe(
        const ForestFactoryProfile& profile) noexcept
    {
        namespace morphology = epochengine::authoring::morphology;
        auto recipe = morphology::default_recipe(morphology::Domain::plant);
        recipe.dimension = profile.previewMode == ForestPreviewMode::Mode2D
            ? morphology::DimensionMode::planar_2d
            : morphology::DimensionMode::spatial_3d;
        recipe.duration_seconds = (std::max)(0.001f, profile.temporal.durationSeconds);
        recipe.growth.seed = profile.seed.value;
        recipe.growth.generations = (std::max)(1u,
            (std::min)(profile.branch.levels, profile.config.maxBranchDepth));
        const std::uint32_t targetBranchesPerNode = static_cast<std::uint32_t>(
            (std::clamp)(std::round(profile.branch.branchesPerNode), 1.0f, 8.0f));
        recipe.growth.children_per_node = (std::max)(1u,
            (std::min)(profile.branch.childrenPerNode, targetBranchesPerNode));
        recipe.growth.length_decay = (std::clamp)(
            0.58f + profile.branch.curve * 0.24f,
            0.45f,
            0.94f);
        float branchLengthScale = 0.0f;
        float generationScale = 1.0f;
        for (std::uint32_t generation = 0u;
            generation < recipe.growth.generations;
            ++generation)
        {
            branchLengthScale += generationScale;
            generationScale *= recipe.growth.length_decay;
        }
        const float authoredNodeStep = (std::max)(0.01f,
            (std::min)(
                profile.branch.nodeStepMeters,
                profile.branch.branchLengthMeters));
        const float authoredAxialHeight = (std::max)(
            0.01f,
            profile.branch.startHeightMeters +
                authoredNodeStep * branchLengthScale);
        const float targetScale = profile.config.targetHeightMeters /
            authoredAxialHeight;
        recipe.growth.root_length_meters = (std::max)(
            0.01f,
            profile.branch.startHeightMeters * targetScale);
        recipe.growth.segment_length_meters = authoredNodeStep * targetScale;
        recipe.growth.root_radius_meters = profile.config.trunkRadiusMeters;
        recipe.growth.radius_decay = 0.72f;
        recipe.growth.branch_angle_degrees = profile.branch.angleDegrees;
        recipe.growth.spread_degrees = profile.branch.spreadDegrees;
        recipe.growth.twist_degrees = profile.branch.twistDegrees;
        recipe.growth.jitter_degrees = profile.branch.jitterDegrees;
        recipe.growth.upward_bias = profile.branch.upwardBend + 0.52f;
        recipe.growth.outward_bias = profile.branch.outwardBias;
        recipe.growth.sag = profile.branch.sag;
        recipe.growth.sapling_pre_age_seconds = recipe.duration_seconds * 0.04f;
        recipe.growth.generation_delay_seconds = recipe.duration_seconds * 0.54f /
            static_cast<float>(recipe.growth.generations + 1u);
        recipe.growth.segment_growth_seconds = recipe.duration_seconds * 0.32f;
        recipe.growth.terminal_delay_seconds = recipe.duration_seconds * 0.02f;
        recipe.growth.terminal_growth_seconds = recipe.duration_seconds * 0.12f;
        recipe.limits.maximum_depth = profile.config.maxBranchDepth;
        recipe.limits.maximum_children_per_node = 8u;
        recipe.limits.maximum_nodes = (std::max)(2u,
            (std::min)(profile.config.maxPreviewSegments + 1u, 16'384u));
        recipe.limits.maximum_segments = (std::max)(1u,
            (std::min)(profile.config.maxPreviewSegments, 16'383u));
        recipe.limits.maximum_terminals =
            static_cast<std::uint32_t>(kForestPreviewMaxLeaves);
        return recipe;
    }

    [[nodiscard]] inline MorphologyGraph build_morphology_graph(
        const ForestFactoryProfile& profile)
    {
        return epochengine::authoring::morphology::build_graph(
            morphology_recipe(profile));
    }

    [[nodiscard]] inline MorphologySample sample_morphology_graph(
        const ForestFactoryProfile& profile,
        const MorphologyGraph& graph)
    {
        const float duration = (std::max)(0.001f, profile.temporal.durationSeconds);
        const float sampleTime = profile.temporal.reverse
            ? duration - (std::clamp)(profile.temporal.timeSeconds, 0.0f, duration)
            : profile.temporal.timeSeconds;
        return epochengine::authoring::morphology::sample(graph, sampleTime);
    }

    [[nodiscard]] inline MorphologyVoxelLodPlan plan_morphology_lods(
        const MorphologyGraph& graph,
        epochengine::voxel::LodPolicy policy = {},
        float baseCellSizeMeters = 0.05f,
        std::uint8_t levelCount = 5u)
    {
        return epochengine::authoring::morphology::plan_voxel_lods(
            graph,
            policy,
            baseCellSizeMeters,
            levelCount);
    }
    [[nodiscard]] constexpr bool valid(const ForestFactoryConfig& config) noexcept
    {
        return config.maxBranchDepth > 0u &&
               config.maxPreviewSegments > 0u &&
               config.trunkRadiusMeters > 0.0F &&
               config.targetHeightMeters > 0.0F;
    }

    [[nodiscard]] inline float clamp01(float value) noexcept
    {
        return (std::clamp)(value, 0.0F, 1.0F);
    }

    [[nodiscard]] inline float degrees_to_radians(float degrees) noexcept
    {
        return degrees * 0.017453292519943295769F;
    }

    [[nodiscard]] inline epochengine::voxel::Float3 add(epochengine::voxel::Float3 lhs, epochengine::voxel::Float3 rhs) noexcept
    {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    [[nodiscard]] inline epochengine::voxel::Float3 scale(epochengine::voxel::Float3 value, float amount) noexcept
    {
        return { value.x * amount, value.y * amount, value.z * amount };
    }

    [[nodiscard]] inline epochengine::voxel::Float3 normalize(epochengine::voxel::Float3 value) noexcept
    {
        const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
        if (length <= 0.0001F)
            return { 0.0F, 1.0F, 0.0F };
        const float inv = 1.0F / length;
        return { value.x * inv, value.y * inv, value.z * inv };
    }

    [[nodiscard]] inline float deterministic_jitter(ForestSeed seed, std::uint32_t depth, std::uint32_t child) noexcept
    {
        std::uint64_t value = seed.value ^ (static_cast<std::uint64_t>(depth + 1u) * 0x9E3779B97F4A7C15ull);
        value ^= static_cast<std::uint64_t>(child + 17u) * 0xBF58476D1CE4E5B9ull;
        value ^= value >> 30u;
        value *= 0xBF58476D1CE4E5B9ull;
        value ^= value >> 27u;
        value *= 0x94D049BB133111EBull;
        value ^= value >> 31u;
        const auto unit = static_cast<float>(value & 0xFFFFu) / 65535.0F;
        return unit * 2.0F - 1.0F;
    }

    [[nodiscard]] constexpr ForestPreviewStats estimate_preview_stats(const ForestFactoryProfile& profile) noexcept
    {
        std::uint32_t nodes = 1;
        std::uint32_t frontier = 1;
        const auto levels = profile.branch.levels > profile.config.maxBranchDepth
            ? profile.config.maxBranchDepth
            : profile.branch.levels;
        const auto density = static_cast<std::uint32_t>((std::clamp)(
            std::round(profile.branch.branchesPerNode), 1.0f, 8.0f));
        const auto children = profile.branch.childrenPerNode == 0u
            ? 1u
            : (std::min)(profile.branch.childrenPerNode, density);

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

    [[nodiscard]] inline ForestPreviewGeometry build_preview_geometry(
        const ForestFactoryProfile& profile,
        const MorphologySample& sample)
    {
        ForestPreviewGeometry geometry{};
        for (const auto& segment : sample.segments)
        {
            if (geometry.segmentCount >= geometry.segments.size())
                break;
            geometry.segments[geometry.segmentCount++] = ForestPreviewSegment{
                .start = segment.start,
                .end = segment.end,
                .radius = (std::max)(
                    segment.radius_start_meters,
                    segment.radius_end_meters),
                .depth = segment.depth};
        }

        const float presetLeafScale = profile.preset == ForestPreset::Fern
            ? 0.34f
            : (profile.preset == ForestPreset::Bush ? 0.28f : 0.24f);
        const std::uint32_t leafCopies =
            profile.preset == ForestPreset::Fern ? 3u : 1u;
        for (const auto& terminal : sample.terminals)
        {
            for (std::uint32_t copy = 0u;
                copy < leafCopies && geometry.leafCount < geometry.leaves.size();
                ++copy)
            {
                const float offset =
                    (static_cast<float>(copy) -
                        static_cast<float>(leafCopies - 1u) * 0.5f) * 0.08f;
                geometry.leaves[geometry.leafCount++] = ForestPreviewLeaf{
                    .position = add(terminal.position, {
                        offset,
                        0.02f * static_cast<float>(copy),
                        -offset}),
                    .size = (std::max)(presetLeafScale, terminal.scale_meters),
                    .sourceSegment = geometry.segmentCount == 0u
                        ? 0u
                        : (std::min)(terminal.source.index,
                            static_cast<std::uint32_t>(geometry.segmentCount - 1u))};
            }
        }

        geometry.stats = ForestPreviewStats{
            .nodes = static_cast<std::uint32_t>(geometry.segmentCount + 1u),
            .branches = static_cast<std::uint32_t>(
                geometry.segmentCount > 0u ? geometry.segmentCount - 1u : 0u),
            .leaves = static_cast<std::uint32_t>(geometry.leafCount),
            .vertices = static_cast<std::uint32_t>(
                geometry.segmentCount * 12u + geometry.leafCount * 6u),
            .triangles = static_cast<std::uint32_t>(
                geometry.segmentCount * 8u + geometry.leafCount * 2u)};
        return geometry;
    }

    [[nodiscard]] inline ForestPreviewGeometry build_preview_geometry(
        const ForestFactoryProfile& profile)
    {
        const MorphologyGraph graph = build_morphology_graph(profile);
        if (!epochengine::authoring::morphology::validate(graph))
            return {};
        const MorphologySample sample = sample_morphology_graph(profile, graph);
        return build_preview_geometry(profile, sample);
    }

    [[nodiscard]] inline ForestVoxelOccupancySummary estimate_voxel_occupancy(
        const ForestFactoryProfile& profile,
        const ForestPreviewGeometry& geometry,
        float requestedCellSizeMeters = 0.20F) noexcept
    {
        const float cellSizeMeters = (std::clamp)(requestedCellSizeMeters, 0.05F, 2.0F);
        const float margin = (std::max)(profile.config.trunkRadiusMeters * 4.0F, cellSizeMeters * 2.0F);
        epochengine::voxel::Float3 minPoint{ -margin, 0.0F, -margin };
        epochengine::voxel::Float3 maxPoint{ margin, margin, margin };

        auto include_point = [&](epochengine::voxel::Float3 point, float radius) noexcept
        {
            const float padded = (std::max)(radius, cellSizeMeters);
            minPoint.x = (std::min)(minPoint.x, point.x - padded);
            minPoint.y = (std::min)(minPoint.y, point.y - padded);
            minPoint.z = (std::min)(minPoint.z, point.z - padded);
            maxPoint.x = (std::max)(maxPoint.x, point.x + padded);
            maxPoint.y = (std::max)(maxPoint.y, point.y + padded);
            maxPoint.z = (std::max)(maxPoint.z, point.z + padded);
        };

        std::uint64_t trunkCells = 0u;
        std::uint64_t branchCells = 0u;
        for (std::size_t i = 0u; i < geometry.segmentCount; ++i)
        {
            const auto& segment = geometry.segments[i];
            include_point(segment.start, segment.radius);
            include_point(segment.end, segment.radius);

            const float dx = segment.end.x - segment.start.x;
            const float dy = segment.end.y - segment.start.y;
            const float dz = segment.end.z - segment.start.z;
            const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
            const auto lengthCells = static_cast<std::uint64_t>((std::max)(1.0F, std::ceil(length / cellSizeMeters)));
            const auto radiusCells = static_cast<std::uint64_t>((std::max)(1.0F, std::ceil(segment.radius / cellSizeMeters)));
            const std::uint64_t estimatedCells = lengthCells * radiusCells * radiusCells;
            if (i == 0u)
                trunkCells += estimatedCells;
            else
                branchCells += estimatedCells;
        }

        std::uint64_t foliageCells = 0u;
        for (std::size_t i = 0u; i < geometry.leafCount; ++i)
        {
            const auto& leaf = geometry.leaves[i];
            include_point(leaf.position, leaf.size);
            const auto leafCells = static_cast<std::uint64_t>((std::max)(1.0F, std::ceil(leaf.size / cellSizeMeters)));
            foliageCells += leafCells * leafCells;
        }

        auto cells_for_extent = [&](float extent) noexcept -> std::uint32_t
        {
            const auto cells = static_cast<std::uint32_t>((std::max)(4.0F, std::ceil(extent / cellSizeMeters)));
            return (std::clamp)(cells, 4u, 128u);
        };

        ForestVoxelOccupancySummary summary{};
        summary.chunk.cellSizeMeters = cellSizeMeters;
        summary.chunk.cellsX = cells_for_extent(maxPoint.x - minPoint.x);
        summary.chunk.cellsY = cells_for_extent(maxPoint.y - minPoint.y);
        summary.chunk.cellsZ = cells_for_extent(maxPoint.z - minPoint.z);
        summary.chunk.lodLevel = 0u;
        summary.boundsMinMeters = { minPoint.x, minPoint.y, minPoint.z };
        summary.boundsMaxMeters = { maxPoint.x, maxPoint.y, maxPoint.z };
        summary.trunkCells = trunkCells;
        summary.branchCells = branchCells;
        summary.foliageCells = foliageCells;
        summary.activeCells = (std::min)(
            trunkCells + branchCells + foliageCells,
            epochengine::voxel::dense_cell_count(summary.chunk));
        summary.denseBytes = epochengine::voxel::dense_cell_bytes(summary.chunk);
        return summary;
    }

    [[nodiscard]] inline CompiledForestAsset compile_forest_asset(
        const ForestAssetDocument& document,
        epochengine::voxel::LodPolicy lodPolicy = {},
        float baseCellSizeMeters = 0.05f,
        std::uint8_t lodLevelCount = 5u)
    {
        CompiledForestAsset asset{};
        asset.genome = document.genome;
        asset.sourceRevision = document.revision;
        asset.sourceContentHash = document.contentHash;
        asset.graph = build_morphology_graph(document.profile);
        if (!epochengine::authoring::morphology::validate(asset.graph))
            return asset;
        asset.sample = sample_morphology_graph(document.profile, asset.graph);
        asset.preview = build_preview_geometry(document.profile, asset.sample);
        asset.voxelLods = plan_morphology_lods(
            asset.graph, lodPolicy, baseCellSizeMeters, lodLevelCount);
        asset.voxelOccupancy = estimate_voxel_occupancy(
            document.profile, asset.preview, baseCellSizeMeters);
        asset.valid = asset.sourceContentHash != 0u &&
            asset.graph.content_hash != 0u &&
            !asset.sample.segments.empty() &&
            asset.preview.segmentCount > 0u &&
            !asset.voxelLods.levels.empty();
        return asset;
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
