module;

#include <algorithm>

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
#   include <glad/glad.h>
#endif

module opengl.context;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import core.context;
import core.commandline;
import opengl.textures;

namespace epochengine::openglcontext
{
    void opengl_present()
    {
        // OpenGL swap is handled once per frame in opengl_process.
    }

    int opengl_get_width()
    {
        auto& backend = opengltextures::get_opengl_backend();
        if (backend.glState.width > 0)
            return static_cast<int>(backend.glState.width);
        return (std::max)(1, core::cli::window_width);
    }

    int opengl_get_height()
    {
        auto& backend = opengltextures::get_opengl_backend();
        if (backend.glState.height > 0)
            return static_cast<int>(backend.glState.height);
        return (std::max)(1, core::cli::window_height);
    }

    void opengl_clear()
    {
#if EPOCH_USE_CLEAR_COLOR
        const auto color = core::clear_color_for_context(core::ContextType::OpenGL);
        glClearColor(color[0], color[1], color[2], color[3]);
        glClear(GL_COLOR_BUFFER_BIT);
#endif
    }
}
#endif
