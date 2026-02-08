// ============================================================================
// modules/acontext.vulkan.context-api.ixx
// Partition: acontext.vulkan.context:api
// Exported engine-facing API surface (declarations only).
//
// Intent:
// - This file exports ONLY the functions the engine calls.
// - No Vulkan types leak through this partition.
// - Definitions live in: acontext.vulkan.context-api.unit.ixx
// ============================================================================

module;

export module acontext.vulkan.context:api;


import aengine.core.context;
import aengine.context.commandqueue;

import <functional>;
import <memory>;

namespace almondnamespace::vulkancontext
{
    // Engine-facing API (no Vulkan types in the signatures).
    export bool vulkan_initialize(
        std::shared_ptr<core::Context> ctx,
        void* parentWindowOpaque = nullptr,
        unsigned int w = 800,
        unsigned int h = 600,
        std::function<void(int, int)> onResize = nullptr);

    export bool vulkan_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue);
    export void vulkan_present();
    export void vulkan_cleanup(std::shared_ptr<core::Context> ctx);
    export int  vulkan_get_width();
    export int  vulkan_get_height();
}
