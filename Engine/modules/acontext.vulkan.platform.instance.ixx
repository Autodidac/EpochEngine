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
module;

#include <include/aengine.config.hpp>

#if defined(_WIN32)
#   ifndef VK_USE_PLATFORM_WIN32_KHR
#       define VK_USE_PLATFORM_WIN32_KHR
#   endif
#endif

// We are using Vk* / PFN_* types.
#include <vulkan/vulkan.h>

#ifndef ALMOND_USING_VULKAN
#   define ALMOND_USING_VULKAN 1
#endif

#include <include/acontext.vulkan.hpp>

export module acontext.vulkan.platform.instance;

#if ALMOND_VULKAN_CUSTOM_LOADER
import acontext.vulkan.platform.dispatcher;

export namespace epochnamespace::vulkancontext::platform
{
    inline VkInstance createInstance(const VkInstanceCreateInfo& createInfo) noexcept
    {
        VkInstance instance = VK_NULL_HANDLE;

        auto fp = getInstanceProcAddr(); // same namespace now
        if (!fp)
            return VK_NULL_HANDLE;

        auto vkCreateInstance =
            reinterpret_cast<PFN_vkCreateInstance>(fp(VK_NULL_HANDLE, "vkCreateInstance"));

        if (!vkCreateInstance)
            return VK_NULL_HANDLE;

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        return instance;
    }

    inline void destroyInstance(VkInstance instance, const InstanceDispatchTable& table) noexcept
    {
        if (table.vkDestroyInstance && instance)
            table.vkDestroyInstance(instance, nullptr);
    }
} // namespace epochnamespace::vulkancontext::platform
#else
export namespace epochnamespace::vulkancontext::platform
{
    // Custom loader disabled: no instance entry points are exported.
}
#endif

