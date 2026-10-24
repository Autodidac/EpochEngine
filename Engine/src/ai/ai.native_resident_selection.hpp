// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once

namespace epochengine::ai::native_runtime::detail
{
    // The first loaded native model establishes chat ownership. An additional
    // resident model must not replace the foundation unless explicitly chosen.
    [[nodiscard]] constexpr bool should_establish_foundation(
        bool has_foundation, bool select_for_requests) noexcept
    {
        return select_for_requests || !has_foundation;
    }
}
