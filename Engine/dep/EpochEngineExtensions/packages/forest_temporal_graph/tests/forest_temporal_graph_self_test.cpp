// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/forest_temporal_graph/forest_temporal_graph.hpp"

#include <stdexcept>

using namespace epoch::extensions::forest_temporal_graph;

int main()
{
    Grammar grammar;
    grammar.axiom = "F";
    grammar.rules.push_back(Rule{'F', "F[+F]F[-F]F"});

    const auto program = expand(grammar, 2);
    if (program.size() <= grammar.axiom.size())
    {
        throw std::runtime_error("forest temporal graph expansion did not grow");
    }

    const auto segments = build_branch_segments(program, TurtleSettings{1.0F, 22.5F});
    if (segments.size() < 5U)
    {
        throw std::runtime_error("forest temporal graph segment generation produced too few segments");
    }

    return 0;
}
