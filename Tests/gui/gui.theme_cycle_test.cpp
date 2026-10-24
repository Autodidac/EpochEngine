#include "../../Engine/src/epochgui/gui.theme_cycle.hpp"

#include <cassert>
#include <chrono>

int main()
{
    using epochengine::gui::detail::ThemeCycleClock;
    using namespace std::chrono_literals;
    const auto t0 = ThemeCycleClock::Clock::time_point{};
    ThemeCycleClock cycle{};

    // Initial interval is 30 seconds; frame frequency does not advance it.
    assert(cycle.slot(t0) == 0);
    assert(cycle.slot(t0 + 29999ms) == 0);
    assert(cycle.slot(t0 + 30s) == 1);
    assert(cycle.slot(t0 + 59s) == 1);
    assert(cycle.slot(t0 + 60s) == 2);
    assert(cycle.slot(t0 + 7min) == 14);

    // Selecting a new interval begins a fresh cycle on the next frame.
    cycle.set_interval(180u);
    assert(cycle.slot(t0 + 11min) == 0);
    assert(cycle.slot(t0 + 13min + 59s) == 0);
    assert(cycle.slot(t0 + 14min) == 1);
    cycle.set_interval(180u); // Unchanged setting must not reset time.
    assert(cycle.slot(t0 + 17min) == 2);

    // Leaving Cycle mode and re-entering restarts from its first theme.
    cycle.deactivate();
    assert(cycle.slot(t0 + 20min) == 0);
    assert(cycle.slot(t0 + 23min) == 1);
    return 0;
}
