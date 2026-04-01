/************************************************
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•—  â–ˆâ–ˆâ•—   *
 *  â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ•”â•â•â•  â–ˆâ–ˆâ•”â•â•â•â• â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘     â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â•šâ•â•â•â•â•â•â•â•šâ•â•      â•šâ•â•â•â•â•â•  â•šâ•â•â•â•â•â•â•šâ•â•  â•šâ•â•   *
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

#include <array>
#include <chrono>
#include <cstdio>
#include <format>
#include <ctime>
#include <string>
#include <string_view>
#include <unordered_map>

export module core.timer;

namespace epochnamespace
{
    namespace timing
    {
        export using Clock = std::chrono::steady_clock;

        // ---------------------------------------------------------------------
        // TIMER
        // ---------------------------------------------------------------------

        export struct Timer
        {
            Clock::time_point start = Clock::now(); // last resume
            double accumulated = 0.0;               // unscaled seconds
            double timeScale = 1.0;
            bool   paused = false;
        };

        // ---------------------------------------------------------------------
        // CREATION
        // ---------------------------------------------------------------------

        export inline Timer createTimer(double scale = 1.0) noexcept
        {
            Timer t{};
            t.timeScale = scale;
            return t;
        }

        // ---------------------------------------------------------------------
        // INTERNAL: unscaled elapsed
        // ---------------------------------------------------------------------

        [[nodiscard]]
        inline double unscaledElapsed(const Timer& t) noexcept
        {
            if (t.paused)
                return t.accumulated;

            return t.accumulated +
                std::chrono::duration<double>(Clock::now() - t.start).count();
        }

        // ---------------------------------------------------------------------
        // QUERIES
        // ---------------------------------------------------------------------

        export inline double elapsed(const Timer& t) noexcept
        {
            return unscaledElapsed(t) * t.timeScale;
        }

        export inline double realElapsed(const Timer& t) noexcept
        {
            return unscaledElapsed(t);
        }

        // ---------------------------------------------------------------------
        // CONTROL
        // ---------------------------------------------------------------------

        export inline void pause(Timer& t) noexcept
        {
            if (!t.paused)
            {
                t.accumulated = unscaledElapsed(t);
                t.paused = true;
            }
        }

        export inline void resume(Timer& t) noexcept
        {
            if (t.paused)
            {
                t.start = Clock::now();
                t.paused = false;
            }
        }

        export inline void reset(Timer& t) noexcept
        {
            t.start = Clock::now();
            t.accumulated = 0.0;
            t.paused = false;
        }

        // Manual advance in REAL seconds (never scaled)
        export inline void advance(Timer& t, double seconds) noexcept
        {
            t.accumulated += seconds;
        }

        // Scaling never rewrites history
        export inline void setScale(Timer& t, double newScale) noexcept
        {
            t.accumulated = unscaledElapsed(t);
            t.start = Clock::now();
            t.timeScale = newScale;
        }

        // ---------------------------------------------------------------------
        // REGISTRY
        // ---------------------------------------------------------------------

        export struct ScopedTimers
        {
            std::unordered_map<std::string, Timer> timers;
        };

        export inline std::unordered_map<std::string, ScopedTimers>&
            timerRegistry()
        {
            static std::unordered_map<std::string, ScopedTimers> registry;
            return registry;
        }

        export inline Timer&
            createNamedTimer(std::string_view group,
                std::string_view name,
                double scale = 1.0)
        {
            return timerRegistry()[std::string(group)]
                .timers[std::string(name)] = createTimer(scale);
        }

        export inline Timer*
            getTimer(std::string_view group, std::string_view name)
        {
            auto g = timerRegistry().find(std::string(group));
            if (g == timerRegistry().end())
                return nullptr;

            auto t = g->second.timers.find(std::string(name));
            return (t != g->second.timers.end()) ? &t->second : nullptr;
        }

        export inline void
            removeTimer(std::string_view group, std::string_view name)
        {
            auto g = timerRegistry().find(std::string(group));
            if (g == timerRegistry().end())
                return;

            g->second.timers.erase(std::string(name));
            if (g->second.timers.empty())
                timerRegistry().erase(g);
        }

        // ---------------------------------------------------------------------
        // SYSTEM TIME STRING
        // ---------------------------------------------------------------------

        export inline std::string getCurrentTimeString()
        {
            const auto now = std::chrono::system_clock::now();
            const std::time_t time_value = std::chrono::system_clock::to_time_t(now);

            std::tm local_tm{};
#if defined(_WIN32)
            localtime_s(&local_tm, &time_value);
#else
            localtime_r(&time_value, &local_tm);
#endif

            std::array<char, 32> buffer{};
            std::snprintf(
                buffer.data(),
                buffer.size(),
                "%04d-%02d-%02d %02d:%02d:%02d",
                local_tm.tm_year + 1900,
                local_tm.tm_mon + 1,
                local_tm.tm_mday,
                local_tm.tm_hour,
                local_tm.tm_min,
                local_tm.tm_sec);

            return std::string{ buffer.data() };
        }
    }
} // namespace epochnamespace::timing
