module;

#include <include/engine.config.hpp>
#include "opengl_capture_bridge.hpp"
#include "opengl_process_impl.hpp"
#include "opengl_preview_bridge.hpp"
#include "opengl_upload_bridge.hpp"

module opengl.context;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import core.context;
import context.commandqueue;
import context.multiplexer;
import engine.gui;

namespace epochnamespace::openglcontext
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

        openglbridge::process_pending_uploads();
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

        (void)queue.drain();
        openglbridge::render_scene_preview(ctx, framebufferWidth, framebufferHeight);
        (void)epochnamespace::gui::render_deferred_batch(ctx.get());
        openglbridge::capture_frame_if_requested(framebufferWidth, framebufferHeight, windowId);
    }

    bool opengl_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        return process_impl(std::move(ctx), queue);
    }
}
#endif
