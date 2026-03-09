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

// This module uses Vk* and PFN_* types => MUST include Vulkan C header.
#if defined(_WIN32)
#   ifndef VK_USE_PLATFORM_WIN32_KHR
#       define VK_USE_PLATFORM_WIN32_KHR
#   endif
#endif

#include <vulkan/vulkan.h>

#include <include/aframework.hpp>
#include <include/acontext.vulkan.hpp>

export module acontext.vulkan.platform.dispatcher;

#if ALMOND_VULKAN_CUSTOM_LOADER
import acontext.vulkan.platform.loader; // epochnamespace::vulkan::LoadLibrary / LoadFunction wrappers

export namespace epochnamespace::vulkancontext::platform
{
    // Global function to get vkGetInstanceProcAddr from the Vulkan Loader.
    inline auto getInstanceProcAddr() noexcept -> PFN_vkGetInstanceProcAddr
    {
        static PFN_vkGetInstanceProcAddr fp = nullptr;
        if (!fp)
        {
            // IMPORTANT: qualify the wrapper so we don't hit Win32 LoadLibraryW.
            void* lib = epochnamespace::vulkan::LoadLibrary();
            if (lib)
            {
                fp = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                    epochnamespace::vulkan::LoadFunction(lib, "vkGetInstanceProcAddr"));
            }
        }
        return fp;
    }

    // Global function to get vkGetDeviceProcAddr from the Vulkan Loader.
    inline auto getDeviceProcAddr() noexcept -> PFN_vkGetDeviceProcAddr
    {
        static PFN_vkGetDeviceProcAddr fp = nullptr;
        if (!fp)
        {
            void* lib = epochnamespace::vulkan::LoadLibrary();
            if (lib)
            {
                fp = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                    epochnamespace::vulkan::LoadFunction(lib, "vkGetDeviceProcAddr"));
            }
        }
        return fp;
    }

    // Loader-based retrieval of instance-level functions.
    inline auto GetInstanceFunction(VkInstance instance, const char* funcName) noexcept -> PFN_vkVoidFunction
    {
        auto fp = getInstanceProcAddr();
        return fp ? fp(instance, funcName) : nullptr;
    }

    // Loader-based retrieval of device-level functions.
    inline auto GetDeviceFunction(VkDevice device, const char* funcName) noexcept -> PFN_vkVoidFunction
    {
        auto fp = getDeviceProcAddr();
        return fp ? fp(device, funcName) : nullptr;
    }

    struct InstanceDispatchTable
    {
        PFN_vkDestroyInstance vkDestroyInstance{};
        PFN_vkCreateDevice    vkCreateDevice{};
    };

    inline auto createInstanceDispatchTable(VkInstance instance) noexcept -> InstanceDispatchTable
    {
        InstanceDispatchTable table{};
        table.vkDestroyInstance =
            reinterpret_cast<PFN_vkDestroyInstance>(GetInstanceFunction(instance, "vkDestroyInstance"));
        table.vkCreateDevice =
            reinterpret_cast<PFN_vkCreateDevice>(GetInstanceFunction(instance, "vkCreateDevice"));
        return table;
    }

    struct DeviceDispatchTable
    {
        PFN_vkDestroyDevice vkDestroyDevice{};
        PFN_vkQueueSubmit   vkQueueSubmit{};
    };

    inline auto createDeviceDispatchTable(VkDevice device) noexcept -> DeviceDispatchTable
    {
        DeviceDispatchTable table{};
        table.vkDestroyDevice =
            reinterpret_cast<PFN_vkDestroyDevice>(GetDeviceFunction(device, "vkDestroyDevice"));
        table.vkQueueSubmit =
            reinterpret_cast<PFN_vkQueueSubmit>(GetDeviceFunction(device, "vkQueueSubmit"));
        return table;
    }
} // namespace epochnamespace::vulkancontext::platform
#else
export namespace epochnamespace::vulkancontext::platform
{
    // Custom loader disabled: no dispatcher entry points are exported.
}
#endif
