module;

#include <memory>

#include <include/aengine.config.hpp>

export module opengl.backend;

import core.context;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
export namespace epochnamespace::openglbackend
{
    void configure(const std::shared_ptr<core::Context>& ctx);
}
#endif
