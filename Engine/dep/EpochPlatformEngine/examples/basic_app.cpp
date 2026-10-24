// SPDX-License-Identifier: MPL-2.0
#include <epochplatformengine/epoch_platform_engine.hpp>

#include <atomic>
#include <iostream>
#include <memory>

namespace
{
    class ExampleApplication final : public epochengine::platform::IApplication
    {
    public:
        [[nodiscard]] epochengine::platform::ApplicationConfig configure(
            const epochengine::platform::ApplicationLaunchInfo&) override
        {
            return {
                .runtime = {
                    .dedicated_driver_threads = true,
                    .auto_register_builtin_drivers = true,
                    .idle_wait = std::chrono::milliseconds(1)
                },
                .quit_when_no_active_contexts = true,
                .main_loop_idle_wait = std::chrono::milliseconds(1)
            };
        }

        [[nodiscard]] epochengine::platform::Status on_start(
            epochengine::platform::Runtime& runtime,
            const epochengine::platform::ApplicationLaunchInfo&) override
        {
            using namespace epochengine::platform;

            auto primary = runtime.create_context(
                ContextDesc{
                    .driver = DriverKind::automatic,
                    .type = ContextType::software,
                    .window = WindowDesc{
                        .title = "EpochPlatformEngine Primary",
                        .width = 960,
                        .height = 540,
                        .visible = true
                    },
                    .route = "primary"
                },
                ContextCallbacks{
                    .initialize = [](const ContextSnapshot& snapshot) -> Status
                    {
                        std::cout << "Initialized " << snapshot.route << " on "
                                  << to_string(snapshot.driver_kind) << '\n';
                        return {};
                    },
                    .frame = [this](const ContextSnapshot&, std::chrono::nanoseconds)
                    {
                        return ++primary_frames_ < 180;
                    },
                    .shutdown = [](const ContextSnapshot& snapshot)
                    {
                        std::cout << "Stopped " << snapshot.route << '\n';
                    }
                });
            if (!primary)
                return std::unexpected(primary.error());
            primary_ = *primary;

            auto worker = runtime.create_context(
                ContextDesc{
                    .driver = DriverKind::headless,
                    .type = ContextType::noop,
                    .window = WindowDesc{
                        .title = "EpochPlatformEngine Background Context",
                        .width = 640,
                        .height = 360,
                        .visible = false
                    },
                    .route = "background"
                },
                ContextCallbacks{
                    .frame = [this](const ContextSnapshot&, std::chrono::nanoseconds)
                    {
                        return ++background_frames_ < 120;
                    }
                });
            if (!worker)
                return std::unexpected(worker.error());
            background_ = *worker;

            return {};
        }

        [[nodiscard]] bool on_frame(
            epochengine::platform::Runtime& runtime,
            std::chrono::nanoseconds) override
        {
            return runtime.active_context_count() != 0;
        }

        void on_stop(epochengine::platform::Runtime&) noexcept override
        {
            std::cout << "EpochPlatformEngine example completed with "
                      << primary_frames_.load() << " primary frames and "
                      << background_frames_.load() << " background frames.\n";
        }

    private:
        epochengine::platform::ContextId primary_{};
        epochengine::platform::ContextId background_{};
        std::atomic_uint32_t primary_frames_{};
        std::atomic_uint32_t background_frames_{};
    };

    epochengine::platform::IApplication* EPOCH_PLATFORM_CALL create_application(
        const epochengine::platform::ApplicationLaunchInfo&) noexcept
    {
        return new (std::nothrow) ExampleApplication();
    }

    void EPOCH_PLATFORM_CALL destroy_application(epochengine::platform::IApplication* application) noexcept
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
