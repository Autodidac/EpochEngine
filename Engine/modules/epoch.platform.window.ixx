/**************************************************************
 *   Epoch Engine - Modern Renderer Skeleton (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

#include "../include/_epoch.stl_types.hpp"

export module epoch.platform.window;

import core.error;

export namespace epoch::platform
{
    struct WindowHandle
    {
        std::uintptr_t value = 0;

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return value != 0;
        }
    };

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
        virtual void pump_events(const epoch::function_ref<void(const WindowEvent&)>& handler) noexcept = 0;
        virtual void request_close(WindowHandle handle) noexcept = 0;
        virtual void set_title(WindowHandle handle, string_view title) noexcept = 0;
        [[nodiscard]] virtual WindowHandle primary_window() const noexcept = 0;
    };

    [[nodiscard]] core::error::result<std::unique_ptr<IWindowSystem>> create_window_system() noexcept;
} // namespace epoch::platform
