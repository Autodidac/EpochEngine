module;

#include <cstdint>
#include <string>
#include <vector>

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
#   include <glad/glad.h>
#endif

export module opengl.capture;

import core.commandline;
import core.logger;
import image.writer;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
export namespace epochnamespace::openglcapture
{
    inline void capture_frame_if_requested(const int width, const int height, const std::uintptr_t windowId)
    {
        const auto capturePath = core::cli::reserve_capture_path("opengl", windowId);
        if (capturePath.empty() || width <= 0 || height <= 0)
            return;

        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u,
            0u);

        GLboolean doubleBuffered = GL_TRUE;
        glGetBooleanv(GL_DOUBLEBUFFER, &doubleBuffered);
        glFinish();
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(doubleBuffered == GL_TRUE ? GL_BACK : GL_FRONT);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        if (a_writeImage(capturePath, pixels, width, height, true))
            logger::info("OpenGL", std::string("Captured frame to ") + capturePath.string());
        else
            logger::warn("OpenGL", std::string("Failed to write capture to ") + capturePath.string());
    }
}
#endif
