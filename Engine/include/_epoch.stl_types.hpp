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
#pragma once

#include "../src/epoch.api_types.hpp"

// Centralized STL includes for header-importing translation units.
// Modules should STILL include what they use in their global module fragment.
#include <algorithm>
#include <compare>
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

namespace epochengine
{
    #if defined(__cpp_lib_expected) && (__cpp_lib_expected >= 202202L)
    template <class E>
    using unexpected = std::unexpected<E>;

    template <class T, class E>
    using expected = std::expected<T, E>;
    #else
    template <class E>
    class unexpected
    {
    public:
        unexpected(const E& error) : error_(error) {}
        unexpected(E&& error) : error_(std::move(error)) {}

        [[nodiscard]] const E& error() const& noexcept { return error_; }
        [[nodiscard]] E&       error() & noexcept { return error_; }
        [[nodiscard]] E&&      error() && noexcept { return std::move(error_); }

    private:
        E error_;
    };

    template <class T, class E>
    class expected
    {
    public:
        expected(const T& value) : value_(value) {}
        expected(T&& value) : value_(std::move(value)) {}
        template <class U>
            requires (!std::same_as<std::remove_cvref_t<U>, T> &&
                      !std::same_as<std::remove_cvref_t<U>, unexpected<E>> &&
                      std::constructible_from<T, U&&>)
        expected(U&& value) : value_(std::in_place, std::forward<U>(value)) {}
        expected(const unexpected<E>& error) : error_(error.error()) {}
        expected(unexpected<E>&& error) : error_(std::move(error).error()) {}

        [[nodiscard]] explicit operator bool() const noexcept { return value_.has_value(); }
        [[nodiscard]] bool has_value() const noexcept { return value_.has_value(); }

        [[nodiscard]] T&       value() & { return *value_; }
        [[nodiscard]] const T& value() const& { return *value_; }
        [[nodiscard]] T&&      value() && { return std::move(*value_); }

        [[nodiscard]] E&       error() & { return *error_; }
        [[nodiscard]] const E& error() const& { return *error_; }
        [[nodiscard]] E&&      error() && { return std::move(*error_); }

        [[nodiscard]] T&       operator*() & noexcept { return *value_; }
        [[nodiscard]] const T& operator*() const& noexcept { return *value_; }
        [[nodiscard]] T*       operator->() noexcept { return std::addressof(*value_); }
        [[nodiscard]] const T* operator->() const noexcept { return std::addressof(*value_); }

    private:
        std::optional<T> value_{};
        std::optional<E> error_{};
    };

    template <class E>
    class expected<void, E>
    {
    public:
        expected() noexcept = default;
        expected(const unexpected<E>& error) : has_value_(false), error_(error.error()) {}
        expected(unexpected<E>&& error) : has_value_(false), error_(std::move(error).error()) {}

        [[nodiscard]] explicit operator bool() const noexcept { return has_value_; }
        [[nodiscard]] bool has_value() const noexcept { return has_value_; }

        void value() const noexcept {}

        [[nodiscard]] E&       error() & { return *error_; }
        [[nodiscard]] const E& error() const& { return *error_; }
        [[nodiscard]] E&&      error() && { return std::move(*error_); }

    private:
        bool has_value_ = true;
        std::optional<E> error_{};
    };
    #endif

    // ------------------------------------------------------------------------
    // Owned UTF-8 string wrapper (backed by std::string)
    // ------------------------------------------------------------------------
    struct string
    {
        std::string impl;

        string() = default;
        string(const char* s) : impl(s ? s : "") {}
        string(std::string s) : impl(std::move(s)) {}

        explicit string(epochengine::string_view v)
            : impl(v.data ? std::string(v.data, v.size) : std::string{})
        {
        }

        [[nodiscard]] const char* c_str() const noexcept { return impl.c_str(); }
        [[nodiscard]] const char* data()  const noexcept { return impl.data(); }
        [[nodiscard]] std::size_t size()  const noexcept { return impl.size(); }
        [[nodiscard]] bool empty()        const noexcept { return impl.empty(); }

        [[nodiscard]] epochengine::string_view view() const noexcept
        {
            return epochengine::string_view{ impl.data(), impl.size() };
        }

        [[nodiscard]] operator epochengine::string_view() const noexcept { return view(); }

        void reserve(std::size_t n) { impl.reserve(n); }
        void append(epochengine::string_view v) { impl.append(v.data ? v.data : "", v.size); }
    };

    // ------------------------------------------------------------------------
    // epoch <-> std adapters
    // ------------------------------------------------------------------------
    [[nodiscard]] constexpr epochengine::string_view to_view(epochengine::string_view v) noexcept { return v; }

    template <std::size_t N>
    [[nodiscard]] constexpr epochengine::string_view to_view(const char(&lit)[N]) noexcept
    {
        return epochengine::string_view{ lit, N ? (N - 1) : 0 };
    }

    [[nodiscard]] inline epochengine::string_view to_view(std::string_view v) noexcept
    {
        return epochengine::string_view{ v.data(), v.size() };
    }

    [[nodiscard]] inline std::string_view to_std(epochengine::string_view v) noexcept
    {
        return std::string_view{ v.data ? v.data : "", v.size };
    }

    [[nodiscard]] inline std::string_view to_std(const epochengine::string& s) noexcept
    {
        return std::string_view{ s.impl.data(), s.impl.size() };
    }

    template <class T>
    [[nodiscard]] inline epochengine::span<T> to_span(std::span<T> s) noexcept
    {
        return epochengine::span<T>{ s.data(), s.size() };
    }

    template <class T>
    [[nodiscard]] inline std::span<T> to_std(epochengine::span<T> s) noexcept
    {
        return std::span<T>{ s.data, s.size };
    }

    // ------------------------------------------------------------------------
    // comparisons
    //
    // IMPORTANT:
    // - epochengine::string_view == epochengine::string_view already lives in epoch.api_types.hpp.
    // - Do NOT add overloads with std::string_view on the LEFT. That makes
    //   `std::string_view == "literal"` ambiguous (std vs epoch operator paths).
    // ------------------------------------------------------------------------
    [[nodiscard]] constexpr bool equals(epochengine::string_view a, epochengine::string_view b) noexcept
    {
        if (a.size != b.size) return false;
        for (std::size_t i = 0; i < a.size; ++i)
            if (a.data[i] != b.data[i]) return false;
        return true;
    }

    // epochengine::string_view <-> std::string_view (epoch on LEFT only)
    [[nodiscard]] inline bool operator==(epochengine::string_view a, std::string_view b) noexcept
    {
        return equals(a, epochengine::to_view(b));
    }

    // epochengine::string <-> epochengine::string_view / std::string_view / const char*
    [[nodiscard]] inline bool operator==(const epochengine::string& a, epochengine::string_view b) noexcept
    {
        return equals(a.view(), b);
    }

    [[nodiscard]] inline bool operator==(epochengine::string_view a, const epochengine::string& b) noexcept
    {
        return equals(a, b.view());
    }

    [[nodiscard]] inline bool operator==(const epochengine::string& a, std::string_view b) noexcept
    {
        return equals(a.view(), epochengine::to_view(b));
    }

    [[nodiscard]] inline bool operator==(const epochengine::string& a, const char* b) noexcept
    {
        const std::size_t n = b ? std::char_traits<char>::length(b) : 0u;
        return equals(a.view(), epochengine::string_view{ b ? b : "", n });
    }

    [[nodiscard]] inline bool operator==(epochengine::string_view a, const char* b) noexcept
    {
        const std::size_t n = b ? std::char_traits<char>::length(b) : 0u;
        return equals(a, epochengine::string_view{ b ? b : "", n });
    }


    // ------------------------------------------------------------------------
    // Type aliases
    // ------------------------------------------------------------------------
    template <class T>
    using optional = std::optional<T>;

    template <class T, class Alloc = std::allocator<T>>
    using small_vector = std::vector<T, Alloc>;

    using format_args = std::format_args;
} // namespace epochengine

// ------------------------------------------------------------------------
// std customizations (hash + formatters)
// ------------------------------------------------------------------------
namespace std
{
    template <>
    struct hash<epochengine::string>
    {
        size_t operator()(const epochengine::string& s) const noexcept
        {
            return std::hash<std::string_view>{}(epochengine::to_std(s));
        }
    };

    template <>
    struct formatter<epochengine::string_view, char> : formatter<std::string_view, char>
    {
        auto format(epochengine::string_view v, format_context& ctx) const
        {
            return formatter<std::string_view, char>::format(epochengine::to_std(v), ctx);
        }
    };

    template <>
    struct formatter<epochengine::string, char> : formatter<std::string_view, char>
    {
        auto format(const epochengine::string& v, format_context& ctx) const
        {
            return formatter<std::string_view, char>::format(epochengine::to_std(v), ctx);
        }
    };
} // namespace std
