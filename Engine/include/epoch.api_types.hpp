#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <memory> // std::addressof

namespace epoch
{
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using i8 = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;

    using f32 = float;
    using f64 = double;

    // ---------------------------------------------------------------------
    // string_view (ABI-safe)
    // ---------------------------------------------------------------------
    struct string_view
    {
        const char* data = nullptr;
        std::size_t size = 0;

        constexpr string_view() = default;
        constexpr string_view(const char* p, std::size_t n) : data(p), size(n) {}

        template <std::size_t N>
        constexpr string_view(const char(&lit)[N])
            : data(lit), size(N ? (N - 1) : 0) {
        }

        [[nodiscard]] constexpr bool empty() const noexcept { return size == 0; }
        [[nodiscard]] constexpr const char* begin() const noexcept { return data; }
        [[nodiscard]] constexpr const char* end()   const noexcept { return data + size; }

        [[nodiscard]] constexpr char operator[](std::size_t i) const noexcept
        {
            return data[i];
        }

        [[nodiscard]] constexpr string_view substr(std::size_t pos, std::size_t n) const noexcept
        {
            if (pos > size) return {};
            const std::size_t remain = size - pos;
            const std::size_t take = (n > remain) ? remain : n;
            return { data + pos, take };
        }

        [[nodiscard]] constexpr string_view substr(std::size_t pos) const noexcept
        {
            return substr(pos, size);
        }
    };

    // Explicit equality — REQUIRED
    [[nodiscard]] constexpr bool operator==(string_view a, string_view b) noexcept
    {
        if (a.size != b.size) return false;
        for (std::size_t i = 0; i < a.size; ++i)
            if (a.data[i] != b.data[i]) return false;
        return true;
    }

    [[nodiscard]] constexpr bool operator!=(string_view a, string_view b) noexcept
    {
        return !(a == b);
    }

    // ---------------------------------------------------------------------
    // span (ABI-safe)
    // ---------------------------------------------------------------------
    template <class T>
    struct span
    {
        T* data = nullptr;
        std::size_t size = 0;

        constexpr span() = default;
        constexpr span(T* p, std::size_t n) : data(p), size(n) {}

        [[nodiscard]] constexpr bool empty() const noexcept { return size == 0; }
        [[nodiscard]] constexpr T* begin() const noexcept { return data; }
        [[nodiscard]] constexpr T* end()   const noexcept { return data + size; }

        [[nodiscard]] constexpr T& operator[](std::size_t i) const noexcept
        {
            return data[i];
        }
    };

    template <class T>
    using array_view = span<T>;

    // ---------------------------------------------------------------------
    // function_ref (non-owning, STL-free)
    // ---------------------------------------------------------------------
    template <class>
    class function_ref;

    template <class R, class... Args>
    class function_ref<R(Args...)>
    {
        void* obj = nullptr;
        R(*thunk)(void*, Args...) = nullptr;

    public:
        constexpr function_ref() = default;

        template <class F>
            requires (!std::is_same_v<std::remove_cvref_t<F>, function_ref>)
        constexpr function_ref(F&& f) noexcept
            : obj((void*)std::addressof(f))
            , thunk([](void* o, Args... a) -> R
                {
                    return (*static_cast<std::remove_reference_t<F>*>(o))(
                        static_cast<Args&&>(a)...);
                })
        {
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return thunk != nullptr;
        }

        constexpr R operator()(Args... a) const
        {
            return thunk(obj, static_cast<Args&&>(a)...);
        }
    };

    // Forward-declared owning string (defined in STL layer)
    struct string;
}
