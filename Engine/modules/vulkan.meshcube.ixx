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


namespace epochnamespace::vulkancontext
{
    using Vertex = Application::Vertex;

    namespace
    {
        [[nodiscard]] inline float preview_color_to_vulkan(float value) noexcept
        {
            return std::pow((std::clamp)(value, 0.0f, 1.0f), 2.2f);
        }

        [[nodiscard]] inline std::array<float, 3> preview_color_to_vulkan(
            const epochnamespace::previewgrid::Vec3& color) noexcept
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
        const epochnamespace::core::Context* ctx)
    {
        std::vector<Vertex> out{};
        const auto source = epochnamespace::previewgrid::grid_vertices();
        const auto markerVertices = epochnamespace::previewgrid::look_marker_vertices_for(ctx);
        const std::size_t markerCount = epochnamespace::previewgrid::look_marker_vertex_count_for(ctx);
        const auto objectVertices = epochnamespace::previewgrid::object_marker_vertices_for(ctx);
        out.reserve(source.size() + markerCount + objectVertices.size());

        for (const auto& vertex : source)
        {
            const auto color = preview_color_to_vulkan(vertex.color);
            out.push_back(Vertex{
                { vertex.position.x, vertex.position.y, vertex.position.z },
                { color[0], color[1], color[2] },
                { 0.0f, 0.0f }
            });
        }

        for (std::size_t i = 0; i < markerCount; ++i)
        {
            const auto color = preview_color_to_vulkan(markerVertices[i].color);
            out.push_back(Vertex{
                { markerVertices[i].position.x, markerVertices[i].position.y, markerVertices[i].position.z },
                { color[0], color[1], color[2] },
                { 0.0f, 0.0f }
            });
        }

        for (const auto& vertex : objectVertices)
        {
            const auto color = preview_color_to_vulkan(vertex.color);
            out.push_back(Vertex{
                { vertex.position.x, vertex.position.y, vertex.position.z },
                { color[0], color[1], color[2] },
                { 0.0f, 0.0f }
            });
        }

        return out;
    }

    [[nodiscard]] inline std::vector<std::uint16_t> build_preview_indices(
        const epochnamespace::core::Context* ctx)
    {
        std::vector<std::uint16_t> out{};
        const auto source = epochnamespace::previewgrid::grid_indices();
        const std::size_t markerCount = epochnamespace::previewgrid::look_marker_vertex_count_for(ctx);
        const auto objectVertices = epochnamespace::previewgrid::object_marker_vertices_for(ctx);
        out.reserve(source.size() + markerCount + objectVertices.size());

        for (const auto index : source)
            out.push_back(static_cast<std::uint16_t>(index));

        const std::uint16_t baseVertex =
            static_cast<std::uint16_t>(epochnamespace::previewgrid::grid_vertices().size());
        for (std::uint16_t i = 0; i < static_cast<std::uint16_t>(markerCount); ++i)
            out.push_back(static_cast<std::uint16_t>(baseVertex + i));

        const std::uint16_t objectBase =
            static_cast<std::uint16_t>(baseVertex + markerCount);
        for (std::uint16_t i = 0; i < static_cast<std::uint16_t>(objectVertices.size()); ++i)
            out.push_back(static_cast<std::uint16_t>(objectBase + i));

        return out;
    }

    export std::vector<Vertex> preview_vertices_for(const epochnamespace::core::Context* ctx)
    {
        return build_preview_vertices(ctx);
    }

    export std::vector<std::uint16_t> preview_indices_for(const epochnamespace::core::Context* ctx)
    {
        return build_preview_indices(ctx);
    }
}
