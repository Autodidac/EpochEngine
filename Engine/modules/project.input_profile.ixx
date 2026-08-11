/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module project.input_profile;

import project.asset_registry;

export namespace epochengine::project_input
{
    inline constexpr std::string_view canonical_source_path{
        "Assets/Config/input_profile.epochinput"};
    inline constexpr std::string_view canonical_artifact_path{
        "Library/InputProfiles/input_profile.epochinputc"};
    inline constexpr std::int32_t normalized_unit{32'767};

    struct ContentHash final
    {
        std::array<std::uint64_t, 4> words{};

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return words == std::array<std::uint64_t, 4>{};
        }

        friend constexpr bool operator==(
            const ContentHash& left,
            const ContentHash& right) noexcept
        {
            return left.words == right.words;
        }

        friend constexpr auto operator<=>(
            const ContentHash&, const ContentHash&) noexcept = default;
    };

    struct ProfileRevision final
    {
        ContentHash content{};
        std::uint64_t sequence{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return !content.empty() && sequence != 0u;
        }

        friend constexpr bool operator==(
            const ProfileRevision& left,
            const ProfileRevision& right) noexcept
        {
            return left.content == right.content
                && left.sequence == right.sequence;
        }

        friend constexpr auto operator<=>(
            const ProfileRevision&, const ProfileRevision&) noexcept = default;
    };

    struct ProfileId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value != 0u;
        }

        friend constexpr auto operator<=>(
            ProfileId, ProfileId) noexcept = default;
    };

    struct ActionId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value != 0u;
        }

        friend constexpr auto operator<=>(
            ActionId, ActionId) noexcept = default;
    };

    struct BindingId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value != 0u;
        }

        friend constexpr auto operator<=>(
            BindingId, BindingId) noexcept = default;
    };

    enum class ActionSemantic : std::uint8_t
    {
        invalid,
        move_x,
        move_y,
        jump,
        interact,
        pause,
        reset,
        count
    };

    enum class ActionValueKind : std::uint8_t
    {
        axis,
        button
    };

    enum class BindingDevice : std::uint8_t
    {
        keyboard,
        controller_button,
        controller_axis
    };

    enum class AxisPolarity : std::uint8_t
    {
        bipolar,
        positive,
        negative
    };

    enum class AxisResponse : std::uint8_t
    {
        linear,
        squared,
        cubic
    };

    enum class KeyCode : std::uint16_t
    {
        invalid = 0,
        a = 4,
        b,
        c,
        d,
        e,
        f,
        g,
        h,
        i,
        j,
        k,
        l,
        m,
        n,
        o,
        p,
        q,
        r,
        s,
        t,
        u,
        v,
        w,
        x,
        y,
        z,
        enter = 40,
        escape,
        backspace,
        tab,
        space,
        right = 79,
        left,
        down,
        up,
        left_control = 224,
        left_shift,
        left_alt,
        left_super,
        right_control,
        right_shift,
        right_alt,
        right_super
    };

    enum class ControllerButton : std::uint16_t
    {
        invalid,
        south,
        east,
        west,
        north,
        back,
        guide,
        start,
        left_stick,
        right_stick,
        left_shoulder,
        right_shoulder,
        dpad_up,
        dpad_down,
        dpad_left,
        dpad_right
    };

    enum class ControllerAxis : std::uint16_t
    {
        invalid,
        left_x,
        left_y,
        right_x,
        right_y,
        left_trigger,
        right_trigger
    };

    enum class Modifier : std::uint8_t
    {
        shift = 1u << 0u,
        control = 1u << 1u,
        alt = 1u << 2u,
        super = 1u << 3u
    };

    using ModifierMask = std::uint8_t;

    [[nodiscard]] constexpr ModifierMask modifier_mask(
        Modifier modifier) noexcept
    {
        return static_cast<ModifierMask>(modifier);
    }

    [[nodiscard]] constexpr ModifierMask operator|(
        Modifier left, Modifier right) noexcept
    {
        return static_cast<ModifierMask>(
            modifier_mask(left) | modifier_mask(right));
    }

    struct ActionDefinition final
    {
        ActionId id{};
        ActionSemantic semantic{ActionSemantic::invalid};
        ActionValueKind value_kind{ActionValueKind::button};

        friend constexpr auto operator<=>(
            const ActionDefinition&,
            const ActionDefinition&) noexcept = default;
    };

    struct BindingDefinition final
    {
        BindingId id{};
        ActionId action{};
        BindingDevice device{BindingDevice::keyboard};
        std::uint16_t code{};
        std::uint8_t controller_slot{};
        AxisPolarity polarity{AxisPolarity::positive};
        AxisResponse response{AxisResponse::linear};
        ModifierMask required_modifiers{};
        ModifierMask forbidden_modifiers{};
        std::int32_t scale_q15{normalized_unit};
        std::uint16_t dead_zone_q15{};
        std::uint16_t saturation_q15{
            static_cast<std::uint16_t>(normalized_unit)};

        friend constexpr auto operator<=>(
            const BindingDefinition&,
            const BindingDefinition&) noexcept = default;
    };

    struct ProfileSource final
    {
        ProfileId id{};
        std::string display_name{};
        ProfileRevision revision{};
        std::vector<ActionDefinition> actions{};
        std::vector<BindingDefinition> bindings{};

        friend bool operator==(
            const ProfileSource&,
            const ProfileSource&) noexcept = default;
    };

    struct ProfileLimits final
    {
        std::uint32_t maximum_actions{16u};
        std::uint32_t maximum_bindings{128u};
        std::uint32_t maximum_bindings_per_action{24u};
        std::uint32_t maximum_display_name_bytes{96u};
        std::uint32_t maximum_input_samples{256u};
        std::uint32_t maximum_controller_slots{8u};
        std::uint64_t maximum_serialized_bytes{1024u * 1024u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_actions >= 6u && maximum_actions <= 256u
                && maximum_bindings >= 6u
                && maximum_bindings <= 4096u
                && maximum_bindings_per_action != 0u
                && maximum_bindings_per_action <= maximum_bindings
                && maximum_display_name_bytes != 0u
                && maximum_display_name_bytes <= 4096u
                && maximum_input_samples != 0u
                && maximum_input_samples <= 4096u
                && maximum_controller_slots != 0u
                && maximum_controller_slots <= 32u
                && maximum_serialized_bytes >= 256u
                && maximum_serialized_bytes <= 64u * 1024u * 1024u;
        }
    };

    enum class ValidationCode : std::uint8_t
    {
        ready,
        invalid_limits,
        invalid_profile,
        invalid_name,
        invalid_revision,
        action_limit_exceeded,
        binding_limit_exceeded,
        missing_action,
        duplicate_action,
        invalid_action,
        duplicate_binding,
        duplicate_binding_source,
        invalid_binding,
        binding_action_missing,
        binding_action_limit_exceeded,
        content_hash_mismatch,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view validation_code_name(
        ValidationCode code) noexcept
    {
        switch (code)
        {
        case ValidationCode::ready: return "ready";
        case ValidationCode::invalid_limits: return "invalid_limits";
        case ValidationCode::invalid_profile: return "invalid_profile";
        case ValidationCode::invalid_name: return "invalid_name";
        case ValidationCode::invalid_revision: return "invalid_revision";
        case ValidationCode::action_limit_exceeded:
            return "action_limit_exceeded";
        case ValidationCode::binding_limit_exceeded:
            return "binding_limit_exceeded";
        case ValidationCode::missing_action: return "missing_action";
        case ValidationCode::duplicate_action: return "duplicate_action";
        case ValidationCode::invalid_action: return "invalid_action";
        case ValidationCode::duplicate_binding: return "duplicate_binding";
        case ValidationCode::duplicate_binding_source:
            return "duplicate_binding_source";
        case ValidationCode::invalid_binding: return "invalid_binding";
        case ValidationCode::binding_action_missing:
            return "binding_action_missing";
        case ValidationCode::binding_action_limit_exceeded:
            return "binding_action_limit_exceeded";
        case ValidationCode::content_hash_mismatch:
            return "content_hash_mismatch";
        case ValidationCode::allocation_failure:
            return "allocation_failure";
        }
        return "unknown";
    }

    [[nodiscard]] constexpr std::string_view action_semantic_name(
        ActionSemantic semantic) noexcept
    {
        switch (semantic)
        {
        case ActionSemantic::move_x: return "move_x";
        case ActionSemantic::move_y: return "move_y";
        case ActionSemantic::jump: return "jump";
        case ActionSemantic::interact: return "interact";
        case ActionSemantic::pause: return "pause";
        case ActionSemantic::reset: return "reset";
        case ActionSemantic::invalid:
        case ActionSemantic::count:
            break;
        }
        return "invalid";
    }

    [[nodiscard]] ProfileId stable_profile_id(
        std::string_view stableName) noexcept;
    [[nodiscard]] ActionId stable_action_id(
        ActionSemantic semantic) noexcept;
    [[nodiscard]] BindingId stable_binding_id(
        std::string_view stableName) noexcept;
    [[nodiscard]] ActionValueKind expected_value_kind(
        ActionSemantic semantic) noexcept;
    [[nodiscard]] ContentHash profile_content_hash(
        const ProfileSource& source) noexcept;
    [[nodiscard]] ValidationCode validate_profile_source(
        const ProfileSource& source,
        const ProfileLimits& limits = {}) noexcept;
    [[nodiscard]] ValidationCode seal_profile_source(
        ProfileSource& source,
        const ProfileLimits& limits = {}) noexcept;
    [[nodiscard]] ProfileSource make_legacy_default_profile(
        std::uint64_t revisionSequence = 1u) noexcept;

    enum class CodecCode : std::uint8_t
    {
        ready,
        invalid_value,
        serialized_budget_exceeded,
        malformed_data,
        unsupported_version,
        integrity_failure,
        trailing_data,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view codec_code_name(
        CodecCode code) noexcept
    {
        switch (code)
        {
        case CodecCode::ready: return "ready";
        case CodecCode::invalid_value: return "invalid_value";
        case CodecCode::serialized_budget_exceeded:
            return "serialized_budget_exceeded";
        case CodecCode::malformed_data: return "malformed_data";
        case CodecCode::unsupported_version: return "unsupported_version";
        case CodecCode::integrity_failure: return "integrity_failure";
        case CodecCode::trailing_data: return "trailing_data";
        case CodecCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct SerializedProfile final
    {
        CodecCode code{CodecCode::invalid_value};
        std::vector<std::byte> bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == CodecCode::ready && !bytes.empty();
        }
    };

    struct DeserializedProfile final
    {
        CodecCode code{CodecCode::malformed_data};
        ProfileSource source{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == CodecCode::ready;
        }
    };

    [[nodiscard]] SerializedProfile serialize_profile_source(
        const ProfileSource& source,
        const ProfileLimits& limits = {}) noexcept;
    [[nodiscard]] DeserializedProfile deserialize_profile_source(
        std::span<const std::byte> bytes,
        const ProfileLimits& limits = {}) noexcept;

    struct CompiledInputProfile final
    {
        std::uint64_t project_key{};
        ProfileId profile_id{};
        std::string display_name{};
        ProfileRevision source_revision{};
        ContentHash artifact_hash{};
        std::vector<ActionDefinition> actions{};
        std::vector<BindingDefinition> bindings{};

        friend bool operator==(
            const CompiledInputProfile&,
            const CompiledInputProfile&) noexcept = default;
    };

    struct CompiledProfileResult final
    {
        ValidationCode code{ValidationCode::invalid_profile};
        CompiledInputProfile artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ValidationCode::ready;
        }
    };

    [[nodiscard]] ContentHash compiled_profile_hash(
        const CompiledInputProfile& artifact) noexcept;
    [[nodiscard]] ValidationCode validate_compiled_profile(
        const CompiledInputProfile& artifact,
        const ProfileLimits& limits = {}) noexcept;
    [[nodiscard]] CompiledProfileResult compile_profile(
        std::string_view projectId,
        const ProfileSource& source,
        const ProfileLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;
    [[nodiscard]] SerializedProfile serialize_compiled_profile(
        const CompiledInputProfile& artifact,
        const ProfileLimits& limits = {}) noexcept;

    struct DeserializedCompiledProfile final
    {
        CodecCode code{CodecCode::malformed_data};
        CompiledInputProfile artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == CodecCode::ready;
        }
    };

    [[nodiscard]] DeserializedCompiledProfile deserialize_compiled_profile(
        std::span<const std::byte> bytes,
        const ProfileLimits& limits = {}) noexcept;

    struct KeyboardSample final
    {
        KeyCode key{KeyCode::invalid};
        bool held{};
        bool pressed{};

        friend constexpr auto operator<=>(
            const KeyboardSample&,
            const KeyboardSample&) noexcept = default;
    };

    struct ControllerButtonSample final
    {
        std::uint8_t controller_slot{};
        ControllerButton button{ControllerButton::invalid};
        bool held{};
        bool pressed{};

        friend constexpr auto operator<=>(
            const ControllerButtonSample&,
            const ControllerButtonSample&) noexcept = default;
    };

    struct ControllerAxisSample final
    {
        std::uint8_t controller_slot{};
        ControllerAxis axis{ControllerAxis::invalid};
        std::int16_t value_q15{};

        friend constexpr auto operator<=>(
            const ControllerAxisSample&,
            const ControllerAxisSample&) noexcept = default;
    };

    struct InputSnapshot final
    {
        std::uint64_t frame_index{};
        ModifierMask modifiers{};
        std::vector<KeyboardSample> keyboard{};
        std::vector<ControllerButtonSample> controller_buttons{};
        std::vector<ControllerAxisSample> controller_axes{};

        friend bool operator==(
            const InputSnapshot&,
            const InputSnapshot&) noexcept = default;
    };

    struct ActionValue final
    {
        ActionId action{};
        std::int32_t value_q15{};
        bool pressed{};

        friend constexpr auto operator<=>(
            const ActionValue&,
            const ActionValue&) noexcept = default;
    };

    enum class EvaluationCode : std::uint8_t
    {
        ready,
        invalid_artifact,
        input_limit_exceeded,
        duplicate_input,
        invalid_input,
        allocation_failure
    };

    struct ActionFrame final
    {
        EvaluationCode code{EvaluationCode::invalid_artifact};
        std::uint64_t frame_index{};
        std::vector<ActionValue> actions{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == EvaluationCode::ready;
        }

        friend bool operator==(
            const ActionFrame&,
            const ActionFrame&) noexcept = default;
    };

    [[nodiscard]] std::int32_t apply_axis_response(
        std::int32_t rawValueQ15,
        const BindingDefinition& binding) noexcept;
    [[nodiscard]] ActionFrame evaluate_action_frame(
        const CompiledInputProfile& artifact,
        const InputSnapshot& input,
        const ProfileLimits& limits = {}) noexcept;
    [[nodiscard]] const ActionValue* find_action(
        const ActionFrame& frame,
        ActionSemantic semantic,
        const CompiledInputProfile& artifact) noexcept;

    struct StoreLimits final
    {
        ProfileLimits profile{};
        project_assets::RegistryLimits registry{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return profile.valid() && registry.valid();
        }
    };

    enum class StoreCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_store,
        invalid_value,
        not_found,
        stale_revision,
        revision_conflict,
        directory_failure,
        read_failure,
        write_failure,
        integrity_failure,
        atomic_replace_failure,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view store_code_name(
        StoreCode code) noexcept
    {
        switch (code)
        {
        case StoreCode::ready: return "ready";
        case StoreCode::unchanged: return "unchanged";
        case StoreCode::invalid_store: return "invalid_store";
        case StoreCode::invalid_value: return "invalid_value";
        case StoreCode::not_found: return "not_found";
        case StoreCode::stale_revision: return "stale_revision";
        case StoreCode::revision_conflict: return "revision_conflict";
        case StoreCode::directory_failure: return "directory_failure";
        case StoreCode::read_failure: return "read_failure";
        case StoreCode::write_failure: return "write_failure";
        case StoreCode::integrity_failure: return "integrity_failure";
        case StoreCode::atomic_replace_failure:
            return "atomic_replace_failure";
        case StoreCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct StoreMetrics final
    {
        std::uint64_t source_save_requests{};
        std::uint64_t source_saves{};
        std::uint64_t source_load_requests{};
        std::uint64_t source_loads{};
        std::uint64_t artifact_save_requests{};
        std::uint64_t artifact_saves{};
        std::uint64_t artifact_load_requests{};
        std::uint64_t artifact_loads{};
        std::uint64_t unchanged_writes{};
        std::uint64_t rejected_operations{};
        std::uint64_t bytes_read{};
        std::uint64_t bytes_written{};
    };

    struct StoredProfile final
    {
        StoreCode code{StoreCode::invalid_store};
        std::filesystem::path storage_path{};
        ProfileRevision revision{};
        std::uint64_t serialized_bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == StoreCode::ready
                    || code == StoreCode::unchanged)
                && !storage_path.empty() && revision
                && serialized_bytes != 0u;
        }
    };

    struct LoadedProfile final
    {
        StoreCode code{StoreCode::invalid_store};
        std::filesystem::path storage_path{};
        ProfileSource source{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == StoreCode::ready && !storage_path.empty();
        }
    };

    struct StoredArtifact final
    {
        StoreCode code{StoreCode::invalid_store};
        std::filesystem::path storage_path{};
        ContentHash artifact_hash{};
        ProfileRevision source_revision{};
        std::uint64_t serialized_bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == StoreCode::ready
                    || code == StoreCode::unchanged)
                && !storage_path.empty() && !artifact_hash.empty()
                && source_revision && serialized_bytes != 0u;
        }
    };

    struct LoadedArtifact final
    {
        StoreCode code{StoreCode::invalid_store};
        std::filesystem::path storage_path{};
        CompiledInputProfile artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == StoreCode::ready && !storage_path.empty();
        }
    };

    class ProjectInputProfileStore final
    {
    public:
        ProjectInputProfileStore(
            std::string projectId,
            std::filesystem::path projectRoot,
            StoreLimits limits = {}) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] std::string_view project_id() const noexcept;
        [[nodiscard]] std::uint64_t project_key() const noexcept;
        [[nodiscard]] const std::filesystem::path& project_root() const noexcept;
        [[nodiscard]] std::filesystem::path source_path() const;
        [[nodiscard]] std::filesystem::path artifact_path() const;
        [[nodiscard]] const StoreLimits& limits() const noexcept;
        [[nodiscard]] StoreMetrics metrics() const noexcept;

        [[nodiscard]] StoredProfile save_source(
            const ProfileSource& source) noexcept;
        [[nodiscard]] LoadedProfile load_source() noexcept;
        [[nodiscard]] StoredArtifact publish_artifact(
            const CompiledInputProfile& artifact) noexcept;
        [[nodiscard]] LoadedArtifact load_artifact() noexcept;

    private:
        [[nodiscard]] StoredProfile reject_source(StoreCode code) noexcept;
        [[nodiscard]] LoadedProfile reject_source_load(StoreCode code) noexcept;
        [[nodiscard]] StoredArtifact reject_artifact(StoreCode code) noexcept;
        [[nodiscard]] LoadedArtifact reject_artifact_load(
            StoreCode code) noexcept;

        std::string project_id_{};
        std::filesystem::path project_root_{};
        StoreLimits limits_{};
        std::uint64_t project_key_{};
        StoreMetrics metrics_{};
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        legacy_default,
        source_roundtrip,
        artifact_roundtrip,
        malformed_rejection,
        duplicate_rejection,
        dead_zone,
        deterministic_evaluation,
        modifier_evaluation,
        replay_identity,
        temporary_root,
        source_save_reopen,
        artifact_save_reopen,
        stale_revision,
        malformed_store,
        metrics
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::legacy_default: return "legacy_default";
        case ContractFailure::source_roundtrip: return "source_roundtrip";
        case ContractFailure::artifact_roundtrip: return "artifact_roundtrip";
        case ContractFailure::malformed_rejection:
            return "malformed_rejection";
        case ContractFailure::duplicate_rejection:
            return "duplicate_rejection";
        case ContractFailure::dead_zone: return "dead_zone";
        case ContractFailure::deterministic_evaluation:
            return "deterministic_evaluation";
        case ContractFailure::modifier_evaluation:
            return "modifier_evaluation";
        case ContractFailure::replay_identity: return "replay_identity";
        case ContractFailure::temporary_root: return "temporary_root";
        case ContractFailure::source_save_reopen:
            return "source_save_reopen";
        case ContractFailure::artifact_save_reopen:
            return "artifact_save_reopen";
        case ContractFailure::stale_revision: return "stale_revision";
        case ContractFailure::malformed_store: return "malformed_store";
        case ContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure
        project_input_profile_contract_failure() noexcept;
}
