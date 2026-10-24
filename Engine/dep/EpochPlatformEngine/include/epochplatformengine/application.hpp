// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "runtime.hpp"

#include <chrono>

namespace epochengine::platform
{
    struct ApplicationLaunchInfo final
    {
        int argc{};
        const char* const* argv{};
        void* native_application_instance{};
        int native_show_command{};
    };

    struct ApplicationConfig final
    {
        RuntimeDesc runtime{};
        bool quit_when_no_active_contexts{ true };
        std::chrono::milliseconds main_loop_idle_wait{ 1 };
    };

    class EPOCH_PLATFORM_API IApplication
    {
    public:
        virtual ~IApplication() noexcept = default;

        [[nodiscard]] virtual ApplicationConfig configure(const ApplicationLaunchInfo&) { return {}; }
        [[nodiscard]] virtual Status on_start(Runtime&, const ApplicationLaunchInfo&) = 0;
        [[nodiscard]] virtual bool on_frame(Runtime&, std::chrono::nanoseconds delta) = 0;
        virtual void on_stop(Runtime&) noexcept = 0;
    };

    struct ApplicationFactory final
    {
        IApplication* (EPOCH_PLATFORM_CALL* create)(const ApplicationLaunchInfo&) noexcept{};
        void (EPOCH_PLATFORM_CALL* destroy)(IApplication*) noexcept{};
    };

    [[nodiscard]] EPOCH_PLATFORM_API int run_application(const ApplicationFactory& factory, const ApplicationLaunchInfo& launch) noexcept;
}

extern "C" void EPOCH_PLATFORM_CALL epoch_platform_application_factory(epochengine::platform::ApplicationFactory* factory) noexcept;
