/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include <cstdint>
#include <iostream>
#include <source_location>

// Global module fragment: macros + headers only.
#include <include/aengine.config.hpp> // for EPOCH_USING Macros

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
    // IMPORTANT:
    // OpenGL symbols must be declared in *this* module unit.
    // Do not rely on other modules/TUs "bringing in" gl* names.
    //
    // Use the same loader you use everywhere else (GLAD).
    // If your project provides a different header path for GLAD, adjust here.
#   include <glad/glad.h>
#endif

export module opengl.renderer;

import aengine.platform;
import aengine.cli;
import core.context;
import core.logger;

import opengl.context;
import opengl.state;
import opengl.quad;
import opengl.textures;

import aspritehandle;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)

export namespace epochnamespace::openglrenderer
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
        if (!epochnamespace::openglquad::ensure_quad_pipeline(glState))
            logger::error("OpenGL", "Failed to rebuild quad pipeline.");
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
            logger::errorf_loc("OpenGL", std::source_location::current(), "GL error at {}: 0x{:X}", location, static_cast<unsigned int>(err));
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
        logger::infof_loc("OpenGL.Debug", std::source_location::current(), "VAO {} (expected {})", v, expectedVAO);

        glGetIntegerv(GL_TEXTURE_BINDING_2D, &v);
        logger::infof_loc("OpenGL.Debug", std::source_location::current(), "TEX {} (expected {})", v, expectedTex);

        if (uUVRegionLoc >= 0)
        {
            GLfloat uv[4]{};
            glGetUniformfv(shader, uUVRegionLoc, uv);
            logger::infof_loc("OpenGL.Debug", std::source_location::current(), "UV ({}, {}, {}, {})", uv[0], uv[1], uv[2], uv[3]);
        }

        if (uTransformLoc >= 0)
        {
            GLfloat tr[4]{};
            glGetUniformfv(shader, uTransformLoc, tr);
            logger::infof_loc("OpenGL.Debug", std::source_location::current(), "Xform ({}, {}, {}, {})", tr[0], tr[1], tr[2], tr[3]);
        }
    }

    // --------------------------------------------------------
    // DRAWING
    // --------------------------------------------------------

    inline void draw_quad(const openglquad::Quad& quad, GLuint texture)
    {
        auto& glState = renderer_gl_state_with_pipeline();
        auto& pipe = epochnamespace::openglquad::quad_pipeline_state();

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
#if EPOCH_USE_CLEAR_COLOR
        const auto color = core::clear_color_for_context(core::ContextType::OpenGL);
        glClearColor(color[0], color[1], color[2], color[3]);
        glViewport(0, 0, core::cli::window_width, core::cli::window_height);
        glClear(GL_COLOR_BUFFER_BIT);
#endif
    }

    inline void end_frame() noexcept
    {
        // presentation handled elsewhere
    }
}

#endif // EPOCH_USING_OPENGL
