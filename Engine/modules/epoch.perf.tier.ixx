module;

#include "../include/_epoch.stl_types.hpp"
#include <string_view>

export module epoch.perf.tier;

import core.env;
import core.time;

export namespace epoch::perf
{
    enum class tier : std::uint8_t
    {
        uncapped,
        mobile_30,
        deck_40,
        desktop_60
    };

    [[nodiscard]] tier tier_from_env() noexcept;
    [[nodiscard]] double target_fps_for(tier t) noexcept;
    [[nodiscard]] const char* to_string(tier t) noexcept;

    struct frame_limiter
    {
        double target_dt = 0.0;
        double next_time = 0.0;
        bool started = false;

        void set_target_fps(double fps) noexcept
        {
            target_dt = (fps > 0.0) ? (1.0 / fps) : 0.0;
            started = false;
            next_time = 0.0;
        }

        void reset() noexcept
        {
            started = false;
            next_time = 0.0;
        }

        void wait_for_next_frame() noexcept;
    };
}

namespace epoch::perf
{
    namespace
    {
        [[nodiscard]] inline bool sv_eq(std::string_view a, const char* b) noexcept
        {
            // compare() avoids operator== ambiguity caused by your epoch overloads.
            return a.compare(b) == 0;
        }
    }

    tier tier_from_env() noexcept
    {
        if (auto v = epoch::core::env::get(epoch::string_view{ "EPOCH_TIER" }))
        {
            const std::string_view s = epoch::to_std(*v);

            if (sv_eq(s, "mobile") || sv_eq(s, "30")) return tier::mobile_30;
            if (sv_eq(s, "deck") || sv_eq(s, "40")) return tier::deck_40;
            if (sv_eq(s, "desktop") || sv_eq(s, "60")) return tier::desktop_60;
            if (sv_eq(s, "uncapped") || sv_eq(s, "0"))  return tier::uncapped;
        }
        return tier::desktop_60;
    }

    double target_fps_for(tier t) noexcept
    {
        switch (t)
        {
        case tier::mobile_30:  return 30.0;
        case tier::deck_40:    return 40.0;
        case tier::desktop_60: return 60.0;
        case tier::uncapped:   return 0.0;
        }
        return 60.0;
    }

    const char* to_string(tier t) noexcept
    {
        switch (t)
        {
        case tier::mobile_30:  return "mobile_30";
        case tier::deck_40:    return "deck_40";
        case tier::desktop_60: return "desktop_60";
        case tier::uncapped:   return "uncapped";
        }
        return "desktop_60";
    }

    void frame_limiter::wait_for_next_frame() noexcept
    {
        if (target_dt <= 0.0)
            return;

        const double now0 = epoch::core::time::now_seconds();

        if (!started)
        {
            started = true;
            next_time = now0;
        }

        next_time += target_dt;

        for (;;)
        {
            const double now = epoch::core::time::now_seconds();
            const double remaining = next_time - now;
            if (remaining <= 0.0)
                break;

            const double ms = remaining * 1000.0;
            if (ms > 2.0)
                epoch::core::time::sleep_ms(static_cast<std::uint32_t>(ms - 1.0));
        }
    }
}
