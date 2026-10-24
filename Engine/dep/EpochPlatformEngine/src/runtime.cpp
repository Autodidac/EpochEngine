// SPDX-License-Identifier: MPL-2.0
#include "runtime_internal.hpp"

#include <algorithm>

namespace epochengine::platform
{
    Runtime::Runtime(RuntimeDesc desc)
        : impl_(std::make_unique<Impl>(std::move(desc)))
    {
    }

    Runtime::~Runtime() noexcept = default;
    Runtime::Runtime(Runtime&&) noexcept = default;
    Runtime& Runtime::operator=(Runtime&&) noexcept = default;

    Status Runtime::register_driver(const DriverKind kind, const DriverFactory factory)
    {
        if (kind == DriverKind::automatic || factory == nullptr)
            return std::unexpected(make_runtime_error(ErrorCode::invalid_argument, "register_driver requires a concrete kind and non-null factory"));

        std::unique_lock lock(impl_->registry_mutex);
        if (impl_->hosts.contains(kind))
            return std::unexpected(make_runtime_error(ErrorCode::invalid_argument, "Cannot replace a driver after its host has started"));
        impl_->factories[kind] = factory;
        return {};
    }

    bool Runtime::has_driver(const DriverKind kind) const noexcept
    {
        std::shared_lock lock(impl_->registry_mutex);
        return impl_->factories.contains(kind);
    }

    std::vector<DriverKind> Runtime::available_drivers() const
    {
        std::vector<DriverKind> result;
        std::shared_lock lock(impl_->registry_mutex);
        result.reserve(impl_->factories.size());
        for (const auto& [kind, factory] : impl_->factories)
        {
            if (factory != nullptr)
                result.emplace_back(kind);
        }
        std::ranges::sort(result, {}, [](const DriverKind value) { return static_cast<std::uint8_t>(value); });
        return result;
    }

    Result<ContextId> Runtime::create_context(const ContextDesc& desc, ContextCallbacks callbacks)
    {
        if (desc.window.width <= 0 || desc.window.height <= 0)
            return std::unexpected(make_runtime_error(ErrorCode::invalid_argument, "Context window dimensions must be positive"));

        auto host_result = impl_->ensure_host(desc.driver, desc.thread_policy);
        if (!host_result && desc.driver == DriverKind::automatic && default_driver_kind() != DriverKind::headless)
            host_result = impl_->ensure_host(DriverKind::headless, desc.thread_policy);
        if (!host_result)
            return std::unexpected(host_result.error());
        auto* host = *host_result;

        const ContextId id{ impl_->next_context_id.fetch_add(1, std::memory_order_relaxed) };
        auto record = std::make_shared<ContextRecord>();
        record->id = id;
        record->driver_id = host->id;
        record->driver_kind = host->kind;
        record->desc = desc;
        record->desc.driver = host->kind;
        record->callbacks = std::move(callbacks);
        record->width.store(desc.window.width, std::memory_order_relaxed);
        record->height.store(desc.window.height, std::memory_order_relaxed);
        record->lifecycle.store(LifecycleState::initializing, std::memory_order_release);

        {
            std::unique_lock lock(impl_->registry_mutex);
            impl_->contexts.emplace(id, record);
        }

        Result<NativeWindowHandle> native_result;
        try
        {
            native_result = host->execute_sync([host, record]
            {
                return host->driver->create_window(record->id, record->desc.window);
            });
        }
        catch (const std::exception& exception)
        {
            native_result = std::unexpected(make_runtime_error(ErrorCode::thread_failure, exception.what()));
        }
        catch (...)
        {
            native_result = std::unexpected(make_runtime_error(ErrorCode::thread_failure, "Unknown driver-thread failure"));
        }

        if (!native_result)
        {
            record->lifecycle.store(LifecycleState::failed, std::memory_order_release);
            std::unique_lock lock(impl_->registry_mutex);
            impl_->contexts.erase(id);
            return std::unexpected(native_result.error());
        }

        {
            std::scoped_lock lock(record->state_mutex);
            record->native = *native_result;
        }

        if (record->callbacks.initialize)
        {
            Status initialize_status;
            try
            {
                initialize_status = host->execute_sync([this, record]
                {
                    return record->callbacks.initialize(impl_->snapshot(*record));
                });
            }
            catch (const std::exception& exception)
            {
                initialize_status = std::unexpected(make_runtime_error(ErrorCode::initialization_failed, exception.what()));
            }
            catch (...)
            {
                initialize_status = std::unexpected(make_runtime_error(ErrorCode::initialization_failed, "Unknown context initialization failure"));
            }

            if (!initialize_status)
            {
                host->execute_sync([this, host, record] { impl_->finalize_context_on_host(*host, record); });
                std::unique_lock lock(impl_->registry_mutex);
                impl_->contexts.erase(id);
                return std::unexpected(initialize_status.error());
            }
        }

        record->last_frame = std::chrono::steady_clock::now();
        record->lifecycle.store(LifecycleState::ready, std::memory_order_release);
        return id;
    }

    Status Runtime::destroy_context(const ContextId context)
    {
        const auto record = impl_->find_context(context);
        if (!record)
            return std::unexpected(make_runtime_error(ErrorCode::context_not_found, "Context not found"));

        auto* host = impl_->find_host(record->driver_kind);
        if (host == nullptr)
            return std::unexpected(make_runtime_error(ErrorCode::driver_unavailable, "Owning platform driver is unavailable"));

        try
        {
            host->execute_sync([this, host, record] { impl_->finalize_context_on_host(*host, record); });
        }
        catch (const std::exception& exception)
        {
            return std::unexpected(make_runtime_error(ErrorCode::thread_failure, exception.what()));
        }

        std::unique_lock lock(impl_->registry_mutex);
        impl_->contexts.erase(context);
        return {};
    }

    void Runtime::request_context_close(const ContextId context) noexcept
    {
        const auto record = impl_->find_context(context);
        if (!record)
            return;
        record->close_requested.store(true, std::memory_order_release);
        if (auto* host = impl_->find_host(record->driver_kind))
            host->execute_async([host, context] { host->driver->request_close(context); });
    }

    Status Runtime::dispatch(const ContextId context, ContextCommand command)
    {
        if (!command)
            return std::unexpected(make_runtime_error(ErrorCode::invalid_argument, "dispatch requires a valid command"));
        const auto record = impl_->find_context(context);
        if (!record)
            return std::unexpected(make_runtime_error(ErrorCode::context_not_found, "Context not found"));
        auto* host = impl_->find_host(record->driver_kind);
        if (!host)
            return std::unexpected(make_runtime_error(ErrorCode::driver_unavailable, "Owning platform driver is unavailable"));

        host->execute_async([owner = impl_.get(), record, command = std::move(command)]() mutable
        {
            if (record->lifecycle.load(std::memory_order_acquire) == LifecycleState::ready)
                command(owner->snapshot(*record));
        });
        return {};
    }

    Status Runtime::set_title(const ContextId context, const std::string_view title)
    {
        const auto record = impl_->find_context(context);
        if (!record)
            return std::unexpected(make_runtime_error(ErrorCode::context_not_found, "Context not found"));
        auto* host = impl_->find_host(record->driver_kind);
        if (!host)
            return std::unexpected(make_runtime_error(ErrorCode::driver_unavailable, "Owning platform driver is unavailable"));

        auto status = host->execute_sync([host, context, title = std::string(title)] { return host->driver->set_title(context, title); });
        if (status)
            record->desc.window.title.assign(title);
        return status;
    }

    Status Runtime::set_visible(const ContextId context, const bool visible)
    {
        const auto record = impl_->find_context(context);
        if (!record)
            return std::unexpected(make_runtime_error(ErrorCode::context_not_found, "Context not found"));
        auto* host = impl_->find_host(record->driver_kind);
        if (!host)
            return std::unexpected(make_runtime_error(ErrorCode::driver_unavailable, "Owning platform driver is unavailable"));

        auto status = host->execute_sync([host, context, visible] { return host->driver->set_visible(context, visible); });
        if (status)
            record->desc.window.visible = visible;
        return status;
    }

    Status Runtime::resize(const ContextId context, const std::int32_t width, const std::int32_t height)
    {
        if (width <= 0 || height <= 0)
            return std::unexpected(make_runtime_error(ErrorCode::invalid_argument, "resize dimensions must be positive"));
        const auto record = impl_->find_context(context);
        if (!record)
            return std::unexpected(make_runtime_error(ErrorCode::context_not_found, "Context not found"));
        auto* host = impl_->find_host(record->driver_kind);
        if (!host)
            return std::unexpected(make_runtime_error(ErrorCode::driver_unavailable, "Owning platform driver is unavailable"));

        auto status = host->execute_sync([host, context, width, height] { return host->driver->resize(context, width, height); });
        if (status)
        {
            record->width.store(width, std::memory_order_release);
            record->height.store(height, std::memory_order_release);
        }
        return status;
    }

    Status Runtime::reparent(const ContextId context, const NativeWindowHandle parent, const WindowMode mode)
    {
        const auto record = impl_->find_context(context);
        if (!record)
            return std::unexpected(make_runtime_error(ErrorCode::context_not_found, "Context not found"));
        auto* host = impl_->find_host(record->driver_kind);
        if (!host)
            return std::unexpected(make_runtime_error(ErrorCode::driver_unavailable, "Owning platform driver is unavailable"));

        auto status = host->execute_sync([host, context, parent, mode] { return host->driver->reparent(context, parent, mode); });
        if (status)
        {
            record->desc.window.parent = parent;
            record->desc.window.mode = mode;
            std::scoped_lock lock(record->state_mutex);
            record->native = host->driver->native_handle(context);
        }
        return status;
    }

    Result<ContextSnapshot> Runtime::context(const ContextId context) const
    {
        const auto record = impl_->find_context(context);
        if (!record)
            return std::unexpected(make_runtime_error(ErrorCode::context_not_found, "Context not found"));
        return impl_->snapshot(*record);
    }

    std::vector<ContextSnapshot> Runtime::contexts() const
    {
        std::vector<std::shared_ptr<ContextRecord>> records;
        {
            std::shared_lock lock(impl_->registry_mutex);
            records.reserve(impl_->contexts.size());
            for (const auto& [id, record] : impl_->contexts)
            {
                (void)id;
                records.emplace_back(record);
            }
        }

        std::vector<ContextSnapshot> result;
        result.reserve(records.size());
        for (const auto& record : records)
            result.emplace_back(impl_->snapshot(*record));
        return result;
    }

    bool Runtime::poll_event(const ContextId context, WindowEvent& event)
    {
        const auto record = impl_->find_context(context);
        if (!record)
            return false;
        std::scoped_lock lock(record->state_mutex);
        if (record->events.empty())
            return false;
        event = std::move(record->events.front());
        record->events.pop_front();
        return true;
    }

    void Runtime::tick()
    {
        std::vector<Impl::DriverHost*> caller_hosts;
        {
            std::shared_lock lock(impl_->registry_mutex);
            for (const auto& [kind, host] : impl_->hosts)
            {
                (void)kind;
                if (!host->threaded())
                    caller_hosts.emplace_back(host.get());
            }
        }
        for (auto* host : caller_hosts)
            host->tick_caller_thread();
    }

    void Runtime::collect_stopped_contexts()
    {
        std::unique_lock lock(impl_->registry_mutex);
        std::erase_if(impl_->contexts, [](const auto& item)
        {
            return item.second->lifecycle.load(std::memory_order_acquire) == LifecycleState::stopped;
        });
    }

    void Runtime::request_stop() noexcept
    {
        impl_->running.store(false, std::memory_order_release);
    }

    bool Runtime::running() const noexcept
    {
        return impl_->running.load(std::memory_order_acquire);
    }

    std::size_t Runtime::active_context_count() const noexcept
    {
        std::shared_lock lock(impl_->registry_mutex);
        return static_cast<std::size_t>(std::ranges::count_if(impl_->contexts, [](const auto& item)
        {
            const auto state = item.second->lifecycle.load(std::memory_order_acquire);
            return state == LifecycleState::initializing || state == LifecycleState::ready;
        }));
    }

    std::size_t Runtime::context_count() const noexcept
    {
        std::shared_lock lock(impl_->registry_mutex);
        return impl_->contexts.size();
    }
}
