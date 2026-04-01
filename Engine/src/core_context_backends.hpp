#pragma once

#include <memory>

namespace epochnamespace::core
{
    class Context;

    namespace detail
    {
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        void register_opengl_backend();
#endif
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
        void register_sfml_backend();
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        extern "C" void epoch_register_raylib_backend();
#endif
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        void register_sdl_backend();
#endif
    }
}

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
namespace epochnamespace::openglbackend
{
    void configure(const std::shared_ptr<epochnamespace::core::Context>& ctx);
}
#endif

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
namespace epochnamespace::sfmlbackend
{
    void configure(const std::shared_ptr<epochnamespace::core::Context>& ctx);
}
#endif

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
namespace epochnamespace::raylibbackend
{
    void configure(const std::shared_ptr<epochnamespace::core::Context>& ctx);
}
#endif

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
namespace epochnamespace::sdlbackend
{
    void configure(const std::shared_ptr<epochnamespace::core::Context>& ctx);
}
#endif
