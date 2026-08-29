// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

export module forest.preview_mesh;

import authoring.morphology;
import voxel.field;

export namespace epochengine::forest::preview_mesh
{
    inline constexpr std::uint32_t kRadialSides = 8u;
    inline constexpr std::size_t kMaximumSegments = 256u;
    inline constexpr std::size_t kMaximumLeaves = 384u;
    inline constexpr std::size_t kMaximumVertices =
        kMaximumSegments * 28u + kMaximumLeaves * 8u;
    inline constexpr std::size_t kMaximumIndices =
        kMaximumSegments * 96u + kMaximumLeaves * 12u;

    enum class MaterialSlot : std::uint8_t
    {
        wood = 0u,
        foliage = 1u
    };

    struct Segment final
    {
        epochengine::voxel::Float3 start{};
        epochengine::voxel::Float3 end{};
        float radiusStart{0.08f};
        float radiusEnd{0.06f};
        std::uint32_t depth{};
        std::uint32_t sourceSegment{};
    };

    struct Leaf final
    {
        epochengine::voxel::Float3 position{};
        epochengine::voxel::Float3 outward{1.0f, 0.0f, 0.0f};
        epochengine::voxel::Float3 up{0.0f, 1.0f, 0.0f};
        float size{0.24f};
        std::uint32_t sourceSegment{};
        std::uint32_t variant{};
    };

    struct Geometry final
    {
        std::array<Segment, kMaximumSegments> segments{};
        std::size_t segmentCount{};
        std::array<Leaf, kMaximumLeaves> leaves{};
        std::size_t leafCount{};
    };

    struct Vector3 final
    {
        float x{};
        float y{};
        float z{};
    };

    struct Vector2 final
    {
        float x{};
        float y{};
    };

    struct Vertex final
    {
        Vector3 position{};
        Vector3 normal{0.0f, 1.0f, 0.0f};
        Vector2 uv{};
        MaterialSlot material{MaterialSlot::wood};
    };

    struct Bounds final
    {
        Vector3 minimum{};
        Vector3 maximum{};
        bool valid{};
    };

    struct IndexedMesh final
    {
        std::vector<Vertex> vertices{};
        std::vector<std::uint32_t> indices{};
        Bounds bounds{};
        std::uint32_t segmentCount{};
        std::uint32_t leafCardCount{};
        std::uint32_t rootCaps{};
        std::uint32_t terminalCaps{};
        std::uint32_t suppressedInternalCaps{};
        std::uint64_t contentSignature{};
        bool budgetExceeded{};
        bool valid{};
    };

    namespace detail
    {
        [[nodiscard]] constexpr Vector3 add(Vector3 lhs, Vector3 rhs) noexcept
        {
            return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
        }

        [[nodiscard]] constexpr Vector3 subtract(Vector3 lhs, Vector3 rhs) noexcept
        {
            return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
        }

        [[nodiscard]] constexpr Vector3 scale(Vector3 value, float amount) noexcept
        {
            return {value.x * amount, value.y * amount, value.z * amount};
        }

        [[nodiscard]] constexpr float dot(Vector3 lhs, Vector3 rhs) noexcept
        {
            return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
        }

        [[nodiscard]] constexpr Vector3 cross(Vector3 lhs, Vector3 rhs) noexcept
        {
            return {
                lhs.y * rhs.z - lhs.z * rhs.y,
                lhs.z * rhs.x - lhs.x * rhs.z,
                lhs.x * rhs.y - lhs.y * rhs.x};
        }

        [[nodiscard]] inline float length(Vector3 value) noexcept
        {
            return std::sqrt(dot(value, value));
        }

        [[nodiscard]] inline Vector3 normalize(
            Vector3 value,
            Vector3 fallback = {0.0f, 1.0f, 0.0f}) noexcept
        {
            const float magnitude = length(value);
            if (!(magnitude > 0.000001f) || !std::isfinite(magnitude))
                return fallback;
            return scale(value, 1.0f / magnitude);
        }

        [[nodiscard]] constexpr Vector3 convert(
            epochengine::voxel::Float3 value) noexcept
        {
            return {value.x, value.y, value.z};
        }

        [[nodiscard]] inline bool finite(Vector3 value) noexcept
        {
            return std::isfinite(value.x)
                && std::isfinite(value.y)
                && std::isfinite(value.z);
        }

        [[nodiscard]] inline bool same_point(Vector3 lhs, Vector3 rhs) noexcept
        {
            constexpr float epsilon = 0.00001f;
            return std::abs(lhs.x - rhs.x) <= epsilon
                && std::abs(lhs.y - rhs.y) <= epsilon
                && std::abs(lhs.z - rhs.z) <= epsilon;
        }

        inline void include(Bounds& bounds, Vector3 point) noexcept
        {
            if (!bounds.valid)
            {
                bounds.minimum = point;
                bounds.maximum = point;
                bounds.valid = true;
                return;
            }
            bounds.minimum.x = (std::min)(bounds.minimum.x, point.x);
            bounds.minimum.y = (std::min)(bounds.minimum.y, point.y);
            bounds.minimum.z = (std::min)(bounds.minimum.z, point.z);
            bounds.maximum.x = (std::max)(bounds.maximum.x, point.x);
            bounds.maximum.y = (std::max)(bounds.maximum.y, point.y);
            bounds.maximum.z = (std::max)(bounds.maximum.z, point.z);
        }

        [[nodiscard]] constexpr std::uint64_t mix(
            std::uint64_t state,
            std::uint64_t value) noexcept
        {
            state ^= value + 0x9e3779b97f4a7c15ull
                + (state << 6u) + (state >> 2u);
            return state;
        }

        [[nodiscard]] inline std::uint64_t signature(
            const IndexedMesh& mesh) noexcept
        {
            std::uint64_t hash = 0x45504f4348464d45ull;
            const auto addFloat = [&](float value) noexcept
            {
                hash = mix(hash, std::bit_cast<std::uint32_t>(value));
            };
            for (const Vertex& vertex : mesh.vertices)
            {
                addFloat(vertex.position.x);
                addFloat(vertex.position.y);
                addFloat(vertex.position.z);
                addFloat(vertex.normal.x);
                addFloat(vertex.normal.y);
                addFloat(vertex.normal.z);
                addFloat(vertex.uv.x);
                addFloat(vertex.uv.y);
                hash = mix(hash, static_cast<std::uint64_t>(vertex.material));
            }
            for (const std::uint32_t index : mesh.indices)
                hash = mix(hash, index);
            hash = mix(hash, mesh.rootCaps);
            hash = mix(hash, mesh.terminalCaps);
            hash = mix(hash, mesh.suppressedInternalCaps);
            return hash == 0u ? 1u : hash;
        }
    }

    [[nodiscard]] inline Geometry extract(
        const epochengine::authoring::morphology::Graph& graph,
        const epochengine::authoring::morphology::Sample& sample) noexcept
    {
        Geometry geometry{};
        geometry.segmentCount = (std::min)(sample.segments.size(), kMaximumSegments);
        for (std::size_t index = 0u; index < geometry.segmentCount; ++index)
        {
            const auto& source = sample.segments[index];
            geometry.segments[index] = Segment{
                .start = source.start,
                .end = source.end,
                .radiusStart = source.radius_start_meters,
                .radiusEnd = source.radius_end_meters,
                .depth = source.depth,
                .sourceSegment = source.source.index};
        }

        geometry.leafCount = (std::min)(sample.terminals.size(), kMaximumLeaves);
        for (std::size_t index = 0u; index < geometry.leafCount; ++index)
        {
            const auto& source = sample.terminals[index];
            epochengine::voxel::Float3 outward{1.0f, 0.0f, 0.0f};
            epochengine::voxel::Float3 up{0.0f, 1.0f, 0.0f};
            std::uint32_t variant{};
            std::uint32_t sourceSegment{};
            if (source.source.index < graph.terminals.size())
            {
                const auto& terminal = graph.terminals[source.source.index];
                outward = terminal.outward;
                up = terminal.up;
                variant = terminal.variant;
                for (const auto& segment : graph.segments)
                {
                    if (segment.child_node == terminal.node)
                    {
                        sourceSegment = segment.id.index;
                        break;
                    }
                }
            }
            geometry.leaves[index] = Leaf{
                .position = source.position,
                .outward = outward,
                .up = up,
                .size = source.scale_meters,
                .sourceSegment = sourceSegment,
                .variant = variant};
        }
        return geometry;
    }

    [[nodiscard]] inline IndexedMesh build(
        const Geometry& geometry)
    {
        IndexedMesh mesh{};
        if (geometry.segmentCount > geometry.segments.size()
            || geometry.leafCount > geometry.leaves.size())
        {
            mesh.budgetExceeded = true;
            return mesh;
        }

        mesh.vertices.reserve((std::min)(
            kMaximumVertices,
            geometry.segmentCount * 22u + geometry.leafCount * 8u));
        mesh.indices.reserve((std::min)(
            kMaximumIndices,
            geometry.segmentCount * 72u + geometry.leafCount * 12u));

        const auto roomFor = [&](std::size_t vertexCount,
                                 std::size_t indexCount) noexcept
        {
            return mesh.vertices.size() + vertexCount <= kMaximumVertices
                && mesh.indices.size() + indexCount <= kMaximumIndices;
        };
        const auto pushVertex = [&](const Vertex& vertex)
        {
            detail::include(mesh.bounds, vertex.position);
            mesh.vertices.push_back(vertex);
        };
        const auto pushIndex = [&](std::size_t index)
        {
            mesh.indices.push_back(static_cast<std::uint32_t>(index));
        };

        for (std::size_t segmentIndex = 0u;
            segmentIndex < geometry.segmentCount; ++segmentIndex)
        {
            const Segment& source = geometry.segments[segmentIndex];
            const Vector3 start = detail::convert(source.start);
            const Vector3 end = detail::convert(source.end);
            const Vector3 axis = detail::subtract(end, start);
            const float axisLength = detail::length(axis);
            const float startRadius = source.radiusStart;
            const float endRadius = source.radiusEnd;
            if (!detail::finite(start) || !detail::finite(end)
                || !(axisLength > 0.000001f)
                || !std::isfinite(startRadius) || !std::isfinite(endRadius)
                || !(startRadius > 0.0f) || !(endRadius > 0.0f))
            {
                return {};
            }

            bool rootStart = true;
            bool terminalEnd = true;
            for (std::size_t otherIndex = 0u;
                otherIndex < geometry.segmentCount; ++otherIndex)
            {
                if (otherIndex == segmentIndex)
                    continue;
                const Segment& other = geometry.segments[otherIndex];
                rootStart = rootStart
                    && !detail::same_point(start, detail::convert(other.end));
                terminalEnd = terminalEnd
                    && !detail::same_point(end, detail::convert(other.start));
            }

            const std::size_t capCount =
                static_cast<std::size_t>(rootStart)
                + static_cast<std::size_t>(terminalEnd);
            const std::size_t addedVertices =
                static_cast<std::size_t>(kRadialSides + 1u) * 2u + capCount;
            const std::size_t addedIndices =
                static_cast<std::size_t>(kRadialSides) * 6u
                + capCount * static_cast<std::size_t>(kRadialSides) * 3u;
            if (!roomFor(addedVertices, addedIndices))
            {
                mesh.budgetExceeded = true;
                mesh.valid = false;
                return mesh;
            }

            const Vector3 tangent = detail::normalize(axis);
            const Vector3 reference = std::abs(tangent.y) < 0.92f
                ? Vector3{0.0f, 1.0f, 0.0f}
                : Vector3{1.0f, 0.0f, 0.0f};
            const Vector3 ringU = detail::normalize(
                detail::cross(reference, tangent), {0.0f, 0.0f, 1.0f});
            const Vector3 ringV = detail::normalize(
                detail::cross(tangent, ringU), {1.0f, 0.0f, 0.0f});
            const float taperSlope = (startRadius - endRadius) / axisLength;
            const std::size_t startRing = mesh.vertices.size();
            constexpr float tau = 6.28318530717958647692f;
            for (std::uint32_t ring = 0u; ring <= 1u; ++ring)
            {
                const Vector3 center = ring == 0u ? start : end;
                const float radius = ring == 0u ? startRadius : endRadius;
                for (std::uint32_t side = 0u; side <= kRadialSides; ++side)
                {
                    const float fraction = static_cast<float>(side)
                        / static_cast<float>(kRadialSides);
                    const float angle = tau * fraction;
                    const Vector3 radial = detail::add(
                        detail::scale(ringU, std::cos(angle)),
                        detail::scale(ringV, std::sin(angle)));
                    const Vector3 normal = detail::normalize(detail::add(
                        radial, detail::scale(tangent, taperSlope)), radial);
                    pushVertex(Vertex{
                        .position = detail::add(center, detail::scale(radial, radius)),
                        .normal = normal,
                        .uv{fraction, static_cast<float>(ring)},
                        .material = MaterialSlot::wood});
                }
            }

            const std::size_t endRing = startRing + kRadialSides + 1u;
            for (std::uint32_t side = 0u; side < kRadialSides; ++side)
            {
                const std::size_t s0 = startRing + side;
                const std::size_t s1 = s0 + 1u;
                const std::size_t e0 = endRing + side;
                const std::size_t e1 = e0 + 1u;
                pushIndex(s0); pushIndex(e1); pushIndex(e0);
                pushIndex(s0); pushIndex(s1); pushIndex(e1);
            }

            if (rootStart)
            {
                const std::size_t centerIndex = mesh.vertices.size();
                pushVertex(Vertex{
                    .position = start,
                    .normal = detail::scale(tangent, -1.0f),
                    .uv{0.5f, 0.5f},
                    .material = MaterialSlot::wood});
                for (std::uint32_t side = 0u; side < kRadialSides; ++side)
                {
                    pushIndex(centerIndex);
                    pushIndex(startRing + side + 1u);
                    pushIndex(startRing + side);
                }
                ++mesh.rootCaps;
            }
            else
            {
                ++mesh.suppressedInternalCaps;
            }

            if (terminalEnd)
            {
                const std::size_t centerIndex = mesh.vertices.size();
                pushVertex(Vertex{
                    .position = end,
                    .normal = tangent,
                    .uv{0.5f, 0.5f},
                    .material = MaterialSlot::wood});
                for (std::uint32_t side = 0u; side < kRadialSides; ++side)
                {
                    pushIndex(centerIndex);
                    pushIndex(endRing + side);
                    pushIndex(endRing + side + 1u);
                }
                ++mesh.terminalCaps;
            }
            else
            {
                ++mesh.suppressedInternalCaps;
            }
            ++mesh.segmentCount;
        }

        for (std::size_t leafIndex = 0u;
            leafIndex < geometry.leafCount; ++leafIndex)
        {
            if (!roomFor(8u, 12u))
            {
                mesh.budgetExceeded = true;
                mesh.valid = false;
                return mesh;
            }
            const Leaf& leaf = geometry.leaves[leafIndex];
            const Vector3 center = detail::convert(leaf.position);
            const Vector3 outward = detail::normalize(
                detail::convert(leaf.outward), {1.0f, 0.0f, 0.0f});
            Vector3 up = detail::convert(leaf.up);
            up = detail::subtract(up, detail::scale(outward, detail::dot(up, outward)));
            up = detail::normalize(up, {0.0f, 1.0f, 0.0f});
            Vector3 side = detail::normalize(
                detail::cross(up, outward), {0.0f, 0.0f, 1.0f});
            if ((leaf.variant & 1u) != 0u)
                side = detail::scale(side, -1.0f);
            const float halfWidth = (std::max)(0.01f, leaf.size * 0.50f);
            const float halfHeight = (std::max)(0.015f, leaf.size * 0.72f);
            const std::array<Vector3, 4> positions{{
                detail::add(center, detail::add(
                    detail::scale(side, -halfWidth), detail::scale(up, -halfHeight))),
                detail::add(center, detail::add(
                    detail::scale(side, halfWidth), detail::scale(up, -halfHeight))),
                detail::add(center, detail::add(
                    detail::scale(side, halfWidth), detail::scale(up, halfHeight))),
                detail::add(center, detail::add(
                    detail::scale(side, -halfWidth), detail::scale(up, halfHeight)))}};
            const std::array<Vector2, 4> uvs{{
                {0.0f, 0.0f}, {1.0f, 0.0f},
                {1.0f, 1.0f}, {0.0f, 1.0f}}};
            const std::size_t front = mesh.vertices.size();
            for (std::size_t corner = 0u; corner < positions.size(); ++corner)
                pushVertex(Vertex{positions[corner], outward, uvs[corner], MaterialSlot::foliage});
            const std::size_t back = mesh.vertices.size();
            for (std::size_t corner = 0u; corner < positions.size(); ++corner)
                pushVertex(Vertex{positions[corner], detail::scale(outward, -1.0f), uvs[corner], MaterialSlot::foliage});
            pushIndex(front); pushIndex(front + 1u); pushIndex(front + 2u);
            pushIndex(front); pushIndex(front + 2u); pushIndex(front + 3u);
            pushIndex(back); pushIndex(back + 2u); pushIndex(back + 1u);
            pushIndex(back); pushIndex(back + 3u); pushIndex(back + 2u);
            ++mesh.leafCardCount;
        }

        mesh.valid = mesh.segmentCount == geometry.segmentCount
            && mesh.leafCardCount == geometry.leafCount
            && mesh.bounds.valid
            && !mesh.vertices.empty()
            && !mesh.indices.empty()
            && !mesh.budgetExceeded;
        if (mesh.valid)
            mesh.contentSignature = detail::signature(mesh);
        return mesh;
    }

    [[nodiscard]] inline IndexedMesh build(
        const epochengine::authoring::morphology::Graph& graph,
        const epochengine::authoring::morphology::Sample& sample)
    {
        if (sample.segments.size() > kMaximumSegments
            || sample.terminals.size() > kMaximumLeaves)
        {
            IndexedMesh refused{};
            refused.budgetExceeded = true;
            return refused;
        }
        return build(extract(graph, sample));
    }

    [[nodiscard]] inline bool projection_admitted(
        const Geometry& geometry)
    {
        const IndexedMesh mesh = build(geometry);
        return mesh.valid
            && mesh.segmentCount == geometry.segmentCount
            && mesh.leafCardCount == geometry.leafCount;
    }

    [[nodiscard]] inline bool projection_admitted(
        const epochengine::authoring::morphology::Graph& graph,
        const epochengine::authoring::morphology::Sample& sample)
    {
        if (sample.segments.size() > kMaximumSegments
            || sample.terminals.size() > kMaximumLeaves)
        {
            return false;
        }
        return projection_admitted(extract(graph, sample));
    }

    [[nodiscard]] inline bool segment_projection_admitted(
        float length,
        float radiusStart,
        float radiusEnd)
    {
        Geometry geometry{};
        geometry.segmentCount = 1u;
        geometry.segments[0] = Segment{
            .start{0.0f, 0.0f, 0.0f},
            .end{0.0f, length, 0.0f},
            .radiusStart = radiusStart,
            .radiusEnd = radiusEnd};
        return projection_admitted(geometry);
    }

    struct ContractReport final
    {
        bool deterministic{};
        bool taperedEndpointRadii{};
        bool connectedCapsSuppressed{};
        bool rootAndTerminalCapsPresent{};
        bool indexedWindingAndNormals{};
        bool uvBoundsAndMaterials{};
        bool terminalOrientedLeafCard{};
        bool strictBudgetRefusal{};
        bool productionProjectionAdmission{};

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return deterministic && taperedEndpointRadii
                && connectedCapsSuppressed && rootAndTerminalCapsPresent
                && indexedWindingAndNormals && uvBoundsAndMaterials
                && terminalOrientedLeafCard && strictBudgetRefusal
                && productionProjectionAdmission;
        }
    };

    [[nodiscard]] inline ContractReport run_contract()
    {
        Geometry geometry{};
        geometry.segmentCount = 2u;
        geometry.segments[0] = Segment{
            .start{0.0f, 0.0f, 0.0f},
            .end{0.0f, 1.0f, 0.0f},
            .radiusStart = 0.30f,
            .radiusEnd = 0.20f,
            .depth = 0u,
            .sourceSegment = 0u};
        geometry.segments[1] = Segment{
            .start{0.0f, 1.0f, 0.0f},
            .end{0.8f, 1.7f, 0.2f},
            .radiusStart = 0.20f,
            .radiusEnd = 0.07f,
            .depth = 1u,
            .sourceSegment = 1u};
        geometry.leafCount = 1u;
        geometry.leaves[0] = Leaf{
            .position{0.8f, 1.7f, 0.2f},
            .outward{1.0f, 0.0f, 0.0f},
            .up{0.0f, 1.0f, 0.0f},
            .size = 0.42f,
            .sourceSegment = 1u,
            .variant = 0u};

        const IndexedMesh first = build(geometry);
        const IndexedMesh second = build(geometry);
        bool indicesValid = first.valid;
        bool normalsValid = first.valid;
        bool uvValid = first.valid;
        bool materialsValid = false;
        for (const std::uint32_t index : first.indices)
            indicesValid = indicesValid && index < first.vertices.size();
        for (const Vertex& vertex : first.vertices)
        {
            const float normalLength = detail::length(vertex.normal);
            normalsValid = normalsValid
                && std::abs(normalLength - 1.0f) < 0.001f;
            uvValid = uvValid
                && vertex.uv.x >= 0.0f && vertex.uv.x <= 1.0f
                && vertex.uv.y >= 0.0f && vertex.uv.y <= 1.0f;
            materialsValid = materialsValid
                || vertex.material == MaterialSlot::foliage;
        }

        const std::size_t firstStartRing = 0u;
        const std::size_t firstEndRing = kRadialSides + 1u;
        const float startRingRadius = detail::length(detail::subtract(
            first.vertices[firstStartRing].position, {0.0f, 0.0f, 0.0f}));
        const float endRingRadius = detail::length(detail::subtract(
            first.vertices[firstEndRing].position, {0.0f, 1.0f, 0.0f}));
        bool leafOrientation = first.valid;
        const std::size_t leafStart = first.vertices.size() - 8u;
        for (std::size_t index = leafStart; index < first.vertices.size(); ++index)
        {
            leafOrientation = leafOrientation
                && std::abs(first.vertices[index].position.x - 0.8f) < 0.0001f
                && std::abs(std::abs(first.vertices[index].normal.x) - 1.0f) < 0.0001f;
        }

        Geometry overBudget{};
        overBudget.segmentCount = overBudget.segments.size() + 1u;
        const IndexedMesh refused = build(overBudget);
        return ContractReport{
            .deterministic = first.valid
                && first.contentSignature != 0u
                && first.contentSignature == second.contentSignature,
            .taperedEndpointRadii = std::abs(startRingRadius - 0.30f) < 0.0001f
                && std::abs(endRingRadius - 0.20f) < 0.0001f
                && endRingRadius < startRingRadius,
            .connectedCapsSuppressed = first.suppressedInternalCaps == 2u,
            .rootAndTerminalCapsPresent = first.rootCaps == 1u
                && first.terminalCaps == 1u,
            .indexedWindingAndNormals = indicesValid && normalsValid,
            .uvBoundsAndMaterials = uvValid && materialsValid
                && first.bounds.valid,
            .terminalOrientedLeafCard = leafOrientation
                && first.leafCardCount == 1u,
            .strictBudgetRefusal = !refused.valid && refused.budgetExceeded,
            .productionProjectionAdmission = projection_admitted(geometry)
                && !projection_admitted(overBudget)
                && segment_projection_admitted(1.0f, 0.20f, 0.08f)
                && !segment_projection_admitted(0.0f, 0.20f, 0.08f)
                && !segment_projection_admitted(1.0f, 0.0f, 0.08f)};
    }
}
