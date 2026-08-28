/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module editor.project_input_settings;

import project.input_profile;

namespace epochengine::editor_project_input_settings
{
    namespace
    {
        [[nodiscard]] bool valid_project_id(
            std::string_view projectId) noexcept
        {
            if (projectId.empty() || projectId.size() > 256u)
                return false;
            return std::none_of(
                projectId.begin(), projectId.end(), [](char value) noexcept
                {
                    const auto byte = static_cast<unsigned char>(value);
                    return byte < 0x20u || value == '/' || value == '\\';
                });
        }

        [[nodiscard]] bool same_binding_source(
            const project_input::BindingDefinition& left,
            const project_input::BindingDefinition& right) noexcept
        {
            return left.device == right.device
                && left.code == right.code
                && left.controller_slot == right.controller_slot
                && left.polarity == right.polarity
                && left.required_modifiers == right.required_modifiers
                && left.forbidden_modifiers == right.forbidden_modifiers;
        }

        [[nodiscard]] std::string action_label(
            project_input::ActionSemantic semantic)
        {
            std::string label{project_input::action_semantic_name(semantic)};
            std::replace(label.begin(), label.end(), '_', ' ');
            if (!label.empty())
            {
                label.front() = static_cast<char>(std::toupper(
                    static_cast<unsigned char>(label.front())));
            }
            return label;
        }

        [[nodiscard]] std::string source_label(
            const project_input::BindingDefinition& binding)
        {
            switch (binding.device)
            {
            case project_input::BindingDevice::keyboard:
                return std::string{project_input::key_code_name(
                    static_cast<project_input::KeyCode>(binding.code))};
            case project_input::BindingDevice::controller_button:
                return std::string{project_input::controller_button_name(
                           static_cast<project_input::ControllerButton>(
                               binding.code))}
                    + " / controller "
                    + std::to_string(binding.controller_slot + 1u);
            case project_input::BindingDevice::controller_axis:
                return std::string{project_input::controller_axis_name(
                           static_cast<project_input::ControllerAxis>(
                               binding.code))}
                    + " / controller "
                    + std::to_string(binding.controller_slot + 1u);
            }
            return "Invalid";
        }

        [[nodiscard]] const project_input::ActionDefinition* find_action(
            const project_input::ProfileSource& source,
            project_input::ActionId id) noexcept
        {
            const auto found = std::find_if(
                source.actions.begin(), source.actions.end(),
                [id](const auto& action) noexcept
                {
                    return action.id == id;
                });
            return found == source.actions.end() ? nullptr : &*found;
        }

        [[nodiscard]] project_input::BindingDefinition* find_binding(
            project_input::ProfileSource& source,
            project_input::BindingId id) noexcept
        {
            const auto found = std::find_if(
                source.bindings.begin(), source.bindings.end(),
                [id](const auto& binding) noexcept
                {
                    return binding.id == id;
                });
            return found == source.bindings.end() ? nullptr : &*found;
        }

        [[nodiscard]] std::vector<BindingConflict> requested_conflicts(
            const project_input::ProfileSource& source,
            project_input::BindingId requested)
        {
            auto conflicts = binding_conflicts(source);
            std::erase_if(
                conflicts,
                [requested](const BindingConflict& conflict) noexcept
                {
                    return conflict.binding != requested
                        && conflict.conflicting_binding != requested;
                });
            for (auto& conflict : conflicts)
            {
                if (conflict.binding != requested)
                {
                    std::swap(
                        conflict.binding,
                        conflict.conflicting_binding);
                }
            }
            return conflicts;
        }

        [[nodiscard]] SettingsCode edit_settings_code(
            const project_input::ProfileEditResult& result) noexcept
        {
            using Edit = project_input::ProfileEditCode;
            switch (result.code)
            {
            case Edit::ready: return SettingsCode::ready;
            case Edit::unchanged: return SettingsCode::unchanged;
            case Edit::revision_exhausted:
                return SettingsCode::revision_exhausted;
            case Edit::allocation_failure:
                return SettingsCode::allocation_failure;
            case Edit::invalid_source:
                return SettingsCode::invalid_profile;
            case Edit::validation_failed:
                return result.validation
                    == project_input::ValidationCode::duplicate_binding_source
                    ? SettingsCode::binding_conflict
                    : SettingsCode::invalid_profile;
            case Edit::invalid_binding:
            case Edit::unsupported_device:
            case Edit::invalid_key:
            case Edit::invalid_controller_button:
            case Edit::invalid_controller_axis:
            case Edit::invalid_controller_slot:
            case Edit::invalid_dead_zone:
                return SettingsCode::invalid_binding;
            }
            return SettingsCode::invalid_binding;
        }

        [[nodiscard]] std::vector<BindingView> make_binding_views(
            const project_input::ProfileSource& source)
        {
            std::vector<BindingView> result{};
            result.reserve(source.bindings.size());
            for (const auto& binding : source.bindings)
            {
                const auto* action = find_action(source, binding.action);
                const auto semantic = action
                    ? action->semantic
                    : project_input::ActionSemantic::invalid;
                result.push_back({
                    .id = binding.id,
                    .action = binding.action,
                    .semantic = semantic,
                    .value_kind = action
                        ? action->value_kind
                        : project_input::ActionValueKind::button,
                    .device = binding.device,
                    .code = binding.code,
                    .controller_slot = binding.controller_slot,
                    .polarity = binding.polarity,
                    .response = binding.response,
                    .required_modifiers = binding.required_modifiers,
                    .forbidden_modifiers = binding.forbidden_modifiers,
                    .scale_q15 = binding.scale_q15,
                    .dead_zone_q15 = binding.dead_zone_q15,
                    .saturation_q15 = binding.saturation_q15,
                    .action_label = action_label(semantic),
                    .source_label = source_label(binding)});
            }
            return result;
        }
    }

    std::vector<BindingConflict> binding_conflicts(
        const project_input::ProfileSource& source) noexcept
    {
        try
        {
            std::vector<BindingConflict> result{};
            for (std::size_t index = 0u;
                 index < source.bindings.size(); ++index)
            {
                const auto& binding = source.bindings[index];
                for (std::size_t prior = 0u; prior < index; ++prior)
                {
                    const auto& candidate = source.bindings[prior];
                    if (!same_binding_source(candidate, binding))
                        continue;
                    result.push_back({
                        .binding = binding.id,
                        .conflicting_binding = candidate.id,
                        .device = binding.device,
                        .code = binding.code,
                        .controller_slot = binding.controller_slot,
                        .required_modifiers = binding.required_modifiers,
                        .forbidden_modifiers = binding.forbidden_modifiers});
                }
            }
            return result;
        }
        catch (...)
        {
            return {};
        }
    }

    Controller::Controller(project_input::ProfileLimits limits) noexcept
        : m_limits{limits}
    {
        m_evidence.validation = limits.valid()
            ? project_input::ValidationCode::invalid_profile
            : project_input::ValidationCode::invalid_limits;
    }

    SettingsEvidence Controller::load(
        std::string_view projectId,
        project_input::ProfileSource source) noexcept
    {
        try
        {
            if (!valid_project_id(projectId))
            {
                m_evidence = {
                    .code = SettingsCode::invalid_project,
                    .validation = project_input::ValidationCode::invalid_profile};
                return m_evidence;
            }
            const auto validation = project_input::validate_profile_source(
                source, m_limits);
            if (validation != project_input::ValidationCode::ready)
            {
                m_evidence = {
                    .code = SettingsCode::invalid_profile,
                    .validation = validation,
                    .conflicts = binding_conflicts(source)};
                return m_evidence;
            }

            m_projectId.assign(projectId);
            m_applied = source;
            m_working = std::move(source);
            m_loaded = true;
            m_dirty = false;
            m_evidence = {
                .code = SettingsCode::ready,
                .validation = project_input::ValidationCode::ready,
                .edit = project_input::ProfileEditCode::unchanged,
                .codec = project_input::CodecCode::ready};
            return m_evidence;
        }
        catch (...)
        {
            m_evidence = {
                .code = SettingsCode::allocation_failure,
                .validation = project_input::ValidationCode::allocation_failure,
                .edit = project_input::ProfileEditCode::allocation_failure,
                .codec = project_input::CodecCode::allocation_failure};
            return m_evidence;
        }
    }

    SettingsEvidence Controller::load_serialized(
        std::string_view projectId,
        std::span<const std::byte> bytes) noexcept
    {
        const auto decoded = project_input::deserialize_profile_source(
            bytes, m_limits);
        if (!decoded)
        {
            m_evidence = {
                .code = SettingsCode::codec_failure,
                .validation = project_input::ValidationCode::invalid_profile,
                .codec = decoded.code};
            return m_evidence;
        }
        auto evidence = load(projectId, decoded.source);
        if (evidence)
        {
            evidence.codec = project_input::CodecCode::ready;
            m_evidence = evidence;
        }
        return evidence;
    }

    SettingsEvidence Controller::reset_defaults() noexcept
    {
        if (!m_loaded)
        {
            m_evidence = {.code = SettingsCode::not_loaded};
            return m_evidence;
        }
        if (m_working.revision.sequence
            == (std::numeric_limits<std::uint64_t>::max)())
        {
            m_evidence = {
                .code = SettingsCode::revision_exhausted,
                .validation = project_input::ValidationCode::invalid_revision,
                .edit = project_input::ProfileEditCode::revision_exhausted};
            return m_evidence;
        }
        try
        {
            m_working = project_input::make_legacy_default_profile(
                m_working.revision.sequence + 1u);
            const auto validation = project_input::validate_profile_source(
                m_working, m_limits);
            if (validation != project_input::ValidationCode::ready)
            {
                m_evidence = {
                    .code = SettingsCode::invalid_profile,
                    .validation = validation};
                return m_evidence;
            }
            m_dirty = m_working != m_applied;
            m_evidence = {
                .code = m_dirty
                    ? SettingsCode::ready : SettingsCode::unchanged,
                .validation = project_input::ValidationCode::ready,
                .edit = m_dirty
                    ? project_input::ProfileEditCode::ready
                    : project_input::ProfileEditCode::unchanged,
                .codec = project_input::CodecCode::ready};
            return m_evidence;
        }
        catch (...)
        {
            m_evidence = {
                .code = SettingsCode::allocation_failure,
                .validation = project_input::ValidationCode::allocation_failure,
                .edit = project_input::ProfileEditCode::allocation_failure,
                .codec = project_input::CodecCode::allocation_failure};
            return m_evidence;
        }
    }

    SettingsEvidence Controller::discard() noexcept
    {
        if (!m_loaded)
        {
            m_evidence = {.code = SettingsCode::not_loaded};
            return m_evidence;
        }
        try
        {
            const bool changed = m_dirty;
            m_working = m_applied;
            m_dirty = false;
            m_evidence = {
                .code = changed
                    ? SettingsCode::ready : SettingsCode::unchanged,
                .validation = project_input::ValidationCode::ready,
                .edit = changed
                    ? project_input::ProfileEditCode::ready
                    : project_input::ProfileEditCode::unchanged,
                .codec = project_input::CodecCode::ready};
            return m_evidence;
        }
        catch (...)
        {
            m_evidence = {
                .code = SettingsCode::allocation_failure,
                .validation = project_input::ValidationCode::allocation_failure,
                .edit = project_input::ProfileEditCode::allocation_failure,
                .codec = project_input::CodecCode::allocation_failure};
            return m_evidence;
        }
    }

    SettingsEvidence Controller::reject_conflicts(
        std::vector<BindingConflict> conflicts) noexcept
    {
        m_evidence = {
            .code = SettingsCode::binding_conflict,
            .validation =
                project_input::ValidationCode::duplicate_binding_source,
            .edit = project_input::ProfileEditCode::validation_failed,
            .codec = project_input::CodecCode::ready,
            .conflicts = std::move(conflicts)};
        return m_evidence;
    }

    SettingsEvidence Controller::accept_edit(
        project_input::ProfileEditResult edit,
        std::vector<BindingConflict> conflicts) noexcept
    {
        if (!m_loaded)
        {
            m_evidence = {.code = SettingsCode::not_loaded};
            return m_evidence;
        }
        const SettingsCode code = edit_settings_code(edit);
        if (!edit)
        {
            m_evidence = {
                .code = code,
                .validation = edit.validation,
                .edit = edit.code,
                .codec = project_input::CodecCode::ready,
                .conflicts = std::move(conflicts)};
            return m_evidence;
        }
        try
        {
            m_working = std::move(edit.source);
            m_dirty = m_working != m_applied;
            m_evidence = {
                .code = code,
                .validation = project_input::ValidationCode::ready,
                .edit = edit.code,
                .codec = project_input::CodecCode::ready};
            return m_evidence;
        }
        catch (...)
        {
            m_evidence = {
                .code = SettingsCode::allocation_failure,
                .validation = project_input::ValidationCode::allocation_failure,
                .edit = project_input::ProfileEditCode::allocation_failure,
                .codec = project_input::CodecCode::allocation_failure};
            return m_evidence;
        }
    }

    SettingsEvidence Controller::rebind_keyboard(
        project_input::BindingId binding,
        project_input::KeyCode key) noexcept
    {
        if (!m_loaded)
            return accept_edit({});
        try
        {
            auto candidate = m_working;
            if (auto* target = find_binding(candidate, binding);
                target && target->device
                    == project_input::BindingDevice::keyboard
                    && project_input::valid_key_code(key))
            {
                target->code = static_cast<std::uint16_t>(key);
                auto conflicts = requested_conflicts(candidate, binding);
                if (!conflicts.empty())
                    return reject_conflicts(std::move(conflicts));
            }
            return accept_edit(project_input::rebind_keyboard(
                m_working, binding, key, m_limits));
        }
        catch (...)
        {
            return accept_edit({
                project_input::ProfileEditCode::allocation_failure,
                project_input::ValidationCode::allocation_failure,
                {}});
        }
    }

    SettingsEvidence Controller::rebind_controller_button(
        project_input::BindingId binding,
        project_input::ControllerButton button,
        std::uint8_t controllerSlot) noexcept
    {
        if (!m_loaded)
            return accept_edit({});
        try
        {
            auto candidate = m_working;
            if (auto* target = find_binding(candidate, binding);
                target && target->device
                    == project_input::BindingDevice::controller_button)
            {
                target->code = static_cast<std::uint16_t>(button);
                target->controller_slot = controllerSlot;
                auto conflicts = requested_conflicts(candidate, binding);
                if (!conflicts.empty())
                    return reject_conflicts(std::move(conflicts));
            }
            return accept_edit(project_input::rebind_controller_button(
                m_working, binding, button, controllerSlot, m_limits));
        }
        catch (...)
        {
            return accept_edit({
                project_input::ProfileEditCode::allocation_failure,
                project_input::ValidationCode::allocation_failure,
                {}});
        }
    }

    SettingsEvidence Controller::rebind_controller_axis(
        project_input::BindingId binding,
        project_input::ControllerAxis axis,
        std::uint8_t controllerSlot) noexcept
    {
        if (!m_loaded)
            return accept_edit({});
        try
        {
            auto candidate = m_working;
            if (auto* target = find_binding(candidate, binding);
                target && target->device
                    == project_input::BindingDevice::controller_axis)
            {
                target->code = static_cast<std::uint16_t>(axis);
                target->controller_slot = controllerSlot;
                auto conflicts = requested_conflicts(candidate, binding);
                if (!conflicts.empty())
                    return reject_conflicts(std::move(conflicts));
            }
            return accept_edit(project_input::rebind_controller_axis(
                m_working, binding, axis, controllerSlot, m_limits));
        }
        catch (...)
        {
            return accept_edit({
                project_input::ProfileEditCode::allocation_failure,
                project_input::ValidationCode::allocation_failure,
                {}});
        }
    }

    SettingsEvidence Controller::set_controller_dead_zone(
        std::uint16_t deadZoneQ15) noexcept
    {
        if (!m_loaded)
            return accept_edit({});
        return accept_edit(project_input::set_controller_dead_zone(
            m_working, deadZoneQ15, m_limits));
    }

    SavePlan Controller::prepare_save() const noexcept
    {
        if (!m_loaded)
            return {.code = SettingsCode::not_loaded};
        if (!m_dirty)
        {
            return {
                .code = SettingsCode::unchanged,
                .validation = project_input::ValidationCode::ready,
                .source_codec = project_input::CodecCode::ready,
                .artifact_codec = project_input::CodecCode::ready};
        }
        try
        {
            const auto validation = project_input::validate_profile_source(
                m_working, m_limits);
            if (validation != project_input::ValidationCode::ready)
            {
                return {
                    .code = SettingsCode::invalid_profile,
                    .validation = validation};
            }
            const auto compiled = project_input::compile_profile(
                m_projectId, m_working, m_limits);
            if (!compiled)
            {
                return {
                    .code = SettingsCode::compile_failure,
                    .validation = compiled.code};
            }
            auto sourceBytes = project_input::serialize_profile_source(
                m_working, m_limits);
            if (!sourceBytes)
            {
                return {
                    .code = SettingsCode::codec_failure,
                    .validation = project_input::ValidationCode::ready,
                    .source_codec = sourceBytes.code};
            }
            auto artifactBytes = project_input::serialize_compiled_profile(
                compiled.artifact, m_limits);
            if (!artifactBytes)
            {
                return {
                    .code = SettingsCode::codec_failure,
                    .validation = project_input::ValidationCode::ready,
                    .source_codec = sourceBytes.code,
                    .artifact_codec = artifactBytes.code};
            }
            return {
                .code = SettingsCode::ready,
                .validation = project_input::ValidationCode::ready,
                .source_codec = sourceBytes.code,
                .artifact_codec = artifactBytes.code,
                .project_id = m_projectId,
                .source_path = std::string{project_input::canonical_source_path},
                .artifact_path = std::string{
                    project_input::canonical_artifact_path},
                .expected_base_revision = m_applied.revision,
                .revision = m_working.revision,
                .source = m_working,
                .artifact = compiled.artifact,
                .source_bytes = std::move(sourceBytes.bytes),
                .artifact_bytes = std::move(artifactBytes.bytes)};
        }
        catch (...)
        {
            return {
                .code = SettingsCode::allocation_failure,
                .validation = project_input::ValidationCode::allocation_failure,
                .source_codec = project_input::CodecCode::allocation_failure,
                .artifact_codec = project_input::CodecCode::allocation_failure};
        }
    }

    SettingsEvidence Controller::confirm_saved(
        project_input::ProfileRevision revision) noexcept
    {
        if (!m_loaded)
        {
            m_evidence = {.code = SettingsCode::not_loaded};
            return m_evidence;
        }
        if (!revision || revision != m_working.revision)
        {
            m_evidence = {
                .code = SettingsCode::stale_revision,
                .validation = project_input::ValidationCode::invalid_revision,
                .codec = project_input::CodecCode::ready};
            return m_evidence;
        }
        try
        {
            m_applied = m_working;
            m_dirty = false;
            m_evidence = {
                .code = SettingsCode::ready,
                .validation = project_input::ValidationCode::ready,
                .edit = project_input::ProfileEditCode::unchanged,
                .codec = project_input::CodecCode::ready};
            return m_evidence;
        }
        catch (...)
        {
            m_evidence = {
                .code = SettingsCode::allocation_failure,
                .validation = project_input::ValidationCode::allocation_failure,
                .edit = project_input::ProfileEditCode::allocation_failure,
                .codec = project_input::CodecCode::allocation_failure};
            return m_evidence;
        }
    }

    SettingsSnapshot Controller::snapshot() const noexcept
    {
        try
        {
            SettingsSnapshot result{
                .loaded = m_loaded,
                .dirty = m_dirty,
                .project_id = m_projectId,
                .applied_revision = m_applied.revision,
                .working_revision = m_working.revision,
                .evidence = m_evidence};
            if (!m_loaded)
                return result;

            result.display_name = m_working.display_name;
            result.action_count = static_cast<std::uint32_t>(
                m_working.actions.size());
            result.binding_count = static_cast<std::uint32_t>(
                m_working.bindings.size());
            result.bindings = make_binding_views(m_working);

            bool foundAxis{};
            for (const auto& binding : m_working.bindings)
            {
                if (binding.device
                    != project_input::BindingDevice::controller_axis)
                {
                    continue;
                }
                if (!foundAxis)
                {
                    foundAxis = true;
                    result.controller_dead_zone_q15 = binding.dead_zone_q15;
                }
                else if (result.controller_dead_zone_q15
                    != binding.dead_zone_q15)
                {
                    result.uniform_controller_dead_zone = false;
                }
            }
            return result;
        }
        catch (...)
        {
            return {
                .loaded = m_loaded,
                .dirty = m_dirty,
                .evidence = {
                    .code = SettingsCode::allocation_failure,
                    .validation =
                        project_input::ValidationCode::allocation_failure,
                    .edit =
                        project_input::ProfileEditCode::allocation_failure,
                    .codec = project_input::CodecCode::allocation_failure}};
        }
    }

    const project_input::ProfileSource* Controller::working_source()
        const noexcept
    {
        return m_loaded ? &m_working : nullptr;
    }
}
