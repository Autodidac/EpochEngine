"""The saved calculator's scientific grammar and shared arithmetic on the CPU."""
import hashlib
import json
from pathlib import Path
import unittest

from Tests.support.cpp import compile_and_run


ROOT = Path(__file__).resolve().parents[2]
PROJECT_SOURCE = ROOT / "Engine/examples/BasicSoftwareExampleCalculator/source"

CONSUMER = r'''
#include "calculator.scientific.hpp"
#include <cassert>
#include <cmath>
#include <string>

using namespace calculator_example;

void expect(std::string_view expression, double expected, AngleUnit unit = AngleUnit::degrees)
{
    const auto result = evaluate_expression(expression, unit);
    assert(result && std::abs(result.value - expected) < 1e-12);
}

void reject(std::string_view expression, CalculationError error, AngleUnit unit = AngleUnit::degrees)
{
    const auto result = evaluate_expression(expression, unit);
    assert(!result && result.value == 0.0 && result.error == error);
    assert(result.position <= expression.size());
    assert(!calculation_error_message(result.error).empty());
}

int main()
{
    expect("2+3*4", 14);
    expect("(2+3)*4", 20);
    expect("8 / 2 / 2 - 3 - 1", -2);
    expect("2^3^2", 512);
    expect("-2^2", -4);
    expect("(-2)^2", 4);
    expect("2^-2", .25);
    expect("2^-2^2", .0625);
    expect("2--3", 5);
    expect("\t.5 + 1. + 1e-2 - 2E-3\r\n", 1.508);
    expect("sqrt(abs(-49)) + ln(e) + log(100) + log10(10) + exp(0)", 12);
    expect("sin(30) + cos(60) + tan(45)", 2);
    expect("sin(720000030)", .5);
    expect("sin(pi/2)+cos(pi)+tan(pi/4)", 1, AngleUnit::radians);
    expect("asin(.5)+acos(.5)+atan(1)", 135);
    expect("asin(.5)+acos(.5)+atan(1)", std::numbers::pi_v<double> * .75, AngleUnit::radians);

    reject("", CalculationError::expected_value);
    reject("(2+3", CalculationError::expected_closing_parenthesis);
    reject("sin pi", CalculationError::expected_opening_parenthesis);
    reject("sin()", CalculationError::expected_value);
    reject("reference(4)", CalculationError::unknown_identifier);
    reject("nan", CalculationError::unknown_identifier);
    reject(".e2", CalculationError::invalid_number);
    reject("2e+", CalculationError::invalid_number);
    reject("2(3)", CalculationError::unexpected_token);
    reject("1; 2", CalculationError::unexpected_token);
    reject(std::string_view{"1\0+2", 4u}, CalculationError::unexpected_token);
    reject("1e309", CalculationError::out_of_range);
    reject("1e308*2", CalculationError::out_of_range);
    reject("exp(1000)", CalculationError::out_of_range);
    reject("2^1024", CalculationError::out_of_range);
    reject("sqrt(-1)", CalculationError::domain);
    reject("(-2)^.5", CalculationError::domain);
    reject("log(0)", CalculationError::domain);
    reject("asin(2)", CalculationError::domain);
    reject("tan(90)", CalculationError::domain);
    reject("tan(pi/2)", CalculationError::domain, AngleUnit::radians);
    reject("0^(-1)", CalculationError::divide_by_zero);
    reject("1/(-0.0)", CalculationError::divide_by_zero);
    const auto division = evaluate_expression("12 + 4 / 0");
    assert(!division && division.position == 7u);

    expect(std::string(maximum_expression_length - 1u, ' ') + "1", 1);
    reject(std::string(maximum_expression_length, ' ') + "1", CalculationError::expression_too_long);
    expect(std::string(maximum_expression_depth, '(') + "1" + std::string(maximum_expression_depth, ')'), 1);
    reject(std::string(maximum_expression_depth + 1u, '-') + "1", CalculationError::expression_too_deep);

    Calculator basic;
    for (auto action : {"calculator.8", "calculator.divide", "calculator.0", "calculator.equals"})
        basic.press(*find_key(action));
    assert(basic.display() == calculation_error_message(evaluate_expression("8/0").error));
    for (auto action : {"calculator.9", "calculator.divide", "calculator.4", "calculator.equals"})
        basic.press(*find_key(action));
    assert(basic.display() == "2.25");
    expect("9/4", 2.25);
    std::string formatted;
    const auto divisionResult = evaluate_expression("1/3");
    assert(divisionResult && format_calculation_value(divisionResult.value, formatted));
    assert(formatted == "0.33333333333333");
    assert(!format_calculation_value(std::numeric_limits<double>::infinity(), formatted));
    assert(formatted == "0.33333333333333");

    CalculationTrace trace;
    constexpr std::string_view worked = "  sin(30) + sqrt(16) * 2^3  ";
    const auto observed = evaluate_expression(worked, AngleUnit::degrees, &trace);
    assert(observed && observed.value == 32.5 && trace.count == 5u);
    const auto expression_at = [&](std::size_t index) {
        const auto& step = trace.steps[index];
        return worked.substr(step.begin, step.end - step.begin);
    };
    assert(expression_at(0) == "sin(30)" && std::abs(trace.steps[0].value - .5) < 1e-12);
    assert(expression_at(1) == "sqrt(16)" && trace.steps[1].value == 4);
    assert(expression_at(2) == "2^3" && trace.steps[2].value == 8);
    assert(expression_at(3) == "sqrt(16) * 2^3" && trace.steps[3].value == 32);
    assert(expression_at(4) == "sin(30) + sqrt(16) * 2^3" && trace.steps[4].value == 32.5);
    assert(!evaluate_expression("sqrt(16) + 1 / 0", AngleUnit::degrees, &trace) && trace.count == 0);
    assert(evaluate_expression("5", AngleUnit::degrees, &trace) && trace.count == 0);
    const auto negativePower = evaluate_expression(" -2^3^2 ", AngleUnit::degrees, &trace);
    assert(negativePower && negativePower.value == -512 && trace.count == 3u);
    assert(trace.steps[0].begin == 4u && trace.steps[0].value == 9);
    assert(trace.steps[1].begin == 2u && trace.steps[1].value == 512);
    assert(trace.steps[2].begin == 1u && trace.steps[2].value == -512);
}
'''


class CalculatorScientific(unittest.TestCase):
    def test_expression_and_shared_basic_arithmetic(self):
        report = ROOT / "Artifacts/Reports/calculator-scientific"
        report.mkdir(parents=True, exist_ok=True)
        source = report / "calculator.scientific_contract.cpp"
        source.write_text(CONSUMER, encoding="utf-8")
        runs = compile_and_run(self, source, report, includes=(PROJECT_SOURCE,))
        sources = (PROJECT_SOURCE / filename for filename in ("calculator.arithmetic.hpp", "calculator.scientific.hpp"))
        (report / "result.json").write_text(json.dumps({
            "runs": runs,
            "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in sources},
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
