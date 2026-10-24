// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace epochengine::ai::native_runtime::gpu_budget {

// The slider targets the model's layer count, not GPU utilization or VRAM.
// 512 is an input-validation ceiling, not a performance target.
// Weight-file size is an approximation only:
// KV memory, GPU compute buffers, driver allocations and per-layer weight
// variance are deliberately left as additional headroom.
inline constexpr std::uint32_t maximum_gpu_layers = 512U;
inline constexpr std::uint64_t gib = 1024ull * 1024ull * 1024ull;

[[nodiscard]] constexpr std::uint32_t weight_limited_layers(
    std::uint64_t weight_bytes, std::uint64_t budget_bytes,
    std::uint32_t layer_hint, std::uint32_t requested) noexcept {
    const auto layers = std::clamp(layer_hint, 1U, maximum_gpu_layers);
    if (weight_bytes == 0u) return std::min({requested, layers, 30U});
    const auto fitting = static_cast<std::uint32_t>(std::clamp(
        static_cast<long double>(budget_bytes) * layers / weight_bytes,
        0.0L, static_cast<long double>(layers)));
    return std::min(requested, fitting);
}

[[nodiscard]] constexpr std::uint64_t weight_budget_bytes(double configured_vram_gib,
    std::size_t resident_count) noexcept {
    if (resident_count == 0 || !(configured_vram_gib > 0.0)) return 0;
    // Leave at least 20% of the configured VRAM budget for the renderer,
    // GPU KV/compute buffers, driver, and desktop. A physical 16 GiB board
    // cannot hold 28% offload of 79 GiB Flash plus two other models.
    // 11 GiB is an upper ceiling for resident weights, not a utilization target.
    const double budget = std::clamp(configured_vram_gib * 0.80, 0.0, 11.0);
    return static_cast<std::uint64_t>(budget * static_cast<double>(gib));
}

// Allocate *weight bytes*, not percentages of the GGUF file. Favor the models
// used most often while letting the largest model retain a useful fraction
// of VRAM. 'rank' is sorted by measured GGUF bytes: smallest first.
// For three models: 18/34/48% of the safe cohort pool. Larger weights
// naturally yield lower slider percentages; no per-profile setting is changed.
[[nodiscard]] constexpr std::uint32_t offload_cap_percent(
    std::uint64_t weight_bytes, std::uint64_t group_weight_budget_bytes,
    std::size_t resident_count, std::size_t weight_rank,
    std::uint32_t layer_count_hint = 48U) noexcept {
    if (group_weight_budget_bytes == 0) return 100;
    if (weight_bytes == 0) return 10; // Unknown file size: conservative cap.
    const unsigned weight = resident_count == 1 ? 100u
        : resident_count == 3 ? (weight_rank == 0 ? 18u : weight_rank == 1 ? 34u : 48u)
        : (weight_rank == 0 ? 40u : 60u);
    const std::uint64_t allotted = static_cast<std::uint64_t>(
        static_cast<long double>(group_weight_budget_bytes) * weight / 100.0L);
    // The temporary cohort cap is expressed on the same effective-layer
    // scale as RuntimeModelProfile. Preserve the *weight-byte* budget when
    // tuning the per-model ceiling, otherwise three loads overcommit VRAM.
    const long double layer_ceiling_percent =
        static_cast<long double>(std::min(maximum_gpu_layers, std::max(1U, layer_count_hint))) *
            100.0L / static_cast<long double>(std::max(1U, layer_count_hint));
    const long double ratio = static_cast<long double>(allotted) * 10000.0L /
        (static_cast<long double>(weight_bytes) * layer_ceiling_percent);
    return static_cast<std::uint32_t>(std::clamp(ratio, 0.0L, 100.0L));
}


} // namespace epochengine::ai::native_runtime::gpu_budget
