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
 // ============================================================================
// src/core.time.cpp
// ============================================================================

module;

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
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

    std::string system_time_string()
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
