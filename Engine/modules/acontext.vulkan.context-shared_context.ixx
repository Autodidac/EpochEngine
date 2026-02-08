// ============================================================================
// modules/acontext.vulkan.context-shared_context.ixx
// Partition: acontext.vulkan.context:shared_context
// Shared types (GLM lives here; Vulkan-Hpp moved to :shared_vk).
// ============================================================================

module;

#ifndef GLM_ENABLE_EXPERIMENTAL
#   define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

export module acontext.vulkan.context:shared_context;

import <cstdint>;
import <optional>;
import <vector>;

namespace almondnamespace::vulkancontext
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
