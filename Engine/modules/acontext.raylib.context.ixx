/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝
 *
 *   AlmondShell – Raylib Context (Portable)
 **************************************************************/

module;

#include <include/aengine.config.hpp> // for ALMOND_USING Macros

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif

// Win32 has BOOL CloseWindow(HWND). Raylib has void CloseWindow(void).
// Prevent the collision in this TU by temporarily renaming Win32's symbol name during header include.
//#   define CloseWindow CloseWindow_Win32
#   include <include/aframework.hpp>
//#   undef CloseWindow

#   include <wingdi.h> // HGLRC + wgl*
#endif

export module acontext.raylib.context;

import aengine.core.context;
import aengine.core.commandline;
import aengine.core.logger;
import aengine.context.multiplexer;
import aengine.diagnostics;
import aatlas.manager;

import acontext.raylib.state;
import acontext.raylib.textures;
import acontext.raylib.renderer;
import acontext.raylib.input;
import acontext.raylib.api;

import <algorithm>;
import <cmath>;
import <cstdint>;
import <chrono>;
import <format>;
import <functional>;
import <memory>;
import <string>;
import <string_view>;
import <thread>;
import <utility>;


#if defined(ALMOND_USING_RAYLIB)

namespace almondnamespace::raylibcontext
{
    using NativeWindowHandle = void*;

    inline std::string& title_storage()
    {
        static std::string s_title = "AlmondShell";
        return s_title;
    }

#if defined(_WIN32)
    namespace detail
    {
        [[nodiscard]] inline HGLRC current_context() noexcept { return ::wglGetCurrentContext(); }
        [[nodiscard]] inline HDC   current_dc() noexcept { return ::wglGetCurrentDC(); }

        inline bool make_current(HDC dc, HGLRC rc) noexcept
        {
            return ::wglMakeCurrent(dc, rc) != FALSE;
        }

        inline void clear_current() noexcept
        {
            (void)::wglMakeCurrent(nullptr, nullptr);
        }

        [[nodiscard]] inline bool contexts_match(HDC a_dc, HGLRC a_rc, HDC b_dc, HGLRC b_rc) noexcept
        {
            return a_dc == b_dc && a_rc == b_rc;
        }

        // Debug helper: verify the multiplexer has made the raylib context current
        inline void debug_expect_raylib_current(const almondnamespace::raylibstate::RaylibState& st, const char* where)
        {
#if defined(_DEBUG)
            const auto dc = current_dc();
            const auto rc = current_context();
            if (dc != st.hdc || rc != st.hglrc)
            {
                logger::warn(
                    "Raylib",
                    std::format("raylib context not current at {} (current dc/rc != raylib dc/rc)", where));
            }
#else
            (void)st; (void)where;
#endif
        }


        // If the raylib window is docked as a WS_CHILD, promote it to a top-level window
        // before letting raylib destroy it. This avoids edge cases where the dock host or
        // its thread is already tearing down, which can deadlock inside DestroyWindow.
        inline void promote_raylib_to_top_level(HWND hwnd) noexcept
        {
            if (!hwnd || ::IsWindow(hwnd) == FALSE)
                return;

            const LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
            const bool isChild = (style & WS_CHILD) != 0;
            const HWND parent = ::GetParent(hwnd);

            if (!isChild && !parent)
                return;

            RECT rc{};
            ::GetWindowRect(hwnd, &rc);

            // Detach from any parent and restore overlapped style.
            ::SetParent(hwnd, nullptr);

            LONG_PTR newStyle = style;
            newStyle &= ~static_cast<LONG_PTR>(WS_CHILD);
            newStyle |= static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW | WS_VISIBLE);

            ::SetWindowLongPtrW(hwnd, GWL_STYLE, newStyle);
            ::SetWindowPos(hwnd,
                HWND_TOP,
                rc.left,
                rc.top,
                (std::max)(1, static_cast<int>(rc.right - rc.left)),
                (std::max)(1, static_cast<int>(rc.bottom - rc.top)),
                SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }

        inline void adopt_raylib_window(
            almondnamespace::raylibstate::RaylibState& st,
            std::shared_ptr<core::Context> ctx,
            HWND parent,
            HWND raylibHwnd)
        {
            if (!raylibHwnd)
            {
                logger::warn("Raylib", "Raylib returned a null window handle.");
                return;
            }

            HWND dockParent = parent;
            if (parent && parent != raylibHwnd)
            {
                if (const HWND hostParent = ::GetParent(parent); hostParent && hostParent != raylibHwnd)
                    dockParent = hostParent;
            }

            st.parent = dockParent;
            st.hwnd = raylibHwnd;

            bool attachedToDock = false;
            if (dockParent && dockParent != raylibHwnd)
            {
                if (::GetParent(raylibHwnd) != dockParent)
                {
                    ::SetParent(raylibHwnd, dockParent);
                }
                attachedToDock = true;

                LONG_PTR style = ::GetWindowLongPtrW(raylibHwnd, GWL_STYLE);
                style &= ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW);
                style |= (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
                ::SetWindowLongPtrW(raylibHwnd, GWL_STYLE, style);
                ::SetWindowPos(raylibHwnd,
                    nullptr,
                    0,
                    0,
                    static_cast<int>(st.width),
                    static_cast<int>(st.height),
                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

                almondnamespace::core::MakeDockable(raylibHwnd, dockParent);

                if (attachedToDock && parent && parent != dockParent)
                {
                    ::ShowWindow(parent, SW_HIDE);
                    ::DestroyWindow(parent);
                }
            }

            if (ctx)
            {
                const HWND previousHost = ctx->windowData ? ctx->windowData->hwnd : nullptr;
                ctx->hwnd = raylibHwnd;
                ctx->native_window = raylibHwnd;
                if (ctx->windowData)
                {
                    ctx->windowData->hwnd = raylibHwnd;
                    ctx->windowData->host_hwnd = previousHost ? previousHost : parent;
                    ctx->windowData->hwndChild = raylibHwnd;
                }
            }
        }

    }
#endif

    namespace detail
    {
        inline void ensure_frame_started(almondnamespace::raylibstate::RaylibState& st)
        {
            if (st.frameActive)
                return;

            almondnamespace::raylib_api::begin_drawing();
            st.frameActive = true;
            st.frameInTextureMode = false;
        }
    }

    export inline bool raylib_initialize(
        std::shared_ptr<core::Context> ctx,
        NativeWindowHandle parent = nullptr,
        unsigned width = 0,
        unsigned height = 0,
        std::function<void(int, int)> resizeCallback = nullptr,
        std::string title = {})
    {
        auto& st = almondnamespace::raylibstate::s_raylibstate;

        if (width == 0)  width = static_cast<unsigned>(core::cli::window_width);
        if (height == 0) height = static_cast<unsigned>(core::cli::window_height);

        st.width = (std::max)(1u, width);
        st.height = (std::max)(1u, height);

        if (!title.empty())
            title_storage() = std::move(title);

        // Prevent re-initializing raylib (it initializes global state once).
        if (st.running)
        {
            return true;
        }

#if defined(_WIN32)
        // Capture whatever context the multiplexer/docking currently has bound.
        const HDC   previousDC = detail::current_dc();
        const HGLRC previousContext = detail::current_context();
#endif

        st.owner_ctx = ctx.get();
        st.owner_thread = std::this_thread::get_id();
        st.userResize = std::move(resizeCallback);

#if defined(_WIN32)
        st.hwnd = static_cast<HWND>(parent ? parent : (ctx ? ctx->hwnd : nullptr));
        st.hdc = ctx ? ctx->hdc : detail::current_dc();
        st.hglrc = ctx ? ctx->hglrc : detail::current_context();
        st.ownsDC = false;

        if (!st.hdc || !st.hglrc)
        {
            logger::warn("Raylib", "Missing host OpenGL context; raylib will create its own.");
        }
#else
        st.hwnd = parent ? parent : (ctx ? ctx->hwnd : nullptr);
#endif

#if defined(_WIN32)
        if (st.hdc && st.hglrc &&
            !detail::contexts_match(previousDC, previousContext, st.hdc, st.hglrc))
        {
            (void)detail::make_current(st.hdc, st.hglrc);
        }
#endif

        almondnamespace::raylib_api::set_config_flags(
            static_cast<unsigned>(almondnamespace::raylib_api::flag_msaa_4x_hint));

        almondnamespace::raylib_api::init_window(
            static_cast<int>(st.width),
            static_cast<int>(st.height),
            title_storage().c_str());

#if defined(_WIN32)
        // Raylib creates its own OpenGL context. Capture it now so we bind the right dc/rc later.
        st.hdc = detail::current_dc();
        st.hglrc = detail::current_context();
        if (!st.hdc || !st.hglrc)
        {
            logger::warn("Raylib", "Failed to capture Raylib OpenGL context after initialization.");
            st.running = false;
            st.cleanupIssued = false;
            return false;
        }
        if (ctx && ctx->windowData)
        {
            ctx->windowData->hdc = st.hdc;
            ctx->windowData->glContext = st.hglrc;
            ctx->windowData->usesSharedContext = false;
        }
#if defined(_DEBUG)
        logger::info(
            "Raylib",
            std::format(
                "Updated raylib GL context dc={:p} rc={:p}",
                static_cast<const void*>(st.hdc),
                static_cast<const void*>(st.hglrc)));
#endif

        const HWND raylibHwnd = static_cast<HWND>(almondnamespace::raylib_api::get_window_handle());
        const HWND parentHwnd = static_cast<HWND>(parent);
        HWND adoptedHwnd = raylibHwnd;
        for (int attempt = 0; attempt < 5 && !adoptedHwnd; ++attempt)
        {
            almondnamespace::raylib_api::begin_drawing();
            almondnamespace::raylib_api::end_drawing();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            adoptedHwnd = static_cast<HWND>(almondnamespace::raylib_api::get_window_handle());
        }
        if (adoptedHwnd)
        {
            detail::adopt_raylib_window(st, ctx, parentHwnd, adoptedHwnd);
        }
        else if (parentHwnd)
        {
            logger::warn("Raylib", "Failed to acquire Raylib window handle for docking.");
        }
#endif

        almondnamespace::raylib_api::set_target_fps(0);

        st.onResize = [](int w, int h)
            {
                auto& state = almondnamespace::raylibstate::s_raylibstate;
                const int clampedW = (std::max)(1, w);
                const int clampedH = (std::max)(1, h);
                state.width = static_cast<unsigned>(clampedW);
                state.height = static_cast<unsigned>(clampedH);

                //   (void)raylib_make_current();
#if defined(_WIN32)
                detail::debug_expect_raylib_current(state, "raylib_resize");
#endif

                if (state.userResize)
                    state.userResize(clampedW, clampedH);
            };

#if defined(_WIN32)
        // Restore previous GL binding for the dock/multiplexer host.
        if (!detail::contexts_match(previousDC, previousContext, st.hdc, st.hglrc))
        {
            if (previousDC && previousContext)
                (void)detail::make_current(previousDC, previousContext);
            else
                detail::clear_current();
        }
#endif

        if (ctx)
            ctx->onResize = st.onResize;

        st.running = true;
        st.cleanupIssued = false;

        almondnamespace::atlasmanager::register_backend_uploader(
            almondnamespace::core::ContextType::RayLib,
            almondnamespace::raylibtextures::ensure_uploaded);

        return true;
    }

    export inline bool raylib_make_current() noexcept
    {
#if defined(_WIN32)
        auto& st = almondnamespace::raylibstate::s_raylibstate;
        if (!st.hdc || !st.hglrc)
            return false;

        return detail::make_current(st.hdc, st.hglrc);
#else
        return true;
#endif
    }

    namespace detail
    {
        void raylib_cleanup_owner_thread(almondnamespace::core::Context* ctx);
    }

    export inline void raylib_process()
    {
        auto& st = almondnamespace::raylibstate::s_raylibstate;
        if (st.cleanupRequested)
        {
            detail::raylib_cleanup_owner_thread(st.owner_ctx);
            return;
        }
        if (!st.running)
            return;

        const std::uintptr_t windowId = st.hwnd
            ? reinterpret_cast<std::uintptr_t>(st.hwnd)
            : (st.owner_ctx && st.owner_ctx->windowData
                ? reinterpret_cast<std::uintptr_t>(st.owner_ctx->windowData->hwnd)
                : 0);

        almond::diagnostics::FrameTiming frameTimer{
            almondnamespace::core::ContextType::RayLib,
            windowId,
            "Raylib"
        };

#if defined(_WIN32)
        if (!raylib_make_current())
        {
            logger::warn(
                "Raylib",
                "Failed to make raylib context current during process; shutting down.");
            st.running = false;
            return;
        }
#endif

        if (almondnamespace::raylib_api::window_should_close())
        {
            st.running = false;
            return;
        }

#if defined(_WIN32)
        // Expect the multiplexer to have activated this backend before calling process.
        detail::debug_expect_raylib_current(st, "raylib_process");
#endif

        frameTimer.finish();
    }

    export inline void raylib_idle_frame()
    {
        auto& st = almondnamespace::raylibstate::s_raylibstate;
        if (!st.running)
            return;

#if defined(_WIN32)
        if (!raylib_make_current())
            return;
#endif

        if (!st.frameActive)
        {
            almondnamespace::raylib_api::begin_drawing();
            st.frameActive = true;
            st.frameInTextureMode = false;
        }

        almondnamespace::raylib_api::end_drawing();
        st.frameActive = false;
        st.frameInTextureMode = false;

#if defined(_WIN32)
        detail::clear_current();
#endif
    }

    export inline void raylib_clear(float r, float g, float b, float a)
    {
        (void)r;
        (void)g;
        (void)b;
        (void)a;

        auto& st = almondnamespace::raylibstate::s_raylibstate;
        if (!st.running)
            return;

#if defined(_WIN32)
        if (!raylib_make_current())
            return;
#endif

        detail::ensure_frame_started(st);
        const auto clearColor = core::clear_color_for_context(core::ContextType::RayLib);
        almondnamespace::raylib_api::clear_background(
            almondnamespace::raylib_api::Color{
                static_cast<unsigned char>(std::clamp(clearColor[0], 0.0f, 1.0f) * 255.0f),
                static_cast<unsigned char>(std::clamp(clearColor[1], 0.0f, 1.0f) * 255.0f),
                static_cast<unsigned char>(std::clamp(clearColor[2], 0.0f, 1.0f) * 255.0f),
                static_cast<unsigned char>(std::clamp(clearColor[3], 0.0f, 1.0f) * 255.0f)
            });
    }

    export inline void raylib_present()
    {
        auto& st = almondnamespace::raylibstate::s_raylibstate;
        if (!st.running)
            return;

        if (!raylib_make_current())
            return;
        if (st.frameActive)
        {
            if (st.frameInTextureMode)
                almondnamespace::raylib_api::end_texture_mode();
            else
                almondnamespace::raylib_api::end_drawing();
            st.frameActive = false;
            st.frameInTextureMode = false;
        }

#if defined(_WIN32)
        detail::clear_current();
#endif
    }

    namespace detail
    {
        inline void raylib_cleanup_owner_thread(almondnamespace::core::Context* ctx)
        {
            auto& st = almondnamespace::raylibstate::s_raylibstate;

#if defined(_WIN32)
            const HDC   previousDC = detail::current_dc();
            const HGLRC previousContext = detail::current_context();
            const bool window_alive = (st.hwnd != nullptr) && (::IsWindow(st.hwnd) != FALSE);
#endif

            almondnamespace::atlasmanager::unregister_backend_uploader(
                almondnamespace::core::ContextType::RayLib);

#if defined(_WIN32)
            (void)raylib_make_current();
#endif

            almondnamespace::raylibtextures::shutdown_current_context_backend();
            if (st.frameActive)
            {
                if (st.frameInTextureMode)
                    almondnamespace::raylib_api::end_texture_mode();
                else
                    almondnamespace::raylib_api::end_drawing();
                st.frameActive = false;
                st.frameInTextureMode = false;
            }

            if (st.offscreen.id != 0)
            {
                almondnamespace::raylib_api::unload_render_texture(st.offscreen);
                st.offscreen = {};
                st.offscreenWidth = 0;
                st.offscreenHeight = 0;
            }

            if (ctx && ctx->windowData)
            {
#if defined(_WIN32)
                ctx->windowData->hdc = nullptr;
                ctx->windowData->glContext = nullptr;
                ctx->windowData->usesSharedContext = false;
#endif
            }

            if (almondnamespace::raylib_api::is_window_ready())
            {
#if defined(_WIN32)
                if (window_alive)
                {
                    // If docked as child, detach to top-level before closing to avoid teardown deadlocks.
                    detail::promote_raylib_to_top_level(st.hwnd);
                    almondnamespace::raylib_api::close_window();
                }
#else
                almondnamespace::raylib_api::close_window();
#endif
            }

#if defined(_WIN32)
            if (st.ownsDC && st.hdc && st.hwnd)
                ::ReleaseDC(st.hwnd, st.hdc);

            if (!detail::contexts_match(previousDC, previousContext, st.hdc, st.hglrc))
            {
                if (previousDC && previousContext)
                    (void)detail::make_current(previousDC, previousContext);
                else
                    detail::clear_current();
            }
#endif

            st = {};
        }
    }

    export inline void raylib_cleanup(std::shared_ptr<core::Context> ctx)
    {
        auto& st = almondnamespace::raylibstate::s_raylibstate;
        if (st.cleanupIssued)
            return;

#if defined(_WIN32)
        const bool on_owner_thread = (st.owner_thread == std::this_thread::get_id());
#else
        const bool on_owner_thread = true;
#endif

        st.cleanupIssued = true;
        st.running = false;

        if (!on_owner_thread)
        {
            st.cleanupRequested = true;
            return;
        }

        detail::raylib_cleanup_owner_thread(ctx.get());
    }

    export inline void raylib_set_window_title(std::string_view title)
    {
        title_storage().assign(title.begin(), title.end());
        almondnamespace::raylib_api::set_window_title(title_storage().c_str());
    }

    export inline int raylib_get_width()
    {
        return static_cast<int>(almondnamespace::raylibstate::s_raylibstate.width);
    }

    export inline int raylib_get_height()
    {
        return static_cast<int>(almondnamespace::raylibstate::s_raylibstate.height);
    }

    export inline almondnamespace::raylib_api::Vector2 raylib_get_mouse_position()
    {
        return almondnamespace::raylib_api::get_mouse_position();
    }

    export inline NativeWindowHandle raylib_get_native_window()
    {
        return almondnamespace::raylibstate::s_raylibstate.hwnd;
    }
}

#endif // ALMOND_USING_RAYLIB
