// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace epoch::extensions::forest_temporal_graph
{
    struct Rule
    {
        char symbol{};
        std::string replacement;
    };

    struct Grammar
    {
        std::string axiom{"F"};
        std::vector<Rule> rules;
    };

    struct TemporalGraphSettings
    {
        std::uint32_t iterations{2};
        float seconds{}; // Timeline position for future animated growth profiles.
    };

    struct TurtleSettings
    {
        float stepLength{1.0F};
        float turnDegrees{25.0F};
    };

    struct Vec2
    {
        float x{};
        float y{};
    };

    struct BranchSegment
    {
        Vec2 from{};
        Vec2 to{};
        std::uint32_t depth{};
    };

    [[nodiscard]] std::string expand(const Grammar& grammar, std::uint32_t iterations);
    [[nodiscard]] std::vector<BranchSegment> build_branch_segments(std::string_view program, TurtleSettings settings);
}
