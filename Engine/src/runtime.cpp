#include "../include/app_api.h"
#include "../include/epoch.api_types.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <print>
#include <utility>

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
import runtime;

import aengine.cli;

import core.env;
import core.format;
import core.log;
import core.time;
import epoch.perf.select;
import epoch.perf.tier;
import epoch.platform.capabilities;
import epoch.platform.context;
import epoch.platform.window;
import epoch.systems;

namespace epochnamespace::core::bridge
{
    int run_legacy_runtime(bool editor_mode);
}

namespace runtime
{
    namespace
    {
        std::string_view to_std(epoch::string_view v) noexcept
        {
            return std::string_view{ v.data ? v.data : "", v.size };
        }

        struct RuntimePlatformGuard
        {
            std::unique_ptr<epoch::platform::IWindowSystem> window_system{};
            std::unique_ptr<epoch::platform::IGraphicsContext> graphics_context{};
            epoch::platform::WindowHandle window_handle{};

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
            if (epochnamespace::core::cli::smoke_requested)
                return true;

            if (auto v = epoch::core::env::get("DEMO_SMOKE"))
            {
                const auto value = to_std(*v);
                return value == std::string_view{ "1" }
                    || value == std::string_view{ "true" }
                    || value == std::string_view{ "on" };
            }

            return false;
        }

        [[nodiscard]] epoch::Capabilities probe_capabilities(const epoch::platform::IGraphicsContext* context) noexcept
        {
            epoch::Capabilities caps{};
            caps.device_name = "epoch-runtime";

            if (!context)
            {
                caps.api_name = "none";
                return caps;
            }

            using Backend = epoch::platform::GraphicsBackend;
            switch (context->backend())
            {
            case Backend::vulkan:
                caps.api_name = "vulkan";
                caps.descriptor_indexing = true;
                caps.bindless_textures = true;
                caps.indirect_draw = true;
                caps.multi_draw_indirect = true;
                caps.subgroup_ops = true;
                caps.async_compute = true;
                caps.max_sampled_images = 8192;
                caps.max_samplers = 4096;
                break;
            case Backend::d3d12:
                caps.api_name = "d3d12";
                caps.descriptor_indexing = true;
                caps.bindless_textures = true;
                caps.indirect_draw = true;
                caps.multi_draw_indirect = true;
                caps.subgroup_ops = true;
                caps.async_compute = true;
                caps.max_sampled_images = 8192;
                caps.max_samplers = 4096;
                break;
            case Backend::opengl:
                caps.api_name = "opengl";
                caps.indirect_draw = true;
                caps.multi_draw_indirect = true;
                caps.max_sampled_images = 2048;
                caps.max_samplers = 1024;
                break;
            case Backend::null_backend:
            default:
                caps.api_name = "null";
                caps.indirect_draw = false;
                caps.max_sampled_images = 512;
                caps.max_samplers = 256;
                caps.max_storage_buffers = 64;
                caps.max_uniform_buffers = 64;
                break;
            }

            return caps;
        }

        int run_legacy_bridge(bool editor_requested)
        {
            epoch::core::log::write(
                epoch::core::log::level::info,
                "engine",
                editor_requested
                    ? "epoch runtime bridge -> legacy editor"
                    : "epoch runtime bridge -> legacy engine");

            return epochnamespace::core::bridge::run_legacy_runtime(editor_requested);
        }

        int run_epoch_native()
        {
            epoch::core::log::write(epoch::core::log::level::info, "engine", "startup");
            epoch::core::log::write(epoch::core::log::level::info, "engine", "epoch native runtime");

            const app_callbacks_v1* callbacks = app_get_callbacks();
            if (!callbacks)
            {
                epoch::core::log::write(epoch::core::log::level::error, "engine", "app_get_callbacks() returned nullptr");
                return 1;
            }

            if (callbacks->version != APP_API_VERSION)
            {
                epoch::core::log::write(epoch::core::log::level::error, "engine",
                    epoch::core::format::str("app callbacks version mismatch ({} != {})",
                        callbacks->version, APP_API_VERSION));
                return 1;
            }

            void* cb_user = callbacks->user;

            RuntimePlatformGuard platform_guard{};

            auto window_system_result = epoch::platform::create_window_system();
            if (!window_system_result)
            {
                const auto& err = window_system_result.error();
                epoch::core::log::write(epoch::core::log::level::error, "engine",
                    epoch::core::format::str("window system init failed: {}", err.message));
                return 1;
            }
            platform_guard.window_system = std::move(*window_system_result);

            epoch::platform::WindowDesc window_desc{};
            auto window_result = platform_guard.window_system->create_window(window_desc);
            if (!window_result)
            {
                const auto& err = window_result.error();
                epoch::core::log::write(epoch::core::log::level::error, "engine",
                    epoch::core::format::str("window creation failed: {}", err.message));
                return 1;
            }
            platform_guard.window_handle = *window_result;

            epoch::platform::ContextDesc context_desc{};
            auto context_result = epoch::platform::create_graphics_context(context_desc);
            if (!context_result)
            {
                const auto& err = context_result.error();
                epoch::core::log::write(epoch::core::log::level::error, "engine",
                    epoch::core::format::str("graphics context init failed: {}", err.message));
                return 1;
            }
            platform_guard.graphics_context = std::move(*context_result);

            if (auto surface_result = platform_guard.graphics_context->create_surface(platform_guard.window_handle); !surface_result)
            {
                const auto& err = surface_result.error();
                epoch::core::log::write(epoch::core::log::level::error, "engine",
                    epoch::core::format::str("surface creation failed: {}", err.message));
                return 1;
            }

            auto& systems_registry = epoch::systems::Registry::instance();
            if (!systems_registry.initialize())
            {
                epoch::core::log::write(epoch::core::log::level::error, "engine", "system registry init failed");
                return 1;
            }

            const int init_rc = callbacks->on_init ? callbacks->on_init(cb_user) : 0;
            if (init_rc != 0)
            {
                epoch::core::log::write(epoch::core::log::level::error, "engine",
                    epoch::core::format::str("app init failed with code {}", init_rc));
                systems_registry.shutdown();
                return init_rc;
            }

            epoch::core::time::frame_clock fc{};
            fc.start();

            const epoch::Capabilities caps = probe_capabilities(platform_guard.graphics_context.get());
            const auto perf_tier = epoch::perf::select_tier(caps);
            const double target_fps = epoch::perf::target_fps_for(perf_tier);

            epoch::perf::frame_limiter limiter{};
            limiter.set_target_fps(target_fps);

            epoch::core::log::write(epoch::core::log::level::info, "engine",
                epoch::core::format::str("perf tier={}, target_fps={}",
                    epoch::perf::to_string(perf_tier), target_fps));

            const bool smoke = smoke_mode();
            const std::uint64_t max_frames = smoke ? 3u : ~0ull;

            constexpr std::uint64_t FPS_PRINT_EVERY = 10;
            constexpr double WARMUP_SECONDS = 7.0;

            const double t0 = epoch::core::time::now_seconds();

            double last_t = t0;
            std::uint64_t last_frame = fc.frame_index;

            double min_fps = std::numeric_limits<double>::infinity();
            double max_fps = 0.0;
            double fps = 0.0;

            bool warmup_done = false;
            bool window_close_requested = false;

            while (fc.frame_index < max_frames)
            {
                fc.tick();
                systems_registry.update(fc.dt_seconds());

                platform_guard.window_system->pump_events([&](const epoch::platform::WindowEvent& event)
                    {
                        switch (event.type)
                        {
                        case epoch::platform::WindowEventType::close:
                            window_close_requested = true;
                            break;
                        case epoch::platform::WindowEventType::resized:
                            platform_guard.graphics_context->resize_surface(event.handle, event.width, event.height);
                            break;
                        default:
                            break;
                        }
                    });

                const double now = epoch::core::time::now_seconds();
                const double elapsed_since_start = now - t0;

                if (!warmup_done)
                {
                    const double remaining = WARMUP_SECONDS - elapsed_since_start;
                    if (remaining > 0.0)
                    {
                        std::print("\r[ engine ] warming up... {:.0f}s", remaining);
                    }
                    else
                    {
                        warmup_done = true;
                        last_t = now;
                        last_frame = fc.frame_index;
                        std::print("\r[ engine ] warm-up complete        ");
                    }
                }
                else
                {
                    if (callbacks->on_tick)
                        callbacks->on_tick(cb_user, fc.frame_index, fc.dt_seconds());

                    if ((fc.frame_index % FPS_PRINT_EVERY) == 0)
                    {
                        std::print(
                            "\r\n[ engine ] running (frame={}, dt_ms={:.3f})",
                            fc.frame_index,
                            fc.dt_seconds() * 1000.0
                        );

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

                        std::print(
                            "\r\n[ engine ] FPS={:.2f} (min={:.2f}, max={:.2f})",
                            fps, min_fps, max_fps
                        );

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
                epoch::core::log::write(epoch::core::log::level::info, "engine", "smoke complete");

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

        if (options.path == Path::LegacyParity)
            return run_legacy_bridge(false);

        return run_epoch_native();
    }
} // namespace runtime


