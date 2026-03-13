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

#ifndef ALMOND_USING_VULKAN
#   define ALMOND_USING_VULKAN 1
#endif

export module acontext.vulkan.context:api;


import aengine.core.context;
import aengine.context.commandqueue;

import <functional>;
import <memory>;

namespace epochnamespace::vulkancontext
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

