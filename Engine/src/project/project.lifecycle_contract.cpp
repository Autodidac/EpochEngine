/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#include <cstdint>

import project.lifecycle;

namespace epochengine::project_lifecycle::contract
{
    static_assert(runtime_contract_failure() == ContractFailure::none);

    constexpr ProjectStamp kProject{0x706f6e67u, 9u};
    constexpr BuildInputEvidence kInputs{
        .project = kProject,
        .scene_generation = 12u,
        .input_generation = 15u};
    constexpr ActiveBuildEvidence kActive{
        .project = kProject,
        .input_generation = 15u,
        .build_generation = 4u};
    constexpr BuildArtifactEvidence kArtifact{
        .project = kProject,
        .input_generation = 15u,
        .build_generation = 4u,
        .artifact_generation = 20u,
        .verified = true};

    static_assert(kInputs.valid_for(kProject, 12u));
    static_assert(!kInputs.valid_for(
        ProjectStamp{0x706f6e68u, 9u}, 12u));
    static_assert(kActive.valid_for(kProject, kInputs));
    static_assert(!kActive.valid_for(
        ProjectStamp{kProject.project_key, kProject.generation + 1u}, kInputs));
    constexpr BuildInputEvidence kForeignInputs{
        .project = ProjectStamp{kProject.project_key + 1u, kProject.generation},
        .scene_generation = kInputs.scene_generation,
        .input_generation = kInputs.input_generation};
    static_assert(!kActive.valid_for(kProject, kForeignInputs));
    static_assert(kArtifact.valid_for(kProject, kInputs));
    static_assert(!kArtifact.valid_for(kProject, kForeignInputs));
    constexpr RuntimeEvidence kRuntime{
        .project = kProject,
        .artifact_generation = kArtifact.artifact_generation,
        .runtime_generation = 2u};
    static_assert(kRuntime.valid_for(kProject, kArtifact));
    constexpr BuildArtifactEvidence kUnverifiedArtifact{
        .project = kArtifact.project,
        .input_generation = kArtifact.input_generation,
        .build_generation = kArtifact.build_generation,
        .artifact_generation = kArtifact.artifact_generation,
        .verified = false};
    static_assert(!kRuntime.valid_for(kProject, kUnverifiedArtifact));

    [[nodiscard]] constexpr bool tracker_contract()
    {
        BuildEvidenceTracker tracker{};
        const std::uint64_t first = tracker.begin(
            kProject, kInputs.input_generation);
        if (first == 0u
            || !tracker.active_evidence().valid_for(kProject, kInputs)
            || !tracker.complete(first, 20u, true))
        {
            return false;
        }

        const std::uint64_t second = tracker.begin(
            kProject, kInputs.input_generation);
        return second != 0u
            && !tracker.complete(second, 20u, true)
            && tracker.complete(second, 21u, true)
            && tracker.artifact().artifact_generation == 21u;
    }

    static_assert(tracker_contract());
}
#if defined(EPOCH_PROJECT_LIFECYCLE_CONTRACT_MAIN)
int main()
{
    return epochengine::project_lifecycle::runtime_contract_failure()
            == epochengine::project_lifecycle::ContractFailure::none
        && epochengine::project_lifecycle::contract::tracker_contract()
        ? 0
        : 1;
}
#endif