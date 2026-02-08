// ============================================================================
// src/core.time.cpp
// ============================================================================

module;

#include <chrono>
#include <cstdint>
#include <thread>

module core.time;

namespace epoch::core::time
{
    std::uint64_t now_ns() noexcept
    {
        const auto t = steady::now().time_since_epoch();
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t).count());
    }

    double now_seconds() noexcept
    {
        // IMPORTANT: keep it as floating-point seconds (no integer truncation).
        const auto t = steady::now().time_since_epoch();
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t).count();
        return static_cast<double>(ns) * 1e-9;
    }

    void sleep_ms(std::uint32_t ms)
    {
        // std::this_thread::sleep_for is portable; resolution depends on OS timer.
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    void frame_clock::start() noexcept
    {
        frame_index = 0;
        last_ns = now_ns();
        dt_ns = 0;
    }

    void frame_clock::tick() noexcept
    {
        const std::uint64_t t = now_ns();
        dt_ns = t - last_ns;
        last_ns = t;
        ++frame_index;
    }
}
