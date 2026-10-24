// SPDX-License-Identifier: MPL-2.0
#include <epochplatformengine/application.hpp>

#include <chrono>
#include <thread>

namespace epochengine::platform
{
    int run_application(const ApplicationFactory& factory, const ApplicationLaunchInfo& launch) noexcept
    {
        if (factory.create == nullptr || factory.destroy == nullptr)
            return 2;

        IApplication* application = nullptr;
        try
        {
            application = factory.create(launch);
            if (application == nullptr)
                return 3;

            ApplicationConfig config = application->configure(launch);
            config.runtime.native_application_instance = launch.native_application_instance;
            Runtime runtime(config.runtime);

            const Status started = application->on_start(runtime, launch);
            if (!started)
            {
                factory.destroy(application);
                return 4;
            }

            auto previous = std::chrono::steady_clock::now();
            while (runtime.running())
            {
                const auto now = std::chrono::steady_clock::now();
                const auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous);
                previous = now;

                runtime.tick();
                if (!application->on_frame(runtime, delta))
                    runtime.request_stop();

                runtime.collect_stopped_contexts();
                if (config.quit_when_no_active_contexts && runtime.active_context_count() == 0)
                    runtime.request_stop();

                if (config.main_loop_idle_wait.count() > 0)
                    std::this_thread::sleep_for(config.main_loop_idle_wait);
            }

            application->on_stop(runtime);
            factory.destroy(application);
            return 0;
        }
        catch (...)
        {
            if (application != nullptr)
                factory.destroy(application);
            return 5;
        }
    }
}
