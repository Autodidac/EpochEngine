// SPDX-License-Identifier: MPL-2.0
#pragma once
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include "export.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace epochengine::platform
{
    enum class ErrorCode : std::uint8_t
    {
        none = 0,
        invalid_argument,
        unsupported,
        driver_unavailable,
        initialization_failed,
        window_creation_failed,
        context_not_found,
        driver_failure,
        thread_failure,
        internal_error
    };

    struct Error final
    {
        ErrorCode code{ ErrorCode::none };
        std::string message{};
    };

    template <typename T>
    using Result = std::expected<T, Error>;

    using Status = Result<void>;

    struct DriverId final
    {
        std::uint64_t value{};
        [[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
        friend constexpr bool operator==(DriverId, DriverId) noexcept = default;
    };

    struct ContextId final
    {
        std::uint64_t value{};
        [[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
        friend constexpr bool operator==(ContextId, ContextId) noexcept = default;
    };

    enum class RuntimePlatform : std::uint8_t
    {
        Unknown = 0,
        Windows,
        Linux,
        MacOS,
        Android,
        IOS,
        Web
    };

    enum class DriverKind : std::uint8_t
    {
        automatic = 0,
        headless,
        win32,
        x11,
        cocoa,
        custom
    };

    enum class ContextType : std::uint8_t
    {
        none = 0,
        opengl,
        sdl,
        sfml,
        raylib,
        vulkan,
        directx,
        software,
        custom,
        noop
    };

    enum class LifecycleState : std::uint8_t
    {
        pending = 0,
        initializing,
        ready,
        failed,
        stopping,
        stopped
    };

    enum class ThreadPolicy : std::uint8_t
    {
        automatic = 0,
        caller_thread,
        dedicated_driver_thread
    };

    enum class NativeHandleKind : std::uint8_t
    {
        none = 0,
        headless,
        win32,
        x11,
        cocoa,
        custom
    };

    struct NativeWindowHandle final
    {
        NativeHandleKind kind{ NativeHandleKind::none };
        void* display{};
        void* window{};
        void* parent{};
        void* device_context{};
        void* graphics_context{};
        std::uintptr_t value{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return kind != NativeHandleKind::none && (window != nullptr || value != 0 || kind == NativeHandleKind::headless);
        }
    };

    enum class WindowMode : std::uint8_t
    {
        top_level = 0,
        child,
        detached
    };

    struct WindowDesc final
    {
        std::string title{ "EpochPlatformEngine" };
        std::int32_t width{ 1280 };
        std::int32_t height{ 720 };
        std::int32_t x{ -1 };
        std::int32_t y{ -1 };
        WindowMode mode{ WindowMode::top_level };
        NativeWindowHandle parent{};
        bool visible{ true };
        bool resizable{ true };
        bool decorated{ true };
        bool high_dpi{ true };
        bool accept_file_drop{ false };
    };

    enum class WindowEventType : std::uint8_t
    {
        none = 0,
        close_requested,
        destroyed,
        resized,
        moved,
        focus_gained,
        focus_lost,
        key_pressed,
        key_released,
        text_input,
        mouse_moved,
        mouse_button_pressed,
        mouse_button_released,
        mouse_wheel,
        dpi_changed,
        file_dropped
    };

    struct WindowEvent final
    {
        WindowEventType type{ WindowEventType::none };
        ContextId context{};
        std::int32_t x{};
        std::int32_t y{};
        // For resized events these are client dimensions. For
        // dpi_changed events they carry horizontal and vertical monitor DPI.
        std::int32_t width{};
        std::int32_t height{};
        std::int32_t key{};
        std::int32_t button{};
        std::int32_t wheel_delta{};
        char32_t codepoint{};
        bool repeated{};
        std::string text{};
    };

    struct DriverCapabilities final
    {
        bool native_windows{};
        bool multiple_windows{ true };
        bool parented_windows{};
        bool detached_windows{};
        bool high_dpi{};
        bool file_drop{};
        bool supports_dedicated_thread{ true };
        bool requires_main_thread{};
    };

    struct RuntimePolicy final
    {
        RuntimePlatform platform{ RuntimePlatform::Unknown };
        DriverKind preferred_driver{ DriverKind::headless };
        bool supports_parented_multiwindow{};
        bool prefer_single_context_runtime{};
    };

    [[nodiscard]] constexpr RuntimePlatform current_platform() noexcept
    {
#if defined(__EMSCRIPTEN__)
        return RuntimePlatform::Web;
#elif defined(__ANDROID__)
        return RuntimePlatform::Android;
#elif defined(__APPLE__) && TARGET_OS_IPHONE
        return RuntimePlatform::IOS;
#elif defined(_WIN32)
        return RuntimePlatform::Windows;
#elif defined(__APPLE__)
        return RuntimePlatform::MacOS;
#elif defined(__linux__)
        return RuntimePlatform::Linux;
#else
        return RuntimePlatform::Unknown;
#endif
    }

    [[nodiscard]] EPOCH_PLATFORM_API RuntimePolicy current_runtime_policy() noexcept;
    [[nodiscard]] EPOCH_PLATFORM_API std::string_view to_string(RuntimePlatform value) noexcept;
    [[nodiscard]] EPOCH_PLATFORM_API std::string_view to_string(DriverKind value) noexcept;
    [[nodiscard]] EPOCH_PLATFORM_API std::string_view to_string(ContextType value) noexcept;
}

namespace std
{
    template <>
    struct hash<epochengine::platform::DriverId>
    {
        size_t operator()(const epochengine::platform::DriverId id) const noexcept { return hash<uint64_t>{}(id.value); }
    };

    template <>
    struct hash<epochengine::platform::ContextId>
    {
        size_t operator()(const epochengine::platform::ContextId id) const noexcept { return hash<uint64_t>{}(id.value); }
    };
}
