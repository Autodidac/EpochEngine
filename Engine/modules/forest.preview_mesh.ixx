// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

export module forest.preview_mesh;

import authoring.morphology;
import voxel.field;
import asset.texture_artifact;

export namespace epochengine::forest::preview_mesh
{
    inline constexpr std::uint32_t kRadialSides = 8u;
    inline constexpr std::size_t kMaximumSegments = 256u;
    inline constexpr std::size_t kMaximumLeaves = 384u;
    inline constexpr std::size_t kMaximumVertices =
        kMaximumSegments * 28u + kMaximumLeaves * 12u;
    inline constexpr std::size_t kMaximumIndices =
        kMaximumSegments * 96u + kMaximumLeaves * 24u;

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

    // Two padded tiles share one ordinary base-color material. Keeping the
    // slots separate prevents trunk bark from being sampled by the leaf cards.
    [[nodiscard]] constexpr Vector2 material_uv(MaterialSlot slot, Vector2 uv) noexcept
    {
        return {(4.5f + (slot == MaterialSlot::foliage ? 128.0f : 0.0f)
                    + (std::clamp)(uv.x, 0.0f, 1.0f) * 119.0f) / 256.0f,
                slot == MaterialSlot::wood ? uv.y : (4.5f + (1.0f - (std::clamp)(uv.y, 0.0f, 1.0f)) * 119.0f) / 128.0f};
    }

    inline constexpr std::uint32_t kBarkTileCount = 16u;
    // Each choice has its own padded atlas, so repeating bark never samples
    // another style. Zero is the original/legacy bark choice.
    [[nodiscard]] const asset::texture::CompiledTextureArtifact& material_atlas(
        std::uint32_t barkTileIndex = 0u);
    // False means the packaged source artwork is unavailable; fallback is explicit.
    [[nodiscard]] bool reference_material_available();

    struct MaterialAtlasResult final
    {
        asset::texture::CompiledTextureArtifact artifact{};
        std::string diagnostic{};
        [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(artifact); }
    };
    // Reuses the same padded two-slot material. Inputs are exact project
    // artifacts, never files or model outputs inferred by a filename.
    [[nodiscard]] MaterialAtlasResult compose_material_atlas(
        const asset::texture::CompiledTextureArtifact* bark,
        const asset::texture::CompiledTextureArtifact* foliage,
        std::uint32_t barkTileIndex,
        std::array<float, 3u> fallbackWoodTint,
        std::array<float, 3u> fallbackFoliageTint);

    // A placed plant keeps its authored root at the instance pivot. Rotation
    // aligns local +Y with gravity-up; local +Z follows the projected heading.
    // No planet-scale coordinate is baked into the reusable plant mesh.
    [[nodiscard]] inline std::optional<std::array<float,3>> surface_rotation(
        std::array<double,3> up,std::array<double,3> heading) noexcept
    {
        const auto normalize=[](std::array<double,3>& value){
            double square{};for(const auto v:value){if(!std::isfinite(v))return false;square+=v*v;}
            if(!std::isfinite(square)||square<1e-20)return false;
            const auto length=std::sqrt(square);for(auto& v:value)v/=length;return true;
        };
        const auto cross=[](const auto& a,const auto& b){return std::array<double,3>{
            a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
        if(!normalize(up)||!normalize(heading))return {};
        auto right=cross(up,heading);
        if(!normalize(right))
        {
            heading=std::abs(up[1])<0.9?std::array<double,3>{0,1,0}:std::array<double,3>{1,0,0};
            right=cross(up,heading);if(!normalize(right))return {};
        }
        auto forward=cross(right,up);if(!normalize(forward))return {};
        // Same Rx*Ry*Rz convention as the ordinary scene transform.
        const double y=std::asin((std::clamp)(forward[0],-1.0,1.0));
        const bool pole=std::abs(std::cos(y))<1e-8;
        const double x=pole?std::atan2(up[2],up[1]):std::atan2(-forward[1],forward[2]);
        const double z=pole?0.0:std::atan2(-up[0],right[0]);
        constexpr double degrees=57.295779513082320876;
        return std::array<float,3>{float(x*degrees),float(y*degrees),float(z*degrees)};
    }

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
        std::uint32_t overlappingJunctions{};
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

        [[nodiscard]] inline Vector3 leaf_attachment(
            Vector3 authored, const Segment& segment,
            std::span<const Vertex> rings) noexcept
        {
            constexpr std::size_t ringSize = kRadialSides + 1u;
            if (rings.size() < ringSize * 2u) return authored;
            const auto start = convert(segment.start);
            const auto axis = subtract(convert(segment.end), start);
            const auto square = dot(axis, axis);
            if (!(square > 0.000000000001f)) return authored;
            const float t = (std::clamp)(dot(subtract(authored, start), axis) / square, 0.0f, 1.0f);
            auto radial = subtract(authored, add(start, scale(axis, t)));
            // Legacy terminal leaves originate at the authored branch end.
            if (!(dot(radial, radial) > 0.000000000001f)) return authored;
            Vector3 first{}, last{};
            for (std::size_t side = 0u; side < kRadialSides; ++side)
            {
                first = add(first, scale(rings[side].position, 1.0f / kRadialSides));
                last = add(last, scale(rings[ringSize + side].position, 1.0f / kRadialSides));
            }
            const auto tangent = normalize(subtract(last, first));
            radial = subtract(radial, scale(tangent, dot(radial, tangent)));
            const auto direction = normalize(radial);
            const auto center = add(first, scale(subtract(last, first), t));
            float distance = (std::numeric_limits<float>::max)();
            // Intersect the attachment ray with the actual octagonal branch,
            // after its junction fitting/inset. Its radius policy has one owner.
            for (std::size_t side = 0u; side < kRadialSides; ++side)
            {
                const auto point = [&](std::size_t index) {
                    const auto a = rings[index].position;
                    return add(a, scale(subtract(rings[ringSize + index].position, a), t));
                };
                const auto a = point(side), b = point(side + 1u);
                const auto normal = cross(subtract(b, a), tangent);
                const float denominator = dot(normal, direction);
                if (!(denominator > 0.000000000001f)) continue;
                const float hit = dot(normal, subtract(a, center)) / denominator;
                if (std::isfinite(hit) && hit > 0.0f) distance = (std::min)(distance, hit);
            }
            if (distance == (std::numeric_limits<float>::max)()) return authored;
            return add(center, scale(direction, distance * 0.98f));
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
            hash = mix(hash, mesh.overlappingJunctions);
            return hash == 0u ? 1u : hash;
        }
    }

    [[nodiscard]] Geometry extract(
        const epochengine::authoring::morphology::Graph& graph,
        const epochengine::authoring::morphology::Sample& sample) noexcept;

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
            geometry.segmentCount * 22u + geometry.leafCount * 12u));
        mesh.indices.reserve((std::min)(
            kMaximumIndices,
            geometry.segmentCount * 72u + geometry.leafCount * 24u));

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
        std::array<std::size_t, kMaximumSegments> segmentRings{};

        for (std::size_t segmentIndex = 0u;
            segmentIndex < geometry.segmentCount; ++segmentIndex)
        {
            const Segment& source = geometry.segments[segmentIndex];
            Vector3 start = detail::convert(source.start);
            const Vector3 end = detail::convert(source.end);
            Vector3 axis = detail::subtract(end, start);
            float axisLength = detail::length(axis);
            float startRadius = source.radiusStart;
            float endRadius = source.radiusEnd;
            if (!detail::finite(start) || !detail::finite(end)
                || !(axisLength > 0.000001f)
                || !std::isfinite(startRadius) || !std::isfinite(endRadius)
                || !(startRadius > 0.0f) || !(endRadius > 0.0f))
            {
                return {};
            }

            bool rootStart = true;
            bool terminalEnd = true;
            const Segment* parent = nullptr;
            for (std::size_t otherIndex = 0u;
                otherIndex < geometry.segmentCount; ++otherIndex)
            {
                if (otherIndex == segmentIndex)
                    continue;
                const Segment& other = geometry.segments[otherIndex];
                if (detail::same_point(start, detail::convert(other.end)))
                {
                    rootStart = false;
                    if (!parent) parent = &other;
                }
                terminalEnd = terminalEnd
                    && !detail::same_point(end, detail::convert(other.start));
            }

            if (parent)
            {
                // Independently oriented branch rings do not form a shared
                // surface. Bury the child root ring in the parent volume.
                // Internal disks would be exposed across branch junctions.
                // The authored node/endpoints and graph remain unchanged.
                const auto parentAxis = detail::subtract(
                    detail::convert(parent->end), detail::convert(parent->start));
                const float parentLength = detail::length(parentAxis);
                const float parentRadius = (std::min)(parent->radiusStart, parent->radiusEnd);
                if (!std::isfinite(parentLength) || !(parentLength > 0.000001f)
                    || !std::isfinite(parentRadius) || !(parentRadius > 0.0f)) return {};
                const float inset = (std::min)(parentLength * 0.25f, parentRadius * 0.75f);
                start = detail::subtract(start,
                    detail::scale(detail::normalize(parentAxis), inset));
                const float fittedStartRadius = (std::min)(startRadius, parentRadius * 0.60f);
                // Fit the whole child cross-section, preserving its authored taper.
                // Shrinking only the buried ring made .72-decay branches widen
                // towards their tips after fitting their roots to .60 of the parent.
                endRadius *= fittedStartRadius / startRadius;
                startRadius = fittedStartRadius;
                axis = detail::subtract(end, start);
                axisLength = detail::length(axis);
                if (!(axisLength > 0.000001f) || !std::isfinite(axisLength)) return {};
            }

            const std::size_t capCount = static_cast<std::size_t>(rootStart)
                + static_cast<std::size_t>(terminalEnd);
            if (parent) ++mesh.overlappingJunctions;
            const std::size_t addedVertices =
                static_cast<std::size_t>(kRadialSides + 1u) * (terminalEnd ? 4u : 2u)
                + capCount * (kRadialSides + 1u);
            const std::size_t addedIndices =
                static_cast<std::size_t>(kRadialSides) * 6u * (terminalEnd ? 3u : 1u)
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
            // A terminal bud tapers beyond the authored node; the node remains the
            // attachment point for its leaves. Connected nodes get no tip.
            const std::array<float, 4> ringDistances{
                0.0f, axisLength, axisLength + endRadius * 0.95f,
                axisLength + endRadius * 1.33f};
            const std::array<float, 4> ringRadii{
                startRadius, endRadius, endRadius * 0.46f, endRadius * 0.12f};
            const std::uint32_t ringCount = terminalEnd ? 4u : 2u;
            const std::size_t startRing = mesh.vertices.size();
            segmentRings[segmentIndex] = startRing;
            constexpr float tau = 6.28318530717958647692f;
            for (std::uint32_t ring = 0u; ring < ringCount; ++ring)
            {
                const Vector3 center = detail::add(start,
                    detail::scale(tangent, ringDistances[ring]));
                const float radius = ringRadii[ring];
                const auto slopeBand = (std::min)(ring, ringCount - 2u);
                const float taperSlope = (ringRadii[slopeBand] - ringRadii[slopeBand + 1u])
                    / (ringDistances[slopeBand + 1u] - ringDistances[slopeBand]);
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
                        .uv{fraction, ringDistances[ring]},
                        .material = MaterialSlot::wood});
                }
            }

            for (std::uint32_t band = 0u; band + 1u < ringCount; ++band)
            {
                const std::size_t lower = startRing + band * (kRadialSides + 1u);
                const std::size_t upper = lower + kRadialSides + 1u;
                for (std::uint32_t side = 0u; side < kRadialSides; ++side)
                {
                    pushIndex(lower + side); pushIndex(upper + side + 1u); pushIndex(upper + side);
                    pushIndex(lower + side); pushIndex(lower + side + 1u); pushIndex(upper + side + 1u);
                }
            }

            const auto appendCap = [&](Vector3 center, float radius, bool reverse)
            {
                // Cap vertices have planar normals and radial UVs; sharing the
                // cylindrical rim stretches the texture and smooths the cut face.
                const auto normal = detail::scale(tangent, reverse ? -1.0f : 1.0f);
                const std::size_t centerIndex = mesh.vertices.size();
                pushVertex(Vertex{.position = center, .normal = normal,
                    .uv{0.5f, 0.5f}, .material = MaterialSlot::wood});
                for (std::uint32_t side = 0u; side < kRadialSides; ++side)
                {
                    const float angle = tau * static_cast<float>(side) / static_cast<float>(kRadialSides);
                    const float u = std::cos(angle), v = std::sin(angle);
                    const auto radial = detail::add(detail::scale(ringU, u), detail::scale(ringV, v));
                    pushVertex(Vertex{.position = detail::add(center, detail::scale(radial, radius)),
                        .normal = normal, .uv{0.5f + u * 0.5f, 0.5f + v * 0.5f},
                        .material = MaterialSlot::wood});
                }
                for (std::uint32_t side = 0u; side < kRadialSides; ++side)
                {
                    const std::size_t first = centerIndex + 1u + side;
                    const std::size_t next = centerIndex + 1u + (side + 1u) % kRadialSides;
                    pushIndex(centerIndex);
                    pushIndex(reverse ? next : first);
                    pushIndex(reverse ? first : next);
                }
            };
            if (rootStart)
            {
                appendCap(start, startRadius, true);
                ++mesh.rootCaps;
            }
            if (terminalEnd)
            {
                appendCap(detail::add(start, detail::scale(tangent, ringDistances[3])),
                    ringRadii[3], false);
                ++mesh.terminalCaps;
            }
            ++mesh.segmentCount;
        }

        for (std::size_t leafIndex = 0u;
            leafIndex < geometry.leafCount; ++leafIndex)
        {
            if (!roomFor(12u, 24u))
            {
                mesh.budgetExceeded = true;
                mesh.valid = false;
                return mesh;
            }
            const Leaf& leaf = geometry.leaves[leafIndex];
            // Terminal position is the stem attachment; growing leaves stay attached.
            Vector3 base = detail::convert(leaf.position);
            for (std::size_t segmentIndex = 0u; segmentIndex < geometry.segmentCount; ++segmentIndex)
            {
                const auto& segment = geometry.segments[segmentIndex];
                if (segment.sourceSegment != leaf.sourceSegment) continue;
                base = detail::leaf_attachment(base, segment,
                    std::span<const Vertex>{mesh.vertices}.subspan(segmentRings[segmentIndex],
                        (kRadialSides + 1u) * 2u));
                break;
            }
            const Vector3 outward = detail::normalize(
                detail::convert(leaf.outward), {1.0f, 0.0f, 0.0f});
            Vector3 up = detail::convert(leaf.up);
            up = detail::subtract(up, detail::scale(outward, detail::dot(up, outward)));
            up = detail::normalize(up, {0.0f, 1.0f, 0.0f});
            Vector3 side = detail::normalize(
                detail::cross(up, outward), {0.0f, 0.0f, 1.0f});
            const float halfWidth = (std::max)(0.01f, leaf.size * 0.50f);
            const float halfHeight = (std::max)(0.015f, leaf.size * 0.72f);
            // Pointed silhouette is actual geometry, so opaque and fallback
            // backends never draw the old rectangular leaf card corners.
            const std::array<Vector2, 6> uvs = reference_material_available()
                ? std::array<Vector2,6>{{{0.5f,0.0f},{1.0f,0.0f},{1.0f,1.0f},
                    {0.5f,1.0f},{0.0f,1.0f},{0.0f,0.0f}}}
                : std::array<Vector2,6>{{{0.5f, 0.0f}, {1.0f, 0.30f}, {0.82f, 0.72f},
                    {0.5f, 1.0f}, {0.18f, 0.72f}, {0.0f, 0.30f}}};
            std::array<Vector3, 6> positions{};
            for (std::size_t corner = 0; corner < positions.size(); ++corner)
                positions[corner] = detail::add(base, detail::add(
                    detail::scale(side, (uvs[corner].x * 2.0f - 1.0f) * halfWidth),
                    detail::scale(up, uvs[corner].y * 2.0f * halfHeight)));
            const std::size_t front = mesh.vertices.size();
            for (std::size_t corner = 0u; corner < positions.size(); ++corner)
                pushVertex(Vertex{positions[corner], outward, uvs[corner], MaterialSlot::foliage});
            const std::size_t back = mesh.vertices.size();
            for (std::size_t corner = 0u; corner < positions.size(); ++corner)
                pushVertex(Vertex{positions[corner], detail::scale(outward, -1.0f), uvs[corner], MaterialSlot::foliage});
            for (std::size_t corner = 1u; corner + 1u < positions.size(); ++corner)
            {
                pushIndex(front); pushIndex(front + corner); pushIndex(front + corner + 1u);
                pushIndex(back); pushIndex(back + corner + 1u); pushIndex(back + corner);
            }
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
        bool connectedJunctionsOverlap{};
        bool rootAndTerminalCapsPresent{};
        bool indexedWindingAndNormals{};
        bool uvBoundsAndMaterials{};
        bool terminalOrientedLeafCard{};
        bool strictBudgetRefusal{};
        bool productionProjectionAdmission{};

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return deterministic && taperedEndpointRadii
                && connectedJunctionsOverlap && rootAndTerminalCapsPresent
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
                && std::isfinite(vertex.uv.y) && vertex.uv.y >= 0.0f
                && (vertex.material == MaterialSlot::wood || vertex.uv.y <= 1.0f);
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
        const std::size_t leafStart = first.vertices.size() - 12u;
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
            .connectedJunctionsOverlap = first.overlappingJunctions == 1u,
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
