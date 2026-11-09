// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "../../_api/epoch_package_script_api.hpp"

namespace
{
    using epoch::packages::script::Operation;
    using epoch::packages::script::OperationKind;
    using epoch::packages::script::PackageScriptPlan;

    constexpr Operation kOperations[] = {
        { OperationKind::HumanApprovalGate, "Confirm dedicated-server package", "Authoritative dedicated server support is optional and can run only after explicit human network approval.", true },
        { OperationKind::FetchGitSource, "Fetch package source", "Fetch dedicated-server planning source into cache/packages/engine_authoritative_dedicated_server/source/.", true },
        { OperationKind::StageCachePackage, "Stage headless authority plan", "Stage tick, snapshot, and approval-gate code without binding a port during install.", true },
        { OperationKind::ConfigureBuild, "Configure dedicated-server self-test", "Configure the headless authority planning smoke target.", true },
        { OperationKind::CompileAdapter, "Compile dedicated-server self-test", "Build the planning target and approval-gate self-test.", true }
    };

    constexpr PackageScriptPlan kPlan{
        "engine_authoritative_dedicated_server",
        "1",
        "cache/packages/engine_authoritative_dedicated_server",
        "https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git",
        "packages/engine_authoritative_dedicated_server",
        true,
        true,
        false,
        true,
        kOperations,
        sizeof(kOperations) / sizeof(kOperations[0])
    };
}

EPOCH_PACKAGE_SCRIPT_EXPORT const epoch::packages::script::PackageScriptPlan* epoch_package_script_plan() noexcept
{
    return &kPlan;
}
