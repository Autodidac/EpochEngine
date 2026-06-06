module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
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

    inline constexpr std::size_t kForestPreviewMaxSegments = 256u;
    inline constexpr std::size_t kForestPreviewMaxLeaves = 384u;

    struct ForestPreviewSegment
    {
        epoch::voxel::Float3 start{};
        epoch::voxel::Float3 end{};
        float radius{0.08F};
        std::uint32_t depth{};
    };

    struct ForestPreviewLeaf
    {
        epoch::voxel::Float3 position{};
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
        epoch::voxel::ChunkDesc chunk{};
        epoch::voxel::CellSemantic semantics{
            epoch::voxel::CellSemantic::Geometry |
            epoch::voxel::CellSemantic::Lighting |
            epoch::voxel::CellSemantic::Navigation |
            epoch::voxel::CellSemantic::Visibility |
            epoch::voxel::CellSemantic::ProceduralVegetation};
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

    [[nodiscard]] inline float clamp01(float value) noexcept
    {
        return (std::clamp)(value, 0.0F, 1.0F);
    }

    [[nodiscard]] inline float degrees_to_radians(float degrees) noexcept
    {
        return degrees * 0.017453292519943295769F;
    }

    [[nodiscard]] inline epoch::voxel::Float3 add(epoch::voxel::Float3 lhs, epoch::voxel::Float3 rhs) noexcept
    {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    [[nodiscard]] inline epoch::voxel::Float3 scale(epoch::voxel::Float3 value, float amount) noexcept
    {
        return { value.x * amount, value.y * amount, value.z * amount };
    }

    [[nodiscard]] inline epoch::voxel::Float3 normalize(epoch::voxel::Float3 value) noexcept
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

    [[nodiscard]] inline ForestPreviewGeometry build_preview_geometry(const ForestFactoryProfile& profile) noexcept
    {
        ForestPreviewGeometry geometry{};
        const float duration = (std::max)(0.001F, profile.temporal.durationSeconds);
        const float temporalProgress = profile.temporal.reverse
            ? (1.0F - clamp01(profile.temporal.timeSeconds / duration))
            : clamp01(profile.temporal.timeSeconds / duration);
        const float growth = (std::max)(0.18F, temporalProgress);
        const std::uint32_t levels = (std::min)(profile.branch.levels, profile.config.maxBranchDepth);
        const std::uint32_t children = (std::max)(1u, profile.branch.childrenPerNode);
        const float trunkHeight = profile.config.targetHeightMeters * 0.36F * growth;

        auto add_segment = [&](epoch::voxel::Float3 start, epoch::voxel::Float3 direction, float length, float radius, std::uint32_t depth) noexcept -> std::size_t
        {
            if (geometry.segmentCount >= geometry.segments.size())
                return geometry.segmentCount;

            const auto index = geometry.segmentCount++;
            geometry.segments[index] = ForestPreviewSegment{
                .start = start,
                .end = add(start, scale(normalize(direction), length)),
                .radius = radius,
                .depth = depth
            };
            return index;
        };

        const auto trunkIndex = add_segment({ 0.0F, 0.0F, 0.0F }, { 0.0F, 1.0F, 0.0F }, trunkHeight, profile.config.trunkRadiusMeters, 0u);
        (void)trunkIndex;

        std::array<std::size_t, kForestPreviewMaxSegments> frontier{};
        std::array<std::size_t, kForestPreviewMaxSegments> nextFrontier{};
        std::size_t frontierCount = geometry.segmentCount > 0u ? 1u : 0u;
        frontier[0] = 0u;

        for (std::uint32_t depth = 1u; depth <= levels && frontierCount > 0u; ++depth)
        {
            std::size_t nextCount = 0u;
            const float depthFactor = static_cast<float>(depth) / static_cast<float>((std::max)(1u, levels));
            const float length = profile.branch.branchLengthMeters * growth * (1.0F - depthFactor * 0.42F);
            const float radius = (std::max)(0.025F, profile.config.trunkRadiusMeters * (1.0F - depthFactor * 0.72F));
            const float pitch = degrees_to_radians(profile.branch.angleDegrees + profile.branch.upwardBend * 18.0F);
            const float upward = (std::max)(0.12F, std::sin(pitch) + profile.branch.upwardBend * (1.0F - depthFactor));
            const float outward = (std::max)(0.08F, std::cos(pitch) * profile.branch.outwardBias);

            for (std::size_t parentIndex = 0u; parentIndex < frontierCount; ++parentIndex)
            {
                const auto& parent = geometry.segments[frontier[parentIndex]];
                for (std::uint32_t child = 0u; child < children; ++child)
                {
                    const float childRatio = children > 1u
                        ? static_cast<float>(child) / static_cast<float>(children)
                        : 0.0F;
                    const float yawDegrees =
                        childRatio * profile.branch.spreadDegrees
                        + profile.branch.twistDegrees * static_cast<float>(depth)
                        + deterministic_jitter(profile.seed, depth, child) * profile.branch.jitterDegrees;
                    const float yaw = degrees_to_radians(yawDegrees);
                    const epoch::voxel::Float3 direction = normalize({
                        std::cos(yaw) * outward,
                        upward - profile.branch.sag * depthFactor,
                        std::sin(yaw) * outward
                    });
                    const std::size_t segmentIndex = add_segment(parent.end, direction, length, radius, depth);
                    if (segmentIndex >= geometry.segments.size() || segmentIndex >= geometry.segmentCount)
                        continue;
                    if (nextCount < nextFrontier.size())
                        nextFrontier[nextCount++] = segmentIndex;
                }
            }

            frontier = nextFrontier;
            frontierCount = nextCount;
        }

        const float leafSize = profile.preset == ForestPreset::Fern ? 0.34F : (profile.preset == ForestPreset::Bush ? 0.28F : 0.24F);
        const std::uint32_t leafCopies = profile.preset == ForestPreset::Fern ? 3u : 1u;
        for (std::size_t i = 1u; i < geometry.segmentCount && geometry.leafCount < geometry.leaves.size(); ++i)
        {
            const auto& segment = geometry.segments[i];
            if (segment.depth + 1u < levels)
                continue;

            for (std::uint32_t copy = 0u; copy < leafCopies && geometry.leafCount < geometry.leaves.size(); ++copy)
            {
                const float offset = (static_cast<float>(copy) - static_cast<float>(leafCopies - 1u) * 0.5F) * 0.08F;
                geometry.leaves[geometry.leafCount++] = ForestPreviewLeaf{
                    .position = add(segment.end, { offset, 0.02F * static_cast<float>(copy), -offset }),
                    .size = leafSize,
                    .sourceSegment = static_cast<std::uint32_t>(i)
                };
            }
        }

        geometry.stats = ForestPreviewStats{
            .nodes = static_cast<std::uint32_t>(geometry.segmentCount + 1u),
            .branches = static_cast<std::uint32_t>(geometry.segmentCount > 0u ? geometry.segmentCount - 1u : 0u),
            .leaves = static_cast<std::uint32_t>(geometry.leafCount),
            .vertices = static_cast<std::uint32_t>(geometry.segmentCount * 12u + geometry.leafCount * 6u),
            .triangles = static_cast<std::uint32_t>(geometry.segmentCount * 8u + geometry.leafCount * 2u)
        };
        return geometry;
    }

    [[nodiscard]] inline ForestVoxelOccupancySummary estimate_voxel_occupancy(
        const ForestFactoryProfile& profile,
        const ForestPreviewGeometry& geometry,
        float requestedCellSizeMeters = 0.20F) noexcept
    {
        const float cellSizeMeters = (std::clamp)(requestedCellSizeMeters, 0.05F, 2.0F);
        const float margin = (std::max)(profile.config.trunkRadiusMeters * 4.0F, cellSizeMeters * 2.0F);
        epoch::voxel::Float3 minPoint{ -margin, 0.0F, -margin };
        epoch::voxel::Float3 maxPoint{ margin, margin, margin };

        auto include_point = [&](epoch::voxel::Float3 point, float radius) noexcept
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
            epoch::voxel::dense_cell_count(summary.chunk));
        summary.denseBytes = epoch::voxel::dense_cell_bytes(summary.chunk);
        return summary;
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
