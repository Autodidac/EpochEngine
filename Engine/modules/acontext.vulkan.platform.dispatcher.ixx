module;

#include "../include/epoch.config.hpp"

// This module uses Vk* and PFN_* types => include Vulkan C header.
// (We intentionally use the **linked** Vulkan loader; no custom DLL loader.)
#if defined(_WIN32)
#   ifndef VK_USE_PLATFORM_WIN32_KHR
#       define VK_USE_PLATFORM_WIN32_KHR 1
#   endif
#endif

#include <vulkan/vulkan.h>

#include "../include/aframework.hpp"
#include "../include/acontext.vulkan.hpp"

export module acontext.vulkan.platform.dispatcher;

export namespace almondnamespace::vulkancontext::platform
{
    // With static dispatch / linked loader, these are exported by the Vulkan loader.
    inline auto getInstanceProcAddr() noexcept -> PFN_vkGetInstanceProcAddr
    {
        return &vkGetInstanceProcAddr;
    }

    inline auto getDeviceProcAddr() noexcept -> PFN_vkGetDeviceProcAddr
    {
        return &vkGetDeviceProcAddr;
    }

    inline auto GetInstanceFunction(VkInstance instance, const char* funcName) noexcept -> PFN_vkVoidFunction
    {
        auto fp = getInstanceProcAddr();
        return fp ? fp(instance, funcName) : nullptr;
    }

    inline auto GetDeviceFunction(VkDevice device, const char* funcName) noexcept -> PFN_vkVoidFunction
    {
        auto fp = getDeviceProcAddr();
        return fp ? fp(device, funcName) : nullptr;
    }
} // namespace almondnamespace::vulkancontext::platform
