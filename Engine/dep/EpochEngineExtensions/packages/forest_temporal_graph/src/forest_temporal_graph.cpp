// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/forest_temporal_graph/forest_temporal_graph.hpp"

#include <cmath>
#include <numbers>

namespace epoch::extensions::forest_temporal_graph
{
    namespace
    {
        struct Turtle
        {
            Vec2 position{};
            float headingRadians{std::numbers::pi_v<float> * 0.5F};
            std::uint32_t depth{};
        };

        [[nodiscard]] std::string_view replacement_for(char symbol, std::span<const Rule> rules) noexcept
        {
            for (const auto& rule : rules)
            {
                if (rule.symbol == symbol)
                {
                    return rule.replacement;
                }
            }
            return {};
        }
    }

    std::string expand(const Grammar& grammar, std::uint32_t iterations)
    {
        auto current = grammar.axiom;
        for (std::uint32_t iteration = 0; iteration < iterations; ++iteration)
        {
            std::string next;
            next.reserve(current.size() * 2U);
            for (const auto symbol : current)
            {
                const auto replacement = replacement_for(symbol, grammar.rules);
                if (replacement.empty())
                {
                    next.push_back(symbol);
                }
                else
                {
                    next.append(replacement);
                }
            }
            current = std::move(next);
        }
        return current;
    }

    std::vector<BranchSegment> build_branch_segments(std::string_view program, TurtleSettings settings)
    {
        std::vector<BranchSegment> segments;
        std::vector<Turtle> stack;
        Turtle turtle{};
        const auto turn = settings.turnDegrees * (std::numbers::pi_v<float> / 180.0F);

        for (const auto symbol : program)
        {
            switch (symbol)
            {
                case 'F':
                {
                    const Vec2 next{
                        turtle.position.x + std::cos(turtle.headingRadians) * settings.stepLength,
                        turtle.position.y + std::sin(turtle.headingRadians) * settings.stepLength
                    };
                    segments.push_back(BranchSegment{turtle.position, next, turtle.depth});
                    turtle.position = next;
                    break;
                }
                case '+':
                    turtle.headingRadians += turn;
                    break;
                case '-':
                    turtle.headingRadians -= turn;
                    break;
                case '[':
                    stack.push_back(turtle);
                    ++turtle.depth;
                    break;
                case ']':
                    if (!stack.empty())
                    {
                        turtle = stack.back();
                        stack.pop_back();
                    }
                    break;
                default:
                    break;
            }
        }

        return segments;
    }
}
