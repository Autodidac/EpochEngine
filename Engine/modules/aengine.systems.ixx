module;


export module aengine.systems;


// ------------------------------------------------------------
// Engine headers (order-sensitive, header units)
// ------------------------------------------------------------
import aengine.platform;
import ampmcboundedqueue;   // Lock-free MPMCQueue<T>
// import "anet.hpp";            // for poll()

// ------------------------------------------------------------
// Standard library
// ------------------------------------------------------------
import <span>;
import <atomic>;
import <chrono>;
import <fstream>;
import <coroutine>;
import <filesystem>;
import <functional>;
import <semaphore>;
import <thread>;
import <vector>;
import <string>;
import <cstddef>;

// ============================================================
// Named module
// ============================================================

//export module aengine.core.scheduler;

// ============================================================
// Scheduler + coroutine utilities
// ============================================================

export namespace almondnamespace
{
    // ---------------------------------------------------------
    // Coroutine Task (public coroutine handle type)
    // ---------------------------------------------------------
    struct Task
    {
        struct promise_type
        {
            Task get_return_object()
            {
                return Task{
                    std::coroutine_handle<promise_type>::from_promise(*this)
                };
            }

            std::suspend_always initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() { std::terminate(); }
        };

        using handle_t = std::coroutine_handle<promise_type>;
        handle_t h;

        explicit Task(handle_t h_) noexcept : h(h_) {}
        Task(Task&& o) noexcept : h(o.h) { o.h = nullptr; }
        ~Task() { if (h) h.destroy(); }

        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;
    };

    // ---------------------------------------------------------
    // Internal job queue + worker pool
    // ---------------------------------------------------------
    export MPMCQueue<std::function<void()>> g_jobQueue{ 1024 };
    export std::vector<std::thread>         g_workers;
    export std::atomic<bool>                g_running{ false };

    // ---------------------------------------------------------
    export void scheduler_start(int threadCount)
    {
        g_running = true;

        for (int i = 0; i < threadCount; ++i) {
            g_workers.emplace_back([] {
                std::function<void()> job;

                while (g_running) {
                    if (g_jobQueue.dequeue(job)) {
                        job();
                    }
                    else {
                        std::this_thread::yield();
                    }
                }

                // Drain remaining jobs
                while (g_jobQueue.dequeue(job)) {
                    job();
                }
                });
        }
    }

    // ---------------------------------------------------------
    export void scheduler_stop()
    {
        g_running = false;

        for (auto& t : g_workers) {
            if (t.joinable()) {
                t.join();
            }
        }

        g_workers.clear();
    }

    // ---------------------------------------------------------
    export void scheduler_enqueue(std::function<void()> job)
    {
        while (!g_jobQueue.enqueue(std::move(job))) {
            std::this_thread::yield();
        }
    }

    // ---------------------------------------------------------
    // Awaitables (no heap, no classes, clean C++23)
    // ---------------------------------------------------------

    // LoadAssetAwaitable — runs blocking disk I/O on a worker
    struct LoadAssetAwaitable
    {
        std::string path;

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) const noexcept
        {
            scheduler_enqueue([h, path = path]() {
                std::ifstream in(path, std::ios::binary);
                std::vector<std::byte> buf;

                if (in) {
                    in.seekg(0, std::ios::end);
                    buf.resize(static_cast<std::size_t>(in.tellg()));
                    in.seekg(0);
                    in.read(reinterpret_cast<char*>(buf.data()), buf.size());
                }

                h.resume();
                });
        }

        std::vector<std::byte> await_resume() const noexcept
        {
            return {}; // result handled externally if needed
        }
    };

    // ---------------------------------------------------------
    // ReceiveNetworkAwaitable — polls network input then resumes
    struct ReceiveNetworkAwaitable
    {
        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) const noexcept
        {
            scheduler_enqueue([h]() {
                // net::poll(); // from anet.hpp
                h.resume();
                });
        }

        std::vector<std::byte> await_resume() const noexcept
        {
            return {};
        }
    };

    // ---------------------------------------------------------
    // NextFrame — requeues coroutine for next engine tick
    struct NextFrame
    {
        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) const noexcept
        {
            scheduler_enqueue([h]() { h.resume(); });
        }

        void await_resume() const noexcept {}
    };
}
