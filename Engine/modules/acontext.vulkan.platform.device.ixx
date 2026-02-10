module;

#include <include/aengine.config.hpp>

#if defined(_WIN32)
#   ifndef VK_USE_PLATFORM_WIN32_KHR
#       define VK_USE_PLATFORM_WIN32_KHR
#   endif
#endif

#include <include/acontext.vulkan.hpp>
// Include Vulkan-Hpp after config.
#include <vulkan/vulkan.hpp>


export module acontext.vulkan.platform.device;

#if ALMOND_VULKAN_CUSTOM_LOADER
import acontext.vulkan.platform.dispatcher;

export namespace almondnamespace::vulkancontext::platform
{
    inline auto createDevice(
        VkPhysicalDevice physicalDevice,
        const VkDeviceCreateInfo& createInfo,
        const InstanceDispatchTable& instTable) noexcept -> VkDevice
    {
        VkDevice device = VK_NULL_HANDLE;

        if (physicalDevice && instTable.vkCreateDevice)
        {
            if (instTable.vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) == VK_SUCCESS)
                return device;
        }

        return VK_NULL_HANDLE;
    }

    inline void destroyDevice(VkDevice device, const DeviceDispatchTable& table) noexcept
    {
        if (device && table.vkDestroyDevice)
            table.vkDestroyDevice(device, nullptr);
    }
} // namespace almondnamespace::vulkancontext::platform
#else
export namespace almondnamespace::vulkancontext::platform
{
    // Custom loader disabled: no device entry points are exported.
}
#endif
