
// aengine.context.multiplexer.ixx
module;

#if defined(_WIN32)
#     include <include/aframework.hpp>
//#   include <windowsx.h>
//#   include <shellapi.h>
#   include <commctrl.h>
#endif

#if defined(__linux__)
#   include <X11/Xlib.h>
#   include <X11/Xutil.h>
#   include <GL/glx.h>
#endif

#include <include/aengine.config.hpp> // for ALMOND_USING Macros

export module aengine.context.multiplexer;

import <atomic>;
import <cstdint>;
import <functional>;
import <memory>;
import <mutex>;
import <queue>;
import <thread>;
import <unordered_map>;
import <vector>;

import aengine.platform;

import aengine.context.type;         // almondnamespace::core::ContextType
import aengine.context.commandqueue; // almondnamespace::core::CommandQueue
import aengine.context.window;       // almondnamespace::core::WindowData
import aengine.core.context;         // almondnamespace::core::Context + Set/Get current render ctx

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

export namespace almondnamespace::core
{
#if defined(_WIN32)

    export struct DragState
    {
        bool dragging = false;
        POINT lastMousePos{};
        HWND draggedWindow = nullptr;
        HWND originalParent = nullptr;
    };

    export std::unordered_map<HWND, std::thread>& Threads() noexcept;
    export DragState& Drag() noexcept;

    export void MakeDockable(HWND hwnd, HWND parent);

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

        HWND GetParentWindow() const { return parent; }
        const std::vector<std::unique_ptr<WindowData>>& GetWindows() const { return windows; }

        using RenderCommand = std::function<void()>;
        void EnqueueRenderCommand(HWND hwnd, RenderCommand cmd);

        // Stable API used across the engine (GUI etc.)
        static void SetCurrent(std::shared_ptr<core::Context> ctx) { almondnamespace::core::set_current_render_context(std::move(ctx)); }
        static std::shared_ptr<core::Context> GetCurrent() { return almondnamespace::core::get_current_render_context(); }

        static LRESULT CALLBACK ParentProc(HWND, UINT, WPARAM, LPARAM);
        static LRESULT CALLBACK ChildProc(HWND, UINT, WPARAM, LPARAM);
        void HandleDropFiles(HWND, HDROP);

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

        HGLRC sharedContext = nullptr;
        HWND  parent = nullptr;

        void RenderLoop(WindowData& win);
        void SetupPixelFormat(HDC hdc);
        HGLRC CreateSharedGLContext(HDC hdc);
        int get_title_bar_thickness(const HWND window_handle);

        inline static MultiContextManager* s_activeInstance = nullptr;
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

#else

    export class MultiContextManager
    {
    public:
        using ResizeCallback = std::function<void(int, int)>;
        using RenderCommand = std::function<void()>;

        static void ShowConsole() {}
        bool Initialize(HINSTANCE, int, int, int, int, int, bool) { return false; }
        void StopAll() {}
        bool IsRunning() const noexcept { return false; }
        void StopRunning() noexcept {}

        void AddWindow(HWND, HWND, HDC, HGLRC, bool, ResizeCallback, ContextType) {}
        void RemoveWindow(HWND) {}
        void CleanupFinishedWindows() {}
        void ArrangeDockedWindowsGrid() {}
        void StartRenderThreads() {}
        void HandleResize(HWND, int, int) {}

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

#endif
} // namespace almondnamespace::core
