/**************************************************************
 *   This file is part of the Almond Project.
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell
 **************************************************************/
module;

// Global module fragment: macros + native headers only.
#include <include/aengine.config.hpp> // for ALMOND_USING Macros

#if defined(ALMOND_USING_OPENGL)

#if defined(_WIN32)

// Keep Windows macros local and consistent.
#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#   define NOMINMAX
#endif
#ifndef _WINSOCKAPI_
#   define _WINSOCKAPI_
#endif

// NOTE:
// - DO NOT include any GL headers here.
// - DO NOT include GL/wglext.h here.
// This module only needs core WGL + Win32 types and functions.
#include <aframework.hpp>
#include <wingdi.h>

#elif defined(__linux__)

#   pragma push_macro("Font")
#   define Font almondshell_X11Font
#   include <X11/Xlib.h>
#   include <GL/glx.h>
#   pragma pop_macro("Font")
#   include <dlfcn.h>
#   include <cstdint>

#endif

#endif // ALMOND_USING_OPENGL

export module acontext.opengl.platform;

import <bit>;
import <cstdint>;
import <ranges>;
import <utility>;
import <algorithm>;

#if defined(ALMOND_USING_OPENGL)

export namespace almondnamespace::openglcontext::PlatformGL
{
    struct PlatformGLContext
    {
#if defined(_WIN32)
        HDC   device = nullptr;
        HGLRC context = nullptr;
#elif defined(__linux__)
        Display* display = nullptr;
        GLXDrawable drawable = 0;
        GLXContext  context = nullptr;
#else
        void* placeholder = nullptr;
#endif

        [[nodiscard]] bool valid() const noexcept
        {
#if defined(_WIN32)
            return device != nullptr && context != nullptr;
#elif defined(__linux__)
            return display != nullptr && drawable != 0 && context != nullptr;
#else
            return false;
#endif
        }
    };

#if defined(_WIN32) || defined(__linux__)
    inline bool operator==(const PlatformGLContext& lhs, const PlatformGLContext& rhs) noexcept
    {
#if defined(_WIN32)
        return lhs.device == rhs.device && lhs.context == rhs.context;
#elif defined(__linux__)
        return lhs.display == rhs.display && lhs.drawable == rhs.drawable && lhs.context == rhs.context;
#else
        return false;
#endif
    }

    inline bool operator!=(const PlatformGLContext& lhs, const PlatformGLContext& rhs) noexcept
    {
        return !(lhs == rhs);
    }
#endif

    inline PlatformGLContext get_current() noexcept
    {
        PlatformGLContext ctx{};

#if defined(_WIN32)
        ctx.device = ::wglGetCurrentDC();
        ctx.context = ::wglGetCurrentContext();
#elif defined(__linux__)
        ctx.display = ::glXGetCurrentDisplay();
        ctx.drawable = ::glXGetCurrentDrawable();
        ctx.context = ::glXGetCurrentContext();
#else
        // unsupported
#endif
        return ctx;
    }

    inline bool make_current(const PlatformGLContext& ctx) noexcept
    {
#if defined(_WIN32)
        if (!ctx.valid())
            return ::wglMakeCurrent(nullptr, nullptr) == TRUE;
        return ::wglMakeCurrent(ctx.device, ctx.context) == TRUE;

#elif defined(__linux__)
        if (!ctx.valid())
        {
            if (auto* dpy = ::glXGetCurrentDisplay())
                return ::glXMakeCurrent(dpy, 0, nullptr) == True;
            return true;
        }
        return ::glXMakeCurrent(ctx.display, ctx.drawable, ctx.context) == True;

#else
        (void)ctx;
        return false;
#endif
    }

    inline void clear_current() noexcept
    {
#if defined(_WIN32)
        (void)::wglMakeCurrent(nullptr, nullptr);
#elif defined(__linux__)
        if (auto* dpy = ::glXGetCurrentDisplay())
            (void)::glXMakeCurrent(dpy, 0, nullptr);
#endif
    }

    inline void swap_buffers(const PlatformGLContext& ctx) noexcept
    {
#if defined(_WIN32)
        if (ctx.device) (void)::SwapBuffers(ctx.device);
#elif defined(__linux__)
        if (ctx.display && ctx.drawable) ::glXSwapBuffers(ctx.display, ctx.drawable);
#else
        (void)ctx;
#endif
    }

    inline void* get_proc_address(const char* name) noexcept
    {
        if (!name || *name == '\0') return nullptr;

#if defined(_WIN32)
        // wglGetProcAddress handles extensions but may return sentinel values.
        const auto wglProc = ::wglGetProcAddress(name);
        if (wglProc)
        {
            const auto addr = std::bit_cast<std::uintptr_t>(wglProc);
            constexpr std::uintptr_t kSentinelValues[] = {
                0u, 1u, 2u, 3u, static_cast<std::uintptr_t>(-1)
            };

            const bool isSentinel = std::ranges::any_of(
                kSentinelValues,
                [addr](std::uintptr_t s) noexcept { return addr == s; });

            if (!isSentinel)
                return reinterpret_cast<void*>(wglProc);
        }

        static HMODULE opengl32 = ::GetModuleHandleW(L"opengl32.dll");
        if (!opengl32) opengl32 = ::LoadLibraryW(L"opengl32.dll");
        if (!opengl32) return nullptr;

        const auto coreProc = ::GetProcAddress(opengl32, name);
        return reinterpret_cast<void*>(coreProc);

#elif defined(__linux__)
        // glXGetProcAddressARB returns function pointer for extensions
        if (auto p = ::glXGetProcAddressARB(reinterpret_cast<const unsigned char*>(name)))
            return reinterpret_cast<void*>(p);

        static void* libGL = ::dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
        return libGL ? ::dlsym(libGL, name) : nullptr;

#else
        return nullptr;
#endif
    }

    class ScopedContext
    {
    public:
        ScopedContext() = default;
        explicit ScopedContext(const PlatformGLContext& target) { set(target); }

        ScopedContext(const ScopedContext&) = delete;
        ScopedContext& operator=(const ScopedContext&) = delete;

        ScopedContext(ScopedContext&& other) noexcept { move_from(other); }
        ScopedContext& operator=(ScopedContext&& other) noexcept
        {
            if (this != &other) { release(); move_from(other); }
            return *this;
        }

        ~ScopedContext() { release(); }

        bool set(const PlatformGLContext& target) noexcept
        {
            release();
            target_ = target;

            if (!target.valid()) { success_ = false; return false; }

            previous_ = get_current();
            if (previous_ == target_) { success_ = true; switched_ = false; return true; }

            switched_ = make_current(target_);
            success_ = switched_;
            return success_;
        }

        void release() noexcept
        {
            if (switched_)
            {
                if (previous_.valid()) (void)make_current(previous_);
                else clear_current();
            }
            switched_ = false;
            success_ = false;
            previous_ = {};
            target_ = {};
        }

        [[nodiscard]] bool success()  const noexcept { return success_; }
        [[nodiscard]] bool switched() const noexcept { return switched_; }
        [[nodiscard]] bool ok() const noexcept { return success_; }
        [[nodiscard]] const PlatformGLContext& target() const noexcept { return target_; }

    private:
        void move_from(ScopedContext& other) noexcept
        {
            previous_ = other.previous_;
            target_ = other.target_;
            switched_ = other.switched_;
            success_ = other.success_;

            other.switched_ = false;
            other.success_ = false;
            other.previous_ = {};
            other.target_ = {};
        }

        PlatformGLContext previous_{};
        PlatformGLContext target_{};
        bool switched_ = false;
        bool success_ = false;
    };
}

#endif // ALMOND_USING_OPENGL
