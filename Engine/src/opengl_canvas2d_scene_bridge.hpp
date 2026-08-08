/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#pragma once

#include <memory>

namespace epochengine::core
{
    class Context;
    struct RenderViewport;
}

namespace epochengine::openglbridge
{
    [[nodiscard]] bool render_canvas2d_scene_content(
        const std::shared_ptr<core::Context>& ctx,
        const core::RenderViewport& viewport,
        int framebuffer_width,
        int framebuffer_height);

    void release_canvas2d_scene_renderer(const core::Context* ctx) noexcept;
}
