/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#pragma once

#include <algorithm>
#include <cstddef>

namespace epochengine::editor_ai_development_panel::discovery_budget
{
    // This is an implementation reminder, not an admission ceiling. The initial
    // host context counts as a round, and a necessary dependency can legitimately
    // require more than two further reads. Actual repeated-evidence detection
    // belongs to the cumulative workspace's stagnation watchdog.
    inline constexpr std::size_t initial_rounds = 3u;
    inline constexpr std::size_t additional_rounds_per_repair = 2u;
    inline constexpr std::size_t maximum_repair_round_allowances = 3u;

    [[nodiscard]] constexpr std::size_t focus_after_rounds(
        const std::size_t repair_attempts) noexcept
    {
        return initial_rounds + additional_rounds_per_repair
            * (std::min)(repair_attempts, maximum_repair_round_allowances);
    }

    [[nodiscard]] constexpr bool prioritize_edit(
        const std::size_t verified_rounds,
        const std::size_t repair_attempts) noexcept
    {
        return verified_rounds >= focus_after_rounds(repair_attempts) - 1u;
    }
}
