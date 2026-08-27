/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

module project.input_controller;

import input.controller;
import project.input_profile;

namespace epochengine::project_input_controller
{
    namespace
    {
        [[nodiscard]] constexpr std::optional<controller_input::Button>
        physical_button(project_input::ControllerButton button) noexcept
        {
            using Physical = controller_input::Button;
            using Project = project_input::ControllerButton;
            switch (button)
            {
            case Project::south: return Physical::south;
            case Project::east: return Physical::east;
            case Project::west: return Physical::west;
            case Project::north: return Physical::north;
            case Project::back: return Physical::back;
            case Project::guide: return Physical::guide;
            case Project::start: return Physical::start;
            case Project::left_stick: return Physical::left_stick;
            case Project::right_stick: return Physical::right_stick;
            case Project::left_shoulder: return Physical::left_shoulder;
            case Project::right_shoulder: return Physical::right_shoulder;
            case Project::dpad_up: return Physical::dpad_up;
            case Project::dpad_down: return Physical::dpad_down;
            case Project::dpad_left: return Physical::dpad_left;
            case Project::dpad_right: return Physical::dpad_right;
            case Project::invalid: break;
            }
            return std::nullopt;
        }

        [[nodiscard]] constexpr std::optional<controller_input::Axis>
        physical_axis(project_input::ControllerAxis axis) noexcept
        {
            using Physical = controller_input::Axis;
            using Project = project_input::ControllerAxis;
            switch (axis)
            {
            case Project::left_x: return Physical::left_x;
            case Project::left_y: return Physical::left_y;
            case Project::right_x: return Physical::right_x;
            case Project::right_y: return Physical::right_y;
            case Project::left_trigger: return Physical::left_trigger;
            case Project::right_trigger: return Physical::right_trigger;
            case Project::invalid: break;
            }
            return std::nullopt;
        }

        [[nodiscard]] bool valid_snapshot(
            const controller_input::Snapshot& snapshot) noexcept
        {
            for (std::size_t slot = 0u;
                 slot < controller_input::maximum_controller_slots; ++slot)
            {
                const auto& device = snapshot.devices[slot];
                if (device.connected)
                {
                    if (!snapshot.provider_available
                        || !device.handle
                        || device.handle.slot != slot
                        || device.provider_id == 0u
                        || !std::all_of(
                            device.axes.begin(), device.axes.end(),
                            [](std::int16_t value) noexcept
                            {
                                return value !=
                                    (std::numeric_limits<std::int16_t>::min)();
                            }))
                    {
                        return false;
                    }
                    for (std::size_t button = 0u;
                         button < device.pressed.size(); ++button)
                    {
                        if (device.pressed[button] && !device.held[button])
                            return false;
                    }
                    for (std::size_t prior = 0u; prior < slot; ++prior)
                    {
                        if (snapshot.devices[prior].connected
                            && snapshot.devices[prior].provider_id
                                == device.provider_id)
                        {
                            return false;
                        }
                    }
                    continue;
                }

                if (device.provider_id != 0u
                    || std::any_of(
                        device.held.begin(), device.held.end(),
                        [](bool held) noexcept { return held; })
                    || std::any_of(
                        device.pressed.begin(), device.pressed.end(),
                        [](bool pressed) noexcept { return pressed; })
                    || std::any_of(
                        device.axes.begin(), device.axes.end(),
                        [](std::int16_t value) noexcept { return value != 0; })
                    || (device.handle.generation != 0u
                        && device.handle.slot != slot))
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool contains_button(
            const std::vector<project_input::ControllerButtonSample>& samples,
            std::uint8_t slot,
            project_input::ControllerButton button) noexcept
        {
            return std::any_of(
                samples.begin(), samples.end(),
                [&](const auto& sample) noexcept
                {
                    return sample.controller_slot == slot
                        && sample.button == button;
                });
        }

        [[nodiscard]] bool contains_axis(
            const std::vector<project_input::ControllerAxisSample>& samples,
            std::uint8_t slot,
            project_input::ControllerAxis axis) noexcept
        {
            return std::any_of(
                samples.begin(), samples.end(),
                [&](const auto& sample) noexcept
                {
                    return sample.controller_slot == slot
                        && sample.axis == axis;
                });
        }
    }

    SampleResult SnapshotSampler::sample(
        const project_input::CompiledInputProfile& profile,
        const controller_input::Snapshot& physical,
        project_input::InputSnapshot& destination,
        const project_input::ProfileLimits& limits) noexcept
    {
        if (project_input::validate_compiled_profile(profile, limits)
            != project_input::ValidationCode::ready)
        {
            return {SampleCode::invalid_profile, physical.revision};
        }
        if (!valid_snapshot(physical))
            return {SampleCode::invalid_snapshot, physical.revision};
        if (m_hasAcceptedRevision && physical.revision < m_acceptedRevision)
            return {SampleCode::stale_revision, physical.revision};

        const bool repeated = m_hasAcceptedRevision
            && physical.revision == m_acceptedRevision;
        try
        {
            auto buttons = destination.controller_buttons;
            auto axes = destination.controller_axes;
            for (const auto& binding : profile.bindings)
            {
                if (binding.controller_slot
                    >= controller_input::maximum_controller_slots)
                {
                    continue;
                }
                const auto& device = physical.devices[binding.controller_slot];
                if (!device.connected)
                    continue;

                if (binding.device
                    == project_input::BindingDevice::controller_button)
                {
                    const auto projectButton =
                        static_cast<project_input::ControllerButton>(
                            binding.code);
                    const auto button = physical_button(projectButton);
                    if (!button)
                        return {SampleCode::invalid_profile, physical.revision};
                    if (contains_button(
                            buttons,
                            binding.controller_slot,
                            projectButton))
                    {
                        return {
                            SampleCode::duplicate_input,
                            physical.revision};
                    }
                    const auto index = static_cast<std::size_t>(*button);
                    buttons.push_back({
                        .controller_slot = binding.controller_slot,
                        .button = projectButton,
                        .held = device.held[index],
                        .pressed = !repeated && device.pressed[index]});
                }
                else if (binding.device
                    == project_input::BindingDevice::controller_axis)
                {
                    const auto projectAxis =
                        static_cast<project_input::ControllerAxis>(
                            binding.code);
                    const auto axis = physical_axis(projectAxis);
                    if (!axis)
                        return {SampleCode::invalid_profile, physical.revision};
                    if (contains_axis(
                            axes,
                            binding.controller_slot,
                            projectAxis))
                    {
                        return {
                            SampleCode::duplicate_input,
                            physical.revision};
                    }
                    const auto index = static_cast<std::size_t>(*axis);
                    axes.push_back({
                        .controller_slot = binding.controller_slot,
                        .axis = projectAxis,
                        .value_q15 = device.axes[index]});
                }
            }

            const std::size_t maximum = limits.maximum_input_samples;
            if (destination.keyboard.size() > maximum
                || buttons.size() > maximum - destination.keyboard.size()
                || axes.size()
                    > maximum - destination.keyboard.size() - buttons.size())
            {
                return {
                    SampleCode::input_limit_exceeded,
                    physical.revision};
            }

            const std::uint32_t appendedButtons = static_cast<std::uint32_t>(
                buttons.size() - destination.controller_buttons.size());
            const std::uint32_t appendedAxes = static_cast<std::uint32_t>(
                axes.size() - destination.controller_axes.size());
            destination.controller_buttons.swap(buttons);
            destination.controller_axes.swap(axes);
            m_hasAcceptedRevision = true;
            m_acceptedRevision = physical.revision;
            return {
                repeated ? SampleCode::repeated_revision : SampleCode::ready,
                physical.revision,
                appendedButtons,
                appendedAxes};
        }
        catch (...)
        {
            return {SampleCode::allocation_failure, physical.revision};
        }
    }

    void SnapshotSampler::reset() noexcept
    {
        m_hasAcceptedRevision = false;
        m_acceptedRevision = 0u;
    }
}
