// acontext.vulkan.dispatch_storage.cpp
// Dedicated TU for Vulkan-Hpp dynamic dispatch storage.

#include <include/acontext.vulkan.hpp>

#if defined(ALMOND_USING_VULKAN)
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif // ALMOND_USING_VULKAN
