/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

module editor.project_input_settings;

import project.input_profile;

namespace epochengine::editor_project_input_settings
{
    namespace
    {
        [[nodiscard]] project_input::BindingId keyboard_binding(
            const project_input::ProfileSource& source,
            project_input::ActionSemantic semantic,
            project_input::KeyCode key) noexcept
        {
            const auto action = std::find_if(
                source.actions.begin(), source.actions.end(),
                [semantic](const auto& candidate) noexcept
                {
                    return candidate.semantic == semantic;
                });
            if (action == source.actions.end())
                return {};
            const auto binding = std::find_if(
                source.bindings.begin(), source.bindings.end(),
                [&](const auto& candidate) noexcept
                {
                    return candidate.action == action->id
                        && candidate.device
                            == project_input::BindingDevice::keyboard
                        && candidate.code == static_cast<std::uint16_t>(key);
                });
            return binding == source.bindings.end()
                ? project_input::BindingId{} : binding->id;
        }
    }

    ContractFailure run_contract() noexcept
    {
        Controller controller{};
        const auto defaults = project_input::make_legacy_default_profile();

        if (controller.load({}, defaults).code
            != SettingsCode::invalid_project)
        {
            return ContractFailure::invalid_load;
        }
        if (!controller.load("project:input-settings", defaults))
            return ContractFailure::source_load;

        const auto initial = controller.snapshot();
        if (!initial.loaded || initial.dirty
            || initial.project_id != "project:input-settings"
            || initial.action_count != defaults.actions.size()
            || initial.binding_count != defaults.bindings.size()
            || initial.bindings.size() != defaults.bindings.size()
            || !initial.uniform_controller_dead_zone
            || !controller.working_source())
        {
            return ContractFailure::snapshot;
        }

        const auto jump = keyboard_binding(
            defaults,
            project_input::ActionSemantic::jump,
            project_input::KeyCode::space);
        const auto moveUp = keyboard_binding(
            defaults,
            project_input::ActionSemantic::move_y,
            project_input::KeyCode::w);
        if (!jump || !moveUp)
            return ContractFailure::snapshot;

        const auto rejected = controller.rebind_keyboard(
            jump, project_input::KeyCode::w);
        if (rejected.code != SettingsCode::binding_conflict
            || rejected.validation
                != project_input::ValidationCode::duplicate_binding_source
            || rejected.conflicts.size() != 1u
            || rejected.conflicts.front().binding != jump
            || rejected.conflicts.front().conflicting_binding != moveUp)
        {
            return ContractFailure::conflict_evidence;
        }
        if (controller.dirty()
            || !controller.working_source()
            || *controller.working_source() != defaults)
        {
            return ContractFailure::non_destructive_conflict;
        }

        const auto changed = controller.rebind_keyboard(
            jump, project_input::KeyCode::enter);
        if (changed.code != SettingsCode::ready
            || !controller.dirty()
            || !controller.working_source()
            || controller.working_source()->revision.sequence != 2u)
        {
            return ContractFailure::staged_edit;
        }

        const SavePlan plan = controller.prepare_save();
        if (!plan
            || plan.project_id != "project:input-settings"
            || plan.source_path != project_input::canonical_source_path
            || plan.artifact_path != project_input::canonical_artifact_path
            || plan.expected_base_revision != defaults.revision
            || plan.revision != controller.working_source()->revision
            || plan.source != *controller.working_source()
            || plan.artifact.source_revision != plan.revision)
        {
            return ContractFailure::save_plan;
        }

        const auto decodedSource = project_input::deserialize_profile_source(
            std::span<const std::byte>{plan.source_bytes});
        const auto decodedArtifact =
            project_input::deserialize_compiled_profile(
                std::span<const std::byte>{plan.artifact_bytes});
        if (!decodedSource || decodedSource.source != plan.source
            || !decodedArtifact
            || decodedArtifact.artifact != plan.artifact)
        {
            return ContractFailure::save_round_trip;
        }

        if (controller.confirm_saved(defaults.revision).code
            != SettingsCode::stale_revision
            || !controller.dirty())
        {
            return ContractFailure::stale_confirmation;
        }
        if (!controller.confirm_saved(plan.revision)
            || controller.dirty()
            || controller.prepare_save().code != SettingsCode::unchanged)
        {
            return ContractFailure::confirmation;
        }

        constexpr std::uint16_t deadZone = 7'200u;
        if (!controller.set_controller_dead_zone(deadZone)
            || !controller.dirty())
        {
            return ContractFailure::dead_zone;
        }
        const auto deadZoneSnapshot = controller.snapshot();
        if (!deadZoneSnapshot.uniform_controller_dead_zone
            || deadZoneSnapshot.controller_dead_zone_q15 != deadZone)
        {
            return ContractFailure::dead_zone;
        }
        if (!controller.discard() || controller.dirty()
            || controller.snapshot().controller_dead_zone_q15 == deadZone)
        {
            return ContractFailure::discard;
        }

        if (!controller.reset_defaults() || !controller.dirty())
            return ContractFailure::reset;
        const SavePlan resetPlan = controller.prepare_save();
        if (!resetPlan || resetPlan.revision.sequence != 3u)
            return ContractFailure::reset;

        Controller restored{};
        if (!restored.load_serialized(
                "project:input-settings",
                std::span<const std::byte>{resetPlan.source_bytes})
            || restored.dirty()
            || !restored.working_source()
            || *restored.working_source() != resetPlan.source)
        {
            return ContractFailure::serialized_load;
        }
        return ContractFailure::none;
    }
}

#if defined(EPOCH_EDITOR_PROJECT_INPUT_SETTINGS_CONTRACT_MAIN)
int main()
{
    return static_cast<int>(
        epochengine::editor_project_input_settings::run_contract());
}
#endif
