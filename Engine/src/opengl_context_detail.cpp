#include <algorithm>
#include <cstdint>

#include <include/aengine.config.hpp>

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#   include <wingdi.h>
#elif defined(__linux__)
#   include <X11/Xlib.h>
#   include <GL/glx.h>
#endif
#endif

#include "opengl_context_detail.hpp"

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import core.context;
import opengl.platform;
import opengl.state;

namespace epochnamespace::openglcontext::contextdetail
{
    PlatformGL::PlatformGLContext state_to_platform_context(const openglstate::OpenGL4State& state) noexcept
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

    PlatformGL::PlatformGLContext context_to_platform_context(const core::Context* ctx) noexcept
    {
        PlatformGL::PlatformGLContext result{};
        if (!ctx)
            return result;

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

    bool drawable_alive(const core::Context* ctx, const openglstate::OpenGL4State& state) noexcept
    {
#if defined(_WIN32)
        HWND hwnd = nullptr;
        if (ctx && ctx->windowData && ctx->windowData->hwnd)
            hwnd = ctx->windowData->hwnd;
        if (!hwnd && ctx && ctx->hwnd)
            hwnd = ctx->hwnd;
        if (!hwnd)
            hwnd = state.hwnd;
        return !hwnd || (::IsWindow(hwnd) != FALSE);
#else
        (void)ctx;
        (void)state;
        return true;
#endif
    }

    DrawableSize query_drawable_size(const core::Context* ctx, const openglstate::OpenGL4State& state) noexcept
    {
        DrawableSize size{};
#if defined(_WIN32)
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
#else
        (void)ctx;
        (void)state;
#endif
        return size;
    }

    DrawableSize query_drawable_size(
        const PlatformGL::PlatformGLContext& active,
        const openglstate::OpenGL4State& state) noexcept
    {
        DrawableSize size{};
#if defined(__linux__)
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
#else
        (void)active;
        (void)state;
#endif
        return size;
    }
}
#endif
