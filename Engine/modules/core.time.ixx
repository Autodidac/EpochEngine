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

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

export module core.time;

export namespace epochengine::core::time
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

    struct simulation_stats
    {
        std::uint64_t frame_index = 0;
        std::uint64_t simulated_steps = 0;
        std::uint32_t step_budget = 0;
        double real_dt_seconds = 0.0;
        double scaled_dt_seconds = 0.0;
        double fixed_dt_seconds = 1.0 / 60.0;
        double accumulator_seconds = 0.0;
        double simulated_seconds = 0.0;
        double time_scale = 1.0;
        bool paused = false;
        std::uint32_t max_steps_per_frame = 8;
    };

    struct simulation_clock
    {
        frame_clock frame{};
        double fixed_dt_seconds = 1.0 / 60.0;
        double accumulator_seconds = 0.0;
        double simulated_seconds = 0.0;
        double real_dt_seconds = 0.0;
        double scaled_dt_seconds = 0.0;
        double time_scale = 1.0;
        std::uint64_t simulated_steps = 0;
        std::uint32_t max_steps_per_frame = 8;
        bool paused = false;
        bool step_once_requested = false;

        void start() noexcept;
        void tick(double real_dt_override = -1.0) noexcept;

        [[nodiscard]] std::uint32_t step_budget() const noexcept;
        void consume_steps(std::uint32_t count) noexcept;

        void set_paused(bool value) noexcept { paused = value; }
        void toggle_pause() noexcept { paused = !paused; }
        void request_single_step() noexcept { step_once_requested = true; }

        void set_time_scale(double value) noexcept
        {
            time_scale = (std::clamp)(value, 0.0, 8.0);
        }

        void set_max_steps_per_frame(std::uint32_t value) noexcept
        {
            max_steps_per_frame = (std::clamp)(value, 1u, 16u);
        }

        void set_fixed_dt_seconds(double value) noexcept
        {
            fixed_dt_seconds = (std::clamp)(value, 1.0 / 240.0, 1.0 / 15.0);
        }

        [[nodiscard]] simulation_stats stats() const noexcept
        {
            return simulation_stats{
                .frame_index = frame.frame_index,
                .simulated_steps = simulated_steps,
                .step_budget = step_budget(),
                .real_dt_seconds = real_dt_seconds,
                .scaled_dt_seconds = scaled_dt_seconds,
                .fixed_dt_seconds = fixed_dt_seconds,
                .accumulator_seconds = accumulator_seconds,
                .simulated_seconds = simulated_seconds,
                .time_scale = time_scale,
                .paused = paused,
                .max_steps_per_frame = max_steps_per_frame
            };
        }
    };
}
