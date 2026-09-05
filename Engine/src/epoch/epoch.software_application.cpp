// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include "epoch.software_application_internal.hpp"

#include <chrono>
#include <atomic>
#include <thread>

import context.admission;
import context.type;
import core.log;

namespace epochengine::software
{
    namespace
    {
        std::atomic_flag platform_window_active = ATOMIC_FLAG_INIT;

        void log_report(const RunReport& report) noexcept
        {
            try
            {
                core::log::write(
                    report.succeeded() ? core::log::level::info : core::log::level::error,
                    "Epoch.Software", { report.diagnostic.data(), report.diagnostic.size() });
            }
            catch (...)
            {
                // Logging cannot prevent application/window cleanup.
            }
        }

        struct WindowLease
        {
            std::unique_ptr<platform::IWindowSystem> system{};
            platform::WindowHandle handle{};
            bool owns_slot{};

            void release() noexcept
            {
                if (system && handle.valid())
                    system->destroy_window(std::exchange(handle, {}));
                system.reset();
                if (std::exchange(owns_slot, false))
                    platform_window_active.clear(std::memory_order_release);
            }

            ~WindowLease() noexcept { release(); }
        };
    }

    RunReport detail::run_with_window_factory(
        const LaunchOptions& options,
        const app_callbacks_v1& supplied_callbacks,
        WindowSystemFactory factory,
        bool native_window_supported) noexcept
    {
        RunReport report{};
        WindowLease window{};
        app_callbacks_v1 callbacks{};
        bool inside_callback{};
        const auto fail = [&](ExitReason reason, std::string_view diagnostic)
        {
            report.reason = reason;
            report.diagnostic = diagnostic;
        };

        const auto execute = [&]
        {
            if ((options.profile != Profile::cli && options.profile != Profile::platform_window)
                || options.tick_interval_ms > 1000)
            {
                fail(ExitReason::invalid_options, "Invalid software profile or tick interval (maximum 1000 ms).");
                return;
            }
            if (supplied_callbacks.version != APP_API_VERSION
                || supplied_callbacks.size < sizeof(app_callbacks_v1) || !supplied_callbacks.on_tick)
            {
                fail(ExitReason::invalid_callbacks, "Application callbacks require the current ABI, full size, and on_tick.");
                return;
            }
            callbacks = supplied_callbacks;

            namespace admission = core::contextadmission;
            admission::ContextProfile profile{};
            const bool is_window = options.profile == Profile::platform_window;
            profile.backend = is_window ? core::ContextType::Custom : core::ContextType::Noop;
            profile.purpose = is_window ? admission::Purpose::platform_window : admission::Purpose::headless;
            profile.window_ownership = is_window ? admission::Ownership::owned : admission::Ownership::none;
            profile.capabilities.availability = !is_window || native_window_supported
                ? admission::Support::supported : admission::Support::unsupported;
            profile.capabilities.embedding = admission::Support::unsupported;
            profile.capabilities.input_route = admission::Support::unsupported;
            profile.capabilities.native_presentation = admission::Support::unsupported;
            profile.capabilities.final_capture = admission::Support::unsupported;
            if (admission::preflight(profile) != admission::Refusal::none)
            {
                fail(ExitReason::unsupported_platform,
                    "PlatformWindow has no native implementation on this platform; no null window was substituted.");
                return;
            }
            report.profile_preflight_passed = true;

            if (options.profile == Profile::platform_window)
            {
                const auto& requested = options.window;
                if (requested.width < 1 || requested.height < 1
                    || requested.width > 16384 || requested.height > 16384
                    || requested.title.size() > 1024
                    || requested.title.find('\0') != std::string::npos)
                {
                    fail(ExitReason::invalid_options, "Invalid native-window dimensions or title.");
                    return;
                }
                if (platform_window_active.test_and_set(std::memory_order_acquire))
                {
                    fail(ExitReason::window_host_busy,
                        "A PlatformWindow application is already active; this native host is single-session.");
                    return;
                }
                window.owns_slot = true;
                if (!factory)
                {
                    fail(ExitReason::window_creation_failed, "No native window factory is available.");
                    return;
                }
                auto created_system = factory();
                if (!created_system || !*created_system)
                {
                    fail(ExitReason::window_creation_failed, "Native window-system creation failed.");
                    return;
                }
                window.system = std::move(*created_system);
                platform::WindowDesc desc{};
                desc.title = requested.title;
                desc.width = requested.width;
                desc.height = requested.height;
                desc.resizable = requested.resizable;
                desc.visible = requested.visible;
                auto created_window = window.system->create_window(desc);
                if (!created_window || !created_window->valid())
                {
                    fail(ExitReason::window_creation_failed, "Native window creation failed.");
                    return;
                }
                window.handle = *created_window;
                report.window_created = true;
                if (requested.on_event)
                {
                    inside_callback = true;
                    requested.on_event(requested.observer_user,
                        { WindowEventKind::created, window.handle.value,
                            requested.width, requested.height });
                    inside_callback = false;
                }
            }

            report.init_entered = true;
            inside_callback = true;
            report.application_init_code = callbacks.on_init ? callbacks.on_init(callbacks.user) : 0;
            inside_callback = false;
            if (report.application_init_code != 0)
            {
                fail(ExitReason::application_init_failed, "Application initialization failed; partial state will be shut down.");
                return;
            }
            report.initialized = true;

            using Clock = std::chrono::steady_clock;
            auto previous_tick = Clock::now();
            while (options.maximum_ticks == 0 || report.ticks < options.maximum_ticks)
            {
                inside_callback = true;
                const bool application_quit = callbacks.should_quit && callbacks.should_quit(callbacks.user);
                inside_callback = false;
                if (application_quit)
                {
                    fail(ExitReason::application_quit, "Application requested a clean shutdown.");
                    return;
                }
                if (window.system)
                {
                    bool close_requested = false;
                    bool observer_failed = false;
                    // IWindowSystem::pump_events is noexcept: user exceptions
                    // must be contained inside this handler, not escape it.
                    const auto observe = [&](const platform::WindowEvent& event) noexcept
                    {
                        if (event.handle.value != window.handle.value || observer_failed)
                            return;
                        WindowEventKind kind{};
                        if (event.type == platform::WindowEventType::close)
                        {
                            close_requested = true;
                            kind = WindowEventKind::close_requested;
                        }
                        else if (event.type == platform::WindowEventType::resized)
                            kind = WindowEventKind::resized;
                        else
                            return;
                        try
                        {
                            if (options.window.on_event)
                                options.window.on_event(options.window.observer_user,
                                    { kind, event.handle.value, event.width, event.height });
                        }
                        catch (...)
                        {
                            observer_failed = true;
                        }
                    };
                    window.system->pump_events(observe);
                    if (observer_failed)
                    {
                        fail(ExitReason::callback_failed, "The window-event callback threw; the application was stopped.");
                        return;
                    }
                    if (close_requested || !window.system->primary_window().valid())
                    {
                        fail(ExitReason::window_closed, "The native window requested a clean shutdown.");
                        return;
                    }
                }

                const auto tick_started = Clock::now();
                const double elapsed = std::chrono::duration<double>(tick_started - previous_tick).count();
                previous_tick = tick_started;
                inside_callback = true;
                callbacks.on_tick(callbacks.user, report.ticks, elapsed);
                inside_callback = false;
                ++report.ticks;
                if (options.maximum_ticks != 0 && report.ticks == options.maximum_ticks)
                    break;
                if (options.tick_interval_ms != 0)
                    std::this_thread::sleep_until(tick_started
                        + std::chrono::milliseconds(options.tick_interval_ms));
            }
            fail(ExitReason::tick_limit, "Application completed its requested tick budget.");
        };

        try
        {
            execute();
        }
        catch (...)
        {
            fail(inside_callback ? ExitReason::callback_failed : ExitReason::host_failed,
                inside_callback ? "Application callback threw; owned resources will be released."
                    : "Software host startup failed; owned resources will be released.");
        }

        if (report.init_entered && callbacks.on_shutdown)
        {
            report.shutdown_entered = true;
            try
            {
                callbacks.on_shutdown(callbacks.user);
            }
            catch (...)
            {
                report.shutdown_failed = true;
                if (report.reason == ExitReason::tick_limit
                    || report.reason == ExitReason::application_quit
                    || report.reason == ExitReason::window_closed)
                    fail(ExitReason::callback_failed, "Application shutdown threw; the native window was still released.");
            }
        }
        window.release();
        report.window_released = report.window_created;
        log_report(report);
        return report;
    }

    RunReport run(const LaunchOptions& options, const app_callbacks_v1& callbacks) noexcept
    {
#if defined(_WIN32)
        constexpr bool native_window_supported = true;
#else
        constexpr bool native_window_supported = false;
#endif
        return detail::run_with_window_factory(options, callbacks,
            &platform::create_window_system, native_window_supported);
    }
}
