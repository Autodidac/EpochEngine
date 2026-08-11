/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

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