/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

module project.input_profile;

namespace epochengine::project_input
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
                path /= "epoch_project_input_profile_contract_"
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

        [[nodiscard]] bool write_malformed_file(
            const fs::path& path) noexcept
        {
            try
            {
                std::ofstream output{
                    path, std::ios::binary | std::ios::trunc};
                if (!output)
                    return false;
                constexpr char bytes[]{'E', 'P', 'I', 'N', 'S', 'R', 'C'};
                output.write(bytes, sizeof(bytes));
                output.flush();
                return static_cast<bool>(output);
            }
            catch (...)
            {
                return false;
            }
        }

        [[nodiscard]] BindingDefinition* find_binding(
            ProfileSource& source,
            ActionSemantic semantic,
            BindingDevice device,
            std::uint16_t code) noexcept
        {
            const ActionId action = stable_action_id(semantic);
            for (BindingDefinition& binding : source.bindings)
            {
                if (binding.action == action && binding.device == device
                    && binding.code == code)
                {
                    return &binding;
                }
            }
            return nullptr;
        }

        [[nodiscard]] const BindingDefinition* find_binding(
            const CompiledInputProfile& artifact,
            ActionSemantic semantic,
            BindingDevice device) noexcept
        {
            const ActionId action = stable_action_id(semantic);
            for (const BindingDefinition& binding : artifact.bindings)
            {
                if (binding.action == action && binding.device == device)
                    return &binding;
            }
            return nullptr;
        }
    }

    ContractFailure project_input_profile_contract_failure() noexcept
    {
        const ProfileLimits limits{};
        ProfileSource source = make_legacy_default_profile(2u);
        if (validate_profile_source(source, limits) != ValidationCode::ready
            || source.actions.size() != 6u
            || source.bindings.size() != 18u
            || source.revision.sequence != 2u)
        {
            return ContractFailure::legacy_default;
        }

        const SerializedProfile firstSourceBytes = serialize_profile_source(
            source, limits);
        const SerializedProfile secondSourceBytes = serialize_profile_source(
            source, limits);
        const DeserializedProfile restoredSource = firstSourceBytes
            ? deserialize_profile_source(firstSourceBytes.bytes, limits)
            : DeserializedProfile{};
        if (!firstSourceBytes || !secondSourceBytes
            || firstSourceBytes.bytes != secondSourceBytes.bytes
            || !restoredSource || restoredSource.source != source)
        {
            return ContractFailure::source_roundtrip;
        }

        const CompiledProfileResult compiled = compile_profile(
            "input-profile-contract", source, limits);
        const SerializedProfile firstArtifactBytes = compiled
            ? serialize_compiled_profile(compiled.artifact, limits)
            : SerializedProfile{};
        const SerializedProfile secondArtifactBytes = compiled
            ? serialize_compiled_profile(compiled.artifact, limits)
            : SerializedProfile{};
        const DeserializedCompiledProfile restoredArtifact =
            firstArtifactBytes
            ? deserialize_compiled_profile(firstArtifactBytes.bytes, limits)
            : DeserializedCompiledProfile{};
        if (!compiled || !firstArtifactBytes || !secondArtifactBytes
            || firstArtifactBytes.bytes != secondArtifactBytes.bytes
            || !restoredArtifact
            || restoredArtifact.artifact != compiled.artifact)
        {
            return ContractFailure::artifact_roundtrip;
        }

        std::vector<std::byte> truncated = firstSourceBytes.bytes;
        truncated.resize(truncated.size() - 1u);
        std::vector<std::byte> corrupted = firstSourceBytes.bytes;
        corrupted.back() ^= std::byte{0x5au};
        std::vector<std::byte> trailing = firstSourceBytes.bytes;
        trailing.push_back(std::byte{0u});
        if (deserialize_profile_source(truncated, limits)
            || deserialize_profile_source(corrupted, limits)
            || deserialize_profile_source(trailing, limits))
        {
            return ContractFailure::malformed_rejection;
        }

        ProfileSource duplicateId = source;
        duplicateId.revision.sequence = 3u;
        duplicateId.bindings.push_back(duplicateId.bindings.front());
        if (seal_profile_source(duplicateId, limits)
            != ValidationCode::duplicate_binding)
        {
            return ContractFailure::duplicate_rejection;
        }
        ProfileSource duplicateSource = source;
        duplicateSource.revision.sequence = 3u;
        duplicateSource.bindings.back().id = stable_binding_id(
            "contract.duplicate-source");
        duplicateSource.bindings.back().device =
            duplicateSource.bindings.front().device;
        duplicateSource.bindings.back().code =
            duplicateSource.bindings.front().code;
        duplicateSource.bindings.back().controller_slot =
            duplicateSource.bindings.front().controller_slot;
        duplicateSource.bindings.back().polarity =
            duplicateSource.bindings.front().polarity;
        duplicateSource.bindings.back().required_modifiers =
            duplicateSource.bindings.front().required_modifiers;
        duplicateSource.bindings.back().forbidden_modifiers =
            duplicateSource.bindings.front().forbidden_modifiers;
        if (seal_profile_source(duplicateSource, limits)
            != ValidationCode::duplicate_binding_source)
        {
            return ContractFailure::duplicate_rejection;
        }

        const BindingDefinition* resetBinding = find_binding(
            compiled.artifact,
            ActionSemantic::reset,
            BindingDevice::keyboard);
        const BindingDefinition* controllerAxisBinding = find_binding(
            compiled.artifact,
            ActionSemantic::move_x,
            BindingDevice::controller_axis);
        if (!resetBinding || !controllerAxisBinding)
            return ContractFailure::profile_rebind;
        const auto rebound = rebind_keyboard(
            source, resetBinding->id, KeyCode::t, limits);
        if (!rebound
            || rebound.code != ProfileEditCode::ready
            || rebound.source.revision.sequence != source.revision.sequence + 1u
            || rebound.source.revision.content == source.revision.content)
        {
            return ContractFailure::profile_rebind;
        }
        const auto reboundArtifact = compile_profile(
            "input-profile-contract", rebound.source, limits);
        InputSnapshot reboundInput{};
        reboundInput.frame_index = 9u;
        reboundInput.keyboard.push_back({KeyCode::t, true, true});
        const auto reboundFrame = reboundArtifact
            ? evaluate_action_frame(
                reboundArtifact.artifact, reboundInput, limits)
            : ActionFrame{};
        const auto* reboundReset = reboundArtifact
            ? find_action(
                reboundFrame,
                ActionSemantic::reset,
                reboundArtifact.artifact)
            : nullptr;
        if (!reboundArtifact || !reboundFrame || !reboundReset
            || reboundReset->value_q15 != normalized_unit
            || !reboundReset->pressed)
        {
            return ContractFailure::profile_rebind;
        }
        const auto controllerRebind = rebind_keyboard(
            source, controllerAxisBinding->id, KeyCode::t, limits);
        const auto duplicateRebind = rebind_keyboard(
            source, resetBinding->id, KeyCode::e, limits);
        if (controllerRebind.code != ProfileEditCode::unsupported_device
            || duplicateRebind.code != ProfileEditCode::validation_failed
            || duplicateRebind.validation
                != ValidationCode::duplicate_binding_source)
        {
            return ContractFailure::profile_rebind;
        }

        const BindingDefinition* jumpControllerBinding = find_binding(
            compiled.artifact,
            ActionSemantic::jump,
            BindingDevice::controller_button);
        if (!jumpControllerBinding)
            return ContractFailure::controller_rebind;
        const auto controllerButtonRebound = rebind_controller_button(
            source,
            jumpControllerBinding->id,
            ControllerButton::east,
            1u,
            limits);
        const auto controllerAxisRebound = rebind_controller_axis(
            source,
            controllerAxisBinding->id,
            ControllerAxis::right_x,
            2u,
            limits);
        const auto wrongControllerKind = rebind_controller_axis(
            source,
            jumpControllerBinding->id,
            ControllerAxis::right_x,
            0u,
            limits);
        const auto duplicateControllerSource = rebind_controller_button(
            source,
            jumpControllerBinding->id,
            ControllerButton::west,
            0u,
            limits);
        const auto invalidControllerSlot = rebind_controller_button(
            source,
            jumpControllerBinding->id,
            ControllerButton::east,
            static_cast<std::uint8_t>(limits.maximum_controller_slots),
            limits);
        const auto controllerButtonArtifact = controllerButtonRebound
            ? compile_profile(
                "input-profile-contract",
                controllerButtonRebound.source,
                limits)
            : CompiledProfileResult{};
        InputSnapshot controllerButtonInput{};
        controllerButtonInput.frame_index = 10u;
        controllerButtonInput.controller_buttons.push_back({
            1u, ControllerButton::east, true, true});
        const auto controllerButtonFrame = controllerButtonArtifact
            ? evaluate_action_frame(
                controllerButtonArtifact.artifact,
                controllerButtonInput,
                limits)
            : ActionFrame{};
        const auto* controllerJump = controllerButtonArtifact
            ? find_action(
                controllerButtonFrame,
                ActionSemantic::jump,
                controllerButtonArtifact.artifact)
            : nullptr;
        if (!controllerButtonRebound || !controllerAxisRebound
            || controllerButtonRebound.source.revision.sequence
                != source.revision.sequence + 1u
            || controllerAxisRebound.source.revision.sequence
                != source.revision.sequence + 1u
            || wrongControllerKind.code != ProfileEditCode::unsupported_device
            || duplicateControllerSource.code
                != ProfileEditCode::validation_failed
            || duplicateControllerSource.validation
                != ValidationCode::duplicate_binding_source
            || invalidControllerSlot.code
                != ProfileEditCode::invalid_controller_slot
            || !controllerButtonArtifact || !controllerButtonFrame
            || !controllerJump
            || controllerJump->value_q15 != normalized_unit
            || !controllerJump->pressed)
        {
            return ContractFailure::controller_rebind;
        }

        constexpr std::uint16_t editedDeadZone{6'000u};
        const auto deadZoneEdited = set_controller_dead_zone(
            source, editedDeadZone, limits);
        const auto deadZoneUnchanged = deadZoneEdited
            ? set_controller_dead_zone(
                deadZoneEdited.source, editedDeadZone, limits)
            : ProfileEditResult{};
        const auto deadZoneInvalid = set_controller_dead_zone(
            source,
            static_cast<std::uint16_t>(normalized_unit),
            limits);
        bool allAxesEdited = deadZoneEdited
            && std::all_of(
                deadZoneEdited.source.bindings.begin(),
                deadZoneEdited.source.bindings.end(),
                [editedDeadZone](const BindingDefinition& binding)
                {
                    return binding.device != BindingDevice::controller_axis
                        || binding.dead_zone_q15 == editedDeadZone;
                });
        if (!deadZoneEdited || !allAxesEdited
            || deadZoneEdited.source.revision.sequence
                != source.revision.sequence + 1u
            || deadZoneUnchanged.code != ProfileEditCode::unchanged
            || deadZoneInvalid.code != ProfileEditCode::invalid_dead_zone)
        {
            return ContractFailure::dead_zone_edit;
        }

        const BindingDefinition* moveAxis = find_binding(
            compiled.artifact,
            ActionSemantic::move_x,
            BindingDevice::controller_axis);
        if (!moveAxis)
            return ContractFailure::dead_zone;
        InputSnapshot insideDeadZone{};
        insideDeadZone.frame_index = 10u;
        insideDeadZone.controller_axes.push_back({
            moveAxis->controller_slot,
            static_cast<ControllerAxis>(moveAxis->code),
            static_cast<std::int16_t>(moveAxis->dead_zone_q15)});
        const ActionFrame deadFrame = evaluate_action_frame(
            compiled.artifact, insideDeadZone, limits);
        const ActionValue* deadMove = find_action(
            deadFrame, ActionSemantic::move_x, compiled.artifact);
        insideDeadZone.controller_axes.front().value_q15 =
            static_cast<std::int16_t>(moveAxis->saturation_q15);
        const ActionFrame saturatedFrame = evaluate_action_frame(
            compiled.artifact, insideDeadZone, limits);
        const ActionValue* saturatedMove = find_action(
            saturatedFrame, ActionSemantic::move_x, compiled.artifact);
        if (!deadFrame || !deadMove || deadMove->value_q15 != 0
            || !saturatedFrame || !saturatedMove
            || saturatedMove->value_q15 != normalized_unit)
        {
            return ContractFailure::dead_zone;
        }

        InputSnapshot gameplayInput{};
        gameplayInput.frame_index = 11u;
        gameplayInput.keyboard = {
            {KeyCode::d, true, false},
            {KeyCode::w, true, false},
            {KeyCode::space, true, true}};
        const ActionFrame gameplayFrame = evaluate_action_frame(
            compiled.artifact, gameplayInput, limits);
        const ActionValue* moveX = find_action(
            gameplayFrame, ActionSemantic::move_x, compiled.artifact);
        const ActionValue* moveY = find_action(
            gameplayFrame, ActionSemantic::move_y, compiled.artifact);
        const ActionValue* jump = find_action(
            gameplayFrame, ActionSemantic::jump, compiled.artifact);
        if (!gameplayFrame || !moveX || !moveY || !jump
            || moveX->value_q15 != normalized_unit
            || moveY->value_q15 != normalized_unit
            || jump->value_q15 != normalized_unit || !jump->pressed)
        {
            return ContractFailure::deterministic_evaluation;
        }

        ActionFrame injectedFrame = evaluate_action_frame(
            compiled.artifact, InputSnapshot{.frame_index = 11u}, limits);
        constexpr std::array impulses{
            ActionImpulse{ActionSemantic::move_x, -16'384, false},
            ActionImpulse{ActionSemantic::interact, normalized_unit, true}};
        if (inject_action_impulses(
                compiled.artifact, injectedFrame, impulses, limits)
                != InjectionCode::ready)
        {
            return ContractFailure::action_injection;
        }
        const ActionValue* injectedMove = find_action(
            injectedFrame, ActionSemantic::move_x, compiled.artifact);
        const ActionValue* injectedInteract = find_action(
            injectedFrame, ActionSemantic::interact, compiled.artifact);
        constexpr std::array duplicateImpulses{
            ActionImpulse{ActionSemantic::jump, normalized_unit, true},
            ActionImpulse{ActionSemantic::jump, normalized_unit, true}};
        ActionFrame rejectedFrame = injectedFrame;
        if (!injectedMove || injectedMove->value_q15 != -16'384
            || !injectedInteract
            || injectedInteract->value_q15 != normalized_unit
            || !injectedInteract->pressed
            || action_semantic_from_name("move_x")
                != ActionSemantic::move_x
            || action_semantic_from_name("project.move_x")
                != ActionSemantic::invalid
            || inject_action_impulses(
                    compiled.artifact,
                    rejectedFrame,
                    duplicateImpulses,
                    limits) != InjectionCode::duplicate_action
            || rejectedFrame != injectedFrame)
        {
            return ContractFailure::action_injection;
        }

        ProfileSource modified = source;
        modified.revision.sequence = 3u;
        BindingDefinition* reset = find_binding(
            modified,
            ActionSemantic::reset,
            BindingDevice::keyboard,
            static_cast<std::uint16_t>(KeyCode::r));
        if (!reset)
            return ContractFailure::modifier_evaluation;
        reset->required_modifiers = modifier_mask(Modifier::control);
        reset->forbidden_modifiers = modifier_mask(Modifier::shift);
        if (seal_profile_source(modified, limits) != ValidationCode::ready)
            return ContractFailure::modifier_evaluation;
        const CompiledProfileResult modifiedArtifact = compile_profile(
            "input-profile-contract", modified, limits);
        InputSnapshot modifierInput{};
        modifierInput.frame_index = 12u;
        modifierInput.keyboard.push_back({KeyCode::r, true, true});
        ActionFrame modifierFrame = evaluate_action_frame(
            modifiedArtifact.artifact, modifierInput, limits);
        const ActionValue* resetValue = find_action(
            modifierFrame, ActionSemantic::reset, modifiedArtifact.artifact);
        if (!modifiedArtifact || !modifierFrame || !resetValue
            || resetValue->value_q15 != 0 || resetValue->pressed)
        {
            return ContractFailure::modifier_evaluation;
        }
        modifierInput.modifiers = modifier_mask(Modifier::control);
        modifierFrame = evaluate_action_frame(
            modifiedArtifact.artifact, modifierInput, limits);
        resetValue = find_action(
            modifierFrame, ActionSemantic::reset, modifiedArtifact.artifact);
        if (!modifierFrame || !resetValue
            || resetValue->value_q15 != normalized_unit
            || !resetValue->pressed)
        {
            return ContractFailure::modifier_evaluation;
        }
        modifierInput.modifiers = Modifier::control | Modifier::shift;
        modifierFrame = evaluate_action_frame(
            modifiedArtifact.artifact, modifierInput, limits);
        resetValue = find_action(
            modifierFrame, ActionSemantic::reset, modifiedArtifact.artifact);
        if (!modifierFrame || !resetValue
            || resetValue->value_q15 != 0 || resetValue->pressed)
        {
            return ContractFailure::modifier_evaluation;
        }

        std::vector<InputSnapshot> replay{
            InputSnapshot{20u, 0u, {{KeyCode::a, true, false}}, {}, {}},
            InputSnapshot{21u, 0u, {{KeyCode::space, true, true}}, {}, {}},
            InputSnapshot{
                22u,
                0u,
                {},
                {{0u, ControllerButton::west, true, true}},
                {{0u, ControllerAxis::left_x, 16'384}}},
            InputSnapshot{23u, 0u, {}, {}, {}}};
        for (const InputSnapshot& input : replay)
        {
            const ActionFrame first = evaluate_action_frame(
                compiled.artifact, input, limits);
            const ActionFrame second = evaluate_action_frame(
                compiled.artifact, input, limits);
            const ActionFrame reopened = evaluate_action_frame(
                restoredArtifact.artifact, input, limits);
            if (!first || first != second || first != reopened)
                return ContractFailure::replay_identity;
        }

        ContractRoot root{};
        if (!root.valid())
            return ContractFailure::temporary_root;
        ProjectInputProfileStore store{
            "input-profile-contract", root.path};
        const StoredProfile savedSource = store.save_source(source);
        if (!savedSource
            || savedSource.storage_path.lexically_relative(root.path)
                != fs::path{canonical_source_path})
        {
            return ContractFailure::source_save_reopen;
        }
        ProjectInputProfileStore reopenedStore{
            "input-profile-contract", root.path};
        const LoadedProfile loadedSource = reopenedStore.load_source();
        if (!loadedSource || loadedSource.source != source)
            return ContractFailure::source_save_reopen;

        const StoredArtifact savedArtifact = store.publish_artifact(
            compiled.artifact);
        if (!savedArtifact
            || savedArtifact.storage_path.lexically_relative(root.path)
                != fs::path{canonical_artifact_path})
        {
            return ContractFailure::artifact_save_reopen;
        }
        ProjectInputProfileStore reopenedArtifactStore{
            "input-profile-contract", root.path};
        const LoadedArtifact loadedArtifact =
            reopenedArtifactStore.load_artifact();
        if (!loadedArtifact || loadedArtifact.artifact != compiled.artifact)
            return ContractFailure::artifact_save_reopen;

        ContractRoot pairedRoot{};
        if (!pairedRoot.valid())
            return ContractFailure::temporary_root;
        ProjectInputProfileStore pairedStore{
            "input-profile-contract", pairedRoot.path};
        const StoredProfilePair savedPair =
            pairedStore.publish_source_and_artifact(
                source, compiled.artifact);
        const StoredProfilePair unchangedPair =
            pairedStore.publish_source_and_artifact(
                source, compiled.artifact);
        ProjectInputProfileStore reopenedPairStore{
            "input-profile-contract", pairedRoot.path};
        const LoadedProfile pairedSource =
            reopenedPairStore.load_source();
        const LoadedArtifact pairedArtifact =
            reopenedPairStore.load_artifact();
        if (!savedPair || savedPair.code != StoreCode::ready
            || savedPair.source.code != StoreCode::ready
            || savedPair.artifact.code != StoreCode::ready
            || !unchangedPair
            || unchangedPair.code != StoreCode::unchanged
            || unchangedPair.source.code != StoreCode::unchanged
            || unchangedPair.artifact.code != StoreCode::unchanged
            || !pairedSource || pairedSource.source != source
            || !pairedArtifact
            || pairedArtifact.artifact != compiled.artifact)
        {
            return ContractFailure::paired_save_reopen;
        }
        const StoreMetrics pairedMetrics = pairedStore.metrics();
        if (pairedMetrics.paired_save_requests != 2u
            || pairedMetrics.paired_saves != 2u
            || pairedMetrics.paired_partial_saves != 0u
            || pairedMetrics.source_save_requests != 2u
            || pairedMetrics.source_saves != 1u
            || pairedMetrics.artifact_save_requests != 2u
            || pairedMetrics.artifact_saves != 1u
            || pairedMetrics.unchanged_writes != 2u
            || pairedMetrics.rejected_operations != 0u
            || pairedMetrics.bytes_read == 0u
            || pairedMetrics.bytes_written == 0u)
        {
            return ContractFailure::paired_metrics;
        }

        ContractRoot mismatchedRoot{};
        if (!mismatchedRoot.valid())
            return ContractFailure::temporary_root;
        ProjectInputProfileStore mismatchedStore{
            "input-profile-contract", mismatchedRoot.path};
        if (mismatchedStore.publish_source_and_artifact(
                source, modifiedArtifact.artifact).code
                != StoreCode::invalid_value
            || fs::exists(mismatchedStore.source_path())
            || fs::exists(mismatchedStore.artifact_path()))
        {
            return ContractFailure::paired_mismatch_rejection;
        }

        ProfileSource stalePairSource = make_legacy_default_profile(1u);
        const CompiledProfileResult stalePairArtifact = compile_profile(
            "input-profile-contract", stalePairSource, limits);
        ProfileSource conflictPairSource = modified;
        conflictPairSource.revision.sequence = source.revision.sequence;
        if (seal_profile_source(conflictPairSource, limits)
                != ValidationCode::ready)
        {
            return ContractFailure::paired_stale_conflict;
        }
        const CompiledProfileResult conflictPairArtifact = compile_profile(
            "input-profile-contract", conflictPairSource, limits);
        if (!stalePairArtifact || !conflictPairArtifact
            || pairedStore.publish_source_and_artifact(
                stalePairSource, stalePairArtifact.artifact).code
                != StoreCode::stale_revision
            || pairedStore.publish_source_and_artifact(
                conflictPairSource, conflictPairArtifact.artifact).code
                != StoreCode::revision_conflict)
        {
            return ContractFailure::paired_stale_conflict;
        }

        ContractRoot blockedRoot{};
        if (!blockedRoot.valid())
            return ContractFailure::temporary_root;
        {
            std::ofstream blocker{
                blockedRoot.path / "Library",
                std::ios::binary | std::ios::trunc};
            blocker << "blocked";
            if (!blocker)
                return ContractFailure::paired_stage_failure;
        }
        ProjectInputProfileStore blockedStore{
            "input-profile-contract", blockedRoot.path};
        const auto blockedPair =
            blockedStore.publish_source_and_artifact(
                source, compiled.artifact);
        const StoreMetrics blockedMetrics = blockedStore.metrics();
        if (blockedPair.code != StoreCode::directory_failure
            || fs::exists(blockedStore.source_path())
            || fs::exists(blockedStore.artifact_path())
            || blockedMetrics.paired_save_requests != 1u
            || blockedMetrics.paired_saves != 0u
            || blockedMetrics.source_saves != 0u
            || blockedMetrics.artifact_saves != 0u)
        {
            return ContractFailure::paired_stage_failure;
        }

        ContractRoot corruptArtifactRoot{};
        if (!corruptArtifactRoot.valid())
            return ContractFailure::temporary_root;
        ProjectInputProfileStore corruptArtifactStore{
            "input-profile-contract", corruptArtifactRoot.path};
        if (!corruptArtifactStore.publish_source_and_artifact(
                source, compiled.artifact)
            || !write_malformed_file(corruptArtifactStore.artifact_path()))
        {
            return ContractFailure::paired_disposable_artifact_repair;
        }
        const auto corruptArtifactRepair =
            corruptArtifactStore.publish_source_and_artifact(
                modified, modifiedArtifact.artifact);
        ProjectInputProfileStore reopenedCorruptArtifactStore{
            "input-profile-contract", corruptArtifactRoot.path};
        const auto repairedCorruptSource =
            reopenedCorruptArtifactStore.load_source();
        const auto repairedCorruptArtifact =
            reopenedCorruptArtifactStore.load_artifact();
        if (!corruptArtifactRepair
            || corruptArtifactRepair.source.code != StoreCode::ready
            || corruptArtifactRepair.artifact.code != StoreCode::ready
            || !repairedCorruptSource
            || repairedCorruptSource.source != modified
            || !repairedCorruptArtifact
            || repairedCorruptArtifact.artifact != modifiedArtifact.artifact)
        {
            return ContractFailure::paired_disposable_artifact_repair;
        }

        ContractRoot artifactAheadRoot{};
        if (!artifactAheadRoot.valid())
            return ContractFailure::temporary_root;
        ProjectInputProfileStore artifactAheadStore{
            "input-profile-contract", artifactAheadRoot.path};
        ProfileSource artifactAheadSource = modified;
        artifactAheadSource.revision.sequence = 4u;
        if (seal_profile_source(artifactAheadSource, limits)
                != ValidationCode::ready)
        {
            return ContractFailure::paired_artifact_ahead_repair;
        }
        const CompiledProfileResult artifactAheadCompiled = compile_profile(
            "input-profile-contract", artifactAheadSource, limits);
        if (!artifactAheadCompiled
            || !artifactAheadStore.save_source(source)
            || !artifactAheadStore.publish_artifact(
                artifactAheadCompiled.artifact))
        {
            return ContractFailure::paired_artifact_ahead_repair;
        }
        const auto repairedPair =
            artifactAheadStore.publish_source_and_artifact(
                modified, modifiedArtifact.artifact);
        ProjectInputProfileStore repairedStore{
            "input-profile-contract", artifactAheadRoot.path};
        const auto repairedSource = repairedStore.load_source();
        const auto repairedArtifact = repairedStore.load_artifact();
        if (!repairedPair
            || repairedPair.source.code != StoreCode::ready
            || repairedPair.artifact.code != StoreCode::ready
            || !repairedSource || repairedSource.source != modified
            || !repairedArtifact
            || repairedArtifact.artifact != modifiedArtifact.artifact)
        {
            return ContractFailure::paired_artifact_ahead_repair;
        }

        ContractRoot partialRoot{};
        if (!partialRoot.valid())
            return ContractFailure::temporary_root;
        ProjectInputProfileStore partialStore{
            "input-profile-contract", partialRoot.path};
        if (!partialStore.save_source(source))
            return ContractFailure::paired_partial_save_reopen;
        std::error_code partialError{};
        fs::create_directories(partialStore.artifact_path(), partialError);
        if (partialError)
            return ContractFailure::paired_partial_save_reopen;
        {
            std::ofstream blocker{
                partialStore.artifact_path() / "keep",
                std::ios::binary | std::ios::trunc};
            blocker << "block replacement";
            if (!blocker)
                return ContractFailure::paired_partial_save_reopen;
        }
        const StoreMetrics beforePartial = partialStore.metrics();
        const StoredProfilePair partialPair =
            partialStore.publish_source_and_artifact(
                modified, modifiedArtifact.artifact);
        const StoreMetrics afterPartial = partialStore.metrics();
        ProjectInputProfileStore reopenedPartialStore{
            "input-profile-contract", partialRoot.path};
        const LoadedProfile reopenedPartialSource =
            reopenedPartialStore.load_source();
        if (partialPair.code != StoreCode::atomic_replace_failure
            || partialPair.source.code != StoreCode::ready
            || partialPair.artifact.code
                != StoreCode::atomic_replace_failure
            || !reopenedPartialSource
            || reopenedPartialSource.source != modified
            || reopenedPartialStore.load_artifact().code == StoreCode::ready
            || afterPartial.paired_save_requests
                != beforePartial.paired_save_requests + 1u
            || afterPartial.paired_saves != beforePartial.paired_saves
            || afterPartial.paired_partial_saves
                != beforePartial.paired_partial_saves + 1u
            || afterPartial.source_save_requests
                != beforePartial.source_save_requests + 1u
            || afterPartial.source_saves != beforePartial.source_saves + 1u
            || afterPartial.artifact_save_requests
                != beforePartial.artifact_save_requests + 1u
            || afterPartial.artifact_saves != beforePartial.artifact_saves
            || afterPartial.rejected_operations
                != beforePartial.rejected_operations + 1u
            || afterPartial.bytes_written
                != beforePartial.bytes_written
                    + partialPair.source.serialized_bytes)
        {
            return ContractFailure::paired_partial_save_reopen;
        }

        const ProfileSource stale = make_legacy_default_profile(1u);
        if (store.save_source(stale).code != StoreCode::stale_revision)
            return ContractFailure::stale_revision;

        const StoreMetrics metrics = store.metrics();
        if (metrics.source_save_requests != 2u
            || metrics.source_saves != 1u
            || metrics.artifact_save_requests != 1u
            || metrics.artifact_saves != 1u
            || metrics.rejected_operations != 1u
            || metrics.bytes_read == 0u || metrics.bytes_written == 0u)
        {
            return ContractFailure::metrics;
        }

        ContractRoot editedRoot{};
        if (!editedRoot.valid())
            return ContractFailure::temporary_root;
        ProjectInputProfileStore editedStore{
            "input-profile-contract", editedRoot.path};
        if (!editedStore.save_source(rebound.source)
            || !editedStore.publish_artifact(reboundArtifact.artifact))
        {
            return ContractFailure::profile_rebind;
        }
        ProjectInputProfileStore reopenedEditedStore{
            "input-profile-contract", editedRoot.path};
        const auto reopenedEditedSource = reopenedEditedStore.load_source();
        const auto reopenedEditedArtifact = reopenedEditedStore.load_artifact();
        if (!reopenedEditedSource || !reopenedEditedArtifact
            || reopenedEditedSource.source != rebound.source
            || reopenedEditedArtifact.artifact != reboundArtifact.artifact)
        {
            return ContractFailure::profile_rebind;
        }
        const auto reopenedReboundFrame = evaluate_action_frame(
            reopenedEditedArtifact.artifact, reboundInput, limits);
        const auto* reopenedReboundReset = find_action(
            reopenedReboundFrame,
            ActionSemantic::reset,
            reopenedEditedArtifact.artifact);
        if (!reopenedReboundFrame || !reopenedReboundReset
            || reopenedReboundReset->value_q15 != normalized_unit
            || !reopenedReboundReset->pressed)
        {
            return ContractFailure::profile_rebind;
        }

        if (!write_malformed_file(reopenedStore.source_path()))
            return ContractFailure::malformed_store;
        ProjectInputProfileStore malformedStore{
            "input-profile-contract", root.path};
        if (malformedStore.load_source().code != StoreCode::integrity_failure)
            return ContractFailure::malformed_store;

        return ContractFailure::none;
    }
}
#if defined(EPOCH_PROJECT_INPUT_PROFILE_CONTRACT_MAIN)
int main()
{
    return epochengine::project_input::project_input_profile_contract_failure()
        == epochengine::project_input::ContractFailure::none ? 0 : 1;
}
#endif
