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
// context.multiplexer.ixx
module;

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#     include <include/framework.hpp>
//#   include <windowsx.h>
//#   include <shellapi.h>
#   include <commctrl.h>
#endif

#if defined(__linux__)
#   include <X11/Xlib.h>
#   include <X11/Xutil.h>
#   include <GL/glx.h>
#endif

#include <include/engine.config.hpp> // for EPOCH_USING Macros

export module context.multiplexer;

import engine.platform;
import context.type;         // epochnamespace::core::ContextType
import context.commandqueue; // epochnamespace::core::CommandQueue
import context.window;       // epochnamespace::core::WindowData
import core.context;         // epochnamespace::core::Context + Set/Get current render ctx

export namespace epochnamespace::platform
{
#if defined(__linux__)
    bool pump_events();
#endif
}

#if !defined(_WIN32)
struct POINT { long x{}; long y{}; };
using HINSTANCE = void*;
using HDROP = void*;
using ATOM = unsigned int;
using LRESULT = long;
using UINT = unsigned int;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
using LPCWSTR = const wchar_t*;
using UINT_PTR = std::uintptr_t;
using DWORD_PTR = std::uintptr_t;
#   ifndef CALLBACK
#       define CALLBACK
#   endif
using HWND = void*;
using HDC = void*;
using HGLRC = void*;
#endif

namespace epochnamespace::core
{
    export struct DetachedContextWindowRequest
    {
        ContextType type{ ContextType::OpenGL };
        std::string title{ "Epoch Context" };
        std::string gui_route{};
        int width{ 720 };
        int height{ 440 };
        bool start_docked{ false };
    };

#if defined(_WIN32)

    export struct DragState
    {
        bool dragging = false;
        POINT lastMousePos{};
        POINT dragStartMousePos{};
        POINT dragWindowOffset{};
        HWND draggedWindow = nullptr;
        HWND originalParent = nullptr;
        bool proxyUndockPending = false;
        bool proxyRedockPending = false;
    };

    export std::unordered_map<HWND, std::thread>& Threads() noexcept;
    export DragState& Drag() noexcept;

    export void MakeDockable(HWND hwnd, HWND parent);
    export class MultiContextManager;
    export MultiContextManager* GetActiveMultiContextManager() noexcept;
    export void RequestActiveParentLayout() noexcept;

    export class MultiContextManager
    {
    public:
        static void ShowConsole();

        bool Initialize(
            HINSTANCE hInst,
            int RayLibWinCount = 0,
            int SDLWinCount = 0,
            int SFMLWinCount = 0,
            int VulkanWinCount = 0,
            int OpenGLWinCount = 0,
            int DirectXWinCount = 0,
            int SoftwareWinCount = 0,
            bool parented = true);

        void StopAll();
        bool IsRunning() const noexcept;
        void StopRunning() noexcept;

        using ResizeCallback = std::function<void(int, int)>;

        void AddWindow(
            HWND hwnd,
            HWND parent,
            HDC hdc,
            HGLRC glContext,
            bool usesSharedContext,
            ResizeCallback onResize,
            ContextType type);

        void RemoveWindow(HWND hwnd);
        void CleanupFinishedWindows();
        void ArrangeDockedWindowsGrid();
        void HandleResize(HWND hwnd, int width, int height);
        void StartRenderThreads();
        bool OpenDetachedContextWindow(const DetachedContextWindowRequest& request);

        HWND GetParentWindow() const { return parent; }
        const std::vector<std::unique_ptr<WindowData>>& GetWindows() const { return windows; }

        using RenderCommand = std::function<void()>;
        void EnqueueRenderCommand(HWND hwnd, RenderCommand cmd);

        // Stable API used across the engine (GUI etc.)
        static void SetCurrent(std::shared_ptr<core::Context> ctx) { epochnamespace::core::set_current_render_context(std::move(ctx)); }
        static std::shared_ptr<core::Context> GetCurrent() { return epochnamespace::core::get_current_render_context(); }

        static LRESULT CALLBACK ParentProc(HWND, UINT, WPARAM, LPARAM);
        static LRESULT CALLBACK ChildProc(HWND, UINT, WPARAM, LPARAM);
        void HandleDropFiles(HWND, HDROP);
        static void AttachBackendInputBridge(HWND hwnd) noexcept;

        static ATOM RegisterParentClass(HINSTANCE, LPCWSTR);
        static ATOM RegisterChildClass(HINSTANCE, LPCWSTR);

        WindowData* findWindowByHWND(HWND hwnd);
        const WindowData* findWindowByHWND(HWND hwnd) const;

        WindowData* findWindowByContext(const std::shared_ptr<core::Context>& ctx);
        const WindowData* findWindowByContext(const std::shared_ptr<core::Context>& ctx) const;

    private:
        std::vector<std::unique_ptr<WindowData>> windows;
        std::atomic<bool> running{ false };
        mutable std::recursive_mutex windowsMutex;
        DWORD uiThreadId = 0;

        HGLRC sharedContext = nullptr;
        HWND  parent = nullptr;

        void RenderLoop(WindowData& win);
        void SetupPixelFormat(HDC hdc);
        HGLRC CreateSharedGLContext(HDC hdc);
        int get_title_bar_thickness(const HWND window_handle);
        bool CreateDetachedContextWindowOnOwnerThread(const DetachedContextWindowRequest& request);

        inline static MultiContextManager* s_activeInstance = nullptr;

        friend MultiContextManager* GetActiveMultiContextManager() noexcept;
    };

#elif defined(__linux__)

    export class MultiContextManager
    {
    public:
        using ResizeCallback = std::function<void(int, int)>;
        using RenderCommand = std::function<void()>;

        static void ShowConsole();

        bool Initialize(
            HINSTANCE hInst,
            int RayLibWinCount = 0,
            int SDLWinCount = 0,
            int SFMLWinCount = 0,
            int VulkanWinCount = 0,
            int OpenGLWinCount = 0,
            int DirectXWinCount = 0,
            int SoftwareWinCount = 0,
            bool parented = false);

        void StopAll();
        bool IsRunning() const noexcept;
        void StopRunning() noexcept;

        void AddWindow(HWND hwnd, HWND parent, HDC hdc, HGLRC glContext,
            bool usesSharedContext,
            ResizeCallback onResize,
            ContextType type);

        void RemoveWindow(HWND hwnd);
        void CleanupFinishedWindows() {}
        void ArrangeDockedWindowsGrid();
        void HandleResize(HWND hwnd, int width, int height);
        void StartRenderThreads();
        bool OpenDetachedContextWindow(const DetachedContextWindowRequest&) { return false; }

        HWND GetParentWindow() const { return nullptr; }
        const std::vector<std::unique_ptr<WindowData>>& GetWindows() const { return windows; }

        void EnqueueRenderCommand(HWND hwnd, RenderCommand cmd);

        static void SetCurrent(std::shared_ptr<core::Context> ctx) { core::set_current_render_context(std::move(ctx)); }
        static std::shared_ptr<core::Context> GetCurrent() { return core::get_current_render_context(); }

        WindowData* findWindowByHWND(HWND hwnd);
        const WindowData* findWindowByHWND(HWND hwnd) const;
        WindowData* findWindowByContext(const std::shared_ptr<core::Context>& ctx);
        const WindowData* findWindowByContext(const std::shared_ptr<core::Context>& ctx) const;

    private:
        void RenderLoop(WindowData& win);
        GLXContext CreateGLXContext();
        void DestroyWindowData(WindowData& win);

        std::vector<std::unique_ptr<WindowData>> windows;
        std::unordered_map<::Window, std::thread> threads;
        std::atomic<bool> running{ true };
        mutable std::mutex windowsMutex;

        Display* display = nullptr;
        int screen = 0;
        GLXFBConfig fbConfig = nullptr;
        Colormap colormap = 0;
        GLXContext sharedContext = nullptr;
        Atom wmDeleteMessage = 0;
        XVisualInfo visualInfo{};

        inline static MultiContextManager* s_activeInstance = nullptr;

        friend MultiContextManager* GetActiveMultiContextManager() noexcept;
        friend void HandleX11Configure(::Window window, int width, int height);
    };

    export MultiContextManager* GetActiveMultiContextManager() noexcept;
    export void HandleX11Configure(::Window window, int width, int height);
    export void RequestActiveParentLayout() noexcept;

#else

    export class MultiContextManager
    {
    public:
        using ResizeCallback = std::function<void(int, int)>;
        using RenderCommand = std::function<void()>;

        static void ShowConsole() {}
        bool Initialize(HINSTANCE, int, int, int, int, int, int, int, bool) { return false; }
        void StopAll() {}
        bool IsRunning() const noexcept { return false; }
        void StopRunning() noexcept {}

        void AddWindow(HWND, HWND, HDC, HGLRC, bool, ResizeCallback, ContextType) {}
        void RemoveWindow(HWND) {}
        void CleanupFinishedWindows() {}
        void ArrangeDockedWindowsGrid() {}
        void StartRenderThreads() {}
        void HandleResize(HWND, int, int) {}
        bool OpenDetachedContextWindow(const DetachedContextWindowRequest&) { return false; }

        HWND GetParentWindow() const { return nullptr; }
        const std::vector<std::unique_ptr<WindowData>>& GetWindows() const { return s_emptyWindows; }

        void EnqueueRenderCommand(HWND, RenderCommand) {}

        static void SetCurrent(std::shared_ptr<core::Context> ctx) { core::set_current_render_context(std::move(ctx)); }
        static std::shared_ptr<core::Context> GetCurrent() { return core::get_current_render_context(); }

        static LRESULT CALLBACK ParentProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
        static LRESULT CALLBACK ChildProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
        void HandleDropFiles(HWND, HDROP) {}

        WindowData* findWindowByHWND(HWND) { return nullptr; }
        const WindowData* findWindowByHWND(HWND) const { return nullptr; }
        WindowData* findWindowByContext(const std::shared_ptr<core::Context>&) { return nullptr; }
        const WindowData* findWindowByContext(const std::shared_ptr<core::Context>&) const { return nullptr; }

    private:
        inline static const std::vector<std::unique_ptr<WindowData>> s_emptyWindows{};
    };

    export inline MultiContextManager* GetActiveMultiContextManager() noexcept { return nullptr; }
    export inline void RequestActiveParentLayout() noexcept {}

#endif
} // namespace epochnamespace::core
