// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <limits>
#include <numbers>

#include "calculator.arithmetic.hpp"

namespace calculator_example
{
    enum class AngleUnit { degrees, radians };

    inline constexpr std::size_t maximum_expression_length = 256u;
    inline constexpr std::size_t maximum_expression_depth = 32u;

    struct CalculationStep final
    {
        std::size_t begin{}, end{};
        double value{};
    };

    struct CalculationTrace final
    {
        // Each step consumes at least one character in the bounded input.
        std::array<CalculationStep, maximum_expression_length> steps{};
        std::size_t count{};
    };

    namespace detail
    {
        class ExpressionParser final
        {
        public:
            ExpressionParser(std::string_view text, AngleUnit angle,
                CalculationTrace* trace) noexcept : text_{text}, angle_{angle}, trace_{trace} {}

            [[nodiscard]] CalculationResult evaluate() noexcept
            {
                if (text_.size() > maximum_expression_length)
                    return {.error = CalculationError::expression_too_long, .position = maximum_expression_length};
                const double value = sum(0u);
                if (!status_) return status_;
                (void)peek();
                if (position_ != text_.size())
                    return {.error = CalculationError::unexpected_token, .position = position_};
                return {.value = value};
            }

        private:
            static constexpr double pi = std::numbers::pi_v<double>;
            std::string_view text_;
            AngleUnit angle_;
            CalculationTrace* trace_{};
            std::size_t position_{};
            CalculationResult status_{};

            [[nodiscard]] static bool is_digit(char ch) noexcept { return ch >= '0' && ch <= '9'; }
            [[nodiscard]] static bool is_letter(char ch) noexcept { return ch >= 'a' && ch <= 'z'; }

            [[nodiscard]] char peek() noexcept
            {
                while (position_ < text_.size())
                {
                    const char ch = text_[position_];
                    if (!is_space(ch)) return ch;
                    ++position_;
                }
                return '\0';
            }

            [[nodiscard]] double fail(CalculationError error, std::size_t position) noexcept
            {
                if (status_) status_ = {.error = error, .position = position};
                return 0.0;
            }

            [[nodiscard]] double step(std::size_t begin, double value) noexcept
            {
                if (!trace_ || !status_) return value;
                auto end = position_;
                while (end > begin && is_space(text_[end - 1u])) --end;
                trace_->steps[trace_->count++] = {begin, end, value};
                return value;
            }

            [[nodiscard]] static bool is_space(char ch) noexcept
            { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; }

            [[nodiscard]] double binary(char operation, double left, double right, std::size_t at) noexcept
            {
                if (!status_) return 0.0;
                const auto result = apply_basic_operation(operation, left, right);
                return result ? result.value : fail(result.error, at);
            }

            [[nodiscard]] double sum(std::size_t depth) noexcept
            {
                (void)peek();
                const auto start = position_;
                double result = product(depth);
                while (status_)
                {
                    const char op = peek();
                    if (op != '+' && op != '-') break;
                    const auto at = position_++;
                    const double right = product(depth);
                    result = step(start, binary(op, result, right, at));
                }
                return result;
            }

            [[nodiscard]] double product(std::size_t depth) noexcept
            {
                (void)peek();
                const auto start = position_;
                double result = unary(depth);
                while (status_)
                {
                    const char op = peek();
                    if (op != '*' && op != '/') break;
                    const auto at = position_++;
                    const double right = unary(depth);
                    result = step(start, binary(op, result, right, at));
                }
                return result;
            }

            [[nodiscard]] double unary(std::size_t depth) noexcept
            {
                const char next = peek();
                const auto start = position_;
                if (depth > maximum_expression_depth)
                    return fail(CalculationError::expression_too_deep, position_);
                if (next == '+' || next == '-')
                {
                    ++position_;
                    const double value = unary(depth + 1u);
                    return step(start, next == '-' ? -value : value);
                }

                // Powers bind before unary signs and associate to the right.
                const double base = primary(depth);
                if (!status_ || peek() != '^') return base;
                const auto at = position_++;
                const double exponent = unary(depth + 1u);
                if (!status_) return 0.0;
                if (base == 0.0 && exponent < 0.0) return fail(CalculationError::divide_by_zero, at);
                return step(start, finite_result(std::pow(base, exponent), at));
            }

            [[nodiscard]] double primary(std::size_t depth) noexcept
            {
                const char next = peek();
                if (next == '(') return group(depth);
                if (is_letter(next)) return identifier(depth);
                if (is_digit(next) || next == '.') return number();
                return fail(CalculationError::expected_value, position_);
            }

            [[nodiscard]] double group(std::size_t depth) noexcept
            {
                ++position_;
                const double value = sum(depth + 1u);
                if (!status_) return 0.0;
                if (peek() != ')') return fail(CalculationError::expected_closing_parenthesis, position_);
                ++position_;
                return value;
            }

            [[nodiscard]] double number() noexcept
            {
                const auto at = position_;
                const auto* start = text_.data();
                const auto* end = start + text_.size();
                double value{};
                const auto read = std::from_chars(start + at, end, value);
                if (read.ec == std::errc::result_out_of_range) return fail(CalculationError::out_of_range, at);
                if (read.ec != std::errc{} || (read.ptr != end && (*read.ptr == 'e' || *read.ptr == 'E')))
                    return fail(CalculationError::invalid_number, at);
                position_ = static_cast<std::size_t>(read.ptr - start);
                return value;
            }

            enum class Function { sin, cos, tan, asin, acos, atan, sqrt, abs, ln, log10, exp };
            struct NamedFunction final { std::string_view name; Function function; };

            [[nodiscard]] double identifier(std::size_t depth) noexcept
            {
                const auto at = position_;
                do { ++position_; }
                while (position_ < text_.size() && (is_letter(text_[position_]) || is_digit(text_[position_])));
                const auto name = text_.substr(at, position_ - at);
                if (name == "pi") return pi;
                if (name == "e") return std::numbers::e_v<double>;

                static constexpr std::array functions{
                    NamedFunction{"sin", Function::sin}, NamedFunction{"cos", Function::cos},
                    NamedFunction{"tan", Function::tan}, NamedFunction{"asin", Function::asin},
                    NamedFunction{"acos", Function::acos}, NamedFunction{"atan", Function::atan},
                    NamedFunction{"sqrt", Function::sqrt}, NamedFunction{"abs", Function::abs},
                    NamedFunction{"ln", Function::ln}, NamedFunction{"log", Function::log10},
                    NamedFunction{"log10", Function::log10}, NamedFunction{"exp", Function::exp}};
                for (const auto& entry : functions)
                {
                    if (entry.name != name) continue;
                    if (peek() != '(') return fail(CalculationError::expected_opening_parenthesis, position_);
                    const double argument = group(depth);
                    return status_ ? step(at, apply(entry.function, argument, at)) : 0.0;
                }
                return fail(CalculationError::unknown_identifier, at);
            }

            [[nodiscard]] double finite_result(double value, std::size_t at) noexcept
            {
                if (std::isnan(value)) return fail(CalculationError::domain, at);
                if (!std::isfinite(value)) return fail(CalculationError::out_of_range, at);
                return value;
            }

            [[nodiscard]] double angle_radians(double value) const noexcept
            {
                return angle_ == AngleUnit::radians ? value : std::remainder(value, 360.0) * (pi / 180.0);
            }

            [[nodiscard]] double angle_from_radians(double value) const noexcept
            {
                return angle_ == AngleUnit::radians ? value : value * (180.0 / pi);
            }

            [[nodiscard]] double apply(Function function, double value, std::size_t at) noexcept
            {
                switch (function)
                {
                case Function::sin: return std::sin(angle_radians(value));
                case Function::cos: return std::cos(angle_radians(value));
                case Function::tan:
                {
                    const double radians = angle_radians(value);
                    if (std::abs(std::cos(radians)) <= std::numeric_limits<double>::epsilon())
                        return fail(CalculationError::domain, at);
                    return finite_result(std::tan(radians), at);
                }
                case Function::asin: return finite_result(angle_from_radians(std::asin(value)), at);
                case Function::acos: return finite_result(angle_from_radians(std::acos(value)), at);
                case Function::atan: return angle_from_radians(std::atan(value));
                case Function::sqrt: return finite_result(std::sqrt(value), at);
                case Function::abs: return std::abs(value);
                case Function::ln: case Function::log10:
                    if (value <= 0.0) return fail(CalculationError::domain, at);
                    return function == Function::ln ? std::log(value) : std::log10(value);
                case Function::exp: return finite_result(std::exp(value), at);
                }
                return fail(CalculationError::unknown_identifier, at);
            }
        };
    }

    // The restricted grammar accepts numbers, named constants, parentheses,
    // unary +/-, + - * / ^ and the unary functions admitted above.
    [[nodiscard]] inline CalculationResult evaluate_expression(std::string_view expression,
        AngleUnit angle = AngleUnit::degrees, CalculationTrace* trace = nullptr) noexcept
    {
        if (trace) trace->count = 0;
        const auto result = detail::ExpressionParser{expression, angle, trace}.evaluate();
        if (trace && !result) trace->count = 0;
        return result;
    }
}
