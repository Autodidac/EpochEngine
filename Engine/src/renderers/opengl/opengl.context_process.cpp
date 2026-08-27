module;

#include <include/engine.config.hpp>
#include "opengl.context_process_impl.hpp"
#include "opengl.scene_preview.hpp"

module opengl.context;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import atlas.manager;
import core.context;
import context.commandqueue;
import context.multiplexer;
import context.type;
import opengl.textures;
import gui.engine;

namespace epochengine::openglcontext
{
    void opengl_render_active_frame(
        std::shared_ptr<core::Context> ctx,
        core::CommandQueue& queue,
        int framebufferWidth,
        int framebufferHeight,
        std::uintptr_t windowId)
    {
        if (!ctx)
            return;

        atlasmanager::process_pending_uploads(core::ContextType::OpenGL);
        const auto previousContext = core::MultiContextManager::GetCurrent();
        core::MultiContextManager::SetCurrent(ctx);

        struct ScopedCurrentContext
        {
            std::shared_ptr<core::Context> previous;
            ~ScopedCurrentContext()
            {
                core::MultiContextManager::SetCurrent(std::move(previous));
            }
        } scoped{ previousContext };

        // Stable OpenGL editor draw contract:
        // GUI sprites are captured as one deferred snapshot, while modal/menu
        // sprites are captured as a matching top-layer snapshot. Replaying both
        // snapshots around the scene pass keeps modal scrims, Package Manager,
        // and dropdowns from interleaving with a stale scene frame.
        const bool overlayPriority = ctx->gui_overlay_priority();
        (void)queue.drain();
        if (!overlayPriority)
        {
            const bool spriteBatch =
                opengltextures::begin_sprite_batch(ctx.get());
            (void)gui::render_deferred_batch(ctx.get());
            if (spriteBatch)
                opengltextures::end_sprite_batch();
        }
        openglscene::render_scene_preview(ctx, framebufferWidth, framebufferHeight);
        (void)queue.drain();
        const bool spriteBatch =
            opengltextures::begin_sprite_batch(ctx.get());
        (void)gui::render_deferred_batch(ctx.get());
        (void)gui::render_top_layer_batch(ctx.get());
        if (spriteBatch)
            opengltextures::end_sprite_batch();
    }

    bool opengl_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        return process_impl(std::move(ctx), queue);
    }
}
#endif
