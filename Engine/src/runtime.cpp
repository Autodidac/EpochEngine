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

#include "../include/epoch.runtime_bridge.hpp"
#include "../include/app_api.h"
#include "../src/epoch.api_types.hpp"
#include "../include/_epoch.stl_types.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <source_location>
#include <utility>

module runtime;

//import engine.cli;
import core.logger;
import engine.platform;

import core.env;
import core.format;
import core.time;
import perf.tier;
import platform.context;
import platform.runtime;
import platform.window;
import epoch.systems;
import core.commandline;

extern "C"
{
    namespace
    {
        int APP_CALL epoch_default_on_init(void*)
        {
            return 0;
        }

        void APP_CALL epoch_default_on_tick(void*, std::uint64_t, double)
        {
        }

        bool APP_CALL epoch_default_should_quit(void*)
        {
            return false;
        }

        void APP_CALL epoch_default_on_shutdown(void*)
        {
        }

        const app_callbacks_v1 k_epoch_default_callbacks{
            APP_API_VERSION,
            sizeof(app_callbacks_v1),
            nullptr,
            &epoch_default_on_init,
            &epoch_default_on_tick,
            &epoch_default_should_quit,
            &epoch_default_on_shutdown,
        };
    }

#if !defined(EPOCH_EXTERNAL_APP_CALLBACKS)
    const app_callbacks_v1* APP_CALL app_get_callbacks(void)
    {
        return &k_epoch_default_callbacks;
    }
#endif
}

namespace runtime
{
    namespace
    {
#if defined(EPOCH_EXTERNAL_APP_CALLBACKS)
        constexpr bool k_has_external_app_callbacks = true;
#else
        constexpr bool k_has_external_app_callbacks = false;
#endif

        inline void runtime_info(const std::string_view message)
        {
            epochengine::logger::get("Epoch.Runtime").log(
                epochengine::logger::LogLevel::INFO,
                message,
                std::source_location::current());
        }

        inline void runtime_error(const std::string_view message)
        {
            epochengine::logger::get("Epoch.Runtime").log(
                epochengine::logger::LogLevel::Error,
                message,
                std::source_location::current());
        }

        [[nodiscard]] inline std::string_view as_std_view(const epochengine::string& text) noexcept
        {
            return std::string_view{ text.data(), text.size() };
        }

        struct RuntimePlatformGuard
        {
            std::unique_ptr<epochengine::platform::IWindowSystem> window_system{};
            std::unique_ptr<epochengine::platform::IGraphicsContext> graphics_context{};
            epochengine::platform::WindowHandle window_handle{};

            ~RuntimePlatformGuard() noexcept
            {
                if (graphics_context)
                    graphics_context->teardown();
                if (window_system && window_handle.valid())
                    window_system->destroy_window(window_handle);
            }
        };

        [[nodiscard]] bool smoke_mode()
        {
            if (epochengine::core::cli::smoke_requested)
                return true;

            if (auto v = epochengine::core::env::get("DEMO_SMOKE"))
            {
                const auto value = as_std_view(*v);
                return value == std::string_view{ "1" }
                    || value == std::string_view{ "true" }
                    || value == std::string_view{ "on" };
            }

            return false;
        }

        int run_legacy_bridge(bool editor_requested)
        {
            runtime_info(
                editor_requested
                    ? "epoch runtime bridge -> legacy editor"
                    : "epoch runtime bridge -> legacy engine");

            return epochengine::core::bridge::run_legacy_runtime(editor_requested);
        }

        int run_epoch_native()
        {
            runtime_info("startup");
            runtime_info("epoch native runtime");

            const app_callbacks_v1* callbacks = app_get_callbacks();
            if (!callbacks)
            {
                runtime_error("app_get_callbacks() returned nullptr");
                return 1;
            }

            if (callbacks->version != APP_API_VERSION)
            {
                runtime_error(as_std_view(epochengine::core::format::str(
                    "app callbacks version mismatch ({} != {})",
                    callbacks->version,
                    APP_API_VERSION)));
                return 1;
            }

            void* cb_user = callbacks->user;

            RuntimePlatformGuard platform_guard{};

            auto window_system_result = epochengine::platform::create_window_system();
            if (!window_system_result)
            {
                const auto& err = window_system_result.error();
                runtime_error(as_std_view(epochengine::core::format::str("window system init failed: {}", err.message)));
                return 1;
            }
            platform_guard.window_system = std::move(*window_system_result);

            epochengine::platform::WindowDesc window_desc{};
            auto window_result = platform_guard.window_system->create_window(window_desc);
            if (!window_result)
            {
                const auto& err = window_result.error();
                runtime_error(as_std_view(epochengine::core::format::str("window creation failed: {}", err.message)));
                return 1;
            }
            platform_guard.window_handle = *window_result;

            epochengine::platform::ContextDesc context_desc{};
            auto context_result = epochengine::platform::create_graphics_context(context_desc);
            if (!context_result)
            {
                const auto& err = context_result.error();
                runtime_error(as_std_view(epochengine::core::format::str("graphics context init failed: {}", err.message)));
                return 1;
            }
            platform_guard.graphics_context = std::move(*context_result);

            if (auto surface_result = platform_guard.graphics_context->create_surface(platform_guard.window_handle); !surface_result)
            {
                const auto& err = surface_result.error();
                runtime_error(as_std_view(epochengine::core::format::str("surface creation failed: {}", err.message)));
                return 1;
            }

            auto& systems_registry = epochengine::systems::Registry::instance();
            if (!systems_registry.initialize())
            {
                runtime_error("system registry init failed");
                return 1;
            }

            const int init_rc = callbacks->on_init ? callbacks->on_init(cb_user) : 0;
            if (init_rc != 0)
            {
                runtime_error(as_std_view(epochengine::core::format::str("app init failed with code {}", init_rc)));
                systems_registry.shutdown();
                return init_rc;
            }

            epochengine::core::time::frame_clock fc{};
            fc.start();

            const auto runtime_profile = epochengine::platform::build_runtime_frame_profile(platform_guard.graphics_context.get());

            epochengine::perf::frame_limiter limiter{};
            limiter.set_target_fps(runtime_profile.target_fps);
            epochengine::platform::log_runtime_profile("Epoch.Runtime", "Epoch.Perf", runtime_profile);

            const bool smoke = smoke_mode();
            const std::uint64_t max_frames = smoke
                ? (epochengine::core::cli::capture_requested ? 180u : 3u)
                : ~0ull;

            constexpr std::uint64_t FPS_PRINT_EVERY = 10;
            constexpr double WARMUP_SECONDS = 7.0;

            const double t0 = epochengine::core::time::now_seconds();

            double last_t = t0;
            std::uint64_t last_frame = fc.frame_index;

            double min_fps = std::numeric_limits<double>::infinity();
            double max_fps = 0.0;
            double fps = 0.0;

            bool warmup_done = false;
            int last_warmup_second_logged = -1;
            bool window_close_requested = false;

            while (fc.frame_index < max_frames)
            {
                fc.tick();
                systems_registry.update(fc.dt_seconds());

                platform_guard.window_system->pump_events([&](const epochengine::platform::WindowEvent& event)
                    {
                        switch (event.type)
                        {
                        case epochengine::platform::WindowEventType::close:
                            window_close_requested = true;
                            break;
                        case epochengine::platform::WindowEventType::resized:
                            platform_guard.graphics_context->resize_surface(event.handle, event.width, event.height);
                            break;
                        default:
                            break;
                        }
                    });

                const double now = epochengine::core::time::now_seconds();
                const double elapsed_since_start = now - t0;

                if (!warmup_done)
                {
                    const double remaining = WARMUP_SECONDS - elapsed_since_start;
                    if (remaining > 0.0)
                    {
                        const int whole_remaining = static_cast<int>(std::ceil(remaining));
                        if (whole_remaining != last_warmup_second_logged)
                        {
                            last_warmup_second_logged = whole_remaining;
                            runtime_info(as_std_view(epochengine::core::format::str(
                                "warming up ({}s remaining)",
                                whole_remaining)));
                        }
                    }
                    else
                    {
                        warmup_done = true;
                        last_t = now;
                        last_frame = fc.frame_index;
                        runtime_info("warm-up complete");
                    }
                }
                else
                {
                    if (callbacks->on_tick)
                        callbacks->on_tick(cb_user, fc.frame_index, fc.dt_seconds());

                    if ((fc.frame_index % FPS_PRINT_EVERY) == 0)
                    {
                        runtime_info(as_std_view(epochengine::core::format::str(
                            "running (frame={}, dt_ms={:.3f})",
                            fc.frame_index,
                            fc.dt_seconds() * 1000.0
                        )));

                        const std::uint64_t frames = fc.frame_index - last_frame;
                        const double elapsed = now - last_t;

                        if (elapsed > 0.0)
                        {
                            fps = static_cast<double>(frames) / elapsed;
                            min_fps = std::min(min_fps, fps);
                            max_fps = std::max(max_fps, fps);
                        }
                        else
                        {
                            fps = 0.0;
                        }

                        runtime_info(as_std_view(epochengine::core::format::str(
                            "FPS={:.2f} (min={:.2f}, max={:.2f})",
                            fps, min_fps, max_fps
                        )));

                        last_t = now;
                        last_frame = fc.frame_index;
                    }
                }

                const bool app_quit = (callbacks->should_quit) ? callbacks->should_quit(cb_user) : false;
                if (window_close_requested || app_quit)
                    break;

                limiter.wait_for_next_frame();
            }

            if (callbacks->on_shutdown)
                callbacks->on_shutdown(cb_user);

            systems_registry.shutdown();

            if (smoke)
                runtime_info("smoke complete");

            return 0;
        }
    }

    int run()
    {
        return run({});
    }

    int run(const LaunchOptions& options)
    {
        if (options.editor_requested)
            return run_legacy_bridge(true);

        if constexpr (!k_has_external_app_callbacks)
        {
            runtime_info("native app callbacks not linked; using full engine runtime");
            return run_legacy_bridge(false);
        }

        if (options.path == Path::LegacyParity)
            return run_legacy_bridge(false);

        return run_epoch_native();
    }
} // namespace runtime
