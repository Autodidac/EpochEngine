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
 // acontext.opengl.context.ixx
module;

// NOTE: Keep your engine config include if it sets global compile flags.
// Do NOT rely on it for Win32 type definitions in a module global fragment.
#include "../include/aengine.config.hpp"

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
#  else
#    include <GL/wglext.h>
#  endif
#else
#  include <GL/wglext.h>
#endif

#elif defined(__linux__)
#   include <glad/glad.h>
#   include <X11/Xlib.h>
#   include <X11/Xutil.h>
#   include <GL/glx.h>
#   include <GL/glxext.h>
#endif

#include <chrono>

export module acontext.opengl.context;

// ------------------------------------------------------------
// Core engine modules
// ------------------------------------------------------------
import aengine.core.context;
import aengine.context.multiplexer;
import aengine.context.commandqueue;
import aengine.context.window;
import aengine.input;
import aplatformpump;
import aatlas.manager;
import aengine.core.commandline;
import aengine.diagnostics;
import aengine.telemetry;

// ------------------------------------------------------------
// OpenGL backend modules
// ------------------------------------------------------------
import acontext.opengl.textures;
import acontext.opengl.state;
import acontext.opengl.platform;
import acontext.opengl.quad;

// ------------------------------------------------------------
// Standard library
// ------------------------------------------------------------
import <algorithm>;
import <array>;
import <cmath>;
import <cstdint>;
import <format>;
import <functional>;
import <fstream>;
import <iostream>;
import <mutex>;
import <stdexcept>;
import <string>;
import <string_view>;
import <utility>;
import <vector>;

export namespace epochnamespace::openglcontext
{
#if !defined(ALMOND_USING_OPENGL)

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
        inline const wchar_t* gl_child_class_name() noexcept { return L"AlmondGLChild"; }

        inline void ensure_gl_child_class_registered()
        {
            static bool s_registered = false;
            if (s_registered) return;

            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = ::GetModuleHandleW(nullptr);
            wc.lpszClassName = gl_child_class_name();

            // If it already exists, RegisterClassExW will fail with ERROR_CLASS_ALREADY_EXISTS.
            if (!::RegisterClassExW(&wc))
            {
                const DWORD err = ::GetLastError();
                if (err != ERROR_CLASS_ALREADY_EXISTS)
                    throw std::runtime_error(std::format("[OpenGL] RegisterClassExW failed (err={})", err));
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

        inline PlatformGL::PlatformGLContext state_to_platform_context(const epochnamespace::openglstate::OpenGL4State& state) noexcept
        {
            PlatformGL::PlatformGLContext ctx{};
#if defined(_WIN32)
            ctx.device = state.hdc;
            ctx.context = state.hglrc;
#elif defined(__linux__)
            ctx.display = state.display;
            ctx.drawable = state.drawable ? state.drawable : state.window;
            ctx.context = state.glxContext;
#endif
            return ctx;
        }

        inline PlatformGL::PlatformGLContext context_to_platform_context(const core::Context* ctx) noexcept
        {
            PlatformGL::PlatformGLContext result{};
            if (!ctx) return result;

#if defined(_WIN32)
            result.device = static_cast<HDC>(ctx->native_drawable);
            result.context = static_cast<HGLRC>(ctx->native_gl_context);
#elif defined(__linux__)
            result.display = static_cast<Display*>(ctx->native_drawable);
            result.drawable = static_cast<GLXDrawable>(reinterpret_cast<std::uintptr_t>(ctx->native_window));
            result.context = static_cast<GLXContext>(ctx->native_gl_context);
#endif
            return result;
        }

#if defined(_WIN32) || defined(__linux__)
        struct DrawableSize
        {
            int width = 0;
            int height = 0;
            [[nodiscard]] bool valid() const noexcept { return width > 0 && height > 0; }
        };
#endif

#if defined(_WIN32)
        inline DrawableSize query_drawable_size(const core::Context* ctx,
            const epochnamespace::openglstate::OpenGL4State& state) noexcept
        {
            DrawableSize size{};
            HWND hwnd = nullptr;

            if (ctx && ctx->windowData && ctx->windowData->hwnd)
                hwnd = ctx->windowData->hwnd;
            if (!hwnd && ctx && ctx->hwnd)
                hwnd = ctx->hwnd;
            if (!hwnd)
                hwnd = state.hwnd;

            if (!hwnd)
                return size;

            RECT rc{};
            if (!::GetClientRect(hwnd, &rc))
                return size;

            size.width = (std::max)(1, static_cast<int>(rc.right - rc.left));
            size.height = (std::max)(1, static_cast<int>(rc.bottom - rc.top));
            return size;
        }
#elif defined(__linux__)
        inline DrawableSize query_drawable_size(const PlatformGL::PlatformGLContext& active,
            const epochnamespace::openglstate::OpenGL4State& state) noexcept
        {
            DrawableSize size{};
            Display* display = active.display ? active.display : state.display;
            GLXDrawable drawable = active.drawable ? active.drawable : state.drawable;

            if (!display || drawable == 0)
                return size;

            unsigned int w = 0;
            unsigned int h = 0;
            ::glXQueryDrawable(display, drawable, GLX_WIDTH, &w);
            ::glXQueryDrawable(display, drawable, GLX_HEIGHT, &h);

            size.width = static_cast<int>((std::max)(1u, w));
            size.height = static_cast<int>((std::max)(1u, h));
            return size;
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
    inline bool opengl_initialize(std::shared_ptr<core::Context> ctx,
        void* parentWindowOpaque = nullptr,
        unsigned int w = 800,
        unsigned int h = 600,
        std::function<void(int, int)> onResize = nullptr)
    {
        if (!ctx)
            throw std::runtime_error("[OpenGL] opengl_initialize requires non-null Context");

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
            throw std::runtime_error("[OpenGL] No parent HWND available");

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
                    throw std::runtime_error("[OpenGL] CreateWindowExW failed for child GL window");
            }

            glState.hdc = ::GetDC(glState.hwnd);
            if (!glState.hdc)
                throw std::runtime_error("[OpenGL] GetDC failed");

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
                if (!pf) throw std::runtime_error("[OpenGL] ChoosePixelFormat failed");
                if (!::SetPixelFormat(glState.hdc, pf, &pfd))
                    throw std::runtime_error("[OpenGL] SetPixelFormat failed");
            }

            // ---- WGL bootstrap: temp context stays current while loading + creating ----
            HGLRC tmp = ::wglCreateContext(glState.hdc);
            if (!tmp) throw std::runtime_error("[OpenGL] wglCreateContext(temp) failed");
            if (::wglMakeCurrent(glState.hdc, tmp) != TRUE)
            {
                ::wglDeleteContext(tmp);
                throw std::runtime_error("[OpenGL] wglMakeCurrent(temp) failed");
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
                    throw std::runtime_error("[OpenGL] wglMakeCurrent(final) failed");
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
            throw std::runtime_error("[OpenGL] PlatformGL::make_current(final) failed");

        // Load GL entry points with the single authoritative loader.
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(PlatformGL::get_proc_address)))
            throw std::runtime_error("[OpenGL] gladLoadGLLoader failed");

        const auto* versionBytes = ::glGetString(GL_VERSION);
        if (!versionBytes)
            throw std::runtime_error("[OpenGL] glGetString(GL_VERSION) returned null");

        const std::string_view version{ reinterpret_cast<const char*>(versionBytes) };
        if (version.empty())
            throw std::runtime_error("[OpenGL] GL_VERSION string is empty");

        ctx->native_window = glState.hwnd;
        ctx->native_drawable = glState.hdc;
        ctx->native_gl_context = glState.hglrc;

        // Keep the context current through init so pipeline creation has
        // a valid active GL context on this thread.

#elif defined(__linux__)
        (void)parentWindowOpaque;

        Display* display = glState.display ? glState.display : XOpenDisplay(nullptr);
        if (!display)
            throw std::runtime_error("[OpenGL] XOpenDisplay failed");
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
            throw std::runtime_error("[OpenGL] glXChooseFBConfig failed");

        glState.fbConfig = configs[0];
        XFree(configs);

        XVisualInfo* vi = glXGetVisualFromFBConfig(display, glState.fbConfig);
        if (!vi)
            throw std::runtime_error("[OpenGL] glXGetVisualFromFBConfig failed");

        if (!glState.colormap)
        {
            glState.colormap = XCreateColormap(display, RootWindow(display, vi->screen), vi->visual, AllocNone);
            if (!glState.colormap)
            {
                XFree(vi);
                throw std::runtime_error("[OpenGL] XCreateColormap failed");
            }
        }

        if (!glState.window)
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
                throw std::runtime_error("[OpenGL] XCreateWindow failed");
            }

            XStoreName(display, glState.window, "Almond OpenGL");
            XMapWindow(display, glState.window);
            XFlush(display);
        }

        XFree(vi);
        glState.drawable = glState.window;

        if (!glState.glxContext)
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
                throw std::runtime_error("[OpenGL] Failed to create GLX context");
        }

        PlatformGL::PlatformGLContext finalCtx{};
        finalCtx.display = display;
        finalCtx.drawable = glState.drawable;
        finalCtx.context = glState.glxContext;

        PlatformGL::ScopedContext contextGuard{ finalCtx };
        if (!contextGuard.ok())
            throw std::runtime_error("[OpenGL] glXMakeCurrent failed");

        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(PlatformGL::get_proc_address)))
            throw std::runtime_error("[OpenGL] gladLoadGLLoader failed");

        const auto* versionBytes = ::glGetString(GL_VERSION);
        if (!versionBytes)
            throw std::runtime_error("[OpenGL] glGetString(GL_VERSION) returned null");

        const std::string_view version{ reinterpret_cast<const char*>(versionBytes) };
        if (version.empty())
            throw std::runtime_error("[OpenGL] GL_VERSION string is empty");

        ctx->native_window = reinterpret_cast<void*>(static_cast<std::uintptr_t>(glState.window));
        ctx->native_drawable = display;
        ctx->native_gl_context = glState.glxContext;

        // Keep the context current through init so pipeline creation has
        // a valid active GL context on this thread.
#else
        (void)parentWindowOpaque;
        throw std::runtime_error("[OpenGL] Unsupported platform");
#endif

        if (!epochnamespace::openglquad::ensure_quad_pipeline(glState))
            throw std::runtime_error("[OpenGL] Failed to build/ensure quad pipeline");

        atlasmanager::register_backend_uploader(core::ContextType::OpenGL,
            [](const TextureAtlas& atlas) { opengltextures::ensure_uploaded(atlas); });

        ctx->is_key_held = [](epochnamespace::input::Key k) { return epochnamespace::input::is_key_held(k); };
        ctx->is_key_down = [](epochnamespace::input::Key k) { return epochnamespace::input::is_key_down(k); };
        ctx->is_mouse_button_held = [](epochnamespace::input::MouseButton b) { return epochnamespace::input::is_mouse_button_held(b); };
        ctx->is_mouse_button_down = [](epochnamespace::input::MouseButton b) { return epochnamespace::input::is_mouse_button_down(b); };

        return true;
    }

    inline void opengl_present()
    {
        // OpenGL swap is handled once per frame in opengl_process.
    }

    inline int opengl_get_width()
    {
        auto& backend = opengltextures::get_opengl_backend();
        if (backend.glState.width > 0)
            return static_cast<int>(backend.glState.width);
        return (std::max)(1, core::cli::window_width);
    }

    inline int opengl_get_height()
    {
        auto& backend = opengltextures::get_opengl_backend();
        if (backend.glState.height > 0)
            return static_cast<int>(backend.glState.height);
        return (std::max)(1, core::cli::window_height);
    }

    inline void opengl_clear()
    {
#if ALMOND_USE_CLEAR_COLOR
        const auto color = core::clear_color_for_context(core::ContextType::OpenGL);
        glClearColor(color[0], color[1], color[2], color[3]);
        glClear(GL_COLOR_BUFFER_BIT);
#endif
    }

    namespace detail
    {
        using Mat4 = std::array<float, 16>;

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

        inline void destroy_scene_preview_pipeline(epochnamespace::openglstate::OpenGL4State& state) noexcept
        {
            if (state.sceneEbo && glIsBuffer(state.sceneEbo)) glDeleteBuffers(1, &state.sceneEbo);
            if (state.sceneVbo && glIsBuffer(state.sceneVbo)) glDeleteBuffers(1, &state.sceneVbo);
            if (state.sceneVao && glIsVertexArray(state.sceneVao)) glDeleteVertexArrays(1, &state.sceneVao);
            if (state.sceneShader && glIsProgram(state.sceneShader)) glDeleteProgram(state.sceneShader);

            state.sceneShader = 0;
            state.sceneMvpLoc = -1;
            state.sceneVao = 0;
            state.sceneVbo = 0;
            state.sceneEbo = 0;
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
            throw std::runtime_error("[OpenGL] Scene preview shader compile failed: " + log);
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
            throw std::runtime_error("[OpenGL] Scene preview program link failed: " + log);
        }

        [[nodiscard]] inline Mat4 identity_matrix() noexcept
        {
            return Mat4{
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            };
        }

        [[nodiscard]] inline Mat4 multiply(const Mat4& lhs, const Mat4& rhs) noexcept
        {
            Mat4 out{};
            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                {
                    float sum = 0.0f;
                    for (int k = 0; k < 4; ++k)
                        sum += lhs[k * 4 + row] * rhs[column * 4 + k];
                    out[column * 4 + row] = sum;
                }
            }
            return out;
        }

        [[nodiscard]] inline Mat4 translation(float x, float y, float z) noexcept
        {
            Mat4 out = identity_matrix();
            out[12] = x;
            out[13] = y;
            out[14] = z;
            return out;
        }

        [[nodiscard]] inline Mat4 rotation_x(float radians) noexcept
        {
            Mat4 out = identity_matrix();
            const float c = std::cos(radians);
            const float s = std::sin(radians);
            out[5] = c;
            out[6] = s;
            out[9] = -s;
            out[10] = c;
            return out;
        }

        [[nodiscard]] inline Mat4 rotation_y(float radians) noexcept
        {
            Mat4 out = identity_matrix();
            const float c = std::cos(radians);
            const float s = std::sin(radians);
            out[0] = c;
            out[2] = -s;
            out[8] = s;
            out[10] = c;
            return out;
        }

        [[nodiscard]] inline Mat4 perspective(float fovRadians, float aspect, float zNear, float zFar) noexcept
        {
            Mat4 out{};
            const float tanHalf = std::tan(fovRadians * 0.5f);
            out[0] = 1.0f / ((std::max)(0.001f, aspect) * tanHalf);
            out[5] = 1.0f / tanHalf;
            out[10] = -(zFar + zNear) / (zFar - zNear);
            out[11] = -1.0f;
            out[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
            return out;
        }

        inline bool ensure_scene_preview_pipeline(epochnamespace::openglstate::OpenGL4State& state)
        {
            if (state.sceneShader && state.sceneVao && state.sceneVbo && state.sceneEbo)
                return true;

            destroy_scene_preview_pipeline(state);

            GLint major = 0;
            GLint minor = 0;
            glGetIntegerv(GL_MAJOR_VERSION, &major);
            glGetIntegerv(GL_MINOR_VERSION, &minor);
            if (major == 0 && minor == 0)
            {
                const auto parsed = parse_gl_version(reinterpret_cast<const char*>(glGetString(GL_VERSION)));
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

            const GLuint vertexShader = compile_scene_shader(GL_VERTEX_SHADER, vertexSource);
            GLuint fragmentShader = 0;
            try
            {
                fragmentShader = compile_scene_shader(GL_FRAGMENT_SHADER, fragmentSource);
                state.sceneShader = link_scene_program(vertexShader, fragmentShader);
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

            constexpr float vertices[] = {
                -1.0f, -1.0f, -1.0f, 0.95f, 0.35f, 0.20f,
                 1.0f, -1.0f, -1.0f, 0.95f, 0.35f, 0.20f,
                 1.0f,  1.0f, -1.0f, 0.95f, 0.35f, 0.20f,
                -1.0f,  1.0f, -1.0f, 0.95f, 0.35f, 0.20f,
                -1.0f, -1.0f,  1.0f, 0.15f, 0.75f, 0.95f,
                 1.0f, -1.0f,  1.0f, 0.15f, 0.75f, 0.95f,
                 1.0f,  1.0f,  1.0f, 0.15f, 0.75f, 0.95f,
                -1.0f,  1.0f,  1.0f, 0.15f, 0.75f, 0.95f
            };

            constexpr unsigned int indices[] = {
                4, 5, 6, 6, 7, 4,
                0, 3, 2, 2, 1, 0,
                0, 4, 7, 7, 3, 0,
                1, 2, 6, 6, 5, 1,
                3, 7, 6, 6, 2, 3,
                0, 1, 5, 5, 4, 0
            };

            glGenVertexArrays(1, &state.sceneVao);
            glGenBuffers(1, &state.sceneVbo);
            glGenBuffers(1, &state.sceneEbo);

            glBindVertexArray(state.sceneVao);
            glBindBuffer(GL_ARRAY_BUFFER, state.sceneVbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.sceneEbo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * static_cast<GLsizei>(sizeof(float)), reinterpret_cast<void*>(0));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * static_cast<GLsizei>(sizeof(float)), reinterpret_cast<void*>(3 * sizeof(float)));
            glBindVertexArray(0);

            state.sceneMvpLoc = glGetUniformLocation(state.sceneShader, "uMvp");
            return state.sceneShader != 0 && state.sceneMvpLoc >= 0;
        }

        inline void render_scene_preview(
            epochnamespace::openglstate::OpenGL4State& state,
            int framebufferWidth,
            int framebufferHeight,
            int viewportX,
            int viewportY,
            int viewportWidth,
            int viewportHeight)
        {
            if (!ensure_scene_preview_pipeline(state))
                return;

            static const auto startTime = std::chrono::steady_clock::now();
            const float timeSeconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
            const int glViewportY = (std::max)(0, framebufferHeight - (viewportY + viewportHeight));
            const float aspect = viewportHeight > 0
                ? (viewportWidth / static_cast<float>(viewportHeight))
                : 1.0f;

            const Mat4 model = multiply(rotation_y(timeSeconds * 0.85f), rotation_x(timeSeconds * 0.55f));
            const Mat4 view = translation(0.0f, 0.0f, -4.0f);
            const Mat4 projection = perspective(0.95f, aspect, 0.1f, 32.0f);
            const Mat4 mvp = multiply(projection, multiply(view, model));

            glEnable(GL_SCISSOR_TEST);
            glScissor(viewportX, glViewportY, viewportWidth, viewportHeight);
            glViewport(viewportX, glViewportY, viewportWidth, viewportHeight);
            glClearColor(0.06f, 0.08f, 0.11f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glUseProgram(state.sceneShader);
            glUniformMatrix4fv(state.sceneMvpLoc, 1, GL_FALSE, mvp.data());
            glBindVertexArray(state.sceneVao);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
            glUseProgram(0);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_SCISSOR_TEST);
            glViewport(0, 0, framebufferWidth, framebufferHeight);
        }
    }
    inline bool opengl_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        if (!ctx) return false;

        auto& backend = opengltextures::get_opengl_backend();
        auto& glState = backend.glState;

        const std::uintptr_t windowId = ctx->windowData
            ? reinterpret_cast<std::uintptr_t>(ctx->windowData->hwnd)
            : 0;

        diagnostics::FrameTiming frameTimer{ core::ContextType::OpenGL, windowId, "OpenGL" };

        PlatformGL::ScopedContext guard;
        auto desired = detail::context_to_platform_context(ctx.get());
        if (!desired.valid() || !guard.set(desired))
        {
            auto fallback = detail::state_to_platform_context(glState);
            if (!fallback.valid() || !guard.set(fallback))
                return false;
        }

        const auto previousContext = core::MultiContextManager::GetCurrent();
        core::MultiContextManager::SetCurrent(ctx);

        atlasmanager::process_pending_uploads(core::ContextType::OpenGL);

        int fbW = (std::max)(1, opengl_get_width());
        int fbH = (std::max)(1, opengl_get_height());

#if defined(_WIN32)
        if (auto size = detail::query_drawable_size(ctx.get(), glState); size.valid())
        {
            fbW = size.width;
            fbH = size.height;
        }
#elif defined(__linux__)
        if (auto size = detail::query_drawable_size(guard.target(), glState); size.valid())
        {
            fbW = size.width;
            fbH = size.height;
        }
#endif

        glState.width = static_cast<unsigned int>((std::max)(1, fbW));
        glState.height = static_cast<unsigned int>((std::max)(1, fbH));
        ctx->framebufferWidth = fbW;
        ctx->framebufferHeight = fbH;

        glViewport(0, 0, fbW, fbH);

        if (!epochnamespace::openglquad::ensure_quad_pipeline(glState))
        {
            PlatformGL::swap_buffers(guard.target());
            return true;
        }

        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(fbW),
            telemetry::RendererTelemetryTags{ core::ContextType::OpenGL, windowId, "width" });
        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(fbH),
            telemetry::RendererTelemetryTags{ core::ContextType::OpenGL, windowId, "height" });

        const auto sceneViewport = ctx->scene_viewport();
        if (sceneViewport.valid())
        {
            const int viewportX = (std::max)(0, (std::min)(sceneViewport.x, fbW - 1));
            const int viewportY = (std::max)(0, (std::min)(sceneViewport.y, fbH - 1));
            const int viewportWidth = (std::max)(1, (std::min)(sceneViewport.width, fbW - viewportX));
            const int viewportHeight = (std::max)(1, (std::min)(sceneViewport.height, fbH - viewportY));
            detail::render_scene_preview(glState, fbW, fbH, viewportX, viewportY, viewportWidth, viewportHeight);
        }
        else
        {
            opengl_clear();
        }
        const std::size_t depth = queue.depth();
        telemetry::emit_gauge(
            "renderer.command_queue.depth",
            static_cast<std::int64_t>(depth),
            telemetry::RendererTelemetryTags{ core::ContextType::OpenGL, windowId });

        {
            struct ScopedCurrentContext
            {
                std::shared_ptr<core::Context> previous;
                ~ScopedCurrentContext() { core::MultiContextManager::SetCurrent(std::move(previous)); }
            } scoped{ previousContext };

            const bool drained = queue.drain();

#if defined(_WIN32)
            static thread_local int s_debugFrames = 0;
            if (s_debugFrames < 8)
            {
                unsigned char pixel[4]{ 0, 0, 0, 0 };
                const int sampleX = (std::max)(0, fbW / 2);
                const int sampleY = (std::max)(0, fbH / 2);
                glReadPixels(sampleX, sampleY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
                std::ofstream diag("opengl_runtime_diag.txt", std::ios::app);
                diag
                    << "[OpenGL] frame hwnd=" << static_cast<void*>(ctx->windowData ? ctx->windowData->hwnd : nullptr)
                    << " hglrc=" << static_cast<void*>(guard.target().context)
                    << " fb=" << fbW << "x" << fbH
                    << " queueDepth=" << depth
                    << " drained=" << drained
                    << " centerPixel=("
                    << static_cast<int>(pixel[0]) << ","
                    << static_cast<int>(pixel[1]) << ","
                    << static_cast<int>(pixel[2]) << ","
                    << static_cast<int>(pixel[3]) << ")"
                    << "\n";
                ++s_debugFrames;
            }
#endif
        }

        PlatformGL::swap_buffers(guard.target());

        frameTimer.finish();

        return true;
    }

    inline void opengl_cleanup(std::shared_ptr<core::Context> ctx)
    {
        auto& backend = opengltextures::get_opengl_backend();
        auto& glState = backend.glState;

#if defined(_WIN32)
        {
            PlatformGL::ScopedContext cleanupGuard;
            auto cleanupContext = ctx ? detail::context_to_platform_context(ctx.get()) : PlatformGL::PlatformGLContext{};
            if (!cleanupContext.valid())
                cleanupContext = detail::state_to_platform_context(glState);

            bool canActivateCleanup = cleanupContext.valid();
            HWND cleanupHwnd = ctx && ctx->windowData && ctx->windowData->hwnd ? ctx->windowData->hwnd : glState.hwnd;
            if (cleanupHwnd && (::IsWindow(cleanupHwnd) == FALSE))
                canActivateCleanup = false;

            if (canActivateCleanup && cleanupGuard.set(cleanupContext))
            {
                detail::destroy_scene_preview_pipeline(glState);
                opengltextures::clear_gpu_atlases();
            }
        }
        PlatformGL::clear_current();

        if (glState.hglrc) { ::wglDeleteContext(glState.hglrc); glState.hglrc = nullptr; }
        if (glState.hdc && glState.hwnd) { ::ReleaseDC(glState.hwnd, glState.hdc); }
        glState.hdc = nullptr;

        if (glState.hwnd) { ::DestroyWindow(glState.hwnd); glState.hwnd = nullptr; }
        glState.parent = nullptr;

#elif defined(__linux__)
        {
            PlatformGL::ScopedContext cleanupGuard;
            auto cleanupContext = ctx ? detail::context_to_platform_context(ctx.get()) : PlatformGL::PlatformGLContext{};
            if (!cleanupContext.valid())
                cleanupContext = detail::state_to_platform_context(glState);
            if (cleanupContext.valid() && cleanupGuard.set(cleanupContext))
            {
                detail::destroy_scene_preview_pipeline(glState);
                opengltextures::clear_gpu_atlases();
            }
        }
        PlatformGL::clear_current();
        if (glState.display && glState.glxContext) glXDestroyContext(glState.display, glState.glxContext);
        if (glState.display && glState.window) XDestroyWindow(glState.display, glState.window);
        if (glState.display && glState.colormap) XFreeColormap(glState.display, glState.colormap);
        if (glState.display) XCloseDisplay(glState.display);

        glState.display = nullptr;
        glState.window = 0;
        glState.drawable = 0;
        glState.glxContext = nullptr;
        glState.colormap = 0;
        glState.fbConfig = nullptr;
#endif
    }

#endif // ALMOND_USING_OPENGL
} // namespace epochnamespace::openglcontext











