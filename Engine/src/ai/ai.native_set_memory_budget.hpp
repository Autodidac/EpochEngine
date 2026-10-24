// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Cross-platform, allocation-free policy for admitting additional native models.
// This is a system-RAM headroom heuristic, NOT a VRAM measurement or an exact
// prediction of GGUF weights, KV context, or GPU offload requirements.
#include <algorithm>
#include <cstdint>

namespace epochengine::ai::native_runtime::resident_budget {
struct Sample final {
    std::uint64_t total{}, available{};
    [[nodiscard]] constexpr bool valid() const noexcept {
        return total != 0u && available <= total;
    }
};
[[nodiscard]] constexpr std::uint64_t threshold_bytes(
    Sample ram, std::uint64_t expected_model_file_bytes) noexcept {
    constexpr std::uint64_t gib = 1024ull * 1024ull * 1024ull;
    const auto reserve = (std::max)(3u * gib, ram.total / 20u);
    const auto anticipated = expected_model_file_bytes == 0u ? 2u * gib
        : (std::min)(4u * gib, expected_model_file_bytes / 3u);
    return reserve + anticipated;
}
[[nodiscard]] constexpr bool admit(Sample ram,
    std::uint64_t expected_model_file_bytes) noexcept {
    return !ram.valid() ||
        ram.available >= threshold_bytes(ram, expected_model_file_bytes);
}
} // namespace epochengine::ai::native_runtime::resident_budget
