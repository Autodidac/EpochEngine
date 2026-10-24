// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "types.hpp"

#include <memory>
#include <string_view>

namespace epochengine::platform
{
    struct DriverInit final
    {
        DriverId id{};
        ThreadPolicy thread_policy{ ThreadPolicy::automatic };
        void* native_application_instance{};
    };

    struct EventSink final
    {
        void* user{};
        void (*emit)(void* user, const WindowEvent& event) noexcept{};

        void operator()(const WindowEvent& event) const noexcept
        {
            if (emit != nullptr)
                emit(user, event);
        }
    };

    class EPOCH_PLATFORM_API IPlatformDriver
    {
    public:
        virtual ~IPlatformDriver() noexcept = default;

        [[nodiscard]] virtual DriverKind kind() const noexcept = 0;
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;
        [[nodiscard]] virtual DriverCapabilities capabilities() const noexcept = 0;

        [[nodiscard]] virtual Status initialize(const DriverInit& init) noexcept = 0;
        virtual void shutdown() noexcept = 0;

        [[nodiscard]] virtual Result<NativeWindowHandle> create_window(ContextId context, const WindowDesc& desc) noexcept = 0;
        virtual void destroy_window(ContextId context) noexcept = 0;
        virtual void pump_events(const EventSink& sink) noexcept = 0;

        [[nodiscard]] virtual NativeWindowHandle native_handle(ContextId context) const noexcept = 0;
        [[nodiscard]] virtual Status set_title(ContextId context, std::string_view title) noexcept = 0;
        [[nodiscard]] virtual Status set_visible(ContextId context, bool visible) noexcept = 0;
        [[nodiscard]] virtual Status resize(ContextId context, std::int32_t width, std::int32_t height) noexcept = 0;
        [[nodiscard]] virtual Status reparent(ContextId context, NativeWindowHandle parent, WindowMode mode) noexcept = 0;
        virtual void request_close(ContextId context) noexcept = 0;
    };

    using DriverFactory = std::unique_ptr<IPlatformDriver> (*)();

    [[nodiscard]] EPOCH_PLATFORM_API std::unique_ptr<IPlatformDriver> create_headless_driver();
#if defined(_WIN32)
    [[nodiscard]] EPOCH_PLATFORM_API std::unique_ptr<IPlatformDriver> create_win32_driver();
#endif
#if defined(__linux__) && defined(EPOCH_PLATFORM_HAS_X11)
    [[nodiscard]] EPOCH_PLATFORM_API std::unique_ptr<IPlatformDriver> create_x11_driver();
#endif
#if defined(__APPLE__)
    [[nodiscard]] EPOCH_PLATFORM_API std::unique_ptr<IPlatformDriver> create_cocoa_driver();
#endif
}
