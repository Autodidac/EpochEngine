#include "opengl.scene_preview.hpp"
#include "opengl.canvas2d_scene.hpp"

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import core.context;
import opengl.preview;
import opengl.textures;
import opengl.platform;
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
            ctx->gui_overlay_priority(),
            ctx->frame_window_state()
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

        const auto native = openglcontext::PlatformGL::get_current();
        if (!native.valid()) return;
        auto& backend = opengltextures::get_opengl_backend();
        auto& glState = opengltextures::state_for_platform_context(
            backend, native, ctx.get());
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
