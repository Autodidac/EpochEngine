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

#include <string>
#include <string_view>
#include <unordered_map>

export module core.timer;

import core.time;

export namespace epochnamespace::timing
{
    using Clock = epoch::core::time::steady;
    using Timer = epoch::core::time::timer;
    using ScopedTimers = epoch::core::time::scoped_timers;

    inline Timer createTimer(double scale = 1.0) noexcept
    {
        return epoch::core::time::make_timer(scale);
    }

    [[nodiscard]] inline double unscaledElapsed(const Timer& t) noexcept
    {
        return epoch::core::time::unscaled_elapsed(t);
    }

    [[nodiscard]] inline double elapsed(const Timer& t) noexcept
    {
        return epoch::core::time::elapsed(t);
    }

    [[nodiscard]] inline double realElapsed(const Timer& t) noexcept
    {
        return epoch::core::time::real_elapsed(t);
    }

    inline void pause(Timer& t) noexcept
    {
        epoch::core::time::pause(t);
    }

    inline void resume(Timer& t) noexcept
    {
        epoch::core::time::resume(t);
    }

    inline void reset(Timer& t) noexcept
    {
        epoch::core::time::reset(t);
    }

    inline void advance(Timer& t, double seconds) noexcept
    {
        epoch::core::time::advance(t, seconds);
    }

    inline void setScale(Timer& t, double newScale) noexcept
    {
        epoch::core::time::set_scale(t, newScale);
    }

    inline std::unordered_map<std::string, ScopedTimers>& timerRegistry()
    {
        return epoch::core::time::registry();
    }

    inline Timer& createNamedTimer(
        std::string_view group,
        std::string_view name,
        double scale = 1.0)
    {
        return epoch::core::time::create_named_timer(group, name, scale);
    }

    [[nodiscard]] inline Timer* getTimer(
        std::string_view group,
        std::string_view name)
    {
        return epoch::core::time::get_timer(group, name);
    }

    inline void removeTimer(std::string_view group, std::string_view name)
    {
        epoch::core::time::remove_timer(group, name);
    }

    [[nodiscard]] inline std::string getCurrentTimeString()
    {
        return epoch::core::time::system_time_string();
    }
}
