/************************************************
 *  ¦¦¦¦¦¦¦+¦¦¦¦¦¦+  ¦¦¦¦¦¦+  ¦¦¦¦¦¦+¦¦+  ¦¦+   *
 *  ¦¦+----+¦¦+--¦¦+¦¦+---¦¦+¦¦+----+¦¦¦  ¦¦¦   *
 *  ¦¦¦¦¦+  ¦¦¦¦¦¦++¦¦¦   ¦¦¦¦¦¦     ¦¦¦¦¦¦¦¦   *
 *  ¦¦+--+  ¦¦+---+ ¦¦¦   ¦¦¦¦¦¦     ¦¦+--¦¦¦   *
 *  ¦¦¦¦¦¦¦+¦¦¦     +¦¦¦¦¦¦+++¦¦¦¦¦¦+¦¦¦  ¦¦¦   *
 *  +------++-+      +-----+  +-----++-+  +-+   *
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

#ifndef ALMOND_USING_VULKAN
#   define ALMOND_USING_VULKAN 1
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
    constinit inline std::array<Vertex, 24> kCubeVertices = { {
        {{-0.5f, -0.5f,  0.5f}, {0, 0, 1}, {0, 0}},
        {{ 0.5f, -0.5f,  0.5f}, {0, 0, 1}, {1, 0}},
        {{ 0.5f,  0.5f,  0.5f}, {0, 0, 1}, {1, 1}},
        {{-0.5f,  0.5f,  0.5f}, {0, 0, 1}, {0, 1}},

        {{ 0.5f, -0.5f, -0.5f}, {0, 0, -1}, {0, 0}},
        {{-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {1, 0}},
        {{-0.5f,  0.5f, -0.5f}, {0, 0, -1}, {1, 1}},
        {{ 0.5f,  0.5f, -0.5f}, {0, 0, -1}, {0, 1}},

        {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 0}},
        {{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}, {1, 0}},
        {{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, {1, 1}},
        {{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}, {0, 1}},

        {{ 0.5f, -0.5f,  0.5f}, {1, 0, 0}, {0, 0}},
        {{ 0.5f, -0.5f, -0.5f}, {1, 0, 0}, {1, 0}},
        {{ 0.5f,  0.5f, -0.5f}, {1, 0, 0}, {1, 1}},
        {{ 0.5f,  0.5f,  0.5f}, {1, 0, 0}, {0, 1}},

        {{-0.5f,  0.5f,  0.5f}, {0, 1, 0}, {0, 0}},
        {{ 0.5f,  0.5f,  0.5f}, {0, 1, 0}, {1, 0}},
        {{ 0.5f,  0.5f, -0.5f}, {0, 1, 0}, {1, 1}},
        {{-0.5f,  0.5f, -0.5f}, {0, 1, 0}, {0, 1}},

        {{-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {0, 0}},
        {{ 0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 0}},
        {{ 0.5f, -0.5f,  0.5f}, {0, -1, 0}, {1, 1}},
        {{-0.5f, -0.5f,  0.5f}, {0, -1, 0}, {0, 1}},
    } };

    static constexpr std::array<std::uint16_t, 36> kCubeIndices = { {
        0, 1, 2,  0, 2, 3,
        4, 5, 6,  4, 6, 7,
        8, 9, 10,  8, 10, 11,
        12, 13, 14,  12, 14, 15,
        16, 17, 18,  16, 18, 19,
        20, 21, 22,  20, 22, 23
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

