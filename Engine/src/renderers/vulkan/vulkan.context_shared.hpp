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
#pragma once
#include <include/engine.config.hpp>

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
//     Define it in exactly ONE TU (see src/renderers/vulkan/vulkan.platform_context.cpp).
// ============================================================================

#if defined(EPOCH_USING_VULKAN)

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
#      error "VULKAN_HPP_DISPATCH_LOADER_DYNAMIC must be 1 for Epoch custom-loader mode."
#    endif
#  endif

// Keep exceptions off unless you intentionally want them.
#  ifndef VULKAN_HPP_NO_EXCEPTIONS
#    define VULKAN_HPP_NO_EXCEPTIONS 1
#  endif

// In no-exception mode, do not break into debugger on non-success results.
// Callers in this codebase explicitly check vk::Result values.
#  ifndef VULKAN_HPP_ASSERT_ON_RESULT
#    define VULKAN_HPP_ASSERT_ON_RESULT(...) ((void)0)
#  endif

#  ifndef EPOCH_VULKAN_CUSTOM_LOADER
#    define EPOCH_VULKAN_CUSTOM_LOADER 1
#  endif

#endif // EPOCH_USING_VULKAN
