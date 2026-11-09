// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Module facade; reviewed upstream exploration owners retain their MIT terms.
module;
#include "epoch/planet/simulation.hpp"
#include <stdexcept>
#include <stop_token>

export module planet.exploration;

export namespace epoch::extensions::planetary_terrain::exploration
{
    using ::epoch::planet::TravelMode;
    using ::epoch::planet::Camera;
    using ::epoch::planet::PilotInput;
    using ::epoch::planet::SessionState;
    using ::epoch::planet::Simulation;
    using ::epoch::planet::valid_state;

    enum class StepCode { advanced, cancelled, invalid_input };

    // The caller owns time, input, state and scheduling. Cancellation is checked
    // before the synchronous bounded tick; no partial tick is published.
    [[nodiscard]] inline StepCode advance(Simulation& simulation, double seconds,
        const PilotInput& input, std::stop_token cancellation = {})
    {
        if (cancellation.stop_requested()) return StepCode::cancelled;
        try
        {
            simulation.step(seconds, input);
            return StepCode::advanced;
        }
        catch (const std::invalid_argument&)
        {
            return StepCode::invalid_input;
        }
    }
}
