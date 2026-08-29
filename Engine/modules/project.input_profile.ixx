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

    [[nodiscard]] constexpr bool valid_key_code(KeyCode key) noexcept
    {
        const auto code = static_cast<std::uint16_t>(key);
        return (code >= static_cast<std::uint16_t>(KeyCode::a)
                && code <= static_cast<std::uint16_t>(KeyCode::z))
            || (code >= static_cast<std::uint16_t>(KeyCode::enter)
                && code <= static_cast<std::uint16_t>(KeyCode::space))
            || (code >= static_cast<std::uint16_t>(KeyCode::right)
                && code <= static_cast<std::uint16_t>(KeyCode::up))
            || (code >= static_cast<std::uint16_t>(KeyCode::left_control)
                && code <= static_cast<std::uint16_t>(KeyCode::right_super));
    }

    [[nodiscard]] constexpr std::string_view key_code_name(
        KeyCode key) noexcept
    {
        switch (key)
        {
        case KeyCode::a: return "A";
        case KeyCode::b: return "B";
        case KeyCode::c: return "C";
        case KeyCode::d: return "D";
        case KeyCode::e: return "E";
        case KeyCode::f: return "F";
        case KeyCode::g: return "G";
        case KeyCode::h: return "H";
        case KeyCode::i: return "I";
        case KeyCode::j: return "J";
        case KeyCode::k: return "K";
        case KeyCode::l: return "L";
        case KeyCode::m: return "M";
        case KeyCode::n: return "N";
        case KeyCode::o: return "O";
        case KeyCode::p: return "P";
        case KeyCode::q: return "Q";
        case KeyCode::r: return "R";
        case KeyCode::s: return "S";
        case KeyCode::t: return "T";
        case KeyCode::u: return "U";
        case KeyCode::v: return "V";
        case KeyCode::w: return "W";
        case KeyCode::x: return "X";
        case KeyCode::y: return "Y";
        case KeyCode::z: return "Z";
        case KeyCode::enter: return "Enter";
        case KeyCode::escape: return "Escape";
        case KeyCode::backspace: return "Backspace";
        case KeyCode::tab: return "Tab";
        case KeyCode::space: return "Space";
        case KeyCode::right: return "Right";
        case KeyCode::left: return "Left";
        case KeyCode::down: return "Down";
        case KeyCode::up: return "Up";
        case KeyCode::left_control: return "Left Ctrl";
        case KeyCode::left_shift: return "Left Shift";
        case KeyCode::left_alt: return "Left Alt";
        case KeyCode::left_super: return "Left Super";
        case KeyCode::right_control: return "Right Ctrl";
        case KeyCode::right_shift: return "Right Shift";
        case KeyCode::right_alt: return "Right Alt";
        case KeyCode::right_super: return "Right Super";
        case KeyCode::invalid:
            break;
        }
        return "Invalid";
    }

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

    [[nodiscard]] constexpr std::string_view controller_button_name(
        ControllerButton button) noexcept
    {
        switch (button)
        {
        case ControllerButton::south: return "South / A";
        case ControllerButton::east: return "East / B";
        case ControllerButton::west: return "West / X";
        case ControllerButton::north: return "North / Y";
        case ControllerButton::back: return "Back";
        case ControllerButton::guide: return "Guide";
        case ControllerButton::start: return "Start";
        case ControllerButton::left_stick: return "Left Stick";
        case ControllerButton::right_stick: return "Right Stick";
        case ControllerButton::left_shoulder: return "Left Shoulder";
        case ControllerButton::right_shoulder: return "Right Shoulder";
        case ControllerButton::dpad_up: return "D-pad Up";
        case ControllerButton::dpad_down: return "D-pad Down";
        case ControllerButton::dpad_left: return "D-pad Left";
        case ControllerButton::dpad_right: return "D-pad Right";
        case ControllerButton::invalid:
            break;
        }
        return "Invalid";
    }

    [[nodiscard]] constexpr std::string_view controller_axis_name(
        ControllerAxis axis) noexcept
    {
        switch (axis)
        {
        case ControllerAxis::left_x: return "Left Stick X";
        case ControllerAxis::left_y: return "Left Stick Y";
        case ControllerAxis::right_x: return "Right Stick X";
        case ControllerAxis::right_y: return "Right Stick Y";
        case ControllerAxis::left_trigger: return "Left Trigger";
        case ControllerAxis::right_trigger: return "Right Trigger";
        case ControllerAxis::invalid:
            break;
        }
        return "Invalid";
    }

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

    [[nodiscard]] constexpr ActionSemantic action_semantic_from_name(
        std::string_view name) noexcept
    {
        if (name == "move_x") return ActionSemantic::move_x;
        if (name == "move_y") return ActionSemantic::move_y;
        if (name == "jump") return ActionSemantic::jump;
        if (name == "interact") return ActionSemantic::interact;
        if (name == "pause") return ActionSemantic::pause;
        if (name == "reset") return ActionSemantic::reset;
        return ActionSemantic::invalid;
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

    enum class ProfileEditCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_source,
        invalid_binding,
        unsupported_device,
        invalid_key,
        invalid_controller_button,
        invalid_controller_axis,
        invalid_controller_slot,
        invalid_dead_zone,
        revision_exhausted,
        validation_failed,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view profile_edit_code_name(
        ProfileEditCode code) noexcept
    {
        switch (code)
        {
        case ProfileEditCode::ready: return "ready";
        case ProfileEditCode::unchanged: return "unchanged";
        case ProfileEditCode::invalid_source: return "invalid_source";
        case ProfileEditCode::invalid_binding: return "invalid_binding";
        case ProfileEditCode::unsupported_device: return "unsupported_device";
        case ProfileEditCode::invalid_key: return "invalid_key";
        case ProfileEditCode::invalid_controller_button:
            return "invalid_controller_button";
        case ProfileEditCode::invalid_controller_axis:
            return "invalid_controller_axis";
        case ProfileEditCode::invalid_controller_slot:
            return "invalid_controller_slot";
        case ProfileEditCode::invalid_dead_zone: return "invalid_dead_zone";
        case ProfileEditCode::revision_exhausted: return "revision_exhausted";
        case ProfileEditCode::validation_failed: return "validation_failed";
        case ProfileEditCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct ProfileEditResult final
    {
        ProfileEditCode code{ProfileEditCode::invalid_source};
        ValidationCode validation{ValidationCode::invalid_profile};
        ProfileSource source{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ProfileEditCode::ready
                || code == ProfileEditCode::unchanged;
        }
    };

    [[nodiscard]] ProfileEditResult rebind_keyboard(
        ProfileSource source,
        BindingId binding,
        KeyCode key,
        const ProfileLimits& limits = {}) noexcept;
    [[nodiscard]] ProfileEditResult rebind_controller_button(
        ProfileSource source,
        BindingId binding,
        ControllerButton button,
        std::uint8_t controllerSlot,
        const ProfileLimits& limits = {}) noexcept;
    [[nodiscard]] ProfileEditResult rebind_controller_axis(
        ProfileSource source,
        BindingId binding,
        ControllerAxis axis,
        std::uint8_t controllerSlot,
        const ProfileLimits& limits = {}) noexcept;
    [[nodiscard]] ProfileEditResult set_controller_dead_zone(
        ProfileSource source,
        std::uint16_t deadZoneQ15,
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

    struct ActionImpulse final
    {
        ActionSemantic semantic{ActionSemantic::invalid};
        std::int32_t value_q15{};
        bool pressed{};

        friend constexpr auto operator<=>(
            const ActionImpulse&,
            const ActionImpulse&) noexcept = default;
    };

    enum class InjectionCode : std::uint8_t
    {
        ready,
        invalid_artifact,
        invalid_frame,
        action_limit_exceeded,
        invalid_action,
        action_missing,
        duplicate_action
    };

    [[nodiscard]] constexpr std::string_view injection_code_name(
        InjectionCode code) noexcept
    {
        switch (code)
        {
        case InjectionCode::ready: return "ready";
        case InjectionCode::invalid_artifact: return "invalid_artifact";
        case InjectionCode::invalid_frame: return "invalid_frame";
        case InjectionCode::action_limit_exceeded:
            return "action_limit_exceeded";
        case InjectionCode::invalid_action: return "invalid_action";
        case InjectionCode::action_missing: return "action_missing";
        case InjectionCode::duplicate_action: return "duplicate_action";
        }
        return "unknown";
    }

    [[nodiscard]] std::int32_t apply_axis_response(
        std::int32_t rawValueQ15,
        const BindingDefinition& binding) noexcept;
    [[nodiscard]] ActionFrame evaluate_action_frame(
        const CompiledInputProfile& artifact,
        const InputSnapshot& input,
        const ProfileLimits& limits = {}) noexcept;
    [[nodiscard]] InjectionCode inject_action_impulses(
        const CompiledInputProfile& artifact,
        ActionFrame& frame,
        std::span<const ActionImpulse> impulses,
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
        std::uint64_t paired_save_requests{};
        std::uint64_t paired_saves{};
        std::uint64_t paired_partial_saves{};
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

    struct StoredProfilePair final
    {
        StoreCode code{StoreCode::invalid_store};
        StoredProfile source{};
        StoredArtifact artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == StoreCode::ready
                    || code == StoreCode::unchanged)
                && source && artifact;
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

        [[nodiscard]] StoredProfilePair publish_source_and_artifact(
            const ProfileSource& source,
            const CompiledInputProfile& artifact) noexcept;
        [[nodiscard]] StoredProfile save_source(
            const ProfileSource& source) noexcept;
        [[nodiscard]] LoadedProfile load_source() noexcept;
        [[nodiscard]] StoredArtifact publish_artifact(
            const CompiledInputProfile& artifact) noexcept;
        [[nodiscard]] LoadedArtifact load_artifact() noexcept;

    private:
        [[nodiscard]] StoredProfilePair reject_pair(StoreCode code) noexcept;
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
        profile_rebind,
        controller_rebind,
        dead_zone,
        dead_zone_edit,
        deterministic_evaluation,
        action_injection,
        modifier_evaluation,
        replay_identity,
        temporary_root,
        source_save_reopen,
        artifact_save_reopen,
        paired_save_reopen,
        paired_mismatch_rejection,
        paired_stage_failure,
        paired_stale_conflict,
        paired_disposable_artifact_repair,
        paired_partial_save_reopen,
        paired_artifact_ahead_repair,
        paired_metrics,
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
        case ContractFailure::profile_rebind: return "profile_rebind";
        case ContractFailure::controller_rebind: return "controller_rebind";
        case ContractFailure::dead_zone: return "dead_zone";
        case ContractFailure::dead_zone_edit: return "dead_zone_edit";
        case ContractFailure::deterministic_evaluation:
            return "deterministic_evaluation";
        case ContractFailure::action_injection: return "action_injection";
        case ContractFailure::modifier_evaluation:
            return "modifier_evaluation";
        case ContractFailure::replay_identity: return "replay_identity";
        case ContractFailure::temporary_root: return "temporary_root";
        case ContractFailure::source_save_reopen:
            return "source_save_reopen";
        case ContractFailure::artifact_save_reopen:
            return "artifact_save_reopen";
        case ContractFailure::paired_save_reopen:
            return "paired_save_reopen";
        case ContractFailure::paired_mismatch_rejection:
            return "paired_mismatch_rejection";
        case ContractFailure::paired_stage_failure:
            return "paired_stage_failure";
        case ContractFailure::paired_stale_conflict:
            return "paired_stale_conflict";
        case ContractFailure::paired_disposable_artifact_repair:
            return "paired_disposable_artifact_repair";
        case ContractFailure::paired_partial_save_reopen:
            return "paired_partial_save_reopen";
        case ContractFailure::paired_artifact_ahead_repair:
            return "paired_artifact_ahead_repair";
        case ContractFailure::paired_metrics: return "paired_metrics";
        case ContractFailure::stale_revision: return "stale_revision";
        case ContractFailure::malformed_store: return "malformed_store";
        case ContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure
        project_input_profile_contract_failure() noexcept;
}
