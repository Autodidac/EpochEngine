// SPDX-License-Identifier: MPL-2.0
// Canonical native API: declarations retain their header/global-module identity.
module;
#include <epochplatformengine/epoch_platform_engine.hpp>
export module platform.native;
export namespace epochengine::platform
{
    using ::epochengine::platform::ApplicationLaunchInfo;
    using ::epochengine::platform::ApplicationConfig;
    using ::epochengine::platform::IApplication;
    using ::epochengine::platform::ApplicationFactory;
    using ::epochengine::platform::run_application;
    using ::epochengine::platform::ErrorCode;
    using ::epochengine::platform::Error;
    using ::epochengine::platform::Result;
    using ::epochengine::platform::Status;
    using ::epochengine::platform::DriverId;
    using ::epochengine::platform::ContextId;
    using ::epochengine::platform::RuntimePlatform;
    using ::epochengine::platform::DriverKind;
    using ::epochengine::platform::ContextType;
    using ::epochengine::platform::LifecycleState;
    using ::epochengine::platform::ThreadPolicy;
    using ::epochengine::platform::NativeHandleKind;
    using ::epochengine::platform::NativeWindowHandle;
    using ::epochengine::platform::WindowMode;
    using ::epochengine::platform::WindowDesc;
    using ::epochengine::platform::WindowEventType;
    using ::epochengine::platform::WindowEvent;
    using ::epochengine::platform::DriverCapabilities;
    using ::epochengine::platform::RuntimePolicy;
    using ::epochengine::platform::current_platform;
    using ::epochengine::platform::current_runtime_policy;
    using ::epochengine::platform::to_string;
    using ::epochengine::platform::DriverInit;
    using ::epochengine::platform::EventSink;
    using ::epochengine::platform::IPlatformDriver;
    using ::epochengine::platform::DriverFactory;
    using ::epochengine::platform::create_headless_driver;
    using ::epochengine::platform::ContextSnapshot;
    using ::epochengine::platform::ContextInitializeCallback;
    using ::epochengine::platform::ContextFrameCallback;
    using ::epochengine::platform::ContextShutdownCallback;
    using ::epochengine::platform::ContextEventCallback;
    using ::epochengine::platform::ContextCommand;
    using ::epochengine::platform::ContextCallbacks;
    using ::epochengine::platform::ContextDesc;
    using ::epochengine::platform::RuntimeDesc;
    using ::epochengine::platform::Runtime;
    using ::epochengine::platform::default_driver_kind;
#if defined(_WIN32)
    using ::epochengine::platform::create_win32_driver;
#endif
#if defined(__linux__) && defined(EPOCH_PLATFORM_HAS_X11)
    using ::epochengine::platform::create_x11_driver;
#endif
#if defined(__APPLE__)
    using ::epochengine::platform::create_cocoa_driver;
#endif
}
