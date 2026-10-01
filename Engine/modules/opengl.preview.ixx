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
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <optional>

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
#   include <glad/glad.h>
#endif

export module opengl.preview;

import core.context;
import opengl.state;
import package.registry;
import render.arcade;
import render.event_debug;
import render.lighting;
import render.neuromorphic_invalidation;
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

        if (state.sceneCacheFramebuffer && glIsFramebuffer(state.sceneCacheFramebuffer)) glDeleteFramebuffers(1, &state.sceneCacheFramebuffer);
        if (state.sceneCacheDepth && glIsRenderbuffer(state.sceneCacheDepth)) glDeleteRenderbuffers(1, &state.sceneCacheDepth);
        if (state.sceneCacheColor && glIsTexture(state.sceneCacheColor)) glDeleteTextures(1, &state.sceneCacheColor);
        state.sceneCacheFramebuffer = 0;
        state.sceneCacheDepth = 0;
        state.sceneCacheColor = 0;
        state.sceneCacheWidth = 0;
        state.sceneCacheHeight = 0;
        state.sceneCacheCameraRevision = 0;
        state.sceneCacheGeometryRevision = 0;
        state.sceneCacheValid = false;
        state.sceneInvalidationNetwork.reset();

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
        state.sceneGridSignature = 0;
        state.sceneGridIndexCount = 0;
        state.sceneMarkerVao = 0;
        state.sceneMarkerVbo = 0;
    }

    inline bool ensure_scene_preview_pipeline(
        const core::Context* ctx,
        epochengine::openglstate::OpenGL4State& state)
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

        const auto gridGeometry = epochengine::previewgrid::grid_geometry_for(ctx);
        const auto& vertices = gridGeometry->vertices;
        const auto& indices = gridGeometry->indices;

        glGenVertexArrays(1, &state.sceneVao);
        glGenBuffers(1, &state.sceneVbo);
        glGenBuffers(1, &state.sceneEbo);

        glBindVertexArray(state.sceneVao);
        glBindBuffer(GL_ARRAY_BUFFER, state.sceneVbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(vertices.size() * sizeof(vertices[0])),
            vertices.data(),
            GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.sceneEbo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(indices.size() * sizeof(indices[0])),
            indices.data(),
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
        state.sceneGridSignature = gridGeometry->signature;
        state.sceneGridIndexCount = static_cast<GLsizei>(indices.size());

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
        GLint destinationFramebuffer = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &destinationFramebuffer);
        detail::render_arcade_attract_pattern(state, textureWidth, textureHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(destinationFramebuffer));

        const int glViewportY = (std::max)(0, framebufferHeight - (viewportY + viewportHeight));
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

    namespace detail
    {
        struct DirtyRect final
        {
            int x{};
            int y{};
            int width{};
            int height{};
        };

        [[nodiscard]] inline bool ensure_scene_cache(
            epochengine::openglstate::OpenGL4State& state,
            int width,
            int height)
        {
            if (width <= 0 || height <= 0 || !glBlitFramebuffer)
                return false;
            if (state.sceneCacheFramebuffer
                && state.sceneCacheColor
                && state.sceneCacheDepth
                && state.sceneCacheWidth == width
                && state.sceneCacheHeight == height)
                return true;

            if (state.sceneCacheFramebuffer && glIsFramebuffer(state.sceneCacheFramebuffer))
                glDeleteFramebuffers(1, &state.sceneCacheFramebuffer);
            if (state.sceneCacheDepth && glIsRenderbuffer(state.sceneCacheDepth))
                glDeleteRenderbuffers(1, &state.sceneCacheDepth);
            if (state.sceneCacheColor && glIsTexture(state.sceneCacheColor))
                glDeleteTextures(1, &state.sceneCacheColor);

            state.sceneCacheFramebuffer = 0;
            state.sceneCacheColor = 0;
            state.sceneCacheDepth = 0;
            state.sceneCacheValid = false;

            glGenTextures(1, &state.sceneCacheColor);
            glBindTexture(GL_TEXTURE_2D, state.sceneCacheColor);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            glGenRenderbuffers(1, &state.sceneCacheDepth);
            glBindRenderbuffer(GL_RENDERBUFFER, state.sceneCacheDepth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

            glGenFramebuffers(1, &state.sceneCacheFramebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, state.sceneCacheFramebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, state.sceneCacheColor, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, state.sceneCacheDepth);

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                if (state.sceneCacheFramebuffer) glDeleteFramebuffers(1, &state.sceneCacheFramebuffer);
                if (state.sceneCacheDepth) glDeleteRenderbuffers(1, &state.sceneCacheDepth);
                if (state.sceneCacheColor) glDeleteTextures(1, &state.sceneCacheColor);
                state.sceneCacheFramebuffer = 0;
                state.sceneCacheDepth = 0;
                state.sceneCacheColor = 0;
                state.sceneCacheWidth = 0;
                state.sceneCacheHeight = 0;
                return false;
            }

            state.sceneCacheWidth = width;
            state.sceneCacheHeight = height;
            state.sceneCacheCameraRevision = 0;
            state.sceneCacheGeometryRevision = 0;
            return true;
        }

        [[nodiscard]] inline std::optional<DirtyRect> marker_dirty_rect(
            const epochengine::previewgrid::ObjectMarker& marker,
            const Mat4& mvp,
            int width,
            int height) noexcept
        {
            const float maxScale = (std::max)({
                std::abs(marker.scale.x),
                std::abs(marker.scale.y),
                std::abs(marker.scale.z),
                marker.radius * 2.0f,
                0.25f
            });
            // Bounding sphere projected as an AABB. sqrt(3)/2 encloses a rotated unit cube.
            const float extent = maxScale * 0.90f + marker.radius;
            const epochengine::previewgrid::Vec3 corners[8]{
                { marker.position.x - extent, marker.position.y - extent, marker.position.z - extent },
                { marker.position.x + extent, marker.position.y - extent, marker.position.z - extent },
                { marker.position.x - extent, marker.position.y + extent, marker.position.z - extent },
                { marker.position.x + extent, marker.position.y + extent, marker.position.z - extent },
                { marker.position.x - extent, marker.position.y - extent, marker.position.z + extent },
                { marker.position.x + extent, marker.position.y - extent, marker.position.z + extent },
                { marker.position.x - extent, marker.position.y + extent, marker.position.z + extent },
                { marker.position.x + extent, marker.position.y + extent, marker.position.z + extent }
            };

            float minX = 1.0f;
            float maxX = -1.0f;
            float minY = 1.0f;
            float maxY = -1.0f;
            bool any = false;
            for (const auto& corner : corners)
            {
                const auto clip = epochengine::previewgrid::transform_point(mvp, corner);
                if (clip.w <= 0.0001f)
                    return DirtyRect{ 0, 0, width, height };
                const float x = clip.x / clip.w;
                const float y = clip.y / clip.w;
                minX = (std::min)(minX, x);
                maxX = (std::max)(maxX, x);
                minY = (std::min)(minY, y);
                maxY = (std::max)(maxY, y);
                any = true;
            }
            if (!any || maxX < -1.0f || minX > 1.0f || maxY < -1.0f || minY > 1.0f)
                return std::nullopt;

            minX = (std::clamp)(minX, -1.0f, 1.0f);
            maxX = (std::clamp)(maxX, -1.0f, 1.0f);
            minY = (std::clamp)(minY, -1.0f, 1.0f);
            maxY = (std::clamp)(maxY, -1.0f, 1.0f);

            constexpr int pad = 6;
            int x0 = static_cast<int>((minX * 0.5f + 0.5f) * static_cast<float>(width)) - pad;
            int x1 = static_cast<int>((maxX * 0.5f + 0.5f) * static_cast<float>(width)) + pad;
            int y0 = static_cast<int>((minY * 0.5f + 0.5f) * static_cast<float>(height)) - pad;
            int y1 = static_cast<int>((maxY * 0.5f + 0.5f) * static_cast<float>(height)) + pad;
            x0 = (std::clamp)(x0, 0, width);
            x1 = (std::clamp)(x1, 0, width);
            y0 = (std::clamp)(y0, 0, height);
            y1 = (std::clamp)(y1, 0, height);
            if (x1 <= x0 || y1 <= y0)
                return std::nullopt;
            return DirtyRect{ x0, y0, x1 - x0, y1 - y0 };
        }

        [[nodiscard]] inline std::optional<DirtyRect> light_influence_dirty_rect(
            const epochengine::lighting::LightInfluenceVolume& influence,
            const Mat4& mvp,
            int width,
            int height) noexcept
        {
            if (!std::isfinite(influence.radius) || influence.radius <= 0.0f)
                return DirtyRect{0, 0, width, height};
            epochengine::previewgrid::ObjectMarker proxy{};
            proxy.position = {
                influence.center.x,
                influence.center.y,
                influence.center.z
            };
            const float scale = influence.radius / 0.90f;
            proxy.scale = {scale, scale, scale};
            proxy.radius = 0.0f;
            return marker_dirty_rect(proxy, mvp, width, height);
        }

        inline void append_union_rect(
            std::vector<DirtyRect>& rects,
            const DirtyRect& candidate,
            int width,
            int height)
        {
            if (candidate.width <= 0 || candidate.height <= 0)
                return;
            if (candidate.x == 0 && candidate.y == 0
                && candidate.width >= width && candidate.height >= height)
            {
                rects.assign(1, DirtyRect{ 0, 0, width, height });
                return;
            }

            DirtyRect merged = candidate;
            std::size_t index = 0u;
            while (index < rects.size())
            {
                const auto& rect = rects[index];
                const int ax1 = rect.x + rect.width;
                const int ay1 = rect.y + rect.height;
                const int bx1 = merged.x + merged.width;
                const int by1 = merged.y + merged.height;
                const bool touches = !(ax1 + 4 < merged.x || bx1 + 4 < rect.x
                    || ay1 + 4 < merged.y || by1 + 4 < rect.y);
                if (!touches)
                {
                    ++index;
                    continue;
                }

                const int x0 = (std::min)(rect.x, merged.x);
                const int y0 = (std::min)(rect.y, merged.y);
                const int x1 = (std::max)(ax1, bx1);
                const int y1 = (std::max)(ay1, by1);
                merged = DirtyRect{ x0, y0, x1 - x0, y1 - y0 };
                rects.erase(rects.begin() + static_cast<std::ptrdiff_t>(index));
                index = 0u;
            }
            rects.push_back(merged);
        }

        inline void draw_dirty_rect_overlay(
            const std::vector<DirtyRect>& rects,
            int viewportX,
            int destinationY,
            bool fullRedraw) noexcept
        {
            if (rects.empty() || !epochengine::render_event_debug::dirty_overlay_enabled())
                return;

            constexpr int thickness = 2;
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDisable(GL_BLEND);
            glEnable(GL_SCISSOR_TEST);
            if (fullRedraw)
                glClearColor(1.0f, 0.20f, 0.12f, 1.0f);
            else
                glClearColor(0.95f, 0.82f, 0.10f, 1.0f);

            for (const auto& rect : rects)
            {
                const int x = viewportX + rect.x;
                const int y = destinationY + rect.y;
                const int w = (std::max)(0, rect.width);
                const int h = (std::max)(0, rect.height);
                if (w <= 0 || h <= 0)
                    continue;

                glScissor(x, y, w, (std::min)(thickness, h));
                glClear(GL_COLOR_BUFFER_BIT);
                glScissor(x, y + (std::max)(0, h - thickness), w, (std::min)(thickness, h));
                glClear(GL_COLOR_BUFFER_BIT);
                glScissor(x, y, (std::min)(thickness, w), h);
                glClear(GL_COLOR_BUFFER_BIT);
                glScissor(x + (std::max)(0, w - thickness), y, (std::min)(thickness, w), h);
                glClear(GL_COLOR_BUFFER_BIT);
            }
        }

        inline void draw_vacated_rect_overlay(
            const std::vector<DirtyRect>& rects,
            int viewportX,
            int destinationY) noexcept
        {
            if (rects.empty() || !epochengine::render_event_debug::vacated_overlay_enabled())
                return;

            constexpr int thickness = 2;
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDisable(GL_BLEND);
            glEnable(GL_SCISSOR_TEST);
            // Cyan identifies the previous/vacated object footprint. This overlay is
            // diagnostic only; previous bounds are always invalidated regardless of this toggle.
            glClearColor(0.10f, 0.90f, 1.0f, 1.0f);

            for (const auto& rect : rects)
            {
                const int x = viewportX + rect.x;
                const int y = destinationY + rect.y;
                const int w = (std::max)(0, rect.width);
                const int h = (std::max)(0, rect.height);
                if (w <= 0 || h <= 0)
                    continue;

                glScissor(x, y, w, (std::min)(thickness, h));
                glClear(GL_COLOR_BUFFER_BIT);
                glScissor(x, y + (std::max)(0, h - thickness), w, (std::min)(thickness, h));
                glClear(GL_COLOR_BUFFER_BIT);
                glScissor(x, y, (std::min)(thickness, w), h);
                glClear(GL_COLOR_BUFFER_BIT);
                glScissor(x + (std::max)(0, w - thickness), y, (std::min)(thickness, w), h);
                glClear(GL_COLOR_BUFFER_BIT);
            }
        }
    }

    inline void render_scene_preview_conventional(
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

        if (previewMode == core::ScenePreviewMode::Editor && ensure_scene_preview_pipeline(ctx, state))
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

            const auto gridGeometry = epochengine::previewgrid::grid_geometry_for(ctx);
            if (state.sceneGridSignature != gridGeometry->signature)
            {
                glBindVertexArray(state.sceneVao);
                glBindBuffer(GL_ARRAY_BUFFER, state.sceneVbo);
                glBufferData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(
                        gridGeometry->vertices.size() * sizeof(gridGeometry->vertices[0])),
                    gridGeometry->vertices.data(),
                    GL_DYNAMIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.sceneEbo);
                glBufferData(
                    GL_ELEMENT_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(
                        gridGeometry->indices.size() * sizeof(gridGeometry->indices[0])),
                    gridGeometry->indices.data(),
                    GL_DYNAMIC_DRAW);
                state.sceneGridSignature = gridGeometry->signature;
                state.sceneGridIndexCount =
                    static_cast<GLsizei>(gridGeometry->indices.size());
            }

            // The grid is visual reference, not depth authority. Let objects draw over it
            // deterministically instead of fighting coplanar/near-coplanar helper pixels.
            glDepthMask(GL_FALSE);
            glBindVertexArray(state.sceneVao);
            glDrawElements(
                GL_LINES,
                state.sceneGridIndexCount,
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
        using clock = std::chrono::steady_clock;
        const auto renderStart = clock::now();
        const auto neuralTimestampNs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                renderStart.time_since_epoch()).count());
        const auto elapsed_us = [&]() noexcept -> std::uint64_t
        {
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    clock::now() - renderStart).count());
        };
        const auto elapsed_ns_from = [](const clock::time_point start) noexcept -> std::uint64_t
        {
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    clock::now() - start).count());
        };

        const detail::ScopedPreviewGLState preservedState;
        if (!ctx || viewportWidth <= 0 || viewportHeight <= 0)
            return;

        const int destinationY = (std::max)(0, framebufferHeight - (viewportY + viewportHeight));
        const auto clearColor = epochengine::previewgrid::kClearColor;
        const std::uint64_t cameraRevision = epochengine::previewgrid::camera_revision_for(ctx);
        const std::uint64_t geometryRevision = epochengine::previewgrid::preview_content_revision_for(ctx);

        epochengine::render_event_debug::FallbackReason conventionalReason =
            epochengine::render_event_debug::FallbackReason::none;
        bool useConventional = false;
        if (!epochengine::render_event_debug::event_path_enabled())
        {
            useConventional = true;
            conventionalReason = epochengine::render_event_debug::FallbackReason::event_path_disabled;
        }
        else if (previewMode != core::ScenePreviewMode::Editor)
        {
            useConventional = true;
            conventionalReason = epochengine::render_event_debug::FallbackReason::unsupported_preview_mode;
        }
        else if (!ensure_scene_preview_pipeline(ctx, state))
        {
            useConventional = true;
            conventionalReason = epochengine::render_event_debug::FallbackReason::preview_pipeline_unavailable;
        }
        else if (!detail::ensure_scene_cache(state, viewportWidth, viewportHeight))
        {
            useConventional = true;
            conventionalReason = epochengine::render_event_debug::FallbackReason::scene_cache_unavailable;
        }

        if (useConventional)
        {
            render_scene_preview_conventional(
                ctx, state, previewMode, framebufferWidth, framebufferHeight,
                viewportX, viewportY, viewportWidth, viewportHeight);
            epochengine::render_event_debug::publish(
                epochengine::render_event_debug::Path::conventional,
                conventionalReason,
                0u,
                0u,
                100.0f,
                cameraRevision,
                geometryRevision,
                elapsed_us());
            return;
        }

        const auto benchmarkLane = epochengine::render_event_debug::begin_benchmark_frame();
        clock::time_point benchmarkStart{};
        if (benchmarkLane != epochengine::render_event_debug::BenchmarkLane::disabled)
        {
            // Drain earlier GUI/backend work before timing this scene path. The matching
            // glFinish below makes each A/B sample represent scene submission + GPU completion
            // rather than whichever previous frame happened to still be in flight.
            glFinish();
            benchmarkStart = clock::now();
        }

        const auto camera = epochengine::previewgrid::camera_for(ctx);
        const float aspect = viewportHeight > 0
            ? (viewportWidth / static_cast<float>(viewportHeight))
            : 1.0f;
        const detail::Mat4 proj = epochengine::previewgrid::projection_for(ctx, aspect, camera);
        const detail::Mat4 view = epochengine::previewgrid::look_at(camera.eye, camera.target, camera.up);
        const detail::Mat4 mvp = epochengine::previewgrid::multiply(proj, view);
        const auto changeFrame = epochengine::previewgrid::consume_object_marker_change_frame_for(ctx);
        const auto lightingDamage = epochengine::previewgrid::consume_lighting_invalidation_frame_for(ctx);
        const bool sampledSurfaceActive = !epochengine::previewgrid::sampled_render_surface_markers_for(ctx).empty();

        std::vector<detail::DirtyRect> dirtyRects{};
        std::vector<detail::DirtyRect> vacatedRects{};
        auto fallbackReason = epochengine::render_event_debug::FallbackReason::none;
        bool fullRedraw = false;

        if (!state.sceneCacheValid)
        {
            fullRedraw = true;
            fallbackReason = epochengine::render_event_debug::FallbackReason::cache_invalid;
        }
        else if (state.sceneCacheCameraRevision != cameraRevision)
        {
            fullRedraw = true;
            fallbackReason = epochengine::render_event_debug::FallbackReason::camera_changed;
        }
        else if (sampledSurfaceActive)
        {
            fullRedraw = true;
            fallbackReason = epochengine::render_event_debug::FallbackReason::sampled_surface_active;
        }

        if (benchmarkLane == epochengine::render_event_debug::BenchmarkLane::full_baseline)
        {
            // The B lane deliberately reconstructs the entire persistent scene cache. Using
            // the same cache target and final blit as the selective A lane keeps presentation
            // overhead identical and, critically, leaves the cache current for the next A frame.
            fullRedraw = true;
            fallbackReason = epochengine::render_event_debug::FallbackReason::benchmark_full_baseline;
        }

        std::uint64_t dirtyArea = 0u;
        const std::uint64_t frameArea =
            static_cast<std::uint64_t>(viewportWidth) * static_cast<std::uint64_t>(viewportHeight);

        if (!fullRedraw && state.sceneCacheGeometryRevision != geometryRevision)
        {
            bool haveLocalizedDamage = false;
            bool damageUnavailable = false;

            const bool objectFrameCurrent = changeFrame.revision > state.sceneCacheGeometryRevision
                && changeFrame.revision <= geometryRevision;
            if (objectFrameCurrent)
            {
                if (changeFrame.global_invalidation)
                {
                    fullRedraw = true;
                    fallbackReason = epochengine::render_event_debug::FallbackReason::global_invalidation;
                }
                else
                {
                    for (const auto& change : changeFrame.changes)
                    {
                        if (change.had_previous)
                        {
                            if (const auto rect = detail::marker_dirty_rect(
                                    change.previous, mvp, viewportWidth, viewportHeight))
                            {
                                detail::append_union_rect(
                                    dirtyRects, *rect, viewportWidth, viewportHeight);
                                detail::append_union_rect(
                                    vacatedRects, *rect, viewportWidth, viewportHeight);
                                haveLocalizedDamage = true;
                            }
                        }
                        if (change.has_current)
                        {
                            if (const auto rect = detail::marker_dirty_rect(
                                    change.current, mvp, viewportWidth, viewportHeight))
                            {
                                detail::append_union_rect(
                                    dirtyRects, *rect, viewportWidth, viewportHeight);
                                haveLocalizedDamage = true;
                            }
                        }
                    }
                }
            }

            const bool lightingFrameCurrent = lightingDamage.revision > state.sceneCacheGeometryRevision
                && lightingDamage.revision <= geometryRevision;
            if (!fullRedraw && lightingFrameCurrent)
            {
                if (lightingDamage.global_invalidation)
                {
                    fullRedraw = true;
                    fallbackReason = epochengine::render_event_debug::FallbackReason::global_invalidation;
                }
                else
                {
                    for (const auto& change : lightingDamage.changes)
                    {
                        if (change.had_previous)
                        {
                            if (const auto rect = detail::light_influence_dirty_rect(
                                    change.previous, mvp, viewportWidth, viewportHeight))
                            {
                                detail::append_union_rect(
                                    dirtyRects, *rect, viewportWidth, viewportHeight);
                                haveLocalizedDamage = true;
                            }
                        }
                        if (change.has_current)
                        {
                            if (const auto rect = detail::light_influence_dirty_rect(
                                    change.current, mvp, viewportWidth, viewportHeight))
                            {
                                detail::append_union_rect(
                                    dirtyRects, *rect, viewportWidth, viewportHeight);
                                haveLocalizedDamage = true;
                            }
                        }
                    }
                }
            }

            if (!fullRedraw && !haveLocalizedDamage)
            {
                damageUnavailable = true;
                fullRedraw = true;
                fallbackReason = epochengine::render_event_debug::FallbackReason::change_frame_unavailable;
            }

            if (!fullRedraw)
            {
                dirtyArea = 0u;
                for (const auto& rect : dirtyRects)
                {
                    dirtyArea += static_cast<std::uint64_t>(rect.width)
                        * static_cast<std::uint64_t>(rect.height);
                }

                if (dirtyRects.size() > 12u)
                {
                    fullRedraw = true;
                    fallbackReason = epochengine::render_event_debug::FallbackReason::dirty_region_limit;
                }
                else if (frameArea > 0u && dirtyArea * 10u >= frameArea * 6u)
                {
                    fullRedraw = true;
                    fallbackReason = epochengine::render_event_debug::FallbackReason::dirty_coverage_limit;
                }
                else if (!dirtyRects.empty())
                {
                    std::vector<epochengine::render::neuromorphic_invalidation::ChangeSample> neuralSamples{};
                    neuralSamples.reserve(dirtyRects.size());
                    for (std::size_t index = 0u; index < dirtyRects.size(); ++index)
                    {
                        const auto& rect = dirtyRects[index];
                        const std::uint64_t area = static_cast<std::uint64_t>(rect.width)
                            * static_cast<std::uint64_t>(rect.height);
                        const float areaFraction = frameArea > 0u
                            ? static_cast<float>(area) / static_cast<float>(frameArea)
                            : 1.0f;
                        const std::uint64_t source =
                            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(rect.x)) << 32u)
                            ^ static_cast<std::uint64_t>(static_cast<std::uint32_t>(rect.y))
                            ^ (static_cast<std::uint64_t>(index + 1u) * 0x9e3779b97f4a7c15ull);
                        neuralSamples.push_back({
                            .source = source == 0u ? static_cast<std::uint64_t>(index + 1u) : source,
                            .magnitude = (std::clamp)(0.30f + areaFraction * 8.0f, 0.30f, 4.0f),
                            .salience = 1.0f
                        });
                    }

                    const auto neuralDecision = state.sceneInvalidationNetwork.evaluate(
                        neuralTimestampNs,
                        neuralSamples);
                    if (neuralDecision.force_full_redraw)
                    {
                        fullRedraw = true;
                        fallbackReason = epochengine::render_event_debug::FallbackReason::neuromorphic_dense_activity;
                    }
                }
            }
            (void)damageUnavailable;
        }

        if (fullRedraw)
        {
            if (fallbackReason != epochengine::render_event_debug::FallbackReason::neuromorphic_dense_activity
                && fallbackReason != epochengine::render_event_debug::FallbackReason::benchmark_full_baseline)
            {
                state.sceneInvalidationNetwork.reset(neuralTimestampNs);
            }
            dirtyRects.assign(1, detail::DirtyRect{ 0, 0, viewportWidth, viewportHeight });
            vacatedRects.clear();
            dirtyArea = frameArea;
        }

        if (!dirtyRects.empty())
        {
            glBindFramebuffer(GL_FRAMEBUFFER, state.sceneCacheFramebuffer);
            glViewport(0, 0, viewportWidth, viewportHeight);
            glDisable(GL_BLEND);
            glEnable(GL_SCISSOR_TEST);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_TRUE);

            const auto gridGeometry = epochengine::previewgrid::grid_geometry_for(ctx);
            if (state.sceneGridSignature != gridGeometry->signature)
            {
                glBindVertexArray(state.sceneVao);
                glBindBuffer(GL_ARRAY_BUFFER, state.sceneVbo);
                glBufferData(GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(gridGeometry->vertices.size() * sizeof(gridGeometry->vertices[0])),
                    gridGeometry->vertices.data(), GL_DYNAMIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.sceneEbo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(gridGeometry->indices.size() * sizeof(gridGeometry->indices[0])),
                    gridGeometry->indices.data(), GL_DYNAMIC_DRAW);
                state.sceneGridSignature = gridGeometry->signature;
                state.sceneGridIndexCount = static_cast<GLsizei>(gridGeometry->indices.size());
            }

            auto solidVertices = epochengine::previewgrid::object_solid_vertices_for(ctx);
            std::vector<epochengine::previewgrid::Vertex> dynamicVertices{};
            const auto focusVertices = epochengine::previewgrid::look_marker_vertices_for(ctx);
            const auto focusCount = epochengine::previewgrid::look_marker_vertex_count_for(ctx);
            if (focusCount > 0)
                dynamicVertices.insert(dynamicVertices.end(), focusVertices.begin(), focusVertices.begin() + focusCount);
            auto objectVertices = epochengine::previewgrid::object_marker_vertices_for(ctx);
            dynamicVertices.insert(dynamicVertices.end(), objectVertices.begin(), objectVertices.end());

            for (const auto& rect : dirtyRects)
            {
                glScissor(rect.x, rect.y, rect.width, rect.height);
                glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                glUseProgram(state.sceneShader);
                glUniformMatrix4fv(state.sceneMvpLoc, 1, GL_FALSE, mvp.data());

                glDepthMask(GL_FALSE);
                glEnable(GL_DEPTH_TEST);
                glBindVertexArray(state.sceneVao);
                glDrawElements(GL_LINES, state.sceneGridIndexCount, GL_UNSIGNED_INT, nullptr);

                if (solidVertices.size() >= 3 && state.sceneMarkerVao && state.sceneMarkerVbo)
                {
                    glDepthMask(GL_TRUE);
                    glEnable(GL_DEPTH_TEST);
                    glBindVertexArray(state.sceneMarkerVao);
                    glBindBuffer(GL_ARRAY_BUFFER, state.sceneMarkerVbo);
                    glBufferData(GL_ARRAY_BUFFER,
                        static_cast<GLsizeiptr>(solidVertices.size() * sizeof(solidVertices[0])),
                        solidVertices.data(), GL_DYNAMIC_DRAW);
                    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(solidVertices.size()));
                }

                render_engine_arcade_sampled_surface_preview(
                    ctx, state, mvp, viewportWidth, viewportHeight, 0, 0, viewportWidth, viewportHeight);

                if (dynamicVertices.size() >= 2 && state.sceneMarkerVao && state.sceneMarkerVbo)
                {
                    glDepthMask(GL_FALSE);
                    glDisable(GL_DEPTH_TEST);
                    glBindVertexArray(state.sceneMarkerVao);
                    glBindBuffer(GL_ARRAY_BUFFER, state.sceneMarkerVbo);
                    glBufferData(GL_ARRAY_BUFFER,
                        static_cast<GLsizeiptr>(dynamicVertices.size() * sizeof(dynamicVertices[0])),
                        dynamicVertices.data(), GL_DYNAMIC_DRAW);
                    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(dynamicVertices.size()));
                }
            }

            glBindVertexArray(0);
            glUseProgram(0);
            state.sceneCacheCameraRevision = cameraRevision;
            state.sceneCacheGeometryRevision = geometryRevision;
            state.sceneCacheValid = true;
        }
        else if (!fullRedraw && state.sceneCacheGeometryRevision != geometryRevision)
        {
            // A valid change frame can contain only off-screen mutations. No pixels need
            // reconstruction, but the cache still advances to the new scene revision.
            state.sceneCacheGeometryRevision = geometryRevision;
        }

        // Present the complete cached scene every frame. Unchanged scene pixels are reused;
        // GUI composition can continue normally on the real framebuffer.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, state.sceneCacheFramebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(preservedState.drawFramebuffer));
        glDisable(GL_SCISSOR_TEST);
        glBlitFramebuffer(
            0, 0, viewportWidth, viewportHeight,
            viewportX, destinationY, viewportX + viewportWidth, destinationY + viewportHeight,
            GL_COLOR_BUFFER_BIT, GL_NEAREST);

        std::uint64_t benchmarkDurationNs = 0u;
        if (benchmarkLane != epochengine::render_event_debug::BenchmarkLane::disabled)
        {
            glFinish();
            benchmarkDurationNs = elapsed_ns_from(benchmarkStart);
            epochengine::render_event_debug::record_benchmark_sample(
                benchmarkLane,
                benchmarkDurationNs);
        }

        detail::draw_dirty_rect_overlay(
            dirtyRects,
            viewportX,
            destinationY,
            fullRedraw);
        detail::draw_vacated_rect_overlay(
            vacatedRects,
            viewportX,
            destinationY);

        if (benchmarkLane != epochengine::render_event_debug::BenchmarkLane::disabled
            && (epochengine::render_event_debug::dirty_overlay_enabled()
                || epochengine::render_event_debug::vacated_overlay_enabled()))
        {
            // Keep optional diagnostics out of the measured interval and also out of the next
            // sample by draining them after they are drawn.
            glFinish();
        }

        const float dirtyCoverage = frameArea > 0u
            ? (std::min)(100.0f,
                static_cast<float>(dirtyArea) * 100.0f / static_cast<float>(frameArea))
            : 0.0f;
        const auto path = fullRedraw
            ? epochengine::render_event_debug::Path::full
            : (!dirtyRects.empty()
                ? epochengine::render_event_debug::Path::partial
                : epochengine::render_event_debug::Path::cached);
        epochengine::render_event_debug::publish(
            path,
            fallbackReason,
            static_cast<std::uint32_t>(dirtyRects.size()),
            static_cast<std::uint32_t>(vacatedRects.size()),
            dirtyCoverage,
            cameraRevision,
            geometryRevision,
            benchmarkDurationNs > 0u
                ? (benchmarkDurationNs + 999u) / 1000u
                : elapsed_us());
    }

}
#endif
