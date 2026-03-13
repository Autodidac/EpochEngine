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

export module core.time;

export namespace epoch::core::time
{
    using steady = std::chrono::steady_clock;

    // Monotonic timestamp in nanoseconds since an arbitrary epoch.
    std::uint64_t now_ns() noexcept;

    // Monotonic timestamp in nanoseconds since an arbitrary epoch.
    double now_seconds() noexcept;

    void sleep_ms(std::uint32_t ms);

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
