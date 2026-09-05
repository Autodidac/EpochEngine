// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include "epoch.software_application.hpp"

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

import core.log;

namespace
{
    using namespace epochengine::software;

    void log(std::string_view message, bool error = false)
    {
        epochengine::core::log::write(
            error ? epochengine::core::log::level::error : epochengine::core::log::level::info,
            "Epoch.SoftwareApp", { message.data(), message.size() });
    }

    void usage()
    {
        log("EpochSoftware --profile cli|platform-window [--ticks N] [--interval-ms N] [--title TEXT]");
        log("CLI defaults to one immediate tick. PlatformWindow defaults to 16 ms ticks until closed.");
        log("PlatformWindow is a native OS window, not a renderer. No editor, GUI, updater, project or AI is started.");
    }

    bool number(std::string_view text, std::uint64_t& value)
    {
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        return !text.empty() && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
    }

    struct Application
    {
        std::uint64_t ticks{};
        bool invalid_tick{};
    };

    int APP_CALL initialize(void*)
    {
        log("application.init; first tick has no warmup delay");
        return 0;
    }

    void APP_CALL tick(void* user, std::uint64_t frame, double elapsed)
    {
        auto& app = *static_cast<Application*>(user);
        app.invalid_tick = app.invalid_tick || frame != app.ticks || elapsed < 0.0;
        ++app.ticks;
    }

    bool APP_CALL should_quit(void* user)
    {
        return static_cast<Application*>(user)->invalid_tick;
    }

    void APP_CALL shutdown(void*)
    {
        log("application.shutdown; no child processes or renderer were started");
    }

    void observe_window(void*, const WindowEvent& event)
    {
        switch (event.kind)
        {
        case WindowEventKind::created:
            log("window.created; native handle available during this application only; no rendering claimed");
            break;
        case WindowEventKind::resized:
            log("window.resized width=" + std::to_string(event.width)
                + " height=" + std::to_string(event.height));
            break;
        case WindowEventKind::close_requested:
            log("window.close_requested");
            break;
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        LaunchOptions options{};
        bool profile_selected = false;
        bool ticks_selected = false;
        bool interval_selected = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view key{ argv[i] };
            if (key == "--help" || key == "-h")
            {
                usage();
                return 0;
            }
            if (i + 1 == argc)
            {
                log("Missing argument value.", true);
                return 2;
            }
            const std::string_view value{ argv[++i] };
            if (key == "--profile" && !profile_selected)
            {
                if (value == "cli") options.profile = Profile::cli;
                else if (value == "platform-window") options.profile = Profile::platform_window;
                else
                {
                    log("Unknown profile. Select cli or platform-window.", true);
                    return 2;
                }
                profile_selected = true;
            }
            else if (key == "--ticks" && !ticks_selected)
            {
                if (!number(value, options.maximum_ticks))
                {
                    log("Tick count must be an unsigned integer; zero means until closed/requested quit.", true);
                    return 2;
                }
                ticks_selected = true;
            }
            else if (key == "--interval-ms" && !interval_selected)
            {
                std::uint64_t interval{};
                if (!number(value, interval) || interval > 1000)
                {
                    log("Tick interval must be between 0 and 1000 ms.", true);
                    return 2;
                }
                options.tick_interval_ms = static_cast<std::uint32_t>(interval);
                interval_selected = true;
            }
            else if (key == "--title") options.window.title = value;
            else
            {
                log("Unknown or repeated argument.", true);
                return 2;
            }
        }
        if (!profile_selected)
        {
            usage();
            return argc == 1 ? 0 : 2;
        }
        if (options.profile == Profile::platform_window)
        {
            if (!ticks_selected) options.maximum_ticks = 0;
            if (!interval_selected) options.tick_interval_ms = 16;
            options.window.on_event = observe_window;
        }
        Application app{};
        const app_callbacks_v1 callbacks{
            APP_API_VERSION, sizeof(app_callbacks_v1), &app,
            initialize, tick, should_quit, shutdown };
        const auto report = run(options, callbacks);
        const bool passed = report.succeeded() && !app.invalid_tick && report.ticks == app.ticks;
        log(std::string{ "software_application.result=" } + (passed ? "pass" : "fail")
            + " profile=" + std::string{ profile_name(options.profile) }
            + " ticks=" + std::to_string(report.ticks)
            + " window_created=" + std::to_string(report.window_created)
            + " window_released=" + std::to_string(report.window_released), !passed);
        return passed ? 0 : 1;
    }
    catch (...)
    {
        log("Software application entry failed.", true);
        return 1;
    }
}
