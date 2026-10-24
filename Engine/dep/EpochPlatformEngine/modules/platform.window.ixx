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
 /**************************************************************
 *   Epoch Engine - Modern Renderer Skeleton (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

#include "core.stl_types.hpp"
#include <span>

export module platform.window;

import core.error;

export namespace epochengine::platform::window_adapter
{
    struct WindowHandle
    {
        std::uintptr_t value = 0;

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return value != 0;
        }
    };

    enum class NativeInputKind : std::uint8_t
    {
        other, focus_gained, focus_lost, retiring, text, key,
        left_down, right_down, move, left_up, right_up, wheel,
    };

    struct NativeInputMessage
    {
        WindowHandle window{};
        NativeInputKind kind{};
        std::uint32_t message{};
        std::uintptr_t key{};
        std::intptr_t flags{};
        bool close_shortcut{};
        bool repeated{};
    };

    enum class NativeInputAction : std::uint8_t
    {
        close_shortcut, focus_gained, focus_lost, retiring, input, proxy_drag,
    };

    struct NativeInputResult
    {
        bool handled{};
        std::intptr_t value{};
    };

    struct NativeInputBinding
    {
        void* owner{};
        NativeInputResult (*callback)(void*, NativeInputAction,
            const NativeInputMessage&) noexcept {};
    };

    // Platform owns native subclass lifetime and message-consumption ordering.
    // The admitted owner retains GUI routing, context retirement and drag policy.
    [[nodiscard]] NativeInputResult dispatch_native_input(
        NativeInputBinding binding, const NativeInputMessage& message) noexcept;
    [[nodiscard]] bool attach_native_input(WindowHandle window,
        NativeInputBinding binding) noexcept;
    void detach_native_input(WindowHandle window, NativeInputBinding binding) noexcept;

    struct NativeChildWindow
    {
        WindowHandle identity{};
        WindowHandle surface{};
    };

    // The caller admits owned surfaces and retains shutdown authority. Platform
    // selects the hovered/last-hovered child; it never destroys foreign HWNDs.
    [[nodiscard]] WindowHandle native_child_close_target(WindowHandle group,
        std::span<const NativeChildWindow> children, WindowHandle receiver) noexcept;
    void forget_native_child_close_group(WindowHandle group) noexcept;

    enum class WindowEventType : std::uint8_t
    {
        none,
        close,
        resized,
    };

    struct WindowEvent
    {
        WindowEventType type = WindowEventType::none;
        WindowHandle handle{};
        std::int32_t width = 0;
        std::int32_t height = 0;
    };

    struct WindowDesc
    {
        string title{ "Epoch" };
        std::int32_t width = 1280;
        std::int32_t height = 720;
        bool resizable = true;
        bool visible = true;
    };

    class IWindowSystem
    {
    public:
        virtual ~IWindowSystem() noexcept = default;

        [[nodiscard]] virtual core::error::result<WindowHandle> create_window(const WindowDesc& desc) noexcept = 0;
        virtual void destroy_window(WindowHandle handle) noexcept = 0;
        virtual void pump_events(const epochengine::function_ref<void(const WindowEvent&)>& handler) noexcept = 0;
        virtual void request_close(WindowHandle handle) noexcept = 0;
        virtual void set_title(WindowHandle handle, string_view title) noexcept = 0;
        [[nodiscard]] virtual WindowHandle primary_window() const noexcept = 0;
    };

    // Minimum positive OS pause, without a frame-rate deadline. Actual duration
    // depends on the platform scheduler; this is not a nanosecond precision claim.
    void cooperative_pause() noexcept;

    [[nodiscard]] core::error::result<std::unique_ptr<IWindowSystem>> create_window_system() noexcept;
} // namespace epochengine::platform::window_adapter
