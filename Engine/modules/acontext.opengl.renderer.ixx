/**************************************************************
 *   AlmondShell - Modular C++ Framework
 **************************************************************/

module;

// Global module fragment: macros + headers only.
#include <include/aengine.config.hpp> // for ALMOND_USING Macros

#if defined(ALMOND_USING_OPENGL)
    // IMPORTANT:
    // OpenGL symbols must be declared in *this* module unit.
    // Do not rely on other modules/TUs "bringing in" gl* names.
    //
    // Use the same loader you use everywhere else (GLAD).
    // If your project provides a different header path for GLAD, adjust here.
#   include <glad/glad.h>
#endif

export module acontext.opengl.renderer;

import <cstdint>;
import <iostream>;

import aengine.platform;
import aengine.cli;
import aengine.core.context;

import acontext.opengl.context;
import acontext.opengl.state;
import acontext.opengl.quad;
import acontext.opengl.textures;

import aspritehandle;

#if defined(ALMOND_USING_OPENGL)

export namespace almondnamespace::openglrenderer
{
    // --------------------------------------------------------
    // GL STATE ACCESS
    // --------------------------------------------------------

    inline openglstate::OpenGL4State& renderer_gl_state() noexcept
    {
        static openglstate::OpenGL4State* cached = nullptr;
        if (!cached)
            cached = &opengltextures::get_opengl_backend().glState;
        return *cached;
    }

    inline openglstate::OpenGL4State& renderer_gl_state_with_pipeline() noexcept
    {
        auto& glState = renderer_gl_state();
        if (!almondnamespace::openglquad::ensure_quad_pipeline())
            std::cerr << "[OpenGL] Failed to rebuild quad pipeline\n";
        return glState;
    }

    // --------------------------------------------------------
    // TYPES
    // --------------------------------------------------------

    struct RendererContext
    {
        enum class RenderMode
        {
            SingleTexture,
            TextureAtlas
        };

        RenderMode mode = RenderMode::TextureAtlas;
    };

    // --------------------------------------------------------
    // UTILITIES
    // --------------------------------------------------------

    inline void check_gl_error(const char* location) noexcept
    {
        const GLenum err = glGetError();
        if (err != GL_NO_ERROR)
            std::cerr << "[GL ERROR] " << location << " : 0x"
            << std::hex << static_cast<unsigned int>(err) << std::dec << '\n';
    }

    inline bool is_handle_live(const SpriteHandle& h) noexcept
    {
        return h.is_valid();
    }

    // --------------------------------------------------------
    // DEBUG HELPERS
    // --------------------------------------------------------

    inline void debug_gl_state(
        GLuint shader,
        GLuint expectedVAO,
        GLuint expectedTex,
        GLint  uUVRegionLoc,
        GLint  uTransformLoc) noexcept
    {
        GLint v = 0;

        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &v);
        std::cerr << "[Debug] VAO " << v << " (expected " << expectedVAO << ")\n";

        glGetIntegerv(GL_TEXTURE_BINDING_2D, &v);
        std::cerr << "[Debug] TEX " << v << " (expected " << expectedTex << ")\n";

        if (uUVRegionLoc >= 0)
        {
            GLfloat uv[4]{};
            glGetUniformfv(shader, uUVRegionLoc, uv);
            std::cerr << "[Debug] UV (" << uv[0] << "," << uv[1]
                << "," << uv[2] << "," << uv[3] << ")\n";
        }

        if (uTransformLoc >= 0)
        {
            GLfloat tr[4]{};
            glGetUniformfv(shader, uTransformLoc, tr);
            std::cerr << "[Debug] Xform (" << tr[0] << "," << tr[1]
                << "," << tr[2] << "," << tr[3] << ")\n";
        }
    }

    // --------------------------------------------------------
    // DRAWING
    // --------------------------------------------------------

    inline void draw_quad(const openglquad::Quad& quad, GLuint texture)
    {
        auto& glState = renderer_gl_state_with_pipeline();
        auto& pipe = almondnamespace::openglquad::quad_pipeline_state();

        glUseProgram(pipe.shader);

        if (pipe.uUVRegionLoc >= 0)
            glUniform4f(pipe.uUVRegionLoc, 0.f, 0.f, 1.f, 1.f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(quad.vao());

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDrawElements(GL_TRIANGLES, quad.index_count(), GL_UNSIGNED_INT, nullptr);

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_BLEND);
    }

    // --------------------------------------------------------
    // FRAME CONTROL
    // --------------------------------------------------------

    inline void begin_frame()
    {
        const auto color = core::clear_color_for_context(core::ContextType::OpenGL);
        glClearColor(color[0], color[1], color[2], color[3]);
        glViewport(0, 0, core::cli::window_width, core::cli::window_height);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    inline void end_frame() noexcept
    {
        // presentation handled elsewhere
    }
}

#endif // ALMOND_USING_OPENGL
