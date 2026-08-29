/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

module editor.project_input_settings;

import input.controller;
import project.input_controller;
import project.input_profile;

namespace epochengine::editor_project_input_settings
{
    namespace
    {
        namespace fs = std::filesystem;

        [[nodiscard]] std::uint64_t next_contract_id() noexcept
        {
            static std::atomic<std::uint64_t> next{1u};
            std::uint64_t value = next.fetch_add(
                1u, std::memory_order_relaxed);
            if (value == 0u)
                value = next.fetch_add(1u, std::memory_order_relaxed);
            return value;
        }

        struct ContractRoot final
        {
            fs::path path{};

            ContractRoot()
            {
                std::error_code error{};
                path = fs::temp_directory_path(error);
                if (error)
                {
                    path.clear();
                    return;
                }
                path /= "epoch_editor_project_input_settings_contract_"
                    + std::to_string(next_contract_id());
                fs::remove_all(path, error);
                error.clear();
                fs::create_directories(path, error);
                if (error)
                    path.clear();
            }

            ~ContractRoot()
            {
                if (path.empty())
                    return;
                std::error_code error{};
                fs::remove_all(path, error);
            }

            [[nodiscard]] bool valid() const noexcept
            {
                return !path.empty();
            }
        };

        [[nodiscard]] controller_input::Snapshot controller_snapshot(
            std::uint64_t revision,
            std::int32_t leftX) noexcept
        {
            controller_input::Snapshot snapshot{};
            snapshot.revision = revision;
            snapshot.provider_available = true;
            auto& device = snapshot.devices[0];
            device.handle = {0u, 1u};
            device.connected = true;
            device.provider_id = 41u;
            device.axes[static_cast<std::size_t>(
                controller_input::Axis::left_x)] = leftX;
            return snapshot;
        }

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

        ContractRoot root{};
        if (!root.valid())
            return ContractFailure::paired_persistence;
        project_input::ProjectInputProfileStore store{
            plan.project_id, root.path};
        if (!store.publish_source_and_artifact(
                plan.source, plan.artifact))
        {
            return ContractFailure::paired_persistence;
        }
        project_input::ProjectInputProfileStore reopenedStore{
            plan.project_id, root.path};
        const auto persistedSource = reopenedStore.load_source();
        const auto persistedArtifact = reopenedStore.load_artifact();
        if (!persistedSource || persistedSource.source != plan.source
            || !persistedArtifact
            || persistedArtifact.artifact != plan.artifact)
        {
            return ContractFailure::paired_persistence;
        }

        project_input::InputSnapshot keyboardInput{.frame_index = 1u};
        keyboardInput.keyboard.push_back({
            project_input::KeyCode::enter, true, true});
        const auto keyboardFrame = project_input::evaluate_action_frame(
            persistedArtifact.artifact, keyboardInput);
        const auto* persistedJump = project_input::find_action(
            keyboardFrame,
            project_input::ActionSemantic::jump,
            persistedArtifact.artifact);
        if (!keyboardFrame || !persistedJump || !persistedJump->pressed
            || persistedJump->value_q15
                != project_input::normalized_unit)
        {
            return ContractFailure::persisted_keyboard;
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

        if (!controller.set_controller_dead_zone(deadZone))
            return ContractFailure::dead_zone;
        const SavePlan deadZonePlan = controller.prepare_save();
        if (!deadZonePlan
            || !store.publish_source_and_artifact(
                deadZonePlan.source, deadZonePlan.artifact)
            || !controller.confirm_saved(deadZonePlan.revision))
        {
            return ContractFailure::paired_persistence;
        }
        project_input::ProjectInputProfileStore deadZoneStore{
            deadZonePlan.project_id, root.path};
        const auto deadZoneArtifact = deadZoneStore.load_artifact();
        if (!deadZoneArtifact)
            return ContractFailure::paired_persistence;

        project_input_controller::SnapshotSampler sampler{};
        project_input::InputSnapshot belowDeadZone{.frame_index = 2u};
        const auto belowSample = sampler.sample(
            deadZoneArtifact.artifact,
            controller_snapshot(1u, 6'000),
            belowDeadZone);
        const auto belowFrame = project_input::evaluate_action_frame(
            deadZoneArtifact.artifact, belowDeadZone);
        const auto* belowMove = project_input::find_action(
            belowFrame,
            project_input::ActionSemantic::move_x,
            deadZoneArtifact.artifact);
        project_input::InputSnapshot aboveDeadZone{.frame_index = 3u};
        const auto aboveSample = sampler.sample(
            deadZoneArtifact.artifact,
            controller_snapshot(2u, 16'384),
            aboveDeadZone);
        const auto aboveFrame = project_input::evaluate_action_frame(
            deadZoneArtifact.artifact, aboveDeadZone);
        const auto* aboveMove = project_input::find_action(
            aboveFrame,
            project_input::ActionSemantic::move_x,
            deadZoneArtifact.artifact);
        if (!belowSample || !belowFrame || !belowMove
            || belowMove->value_q15 != 0
            || !aboveSample || !aboveFrame || !aboveMove
            || aboveMove->value_q15 <= 0)
        {
            return ContractFailure::persisted_controller_dead_zone;
        }

        if (!controller.reset_defaults() || !controller.dirty())
            return ContractFailure::reset;
        const SavePlan resetPlan = controller.prepare_save();
        if (!resetPlan || resetPlan.revision.sequence != 4u
            || !store.publish_source_and_artifact(
                resetPlan.source, resetPlan.artifact)
            || !controller.confirm_saved(resetPlan.revision))
        {
            return ContractFailure::reset;
        }

        project_input::ProjectInputProfileStore resetStore{
            resetPlan.project_id, root.path};
        const auto resetSource = resetStore.load_source();
        const auto resetArtifact = resetStore.load_artifact();
        if (!resetSource || resetSource.source != resetPlan.source
            || !resetArtifact
            || resetArtifact.artifact != resetPlan.artifact)
        {
            return ContractFailure::persisted_default_restore;
        }

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
