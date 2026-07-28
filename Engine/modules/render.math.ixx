// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <cmath>

export module render.math;

export namespace epochengine::render_math
{
    struct Float3 final
    {
        float x{};
        float y{};
        float z{};

        friend constexpr bool operator==(const Float3&, const Float3&) noexcept = default;
    };

    struct LinearRgb final
    {
        float r{};
        float g{};
        float b{};

        friend constexpr bool operator==(const LinearRgb&, const LinearRgb&) noexcept = default;
    };

    [[nodiscard]] constexpr Float3 add(Float3 lhs, Float3 rhs) noexcept
    {
        return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
    }

    [[nodiscard]] constexpr Float3 subtract(Float3 lhs, Float3 rhs) noexcept
    {
        return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
    }

    [[nodiscard]] constexpr Float3 multiply(Float3 lhs, Float3 rhs) noexcept
    {
        return {lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z};
    }

    [[nodiscard]] constexpr Float3 scale(Float3 value, float factor) noexcept
    {
        return {value.x * factor, value.y * factor, value.z * factor};
    }

    [[nodiscard]] constexpr float dot(Float3 lhs, Float3 rhs) noexcept
    {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    [[nodiscard]] constexpr Float3 cross(Float3 lhs, Float3 rhs) noexcept
    {
        return {
            lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x
        };
    }

    [[nodiscard]] inline bool finite(Float3 value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    [[nodiscard]] inline float length_squared(Float3 value) noexcept
    {
        return dot(value, value);
    }

    [[nodiscard]] inline float length(Float3 value) noexcept
    {
        return std::sqrt(length_squared(value));
    }

    [[nodiscard]] inline Float3 normalize(
        Float3 value,
        Float3 fallback = {0.0f, 1.0f, 0.0f}) noexcept
    {
        const float magnitude = length(value);
        if (!std::isfinite(magnitude) || magnitude <= 1.0e-7f)
        {
            return fallback;
        }
        return scale(value, 1.0f / magnitude);
    }

    [[nodiscard]] constexpr Float3 point_at(
        Float3 origin,
        Float3 direction,
        float distance) noexcept
    {
        return add(origin, scale(direction, distance));
    }

    [[nodiscard]] constexpr LinearRgb add(LinearRgb lhs, LinearRgb rhs) noexcept
    {
        return {lhs.r + rhs.r, lhs.g + rhs.g, lhs.b + rhs.b};
    }

    [[nodiscard]] constexpr LinearRgb multiply(LinearRgb lhs, LinearRgb rhs) noexcept
    {
        return {lhs.r * rhs.r, lhs.g * rhs.g, lhs.b * rhs.b};
    }

    [[nodiscard]] constexpr LinearRgb scale(LinearRgb value, float factor) noexcept
    {
        return {value.r * factor, value.g * factor, value.b * factor};
    }

    [[nodiscard]] constexpr LinearRgb saturate(LinearRgb value) noexcept
    {
        return {
            (std::clamp)(value.r, 0.0f, 1.0f),
            (std::clamp)(value.g, 0.0f, 1.0f),
            (std::clamp)(value.b, 0.0f, 1.0f)
        };
    }
}