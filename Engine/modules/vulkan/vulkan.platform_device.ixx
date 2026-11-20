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

#include <include/engine.config.hpp>

#if defined(_WIN32)
#   ifndef VK_USE_PLATFORM_WIN32_KHR
#       define VK_USE_PLATFORM_WIN32_KHR
#   endif
#endif

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

#include <../src/renderers/vulkan/vulkan.context_shared.hpp>
// Include Vulkan-Hpp after config.
#include <compare>
#include <vulkan/vulkan.hpp>


export module vulkan.platform_device;

#if EPOCH_VULKAN_CUSTOM_LOADER
import vulkan.platform_dispatcher;

export namespace epochengine::vulkancontext::platform
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
} // namespace epochengine::vulkancontext::platform
#else
export namespace epochengine::vulkancontext::platform
{
    // Custom loader disabled: no device entry points are exported.
}
#endif
