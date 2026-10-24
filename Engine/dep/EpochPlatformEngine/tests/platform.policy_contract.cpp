// SPDX-License-Identifier: MPL-2.0
#include <array>
#include <string_view>
import platform.engine;
int main()
{
    using namespace epochengine::platform;
    static_assert(static_cast<int>(RuntimePlatform::Windows) == 1);
    static_assert(static_cast<int>(RuntimePlatform::Linux) == 2);
    static_assert(static_cast<int>(RuntimePlatform::MacOS) == 3);
    static_assert(static_cast<int>(RuntimePlatform::Unknown) == 0);
    constexpr std::array platforms{RuntimePlatform::Windows, RuntimePlatform::Linux,
        RuntimePlatform::MacOS, RuntimePlatform::Android, RuntimePlatform::IOS,
        RuntimePlatform::Web, RuntimePlatform::Unknown};
    for (const auto platform : platforms)
    {
        const auto policy = policy::runtime_policy_for(platform);
        const auto support = policy::services_for(platform);
        if (policy.platform != platform || support.limitation.empty()) return 1;
        if (support.multicontext_host) return 2;
        if (support.native_window != (platform == RuntimePlatform::Windows)) return 3;
        if (support.native_input_adapter != support.native_window) return 4;
        const bool desktop = platform == RuntimePlatform::Windows || platform == RuntimePlatform::Linux
            || platform == RuntimePlatform::MacOS;
        if (support.desktop_entry != desktop) return 5;
    }
    if (policy::runtime_policy_for(RuntimePlatform::Android).platform_key != "android"
        || policy::runtime_policy_for(RuntimePlatform::IOS).platform_key != "ios"
        || policy::runtime_policy_for(RuntimePlatform::Web).platform_key != "web") return 6;
    if (policy::current_runtime_policy().platform != current_platform()) return 7;
    return 0;
}
