// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Native process owners lend observation access to their existing handles.
#pragma once

#if defined(_WIN32)
#include "platform.framework.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace epochengine::platform::process_observation
{
    enum class TargetKind : std::uint8_t { process, job };

    struct Target final
    {
        HANDLE handle{};
        TargetKind kind{};
        std::uint64_t generation{};
    };

    struct Registry final
    {
        SRWLOCK lock = SRWLOCK_INIT;
        std::array<Target, 32u> targets{};
        std::uint64_t next_generation{};
        std::size_t overflow{};
    };

    // Constant-initialized and trivially destructible so a retiring telemetry
    // worker can never race a static destructor. No handle belongs to this object.
    inline constinit Registry registry{};

    class Lease final
    {
        static constexpr std::size_t overflow_slot = registry.targets.size();
        static constexpr std::size_t empty_slot = overflow_slot + 1u;
        std::size_t slot_{empty_slot};

    public:
        Lease() noexcept = default;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        Lease(Lease&& other) noexcept : slot_{std::exchange(other.slot_, empty_slot)} {}
        Lease& operator=(Lease&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                slot_ = std::exchange(other.slot_, empty_slot);
            }
            return *this;
        }
        ~Lease() { reset(); }

        void attach(HANDLE handle, TargetKind kind) noexcept
        {
            reset();
            if (!handle || handle == INVALID_HANDLE_VALUE) return;
            ::AcquireSRWLockExclusive(&registry.lock);
            slot_ = 0u;
            while (slot_ < overflow_slot && registry.targets[slot_].handle) ++slot_;
            if (slot_ < overflow_slot)
                registry.targets[slot_] = {handle, kind, ++registry.next_generation};
            else
                ++registry.overflow;
            ::ReleaseSRWLockExclusive(&registry.lock);
        }

        // Call before closing the owner's handle. Borrowers may take a short
        // shared lock but never retain a Job handle past that lock.
        void reset() noexcept
        {
            if (slot_ == empty_slot) return;
            ::AcquireSRWLockExclusive(&registry.lock);
            if (slot_ < overflow_slot) registry.targets[slot_] = {};
            else --registry.overflow;
            slot_ = empty_slot;
            ::ReleaseSRWLockExclusive(&registry.lock);
        }
    };
}
#endif
