#pragma once

#include <memory>

namespace epochnamespace::core
{
    class Context;
}

namespace epochnamespace::openglbridge
{
    void render_scene_preview(
        const std::shared_ptr<epochnamespace::core::Context>& ctx,
        int framebufferWidth,
        int framebufferHeight);
}
