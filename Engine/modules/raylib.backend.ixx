module;

#include <memory>

#include <include/aengine.config.hpp>

export module raylib.backend;

import core.context;

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
export namespace epochnamespace::raylibbackend
{
    void configure(const std::shared_ptr<core::Context>& ctx);
}
#endif
