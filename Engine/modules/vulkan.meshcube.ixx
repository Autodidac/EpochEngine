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

#include <array>
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

    // Keep data local to this partition (NOT exported as symbols)
    [[nodiscard]] inline std::vector<Vertex> build_preview_vertices(
        const epochnamespace::core::Context* ctx)
    {
        std::vector<Vertex> out{};
        const auto source = epochnamespace::previewgrid::grid_vertices();
        const auto markerVertices = epochnamespace::previewgrid::look_marker_vertices_for(ctx);
        const std::size_t markerCount = epochnamespace::previewgrid::look_marker_vertex_count_for(ctx);
        out.reserve(source.size() + markerCount);

        for (const auto& vertex : source)
        {
            out.push_back(Vertex{
                { vertex.position.x, vertex.position.y, vertex.position.z },
                { vertex.color.x, vertex.color.y, vertex.color.z },
                { 0.0f, 0.0f }
            });
        }

        for (std::size_t i = 0; i < markerCount; ++i)
        {
            out.push_back(Vertex{
                { markerVertices[i].position.x, markerVertices[i].position.y, markerVertices[i].position.z },
                { markerVertices[i].color.x, markerVertices[i].color.y, markerVertices[i].color.z },
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
        out.reserve(source.size() + 8u);

        for (const auto index : source)
            out.push_back(static_cast<std::uint16_t>(index));

        const std::size_t markerCount = epochnamespace::previewgrid::look_marker_vertex_count_for(ctx);
        if (markerCount == 0)
            return out;

        const std::uint16_t baseVertex =
            static_cast<std::uint16_t>(epochnamespace::previewgrid::grid_vertices().size());
        for (std::uint16_t i = 0; i < static_cast<std::uint16_t>(markerCount); ++i)
            out.push_back(static_cast<std::uint16_t>(baseVertex + i));

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
