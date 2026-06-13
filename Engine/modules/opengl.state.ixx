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
#include <cstdint>
#include <functional>

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

#if defined(__linux__)
#   include <X11/Xlib.h>
#   include <GL/glx.h>
#endif

#endif // EPOCH_USING_OPENGL

export module opengl.state;

import core.timer;          // timing::Timer, timing::createTimer(...)
import opengl.platform;   // PlatformGLContext

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)

export namespace epochnamespace::openglstate
{
    struct OpenGL4State
    {
#if defined(_WIN32)
        HWND    hwnd = nullptr;
        HDC     hdc = nullptr;
        HGLRC   hglrc = nullptr;

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

        // GL objects / uniform locations
        GLuint shader = 0;
        GLint  uUVRegionLoc = -1;
        GLint  uTransformLoc = -1;
        GLint  uSamplerLoc = -1;
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        GLuint fbo = 0;

        GLuint sceneShader = 0;
        GLint sceneMvpLoc = -1;
        GLuint sceneVao = 0;
        GLuint sceneVbo = 0;
        GLuint sceneEbo = 0;
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
