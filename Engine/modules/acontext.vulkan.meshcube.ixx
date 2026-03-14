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

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

export module acontext.vulkan.context:meshcube;

import :shared_vk;

import <array>;
import <cstdint>;
import <span>;

namespace epochnamespace::vulkancontext
{
    using Vertex = Application::Vertex;

    // Keep data local to this partition (NOT exported as symbols)
    constinit inline std::array<Vertex, 4> kCubeVertices = { {
        {{-12.0f, 0.0f, -12.0f}, {0, 1, 0}, {0, 0}},
        {{ 12.0f, 0.0f, -12.0f}, {0, 1, 0}, {12, 0}},
        {{ 12.0f, 0.0f,  12.0f}, {0, 1, 0}, {12, 12}},
        {{-12.0f, 0.0f,  12.0f}, {0, 1, 0}, {0, 12}},
    } };

    static constexpr std::array<std::uint16_t, 6> kCubeIndices = { {
        0, 1, 2,
        0, 2, 3
    } };

    // Exported accessors (cheap, BMI-safe)
    export std::span<const Vertex> cube_vertices() noexcept
    {
        return { kCubeVertices.data(), kCubeVertices.size() };
    }

    export std::span<const std::uint16_t> cube_indices() noexcept
    {
        return { kCubeIndices.data(), kCubeIndices.size() };
    }
}
