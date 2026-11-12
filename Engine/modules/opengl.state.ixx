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

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

// Global module fragment: macros + native headers + GL typedefs only.
#include <include/engine.config.hpp> // for EPOCH_USING Macros

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   ifndef _WINSOCKAPI_
#       define _WINSOCKAPI_
#   endif

#   include <windows.h>
#   include <wingdi.h>
#endif

// OpenGL basic typedefs (GLuint, GLint, etc.)
// IMPORTANT: do NOT include GL/wglext.h here.
#   include <glad/glad.h>
#   include "../src/renderers/opengl/opengl.frame_admission.hpp"

#if defined(__linux__)
#   include <X11/Xlib.h>
#   include <GL/glx.h>
#endif

#endif // EPOCH_USING_OPENGL

export module opengl.state;

import core.timer;          // timing::Timer, timing::createTimer(...)
import opengl.platform;   // PlatformGLContext
import opengl.native_present;
import render.device;
import render.device_opengl_family;
import render.texture_residency;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)

export namespace epochengine::openglstate
{
    struct MeshBaseColorResources final
    {
        OpenGLFamilyRenderDevice device{RendererBackendKind::opengl};
        texture_residency::TextureResidencyCache residency{device, 1, [] {
            texture_residency::ResidencyLimits limits{};
            limits.maximum_resident_bytes=64ull*1024*1024;
            limits.maximum_single_texture_bytes=32ull*1024*1024;
            limits.maximum_transient_replacement_bytes=32ull*1024*1024;
            limits.maximum_upload_bytes_per_frame=64ull*1024*1024;
            return limits;
        }()};
        GLuint program{}, vao{}, vbo{};
        GLint mvp{-1}, texture{-1}, decode_srgb{-1}, encode_srgb{-1};
        GLint alpha_cutout{-1}, alpha_cutoff{-1};
        std::uint64_t sequence{};
        bool failed{};
    };

    struct CloudVolumeResources final
    {
        GLuint program{}, composite_program{}, vao{}, depth_texture{}, color_texture{}, framebuffer{};
        int width{}, height{};
        bool failed{};
        const char* diagnostic{};
    };

    struct OpenGL4State
    {
#if defined(_WIN32)
        HWND    hwnd = nullptr;
        HDC     hdc = nullptr;
        HGLRC   hglrc = nullptr;
        bool ownsWindow = false;
        bool ownsDc = false;
        bool ownsContext = false;

        WNDPROC oldWndProc = nullptr;
        WNDPROC getOldWndProc() const noexcept { return oldWndProc; }

        HWND parent = nullptr;
        std::function<void(int, int)> onResize{};

        unsigned int width{ 400 };
        unsigned int height{ 300 };

        bool running{ false };

#elif defined(__linux__)
        ::Window    window = 0;
        GLXDrawable drawable = 0;
        Display* display = nullptr;
        GLXContext  glxContext = nullptr;
        GLXFBConfig fbConfig = nullptr;
        Colormap    colormap = 0;

        bool ownsDisplay = false;
        bool ownsWindow = false;
        bool ownsContext = false;
        bool ownsColormap = false;

        unsigned int width{ 400 };
        unsigned int height{ 300 };
#endif

        struct MouseState
        {
            std::array<bool, 5> down{};
            std::array<bool, 5> pressed{};
            std::array<bool, 5> prevDown{};
            int lastX = 0;
            int lastY = 0;
        } mouse{};

        struct KeyboardState
        {
            std::bitset<256> down{};
            std::bitset<256> pressed{};
            std::bitset<256> prevDown{};
        } keyboard{};

        timing::Timer pollTimer = timing::createTimer(1.0);
        timing::Timer fpsTimer = timing::createTimer(1.0);
        int frameCount = 0;
        epochengine::openglcontext::frame_admission::State frameAdmission{};
        epochengine::openglcontext::native_present::State nativePresent{};
        unsigned nativePresentSyncInterval{};

        std::unique_ptr<MeshBaseColorResources> meshBaseColor{};
        CloudVolumeResources cloudVolume{};

        // GL objects / uniform locations
        GLuint shader = 0;
        GLint  uUVRegionLoc = -1;
        GLint  uTransformLoc = -1;
        GLint  uSamplerLoc = -1;
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        GLuint fbo = 0;
        // Owned by this native context, separate from the immediate quad.
        GLuint spriteStreamVao = 0;
        GLuint spriteStreamVbo = 0;
        bool spriteStreamUnavailable = false;
        std::size_t spriteStreamCapacitySprites = 0;
        std::size_t spriteStreamWriteVertex = 0;
        bool spriteStreamFramePrepared = false;
        bool spriteStreamFrameAllocated = false;
        std::uint64_t guiSpritesSubmitted = 0;
        std::uint64_t guiDrawCallsSubmitted = 0;
        std::uint64_t meshTrianglesSubmitted = 0;
        std::uint64_t meshDrawCallsSubmitted = 0;
        std::uint64_t meshUploadBytesSubmitted = 0;

        GLuint sceneShader = 0;
        GLint sceneMvpLoc = -1;
        GLint sceneWireLoc = -1;
        GLuint sceneVao = 0;
        GLuint sceneVbo = 0;
        GLuint sceneEbo = 0;
        std::uint64_t sceneGridSignature = 0;
        GLsizei sceneGridIndexCount = 0;
        GLuint sceneMarkerVao = 0;
        GLuint sceneMarkerVbo = 0;

        GLuint arcadeScreenShader = 0;
        GLint arcadeScreenMvpLoc = -1;
        GLint arcadeScreenTextureLoc = -1;
        GLuint arcadeScreenVao = 0;
        GLuint arcadeScreenVbo = 0;
        GLuint arcadeScreenEbo = 0;
        GLuint arcadeScreenTexture = 0;
        GLuint arcadeScreenFramebuffer = 0;
        GLuint arcadeScreenDepth = 0;
        std::uint64_t arcadeScreenFrame = 0;
    };

    // header-dominant style: single TU-safe in C++20+ modules
    inline OpenGL4State s_openglstate{};
}

#endif // EPOCH_USING_OPENGL
