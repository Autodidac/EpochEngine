// Epoch system notes (auto-checked):
// EpochGUI: use consistent draw/hit-test geometry and preserve child input ownership.
#pragma once

#include <chrono>
#include <cstdint>

namespace epochengine::gui::detail
{
    // Frame-rate-independent scheduler used by the editor's optional theme cycle.
    // The GUI clock is monotonic; drawing never waits or polls in a blocking loop.
    class ThemeCycleClock final
    {
    public:
        using Clock = std::chrono::steady_clock;

        void set_interval(std::uint32_t seconds) noexcept
        {
            if (intervalSeconds_ != seconds)
            {
                intervalSeconds_ = seconds;
                active_ = false;
            }
        }

        void deactivate() noexcept { active_ = false; }

        [[nodiscard]] std::uint64_t slot(Clock::time_point now) noexcept
        {
            if (!active_)
            {
                start_ = now;
                active_ = true;
            }
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - start_).count();
            return static_cast<std::uint64_t>(elapsed > 0 ? elapsed : 0)
                / (intervalSeconds_ == 0u ? 30u : intervalSeconds_);
        }

    private:
        Clock::time_point start_{};
        std::uint32_t intervalSeconds_{30u};
        bool active_{};
    };
}
