// SPDX-License-Identifier: MPL-2.0
#include <cstdint>
#include <type_traits>
#include <epochplatformengine/types.hpp>
import platform.native;
import platform.engine;
import platform.window;
import context.type;
int main()
{
    using namespace epochengine::platform;
    static_assert(sizeof(RuntimePlatform) == sizeof(std::uint8_t));
    static_assert(static_cast<unsigned>(RuntimePlatform::Unknown) == 0);
    static_assert(static_cast<unsigned>(RuntimePlatform::Windows) == 1);
    static_assert(static_cast<unsigned>(RuntimePlatform::Linux) == 2);
    static_assert(static_cast<unsigned>(RuntimePlatform::MacOS) == 3);
    static_assert(!std::is_same_v<RuntimePolicy, policy::RuntimePolicy>);
    static_assert(!std::is_same_v<WindowDesc, window_adapter::WindowDesc>);
    static_assert(!std::is_same_v<WindowEvent, window_adapter::WindowEvent>);
    static_assert(static_cast<int>(epochengine::core::ContextType::None) == 0);
    static_assert(static_cast<int>(epochengine::core::ContextType::OpenGL) == 1);
    static_assert(static_cast<int>(epochengine::core::ContextType::Software) == 7);
    static_assert(static_cast<int>(epochengine::core::ContextType::Noop) == 9);
    if (policy::current_runtime_policy().platform != current_platform()) return 6;
    if (current_runtime_policy().platform != current_platform()) return 7;
    if (policy::runtime_policy_for(RuntimePlatform::Linux).source_snapshot_archive
        != policy::SourceSnapshotArchive::TarGz) return 8;
    auto driver = create_headless_driver();
    if (!driver || !driver->initialize({})) return 1;
    const ContextId id{73};
    auto window = driver->create_window(id, {.width=320,.height=200});
    if (!window || !window->valid() || driver->capabilities().native_windows) return 2;
    if (!driver->set_title(id, "module consumer") || !driver->resize(id, 640, 400)) return 3;
    driver->destroy_window(id);
    if (driver->native_handle(id).valid()) return 4;
    driver->shutdown();
    if (to_string(current_platform()).empty()) return 5;
    return 0;
}
