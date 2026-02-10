#pragma once
#include "epoch.api_types.hpp"

// Centralized STL includes for header-importing translation units.
// Modules should STILL include what they use in their global module fragment.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace epoch
{
    // ------------------------------------------------------------------------
    // Owned UTF-8 string wrapper (backed by std::string)
    // ------------------------------------------------------------------------
    struct string
    {
        std::string impl;

        string() = default;
        string(const char* s) : impl(s ? s : "") {}
        string(std::string s) : impl(std::move(s)) {}

        explicit string(epoch::string_view v)
            : impl(v.data ? std::string(v.data, v.size) : std::string{})
        {
        }

        [[nodiscard]] const char* c_str() const noexcept { return impl.c_str(); }
        [[nodiscard]] const char* data()  const noexcept { return impl.data(); }
        [[nodiscard]] std::size_t size()  const noexcept { return impl.size(); }
        [[nodiscard]] bool empty()        const noexcept { return impl.empty(); }

        [[nodiscard]] epoch::string_view view() const noexcept
        {
            return epoch::string_view{ impl.data(), impl.size() };
        }

        [[nodiscard]] operator epoch::string_view() const noexcept { return view(); }

        void reserve(std::size_t n) { impl.reserve(n); }
        void append(epoch::string_view v) { impl.append(v.data ? v.data : "", v.size); }
    };

    // ------------------------------------------------------------------------
    // epoch <-> std adapters
    // ------------------------------------------------------------------------
    [[nodiscard]] constexpr epoch::string_view to_view(epoch::string_view v) noexcept { return v; }

    template <std::size_t N>
    [[nodiscard]] constexpr epoch::string_view to_view(const char(&lit)[N]) noexcept
    {
        return epoch::string_view{ lit, N ? (N - 1) : 0 };
    }

    [[nodiscard]] inline epoch::string_view to_view(std::string_view v) noexcept
    {
        return epoch::string_view{ v.data(), v.size() };
    }

    [[nodiscard]] inline std::string_view to_std(epoch::string_view v) noexcept
    {
        return std::string_view{ v.data ? v.data : "", v.size };
    }

    [[nodiscard]] inline std::string_view to_std(const epoch::string& s) noexcept
    {
        return std::string_view{ s.impl.data(), s.impl.size() };
    }

    template <class T>
    [[nodiscard]] inline epoch::span<T> to_span(std::span<T> s) noexcept
    {
        return epoch::span<T>{ s.data(), s.size() };
    }

    template <class T>
    [[nodiscard]] inline std::span<T> to_std(epoch::span<T> s) noexcept
    {
        return std::span<T>{ s.data, s.size };
    }

    // ------------------------------------------------------------------------
    // comparisons
    //
    // IMPORTANT:
    // - epoch::string_view == epoch::string_view already lives in epoch.api_types.hpp.
    // - Do NOT add overloads with std::string_view on the LEFT. That makes
    //   `std::string_view == "literal"` ambiguous (std vs epoch operator paths).
    // ------------------------------------------------------------------------
    [[nodiscard]] constexpr bool equals(epoch::string_view a, epoch::string_view b) noexcept
    {
        if (a.size != b.size) return false;
        for (std::size_t i = 0; i < a.size; ++i)
            if (a.data[i] != b.data[i]) return false;
        return true;
    }

    // epoch::string_view <-> std::string_view (epoch on LEFT only)
    [[nodiscard]] inline bool operator==(epoch::string_view a, std::string_view b) noexcept
    {
        return equals(a, epoch::to_view(b));
    }

    // epoch::string <-> epoch::string_view / std::string_view / const char*
    [[nodiscard]] inline bool operator==(const epoch::string& a, epoch::string_view b) noexcept
    {
        return equals(a.view(), b);
    }

    [[nodiscard]] inline bool operator==(epoch::string_view a, const epoch::string& b) noexcept
    {
        return equals(a, b.view());
    }

    [[nodiscard]] inline bool operator==(const epoch::string& a, std::string_view b) noexcept
    {
        return equals(a.view(), epoch::to_view(b));
    }

    [[nodiscard]] inline bool operator==(const epoch::string& a, const char* b) noexcept
    {
        const std::size_t n = b ? std::char_traits<char>::length(b) : 0u;
        return equals(a.view(), epoch::string_view{ b ? b : "", n });
    }

    [[nodiscard]] inline bool operator==(epoch::string_view a, const char* b) noexcept
    {
        const std::size_t n = b ? std::char_traits<char>::length(b) : 0u;
        return equals(a, epoch::string_view{ b ? b : "", n });
    }


    // ------------------------------------------------------------------------
    // Type aliases
    // ------------------------------------------------------------------------
    template <class T>
    using optional = std::optional<T>;

    template <class T, class E>
    using expected = std::expected<T, E>;

    template <class T, class Alloc = std::allocator<T>>
    using small_vector = std::vector<T, Alloc>;

    using format_args = std::format_args;
} // namespace epoch

// ------------------------------------------------------------------------
// std customizations (hash + formatters)
// ------------------------------------------------------------------------
namespace std
{
    template <>
    struct hash<epoch::string>
    {
        size_t operator()(const epoch::string& s) const noexcept
        {
            return std::hash<std::string_view>{}(epoch::to_std(s));
        }
    };

    template <>
    struct formatter<epoch::string_view, char> : formatter<std::string_view, char>
    {
        auto format(epoch::string_view v, format_context& ctx) const
        {
            return formatter<std::string_view, char>::format(epoch::to_std(v), ctx);
        }
    };

    template <>
    struct formatter<epoch::string, char> : formatter<std::string_view, char>
    {
        auto format(const epoch::string& v, format_context& ctx) const
        {
            return formatter<std::string_view, char>::format(epoch::to_std(v), ctx);
        }
    };
} // namespace std
