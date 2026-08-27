// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

// Stable native boundary shared by Epoch and separately built extension
// binaries. No STL types, exceptions, RTTI, allocators, or compiler-specific
// class layout may cross this C-shaped ABI.

#if defined(_WIN32)
#  if defined(EPOCH_EXTENSION_BUILD)
#    define EPOCH_EXTENSION_EXPORT __declspec(dllexport)
#  else
#    define EPOCH_EXTENSION_EXPORT
#  endif
#else
#  define EPOCH_EXTENSION_EXPORT __attribute__((visibility("default")))
#endif

#define EPOCH_EXTENSION_ABI_VERSION 1u
#define EPOCH_EXTENSION_QUERY_SYMBOL "epoch_extension_query_v1"

enum epoch_extension_log_level : unsigned int
{
    EPOCH_EXTENSION_LOG_INFO = 0u,
    EPOCH_EXTENSION_LOG_WARNING = 1u,
    EPOCH_EXTENSION_LOG_ERROR = 2u
};

enum epoch_extension_capability : unsigned long long
{
    EPOCH_EXTENSION_CAPABILITY_NONE = 0ull,
    EPOCH_EXTENSION_CAPABILITY_RUNTIME = 1ull << 0u,
    EPOCH_EXTENSION_CAPABILITY_AUTHORING = 1ull << 1u,
    EPOCH_EXTENSION_CAPABILITY_RENDER = 1ull << 2u,
    EPOCH_EXTENSION_CAPABILITY_AUDIO = 1ull << 3u,
    EPOCH_EXTENSION_CAPABILITY_PHYSICS = 1ull << 4u,
    EPOCH_EXTENSION_CAPABILITY_IMPORTER = 1ull << 5u,
    EPOCH_EXTENSION_CAPABILITY_NATIVE_TOOL = 1ull << 6u,
    EPOCH_EXTENSION_CAPABILITY_NETWORK = 1ull << 7u
};

struct epoch_extension_host_api
{
    unsigned int abi_version;
    unsigned int struct_size;
    void* user_data;
    void (*log)(
        void* user_data,
        unsigned int level,
        const char* message,
        unsigned long long message_size);
};

struct epoch_extension_descriptor
{
    unsigned int abi_version;
    unsigned int struct_size;
    const char* plugin_id;
    const char* display_name;
    const char* plugin_version;
    unsigned long long capabilities;
    unsigned char (*on_load)(const epoch_extension_host_api* host);
    void (*on_unload)(const epoch_extension_host_api* host);
};

using epoch_extension_query_fn =
    const epoch_extension_descriptor* (*)(
        unsigned int requested_abi_version);

extern "C" EPOCH_EXTENSION_EXPORT
const epoch_extension_descriptor* epoch_extension_query_v1(
    unsigned int requested_abi_version);