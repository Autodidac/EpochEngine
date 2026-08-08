/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include <vector>
#include <atomic>
#include <optional>
#include <stdexcept>
#include <coroutine>
#include <memory>
#include <iostream>
#include <source_location>
#include <algorithm>
#include <cstdint>

export module sprite.pool;

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Standard library
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Engine modules
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

import sprite.handle;          // SpriteHandle
import systems.task;         // Task
import core.mpmc_bounded_queue;
import taskgraph.dotsystem;      // taskgraph::TaskGraph, Node
import core.logger;

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

namespace epochengine::spritepool
{
    using epochengine::SpriteHandle;
    using epochengine::Task;
    using epochengine::taskgraph::Node;

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Pool state (module-local singletons)
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    export inline std::vector<std::uint8_t>  usedFlags;     // 0 = free, 1 = used
    export inline std::vector<std::uint32_t> generations;  // generation per slot
    export inline std::atomic<std::size_t>   lastAllocIndex{ 0 };
    export inline std::size_t                capacity = 0;

    // Optional async task graph
    inline taskgraph::TaskGraph* g_taskGraph = nullptr;

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Lifecycle
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    export inline void initialize(std::size_t cap) noexcept
    {
        capacity = cap;
        usedFlags.assign(capacity, 0);
        generations.assign(capacity, 0);
        lastAllocIndex.store(0, std::memory_order_relaxed);

#if defined(DEBUG_TEXTURE_RENDERING_VERBOSE)
        logger::infof_loc("SpritePool", std::source_location::current(), "Initialized with capacity {}", capacity);
#endif
    }

    export inline void clear() noexcept
    {
        usedFlags.clear();
        generations.clear();
        lastAllocIndex.store(0, std::memory_order_relaxed);
        capacity = 0;
        logger::info("SpritePool", "Cleared");
    }

    export inline void reset() noexcept
    {
        if (capacity == 0)
        {
            logger::warn("SpritePool", "Reset on uninitialized pool.");
            return;
        }

        std::fill(usedFlags.begin(), usedFlags.end(), 0);
        std::fill(generations.begin(), generations.end(), 0);
        lastAllocIndex.store(0, std::memory_order_relaxed);

#if defined(DEBUG_TEXTURE_RENDERING_VERBOSE)
        logger::info("SpritePool", "Reset");
#endif
    }

    export inline void set_task_graph(taskgraph::TaskGraph* graph) noexcept
    {
        g_taskGraph = graph;
        logger::info("SpritePool", "Task graph set.");
    }

    export inline void validate_pool() noexcept
    {
        std::size_t freeCount = 0;
        for (std::size_t i = 0; i < capacity; ++i)
            if (usedFlags[i] == 0) ++freeCount;

        logger::infof_loc("SpritePool", std::source_location::current(), "{} free slots out of {}", freeCount, capacity);
    }

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Allocation internals
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

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

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Synchronous allocate
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    export inline SpriteHandle allocate() noexcept
    {
        auto idx = try_allocate_round_robin();
        if (!idx)
            return SpriteHandle::invalid();

        const std::uint32_t id =
            static_cast<std::uint32_t>(*idx);

        return SpriteHandle{ id, generations[id] };
    }

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Async allocate
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

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

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Free / lifetime checks
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

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
