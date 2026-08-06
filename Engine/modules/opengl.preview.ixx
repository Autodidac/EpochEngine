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

#include <cstddef>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
#   include <glad/glad.h>
#endif

export module opengl.preview;

import core.context;
import opengl.state;
import package.registry;
import render.arcade;
import render.preview_grid;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
export namespace epochengine::openglpreview
{
    namespace detail
    {
        using Mat4 = epochengine::previewgrid::Mat4;

        struct ScopedPreviewGLState final
        {
            GLboolean blendEnabled = GL_FALSE;
            GLboolean cullFaceEnabled = GL_FALSE;
            GLboolean scissorEnabled = GL_FALSE;
            GLboolean depthTestEnabled = GL_FALSE;
            GLboolean depthMask = GL_FALSE;
            GLint depthFunc = GL_LESS;
            GLint viewport[4]{};
            GLint scissorBox[4]{};
            GLint program = 0;
            GLint vertexArray = 0;
            GLint arrayBuffer = 0;
            GLint elementArrayBuffer = 0;
            GLint drawFramebuffer = 0;
            GLint readFramebuffer = 0;
            GLint activeTexture = 0;
            GLint texture2D = 0;
            GLfloat clearColor[4]{};

            ScopedPreviewGLState() noexcept
            {
                blendEnabled = glIsEnabled(GL_BLEND);
                cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
                scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
                depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
                glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
                glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
                glGetIntegerv(GL_VIEWPORT, viewport);
                glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
                glGetIntegerv(GL_CURRENT_PROGRAM, &program);
                glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
                glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
                glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementArrayBuffer);
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
                glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2D);
                glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
            }

            ScopedPreviewGLState(const ScopedPreviewGLState&) = delete;
            ScopedPreviewGLState& operator=(const ScopedPreviewGLState&) = delete;

            ~ScopedPreviewGLState() noexcept
            {
                glUseProgram(static_cast<GLuint>(program));
                glBindVertexArray(static_cast<GLuint>(vertexArray));
                glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer));
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(elementArrayBuffer));
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFramebuffer));
                glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFramebuffer));
                glActiveTexture(static_cast<GLenum>(activeTexture));
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture2D));
                glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
                glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
                glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
                glDepthFunc(static_cast<GLenum>(depthFunc));
                glDepthMask(depthMask);

                if (blendEnabled == GL_TRUE) glEnable(GL_BLEND); else glDisable(GL_BLEND);
                if (cullFaceEnabled == GL_TRUE) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
                if (scissorEnabled == GL_TRUE) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
                if (depthTestEnabled == GL_TRUE) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            }
        };

        [[nodiscard]] inline std::pair<int, int> parse_gl_version(const char* s) noexcept
        {
            if (!s) return { 0, 0 };
            std::string_view text{ s };
            const auto firstDigit = text.find_first_of("0123456789");
            if (firstDigit == std::string_view::npos) return { 0, 0 };
            text.remove_prefix(firstDigit);
            const auto dot = text.find('.');
            if (dot == std::string_view::npos) return { 0, 0 };

            auto to_int = [](std::string_view value) noexcept
            {
                int out = 0;
                for (unsigned char ch : value)
                {
                    if (ch < '0' || ch > '9') break;
                    out = (out * 10) + (ch - '0');
                }
                return out;
            };

            const int major = to_int(text.substr(0, dot));
            text.remove_prefix(dot + 1);
            return { major, to_int(text) };
        }

        [[nodiscard]] inline GLuint compile_scene_shader(GLenum type, const std::string& source)
        {
            GLuint shader = glCreateShader(type);
            const char* text = source.c_str();
            glShaderSource(shader, 1, &text, nullptr);
            glCompileShader(shader);

            GLint compiled = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (compiled == GL_TRUE)
                return shader;

            std::string log(4096, '\0');
            GLsizei used = 0;
            glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), &used, log.data());
            glDeleteShader(shader);
            if (used > 0 && static_cast<std::size_t>(used) < log.size()) log.resize(static_cast<std::size_t>(used));
            throw std::runtime_error("[ OpenGL ] - Scene preview shader compile failed: " + log);
        }

        [[nodiscard]] inline GLuint link_scene_program(GLuint vertexShader, GLuint fragmentShader)
        {
            GLuint program = glCreateProgram();
            glAttachShader(program, vertexShader);
            glAttachShader(program, fragmentShader);
            glBindAttribLocation(program, 0, "aPos");
            glBindAttribLocation(program, 1, "aColor");
            if (glBindFragDataLocation)
                glBindFragDataLocation(program, 0, "outColor");
            glLinkProgram(program);

            GLint linked = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &linked);
            if (linked == GL_TRUE)
                return program;

            std::string log(4096, '\0');
            GLsizei used = 0;
            glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), &used, log.data());
            glDeleteProgram(program);
            if (used > 0 && static_cast<std::size_t>(used) < log.size()) log.resize(static_cast<std::size_t>(used));
            throw std::runtime_error("[ OpenGL ] - Scene preview program link failed: " + log);
        }

        [[nodiscard]] inline GLuint link_textured_scene_program(GLuint vertexShader, GLuint fragmentShader)
        {
            GLuint program = glCreateProgram();
            glAttachShader(program, vertexShader);
            glAttachShader(program, fragmentShader);
            glBindAttribLocation(program, 0, "aPos");
            glBindAttribLocation(program, 1, "aUv");
            if (glBindFragDataLocation)
                glBindFragDataLocation(program, 0, "outColor");
            glLinkProgram(program);

            GLint linked = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &linked);
            if (linked == GL_TRUE)
                return program;

            std::string log(4096, '\0');
            GLsizei used = 0;
            glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), &used, log.data());
            glDeleteProgram(program);
            if (used > 0 && static_cast<std::size_t>(used) < log.size()) log.resize(static_cast<std::size_t>(used));
            throw std::runtime_error("[ OpenGL ] - Arcade screen program link failed: " + log);
        }
    }

    inline void destroy_scene_preview_pipeline(epochengine::openglstate::OpenGL4State& state) noexcept
    {
        if (state.arcadeScreenFramebuffer && glIsFramebuffer(state.arcadeScreenFramebuffer))
            glDeleteFramebuffers(1, &state.arcadeScreenFramebuffer);
        if (state.arcadeScreenDepth && glIsRenderbuffer(state.arcadeScreenDepth))
            glDeleteRenderbuffers(1, &state.arcadeScreenDepth);
        if (state.arcadeScreenTexture && glIsTexture(state.arcadeScreenTexture))
            glDeleteTextures(1, &state.arcadeScreenTexture);
        if (state.arcadeScreenEbo && glIsBuffer(state.arcadeScreenEbo))
            glDeleteBuffers(1, &state.arcadeScreenEbo);
        if (state.arcadeScreenVbo && glIsBuffer(state.arcadeScreenVbo))
            glDeleteBuffers(1, &state.arcadeScreenVbo);
        if (state.arcadeScreenVao && glIsVertexArray(state.arcadeScreenVao))
            glDeleteVertexArrays(1, &state.arcadeScreenVao);
        if (state.arcadeScreenShader && glIsProgram(state.arcadeScreenShader))
            glDeleteProgram(state.arcadeScreenShader);

        state.arcadeScreenShader = 0;
        state.arcadeScreenMvpLoc = -1;
        state.arcadeScreenTextureLoc = -1;
        state.arcadeScreenVao = 0;
        state.arcadeScreenVbo = 0;
        state.arcadeScreenEbo = 0;
        state.arcadeScreenTexture = 0;
        state.arcadeScreenFramebuffer = 0;
        state.arcadeScreenDepth = 0;
        state.arcadeScreenFrame = 0;

        if (state.sceneMarkerVbo && glIsBuffer(state.sceneMarkerVbo)) glDeleteBuffers(1, &state.sceneMarkerVbo);
        if (state.sceneMarkerVao && glIsVertexArray(state.sceneMarkerVao)) glDeleteVertexArrays(1, &state.sceneMarkerVao);
        if (state.sceneEbo && glIsBuffer(state.sceneEbo)) glDeleteBuffers(1, &state.sceneEbo);
        if (state.sceneVbo && glIsBuffer(state.sceneVbo)) glDeleteBuffers(1, &state.sceneVbo);
        if (state.sceneVao && glIsVertexArray(state.sceneVao)) glDeleteVertexArrays(1, &state.sceneVao);
        if (state.sceneShader && glIsProgram(state.sceneShader)) glDeleteProgram(state.sceneShader);

        state.sceneShader = 0;
        state.sceneMvpLoc = -1;
        state.sceneVao = 0;
        state.sceneVbo = 0;
        state.sceneEbo = 0;
        state.sceneMarkerVao = 0;
        state.sceneMarkerVbo = 0;
    }

    inline bool ensure_scene_preview_pipeline(epochengine::openglstate::OpenGL4State& state)
    {
        if (state.sceneShader
            && state.sceneVao
            && state.sceneVbo
            && state.sceneEbo
            && state.sceneMarkerVao
            && state.sceneMarkerVbo)
            return true;

        destroy_scene_preview_pipeline(state);

        GLint major = 0;
        GLint minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        if (major == 0 && minor == 0)
        {
            const auto parsed = detail::parse_gl_version(reinterpret_cast<const char*>(glGetString(GL_VERSION)));
            major = parsed.first;
            minor = parsed.second;
        }

        std::string vertexSource;
        std::string fragmentSource;
        if (major > 3 || (major == 3 && minor >= 3))
        {
            vertexSource = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMvp;
out vec3 vColor;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    vColor = aColor;
})";
            fragmentSource = R"(#version 330 core
in vec3 vColor;
out vec4 outColor;
void main() {
    outColor = vec4(vColor, 1.0);
})";
        }
        else
        {
            vertexSource = R"(#version 120
attribute vec3 aPos;
attribute vec3 aColor;
uniform mat4 uMvp;
varying vec3 vColor;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    vColor = aColor;
})";
            fragmentSource = R"(#version 120
varying vec3 vColor;
void main() {
    gl_FragColor = vec4(vColor, 1.0);
})";
        }

        const GLuint vertexShader = detail::compile_scene_shader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = 0;
        try
        {
            fragmentShader = detail::compile_scene_shader(GL_FRAGMENT_SHADER, fragmentSource);
            state.sceneShader = detail::link_scene_program(vertexShader, fragmentShader);
        }
        catch (...)
        {
            if (vertexShader) glDeleteShader(vertexShader);
            if (fragmentShader) glDeleteShader(fragmentShader);
            destroy_scene_preview_pipeline(state);
            throw;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        const auto vertices = epochengine::previewgrid::grid_vertices();
        const auto indices = epochengine::previewgrid::grid_indices();

        glGenVertexArrays(1, &state.sceneVao);
        glGenBuffers(1, &state.sceneVbo);
        glGenBuffers(1, &state.sceneEbo);

        glBindVertexArray(state.sceneVao);
        glBindBuffer(GL_ARRAY_BUFFER, state.sceneVbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(vertices.size() * sizeof(vertices[0])),
            vertices.data(),
            GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.sceneEbo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(indices.size() * sizeof(indices[0])),
            indices.data(),
            GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(epochengine::previewgrid::Vertex)),
            reinterpret_cast<void*>(offsetof(epochengine::previewgrid::Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(epochengine::previewgrid::Vertex)),
            reinterpret_cast<void*>(offsetof(epochengine::previewgrid::Vertex, color)));
        glBindVertexArray(0);

        glGenVertexArrays(1, &state.sceneMarkerVao);
        glGenBuffers(1, &state.sceneMarkerVbo);
        glBindVertexArray(state.sceneMarkerVao);
        glBindBuffer(GL_ARRAY_BUFFER, state.sceneMarkerVbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(sizeof(epochengine::previewgrid::Vertex) * 8u),
            nullptr,
            GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(epochengine::previewgrid::Vertex)),
            reinterpret_cast<void*>(offsetof(epochengine::previewgrid::Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(epochengine::previewgrid::Vertex)),
            reinterpret_cast<void*>(offsetof(epochengine::previewgrid::Vertex, color)));
        glBindVertexArray(0);

        state.sceneMvpLoc = glGetUniformLocation(state.sceneShader, "uMvp");
        return state.sceneShader != 0 && state.sceneMvpLoc >= 0;
    }

    namespace detail
    {
        struct ArcadeScreenVertex
        {
            epochengine::previewgrid::Vec3 position{};
            float u = 0.0f;
            float v = 0.0f;
        };

        [[nodiscard]] inline std::array<ArcadeScreenVertex, 4> make_arcade_screen_vertices(
            const epochengine::previewgrid::ObjectMarker& marker) noexcept
        {
            const auto safe_axis = [](float value, float fallback) noexcept
            {
                const float magnitude = value < 0.0f ? -value : value;
                return (std::max)(magnitude, fallback);
            };

            const epochengine::previewgrid::Vec3 half{
                safe_axis(marker.scale.x * 0.5f, 0.25f),
                safe_axis(marker.scale.y * 0.5f, 0.18f),
                safe_axis(marker.scale.z * 0.5f, 0.018f)
            };
            const float z =
                epochengine::render_arcade::screen_sample_plane_z(
                    marker.position.z, marker.scale.z);
            const float left = marker.position.x - half.x;
            const float right = marker.position.x + half.x;
            const float bottom = marker.position.y - half.y;
            const float top = marker.position.y + half.y;

            return std::array<ArcadeScreenVertex, 4>{
                ArcadeScreenVertex{ .position{ left, bottom, z }, .u = 0.0f, .v = 0.0f },
                ArcadeScreenVertex{ .position{ right, bottom, z }, .u = 1.0f, .v = 0.0f },
                ArcadeScreenVertex{ .position{ right, top, z }, .u = 1.0f, .v = 1.0f },
                ArcadeScreenVertex{ .position{ left, top, z }, .u = 0.0f, .v = 1.0f }
            };
        }

        inline void clear_arcade_rect(
            int x,
            int y,
            int width,
            int height,
            int targetWidth,
            int targetHeight,
            float r,
            float g,
            float b,
            float a) noexcept
        {
            if (width <= 0 || height <= 0 || targetWidth <= 0 || targetHeight <= 0)
                return;

            const int x0 = (std::clamp)(x, 0, targetWidth);
            const int y0 = (std::clamp)(y, 0, targetHeight);
            const int x1 = (std::clamp)(x + width, 0, targetWidth);
            const int y1 = (std::clamp)(y + height, 0, targetHeight);
            if (x1 <= x0 || y1 <= y0)
                return;

            glScissor(x0, y0, x1 - x0, y1 - y0);
            glClearColor(r, g, b, a);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        inline void render_arcade_attract_pattern(
            epochengine::openglstate::OpenGL4State& state,
            int width,
            int height) noexcept
        {
            glBindFramebuffer(GL_FRAMEBUFFER, state.arcadeScreenFramebuffer);
            glViewport(0, 0, width, height);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, 0, width, height);
            glClearColor(0.015f, 0.025f, 0.045f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            epochengine::render_arcade::emit_arcade_attract_pattern(
                width,
                height,
                state.arcadeScreenFrame++,
                [&](const epochengine::render_arcade::ArcadePreviewRect& rect)
                {
                    clear_arcade_rect(
                        rect.x,
                        rect.y,
                        rect.width,
                        rect.height,
                        width,
                        height,
                        rect.color[0],
                        rect.color[1],
                        rect.color[2],
                        rect.color[3]);
                });
        }
    }

    inline bool ensure_arcade_screen_preview_pipeline(epochengine::openglstate::OpenGL4State& state)
    {
        if (state.arcadeScreenShader
            && state.arcadeScreenVao
            && state.arcadeScreenVbo
            && state.arcadeScreenEbo
            && state.arcadeScreenTexture
            && state.arcadeScreenFramebuffer
            && state.arcadeScreenDepth)
        {
            return true;
        }

        const int textureWidth = static_cast<int>(epochengine::package_registry::engine_arcade_render_texture_width());
        const int textureHeight = static_cast<int>(epochengine::package_registry::engine_arcade_render_texture_height());
        if (textureWidth <= 0 || textureHeight <= 0)
            return false;

        std::string vertexSource;
        std::string fragmentSource;
        GLint major = 0;
        GLint minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        if (major == 0 && minor == 0)
        {
            const auto parsed = detail::parse_gl_version(reinterpret_cast<const char*>(glGetString(GL_VERSION)));
            major = parsed.first;
            minor = parsed.second;
        }

        if (major > 3 || (major == 3 && minor >= 3))
        {
            vertexSource = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
uniform mat4 uMvp;
out vec2 vUv;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    vUv = aUv;
})";
            fragmentSource = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uTexture;
out vec4 outColor;
void main() {
    vec4 texel = texture(uTexture, vUv);
    outColor = vec4(texel.rgb, 1.0);
})";
        }
        else
        {
            vertexSource = R"(#version 120
attribute vec3 aPos;
attribute vec2 aUv;
uniform mat4 uMvp;
varying vec2 vUv;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    vUv = aUv;
})";
            fragmentSource = R"(#version 120
varying vec2 vUv;
uniform sampler2D uTexture;
void main() {
    vec4 texel = texture2D(uTexture, vUv);
    gl_FragColor = vec4(texel.rgb, 1.0);
})";
        }

        const GLuint vertexShader = detail::compile_scene_shader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = 0;
        try
        {
            fragmentShader = detail::compile_scene_shader(GL_FRAGMENT_SHADER, fragmentSource);
            state.arcadeScreenShader = detail::link_textured_scene_program(vertexShader, fragmentShader);
        }
        catch (...)
        {
            if (vertexShader) glDeleteShader(vertexShader);
            if (fragmentShader) glDeleteShader(fragmentShader);
            destroy_scene_preview_pipeline(state);
            throw;
        }
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        glGenTextures(1, &state.arcadeScreenTexture);
        glBindTexture(GL_TEXTURE_2D, state.arcadeScreenTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            textureWidth,
            textureHeight,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            nullptr);

        glGenRenderbuffers(1, &state.arcadeScreenDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, state.arcadeScreenDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, textureWidth, textureHeight);

        glGenFramebuffers(1, &state.arcadeScreenFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, state.arcadeScreenFramebuffer);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            state.arcadeScreenTexture,
            0);
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER,
            state.arcadeScreenDepth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            destroy_scene_preview_pipeline(state);
            return false;
        }

        glGenVertexArrays(1, &state.arcadeScreenVao);
        glGenBuffers(1, &state.arcadeScreenVbo);
        glGenBuffers(1, &state.arcadeScreenEbo);
        glBindVertexArray(state.arcadeScreenVao);
        glBindBuffer(GL_ARRAY_BUFFER, state.arcadeScreenVbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(sizeof(detail::ArcadeScreenVertex) * 4u),
            nullptr,
            GL_DYNAMIC_DRAW);
        const std::array<std::uint32_t, 6> indices{ 0u, 1u, 2u, 0u, 2u, 3u };
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.arcadeScreenEbo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(indices.size() * sizeof(indices[0])),
            indices.data(),
            GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(detail::ArcadeScreenVertex)),
            reinterpret_cast<void*>(offsetof(detail::ArcadeScreenVertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            2,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(detail::ArcadeScreenVertex)),
            reinterpret_cast<void*>(offsetof(detail::ArcadeScreenVertex, u)));
        glBindVertexArray(0);

        state.arcadeScreenMvpLoc = glGetUniformLocation(state.arcadeScreenShader, "uMvp");
        state.arcadeScreenTextureLoc = glGetUniformLocation(state.arcadeScreenShader, "uTexture");
        return state.arcadeScreenMvpLoc >= 0 && state.arcadeScreenTextureLoc >= 0;
    }

    inline void render_engine_arcade_sampled_surface_preview(
        const core::Context* ctx,
        epochengine::openglstate::OpenGL4State& state,
        const detail::Mat4& mvp,
        int framebufferWidth,
        int framebufferHeight,
        int viewportX,
        int viewportY,
        int viewportWidth,
        int viewportHeight)
    {
        const auto screenMarkers = epochengine::previewgrid::sampled_render_surface_markers_for(ctx);
        if (screenMarkers.empty())
            return;

        if (!ensure_arcade_screen_preview_pipeline(state))
            return;

        const int textureWidth = static_cast<int>(epochengine::package_registry::engine_arcade_render_texture_width());
        const int textureHeight = static_cast<int>(epochengine::package_registry::engine_arcade_render_texture_height());
        detail::render_arcade_attract_pattern(state, textureWidth, textureHeight);

        const int glViewportY = (std::max)(0, framebufferHeight - (viewportY + viewportHeight));
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(viewportX, glViewportY, viewportWidth, viewportHeight);
        glScissor(viewportX, glViewportY, viewportWidth, viewportHeight);
        glEnable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        glUseProgram(state.arcadeScreenShader);
        glUniformMatrix4fv(state.arcadeScreenMvpLoc, 1, GL_FALSE, mvp.data());
        glUniform1i(state.arcadeScreenTextureLoc, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, state.arcadeScreenTexture);
        glBindVertexArray(state.arcadeScreenVao);
        glBindBuffer(GL_ARRAY_BUFFER, state.arcadeScreenVbo);

        for (const auto& marker : screenMarkers)
        {
            const auto vertices = detail::make_arcade_screen_vertices(marker);
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(vertices.size() * sizeof(vertices[0])),
                vertices.data(),
                GL_DYNAMIC_DRAW);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        }

        glBindVertexArray(0);
        glUseProgram(0);
    }

    inline void render_scene_preview(
        const core::Context* ctx,
        epochengine::openglstate::OpenGL4State& state,
        core::ScenePreviewMode previewMode,
        int framebufferWidth,
        int framebufferHeight,
        int viewportX,
        int viewportY,
        int viewportWidth,
        int viewportHeight)
    {
        const detail::ScopedPreviewGLState preservedState;
        const int glViewportY = (std::max)(0, framebufferHeight - (viewportY + viewportHeight));

        glDisable(GL_BLEND);
        glEnable(GL_SCISSOR_TEST);
        glScissor(viewportX, glViewportY, viewportWidth, viewportHeight);
        glViewport(viewportX, glViewportY, viewportWidth, viewportHeight);
        glDepthMask(GL_TRUE);
        const auto clearColor = epochengine::previewgrid::kClearColor;
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (previewMode == core::ScenePreviewMode::Editor && ensure_scene_preview_pipeline(state))
        {
            const auto camera = epochengine::previewgrid::camera_for(ctx);
            const float aspect = viewportHeight > 0
                ? (viewportWidth / static_cast<float>(viewportHeight))
                : 1.0f;
            const detail::Mat4 proj = epochengine::previewgrid::projection_for(ctx, aspect, camera);
            const detail::Mat4 view = epochengine::previewgrid::look_at(
                camera.eye,
                camera.target,
                camera.up);
            const detail::Mat4 mvp = epochengine::previewgrid::multiply(proj, view);

            glEnable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDepthFunc(GL_LEQUAL);
            glUseProgram(state.sceneShader);
            glUniformMatrix4fv(state.sceneMvpLoc, 1, GL_FALSE, mvp.data());

            // The grid is visual reference, not depth authority. Let objects draw over it
            // deterministically instead of fighting coplanar/near-coplanar helper pixels.
            glDepthMask(GL_FALSE);
            glBindVertexArray(state.sceneVao);
            glDrawElements(
                GL_LINES,
                static_cast<GLsizei>(epochengine::previewgrid::grid_indices().size()),
                GL_UNSIGNED_INT,
                nullptr);

            auto solidVertices = epochengine::previewgrid::object_solid_vertices_for(ctx);
            if (solidVertices.size() >= 3 && state.sceneMarkerVao && state.sceneMarkerVbo)
            {
                glDepthMask(GL_TRUE);
                glBindVertexArray(state.sceneMarkerVao);
                glBindBuffer(GL_ARRAY_BUFFER, state.sceneMarkerVbo);
                glBufferData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(solidVertices.size() * sizeof(solidVertices[0])),
                    solidVertices.data(),
                    GL_DYNAMIC_DRAW);
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(solidVertices.size()));
            }

            render_engine_arcade_sampled_surface_preview(
                ctx,
                state,
                mvp,
                framebufferWidth,
                framebufferHeight,
                viewportX,
                viewportY,
                viewportWidth,
                viewportHeight);

            std::vector<epochengine::previewgrid::Vertex> dynamicVertices{};
            const auto focusVertices = epochengine::previewgrid::look_marker_vertices_for(ctx);
            const auto focusCount = epochengine::previewgrid::look_marker_vertex_count_for(ctx);
            if (focusCount > 0)
                dynamicVertices.insert(dynamicVertices.end(), focusVertices.begin(), focusVertices.begin() + focusCount);
            auto objectVertices = epochengine::previewgrid::object_marker_vertices_for(ctx);
            dynamicVertices.insert(dynamicVertices.end(), objectVertices.begin(), objectVertices.end());

            if (dynamicVertices.size() >= 2 && state.sceneMarkerVao && state.sceneMarkerVbo)
            {
                // Editor helpers are overlay controls. Drawing them without depth test avoids
                // face-edge z-fighting and keeps selection feedback stable while orbiting.
                glDepthMask(GL_FALSE);
                glDisable(GL_DEPTH_TEST);
                glBindVertexArray(state.sceneMarkerVao);
                glBindBuffer(GL_ARRAY_BUFFER, state.sceneMarkerVbo);
                glBufferData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(dynamicVertices.size() * sizeof(dynamicVertices[0])),
                    dynamicVertices.data(),
                    GL_DYNAMIC_DRAW);
                glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(dynamicVertices.size()));
            }
            glBindVertexArray(0);
            glUseProgram(0);
        }
    }
}
#endif
