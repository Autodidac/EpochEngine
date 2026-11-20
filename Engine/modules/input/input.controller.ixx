module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

export module input.controller;

export namespace epochengine::controller_input
{
    inline constexpr std::size_t maximum_controller_slots = 4u;

    enum class Button : std::uint8_t
    {
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
        dpad_right,
        count
    };

    enum class Axis : std::uint8_t
    {
        left_x,
        left_y,
        right_x,
        right_y,
        left_trigger,
        right_trigger,
        count
    };

    [[nodiscard]] constexpr std::string_view button_name(Button button) noexcept
    {
        switch (button)
        {
        case Button::south: return "South / A";
        case Button::east: return "East / B";
        case Button::west: return "West / X";
        case Button::north: return "North / Y";
        case Button::back: return "Back";
        case Button::guide: return "Guide";
        case Button::start: return "Start";
        case Button::left_stick: return "Left Stick";
        case Button::right_stick: return "Right Stick";
        case Button::left_shoulder: return "Left Shoulder";
        case Button::right_shoulder: return "Right Shoulder";
        case Button::dpad_up: return "D-pad Up";
        case Button::dpad_down: return "D-pad Down";
        case Button::dpad_left: return "D-pad Left";
        case Button::dpad_right: return "D-pad Right";
        case Button::count: break;
        }
        return "Invalid";
    }

    [[nodiscard]] constexpr std::string_view axis_name(Axis axis) noexcept
    {
        switch (axis)
        {
        case Axis::left_x: return "Left Stick X";
        case Axis::left_y: return "Left Stick Y";
        case Axis::right_x: return "Right Stick X";
        case Axis::right_y: return "Right Stick Y";
        case Axis::left_trigger: return "Left Trigger";
        case Axis::right_trigger: return "Right Trigger";
        case Axis::count: break;
        }
        return "Invalid";
    }

    struct DeviceHandle final
    {
        std::uint8_t slot{};
        std::uint32_t generation{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return slot < maximum_controller_slots && generation != 0u;
        }

        friend bool operator==(const DeviceHandle&, const DeviceHandle&) noexcept = default;
    };

    struct ProviderDeviceFrame final
    {
        bool connected{};
        std::uint64_t provider_id{};
        std::array<bool, static_cast<std::size_t>(Button::count)> buttons{};
        std::array<std::int16_t, static_cast<std::size_t>(Axis::count)> axes{};

        friend bool operator==(
            const ProviderDeviceFrame&,
            const ProviderDeviceFrame&) noexcept = default;
    };

    struct ProviderFrame final
    {
        bool provider_available{};
        std::array<ProviderDeviceFrame, maximum_controller_slots> devices{};

        friend bool operator==(
            const ProviderFrame&,
            const ProviderFrame&) noexcept = default;
    };

    struct DeviceSnapshot final
    {
        DeviceHandle handle{};
        bool connected{};
        std::uint64_t provider_id{};
        std::array<bool, static_cast<std::size_t>(Button::count)> held{};
        std::array<bool, static_cast<std::size_t>(Button::count)> pressed{};
        std::array<std::int16_t, static_cast<std::size_t>(Axis::count)> axes{};

        friend bool operator==(
            const DeviceSnapshot&,
            const DeviceSnapshot&) noexcept = default;
    };

    struct Snapshot final
    {
        std::uint64_t revision{};
        bool provider_available{};
        std::array<DeviceSnapshot, maximum_controller_slots> devices{};

        friend bool operator==(
            const Snapshot&,
            const Snapshot&) noexcept = default;
    };

    enum class ControllerCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_frame,
        revision_exhausted,
        generation_exhausted,
        unavailable,
        initialization_failed
    };

    [[nodiscard]] constexpr std::string_view controller_code_name(
        ControllerCode code) noexcept
    {
        switch (code)
        {
        case ControllerCode::ready: return "ready";
        case ControllerCode::unchanged: return "unchanged";
        case ControllerCode::invalid_frame: return "invalid_frame";
        case ControllerCode::revision_exhausted: return "revision_exhausted";
        case ControllerCode::generation_exhausted: return "generation_exhausted";
        case ControllerCode::unavailable: return "unavailable";
        case ControllerCode::initialization_failed: return "initialization_failed";
        }
        return "unknown";
    }

    struct PublishResult final
    {
        ControllerCode code{ControllerCode::invalid_frame};
        std::uint64_t revision{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ControllerCode::ready
                || code == ControllerCode::unchanged;
        }
    };

    struct Metrics final
    {
        std::uint64_t publish_requests{};
        std::uint64_t published_frames{};
        std::uint64_t unchanged_frames{};
        std::uint64_t rejected_frames{};
        std::uint64_t connections{};
        std::uint64_t disconnections{};
    };

    [[nodiscard]] bool valid_provider_frame(
        const ProviderFrame& frame) noexcept;
    [[nodiscard]] PublishResult publish_provider_frame(
        const ProviderFrame& frame) noexcept;
    [[nodiscard]] Snapshot snapshot() noexcept;
    [[nodiscard]] Metrics metrics() noexcept;
    [[nodiscard]] std::size_t connected_count() noexcept;
    [[nodiscard]] bool button_held(std::uint8_t slot, Button button) noexcept;
    [[nodiscard]] bool button_pressed(std::uint8_t slot, Button button) noexcept;
    [[nodiscard]] std::int16_t axis_value(std::uint8_t slot, Axis axis) noexcept;
    void reset() noexcept;

    [[nodiscard]] ControllerCode initialize_physical_input() noexcept;
    [[nodiscard]] ControllerCode poll_physical_input() noexcept;
    void shutdown_physical_input() noexcept;
    [[nodiscard]] std::string_view physical_provider_name() noexcept;

    enum class ContractFailure : std::uint8_t
    {
        none,
        invalid_frame,
        connect_edge,
        held_edge,
        unchanged,
        axis_sample,
        disconnect_generation,
        reconnect_generation,
        metrics
    };

    [[nodiscard]] ContractFailure controller_input_contract_failure() noexcept;
}
