module;

export module asprite.pool;

// ────────────────────────────────────────────────────────────
// Standard library
// ────────────────────────────────────────────────────────────

import <vector>;
import <atomic>;
import <optional>;
import <stdexcept>;
import <coroutine>;
import <memory>;
import <iostream>;
import <algorithm>;
import <cstdint>;

// ────────────────────────────────────────────────────────────
// Engine modules
// ────────────────────────────────────────────────────────────

import aspritehandle;          // SpriteHandle
import aengine.systems;         // Task
import ampmcboundedqueue;
import aengine.taskgraph.dotsystem;      // taskgraph::TaskGraph, Node

// ────────────────────────────────────────────────────────────

export namespace almondnamespace::spritepool
{
    using almondnamespace::SpriteHandle;
    using almondnamespace::Task;
    using almondnamespace::taskgraph::Node;

    // ────────────────────────────────────────────────────────
    // Pool state (module-local singletons)
    // ────────────────────────────────────────────────────────

    inline std::vector<std::uint8_t>  usedFlags;     // 0 = free, 1 = used
    inline std::vector<std::uint32_t> generations;  // generation per slot
    inline std::atomic<std::size_t>   lastAllocIndex{ 0 };
    inline std::size_t                capacity = 0;

    // Optional async task graph
    inline taskgraph::TaskGraph* g_taskGraph = nullptr;

    // ────────────────────────────────────────────────────────
    // Lifecycle
    // ────────────────────────────────────────────────────────

    export inline void initialize(std::size_t cap) noexcept
    {
        capacity = cap;
        usedFlags.assign(capacity, 0);
        generations.assign(capacity, 0);
        lastAllocIndex.store(0, std::memory_order_relaxed);

#if defined(DEBUG_TEXTURE_RENDERING_VERBOSE)
        std::cerr << "[SpritePool] Initialized with capacity " << capacity << "\n";
#endif
    }

    export inline void clear() noexcept
    {
        usedFlags.clear();
        generations.clear();
        lastAllocIndex.store(0, std::memory_order_relaxed);
        capacity = 0;
        std::cerr << "[SpritePool] Cleared\n";
    }

    export inline void reset() noexcept
    {
        if (capacity == 0)
        {
            std::cerr << "[SpritePool] Warning: reset on uninitialized pool\n";
            return;
        }

        std::fill(usedFlags.begin(), usedFlags.end(), 0);
        std::fill(generations.begin(), generations.end(), 0);
        lastAllocIndex.store(0, std::memory_order_relaxed);

#if defined(DEBUG_TEXTURE_RENDERING_VERBOSE)
        std::cerr << "[SpritePool] Reset\n";
#endif
    }

    export inline void set_task_graph(taskgraph::TaskGraph* graph) noexcept
    {
        g_taskGraph = graph;
        std::cerr << "[SpritePool] Task graph set\n";
    }

    export inline void validate_pool() noexcept
    {
        std::size_t freeCount = 0;
        for (std::size_t i = 0; i < capacity; ++i)
            if (usedFlags[i] == 0) ++freeCount;

        std::cerr << "[SpritePool] " << freeCount
            << " free slots out of " << capacity << "\n";
    }

    // ────────────────────────────────────────────────────────
    // Allocation internals
    // ────────────────────────────────────────────────────────

    inline bool try_acquire_flag(std::size_t idx) noexcept
    {
        std::atomic_ref<std::uint8_t> flag(usedFlags[idx]);
        std::uint8_t expected = 0;
        return flag.compare_exchange_strong(
            expected, 1, std::memory_order_acquire);
    }

    inline std::optional<std::size_t>
        try_allocate_round_robin() noexcept
    {
        const std::size_t size = capacity;
        const std::size_t start =
            lastAllocIndex.load(std::memory_order_relaxed);

        for (std::size_t offset = 0; offset < size; ++offset)
        {
            const std::size_t idx = (start + offset) % size;
            if (try_acquire_flag(idx))
            {
                lastAllocIndex.store(
                    (idx + 1) % size, std::memory_order_relaxed);
                return idx;
            }
        }

        return std::nullopt;
    }

    // ────────────────────────────────────────────────────────
    // Synchronous allocate
    // ────────────────────────────────────────────────────────

    export inline SpriteHandle allocate() noexcept
    {
        auto idx = try_allocate_round_robin();
        if (!idx)
            return SpriteHandle::invalid();

        const std::uint32_t id =
            static_cast<std::uint32_t>(*idx);

        return SpriteHandle{ id, generations[id] };
    }

    // ────────────────────────────────────────────────────────
    // Async allocate
    // ────────────────────────────────────────────────────────

    struct AllocateAwaitable;

    inline Task spritepool_allocation_coroutine(
        AllocateAwaitable* self);

    struct AllocateAwaitable
    {
        std::optional<std::size_t> index;
        std::coroutine_handle<>    awaiting{};

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) noexcept
        {
            awaiting = h;

            if (!g_taskGraph)
            {
                index = try_allocate_round_robin();
                if (awaiting) awaiting.resume();
                return;
            }

            Task task = spritepool_allocation_coroutine(this);
            auto node = std::make_unique<Node>(std::move(task));
            node->Label = "SpritePoolAllocate";

            g_taskGraph->AddNode(std::move(node));
            g_taskGraph->Execute();
        }

        SpriteHandle await_resume() noexcept
        {
            if (!index)
                return SpriteHandle::invalid();

            const std::uint32_t id =
                static_cast<std::uint32_t>(*index);

            if (id >= generations.size())
                return SpriteHandle::invalid();

            return SpriteHandle{ id, generations[id] };
        }
    };

    inline Task spritepool_allocation_coroutine(
        AllocateAwaitable* self)
    {
        self->index = try_allocate_round_robin();
        if (self->awaiting) self->awaiting.resume();
        co_return;
    }

    export inline AllocateAwaitable allocateAsync() noexcept
    {
        return {};
    }

    // ────────────────────────────────────────────────────────
    // Free / lifetime checks
    // ────────────────────────────────────────────────────────

    export inline void free(const SpriteHandle& handle) noexcept
    {
        const std::size_t idx =
            static_cast<std::size_t>(handle.id);

        if (idx >= capacity) return;

        std::atomic_ref<std::uint8_t> flag(usedFlags[idx]);
        flag.store(0, std::memory_order_release);
        ++generations[idx];
    }

    export inline bool is_alive(
        const SpriteHandle& handle) noexcept
    {
        const std::size_t idx =
            static_cast<std::size_t>(handle.id);

        if (idx >= capacity) return false;

        std::atomic_ref<std::uint8_t> flag(usedFlags[idx]);
        if (flag.load(std::memory_order_acquire) == 0)
            return false;

        return generations[idx] == handle.generation;
    }
}
