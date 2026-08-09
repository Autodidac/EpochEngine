// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

export module authoring.morphology;

import authoring.document;
import voxel.field;

export namespace epochengine::authoring::morphology
{
    struct NodeTag final {};
    struct SegmentTag final {};
    struct TerminalTag final {};

    using NodeId = Handle<NodeTag, std::uint32_t>;
    using SegmentId = Handle<SegmentTag, std::uint32_t>;
    using TerminalId = Handle<TerminalTag, std::uint32_t>;

    enum class Domain : std::uint8_t
    {
        plant,
        vascular,
        respiratory,
        electrical,
        coral,
        generic
    };

    enum class DimensionMode : std::uint8_t
    {
        spatial_3d,
        planar_2_5d,
        planar_2d,
        pattern_2d
    };

    enum class NodeRole : std::uint8_t
    {
        root,
        conduit,
        junction,
        terminal
    };

    enum class TerminalKind : std::uint8_t
    {
        generic,
        foliage,
        capillary_tip,
        alveolus,
        discharge
    };

    [[nodiscard]] constexpr std::string_view domain_name(Domain domain) noexcept
    {
        switch (domain)
        {
        case Domain::plant: return "plant";
        case Domain::vascular: return "vascular";
        case Domain::respiratory: return "respiratory";
        case Domain::electrical: return "electrical";
        case Domain::coral: return "coral";
        case Domain::generic: return "generic";
        default: return "invalid";
        }
    }

    struct TemporalRange final
    {
        float birth_seconds{};
        float end_seconds{1.0f};
    };

    struct GraphLimits final
    {
        std::uint32_t maximum_depth{12};
        std::uint32_t maximum_children_per_node{8};
        std::uint32_t maximum_nodes{16'384};
        std::uint32_t maximum_segments{16'383};
        std::uint32_t maximum_terminals{16'384};
    };

    struct GrowthParameters final
    {
        std::uint64_t seed{1};
        std::uint32_t generations{5};
        std::uint32_t children_per_node{2};
        float root_length_meters{2.4f};
        float segment_length_meters{1.0f};
        float length_decay{0.74f};
        float root_radius_meters{0.12f};
        float radius_decay{0.72f};
        float branch_angle_degrees{34.0f};
        float spread_degrees{360.0f};
        float twist_degrees{137.5f};
        float jitter_degrees{8.0f};
        float upward_bias{0.72f};
        float outward_bias{0.74f};
        float sag{0.02f};
        float sapling_pre_age_seconds{0.24f};
        float generation_delay_seconds{0.35f};
        float segment_growth_seconds{1.2f};
        float terminal_delay_seconds{0.08f};
        float terminal_growth_seconds{0.8f};
    };

    struct Recipe final
    {
        DocumentHandle document{};
        DocumentRevision revision{};
        Domain domain{Domain::generic};
        DimensionMode dimension{DimensionMode::spatial_3d};
        GrowthParameters growth{};
        GraphLimits limits{};
        float duration_seconds{12.0f};
    };

    struct Node final
    {
        NodeId id{};
        NodeId parent{};
        NodeRole role{NodeRole::conduit};
        std::uint32_t depth{};
        voxel::Float3 position{};
        voxel::Float3 tangent{0.0f, 1.0f, 0.0f};
        float radius_meters{0.02f};
        TemporalRange lifetime{};
    };

    struct Segment final
    {
        SegmentId id{};
        NodeId parent_node{};
        NodeId child_node{};
        voxel::Float3 start{};
        voxel::Float3 end{};
        voxel::Float3 tangent_start{0.0f, 1.0f, 0.0f};
        voxel::Float3 tangent_end{0.0f, 1.0f, 0.0f};
        float radius_start_meters{0.02f};
        float radius_end_meters{0.015f};
        std::uint32_t depth{};
        TemporalRange lifetime{};
    };

    struct Terminal final
    {
        TerminalId id{};
        NodeId node{};
        TerminalKind kind{TerminalKind::generic};
        voxel::Float3 position{};
        voxel::Float3 outward{1.0f, 0.0f, 0.0f};
        voxel::Float3 up{0.0f, 1.0f, 0.0f};
        float scale_meters{0.2f};
        std::uint32_t variant{};
        TemporalRange lifetime{};
    };

    struct Graph final
    {
        Recipe recipe{};
        std::vector<Node> nodes{};
        std::vector<Segment> segments{};
        std::vector<Terminal> terminals{};
        std::uint64_t content_hash{};
        bool truncated{};
    };

    enum class ValidationCode : std::uint8_t
    {
        valid,
        invalid_document,
        invalid_revision,
        invalid_duration,
        invalid_limits,
        invalid_growth,
        invalid_identity,
        invalid_parent,
        invalid_lifetime,
        non_finite_geometry,
        budget_exceeded,
        content_hash_mismatch
    };

    struct ValidationResult final
    {
        ValidationCode code{ValidationCode::valid};
        std::size_t element_index{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ValidationCode::valid;
        }
    };

    struct SampledSegment final
    {
        SegmentId source{};
        voxel::Float3 start{};
        voxel::Float3 end{};
        float radius_start_meters{};
        float radius_end_meters{};
        float progress{};
        std::uint32_t depth{};
    };

    struct SampledTerminal final
    {
        TerminalId source{};
        TerminalKind kind{TerminalKind::generic};
        voxel::Float3 position{};
        float scale_meters{};
        float progress{};
    };

    struct Sample final
    {
        float time_seconds{};
        std::vector<SampledSegment> segments{};
        std::vector<SampledTerminal> terminals{};
    };

    struct VoxelLodLevel final
    {
        voxel::ChunkDesc chunk{};
        std::array<float, 3> bounds_min_meters{};
        std::array<float, 3> bounds_max_meters{};
        float minimum_view_distance_meters{};
        std::uint64_t estimated_active_cells{};
        std::uint64_t dense_equivalent_bytes{};
    };

    struct VoxelLodPlan final
    {
        std::vector<VoxelLodLevel> levels{};
        voxel::CellSemantic semantics{
            voxel::CellSemantic::Geometry |
            voxel::CellSemantic::Lighting |
            voxel::CellSemantic::Visibility |
            voxel::CellSemantic::ProceduralMorphology};
        std::uint64_t source_content_hash{};
    };

    [[nodiscard]] inline bool finite(voxel::Float3 value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    [[nodiscard]] inline voxel::Float3 add(voxel::Float3 lhs, voxel::Float3 rhs) noexcept
    {
        return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
    }

    [[nodiscard]] inline voxel::Float3 subtract(voxel::Float3 lhs, voxel::Float3 rhs) noexcept
    {
        return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
    }

    [[nodiscard]] inline voxel::Float3 scale(voxel::Float3 value, float amount) noexcept
    {
        return {value.x * amount, value.y * amount, value.z * amount};
    }

    [[nodiscard]] inline float length(voxel::Float3 value) noexcept
    {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    [[nodiscard]] inline voxel::Float3 normalize(voxel::Float3 value) noexcept
    {
        const float magnitude = length(value);
        if (!(magnitude > 0.000001f))
        {
            return {0.0f, 1.0f, 0.0f};
        }
        return scale(value, 1.0f / magnitude);
    }

    [[nodiscard]] inline voxel::Float3 lerp(
        voxel::Float3 from,
        voxel::Float3 to,
        float amount) noexcept
    {
        return add(from, scale(subtract(to, from), amount));
    }

    [[nodiscard]] inline float clamp01(float value) noexcept
    {
        return (std::clamp)(value, 0.0f, 1.0f);
    }

    [[nodiscard]] inline float sample_progress(TemporalRange range, float time_seconds) noexcept
    {
        if (time_seconds <= range.birth_seconds)
        {
            return 0.0f;
        }
        if (time_seconds >= range.end_seconds)
        {
            return 1.0f;
        }
        return clamp01((time_seconds - range.birth_seconds) /
            (range.end_seconds - range.birth_seconds));
    }

    [[nodiscard]] constexpr std::uint64_t mix_hash(
        std::uint64_t state,
        std::uint64_t value) noexcept
    {
        state ^= value + 0x9e3779b97f4a7c15ull + (state << 6u) + (state >> 2u);
        return state;
    }

    [[nodiscard]] inline std::uint64_t float_bits(float value) noexcept
    {
        return static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(value));
    }

    [[nodiscard]] inline float deterministic_signed(
        std::uint64_t seed,
        std::uint32_t depth,
        std::uint32_t parent,
        std::uint32_t child,
        std::uint32_t channel = 0u) noexcept
    {
        std::uint64_t value = seed;
        value = mix_hash(value, depth + 1u);
        value = mix_hash(value, parent + 17u);
        value = mix_hash(value, child + 37u);
        value = mix_hash(value, channel + 71u);
        value ^= value >> 30u;
        value *= 0xbf58476d1ce4e5b9ull;
        value ^= value >> 27u;
        value *= 0x94d049bb133111ebull;
        value ^= value >> 31u;
        const float unit = static_cast<float>(value & 0x00ffffffu) /
            static_cast<float>(0x00ffffffu);
        return unit * 2.0f - 1.0f;
    }

    [[nodiscard]] constexpr TerminalKind terminal_kind(Domain domain) noexcept
    {
        switch (domain)
        {
        case Domain::plant: return TerminalKind::foliage;
        case Domain::vascular: return TerminalKind::capillary_tip;
        case Domain::respiratory: return TerminalKind::alveolus;
        case Domain::electrical: return TerminalKind::discharge;
        case Domain::coral:
        case Domain::generic:
        default:
            return TerminalKind::generic;
        }
    }

    [[nodiscard]] constexpr Recipe default_recipe(Domain domain) noexcept
    {
        Recipe recipe{};
        recipe.document = make_handle<DocumentTag>(1u, 1u);
        recipe.revision = {
            .content = {{0x45504f43484d4f52ull, 0x50484f4c4f475931ull, 1ull, 1ull}},
            .sequence = 1u};
        recipe.domain = domain;

        switch (domain)
        {
        case Domain::plant:
            recipe.growth.seed = 1337u;
            recipe.growth.generations = 5u;
            recipe.growth.children_per_node = 2u;
            recipe.growth.root_length_meters = 2.7f;
            recipe.growth.segment_length_meters = 1.2f;
            recipe.growth.branch_angle_degrees = 38.0f;
            recipe.growth.upward_bias = 0.78f;
            recipe.growth.outward_bias = 0.72f;
            break;
        case Domain::vascular:
            recipe.growth.seed = 2309u;
            recipe.growth.generations = 7u;
            recipe.growth.children_per_node = 2u;
            recipe.growth.root_length_meters = 0.34f;
            recipe.growth.segment_length_meters = 0.22f;
            recipe.growth.length_decay = 0.79f;
            recipe.growth.root_radius_meters = 0.035f;
            recipe.growth.radius_decay = 0.68f;
            recipe.growth.branch_angle_degrees = 46.0f;
            recipe.growth.spread_degrees = 210.0f;
            recipe.growth.upward_bias = 0.22f;
            recipe.growth.outward_bias = 0.94f;
            recipe.duration_seconds = 8.0f;
            break;
        case Domain::respiratory:
            recipe.growth.seed = 4013u;
            recipe.growth.generations = 6u;
            recipe.growth.children_per_node = 2u;
            recipe.growth.root_length_meters = 0.42f;
            recipe.growth.segment_length_meters = 0.24f;
            recipe.growth.length_decay = 0.76f;
            recipe.growth.root_radius_meters = 0.045f;
            recipe.growth.branch_angle_degrees = 31.0f;
            recipe.growth.spread_degrees = 170.0f;
            recipe.growth.upward_bias = 0.56f;
            recipe.growth.outward_bias = 0.82f;
            recipe.duration_seconds = 9.0f;
            break;
        case Domain::electrical:
            recipe.growth.seed = 7919u;
            recipe.growth.generations = 6u;
            recipe.growth.children_per_node = 2u;
            recipe.growth.root_length_meters = 2.2f;
            recipe.growth.segment_length_meters = 1.5f;
            recipe.growth.length_decay = 0.66f;
            recipe.growth.root_radius_meters = 0.035f;
            recipe.growth.radius_decay = 0.58f;
            recipe.growth.branch_angle_degrees = 24.0f;
            recipe.growth.spread_degrees = 150.0f;
            recipe.growth.jitter_degrees = 28.0f;
            recipe.growth.upward_bias = -0.92f;
            recipe.growth.outward_bias = 0.44f;
            recipe.growth.generation_delay_seconds = 0.018f;
            recipe.growth.segment_growth_seconds = 0.045f;
            recipe.growth.terminal_growth_seconds = 0.025f;
            recipe.duration_seconds = 0.6f;
            break;
        case Domain::coral:
            recipe.growth.seed = 6151u;
            recipe.growth.generations = 5u;
            recipe.growth.children_per_node = 3u;
            recipe.growth.root_length_meters = 0.8f;
            recipe.growth.segment_length_meters = 0.42f;
            recipe.growth.branch_angle_degrees = 52.0f;
            recipe.growth.upward_bias = 0.64f;
            recipe.growth.outward_bias = 0.9f;
            break;
        case Domain::generic:
        default:
            recipe.growth.seed = 1u;
            break;
        }
        return recipe;
    }

    [[nodiscard]] inline ValidationResult validate(const Recipe& recipe) noexcept
    {
        if (!recipe.document.valid()) return {ValidationCode::invalid_document};
        if (!recipe.revision.valid()) return {ValidationCode::invalid_revision};
        if (!std::isfinite(recipe.duration_seconds) || !(recipe.duration_seconds > 0.0f))
            return {ValidationCode::invalid_duration};
        if (recipe.limits.maximum_depth == 0u ||
            recipe.limits.maximum_children_per_node == 0u ||
            recipe.limits.maximum_nodes < 2u ||
            recipe.limits.maximum_segments == 0u ||
            recipe.limits.maximum_terminals == 0u)
            return {ValidationCode::invalid_limits};

        const auto& growth = recipe.growth;
        if (growth.generations == 0u ||
            growth.children_per_node == 0u ||
            growth.generations > recipe.limits.maximum_depth ||
            growth.children_per_node > recipe.limits.maximum_children_per_node ||
            !(growth.root_length_meters > 0.0f) ||
            !(growth.segment_length_meters > 0.0f) ||
            !(growth.length_decay > 0.0f && growth.length_decay <= 1.0f) ||
            !(growth.root_radius_meters > 0.0f) ||
            !(growth.radius_decay > 0.0f && growth.radius_decay <= 1.0f) ||
            !(growth.segment_growth_seconds > 0.0f) ||
            !(growth.terminal_growth_seconds > 0.0f))
            return {ValidationCode::invalid_growth};
        return {};
    }

    [[nodiscard]] inline voxel::Float3 domain_root_direction(Domain domain) noexcept
    {
        return domain == Domain::electrical
            ? voxel::Float3{0.0f, -1.0f, 0.0f}
            : voxel::Float3{0.0f, 1.0f, 0.0f};
    }

    [[nodiscard]] inline Graph build_graph(const Recipe& recipe)
    {
        Graph graph{};
        graph.recipe = recipe;
        if (!validate(recipe))
        {
            return graph;
        }

        graph.nodes.reserve((std::min)(recipe.limits.maximum_nodes, 4096u));
        graph.segments.reserve((std::min)(recipe.limits.maximum_segments, 4096u));
        graph.terminals.reserve((std::min)(recipe.limits.maximum_terminals, 4096u));

        const auto make_node_id = [](std::size_t index) noexcept
        {
            return make_handle<NodeTag>(static_cast<std::uint32_t>(index), 1u);
        };
        const auto make_segment_id = [](std::size_t index) noexcept
        {
            return make_handle<SegmentTag>(static_cast<std::uint32_t>(index), 1u);
        };
        const auto make_terminal_id = [](std::size_t index) noexcept
        {
            return make_handle<TerminalTag>(static_cast<std::uint32_t>(index), 1u);
        };

        const voxel::Float3 root_direction = domain_root_direction(recipe.domain);
        const float root_birth = -recipe.growth.sapling_pre_age_seconds;
        const float root_end = root_birth + recipe.growth.segment_growth_seconds;
        graph.nodes.push_back(Node{
            .id = make_node_id(0u),
            .role = NodeRole::root,
            .position = {},
            .tangent = root_direction,
            .radius_meters = recipe.growth.root_radius_meters,
            .lifetime = {root_birth, root_end}});

        const voxel::Float3 root_end_position = scale(root_direction, recipe.growth.root_length_meters);
        graph.nodes.push_back(Node{
            .id = make_node_id(1u),
            .parent = graph.nodes[0].id,
            .role = NodeRole::junction,
            .depth = 1u,
            .position = root_end_position,
            .tangent = root_direction,
            .radius_meters = recipe.growth.root_radius_meters * recipe.growth.radius_decay,
            .lifetime = {root_birth, root_end}});
        graph.segments.push_back(Segment{
            .id = make_segment_id(0u),
            .parent_node = graph.nodes[0].id,
            .child_node = graph.nodes[1].id,
            .start = {},
            .end = root_end_position,
            .tangent_start = root_direction,
            .tangent_end = root_direction,
            .radius_start_meters = recipe.growth.root_radius_meters,
            .radius_end_meters = graph.nodes[1].radius_meters,
            .depth = 0u,
            .lifetime = {root_birth, root_end}});

        std::vector<std::uint32_t> frontier{1u};
        std::vector<std::uint32_t> next_frontier{};
        next_frontier.reserve(recipe.growth.children_per_node * frontier.size());
        constexpr float degrees_to_radians = 0.017453292519943295769f;

        for (std::uint32_t depth = 1u;
            depth <= recipe.growth.generations && !frontier.empty();
            ++depth)
        {
            next_frontier.clear();
            const float depth_factor = static_cast<float>(depth) /
                static_cast<float>((std::max)(1u, recipe.growth.generations));
            const float segment_length = recipe.growth.segment_length_meters *
                std::pow(recipe.growth.length_decay, static_cast<float>(depth - 1u));
            const float child_radius = (std::max)(
                0.0005f,
                recipe.growth.root_radius_meters *
                    std::pow(recipe.growth.radius_decay, static_cast<float>(depth + 1u)));

            for (const std::uint32_t parent_index : frontier)
            {
                if (parent_index >= graph.nodes.size())
                    continue;
                const Node parent = graph.nodes[parent_index];

                for (std::uint32_t child = 0u;
                    child < recipe.growth.children_per_node;
                    ++child)
                {
                    if (graph.nodes.size() >= recipe.limits.maximum_nodes ||
                        graph.segments.size() >= recipe.limits.maximum_segments)
                    {
                        graph.truncated = true;
                        break;
                    }

                    const float ratio = recipe.growth.children_per_node > 1u
                        ? static_cast<float>(child) /
                            static_cast<float>(recipe.growth.children_per_node)
                        : 0.0f;
                    const float jitter = deterministic_signed(
                        recipe.growth.seed,
                        depth,
                        parent_index,
                        child);
                    const float yaw = (
                        ratio * recipe.growth.spread_degrees +
                        recipe.growth.twist_degrees * static_cast<float>(depth) +
                        jitter * recipe.growth.jitter_degrees) * degrees_to_radians;
                    const float pitch = recipe.growth.branch_angle_degrees * degrees_to_radians;
                    float vertical = std::cos(pitch) * recipe.growth.upward_bias;
                    if (recipe.domain == Domain::electrical)
                        vertical = -(std::abs)(vertical);
                    vertical -= recipe.growth.sag * depth_factor;
                    float planar_z = std::sin(yaw);
                    if (recipe.dimension == DimensionMode::planar_2d ||
                        recipe.dimension == DimensionMode::pattern_2d)
                        planar_z = 0.0f;
                    else if (recipe.dimension == DimensionMode::planar_2_5d)
                        planar_z *= 0.15f;

                    const voxel::Float3 radial{
                        std::cos(yaw) * std::sin(pitch) * recipe.growth.outward_bias,
                        vertical,
                        planar_z * std::sin(pitch) * recipe.growth.outward_bias};
                    const voxel::Float3 inherited = scale(parent.tangent, 0.36f);
                    const voxel::Float3 direction = normalize(add(inherited, radial));
                    const voxel::Float3 end = add(parent.position, scale(direction, segment_length));
                    const float birth = static_cast<float>(depth) *
                        recipe.growth.generation_delay_seconds;
                    const float end_time = birth + recipe.growth.segment_growth_seconds;
                    const std::uint32_t node_index = static_cast<std::uint32_t>(graph.nodes.size());
                    const NodeId node_id = make_node_id(node_index);
                    graph.nodes.push_back(Node{
                        .id = node_id,
                        .parent = parent.id,
                        .role = depth == recipe.growth.generations
                            ? NodeRole::terminal
                            : NodeRole::junction,
                        .depth = depth + 1u,
                        .position = end,
                        .tangent = direction,
                        .radius_meters = child_radius,
                        .lifetime = {birth, end_time}});
                    graph.segments.push_back(Segment{
                        .id = make_segment_id(graph.segments.size()),
                        .parent_node = parent.id,
                        .child_node = node_id,
                        .start = parent.position,
                        .end = end,
                        .tangent_start = parent.tangent,
                        .tangent_end = direction,
                        .radius_start_meters = parent.radius_meters,
                        .radius_end_meters = child_radius,
                        .depth = depth,
                        .lifetime = {birth, end_time}});
                    next_frontier.push_back(node_index);
                }
                if (graph.truncated)
                    break;
            }
            frontier = next_frontier;
            if (graph.truncated)
                break;
        }

        const TerminalKind kind = terminal_kind(recipe.domain);
        for (const std::uint32_t node_index : frontier)
        {
            if (node_index >= graph.nodes.size() ||
                graph.terminals.size() >= recipe.limits.maximum_terminals)
            {
                graph.truncated = graph.terminals.size() >= recipe.limits.maximum_terminals;
                continue;
            }
            const Node& node = graph.nodes[node_index];
            const float birth = node.lifetime.birth_seconds + recipe.growth.terminal_delay_seconds;
            graph.terminals.push_back(Terminal{
                .id = make_terminal_id(graph.terminals.size()),
                .node = node.id,
                .kind = kind,
                .position = node.position,
                .outward = node.tangent,
                .up = domain_root_direction(recipe.domain),
                .scale_meters = (std::max)(node.radius_meters * 5.0f, 0.015f),
                .variant = static_cast<std::uint32_t>(
                    deterministic_signed(recipe.growth.seed, node.depth, node_index, 0u, 4u) > 0.0f),
                .lifetime = {birth, birth + recipe.growth.terminal_growth_seconds}});
        }

        std::uint64_t hash = 14695981039346656037ull;
        hash = mix_hash(hash, static_cast<std::uint64_t>(recipe.domain));
        hash = mix_hash(hash, static_cast<std::uint64_t>(recipe.dimension));
        hash = mix_hash(hash, recipe.growth.seed);
        for (const Node& node : graph.nodes)
        {
            hash = mix_hash(hash, node.id.index);
            hash = mix_hash(hash, node.parent.index);
            hash = mix_hash(hash, float_bits(node.position.x));
            hash = mix_hash(hash, float_bits(node.position.y));
            hash = mix_hash(hash, float_bits(node.position.z));
            hash = mix_hash(hash, float_bits(node.radius_meters));
            hash = mix_hash(hash, float_bits(node.lifetime.birth_seconds));
            hash = mix_hash(hash, float_bits(node.lifetime.end_seconds));
        }
        graph.content_hash = hash == 0u ? 1u : hash;
        return graph;
    }

    [[nodiscard]] inline std::uint64_t recompute_content_hash(const Graph& graph) noexcept
    {
        std::uint64_t hash = 14695981039346656037ull;
        hash = mix_hash(hash, static_cast<std::uint64_t>(graph.recipe.domain));
        hash = mix_hash(hash, static_cast<std::uint64_t>(graph.recipe.dimension));
        hash = mix_hash(hash, graph.recipe.growth.seed);
        for (const Node& node : graph.nodes)
        {
            hash = mix_hash(hash, node.id.index);
            hash = mix_hash(hash, node.parent.index);
            hash = mix_hash(hash, float_bits(node.position.x));
            hash = mix_hash(hash, float_bits(node.position.y));
            hash = mix_hash(hash, float_bits(node.position.z));
            hash = mix_hash(hash, float_bits(node.radius_meters));
            hash = mix_hash(hash, float_bits(node.lifetime.birth_seconds));
            hash = mix_hash(hash, float_bits(node.lifetime.end_seconds));
        }
        return hash == 0u ? 1u : hash;
    }

    [[nodiscard]] inline ValidationResult validate(const Graph& graph) noexcept
    {
        const ValidationResult recipe_result = validate(graph.recipe);
        if (!recipe_result) return recipe_result;
        if (graph.nodes.empty() || graph.segments.empty())
            return {ValidationCode::invalid_identity};
        if (graph.nodes.size() > graph.recipe.limits.maximum_nodes ||
            graph.segments.size() > graph.recipe.limits.maximum_segments ||
            graph.terminals.size() > graph.recipe.limits.maximum_terminals)
            return {ValidationCode::budget_exceeded};

        for (std::size_t index = 0u; index < graph.nodes.size(); ++index)
        {
            const Node& node = graph.nodes[index];
            if (!node.id.valid() || node.id.index != index)
                return {ValidationCode::invalid_identity, index};
            if (index > 0u && (!node.parent.valid() || node.parent.index >= index))
                return {ValidationCode::invalid_parent, index};
            if (!finite(node.position) || !finite(node.tangent) ||
                !std::isfinite(node.radius_meters) || !(node.radius_meters > 0.0f))
                return {ValidationCode::non_finite_geometry, index};
            if (!std::isfinite(node.lifetime.birth_seconds) ||
                !std::isfinite(node.lifetime.end_seconds) ||
                !(node.lifetime.end_seconds > node.lifetime.birth_seconds))
                return {ValidationCode::invalid_lifetime, index};
        }
        for (std::size_t index = 0u; index < graph.segments.size(); ++index)
        {
            const Segment& segment = graph.segments[index];
            if (!segment.id.valid() || segment.id.index != index ||
                !segment.parent_node.valid() || !segment.child_node.valid())
                return {ValidationCode::invalid_identity, index};
            if (segment.parent_node.index >= graph.nodes.size() ||
                segment.child_node.index >= graph.nodes.size() ||
                segment.parent_node.index >= segment.child_node.index)
                return {ValidationCode::invalid_parent, index};
            if (!finite(segment.start) || !finite(segment.end) ||
                !finite(segment.tangent_start) || !finite(segment.tangent_end) ||
                !(segment.radius_start_meters > 0.0f) ||
                !(segment.radius_end_meters > 0.0f))
                return {ValidationCode::non_finite_geometry, index};
            if (!(segment.lifetime.end_seconds > segment.lifetime.birth_seconds))
                return {ValidationCode::invalid_lifetime, index};
        }
        for (std::size_t index = 0u; index < graph.terminals.size(); ++index)
        {
            const Terminal& terminal = graph.terminals[index];
            if (!terminal.id.valid() || terminal.id.index != index ||
                !terminal.node.valid() || terminal.node.index >= graph.nodes.size())
                return {ValidationCode::invalid_identity, index};
            if (!finite(terminal.position) || !finite(terminal.outward) ||
                !finite(terminal.up) || !(terminal.scale_meters > 0.0f))
                return {ValidationCode::non_finite_geometry, index};
            if (!(terminal.lifetime.end_seconds > terminal.lifetime.birth_seconds))
                return {ValidationCode::invalid_lifetime, index};
        }
        if (graph.content_hash == 0u || graph.content_hash != recompute_content_hash(graph))
            return {ValidationCode::content_hash_mismatch};
        return {};
    }

    [[nodiscard]] inline Sample sample(const Graph& graph, float time_seconds)
    {
        Sample result{};
        result.time_seconds = (std::clamp)(time_seconds, 0.0f, graph.recipe.duration_seconds);
        result.segments.reserve(graph.segments.size());
        result.terminals.reserve(graph.terminals.size());

        for (const Segment& segment : graph.segments)
        {
            const float progress = sample_progress(segment.lifetime, result.time_seconds);
            if (!(progress > 0.0f))
                continue;
            result.segments.push_back(SampledSegment{
                .source = segment.id,
                .start = segment.start,
                .end = lerp(segment.start, segment.end, progress),
                .radius_start_meters = segment.radius_start_meters,
                .radius_end_meters = segment.radius_end_meters * progress,
                .progress = progress,
                .depth = segment.depth});
        }
        for (const Terminal& terminal : graph.terminals)
        {
            const float progress = sample_progress(terminal.lifetime, result.time_seconds);
            if (!(progress > 0.0f))
                continue;
            result.terminals.push_back(SampledTerminal{
                .source = terminal.id,
                .kind = terminal.kind,
                .position = terminal.position,
                .scale_meters = terminal.scale_meters * progress,
                .progress = progress});
        }
        return result;
    }

    [[nodiscard]] inline VoxelLodPlan plan_voxel_lods(
        const Graph& graph,
        voxel::LodPolicy policy = {},
        float base_cell_size_meters = 0.05f,
        std::uint8_t level_count = 5u)
    {
        VoxelLodPlan plan{};
        plan.source_content_hash = graph.content_hash;
        if (!validate(graph) || level_count == 0u || !(base_cell_size_meters > 0.0f))
            return plan;

        voxel::Float3 minimum = graph.nodes.front().position;
        voxel::Float3 maximum = graph.nodes.front().position;
        float maximum_radius = graph.nodes.front().radius_meters;
        for (const Node& node : graph.nodes)
        {
            minimum.x = (std::min)(minimum.x, node.position.x);
            minimum.y = (std::min)(minimum.y, node.position.y);
            minimum.z = (std::min)(minimum.z, node.position.z);
            maximum.x = (std::max)(maximum.x, node.position.x);
            maximum.y = (std::max)(maximum.y, node.position.y);
            maximum.z = (std::max)(maximum.z, node.position.z);
            maximum_radius = (std::max)(maximum_radius, node.radius_meters);
        }
        const float margin = (std::max)(maximum_radius * 3.0f, base_cell_size_meters * 2.0f);
        minimum = add(minimum, {-margin, -margin, -margin});
        maximum = add(maximum, {margin, margin, margin});

        const std::uint8_t bounded_levels = (std::min)(level_count, std::uint8_t{16});
        plan.levels.reserve(bounded_levels);
        for (std::uint8_t level = 0u; level < bounded_levels; ++level)
        {
            const float cell_size = base_cell_size_meters * std::pow(2.0f, static_cast<float>(level));
            const auto cells_for_extent = [cell_size](float extent) noexcept
            {
                return (std::clamp)(
                    static_cast<std::uint32_t>((std::max)(4.0f, std::ceil(extent / cell_size))),
                    4u,
                    128u);
            };
            voxel::ChunkDesc chunk{
                .cellsX = cells_for_extent(maximum.x - minimum.x),
                .cellsY = cells_for_extent(maximum.y - minimum.y),
                .cellsZ = cells_for_extent(maximum.z - minimum.z),
                .cellSizeMeters = cell_size,
                .lodLevel = static_cast<std::uint8_t>(policy.nearLod + level)};

            std::uint64_t active_cells = 0u;
            for (const Segment& segment : graph.segments)
            {
                const float segment_length = length(subtract(segment.end, segment.start));
                const auto length_cells = static_cast<std::uint64_t>(
                    (std::max)(1.0f, std::ceil(segment_length / cell_size)));
                const auto radius_cells = static_cast<std::uint64_t>(
                    (std::max)(1.0f, std::ceil(segment.radius_start_meters / cell_size)));
                active_cells += length_cells * radius_cells * radius_cells;
            }
            active_cells += graph.terminals.size();
            active_cells = (std::min)(active_cells, voxel::dense_cell_count(chunk));

            const float distance_ratio = bounded_levels > 1u
                ? static_cast<float>(level) / static_cast<float>(bounded_levels - 1u)
                : 0.0f;
            plan.levels.push_back(VoxelLodLevel{
                .chunk = chunk,
                .bounds_min_meters = {minimum.x, minimum.y, minimum.z},
                .bounds_max_meters = {maximum.x, maximum.y, maximum.z},
                .minimum_view_distance_meters =
                    policy.nearMeters + (policy.farMeters - policy.nearMeters) * distance_ratio,
                .estimated_active_cells = active_cells,
                .dense_equivalent_bytes = voxel::dense_cell_bytes(chunk)});
        }
        return plan;
    }

    struct ContractReport final
    {
        bool plant{};
        bool vascular{};
        bool respiratory{};
        bool electrical{};
        bool deterministic{};
        bool temporal{};
        bool voxel_lod{};

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return plant && vascular && respiratory && electrical &&
                deterministic && temporal && voxel_lod;
        }
    };

    [[nodiscard]] inline ContractReport run_contract()
    {
        ContractReport report{};
        const Graph plant = build_graph(default_recipe(Domain::plant));
        const Graph vascular = build_graph(default_recipe(Domain::vascular));
        const Graph respiratory = build_graph(default_recipe(Domain::respiratory));
        const Graph electrical = build_graph(default_recipe(Domain::electrical));
        report.plant = static_cast<bool>(validate(plant)) && !plant.terminals.empty();
        report.vascular = static_cast<bool>(validate(vascular)) &&
            vascular.terminals.front().kind == TerminalKind::capillary_tip;
        report.respiratory = static_cast<bool>(validate(respiratory)) &&
            respiratory.terminals.front().kind == TerminalKind::alveolus;
        report.electrical = static_cast<bool>(validate(electrical)) &&
            electrical.terminals.front().kind == TerminalKind::discharge;
        const Graph plant_again = build_graph(default_recipe(Domain::plant));
        report.deterministic = plant.content_hash == plant_again.content_hash &&
            plant.nodes.size() == plant_again.nodes.size();
        const Sample early = sample(plant, 0.1f);
        const Sample late = sample(plant, plant.recipe.duration_seconds);
        report.temporal = !early.segments.empty() &&
            late.segments.size() >= early.segments.size() &&
            late.terminals.size() == plant.terminals.size();
        const VoxelLodPlan lod = plan_voxel_lods(plant);
        report.voxel_lod = lod.levels.size() == 5u &&
            lod.levels.front().chunk.lodLevel < lod.levels.back().chunk.lodLevel &&
            lod.levels.front().dense_equivalent_bytes >= lod.levels.back().dense_equivalent_bytes &&
            voxel::morphology_semantics(lod.semantics);
        return report;
    }
}
