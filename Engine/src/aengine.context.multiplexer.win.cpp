/************************************************
 *  Â¦Â¦Â¦Â¦Â¦Â¦Â¦+Â¦Â¦Â¦Â¦Â¦Â¦+  Â¦Â¦Â¦Â¦Â¦Â¦+  Â¦Â¦Â¦Â¦Â¦Â¦+Â¦Â¦+  Â¦Â¦+   *
 *  Â¦Â¦+----+Â¦Â¦+--Â¦Â¦+Â¦Â¦+---Â¦Â¦+Â¦Â¦+----+Â¦Â¦Â¦  Â¦Â¦Â¦   *
 *  Â¦Â¦Â¦Â¦Â¦+  Â¦Â¦Â¦Â¦Â¦Â¦++Â¦Â¦Â¦   Â¦Â¦Â¦Â¦Â¦Â¦     Â¦Â¦Â¦Â¦Â¦Â¦Â¦Â¦   *
 *  Â¦Â¦+--+  Â¦Â¦+---+ Â¦Â¦Â¦   Â¦Â¦Â¦Â¦Â¦Â¦     Â¦Â¦+--Â¦Â¦Â¦   *
 *  Â¦Â¦Â¦Â¦Â¦Â¦Â¦+Â¦Â¦Â¦     +Â¦Â¦Â¦Â¦Â¦Â¦+++Â¦Â¦Â¦Â¦Â¦Â¦+Â¦Â¦Â¦  Â¦Â¦Â¦   *
 *  +------++-+      +-----+  +-----++-+  +-+   *
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
 // context.multiplexer.win.cpp  (TU implementation; NOT a module partition)
 //

#include <include/aengine.config.hpp>

#if defined(_WIN32)
#   ifdef EPOCH_USING_WINMAIN
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
import utility.string_converter;

import aengine.cli;
import core.context;
import core.logger;
import aengine.gui;
import aengine.input;

import context.commandqueue;
import context.multiplexer;
import context.type;
import context.window;
import aengine.telemetry;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import opengl.context;
#endif
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
import software.context;
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
import raylib.context;
#endif

#if defined(_WIN32)

namespace
{
    [[nodiscard]] inline epochnamespace::gui::Vec2 client_mouse_position(
        HWND hwnd,
        LPARAM lParam,
        bool screenCoordinates = false) noexcept
    {
        POINT pt{
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam)
        };

        if (screenCoordinates)
            ::ScreenToClient(hwnd, &pt);

        return {
            static_cast<float>(pt.x),
            static_cast<float>(pt.y)
        };
    }

    inline void push_gui_mouse_event(
        const epochnamespace::core::Context* ctx,
        HWND hwnd,
        epochnamespace::gui::EventType type,
        LPARAM lParam,
        int wheelDelta = 0,
        bool screenCoordinates = false) noexcept
    {
        if (!ctx)
            return;

        epochnamespace::gui::push_input_for_context(ctx, epochnamespace::gui::InputEvent{
            .type = type,
            .mouse_pos = client_mouse_position(hwnd, lParam, screenCoordinates),
            .wheel_delta = wheelDelta
        });
    }

    inline void push_gui_key_event(const epochnamespace::core::Context* ctx, int key) noexcept
    {
        if (!ctx)
            return;

        epochnamespace::gui::push_input_for_context(ctx, epochnamespace::gui::InputEvent{
            .type = epochnamespace::gui::EventType::KeyDown,
            .key = key
        });
    }

    [[nodiscard]] inline std::string utf8_from_codepoint(char32_t codepoint) noexcept
    {
        std::string out{};
        if (codepoint <= 0x7Fu)
        {
            out.push_back(static_cast<char>(codepoint));
        }
        else if (codepoint <= 0x7FFu)
        {
            out.push_back(static_cast<char>(0xC0u | ((codepoint >> 6) & 0x1Fu)));
            out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        }
        else if (codepoint <= 0xFFFFu)
        {
            out.push_back(static_cast<char>(0xE0u | ((codepoint >> 12) & 0x0Fu)));
            out.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        }
        else if (codepoint <= 0x10FFFFu)
        {
            out.push_back(static_cast<char>(0xF0u | ((codepoint >> 18) & 0x07u)));
            out.push_back(static_cast<char>(0x80u | ((codepoint >> 12) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        }
        return out;
    }

    inline void push_gui_text_event(const epochnamespace::core::Context* ctx, char32_t codepoint) noexcept
    {
        if (!ctx)
            return;

        const std::string utf8 = utf8_from_codepoint(codepoint);
        if (utf8.empty())
            return;

        epochnamespace::gui::push_input_for_context(ctx, epochnamespace::gui::InputEvent{
            .type = epochnamespace::gui::EventType::TextInput,
            .text = utf8
        });
    }

    [[nodiscard]] inline std::string utf8_from_key_message(WPARAM wParam, LPARAM lParam) noexcept
    {
        BYTE keyState[256]{};
        if (!::GetKeyboardState(keyState))
            return {};

        if ((::GetKeyState(VK_CONTROL) & 0x8000) != 0)
            return {};

        constexpr int kBufferSize = 8;
        WCHAR translated[kBufferSize]{};
        const UINT scanCode = static_cast<UINT>((lParam >> 16) & 0xFFu);
        const int count = ::ToUnicode(
            static_cast<UINT>(wParam),
            scanCode,
            keyState,
            translated,
            kBufferSize,
            0);
        if (count <= 0)
            return {};

        std::string utf8{};
        for (int i = 0; i < count; ++i)
            utf8 += utf8_from_codepoint(static_cast<char32_t>(translated[i]));
        return utf8;
    }

    inline void push_gui_text_from_key_message(
        const epochnamespace::core::Context* ctx,
        WPARAM wParam,
        LPARAM lParam) noexcept
    {
        if (!ctx)
            return;

        switch (wParam)
        {
        case VK_BACK:
        case VK_RETURN:
        case VK_ESCAPE:
        case VK_TAB:
            return;
        default:
            break;
        }

        const std::string utf8 = utf8_from_key_message(wParam, lParam);
        if (utf8.empty())
            return;

        epochnamespace::gui::push_input_for_context(ctx, epochnamespace::gui::InputEvent{
            .type = epochnamespace::gui::EventType::TextInput,
            .text = utf8
        });
    }

    inline void inject_input_key_event(WPARAM wParam, bool down) noexcept
    {
        epochnamespace::input::inject_virtual_key_event(static_cast<int>(wParam), down);
    }

    [[nodiscard]] inline std::shared_ptr<epochnamespace::core::Context> typed_context(
        const epochnamespace::core::OpaqueContextHandle& opaque) noexcept
    {
        return opaque
            ? std::reinterpret_pointer_cast<epochnamespace::core::Context>(opaque)
            : nullptr;
    }

    // TU-owned globals.
    std::unordered_map<HWND, std::thread> g_threads;
    epochnamespace::core::DragState       g_drag;
    epochnamespace::core::MultiContextManager* g_activeManager = nullptr;
    struct PendingWindowCleanup
    {
        HWND hwnd{};
        std::thread thread{};
        std::unique_ptr<epochnamespace::core::WindowData> window{};
    };
    std::vector<PendingWindowCleanup> g_pendingCleanups;
    constexpr std::string_view kLogSys = "Context.Multiplexer.Win";
    // Some Windows SDK setups don't expose WGL_ARB_create_context declarations here.
    // Provide local fallbacks so this TU can request modern core contexts without extra headers.
#if !defined(WGL_CONTEXT_MAJOR_VERSION_ARB)
    constexpr int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091;
#endif
#if !defined(WGL_CONTEXT_MINOR_VERSION_ARB)
    constexpr int WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
#endif
#if !defined(WGL_CONTEXT_PROFILE_MASK_ARB)
    constexpr int WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126;
#endif
#if !defined(WGL_CONTEXT_CORE_PROFILE_BIT_ARB)
    constexpr int WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x00000001;
#endif
#if !defined(PFNWGLCREATECONTEXTATTRIBSARBPROC)
    using PFNWGLCREATECONTEXTATTRIBSARBPROC = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
#endif

   // [[nodiscard]] inline int clamp_positive(int v) noexcept { return (v < 1) ? 1 : v; }
    [[nodiscard]] inline int clamp_positive(int v) noexcept { return (v < 1) ? 1 : v; }

#if EPOCH_SINGLE_PARENT
    struct SubCtx { HWND originalParent{}; };
    constexpr wchar_t kEpochDockParentProp[] = L"EpochDockParent";
    constexpr int kDockDragStripHeight = 28;

    [[nodiscard]] inline bool is_dock_drag_hotspot(HWND hwnd, LPARAM lp) noexcept
    {
        if (!hwnd)
            return false;

        RECT clientRect{};
        if (!::GetClientRect(hwnd, &clientRect))
            return false;

        const int width = clamp_positive(static_cast<int>(clientRect.right - clientRect.left));
        const int height = clamp_positive(static_cast<int>(clientRect.bottom - clientRect.top));
        const int hotspotHeight = (std::min)(kDockDragStripHeight, height);

        const int x = GET_X_LPARAM(lp);
        const int y = GET_Y_LPARAM(lp);
        return x >= 0 && x < width && y >= 0 && y < hotspotHeight;
    }

    // Dock/undock requests must be processed on the window's owning thread.
    // GLFW/raylib windows are owned by the thread that created them (typically the render thread).
    // Cross-thread SetParent/SetWindowLongPtr/SetWindowPos can deadlock.
    constexpr UINT WM_EPOCH_DOCKCMD = WM_APP + 0x4A11;
    enum class DockCmd : WPARAM
    {
        Undock = 1,
    };

    [[nodiscard]] inline std::shared_ptr<epochnamespace::core::Context> resolve_gui_context_for_hwnd(HWND hwnd) noexcept
    {
        auto* mgr = g_activeManager;
        if (!mgr)
            return {};

        if (auto* win = mgr->findWindowByHWND(hwnd))
            return typed_context(win->context);

        return {};
    }

    inline void forward_gui_input_message(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
    {
        const auto ctx = resolve_gui_context_for_hwnd(hwnd);
        if (!ctx)
            return;

        switch (msg)
        {
        case WM_LBUTTONDOWN:
            ::SetFocus(hwnd);
            push_gui_mouse_event(ctx.get(), hwnd, epochnamespace::gui::EventType::MouseDown, lParam);
            break;
        case WM_MOUSEMOVE:
            push_gui_mouse_event(ctx.get(), hwnd, epochnamespace::gui::EventType::MouseMove, lParam);
            break;
        case WM_LBUTTONUP:
            push_gui_mouse_event(ctx.get(), hwnd, epochnamespace::gui::EventType::MouseUp, lParam);
            break;
        case WM_MOUSEWHEEL:
            push_gui_mouse_event(
                ctx.get(),
                hwnd,
                epochnamespace::gui::EventType::MouseWheel,
                lParam,
                GET_WHEEL_DELTA_WPARAM(wParam),
                true);
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            push_gui_key_event(ctx.get(), static_cast<int>(wParam));
            break;
        case WM_CHAR:
        case WM_SYSCHAR:
            if (wParam >= 0x20u || wParam == 13u || wParam == 8u)
                push_gui_text_event(ctx.get(), static_cast<char32_t>(wParam));
            break;
        default:
            break;
        }
    }

    LRESULT CALLBACK BackendInputProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
    {
        switch (msg)
        {
        case WM_CHAR:
        case WM_SYSCHAR:
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            inject_input_key_event(wp, false);
            return DefSubclassProc(hwnd, msg, wp, lp);
        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE:
        case WM_LBUTTONUP:
        case WM_MOUSEWHEEL:
            forward_gui_input_message(hwnd, msg, wp, lp);
            return DefSubclassProc(hwnd, msg, wp, lp);
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            const auto ctx = resolve_gui_context_for_hwnd(hwnd);
            inject_input_key_event(wp, true);
            forward_gui_input_message(hwnd, msg, wp, lp);
            push_gui_text_from_key_message(ctx.get(), wp, lp);
            return DefSubclassProc(hwnd, msg, wp, lp);
        }
        default:
            return DefSubclassProc(hwnd, msg, wp, lp);
        }
    }


    LRESULT CALLBACK DockableProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR dw)
    {
        auto* ctx = reinterpret_cast<SubCtx*>(dw);

        switch (msg)
        {
        case WM_NCDESTROY:
            ::RemovePropW(hwnd, kEpochDockParentProp);
            delete ctx;
            return DefSubclassProc(hwnd, msg, wp, lp);

        case WM_EPOCH_DOCKCMD:
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
        {
            forward_gui_input_message(hwnd, msg, wp, lp);
            const auto& dragState = epochnamespace::core::Drag();
            const bool continueDrag = dragState.dragging && dragState.draggedWindow == hwnd;
            const bool dragStart = (msg == WM_LBUTTONDOWN) && is_dock_drag_hotspot(hwnd, lp);
            if (dragStart || continueDrag)
                return epochnamespace::core::MultiContextManager::ChildProc(hwnd, msg, wp, lp);
            return DefSubclassProc(hwnd, msg, wp, lp);
        }

        case WM_MOUSEWHEEL:
            forward_gui_input_message(hwnd, msg, wp, lp);
            return DefSubclassProc(hwnd, msg, wp, lp);

        case WM_CHAR:
        case WM_SYSCHAR:
            return 0;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            inject_input_key_event(wp, false);
            return DefSubclassProc(hwnd, msg, wp, lp);

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            const auto guiCtx = resolve_gui_context_for_hwnd(hwnd);
            inject_input_key_event(wp, true);
            forward_gui_input_message(hwnd, msg, wp, lp);
            push_gui_text_from_key_message(guiCtx.get(), wp, lp);
            return DefSubclassProc(hwnd, msg, wp, lp);
        }
        }

        return DefSubclassProc(hwnd, msg, wp, lp);
    }
#endif

    inline void cleanup_window_resources(std::unique_ptr<epochnamespace::core::WindowData>& window) noexcept
    {
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        if (window && window->glContext)
        {
            ::wglMakeCurrent(nullptr, nullptr);
            ::wglDeleteContext(window->glContext);
        }
#endif
        if (window && window->hdc)
        {
            HWND releaseTarget = nullptr;
            if (window->hwndChild && ::IsWindow(window->hwndChild) != FALSE)
                releaseTarget = window->hwndChild;
            else if (window->hwnd && ::IsWindow(window->hwnd) != FALSE)
                releaseTarget = window->hwnd;
            else if (window->host_hwnd && ::IsWindow(window->host_hwnd) != FALSE)
                releaseTarget = window->host_hwnd;

            if (releaseTarget)
                ::ReleaseDC(releaseTarget, window->hdc);
        }
    }

    [[nodiscard]] inline HWND primary_window_handle(const epochnamespace::core::WindowData* window) noexcept
    {
        if (!window)
            return nullptr;

        if ((window->type == epochnamespace::core::ContextType::SDL
                || window->type == epochnamespace::core::ContextType::SFML)
            && window->host_hwnd
            && ::IsWindow(window->host_hwnd) != FALSE)
        {
            return window->host_hwnd;
        }

        if (window->hwnd && ::IsWindow(window->hwnd) != FALSE)
            return window->hwnd;
        if (window->hwndChild && ::IsWindow(window->hwndChild) != FALSE)
            return window->hwndChild;
        if (window->host_hwnd && ::IsWindow(window->host_hwnd) != FALSE)
            return window->host_hwnd;

        if ((window->type == epochnamespace::core::ContextType::SDL
                || window->type == epochnamespace::core::ContextType::SFML)
            && window->host_hwnd)
            return window->host_hwnd;
        return window->hwnd ? window->hwnd : (window->hwndChild ? window->hwndChild : window->host_hwnd);
    }

    [[nodiscard]] inline HWND dock_slot_handle(
        const epochnamespace::core::WindowData* window,
        HWND dockParent) noexcept
    {
        if (!window || !dockParent || ::IsWindow(dockParent) == FALSE)
            return nullptr;

        if (window->hwndChild
            && ::IsWindow(window->hwndChild) != FALSE
            && ::GetParent(window->hwndChild) == dockParent)
        {
            return window->hwndChild;
        }

        if (window->host_hwnd
            && ::IsWindow(window->host_hwnd) != FALSE
            && ::GetParent(window->host_hwnd) == dockParent)
        {
            return window->host_hwnd;
        }

        if (window->hwnd
            && ::IsWindow(window->hwnd) != FALSE
            && ::GetParent(window->hwnd) == dockParent)
        {
            return window->hwnd;
        }

        return nullptr;
    }

    [[nodiscard]] inline bool matches_window_handle(
        const epochnamespace::core::WindowData* window,
        HWND hwnd) noexcept
    {
        return window
            && hwnd
            && (window->hwnd == hwnd
                || window->hwndChild == hwnd
                || window->host_hwnd == hwnd);
    }

    [[nodiscard]] inline bool thread_finished(std::thread& thread) noexcept
    {
        if (!thread.joinable()) return true;
        HANDLE handle = static_cast<HANDLE>(thread.native_handle());
        if (!handle) return false;
        return ::WaitForSingleObject(handle, 0) == WAIT_OBJECT_0;
    }
}

namespace epochnamespace::core
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

    void MultiContextManager::AttachBackendInputBridge(HWND hwnd) noexcept
    {
        if (!hwnd || ::IsWindow(hwnd) == FALSE)
            return;

        (void)::SetWindowSubclass(hwnd, BackendInputProc, 2, 0);
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
            if (epochnamespace::core::cli::updater_shell_requested
                && type == ContextType::OpenGL
                && index == 0)
            {
                return L"Epoch Updater Shell";
            }

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
            [hwnd](const std::unique_ptr<WindowData>& w) { return matches_window_handle(w.get(), hwnd); });
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
        int SoftwareWinCount,
        bool parented)
    {
        const int totalRequested = RayLibWinCount + SDLWinCount + SFMLWinCount + VulkanWinCount + OpenGLWinCount + SoftwareWinCount;
        if (totalRequested <= 0) return false;

        uiThreadId = ::GetCurrentThreadId();
        running.store(true, std::memory_order_release);
        s_activeInstance = this;
        g_activeManager = this;

        RegisterParentClass(hInst, L"EpochParent");
        RegisterChildClass(hInst, L"EpochChild");

        epochnamespace::core::InitializeAllContexts();

        // ---------------- Parent (dock container) ----------------
        if (parented)
        {
            int cols = 1, rows = 1;
            while (cols * rows < totalRequested) (cols <= rows ? ++cols : ++rows);

            int clientW = cli::window_width;
            int clientH = cli::window_height;

            if (totalRequested > 1)
            {
                RECT workArea{};
                constexpr int kMargin = 24;
                if (::SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0))
                {
                    clientW = (std::max)(
                        960,
                        static_cast<int>(workArea.right - workArea.left) - (kMargin * 2));
                    clientH = (std::max)(
                        720,
                        static_cast<int>(workArea.bottom - workArea.top) - (kMargin * 2));
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

            parent = ::CreateWindowExW(
                0,
                L"EpochParent",
                L"Epoch Docking",
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
                epochnamespace::logger::get(kLogSys).log(
                    epochnamespace::logger::LogLevel::INFO,
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
                    const std::string narrowTitle = epochnamespace::text::narrow_utf8(windowTitle);
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
                        epochnamespace::logger::get(kLogSys).logf(
                            epochnamespace::logger::LogLevel::Error,
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
                            : epochnamespace::text::narrow_utf8(backend::BuildChildWindowTitle(type, static_cast<int>(i)));
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
                        epochnamespace::logger::get(kLogSys).logf(
                            epochnamespace::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Deferring OpenGL init to render thread. host={}",
                            static_cast<void*>(hwnd));
#endif
                        break;
#endif
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)

                    case ContextType::Software:
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
                        epochnamespace::logger::get(kLogSys).logf(
                            epochnamespace::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Deferring Software init to render thread. host={}",
                            static_cast<void*>(hwnd));
#endif
                        break;
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
                    case ContextType::RayLib:
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
                        epochnamespace::logger::get(kLogSys).logf(
                            epochnamespace::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Deferring Raylib init to render thread. host={}",
                            static_cast<void*>(hwnd));
#endif
                        break;
#endif
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
                    case ContextType::SDL:
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
                        epochnamespace::logger::get(kLogSys).logf(
                            epochnamespace::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Deferring SDL init to render thread. host={}",
                            static_cast<void*>(hwnd));
#endif
                        break;
#endif
#if defined(EPOCH_USING_VULKAN) && (EPOCH_USING_VULKAN == 1)
                    case ContextType::Vulkan:
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
                        epochnamespace::logger::get(kLogSys).logf(
                            epochnamespace::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Deferring Vulkan init to render thread. host={}",
                            static_cast<void*>(hwnd));
#endif
                        break;
#endif
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
                    case ContextType::SFML:
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
                        epochnamespace::logger::get(kLogSys).logf(
                            epochnamespace::logger::LogLevel::INFO,
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
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)

        make_backend_windows(ContextType::Software, SoftwareWinCount);
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
        if (clampedWidth <= 1 || clampedHeight <= 1)
            backend::ResolveClientSize(hwnd, clampedWidth, clampedHeight);

        std::function<void(int, int)> resizeCallback;
        core::ContextType contextType = core::ContextType::None;
        std::uintptr_t windowId = 0;
        WindowData* window = nullptr;

        {
            std::scoped_lock lock(windowsMutex);
            auto it = std::find_if(windows.begin(), windows.end(),
                [hwnd](const std::unique_ptr<WindowData>& w) { return matches_window_handle(w.get(), hwnd); });
            if (it == windows.end()) return;

            window = it->get();
            window->width = clampedWidth;
            window->height = clampedHeight;

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
                [hwnd](const std::unique_ptr<WindowData>& w) { return matches_window_handle(w.get(), hwnd); });
            if (it == windows.end()) return;

            (*it)->running = false;

            if ((*it)->context)
            {
                auto ctx = typed_context((*it)->context);
                if (ctx->windowData == it->get()) ctx->windowData = nullptr;
                if (ctx->hwnd == hwnd
                    || ctx->hwnd == (*it)->hwnd
                    || ctx->hwnd == (*it)->hwndChild
                    || ctx->hwnd == (*it)->host_hwnd)
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
        HWND threadKey = hwnd;
        if (!threads.contains(threadKey) && removed)
        {
            if (removed->host_hwnd && threads.contains(removed->host_hwnd))
                threadKey = removed->host_hwnd;
            else if (removed->hwndChild && threads.contains(removed->hwndChild))
                threadKey = removed->hwndChild;
            else if (removed->hwnd && threads.contains(removed->hwnd))
                threadKey = removed->hwnd;
        }

        if (threads.contains(threadKey))
        {
            PendingWindowCleanup pending{};
            pending.hwnd = threadKey;
            pending.thread = std::move(threads[threadKey]);
            pending.window = std::move(removed);
            threads.erase(threadKey);
            g_pendingCleanups.emplace_back(std::move(pending));
        }
        else
        {
            cleanup_window_resources(removed);
        }

        CleanupFinishedWindows();

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

        std::vector<WindowData*> dockedWindows;
        dockedWindows.reserve(windows.size());
        for (auto& win : windows)
        {
            const HWND liveHwnd = dock_slot_handle(win.get(), parent);
            if (!liveHwnd || ::IsWindow(liveHwnd) == FALSE)
                continue;

            dockedWindows.push_back(win.get());
        }

        if (dockedWindows.empty())
            return;

        const auto dock_order = [](const WindowData* win) noexcept
        {
            const auto liveContext = (win && win->context) ? typed_context(win->context) : nullptr;
            const auto type = liveContext ? liveContext->type : ContextType::None;
            switch (type)
            {
            case ContextType::RayLib: return 0;
            case ContextType::SDL: return 1;
            case ContextType::SFML: return 2;
            case ContextType::Vulkan: return 3;
            case ContextType::OpenGL: return 4;
            case ContextType::Software: return 5;
            default: return 99;
            }
        };

        std::stable_sort(
            dockedWindows.begin(),
            dockedWindows.end(),
            [&](const WindowData* lhs, const WindowData* rhs)
            {
                return dock_order(lhs) < dock_order(rhs);
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

        for (size_t i = 0; i < dockedWindows.size(); ++i)
        {
            const int c = static_cast<int>(i) % cols;
            const int r = static_cast<int>(i) / cols;

            WindowData& win = *dockedWindows[i];
            const HWND liveHwnd = dock_slot_handle(&win, parent);
            if (!liveHwnd || ::IsWindow(liveHwnd) == FALSE)
                continue;

            const bool usingHiddenHostPlaceholder =
                (win.type == ContextType::SDL || win.type == ContextType::SFML)
                && win.host_hwnd
                && liveHwnd == win.host_hwnd
                && (!win.hwndChild
                    || ::IsWindow(win.hwndChild) == FALSE);

            ::SetWindowPos(liveHwnd, nullptr, c * cw, r * ch, cw, ch,
                usingHiddenHostPlaceholder
                ? (SWP_NOZORDER | SWP_NOACTIVATE)
                : (SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW));

            if (usingHiddenHostPlaceholder)
                ::ShowWindow(liveHwnd, SW_HIDE);

            if (win.host_hwnd
                && win.host_hwnd != liveHwnd
                && ::IsWindow(win.host_hwnd) != FALSE
                && ::GetParent(win.host_hwnd) == parent)
            {
                ::ShowWindow(win.host_hwnd, SW_HIDE);
            }

            HandleResize(liveHwnd, cw, ch);
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
            }
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
        if (g_activeManager == this) g_activeManager = nullptr;
    }

    void MultiContextManager::RenderLoop(WindowData& win)
    {
        auto ctx = typed_context(win.context);
        if (!ctx)
        {
            win.running = false;
            return;
        }

        ctx->windowData = &win;
        MultiContextManager::SetCurrent(ctx);

        struct ResetGuard { ~ResetGuard() { MultiContextManager::SetCurrent(nullptr); } } resetGuard;

        // Raylib must be created+initialized on the SAME thread that will render it.
        // SDL/SFML use their registered backend lifecycle directly.
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        if (ctx->type == ContextType::RayLib)
        {
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
            epochnamespace::logger::get(kLogSys).logf(
                epochnamespace::logger::LogLevel::INFO,
                std::source_location::current(),
                "Raylib init. host={}",
                static_cast<void*>(win.hwnd));
#endif
            const bool initialized = epochnamespace::raylibcontext::raylib_initialize(
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
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
            (ctx->type == ContextType::RayLib) ||
#endif
            false;

        if (!skipGenericInit)
        {
            if (ctx->initialize) ctx->initialize_safe();
            else { win.running = false; return; }
        }

        if (ctx->init_failed)
        {
            epochnamespace::logger::get(kLogSys).logf(
                epochnamespace::logger::LogLevel::Error,
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

            const bool backendOwnsFramePacing =
                ctx->type == ContextType::OpenGL
                || ctx->type == ContextType::Vulkan
                || ctx->type == ContextType::SDL
                || ctx->type == ContextType::SFML
                || ctx->type == ContextType::RayLib;

            if (backendOwnsFramePacing)
                std::this_thread::yield();
            else
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        if (win.running && !win.get_should_close())
            win.commandQueue.drain();
        else
            win.commandQueue.clear();

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
            const auto is_docked_child = [hwnd](HWND candidate) noexcept
            {
                return candidate
                    && candidate != hwnd
                    && ::IsWindow(candidate) != FALSE
                    && ::GetParent(candidate) == hwnd;
            };

            {
                std::scoped_lock lock(mgr->windowsMutex);
                children.reserve(mgr->windows.size());
                for (const auto& win : mgr->windows)
                {
                    if (!win)
                        continue;

                    HWND closeTarget = primary_window_handle(win.get());

                    if (!is_docked_child(closeTarget))
                        continue;

                    win->running = false;
                    win->set_should_close(true);

                    if (auto liveContext = typed_context(win->context);
                        liveContext && liveContext->windowData == win.get())
                    {
                        liveContext->windowData->set_should_close(true);
                    }

                    children.push_back(closeTarget);
                }
            }

            std::sort(children.begin(), children.end());
            children.erase(std::unique(children.begin(), children.end()), children.end());

            // Parent shutdown should close the live child windows in place.
            // Undocking here is the wrong behavior: it visibly tears panes out of the
            // parent host during shutdown and can confuse backend-owned child HWNDs.
            for (HWND child : children)
            {
                ::PostMessageW(child, WM_CLOSE, 0, 0);
            }

            mgr->parent = nullptr;
            ::DestroyWindow(hwnd);
            return 0;
        }

        case WM_DESTROY:
            return 0;
        }

        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT CALLBACK MultiContextManager::ChildProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        static DragState& drag = Drag();
        const auto resolveGuiContext = [hwnd]() -> std::shared_ptr<epochnamespace::core::Context>
        {
            auto* mgr = s_activeInstance;
            if (!mgr)
                return {};

            if (auto* win = mgr->findWindowByHWND(hwnd))
                return typed_context(win->context);

            return {};
        };

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
            ::SetFocus(hwnd);
            const auto ctx = resolveGuiContext();
            push_gui_mouse_event(ctx.get(), hwnd, epochnamespace::gui::EventType::MouseDown, lParam);
            if (!is_dock_drag_hotspot(hwnd, lParam))
                return ::DefWindowProcW(hwnd, msg, wParam, lParam);

            ::SetCapture(hwnd);
            drag.dragging = true;
            drag.draggedWindow = hwnd;
            drag.originalParent = ::GetParent(hwnd);
            if (!drag.originalParent)
                drag.originalParent = static_cast<HWND>(::GetPropW(hwnd, kEpochDockParentProp));
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ::ClientToScreen(hwnd, &pt);
            drag.lastMousePos = pt;
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            const auto ctx = resolveGuiContext();
            push_gui_mouse_event(ctx.get(), hwnd, epochnamespace::gui::EventType::MouseMove, lParam);
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
        {
            const auto ctx = resolveGuiContext();
            push_gui_mouse_event(ctx.get(), hwnd, epochnamespace::gui::EventType::MouseUp, lParam);
            if (drag.dragging && drag.draggedWindow == hwnd)
            {
                const HWND originalParent = drag.originalParent;
                ::ReleaseCapture();
                drag.dragging = false;
                drag.draggedWindow = nullptr;
                drag.originalParent = nullptr;

                if (originalParent && ::IsWindow(originalParent) != FALSE)
                    ::PostMessageW(originalParent, WM_SIZE, 0, 0);
                return 0;
            }
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        case WM_MOUSEWHEEL:
        {
            const auto ctx = resolveGuiContext();
            push_gui_mouse_event(
                ctx.get(),
                hwnd,
                epochnamespace::gui::EventType::MouseWheel,
                lParam,
                GET_WHEEL_DELTA_WPARAM(wParam),
                true);
            return 0;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            const auto ctx = resolveGuiContext();
            inject_input_key_event(wParam, true);
            push_gui_key_event(ctx.get(), static_cast<int>(wParam));
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        case WM_KEYUP:
        case WM_SYSKEYUP:
            inject_input_key_event(wParam, false);
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);

        case WM_CHAR:
        case WM_SYSCHAR:
        {
            const auto ctx = resolveGuiContext();
            if (wParam >= 0x20u || wParam == 13u || wParam == 8u)
                push_gui_text_event(ctx.get(), static_cast<char32_t>(wParam));
            return 0;
        }

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
            ::BeginPaint(hwnd, &ps);
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
