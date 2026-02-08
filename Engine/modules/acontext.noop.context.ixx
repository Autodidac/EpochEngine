// no graphics operations context for testing and driverless engine usage.
// a test framework can be launched from here to automate build testing.

module;

#include <include/aengine.config.hpp>

export module acontext.noop.context;

import <atomic>;
import <memory>;

import aengine.context.commandqueue;
import aengine.core.context;
import aengine.diagnostics;

namespace almondnamespace::noopcontext
{
#if defined(ALMOND_USING_NOOP_HEADLESS)
    inline std::atomic_bool running{ false };
#endif
}

export namespace almondnamespace::noopcontext
{
#if defined(ALMOND_USING_NOOP_HEADLESS)
    inline void noop_initialize()
    {
        running.store(true, std::memory_order_release);
    }

    inline bool noop_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        almond::diagnostics::FrameTiming frameTimer{ core::ContextType::Noop, 0, "Noop" };

        if (ctx)
        {
            ctx->width = 1;
            ctx->height = 1;
            ctx->framebufferWidth = 1;
            ctx->framebufferHeight = 1;
            ctx->virtualWidth = 1;
            ctx->virtualHeight = 1;
        }

        queue.drain();

        frameTimer.finish();

        return running.load(std::memory_order_acquire);
    }

    inline void noop_cleanup()
    {
        running.store(false, std::memory_order_release);
    }

    inline void noop_clear() {}
    inline void noop_present() {}

    inline int noop_get_width() { return 1; }
    inline int noop_get_height() { return 1; }
#endif
}
