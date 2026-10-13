// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <stop_token>

#include <nlohmann/json.hpp>

#include "ai.backend_addons_addon_registry.hpp"
#include "ai.backend_controller_task_intent.hpp"
#include "ai.backend_core_config.hpp"
#include "ai.backend_model_model_provider.hpp"
#include "ai.backend_models_model_registry.hpp"
#include "ai.backend_residency_model_residency_manager.hpp"

namespace epoch_ai::controller {

using json = nlohmann::json;

struct ControllerDecision {
    std::optional<std::string> addon_id;
    double confidence{1.0};
    bool retrieve_context{};
    std::string source;
    std::string reason;
    std::string model;
    bool model_attempted{};
    bool model_used{};
    std::string model_fallback_reason;
    TaskIntent task_intent{TaskIntent::complex};
    std::string shadow_intent;
    double shadow_confidence{};
};

class ArchitectController {
public:
    ArchitectController(
        const core::Config& config,
        model::IModelProvider& model,
        addons::AddonRegistry& addons,
        models::ModelRegistry& models,
        residency::ModelResidencyManager& residency);

    [[nodiscard]] ControllerDecision decide(std::string_view request, std::string_view model_override = {}, std::stop_token stop = {}, const json& addon_models = json::object());
    [[nodiscard]] json describe() const;

private:
    [[nodiscard]] std::optional<ControllerDecision> learned_decision(std::string_view request, std::string_view model_override, std::stop_token stop, const json& addon_models, std::string& failure);
    [[nodiscard]] ControllerDecision fallback_decision(std::string_view request, const json& addon_models) const;

    const core::Config& config_;
    model::IModelProvider& model_;
    addons::AddonRegistry& addons_;
    models::ModelRegistry& models_;
    residency::ModelResidencyManager& residency_;
    PredictiveRouteShadow shadow_;
};

} // namespace epoch_ai::controller
