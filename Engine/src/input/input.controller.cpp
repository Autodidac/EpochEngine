module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>

module input.controller;

namespace epochengine::controller_input
{
    namespace
    {
        std::mutex g_mutex{};
        Snapshot g_snapshot{};
        Metrics g_metrics{};

        [[nodiscard]] constexpr bool valid_axis_value(
            std::int16_t value) noexcept
        {
            return value != (std::numeric_limits<std::int16_t>::min)();
        }

        [[nodiscard]] bool device_payload_clear(
            const ProviderDeviceFrame& device) noexcept
        {
            return device.provider_id == 0u
                && std::none_of(
                    device.buttons.begin(),
                    device.buttons.end(),
                    [](bool held) noexcept { return held; })
                && std::all_of(
                    device.axes.begin(),
                    device.axes.end(),
                    [](std::int16_t value) noexcept { return value == 0; });
        }

        [[nodiscard]] bool snapshot_payload_equal(
            const Snapshot& left,
            const Snapshot& right) noexcept
        {
            if (left.provider_available != right.provider_available)
                return false;
            for (std::size_t slot = 0u; slot < maximum_controller_slots; ++slot)
            {
                const auto& a = left.devices[slot];
                const auto& b = right.devices[slot];
                if (a.handle != b.handle
                    || a.connected != b.connected
                    || a.provider_id != b.provider_id
                    || a.held != b.held
                    || a.pressed != b.pressed
                    || a.axes != b.axes)
                {
                    return false;
                }
            }
            return true;
        }
    }

    bool valid_provider_frame(const ProviderFrame& frame) noexcept
    {
        for (std::size_t slot = 0u; slot < maximum_controller_slots; ++slot)
        {
            const auto& device = frame.devices[slot];
            if (!frame.provider_available && device.connected)
                return false;
            if (!device.connected)
            {
                if (!device_payload_clear(device))
                    return false;
                continue;
            }
            if (device.provider_id == 0u
                || !std::all_of(
                    device.axes.begin(),
                    device.axes.end(),
                    valid_axis_value))
            {
                return false;
            }
            for (std::size_t prior = 0u; prior < slot; ++prior)
            {
                if (frame.devices[prior].connected
                    && frame.devices[prior].provider_id == device.provider_id)
                {
                    return false;
                }
            }
        }
        return true;
    }

    PublishResult publish_provider_frame(const ProviderFrame& frame) noexcept
    {
        std::scoped_lock lock(g_mutex);
        ++g_metrics.publish_requests;
        if (!valid_provider_frame(frame))
        {
            ++g_metrics.rejected_frames;
            return {ControllerCode::invalid_frame, g_snapshot.revision};
        }

        for (std::size_t slot = 0u; slot < maximum_controller_slots; ++slot)
        {
            const auto& before = g_snapshot.devices[slot];
            const auto& incoming = frame.devices[slot];
            const bool identityChanged = before.connected != incoming.connected
                || (before.connected && incoming.connected
                    && before.provider_id != incoming.provider_id);
            if (identityChanged
                && before.handle.generation
                    == (std::numeric_limits<std::uint32_t>::max)())
            {
                ++g_metrics.rejected_frames;
                return {
                    ControllerCode::generation_exhausted,
                    g_snapshot.revision};
            }
        }

        Snapshot next = g_snapshot;
        next.provider_available = frame.provider_available;
        for (std::size_t slot = 0u; slot < maximum_controller_slots; ++slot)
        {
            const auto& before = g_snapshot.devices[slot];
            const auto& incoming = frame.devices[slot];
            auto& device = next.devices[slot];
            const bool sameIdentity = before.connected && incoming.connected
                && before.provider_id == incoming.provider_id;
            const bool identityChanged = before.connected != incoming.connected
                || (before.connected && incoming.connected && !sameIdentity);
            if (identityChanged)
            {
                ++device.handle.generation;
                if (incoming.connected)
                    ++g_metrics.connections;
                if (before.connected)
                    ++g_metrics.disconnections;
            }
            device.handle.slot = static_cast<std::uint8_t>(slot);
            device.connected = incoming.connected;
            device.provider_id = incoming.provider_id;
            device.pressed.fill(false);
            if (!incoming.connected)
            {
                device.held.fill(false);
                device.axes.fill(0);
                continue;
            }
            device.held = incoming.buttons;
            device.axes = incoming.axes;
            for (std::size_t button = 0u;
                 button < device.pressed.size(); ++button)
            {
                const bool previouslyHeld = sameIdentity
                    ? before.held[button]
                    : false;
                device.pressed[button] = incoming.buttons[button]
                    && !previouslyHeld;
            }
        }

        if (snapshot_payload_equal(next, g_snapshot))
        {
            ++g_metrics.unchanged_frames;
            return {ControllerCode::unchanged, g_snapshot.revision};
        }
        if (g_snapshot.revision
            == (std::numeric_limits<std::uint64_t>::max)())
        {
            ++g_metrics.rejected_frames;
            return {ControllerCode::revision_exhausted, g_snapshot.revision};
        }
        next.revision = g_snapshot.revision + 1u;
        g_snapshot = next;
        ++g_metrics.published_frames;
        return {ControllerCode::ready, g_snapshot.revision};
    }

    Snapshot snapshot() noexcept
    {
        std::scoped_lock lock(g_mutex);
        return g_snapshot;
    }

    Metrics metrics() noexcept
    {
        std::scoped_lock lock(g_mutex);
        return g_metrics;
    }

    std::size_t connected_count() noexcept
    {
        const Snapshot current = snapshot();
        return static_cast<std::size_t>(std::count_if(
            current.devices.begin(),
            current.devices.end(),
            [](const DeviceSnapshot& device) noexcept
            {
                return device.connected;
            }));
    }

    bool button_held(std::uint8_t slot, Button button) noexcept
    {
        if (slot >= maximum_controller_slots
            || button >= Button::count)
        {
            return false;
        }
        const Snapshot current = snapshot();
        return current.devices[slot].connected
            && current.devices[slot].held[static_cast<std::size_t>(button)];
    }

    bool button_pressed(std::uint8_t slot, Button button) noexcept
    {
        if (slot >= maximum_controller_slots
            || button >= Button::count)
        {
            return false;
        }
        const Snapshot current = snapshot();
        return current.devices[slot].connected
            && current.devices[slot].pressed[static_cast<std::size_t>(button)];
    }

    std::int16_t axis_value(std::uint8_t slot, Axis axis) noexcept
    {
        if (slot >= maximum_controller_slots || axis >= Axis::count)
            return 0;
        const Snapshot current = snapshot();
        return current.devices[slot].connected
            ? current.devices[slot].axes[static_cast<std::size_t>(axis)]
            : 0;
    }

    void reset() noexcept
    {
        std::scoped_lock lock(g_mutex);
        g_snapshot = {};
        g_metrics = {};
    }
}
