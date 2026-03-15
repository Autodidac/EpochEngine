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

 // modules/acontext.vulkan.context-shared_context.ixx
// Partition: acontext.vulkan.context:shared_context
// Shared types (GLM lives here; Vulkan-Hpp moved to :shared_vk).
// ============================================================================

module;

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

#ifndef GLM_ENABLE_EXPERIMENTAL
#   define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

export module acontext.vulkan.context:shared_context;

import <cstdint>;
import <optional>;
import <vector>;

namespace epochnamespace::vulkancontext
{
    export extern const std::vector<const char*> validationLayers;
    export extern const std::vector<const char*> deviceExtensions;

    struct UniformBufferObject
    {
        alignas(16) glm::mat4 model{};
        alignas(16) glm::mat4 view{};
        alignas(16) glm::mat4 proj{};
    };

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
