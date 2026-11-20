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
// modules/vulkan.context-shared_context.ixx
// Partition: vulkan.context:shared_context
// Shared types (GLM lives here; Vulkan-Hpp moved to :shared_vk).
// ============================================================================

module;

#include <array>
#include <compare>
#include <cstddef>

#include <cstdint>
#include <optional>
#include <vector>

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

#ifndef GLM_ENABLE_EXPERIMENTAL
#   define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

export module vulkan.context:shared_context;

import render.preview_grid;

namespace epochengine::vulkancontext
{
    export extern const std::vector<const char*> validationLayers;
    export extern const std::vector<const char*> deviceExtensions;

    struct UniformBufferObject
    {
        alignas(16) glm::mat4 model{};
        alignas(16) glm::mat4 view{};
        alignas(16) glm::mat4 proj{};
        alignas(16) glm::vec4 water_center_scale{};
        alignas(16) glm::vec4 water_eye{};
        alignas(16) glm::vec4 water_sun{};
        alignas(16) std::array<glm::vec4, previewgrid::water_wave_count> water_waves{};
        alignas(16) std::array<glm::vec4,
            previewgrid::WaterDetailParameters{}.slopes.size()> water_slopes{};
    };
    static_assert(sizeof(glm::mat4) == 64u && sizeof(glm::vec4) == 16u);
    static_assert(offsetof(UniformBufferObject, water_center_scale) == 192u);
    static_assert(offsetof(UniformBufferObject, water_eye) == 208u);
    static_assert(offsetof(UniformBufferObject, water_sun) == 224u);
    static_assert(offsetof(UniformBufferObject, water_waves) == 240u);
    static_assert(offsetof(UniformBufferObject, water_slopes)
        == 240u + 16u * previewgrid::water_wave_count);

    struct QueueFamilyIndices
    {
        std::optional<std::uint32_t> graphicsFamily;
        std::optional<std::uint32_t> presentFamily;

        bool isComplete() const noexcept
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };
}
