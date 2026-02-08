/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
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
 // acontext.opengl.context.ixx
module;

// NOTE: Keep your engine config include if it sets global compile flags.
// Do NOT rely on it for Win32 type definitions in a module global fragment.
#include <include/aengine.config.hpp>

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
import <cstdint>;
import <format>;
import <functional>;
import <iostream>;
import <mutex>;
import <stdexcept>;
import <string>;
import <string_view>;
import <utility>;
import <vector>;

export namespace almondnamespace::openglcontext
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

        inline PlatformGL::PlatformGLContext state_to_platform_context(const almondnamespace::openglstate::OpenGL4State& state) noexcept
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
            const almondnamespace::openglstate::OpenGL4State& state) noexcept
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
            const almondnamespace::openglstate::OpenGL4State& state) noexcept
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

        auto& backend = almondnamespace::opengltextures::get_opengl_backend();
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

        // Keep it current during init; opengl_process will bind per-frame anyway.
        contextGuard.release();

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

        contextGuard.release();
#else
        (void)parentWindowOpaque;
        throw std::runtime_error("[OpenGL] Unsupported platform");
#endif

        if (!almondnamespace::openglquad::ensure_quad_pipeline())
            throw std::runtime_error("[OpenGL] Failed to build/ensure quad pipeline");

        atlasmanager::register_backend_uploader(core::ContextType::OpenGL,
            [](const TextureAtlas& atlas) { opengltextures::ensure_uploaded(atlas); });

        ctx->is_key_held = [](almondnamespace::input::Key k) { return almondnamespace::input::is_key_held(k); };
        ctx->is_key_down = [](almondnamespace::input::Key k) { return almondnamespace::input::is_key_down(k); };
        ctx->is_mouse_button_held = [](almondnamespace::input::MouseButton b) { return almondnamespace::input::is_mouse_button_held(b); };
        ctx->is_mouse_button_down = [](almondnamespace::input::MouseButton b) { return almondnamespace::input::is_mouse_button_down(b); };

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
        const auto color = core::clear_color_for_context(core::ContextType::OpenGL);
        glClearColor(color[0], color[1], color[2], color[3]);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    inline bool opengl_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        if (!ctx) return false;

        auto& backend = opengltextures::get_opengl_backend();
        auto& glState = backend.glState;

        const std::uintptr_t windowId = ctx->windowData
            ? reinterpret_cast<std::uintptr_t>(ctx->windowData->hwnd)
            : 0;

        almond::diagnostics::FrameTiming frameTimer{ core::ContextType::OpenGL, windowId, "OpenGL" };

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

        if (!almondnamespace::openglquad::ensure_quad_pipeline())
        {
            queue.drain();
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

        opengl_clear();
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

            queue.drain();
        }

        PlatformGL::swap_buffers(guard.target());

        frameTimer.finish();

        return true;
    }

    inline void opengl_cleanup(std::shared_ptr<core::Context> ctx)
    {
        auto& backend = opengltextures::get_opengl_backend();
        auto& glState = backend.glState;
        (void)ctx;

#if defined(_WIN32)
        PlatformGL::clear_current();

        if (glState.hglrc) { ::wglDeleteContext(glState.hglrc); glState.hglrc = nullptr; }
        if (glState.hdc && glState.hwnd) { ::ReleaseDC(glState.hwnd, glState.hdc); }
        glState.hdc = nullptr;

        if (glState.hwnd) { ::DestroyWindow(glState.hwnd); glState.hwnd = nullptr; }
        glState.parent = nullptr;

#elif defined(__linux__)
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
} // namespace almondnamespace::openglcontext
