/************************************************
 *  This file is part of the Epoch Project.     *
 *  epochengine - Modular C++ Framework         *
 *                                              *
 *  SPDX-License-Identifier:                    *
 *  LicenseRef-MIT-NoSell                       *
 ***********************************************/
#pragma once

#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

namespace epochengine
{
    namespace format_detail
    {
        struct format_spec final
        {
            char fill{' '};
            char align{};
            char type{};
            int width{};
            int precision{-1};
        };

        [[nodiscard]] inline format_spec parse_spec(std::string_view text)
        {
            format_spec spec{};
            std::size_t cursor = 0;

            if (text.size() >= 2u && (text[1] == '<' || text[1] == '>' || text[1] == '^'))
            {
                spec.fill = text[0];
                spec.align = text[1];
                cursor = 2u;
            }
            else if (cursor < text.size() && (text[cursor] == '<' || text[cursor] == '>' || text[cursor] == '^'))
            {
                spec.align = text[cursor++];
            }

            if (cursor < text.size() && text[cursor] == '0')
            {
                spec.fill = '0';
                spec.align = '>';
                ++cursor;
            }

            while (cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9')
            {
                spec.width = (spec.width * 10) + static_cast<int>(text[cursor] - '0');
                ++cursor;
            }

            if (cursor < text.size() && text[cursor] == '.')
            {
                ++cursor;
                spec.precision = 0;
                while (cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9')
                {
                    spec.precision = (spec.precision * 10) + static_cast<int>(text[cursor] - '0');
                    ++cursor;
                }
            }

            if (cursor < text.size())
                spec.type = text[cursor++];
            if (cursor != text.size())
                throw std::invalid_argument("unsupported Epoch format specification");
            return spec;
        }

        [[nodiscard]] inline std::string apply_width(std::string text, const format_spec& spec)
        {
            if (spec.width <= 0 || text.size() >= static_cast<std::size_t>(spec.width))
                return text;

            const std::size_t padding = static_cast<std::size_t>(spec.width) - text.size();
            if (spec.align == '<')
                return text + std::string(padding, spec.fill);
            if (spec.align == '^')
            {
                const std::size_t left = padding / 2u;
                return std::string(left, spec.fill) + text + std::string(padding - left, spec.fill);
            }

            if (spec.fill == '0' && !text.empty() && (text.front() == '-' || text.front() == '+'))
                return text.substr(0u, 1u) + std::string(padding, '0') + text.substr(1u);
            if (spec.fill == '0' && text.size() > 2u && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
                return text.substr(0u, 2u) + std::string(padding, '0') + text.substr(2u);
            return std::string(padding, spec.fill) + text;
        }

        template <class>
        inline constexpr bool always_false_v = false;

        template <class T>
        [[nodiscard]] std::string integral_text(const T value, const format_spec& spec)
        {
            std::array<char, 96u> buffer{};
            const int base = (spec.type == 'x' || spec.type == 'X' || spec.type == 'p') ? 16 : 10;
            const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, base);
            if (result.ec != std::errc{})
                throw std::invalid_argument("Epoch could not format an integer value");

            std::string text{buffer.data(), result.ptr};
            if (spec.type == 'X')
            {
                for (char& character : text)
                    if (character >= 'a' && character <= 'f')
                        character = static_cast<char>(character - ('a' - 'A'));
            }
            if (spec.type == 'p')
                text.insert(0u, "0x");
            return apply_width(std::move(text), spec);
        }

        template <class T>
        [[nodiscard]] std::string format_value(const T& value, const format_spec& spec)
        {
            using value_type = std::remove_cvref_t<T>;

            if constexpr (std::same_as<value_type, std::string> || std::same_as<value_type, std::string_view>)
            {
                return apply_width(std::string{value}, spec);
            }
            else if constexpr (std::is_array_v<value_type>
                && std::same_as<std::remove_cv_t<std::remove_extent_t<value_type>>, char>)
            {
                return apply_width(std::string{value}, spec);
            }
            else if constexpr (requires {
                { value.data() } -> std::convertible_to<const char*>;
                { value.size() } -> std::convertible_to<std::size_t>;
            })
            {
                return apply_width(
                    std::string{value.data(), static_cast<std::size_t>(value.size())},
                    spec);
            }
            else if constexpr (requires {
                { value.data } -> std::convertible_to<const char*>;
                { value.size } -> std::convertible_to<std::size_t>;
            })
            {
                return apply_width(
                    std::string{value.data ? value.data : "", static_cast<std::size_t>(value.size)},
                    spec);
            }
            else if constexpr (std::is_pointer_v<value_type>
                && std::same_as<std::remove_cv_t<std::remove_pointer_t<value_type>>, char>)
            {
                return apply_width(value ? std::string{value} : std::string{"null"}, spec);
            }
            else if constexpr (std::same_as<value_type, char>)
            {
                if (spec.type == 'x' || spec.type == 'X')
                    return integral_text(static_cast<unsigned int>(static_cast<unsigned char>(value)), spec);
                return apply_width(std::string(1u, value), spec);
            }
            else if constexpr (std::same_as<value_type, bool>)
            {
                return apply_width(value ? "true" : "false", spec);
            }
            else if constexpr (std::is_enum_v<value_type>)
            {
                return integral_text(static_cast<std::underlying_type_t<value_type>>(value), spec);
            }
            else if constexpr (std::is_pointer_v<value_type>)
            {
                if (value == nullptr)
                    return apply_width(spec.type == 'p' ? "0x0" : "null", spec);
                format_spec pointerSpec = spec;
                if (pointerSpec.type == 0)
                    pointerSpec.type = 'p';
                return integral_text(reinterpret_cast<std::uintptr_t>(value), pointerSpec);
            }
            else if constexpr (std::is_integral_v<value_type>)
            {
                return integral_text(value, spec);
            }
            else if constexpr (std::is_floating_point_v<value_type>)
            {
                std::array<char, 128u> buffer{};
                std::to_chars_result result{};
                if (spec.precision >= 0)
                    result = std::to_chars(
                        buffer.data(),
                        buffer.data() + buffer.size(),
                        value,
                        std::chars_format::fixed,
                        spec.precision);
                else
                    result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
                if (result.ec != std::errc{})
                    throw std::invalid_argument("Epoch could not format a floating-point value");
                return apply_width(std::string{buffer.data(), result.ptr}, spec);
            }
            else if constexpr (requires { { value.generic_string() } -> std::convertible_to<std::string>; })
            {
                return apply_width(value.generic_string(), spec);
            }
            else
            {
                static_assert(always_false_v<value_type>, "Epoch formatter cannot render this type");
            }
        }

        template <std::size_t Index = 0u, class Tuple>
        [[nodiscard]] std::string format_tuple_value(
            const Tuple& values,
            const std::size_t requested,
            const format_spec& spec)
        {
            if constexpr (Index < std::tuple_size_v<Tuple>)
            {
                if (Index == requested)
                    return format_value(std::get<Index>(values), spec);
                return format_tuple_value<Index + 1u>(values, requested, spec);
            }
            throw std::invalid_argument("Epoch format argument index is out of range");
        }
    }

    template <class... Args>
    [[nodiscard]] std::string format_text(const std::string_view pattern, Args&&... args)
    {
        const auto values = std::forward_as_tuple(std::forward<Args>(args)...);
        std::string output;
        output.reserve(pattern.size() + sizeof...(Args) * 8u);
        std::size_t nextArgument = 0u;

        for (std::size_t cursor = 0u; cursor < pattern.size();)
        {
            if (pattern[cursor] == '{')
            {
                if (cursor + 1u < pattern.size() && pattern[cursor + 1u] == '{')
                {
                    output.push_back('{');
                    cursor += 2u;
                    continue;
                }

                const std::size_t close = pattern.find('}', cursor + 1u);
                if (close == std::string_view::npos)
                    throw std::invalid_argument("unterminated Epoch format field");

                const std::string_view field = pattern.substr(cursor + 1u, close - cursor - 1u);
                std::string_view specText{};
                if (!field.empty())
                {
                    if (field.front() != ':')
                        throw std::invalid_argument("indexed Epoch format fields are not supported");
                    specText = field.substr(1u);
                }

                output += format_detail::format_tuple_value(values, nextArgument++, format_detail::parse_spec(specText));
                cursor = close + 1u;
                continue;
            }

            if (pattern[cursor] == '}')
            {
                if (cursor + 1u < pattern.size() && pattern[cursor + 1u] == '}')
                {
                    output.push_back('}');
                    cursor += 2u;
                    continue;
                }
                throw std::invalid_argument("unmatched Epoch format closing brace");
            }

            output.push_back(pattern[cursor++]);
        }
        return output;
    }
}
