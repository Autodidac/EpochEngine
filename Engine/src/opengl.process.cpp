module;

#include <include/aengine.config.hpp>
#include "opengl_process_impl.hpp"

module opengl.context;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import core.context;
import context.commandqueue;

namespace epochnamespace::openglcontext
{
    bool opengl_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        return epochnamespace::openglcontext::process_impl(std::move(ctx), queue);
    }
}
#endif
