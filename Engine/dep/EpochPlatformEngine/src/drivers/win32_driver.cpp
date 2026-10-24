// SPDX-License-Identifier: MPL-2.0
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#    define NOMINMAX
#endif

#include <epochplatformengine/driver.hpp>

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>

namespace epochengine::platform
{
    namespace
    {
        [[nodiscard]] std::wstring widen(const std::string_view value)
        {
            if (value.empty())
                return {};
            const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
            if (size <= 0)
                return std::wstring(value.begin(), value.end());
            std::wstring result(static_cast<std::size_t>(size), L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), size);
            return result;
        }

        [[nodiscard]] std::string narrow(const std::wstring_view value)
        {
            if (value.empty())
                return {};
            const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
            if (size <= 0)
                return {};
            std::string result(static_cast<std::size_t>(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
            return result;
        }

        class Win32Driver final : public IPlatformDriver
        {
            struct Payload final
            {
                Win32Driver* owner{};
                ContextId context{};
            };

            struct WindowRecord final
            {
                HWND hwnd{};
                HDC hdc{};
                std::unique_ptr<Payload> payload{};
                WindowDesc desc{};
            };

        public:
            [[nodiscard]] DriverKind kind() const noexcept override { return DriverKind::win32; }
            [[nodiscard]] std::string_view name() const noexcept override { return "EpochPlatformEngine Win32"; }
            [[nodiscard]] DriverCapabilities capabilities() const noexcept override
            {
                return {
                    .native_windows = true,
                    .multiple_windows = true,
                    .parented_windows = true,
                    .detached_windows = true,
                    .high_dpi = true,
                    .file_drop = true,
                    .supports_dedicated_thread = true,
                    .requires_main_thread = false
                };
            }

            [[nodiscard]] Status initialize(const DriverInit& init) noexcept override
            {
                instance_ = init.native_application_instance != nullptr
                    ? static_cast<HINSTANCE>(init.native_application_instance)
                    : GetModuleHandleW(nullptr);
                if (instance_ == nullptr)
                    return std::unexpected(Error{ ErrorCode::initialization_failed, "GetModuleHandleW failed" });

                SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

                WNDCLASSEXW window_class{
                    .cbSize = sizeof(WNDCLASSEXW),
                    .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
                    .lpfnWndProc = &Win32Driver::window_proc,
                    .hInstance = instance_,
                    .hCursor = LoadCursorW(nullptr, IDC_ARROW),
                    .hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
                    .lpszClassName = class_name_
                };

                class_atom_ = RegisterClassExW(&window_class);
                if (class_atom_ == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                    return std::unexpected(Error{ ErrorCode::initialization_failed, "RegisterClassExW failed" });

                initialized_ = true;
                return {};
            }

            void shutdown() noexcept override
            {
                while (!windows_.empty())
                    destroy_window(windows_.begin()->first);
                if (class_atom_ != 0)
                    UnregisterClassW(class_name_, instance_);
                class_atom_ = 0;
                initialized_ = false;
            }

            [[nodiscard]] Result<NativeWindowHandle> create_window(const ContextId context, const WindowDesc& desc) noexcept override
            {
                if (!initialized_)
                    return std::unexpected(Error{ ErrorCode::initialization_failed, "Win32 driver is not initialized" });

                auto payload = std::make_unique<Payload>(Payload{ .owner = this, .context = context });
                const HWND parent = desc.parent.kind == NativeHandleKind::win32
                    ? static_cast<HWND>(desc.parent.window)
                    : nullptr;

                DWORD style{};
                DWORD extended_style{};
                if (desc.mode == WindowMode::child && parent != nullptr)
                {
                    style = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
                }
                else
                {
                    style = desc.decorated ? WS_OVERLAPPEDWINDOW : WS_POPUP;
                    if (!desc.resizable)
                        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
                    extended_style |= WS_EX_APPWINDOW;
                }
                if (desc.visible)
                    style |= WS_VISIBLE;
                if (desc.accept_file_drop)
                    extended_style |= WS_EX_ACCEPTFILES;

                RECT rectangle{ 0, 0, desc.width, desc.height };
                const UINT creation_dpi = GetDpiForSystem();
                AdjustWindowRectExForDpi(
                    &rectangle, style, FALSE, extended_style, creation_dpi);
                const int width = rectangle.right - rectangle.left;
                const int height = rectangle.bottom - rectangle.top;
                const int x = desc.x >= 0 ? desc.x : CW_USEDEFAULT;
                const int y = desc.y >= 0 ? desc.y : CW_USEDEFAULT;
                const std::wstring title = widen(desc.title);

                HWND hwnd = CreateWindowExW(
                    extended_style,
                    class_name_,
                    title.c_str(),
                    style,
                    x,
                    y,
                    width,
                    height,
                    parent,
                    nullptr,
                    instance_,
                    payload.get());
                if (hwnd == nullptr)
                    return std::unexpected(Error{ ErrorCode::window_creation_failed, "CreateWindowExW failed" });

                HDC hdc = GetDC(hwnd);
                if (hdc == nullptr)
                {
                    DestroyWindow(hwnd);
                    return std::unexpected(Error{ ErrorCode::window_creation_failed, "GetDC failed" });
                }

                if (desc.accept_file_drop)
                    DragAcceptFiles(hwnd, TRUE);
                if (desc.visible)
                {
                    ShowWindow(hwnd, SW_SHOW);
                    UpdateWindow(hwnd);
                }

                windows_.emplace(context, WindowRecord{
                    .hwnd = hwnd,
                    .hdc = hdc,
                    .payload = std::move(payload),
                    .desc = desc
                });

                const UINT dpi = GetDpiForWindow(hwnd);
                queue(WindowEvent{
                    .type = WindowEventType::dpi_changed,
                    .context = context,
                    .width = static_cast<std::int32_t>(dpi),
                    .height = static_cast<std::int32_t>(dpi)
                });

                return make_handle(windows_.at(context));
            }

            void destroy_window(const ContextId context) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return;

                WindowRecord& record = iterator->second;
                if (record.hdc != nullptr && record.hwnd != nullptr)
                    ReleaseDC(record.hwnd, record.hdc);
                record.hdc = nullptr;
                if (record.hwnd != nullptr && IsWindow(record.hwnd))
                    DestroyWindow(record.hwnd);
                windows_.erase(iterator);
            }

            void pump_events(const EventSink& sink) noexcept override
            {
                MSG message{};
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }

                std::deque<WindowEvent> local;
                local.swap(events_);
                for (const auto& event : local)
                    sink(event);
            }

            [[nodiscard]] NativeWindowHandle native_handle(const ContextId context) const noexcept override
            {
                const auto iterator = windows_.find(context);
                return iterator == windows_.end() ? NativeWindowHandle{} : make_handle(iterator->second);
            }

            [[nodiscard]] Status set_title(const ContextId context, const std::string_view title) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "Win32 context not found" });
                const std::wstring wide = widen(title);
                if (!SetWindowTextW(iterator->second.hwnd, wide.c_str()))
                    return std::unexpected(Error{ ErrorCode::driver_failure, "SetWindowTextW failed" });
                iterator->second.desc.title.assign(title);
                return {};
            }

            [[nodiscard]] Status set_visible(const ContextId context, const bool visible) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "Win32 context not found" });
                ShowWindow(iterator->second.hwnd, visible ? SW_SHOW : SW_HIDE);
                iterator->second.desc.visible = visible;
                return {};
            }

            [[nodiscard]] Status resize(const ContextId context, const std::int32_t width, const std::int32_t height) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "Win32 context not found" });
                RECT rectangle{ 0, 0, width, height };
                const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(iterator->second.hwnd, GWL_STYLE));
                const DWORD ex_style = static_cast<DWORD>(GetWindowLongPtrW(iterator->second.hwnd, GWL_EXSTYLE));
                AdjustWindowRectExForDpi(&rectangle, style, FALSE, ex_style, GetDpiForWindow(iterator->second.hwnd));
                if (!SetWindowPos(iterator->second.hwnd, nullptr, 0, 0,
                    rectangle.right - rectangle.left, rectangle.bottom - rectangle.top,
                    SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE))
                    return std::unexpected(Error{ ErrorCode::driver_failure, "SetWindowPos failed" });
                iterator->second.desc.width = width;
                iterator->second.desc.height = height;
                return {};
            }

            [[nodiscard]] Status reparent(const ContextId context, const NativeWindowHandle parent, const WindowMode mode) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "Win32 context not found" });

                HWND native_parent = parent.kind == NativeHandleKind::win32 ? static_cast<HWND>(parent.window) : nullptr;
                SetLastError(ERROR_SUCCESS);
                const HWND previous_parent = SetParent(iterator->second.hwnd, native_parent);
                if (previous_parent == nullptr && GetLastError() != ERROR_SUCCESS)
                    return std::unexpected(Error{ ErrorCode::driver_failure, "SetParent failed" });

                LONG_PTR style = GetWindowLongPtrW(iterator->second.hwnd, GWL_STYLE);
                if (mode == WindowMode::child && native_parent != nullptr)
                {
                    style &= ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW | WS_POPUP);
                    style |= WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
                }
                else
                {
                    style &= ~static_cast<LONG_PTR>(WS_CHILD);
                    style |= WS_OVERLAPPEDWINDOW;
                }
                SetWindowLongPtrW(iterator->second.hwnd, GWL_STYLE, style);
                SetWindowPos(iterator->second.hwnd, nullptr, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

                iterator->second.desc.parent = parent;
                iterator->second.desc.mode = mode;
                return {};
            }

            void request_close(const ContextId context) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator != windows_.end())
                    PostMessageW(iterator->second.hwnd, WM_CLOSE, 0, 0);
            }

        private:
            [[nodiscard]] static NativeWindowHandle make_handle(const WindowRecord& record) noexcept
            {
                return {
                    .kind = NativeHandleKind::win32,
                    .window = record.hwnd,
                    .parent = GetParent(record.hwnd),
                    .device_context = record.hdc,
                    .value = reinterpret_cast<std::uintptr_t>(record.hwnd)
                };
            }

            void queue(WindowEvent event) noexcept
            {
                events_.emplace_back(std::move(event));
            }

            static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
            {
                Payload* payload = reinterpret_cast<Payload*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                if (message == WM_NCCREATE)
                {
                    const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
                    payload = static_cast<Payload*>(create->lpCreateParams);
                    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(payload));
                }

                if (payload == nullptr || payload->owner == nullptr)
                    return DefWindowProcW(hwnd, message, wparam, lparam);

                Win32Driver& owner = *payload->owner;
                const ContextId context = payload->context;

                switch (message)
                {
                case WM_CLOSE:
                    owner.queue(WindowEvent{ .type = WindowEventType::close_requested, .context = context });
                    return 0;
                case WM_DESTROY:
                    owner.queue(WindowEvent{ .type = WindowEventType::destroyed, .context = context });
                    return 0;
                case WM_NCDESTROY:
                    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                    break;
                case WM_SIZE:
                    owner.queue(WindowEvent{
                        .type = WindowEventType::resized,
                        .context = context,
                        .width = static_cast<std::int32_t>(LOWORD(lparam)),
                        .height = static_cast<std::int32_t>(HIWORD(lparam))
                    });
                    return 0;
                case WM_MOVE:
                    owner.queue(WindowEvent{
                        .type = WindowEventType::moved,
                        .context = context,
                        .x = static_cast<std::int16_t>(LOWORD(lparam)),
                        .y = static_cast<std::int16_t>(HIWORD(lparam))
                    });
                    return 0;
                case WM_SETFOCUS:
                    owner.queue(WindowEvent{ .type = WindowEventType::focus_gained, .context = context });
                    return 0;
                case WM_KILLFOCUS:
                    owner.queue(WindowEvent{ .type = WindowEventType::focus_lost, .context = context });
                    return 0;
                case WM_KEYDOWN:
                case WM_SYSKEYDOWN:
                    owner.queue(WindowEvent{
                        .type = WindowEventType::key_pressed,
                        .context = context,
                        .key = static_cast<std::int32_t>(wparam),
                        .repeated = (lparam & (1LL << 30)) != 0
                    });
                    // Observation does not consume native system commands (for example Alt-F4).
                    return message == WM_SYSKEYDOWN ? DefWindowProcW(hwnd, message, wparam, lparam) : 0;
                case WM_KEYUP:
                case WM_SYSKEYUP:
                    owner.queue(WindowEvent{
                        .type = WindowEventType::key_released,
                        .context = context,
                        .key = static_cast<std::int32_t>(wparam)
                    });
                    return message == WM_SYSKEYUP ? DefWindowProcW(hwnd, message, wparam, lparam) : 0;
                case WM_CHAR:
                    owner.queue(WindowEvent{
                        .type = WindowEventType::text_input,
                        .context = context,
                        .codepoint = static_cast<char32_t>(wparam)
                    });
                    return 0;
                case WM_MOUSEMOVE:
                    owner.queue(WindowEvent{
                        .type = WindowEventType::mouse_moved,
                        .context = context,
                        .x = GET_X_LPARAM(lparam),
                        .y = GET_Y_LPARAM(lparam)
                    });
                    return 0;
                case WM_LBUTTONDOWN:
                case WM_RBUTTONDOWN:
                case WM_MBUTTONDOWN:
                    owner.queue(WindowEvent{
                        .type = WindowEventType::mouse_button_pressed,
                        .context = context,
                        .x = GET_X_LPARAM(lparam),
                        .y = GET_Y_LPARAM(lparam),
                        .button = message == WM_LBUTTONDOWN ? 1 : (message == WM_RBUTTONDOWN ? 2 : 3)
                    });
                    return 0;
                case WM_LBUTTONUP:
                case WM_RBUTTONUP:
                case WM_MBUTTONUP:
                    owner.queue(WindowEvent{
                        .type = WindowEventType::mouse_button_released,
                        .context = context,
                        .x = GET_X_LPARAM(lparam),
                        .y = GET_Y_LPARAM(lparam),
                        .button = message == WM_LBUTTONUP ? 1 : (message == WM_RBUTTONUP ? 2 : 3)
                    });
                    return 0;
                case WM_MOUSEWHEEL:
                {
                    POINT point{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
                    ScreenToClient(hwnd, &point);
                    owner.queue(WindowEvent{
                        .type = WindowEventType::mouse_wheel,
                        .context = context,
                        .x = point.x,
                        .y = point.y,
                        .wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam)
                    });
                    return 0;
                }
                case WM_DPICHANGED:
                {
                    const auto* suggested = reinterpret_cast<const RECT*>(lparam);
                    SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                        suggested->right - suggested->left, suggested->bottom - suggested->top,
                        SWP_NOZORDER | SWP_NOACTIVATE);
                    owner.queue(WindowEvent{
                        .type = WindowEventType::dpi_changed,
                        .context = context,
                        .width = static_cast<std::int32_t>(LOWORD(wparam)),
                        .height = static_cast<std::int32_t>(HIWORD(wparam))
                    });
                    return 0;
                }
                case WM_DROPFILES:
                {
                    const HDROP drop = reinterpret_cast<HDROP>(wparam);
                    const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
                    for (UINT index = 0; index < count; ++index)
                    {
                        const UINT length = DragQueryFileW(drop, index, nullptr, 0);
                        std::wstring path(static_cast<std::size_t>(length + 1), L'\0');
                        DragQueryFileW(drop, index, path.data(), length + 1);
                        path.resize(length);
                        owner.queue(WindowEvent{
                            .type = WindowEventType::file_dropped,
                            .context = context,
                            .text = narrow(path)
                        });
                    }
                    DragFinish(drop);
                    return 0;
                }
                default:
                    break;
                }

                return DefWindowProcW(hwnd, message, wparam, lparam);
            }

            static constexpr wchar_t class_name_[] = L"EpochPlatformEngine.NativeWindow";
            HINSTANCE instance_{};
            ATOM class_atom_{};
            bool initialized_{};
            std::unordered_map<ContextId, WindowRecord> windows_{};
            std::deque<WindowEvent> events_{};
        };
    }

    std::unique_ptr<IPlatformDriver> create_win32_driver()
    {
        return std::make_unique<Win32Driver>();
    }
}

#endif
