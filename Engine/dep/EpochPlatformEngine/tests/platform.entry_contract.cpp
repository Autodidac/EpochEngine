// SPDX-License-Identifier: MPL-2.0
#include <cstdint>
#include <string_view>
import platform.application;

namespace app = epochengine::platform::application;
namespace
{
    unsigned ticks{};
    bool initialized{};
    bool startup_observed{};
    bool arguments_valid{};
    int initialize(void*) { initialized = true; return arguments_valid && startup_observed ? 0 : 31; }
    void tick(void*, std::uint64_t frame, double elapsed)
    {
        if (!initialized || frame != ticks || elapsed < 0) throw 1;
        ++ticks;
    }
    void shutdown(void*)
    {
        if (!initialized || ticks != 3) throw 2;
        initialized = false;
    }
    void startup(void*, const app::StartupReport& report)
    {
        if (initialized || report.platform_key.empty() || report.selected_tier.empty()
            || report.capabilities_observed) throw 3;
        startup_observed = true;
    }
    app::LaunchOptions options{};
    app::app_callbacks_v1 callbacks{app::callback_api_version, sizeof(app::app_callbacks_v1),
        nullptr, initialize, tick, nullptr, shutdown};
}
extern "C" const app::LaunchOptions* app_get_launch_options(int argc, char** argv) noexcept
{
    arguments_valid = argc == 2 && std::string_view{argv[1]} == "--contract" && argv[argc] == nullptr;
    options.maximum_ticks = 3;
    options.on_startup = startup;
    return &options;
}
extern "C" const app::app_callbacks_v1* app_get_callbacks(void) { return &callbacks; }
