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
#include <functional>
#include <memory>
#include <span>
#include <string_view>

export module software.context;

import sprite.handle;
import atlas.texture;
import core.context;
import context.commandqueue;

export namespace epochengine::anativecontext
{
    enum class SoftwareCanvas2DContractFailure : std::uint8_t
    {
        none,
        runtime_session,
        clip_region,
        color_packing,
        clipped_blit,
        resize_blit,
        alpha_composition
    };

    [[nodiscard]] constexpr std::string_view software_canvas2d_contract_failure_name(
        SoftwareCanvas2DContractFailure failure) noexcept
    {
        switch (failure)
        {
        case SoftwareCanvas2DContractFailure::none: return "pass";
        case SoftwareCanvas2DContractFailure::runtime_session: return "runtime_session";
        case SoftwareCanvas2DContractFailure::clip_region: return "clip_region";
        case SoftwareCanvas2DContractFailure::color_packing: return "color_packing";
        case SoftwareCanvas2DContractFailure::clipped_blit: return "clipped_blit";
        case SoftwareCanvas2DContractFailure::resize_blit: return "resize_blit";
        case SoftwareCanvas2DContractFailure::alpha_composition:
            return "alpha_composition";
        }
        return "unknown";
    }

    [[nodiscard]] SoftwareCanvas2DContractFailure
        software_canvas2d_backend_contract_failure() noexcept;
    [[nodiscard]] bool software_canvas2d_backend_contract() noexcept;

    int get_width();
    int get_height();

    bool softrenderer_initialize(
        std::shared_ptr<core::Context> ctx,
        void* parentWnd = nullptr,
        unsigned int w = 400,
        unsigned int h = 300,
        std::function<void(int, int)> onResize = nullptr);

    void draw_sprite(
        SpriteHandle handle,
        std::span<const TextureAtlas* const> atlases,
        float x,
        float y,
        float width,
        float height) noexcept;

    bool softrenderer_process(core::Context& ctx, core::CommandQueue& queue);
    void softrenderer_cleanup(std::shared_ptr<core::Context>& ctx);
}
