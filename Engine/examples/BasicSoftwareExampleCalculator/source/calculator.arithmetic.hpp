// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>

namespace calculator_example
{
    inline constexpr std::string_view display_widget = "CalculatorDisplay";
    inline constexpr std::size_t display_limit = 32u;

    struct Key final
    {
        std::string_view label;
        std::string_view action;
        char input;
    };

    inline constexpr std::array keys{
        Key{"Clear", "calculator.clear", 'c'},
        Key{"+/-", "calculator.sign", 's'},
        Key{"Back", "calculator.backspace", 'b'},
        Key{"/", "calculator.divide", '/'},
        Key{"7", "calculator.7", '7'}, Key{"8", "calculator.8", '8'}, Key{"9", "calculator.9", '9'},
        Key{"x", "calculator.multiply", '*'},
        Key{"4", "calculator.4", '4'}, Key{"5", "calculator.5", '5'}, Key{"6", "calculator.6", '6'},
        Key{"-", "calculator.subtract", '-'},
        Key{"1", "calculator.1", '1'}, Key{"2", "calculator.2", '2'}, Key{"3", "calculator.3", '3'},
        Key{"+", "calculator.add", '+'},
        Key{"0", "calculator.0", '0'}, Key{".", "calculator.decimal", '.'},
        Key{"=", "calculator.equals", '='}};

    [[nodiscard]] constexpr const Key* find_key(std::string_view action) noexcept
    {
        for (const auto& key : keys)
            if (key.action == action) return &key;
        return nullptr;
    }

    enum class CalculationError
    {
        none, divide_by_zero, out_of_range, domain, expected_value, expected_opening_parenthesis,
        expected_closing_parenthesis, unknown_identifier, invalid_number, unexpected_token,
        expression_too_long, expression_too_deep
    };

    struct CalculationResult final
    {
        double value{};
        CalculationError error{};
        std::size_t position{};

        [[nodiscard]] explicit operator bool() const noexcept { return error == CalculationError::none; }
    };

    [[nodiscard]] constexpr std::string_view calculation_error_message(CalculationError error) noexcept
    {
        switch (error)
        {
        case CalculationError::none: return {};
        case CalculationError::divide_by_zero: return "Cannot divide by zero";
        case CalculationError::out_of_range: return "Result out of range";
        case CalculationError::domain: return "Outside the function domain";
        case CalculationError::expected_value: return "Expected a value";
        case CalculationError::expected_opening_parenthesis: return "Expected ( after function";
        case CalculationError::expected_closing_parenthesis: return "Expected )";
        case CalculationError::unknown_identifier: return "Unknown function or constant";
        case CalculationError::invalid_number: return "Invalid number";
        case CalculationError::unexpected_token: return "Unexpected token";
        case CalculationError::expression_too_long: return "Expression too long";
        case CalculationError::expression_too_deep: return "Expression nested too deeply";
        }
        return {};
    }

    [[nodiscard]] inline bool format_calculation_value(double value, std::string& output)
    {
        if (!std::isfinite(value)) return false;
        std::array<char, display_limit> result{};
        const auto converted = std::to_chars(result.data(), result.data() + result.size(),
            value, std::chars_format::general, 14);
        if (converted.ec != std::errc{}) return false;
        output.assign(result.data(), converted.ptr);
        return true;
    }

    namespace detail
    {
        [[nodiscard]] inline CalculationResult apply_basic_operation(char operation, double left, double right) noexcept
        {
            double result = right;
            switch (operation)
            {
            case '+': result = left + right; break;
            case '-': result = left - right; break;
            case '*': result = left * right; break;
            case '/':
                if (right == 0.0) return {.error = CalculationError::divide_by_zero};
                result = left / right;
                break;
            }
            if (!std::isfinite(result)) return {.error = CalculationError::out_of_range};
            return {.value = result};
        }
    }

    class Calculator final
    {
    public:
        [[nodiscard]] const std::string& display() const noexcept { return display_; }

        void press(const Key& key)
        {
            const char input = key.input;
            if (input >= '0' && input <= '9')
            {
                start_entry();
                if (display_ == "0" || display_ == "-0") display_.back() = input;
                else if (display_.size() < maximum_entry_size) display_.push_back(input);
                return;
            }

            switch (input)
            {
            case '.':
                start_entry();
                if (display_.size() < maximum_entry_size && display_.find('.') == std::string::npos)
                    display_.push_back('.');
                break;
            case 's':
                if (error_ || operation_ || display_ == "0") start_entry();
                if (display_.starts_with('-')) display_.erase(0u, 1u);
                else display_.insert(display_.begin(), '-');
                if (!editing_) accumulator_ = read_entry();
                break;
            case 'b':
                if (editing_)
                {
                    display_.pop_back();
                    if (display_.empty() || display_ == "-") display_ = "0";
                }
                break;
            case 'c':
                *this = {};
                break;
            case '+': case '-': case '*': case '/':
                if (!error_ && (!editing_ || commit_entry())) operation_ = input;
                break;
            case '=':
                if (editing_ && commit_entry()) operation_ = {};
                break;
            }
        }

    private:
        static constexpr std::size_t maximum_entry_size = 16u;
        std::string display_{"0"};
        double accumulator_{};
        char operation_{};
        bool editing_{};
        bool error_{};

        void start_entry()
        {
            if (editing_) return;
            if (error_) *this = {};
            display_ = "0";
            editing_ = true;
        }

        [[nodiscard]] double read_entry() const noexcept
        {
            double value{};
            const auto* end = display_.data() + display_.size();
            const auto parsed = std::from_chars(display_.data(), end, value);
            return parsed.ec == std::errc{} && parsed.ptr == end ? value : 0.0;
        }

        [[nodiscard]] bool fail(std::string_view message)
        {
            display_ = message;
            editing_ = false;
            error_ = true;
            return false;
        }

        [[nodiscard]] bool commit_entry()
        {
            const auto calculated = detail::apply_basic_operation(operation_, accumulator_, read_entry());
            if (!calculated) return fail(calculation_error_message(calculated.error));
            if (!format_calculation_value(calculated.value, display_))
                return fail(calculation_error_message(CalculationError::out_of_range));
            accumulator_ = calculated.value;
            editing_ = false;
            return true;
        }
    };
}
