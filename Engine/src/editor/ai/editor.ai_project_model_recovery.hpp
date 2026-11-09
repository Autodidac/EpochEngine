// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "../../ai/ai.model_behavior.hpp"

namespace epochengine::editor_ai_project_model_recovery
{
    // A rejected plan is a request-level observation, not a capability score.
    // Transport, cancellation and host prerequisites must never count as it.
    enum class Outcome { rejected_plan, transport_failure, cancelled, host_requirement };
    struct Model final
    {
        std::string_view id{}, uri{};
        bool resident{}, behavior_flagged{};
    };
    struct Role final { std::string_view name{}, model{}; };
    struct Binding final
    {
        std::string project_id{}, project_root{};
        std::uint64_t project_generation{}, scene_revision{}, gui_revision{};
        std::uint64_t completion_generation{}, model_selection_revision{};
        friend bool operator==(const Binding&, const Binding&) = default;
    };
    struct Pending final
    {
        Binding binding{};
        std::string request{}, failed_model{}, target_model{}, target_role{};
        bool resident{};
        [[nodiscard]] bool current(const Binding& value) const noexcept
        { return !request.empty() && binding == value; }
    };
    [[nodiscard]] inline const Model* find_model(std::span<const Model> models,
        std::string_view identity) noexcept
    {
        if (identity.empty()) return nullptr;
        for (const auto& model : models)
            if (model.id == identity || (!model.uri.empty() && model.uri == identity)) return &model;
        return nullptr;
    }
    [[nodiscard]] inline Pending offer(Outcome outcome, std::string request,
        Binding binding, std::string_view actual_model,
        std::span<const Model> models, std::span<const Role> roles)
    {
        if (outcome != Outcome::rejected_plan || request.empty()) return {};
        const auto* failed = find_model(models, actual_model);
        // A provider display label or unrelated last-response hint cannot bind
        // failure to a model. The host's canonical identity must resolve exactly.
        if (!failed) return {};
        Pending pending{std::move(binding), std::move(request), std::string{failed->id}};
        constexpr std::array<std::string_view, 3> eligible_roles{
            "reviewer", "implementation", "foundation"};
        // Prefer an already loaded role assignment. No parameter-count ranking,
        // downloads, preparation, inference or paid-provider fallback occurs.
        for (const bool resident : {true, false})
            for (const auto name : eligible_roles)
                for (const auto& role : roles)
                {
                    if (role.name != name) continue;
                    const auto* model = find_model(models, role.model);
                    if (!model || model->id.empty() || model->id == failed->id
                        || model->resident != resident || model->behavior_flagged) continue;
                    pending.target_model = model->id;
                    pending.target_role = name;
                    pending.resident = resident;
                    return pending;
                }
        return pending;
    }
    [[nodiscard]] inline bool target_ready(const Pending& pending,
        std::span<const Model> models, std::span<const Role> roles) noexcept
    {
        const auto* target = find_model(models, pending.target_model);
        if (!target || !target->resident || target->behavior_flagged) return false;
        for (const auto& role : roles)
            if (role.name == pending.target_role && find_model(models, role.model) == target) return true;
        return false;
    }
    [[nodiscard]] inline std::string disclosure(const Pending& pending)
    {
        if (pending.request.empty()) return {};
        std::string text = "The plan from " + pending.failed_model
            + " was rejected before execution. This does not establish the model's general ability. ";
        if (pending.target_model.empty())
            return text + "Choose a local model in Model Manager to retry. The request is retained; nothing was sent or loaded.";
        text += "Alternative: " + pending.target_model + " (" + pending.target_role + "). ";
        return text + (pending.resident
            ? "Prepare Local Retry selects this already loaded model and puts the same request back in the composer. Review it and press Send; tool approval is still required."
            : "This assigned model is not loaded. Choose Model opens Model Manager; loading still requires your normal approval. The request is retained.");
    }
    template<class Snapshot>
    [[nodiscard]] inline auto native_views(const Snapshot& snapshot)
    {
        std::pair<std::vector<Model>, std::vector<Role>> views;
        for (const auto& model : snapshot.models)
            views.first.push_back({model.id, model.uri, model.active,
                ai::model_behavior::classify_text(model.id, model.behavior).flagged()});
        for (const auto& role : snapshot.roles)
            if (role.role == "assignment") views.second.push_back({role.id, role.uri});
        return views;
    }
    template<class State>
    [[nodiscard]] inline Binding current_binding(const State& editor,
        std::uint64_t completion_generation)
    {
        return {editor.projectId, editor.projectRoot,
            editor.projectLifecycle.evidence().generations.selected_project.generation,
            editor.sceneDocumentRevision,
            editor.guiDocument ? editor.guiDocument->revision().sequence : 0u,
            completion_generation, editor.aiChatUserModelSelectionRevision};
    }
    template<class Snapshot>
    [[nodiscard]] inline Pending offer_native(std::string request, Binding binding,
        std::string_view actual_model, const Snapshot& snapshot)
    {
        const auto views = native_views(snapshot);
        return offer(Outcome::rejected_plan, std::move(request), std::move(binding),
            actual_model, views.first, views.second);
    }
    template<class Snapshot>
    [[nodiscard]] inline bool target_ready_native(const Pending& pending, const Snapshot& snapshot)
    {
        if (snapshot.busy) return false;
        const auto views = native_views(snapshot);
        return target_ready(pending, views.first, views.second);
    }
}
