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

export namespace epochnamespace::vulkancontext::platform
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
} // namespace epochnamespace::vulkancontext::platform
#else
export namespace epochnamespace::vulkancontext::platform
{
    // Custom loader disabled: no device entry points are exported.
}
#endif
