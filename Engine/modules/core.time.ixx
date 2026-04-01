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

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

export module core.time;

export namespace epoch::core::time
{
    using steady = std::chrono::steady_clock;

    // Monotonic timestamp in nanoseconds since an arbitrary epoch.
    std::uint64_t now_ns() noexcept;

    // Monotonic timestamp in nanoseconds since an arbitrary epoch.
    double now_seconds() noexcept;

    std::string system_time_string();

    void sleep_ms(std::uint32_t ms);

    struct timer
    {
        steady::time_point start = steady::now();
        double accumulated_seconds = 0.0;
        double scale = 1.0;
        bool paused = false;
    };

    [[nodiscard]] inline timer make_timer(double initial_scale = 1.0) noexcept
    {
        timer t{};
        t.scale = initial_scale;
        return t;
    }

    [[nodiscard]] inline double unscaled_elapsed(const timer& t) noexcept
    {
        if (t.paused)
            return t.accumulated_seconds;

        return t.accumulated_seconds
            + std::chrono::duration_cast<std::chrono::duration<double>>(steady::now() - t.start).count();
    }

    [[nodiscard]] inline double elapsed(const timer& t) noexcept
    {
        return unscaled_elapsed(t) * t.scale;
    }

    [[nodiscard]] inline double real_elapsed(const timer& t) noexcept
    {
        return unscaled_elapsed(t);
    }

    inline void pause(timer& t) noexcept
    {
        if (!t.paused)
        {
            t.accumulated_seconds = unscaled_elapsed(t);
            t.paused = true;
        }
    }

    inline void resume(timer& t) noexcept
    {
        if (t.paused)
        {
            t.start = steady::now();
            t.paused = false;
        }
    }

    inline void reset(timer& t) noexcept
    {
        t.start = steady::now();
        t.accumulated_seconds = 0.0;
        t.paused = false;
    }

    inline void advance(timer& t, double seconds) noexcept
    {
        t.accumulated_seconds += seconds;
    }

    inline void set_scale(timer& t, double new_scale) noexcept
    {
        t.accumulated_seconds = unscaled_elapsed(t);
        t.start = steady::now();
        t.scale = new_scale;
    }

    struct scoped_timers
    {
        std::unordered_map<std::string, timer> timers;
    };

    [[nodiscard]] inline std::unordered_map<std::string, scoped_timers>& registry()
    {
        static std::unordered_map<std::string, scoped_timers> entries;
        return entries;
    }

    inline timer& create_named_timer(
        std::string_view group,
        std::string_view name,
        double initial_scale = 1.0)
    {
        return registry()[std::string(group)]
            .timers[std::string(name)] = make_timer(initial_scale);
    }

    [[nodiscard]] inline timer* get_timer(
        std::string_view group,
        std::string_view name)
    {
        auto group_it = registry().find(std::string(group));
        if (group_it == registry().end())
            return nullptr;

        auto timer_it = group_it->second.timers.find(std::string(name));
        return timer_it == group_it->second.timers.end() ? nullptr : &timer_it->second;
    }

    inline void remove_timer(std::string_view group, std::string_view name)
    {
        auto group_it = registry().find(std::string(group));
        if (group_it == registry().end())
            return;

        group_it->second.timers.erase(std::string(name));
        if (group_it->second.timers.empty())
            registry().erase(group_it);
    }

    struct frame_clock
    {
        std::uint64_t frame_index = 0;
        std::uint64_t last_ns = 0;
        std::uint64_t dt_ns = 0;

        void start() noexcept;
        void tick() noexcept;

        double dt_seconds() const noexcept
        {
            return static_cast<double>(dt_ns) * 1e-9;
        }
    };
}
