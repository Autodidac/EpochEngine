/************************************************
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•—  â–ˆâ–ˆâ•—   *
 *  â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ•”â•â•â•  â–ˆâ–ˆâ•”â•â•â•â• â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘     â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â•šâ•â•â•â•â•â•â•â•šâ•â•      â•šâ•â•â•â•â•â•  â•šâ•â•â•â•â•â•â•šâ•â•  â•šâ•â•   *
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
 // opengl.context.ixx
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// NOTE: Keep your engine config include if it sets global compile flags.
// Do NOT rely on it for Win32 type definitions in a module global fragment.
#include "../include/engine.config.hpp"
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)

// OS + GL headers in global module fragment.
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#   define NOMINMAX
#endif

// Windows base types MUST come first for WGL/wingdi.
#include <windows.h>
#include <wingdi.h>

// -----------------------------------------------------------------------------
// IMPORTANT:
// wglext.h (and glad_wgl.h) require OpenGL base types (GLenum/GLint/GLuint/etc).
// Ensure glad.h (preferred) or gl.h is included BEFORE wglext/glad_wgl.
// -----------------------------------------------------------------------------
#if defined(__has_include)
#  if __has_include(<glad/glad.h>)
#    include <glad/glad.h>
#  else
#    include <GL/gl.h>
#  endif
#else
#  include <glad/glad.h>
#endif

#if defined(__has_include)
#  if __has_include(<glad/glad_wgl.h>)
#    include <glad/glad_wgl.h>
#    define EPOCH_HAS_WGL_EXTENSION_HEADERS 1
#  elif __has_include(<GL/wglext.h>)
#    include <GL/wglext.h>
#    define EPOCH_HAS_WGL_EXTENSION_HEADERS 1
#  endif
#endif

#ifndef EPOCH_HAS_WGL_EXTENSION_HEADERS
#  ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#    define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#  endif
#  ifndef WGL_CONTEXT_MINOR_VERSION_ARB
#    define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#  endif
#  ifndef WGL_CONTEXT_PROFILE_MASK_ARB
#    define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#  endif
#  ifndef WGL_CONTEXT_CORE_PROFILE_BIT_ARB
#    define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#  endif
#endif

#elif defined(__linux__)
#   include <glad/glad.h>
#   include <X11/Xlib.h>
#   include <X11/Xutil.h>
#   include <GL/glx.h>
#   include <GL/glxext.h>
#endif

#include <chrono>
#endif

#include "opengl.context_detail.hpp"
#include "opengl.canvas2d_scene.hpp"

module opengl.context;
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
#if defined(_WIN32) && !defined(EPOCH_HAS_WGL_EXTENSION_HEADERS)
using PFNWGLCREATECONTEXTATTRIBSARBPROC = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
#endif
// ------------------------------------------------------------
// Core engine modules
// ------------------------------------------------------------
import core.context;
import context.multiplexer;
import context.commandqueue;
import context.window;
import context.type;
import input.engine;
import platform.pump;
import atlas.manager;
import atlas.texture;
import core.commandline;
import core.logger;
import diagnostics.engine;
import telemetry.engine;
import opengl.capture;
import opengl.preview;

// ------------------------------------------------------------
// OpenGL backend modules
// ------------------------------------------------------------
import opengl.textures;
import opengl.state;
import opengl.platform;
import opengl.quad;

// ------------------------------------------------------------
// Standard library
// ------------------------------------------------------------

namespace epochengine::openglcontext
{
#if !defined(EPOCH_USING_OPENGL)

    inline bool opengl_initialize(std::shared_ptr<core::Context>, void* = nullptr,
        unsigned int = 0, unsigned int = 0, std::function<void(int, int)> = nullptr) {
        return false;
    }

    inline void opengl_present() {}
    inline int  opengl_get_width() { return 1; }
    inline int  opengl_get_height() { return 1; }
    inline void opengl_clear() {}
    inline bool opengl_process(std::shared_ptr<core::Context>, core::CommandQueue&) { return false; }
    inline void opengl_cleanup(std::shared_ptr<core::Context>) {}

#else

    // ------------------------------------------------------------
    // Public API
    // ------------------------------------------------------------
    void opengl_cleanup(std::shared_ptr<core::Context> ctx)
    {
        openglcanvas2d::release_canvas2d_scene_renderer(ctx.get());

        auto& backend = opengltextures::get_opengl_backend();
        auto& glState = backend.glState;

#if defined(_WIN32)
        {
            PlatformGL::ScopedContext cleanupGuard;
            auto cleanupContext = ctx ? contextdetail::context_to_platform_context(ctx.get()) : PlatformGL::PlatformGLContext{};
            if (!cleanupContext.valid())
                cleanupContext = contextdetail::state_to_platform_context(glState);

            bool canActivateCleanup = cleanupContext.valid();
            HWND cleanupHwnd = ctx && ctx->windowData && ctx->windowData->hwnd ? ctx->windowData->hwnd : glState.hwnd;
            if (cleanupHwnd && (::IsWindow(cleanupHwnd) == FALSE))
                canActivateCleanup = false;

            if (canActivateCleanup && cleanupGuard.set(cleanupContext))
            {
                openglpreview::destroy_scene_preview_pipeline(glState);
                opengltextures::clear_gpu_atlases();
            }
        }
        PlatformGL::clear_current();

        if (glState.ownsContext && glState.hglrc) { ::wglDeleteContext(glState.hglrc); }
        glState.hglrc = nullptr;
        glState.ownsContext = false;
        if (glState.ownsDc && glState.hdc && glState.hwnd) { ::ReleaseDC(glState.hwnd, glState.hdc); }
        glState.hdc = nullptr;
        glState.ownsDc = false;

        if (glState.ownsWindow && glState.hwnd) { ::DestroyWindow(glState.hwnd); }
        glState.hwnd = nullptr;
        glState.ownsWindow = false;
        glState.parent = nullptr;

#elif defined(__linux__)
        {
            PlatformGL::ScopedContext cleanupGuard;
            auto cleanupContext = ctx ? contextdetail::context_to_platform_context(ctx.get()) : PlatformGL::PlatformGLContext{};
            if (!cleanupContext.valid())
                cleanupContext = contextdetail::state_to_platform_context(glState);
            if (cleanupContext.valid() && cleanupGuard.set(cleanupContext))
            {
                openglpreview::destroy_scene_preview_pipeline(glState);
                opengltextures::clear_gpu_atlases();
            }
        }
        PlatformGL::clear_current();
        if (glState.display && glState.glxContext && glState.ownsContext) glXDestroyContext(glState.display, glState.glxContext);
        if (glState.display && glState.window && glState.ownsWindow) XDestroyWindow(glState.display, glState.window);
        if (glState.display && glState.colormap && glState.ownsColormap) XFreeColormap(glState.display, glState.colormap);
        if (glState.display && glState.ownsDisplay) XCloseDisplay(glState.display);

        glState.display = nullptr;
        glState.window = 0;
        glState.drawable = 0;
        glState.glxContext = nullptr;
        glState.colormap = 0;
        glState.fbConfig = nullptr;
        glState.ownsDisplay = false;
        glState.ownsWindow = false;
        glState.ownsContext = false;
        glState.ownsColormap = false;
#endif
    }

#endif // EPOCH_USING_OPENGL
} // namespace epochengine::openglcontext

#endif
