/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <string_view>

export module project.input_controller;

export import input.controller;
export import project.input_profile;

export namespace epochengine::project_input_controller
{
    enum class SampleCode : std::uint8_t
    {
        ready,
        repeated_revision,
        stale_revision,
        invalid_snapshot,
        invalid_profile,
        duplicate_input,
        input_limit_exceeded,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view sample_code_name(
        SampleCode code) noexcept
    {
        switch (code)
        {
        case SampleCode::ready: return "ready";
        case SampleCode::repeated_revision: return "repeated_revision";
        case SampleCode::stale_revision: return "stale_revision";
        case SampleCode::invalid_snapshot: return "invalid_snapshot";
        case SampleCode::invalid_profile: return "invalid_profile";
        case SampleCode::duplicate_input: return "duplicate_input";
        case SampleCode::input_limit_exceeded:
            return "input_limit_exceeded";
        case SampleCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct SampleResult final
    {
        SampleCode code{SampleCode::invalid_snapshot};
        std::uint64_t physical_revision{};
        std::uint32_t button_samples{};
        std::uint32_t axis_samples{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == SampleCode::ready
                || code == SampleCode::repeated_revision;
        }
    };

    class SnapshotSampler final
    {
    public:
        [[nodiscard]] SampleResult sample(
            const project_input::CompiledInputProfile& profile,
            const controller_input::Snapshot& physical,
            project_input::InputSnapshot& destination,
            const project_input::ProfileLimits& limits = {}) noexcept;

        void reset() noexcept;

        [[nodiscard]] bool has_accepted_revision() const noexcept
        {
            return m_hasAcceptedRevision;
        }

        [[nodiscard]] std::uint64_t accepted_revision() const noexcept
        {
            return m_acceptedRevision;
        }

    private:
        bool m_hasAcceptedRevision{};
        std::uint64_t m_acceptedRevision{};
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        first_edge,
        repeated_edge,
        axis_continuity,
        changed_revision,
        disconnected_device,
        stale_revision,
        invalid_generation,
        duplicate_destination,
        bounded_destination,
        reset
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "none";
        case ContractFailure::first_edge: return "first_edge";
        case ContractFailure::repeated_edge: return "repeated_edge";
        case ContractFailure::axis_continuity: return "axis_continuity";
        case ContractFailure::changed_revision: return "changed_revision";
        case ContractFailure::disconnected_device:
            return "disconnected_device";
        case ContractFailure::stale_revision: return "stale_revision";
        case ContractFailure::invalid_generation:
            return "invalid_generation";
        case ContractFailure::duplicate_destination:
            return "duplicate_destination";
        case ContractFailure::bounded_destination:
            return "bounded_destination";
        case ContractFailure::reset: return "reset";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure run_contract() noexcept;
}
