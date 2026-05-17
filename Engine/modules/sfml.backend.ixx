module;

#include <memory>

#include <include/engine.config.hpp>

export module sfml.backend;

import core.context;

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
export namespace epochnamespace::sfmlbackend
{
    void configure(const std::shared_ptr<core::Context>& ctx);
}
#endif
