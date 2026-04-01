#pragma once

#include <memory>

namespace epochnamespace::core
{
    class Context;
    struct CommandQueue;
}

namespace epochnamespace::openglcontext
{
    bool process_impl(
        std::shared_ptr<core::Context> ctx,
        core::CommandQueue& queue);
}
