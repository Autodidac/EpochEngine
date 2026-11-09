// EpochEngine C++23: exact, per-request model telemetry arithmetic.
// Token counts and bytes are different units. No tokenizer estimates or
// transport bitrate are ever presented as measured model token rates.
#pragma once
#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

namespace epochengine::ai::metrics
{
    [[nodiscard]] inline std::optional<double> positive_finite(double value) noexcept
    {
        return std::isfinite(value) && value > 0.0
            ? std::optional<double>{value} : std::nullopt;
    }

    [[nodiscard]] inline std::optional<double> reported_positive(std::string_view encoded) noexcept
    {
        if (encoded.empty()) return std::nullopt;
        double value{};
        const auto [end, error] = std::from_chars(
            encoded.data(), encoded.data() + encoded.size(), value);
        if (error != std::errc{} || end != encoded.data() + encoded.size()) return std::nullopt;
        return positive_finite(value);
    }

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

    // Providers may exclude a boundary token from their decode rate. Retain
    // their valid rate as reported; counts and durations remain independent.
    [[nodiscard]] inline std::optional<double> generation_rate(
        std::optional<double> reported_rate, std::uint64_t reported_output_tokens,
        double generation_milliseconds) noexcept
    {
        if (reported_rate)
            if (const auto rate = positive_finite(*reported_rate)) return rate;
        return tokens_per_second(reported_output_tokens, generation_milliseconds);
    }
}
