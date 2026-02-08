// acontext.vulkan.dispatch_storage.cpp
// Dedicated TU for Vulkan-Hpp dynamic dispatch storage.

#include <include/acontext.vulkan.hpp>

#if defined(ALMOND_USING_VULKAN)

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#   define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif

#include <vulkan/vulkan.hpp>

namespace vk::detail
{
    DispatchLoaderDynamic defaultDispatchLoaderDynamic;
}

#endif // ALMOND_USING_VULKAN
