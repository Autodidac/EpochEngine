/**************************************************************
 *   Epoch Engine - Modern Renderer Skeleton (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
#pragma once

#include <cstdint>
#include <compare>

namespace epoch
{
    using u8  = ::uint8_t;
    using u16 = ::uint16_t;
    using u32 = ::uint32_t;
    using u64 = ::uint64_t;

    using i32 = ::int32_t;
    using i64 = ::int64_t;

    using f32 = float;
    using f64 = double;

    template <class Tag, class T = u32>
    struct Handle
    {
        T value{};
        constexpr explicit operator bool() const noexcept { return value != 0; }
        constexpr auto operator<=>(const Handle&) const = default;
    };
} // namespace epoch
