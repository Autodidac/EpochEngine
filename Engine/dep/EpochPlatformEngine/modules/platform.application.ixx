// SPDX-License-Identifier: MPL-2.0
module;
#include "epoch.software_application.hpp"
export module platform.application;
export import platform.engine;
export namespace epochengine::platform::application
{
    using ::app_callbacks_v1;
    inline constexpr std::uint32_t callback_api_version = APP_API_VERSION;
    using software::Profile;
    using software::WindowEventKind;
    using software::WindowEvent;
    using software::WindowOptions;
    using software::LaunchOptions;
    using software::StartupReport;
    using software::ExitReason;
    using software::RunReport;
    using software::run;
    using software::profile_name;
}
// Required only by executables opting into Epoch::PlatformEntry. Applications
// own options/callback storage; the provider is called once before callbacks.
export extern "C" const epochengine::software::LaunchOptions*
    app_get_launch_options(int argc, char** argv) noexcept;
export extern "C" const app_callbacks_v1* APP_CALL app_get_callbacks(void);
