module;

// Keep this in the global module fragment so it doesn't leak macros into importers.
#include <include/acontext.vulkan.hpp>
#include <vulkan/vulkan.hpp>

// ============================================================================
// modules/acontext.vulkan.context-dispatch_storage.ixx
//
// Static-dispatch build: no Vulkan-Hpp dynamic dispatcher storage needed.
// This partition exists only to keep build scripts / module graphs stable.
// ============================================================================

export module acontext.vulkan.context:dispatch_storage;

