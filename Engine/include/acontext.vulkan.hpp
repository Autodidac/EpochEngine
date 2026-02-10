#pragma once
#include <include/aengine.config.hpp>

// ============================================================================
// include/acontext.vulkan.hpp
//
// Vulkan macro configuration (single source of truth).
//
// This configuration uses Vulkan-Hpp **STATIC** dispatch (default loader).
// That means:
//   - NO VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
//   - NO VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
//   - NO global dispatcher init()
//   - Link against the platform Vulkan loader (Windows: vulkan-1.lib,
//     Linux: -lvulkan).
//
// Rationale for Almond multi-context:
//   Dynamic-dispatch mode relies on a process-global dispatcher state which is
//   easy to trip over when you have multiple independent Vulkan contexts,
//   hot-reload, and mixed threading. Static dispatch avoids that entire class
//   of problems.
//
// Every TU/module unit that includes <vulkan/vulkan.hpp> MUST include this
// header first (in the global module fragment or before any Vulkan headers).
// ============================================================================

#if defined(ALMOND_USING_VULKAN)

#  if defined(_WIN32)
#    ifndef VK_USE_PLATFORM_WIN32_KHR
#      define VK_USE_PLATFORM_WIN32_KHR 1
#    endif
#  endif

// Keep exceptions off unless you intentionally want them.
#  ifndef VULKAN_HPP_NO_EXCEPTIONS
#    define VULKAN_HPP_NO_EXCEPTIONS 1
#  endif

// Optional: keep RAII on by default; you already use Unique handles.
// (No macro needed unless you want to force-disable it.)

// Optional: you can enable typesafe conversions if desired.
// #define VULKAN_HPP_TYPESAFE_CONVERSION 1

#endif // ALMOND_USING_VULKAN
