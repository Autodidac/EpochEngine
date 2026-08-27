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

#include "../include/core.stl_types.hpp"

export module core.math;

export namespace epochengine::core::math
{
    template <class T>
    concept arithmetic = std::is_arithmetic_v<T>;

    template <arithmetic T, class Domain = void>
    struct Vector2 final
    {
        T x{};
        T y{};

        [[nodiscard]] friend constexpr bool operator==(
            const Vector2&,
            const Vector2&) noexcept = default;
    };

    using Float2 = Vector2<float>;
    using Double2 = Vector2<double>;

    template <arithmetic T, class Domain>
    [[nodiscard]] constexpr Vector2<T, Domain> add(
        Vector2<T, Domain> left,
        Vector2<T, Domain> right) noexcept
    {
        return {left.x + right.x, left.y + right.y};
    }

    template <arithmetic T, class Domain>
    [[nodiscard]] constexpr Vector2<T, Domain> subtract(
        Vector2<T, Domain> left,
        Vector2<T, Domain> right) noexcept
    {
        return {left.x - right.x, left.y - right.y};
    }

    template <arithmetic T, class Domain>
    [[nodiscard]] constexpr Vector2<T, Domain> scale(
        Vector2<T, Domain> value,
        T factor) noexcept
    {
        return {value.x * factor, value.y * factor};
    }

    template <arithmetic T, class Domain>
    [[nodiscard]] constexpr T dot(
        Vector2<T, Domain> left,
        Vector2<T, Domain> right) noexcept
    {
        return left.x * right.x + left.y * right.y;
    }

    template <arithmetic T>
    constexpr T clamp(T v, T lo, T hi) noexcept
    {
        return (v < lo) ? lo : (v > hi) ? hi : v;
    }

    template <arithmetic T>
    constexpr T lerp(T a, T b, T t) noexcept
    {
        return a + (b - a) * t;
    }

    constexpr double pi = 3.141592653589793238462643383279502884;

    constexpr double radians(double degrees) noexcept { return degrees * (pi / 180.0); }
    constexpr double degrees(double radians) noexcept { return radians * (180.0 / pi); }

    constexpr std::size_t align_up(std::size_t v, std::size_t align) noexcept
    {
        return (align == 0) ? v : ((v + (align - 1)) / align) * align;
    }
}
