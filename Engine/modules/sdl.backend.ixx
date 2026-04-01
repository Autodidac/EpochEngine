module;

#include <memory>

#include <include/aengine.config.hpp>

export module sdl.backend;

import core.context;

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
export namespace epochnamespace::sdlbackend
{
    void configure(const std::shared_ptr<core::Context>& ctx);
}
#endif
