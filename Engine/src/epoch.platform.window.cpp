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
module;

#include "../include/_epoch.stl_types.hpp"
#include "../src/epoch.api_types.hpp"

#include <unordered_map>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

module platform.window;

import core.error;

namespace epoch::platform
{
    namespace
    {
#ifdef _WIN32
        static std::wstring widen_utf8(epoch::string_view text)
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
                for (const auto& [hwnd, _] : windows_)
                {
                    if (::IsWindow(hwnd))
                        ::DestroyWindow(hwnd);
                }
                windows_.clear();

                if (class_atom_ != 0)
                    ::UnregisterClassW(class_name(), instance_);
            }

            [[nodiscard]] core::error::result<WindowHandle> create_window(const WindowDesc& desc) noexcept override
            {
                if (!ensure_class_registered())
                    return epoch::unexpected(core::error::failed("failed to register Win32 window class"));

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
                    return epoch::unexpected(core::error::failed("failed to create Win32 window"));

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

                for (const auto& e : events_)
                    handler(e);

                events_.clear();
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
                    auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
                    self = static_cast<Win32WindowSystem*>(create->lpCreateParams);
                    ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
                    return TRUE;
                }

                switch (msg)
                {
                case WM_CREATE:
                {
                    // Intentionally empty.
                    // Editor UI is rendered via the engine GUI (software/raylib/opengl/vulkan contexts).
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
#else
        class NullWindowSystem final : public IWindowSystem
        {
        public:
            [[nodiscard]] core::error::result<WindowHandle> create_window(const WindowDesc&) noexcept override
            {
                WindowHandle handle{ next_handle_++ };
                windows_.push_back(handle);
                if (!primary_.valid())
                    primary_ = handle;
                return handle;
            }

            void destroy_window(WindowHandle handle) noexcept override
            {
                if (!handle.valid())
                    return;

                auto it = std::find_if(windows_.begin(), windows_.end(),
                    [&](WindowHandle existing) { return existing.value == handle.value; });

                if (it != windows_.end())
                    windows_.erase(it);

                if (primary_.value == handle.value)
                    primary_ = {};
            }

            void pump_events(const function_ref<void(const WindowEvent&)>& handler) noexcept override
            {
                if (!handler)
                {
                    events_.clear();
                    return;
                }

                for (const auto& e : events_)
                    handler(e);

                events_.clear();
            }

            void request_close(WindowHandle handle) noexcept override
            {
                if (!handle.valid())
                    return;

                WindowEvent e{};
                e.type = WindowEventType::close;
                e.handle = handle;
                events_.push_back(e);
            }

            void set_title(WindowHandle, string_view) noexcept override {}

            [[nodiscard]] WindowHandle primary_window() const noexcept override
            {
                return primary_;
            }

        private:
            std::vector<WindowHandle> windows_{};
            std::vector<WindowEvent> events_{};
            std::uintptr_t next_handle_ = 1;
            WindowHandle primary_{};
        };
#endif
    } // namespace

    core::error::result<std::unique_ptr<IWindowSystem>> create_window_system() noexcept
    {
#ifdef _WIN32
        return std::make_unique<Win32WindowSystem>();
#else
        return std::make_unique<NullWindowSystem>();
#endif
    }
} // namespace epoch::platform
