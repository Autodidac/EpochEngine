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

#include <cstdint>

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

#include <include/aengine.config.hpp>

export module acontext.vulkan.context:renderer;

#if defined(EPOCH_USING_VULKAN)

export namespace epochnamespace::vulkanrenderer
{
    struct RendererContext
    {
        enum class RenderMode
        {
            SingleTexture,
            TextureAtlas
        };

        RenderMode mode = RenderMode::TextureAtlas;
        bool enable_sprite_draws = true;
    };

    struct VulkanConfig
    {
        std::uint32_t max_frames_in_flight = 2;
        bool enable_validation_layers =
#if defined(NDEBUG)
            false;
#else
            true;
#endif
        bool prefer_mailbox_present = true;
        bool enable_sampler_anisotropy = true;
    };

    inline RendererContext vulkan_renderer{};
    inline VulkanConfig vulkan_config{};
}

#endif
