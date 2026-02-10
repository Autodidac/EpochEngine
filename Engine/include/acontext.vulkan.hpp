#pragma once
#include <include/aengine.config.hpp>

// ============================================================================
// include/avulkan.config.hpp
//
// Vulkan macro configuration (single source of truth).
//
// This configuration enables Vulkan-Hpp DYNAMIC dispatch (custom loader mode).
// Every TU/module unit that includes <vulkan/vulkan.hpp> MUST include this header
// first (in the global module fragment or before any Vulkan headers).
//
// IMPORTANT:
//   - Do NOT define VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE here.
//     Define it in exactly ONE TU (see src/acontext.vulkan.platform.context.cpp).
// ============================================================================

#if defined(ALMOND_USING_VULKAN)

#  if defined(_WIN32)
#    ifndef VK_USE_PLATFORM_WIN32_KHR
#      define VK_USE_PLATFORM_WIN32_KHR 1
#    endif
#  endif

// Enforce dynamic dispatcher everywhere for custom loader mode.
#  ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#    define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#  else
#    if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC != 1
#      error "VULKAN_HPP_DISPATCH_LOADER_DYNAMIC must be 1 for Almond custom-loader mode."
#    endif
#  endif

// Keep exceptions off unless you intentionally want them.
#  ifndef VULKAN_HPP_NO_EXCEPTIONS
#    define VULKAN_HPP_NO_EXCEPTIONS 1
#  endif

#  ifndef ALMOND_VULKAN_CUSTOM_LOADER
#    define ALMOND_VULKAN_CUSTOM_LOADER 1
#  endif

#endif // ALMOND_USING_VULKAN
