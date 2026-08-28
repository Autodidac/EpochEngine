/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module editor.project_input_settings;

export import project.input_profile;

export namespace epochengine::editor_project_input_settings
{
    enum class SettingsCode : std::uint8_t
    {
        ready,
        unchanged,
        not_loaded,
        invalid_project,
        invalid_profile,
        invalid_binding,
        binding_conflict,
        stale_revision,
        codec_failure,
        compile_failure,
        revision_exhausted,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view settings_code_name(
        SettingsCode code) noexcept
    {
        switch (code)
        {
        case SettingsCode::ready: return "ready";
        case SettingsCode::unchanged: return "unchanged";
        case SettingsCode::not_loaded: return "not_loaded";
        case SettingsCode::invalid_project: return "invalid_project";
        case SettingsCode::invalid_profile: return "invalid_profile";
        case SettingsCode::invalid_binding: return "invalid_binding";
        case SettingsCode::binding_conflict: return "binding_conflict";
        case SettingsCode::stale_revision: return "stale_revision";
        case SettingsCode::codec_failure: return "codec_failure";
        case SettingsCode::compile_failure: return "compile_failure";
        case SettingsCode::revision_exhausted:
            return "revision_exhausted";
        case SettingsCode::allocation_failure:
            return "allocation_failure";
        }
        return "unknown";
    }

    struct BindingConflict final
    {
        project_input::BindingId binding{};
        project_input::BindingId conflicting_binding{};
        project_input::BindingDevice device{
            project_input::BindingDevice::keyboard};
        std::uint16_t code{};
        std::uint8_t controller_slot{};
        project_input::ModifierMask required_modifiers{};
        project_input::ModifierMask forbidden_modifiers{};

        friend constexpr bool operator==(
            const BindingConflict&,
            const BindingConflict&) noexcept = default;
    };

    struct BindingView final
    {
        project_input::BindingId id{};
        project_input::ActionId action{};
        project_input::ActionSemantic semantic{
            project_input::ActionSemantic::invalid};
        project_input::ActionValueKind value_kind{
            project_input::ActionValueKind::button};
        project_input::BindingDevice device{
            project_input::BindingDevice::keyboard};
        std::uint16_t code{};
        std::uint8_t controller_slot{};
        project_input::AxisPolarity polarity{
            project_input::AxisPolarity::positive};
        project_input::AxisResponse response{
            project_input::AxisResponse::linear};
        project_input::ModifierMask required_modifiers{};
        project_input::ModifierMask forbidden_modifiers{};
        std::int32_t scale_q15{project_input::normalized_unit};
        std::uint16_t dead_zone_q15{};
        std::uint16_t saturation_q15{
            static_cast<std::uint16_t>(project_input::normalized_unit)};
        std::string action_label{};
        std::string source_label{};

        friend bool operator==(
            const BindingView&,
            const BindingView&) noexcept = default;
    };

    struct SettingsEvidence final
    {
        SettingsCode code{SettingsCode::not_loaded};
        project_input::ValidationCode validation{
            project_input::ValidationCode::invalid_profile};
        project_input::ProfileEditCode edit{
            project_input::ProfileEditCode::invalid_source};
        project_input::CodecCode codec{
            project_input::CodecCode::invalid_value};
        std::vector<BindingConflict> conflicts{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == SettingsCode::ready
                || code == SettingsCode::unchanged;
        }
    };

    struct SettingsSnapshot final
    {
        bool loaded{};
        bool dirty{};
        std::string project_id{};
        std::string display_name{};
        project_input::ProfileRevision applied_revision{};
        project_input::ProfileRevision working_revision{};
        std::uint32_t action_count{};
        std::uint32_t binding_count{};
        std::uint16_t controller_dead_zone_q15{};
        bool uniform_controller_dead_zone{true};
        std::vector<BindingView> bindings{};
        SettingsEvidence evidence{};
    };

    struct SavePlan final
    {
        SettingsCode code{SettingsCode::not_loaded};
        project_input::ValidationCode validation{
            project_input::ValidationCode::invalid_profile};
        project_input::CodecCode source_codec{
            project_input::CodecCode::invalid_value};
        project_input::CodecCode artifact_codec{
            project_input::CodecCode::invalid_value};
        std::string project_id{};
        std::string source_path{};
        std::string artifact_path{};
        project_input::ProfileRevision expected_base_revision{};
        project_input::ProfileRevision revision{};
        project_input::ProfileSource source{};
        project_input::CompiledInputProfile artifact{};
        std::vector<std::byte> source_bytes{};
        std::vector<std::byte> artifact_bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == SettingsCode::ready
                && validation == project_input::ValidationCode::ready
                && source_codec == project_input::CodecCode::ready
                && artifact_codec == project_input::CodecCode::ready
                && !source_bytes.empty()
                && !artifact_bytes.empty();
        }
    };

    [[nodiscard]] std::vector<BindingConflict> binding_conflicts(
        const project_input::ProfileSource& source) noexcept;

    class Controller final
    {
    public:
        explicit Controller(
            project_input::ProfileLimits limits = {}) noexcept;

        [[nodiscard]] SettingsEvidence load(
            std::string_view project_id,
            project_input::ProfileSource source) noexcept;
        [[nodiscard]] SettingsEvidence load_serialized(
            std::string_view project_id,
            std::span<const std::byte> bytes) noexcept;
        [[nodiscard]] SettingsEvidence reset_defaults() noexcept;
        [[nodiscard]] SettingsEvidence discard() noexcept;

        [[nodiscard]] SettingsEvidence rebind_keyboard(
            project_input::BindingId binding,
            project_input::KeyCode key) noexcept;
        [[nodiscard]] SettingsEvidence rebind_controller_button(
            project_input::BindingId binding,
            project_input::ControllerButton button,
            std::uint8_t controller_slot) noexcept;
        [[nodiscard]] SettingsEvidence rebind_controller_axis(
            project_input::BindingId binding,
            project_input::ControllerAxis axis,
            std::uint8_t controller_slot) noexcept;
        [[nodiscard]] SettingsEvidence set_controller_dead_zone(
            std::uint16_t dead_zone_q15) noexcept;

        [[nodiscard]] SavePlan prepare_save() const noexcept;
        [[nodiscard]] SettingsEvidence confirm_saved(
            project_input::ProfileRevision revision) noexcept;

        [[nodiscard]] SettingsSnapshot snapshot() const noexcept;
        [[nodiscard]] const project_input::ProfileSource*
            working_source() const noexcept;
        [[nodiscard]] bool loaded() const noexcept { return m_loaded; }
        [[nodiscard]] bool dirty() const noexcept { return m_dirty; }

    private:
        [[nodiscard]] SettingsEvidence accept_edit(
            project_input::ProfileEditResult edit,
            std::vector<BindingConflict> conflicts = {}) noexcept;
        [[nodiscard]] SettingsEvidence reject_conflicts(
            std::vector<BindingConflict> conflicts) noexcept;

        project_input::ProfileLimits m_limits{};
        std::string m_projectId{};
        project_input::ProfileSource m_applied{};
        project_input::ProfileSource m_working{};
        SettingsEvidence m_evidence{};
        bool m_loaded{};
        bool m_dirty{};
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        invalid_load,
        source_load,
        snapshot,
        conflict_evidence,
        non_destructive_conflict,
        staged_edit,
        save_plan,
        save_round_trip,
        stale_confirmation,
        confirmation,
        dead_zone,
        discard,
        reset,
        serialized_load
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "none";
        case ContractFailure::invalid_load: return "invalid_load";
        case ContractFailure::source_load: return "source_load";
        case ContractFailure::snapshot: return "snapshot";
        case ContractFailure::conflict_evidence: return "conflict_evidence";
        case ContractFailure::non_destructive_conflict:
            return "non_destructive_conflict";
        case ContractFailure::staged_edit: return "staged_edit";
        case ContractFailure::save_plan: return "save_plan";
        case ContractFailure::save_round_trip: return "save_round_trip";
        case ContractFailure::stale_confirmation:
            return "stale_confirmation";
        case ContractFailure::confirmation: return "confirmation";
        case ContractFailure::dead_zone: return "dead_zone";
        case ContractFailure::discard: return "discard";
        case ContractFailure::reset: return "reset";
        case ContractFailure::serialized_load: return "serialized_load";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure run_contract() noexcept;
}
