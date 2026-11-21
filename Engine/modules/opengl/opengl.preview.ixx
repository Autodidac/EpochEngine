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
#include <atomic>
#include <cmath>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include <optional>
#include <source_location>

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
#   include <glad/glad.h>
#   include <src/render/render.atmosphere_shader.hpp>
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
import core.env;
import package.registry;
import render.arcade;
import render.lighting;
import render.preview_atmosphere;
import render.preview_grid;
import render.water_frame_evidence;

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
        if (state.scenePaintTable) glDeleteTextures(1, &state.scenePaintTable);

        state.sceneShader = 0;
        state.sceneMvpLoc = -1;
        state.sceneWireLoc = -1;
        state.scenePaintTableLoc = -1;
        state.scenePaintRowsLoc = -1;
        state.sceneSprayLoc = -1;
        state.sceneAtmosphereLocs.fill(-1);
        state.sceneWaterLocs.fill(-1);
        state.scenePaintTable = 0;
        state.scenePaintTexels.clear();
        state.sceneVao = 0;
        state.sceneVbo = 0;
        state.sceneEbo = 0;
        state.sceneGridSignature = 0;
        state.sceneGridBufferHoldsSky = false;
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
uniform bool uAtmospherePass;
uniform bool uSprayPass;
out vec3 vColor;
out vec3 vDrawPosition;
noperspective out vec3 vWireBarycentric;
void main() {
    vec2 screen = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
    gl_Position = uAtmospherePass ? vec4(screen, 0.0, 1.0) : uMvp * vec4(aPos, 1.0);
    if (uSprayPass) gl_PointSize = aColor.x;
    vColor = aColor;
    vDrawPosition = uAtmospherePass ? vec3(screen,0.0) : aPos;
    int corner = gl_VertexID % 3;
    vWireBarycentric = vec3(corner == 0, corner == 1, corner == 2);
})";
            fragmentSource = R"(#version 330 core
in vec3 vColor;
in vec3 vDrawPosition;
noperspective in vec3 vWireBarycentric;
uniform bool uWireEdges;
uniform sampler2D uPaintTable;
uniform int uPaintRows;
uniform bool uAtmospherePass;
uniform bool uSprayPass;
uniform float uWaterScale;
uniform vec3 uWaterCenter,uWaterEye;
uniform vec4 uWaterSun,uWaterWaves[15],uWaterSlopes[3];
out vec4 outColor;
vec3 waterReflection(vec3 up, vec3 normal, vec3 eye, vec4 sun) {
    float facing = clamp(dot(eye, normal), 0.0, 1.0);
    float fresnel = 0.0204 + 0.9796 * pow(1.0 - facing, 5.0);
    float skyElevation = clamp(dot(reflect(-eye, normal), up), 0.0, 1.0);
    float sunElevation = dot(up, sun.xyz);
    float daylight = smoothstep(-0.09, 0.22, sunElevation) * sun.w;
    float twilight = (1.0 - smoothstep(0.02, 0.24, sunElevation))
                   * smoothstep(-0.10, 0.03, sunElevation) * sun.w;
    vec3 sky = mix(vec3(0.13, 0.19, 0.24), vec3(0.045, 0.105, 0.20), skyElevation);
    sky = mix(sky, vec3(0.18, 0.105, 0.055), twilight * (1.0 - skyElevation));
    vec3 halfSum = eye + sun.xyz;
    vec3 halfway = halfSum * inversesqrt(max(dot(halfSum, halfSum), 1e-6));
    float alignment = clamp(dot(normal, halfway), 0.0, 1.0);
    float specular = 0.045 * pow(alignment, 22.0) + 0.23 * pow(alignment, 112.0);
    float visibleSun = smoothstep(0.0, 0.06, sunElevation) * sun.w;
    float masking = smoothstep(0.0, 0.10, facing) * smoothstep(0.0, 0.10, dot(normal, sun.xyz));
    vec3 sunTint = mix(vec3(0.99, 0.61, 0.29), vec3(0.87, 0.94, 1.0),
                       smoothstep(0.04, 0.28, sunElevation));
    return sky * (0.25 + 0.75 * daylight) * fresnel + sunTint * specular * masking * visibleSun;
}
// Sharpen the front of the long swells. Neighboring phases give a crest
// both broad gaps and broken edges, and every foam term follows its phase.
float waterRolls(vec3 phase, vec3 phaseSine, vec3 phaseCosine, vec3 dx, vec3 dy,
                 vec3 waveX, vec3 waveY, vec3 waveZ, float strength, inout vec3 slope) {
    vec3 waves[3] = vec3[3](waveX, waveY, waveZ);
    const vec3 steepness = vec3(0.080, 0.115, 0.165);
    float foam = 0.0;
    for (int i = 0; i < 3; ++i) {
        int transverse = (i + 1) % 3;
        int crossing = (i + 2) % 3;
        vec3 wave = waves[i];
        float footprint = abs(dot(wave, dx)) + abs(dot(wave, dy));
        float resolved = 1.0 - smoothstep(0.65, 2.45, footprint);
        float s = phaseSine[i], c = phaseCosine[i];
        float group = 0.60 * phaseCosine[transverse] + 0.40 * phaseSine[crossing];
        float envelope = 0.30 + 0.70 * smoothstep(-0.60, 0.65, group);
        float front = 0.80 * c + 0.52 * s * c + 0.22 * (1.0 - 2.0 * s * s);
        slope += normalize(wave) * (front * steepness[i] * envelope * resolved * strength);
        // Whitecap fragments trail the sharp front and vanish before the
        // fine pattern becomes smaller than a pixel.
        float height = 0.80 * s + 0.26 * s * s + 0.22 * s * c;
        if (height <= 0.91 || group <= 0.45 || footprint >= 0.70) continue;
        float whiten = (1.0 - smoothstep(0.20, 0.70, footprint)) * smoothstep(0.45, 0.84, group);
        float lip = smoothstep(0.91, 1.06, height);
        float edge = abs(dot(waves[transverse] - 2.0 * waves[crossing], dx))
                   + abs(dot(waves[transverse] - 2.0 * waves[crossing], dy));
        float breakup = (1.0 - smoothstep(0.28, 0.90, edge))
                     * smoothstep(0.25, 0.85, sin(phase[transverse] - 2.0 * phase[crossing]));
        foam = max(foam, whiten * lip * mix(0.22, 1.0, breakup));
    }
    return foam * strength;
}

vec3 waterDetail(vec3 base, vec3 dx, vec3 dy) {
    vec3 p=vDrawPosition*uWaterScale;
    vec3 toEye=(uWaterEye-vDrawPosition)*uWaterScale;
    float distanceToEye=length(toEye);
    float swellStrength=1.0-smoothstep(75.0,210.0,distanceToEye);
    if(distanceToEye<1e-4||swellStrength<=0.0) return base;
    vec3 directionX=uWaterWaves[12].xyz,directionY=uWaterWaves[13].xyz,directionZ=uWaterWaves[14].xyz;
    vec3 longPhase=vec3(dot(directionX,p)+uWaterWaves[12].w,
                       dot(directionY,p)+uWaterWaves[13].w,
                       dot(directionZ,p)+uWaterWaves[14].w);
    vec3 up=normalize(p-uWaterCenter),eye=toEye/distanceToEye;
    vec3 warpRate=cos(longPhase);
    vec3 drift=sin(longPhase);
    vec3 slope=vec3(0.0);
    float foam=waterRolls(longPhase,drift,warpRate,dx,dy,directionX,directionY,directionZ,
                         swellStrength,slope);
    float rippleStrength=1.0-smoothstep(40.0,135.0,distanceToEye);
    if(rippleStrength>0.0) {
        const vec3 warpX=vec3(0.27,0.13,-0.21),warpY=vec3(0.11,-0.23,-0.19),warpZ=vec3(0.15,0.09,0.18);
        vec3 warped=p+warpX*drift.x+warpY*drift.y+warpZ*drift.z;
        dx+=warpX*(dot(directionX,dx)*warpRate.x)
            +warpY*(dot(directionY,dx)*warpRate.y)+warpZ*(dot(directionZ,dx)*warpRate.z);
        dy+=warpX*(dot(directionX,dy)*warpRate.x)
            +warpY*(dot(directionY,dy)*warpRate.y)+warpZ*(dot(directionZ,dy)*warpRate.z);
        for(int i=0;i<12;++i) {
            vec4 wave=uWaterWaves[i];
            // Filter angular phase per pixel before adding sub-mesh wave slopes.
            float footprint=abs(dot(wave.xyz,dx))+abs(dot(wave.xyz,dy));
            if(footprint>=2.45) continue;
            float resolved=1.0-smoothstep(0.65,2.45,footprint);
            vec3 gradient=wave.xyz+directionX*dot(wave.xyz,warpX)*warpRate.x
                +directionY*dot(wave.xyz,warpY)*warpRate.y+directionZ*dot(wave.xyz,warpZ)*warpRate.z;
            int band=i/4;
            float crestEnvelope=0.44+0.56*smoothstep(-0.65,0.7,
                cos(longPhase[band]+0.7*drift[(band+1)%3]+1.6*float(i)));
            float crest=uWaterSlopes[band][i%4];
            slope+=gradient/length(wave.xyz)*(cos(dot(wave.xyz,warped)+wave.w)*crest*resolved*crestEnvelope*rippleStrength);
        }
    }
    if(dot(slope,slope)==0.0&&foam==0.0) return base;
    vec3 normal=normalize(up-(slope-up*dot(up,slope)));
    // The opaque base already includes the broad reflection.
    vec3 delta=waterReflection(up,normal,eye,uWaterSun)-waterReflection(up,up,eye,uWaterSun);
    float daylight=smoothstep(-0.10,0.38,dot(up,uWaterSun.xyz))*uWaterSun.w;
    vec3 foamColor=mix(vec3(0.15,0.21,0.24),vec3(0.64,0.74,0.75),daylight);
    return mix(clamp(base+delta,0.0,1.0),foamColor,foam*0.40);
}
vec3 forestCoverage(vec3 base) {
    if (uPaintRows <= 0) return base;
    // Derivatives are taken before coverage branches or first-match exits.
    vec3 drawDx = dFdx(vDrawPosition), drawDy = dFdy(vDrawPosition);
    for (int row = 0; row < min(uPaintRows, 65); ++row) {
        vec4 anchor = texelFetch(uPaintTable, ivec2(0,row), 0);
        int count = int(anchor.w);
        if (count < 3 || count > 128) continue;
        vec3 horizontal = texelFetch(uPaintTable, ivec2(1,row), 0).xyz;
        vec3 vertical = texelFetch(uPaintTable, ivec2(2,row), 0).xyz;
        vec3 depth = texelFetch(uPaintTable, ivec2(3,row), 0).xyz;
        vec3 delta = vDrawPosition - anchor.xyz;
        vec2 numerator = vec2(dot(horizontal,delta),dot(vertical,delta));
        float denominator = 1.0 + dot(depth,delta);
        float radiusSquared = dot(numerator,numerator) + denominator*denominator;
        if (denominator <= 0.0 || radiusSquared < 0.25 || radiusSquared > 2.25) continue;
        vec2 point = numerator / denominator;
        vec2 chartDx = (vec2(dot(horizontal,drawDx),dot(vertical,drawDx))
            - point*dot(depth,drawDx)) / denominator;
        vec2 chartDy = (vec2(dot(horizontal,drawDy),dot(vertical,drawDy))
            - point*dot(depth,drawDy)) / denominator;
        float pixel = max(max(length(chartDx),length(chartDy)),1e-8);
        vec4 bounds = texelFetch(uPaintTable, ivec2(4,row), 0);
        if (any(lessThan(point,bounds.xy-2.0*pixel)) || any(greaterThan(point,bounds.zw+2.0*pixel))) continue;
        bool inside = false;
        float distanceToEdge = 1e30;
        vec2 a = texelFetch(uPaintTable, ivec2(7+count-1,row), 0).xy;
        for (int edge = 0; edge < count; ++edge) {
            vec2 b = texelFetch(uPaintTable, ivec2(7+edge,row), 0).xy;
            vec2 segment = b-a;
            float t = clamp(dot(point-a,segment)/max(dot(segment,segment),1e-20),0.0,1.0);
            distanceToEdge = min(distanceToEdge,length(point-(a+t*segment)));
            if ((a.y > point.y) != (b.y > point.y)) {
                float crossing = a.x + (point.y-a.y)*(b.x-a.x)/(b.y-a.y);
                if (point.x < crossing) inside = !inside;
            }
            a = b;
        }
        float edgeAlpha = 1.0-smoothstep(0.75*pixel,1.75*pixel,distanceToEdge);
        if (!inside && edgeAlpha <= 0.0) continue;
        vec4 fill = texelFetch(uPaintTable, ivec2(5,row), 0);
        vec4 outline = texelFetch(uPaintTable, ivec2(6,row), 0);
        vec3 filled = mix(base,fill.rgb,inside ? clamp(fill.a,0.0,1.0) : 0.0);
        return mix(filled,outline.rgb,clamp(outline.a,0.0,1.0)*edgeAlpha);
    }
    return base;
}
void main() {
    if (uSprayPass) {
        vec2 offset = (gl_PointCoord - vec2(0.5)) * vec2(2.25, 2.0);
        float shape = 1.0 - smoothstep(0.06, 1.0, dot(offset, offset));
        outColor = vec4(mix(vec3(0.36,0.48,0.54), vec3(0.88,0.95,0.98), vColor.z), vColor.y*shape);
        return;
    }
    if (uAtmospherePass) {
        outColor=skyAtmosphere(vDrawPosition.xy);
        if(outColor.a<=0.0)discard;
        return;
    }
    outColor = vec4(forestCoverage(vColor), 1.0);
    if (uWaterScale>0.0) {
        vec3 dx=dFdx(vDrawPosition)*uWaterScale,dy=dFdy(vDrawPosition)*uWaterScale;
        outColor.rgb=waterDetail(outColor.rgb,dx,dy);
    }
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
            fragmentSource.insert(fragmentSource.find('\n') + 1, renderatmosphereshader::fragment_shared);
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
        state.scenePaintTableLoc = glGetUniformLocation(state.sceneShader, "uPaintTable");
        state.scenePaintRowsLoc = glGetUniformLocation(state.sceneShader, "uPaintRows");
        state.sceneSprayLoc = glGetUniformLocation(state.sceneShader, "uSprayPass");
        constexpr std::array skyNames{"uAtmospherePass","uSkyEye","uSkyForward","uSkyRight",
            "uSkyUp","uSkySun","uSkyThickness","uSkySunVisible"};
        for(std::size_t index=0;index<skyNames.size();++index)
            state.sceneAtmosphereLocs[index]=glGetUniformLocation(state.sceneShader,skyNames[index]);
        constexpr std::array waterNames{"uWaterScale","uWaterCenter","uWaterEye","uWaterSun","uWaterWaves[0]","uWaterSlopes[0]"};
        for(std::size_t index=0;index<waterNames.size();++index)
            state.sceneWaterLocs[index]=glGetUniformLocation(state.sceneShader,waterNames[index]);
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
            if(mesh) fragment+=varyingIn+"vec3 vColor;\nuniform int uDecodeSrgb;\nuniform int uEncodeSrgb;\nuniform int uAlphaCutout;\nuniform float uAlphaCutoff;\nuniform int uNeutralSurface;\n"
                "vec3 decodeSrgb(vec3 c){return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(0.04045),c));}\n"
                "vec3 encodeSrgb(vec3 c){return mix(c*12.92,1.055*pow(max(c,vec3(0.0)),vec3(1.0/2.4))-0.055,step(vec3(0.0031308),c));}\n";
            fragment+="void main(){vec4 texel="+std::string(modern?"texture":"texture2D")+"(uTexture,vUv);vec3 color=texel.rgb;";
            if(mesh) fragment+="if(uAlphaCutout!=0&&texel.a<uAlphaCutoff)discard;if(uNeutralSurface!=0)color=vec3(1.0);else if(uDecodeSrgb!=0)color=decodeSrgb(color);color*=vColor;if(uEncodeSrgb!=0)color=encodeSrgb(color);";
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
                resources.neutral_surface=glGetUniformLocation(resources.program,"uNeutralSurface");
                glGenVertexArrays(1,&resources.vao);glGenBuffers(1,&resources.vbo);
                glBindVertexArray(resources.vao);glBindBuffer(GL_ARRAY_BUFFER,resources.vbo);
                using Vertex=epochengine::previewgrid::TexturedVertex;
                glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,position)));
                glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,uv)));
                glEnableVertexAttribArray(2);glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,color)));
            }
            if(!resources.program||!resources.vao||!resources.vbo||resources.mvp<0||resources.texture<0
                ||resources.decode_srgb<0||resources.encode_srgb<0
                ||resources.alpha_cutout<0||resources.alpha_cutoff<0||resources.neutral_surface<0) return fail("Textured mesh shader or native geometry allocation unavailable.");
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
                glUniform1i(resources.neutral_surface,range.source->marker.neutralSurface
                    && !range.source->marker.unlit && !range.source->marker.editorOnly ? 1 : 0);
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

    // Scoped to the isolated terrain's existing opaque draw. No second depth pass,
    // and no coverage state survives into textures, wireframe, helpers or GUI.
    struct ScopedForestCoverage final
    {
        epochengine::openglstate::OpenGL4State& state;
        GLint activeTexture{}, sampler{}, swapBytes{};
        std::optional<epochengine::opengltextures::ScopedNativeTextureState> textureState{};

        ScopedForestCoverage(epochengine::openglstate::OpenGL4State& owner,
            const epochengine::previewgrid::SolidDrawFrame& frame) : state{owner}
        {
            if (state.scenePaintRowsLoc < 0 || state.scenePaintTableLoc < 0) return;
            glUniform1i(state.scenePaintRowsLoc, 0);
            // The aggregate is safe only when every triangle belongs to this terrain.
            if (frame.markers.size() != 1u || !frame.geometry || frame.geometry->empty()) return;
            const auto& marker = frame.markers.front();
            if ((marker.textured && !marker.textured->empty()) || !marker.marker.paintView
                || marker.marker.paintView->lassos.empty()) return;
            auto texels = epochengine::previewgrid::pack_mesh_paint_coverage(marker.marker);
            constexpr auto width = epochengine::previewgrid::kMeshPaintCoverageRowTexels;
            if (texels.empty() || texels.size()%width != 0u
                || texels.size()/width > epochengine::previewgrid::kMaximumMeshPaintCoverageRows) return;
            glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
            glActiveTexture(GL_TEXTURE0);
            glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
            glGetIntegerv(GL_UNPACK_SWAP_BYTES, &swapBytes);
            textureState.emplace();
            glBindSampler(0, 0);
            if (!state.scenePaintTable) glGenTextures(1, &state.scenePaintTable);
            glBindTexture(GL_TEXTURE_2D, state.scenePaintTable);
            if (!state.scenePaintTable) return;
            if (state.scenePaintTexels != texels)
            {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
                epochengine::opengltextures::clear_gl_errors();
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, static_cast<GLsizei>(width),
                    static_cast<GLsizei>(texels.size()/width), 0, GL_RGBA, GL_FLOAT, texels.data());
                if (glGetError() != GL_NO_ERROR) return;
                state.scenePaintTexels = std::move(texels);
            }
            glUniform1i(state.scenePaintTableLoc, 0);
            glUniform1i(state.scenePaintRowsLoc, static_cast<GLint>(state.scenePaintTexels.size()/width));
        }

        ~ScopedForestCoverage() noexcept
        {
            if (state.scenePaintRowsLoc >= 0) glUniform1i(state.scenePaintRowsLoc, 0);
            if (textureState)
            {
                glActiveTexture(GL_TEXTURE0);
                textureState.reset();
                glPixelStorei(GL_UNPACK_SWAP_BYTES, swapBytes);
                glBindSampler(0, static_cast<GLuint>(sampler));
                glActiveTexture(static_cast<GLenum>(activeTexture));
            }
        }
        ScopedForestCoverage(const ScopedForestCoverage&) = delete;
        ScopedForestCoverage& operator=(const ScopedForestCoverage&) = delete;
    };

    inline std::size_t render_shoreline_spray(const previewgrid::PublishedSceneFrame& scene,
        openglstate::OpenGL4State& state,const detail::Mat4& mvp,int viewport_height)
    {
        if(state.sceneSprayLoc<0||!state.sceneMarkerVao||!state.sceneMarkerVbo)return 0u;
        const auto batch=previewgrid::shoreline_spray_for(scene,viewport_height,
            previewgrid::ShorelineSprayBatch::desktop_portable_budget);
        if(batch.size==0u)return 0u;
        const bool blend=glIsEnabled(GL_BLEND)==GL_TRUE;
        const bool depth=glIsEnabled(GL_DEPTH_TEST)==GL_TRUE;
        const bool point_size=glIsEnabled(GL_PROGRAM_POINT_SIZE)==GL_TRUE;
        GLboolean depth_write{};
        GLint depth_func{},src_rgb{},dst_rgb{},src_alpha{},dst_alpha{},equation_rgb{},equation_alpha{};
        glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_write);
        glGetIntegerv(GL_DEPTH_FUNC,&depth_func);
        glGetIntegerv(GL_BLEND_SRC_RGB,&src_rgb);
        glGetIntegerv(GL_BLEND_DST_RGB,&dst_rgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA,&src_alpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA,&dst_alpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB,&equation_rgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&equation_alpha);
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_PROGRAM_POINT_SIZE);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE);
        glUseProgram(state.sceneShader);
        glUniformMatrix4fv(state.sceneMvpLoc,1,GL_FALSE,mvp.data());
        glUniform1i(state.sceneSprayLoc,GL_TRUE);
        glBindVertexArray(state.sceneMarkerVao);
        glBindBuffer(GL_ARRAY_BUFFER,state.sceneMarkerVbo);
        glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(batch.size*sizeof(batch.points[0])),
            batch.points.data(),GL_DYNAMIC_DRAW);
        glDrawArrays(GL_POINTS,0,static_cast<GLsizei>(batch.size));
        glUniform1i(state.sceneSprayLoc,GL_FALSE);
        glBlendFuncSeparate(src_rgb,dst_rgb,src_alpha,dst_alpha);
        glBlendEquationSeparate(equation_rgb,equation_alpha);
        glDepthMask(depth_write);
        glDepthFunc(static_cast<GLenum>(depth_func));
        if(!blend)glDisable(GL_BLEND);
        if(!depth)glDisable(GL_DEPTH_TEST);
        if(!point_size)glDisable(GL_PROGRAM_POINT_SIZE);
        ++state.meshDrawCallsSubmitted;
        state.meshUploadBytesSubmitted+=batch.size*sizeof(batch.points[0]);
        return batch.size;
    }

    inline void render_mesh_wire_preview(const epochengine::previewgrid::MeshWireDrawFrame& frame,
        epochengine::openglstate::OpenGL4State& state, const detail::Mat4& mvp)
    {
        if(!frame.depth_triangles||frame.depth_triangles->empty()
            ||!state.sceneMarkerVao||!state.sceneMarkerVbo) return;
        glUseProgram(state.sceneShader);
        glUniformMatrix4fv(state.sceneMvpLoc,1,GL_FALSE,mvp.data());
        if (state.scenePaintRowsLoc >= 0) glUniform1i(state.scenePaintRowsLoc, 0);
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

    namespace detail
    {
        // A radius ratio is independent of authored scale and render-origin rebasing.
        [[nodiscard]] inline double configured_preview_probe_radius() noexcept
        {
            static const double radius=[]() noexcept {
                try
                {
                    const auto value=core::env::get("EPOCH_OPENGL_PREVIEW_SAMPLE_RADIUS");
                    if(!value)return 0.0;
                    double parsed{};
                    const auto end=value->data()+value->size();
                    const auto result=std::from_chars(value->data(),end,parsed);
                    const bool valid=result.ec==std::errc{}&&result.ptr==end&&parsed>=0.5&&parsed<=100.0;
                    auto& log=logger::get("OpenGL.PreviewPass");
                    if(valid)
                        log.logf(logger::LogLevel::INFO,std::source_location::current(),
                            "Preview probe configured for distance/radius {:.9g}.",parsed);
                    else
                        log.logf(logger::LogLevel::INFO,std::source_location::current(),
                            "Preview probe disabled: EPOCH_OPENGL_PREVIEW_SAMPLE_RADIUS requires a ratio from 0.5 to 100.");
                    return valid?parsed:0.0;
                }
                catch(...){}
                return 0.0;
            }();
            return radius;
        }

        using PreviewProbeViewport=std::array<int,4>;

        [[nodiscard]] inline std::chrono::milliseconds configured_preview_probe_dwell()
        {
            static const auto dwell=[] {
                const auto value=core::env::get("EPOCH_OPENGL_PREVIEW_SAMPLE_DWELL_MS");
                if(value)
                {
                    unsigned parsed{};
                    const auto end=value->data()+value->size();
                    const auto result=std::from_chars(value->data(),end,parsed);
                    if(result.ec==std::errc{}&&result.ptr==end&&parsed>=750u&&parsed<=30000u)
                        return std::chrono::milliseconds{parsed};
                }
                return std::chrono::milliseconds{750};
            }();
            return dwell;
        }

        [[nodiscard]] inline const previewgrid::CloudVolume* claim_preview_probe(const core::Context* ctx,
            const previewgrid::PublishedSceneFrame& scene,PreviewProbeViewport viewport,double targetRadius)
        {
            static std::atomic<bool> captured{};
            if(captured.load(std::memory_order_relaxed))return nullptr;
            const auto& camera=scene.camera;
            const auto relativeEye=[&](const previewgrid::CloudVolume& cloud) {
                return std::array{camera.eye.x-cloud.center[0],camera.eye.y-cloud.center[1],camera.eye.z-cloud.center[2]};
            };
            const auto ratio=[&](const previewgrid::CloudVolume& cloud) {
                const auto eye=relativeEye(cloud);
                return std::hypot(eye[0],eye[1],eye[2])/cloud.radius;
            };
            const auto hasBound=[](const previewgrid::CloudVolume& cloud) {
                return cloud.recipe.view_occluder_radius_meters>0.0;
            };
            const auto nearTarget=[&](const previewgrid::CloudVolume& cloud) {
                return hasBound(cloud)&&std::abs(ratio(cloud)-targetRadius)<=0.0002;
            };
            using Clock=std::chrono::steady_clock;
            using Direction=std::array<double,3>;
            struct Stability final
            {
                const core::Context* context{};
                PreviewProbeViewport viewport{};
                Direction eye{},forward{},up{};
                std::uint32_t seed{};
                float fov{};
                Clock::time_point since{},last{};
            };
            static_assert(std::is_trivially_destructible_v<Stability>);
            static thread_local Stability stable;
            static std::atomic<unsigned> eligibilityLogged{};
            const auto firstReport=[&](unsigned kind) {
                return !(eligibilityLogged.load(std::memory_order_relaxed)&kind)
                    &&!(eligibilityLogged.fetch_or(kind,std::memory_order_relaxed)&kind);
            };
            if(camera.projection!=render_camera::ProjectionKind::perspective||!scene.solids)
            {
                stable={};
                return nullptr;
            }
            const auto found=std::ranges::find_if(scene.clouds,nearTarget);
            if(found==scene.clouds.end())
            {
                stable={};
                const auto bounded=std::ranges::find_if(scene.clouds,hasBound);
                if(bounded==scene.clouds.end()&&firstReport(1u))
                    logger::get("OpenGL.PreviewPass").logf(logger::LogLevel::INFO,std::source_location::current(),
                        "Scene {} probe waiting for a cloud with an admitted solid radius; {} cloud volumes.",
                        scene.revision,scene.clouds.size());
                else if(bounded!=scene.clouds.end()&&firstReport(2u))
                    logger::get("OpenGL.PreviewPass").logf(logger::LogLevel::INFO,std::source_location::current(),
                        "Scene {} probe outside the target band: distance/radius {:.9g}, target {:.9g}.",
                        scene.revision,ratio(*bounded),targetRadius);
                return nullptr;
            }
            if(firstReport(4u))
                logger::get("OpenGL.PreviewPass").logf(logger::LogLevel::INFO,std::source_location::current(),
                    "Scene {} probe in the target band: distance/radius {:.9g}. Waiting for camera dwell.",
                    scene.revision,ratio(*found));

            auto eye=relativeEye(*found);
            for(auto& coordinate:eye)coordinate*=found->meters_per_world_unit;
            const auto unit=[](double x,double y,double z) -> Direction {
                const double length=std::hypot(x,y,z);
                return {x/length,y/length,z/length};
            };
            const auto forward=unit(double(camera.target.x)-camera.eye.x,
                double(camera.target.y)-camera.eye.y,double(camera.target.z)-camera.eye.z);
            const auto up=unit(camera.up.x,camera.up.y,camera.up.z);
            const auto withinHeading=[](const Direction& a,const Direction& b) {
                constexpr double cosineQuarterDegree=0.9999904807;
                return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]>=cosineQuarterDegree;
            };
            const auto now=Clock::now();
            if(stable.context!=ctx||stable.viewport!=viewport||stable.seed!=found->recipe.seed
                ||std::hypot(eye[0]-stable.eye[0],eye[1]-stable.eye[1],eye[2]-stable.eye[2])>2.0
                ||!withinHeading(forward,stable.forward)||!withinHeading(up,stable.up)
                ||std::abs(camera.fovRadians-stable.fov)>0.002
                ||now-stable.last>std::chrono::milliseconds(250))
            {
                stable={ctx,viewport,eye,forward,up,found->recipe.seed,camera.fovRadians,now,now};
                return nullptr;
            }
            stable.last=now;
            return now-stable.since>=configured_preview_probe_dwell()
                &&!captured.exchange(true,std::memory_order_relaxed)?&*found:nullptr;
        }

        struct PreviewPassProbe final
        {
            struct ReadState final
            {
                GLint readFramebuffer{},drawFramebuffer{},drawBuffer{},targetReadBuffer{},pixelPackBuffer{};
                GLint alignment{},rowLength{},skipRows{},skipPixels{},swapBytes{};

                ReadState() noexcept
                {
                    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFramebuffer);
                    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFramebuffer);
                    glGetIntegerv(GL_DRAW_BUFFER0,&drawBuffer);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER,static_cast<GLuint>(drawFramebuffer));
                    glGetIntegerv(GL_READ_BUFFER,&targetReadBuffer);
                    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&pixelPackBuffer);
                    glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);
                    glGetIntegerv(GL_PACK_ROW_LENGTH,&rowLength);
                    glGetIntegerv(GL_PACK_SKIP_ROWS,&skipRows);
                    glGetIntegerv(GL_PACK_SKIP_PIXELS,&skipPixels);
                    glGetIntegerv(GL_PACK_SWAP_BYTES,&swapBytes);
                }

                ReadState(const ReadState&)=delete;
                ReadState& operator=(const ReadState&)=delete;

                ~ReadState() noexcept
                {
                    glReadBuffer(static_cast<GLenum>(targetReadBuffer));
                    glBindFramebuffer(GL_READ_FRAMEBUFFER,static_cast<GLuint>(readFramebuffer));
                    glBindBuffer(GL_PIXEL_PACK_BUFFER,static_cast<GLuint>(pixelPackBuffer));
                    glPixelStorei(GL_PACK_ALIGNMENT,alignment);
                    glPixelStorei(GL_PACK_ROW_LENGTH,rowLength);
                    glPixelStorei(GL_PACK_SKIP_ROWS,skipRows);
                    glPixelStorei(GL_PACK_SKIP_PIXELS,skipPixels);
                    glPixelStorei(GL_PACK_SWAP_BYTES,swapBytes);
                }
            };

            PreviewProbeViewport viewport{};
            std::uint64_t revision{};

            [[nodiscard]] bool read(std::string_view stage) const
            {
                const ReadState saved;
                GLint samples{},depthBits{};
                glGetIntegerv(GL_SAMPLES,&samples);
                glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,saved.drawFramebuffer?GL_DEPTH_ATTACHMENT:GL_DEPTH,
                    GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,&depthBits);
                auto& log=logger::get("OpenGL.PreviewPass");
                if(samples>0||depthBits<=0||saved.drawBuffer==GL_NONE||saved.drawBuffer==GL_FRONT_AND_BACK)
                {
                    log.logf(logger::LogLevel::INFO,std::source_location::current(),
                        "Scene {} {} samples unavailable: target={} samples={} depthBits={} drawBuffer={}.",
                        revision,stage,saved.drawFramebuffer,samples,depthBits,saved.drawBuffer);
                    return false;
                }
                glReadBuffer(static_cast<GLenum>(saved.drawBuffer));
                glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
                glPixelStorei(GL_PACK_ALIGNMENT,1);
                glPixelStorei(GL_PACK_ROW_LENGTH,0);
                glPixelStorei(GL_PACK_SKIP_ROWS,0);
                glPixelStorei(GL_PACK_SKIP_PIXELS,0);
                glPixelStorei(GL_PACK_SWAP_BYTES,GL_FALSE);
                const auto readPixel=[&](int x,int y)
                {
                    std::array<GLubyte,4> rgba{};
                    GLfloat depth=std::numeric_limits<GLfloat>::quiet_NaN();
                    glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
                    glReadPixels(x,y,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);
                    log.logf(logger::LogLevel::INFO,std::source_location::current(),
                        "Scene {} {} px=({},{}), rgba=({},{},{},{}), depth={:.9g}.",revision,stage,x,y,
                        unsigned(rgba[0]),unsigned(rgba[1]),unsigned(rgba[2]),unsigned(rgba[3]),depth);
                };
                constexpr std::array<std::array<int,2>,5> quarters{{{2,2},{1,1},{3,1},{1,3},{3,3}}};
                for(const auto& point:quarters)
                    readPixel(viewport[0]+viewport[2]*point[0]/4,viewport[1]+viewport[3]*point[1]/4);

                // Optional viewport-relative pixel from a prior native capture.
                const auto pixel=core::env::get("EPOCH_OPENGL_PREVIEW_SAMPLE_PIXEL");
                if(pixel)
                {
                    const std::string_view coordinate{pixel->data(),pixel->size()};
                    const auto separator=coordinate.find(',');
                    if(separator!=std::string_view::npos)
                    {
                        int x{},y{};
                        const auto begin=pixel->data();
                        const auto comma=begin+separator;
                        const auto end=begin+pixel->size();
                        const auto a=std::from_chars(begin,comma,x);
                        const auto b=std::from_chars(comma+1,end,y);
                        if(a.ec==std::errc{}&&a.ptr==comma&&b.ec==std::errc{}&&b.ptr==end
                            &&x>=0&&x<viewport[2]&&y>=0&&y<viewport[3])
                            readPixel(viewport[0]+x,viewport[1]+y);
                    }
                }
                return true;
            }
        };
    }

    inline void draw_planetary_sky(
        epochengine::openglstate::OpenGL4State& state,
        const epochengine::previewgrid::PublishedSceneFrame& scene, float aspect)
    {
        using namespace epochengine::previewgrid;
        const auto& camera=scene.camera;
        static const auto stars = []
        {
            std::array<Vertex,8192> catalog{};
            std::uint32_t seed = 0x5ced109bu;
            const auto sample = [&seed] {
                seed ^= seed << 13u; seed ^= seed >> 17u; seed ^= seed << 5u;
                return static_cast<float>(seed >> 8u) / 16777216.0f;
            };
            for (auto& star : catalog)
            {
                const float latitude = 2.0f * sample() - 1.0f;
                const float azimuth = 6.283185307f * sample();
                const float radial = std::sqrt(1.0f - latitude * latitude);
                const float light = sample();
                const float brightness = 0.09f + 0.80f * light * light * light * light;
                const float warmth = sample();
                star = {{radial * std::cos(azimuth), latitude, radial * std::sin(azimuth)},
                        {brightness * (0.82f + 0.18f * warmth), brightness * 0.92f,
                         brightness * (1.0f - 0.24f * warmth)}};
            }
            return catalog;
        }();
        const auto view = look_at({}, camera_forward(camera), camera.up);
        const auto mvp = multiply(perspective(camera.fovRadians, aspect, 0.01f, 2.0f), view);
        glBindVertexArray(state.sceneVao);
        if (!state.sceneGridBufferHoldsSky)
        {
            glBindBuffer(GL_ARRAY_BUFFER, state.sceneVbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(stars), stars.data(), GL_STATIC_DRAW);
            state.sceneGridBufferHoldsSky = true;
        }
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        if (scene.space_background)
        {
            glUniformMatrix4fv(state.sceneMvpLoc, 1, GL_FALSE, mvp.data());
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(stars.size()));
        }
        const auto& sky=state.sceneAtmosphereLocs;
        if(sky[0]>=0)
        {
            if (const auto atmosphere = planet_atmosphere_view(scene, aspect))
            {
                const auto uniform=[](GLint location,Vec3 vector){glUniform3f(location,vector.x,vector.y,vector.z);};
                uniform(sky[1], atmosphere->observer);
                uniform(sky[2], atmosphere->forward);
                uniform(sky[3], atmosphere->right);
                uniform(sky[4], atmosphere->up);
                uniform(sky[5], atmosphere->sun);
                glUniform1f(sky[6], atmosphere->thickness);
                glUniform1i(sky[7], atmosphere->sun_visible ? 1 : 0);
                GLint blend[4]{};
                glGetIntegerv(GL_BLEND_SRC_RGB,&blend[0]);
                glGetIntegerv(GL_BLEND_DST_RGB,&blend[1]);
                glGetIntegerv(GL_BLEND_SRC_ALPHA,&blend[2]);
                glGetIntegerv(GL_BLEND_DST_ALPHA,&blend[3]);
                glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE);
                glEnable(GL_BLEND);
                glDisable(GL_CULL_FACE);
                glUniform1i(sky[0],1);
                glDrawArrays(GL_TRIANGLES,0,3);
                glUniform1i(sky[0],0);
                glEnable(GL_CULL_FACE);
                glDisable(GL_BLEND);
                glBlendFuncSeparate(blend[0],blend[1],blend[2],blend[3]);
            }
        }
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
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
        const auto matchedScene = previewMode == core::ScenePreviewMode::Editor
            ? epochengine::previewgrid::scene_frame_for(ctx) : nullptr;
        const auto clearColor = epochengine::previewgrid::scene_clear_color(matchedScene.get());
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (previewMode == core::ScenePreviewMode::Editor && ensure_scene_preview_pipeline(ctx, state))
        {
            const auto camera = matchedScene ? matchedScene->camera
                : epochengine::previewgrid::camera_for(ctx);
            std::optional<detail::PreviewPassProbe> passProbe;
            if(const double sampleRadius=detail::configured_preview_probe_radius();sampleRadius>0.0&&matchedScene
                &&viewportX>=0&&glViewportY>=0
                &&viewportX<=framebufferWidth-viewportWidth&&glViewportY<=framebufferHeight-viewportHeight)
            {
                const detail::PreviewProbeViewport viewport{viewportX,glViewportY,viewportWidth,viewportHeight};
                if(const auto* body=detail::claim_preview_probe(ctx,*matchedScene,viewport,sampleRadius))
                {
                    const auto textured=std::ranges::count_if(matchedScene->solids->markers,
                        [](const auto& entry){return entry.textured&&!entry.textured->empty();});
                    const auto distance=std::hypot(camera.eye.x-body->center[0],camera.eye.y-body->center[1],
                        camera.eye.z-body->center[2]);
                    logger::get("OpenGL.PreviewPass").logf(logger::LogLevel::INFO,std::source_location::current(),
                        "Scene {} probe: viewport=({},{},{},{}), distance/radius={:.9g}, radius={:.9g}, seed={}, "
                        "origin=({:.9g},{:.9g},{:.9g}), eye=({:.9g},{:.9g},{:.9g}), target=({:.9g},{:.9g},{:.9g}), "
                        "near={:.9g}, far={:.9g}, plainVertices={}, texturedMarkers={}, clouds={}.",
                        matchedScene->revision,viewportX,glViewportY,viewportWidth,viewportHeight,distance/body->radius,
                        body->radius,body->recipe.seed,matchedScene->render_origin[0],matchedScene->render_origin[1],
                        matchedScene->render_origin[2],camera.eye.x,camera.eye.y,camera.eye.z,
                        camera.target.x,camera.target.y,camera.target.z,camera.nearPlane,camera.farPlane,
                        matchedScene->solids->geometry->size(),textured,matchedScene->clouds.size());
                    passProbe.emplace(viewport,matchedScene->revision);
                    if(!passProbe->read("clear"))passProbe.reset();
                }
            }
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
            if (state.scenePaintRowsLoc >= 0) glUniform1i(state.scenePaintRowsLoc, 0);
            if (state.sceneWireLoc >= 0) glUniform1i(state.sceneWireLoc, 0);
            if (state.sceneWaterLocs[0] >= 0) glUniform1f(state.sceneWaterLocs[0], 0.0f);
            if (matchedScene && (matchedScene->space_background
                || std::any_of(matchedScene->markers.begin(), matchedScene->markers.end(),
                    [](const auto& marker) { return marker.planetAtmosphere.has_value(); })))
                draw_planetary_sky(state, *matchedScene, aspect);
            if(passProbe)(void)passProbe->read("sky");
            glUniformMatrix4fv(state.sceneMvpLoc, 1, GL_FALSE, mvp.data());

            if (!matchedScene)
            {
                const auto gridGeometry = epochengine::previewgrid::grid_geometry_for(ctx);
                if (state.sceneGridBufferHoldsSky || state.sceneGridSignature != gridGeometry->signature)
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
                    state.sceneGridBufferHoldsSky = false;
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

            const auto solidFrame = matchedScene ? matchedScene->solids
                : epochengine::previewgrid::object_solid_frame_for(ctx);
            const auto solidGeometry = solidFrame->geometry;

            const auto& solidVertices = *solidGeometry;
            bool drew_water_detail = false;
            if (solidVertices.size() >= 3 && state.sceneMarkerVao && state.sceneMarkerVbo)
            {
                ScopedForestCoverage coverage{state,*solidFrame};
                glDepthMask(GL_TRUE);
                glBindVertexArray(state.sceneMarkerVao);
                glBindBuffer(GL_ARRAY_BUFFER, state.sceneMarkerVbo);
                glBufferData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(solidVertices.size() * sizeof(solidVertices[0])),
                    solidVertices.data(),
                    GL_DYNAMIC_DRAW);
                const auto draw=[&](std::size_t first,std::size_t count) {
                    if(count==0u)return;
                    glDrawArrays(GL_TRIANGLES,static_cast<GLint>(first),static_cast<GLsizei>(count));
                    ++state.meshDrawCallsSubmitted;
                };
                const auto& water=state.sceneWaterLocs;
                const auto detail=matchedScene&&std::ranges::all_of(water,[](GLint location){return location>=0;})
                    ?epochengine::previewgrid::water_detail_for(*matchedScene):std::nullopt;
                if(detail)
                {
                    glUniform3f(water[1],detail->center_meters.x,detail->center_meters.y,detail->center_meters.z);
                    glUniform3f(water[2],detail->eye.x,detail->eye.y,detail->eye.z);
                    glUniform4f(water[3],detail->sun.x,detail->sun.y,detail->sun.z,detail->sunlight);
                    glUniform4fv(water[4],static_cast<GLsizei>(detail->waves.size()),detail->waves.front().data());
                    glUniform4fv(water[5],static_cast<GLsizei>(detail->slopes.size()),detail->slopes.front().data());
                    std::size_t first{};
                    for(const auto& range:solidFrame->water_detail_ranges)
                    {
                        draw(first,range.first_vertex-first);
                        glUniform1f(water[0],detail->meters_per_unit);
                        draw(range.first_vertex,range.vertex_count);
                        glUniform1f(water[0],0.0f);
                        first=std::size_t(range.first_vertex)+range.vertex_count;
                    }
                    draw(first,solidVertices.size()-first);
                }
                else draw(0u,solidVertices.size());
                drew_water_detail = detail.has_value();
                state.meshTrianglesSubmitted+=solidVertices.size()/3u;
                state.meshUploadBytesSubmitted+=solidVertices.size()*sizeof(solidVertices[0]);
            }
            if(passProbe)(void)passProbe->read("plain-opaque");

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
            if(passProbe)(void)passProbe->read("textured-and-sampled");

            if(matchedScene)
            {
                const auto spray_points = render_shoreline_spray(*matchedScene,state,mvp,viewportHeight);
                epochengine::render_water_evidence::record(*matchedScene,"opengl",drew_water_detail,spray_points);
                const bool cloudRendered=openglcloud::render(state.cloudVolume,*matchedScene,viewportX,glViewportY,
                    viewportWidth,viewportHeight);
                if(passProbe)(void)passProbe->read(cloudRendered?"cloud-complete":"cloud-refused");
            }

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
            if(passProbe)(void)passProbe->read("scene-helpers");
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
