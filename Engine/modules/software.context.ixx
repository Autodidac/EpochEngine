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

#include <functional>
#include <memory>
#include <span>

export module software.context;

import sprite.handle;
import atlas.texture;
import core.context;
import context.commandqueue;

export namespace epochengine::anativecontext
{
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
