// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "../../_api/epoch_package_script_api.hpp"

namespace
{
    using epoch::packages::script::Operation;
    using epoch::packages::script::OperationKind;
    using epoch::packages::script::PackageScriptPlan;

    constexpr Operation kOperations[] = {
        { OperationKind::HumanApprovalGate, "Confirm voxel authoring import", "Fetch only after the operator approves the package import and source license notice.", true },
        { OperationKind::FetchGitSource, "Fetch source", "Fetch reviewed voxel authoring source into cache/packages/voxel_authoring_tools/source/.", true },
        { OperationKind::StageCachePackage, "Stage authoring adapter", "Stage a minimal Epoch voxel-authoring adapter; do not copy the whole tree into core.", true },
        { OperationKind::ConfigureBuild, "Configure authoring preview", "Configure against Epoch voxel.field and voxel.trace boundaries.", true },
        { OperationKind::CompileAdapter, "Compile authoring preview", "Build the package adapter for editor-side voxel authoring proof.", true },
        { OperationKind::RegisterLivePreview, "Register voxel authoring preview", "Expose the authoring preview only after build evidence is captured.", true }
    };

    constexpr PackageScriptPlan kPlan{
        "voxel_authoring_tools",
        "1",
        "cache/packages/voxel_authoring_tools",
        "https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git",
        "packages/voxel_authoring_tools",
        true,
        true,
        true,
        false,
        kOperations,
        sizeof(kOperations) / sizeof(kOperations[0])
    };
}

EPOCH_PACKAGE_SCRIPT_EXPORT const epoch::packages::script::PackageScriptPlan* epoch_package_script_plan() noexcept
{
    return &kPlan;
}
