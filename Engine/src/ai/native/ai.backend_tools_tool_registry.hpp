// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once
#include <expected>
#include <string>
#include <optional>
#include <unordered_map>
#include <vector>
#include "ai.backend_decision_decision_engine.hpp"
#include "ai.backend_tools_tool.hpp"

namespace epoch_ai::tools {

struct Execution {
    json value;
    decision::ToolReview review;
};

class ToolRegistry {
public:
    explicit ToolRegistry(decision::IDecisionEngine& decisions);
    void add(Tool tool);
    [[nodiscard]] std::vector<json> model_definitions() const;
    [[nodiscard]] std::expected<decision::ToolReview, core::Error> review(
        std::string_view name,
        const json& arguments,
        const Context& context) const;
    [[nodiscard]] std::expected<json, core::Error> invoke(
        std::string_view name,
        const json& arguments,
        const Context& context) const;
    [[nodiscard]] std::expected<Execution, core::Error> execute(
        std::string_view name,
        const json& arguments,
        const Context& context) const;
private:
    decision::IDecisionEngine& decisions_;
    std::unordered_map<std::string, Tool> tools_;
};

} // namespace epoch_ai::tools
