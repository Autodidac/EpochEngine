// SPDX-License-Identifier: MPL-2.0
#include <epochplatformengine/epoch_platform_engine.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <string_view>

namespace
{
    using namespace std::chrono_literals;
    using epochengine::platform::ApplicationConfig;
    using epochengine::platform::ApplicationLaunchInfo;
    using epochengine::platform::ContextDesc;
    using epochengine::platform::ContextId;
    using epochengine::platform::ContextSnapshot;
    using epochengine::platform::ContextType;
    using epochengine::platform::DriverKind;
    using epochengine::platform::IApplication;
    using epochengine::platform::LifecycleState;
    using epochengine::platform::Runtime;
    using epochengine::platform::Status;
    using epochengine::platform::WindowDesc;
    using epochengine::platform::WindowMode;

    constexpr std::string_view primary_route{ "release.primary" };
    constexpr std::string_view tools_route{ "release.tools" };
    constexpr std::string_view worker_route{ "release.worker" };

    [[nodiscard]] bool has_argument(
        const ApplicationLaunchInfo& launch,
        const std::string_view expected) noexcept
    {
        if (launch.argv == nullptr)
            return false;

        for (int index = 1; index < launch.argc; ++index)
        {
            if (launch.argv[index] != nullptr && std::string_view{ launch.argv[index] } == expected)
                return true;
        }
        return false;
    }

    [[nodiscard]] bool is_active(const LifecycleState state) noexcept
    {
        return state == LifecycleState::pending ||
               state == LifecycleState::initializing ||
               state == LifecycleState::ready;
    }

    class ReleaseExampleApplication final : public IApplication
    {
    public:
        [[nodiscard]] ApplicationConfig configure(const ApplicationLaunchInfo&) override
        {
            return {
                .runtime = {
                    .dedicated_driver_threads = true,
                    .auto_register_builtin_drivers = true,
                    .idle_wait = 1ms
                },
                .quit_when_no_active_contexts = true,
                .main_loop_idle_wait = 1ms
            };
        }

        [[nodiscard]] Status on_start(
            Runtime& runtime,
            const ApplicationLaunchInfo& launch) override
        {
            auto_exit_requested_ = has_argument(launch, "--auto-exit");

            auto primary = runtime.create_context(
                ContextDesc{
                    .driver = DriverKind::automatic,
                    .type = ContextType::software,
                    .window = WindowDesc{
                        .title = "EpochPlatformEngine Release Example - Primary",
                        .width = 960,
                        .height = 540,
                        .x = 80,
                        .y = 80,
                        .mode = WindowMode::top_level,
                        .visible = true,
                        .resizable = true,
                        .decorated = true,
                        .high_dpi = true,
                        .accept_file_drop = true
                    },
                    .route = std::string{ primary_route }
                },
                {
                    .initialize = [this](const ContextSnapshot& snapshot) -> Status
                    {
                        primary_headless_.store(
                            snapshot.driver_kind == DriverKind::headless,
                            std::memory_order_release);
                        return {};
                    },
                    .frame = [this](const ContextSnapshot&, std::chrono::nanoseconds)
                    {
                        primary_frames_.fetch_add(1, std::memory_order_relaxed);
                        return true;
                    }
                });
            if (!primary)
                return std::unexpected(primary.error());
            primary_ = *primary;

            auto tools = runtime.create_context(
                ContextDesc{
                    .driver = DriverKind::automatic,
                    .type = ContextType::software,
                    .window = WindowDesc{
                        .title = "EpochPlatformEngine Release Example - Detached Tools",
                        .width = 520,
                        .height = 360,
                        .x = 1080,
                        .y = 140,
                        .mode = WindowMode::detached,
                        .visible = true,
                        .resizable = true,
                        .decorated = true,
                        .high_dpi = true
                    },
                    .route = std::string{ tools_route }
                },
                {
                    .initialize = [this](const ContextSnapshot& snapshot) -> Status
                    {
                        tools_headless_.store(
                            snapshot.driver_kind == DriverKind::headless,
                            std::memory_order_release);
                        return {};
                    },
                    .frame = [this](const ContextSnapshot&, std::chrono::nanoseconds)
                    {
                        tools_frames_.fetch_add(1, std::memory_order_relaxed);
                        return true;
                    }
                });
            if (!tools)
                return std::unexpected(tools.error());
            tools_ = *tools;

            auto worker = runtime.create_context(
                ContextDesc{
                    .driver = DriverKind::headless,
                    .type = ContextType::noop,
                    .window = WindowDesc{
                        .title = "EpochPlatformEngine Release Example - Worker",
                        .width = 1,
                        .height = 1,
                        .visible = false
                    },
                    .route = std::string{ worker_route }
                },
                {
                    .frame = [this](const ContextSnapshot&, std::chrono::nanoseconds)
                    {
                        worker_ticks_.fetch_add(1, std::memory_order_relaxed);
                        return true;
                    }
                });
            if (!worker)
                return std::unexpected(worker.error());
            worker_ = *worker;

            return {};
        }

        [[nodiscard]] bool on_frame(Runtime& runtime, const std::chrono::nanoseconds delta) override
        {
            elapsed_ += delta;
            title_elapsed_ += delta;

            if (title_elapsed_ >= 250ms)
            {
                title_elapsed_ = 0ns;
                update_title(runtime, primary_, "Primary", primary_frames_.load(std::memory_order_relaxed));
                update_title(runtime, tools_, "Detached Tools", tools_frames_.load(std::memory_order_relaxed));
            }

            bool primary_active = false;
            bool tools_active = false;
            for (const auto& context : runtime.contexts())
            {
                if (!is_active(context.lifecycle))
                    continue;
                if (context.route == primary_route)
                    primary_active = true;
                else if (context.route == tools_route)
                    tools_active = true;
            }

            if (!primary_active && !tools_active && !worker_close_requested_.exchange(true))
                runtime.request_context_close(worker_);

            const bool headless_fallback =
                primary_headless_.load(std::memory_order_acquire) &&
                tools_headless_.load(std::memory_order_acquire);

            if ((auto_exit_requested_ || headless_fallback) &&
                elapsed_ >= 8s &&
                !all_close_requested_.exchange(true))
            {
                runtime.request_context_close(primary_);
                runtime.request_context_close(tools_);
                runtime.request_context_close(worker_);
            }

            return runtime.active_context_count() != 0;
        }

        void on_stop(Runtime&) noexcept override {}

    private:
        static void update_title(
            Runtime& runtime,
            const ContextId context,
            const std::string_view label,
            const std::uint64_t frames)
        {
            const auto snapshot = runtime.context(context);
            if (!snapshot || !is_active(snapshot->lifecycle))
                return;

            std::string title{ "EpochPlatformEngine Release Example - " };
            title.append(label);
            title.append(" | ");
            title.append(epochengine::platform::to_string(snapshot->driver_kind));
            title.append(" | frames ");
            title.append(std::to_string(frames));
            static_cast<void>(runtime.set_title(context, title));
        }

        ContextId primary_{};
        ContextId tools_{};
        ContextId worker_{};
        std::atomic_uint64_t primary_frames_{};
        std::atomic_uint64_t tools_frames_{};
        std::atomic_uint64_t worker_ticks_{};
        std::atomic_bool primary_headless_{};
        std::atomic_bool tools_headless_{};
        std::atomic_bool worker_close_requested_{};
        std::atomic_bool all_close_requested_{};
        std::chrono::nanoseconds elapsed_{};
        std::chrono::nanoseconds title_elapsed_{};
        bool auto_exit_requested_{};
    };

    IApplication* EPOCH_PLATFORM_CALL create_application(
        const ApplicationLaunchInfo&) noexcept
    {
        return new (std::nothrow) ReleaseExampleApplication();
    }

    void EPOCH_PLATFORM_CALL destroy_application(IApplication* application) noexcept
    {
        delete application;
    }
}

extern "C" void EPOCH_PLATFORM_CALL epoch_platform_application_factory(
    epochengine::platform::ApplicationFactory* factory) noexcept
{
    if (factory == nullptr)
        return;

    *factory = {
        .create = &create_application,
        .destroy = &destroy_application
    };
}
