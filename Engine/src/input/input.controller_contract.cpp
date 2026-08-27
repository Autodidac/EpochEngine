module;

#include <cstddef>
#include <cstdint>

module input.controller;

namespace epochengine::controller_input
{
    ContractFailure controller_input_contract_failure() noexcept
    {
        reset();
        ProviderFrame invalid{};
        invalid.provider_available = true;
        invalid.devices[0].connected = true;
        invalid.devices[0].provider_id = 7u;
        invalid.devices[1].connected = true;
        invalid.devices[1].provider_id = 7u;
        if (valid_provider_frame(invalid)
            || publish_provider_frame(invalid).code
                != ControllerCode::invalid_frame)
        {
            return ContractFailure::invalid_frame;
        }

        ProviderFrame first{};
        first.provider_available = true;
        first.devices[0].connected = true;
        first.devices[0].provider_id = 42u;
        first.devices[0].buttons[static_cast<std::size_t>(Button::south)] = true;
        first.devices[0].axes[static_cast<std::size_t>(Axis::left_x)] = 12'345;
        const auto firstPublished = publish_provider_frame(first);
        const Snapshot firstSnapshot = snapshot();
        if (!firstPublished
            || firstSnapshot.revision != 1u
            || !firstSnapshot.provider_available
            || !firstSnapshot.devices[0].connected
            || firstSnapshot.devices[0].handle.generation != 1u
            || !button_held(0u, Button::south)
            || !button_pressed(0u, Button::south))
        {
            return ContractFailure::connect_edge;
        }
        if (axis_value(0u, Axis::left_x) != 12'345)
            return ContractFailure::axis_sample;

        const auto heldPublished = publish_provider_frame(first);
        const Snapshot heldSnapshot = snapshot();
        if (!heldPublished
            || heldSnapshot.revision != 2u
            || !button_held(0u, Button::south)
            || button_pressed(0u, Button::south))
        {
            return ContractFailure::held_edge;
        }
        const auto unchangedPublished = publish_provider_frame(first);
        if (unchangedPublished.code != ControllerCode::unchanged
            || snapshot().revision != heldSnapshot.revision)
        {
            return ContractFailure::unchanged;
        }

        ProviderFrame disconnected{};
        disconnected.provider_available = true;
        if (!publish_provider_frame(disconnected))
            return ContractFailure::disconnect_generation;
        const Snapshot disconnectedSnapshot = snapshot();
        if (disconnectedSnapshot.devices[0].connected
            || disconnectedSnapshot.devices[0].handle.generation != 2u
            || button_held(0u, Button::south)
            || axis_value(0u, Axis::left_x) != 0)
        {
            return ContractFailure::disconnect_generation;
        }

        ProviderFrame reconnected{};
        reconnected.provider_available = true;
        reconnected.devices[0].connected = true;
        reconnected.devices[0].provider_id = 84u;
        reconnected.devices[0].buttons[
            static_cast<std::size_t>(Button::east)] = true;
        if (!publish_provider_frame(reconnected))
            return ContractFailure::reconnect_generation;
        const Snapshot reconnectedSnapshot = snapshot();
        if (!reconnectedSnapshot.devices[0].connected
            || reconnectedSnapshot.devices[0].handle.generation != 3u
            || !button_pressed(0u, Button::east))
        {
            return ContractFailure::reconnect_generation;
        }

        const Metrics evidence = metrics();
        if (evidence.publish_requests != 6u
            || evidence.published_frames != 4u
            || evidence.unchanged_frames != 1u
            || evidence.rejected_frames != 1u
            || evidence.connections != 2u
            || evidence.disconnections != 1u)
        {
            return ContractFailure::metrics;
        }
        reset();
        return ContractFailure::none;
    }
}

#if defined(EPOCH_INPUT_CONTROLLER_CONTRACT_MAIN)
int main()
{
    return epochengine::controller_input::controller_input_contract_failure()
        == epochengine::controller_input::ContractFailure::none ? 0 : 1;
}
#endif
