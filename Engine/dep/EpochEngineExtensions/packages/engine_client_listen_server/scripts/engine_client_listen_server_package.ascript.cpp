// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "../../_api/epoch_package_script_api.hpp"

namespace
{
    using epoch::packages::script::Operation;
    using epoch::packages::script::OperationKind;
    using epoch::packages::script::PackageScriptPlan;

    constexpr Operation kOperations[] = {
        { OperationKind::HumanApprovalGate, "Confirm listen-server package", "A client-hosted listener can exist only after explicit human network approval.", true },
        { OperationKind::FetchGitSource, "Fetch package source", "Fetch listen-server planning source into cache/packages/engine_client_listen_server/source/.", true },
        { OperationKind::StageCachePackage, "Stage manual-run listen plan", "Stage configuration and verifier code; do not bind a port during install.", true },
        { OperationKind::ConfigureBuild, "Configure listen-server self-test", "Configure the listen-server planning smoke target.", true },
        { OperationKind::CompileAdapter, "Compile listen-server self-test", "Build the planning target and approval-gate self-test.", true }
    };

    constexpr PackageScriptPlan kPlan{
        "engine_client_listen_server",
        "1",
        "cache/packages/engine_client_listen_server",
        "https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git",
        "packages/engine_client_listen_server",
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
