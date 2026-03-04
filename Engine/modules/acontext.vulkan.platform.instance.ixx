module;

#include <include/acontext.vulkan.hpp>


#if defined(_WIN32)
#   ifndef VK_USE_PLATFORM_WIN32_KHR
#       define VK_USE_PLATFORM_WIN32_KHR 1
#   endif
#endif

// We are using Vk* / PFN_* types.
#include <vulkan/vulkan.h>


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
