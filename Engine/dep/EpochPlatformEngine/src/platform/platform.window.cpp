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
 *   SPDX-License-Identifier: MPL-2.0             *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
// Epoch system notes (auto-checked):
// Epoch STL: honor core.stl_types.hpp types/operators; fix callers, not core overloads.
// Epoch Platform: keep native code isolated behind platform-specific interfaces.
module;

#include "core.stl_types.hpp"
#include "core.abi_types.hpp"

#include <chrono>
#include <thread>
#include <unordered_map>
#include <mutex>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <commctrl.h>
#  include <new>
#endif

module platform.window;

import core.error;
import core.log;

namespace epochengine::platform::window_adapter
{
    namespace
    {
#ifdef _WIN32
        constexpr wchar_t nativeInputProperty[] = L"EpochBackendInputAdapter";
        constexpr UINT_PTR nativeInputSubclass = 2;
        struct NativeInputRegistration { NativeInputBinding binding; };

        NativeInputMessage native_input_message(HWND hwnd, UINT msg,
            WPARAM key, LPARAM flags) noexcept
        {
            NativeInputMessage result{{reinterpret_cast<std::uintptr_t>(hwnd)},
                NativeInputKind::other, msg, key, flags};
            switch (msg)
            {
            case WM_SETFOCUS: result.kind = NativeInputKind::focus_gained; break;
            case WM_KILLFOCUS: result.kind = NativeInputKind::focus_lost; break;
            case WM_NCDESTROY: result.kind = NativeInputKind::retiring; break;
            case WM_CHAR: case WM_SYSCHAR: result.kind = NativeInputKind::text; break;
            case WM_KEYDOWN: case WM_SYSKEYDOWN: result.kind = NativeInputKind::key; break;
            case WM_LBUTTONDOWN: result.kind = NativeInputKind::left_down; break;
            case WM_RBUTTONDOWN: result.kind = NativeInputKind::right_down; break;
            case WM_MOUSEMOVE: result.kind = NativeInputKind::move; break;
            case WM_LBUTTONUP: result.kind = NativeInputKind::left_up; break;
            case WM_RBUTTONUP: result.kind = NativeInputKind::right_up; break;
            case WM_MOUSEWHEEL: result.kind = NativeInputKind::wheel; break;
            default: break;
            }
            result.close_shortcut = msg == WM_SYSKEYDOWN && key == VK_F4
                && (static_cast<std::uintptr_t>(flags) & (std::uintptr_t{1} << 29u)) != 0;
            result.repeated = (static_cast<std::uintptr_t>(flags) & (std::uintptr_t{1} << 30u)) != 0;
            return result;
        }

        LRESULT CALLBACK native_input_proc(HWND hwnd, UINT msg, WPARAM key,
            LPARAM flags, UINT_PTR, DWORD_PTR data)
        {
            const auto* registration = reinterpret_cast<NativeInputRegistration*>(data);
            // Copy before callback: a close action may synchronously destroy the
            // window and retire this registration through a nested WM_NCDESTROY.
            const auto binding = registration->binding;
            if (msg == WM_NCDESTROY)
            {
                ::RemoveWindowSubclass(hwnd, native_input_proc, nativeInputSubclass);
                ::RemovePropW(hwnd, nativeInputProperty);
                delete registration;
            }
            const auto result = dispatch_native_input(binding,
                native_input_message(hwnd, msg, key, flags));
            return result.handled ? static_cast<LRESULT>(result.value)
                : ::DefSubclassProc(hwnd, msg, key, flags);
        }

        bool owns_native_input_thread(HWND hwnd) noexcept
        {
            DWORD process{};
            const auto thread = ::GetWindowThreadProcessId(hwnd, &process);
            return thread == ::GetCurrentThreadId() && process == ::GetCurrentProcessId();
        }

        std::mutex childCloseMutex;
        std::unordered_map<std::uintptr_t, std::uintptr_t> childCloseTargets;

        class CooperativePauseTimer final
        {
        public:
            CooperativePauseTimer() noexcept
            {
#ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
                timer_ = ::CreateWaitableTimerExW(nullptr, nullptr,
                    CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE | SYNCHRONIZE);
#endif
                if (!timer_)
                    timer_ = ::CreateWaitableTimerExW(nullptr, nullptr, 0,
                        TIMER_MODIFY_STATE | SYNCHRONIZE);
            }

            ~CooperativePauseTimer() noexcept { retire(); }
            CooperativePauseTimer(const CooperativePauseTimer&) = delete;
            CooperativePauseTimer& operator=(const CooperativePauseTimer&) = delete;

            [[nodiscard]] bool pause() noexcept
            {
                if (timer_)
                {
                    LARGE_INTEGER due{};
                    due.QuadPart = -1; // Smallest positive relative request: 100ns.
                    if (::SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE)
                        && ::WaitForSingleObject(timer_, 2) == WAIT_OBJECT_0)
                        return true;
                    // A failed/stalled timer is disabled for this thread. Never
                    // spin or allocate/retry handles on subsequent frame turns.
                    retire();
                }
                ::Sleep(1); // Positive OS-granularity fallback, not a Hz policy.
                return false;
            }

        private:
            void retire() noexcept
            {
                if (timer_)
                {
                    ::CancelWaitableTimer(timer_);
                    ::CloseHandle(timer_);
                    timer_ = nullptr;
                }
            }

            HANDLE timer_{};
        };

        static std::wstring widen_utf8(epochengine::string_view text)
        {
            if (text.data == nullptr || text.size == 0)
                return {};

            const int needed = ::MultiByteToWideChar(
                CP_UTF8, 0,
                text.data, static_cast<int>(text.size),
                nullptr, 0
            );
            if (needed <= 0)
                return {};

            std::wstring wide(static_cast<std::size_t>(needed), L'\0');

            ::MultiByteToWideChar(
                CP_UTF8, 0,
                text.data, static_cast<int>(text.size),
                wide.data(), needed
            );
            return wide;
        }

        class Win32WindowSystem final : public IWindowSystem
        {
        public:
            Win32WindowSystem() noexcept = default;

            ~Win32WindowSystem() noexcept override
            {
                while (!windows_.empty())
                {
                    // DestroyWindow synchronously delivers WM_DESTROY, which
                    // also removes this handle. Retire the iterator first.
                    const HWND hwnd = windows_.begin()->first;
                    windows_.erase(windows_.begin());
                    if (::IsWindow(hwnd))
                        ::DestroyWindow(hwnd);
                }

                if (class_atom_ != 0)
                    ::UnregisterClassW(class_name(), instance_);
            }

            [[nodiscard]] core::error::result<WindowHandle> create_window(const WindowDesc& desc) noexcept override
            {
                if (!ensure_class_registered())
                    return epochengine::unexpected(core::error::failed("failed to register Win32 window class"));

                DWORD style = WS_OVERLAPPEDWINDOW;
                if (!desc.resizable)
                    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

                const std::wstring title = widen_utf8(desc.title);

                HWND hwnd = ::CreateWindowExW(
                    0,
                    class_name(),
                    title.empty() ? L"Epoch" : title.c_str(),
                    style,
                    CW_USEDEFAULT,
                    CW_USEDEFAULT,
                    desc.width,
                    desc.height,
                    nullptr,
                    nullptr,
                    instance_,
                    this
                );

                if (!hwnd)
                    return epochengine::unexpected(core::error::failed("failed to create Win32 window"));

                if (desc.visible)
                    ::ShowWindow(hwnd, SW_SHOW);

                windows_.emplace(hwnd, WindowState{ hwnd });

                WindowHandle handle{ reinterpret_cast<std::uintptr_t>(hwnd) };
                if (!primary_.valid())
                    primary_ = handle;

                return handle;
            }

            void destroy_window(WindowHandle handle) noexcept override
            {
                if (!handle.valid())
                    return;

                HWND hwnd = reinterpret_cast<HWND>(handle.value);
                if (::IsWindow(hwnd))
                    ::DestroyWindow(hwnd);

                windows_.erase(hwnd);

                if (primary_.value == handle.value)
                    primary_ = {};
            }

            void pump_events(const function_ref<void(const WindowEvent&)>& handler) noexcept override
            {
                MSG msg{};
                while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
                {
                    ::TranslateMessage(&msg);
                    ::DispatchMessageW(&msg);
                }

                if (!handler)
                {
                    events_.clear();
                    return;
                }

                // A callback can synchronously create WM_SIZE/WM_CLOSE
                // notifications. Deliver only this detached batch; callbacks
                // append to events_ for the next pump, without invalidating it.
                std::vector<WindowEvent> batch{};
                batch.swap(events_);
                for (const auto& event : batch)
                {
                    // A previous callback may have destroyed its window.
                    if (windows_.contains(reinterpret_cast<HWND>(event.handle.value)))
                        handler(event);
                }
            }

            void request_close(WindowHandle handle) noexcept override
            {
                if (!handle.valid())
                    return;

                HWND hwnd = reinterpret_cast<HWND>(handle.value);
                if (::IsWindow(hwnd))
                    ::PostMessageW(hwnd, WM_CLOSE, 0, 0);
            }

            void set_title(WindowHandle handle, string_view title) noexcept override
            {
                if (!handle.valid())
                    return;

                HWND hwnd = reinterpret_cast<HWND>(handle.value);
                const std::wstring wide = widen_utf8(title);
                ::SetWindowTextW(hwnd, wide.empty() ? L"Epoch" : wide.c_str());
            }

            [[nodiscard]] WindowHandle primary_window() const noexcept override
            {
                return primary_;
            }

        private:
            struct WindowState
            {
                HWND hwnd = nullptr;
            };

            static constexpr const wchar_t* class_name() noexcept
            {
                return L"EpochWindowSystem";
            }

            bool ensure_class_registered() noexcept
            {
                if (class_atom_ != 0)
                    return true;

                WNDCLASSW wc{};
                wc.lpfnWndProc = &Win32WindowSystem::wnd_proc;
                wc.hInstance = instance_;
                wc.lpszClassName = class_name();

                class_atom_ = ::RegisterClassW(&wc);
                return class_atom_ != 0;
            }

            void enqueue(WindowEvent event) noexcept
            {
                events_.push_back(event);
            }

            static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
            {
                Win32WindowSystem* self =
                    reinterpret_cast<Win32WindowSystem*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));

                if (msg == WM_NCCREATE)
                {
                    const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
                    if (!create || !create->lpCreateParams)
                        return FALSE;
                    self = static_cast<Win32WindowSystem*>(create->lpCreateParams);
                    ::SetLastError(ERROR_SUCCESS);
                    const LONG_PTR previous = ::SetWindowLongPtrW(
                        hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
                    if (previous == 0 && ::GetLastError() != ERROR_SUCCESS)
                        return FALSE;
                    // Storing the owner is not the default nonclient setup:
                    // DefWindowProc must still initialize window text/chrome.
                    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
                }

                if (msg == WM_NCDESTROY)
                {
                    ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
                }

                switch (msg)
                {
                case WM_CREATE:
                {
                    // Application content is separate from native window setup.
                    return 0;
                }

                case WM_SIZE:
                {
                    if (self)
                    {
                        WindowEvent e{};
                        e.type = WindowEventType::resized;
                        e.handle = WindowHandle{ reinterpret_cast<std::uintptr_t>(hwnd) };
                        e.width = LOWORD(lparam);
                        e.height = HIWORD(lparam);
                        self->enqueue(e);
                    }

                    return 0;
                }

                case WM_CLOSE:
                {
                    if (self)
                    {
                        WindowEvent e{};
                        e.type = WindowEventType::close;
                        e.handle = WindowHandle{ reinterpret_cast<std::uintptr_t>(hwnd) };
                        self->enqueue(e);
                    }
                    return 0;
                }

                case WM_DESTROY:
                {
                    if (self)
                    {
                        self->windows_.erase(hwnd);
                        if (self->primary_.value == reinterpret_cast<std::uintptr_t>(hwnd))
                            self->primary_ = {};
                    }
                    return 0;
                }
                }

                return ::DefWindowProcW(hwnd, msg, wparam, lparam);
            }


            HINSTANCE instance_ = ::GetModuleHandleW(nullptr);
            ATOM class_atom_ = 0;

            std::unordered_map<HWND, WindowState> windows_{};
            std::vector<WindowEvent> events_{};
            WindowHandle primary_{};
        };
#endif
    } // namespace

    WindowHandle native_child_close_target(WindowHandle group,
        std::span<const NativeChildWindow> children, WindowHandle receiver) noexcept
    {
#ifdef _WIN32
        if (!group.valid() || children.empty()) return {};
        try
        {
            POINT pointer{};
            const HWND underPointer = ::GetCursorPos(&pointer)
                ? ::WindowFromPoint(pointer) : nullptr;
            const auto ownsSurface = [](WindowHandle surface) noexcept
            {
                DWORD process{};
                const auto hwnd = reinterpret_cast<HWND>(surface.value);
                return hwnd && ::IsWindow(hwnd)
                    && ::GetWindowThreadProcessId(hwnd, &process)
                    && process == ::GetCurrentProcessId();
            };
            const auto contains = [](WindowHandle surface, HWND target) noexcept
            {
                const auto hwnd = reinterpret_cast<HWND>(surface.value);
                return target && (hwnd == target || ::IsChild(hwnd, target));
            };
            std::scoped_lock lock(childCloseMutex);
            auto& previous = childCloseTargets[group.value];
            WindowHandle first{}, remembered{}, receiving{};
            for (const auto& child : children)
            {
                if (!child.identity.valid() || !ownsSurface(child.surface)) continue;
                if (!first.valid()) first = child.identity;
                if (contains(child.surface, underPointer))
                {
                    previous = child.identity.value;
                    return child.identity;
                }
                if (child.identity.value == previous) remembered = child.identity;
                if (contains(child.surface, reinterpret_cast<HWND>(receiver.value)))
                    receiving = child.identity;
            }
            if (remembered.valid()) return remembered;
            if (receiving.valid()) return receiving;
            return first;
        }
        catch (...) { return {}; }
#else
        (void)group; (void)children; (void)receiver;
        return {};
#endif
    }

    void forget_native_child_close_group(WindowHandle group) noexcept
    {
#ifdef _WIN32
        std::scoped_lock lock(childCloseMutex);
        childCloseTargets.erase(group.value);
#else
        (void)group;
#endif
    }

    NativeInputResult dispatch_native_input(NativeInputBinding binding,
        const NativeInputMessage& message) noexcept
    {
        if (!binding.callback) return {};
        const auto emit = [&](NativeInputAction action) noexcept
        { return binding.callback(binding.owner, action, message); };
        if (message.close_shortcut)
        {
            if (message.repeated) return {true, 0};
            const auto result = emit(NativeInputAction::close_shortcut);
            if (result.handled) return result;
        }
        switch (message.kind)
        {
        case NativeInputKind::focus_gained:
            (void)emit(NativeInputAction::focus_gained); return {};
        case NativeInputKind::focus_lost:
            (void)emit(NativeInputAction::focus_lost); return {};
        case NativeInputKind::retiring:
            (void)emit(NativeInputAction::retiring); return {};
        case NativeInputKind::text: case NativeInputKind::key:
            (void)emit(NativeInputAction::input); return {true, 0};
        case NativeInputKind::left_down: case NativeInputKind::move:
        case NativeInputKind::left_up:
            (void)emit(NativeInputAction::input);
            return emit(NativeInputAction::proxy_drag);
        case NativeInputKind::right_down: case NativeInputKind::right_up:
        case NativeInputKind::wheel:
            (void)emit(NativeInputAction::input); return {};
        default: return {};
        }
    }

    bool attach_native_input(WindowHandle window, NativeInputBinding binding) noexcept
    {
#ifdef _WIN32
        const auto hwnd = reinterpret_cast<HWND>(window.value);
        if (!binding.callback || !owns_native_input_thread(hwnd)) return false;
        DWORD_PTR existing{};
        if (::GetWindowSubclass(hwnd, native_input_proc, nativeInputSubclass, &existing))
        {
            const auto& admitted = reinterpret_cast<NativeInputRegistration*>(existing)->binding;
            return admitted.owner == binding.owner && admitted.callback == binding.callback;
        }
        // Another owner using the legacy marker must not be silently replaced.
        if (::GetPropW(hwnd, nativeInputProperty)) return false;
        auto* registration = new (std::nothrow) NativeInputRegistration{binding};
        if (!registration) return false;
        if (!::SetWindowSubclass(hwnd, native_input_proc, nativeInputSubclass,
                reinterpret_cast<DWORD_PTR>(registration)))
        { delete registration; return false; }
        if (!::SetPropW(hwnd, nativeInputProperty, registration))
        {
            ::RemoveWindowSubclass(hwnd, native_input_proc, nativeInputSubclass);
            delete registration;
            return false;
        }
        return true;
#else
        (void)window; (void)binding; return false;
#endif
    }

    void detach_native_input(WindowHandle window, NativeInputBinding binding) noexcept
    {
#ifdef _WIN32
        const auto hwnd = reinterpret_cast<HWND>(window.value);
        if (!owns_native_input_thread(hwnd)) return;
        DWORD_PTR existing{};
        if (!::GetWindowSubclass(hwnd, native_input_proc, nativeInputSubclass, &existing)) return;
        auto* registration = reinterpret_cast<NativeInputRegistration*>(existing);
        if (registration->binding.owner != binding.owner
            || registration->binding.callback != binding.callback) return;
        if (::RemoveWindowSubclass(hwnd, native_input_proc, nativeInputSubclass))
        {
            ::RemovePropW(hwnd, nativeInputProperty);
            delete registration;
        }
#else
        (void)window; (void)binding;
#endif
    }

    void cooperative_pause() noexcept
    {
#ifdef _WIN32
        thread_local CooperativePauseTimer timer;
        thread_local bool fallbackReported = false;
        if (!timer.pause() && !fallbackReported)
        {
            fallbackReported = true;
            try
            {
                core::log::warn("pacing", "Cooperative timer unavailable; using Sleep(1) at OS scheduler granularity.");
            }
            catch (...) {} // Logging must not turn a scheduling pause into failure.
        }
#else
        std::this_thread::sleep_for(std::chrono::nanoseconds{ 1 });
#endif
    }

    core::error::result<std::unique_ptr<IWindowSystem>> create_window_system() noexcept
    {
#ifdef _WIN32
        return std::make_unique<Win32WindowSystem>();
#else
        return epochengine::unexpected(core::error::make(
            {core::error::core_domain::id, core::error::core_domain::unsupported},
            "Native window-system implementation is unavailable on this platform."));
#endif
    }
} // namespace epochengine::platform::window_adapter
