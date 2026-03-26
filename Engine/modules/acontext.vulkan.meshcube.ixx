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
// Partition: acontext.vulkan.context:meshcube
// Provides cube mesh data without polluting the context BMI.
// ============================================================================

module;

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

export module acontext.vulkan.context:meshcube;

import :shared_vk;
import epoch.render.preview_grid;


namespace epochnamespace::vulkancontext
{
    using Vertex = Application::Vertex;

    // Keep data local to this partition (NOT exported as symbols)
    [[nodiscard]] inline const std::vector<Vertex>& kCubeVertices() noexcept
    {
        static const std::vector<Vertex> vertices = []()
        {
            std::vector<Vertex> out{};
            const auto source = epochnamespace::previewgrid::grid_vertices();
            out.reserve(source.size());

            for (const auto& vertex : source)
            {
                out.push_back(Vertex{
                    { vertex.position.x, vertex.position.y, vertex.position.z },
                    { vertex.color.x, vertex.color.y, vertex.color.z },
                    { 0.0f, 0.0f }
                });
            }

            return out;
        }();

        return vertices;
    }

    [[nodiscard]] inline const std::vector<std::uint16_t>& kCubeIndices() noexcept
    {
        static const std::vector<std::uint16_t> indices = []()
        {
            std::vector<std::uint16_t> out{};
            const auto source = epochnamespace::previewgrid::grid_indices();
            out.reserve(source.size());

            for (const auto index : source)
                out.push_back(static_cast<std::uint16_t>(index));

            return out;
        }();

        return indices;
    }

    // Exported accessors (cheap, BMI-safe)
    export std::span<const Vertex> cube_vertices() noexcept
    {
        const auto& vertices = kCubeVertices();
        return { vertices.data(), vertices.size() };
    }

    export std::span<const std::uint16_t> cube_indices() noexcept
    {
        const auto& indices = kCubeIndices();
        return { indices.data(), indices.size() };
    }
}
