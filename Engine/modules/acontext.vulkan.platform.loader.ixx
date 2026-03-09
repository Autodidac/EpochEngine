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

#include <include/acontext.vulkan.hpp>

#include <cstdlib>

#ifdef _WIN32
    // Preprocessor hygiene before windows.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <include/aframework.hpp>  // include Windows headers
#else
#include <dlfcn.h>
#endif

export module acontext.vulkan.platform.loader;

#if ALMOND_VULKAN_CUSTOM_LOADER
export namespace epochnamespace::vulkan {

    // OS/dynamic-loader calls are runtime by definition: NOT constexpr.
    inline auto LoadLibrary() noexcept -> void*
    {
#ifdef _WIN32
        return static_cast<void*>(::LoadLibraryA("vulkan-1.dll"));
#else
        return ::dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
#endif
    }

    inline auto FreeLibrary(void* lib) noexcept -> void
    {
        if (!lib) return;

#ifdef _WIN32
        ::FreeLibrary(static_cast<HMODULE>(lib));
#else
        ::dlclose(lib);
#endif
    }

    inline auto LoadFunction(void* lib, const char* name) noexcept -> void*
    {
        if (!lib || !name) return nullptr;

#ifdef _WIN32
        return reinterpret_cast<void*>(
            ::GetProcAddress(static_cast<HMODULE>(lib), name)
            );
#else
        return ::dlsym(lib, name);
#endif
    }

} // namespace epochnamespace::vulkan
#else
export namespace epochnamespace::vulkan
{
    // Custom loader disabled: no dynamic loader entry points are exported.
}
#endif
