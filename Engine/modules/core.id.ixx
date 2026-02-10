module;

#include "../include/_epoch.stl_types.hpp"

export module core.id;

export namespace epoch::core::id
{
    template <class Tag, class T = std::uint32_t>
    struct strong_id
    {
        using value_type = T;
        T value{};

        constexpr strong_id() noexcept = default;
        constexpr explicit strong_id(T v) noexcept : value(v) {}

        static constexpr strong_id invalid() noexcept { return strong_id{}; }
        constexpr bool valid() const noexcept { return value != T{}; }

        constexpr auto operator<=>(const strong_id&) const noexcept = default;
    };
}
