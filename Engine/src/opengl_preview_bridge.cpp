#include "opengl_preview_bridge.hpp"

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import core.context;
import opengl.preview;
import opengl.textures;

namespace epochnamespace::openglbridge
{
    void render_scene_preview(
        const std::shared_ptr<core::Context>& ctx,
        int framebufferWidth,
        int framebufferHeight)
    {
        if (!ctx)
            return;

        const auto viewport = ctx->scene_viewport();
        if (!viewport.valid() || ctx->scene_preview_mode() != core::ScenePreviewMode::Editor)
            return;

        auto& glState = opengltextures::get_opengl_backend().glState;
        openglpreview::render_scene_preview(
            ctx.get(),
            glState,
            ctx->scene_preview_mode(),
            framebufferWidth,
            framebufferHeight,
            viewport.x,
            viewport.y,
            viewport.width,
            viewport.height);
    }
}
#endif
