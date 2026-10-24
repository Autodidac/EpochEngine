// SPDX-License-Identifier: MPL-2.0
#include <epochplatformengine/driver.hpp>

#include <deque>
#include <mutex>
#include <unordered_map>

namespace epochengine::platform
{
    namespace
    {
        class HeadlessDriver final : public IPlatformDriver
        {
        public:
            [[nodiscard]] DriverKind kind() const noexcept override { return DriverKind::headless; }
            [[nodiscard]] std::string_view name() const noexcept override { return "EpochPlatformEngine Headless"; }
            [[nodiscard]] DriverCapabilities capabilities() const noexcept override
            {
                return {
                    .native_windows = false,
                    .multiple_windows = true,
                    .parented_windows = false,
                    .detached_windows = true,
                    .high_dpi = false,
                    .file_drop = false,
                    .supports_dedicated_thread = true,
                    .requires_main_thread = false
                };
            }

            [[nodiscard]] Status initialize(const DriverInit&) noexcept override
            {
                initialized_ = true;
                return {};
            }

            void shutdown() noexcept override
            {
                std::scoped_lock lock(mutex_);
                windows_.clear();
                events_.clear();
                initialized_ = false;
            }

            [[nodiscard]] Result<NativeWindowHandle> create_window(const ContextId context, const WindowDesc& desc) noexcept override
            {
                if (!initialized_)
                    return std::unexpected(Error{ ErrorCode::initialization_failed, "Headless driver is not initialized" });

                std::scoped_lock lock(mutex_);
                windows_[context] = desc;
                return NativeWindowHandle{
                    .kind = NativeHandleKind::headless,
                    .value = context.value
                };
            }

            void destroy_window(const ContextId context) noexcept override
            {
                std::scoped_lock lock(mutex_);
                if (windows_.erase(context) != 0)
                    events_.push_back(WindowEvent{ .type = WindowEventType::destroyed, .context = context });
            }

            void pump_events(const EventSink& sink) noexcept override
            {
                std::deque<WindowEvent> local;
                {
                    std::scoped_lock lock(mutex_);
                    local.swap(events_);
                }
                for (const auto& event : local)
                    sink(event);
            }

            [[nodiscard]] NativeWindowHandle native_handle(const ContextId context) const noexcept override
            {
                std::scoped_lock lock(mutex_);
                return windows_.contains(context)
                    ? NativeWindowHandle{ .kind = NativeHandleKind::headless, .value = context.value }
                    : NativeWindowHandle{};
            }

            [[nodiscard]] Status set_title(const ContextId context, const std::string_view title) noexcept override
            {
                std::scoped_lock lock(mutex_);
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "Headless context not found" });
                iterator->second.title.assign(title);
                return {};
            }

            [[nodiscard]] Status set_visible(const ContextId context, const bool visible) noexcept override
            {
                std::scoped_lock lock(mutex_);
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "Headless context not found" });
                iterator->second.visible = visible;
                return {};
            }

            [[nodiscard]] Status resize(const ContextId context, const std::int32_t width, const std::int32_t height) noexcept override
            {
                std::scoped_lock lock(mutex_);
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "Headless context not found" });
                iterator->second.width = width;
                iterator->second.height = height;
                events_.push_back(WindowEvent{
                    .type = WindowEventType::resized,
                    .context = context,
                    .width = width,
                    .height = height
                });
                return {};
            }

            [[nodiscard]] Status reparent(const ContextId context, NativeWindowHandle, WindowMode mode) noexcept override
            {
                std::scoped_lock lock(mutex_);
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "Headless context not found" });
                iterator->second.mode = mode;
                return {};
            }

            void request_close(const ContextId context) noexcept override
            {
                std::scoped_lock lock(mutex_);
                if (windows_.contains(context))
                    events_.push_back(WindowEvent{ .type = WindowEventType::close_requested, .context = context });
            }

        private:
            mutable std::mutex mutex_{};
            std::unordered_map<ContextId, WindowDesc> windows_{};
            std::deque<WindowEvent> events_{};
            bool initialized_{};
        };
    }

    std::unique_ptr<IPlatformDriver> create_headless_driver()
    {
        return std::make_unique<HeadlessDriver>();
    }
}
