/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██╗██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondShell - Modular C++ Framework                      *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 *   Use permitted for Non-Commercial Purposes ONLY,          *
 *   without prior commercial licensing agreement.            *
 *                                                            *
 *   Redistribution Allowed with This Notice and              *
 *   LICENSE file. No obligation to disclose modifications.   *
 *                                                            *
 *   See LICENSE file for full terms.                         *
 *                                                            *
 **************************************************************/
 // acontext.opengl.quad.ixx  (Quad + shader pipeline)
 // QUAD_V3: prints GL/GLSL versions + produces single-line shader errors.

module;

#include <include/aengine.config.hpp>

#if defined(ALMOND_USING_OPENGL)
#   include <glad/glad.h>
#endif

export module acontext.opengl.quad;

import acontext.opengl.state;
import acontext.opengl.platform;

import <algorithm>;
import <cstdint>;
import <iostream>;
import <string>;
import <string_view>;
import <utility>;

#if defined(ALMOND_USING_OPENGL)

export namespace almondnamespace::openglquad
{
    // ---------------------------------------------------------------------
    // Quad VAO wrapper (renderer depends on this type existing)
    // ---------------------------------------------------------------------
    export class Quad
    {
        GLuint vao_ = 0;
        GLuint vbo_ = 0;
        GLuint ebo_ = 0;

        static constexpr float defaultVertices[16] = {
            0.f, 0.f, 0.f, 0.f,
            1.f, 0.f, 1.f, 0.f,
            1.f, 1.f, 1.f, 1.f,
            0.f, 1.f, 0.f, 1.f
        };

        static constexpr unsigned int defaultIndices[6] = { 0,1,2, 2,3,0 };

    public:
        Quad()
        {
            glGenVertexArrays(1, &vao_);
            glBindVertexArray(vao_);

            glGenBuffers(1, &vbo_);
            glBindBuffer(GL_ARRAY_BUFFER, vbo_);
            glBufferData(GL_ARRAY_BUFFER, sizeof(defaultVertices), defaultVertices, GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)0);

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)(2 * sizeof(float)));

            glGenBuffers(1, &ebo_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(defaultIndices), defaultIndices, GL_STATIC_DRAW);

            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        ~Quad()
        {
            if (ebo_) glDeleteBuffers(1, &ebo_);
            if (vbo_) glDeleteBuffers(1, &vbo_);
            if (vao_) glDeleteVertexArrays(1, &vao_);
        }

        Quad(const Quad&) = delete;
        Quad& operator=(const Quad&) = delete;

        Quad(Quad&& o) noexcept : vao_(o.vao_), vbo_(o.vbo_), ebo_(o.ebo_) { o.vao_ = o.vbo_ = o.ebo_ = 0; }
        Quad& operator=(Quad&& o) noexcept
        {
            if (this == &o) return *this;
            if (ebo_) glDeleteBuffers(1, &ebo_);
            if (vbo_) glDeleteBuffers(1, &vbo_);
            if (vao_) glDeleteVertexArrays(1, &vao_);
            vao_ = o.vao_; vbo_ = o.vbo_; ebo_ = o.ebo_;
            o.vao_ = o.vbo_ = o.ebo_ = 0;
            return *this;
        }

        [[nodiscard]] GLuint  vao() const noexcept { return vao_; }
        [[nodiscard]] GLsizei index_count() const noexcept { return 6; }
    };

    // ---------------------------------------------------------------------
    // Quad shader+VAO pipeline (per-thread/per-context)
    // ---------------------------------------------------------------------
    struct QuadPipelineState
    {
        GLuint shader = 0;
        GLint  uUVRegionLoc = -1;
        GLint  uTransformLoc = -1;
        GLint  uSamplerLoc = -1;
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
    };

    inline thread_local QuadPipelineState s_tls_pipeline{};

    export QuadPipelineState& quad_pipeline_state() noexcept { return s_tls_pipeline; }

    // ---------------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------------
    inline std::pair<int, int> parse_ver(const char* s) noexcept
    {
        if (!s) return { 0,0 };
        std::string_view v{ s };
        auto p = v.find_first_of("0123456789");
        if (p == std::string_view::npos) return { 0,0 };
        v.remove_prefix(p);
        auto dot = v.find('.');
        if (dot == std::string_view::npos) return { 0,0 };
        auto maj = v.substr(0, dot);
        v.remove_prefix(dot + 1);
        auto end = v.find_first_not_of("0123456789");
        auto min = v.substr(0, end);

        auto to_i = [](std::string_view x) noexcept {
            int r = 0;
            for (unsigned char c : x) { if (c < '0' || c>'9') return 0; r = r * 10 + (c - '0'); }
            return r;
        };
        return { to_i(maj), to_i(min) };
    }

    inline std::string squash(std::string s)
    {
        for (auto& ch : s) if (ch == '\n' || ch == '\r' || ch == '\t') ch = ' ';
        s.erase(std::unique(s.begin(), s.end(), [](char a, char b) { return a == ' ' && b == ' '; }), s.end());
        return s;
    }

    inline std::string get_info_log(GLuint obj, bool shader)
    {
        std::string log;
        log.resize(8192);
        GLsizei out = 0;
        if (shader) glGetShaderInfoLog(obj, (GLsizei)log.size(), &out, log.data());
        else        glGetProgramInfoLog(obj, (GLsizei)log.size(), &out, log.data());
        if (out > 0 && (std::size_t)out < log.size()) log.resize((std::size_t)out);
        return squash(log);
    }

    inline GLuint compile(GLenum type, const std::string& src)
    {
        if (!glCreateShader)
            throw std::runtime_error("QUAD_V3 glCreateShader=null (context too old or glad not loaded)");

        GLuint sh = glCreateShader(type);
        const char* c = src.c_str();
        glShaderSource(sh, 1, &c, nullptr);
        glCompileShader(sh);

        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            const std::string log = get_info_log(sh, true);
            glDeleteShader(sh);

            std::string msg = "QUAD_V3 Shader compile failed. log=[" + log + "] src=[" + squash(src) + "]";
            throw std::runtime_error(msg);
        }
        return sh;
    }

    inline GLuint link(GLuint vs, GLuint fs, bool bindFragOut)
    {
        GLuint p = glCreateProgram();
        glAttachShader(p, vs);
        glAttachShader(p, fs);

        glBindAttribLocation(p, 0, "aPos");
        glBindAttribLocation(p, 1, "aTexCoord");

        if (bindFragOut && glBindFragDataLocation)
            glBindFragDataLocation(p, 0, "outColor");

        glLinkProgram(p);

        GLint ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            const std::string log = get_info_log(p, false);
            glDeleteProgram(p);
            std::string msg = "QUAD_V3 Program link failed. log=[" + log + "]";
            throw std::runtime_error(msg);
        }
        return p;
    }

    inline void destroy_quad_pipeline(QuadPipelineState& s) noexcept
    {
        if (s.shader && glIsProgram(s.shader)) glDeleteProgram(s.shader);
        s.shader = 0;
        s.uUVRegionLoc = -1;
        s.uTransformLoc = -1;
        s.uSamplerLoc = -1;

        if (s.vao && glIsVertexArray(s.vao)) glDeleteVertexArrays(1, &s.vao);
        if (s.vbo && glIsBuffer(s.vbo)) glDeleteBuffers(1, &s.vbo);
        if (s.ebo && glIsBuffer(s.ebo)) glDeleteBuffers(1, &s.ebo);
        s.vao = s.vbo = s.ebo = 0;
    }

    inline bool build_quad_pipeline(QuadPipelineState& s)
    {
        destroy_quad_pipeline(s);

        // Print versions in one line (so your logger doesn't eat it).
        const char* glv = (const char*)glGetString(GL_VERSION);
        const char* glsl = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
        const char* ven = (const char*)glGetString(GL_VENDOR);
        const char* ren = (const char*)glGetString(GL_RENDERER);
        std::cerr << "QUAD_V3 GL_VERSION=[" << (glv ? glv : "null")
            << "] GLSL=[" << (glsl ? glsl : "null")
            << "] VENDOR=[" << (ven ? ven : "null")
            << "] RENDERER=[" << (ren ? ren : "null") << "]\n";

        GLint maj = 0, min = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &maj);
        glGetIntegerv(GL_MINOR_VERSION, &min);
        if (maj == 0 && min == 0)
        {
            auto [pm, pn] = parse_ver(glv);
            maj = pm; min = pn;
        }
        if (maj == 0 && min == 0) { maj = 2; min = 1; }

        const bool gl30plus = (maj > 3) || (maj == 3 && min >= 0);
        const bool gl33plus = (maj > 3) || (maj == 3 && min >= 3);

        std::string vs, fs;

        if (!gl30plus)
        {
            vs = R"(#version 120
attribute vec2 aPos;
attribute vec2 aTexCoord;
uniform vec4 uTransform;
uniform vec4 uUVRegion;
varying vec2 vUV;
void main() {
    vec2 pos = aPos * uTransform.zw + uTransform.xy;
    gl_Position = vec4(pos, 0.0, 1.0);
    vUV = uUVRegion.xy + aTexCoord * uUVRegion.zw;
})";
            fs = R"(#version 120
varying vec2 vUV;
uniform sampler2D uTexture;
void main() {
    gl_FragColor = texture2D(uTexture, vUV);
})";
        }
        else if (gl33plus)
        {
            vs = R"(#version 330 core
in vec2 aPos;
in vec2 aTexCoord;
uniform vec4 uTransform;
uniform vec4 uUVRegion;
out vec2 vUV;
void main() {
    vec2 pos = aPos * uTransform.zw + uTransform.xy;
    gl_Position = vec4(pos, 0.0, 1.0);
    vUV = uUVRegion.xy + aTexCoord * uUVRegion.zw;
})";
            fs = R"(#version 330 core
in vec2 vUV;
out vec4 outColor;
uniform sampler2D uTexture;
void main() {
    outColor = texture(uTexture, vUV);
})";
        }
        else
        {
            vs = R"(#version 130
in vec2 aPos;
in vec2 aTexCoord;
uniform vec4 uTransform;
uniform vec4 uUVRegion;
out vec2 vUV;
void main() {
    vec2 pos = aPos * uTransform.zw + uTransform.xy;
    gl_Position = vec4(pos, 0.0, 1.0);
    vUV = uUVRegion.xy + aTexCoord * uUVRegion.zw;
})";
            fs = R"(#version 130
in vec2 vUV;
out vec4 outColor;
uniform sampler2D uTexture;
void main() {
    outColor = texture(uTexture, vUV);
})";
        }

        try
        {
            GLuint vsh = compile(GL_VERTEX_SHADER, vs);
            GLuint fsh = compile(GL_FRAGMENT_SHADER, fs);
            s.shader = link(vsh, fsh, gl30plus);
            glDeleteShader(vsh);
            glDeleteShader(fsh);
        }
        catch (const std::exception& e)
        {
            std::cerr << "[OpenGL] Pipeline build failed: " << e.what() << "\n";
            destroy_quad_pipeline(s);
            return false;
        }

        s.uUVRegionLoc = glGetUniformLocation(s.shader, "uUVRegion");
        s.uTransformLoc = glGetUniformLocation(s.shader, "uTransform");
        s.uSamplerLoc = glGetUniformLocation(s.shader, "uTexture");

        if (s.uSamplerLoc >= 0)
        {
            glUseProgram(s.shader);
            glUniform1i(s.uSamplerLoc, 0);
            glUseProgram(0);
        }

        glGenVertexArrays(1, &s.vao);
        glGenBuffers(1, &s.vbo);
        glGenBuffers(1, &s.ebo);

        glBindVertexArray(s.vao);
        glBindBuffer(GL_ARRAY_BUFFER, s.vbo);

        constexpr float quadVerts[] = {
            -0.5f, -0.5f, 0.0f, 0.0f,
             0.5f, -0.5f, 1.0f, 0.0f,
             0.5f,  0.5f, 1.0f, 1.0f,
            -0.5f,  0.5f, 0.0f, 1.0f
        };
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)(2 * sizeof(float)));

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s.ebo);
        constexpr unsigned int quadIdx[] = { 0,1,2, 2,3,0 };
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)sizeof(quadIdx), quadIdx, GL_STATIC_DRAW);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        return true;
    }

    export bool ensure_quad_pipeline()
    {
        auto& s = s_tls_pipeline;

        // If no GL context is current on this thread, glGetString returns null.
        // Do not try to rebuild in that situation.
        if (::glGetString(GL_VERSION) == nullptr)
            return (s.shader != 0) && (s.vao != 0) && (s.vbo != 0) && (s.ebo != 0);

        const bool shaderOk = s.shader != 0 && glIsProgram(s.shader) == GL_TRUE;
        const bool vaoOk    = s.vao    != 0 && glIsVertexArray(s.vao) == GL_TRUE;
        const bool vboOk    = s.vbo    != 0 && glIsBuffer(s.vbo) == GL_TRUE;
        const bool eboOk    = s.ebo    != 0 && glIsBuffer(s.ebo) == GL_TRUE;

        if (shaderOk && vaoOk && vboOk && eboOk)
            return true;

        return build_quad_pipeline(s);
    }

    // Back-compat wrapper: existing code may still call ensure_quad_pipeline(state).
    // We deliberately ignore the passed-in state to avoid cross-context handle stomping.
    export bool ensure_quad_pipeline(almondnamespace::openglstate::OpenGL4State& /*unused*/)
    {
        return ensure_quad_pipeline();
    }
} // namespace almondnamespace::openglquad


#endif // ALMOND_USING_OPENGL
