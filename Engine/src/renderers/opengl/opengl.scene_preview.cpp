#include "opengl.scene_preview.hpp"
#include "opengl.canvas2d_scene.hpp"

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import core.context;
import opengl.preview;
import opengl.textures;
import render.context_frame;

namespace epochengine::openglscene
{
    void render_scene_preview(
        const std::shared_ptr<core::Context>& ctx,
        int framebufferWidth,
        int framebufferHeight)
    {
        if (!ctx)
            return;

        const auto requested = ctx->scene_viewport();
        const auto frame = rendercontext::resolve_frame_plan({
            framebufferWidth,
            framebufferHeight,
            { requested.x, requested.y, requested.width, requested.height },
            ctx->scene_preview_mode() == core::ScenePreviewMode::Editor,
            ctx->gui_overlay_priority()
        });
        if (!frame.scene_visible)
            return;
        const core::RenderViewport viewport{
            frame.scene.x, frame.scene.y, frame.scene.width, frame.scene.height
        };

        if (openglcanvas2d::render_canvas2d_scene_content(
                ctx,
                viewport,
                framebufferWidth,
                framebufferHeight))
        {
            return;
        }

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
