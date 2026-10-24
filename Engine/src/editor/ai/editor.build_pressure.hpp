#pragma once

#include <atomic>
#include <cstdint>

namespace epochengine::editor::build_pressure {
    // Tracks this host process's active supervised AI candidate compiler calls.
    // Never grants an agent permissions or changes build/validation outcome.
    inline std::atomic<std::uint32_t> active_compilers{0};

    class ScopedCompiler final {
    public:
        ScopedCompiler() noexcept {
            active_compilers.fetch_add(1u, std::memory_order_acq_rel);
        }
        ~ScopedCompiler() noexcept {
            active_compilers.fetch_sub(1u, std::memory_order_acq_rel);
        }
        ScopedCompiler(const ScopedCompiler&) = delete;
        ScopedCompiler& operator=(const ScopedCompiler&) = delete;
    };

    [[nodiscard]] inline bool active() noexcept {
        return active_compilers.load(std::memory_order_acquire) != 0u;
    }
}
