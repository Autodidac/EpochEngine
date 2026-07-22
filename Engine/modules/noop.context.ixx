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
 // no graphics operations context for testing and driverless engine usage.
// a test framework can be launched from here to automate build testing.

module;

#include <atomic>
#include <memory>

#include <include/engine.config.hpp>

export module noop.context;

import context.commandqueue;
import core.context;
import engine.diagnostics;

namespace epochengine::noopcontext
{
#if defined(EPOCH_USING_NOOP_HEADLESS)
    inline std::atomic_bool running{ false };
#endif
}

export namespace epochengine::noopcontext
{
#if defined(EPOCH_USING_NOOP_HEADLESS)
    inline void noop_initialize()
    {
        running.store(true, std::memory_order_release);
    }

    inline bool noop_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        diagnostics::FrameTiming frameTimer{ core::ContextType::Noop, 0, "Noop" };

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
