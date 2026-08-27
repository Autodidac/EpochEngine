/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <compare>
#include <cstddef>
#include <cstdint>

module project.input_controller;

import input.controller;
import project.input_profile;

namespace epochengine::project_input_controller
{
    namespace
    {
        [[nodiscard]] controller_input::Snapshot connected_snapshot(
            std::uint64_t revision) noexcept
        {
            controller_input::Snapshot snapshot{};
            snapshot.revision = revision;
            snapshot.provider_available = true;
            auto& device = snapshot.devices[0];
            device.handle = {0u, 1u};
            device.connected = true;
            device.provider_id = 77u;
            device.held[static_cast<std::size_t>(
                controller_input::Button::south)] = true;
            device.pressed[static_cast<std::size_t>(
                controller_input::Button::south)] = true;
            device.axes[static_cast<std::size_t>(
                controller_input::Axis::left_x)] = 16'384;
            return snapshot;
        }

        [[nodiscard]] bool same_input_snapshot(
            const project_input::InputSnapshot& left,
            const project_input::InputSnapshot& right) noexcept
        {
            if (left.frame_index != right.frame_index
                || left.modifiers != right.modifiers
                || left.keyboard.size() != right.keyboard.size()
                || left.controller_buttons.size()
                    != right.controller_buttons.size()
                || left.controller_axes.size()
                    != right.controller_axes.size())
            {
                return false;
            }
            for (std::size_t index = 0u;
                 index < left.keyboard.size(); ++index)
            {
                const auto& a = left.keyboard[index];
                const auto& b = right.keyboard[index];
                if (a.key != b.key || a.held != b.held
                    || a.pressed != b.pressed)
                {
                    return false;
                }
            }
            for (std::size_t index = 0u;
                 index < left.controller_buttons.size(); ++index)
            {
                const auto& a = left.controller_buttons[index];
                const auto& b = right.controller_buttons[index];
                if (a.controller_slot != b.controller_slot
                    || a.button != b.button || a.held != b.held
                    || a.pressed != b.pressed)
                {
                    return false;
                }
            }
            for (std::size_t index = 0u;
                 index < left.controller_axes.size(); ++index)
            {
                const auto& a = left.controller_axes[index];
                const auto& b = right.controller_axes[index];
                if (a.controller_slot != b.controller_slot
                    || a.axis != b.axis
                    || a.value_q15 != b.value_q15)
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] const project_input::ActionValue* action(
            const project_input::CompiledInputProfile& profile,
            const project_input::ActionFrame& frame,
            project_input::ActionSemantic semantic) noexcept
        {
            return project_input::find_action(frame, semantic, profile);
        }
    }

    ContractFailure run_contract() noexcept
    {
        const auto compiled = project_input::compile_profile(
            "controller-adapter-contract",
            project_input::make_legacy_default_profile());
        if (!compiled)
            return ContractFailure::first_edge;

        SnapshotSampler sampler{};
        auto physical = connected_snapshot(1u);
        project_input::InputSnapshot firstInput{.frame_index = 1u};
        const auto first = sampler.sample(
            compiled.artifact, physical, firstInput);
        const auto firstFrame = project_input::evaluate_action_frame(
            compiled.artifact, firstInput);
        const auto* firstJump = action(
            compiled.artifact,
            firstFrame,
            project_input::ActionSemantic::jump);
        const auto* firstMove = action(
            compiled.artifact,
            firstFrame,
            project_input::ActionSemantic::move_x);
        if (!first || first.code != SampleCode::ready
            || first.button_samples != 4u || first.axis_samples != 2u
            || !firstFrame || !firstJump || !firstJump->pressed
            || firstJump->value_q15 != project_input::normalized_unit
            || !firstMove || firstMove->value_q15 <= 0)
        {
            return ContractFailure::first_edge;
        }

        project_input::InputSnapshot repeatedInput{.frame_index = 2u};
        const auto repeated = sampler.sample(
            compiled.artifact, physical, repeatedInput);
        const auto repeatedFrame = project_input::evaluate_action_frame(
            compiled.artifact, repeatedInput);
        const auto* repeatedJump = action(
            compiled.artifact,
            repeatedFrame,
            project_input::ActionSemantic::jump);
        const auto* repeatedMove = action(
            compiled.artifact,
            repeatedFrame,
            project_input::ActionSemantic::move_x);
        if (!repeated || repeated.code != SampleCode::repeated_revision
            || !repeatedFrame || !repeatedJump || repeatedJump->pressed
            || repeatedJump->value_q15 != project_input::normalized_unit)
        {
            return ContractFailure::repeated_edge;
        }
        if (!repeatedMove || repeatedMove->value_q15 != firstMove->value_q15)
            return ContractFailure::axis_continuity;

        physical.revision = 2u;
        physical.devices[0].pressed.fill(false);
        physical.devices[0].held.fill(false);
        physical.devices[0].axes.fill(0);
        project_input::InputSnapshot changedInput{.frame_index = 3u};
        const auto changed = sampler.sample(
            compiled.artifact, physical, changedInput);
        const auto changedFrame = project_input::evaluate_action_frame(
            compiled.artifact, changedInput);
        const auto* changedJump = action(
            compiled.artifact,
            changedFrame,
            project_input::ActionSemantic::jump);
        if (!changed || changed.code != SampleCode::ready
            || !changedFrame || !changedJump || changedJump->pressed
            || changedJump->value_q15 != 0)
        {
            return ContractFailure::changed_revision;
        }

        auto disconnected = physical;
        disconnected.revision = 3u;
        disconnected.devices[0] = {};
        disconnected.devices[0].handle = {0u, 2u};
        project_input::InputSnapshot disconnectedInput{.frame_index = 4u};
        const auto disconnectedResult = sampler.sample(
            compiled.artifact, disconnected, disconnectedInput);
        if (!disconnectedResult
            || !disconnectedInput.controller_buttons.empty()
            || !disconnectedInput.controller_axes.empty())
        {
            return ContractFailure::disconnected_device;
        }

        project_input::InputSnapshot staleInput{.frame_index = 5u};
        staleInput.keyboard.push_back(
            {project_input::KeyCode::a, true, false});
        const auto staleBefore = staleInput;
        const auto stale = sampler.sample(
            compiled.artifact, physical, staleInput);
        if (stale.code != SampleCode::stale_revision
            || !same_input_snapshot(staleInput, staleBefore) || sampler.accepted_revision() != 3u)
        {
            return ContractFailure::stale_revision;
        }

        auto invalid = connected_snapshot(4u);
        invalid.devices[0].handle.generation = 0u;
        project_input::InputSnapshot invalidInput{.frame_index = 6u};
        const auto invalidBefore = invalidInput;
        if (sampler.sample(compiled.artifact, invalid, invalidInput).code
                != SampleCode::invalid_snapshot
            || !same_input_snapshot(invalidInput, invalidBefore))
        {
            return ContractFailure::invalid_generation;
        }

        auto duplicatePhysical = connected_snapshot(4u);
        project_input::InputSnapshot duplicateInput{.frame_index = 7u};
        duplicateInput.controller_buttons.push_back({
            0u, project_input::ControllerButton::south, true, false});
        const auto duplicateBefore = duplicateInput;
        if (sampler.sample(
                compiled.artifact, duplicatePhysical, duplicateInput).code
                != SampleCode::duplicate_input
            || !same_input_snapshot(duplicateInput, duplicateBefore))
        {
            return ContractFailure::duplicate_destination;
        }

        project_input::ProfileLimits tightLimits{};
        tightLimits.maximum_input_samples = 6u;
        project_input::InputSnapshot boundedInput{.frame_index = 8u};
        boundedInput.keyboard.push_back(
            {project_input::KeyCode::a, true, false});
        const auto boundedBefore = boundedInput;
        if (sampler.sample(
                compiled.artifact,
                duplicatePhysical,
                boundedInput,
                tightLimits).code != SampleCode::input_limit_exceeded
            || !same_input_snapshot(boundedInput, boundedBefore))
        {
            return ContractFailure::bounded_destination;
        }

        sampler.reset();
        project_input::InputSnapshot resetInput{.frame_index = 9u};
        const auto resetResult = sampler.sample(
            compiled.artifact, connected_snapshot(1u), resetInput);
        const auto resetFrame = project_input::evaluate_action_frame(
            compiled.artifact, resetInput);
        const auto* resetJump = action(
            compiled.artifact,
            resetFrame,
            project_input::ActionSemantic::jump);
        if (!resetResult || resetResult.code != SampleCode::ready
            || !resetJump || !resetJump->pressed)
        {
            return ContractFailure::reset;
        }
        return ContractFailure::none;
    }
}

#if defined(EPOCH_PROJECT_INPUT_CONTROLLER_CONTRACT_MAIN)
int main()
{
    return epochengine::project_input_controller::run_contract()
        == epochengine::project_input_controller::ContractFailure::none
        ? 0 : 1;
}
#endif
