// EpochEngine C++23: exact, per-request model telemetry arithmetic.
// Token counts and bytes are different units. No tokenizer estimates or
// transport bitrate are ever presented as measured model token rates.
#pragma once
#include <cmath>
#include <cstdint>
#include <optional>

namespace epochengine::ai::metrics
{
    [[nodiscard]] inline std::optional<double> tokens_per_second(
        std::uint64_t reported_output_tokens, double generation_milliseconds) noexcept
    {
        if (reported_output_tokens == 0 || !std::isfinite(generation_milliseconds)
            || generation_milliseconds <= 0.0)
            return std::nullopt;
        const auto rate = static_cast<double>(reported_output_tokens) * 1000.0
            / generation_milliseconds;
        if (!std::isfinite(rate) || rate <= 0.0)
            return std::nullopt;
        return rate;
    }
}
