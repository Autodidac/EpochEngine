// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "epoch.app_api.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace epochengine::software
{
    enum class Profile : std::uint8_t
    {
        cli,
        platform_window,
    };

    enum class WindowEventKind : std::uint8_t
    {
        created,
        resized,
        close_requested,
    };

    struct WindowEvent
    {
        WindowEventKind kind{};
        // Borrowed while the application runs; never destroy or retain it.
        std::uintptr_t native_window{};
        // Created echoes requested outer size; resized reports the native
        // client size. Neither is a framebuffer/presentation measurement.
        std::int32_t width{};
        std::int32_t height{};
    };

    struct WindowOptions
    {
        std::string title{ "Epoch Software" };
        std::int32_t width{ 960 };
        std::int32_t height{ 640 };
        bool resizable{ true };
        bool visible{ true };
        void* observer_user{};
        // Created is delivered before on_init; resize/close precede on_tick.
        void (*on_event)(void*, const WindowEvent&){};
    };

    struct StartupReport
    {
        std::string_view platform_key{};
        std::string_view selected_tier{};
        std::string_view selection_reason{};
        // Existing policy advice, never installed as a timer or FPS cap.
        double suggested_target_fps{};
        bool native_window_available{};
        bool native_input_adapter_available{};
        bool multicontext_host_available{};
        bool capabilities_observed{};
    };

    struct LaunchOptions
    {
        Profile profile{ Profile::cli };
        // Zero runs until should_quit or the native window closes.
        std::uint64_t maximum_ticks{ 1 };
        std::uint32_t tick_interval_ms{};
        WindowOptions window{};
        void* startup_user{};
        // After profile admission, before native resources and on_init. The
        // borrowed immutable snapshot can be copied for later use.
        void (*on_startup)(void*, const StartupReport&){};
    };

    enum class ExitReason : std::uint8_t
    {
        tick_limit,
        application_quit,
        window_closed,
        invalid_options,
        invalid_callbacks,
        unsupported_platform,
        window_host_busy,
        window_creation_failed,
        application_init_failed,
        callback_failed,
        host_failed,
    };

    struct RunReport
    {
        ExitReason reason{ ExitReason::host_failed };
        std::string_view diagnostic{ "Application did not start." };
        std::uint64_t ticks{};
        int application_init_code{};
        bool init_entered{};
        bool initialized{};
        bool shutdown_entered{};
        bool shutdown_failed{};
        bool window_created{};
        bool profile_preflight_passed{};
        // Means the owned lease was released, not a presentation/capture claim.
        bool window_released{};
        StartupReport startup{};
        bool startup_selected{};

        [[nodiscard]] constexpr bool succeeded() const noexcept
        {
            return !shutdown_failed && (reason == ExitReason::tick_limit
                || reason == ExitReason::application_quit
                || reason == ExitReason::window_closed);
        }
    };

    // A separate, editor-free application composition. No legacy fallback,
    // renderer, GUI, updater, project, AI, or child-process service is started.
    // Callbacks run on the calling thread. One PlatformWindow session may run
    // at a time in this software base; the native event pump is not an isolated
    // per-window dispatcher. Keyboard/pointer/text and drawing are not exposed.
    // on_tick is required; on_shutdown,
    // when supplied, runs exactly once after any attempted on_init, including
    // initialization failure. A window remains owned until shutdown returns.
    [[nodiscard]] RunReport run(
        const LaunchOptions& options,
        const app_callbacks_v1& callbacks) noexcept;

    [[nodiscard]] constexpr std::string_view profile_name(Profile profile) noexcept
    {
        switch (profile)
        {
        case Profile::cli: return "cli";
        case Profile::platform_window: return "platform-window";
        }
        return "invalid";
    }
}
