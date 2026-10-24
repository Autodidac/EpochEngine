// SPDX-License-Identifier: MPL-2.0
#pragma once

#if defined(EPOCH_PLATFORM_ENGINE_STATIC)
#    define EPOCH_PLATFORM_API
#elif defined(_WIN32)
#    if defined(EPOCH_PLATFORM_ENGINE_EXPORTS)
#        define EPOCH_PLATFORM_API __declspec(dllexport)
#    else
#        define EPOCH_PLATFORM_API __declspec(dllimport)
#    endif
#elif defined(__GNUC__) || defined(__clang__)
#    if defined(EPOCH_PLATFORM_ENGINE_EXPORTS)
#        define EPOCH_PLATFORM_API __attribute__((visibility("default")))
#    else
#        define EPOCH_PLATFORM_API
#    endif
#else
#    define EPOCH_PLATFORM_API
#endif

#if defined(_WIN32)
#    define EPOCH_PLATFORM_CALL __cdecl
#else
#    define EPOCH_PLATFORM_CALL
#endif
