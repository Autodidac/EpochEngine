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
// modules/acontext.vulkan.meshcube.ixx
// Partition: vulkan.context:meshcube
// Provides cube mesh data without polluting the context BMI.
// ============================================================================
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

export module vulkan.context:meshcube;

import :shared_vk;
import render.preview_grid;


namespace epochengine::vulkancontext
{
    using Vertex = Application::Vertex;

    namespace
    {
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
    }

    // Keep data local to this partition (NOT exported as symbols)
    [[nodiscard]] inline std::vector<Vertex> build_preview_vertices(
        const epochengine::core::Context* ctx)
    {
        std::vector<Vertex> out{};
        const auto solidVertices = epochengine::previewgrid::object_solid_vertices_for(ctx);
        const auto source = epochengine::previewgrid::grid_vertices();
        const auto markerVertices = epochengine::previewgrid::look_marker_vertices_for(ctx);
        const std::size_t markerCount = epochengine::previewgrid::look_marker_vertex_count_for(ctx);
        const auto objectVertices = epochengine::previewgrid::object_marker_vertices_for(ctx);
        out.reserve(solidVertices.size() + source.size() + markerCount + objectVertices.size());

        const auto append_vertex = [&](const epochengine::previewgrid::Vertex& vertex)
        {
            const auto color = preview_color_to_vulkan(vertex.color);
            out.push_back(Vertex{
                { vertex.position.x, vertex.position.y, vertex.position.z },
                { color[0], color[1], color[2] },
                { 0.0f, 0.0f }
            });
        };

        for (const auto& vertex : solidVertices)
            append_vertex(vertex);
        for (const auto& vertex : source)
            append_vertex(vertex);
        for (std::size_t i = 0; i < markerCount; ++i)
            append_vertex(markerVertices[i]);
        for (const auto& vertex : objectVertices)
            append_vertex(vertex);

        return out;
    }

    [[nodiscard]] inline std::vector<std::uint16_t> build_preview_indices(
        const epochengine::core::Context* ctx)
    {
        std::vector<std::uint16_t> out{};
        const auto solidVertices = epochengine::previewgrid::object_solid_vertices_for(ctx);
        const auto sourceVertices = epochengine::previewgrid::grid_vertices();
        const auto sourceIndices = epochengine::previewgrid::grid_indices();
        const std::size_t markerCount = epochengine::previewgrid::look_marker_vertex_count_for(ctx);
        const auto objectVertices = epochengine::previewgrid::object_marker_vertices_for(ctx);
        out.reserve(solidVertices.size() + sourceIndices.size() + markerCount + objectVertices.size());

        for (std::uint16_t i = 0; i < static_cast<std::uint16_t>(solidVertices.size()); ++i)
            out.push_back(i);

        const std::uint16_t lineBase = static_cast<std::uint16_t>(solidVertices.size());
        for (const auto index : sourceIndices)
            out.push_back(static_cast<std::uint16_t>(lineBase + index));

        const std::uint16_t markerBase = static_cast<std::uint16_t>(lineBase + sourceVertices.size());
        for (std::uint16_t i = 0; i < static_cast<std::uint16_t>(markerCount); ++i)
            out.push_back(static_cast<std::uint16_t>(markerBase + i));

        const std::uint16_t objectBase = static_cast<std::uint16_t>(markerBase + markerCount);
        for (std::uint16_t i = 0; i < static_cast<std::uint16_t>(objectVertices.size()); ++i)
            out.push_back(static_cast<std::uint16_t>(objectBase + i));

        return out;
    }
    export std::vector<Vertex> preview_vertices_for(const epochengine::core::Context* ctx)
    {
        return build_preview_vertices(ctx);
    }

    export std::vector<std::uint16_t> preview_indices_for(const epochengine::core::Context* ctx)
    {
        return build_preview_indices(ctx);
    }
    export std::uint32_t preview_solid_index_count_for(const epochengine::core::Context* ctx)
    {
        return static_cast<std::uint32_t>(epochengine::previewgrid::object_solid_vertices_for(ctx).size());
    }
}
