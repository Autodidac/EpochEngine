module;

#include <cstdint>
#include <functional>
#include <memory>

#include <include/engine.config.hpp>

export module opengl.context;

import core.context;
import context.commandqueue;

export namespace epochnamespace::openglcontext
{
#if !defined(EPOCH_USING_OPENGL) || (EPOCH_USING_OPENGL != 1)
    inline bool opengl_initialize(std::shared_ptr<core::Context>, void* = nullptr,
        unsigned int = 0, unsigned int = 0, std::function<void(int, int)> = nullptr)
    {
        return false;
    }

    inline void opengl_present() {}
    inline int opengl_get_width() { return 1; }
    inline int opengl_get_height() { return 1; }
    inline void opengl_clear() {}
    inline bool opengl_process(std::shared_ptr<core::Context>, core::CommandQueue&) { return false; }
    inline void opengl_cleanup(std::shared_ptr<core::Context>) {}
#else
    bool opengl_initialize(std::shared_ptr<core::Context> ctx,
        void* parentWindowOpaque = nullptr,
        unsigned int w = 800,
        unsigned int h = 600,
        std::function<void(int, int)> onResize = nullptr);

    void opengl_present();
    int opengl_get_width();
    int opengl_get_height();
    void opengl_clear();
    bool opengl_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue);
    void opengl_render_active_frame(
        std::shared_ptr<core::Context> ctx,
        core::CommandQueue& queue,
        int framebufferWidth,
        int framebufferHeight,
        std::uintptr_t windowId);
    void opengl_cleanup(std::shared_ptr<core::Context> ctx);
#endif
}
