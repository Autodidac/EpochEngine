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
 //
 // aengine.context.multiplexer.win.cpp  (TU implementation; NOT a module partition)
 //

#include <include/aengine.config.hpp>

#if defined(_WIN32)
#   ifdef ALMOND_USING_WINMAIN
#       include <include/aframework.hpp>
#   endif
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windowsx.h>
#   include <commctrl.h>
#   include <shellapi.h>
#   pragma comment(lib, "comctl32.lib")

#   include <algorithm>
#   include <chrono>
#   include <cstdint>
#   include <format>
#   include <functional>
#   include <iostream>
#   include <memory>
#   include <shared_mutex>
#   include <source_location>
#   include <stdexcept>
#   include <string>
#   include <string_view>
#   include <thread>
#   include <unordered_map>
#   include <utility>
#   include <vector>

#   include <glad/glad.h>
#endif

import aengine.platform;
import autility.string.converter;

import aengine.cli;
import aengine.core.context;
import aengine.core.logger;

import aengine.context.commandqueue;
import aengine.context.multiplexer;
import aengine.context.type;
import aengine.context.window;
import aengine.telemetry;

#if defined(ALMOND_USING_OPENGL)
import acontext.opengl.context;
#endif
#if defined(ALMOND_USING_SOFTWARE_RENDERER)
import acontext.softrenderer.context;
#endif
#if defined(ALMOND_USING_SFML)
import acontext.sfml.context;
#endif
#if defined(ALMOND_USING_RAYLIB)
import acontext.raylib.context;
#endif
#if defined(ALMOND_USING_SDL)
import acontext.sdl.context;
#endif

#if defined(_WIN32)

namespace
{
    // TU-owned globals.
    std::unordered_map<HWND, std::thread> g_threads;
    almondnamespace::core::DragState       g_drag;
    struct PendingWindowCleanup
    {
        HWND hwnd{};
        std::thread thread{};
        std::unique_ptr<almondnamespace::core::WindowData> window{};
    };
    std::vector<PendingWindowCleanup> g_pendingCleanups;
    constexpr std::string_view kLogSys = "Context.Multiplexer.Win";

   // [[nodiscard]] inline int clamp_positive(int v) noexcept { return (v < 1) ? 1 : v; }

#if ALMOND_SINGLE_PARENT
    struct SubCtx { HWND originalParent{}; };

    // Dock/undock requests must be processed on the window's owning thread.
    // GLFW/raylib windows are owned by the thread that created them (typically the render thread).
    // Cross-thread SetParent/SetWindowLongPtr/SetWindowPos can deadlock.
    constexpr UINT WM_ALMOND_DOCKCMD = WM_APP + 0x4A11;
    enum class DockCmd : WPARAM
    {
        Undock = 1,
    };

    [[nodiscard]] inline int clamp_positive(int v) noexcept { return (v < 1) ? 1 : v; }

    LRESULT CALLBACK DockableProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR dw)
    {
        auto* ctx = reinterpret_cast<SubCtx*>(dw);

        switch (msg)
        {
        case WM_NCDESTROY:
            delete ctx;
            return DefSubclassProc(hwnd, msg, wp, lp);

        case WM_ALMOND_DOCKCMD:
        {
            if (static_cast<DockCmd>(wp) == DockCmd::Undock)
            {
                // Convert to a top-level window, preserving client size.
                RECT clientRect{};
                ::GetClientRect(hwnd, &clientRect);
                const int clientW = clamp_positive(static_cast<int>(clientRect.right - clientRect.left));
                const int clientH = clamp_positive(static_cast<int>(clientRect.bottom - clientRect.top));

                LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
                style &= ~WS_CHILD;
                style |= WS_OVERLAPPEDWINDOW | WS_VISIBLE;
                ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);

                const DWORD exStyle = static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
                RECT adjusted{ 0, 0, clientW, clientH };
                if (::AdjustWindowRectEx(&adjusted, static_cast<DWORD>(style), FALSE, exStyle))
                {
                    const int wndW = clamp_positive(adjusted.right - adjusted.left);
                    const int wndH = clamp_positive(adjusted.bottom - adjusted.top);
                    ::SetParent(hwnd, nullptr);
                    ::SetWindowPos(hwnd, nullptr, 0, 0, wndW, wndH,
                        SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                }
                else
                {
                    ::SetParent(hwnd, nullptr);
                    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                }
                return 0;
            }
            break;
        }

        case WM_CLOSE:
        {
            // If this is a GLFW-owned HWND (raylib), do NOT DestroyWindow here.
            // Let GLFW/raylib handle the close.
            wchar_t cls[64]{};
            ::GetClassNameW(hwnd, cls, static_cast<int>(std::size(cls)));
            if (wcsncmp(cls, L"GLFW", 4) == 0)
                return DefSubclassProc(hwnd, msg, wp, lp);

            ::DestroyWindow(hwnd);
            return 0;
        }

        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE:
        case WM_LBUTTONUP:
            return almondnamespace::core::MultiContextManager::ChildProc(hwnd, msg, wp, lp);
        }

        return DefSubclassProc(hwnd, msg, wp, lp);
    }
#endif

    inline void cleanup_window_resources(std::unique_ptr<almondnamespace::core::WindowData>& window) noexcept
    {
#if defined(ALMOND_USING_OPENGL)
        if (window && window->glContext)
        {
            ::wglMakeCurrent(nullptr, nullptr);
            ::wglDeleteContext(window->glContext);
        }
#endif
        if (window && window->hdc && window->hwnd)
            ::ReleaseDC(window->hwnd, window->hdc);
    }

    [[nodiscard]] inline bool thread_finished(std::thread& thread) noexcept
    {
        if (!thread.joinable()) return true;
        HANDLE handle = static_cast<HANDLE>(thread.native_handle());
        if (!handle) return false;
        return ::WaitForSingleObject(handle, 0) == WAIT_OBJECT_0;
    }
}

namespace almondnamespace::core
{
#if defined(ALMOND_USING_RAYLIB)
    // Raylib embeds a real GLFW-created HWND. Re-parenting must be performed on the
    // thread that owns the host HWND, otherwise Win32 can deadlock via cross-thread
    // synchronous messages during SetParent/SetWindowPos.
#endif
    // ------------------------------------------------------------
    // Exported helpers (declared in the interface module)
    // ------------------------------------------------------------
    std::unordered_map<HWND, std::thread>& Threads() noexcept { return g_threads; }
    DragState& Drag() noexcept { return g_drag; }

    void MakeDockable(HWND hwnd, HWND parent)
    {
#if ALMOND_SINGLE_PARENT
        (void)parent;
        if (!hwnd) return;
        auto* ctx = new SubCtx{ parent };
        if (!::SetWindowSubclass(hwnd, DockableProc, 1, reinterpret_cast<DWORD_PTR>(ctx)))
            delete ctx;
#else
        (void)hwnd;
        (void)parent;
#endif
    }

    namespace backend
    {
        std::wstring_view BackendDisplayName(ContextType type) noexcept
        {
            switch (type)
            {
            case ContextType::OpenGL:   return L"OpenGL";
            case ContextType::SDL:      return L"SDL";
            case ContextType::SFML:     return L"SFML";
            case ContextType::RayLib:   return L"Raylib";
            case ContextType::Software: return L"Software";
            case ContextType::Vulkan:   return L"Vulkan";
            case ContextType::DirectX:  return L"DirectX";
            case ContextType::Noop:     return L"Noop";
            case ContextType::Custom:   return L"Custom";
            default:                    return L"Context";
            }
        }

        std::wstring BuildChildWindowTitle(ContextType type, int index)
        {
            const std::wstring_view base = BackendDisplayName(type);
            std::wstring title{ base.begin(), base.end() };
            title += L" Dock ";
            title += std::to_wstring(static_cast<long long>(index) + 1);
            return title;
        }

        void ResolveClientSize(HWND hwnd, int& width, int& height) noexcept
        {
            if (!hwnd) return;
            RECT client{};
            if (!::GetClientRect(hwnd, &client)) return;
            width = clamp_positive(static_cast<int>(client.right - client.left));
            height = clamp_positive(static_cast<int>(client.bottom - client.top));
        }
    }

    // ------------------------------------------------------------
    // Window lookup
    // ------------------------------------------------------------
    WindowData* MultiContextManager::findWindowByHWND(HWND hwnd)
    {
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [hwnd](const std::unique_ptr<WindowData>& w) { return w && w->hwnd == hwnd; });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    const WindowData* MultiContextManager::findWindowByHWND(HWND hwnd) const
    {
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [hwnd](const std::unique_ptr<WindowData>& w) { return w && w->hwnd == hwnd; });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    WindowData* MultiContextManager::findWindowByContext(const std::shared_ptr<Context>& ctx)
    {
        if (!ctx) return nullptr;
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [&](const std::unique_ptr<WindowData>& w) { return w && w->context && w->context.get() == ctx.get(); });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    const WindowData* MultiContextManager::findWindowByContext(const std::shared_ptr<Context>& ctx) const
    {
        if (!ctx) return nullptr;
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [&](const std::unique_ptr<WindowData>& w) { return w && w->context && w->context.get() == ctx.get(); });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    // ------------------------------------------------------------
    // MultiContextManager (public helpers)
    // ------------------------------------------------------------
    bool MultiContextManager::IsRunning() const noexcept
    {
        return running.load(std::memory_order_acquire);
    }

    void MultiContextManager::StopRunning() noexcept
    {
        running.store(false, std::memory_order_release);
    }

    void MultiContextManager::EnqueueRenderCommand(HWND hwnd, RenderCommand cmd)
    {
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [hwnd](const std::unique_ptr<WindowData>& w) { return w && w->hwnd == hwnd; });
        if (it != windows.end()) (*it)->EnqueueCommand(std::move(cmd));
    }

    // ------------------------------------------------------------
    // Implementation
    // ------------------------------------------------------------
    void MultiContextManager::ShowConsole()
    {
        if (::AllocConsole())
        {
            FILE* f{};
            freopen_s(&f, "CONOUT$", "w", stdout);
            freopen_s(&f, "CONIN$", "r", stdin);
            freopen_s(&f, "CONOUT$", "w", stderr);
            std::ios::sync_with_stdio(true);
        }
    }

    ATOM MultiContextManager::RegisterParentClass(HINSTANCE hInst, LPCWSTR name)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = ParentProc;
        wc.hInstance = hInst;
        wc.lpszClassName = name;
        wc.style = CS_OWNDC;
        wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        return ::RegisterClassW(&wc);
    }

    ATOM MultiContextManager::RegisterChildClass(HINSTANCE hInst, LPCWSTR name)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = ChildProc;
        wc.hInstance = hInst;
        wc.lpszClassName = name;
        wc.style = CS_OWNDC;
        wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        return ::RegisterClassW(&wc);
    }

    void MultiContextManager::SetupPixelFormat(HDC hdc)
    {
        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.iLayerType = PFD_MAIN_PLANE;

        const int pf = ::ChoosePixelFormat(hdc, &pfd);
        if (pf == 0 || !::SetPixelFormat(hdc, pf, &pfd))
            throw std::runtime_error("Failed to set pixel format");
    }

    HGLRC MultiContextManager::CreateSharedGLContext(HDC hdc)
    {
        SetupPixelFormat(hdc);
        HGLRC ctx = ::wglCreateContext(hdc);
        if (!ctx) throw std::runtime_error("Failed to create OpenGL context");

        if (sharedContext && !::wglShareLists(sharedContext, ctx))
        {
            ::wglDeleteContext(ctx);
            throw std::runtime_error("Failed to share GL context");
        }

        return ctx;
    }

    int MultiContextManager::get_title_bar_thickness(const HWND window_handle)
    {
        RECT wr{}, cr{};
        ::GetWindowRect(window_handle, &wr);
        ::GetClientRect(window_handle, &cr);

        const int totalH = wr.bottom - wr.top;
        const int clientH = cr.bottom - cr.top;
        const int totalW = wr.right - wr.left;
        const int clientW = cr.right - cr.left;

        const int nonClientH = totalH - clientH;
        const int nonClientW = totalW - clientW;

        const int border = nonClientW / 2;
        const int title = nonClientH - border * 2;
        return title;
    }

    bool MultiContextManager::Initialize(
        HINSTANCE hInst,
        int RayLibWinCount,
        int SDLWinCount,
        int SFMLWinCount,
        int OpenGLWinCount,
        int VulkanWinCount,
        int SoftwareWinCount,
        bool parented)
    {
        const int totalRequested = RayLibWinCount + SDLWinCount + SFMLWinCount + OpenGLWinCount + VulkanWinCount + SoftwareWinCount;
        if (totalRequested <= 0) return false;

        running.store(true, std::memory_order_release);
        s_activeInstance = this;

        RegisterParentClass(hInst, L"AlmondParent");
        RegisterChildClass(hInst, L"AlmondChild");

        almondnamespace::core::InitializeAllContexts();

        // ---------------- Parent (dock container) ----------------
        if (parented)
        {
            int cols = 1, rows = 1;
            while (cols * rows < totalRequested) (cols <= rows ? ++cols : ++rows);

            const int cellW = (totalRequested == 1) ? cli::window_width : 800;
            const int cellH = (totalRequested == 1) ? cli::window_height : 600;

            const int clientW = cols * cellW;
            const int clientH = rows * cellH;

            RECT want{ 0, 0, clientW, clientH };
            const DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN;
            ::AdjustWindowRect(&want, style, FALSE);

            parent = ::CreateWindowExW(
                0,
                L"AlmondParent",
                L"Almond Docking",
                style,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                want.right - want.left,
                want.bottom - want.top,
                nullptr,
                nullptr,
                hInst,
                this);

            if (!GetParentWindow()) return false;
            ::DragAcceptFiles(GetParentWindow(), TRUE);
        }
        else
        {
            parent = nullptr;
        }

#if defined(ALMOND_USING_OPENGL)
        // ---------------- Shared dummy GL context (for wglShareLists + glad bootstrap) ----------------
        {
            HWND dummy = ::CreateWindowExW(
                WS_EX_TOOLWINDOW,
                L"AlmondChild",
                L"Dummy",
                WS_POPUP,
                0, 0, 1, 1,
                nullptr,
                nullptr,
                hInst,
                nullptr);

            if (!dummy) return false;

            HDC dummyDC = ::GetDC(dummy);
            SetupPixelFormat(dummyDC);
            sharedContext = ::wglCreateContext(dummyDC);
            if (!sharedContext)
            {
                ::ReleaseDC(dummy, dummyDC);
                ::DestroyWindow(dummy);
                throw std::runtime_error("Failed to create shared OpenGL context");
            }

            ::wglMakeCurrent(dummyDC, sharedContext);

            static bool gladInitialized = false;
            if (!gladInitialized)
            {
                gladInitialized = (gladLoadGL() != 0);
                almondnamespace::logger::get(kLogSys).log(
                    almondnamespace::logger::LogLevel::WARN,
                    "GLAD loaded on dummy context",
                    std::source_location::current());
            }

            ::wglMakeCurrent(nullptr, nullptr);
            ::ReleaseDC(dummy, dummyDC);
            ::DestroyWindow(dummy);
        }
#endif

        // ---------------- Helper: create N windows for a backend ----------------
        auto make_backend_windows = [&](ContextType type, int count)
            {
                if (count <= 0) return;

                std::vector<HWND> created;
                created.reserve(static_cast<size_t>(count));
                std::vector<std::string> createdTitles;
                createdTitles.reserve(static_cast<size_t>(count));

                for (int i = 0; i < count; ++i)
                {
                    const std::wstring windowTitle = backend::BuildChildWindowTitle(type, i);
                    const std::string narrowTitle = almondnamespace::text::narrow_utf8(windowTitle);

                    HWND hwnd = ::CreateWindowExW(
                        0,
                        L"AlmondChild",
                        windowTitle.c_str(),
                        (parent
                            ? (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS)
                            : (WS_OVERLAPPEDWINDOW | WS_VISIBLE)),
                        0, 0, 800, 600,
                        parent,
                        nullptr,
                        hInst,
                        nullptr);

                    if (!hwnd) continue;

                    ::SetWindowTextW(hwnd, windowTitle.c_str());

                    HDC hdc = ::GetDC(hwnd);
                    HGLRC glrc = nullptr;
                    bool usesSharedContext = false;

#if defined(ALMOND_USING_OPENGL)
                    if (type == ContextType::OpenGL
#if defined(ALMOND_USING_RAYLIB)
                        || type == ContextType::RayLib
#endif
                        )
                    {
                        glrc = CreateSharedGLContext(hdc);
                        usesSharedContext = (glrc != nullptr);
                    }
#if defined(ALMOND_USING_VULKAN)
                    else if (type == ContextType::Vulkan)
                    {
                        // Vulkan GUI overlay uses OpenGL via a shared HGLRC when available.
                        glrc = CreateSharedGLContext(hdc);
                        usesSharedContext = (glrc != nullptr);
                    }
#endif
#endif

                    auto winPtr = std::make_unique<WindowData>(hwnd, hdc, glrc, usesSharedContext, type);
                    winPtr->running = true;
                    winPtr->titleWide = windowTitle;
                    winPtr->titleNarrow = narrowTitle;

                    if (parent) MakeDockable(hwnd, parent);

                    {
                        std::scoped_lock lock(windowsMutex);
                        windows.emplace_back(std::move(winPtr));
                    }

                    created.push_back(hwnd);
                    createdTitles.push_back(narrowTitle);
                }

                std::vector<std::shared_ptr<Context>> ctxs;

                std::shared_ptr<Context> master;
                std::vector<std::shared_ptr<Context>> free_dups;
                free_dups.reserve(static_cast<size_t>((std::max)(0, count - 1)));

                {
                    std::unique_lock lock(g_backendsMutex);
                    auto it = g_backends.find(type);
                    if (it == g_backends.end() || !it->second.master)
                    {
                        almondnamespace::logger::get(kLogSys).logf(
                            almondnamespace::logger::LogLevel::ALMOND_ERROR,
                            std::source_location::current(),
                            "Missing prototype context for backend type {}",
                            static_cast<int>(type));
                        return;
                    }

                    core::BackendState& state = it->second;
                    master = state.master;

                    for (auto& dup : state.duplicates)
                    {
                        if (dup && dup->windowData == nullptr)
                            free_dups.push_back(dup);
                    }
                }

                if (!master) return;

                ctxs.reserve(static_cast<size_t>(count));
                ctxs.push_back(master);

                while (ctxs.size() < static_cast<size_t>(count))
                {
                    if (!free_dups.empty())
                    {
                        ctxs.push_back(std::move(free_dups.back()));
                        free_dups.pop_back();
                        continue;
                    }

                    auto cloned = CloneContext(*master);
                    {
                        std::unique_lock lock(g_backendsMutex);
                        g_backends[type].duplicates.push_back(cloned);
                    }
                    ctxs.push_back(std::move(cloned));
                }

                const size_t n = (std::min)(created.size(), ctxs.size());
                for (size_t i = 0; i < n; ++i)
                {
                    HWND hwnd = created[i];
                    auto* w = findWindowByHWND(hwnd);
                    auto& ctx = ctxs[i];
                    if (!ctx) continue;

                    ctx->type = type;
                    ctx->hwnd = hwnd;

                    int width = 800;
                    int height = 600;
                    backend::ResolveClientSize(hwnd, width, height);
                    ctx->width = width;
                    ctx->height = height;

                    std::string narrowTitle;
                    if (w)
                    {
                        ctx->hdc = w->hdc;
                        ctx->hglrc = w->glContext;
                        ctx->windowData = w;
                        w->context = ctx;
                        w->width = width;
                        w->height = height;
                        w->running = true;

                        if (!ctx->onResize && w->onResize) ctx->onResize = w->onResize;
                        if (!w->titleNarrow.empty()) narrowTitle = w->titleNarrow;
                    }

                    if (narrowTitle.empty())
                    {
                        narrowTitle = (i < createdTitles.size())
                            ? createdTitles[i]
                            : almondnamespace::text::narrow_utf8(backend::BuildChildWindowTitle(type, static_cast<int>(i)));
                    }

                    if (w && w->titleNarrow.empty())
                        w->titleNarrow = narrowTitle;

                    // ---------------- Backend initialization policy ----------------
                    // OpenGL uses the placeholder HWND + wgl contexts -> safe to init now.
                    // Raylib/SDL create their own HWND/GL context internally -> MUST init on the render thread.
                    switch (type)
                    {
#if defined(ALMOND_USING_OPENGL)
                    case ContextType::OpenGL:
                        if (ctx->hdc && ctx->hglrc)
                        {
                            if (!::wglMakeCurrent(ctx->hdc, ctx->hglrc))
                            {
                                almondnamespace::logger::get(kLogSys).logf(
                                    almondnamespace::logger::LogLevel::ALMOND_ERROR,
                                    std::source_location::current(),
                                    "wglMakeCurrent failed for hwnd={}",
                                    static_cast<void*>(hwnd));
                            }
                            else
                            {
                                almondnamespace::logger::get(kLogSys).logf(
                                    almondnamespace::logger::LogLevel::WARN,
                                    std::source_location::current(),
                                    "Running OpenGL init for hwnd={}",
                                    static_cast<void*>(hwnd));
                                almondnamespace::openglcontext::opengl_initialize(
                                    ctx,
                                    hwnd,
                                    ctx->width,
                                    ctx->height,
                                    w ? w->onResize : nullptr);
                                ::wglMakeCurrent(nullptr, nullptr);
                            }
                        }
                        break;
#endif
#if defined(ALMOND_USING_SOFTWARE_RENDERER)
                    case ContextType::Software:
                        almondnamespace::logger::get(kLogSys).logf(
                            almondnamespace::logger::LogLevel::WARN,
                            std::source_location::current(),
                            "Initializing Software renderer for hwnd={}",
                            static_cast<void*>(hwnd));
                        almondnamespace::anativecontext::softrenderer_initialize(
                            ctx,
                            hwnd,
                            ctx->width,
                            ctx->height,
                            w ? w->onResize : nullptr);
                        break;
#endif
#if defined(ALMOND_USING_RAYLIB)
                    case ContextType::RayLib:
                        almondnamespace::logger::get(kLogSys).logf(
                            almondnamespace::logger::LogLevel::WARN,
                            std::source_location::current(),
                            "Deferring Raylib init to render thread. host={}",
                            static_cast<void*>(hwnd));
                        break;
#endif
#if defined(ALMOND_USING_SDL)
                    case ContextType::SDL:
                        almondnamespace::logger::get(kLogSys).logf(
                            almondnamespace::logger::LogLevel::WARN,
                            std::source_location::current(),
                            "Deferring SDL init to render thread. host={}",
                            static_cast<void*>(hwnd));
                        break;
#endif
#if defined(ALMOND_USING_VULKAN)
                    case ContextType::Vulkan:
                        almondnamespace::logger::get(kLogSys).logf(
                            almondnamespace::logger::LogLevel::WARN,
                            std::source_location::current(),
                            "Deferring Vulkan init to render thread. host={}",
                            static_cast<void*>(hwnd));
                        break;
#endif
#if defined(ALMOND_USING_SFML)
                    case ContextType::SFML:
                        almondnamespace::logger::get(kLogSys).logf(
                            almondnamespace::logger::LogLevel::WARN,
                            std::source_location::current(),
                            "Deferring SFML init to render thread. host={}",
                            static_cast<void*>(hwnd));
                        break;
#endif

                    default:
                        break;
                    }
                }
            };

#if defined(ALMOND_USING_RAYLIB)
        make_backend_windows(ContextType::RayLib, RayLibWinCount);
#endif
#if defined(ALMOND_USING_SDL)
        make_backend_windows(ContextType::SDL, SDLWinCount);
#endif
#if defined(ALMOND_USING_VULKAN)
        make_backend_windows(ContextType::Vulkan, VulkanWinCount);
#endif
#if defined(ALMOND_USING_OPENGL)
        make_backend_windows(ContextType::OpenGL, OpenGLWinCount);
#endif
#if defined(ALMOND_USING_SOFTWARE_RENDERER)
        make_backend_windows(ContextType::Software, SoftwareWinCount);
#endif
        // (void)SFMLWinCount; // place holder
#if defined(ALMOND_USING_SFML)
        make_backend_windows(ContextType::SFML, SFMLWinCount);
#endif

        ArrangeDockedWindowsGrid();
        StartRenderThreads();

        {
            std::shared_lock lock(g_backendsMutex);
            return !g_backends.empty();
        }
    }

    void MultiContextManager::AddWindow(
        HWND hwnd,
        HWND parentWnd,
        HDC hdc,
        HGLRC glContext,
        bool usesSharedContext,
        ResizeCallback onResize,
        ContextType type)
    {
        if (!hwnd) return;

        s_activeInstance = this;

        if (!hdc) hdc = ::GetDC(hwnd);

#if defined(ALMOND_USING_OPENGL)
        if ((type == ContextType::OpenGL
#if defined(ALMOND_USING_RAYLIB)
            || type == ContextType::RayLib
#endif
            ) && !glContext)
        {
            glContext = CreateSharedGLContext(hdc);
            static bool gladInitialized = false;
            if (!gladInitialized)
            {
                ::wglMakeCurrent(hdc, glContext);
                gladInitialized = (gladLoadGL() != 0);
                ::wglMakeCurrent(nullptr, nullptr);
            }
        }
#if defined(ALMOND_USING_VULKAN)
        if (type == ContextType::Vulkan && !glContext)
        {
            glContext = CreateSharedGLContext(hdc);
            usesSharedContext = (glContext != nullptr);
        }
#endif
#endif

        if (parentWnd) MakeDockable(hwnd, parentWnd);

        {
            bool needInit = false;
            {
                std::shared_lock lock(g_backendsMutex);
                needInit = g_backends.empty();
            }
            if (needInit) InitializeAllContexts();
        }

        std::shared_ptr<Context> ctx;
        {
            std::unique_lock lock(g_backendsMutex);
            auto& state = g_backends[type];

            if (!state.master)
            {
                ctx = std::make_shared<Context>();
                ctx->type = type;
                state.master = ctx;
            }
            else if (!state.master->windowData)
            {
                ctx = state.master;
            }
            else
            {
                auto it = std::find_if(state.duplicates.begin(), state.duplicates.end(),
                    [](const std::shared_ptr<Context>& dup) { return dup && !dup->windowData; });

                if (it != state.duplicates.end()) ctx = *it;
                else
                {
                    auto dup = CloneContext(*state.master);
                    state.duplicates.push_back(dup);
                    ctx = dup;
                }
            }
        }

        if (!ctx) return;

        ctx->type = type;
        ctx->hwnd = hwnd;
        ctx->hdc = hdc;
        ctx->hglrc = glContext;

        // Keep the cross-backend fields in sync.
        // Many render helpers use native_*.
        ctx->native_window = hwnd;
        ctx->native_drawable = hdc;
        ctx->native_gl_context = glContext;

        auto winPtr = std::make_unique<WindowData>(hwnd, hdc, glContext, usesSharedContext, type);
        winPtr->running = true;
        winPtr->onResize = std::move(onResize);
        winPtr->context = ctx;
        ctx->windowData = winPtr.get();

        RECT rc{};
        ::GetClientRect(hwnd, &rc);
        ctx->width = clamp_positive(static_cast<int>(rc.right - rc.left));
        ctx->height = clamp_positive(static_cast<int>(rc.bottom - rc.top));
        winPtr->width = ctx->width;
        winPtr->height = ctx->height;

        if (!ctx->onResize && winPtr->onResize) ctx->onResize = winPtr->onResize;
        if (!winPtr->onResize && ctx->onResize) winPtr->onResize = ctx->onResize;

        WindowData* rawWin = winPtr.get();
        {
            std::scoped_lock lock(windowsMutex);
            windows.emplace_back(std::move(winPtr));
        }

        auto& threads = Threads();
        if (!threads.contains(hwnd) && rawWin)
            threads[hwnd] = std::thread([this, rawWin]() { RenderLoop(*rawWin); });

        ArrangeDockedWindowsGrid();
    }

    void MultiContextManager::HandleResize(HWND hwnd, int width, int height)
    {
        if (!hwnd) return;

        int clampedWidth = clamp_positive(width);
        int clampedHeight = clamp_positive(height);
        backend::ResolveClientSize(hwnd, clampedWidth, clampedHeight);

        std::function<void(int, int)> resizeCallback;
        core::ContextType contextType = core::ContextType::None;
        std::uintptr_t windowId = 0;
        WindowData* window = nullptr;

        {
            std::scoped_lock lock(windowsMutex);
            auto it = std::find_if(windows.begin(), windows.end(),
                [hwnd](const std::unique_ptr<WindowData>& w) { return w && w->hwnd == hwnd; });
            if (it == windows.end()) return;

            window = it->get();
            window->width = clampedWidth;
            window->height = clampedHeight;

            if (window->context)
            {
                window->context->width = clampedWidth;
                window->context->height = clampedHeight;
                contextType = window->context->type;
                if (window->context->onResize) resizeCallback = window->context->onResize;
            }
            else
            {
                contextType = window->type;
            }

            if (!resizeCallback && window->onResize) resizeCallback = window->onResize;

            if (window->hwnd)
                windowId = reinterpret_cast<std::uintptr_t>(window->hwnd);
        }

        if (window)
        {
            window->commandQueue.enqueue([
                cb = std::move(resizeCallback),
                contextType,
                windowId,
                w = clampedWidth,
                h = clampedHeight
            ]() mutable
                {
                    telemetry::emit_counter(
                        "renderer.resize.count",
                        1,
                        telemetry::RendererTelemetryTags{ contextType, windowId });
                    telemetry::emit_gauge(
                        "renderer.resize.latest_dimensions",
                        w,
                        telemetry::RendererTelemetryTags{ contextType, windowId, "width" });
                    telemetry::emit_gauge(
                        "renderer.resize.latest_dimensions",
                        h,
                        telemetry::RendererTelemetryTags{ contextType, windowId, "height" });

                    if (cb) cb(w, h);
                });
        }
    }

    void MultiContextManager::StartRenderThreads()
    {
        std::vector<HWND> hwnds;
        {
            std::scoped_lock lock(windowsMutex);
            hwnds.reserve(windows.size());
            for (const auto& w : windows)
                if (w && w->hwnd) hwnds.push_back(w->hwnd);
        }

        auto& threads = Threads();

        for (HWND hwnd : hwnds)
        {
            if (threads.contains(hwnd)) continue;

            threads[hwnd] = std::thread([this, hwnd]()
                {
                    WindowData* win = nullptr;
                    {
                        std::scoped_lock lock(windowsMutex);
                        auto it = std::find_if(windows.begin(), windows.end(),
                            [hwnd](const std::unique_ptr<WindowData>& w) { return w && w->hwnd == hwnd; });
                        if (it != windows.end()) win = it->get();
                    }

                    if (win) RenderLoop(*win);
                });
        }
    }

    void MultiContextManager::RemoveWindow(HWND hwnd)
    {
        std::unique_ptr<WindowData> removed;
        bool should_quit = false;

        {
            std::scoped_lock lock(windowsMutex);
            auto it = std::find_if(windows.begin(), windows.end(),
                [hwnd](const std::unique_ptr<WindowData>& w) { return w && w->hwnd == hwnd; });
            if (it == windows.end()) return;

            (*it)->running = false;

            if ((*it)->context)
            {
                auto ctx = (*it)->context;
                if (ctx->windowData == it->get()) ctx->windowData = nullptr;
                if (ctx->hwnd == hwnd)
                {
                    ctx->hwnd = nullptr;
                    ctx->hdc = nullptr;
                    ctx->hglrc = nullptr;
                }
            }

            removed = std::move(*it);
            windows.erase(it);
            should_quit = windows.empty();
        }

        auto& threads = Threads();
        if (threads.contains(hwnd))
        {
            PendingWindowCleanup pending{};
            pending.hwnd = hwnd;
            pending.thread = std::move(threads[hwnd]);
            pending.window = std::move(removed);
            threads.erase(hwnd);
            g_pendingCleanups.emplace_back(std::move(pending));
        }
        else
        {
            cleanup_window_resources(removed);
        }

        CleanupFinishedWindows();

        if (should_quit)
            ::PostQuitMessage(0);
    }

    void MultiContextManager::CleanupFinishedWindows()
    {
        if (g_pendingCleanups.empty())
            return;

        auto it = g_pendingCleanups.begin();
        while (it != g_pendingCleanups.end())
        {
            if (!thread_finished(it->thread))
            {
                ++it;
                continue;
            }

            if (it->thread.joinable())
                it->thread.join();

            cleanup_window_resources(it->window);

            it = g_pendingCleanups.erase(it);
        }
    }

    void MultiContextManager::ArrangeDockedWindowsGrid()
    {
        if (!parent) return;

        std::scoped_lock lock(windowsMutex);
        if (windows.empty()) return;

        const int total = static_cast<int>(windows.size());
        int cols = 1, rows = 1;
        while (cols * rows < total) (cols <= rows ? ++cols : ++rows);

        RECT rcClient{};
        ::GetClientRect(parent, &rcClient);
        const int clientW = rcClient.right - rcClient.left;
        const int clientH = rcClient.bottom - rcClient.top;

        const int cw = clamp_positive(clientW / cols);
        const int ch = clamp_positive(clientH / rows);

        for (size_t i = 0; i < windows.size(); ++i)
        {
            const int c = static_cast<int>(i) % cols;
            const int r = static_cast<int>(i) / cols;

            WindowData& win = *windows[i];
            ::SetWindowPos(win.hwnd, nullptr, c * cw, r * ch, cw, ch,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

            HandleResize(win.hwnd, cw, ch);
        }
    }

    void MultiContextManager::StopAll()
    {
        running.store(false, std::memory_order_release);

        {
            std::scoped_lock lock(windowsMutex);
            for (auto& w : windows)
                if (w) w->running = false;
        }

        for (auto& [hwnd, th] : g_threads)
            if (th.joinable()) th.join();
        g_threads.clear();

        for (auto& pending : g_pendingCleanups)
        {
            if (pending.thread.joinable())
                pending.thread.join();
            cleanup_window_resources(pending.window);
        }
        g_pendingCleanups.clear();

        if (s_activeInstance == this) s_activeInstance = nullptr;
    }

    void MultiContextManager::RenderLoop(WindowData& win)
    {
        auto ctx = win.context;
        if (!ctx)
        {
            win.running = false;
            return;
        }

        ctx->windowData = &win;
        MultiContextManager::SetCurrent(ctx);

        struct ResetGuard { ~ResetGuard() { MultiContextManager::SetCurrent(nullptr); } } resetGuard;

        // Raylib/SDL must be created+initialized on the SAME thread that will render them.
		// they are passed the HWND from outside, but they create their own internal windowing context.
#if defined(ALMOND_USING_SDL)
        if (ctx->type == ContextType::SDL)
        {
            almondnamespace::logger::get(kLogSys).logf(
                almondnamespace::logger::LogLevel::WARN,
                std::source_location::current(),
                "SDL init. host={}",
                static_cast<void*>(win.hwnd));
            almondnamespace::sdlcontext::sdl_initialize(
                ctx,
                win.hwnd,
                static_cast<unsigned>(ctx->width),
                static_cast<unsigned>(ctx->height),
                win.onResize ? win.onResize : ctx->onResize,
                win.titleNarrow);
        }
#endif
#if defined(ALMOND_USING_SFML)
        if (ctx->type == ContextType::SFML)
        {
            almondnamespace::logger::get(kLogSys).logf(
                almondnamespace::logger::LogLevel::WARN,
                std::source_location::current(),
                "SFML init. host={}",
                static_cast<void*>(win.hwnd));
            const bool ok = almondnamespace::sfmlcontext::sfml_initialize(
                ctx,
                win.hwnd,
                static_cast<unsigned>(ctx->width),
                static_cast<unsigned>(ctx->height),
                win.onResize ? win.onResize : ctx->onResize,
                win.titleNarrow
            );

            if (!ok) { win.running = false; return; }
        }
#endif
#if defined(ALMOND_USING_RAYLIB)
        if (ctx->type == ContextType::RayLib)
        {
            almondnamespace::logger::get(kLogSys).logf(
                almondnamespace::logger::LogLevel::WARN,
                std::source_location::current(),
                "Raylib init. host={}",
                static_cast<void*>(win.hwnd));
            const bool initialized = almondnamespace::raylibcontext::raylib_initialize(
                ctx,
                win.hwnd,
                static_cast<unsigned>(ctx->width),
                static_cast<unsigned>(ctx->height),
                win.onResize ? win.onResize : ctx->onResize,
                win.titleNarrow);

            if (!initialized)
            {
                win.running = false;
                return;
            }
        }
#endif

		// skipGenericInit for backends that do their own init above
        const bool skipGenericInit =
#if defined(ALMOND_USING_SFML)
            (ctx->type == ContextType::SFML) ||
#endif
#if defined(ALMOND_USING_RAYLIB)
            (ctx->type == ContextType::RayLib) ||
#endif

#if defined(ALMOND_USING_SDL)
            (ctx->type == ContextType::SDL) ||
#endif
            false;

        if (!skipGenericInit)
        {
            if (ctx->initialize) ctx->initialize_safe();
            else { win.running = false; return; }
        }

        if (ctx->init_failed)
        {
            almondnamespace::logger::get(kLogSys).logf(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                std::source_location::current(),
                "Backend init failed for {}. Keeping window alive with no-op process.",
                ctx->backendName);
            ctx->process = nullptr;
        }

        while (running.load(std::memory_order_acquire) && win.running)
        {
            bool keepRunning = true;

            {
                const std::size_t depth = win.commandQueue.depth();
                telemetry::emit_gauge(
                    "renderer.command_queue.depth",
                    static_cast<std::int64_t>(depth),
                    telemetry::RendererTelemetryTags{
                        ctx->type,
                        reinterpret_cast<std::uintptr_t>(win.hwnd)
                    });
            }

            if (ctx->process) keepRunning = ctx->process_safe(ctx, win.commandQueue);
            else win.commandQueue.drain();

            if (!keepRunning)
            {
                win.running = false;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        win.commandQueue.drain();

        if (ctx->cleanup) ctx->cleanup_safe();
    }

    void MultiContextManager::HandleDropFiles(HWND, HDROP hDrop)
    {
        const UINT count = ::DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; ++i)
        {
            wchar_t path[MAX_PATH]{};
            ::DragQueryFileW(hDrop, i, path, MAX_PATH);
            std::wcout << L"[Drop] " << path << L"\n";
        }
    }

    LRESULT CALLBACK MultiContextManager::ParentProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_NCCREATE)
        {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }

        auto* mgr = reinterpret_cast<MultiContextManager*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!mgr) return ::DefWindowProcW(hwnd, msg, wParam, lParam);

        switch (msg)
        {
        case WM_SIZE:
            mgr->ArrangeDockedWindowsGrid();
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC hdc = ::BeginPaint(hwnd, &ps);
#if defined(_DEBUG)
            ::FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
#endif
            ::EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DROPFILES:
            mgr->HandleDropFiles(hwnd, reinterpret_cast<HDROP>(wParam));
            ::DragFinish(reinterpret_cast<HDROP>(wParam));
            return 0;

        case WM_CLOSE:
        {
            std::vector<HWND> children;
            {
                std::scoped_lock lock(mgr->windowsMutex);
                children.reserve(mgr->windows.size());
                for (const auto& win : mgr->windows)
                {
                    if (win && win->hwnd && ::GetParent(win->hwnd) == hwnd)
                        children.push_back(win->hwnd);
                }
            }

            // DO NOT undock/reparent children from this parent thread.
            // Some children (GLFW/raylib) are owned by other threads; cross-thread SetParent/SetWindowPos can deadlock.
            // Instead, post a request to each child so it can undock itself on its owning thread.
            for (HWND child : children)
            {
#if ALMOND_SINGLE_PARENT
                ::PostMessageW(child, WM_ALMOND_DOCKCMD, static_cast<WPARAM>(DockCmd::Undock), 0);
#endif
                ::PostMessageW(child, WM_CLOSE, 0, 0);
            }

            mgr->parent = nullptr;
            ::DestroyWindow(hwnd);
            return 0;
        }

        case WM_DESTROY:
            if (hwnd == mgr->GetParentWindow())
            {
                bool hasWindows = false;
                {
                    std::scoped_lock lock(mgr->windowsMutex);
                    hasWindows = !mgr->windows.empty();
                }
                if (!hasWindows) ::PostQuitMessage(0);
            }
            return 0;
        }

        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT CALLBACK MultiContextManager::ChildProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        static DragState& drag = Drag();

        if (msg == WM_NCCREATE)
        {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }

        switch (msg)
        {
        case WM_LBUTTONDOWN:
        {
            ::SetCapture(hwnd);
            drag.dragging = true;
            drag.draggedWindow = hwnd;
            drag.originalParent = ::GetParent(hwnd);
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ::ClientToScreen(hwnd, &pt);
            drag.lastMousePos = pt;
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            if (!drag.dragging || drag.draggedWindow != hwnd)
                return ::DefWindowProcW(hwnd, msg, wParam, lParam);

            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ::ClientToScreen(hwnd, &pt);

            const int dx = pt.x - drag.lastMousePos.x;
            const int dy = pt.y - drag.lastMousePos.y;
            drag.lastMousePos = pt;

            RECT wndRect{};
            ::GetWindowRect(hwnd, &wndRect);

            const int newX = wndRect.left + dx;
            const int newY = wndRect.top + dy;
            const int currentW = clamp_positive(static_cast<int>(wndRect.right - wndRect.left));
            const int currentH = clamp_positive(static_cast<int>(wndRect.bottom - wndRect.top));

            RECT clientRect{};
            ::GetClientRect(hwnd, &clientRect);
            const int clientW = clamp_positive(static_cast<int>(clientRect.right - clientRect.left));
            const int clientH = clamp_positive(static_cast<int>(clientRect.bottom - clientRect.top));

            if (drag.originalParent)
            {
                RECT prc{};
                ::GetClientRect(drag.originalParent, &prc);
                POINT tl{ 0,0 };
                ::ClientToScreen(drag.originalParent, &tl);
                ::OffsetRect(&prc, tl.x, tl.y);

                int wndW = clientW;
                int wndH = clientH;

                const bool inside =
                    newX >= prc.left && newY >= prc.top &&
                    (newX + currentW) <= prc.right && (newY + currentH) <= prc.bottom;

                if (inside)
                {
                    if (::GetParent(hwnd) != drag.originalParent)
                    {
                        LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
                        style &= ~(WS_POPUP | WS_OVERLAPPEDWINDOW);
                        style |= WS_CHILD;
                        ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);
                        ::SetParent(hwnd, drag.originalParent);

                        POINT cp{ newX, newY };
                        ::ScreenToClient(drag.originalParent, &cp);
                        ::SetWindowPos(hwnd, nullptr, cp.x, cp.y, wndW, wndH,
                            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                    }
                    else
                    {
                        POINT cp{ newX, newY };
                        ::ScreenToClient(drag.originalParent, &cp);
                        ::SetWindowPos(hwnd, nullptr, cp.x, cp.y, 0, 0,
                            SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
                    }
                }
                else
                {
                    if (::GetParent(hwnd) == drag.originalParent)
                    {
                        LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
                        style &= ~WS_CHILD;
                        style |= WS_OVERLAPPEDWINDOW | WS_VISIBLE;
                        ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);
                        ::SetParent(hwnd, nullptr);
                        RECT adjusted{ 0, 0, clientW, clientH };
                        const DWORD exStyle = static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
                        if (::AdjustWindowRectEx(&adjusted, static_cast<DWORD>(style), FALSE, exStyle))
                        {
                            wndW = clamp_positive(adjusted.right - adjusted.left);
                            wndH = clamp_positive(adjusted.bottom - adjusted.top);
                        }
                        ::SetWindowPos(hwnd, nullptr, newX, newY, wndW, wndH,
                            SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                    }
                    else
                    {
                        ::SetWindowPos(hwnd, nullptr, newX, newY, 0, 0,
                            SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
                    }
                }
            }
            else
            {
                ::SetWindowPos(hwnd, nullptr, newX, newY, 0, 0,
                    SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
            }

            return 0;
        }

        case WM_SIZE:
        {
            if (wParam == SIZE_MINIMIZED) return 0;

            auto* mgr = s_activeInstance;
            if (!mgr) break;

            const int width = clamp_positive(static_cast<int>(LOWORD(lParam)));
            const int height = clamp_positive(static_cast<int>(HIWORD(lParam)));
            mgr->HandleResize(hwnd, width, height);
            return 0;
        }

        case WM_LBUTTONUP:
            if (drag.dragging && drag.draggedWindow == hwnd)
            {
                ::ReleaseCapture();
                drag.dragging = false;
                drag.draggedWindow = nullptr;
                drag.originalParent = nullptr;
            }
            return 0;

        case WM_DROPFILES:
            if (HWND p = ::GetParent(hwnd))
                ::SendMessageW(p, WM_DROPFILES, wParam, lParam);
            ::DragFinish(reinterpret_cast<HDROP>(wParam));
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC hdc = ::BeginPaint(hwnd, &ps);
#if defined(_DEBUG)
            ::FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
#endif
            ::EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CLOSE:
            ::DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (auto* mgr = s_activeInstance)
            {
                mgr->RemoveWindow(hwnd);
            }
            return 0;

        default:
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        return 0;
    }
}

#endif // _WIN32
