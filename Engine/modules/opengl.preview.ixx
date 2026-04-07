module;

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <include/aengine.config.hpp>

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
#   include <glad/glad.h>
#endif

export module opengl.preview;

import core.context;
import opengl.state;
import render.preview_grid;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
export namespace epochnamespace::openglpreview
{
    namespace detail
    {
        using Mat4 = epochnamespace::previewgrid::Mat4;

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
    }

    inline void destroy_scene_preview_pipeline(epochnamespace::openglstate::OpenGL4State& state) noexcept
    {
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

    inline bool ensure_scene_preview_pipeline(epochnamespace::openglstate::OpenGL4State& state)
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

        const auto vertices = epochnamespace::previewgrid::grid_vertices();
        const auto indices = epochnamespace::previewgrid::grid_indices();

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
            static_cast<GLsizei>(sizeof(epochnamespace::previewgrid::Vertex)),
            reinterpret_cast<void*>(offsetof(epochnamespace::previewgrid::Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(epochnamespace::previewgrid::Vertex)),
            reinterpret_cast<void*>(offsetof(epochnamespace::previewgrid::Vertex, color)));
        glBindVertexArray(0);

        glGenVertexArrays(1, &state.sceneMarkerVao);
        glGenBuffers(1, &state.sceneMarkerVbo);
        glBindVertexArray(state.sceneMarkerVao);
        glBindBuffer(GL_ARRAY_BUFFER, state.sceneMarkerVbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(sizeof(epochnamespace::previewgrid::Vertex) * 8u),
            nullptr,
            GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(epochnamespace::previewgrid::Vertex)),
            reinterpret_cast<void*>(offsetof(epochnamespace::previewgrid::Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(epochnamespace::previewgrid::Vertex)),
            reinterpret_cast<void*>(offsetof(epochnamespace::previewgrid::Vertex, color)));
        glBindVertexArray(0);

        state.sceneMvpLoc = glGetUniformLocation(state.sceneShader, "uMvp");
        return state.sceneShader != 0 && state.sceneMvpLoc >= 0;
    }

    inline void render_scene_preview(
        const core::Context* ctx,
        epochnamespace::openglstate::OpenGL4State& state,
        core::ScenePreviewMode previewMode,
        int framebufferWidth,
        int framebufferHeight,
        int viewportX,
        int viewportY,
        int viewportWidth,
        int viewportHeight)
    {
        const int glViewportY = (std::max)(0, framebufferHeight - (viewportY + viewportHeight));

        glEnable(GL_SCISSOR_TEST);
        glScissor(viewportX, glViewportY, viewportWidth, viewportHeight);
        glViewport(viewportX, glViewportY, viewportWidth, viewportHeight);
        const auto clearColor = epochnamespace::previewgrid::kClearColor;
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (previewMode == core::ScenePreviewMode::Editor && ensure_scene_preview_pipeline(state))
        {
            const auto camera = epochnamespace::previewgrid::camera_for(ctx);
            const float aspect = viewportHeight > 0
                ? (viewportWidth / static_cast<float>(viewportHeight))
                : 1.0f;
            const detail::Mat4 proj = epochnamespace::previewgrid::perspective(
                camera.fovRadians,
                aspect,
                camera.nearPlane,
                camera.farPlane);
            const detail::Mat4 view = epochnamespace::previewgrid::look_at(
                camera.eye,
                camera.target,
                camera.up);
            const detail::Mat4 mvp = epochnamespace::previewgrid::multiply(proj, view);

            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glUseProgram(state.sceneShader);
            glUniformMatrix4fv(state.sceneMvpLoc, 1, GL_FALSE, mvp.data());
            glBindVertexArray(state.sceneVao);
            glDrawElements(
                GL_LINES,
                static_cast<GLsizei>(epochnamespace::previewgrid::grid_indices().size()),
                GL_UNSIGNED_INT,
                nullptr);

            const auto markerVertices = epochnamespace::previewgrid::look_marker_vertices_for(ctx);
            const auto markerCount = epochnamespace::previewgrid::look_marker_vertex_count_for(ctx);
            if (markerCount >= 2 && state.sceneMarkerVao && state.sceneMarkerVbo)
            {
                glBindVertexArray(state.sceneMarkerVao);
                glBindBuffer(GL_ARRAY_BUFFER, state.sceneMarkerVbo);
                glBufferData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(markerCount * sizeof(markerVertices[0])),
                    markerVertices.data(),
                    GL_DYNAMIC_DRAW);
                glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(markerCount));
            }
            glBindVertexArray(0);
            glUseProgram(0);
        }

        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glViewport(0, 0, framebufferWidth, framebufferHeight);
    }
}
#endif
