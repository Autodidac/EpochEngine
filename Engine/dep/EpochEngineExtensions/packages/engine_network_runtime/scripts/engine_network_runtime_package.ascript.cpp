// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "../../_api/epoch_package_script_api.hpp"

namespace
{
    using epoch::packages::script::Operation;
    using epoch::packages::script::OperationKind;
    using epoch::packages::script::PackageScriptPlan;

    constexpr Operation kOperations[] = {
        { OperationKind::HumanApprovalGate, "Confirm network contract package", "Network runtime source is inert, but still requires visible package-manager approval before project inclusion.", true },
        { OperationKind::FetchGitSource, "Fetch package source", "Fetch the C++23 network contract source into cache/packages/engine_network_runtime/source/.", true },
        { OperationKind::StageCachePackage, "Stage inert network contracts", "Stage endpoint, packet frame, and policy-contract code without starting a listener.", true },
        { OperationKind::ConfigureBuild, "Configure network contract self-test", "Configure the source package contract smoke target.", true },
        { OperationKind::CompileAdapter, "Compile network contract self-test", "Build the inert network runtime target and self-test.", true }
    };

    constexpr PackageScriptPlan kPlan{
        "engine_network_runtime",
        "1",
        "cache/packages/engine_network_runtime",
        "https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git",
        "packages/engine_network_runtime",
        true,
        true,
        false,
        false,
        kOperations,
        sizeof(kOperations) / sizeof(kOperations[0])
    };
}

EPOCH_PACKAGE_SCRIPT_EXPORT const epoch::packages::script::PackageScriptPlan* epoch_package_script_plan() noexcept
{
    return &kPlan;
}
