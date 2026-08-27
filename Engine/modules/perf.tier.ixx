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

#include "../include/core.stl_types.hpp"
#include <string_view>

export module perf.tier;

import core.env;
import core.time;

export namespace epochengine::perf
{
    enum class tier : std::uint8_t
    {
        uncapped,
        mobile_30,
        deck_40,
        desktop_60,
        editor_120
    };

    enum class frame_limit_preset : std::uint8_t
    {
        fps_60,
        fps_120,
        unlimited
    };

    [[nodiscard]] tier tier_from_env() noexcept;
    [[nodiscard]] double target_fps_for(tier t) noexcept;
    [[nodiscard]] double target_fps_for(frame_limit_preset preset) noexcept;
    [[nodiscard]] const char* to_string(tier t) noexcept;
    [[nodiscard]] const char* to_string(frame_limit_preset preset) noexcept;
    [[nodiscard]] const char* label_for_frame_limit(double fps) noexcept;

    [[nodiscard]] constexpr double next_frame_deadline(
        double previous_deadline,
        double now,
        double target_dt,
        bool started) noexcept
    {
        if (!(target_dt > 0.0))
            return now;
        if (!started || previous_deadline < now - target_dt)
            previous_deadline = now;
        return previous_deadline + target_dt;
    }

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

namespace epochengine::perf
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
        if (auto v = epochengine::core::env::get(epochengine::string_view{ "EPOCH_TIER" }))
        {
            const std::string_view s = epochengine::to_std(*v);

            if (sv_eq(s, "mobile") || sv_eq(s, "30")) return tier::mobile_30;
            if (sv_eq(s, "deck") || sv_eq(s, "40")) return tier::deck_40;
            if (sv_eq(s, "desktop") || sv_eq(s, "60")) return tier::desktop_60;
            if (sv_eq(s, "editor") || sv_eq(s, "120")) return tier::editor_120;
            if (sv_eq(s, "uncapped") || sv_eq(s, "0"))  return tier::uncapped;
        }
        return tier::editor_120;
    }

    double target_fps_for(tier t) noexcept
    {
        switch (t)
        {
        case tier::mobile_30:  return 30.0;
        case tier::deck_40:    return 40.0;
        case tier::desktop_60: return 60.0;
        case tier::editor_120: return 120.0;
        case tier::uncapped:   return 0.0;
        }
        return 120.0;
    }

    double target_fps_for(frame_limit_preset preset) noexcept
    {
        switch (preset)
        {
        case frame_limit_preset::fps_60:    return 60.0;
        case frame_limit_preset::fps_120:   return 120.0;
        case frame_limit_preset::unlimited: return 0.0;
        }
        return 120.0;
    }

    const char* to_string(tier t) noexcept
    {
        switch (t)
        {
        case tier::mobile_30:  return "mobile_30";
        case tier::deck_40:    return "deck_40";
        case tier::desktop_60: return "desktop_60";
        case tier::editor_120: return "editor_120";
        case tier::uncapped:   return "uncapped";
        }
        return "editor_120";
    }

    const char* to_string(frame_limit_preset preset) noexcept
    {
        switch (preset)
        {
        case frame_limit_preset::fps_60:    return "60";
        case frame_limit_preset::fps_120:   return "120";
        case frame_limit_preset::unlimited: return "unlimited";
        }
        return "120";
    }

    const char* label_for_frame_limit(double fps) noexcept
    {
        if (fps <= 0.0)
            return "Unlimited";
        if (fps >= 119.5 && fps <= 120.5)
            return "120 FPS";
        if (fps >= 59.5 && fps <= 60.5)
            return "60 FPS";
        return "Custom";
    }

    void frame_limiter::wait_for_next_frame() noexcept
    {
        if (target_dt <= 0.0)
            return;

        const double now0 = epochengine::core::time::now_seconds();

        next_time = next_frame_deadline(
            next_time, now0, target_dt, started);
        started = true;

        for (;;)
        {
            const double now = epochengine::core::time::now_seconds();
            const double remaining = next_time - now;
            if (remaining <= 0.0)
                break;

            const double ms = remaining * 1000.0;
            if (ms > 2.0)
                epochengine::core::time::sleep_ms(static_cast<std::uint32_t>(ms - 1.0));
        }
    }
}
