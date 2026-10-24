// SPDX-License-Identifier: MPL-2.0
#include "runtime_internal.hpp"

#include <algorithm>

namespace epochengine::platform
{
    Error make_runtime_error(const ErrorCode code, std::string message)
    {
        return Error{ .code = code, .message = std::move(message) };
    }

    bool Runtime::Impl::DriverHost::threaded() const noexcept
    {
        return policy == ThreadPolicy::dedicated_driver_thread;
    }

    Status Runtime::Impl::DriverHost::start(const DriverInit& init)
    {
        if (!threaded())
        {
            owner_thread = std::this_thread::get_id();
            auto status = driver->initialize(init);
            initialized.store(status.has_value(), std::memory_order_release);
            return status;
        }

        auto promise = std::make_shared<std::promise<Status>>();
        auto future = promise->get_future();
        worker = std::jthread([this, init, promise](const std::stop_token token)
        {
            owner_thread = std::this_thread::get_id();
            auto status = driver->initialize(init);
            initialized.store(status.has_value(), std::memory_order_release);
            promise->set_value(status);
            if (!status)
                return;

            const EventSink sink{
                .user = owner,
                .emit = [](void* user, const WindowEvent& event) noexcept
                {
                    static_cast<Impl*>(user)->on_event(event);
                }
            };

            while (!token.stop_requested() && !stopping.load(std::memory_order_acquire))
            {
                process_tasks();
                driver->pump_events(sink);
                owner->run_driver_frames(*this);
                std::unique_lock lock(task_mutex);
                if (tasks.empty())
                    task_cv.wait_for(lock, token, idle_wait, [this] { return !tasks.empty(); });
            }

            process_tasks();
            driver->shutdown();
            initialized.store(false, std::memory_order_release);
        });
        return future.get();
    }

    void Runtime::Impl::DriverHost::process_tasks()
    {
        std::deque<std::function<void()>> local;
        {
            std::scoped_lock lock(task_mutex);
            local.swap(tasks);
        }
        for (auto& task : local)
            task();
    }

    void Runtime::Impl::DriverHost::tick_caller_thread()
    {
        if (threaded() || !initialized.load(std::memory_order_acquire))
            return;
        const EventSink sink{
            .user = owner,
            .emit = [](void* user, const WindowEvent& event) noexcept
            {
                static_cast<Impl*>(user)->on_event(event);
            }
        };
        process_tasks();
        driver->pump_events(sink);
        owner->run_driver_frames(*this);
    }

    void Runtime::Impl::DriverHost::stop() noexcept
    {
        if (stopping.exchange(true, std::memory_order_acq_rel))
            return;
        task_cv.notify_all();
        if (worker.joinable())
        {
            worker.request_stop();
            worker.join();
        }
        else if (initialized.exchange(false, std::memory_order_acq_rel))
        {
            driver->shutdown();
        }
    }

    Runtime::Impl::DriverHost::~DriverHost() noexcept
    {
        stop();
    }

    Runtime::Impl::Impl(RuntimeDesc in_desc) : desc(std::move(in_desc))
    {
        if (desc.auto_register_builtin_drivers)
            register_builtins();
    }

    Runtime::Impl::~Impl() noexcept
    {
        shutdown_all();
    }

    void Runtime::Impl::register_builtins()
    {
        factories.emplace(DriverKind::headless, &create_headless_driver);
#if defined(_WIN32)
        factories.emplace(DriverKind::win32, &create_win32_driver);
#endif
#if defined(__linux__) && defined(EPOCH_PLATFORM_HAS_X11)
        factories.emplace(DriverKind::x11, &create_x11_driver);
#endif
#if defined(__APPLE__)
        factories.emplace(DriverKind::cocoa, &create_cocoa_driver);
#endif
    }

    std::shared_ptr<ContextRecord> Runtime::Impl::find_context(const ContextId id) const
    {
        std::shared_lock lock(registry_mutex);
        const auto it = contexts.find(id);
        return it == contexts.end() ? nullptr : it->second;
    }

    Runtime::Impl::DriverHost* Runtime::Impl::find_host(const DriverKind kind) const
    {
        std::shared_lock lock(registry_mutex);
        const auto it = hosts.find(kind);
        return it == hosts.end() ? nullptr : it->second.get();
    }

    ContextSnapshot Runtime::Impl::snapshot(const ContextRecord& record) const
    {
        ContextSnapshot result{
            .id = record.id,
            .driver_id = record.driver_id,
            .driver_kind = record.driver_kind,
            .context_type = record.desc.type,
            .lifecycle = record.lifecycle.load(std::memory_order_acquire),
            .route = record.desc.route,
            .width = record.width.load(std::memory_order_acquire),
            .height = record.height.load(std::memory_order_acquire),
            .close_requested = record.close_requested.load(std::memory_order_acquire)
        };
        std::scoped_lock lock(record.state_mutex);
        result.native = record.native;
        return result;
    }

    Result<Runtime::Impl::DriverHost*> Runtime::Impl::ensure_host(
        DriverKind requested, const ThreadPolicy requested_policy)
    {
        if (requested == DriverKind::automatic)
            requested = default_driver_kind();
        if (auto* existing = find_host(requested))
            return existing;

        DriverFactory factory{};
        {
            std::shared_lock lock(registry_mutex);
            const auto it = factories.find(requested);
            if (it == factories.end())
                return std::unexpected(make_runtime_error(ErrorCode::driver_unavailable, "Platform driver is not registered"));
            factory = it->second;
        }

        auto driver = factory();
        if (!driver)
            return std::unexpected(make_runtime_error(ErrorCode::driver_unavailable, "Platform driver factory returned null"));

        auto host = std::make_unique<DriverHost>();
        host->owner = this;
        host->id = DriverId{ next_driver_id.fetch_add(1, std::memory_order_relaxed) };
        host->kind = requested;
        host->driver = std::move(driver);
        host->capabilities = host->driver->capabilities();
        host->idle_wait = desc.idle_wait;

        ThreadPolicy policy = requested_policy;
        if (policy == ThreadPolicy::automatic)
            policy = desc.dedicated_driver_threads ? ThreadPolicy::dedicated_driver_thread : ThreadPolicy::caller_thread;
        if (host->capabilities.requires_main_thread || !host->capabilities.supports_dedicated_thread)
            policy = ThreadPolicy::caller_thread;
        host->policy = policy;

        const DriverInit init{
            .id = host->id,
            .thread_policy = policy,
            .native_application_instance = desc.native_application_instance
        };
        auto status = host->start(init);
        if (!status)
        {
            if (requested != DriverKind::headless)
                return ensure_host(DriverKind::headless, requested_policy);
            return std::unexpected(status.error());
        }

        auto* result = host.get();
        {
            std::unique_lock lock(registry_mutex);
            const auto [it, inserted] = hosts.emplace(requested, std::move(host));
            if (!inserted)
                result = it->second.get();
        }
        return result;
    }

    void Runtime::Impl::on_event(const WindowEvent& event) noexcept
    {
        const auto record = find_context(event.context);
        if (!record)
            return;
        {
            std::scoped_lock lock(record->state_mutex);
            record->events.push_back(event);
        }
        if (event.type == WindowEventType::close_requested || event.type == WindowEventType::destroyed)
            record->close_requested.store(true, std::memory_order_release);
        if (event.type == WindowEventType::resized)
        {
            record->width.store(event.width, std::memory_order_release);
            record->height.store(event.height, std::memory_order_release);
        }
        if (record->callbacks.event)
            record->callbacks.event(event);
    }

    void Runtime::Impl::run_driver_frames(DriverHost& host)
    {
        std::vector<std::shared_ptr<ContextRecord>> records;
        {
            std::shared_lock lock(registry_mutex);
            for (const auto& [id, record] : contexts)
            {
                (void)id;
                if (record->driver_id == host.id)
                    records.emplace_back(record);
            }
        }

        const auto now = std::chrono::steady_clock::now();
        for (const auto& record : records)
        {
            if (record->lifecycle.load(std::memory_order_acquire) != LifecycleState::ready)
                continue;
            if (record->close_requested.load(std::memory_order_acquire))
            {
                finalize_context_on_host(host, record);
                continue;
            }
            const auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(now - record->last_frame);
            record->last_frame = now;
            if (record->callbacks.frame && !record->callbacks.frame(snapshot(*record), delta))
                finalize_context_on_host(host, record);
        }
    }

    void Runtime::Impl::finalize_context_on_host(
        DriverHost& host, const std::shared_ptr<ContextRecord>& record) noexcept
    {
        const auto prior = record->lifecycle.exchange(LifecycleState::stopping, std::memory_order_acq_rel);
        if (prior == LifecycleState::stopping || prior == LifecycleState::stopped)
            return;
        try
        {
            if (record->callbacks.shutdown)
                record->callbacks.shutdown(snapshot(*record));
        }
        catch (...)
        {
        }
        host.driver->destroy_window(record->id);
        record->lifecycle.store(LifecycleState::stopped, std::memory_order_release);
    }

    void Runtime::Impl::shutdown_all() noexcept
    {
        running.store(false, std::memory_order_release);
        std::vector<std::shared_ptr<ContextRecord>> records;
        {
            std::shared_lock lock(registry_mutex);
            for (const auto& [id, record] : contexts)
            {
                (void)id;
                records.emplace_back(record);
            }
        }
        for (const auto& record : records)
        {
            if (auto* host = find_host(record->driver_kind))
            {
                try
                {
                    host->execute_sync([this, host, record] { finalize_context_on_host(*host, record); });
                }
                catch (...)
                {
                }
            }
        }
        std::unique_lock lock(registry_mutex);
        contexts.clear();
        hosts.clear();
    }
}
