// SPDX-License-Identifier: MPL-2.0
#include <epochplatformengine/types.hpp>

#include <cstdlib>

namespace epochengine::platform
{
    RuntimePolicy current_runtime_policy() noexcept
    {
        switch (current_platform())
        {
        case RuntimePlatform::Windows:
            return {
                .platform = RuntimePlatform::Windows,
                .preferred_driver = DriverKind::win32,
                .supports_parented_multiwindow = true,
                .prefer_single_context_runtime = false
            };
        case RuntimePlatform::Linux:
        {
#if defined(__linux__)
            const char* const display = std::getenv("DISPLAY");
            const bool display_available = display != nullptr && *display != '\0';
#else
            constexpr bool display_available = false;
#endif
            return {
                .platform = RuntimePlatform::Linux,
#if defined(EPOCH_PLATFORM_HAS_X11)
                .preferred_driver = display_available ? DriverKind::x11 : DriverKind::headless,
#else
                .preferred_driver = DriverKind::headless,
#endif
                .supports_parented_multiwindow = display_available,
                .prefer_single_context_runtime = false
            };
        }
        case RuntimePlatform::MacOS:
            return {
                .platform = RuntimePlatform::MacOS,
                .preferred_driver = DriverKind::cocoa,
                .supports_parented_multiwindow = false,
                .prefer_single_context_runtime = true
            };
        case RuntimePlatform::Unknown:
        default:
            return {};
        }
    }

    std::string_view to_string(const RuntimePlatform value) noexcept
    {
        switch (value)
        {
        case RuntimePlatform::Windows: return "windows";
        case RuntimePlatform::Linux: return "linux";
        case RuntimePlatform::MacOS: return "macos";
        case RuntimePlatform::Android: return "android";
        case RuntimePlatform::IOS: return "ios";
        case RuntimePlatform::Web: return "web";
        default: return "unknown";
        }
    }

    std::string_view to_string(const DriverKind value) noexcept
    {
        switch (value)
        {
        case DriverKind::automatic: return "automatic";
        case DriverKind::headless: return "headless";
        case DriverKind::win32: return "win32";
        case DriverKind::x11: return "x11";
        case DriverKind::cocoa: return "cocoa";
        case DriverKind::custom: return "custom";
        default: return "unknown";
        }
    }

    std::string_view to_string(const ContextType value) noexcept
    {
        switch (value)
        {
        case ContextType::none: return "none";
        case ContextType::opengl: return "opengl";
        case ContextType::sdl: return "sdl";
        case ContextType::sfml: return "sfml";
        case ContextType::raylib: return "raylib";
        case ContextType::vulkan: return "vulkan";
        case ContextType::directx: return "directx";
        case ContextType::software: return "software";
        case ContextType::custom: return "custom";
        case ContextType::noop: return "noop";
        default: return "unknown";
        }
    }

    DriverKind default_driver_kind() noexcept
    {
        return current_runtime_policy().preferred_driver;
    }
}
