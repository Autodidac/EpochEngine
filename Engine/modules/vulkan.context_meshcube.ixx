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
// modules/acontext.vulkan.context_meshcube.ixx
// Partition: vulkan.context:meshcube
// Provides cube mesh data without polluting the context BMI.
// ============================================================================
module;

#include <compare>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

export module vulkan.context:meshcube;

import :shared_vk;
import render.arcade;
import render.preview_grid;


namespace epochengine::vulkancontext
{
    using Vertex = Application::Vertex;

    inline constexpr std::uint32_t kArcadeScreenVertexCount = 4u;
    inline constexpr std::uint32_t kArcadeScreenIndexCount = 6u;
    static_assert(kArcadeScreenVertexCount == 4u && kArcadeScreenIndexCount == 6u);

    export bool preview_indices_are_sequential(std::span<const std::uint32_t> indices) noexcept
    {
        for (std::size_t index = 0; index < indices.size(); ++index)
            if (indices[index] != index) return false;
        return true;
    }

    export bool preview_geometry_matches(const previewgrid::PublishedSceneFrame& current,
        const previewgrid::PublishedSceneFrame& uploaded) noexcept
    {
        // Packet revisions include camera/publication changes. Upload identity is
        // the immutable baked geometry, including reference lighting and textures.
        if (!current.solids || !uploaded.solids || !current.wire || !uploaded.wire
            || current.solids->geometry != uploaded.solids->geometry
            || current.lines != uploaded.lines
            || current.wire->depth_triangles != uploaded.wire->depth_triangles
            || current.wire->helper_lines != uploaded.wire->helper_lines)
            return false;
        const auto same_vector = [](const previewgrid::Vec3& a, const previewgrid::Vec3& b)
        { return a.x == b.x && a.y == b.y && a.z == b.z; };
        // CPU wire generation uses only eye position for outward-face admission;
        // orientation/projection are applied later by the always-updated uniform.
        if (current.wire->depth_triangles && !current.wire->depth_triangles->empty()
            && !same_vector(current.camera.eye, uploaded.camera.eye))
            return false;
        const auto first_surface = [](const previewgrid::PublishedSceneFrame& frame)
            -> const previewgrid::ObjectMarker*
        {
            for (const auto& marker : frame.markers)
            {
                if (!marker.sampledRenderSurface
                    || !previewgrid::object_preview_geometry_route(marker.primitive).sampled_surface)
                    continue;
                if (marker.primitive == previewgrid::ObjectPreviewPrimitive::EngineArcadeScreen
                    && !render_arcade::screen_sample_plane_faces_viewer(
                        marker.position.z, marker.scale.z, frame.camera.eye.z))
                    continue;
                return &marker;
            }
            return nullptr;
        };
        const auto* a = first_surface(current);
        const auto* b = first_surface(uploaded);
        // Only the first admitted sampled surface is baked by this backend.
        return (!a && !b) || (a && b && same_vector(a->position, b->position)
            && same_vector(a->scale, b->scale));
    }

    [[nodiscard]] inline float preview_color_to_vulkan(float value) noexcept
    {
        return std::pow((std::clamp)(value, 0.0f, 1.0f), 2.2f);
    }

    [[nodiscard]] inline std::array<float, 3> preview_color_to_vulkan(
        const epochengine::previewgrid::Vec3& color) noexcept
    {
        return {
            preview_color_to_vulkan(color.x),
            preview_color_to_vulkan(color.y),
            preview_color_to_vulkan(color.z)
        };
    }

    // Keep data local to this partition (NOT exported as symbols)
    [[nodiscard]] inline std::vector<Vertex> build_preview_vertices(
        const epochengine::core::Context* ctx,
        const std::shared_ptr<const previewgrid::PublishedSceneFrame>& matchedScene)
    {
        std::vector<Vertex> out{};
        const auto solidGeometry = matchedScene ? matchedScene->solids->geometry
            : epochengine::previewgrid::object_solid_geometry_for(ctx);
        const auto& solidVertices = *solidGeometry;
        static const previewgrid::GridGeometry emptyGrid{};
        const auto gridGeometry = matchedScene ? std::shared_ptr<const previewgrid::GridGeometry>{}
            : epochengine::previewgrid::grid_geometry_for(ctx);
        const auto& source = gridGeometry ? gridGeometry->vertices : emptyGrid.vertices;
        const auto markerVertices = matchedScene ? std::array<previewgrid::Vertex, 8>{}
            : epochengine::previewgrid::look_marker_vertices_for(ctx);
        const std::size_t markerCount = matchedScene ? 0u
            : epochengine::previewgrid::look_marker_vertex_count_for(ctx);
        const auto objectGeometry = matchedScene ? matchedScene->lines
            : epochengine::previewgrid::object_marker_geometry_for(ctx);
        const auto& objectVertices = *objectGeometry;
        const auto meshWire = matchedScene ? matchedScene->wire
            : previewgrid::object_mesh_wire_frame_for(ctx);
        const auto camera = matchedScene ? matchedScene->camera : previewgrid::camera_for(ctx);
        const auto sampledSurfaces = matchedScene ? previewgrid::sampled_render_surface_markers_for(*matchedScene)
            : previewgrid::sampled_render_surface_markers_for(ctx);
        const bool hasArcadeScreen = !sampledSurfaces.empty();
        out.reserve(solidVertices.size() + (hasArcadeScreen ? kArcadeScreenVertexCount : 0u)
            + source.size() + markerCount + objectVertices.size());

        // Indexed meshes expand shared vertices into this upload. Reuse exact
        // RGB conversions within one build; collisions recompute, never quantize
        // lighting or retain colors from another immutable scene frame.
        struct ConvertedColor final
        {
            std::array<std::uint32_t, 3> key{};
            std::array<float, 3> linear{};
            bool valid{};
        };
        std::array<ConvertedColor, 64> convertedColors{};
        const auto append_vertex = [&](const epochengine::previewgrid::Vertex& vertex)
        {
            const std::array<std::uint32_t, 3> key{
                std::bit_cast<std::uint32_t>(vertex.color.x),
                std::bit_cast<std::uint32_t>(vertex.color.y),
                std::bit_cast<std::uint32_t>(vertex.color.z)};
            const auto mixed = key[0] ^ std::rotl(key[1], 11) ^ std::rotl(key[2], 22);
            auto& converted = convertedColors[(mixed ^ (mixed >> 16u)) & (convertedColors.size() - 1u)];
            if (!converted.valid || converted.key != key)
            {
                converted.key = key;
                converted.linear = preview_color_to_vulkan(vertex.color);
                converted.valid = true;
            }
            const auto& color = converted.linear;
            out.push_back(Vertex{
                { vertex.position.x, vertex.position.y, vertex.position.z },
                { color[0], color[1], color[2] },
                { 0.0f, 0.0f }
            });
        };

        for (const auto& vertex : solidVertices)
            append_vertex(vertex);

        if (hasArcadeScreen)
        {
            const auto& marker = sampledSurfaces.front();
            const float halfX = (std::max)(std::abs(marker.scale.x) * 0.5f, 0.25f);
            const float halfY = (std::max)(std::abs(marker.scale.y) * 0.5f, 0.18f);
            const float z =
                epochengine::render_arcade::screen_sample_plane_z(
                    marker.position.z, marker.scale.z);
            const std::array<epochengine::previewgrid::Vec3, kArcadeScreenVertexCount> positions{
                epochengine::previewgrid::Vec3{ marker.position.x - halfX, marker.position.y - halfY, z },
                epochengine::previewgrid::Vec3{ marker.position.x + halfX, marker.position.y - halfY, z },
                epochengine::previewgrid::Vec3{ marker.position.x + halfX, marker.position.y + halfY, z },
                epochengine::previewgrid::Vec3{ marker.position.x - halfX, marker.position.y + halfY, z }
            };
            const std::array<std::array<float, 2>, kArcadeScreenVertexCount> uvs{
                std::array<float, 2>{ 0.0f, 1.0f },
                std::array<float, 2>{ 1.0f, 1.0f },
                std::array<float, 2>{ 1.0f, 0.0f },
                std::array<float, 2>{ 0.0f, 0.0f }
            };
            for (std::size_t index = 0; index < positions.size(); ++index)
            {
                out.push_back(Vertex{
                    { positions[index].x, positions[index].y, positions[index].z },
                    { 1.0f, 1.0f, 1.0f },
                    { uvs[index][0], uvs[index][1] }
                });
            }
        }
        for (const auto& vertex : source)
            append_vertex(vertex);
        for (std::size_t i = 0; i < markerCount; ++i)
            append_vertex(markerVertices[i]);
        previewgrid::for_each_visible_marker_line(*meshWire, objectVertices, camera.eye,
            [&](const auto& first,const auto& second){append_vertex(first);append_vertex(second);});

        return out;
    }

    [[nodiscard]] inline std::vector<std::uint32_t> build_preview_indices(
        const epochengine::core::Context* ctx,
        const std::shared_ptr<const previewgrid::PublishedSceneFrame>& matchedScene)
    {
        std::vector<std::uint32_t> out{};
        const auto solidGeometry = matchedScene ? matchedScene->solids->geometry
            : epochengine::previewgrid::object_solid_geometry_for(ctx);
        const auto& solidVertices = *solidGeometry;
        static const previewgrid::GridGeometry emptyGrid{};
        const auto gridGeometry = matchedScene ? std::shared_ptr<const previewgrid::GridGeometry>{}
            : epochengine::previewgrid::grid_geometry_for(ctx);
        const auto& sourceVertices = gridGeometry ? gridGeometry->vertices : emptyGrid.vertices;
        const auto& sourceIndices = gridGeometry ? gridGeometry->indices : emptyGrid.indices;
        const std::size_t markerCount = matchedScene ? 0u
            : epochengine::previewgrid::look_marker_vertex_count_for(ctx);
        const auto objectGeometry = matchedScene ? matchedScene->lines
            : epochengine::previewgrid::object_marker_geometry_for(ctx);
        const auto& objectVertices = *objectGeometry;
        const auto meshWire = matchedScene ? matchedScene->wire
            : previewgrid::object_mesh_wire_frame_for(ctx);
        const auto camera = matchedScene ? matchedScene->camera : previewgrid::camera_for(ctx);
        std::uint64_t visibleObjectVertexCount{};
        previewgrid::for_each_visible_marker_line(*meshWire, objectVertices, camera.eye,
            [&](const auto&,const auto&){visibleObjectVertexCount+=2u;});
        const bool hasArcadeScreen =
            !(matchedScene ? previewgrid::sampled_render_surface_markers_for(*matchedScene)
                : previewgrid::sampled_render_surface_markers_for(ctx)).empty();
        const std::uint64_t vertexCount = static_cast<std::uint64_t>(solidVertices.size())
            + (hasArcadeScreen ? kArcadeScreenVertexCount : 0u)
            + sourceVertices.size() + markerCount + visibleObjectVertexCount;
        const std::uint64_t indexCount = static_cast<std::uint64_t>(solidVertices.size())
            + (hasArcadeScreen ? kArcadeScreenIndexCount : 0u)
            + sourceIndices.size() + markerCount + visibleObjectVertexCount;
        if (vertexCount > (std::numeric_limits<std::uint32_t>::max)()
            || indexCount > (std::numeric_limits<std::uint32_t>::max)())
            throw std::length_error("Vulkan preview exceeds 32-bit draw capacity.");
        for (const auto index : sourceIndices)
            if (index >= sourceVertices.size())
                throw std::out_of_range("Vulkan preview grid index is outside its vertices.");
        out.reserve(static_cast<std::size_t>(indexCount));

        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(solidVertices.size()); ++i)
            out.push_back(i);

        const std::uint32_t screenBase = static_cast<std::uint32_t>(solidVertices.size());
        if (hasArcadeScreen)
        {
            constexpr std::array<std::uint32_t, kArcadeScreenIndexCount> screenIndices{
                0u, 1u, 2u, 0u, 2u, 3u
            };
            for (const std::uint32_t index : screenIndices)
                out.push_back(static_cast<std::uint32_t>(screenBase + index));
        }

        const std::uint32_t lineBase = static_cast<std::uint32_t>(
            screenBase + (hasArcadeScreen ? kArcadeScreenVertexCount : 0u));
        for (const auto index : sourceIndices)
            out.push_back(static_cast<std::uint32_t>(lineBase + index));

        const std::uint32_t markerBase = static_cast<std::uint32_t>(lineBase + sourceVertices.size());
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(markerCount); ++i)
            out.push_back(static_cast<std::uint32_t>(markerBase + i));

        const std::uint32_t objectBase = static_cast<std::uint32_t>(markerBase + markerCount);
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(visibleObjectVertexCount); ++i)
            out.push_back(static_cast<std::uint32_t>(objectBase + i));

        return out;
    }
    export std::vector<Vertex> preview_vertices_for(const epochengine::core::Context* ctx,
        const std::shared_ptr<const previewgrid::PublishedSceneFrame>& matchedScene)
    {
        return build_preview_vertices(ctx, matchedScene);
    }

    export std::vector<std::uint32_t> preview_indices_for(const epochengine::core::Context* ctx,
        const std::shared_ptr<const previewgrid::PublishedSceneFrame>& matchedScene)
    {
        return build_preview_indices(ctx, matchedScene);
    }
    export std::uint32_t preview_solid_index_count_for(const epochengine::core::Context* ctx,
        const std::shared_ptr<const previewgrid::PublishedSceneFrame>& matchedScene)
    {
        return static_cast<std::uint32_t>((matchedScene ? matchedScene->solids->geometry
            : previewgrid::object_solid_geometry_for(ctx))->size());
    }
    export std::uint32_t preview_arcade_screen_index_count_for(const epochengine::core::Context* ctx,
        const std::shared_ptr<const previewgrid::PublishedSceneFrame>& matchedScene)
    {
        return (matchedScene ? previewgrid::sampled_render_surface_markers_for(*matchedScene)
            : previewgrid::sampled_render_surface_markers_for(ctx)).empty()
            ? 0u
            : kArcadeScreenIndexCount;
    }
}
