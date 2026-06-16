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
#  ifndef PFNWGLCREATECONTEXTATTRIBSARBPROC
using PFNWGLCREATECONTEXTATTRIBSARBPROC = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
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

#include "renderers/opengl/opengl_context_detail.hpp"

module opengl.context;
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
// ------------------------------------------------------------
// Core engine modules
// ------------------------------------------------------------
import core.context;
import context.multiplexer;
import context.commandqueue;
import context.window;
import context.type;
import engine.input;
import platformpump;
import atlas.manager;
import atlas.texture;
import core.commandline;
import core.logger;
import engine.diagnostics;
import engine.telemetry;
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

namespace epochnamespace::openglcontext
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

    namespace detail
    {
#if defined(_WIN32)
        inline const wchar_t* gl_child_class_name() noexcept { return L"EpochGLChild"; }

        inline LRESULT CALLBACK gl_child_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
        {
            switch (msg)
            {
            case WM_ERASEBKGND:
                return 1;

            case WM_PAINT:
            {
                PAINTSTRUCT ps{};
                ::BeginPaint(hwnd, &ps);
                ::EndPaint(hwnd, &ps);
                return 0;
            }

            default:
                return ::DefWindowProcW(hwnd, msg, wParam, lParam);
            }
        }

        inline void ensure_gl_child_class_registered()
        {
            static bool s_registered = false;
            if (s_registered) return;

            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.style = CS_OWNDC;
            wc.lpfnWndProc = gl_child_proc;
            wc.hInstance = ::GetModuleHandleW(nullptr);
            wc.hbrBackground = nullptr;
            wc.lpszClassName = gl_child_class_name();

            // If it already exists, RegisterClassExW will fail with ERROR_CLASS_ALREADY_EXISTS.
            if (!::RegisterClassExW(&wc))
            {
                const DWORD err = ::GetLastError();
                if (err != ERROR_CLASS_ALREADY_EXISTS)
                    throw std::runtime_error(std::format("[ OpenGL ] - RegisterClassExW failed (err={})", err));
            }

            s_registered = true;
        }
#endif

#if defined(__linux__)
        inline ::Window to_xwindow(void* opaque) noexcept
        {
            return static_cast<::Window>(reinterpret_cast<std::uintptr_t>(opaque));
        }
        inline void* from_xwindow(::Window window) noexcept
        {
            return reinterpret_cast<void*>(static_cast<std::uintptr_t>(window));
        }
#endif

#if defined(_WIN32)
        inline bool is_bad_wgl_ptr(void* p) noexcept
        {
            const auto v = reinterpret_cast<std::uintptr_t>(p);
            return v == 0u || v == 1u || v == 2u || v == 3u || v == static_cast<std::uintptr_t>(-1);
        }
#endif
    } // namespace detail

    // ------------------------------------------------------------
    // Public API
    // ------------------------------------------------------------
    bool opengl_initialize(std::shared_ptr<core::Context> ctx,
        void* parentWindowOpaque,
        unsigned int w,
        unsigned int h,
        std::function<void(int, int)> onResize)
    {
        if (!ctx)
            throw std::runtime_error("[ OpenGL ] - opengl_initialize requires non-null Context");

        auto& backend = epochnamespace::opengltextures::get_opengl_backend();
        auto& glState = backend.glState;

        glState.width = w;
        glState.height = h;
        auto* glStatePtr = &glState;

        ctx->onResize = [glStatePtr, resize = std::move(onResize)](int newWidth, int newHeight) mutable
            {
                const int clampedWidth = (std::max)(1, newWidth);
                const int clampedHeight = (std::max)(1, newHeight);

                glStatePtr->width = static_cast<unsigned int>(clampedWidth);
                glStatePtr->height = static_cast<unsigned int>(clampedHeight);

                if (resize)
                    resize(clampedWidth, clampedHeight);
            };

#if defined(_WIN32)
        HWND parentHwnd = static_cast<HWND>(parentWindowOpaque);
        if (ctx->windowData && ctx->windowData->hwnd)
            parentHwnd = ctx->windowData->hwnd;
        if (!parentHwnd && ctx->hwnd)
            parentHwnd = ctx->hwnd;

        if (!parentHwnd)
            throw std::runtime_error("[ OpenGL ] - No parent HWND available");

        bool usingExternalContext = false;

        // IMPORTANT: match your WindowData naming (your working header used glContext).
        if (ctx->windowData && ctx->windowData->hwnd && ctx->windowData->hdc && ctx->windowData->glContext)
        {
            glState.hwnd = ctx->windowData->hwnd;
            glState.hdc = ctx->windowData->hdc;
            glState.hglrc = ctx->windowData->glContext;
            usingExternalContext = true;
        }
        else if (ctx->hwnd && ctx->hdc && ctx->hglrc)
        {
            glState.hwnd = ctx->hwnd;
            glState.hdc = ctx->hdc;
            glState.hglrc = ctx->hglrc;
            usingExternalContext = true;
        }

        if (!usingExternalContext)
        {
            detail::ensure_gl_child_class_registered();

            if (!glState.hwnd)
            {
                glState.parent = parentHwnd;
                glState.hwnd = ::CreateWindowExW(
                    0,
                    detail::gl_child_class_name(),
                    L"",
                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                    0, 0, static_cast<int>(w), static_cast<int>(h),
                    glState.parent,
                    nullptr,
                    ::GetModuleHandleW(nullptr),
                    nullptr);

                if (!glState.hwnd)
                    throw std::runtime_error("[ OpenGL ] - CreateWindowExW failed for child GL window");
            }

            glState.hdc = ::GetDC(glState.hwnd);
            if (!glState.hdc)
                throw std::runtime_error("[ OpenGL ] - GetDC failed");

            // SetPixelFormat is one-time per HDC.
            if (::GetPixelFormat(glState.hdc) == 0)
            {
                PIXELFORMATDESCRIPTOR pfd{
                    sizeof(PIXELFORMATDESCRIPTOR), 1,
                    PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
                    PFD_TYPE_RGBA, 32,
                    0,0,0,0,0,0,0,0,0,
                    24, 0, 0, 0, 0, 0, 0, 0
                };

                int pf = ::ChoosePixelFormat(glState.hdc, &pfd);
                if (!pf) throw std::runtime_error("[ OpenGL ] - ChoosePixelFormat failed");
                if (!::SetPixelFormat(glState.hdc, pf, &pfd))
                    throw std::runtime_error("[ OpenGL ] - SetPixelFormat failed");
            }

            // ---- WGL bootstrap: temp context stays current while loading + creating ----
            HGLRC tmp = ::wglCreateContext(glState.hdc);
            if (!tmp) throw std::runtime_error("[ OpenGL ] - wglCreateContext(temp) failed");
            if (::wglMakeCurrent(glState.hdc, tmp) != TRUE)
            {
                ::wglDeleteContext(tmp);
                throw std::runtime_error("[ OpenGL ] - wglMakeCurrent(temp) failed");
            }

            // Load wglCreateContextAttribsARB safely (filter WGL sentinel pointers).
            PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;
            {
                void* raw = reinterpret_cast<void*>(::wglGetProcAddress("wglCreateContextAttribsARB"));
                if (!detail::is_bad_wgl_ptr(raw))
                    wglCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(raw);
            }

            // Try modern core context; if it fails or extension missing, keep the temp legacy context as final.
            glState.hglrc = nullptr;

            if (wglCreateContextAttribsARB)
            {
                int attribs46[] = {
                    WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
                    WGL_CONTEXT_MINOR_VERSION_ARB, 6,
                    WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                    0
                };

                glState.hglrc = wglCreateContextAttribsARB(glState.hdc, nullptr, attribs46);

                if (!glState.hglrc)
                {
                    int attribs41[] = {
                        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
                        WGL_CONTEXT_MINOR_VERSION_ARB, 1,
                        WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                        0
                    };
                    glState.hglrc = wglCreateContextAttribsARB(glState.hdc, nullptr, attribs41);
                }
            }

            if (glState.hglrc)
            {
                // Switch to final and destroy temp.
                (void)::wglMakeCurrent(nullptr, nullptr);
                ::wglDeleteContext(tmp);
                tmp = nullptr;

                if (::wglMakeCurrent(glState.hdc, glState.hglrc) != TRUE)
                {
                    ::wglDeleteContext(glState.hglrc);
                    glState.hglrc = nullptr;
                    throw std::runtime_error("[ OpenGL ] - wglMakeCurrent(final) failed");
                }
            }
            else
            {
                // No modern context: the temp legacy context becomes the final context.
                glState.hglrc = tmp;
                tmp = nullptr;
            }
        }

        // Publish through PlatformGL (so the rest of the engine uses the same path).
        PlatformGL::PlatformGLContext finalCtx{};
        finalCtx.device = glState.hdc;
        finalCtx.context = glState.hglrc;

        PlatformGL::ScopedContext contextGuard{ finalCtx };
        if (!contextGuard.ok())
            throw std::runtime_error("[ OpenGL ] - PlatformGL::make_current(final) failed");

        // Load GL entry points with the single authoritative loader.
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(PlatformGL::get_proc_address)))
            throw std::runtime_error("[ OpenGL ] - gladLoadGLLoader failed");

        const auto* versionBytes = ::glGetString(GL_VERSION);
        if (!versionBytes)
            throw std::runtime_error("[ OpenGL ] - glGetString(GL_VERSION) returned null");

        const std::string_view version{ reinterpret_cast<const char*>(versionBytes) };
        if (version.empty())
            throw std::runtime_error("[ OpenGL ] - GL_VERSION string is empty");

        ctx->native_window = glState.hwnd;
        ctx->native_drawable = glState.hdc;
        ctx->native_gl_context = glState.hglrc;

        // Keep the context current through init so pipeline creation has
        // a valid active GL context on this thread.

#elif defined(__linux__)
        ::Window providedWindow = parentWindowOpaque ? detail::to_xwindow(parentWindowOpaque) : 0;
        Display* providedDisplay = nullptr;
        GLXContext providedContext = nullptr;

        if (ctx->windowData)
        {
            if (!providedWindow && ctx->windowData->hwnd)
                providedWindow = detail::to_xwindow(ctx->windowData->hwnd);
            if (!providedDisplay && ctx->windowData->hdc)
                providedDisplay = static_cast<Display*>(ctx->windowData->hdc);
            if (!providedContext && ctx->windowData->glContext)
                providedContext = static_cast<GLXContext>(ctx->windowData->glContext);
        }

        if (!providedWindow && ctx->hwnd)
            providedWindow = detail::to_xwindow(ctx->hwnd);
        if (!providedDisplay && ctx->hdc)
            providedDisplay = static_cast<Display*>(ctx->hdc);
        if (!providedContext && ctx->hglrc)
            providedContext = static_cast<GLXContext>(ctx->hglrc);

        Display* display = providedDisplay ? providedDisplay : glState.display;
        if (!display)
        {
            display = XOpenDisplay(nullptr);
            glState.ownsDisplay = true;
        }
        else
        {
            glState.ownsDisplay = false;
        }

        if (!display)
            throw std::runtime_error("[ OpenGL ] - XOpenDisplay failed");
        glState.display = display;

        const int screen = DefaultScreen(display);

        int fbCount = 0;
        int visualAttribs[] = {
            GLX_X_RENDERABLE, True,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_DOUBLEBUFFER, True,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_ALPHA_SIZE, 8,
            GLX_DEPTH_SIZE, 24,
            0
        };

        GLXFBConfig* configs = glXChooseFBConfig(display, screen, visualAttribs, &fbCount);
        if (!configs || fbCount == 0)
            throw std::runtime_error("[ OpenGL ] - glXChooseFBConfig failed");

        glState.fbConfig = configs[0];
        XFree(configs);

        XVisualInfo* vi = glXGetVisualFromFBConfig(display, glState.fbConfig);
        if (!vi)
            throw std::runtime_error("[ OpenGL ] - glXGetVisualFromFBConfig failed");

        if (!providedWindow && !glState.colormap)
        {
            glState.colormap = XCreateColormap(display, RootWindow(display, vi->screen), vi->visual, AllocNone);
            if (!glState.colormap)
            {
                XFree(vi);
                throw std::runtime_error("[ OpenGL ] - XCreateColormap failed");
            }
            glState.ownsColormap = true;
        }
        else if (providedWindow)
        {
            glState.colormap = 0;
            glState.ownsColormap = false;
        }

        if (providedWindow)
        {
            glState.window = providedWindow;
            glState.drawable = providedWindow;
            glState.ownsWindow = false;
        }
        else if (!glState.window)
        {
            XSetWindowAttributes swa{};
            swa.colormap = glState.colormap;
            swa.event_mask = ExposureMask | StructureNotifyMask;

            glState.window = XCreateWindow(
                display, RootWindow(display, vi->screen),
                0, 0, w, h, 0, vi->depth, InputOutput, vi->visual,
                CWColormap | CWEventMask, &swa);

            if (!glState.window)
            {
                XFree(vi);
                throw std::runtime_error("[ OpenGL ] - XCreateWindow failed");
            }

            XStoreName(display, glState.window, "Epoch OpenGL");
            XMapWindow(display, glState.window);
            XFlush(display);
            glState.ownsWindow = true;
        }

        XFree(vi);
        if (!glState.drawable)
            glState.drawable = glState.window;

        if (providedContext)
        {
            glState.glxContext = providedContext;
            glState.ownsContext = false;
        }
        else if (!glState.glxContext)
        {
            // Use the single authoritative loader.
            auto createContextAttribs =
                reinterpret_cast<PFNGLXCREATECONTEXTATTRIBSARBPROC>(
                    PlatformGL::get_proc_address("glXCreateContextAttribsARB"));

            int contextAttribs[] = {
                GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
                GLX_CONTEXT_MINOR_VERSION_ARB, 3,
                GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
                0
            };

            if (createContextAttribs)
                glState.glxContext = createContextAttribs(display, glState.fbConfig, nullptr, True, contextAttribs);

            if (!glState.glxContext)
                glState.glxContext = glXCreateNewContext(display, glState.fbConfig, GLX_RGBA_TYPE, nullptr, True);

            if (!glState.glxContext)
                throw std::runtime_error("[ OpenGL ] - Failed to create GLX context");
            glState.ownsContext = true;
        }

        PlatformGL::PlatformGLContext finalCtx{};
        finalCtx.display = display;
        finalCtx.drawable = glState.drawable;
        finalCtx.context = glState.glxContext;

        PlatformGL::ScopedContext contextGuard{ finalCtx };
        if (!contextGuard.ok())
            throw std::runtime_error("[ OpenGL ] - glXMakeCurrent failed");

        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(PlatformGL::get_proc_address)))
            throw std::runtime_error("[ OpenGL ] - gladLoadGLLoader failed");

        const auto* versionBytes = ::glGetString(GL_VERSION);
        if (!versionBytes)
            throw std::runtime_error("[ OpenGL ] - glGetString(GL_VERSION) returned null");

        const std::string_view version{ reinterpret_cast<const char*>(versionBytes) };
        if (version.empty())
            throw std::runtime_error("[ OpenGL ] - GL_VERSION string is empty");

        ctx->native_window = detail::from_xwindow(glState.window);
        ctx->native_drawable = display;
        ctx->native_gl_context = glState.glxContext;

        // Keep the context current through init so pipeline creation has
        // a valid active GL context on this thread.
#else
        (void)parentWindowOpaque;
        throw std::runtime_error("[ OpenGL ] - Unsupported platform");
#endif

        if (!epochnamespace::openglquad::ensure_quad_pipeline(glState))
            throw std::runtime_error("[ OpenGL ] - Failed to build/ensure quad pipeline");

        atlasmanager::register_backend_uploader(core::ContextType::OpenGL,
            [](const TextureAtlas& atlas) { opengltextures::ensure_uploaded(atlas); });

        ctx->is_key_held = [](epochnamespace::input::Key k) { return epochnamespace::input::is_key_held(k); };
        ctx->is_key_down = [](epochnamespace::input::Key k) { return epochnamespace::input::is_key_down(k); };
        ctx->is_mouse_button_held = [](epochnamespace::input::MouseButton b) { return epochnamespace::input::is_mouse_button_held(b); };
        ctx->is_mouse_button_down = [](epochnamespace::input::MouseButton b) { return epochnamespace::input::is_mouse_button_down(b); };

        return true;
    }

#endif // EPOCH_USING_OPENGL
} // namespace epochnamespace::openglcontext

#endif
