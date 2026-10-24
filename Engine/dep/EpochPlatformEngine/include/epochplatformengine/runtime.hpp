// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "driver.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace epochengine::platform
{
    struct ContextSnapshot final
    {
        ContextId id{};
        DriverId driver_id{};
        DriverKind driver_kind{ DriverKind::headless };
        ContextType context_type{ ContextType::none };
        LifecycleState lifecycle{ LifecycleState::pending };
        NativeWindowHandle native{};
        std::string route{};
        std::int32_t width{};
        std::int32_t height{};
        bool close_requested{};
    };

    using ContextInitializeCallback = std::function<Status(const ContextSnapshot&)>;
    using ContextFrameCallback = std::function<bool(const ContextSnapshot&, std::chrono::nanoseconds)>;
    using ContextShutdownCallback = std::function<void(const ContextSnapshot&)>;
    using ContextEventCallback = std::function<void(const WindowEvent&)>;
    using ContextCommand = std::function<void(const ContextSnapshot&)>;

    struct ContextCallbacks final
    {
        ContextInitializeCallback initialize{};
        ContextFrameCallback frame{};
        ContextShutdownCallback shutdown{};
        ContextEventCallback event{};
    };

    struct ContextDesc final
    {
        DriverKind driver{ DriverKind::automatic };
        ContextType type{ ContextType::custom };
        ThreadPolicy thread_policy{ ThreadPolicy::automatic };
        WindowDesc window{};
        std::string route{};
    };

    struct RuntimeDesc final
    {
        bool dedicated_driver_threads{ true };
        bool auto_register_builtin_drivers{ true };
        std::chrono::milliseconds idle_wait{ 1 };
        void* native_application_instance{};
    };

#if defined(_MSC_VER) && !defined(EPOCH_PLATFORM_ENGINE_STATIC)
#    pragma warning(push)
#    pragma warning(disable : 4251)
#endif

    class EPOCH_PLATFORM_API Runtime final
    {
    public:
        explicit Runtime(RuntimeDesc desc = {});
        ~Runtime() noexcept;

        Runtime(Runtime&&) noexcept;
        Runtime& operator=(Runtime&&) noexcept;

        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;

        [[nodiscard]] Status register_driver(DriverKind kind, DriverFactory factory);
        [[nodiscard]] bool has_driver(DriverKind kind) const noexcept;
        [[nodiscard]] std::vector<DriverKind> available_drivers() const;

        [[nodiscard]] Result<ContextId> create_context(const ContextDesc& desc, ContextCallbacks callbacks = {});
        [[nodiscard]] Status destroy_context(ContextId context);
        void request_context_close(ContextId context) noexcept;

        [[nodiscard]] Status dispatch(ContextId context, ContextCommand command);
        [[nodiscard]] Status set_title(ContextId context, std::string_view title);
        [[nodiscard]] Status set_visible(ContextId context, bool visible);
        [[nodiscard]] Status resize(ContextId context, std::int32_t width, std::int32_t height);
        [[nodiscard]] Status reparent(ContextId context, NativeWindowHandle parent, WindowMode mode);

        [[nodiscard]] Result<ContextSnapshot> context(ContextId context) const;
        [[nodiscard]] std::vector<ContextSnapshot> contexts() const;
        [[nodiscard]] bool poll_event(ContextId context, WindowEvent& event);

        void tick();
        void collect_stopped_contexts();
        void request_stop() noexcept;
        [[nodiscard]] bool running() const noexcept;
        [[nodiscard]] std::size_t active_context_count() const noexcept;
        [[nodiscard]] std::size_t context_count() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

#if defined(_MSC_VER) && !defined(EPOCH_PLATFORM_ENGINE_STATIC)
#    pragma warning(pop)
#endif

    [[nodiscard]] EPOCH_PLATFORM_API DriverKind default_driver_kind() noexcept;
}
