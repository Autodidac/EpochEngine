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
// Epoch system notes (auto-checked):
// Epoch Context: window/context state, dispatch and input ownership remain scoped.
 // context.multiplexer.win.cpp  (TU implementation; NOT a module partition)
 //

#include <include/engine.config.hpp>
#include "host.log_policy.hpp"
#include "host.frame_rate_sample.hpp"
#include "../../editor/ai/editor.build_pressure.hpp"

#if defined(_WIN32)
#   include "../../../resource/resource.h"
#   ifdef EPOCH_USING_WINMAIN
#       include <../src/platform/platform.framework.hpp>
#   endif
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <timeapi.h>
#   include <windowsx.h>
#   include <commctrl.h>
#   include <dwmapi.h>
#   pragma comment(lib, "winmm.lib")
#   include <shellapi.h>
#   include "../../platform/platform.file_drop_win.hpp"
#   pragma comment(lib, "comctl32.lib")
#   pragma comment(lib, "dwmapi.lib")

#   include <algorithm>
#   include <array>
#   include <atomic>
#   include <chrono>
#   include <cmath>
#   include <cstdint>
#   include <format>
#   include <functional>
#   include <iostream>
#   include <memory>
#   include <mutex>
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


import platform.engine;
import platform.window_attachment;
import platform.window;
import platform.window;
import utility.string_converter;

import epoch.cli;
import core.context;
import core.logger;
import core.commandline;
import gui.engine;

import context.commandqueue;
import context.multiplexer;
import context.type;
import context.window;
import telemetry.engine;
import systems.registry;
import perf.tier;
import platform.budgets;
import platform.work_admission;
import render.context_frame;
import render.event_debug;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import opengl.context;
import opengl.platform;
#endif
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
import software.context;
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
import raylib.context;
    struct ScopedWindowsTimerResolution final
    {
        static constexpr UINT period_ms = 1u;
        bool active{};

        ScopedWindowsTimerResolution() noexcept
            : active(::timeBeginPeriod(period_ms) == 0u)
        {
        }

        ~ScopedWindowsTimerResolution()
        {
            if (active)
                (void)::timeEndPeriod(period_ms);
        }

        ScopedWindowsTimerResolution(
            const ScopedWindowsTimerResolution&) = delete;
        ScopedWindowsTimerResolution& operator=(
            const ScopedWindowsTimerResolution&) = delete;
        ScopedWindowsTimerResolution(ScopedWindowsTimerResolution&&) = delete;
        ScopedWindowsTimerResolution& operator=(
            ScopedWindowsTimerResolution&&) = delete;
    };

#endif

#if defined(_WIN32)

#include "implementation/host.window_interaction.inl"
namespace epochengine::core
{
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
    // Raylib embeds a real GLFW-created HWND. Re-parenting must be performed on the
    // thread that owns the host HWND, otherwise Win32 can deadlock via cross-thread
    // synchronous messages during SetParent/SetWindowPos.
#endif
    // ------------------------------------------------------------
    // Exported helpers (declared in the interface module)
    // ------------------------------------------------------------
    std::unordered_map<HWND, std::thread>& Threads() noexcept { return g_threads; }
    DragState& Drag() noexcept { return g_drag; }

    void publish_routed_panel_tab_drop_target(
        std::string_view route,
        RoutedPanelDockTarget target,
        std::uint32_t insertion_index) noexcept
    {
        std::scoped_lock lock(g_routedPanelTabDrop.mutex);
        if (route.empty() || target == RoutedPanelDockTarget::none)
        {
            g_routedPanelTabDrop.route.clear();
            g_routedPanelTabDrop.target = RoutedPanelDockTarget::none;
            g_routedPanelTabDrop.insertionIndex = 0xffffffffu;
            return;
        }
        g_routedPanelTabDrop.route.assign(route);
        g_routedPanelTabDrop.target = target;
        g_routedPanelTabDrop.insertionIndex = insertion_index;
    }

    RoutedPanelDockDragProjection routed_panel_dock_drag_projection() noexcept
    {
        const DragState& drag = Drag();
        if (!drag.dragging || !drag.draggedWindow)
            return {};

        const WindowData* window =
            resolve_window_data_for_hwnd(drag.draggedWindow);
        if (!window)
            return {};
        const std::string& projectionLabel = window->guiRoute.empty()
            ? window->titleNarrow
            : window->guiRoute;

        HWND parent = drag.originalParent;
        if (!parent || ::IsWindow(parent) == FALSE)
            parent = stored_dock_parent(drag.draggedWindow);
        if (!parent || ::IsWindow(parent) == FALSE)
            return {};

        RECT client{};
        if (::GetClientRect(parent, &client) == FALSE)
            return {};
        POINT local = drag.lastMousePos;
        if (::ScreenToClient(parent, &local) == FALSE)
            return {};

        return RoutedPanelDockDragProjection{
            .active = true,
            .routed_panel = !window->guiRoute.empty(),
            .host_overlay = window->guiRoute.empty(),
            .route = projectionLabel,
            .cursor_x = static_cast<float>(local.x),
            .cursor_y = static_cast<float>(local.y),
            .host_width = static_cast<float>(
                (std::max)(1L, client.right - client.left)),
            .host_height = static_cast<float>(
                (std::max)(1L, client.bottom - client.top)),
            .target = routed_panel_target_for_parent(
                parent,
                drag.lastMousePos,
                !window->guiRoute.empty())
        };
    }

    MultiContextManager* GetActiveMultiContextManager() noexcept { return g_activeManager; }

    void RequestActiveParentLayout() noexcept
    {
        if (auto* mgr = GetActiveMultiContextManager())
        {
            if (const HWND parentHwnd = mgr->GetParentWindow();
                parentHwnd && ::IsWindow(parentHwnd) != FALSE)
            {
                request_parent_layout(parentHwnd);
            }
        }
    }

    void MakeDockable(HWND hwnd, HWND parent)
    {
#if EPOCH_SINGLE_PARENT
        (void)parent;
        if (!hwnd) return;
        auto* ctx = new SubCtx{ parent };
        if (parent)
            ::SetPropW(hwnd, kEpochDockParentProp, parent);
        if (!::SetWindowSubclass(hwnd, DockableProc, 1, reinterpret_cast<DWORD_PTR>(ctx)))
        {
            if (parent)
                ::RemovePropW(hwnd, kEpochDockParentProp);
            delete ctx;
        }
#else
        (void)hwnd;
        (void)parent;
#endif
    }

    HWND MultiContextManager::ResolveHoveredContext(HWND receiver)
    {
        if (!parent || !running.load(std::memory_order_acquire)) return nullptr;
        std::vector<epochengine::platform::window_adapter::NativeChildWindow> admitted;
        {
            std::scoped_lock lock(windowsMutex);
            admitted.reserve(windows.size() * 3u);
            for (const auto& window : windows)
            {
                if (!window || !window->running.load(std::memory_order_acquire)
                    || window->get_should_close()) continue;
                const epochengine::platform::window_adapter::WindowHandle identity{
                    reinterpret_cast<std::uintptr_t>(window->hwnd)};
                for (const HWND surface : {window->hwnd, window->host_hwnd,
                         window->hwndChild.load(std::memory_order_acquire)})
                    if (surface) admitted.push_back({identity,
                        {reinterpret_cast<std::uintptr_t>(surface)}});
            }
        }
        // Native queries occur after dropping the context registry lock.
        const auto target = epochengine::platform::window_adapter::native_child_close_target(
            {reinterpret_cast<std::uintptr_t>(parent)}, admitted,
            {reinterpret_cast<std::uintptr_t>(receiver)});
        return reinterpret_cast<HWND>(target.value);
    }

    bool MultiContextManager::CloseHoveredContext(HWND receiver)
    {
        try
        {
            if (!parent && findWindowByHWND(receiver))
            {
                RemoveWindow(receiver);
                return true;
            }
            if (const HWND target = ResolveHoveredContext(receiver))
            {
                // Existing retirement owns cancellation, join and native
                // destruction. Never destroy the surface from an input hook.
                RemoveWindow(target);
                return true;
            }
        }
        catch (...) { /* A native input callback must not unwind. */ }
        return false;
    }

    void MultiContextManager::AttachBackendInputAdapter(HWND hwnd) noexcept
    {
        if (!hwnd || ::IsWindow(hwnd) == FALSE)
            return;

        // Dockable child panes already forward GUI input through DockableProc.
        // Installing the input adapter again on the same HWND duplicates WM_CHAR/WM_KEYDOWN.
        if (::GetPropW(hwnd, kEpochDockParentProp) || native_class_owns_gui_input(hwnd))
            return;

        (void)epochengine::platform::window_adapter::attach_native_input(
            {reinterpret_cast<std::uintptr_t>(hwnd)},
            {GetActiveMultiContextManager(), backend_input_callback});
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
            if (epochengine::core::cli::updater_shell_requested
                && type == ContextType::OpenGL
                && index == 0)
            {
                return L"Epoch Updater Shell";
            }

            const std::wstring_view base = BackendDisplayName(type);
            std::wstring title{base};
            title += L" ";
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
            [hwnd](const std::unique_ptr<WindowData>& w) { return matches_window_handle(w.get(), hwnd); });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    const WindowData* MultiContextManager::findWindowByHWND(HWND hwnd) const
    {
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [hwnd](const std::unique_ptr<WindowData>& w) { return matches_window_handle(w.get(), hwnd); });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    WindowData* MultiContextManager::findWindowByContext(const std::shared_ptr<Context>& ctx)
    {
        if (!ctx) return nullptr;
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [&](const std::unique_ptr<WindowData>& w) { return w && w->context && w->context.get() == static_cast<void*>(ctx.get()); });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    const WindowData* MultiContextManager::findWindowByContext(const std::shared_ptr<Context>& ctx) const
    {
        if (!ctx) return nullptr;
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [&](const std::unique_ptr<WindowData>& w) { return w && w->context && w->context.get() == static_cast<void*>(ctx.get()); });
        return (it != windows.end()) ? it->get() : nullptr;
    }

    bool MultiContextManager::PromotePrimaryWindow(const std::shared_ptr<Context>& context)
    {
        if (!context || !parent || ::IsWindow(parent) == FALSE)
            return false;

        {
            std::scoped_lock lock(windowsMutex);
            const auto targetIt = std::find_if(
                windows.begin(),
                windows.end(),
                [&](const std::unique_ptr<WindowData>& window)
                {
                    return window
                        && window->context
                        && window->context.get() == static_cast<void*>(context.get());
                });
            if (targetIt == windows.end())
                return false;

            auto* const target = targetIt->get();
            if (!target
                || !target->guiRoute.empty()
                || target->isFloating
                || !target->running.load(std::memory_order_acquire)
                || target->get_should_close()
                || target->backend_lifecycle() != BackendLifecycleState::ready)
            {
                return false;
            }

        }

        ArrangeDockedWindowsGrid();
        return true;
    }

    void MultiContextManager::ConstrainPrimaryWindowToWorkArea(
        const std::shared_ptr<Context>& context)
    {
        HWND topLevel = parent;
        if (!topLevel || ::IsWindow(topLevel) == FALSE)
        {
            std::scoped_lock lock(windowsMutex);
            const auto target = std::find_if(
                windows.begin(),
                windows.end(),
                [&](const std::unique_ptr<WindowData>& window)
                {
                    return window
                        && (!context
                            || (window->context
                                && window->context.get()
                                    == static_cast<void*>(context.get())));
                });
            if (target == windows.end())
                return;

            const WindowData& window = **target;
            HWND nativeWindow = window.host_hwnd;
            if (!nativeWindow || ::IsWindow(nativeWindow) == FALSE)
                nativeWindow = window.hwndChild;
            if (!nativeWindow || ::IsWindow(nativeWindow) == FALSE)
                nativeWindow = window.hwnd;
            if (!nativeWindow || ::IsWindow(nativeWindow) == FALSE)
                return;

            topLevel = ::GetAncestor(nativeWindow, GA_ROOT);
            if (!topLevel)
                topLevel = nativeWindow;
        }

        if (::IsWindow(topLevel) == FALSE || ::IsZoomed(topLevel) != FALSE)
            return;

        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(MONITORINFO);
        const HMONITOR monitor = ::MonitorFromWindow(
            topLevel,
            MONITOR_DEFAULTTONEAREST);
        if (!monitor || ::GetMonitorInfoW(monitor, &monitorInfo) == FALSE)
            return;

        RECT windowRect{};
        if (::GetWindowRect(topLevel, &windowRect) == FALSE)
            return;

        const RECT& workArea = monitorInfo.rcWork;
        const int workWidth = (std::max)(
            1,
            static_cast<int>(workArea.right - workArea.left));
        const int workHeight = (std::max)(
            1,
            static_cast<int>(workArea.bottom - workArea.top));
        const int windowWidth = (std::clamp)(
            static_cast<int>(windowRect.right - windowRect.left),
            1,
            workWidth);
        const int windowHeight = (std::clamp)(
            static_cast<int>(windowRect.bottom - windowRect.top),
            1,
            workHeight);
        const int x = (std::clamp)(
            static_cast<int>(windowRect.left),
            static_cast<int>(workArea.left),
            static_cast<int>(workArea.right) - windowWidth);
        const int y = (std::clamp)(
            static_cast<int>(windowRect.top),
            static_cast<int>(workArea.top),
            static_cast<int>(workArea.bottom) - windowHeight);

        if (x == windowRect.left
            && y == windowRect.top
            && windowWidth == windowRect.right - windowRect.left
            && windowHeight == windowRect.bottom - windowRect.top)
        {
            return;
        }

        ::SetWindowPos(
            topLevel,
            nullptr,
            x,
            y,
            windowWidth,
            windowHeight,
            SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER
                | SWP_SHOWWINDOW);
    }

    // ------------------------------------------------------------
    // MultiContextManager (public helpers)
    // ------------------------------------------------------------
    bool MultiContextManager::IsRunning() const noexcept
    {
        return running.load(std::memory_order_acquire);
    }

    void MultiContextManager::BeginContextReplacement() noexcept
    {
        contextReplacementHolds.fetch_add(1, std::memory_order_acq_rel);
    }

    void MultiContextManager::EndContextReplacement() noexcept
    {
        std::uint32_t previous = contextReplacementHolds.load(std::memory_order_acquire);
        while (previous != 0
            && !contextReplacementHolds.compare_exchange_weak(
                previous,
                previous - 1,
                std::memory_order_acq_rel,
                std::memory_order_acquire))
        {
        }

        if (previous == 0)
            return;

        if (previous != 1)
            return;

        bool noWindows = false;
        {
            std::scoped_lock lock(windowsMutex);
            noWindows = windows.empty();
        }
        if (!noWindows)
            return;

        running.store(false, std::memory_order_release);
        if (uiThreadId != 0 && uiThreadId != ::GetCurrentThreadId())
            ::PostThreadMessageW(uiThreadId, WM_QUIT, 0, 0);
        else
            ::PostQuitMessage(0);
    }

    bool MultiContextManager::ContextReplacementInProgress() const noexcept
    {
        return contextReplacementHolds.load(std::memory_order_acquire) > 0;
    }

    bool MultiContextManager::IsContextRetired(const Context* context) const noexcept
    {
        if (!context)
            return true;

        std::scoped_lock lock(windowsMutex, g_threadStateMutex);
        const bool stillOwned = std::ranges::any_of(
            windows,
            [context](const std::unique_ptr<WindowData>& window)
            {
                const auto liveContext = window ? typed_context(window->context) : nullptr;
                return liveContext && liveContext.get() == context;
            });
        if (stillOwned)
            return false;

        return std::ranges::none_of(
            g_pendingCleanups,
            [context](const PendingWindowCleanup& pending)
            {
                const auto liveContext = pending.window ? typed_context(pending.window->context) : nullptr;
                return liveContext && liveContext.get() == context;
            });
    }

    void MultiContextManager::StopRunning() noexcept
    {
        running.store(false, std::memory_order_release);
    }

    void MultiContextManager::EnqueueRenderCommand(HWND hwnd, RenderCommand cmd)
    {
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [hwnd](const std::unique_ptr<WindowData>& w) { return matches_window_handle(w.get(), hwnd); });
        if (it != windows.end()) (*it)->EnqueueCommand(std::move(cmd));
    }

    bool MultiContextManager::OpenDetachedContextWindow(const DetachedContextWindowRequest& request)
    {
        if (!running.load(std::memory_order_acquire))
            return false;
        if (request.activate_existing_only && uiThreadId == 0)
            return false;

        s_activeInstance = this;
        g_activeManager = this;

        if (uiThreadId == 0 || ::GetCurrentThreadId() == uiThreadId)
            return CreateDetachedContextWindowOnOwnerThread(request);

        HWND target = parent;
        if (!target || ::IsWindow(target) == FALSE)
        {
            std::scoped_lock lock(windowsMutex);
            for (const auto& window : windows)
            {
                target = primary_window_handle(window.get());
                if (target && ::IsWindow(target) != FALSE)
                    break;
            }
        }

        if (!target || ::IsWindow(target) == FALSE)
            return false;

        auto* posted = new DetachedContextWindowRequest(request);
        if (::PostMessageW(
            target,
            WM_EPOCH_OPEN_DETACHED_CONTEXT,
            0,
            reinterpret_cast<LPARAM>(posted)) == FALSE)
        {
            delete posted;
            return false;
        }

        return true;
    }

    bool MultiContextManager::OpenReplacementContextWindow(
        const DetachedContextWindowRequest& request,
        std::shared_ptr<Context>* createdContext)
    {
        if (createdContext)
            createdContext->reset();

        if (!ContextReplacementInProgress()
            || !parent
            || ::IsWindow(parent) == FALSE
            || uiThreadId == 0
            || ::GetCurrentThreadId() != uiThreadId)
        {
            return false;
        }

        DetachedContextWindowRequest dockedRequest = request;
        dockedRequest.start_docked = true;
        return CreateDetachedContextWindowOnOwnerThread(dockedRequest, createdContext);
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

    bool MultiContextManager::CreateDetachedContextWindowOnOwnerThread(
        const DetachedContextWindowRequest& request,
        std::shared_ptr<Context>* createdContext)
    {
        if (createdContext)
            createdContext->reset();

        if (!running.load(std::memory_order_acquire))
            return false;

        s_activeInstance = this;
        g_activeManager = this;

        if (request.activate_existing_only)
        {
            if (request.gui_route.empty()) return false;
            DWORD foregroundProcess{};
            ::GetWindowThreadProcessId(::GetForegroundWindow(), &foregroundProcess);
            // An explicit view change may raise its own companions, but a
            // delayed/background request must not activate another application.
            if (foregroundProcess != ::GetCurrentProcessId()) return false;
            HWND target{}, top{};
            DWORD targetThread{};
            bool ownerFocusQueued{};
            {
                std::scoped_lock lock(windowsMutex);
                for (const auto& window : windows)
                {
                    if (!window || window->guiRoute != request.gui_route
                        || !window->running.load(std::memory_order_acquire)
                        || window->get_should_close()
                        || window->retirementQueued.load(std::memory_order_acquire))
                        continue;
                    target = owner_thread_dock_handle(window.get());
                    if (!target) return false;
                    DWORD targetProcess{};
                    targetThread = ::GetWindowThreadProcessId(target, &targetProcess);
                    if (targetThread == 0 || targetProcess != ::GetCurrentProcessId()) return false;
                    top = ::GetAncestor(target, GA_ROOT);
                    if (!top || ::IsWindow(top) == FALSE) return false;
                    if (targetThread != ::GetCurrentThreadId())
                    {
                        // Native/backend owner work stays bound to its existing
                        // queue lifetime. Nothing reads WindowData after unlock.
                        window->ownerThreadCommandQueue.enqueue(
                            [owner = window.get(), target, route = request.gui_route]()
                            {
                                DWORD queuedForegroundProcess{};
                                ::GetWindowThreadProcessId(::GetForegroundWindow(), &queuedForegroundProcess);
                                if (owner->guiRoute == route
                                    && owner->running.load(std::memory_order_acquire)
                                    && !owner->get_should_close()
                                    && !owner->retirementQueued.load(std::memory_order_acquire)
                                    && queuedForegroundProcess == ::GetCurrentProcessId()
                                    && ::IsWindow(target) != FALSE
                                    && ::GetWindowThreadProcessId(target, nullptr) == ::GetCurrentThreadId())
                                    focus_gui_mouse_target(target);
                            });
                        ownerFocusQueued = true;
                    }
                    break;
                }
            }
            if (!target || !top) return false;
            // Native activation can synchronously dispatch window handlers.
            // Never hold the context registry lock across those callbacks.
            DWORD liveProcess{};
            ::GetWindowThreadProcessId(::GetForegroundWindow(), &foregroundProcess);
            if (::IsWindow(target) == FALSE || ::IsWindow(top) == FALSE
                || foregroundProcess != ::GetCurrentProcessId()
                || ::GetWindowThreadProcessId(target, &liveProcess) != targetThread
                || liveProcess != ::GetCurrentProcessId()) return false;
            if (::IsIconic(top)) ::ShowWindow(top, SW_RESTORE);
            if (::IsWindow(top) == FALSE) return false;
            ::BringWindowToTop(top);
            ::SetForegroundWindow(top);
            if (!ownerFocusQueued && ::IsWindow(target) != FALSE
                && ::GetWindowThreadProcessId(target, &liveProcess) == targetThread
                && liveProcess == ::GetCurrentProcessId())
                focus_gui_mouse_target(target);
            return true;
        }

        const int clientW = clamp_positive(request.width);
        const int clientH = clamp_positive(request.height);
        std::wstring title = epochengine::text::widen_utf16(request.title);
        if (title.empty())
            title = L"Epoch Context";

        int initialX = CW_USEDEFAULT;
        int initialY = CW_USEDEFAULT;
        RECT workArea{};
        if (::SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0) != FALSE)
        {
            constexpr int kMargin = 36;
            initialX = (std::max)(static_cast<int>(workArea.left) + kMargin,
                static_cast<int>(workArea.right) - clientW - kMargin);
            initialY = static_cast<int>(workArea.top) + kMargin + 56;
        }

        const bool proxyChildBackend = backend_uses_proxy_child(request.type);
        const bool adoptedChildBackend = backend_adopts_native_child(request.type);
        const bool delayedPlaceholderVisibility = proxyChildBackend || adoptedChildBackend;
        const bool hasValidParent =
            parent
            && ::IsWindow(parent) != FALSE;

        if (request.start_docked && delayedPlaceholderVisibility && !hasValidParent)
        {
            epochengine::logger::get(kLogSys).logf(
                logger::LogLevel::Error,
                std::source_location::current(),
                "Refusing start-docked native-child context '{}' because no valid parent host exists.",
                request.title);
            return false;
        }

        const bool startDocked =
            request.start_docked
            && hasValidParent;

        const DWORD childStyle = WS_CHILD
            | WS_CLIPSIBLINGS
            | WS_CLIPCHILDREN
            | (delayedPlaceholderVisibility ? 0u : WS_VISIBLE);
        const DWORD floatingStyle = WS_OVERLAPPEDWINDOW
            | WS_VISIBLE
            | WS_CLIPSIBLINGS
            | WS_CLIPCHILDREN;

        HWND hwnd = ::CreateWindowExW(
            startDocked ? 0 : WS_EX_APPWINDOW,
            L"EpochChild",
            title.c_str(),
            startDocked ? childStyle : floatingStyle,
            startDocked ? 0 : initialX,
            startDocked ? 0 : initialY,
            clientW,
            clientH,
            startDocked ? parent : nullptr,
            nullptr,
            ::GetModuleHandleW(nullptr),
            this);

        if (!hwnd)
            return false;

        ::SetWindowTextW(hwnd, title.c_str());
        apply_native_window_chrome(hwnd);
        ::DragAcceptFiles(hwnd, TRUE);
        if (startDocked)
        {
            MakeDockable(hwnd, parent);
        }
        else if (const HWND dockParent = GetParentWindow();
            dockParent && ::IsWindow(dockParent) != FALSE && !proxyChildBackend)
        {
            MakeDockable(hwnd, dockParent);
        }

        HDC hdc = ::GetDC(hwnd);
        HGLRC glrc = nullptr;
        bool usesSharedContext = false;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        if (request.type == ContextType::OpenGL)
        {
            glrc = CreateSharedGLContext(hdc);
            usesSharedContext = (glrc != nullptr);
            if (!glrc)
            {
                ::ReleaseDC(hwnd, hdc);
                ::DestroyWindow(hwnd);
                return false;
            }
        }
#endif

        {
            bool needInit = false;
            {
                std::shared_lock lock(g_backendsMutex);
                needInit = g_backends.empty();
            }
            if (needInit)
                InitializeAllContexts();
        }

        std::shared_ptr<Context> ctx;
        {
            std::unique_lock lock(g_backendsMutex);
            auto it = g_backends.find(request.type);
            if (it == g_backends.end() || !it->second.master)
            {
                ::ReleaseDC(hwnd, hdc);
                ::DestroyWindow(hwnd);
                return false;
            }

            auto& backendSlot = it->second;
            if (!backendSlot.master->windowData)
            {
                ctx = backendSlot.master;
            }
            else
            {
                auto freeIt = std::find_if(
                    backendSlot.duplicates.begin(),
                    backendSlot.duplicates.end(),
                    [](const std::shared_ptr<Context>& dup) { return dup && !dup->windowData; });

                if (freeIt != backendSlot.duplicates.end())
                {
                    ctx = *freeIt;
                }
                else
                {
                    auto dup = CloneContext(*backendSlot.master);
                    backendSlot.duplicates.push_back(dup);
                    ctx = std::move(dup);
                }
            }
        }

        if (!ctx)
        {
            ::ReleaseDC(hwnd, hdc);
            ::DestroyWindow(hwnd);
            return false;
        }

        ctx->type = request.type;
        ctx->hwnd = hwnd;
        ctx->hdc = hdc;
        ctx->hglrc = glrc;
        ctx->native_window = hwnd;
        ctx->native_drawable = hdc;
        ctx->native_gl_context = glrc;

        auto winPtr = std::make_unique<WindowData>(hwnd, hdc, glrc, usesSharedContext, request.type);
        winPtr->running = true;
        winPtr->context = ctx;
        winPtr->titleWide = title;
        winPtr->titleNarrow = epochengine::text::narrow_utf8(title);
        winPtr->guiRoute = request.gui_route;
        winPtr->isFloating = !startDocked;
        winPtr->firstPresentComplete.store(false, std::memory_order_release);
        ctx->windowData = winPtr.get();

        RECT rc{};
        ::GetClientRect(hwnd, &rc);
        ctx->width = clamp_positive(static_cast<int>(rc.right - rc.left));
        ctx->height = clamp_positive(static_cast<int>(rc.bottom - rc.top));
        ctx->framebufferWidth = ctx->width;
        ctx->framebufferHeight = ctx->height;
        winPtr->width = ctx->width;
        winPtr->height = ctx->height;
        if (!ctx->onResize && winPtr->onResize)
            ctx->onResize = winPtr->onResize;

        WindowData* rawWin = winPtr.get();
        {
            std::scoped_lock lock(windowsMutex);
            windows.emplace_back(std::move(winPtr));
        }

        if (rawWin)
        {
            std::scoped_lock lock(g_threadStateMutex);
            if (!g_threads.contains(hwnd))
            {
                g_threads.emplace(
                    hwnd,
                    std::thread([this, rawWin]()
                    {
                        epochengine::systems::threading::ScopedThreadActivity threadActivity{};
                        RenderLoop(*rawWin);
                    }));
            }
        }

        if (!startDocked || !delayedPlaceholderVisibility)
        {
            ::ShowWindow(hwnd, SW_SHOWNORMAL);
            ::BringWindowToTop(hwnd);
            ::SetForegroundWindow(hwnd);
            ::SetFocus(hwnd);
        }
        if (startDocked)
            ArrangeDockedWindowsGrid();
        if (createdContext)
            *createdContext = ctx;
        return true;
    }

    ATOM MultiContextManager::RegisterParentClass(HINSTANCE hInst, LPCWSTR name)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = ParentProc;
        wc.hInstance = hInst;
        wc.hIcon = ::LoadIconW(hInst, MAKEINTRESOURCEW(IDI_EPOCH_EDITOR));
        wc.lpszClassName = name;
        wc.style = CS_OWNDC;
        wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = parent_background_brush();
        return ::RegisterClassW(&wc);
    }

    ATOM MultiContextManager::RegisterChildClass(HINSTANCE hInst, LPCWSTR name)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = ChildProc;
        wc.hInstance = hInst;
        wc.hIcon = ::LoadIconW(hInst, MAKEINTRESOURCEW(IDI_EPOCH_EDITOR));
        wc.lpszClassName = name;
        wc.style = CS_OWNDC;
        wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        return ::RegisterClassW(&wc);
    }

    void MultiContextManager::SetupPixelFormat(HDC hdc)
    {
        if (!hdc) return;
        if (::GetPixelFormat(hdc) != 0)
            return;
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
        HGLRC tempCtx = ::wglCreateContext(hdc);
        if (!tempCtx) throw std::runtime_error("Failed to create temporary OpenGL context");
        if (!::wglMakeCurrent(hdc, tempCtx))
        {
            ::wglDeleteContext(tempCtx);
            throw std::runtime_error("Failed to activate temporary OpenGL context");
        }

        HGLRC finalCtx = nullptr;
        auto* createAttribs = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
            ::wglGetProcAddress("wglCreateContextAttribsARB"));

        if (createAttribs)
        {
            int attribs46[] = {
                WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
                WGL_CONTEXT_MINOR_VERSION_ARB, 6,
                WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                0
            };
            finalCtx = createAttribs(hdc, sharedContext, attribs46);

            if (!finalCtx)
            {
                int attribs41[] = {
                    WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
                    WGL_CONTEXT_MINOR_VERSION_ARB, 1,
                    WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                    0
                };
                finalCtx = createAttribs(hdc, sharedContext, attribs41);
            }
        }

        if (!finalCtx)
        {
            finalCtx = tempCtx;
            tempCtx = nullptr;

            if (sharedContext && !::wglShareLists(sharedContext, finalCtx))
            {
                ::wglMakeCurrent(nullptr, nullptr);
                ::wglDeleteContext(finalCtx);
                throw std::runtime_error("Failed to share OpenGL context");
            }
        }

        ::wglMakeCurrent(nullptr, nullptr);

        if (tempCtx)
            ::wglDeleteContext(tempCtx);

        return finalCtx;
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
        int VulkanWinCount,
        int OpenGLWinCount,
        int DirectXWinCount,
        int SoftwareWinCount,
        bool parented)
    {
        const int totalRequested = RayLibWinCount + SDLWinCount + SFMLWinCount + VulkanWinCount + OpenGLWinCount + DirectXWinCount + SoftwareWinCount;
        if (totalRequested <= 0) return false;

        uiThreadId = ::GetCurrentThreadId();
        running.store(true, std::memory_order_release);
        s_activeInstance = this;
        g_activeManager = this;

        RegisterParentClass(hInst, L"EpochParent");
        RegisterChildClass(hInst, L"EpochChild");

        epochengine::core::InitializeAllContexts();

        // ---------------- Parent (dock container) ----------------
        if (parented)
        {
            int cols = 1, rows = 1;
            while (cols * rows < totalRequested) (cols <= rows ? ++cols : ++rows);

            int clientW = cli::window_width;
            int clientH = cli::window_height;

            POINT launchPoint{};
            (void)::GetCursorPos(&launchPoint);
            RECT workArea{};
            MONITORINFO monitorInfo{};
            monitorInfo.cbSize = sizeof(MONITORINFO);
            const HMONITOR launchMonitor = ::MonitorFromPoint(
                launchPoint,
                MONITOR_DEFAULTTOPRIMARY);
            const bool hasWorkArea = launchMonitor
                && ::GetMonitorInfoW(launchMonitor, &monitorInfo) != FALSE;
            if (hasWorkArea)
                workArea = monitorInfo.rcWork;
            else
                (void)::SystemParametersInfoW(
                    SPI_GETWORKAREA,
                    0,
                    &workArea,
                    0);

            const int workWidth = (std::max)(
                1,
                static_cast<int>(workArea.right - workArea.left));
            const int workHeight = (std::max)(
                1,
                static_cast<int>(workArea.bottom - workArea.top));

            if (!cli::window_width_overridden
                && !cli::window_height_overridden
                && totalRequested == 1)
            {
                const auto desktopPolicy =
                    epochengine::platform::recommended_desktop_window(
                        epochengine::perf::tier_from_env(),
                        static_cast<std::uint32_t>(workWidth),
                        static_cast<std::uint32_t>(workHeight),
                        static_cast<std::uint32_t>(
                            std::thread::hardware_concurrency()));
                clientW = static_cast<int>(desktopPolicy.client_width);
                clientH = static_cast<int>(desktopPolicy.client_height);
            }
            else if (totalRequested > 1)
            {
                constexpr int kMargin = 24;
                if (workWidth > 1 && workHeight > 1)
                {
                    clientW = (std::max)(
                        1,
                        workWidth - (kMargin * 2));
                    clientH = (std::max)(
                        1,
                        workHeight - (kMargin * 2) - 32);
                }
                else
                {
                    clientW = cols * 640;
                    clientH = rows * 360;
                }
            }

            RECT want{ 0, 0, clientW, clientH };
            const DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN;
            ::AdjustWindowRect(&want, style, FALSE);
            const int outerWidth = want.right - want.left;
            const int outerHeight = want.bottom - want.top;
            const int launchX = workArea.right > workArea.left
                ? workArea.left + (std::max)(0, (workWidth - outerWidth) / 2)
                : CW_USEDEFAULT;
            const int launchY = workArea.bottom > workArea.top
                ? workArea.top + (std::max)(0, (workHeight - outerHeight) / 2)
                : CW_USEDEFAULT;

            parent = ::CreateWindowExW(
                0,
                L"EpochParent",
                L"",
                style,
                launchX,
                launchY,
                outerWidth,
                outerHeight,
                nullptr,
                nullptr,
                hInst,
                this);

            if (!GetParentWindow()) return false;
            apply_native_window_chrome(GetParentWindow());
            ::DragAcceptFiles(GetParentWindow(), TRUE);
        }
        else
        {
            parent = nullptr;
        }

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        // ---------------- Shared dummy GL context (for wglShareLists + glad bootstrap) ----------------
        {
            HWND dummy = ::CreateWindowExW(
                WS_EX_TOOLWINDOW,
                L"EpochChild",
                L"Dummy",
                WS_POPUP,
                0, 0, 1, 1,
                nullptr,
                nullptr,
                hInst,
                nullptr);

            if (!dummy) return false;

            HDC dummyDC = ::GetDC(dummy);
            sharedContext = CreateSharedGLContext(dummyDC);
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
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
                epochengine::logger::get(kLogSys).log(
                    epochengine::logger::LogLevel::INFO,
                    "GLAD loaded on dummy context",
                    std::source_location::current());
#endif
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
                    const std::string narrowTitle = epochengine::text::narrow_utf8(windowTitle);
                    const bool singleStandaloneWindow = (!parent && totalRequested == 1);
                    const bool updaterStandaloneWindow = singleStandaloneWindow && cli::updater_shell_requested;
                    const int initialWidth = singleStandaloneWindow ? cli::window_width : 1280;
                    const int initialHeight = singleStandaloneWindow ? cli::window_height : 1277;
                    int initialX = singleStandaloneWindow ? CW_USEDEFAULT : 0;
                    int initialY = singleStandaloneWindow ? CW_USEDEFAULT : 0;

                    if (updaterStandaloneWindow)
                    {
                        RECT workArea{};
                        if (::SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0))
                        {
                            constexpr int kMargin = 24;
                            const int workLeft = static_cast<int>(workArea.left);
                            const int workTop = static_cast<int>(workArea.top);
                            const int workRight = static_cast<int>(workArea.right);
                            const int workBottom = static_cast<int>(workArea.bottom);
                            const int minX = workLeft + kMargin;
                            const int minY = workTop + kMargin;
                            const int maxX = (std::max)(minX, workRight - initialWidth - kMargin);
                            const int maxY = (std::max)(minY, workBottom - initialHeight - kMargin);

                            if (const HWND consoleWindow = ::GetConsoleWindow())
                            {
                                RECT consoleRect{};
                                if (::GetWindowRect(consoleWindow, &consoleRect))
                                {
                                    const int consoleLeft = static_cast<int>(consoleRect.left);
                                    const int consoleTop = static_cast<int>(consoleRect.top);
                                    const int consoleRight = static_cast<int>(consoleRect.right);
                                    const int consoleBottom = static_cast<int>(consoleRect.bottom);
                                    const int rightSideX = consoleRight + kMargin;
                                    const bool fitsRight = rightSideX <= maxX;
                                    if (fitsRight)
                                    {
                                        initialX = rightSideX;
                                        initialY = (std::clamp)(consoleTop, minY, maxY);
                                    }
                                    else
                                    {
                                        initialX = (std::clamp)(consoleLeft, minX, maxX);
                                        initialY = (std::clamp)(consoleBottom + kMargin, minY, maxY);
                                    }
                                }
                            }

                            if (initialX == CW_USEDEFAULT || initialY == CW_USEDEFAULT)
                            {
                                initialX = maxX;
                                initialY = minY + 48;
                            }
                        }
                    }

                    HWND hwnd = ::CreateWindowExW(
                        0,
                        L"EpochChild",
                        windowTitle.c_str(),
                        (parent
                            ? (WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN)
                            : (WS_OVERLAPPEDWINDOW | WS_VISIBLE)),
                        initialX, initialY, initialWidth, initialHeight,
                        parent,
                        nullptr,
                        hInst,
                        nullptr);

                    if (!hwnd) continue;

                    ::SetWindowTextW(hwnd, windowTitle.c_str());

                    HDC hdc = ::GetDC(hwnd);
                    HGLRC glrc = nullptr;
                    bool usesSharedContext = false;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
                    if (type == ContextType::OpenGL)
                    {
                        glrc = CreateSharedGLContext(hdc);
                        usesSharedContext = (glrc != nullptr);
                    }
#if defined(EPOCH_USING_VULKAN) && (EPOCH_USING_VULKAN == 1)
                    else if (type == ContextType::Vulkan)
                    {
                        // Keep Vulkan windows free of WGL state to avoid WSI surface conflicts.
                        glrc = nullptr;
                        usesSharedContext = false;
                    }
#endif
#endif

                    auto winPtr = std::make_unique<WindowData>(hwnd, hdc, glrc, usesSharedContext, type);
                    winPtr->running = true;
                    winPtr->titleWide = windowTitle;
                    winPtr->titleNarrow = narrowTitle;

                    if (parent && !backend_uses_proxy_child(type))
                        MakeDockable(hwnd, parent);

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
                        epochengine::logger::get(kLogSys).logf(
                            epochengine::logger::LogLevel::Error,
                            std::source_location::current(),
                            "Missing prototype context for backend type {}",
                            static_cast<int>(type));
                        return;
                    }

                    core::BackendState& backendSlot = it->second;
                    master = backendSlot.master;

                    for (auto& dup : backendSlot.duplicates)
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
                    ctx->framebufferWidth = width;
                    ctx->framebufferHeight = height;

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
                            : epochengine::text::narrow_utf8(backend::BuildChildWindowTitle(type, static_cast<int>(i)));
                    }

                    if (w && w->titleNarrow.empty())
                        w->titleNarrow = narrowTitle;

                    // ---------------- Backend initialization policy ----------------
                    // OpenGL uses the placeholder HWND + wgl contexts -> safe to init now.
                    // Raylib/SDL create their own HWND/GL context internally -> MUST init on the render thread.
                    switch (type)
                    {
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
                    case ContextType::OpenGL:
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
                        epochengine::logger::get(kLogSys).logf(
                            epochengine::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Deferring OpenGL init to render thread. host={}",
                            static_cast<void*>(hwnd));
#endif
                        break;
#endif
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)

                    case ContextType::Software:
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
                        epochengine::logger::get(kLogSys).logf(
                            epochengine::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Deferring Software init to render thread. host={}",
                            static_cast<void*>(hwnd));
#endif
                        break;
#endif
#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
                    case ContextType::DirectX:
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
                        epochengine::logger::get(kLogSys).logf(
                            epochengine::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Deferring DirectX init to render thread. host={}",
                            static_cast<void*>(hwnd));
#endif
                        break;
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
                    case ContextType::RayLib:
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
                        epochengine::logger::get(kLogSys).logf(
                            epochengine::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Deferring Raylib init to render thread. host={}",
                            static_cast<void*>(hwnd));
#endif
                        break;
#endif
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
                    case ContextType::SDL:
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
                        epochengine::logger::get(kLogSys).logf(
                            epochengine::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Deferring SDL init to render thread. host={}",
                            static_cast<void*>(hwnd));
#endif
                        break;
#endif
#if defined(EPOCH_USING_VULKAN) && (EPOCH_USING_VULKAN == 1)
                    case ContextType::Vulkan:
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
                        epochengine::logger::get(kLogSys).logf(
                            epochengine::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Deferring Vulkan init to render thread. host={}",
                            static_cast<void*>(hwnd));
#endif
                        break;
#endif
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
                    case ContextType::SFML:
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
                        epochengine::logger::get(kLogSys).logf(
                            epochengine::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Deferring SFML init to render thread. host={}",
                            static_cast<void*>(hwnd));
#endif
                        break;
#endif

                    default:
                        break;
                    }
                }
            };

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        make_backend_windows(ContextType::RayLib, RayLibWinCount);
#endif
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        make_backend_windows(ContextType::SDL, SDLWinCount);
#endif
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
        make_backend_windows(ContextType::SFML, SFMLWinCount);
#endif
#if defined(EPOCH_USING_VULKAN) && (EPOCH_USING_VULKAN == 1)
        make_backend_windows(ContextType::Vulkan, VulkanWinCount);
#endif
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        make_backend_windows(ContextType::OpenGL, OpenGLWinCount);
#endif
#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
        make_backend_windows(ContextType::DirectX, DirectXWinCount);
#endif
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)

        make_backend_windows(ContextType::Software, SoftwareWinCount);
#endif
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
        g_activeManager = this;

        if (!hdc) hdc = ::GetDC(hwnd);

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        if (type == ContextType::OpenGL && !glContext)
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
#if defined(EPOCH_USING_VULKAN) && (EPOCH_USING_VULKAN == 1)
        if (type == ContextType::Vulkan)
        {
            // Keep docked Vulkan windows free of WGL state too.
            glContext = nullptr;
            usesSharedContext = false;
        }
#endif
#endif

        if (parentWnd && !backend_uses_proxy_child(type))
            MakeDockable(hwnd, parentWnd);

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
            auto& backendSlot = g_backends[type];

            if (!backendSlot.master)
            {
                ctx = std::make_shared<Context>();
                ctx->type = type;
                backendSlot.master = ctx;
            }
            else if (!backendSlot.master->windowData)
            {
                ctx = backendSlot.master;
            }
            else
            {
                auto it = std::find_if(backendSlot.duplicates.begin(), backendSlot.duplicates.end(),
                    [](const std::shared_ptr<Context>& dup) { return dup && !dup->windowData; });

                if (it != backendSlot.duplicates.end()) ctx = *it;
                else
                {
                    auto dup = CloneContext(*backendSlot.master);
                    backendSlot.duplicates.push_back(dup);
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
        winPtr->firstPresentComplete.store(false, std::memory_order_release);
        ctx->windowData = winPtr.get();

        RECT rc{};
        ::GetClientRect(hwnd, &rc);
        ctx->width = clamp_positive(static_cast<int>(rc.right - rc.left));
        ctx->height = clamp_positive(static_cast<int>(rc.bottom - rc.top));
        ctx->framebufferWidth = ctx->width;
        ctx->framebufferHeight = ctx->height;
        winPtr->width = ctx->width;
        winPtr->height = ctx->height;

        if (!ctx->onResize && winPtr->onResize) ctx->onResize = winPtr->onResize;
        if (type != ContextType::RayLib
            && !winPtr->onResize && ctx->onResize)
            winPtr->onResize = ctx->onResize;

        WindowData* rawWin = winPtr.get();
        {
            std::scoped_lock lock(windowsMutex);
            windows.emplace_back(std::move(winPtr));
        }

        if (rawWin)
        {
            std::scoped_lock lock(g_threadStateMutex);
            if (!g_threads.contains(hwnd))
            {
                g_threads.emplace(
                    hwnd,
                    std::thread([this, rawWin]()
                    {
                        epochengine::systems::threading::ScopedThreadActivity threadActivity{};
                        RenderLoop(*rawWin);
                    }));
            }
        }

        ArrangeDockedWindowsGrid();
    }

    bool MultiContextManager::AddExternalProcessWindow(
        const std::uintptr_t nativeWindow,
        const std::uint64_t processId,
        std::string route,
        const RoutedPanelDockTarget dockTarget)
    {
        const HWND hwnd = reinterpret_cast<HWND>(nativeWindow);
        if (!running.load(std::memory_order_acquire)
            || !parent || ::IsWindow(parent) == FALSE
            || !hwnd || ::IsWindow(hwnd) == FALSE
            || processId == 0u || processId > MAXDWORD || route.empty()
            || uiThreadId == 0 || ::GetCurrentThreadId() != uiThreadId)
        {
            return false;
        }

        DWORD observedProcessId{}, hostProcessId{};
        const DWORD ownerThreadId = ::GetWindowThreadProcessId(parent, &hostProcessId);
        (void)::GetWindowThreadProcessId(hwnd, &observedProcessId);
        if (observedProcessId == 0u
            || static_cast<std::uint64_t>(observedProcessId) != processId
            || hostProcessId != ::GetCurrentProcessId() || ownerThreadId != uiThreadId)
        {
            return false;
        }

        {
            std::scoped_lock lock(windowsMutex);
            const auto existing = std::ranges::find_if(
                    windows,
                    [hwnd](const std::unique_ptr<WindowData>& window)
                    {
                        return matches_window_handle(window.get(), hwnd);
                    });
            if (existing != windows.end())
            {
                const auto attachment = external_attachment(existing->get());
                return attachment && attachment->process_id() == observedProcessId
                    && attachment->host().value == reinterpret_cast<std::uintptr_t>(parent) && (*existing)->guiRoute == route
                    && (*existing)->backend_ready() && attachment->placement_current();
            }
        }

        auto admittedAttachment = epochengine::platform::window_adapter::attach_native_window(
            {nativeWindow}, {reinterpret_cast<std::uintptr_t>(parent)}, observedProcessId);
        if (!admittedAttachment) return false;
        auto attachment = std::move(*admittedAttachment);
        auto window = std::make_unique<WindowData>(
            hwnd, nullptr, nullptr, false, ContextType::Custom);
        window->running.store(true, std::memory_order_release);
        window->guiRoute = std::move(route);
        window->titleNarrow =
            "Sandbox Candidate | PID " + std::to_string(processId);
        window->dockTargetPreference.store(
            static_cast<std::uint8_t>(dockTarget),
            std::memory_order_release);
        window->set_backend_lifecycle(BackendLifecycleState::ready);
        // Native attachment is not render/present evidence. The external
        // process owns its frame loop; these counters deliberately remain zero.
        WindowData* admittedWindow = window.get();
        try
        {
            std::scoped_lock lock(windowsMutex, g_externalAttachmentsMutex);
            windows.reserve(windows.size() + 1);
            g_externalAttachments.emplace(admittedWindow, attachment);
            // Capacity is reserved and unique_ptr movement is noexcept: a
            // foreign HWND is never published without its ownership record.
            windows.emplace_back(std::move(window));
        }
        catch (...)
        {
            return false; // The unpublished lease restores its native state.
        }
        ArrangeDockedWindowsGrid();
        {
            std::scoped_lock lock(windowsMutex);
            const auto retained = std::ranges::find_if(windows,
                [admittedWindow](const auto& item) { return item.get() == admittedWindow; });
            if (retained != windows.end() && (*retained)->backend_ready() && attachment->placement_current())
                return true;
        }
        RemoveWindow(hwnd);
        return false;
    }

    bool MultiContextManager::IsExternalProcessWindowAttached(
        const std::uintptr_t nativeWindow,
        const std::uint64_t processId,
        const std::string_view route) const noexcept
    {
        const HWND hwnd = reinterpret_cast<HWND>(nativeWindow);
        if (!running.load(std::memory_order_acquire) || !hwnd || !parent
            || processId == 0 || processId > MAXDWORD || route.empty())
            return false;
        DWORD hostProcessId{};
        if (::GetWindowThreadProcessId(parent, &hostProcessId) != uiThreadId
            || uiThreadId == 0 || hostProcessId != ::GetCurrentProcessId())
            return false;
        std::scoped_lock lock(windowsMutex);
        for (const auto& window : windows)
        {
            if (!window || window->hwnd != hwnd || window->guiRoute != route)
                continue;
            const auto attachment = external_attachment(window.get());
            return attachment && attachment->window().value == reinterpret_cast<std::uintptr_t>(hwnd)
                && attachment->host().value == reinterpret_cast<std::uintptr_t>(parent) && attachment->process_id() == processId
                && window->running.load(std::memory_order_acquire)
                && !window->should_close.load(std::memory_order_acquire)
                && window->backend_ready() && attachment->placement_current();
        }
        return false;
    }

    void MultiContextManager::RemoveExternalProcessWindow(
        const std::uintptr_t nativeWindow, const bool retiring)
    {
        const HWND hwnd = reinterpret_cast<HWND>(nativeWindow);
        if (!hwnd) return;
        {
            std::scoped_lock lock(windowsMutex);
            const auto found = std::ranges::find_if(windows,
                [hwnd](const std::unique_ptr<WindowData>& window)
                {
                    const auto attachment = external_attachment(window.get());
                    return attachment && attachment->window().value == reinterpret_cast<std::uintptr_t>(hwnd);
                });
            if (found == windows.end()) return;
            if (retiring)
            {
                if (auto attachment = external_attachment(found->get()))
                    attachment->hide_during_retirement(true);
            }
        }
        RemoveWindow(hwnd);
    }

    void MultiContextManager::HandleResize(HWND hwnd, int width, int height)
    {
        if (!hwnd) return;

        int clampedWidth = clamp_positive(width);
        int clampedHeight = clamp_positive(height);
        if (clampedWidth <= 1 || clampedHeight <= 1)
            backend::ResolveClientSize(hwnd, clampedWidth, clampedHeight);

        std::function<void(int, int)> resizeCallback;
        core::ContextType contextType = core::ContextType::None;
        std::uintptr_t windowId = 0;
        WindowData* window = nullptr;
        std::uint64_t resizeGeneration = 0;

        {
            std::scoped_lock lock(windowsMutex);
            auto it = std::find_if(windows.begin(), windows.end(),
                [hwnd](const std::unique_ptr<WindowData>& w) { return matches_window_handle(w.get(), hwnd); });
            if (it == windows.end()) return;

            window = it->get();
            if (window->width == clampedWidth && window->height == clampedHeight)
                return;

            window->width = clampedWidth;
            window->height = clampedHeight;
            resizeGeneration = window->resizeGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;

            if (window->context)
            {
                if (auto liveContext = typed_context(window->context))
                {
                    liveContext->width = clampedWidth;
                    liveContext->height = clampedHeight;
                    contextType = liveContext->type;
                    if (liveContext->onResize) resizeCallback = liveContext->onResize;
                }
            }
            else
            {
                contextType = window->type;
            }

            if (!resizeCallback && window->onResize) resizeCallback = window->onResize;

            if (const HWND liveHwnd = primary_window_handle(window))
                windowId = reinterpret_cast<std::uintptr_t>(liveHwnd);
        }

        if (window)
        {
            const bool ownerThreadResize =
                backend_requires_owner_thread_dock_commands(window);
            if (ownerThreadResize)
                window->firstPresentComplete.store(false, std::memory_order_release);

            auto& resizeQueue = ownerThreadResize
                ? window->ownerThreadCommandQueue
                : window->commandQueue;
            resizeQueue.enqueue([
                cb = std::move(resizeCallback),
                contextType,
                windowId,
                resizeGeneration,
                window,
                w = clampedWidth,
                h = clampedHeight
            ]() mutable
                {
                    if (window && window->resizeGeneration.load(std::memory_order_acquire) != resizeGeneration)
                        return;

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
                if (const HWND key = render_thread_key(w.get())) hwnds.push_back(key);
        }

        std::size_t launchIndex = 0;
        for (HWND hwnd : hwnds)
        {
            const auto startupDelay =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    kRenderThreadStartupStepDelay * static_cast<int>(launchIndex++));

            std::scoped_lock lock(g_threadStateMutex);
            if (g_threads.contains(hwnd))
                continue;

            g_threads.emplace(hwnd, std::thread([this, hwnd, startupDelay]()
                {
                    epochengine::systems::threading::ScopedThreadActivity threadActivity{};
                    if (startupDelay.count() > 0)
                    {
                        epochengine::logger::get(kLogSys).logf(
                            epochengine::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Startup stagger: delaying render thread {} by {} ms.",
                            static_cast<void*>(hwnd),
                            startupDelay.count());
                        std::this_thread::sleep_for(startupDelay);
                    }

                    if (!running.load(std::memory_order_acquire))
                        return;

                    WindowData* win = nullptr;
                    {
                        std::scoped_lock lock(windowsMutex);
                        auto it = std::find_if(windows.begin(), windows.end(),
                            [hwnd](const std::unique_ptr<WindowData>& w)
                            {
                                return render_thread_key(w.get()) == hwnd;
                            });
                        if (it != windows.end()) win = it->get();
                    }

                    if (win) RenderLoop(*win);
                }));
        }
    }

    void MultiContextManager::RemoveWindow(HWND hwnd)
    {
        std::unique_ptr<WindowData> removed;
        PendingWindowCleanup replacementCleanup{};
        bool hasReplacementCleanup = false;
        bool should_quit = false;
        HWND layoutParent = nullptr;
        const bool synchronousReplacementRetirement =
            contextReplacementHolds.load(std::memory_order_acquire) > 0;

        // StopAll owns every active WindowData until its render thread has
        // joined. Native destruction during that phase must not transfer the
        // same object into the ordinary deferred-cleanup lane.
        if (!running.load(std::memory_order_acquire))
        {
            std::scoped_lock lock(windowsMutex);
            auto it = std::find_if(windows.begin(), windows.end(),
                [hwnd](const std::unique_ptr<WindowData>& w) { return matches_window_handle(w.get(), hwnd); });
            if (it != windows.end())
            {
                (*it)->set_should_close(true);
                (*it)->running = false;
            }
            return;
        }

        // Backend-owned HWND procedures execute on renderer threads. The
        // session/event loop reads the active window vector on uiThreadId, so
        // renderer threads may only mark retirement and queue the ownership
        // transfer back to that thread. Mutating the vector here was a Release
        // heap race during rapid context replacement.
        if (uiThreadId != 0 && ::GetCurrentThreadId() != uiThreadId)
        {
            HWND retirementHost = nullptr;
            {
                std::scoped_lock lock(windowsMutex);
                auto it = std::find_if(windows.begin(), windows.end(),
                    [hwnd](const std::unique_ptr<WindowData>& w) { return matches_window_handle(w.get(), hwnd); });
                if (it == windows.end())
                    return;

                (*it)->set_should_close(true);
                (*it)->running = false;
                if ((*it)->retirementQueued.exchange(true, std::memory_order_acq_rel))
                    return;

                retirementHost = parent;
                if (!retirementHost || ::IsWindow(retirementHost) == FALSE)
                {
                    const HWND candidate = (*it)->host_hwnd
                        ? ::GetParent((*it)->host_hwnd)
                        : ((*it)->hwndChild
                            ? ::GetParent((*it)->hwndChild)
                            : ::GetParent((*it)->hwnd));
                    if (candidate && ::IsWindow(candidate) != FALSE)
                        retirementHost = candidate;
                }
            }

            bool posted = false;
            if (retirementHost && ::IsWindow(retirementHost) != FALSE)
            {
                posted = ::PostMessageW(
                    retirementHost,
                    WM_EPOCH_RETIRE_CONTEXT_WINDOW,
                    0,
                    reinterpret_cast<LPARAM>(hwnd)) != FALSE;
            }

            if (!posted)
            {
                std::scoped_lock lock(windowsMutex);
                auto it = std::find_if(windows.begin(), windows.end(),
                    [hwnd](const std::unique_ptr<WindowData>& w) { return matches_window_handle(w.get(), hwnd); });
                if (it != windows.end())
                    (*it)->retirementQueued.store(false, std::memory_order_release);
            }
            return;
        }

        {
            std::scoped_lock lock(windowsMutex, g_threadStateMutex);
            auto it = std::find_if(windows.begin(), windows.end(),
                [hwnd](const std::unique_ptr<WindowData>& w) { return matches_window_handle(w.get(), hwnd); });
            if (it == windows.end()) return;

            if ((*it)->host_hwnd && ::IsWindow((*it)->host_hwnd) != FALSE)
                layoutParent = ::GetParent((*it)->host_hwnd);
            if (!layoutParent && (*it)->hwndChild && ::IsWindow((*it)->hwndChild) != FALSE)
                layoutParent = ::GetParent((*it)->hwndChild);
            if (!layoutParent && (*it)->hwnd && ::IsWindow((*it)->hwnd) != FALSE)
                layoutParent = ::GetParent((*it)->hwnd);

            (*it)->set_should_close(true);
            (*it)->running = false;
            (*it)->retirementQueued.store(true, std::memory_order_release);

            removed = std::move(*it);
            windows.erase(it);
            should_quit = windows.empty()
                && contextReplacementHolds.load(std::memory_order_acquire) == 0;

            HWND threadKey = hwnd;
            if (!g_threads.contains(threadKey) && removed)
            {
                if (removed->host_hwnd && g_threads.contains(removed->host_hwnd))
                    threadKey = removed->host_hwnd;
                else if (removed->hwndChild && g_threads.contains(removed->hwndChild))
                    threadKey = removed->hwndChild;
                else if (removed->hwnd && g_threads.contains(removed->hwnd))
                    threadKey = removed->hwnd;
            }

            auto threadIt = g_threads.find(threadKey);
            if (threadIt != g_threads.end())
            {
                PendingWindowCleanup pending{};
                pending.hwnd = threadKey;
                pending.thread = std::move(threadIt->second);
                pending.window = std::move(removed);
                g_threads.erase(threadIt);
                if (synchronousReplacementRetirement)
                {
                    replacementCleanup = std::move(pending);
                    hasReplacementCleanup = true;
                }
                else
                {
                    g_pendingCleanups.emplace_back(std::move(pending));
                }
            }
        }

        if (hasReplacementCleanup)
        {
            // A whole-editor replacement must not overlap the old renderer's
            // cleanup with construction of the new backend. Keep the UI
            // message pump alive while joining so owner-thread HWND teardown
            // and synchronous Win32 messages can complete without deadlock.
            join_thread_with_message_pump(replacementCleanup.thread);
            forget_native_title_frame_source(replacementCleanup.window.get());
            cleanup_window_resources(replacementCleanup.window);
        }
        else if (removed)
        {
            forget_native_title_frame_source(removed.get());
            cleanup_window_resources(removed);
        }

        CleanupFinishedWindows();

        if (!should_quit)
        {
            if ((!layoutParent || ::IsWindow(layoutParent) == FALSE)
                && GetParentWindow()
                && ::IsWindow(GetParentWindow()) != FALSE)
            {
                layoutParent = GetParentWindow();
            }

            if (layoutParent && ::IsWindow(layoutParent) != FALSE)
                request_parent_layout(layoutParent);
        }

        if (should_quit)
        {
            running.store(false, std::memory_order_release);

            if (uiThreadId != 0 && uiThreadId != ::GetCurrentThreadId())
                ::PostThreadMessageW(uiThreadId, WM_QUIT, 0, 0);
            else
                ::PostQuitMessage(0);
        }
    }

    void MultiContextManager::CleanupFinishedWindows()
    {
        std::vector<PendingWindowCleanup> finished;
        {
            std::scoped_lock lock(g_threadStateMutex);
            auto it = g_pendingCleanups.begin();
            while (it != g_pendingCleanups.end())
            {
                if (!thread_finished(it->thread))
                {
                    ++it;
                    continue;
                }

                finished.emplace_back(std::move(*it));
                it = g_pendingCleanups.erase(it);
            }
        }

        for (auto& pending : finished)
        {
            if (pending.thread.joinable())
                pending.thread.join();
            forget_native_title_frame_source(pending.window.get());
            cleanup_window_resources(pending.window);
        }
    }

    static int native_dock_order(const WindowData* win) noexcept
        {
            if (!win)
                return 999;

            const auto preferredTarget = static_cast<RoutedPanelDockTarget>(
                win->dockTargetPreference.load(std::memory_order_acquire));
            switch (preferredTarget)
            {
            case RoutedPanelDockTarget::left_tabs:
            case RoutedPanelDockTarget::left_context:
                return 0;
            case RoutedPanelDockTarget::right_tabs:
            case RoutedPanelDockTarget::right_context:
                return 200;
            case RoutedPanelDockTarget::bottom_left_tabs:
            case RoutedPanelDockTarget::bottom_right_tabs:
                return 300;
            case RoutedPanelDockTarget::none:
            default:
                break;
            }

            const int manualOrder = win->dockGridOrder.load(std::memory_order_acquire);
            if (manualOrder >= 0) return 100 + manualOrder;

            const auto liveContext = (win && win->context) ? typed_context(win->context) : nullptr;
            const auto type = liveContext ? liveContext->type : ContextType::None;
            switch (type)
            {
            case ContextType::RayLib: return 100;
            case ContextType::SDL: return 101;
            case ContextType::SFML: return 102;
            case ContextType::Vulkan: return 103;
            case ContextType::OpenGL: return 104;
            case ContextType::DirectX: return 105;
            case ContextType::Software: return 106;
            default: return 99;
            }
    }


    void MultiContextManager::ReorderDockedWindow(HWND hwnd, POINT screenPoint)
    {
        auto* moved = findWindowByHWND(hwnd);
        if (!parent || !moved || !moved->guiRoute.empty()) return;
        const RECT bounds = screen_client_rect(parent);
        if (!point_in_rect(bounds, screenPoint)) return;
        std::vector<WindowData*> ordered;
        {
            std::scoped_lock lock(windowsMutex);
            for (const auto& win : windows)
                if (win && win->guiRoute.empty()
                    && (win.get() == moved || dock_slot_handle(win.get(), parent)))
                    ordered.push_back(win.get());
        }
        std::stable_sort(ordered.begin(), ordered.end(), [](auto* lhs, auto* rhs) {
            return native_dock_order(lhs) < native_dock_order(rhs);
        });
        const auto found = std::find(ordered.begin(), ordered.end(), moved);
        if (found == ordered.end()) return;
        int cols = 1, rows = 1;
        while (cols * rows < static_cast<int>(ordered.size())) (cols <= rows ? ++cols : ++rows);
        const int column = (std::min)(cols - 1, static_cast<int>(screenPoint.x - bounds.left)
            / clamp_positive(static_cast<int>(bounds.right - bounds.left) / cols));
        const int row = (std::min)(rows - 1, static_cast<int>(screenPoint.y - bounds.top)
            / clamp_positive(static_cast<int>(bounds.bottom - bounds.top) / rows));
        const auto target = (std::min)(ordered.size() - 1, static_cast<size_t>(row * cols + column));
        const auto old = static_cast<size_t>(found - ordered.begin());
        if (old < target) std::rotate(ordered.begin() + old, ordered.begin() + old + 1, ordered.begin() + target + 1);
        else if (target < old) std::rotate(ordered.begin() + target, ordered.begin() + old, ordered.begin() + old + 1);
        for (size_t index = 0; index < ordered.size(); ++index)
        {
            ordered[index]->dockTargetPreference.store(0u, std::memory_order_release);
            ordered[index]->dockGridOrder.store(static_cast<int>(index), std::memory_order_release);
        }
    }

    void MultiContextManager::ArrangeDockedWindowsGrid()
    {
        if (!parent) return;

        std::vector<WindowData*> dockedWindows;
        {
            std::scoped_lock lock(windowsMutex);
            if (windows.empty()) return;

            dockedWindows.reserve(windows.size());
            for (auto& win : windows)
            {
                if (win)
                    dockedWindows.push_back(win.get());
            }
        }

        // Window retirement transfers ownership to the UI thread before
        // erasing registry entries. Snapshot registry-owned pointers while
        // locked, then perform native window work without holding the registry
        // mutex: SetWindowPos can synchronously enter a backend WndProc, and
        // HandleResize must reacquire windowsMutex to publish the new size.
        std::erase_if(
            dockedWindows,
            [this](WindowData* win)
            {
                if (const auto attachment = external_attachment(win);
                    attachment && !attachment->attached())
                    return true;
                const HWND liveHwnd = dock_slot_handle(win, parent);
                return !liveHwnd || ::IsWindow(liveHwnd) == FALSE;
            });

        if (dockedWindows.empty())
            return;


        std::stable_sort(
            dockedWindows.begin(),
            dockedWindows.end(),
            [&](const WindowData* lhs, const WindowData* rhs)
            {
                return native_dock_order(lhs) < native_dock_order(rhs);
            });

        const int total = static_cast<int>(dockedWindows.size());
        int cols = 1, rows = 1;
        while (cols * rows < total) (cols <= rows ? ++cols : ++rows);

        RECT rcClient{};
        ::GetClientRect(parent, &rcClient);
        const int clientW = rcClient.right - rcClient.left;
        const int clientH = rcClient.bottom - rcClient.top;

        const int cw = clamp_positive(clientW / cols);
        const int ch = clamp_positive(clientH / rows);

        const auto client_size_or_slot = [](HWND hwnd, int fallbackW, int fallbackH) noexcept
        {
            RECT rc{};
            if (hwnd && ::IsWindow(hwnd) != FALSE && ::GetClientRect(hwnd, &rc))
            {
                const int w = clamp_positive(static_cast<int>(rc.right - rc.left));
                const int h = clamp_positive(static_cast<int>(rc.bottom - rc.top));
                return SIZE{ w, h };
            }

            return SIZE{ clamp_positive(fallbackW), clamp_positive(fallbackH) };
        };

        const auto redraw_window_tree = [](HWND hwnd) noexcept
        {
            if (!hwnd || ::IsWindow(hwnd) == FALSE)
                return;

            ::RedrawWindow(
                hwnd,
                nullptr,
                nullptr,
                RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_NOERASE);
        };

        for (size_t i = 0; i < dockedWindows.size(); ++i)
        {
            const int c = static_cast<int>(i) % cols;
            const int r = static_cast<int>(i) / cols;
            const int slotX = c * cw;
            const int slotY = r * ch;
            POINT slotScreen{ slotX, slotY };
            ::ClientToScreen(parent, &slotScreen);

            WindowData& win = *dockedWindows[i];
            if (const auto attachment = external_attachment(&win))
            {
                // Foreign placement has no local render callback or command
                // queue. Verify synchronous native geometry and publish only
                // dimensions; never synthesize a rendered frame generation.
                if (attachment->place(slotX, slotY, cw, ch))
                    win.set_size(cw, ch);
                else
                {
                    win.set_backend_lifecycle(BackendLifecycleState::failed);
                    win.running.store(false, std::memory_order_release);
                }
                continue;
            }
            if (!backend_window_exposable(&win))
            {
                if (win.hwndChild && ::IsWindow(win.hwndChild) != FALSE)
                    ::ShowWindow(win.hwndChild, SW_HIDE);
                if (win.host_hwnd && ::IsWindow(win.host_hwnd) != FALSE)
                    ::ShowWindow(win.host_hwnd, SW_HIDE);
                continue;
            }

            if ((win.type == ContextType::SDL || win.type == ContextType::SFML)
                && has_proxy_shell_pair(&win))
            {
                const HWND proxySlot = dock_slot_handle(&win, parent);
                bool childFitsHost = true;
                if (::GetParent(win.hwndChild) == win.host_hwnd)
                {
                    RECT hostClient{};
                    if (::GetClientRect(win.host_hwnd, &hostClient) != FALSE)
                    {
                        childFitsHost = child_window_matches_parent_slot(
                            win.hwndChild,
                            win.host_hwnd,
                            0,
                            0,
                            clamp_positive(static_cast<int>(hostClient.right - hostClient.left)),
                            clamp_positive(static_cast<int>(hostClient.bottom - hostClient.top)));
                    }
                }

                if (!child_window_matches_parent_slot(proxySlot, parent, slotX, slotY, cw, ch)
                    || !childFitsHost
                    || win.width != cw
                    || win.height != ch)
                {
                    post_proxy_host_command(&win, ProxyDockCmd::Redock, parent, slotScreen.x, slotScreen.y, cw, ch);
                    win.set_size(cw, ch);
                }
                else if (uses_visible_proxy_host(&win))
                {
                    ::ShowWindow(win.host_hwnd, SW_SHOWNA);
                    ::ShowWindow(win.hwndChild, SW_SHOWNA);
                }
                else
                {
                    ::ShowWindow(win.hwndChild, SW_SHOWNA);
                    ::ShowWindow(win.host_hwnd, SW_HIDE);
                }
                continue;
            }

            const HWND liveHwnd = dock_slot_handle(&win, parent);
            if (!liveHwnd || ::IsWindow(liveHwnd) == FALSE)
                continue;

            const bool raylibOwnerThreadWindow =
                backend_requires_owner_thread_dock_commands(&win)
                && win.hwndChild
                && liveHwnd == win.hwndChild
                && win.host_hwnd
                && win.host_hwnd != liveHwnd;
            if (raylibOwnerThreadWindow)
            {
                const bool placementMatches = child_window_matches_parent_slot(
                    liveHwnd,
                    parent,
                    slotX,
                    slotY,
                    cw,
                    ch);
                if (!placementMatches || win.width != cw || win.height != ch)
                {
                    // Raylib owns this GLFW HWND on the render thread. Queue
                    // the complete layout mutation for its owner instead of
                    // entering GLFW's window procedure from the host thread.
                    if (post_owner_thread_dock_command(
                        &win,
                        ProxyDockCmd::Redock,
                        parent,
                        slotScreen.x,
                        slotScreen.y,
                        cw,
                        ch))
                    {
                        HandleResize(liveHwnd, cw, ch);
                    }
                }

                if (::IsWindow(win.host_hwnd) != FALSE
                    && ::GetParent(win.host_hwnd) == parent)
                {
                    ::ShowWindow(win.host_hwnd, SW_HIDE);
                }
                continue;
            }

            const bool usingHiddenHostPlaceholder =
                win.host_hwnd
                && liveHwnd == win.host_hwnd
                && (
                    (win.hwndChild
                        && ::IsWindow(win.hwndChild) != FALSE
                        && ::GetParent(win.hwndChild) != parent
                        && ::GetParent(win.hwndChild) != win.host_hwnd)
                    || ((win.type == ContextType::SDL || win.type == ContextType::SFML)
                        && (!win.hwndChild || ::IsWindow(win.hwndChild) == FALSE))
                );

            UINT layoutFlags = SWP_NOZORDER | SWP_NOACTIVATE;
            if (win.type == ContextType::SDL || win.type == ContextType::SFML)
                layoutFlags |= SWP_ASYNCWINDOWPOS;
            if (!usingHiddenHostPlaceholder)
                layoutFlags |= SWP_SHOWWINDOW;

            ::SetWindowPos(liveHwnd, nullptr, slotX, slotY, cw, ch, layoutFlags);

            if (usingHiddenHostPlaceholder)
                ::ShowWindow(liveHwnd, SW_HIDE);

            if (win.host_hwnd
                && win.host_hwnd != liveHwnd
                && ::IsWindow(win.host_hwnd) != FALSE
                && ::GetParent(win.host_hwnd) == parent)
            {
                ::ShowWindow(win.host_hwnd, SW_HIDE);
            }

            if (win.host_hwnd
                && win.host_hwnd != liveHwnd
                && ::IsWindow(win.host_hwnd) != FALSE
                && ::GetParent(win.host_hwnd) != parent)
            {
                restore_associated_host_window(&win, parent, slotScreen.x, slotScreen.y, cw, ch);
            }

            if (win.host_hwnd
                && win.hwndChild
                && ::IsWindow(win.host_hwnd) != FALSE
                && ::IsWindow(win.hwndChild) != FALSE
                && ::GetParent(win.hwndChild) == win.host_hwnd)
            {
                const SIZE hostClient = client_size_or_slot(win.host_hwnd, cw, ch);
                apply_child_fill_layout(win.hwndChild, win.host_hwnd, hostClient.cx, hostClient.cy);
                redraw_window_tree(win.hwndChild);
            }

            const SIZE liveClient = client_size_or_slot(liveHwnd, cw, ch);
            if (win.width != liveClient.cx || win.height != liveClient.cy)
                HandleResize(liveHwnd, liveClient.cx, liveClient.cy);

            redraw_window_tree(liveHwnd);
            hide_associated_host_window(&win, liveHwnd, parent);
        }
    }

    void MultiContextManager::StopAll()
    {
        running.store(false, std::memory_order_release);

        {
            std::scoped_lock lock(windowsMutex);
            for (auto& w : windows)
            {
                if (!w)
                    continue;

                w->running = false;
                w->set_should_close(true);
                if (auto liveContext = typed_context(w->context);
                    liveContext && liveContext->windowData == w.get())
                {
                    liveContext->windowData->set_should_close(true);
                }
            }
        }

        std::unordered_map<HWND, std::thread> activeThreads;
        std::vector<PendingWindowCleanup> pendingCleanups;
        {
            std::scoped_lock lock(g_threadStateMutex);
            activeThreads.swap(g_threads);
            pendingCleanups.swap(g_pendingCleanups);
        }

        for (auto& [hwnd, th] : activeThreads)
        {
            core::ContextType joinType = core::ContextType::None;
            std::string joinTitle{};
            {
                std::scoped_lock lock(windowsMutex);
                for (const auto& w : windows)
                {
                    if (!w || !matches_window_handle(w.get(), hwnd))
                        continue;

                    joinType = w->type;
                    joinTitle = w->titleNarrow;
                    break;
                }
            }
#if defined(_DEBUG)
            if (th.joinable())
            {
                epochengine::logger::get(kLogSys).logf(
                    epochengine::logger::LogLevel::INFO,
                    std::source_location::current(),
                    "StopAll joining render thread hwnd={} type={} title='{}'",
                    static_cast<void*>(hwnd),
                    static_cast<int>(joinType),
                    joinTitle);
            }
#endif
            join_thread_with_message_pump(th);
#if defined(_DEBUG)
            epochengine::logger::get(kLogSys).logf(
                epochengine::logger::LogLevel::INFO,
                std::source_location::current(),
                "StopAll joined render thread hwnd={}",
                static_cast<void*>(hwnd));
#endif
        }

        for (auto& pending : pendingCleanups)
        {
            if (pending.thread.joinable())
                join_thread_with_message_pump(pending.thread);
            forget_native_title_frame_source(pending.window.get());
            cleanup_window_resources(pending.window);
        }

        std::vector<std::unique_ptr<WindowData>> remainingWindows;
        {
            std::scoped_lock lock(windowsMutex);
            remainingWindows.swap(windows);
        }
        for (auto& window : remainingWindows)
        {
            forget_native_title_frame_source(window.get());
            cleanup_window_resources(window);
        }

        if (s_activeInstance == this) s_activeInstance = nullptr;
        if (g_activeManager == this) g_activeManager = nullptr;
    }

#include "implementation/host.render_loop.inl"
    void MultiContextManager::HandleDropFiles(HWND source, HDROP hDrop)
    {
        platform_file_drop::OwnedDrop drop{hDrop};
        try
        {
            POINT point{};
            if (!::DragQueryPoint(drop.get(), &point)) return;
            ::ClientToScreen(source, &point);
            HWND target = ::WindowFromPoint(point);
            WindowData* window{};
            while (target && !window)
            {
                window = findWindowByHWND(target);
                if (!window) target = ::GetParent(target);
            }
            const auto ctx = window ? typed_context(window->context) : nullptr;
            if (!ctx || !target) return;
            ::ScreenToClient(target, &point);
            int x = point.x, y = point.y;
            if (ctx->normalize_mouse_position) ctx->normalize_mouse_position(x, y);
            auto files = drop.files();
            if (!files.empty())
                gui::push_input_for_context(ctx.get(), gui::InputEvent{
                    .type = gui::EventType::FileDrop,
                    .mouse_pos = {static_cast<float>(x), static_cast<float>(y)},
                    .files = std::move(files)});
        }
        catch (...) { /* Owned drop is still released exactly once. */ }
    }

#include "implementation/host.native_dispatch.inl"

}

#endif // _WIN32
