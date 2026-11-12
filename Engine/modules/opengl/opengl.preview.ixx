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
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <optional>
#include <source_location>

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
#   include <glad/glad.h>
#endif

export module opengl.preview;

import core.context;
import opengl.state;
import opengl.textures;
import opengl.cloud_volume;
import render.texture_artifact;
import render.texture_residency;
import render.canvas2d;
import render.device;
import asset.texture_artifact;
import core.logger;
import package.registry;
import render.arcade;
import render.lighting;
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
            GLboolean colorMask[4]{}, polygonOffsetEnabled = GL_FALSE;
            GLfloat polygonOffsetFactor{}, polygonOffsetUnits{};
            GLint depthFunc = GL_LESS;
            GLint cullFaceMode{}, frontFace{}, polygonMode[2]{};
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
                glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
                polygonOffsetEnabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);
                glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &polygonOffsetFactor);
                glGetFloatv(GL_POLYGON_OFFSET_UNITS, &polygonOffsetUnits);
                glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
                glGetIntegerv(GL_CULL_FACE_MODE, &cullFaceMode);
                glGetIntegerv(GL_FRONT_FACE, &frontFace);
                glGetIntegerv(GL_POLYGON_MODE, polygonMode);
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
                glCullFace(static_cast<GLenum>(cullFaceMode));
                glFrontFace(static_cast<GLenum>(frontFace));
                glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(polygonMode[0]));
                glDepthMask(depthMask);
                glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
                glPolygonOffset(polygonOffsetFactor, polygonOffsetUnits);
                if (polygonOffsetEnabled == GL_TRUE) glEnable(GL_POLYGON_OFFSET_FILL); else glDisable(GL_POLYGON_OFFSET_FILL);

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

        [[nodiscard]] inline GLuint link_textured_scene_program(GLuint vertexShader, GLuint fragmentShader, bool vertexColor = false)
        {
            GLuint program = glCreateProgram();
            glAttachShader(program, vertexShader);
            glAttachShader(program, fragmentShader);
            glBindAttribLocation(program, 0, "aPos");
            glBindAttribLocation(program, 1, "aUv");
            if (vertexColor) glBindAttribLocation(program, 2, "aColor");
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
        openglcloud::release(state.cloudVolume);
        if (state.meshBaseColor)
        {
            auto& resources=*state.meshBaseColor;
            resources.residency.retire_all();
            if(resources.vbo) glDeleteBuffers(1,&resources.vbo);
            if(resources.vao) glDeleteVertexArrays(1,&resources.vao);
            if(resources.program) glDeleteProgram(resources.program);
            state.meshBaseColor.reset();
        }
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
        state.sceneWireLoc = -1;
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
noperspective out vec3 vWireBarycentric;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    vColor = aColor;
    int corner = gl_VertexID % 3;
    vWireBarycentric = vec3(corner == 0, corner == 1, corner == 2);
})";
            fragmentSource = R"(#version 330 core
in vec3 vColor;
noperspective in vec3 vWireBarycentric;
uniform bool uWireEdges;
out vec4 outColor;
void main() {
    outColor = vec4(vColor, 1.0);
    if (uWireEdges) {
        vec3 dx = dFdx(vWireBarycentric);
        vec3 dy = dFdy(vWireBarycentric);
        vec3 slope = max(sqrt(dx * dx + dy * dy), vec3(1e-6));
        vec3 distanceToEdge = vWireBarycentric / slope;
        float nearestEdge = min(distanceToEdge.x, min(distanceToEdge.y, distanceToEdge.z));
        float inradius = 1.0 / (slope.x + slope.y + slope.z);
        // A mesh finer than the pixel grid otherwise turns into a solid patch.
        float densityFade = smoothstep(0.20, 0.85, inradius);
        outColor.a = densityFade * (1.0 - smoothstep(0.25, 0.85, nearestEdge));
    }
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
        state.sceneWireLoc = glGetUniformLocation(state.sceneShader, "uWireEdges");
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

    namespace detail
    {
        [[nodiscard]] inline std::pair<std::string,std::string> world_texture_shader_sources(bool modern, bool mesh)
        {
            const std::string version=modern?"#version 330 core\n":"#version 120\n";
            const std::string attribute=modern?"in ":"attribute ";
            const std::string varyingOut=modern?"out ":"varying ";
            const std::string varyingIn=modern?"in ":"varying ";
            std::string vertex=version+attribute+"vec3 aPos;\n"+attribute+"vec2 aUv;\n"
                +"uniform mat4 uMvp;\n"+varyingOut+"vec2 vUv;\n";
            if(mesh) vertex+=attribute+"vec3 aColor;\n"+varyingOut+"vec3 vColor;\n";
            vertex+="void main(){gl_Position=uMvp*vec4(aPos,1.0);vUv=aUv;";
            if(mesh) vertex+="vColor=aColor;";
            vertex+="}\n";
            std::string fragment=version+varyingIn+"vec2 vUv;\nuniform sampler2D uTexture;\n";
            if(modern) fragment+="out vec4 outColor;\n";
            if(mesh) fragment+=varyingIn+"vec3 vColor;\nuniform int uDecodeSrgb;\nuniform int uEncodeSrgb;\nuniform int uAlphaCutout;\nuniform float uAlphaCutoff;\n"
                "vec3 decodeSrgb(vec3 c){return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(0.04045),c));}\n"
                "vec3 encodeSrgb(vec3 c){return mix(c*12.92,1.055*pow(max(c,vec3(0.0)),vec3(1.0/2.4))-0.055,step(vec3(0.0031308),c));}\n";
            fragment+="void main(){vec4 texel="+std::string(modern?"texture":"texture2D")+"(uTexture,vUv);vec3 color=texel.rgb;";
            if(mesh) fragment+="if(uAlphaCutout!=0&&texel.a<uAlphaCutoff)discard;if(uDecodeSrgb!=0)color=decodeSrgb(color);color*=vColor;if(uEncodeSrgb!=0)color=encodeSrgb(color);";
            fragment+=std::string(modern?"outColor":"gl_FragColor")+"=vec4(color,1.0);}";
            return {std::move(vertex),std::move(fragment)};
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

        const auto [vertexSource,fragmentSource]=detail::world_texture_shader_sources(
            major>3||(major==3&&minor>=3),false);

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
        int viewportHeight,
        const std::shared_ptr<const epochengine::previewgrid::PublishedSceneFrame>& matchedScene)
    {
        const auto screenMarkers = matchedScene
            ? epochengine::previewgrid::sampled_render_surface_markers_for(*matchedScene)
            : epochengine::previewgrid::sampled_render_surface_markers_for(ctx);
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

    // Uses the same context-owned native texture hooks and residency policy as
    // compiled Canvas2D textures; the geometry remains a perspective world mesh.
    [[nodiscard]] inline bool render_mesh_base_color_preview(const core::Context* ctx,
        epochengine::openglstate::OpenGL4State& state,const detail::Mat4& mvp,
        const std::shared_ptr<const epochengine::previewgrid::PublishedSceneFrame>& matchedScene)
    {
        const auto frame=matchedScene ? matchedScene->solids
            : epochengine::previewgrid::object_solid_frame_for(ctx);
        bool hasTexture{};
        for(const auto& entry:frame->markers) hasTexture=hasTexture||bool(entry.textured&&!entry.textured->empty());
        if(!hasTexture) return true;
        const detail::ScopedPreviewGLState preserved;
        struct TextureZeroScope final
        {
            GLint active{}, texture{}, cull{}, winding{}, sampler{};
            bool samplerObjects{};
            TextureZeroScope()
            {
                glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
                glGetIntegerv(GL_CULL_FACE_MODE,&cull);glGetIntegerv(GL_FRONT_FACE,&winding);
                glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
                samplerObjects=GLAD_GL_VERSION_3_3!=0&&glBindSampler!=nullptr;
                if(samplerObjects){glGetIntegerv(GL_SAMPLER_BINDING,&sampler);glBindSampler(0,0);}
            }
            ~TextureZeroScope()
            {
                glFrontFace(static_cast<GLenum>(winding));glCullFace(static_cast<GLenum>(cull));
                glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(texture));
                if(samplerObjects)glBindSampler(0,static_cast<GLuint>(sampler));
                glActiveTexture(static_cast<GLenum>(active));
            }
        } textureZero;
        if(!state.meshBaseColor)
        {
            try
            {
                state.meshBaseColor=std::make_unique<epochengine::openglstate::MeshBaseColorResources>();
                state.meshBaseColor->device.set_native_texture_hooks(epochengine::opengltextures::make_native_texture_hooks());
            }
            catch(const std::exception&)
            {
                epochengine::logger::error("OpenGL.MeshBaseColor","Native texture residency allocation failed; textured mesh unavailable.");
                return false;
            }
        }
        auto& resources=*state.meshBaseColor;
        const auto fail=[&](const char* message) {
            if(!resources.failed) epochengine::logger::error("OpenGL.MeshBaseColor",message);
            resources.failed=true;return false;
        };
        try
        {
            if(!resources.device.native_texture_hooks_ready()) return fail("Native texture upload hooks unavailable; textured mesh was not drawn.");
            if(!resources.program)
            {
                GLint major{},minor{};glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);
                if(!major){const auto version=detail::parse_gl_version(reinterpret_cast<const char*>(glGetString(GL_VERSION)));major=version.first;minor=version.second;}
                const auto sources=detail::world_texture_shader_sources(major>3||(major==3&&minor>=3),true);
                GLuint vertex{},fragment{};
                try
                {
                    vertex=detail::compile_scene_shader(GL_VERTEX_SHADER,sources.first);
                    fragment=detail::compile_scene_shader(GL_FRAGMENT_SHADER,sources.second);
                    resources.program=detail::link_textured_scene_program(vertex,fragment,true);
                }
                catch(...){if(vertex)glDeleteShader(vertex);if(fragment)glDeleteShader(fragment);throw;}
                glDeleteShader(vertex);glDeleteShader(fragment);
                resources.mvp=glGetUniformLocation(resources.program,"uMvp");
                resources.texture=glGetUniformLocation(resources.program,"uTexture");
                resources.decode_srgb=glGetUniformLocation(resources.program,"uDecodeSrgb");
                resources.encode_srgb=glGetUniformLocation(resources.program,"uEncodeSrgb");
                resources.alpha_cutout=glGetUniformLocation(resources.program,"uAlphaCutout");
                resources.alpha_cutoff=glGetUniformLocation(resources.program,"uAlphaCutoff");
                glGenVertexArrays(1,&resources.vao);glGenBuffers(1,&resources.vbo);
                glBindVertexArray(resources.vao);glBindBuffer(GL_ARRAY_BUFFER,resources.vbo);
                using Vertex=epochengine::previewgrid::TexturedVertex;
                glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,position)));
                glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,uv)));
                glEnableVertexAttribArray(2);glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,color)));
            }
            if(!resources.program||!resources.vao||!resources.vbo||resources.mvp<0||resources.texture<0
                ||resources.decode_srgb<0||resources.encode_srgb<0
                ||resources.alpha_cutout<0||resources.alpha_cutoff<0) return fail("Textured mesh shader or native geometry allocation unavailable.");
            if(++resources.sequence==0){resources.residency.retire_all();resources.sequence=1;}
            if(!resources.residency.begin_frame(resources.sequence)) return fail("Textured mesh residency frame refused.");
            struct ReadyRange final
            {
                const epochengine::previewgrid::SolidDrawMarker* source{};
                GLuint texture{};
                epochengine::texture_residency::ResidencyHandle resident{};
            };
            std::vector<ReadyRange> ranges;ranges.reserve(frame->markers.size());
            struct Unpin final
            {
                epochengine::openglstate::MeshBaseColorResources& resources;
                std::vector<ReadyRange>& ranges;
                ~Unpin(){for(const auto& range:ranges)(void)resources.residency.set_pinned(range.resident,false);}
            } unpin{resources,ranges};
            // Admit the complete material set before issuing any textured draws.
            for(const auto& entry:frame->markers)
            {
                if(!entry.textured||entry.textured->empty()) continue;
                if(!entry.marker.baseColorTexture||!epochengine::previewgrid::valid_mesh_base_color_binding(*entry.marker.baseColorTexture))
                    return fail("Invalid compiled base-color mesh binding.");
                const auto& material=*entry.marker.baseColorTexture;
                epochengine::texture_artifact::ResidencySelection selection{};
                selection.usage=epochengine::texture_artifact::TransferUsage::base_color;
                selection.policy.pin=true;selection.frame_sequence=resources.sequence;
                const auto acquired=epochengine::texture_artifact::acquire_compiled_texture(resources.residency,material.artifact,material.logical,selection);
                if(!acquired) return fail("Compiled base-color texture residency refused; textured mesh was not drawn.");
                const auto* native=resources.device.resolve_texture(acquired.residency.physical);
                ranges.push_back({&entry,native?native->texture_object:0,acquired.residency.handle});
                if(!native||!native->native_allocation_ready||!native->texture_object) return fail("Compiled base-color native texture is not ready.");
            }
            glUseProgram(resources.program);glUniformMatrix4fv(resources.mvp,1,GL_FALSE,mvp.data());
            glUniform1i(resources.texture,0);
            // RGBA8 is physical storage; the sealed artifact retains its actual
            // color space. Decode once, light in linear, encode only when the
            // active framebuffer will not perform that final conversion.
            GLint framebuffer{},encoding{GL_LINEAR};glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&framebuffer);
            GLboolean doubleBuffered{};glGetBooleanv(GL_DOUBLEBUFFER,&doubleBuffered);
            const GLenum attachment=framebuffer?GL_COLOR_ATTACHMENT0:(doubleBuffered?GL_BACK_LEFT:GL_FRONT_LEFT);
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,attachment,GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,&encoding);
            glUniform1i(resources.encode_srgb,glIsEnabled(GL_FRAMEBUFFER_SRGB)==GL_TRUE&&encoding==GL_SRGB?0:1);
            glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(GL_CW);
            glBindVertexArray(resources.vao);glBindBuffer(GL_ARRAY_BUFFER,resources.vbo);
            for(const auto& range:ranges)
            {
                const auto& material=*range.source->marker.baseColorTexture;
                glBindTexture(GL_TEXTURE_2D,range.texture);
                const auto filter=[](auto value){return value==epochengine::FilterMode::linear?GL_LINEAR:GL_NEAREST;};
                const auto address=[](auto value){return value==epochengine::AddressMode::repeat?GL_REPEAT:
                    value==epochengine::AddressMode::mirrored_repeat?GL_MIRRORED_REPEAT:GL_CLAMP_TO_EDGE;};
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filter(material.sampler.min_filter));
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filter(material.sampler.mag_filter));
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,address(material.sampler.address_u));
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,address(material.sampler.address_v));
                glUniform1i(resources.decode_srgb,material.artifact.artifact()->identity.profile.color_space==epochengine::asset::texture::ColorSpace::srgb?1:0);
                glUniform1i(resources.alpha_cutout,material.alpha_cutout?1:0);
                glUniform1f(resources.alpha_cutoff,material.alpha_cutoff);
                const auto& vertices=*range.source->textured;
                glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(vertices.size()*sizeof(vertices[0])),vertices.data(),GL_DYNAMIC_DRAW);
                glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(vertices.size()));
                state.meshTrianglesSubmitted+=vertices.size()/3u;
                ++state.meshDrawCallsSubmitted;
                state.meshUploadBytesSubmitted+=vertices.size()*sizeof(vertices[0]);
            }
            resources.failed=false;return true;
        }
        catch(const std::exception&){return fail("Textured mesh compilation/upload failed; no color-only substitute was used.");}
    }

    inline void render_mesh_wire_preview(const epochengine::previewgrid::MeshWireDrawFrame& frame,
        epochengine::openglstate::OpenGL4State& state, const detail::Mat4& mvp)
    {
        if(!frame.depth_triangles||frame.depth_triangles->empty()
            ||!state.sceneMarkerVao||!state.sceneMarkerVbo) return;
        glUseProgram(state.sceneShader);
        glUniformMatrix4fv(state.sceneMvpLoc,1,GL_FALSE,mvp.data());
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CW);
        glBindVertexArray(state.sceneMarkerVao); glBindBuffer(GL_ARRAY_BUFFER,state.sceneMarkerVbo);
        const bool shaderEdges = state.sceneWireLoc >= 0;
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE); glDepthMask(GL_TRUE);
        if (shaderEdges) glDisable(GL_POLYGON_OFFSET_FILL);
        else { glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(1.0f,1.0f); }
        const auto& triangles=*frame.depth_triangles;
        glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(triangles.size()*sizeof(triangles[0])),triangles.data(),GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(triangles.size()));
        state.meshTrianglesSubmitted+=triangles.size()/3u;
        ++state.meshDrawCallsSubmitted;
        state.meshUploadBytesSubmitted+=triangles.size()*sizeof(triangles[0]);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE); glDepthMask(GL_FALSE);
        if (shaderEdges)
        {
            // Fill rasterization is identical in both passes. Test the true
            // surface depth so grazing water cannot reveal hidden terrain edges.
            glDepthFunc(GL_EQUAL);
            struct ScopedWireEdges final
            {
                GLint location, srcRgb{}, dstRgb{}, srcAlpha{}, dstAlpha{}, equationRgb{}, equationAlpha{};
                GLboolean blendEnabled{};
                explicit ScopedWireEdges(GLint uniform) : location{uniform}
                {
                    blendEnabled = glIsEnabled(GL_BLEND);
                    glGetIntegerv(GL_BLEND_SRC_RGB, &srcRgb);
                    glGetIntegerv(GL_BLEND_DST_RGB, &dstRgb);
                    glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha);
                    glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
                    glGetIntegerv(GL_BLEND_EQUATION_RGB, &equationRgb);
                    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equationAlpha);
                    glEnable(GL_BLEND);
                    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
                    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
                    glUniform1i(location, GL_TRUE);
                }
                ~ScopedWireEdges()
                {
                    glUniform1i(location, GL_FALSE);
                    glBlendFuncSeparate(srcRgb, dstRgb, srcAlpha, dstAlpha);
                    glBlendEquationSeparate(equationRgb, equationAlpha);
                    if (blendEnabled == GL_FALSE) glDisable(GL_BLEND);
                }
            } edges{state.sceneWireLoc};
            glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(triangles.size()));
        }
        else
        {
            glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
            glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(triangles.size()));
            glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        }
        state.meshTrianglesSubmitted+=triangles.size()/3u;
        ++state.meshDrawCallsSubmitted;
        glDepthFunc(GL_LEQUAL);
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
            const auto matchedScene = epochengine::previewgrid::scene_frame_for(ctx);
            const auto camera = matchedScene ? matchedScene->camera
                : epochengine::previewgrid::camera_for(ctx);
            const float aspect = viewportHeight > 0
                ? (viewportWidth / static_cast<float>(viewportHeight))
                : 1.0f;
            const detail::Mat4 proj = epochengine::previewgrid::projection_for(ctx, aspect, camera);
            const detail::Mat4 view = epochengine::previewgrid::look_at(
                camera.eye,
                camera.target,
                camera.up);
            const detail::Mat4 mvp = epochengine::previewgrid::multiply(proj, view);

            // Sample actual render-target depth and the admitted camera, not
            // merely the window's requested pixel format. Bounded evidence keeps
            // distant-geometry diagnostics out of the normal per-frame workload.
            struct DepthEvidence { const core::Context* owner{}; unsigned reports{};
                std::int64_t nextNanoseconds{}; float nearPlane{},farPlane{}; };
            static thread_local DepthEvidence depthEvidence;
            if(depthEvidence.owner!=ctx)depthEvidence={ctx};
            if(matchedScene && depthEvidence.reports<32u)
            {
                const auto evidenceNow=std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                // Startup-only samples miss the orbital-to-ground transition.
                // After those samples, record only a factor-of-two depth change,
                // at most once per five seconds and within the same fixed cap.
                const bool depthChanged=depthEvidence.nearPlane<=0.0f
                    ||camera.nearPlane<depthEvidence.nearPlane*0.5f
                    ||camera.nearPlane>depthEvidence.nearPlane*2.0f
                    ||camera.farPlane<depthEvidence.farPlane*0.5f
                    ||camera.farPlane>depthEvidence.farPlane*2.0f;
                if(evidenceNow>=depthEvidence.nextNanoseconds
                    &&(depthEvidence.reports<8u||depthChanged))
                {
                GLint bits{},drawTarget{};GLdouble range[2]{},clear{};
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawTarget);
                glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,
                    drawTarget ? GL_DEPTH_ATTACHMENT : GL_DEPTH,
                    GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,&bits);
                glGetDoublev(GL_DEPTH_RANGE,range);glGetDoublev(GL_DEPTH_CLEAR_VALUE,&clear);
                logger::get("OpenGL.Depth").logf(logger::LogLevel::INFO,
                    std::source_location::current(),
                    "Preview depth bits={}, target={}, range={:.3f}/{:.3f}, clear={:.3f}; near={:.9g}, far={:.9g}; eye=({:.9g},{:.9g},{:.9g}), target=({:.9g},{:.9g},{:.9g})",
                    bits,drawTarget,range[0],range[1],clear,camera.nearPlane,camera.farPlane,
                    camera.eye.x,camera.eye.y,camera.eye.z,camera.target.x,camera.target.y,camera.target.z);
                ++depthEvidence.reports;
                depthEvidence.nearPlane=camera.nearPlane;depthEvidence.farPlane=camera.farPlane;
                depthEvidence.nextNanoseconds=evidenceNow+(depthEvidence.reports<8u?2'000'000'000ll:5'000'000'000ll);
                }
            }

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK); glFrontFace(GL_CW);
            glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
            glDepthFunc(GL_LEQUAL);
            glUseProgram(state.sceneShader);
            glUniformMatrix4fv(state.sceneMvpLoc, 1, GL_FALSE, mvp.data());

            if (!matchedScene)
            {
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

            }

            const auto solidGeometry = matchedScene ? matchedScene->solids->geometry
                : epochengine::previewgrid::object_solid_geometry_for(ctx);

            const auto& solidVertices = *solidGeometry;
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
                state.meshTrianglesSubmitted+=solidVertices.size()/3u;
                ++state.meshDrawCallsSubmitted;
                state.meshUploadBytesSubmitted+=solidVertices.size()*sizeof(solidVertices[0]);
            }

            (void)render_mesh_base_color_preview(ctx,state,mvp,matchedScene);
            render_engine_arcade_sampled_surface_preview(
                ctx,
                state,
                mvp,
                framebufferWidth,
                framebufferHeight,
                viewportX,
                viewportY,
                viewportWidth,
                viewportHeight,
                matchedScene);

            if(matchedScene)
                (void)openglcloud::render(state.cloudVolume,*matchedScene,viewportX,glViewportY,
                    viewportWidth,viewportHeight);

            const auto meshWire = matchedScene ? matchedScene->wire
                : epochengine::previewgrid::object_mesh_wire_frame_for(ctx);
            render_mesh_wire_preview(*meshWire,state,mvp);

            std::vector<epochengine::previewgrid::Vertex> dynamicVertices{};
            if (!matchedScene)
            {
                const auto focusVertices = epochengine::previewgrid::look_marker_vertices_for(ctx);
                const auto focusCount = epochengine::previewgrid::look_marker_vertex_count_for(ctx);
                if (focusCount > 0)
                    dynamicVertices.insert(dynamicVertices.end(), focusVertices.begin(), focusVertices.begin() + focusCount);
            }
            const auto objectGeometry = meshWire->helper_lines ? meshWire->helper_lines
                : matchedScene ? matchedScene->lines
                    : epochengine::previewgrid::object_marker_geometry_for(ctx);
            const auto& objectVertices = *objectGeometry;
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
        if (!ctx || viewportWidth <= 0 || viewportHeight <= 0)
            return;

        render_scene_preview_conventional(
            ctx, state, previewMode, framebufferWidth, framebufferHeight,
            viewportX, viewportY, viewportWidth, viewportHeight);
    }

}
#endif
