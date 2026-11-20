#pragma once

#include <atomic>
#include <cmath>
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

    class FrameCeiling final {
    public:
        [[nodiscard]] double update(bool compiling, bool cpu_valid, double busy,
            std::uint64_t sampled_at_ms, std::uint64_t now_ms) noexcept
        {
            if (compiling) {
                recovery_started_ms_ = 0u;
                return 10.0;
            }

            constexpr std::uint64_t maximum_sample_age_ms = 5'000u;
            constexpr std::uint64_t recovery_ms = 2'000u;
            if (last_sample_ms_ > now_ms || now_ms - last_sample_ms_ > maximum_sample_age_ms) {
                last_sample_ms_ = 0u;
                recovery_started_ms_ = 0u;
                cpu_ceiling_ = 0.0;
            }
            if (!cpu_valid || !std::isfinite(busy) || busy < 0.0 || busy > 1.0
                || sampled_at_ms > now_ms || now_ms - sampled_at_ms > maximum_sample_age_ms) {
                recovery_started_ms_ = 0u;
                return cpu_ceiling_;
            }
            // The host cache publishes approximately once per second.
            // Re-reading one sample on later frames cannot release the ceiling.
            if (sampled_at_ms <= last_sample_ms_)
                return cpu_ceiling_;
            if (last_sample_ms_ != 0u && sampled_at_ms - last_sample_ms_ > recovery_ms)
                recovery_started_ms_ = 0u;
            last_sample_ms_ = sampled_at_ms;

            if (busy >= 0.90) {
                cpu_ceiling_ = 20.0;
                recovery_started_ms_ = 0u;
            } else if (cpu_ceiling_ == 0.0 && busy >= 0.75) {
                cpu_ceiling_ = 30.0;
            } else if (cpu_ceiling_ > 0.0) {
                const double recovery_threshold = cpu_ceiling_ == 20.0 ? 0.85 : 0.70;
                if (busy >= recovery_threshold) {
                    recovery_started_ms_ = 0u;
                } else if (recovery_started_ms_ == 0u) {
                    recovery_started_ms_ = sampled_at_ms;
                } else if (sampled_at_ms - recovery_started_ms_ >= recovery_ms) {
                    cpu_ceiling_ = busy >= 0.70 ? 30.0 : 0.0;
                    recovery_started_ms_ = 0u;
                }
            }
            return cpu_ceiling_;
        }

    private:
        std::uint64_t last_sample_ms_{};
        std::uint64_t recovery_started_ms_{};
        double cpu_ceiling_{};
    };
}
