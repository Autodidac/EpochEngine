// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <epochplatformengine/runtime.hpp>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace epochengine::platform
{
    struct ContextRecord final
    {
        ContextId id{};
        DriverId driver_id{};
        DriverKind driver_kind{ DriverKind::headless };
        ContextDesc desc{};
        ContextCallbacks callbacks{};
        std::atomic<LifecycleState> lifecycle{ LifecycleState::pending };
        std::atomic_bool close_requested{};
        std::atomic<std::int32_t> width{};
        std::atomic<std::int32_t> height{};
        mutable std::mutex state_mutex{};
        NativeWindowHandle native{};
        std::deque<WindowEvent> events{};
        std::chrono::steady_clock::time_point last_frame{ std::chrono::steady_clock::now() };
    };

    [[nodiscard]] Error make_runtime_error(ErrorCode code, std::string message);

    struct Runtime::Impl final
    {
        struct DriverHost final
        {
            Impl* owner{};
            DriverId id{};
            DriverKind kind{ DriverKind::headless };
            ThreadPolicy policy{ ThreadPolicy::caller_thread };
            std::unique_ptr<IPlatformDriver> driver{};
            DriverCapabilities capabilities{};
            std::chrono::milliseconds idle_wait{ 1 };
            std::mutex task_mutex{};
            std::condition_variable_any task_cv{};
            std::deque<std::function<void()>> tasks{};
            std::jthread worker{};
            std::thread::id owner_thread{};
            std::atomic_bool initialized{};
            std::atomic_bool stopping{};

            [[nodiscard]] bool threaded() const noexcept;
            [[nodiscard]] Status start(const DriverInit& init);
            void process_tasks();
            void tick_caller_thread();
            void stop() noexcept;
            ~DriverHost() noexcept;

            template <typename Function>
            auto execute_sync(Function&& function) -> std::invoke_result_t<Function>
            {
                using Return = std::invoke_result_t<Function>;
                if (!threaded() || std::this_thread::get_id() == owner_thread)
                    return std::invoke(std::forward<Function>(function));

                auto callable = std::make_shared<std::decay_t<Function>>(std::forward<Function>(function));
                auto promise = std::make_shared<std::promise<Return>>();
                auto future = promise->get_future();
                {
                    std::scoped_lock lock(task_mutex);
                    tasks.emplace_back([callable, promise]() mutable
                    {
                        try
                        {
                            if constexpr (std::is_void_v<Return>)
                            {
                                std::invoke(*callable);
                                promise->set_value();
                            }
                            else
                            {
                                promise->set_value(std::invoke(*callable));
                            }
                        }
                        catch (...)
                        {
                            promise->set_exception(std::current_exception());
                        }
                    });
                }
                task_cv.notify_all();
                return future.get();
            }

            template <typename Function>
            void execute_async(Function&& function)
            {
                auto callable = std::make_shared<std::decay_t<Function>>(std::forward<Function>(function));
                {
                    std::scoped_lock lock(task_mutex);
                    tasks.emplace_back([callable]() mutable { std::invoke(*callable); });
                }
                task_cv.notify_all();
            }
        };

        explicit Impl(RuntimeDesc desc);
        ~Impl() noexcept;

        RuntimeDesc desc{};
        std::atomic_bool running{ true };
        std::atomic_uint64_t next_driver_id{ 1 };
        std::atomic_uint64_t next_context_id{ 1 };
        mutable std::shared_mutex registry_mutex{};
        std::unordered_map<DriverKind, DriverFactory> factories{};
        std::unordered_map<DriverKind, std::unique_ptr<DriverHost>> hosts{};
        std::unordered_map<ContextId, std::shared_ptr<ContextRecord>> contexts{};

        void register_builtins();
        [[nodiscard]] std::shared_ptr<ContextRecord> find_context(ContextId id) const;
        [[nodiscard]] DriverHost* find_host(DriverKind kind) const;
        [[nodiscard]] ContextSnapshot snapshot(const ContextRecord& record) const;
        [[nodiscard]] Result<DriverHost*> ensure_host(DriverKind requested, ThreadPolicy requested_policy);
        void on_event(const WindowEvent& event) noexcept;
        void run_driver_frames(DriverHost& host);
        void finalize_context_on_host(DriverHost& host, const std::shared_ptr<ContextRecord>& record) noexcept;
        void shutdown_all() noexcept;
    };
}
