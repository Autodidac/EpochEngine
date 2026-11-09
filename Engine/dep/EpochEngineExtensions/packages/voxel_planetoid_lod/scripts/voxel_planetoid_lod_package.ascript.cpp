// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "../../_api/epoch_package_script_api.hpp"

namespace
{
    using epoch::packages::script::Operation;
    using epoch::packages::script::OperationKind;
    using epoch::packages::script::PackageScriptPlan;

    constexpr Operation kOperations[] = {
        { OperationKind::HumanApprovalGate, "Confirm voxel planetoid package", "Use only reviewed renderer-neutral source and keep backend uploads outside this package.", true },
        { OperationKind::FetchGitSource, "Fetch package source", "Fetch the staged package source into cache/packages/voxel_planetoid_lod/source/.", true },
        { OperationKind::StageCachePackage, "Stage planetoid adapter", "Stage field sampling and chunk LOD policy without copying it into EpochEngine mainline.", true },
        { OperationKind::ConfigureBuild, "Configure voxel preview adapter", "Configure only the smallest renderer-neutral voxel preview adapter needed for editor proof.", true },
        { OperationKind::CompileAdapter, "Compile voxel preview", "Build the adapter as a package preview target through the engine Package Manager gate.", true },
        { OperationKind::RegisterLivePreview, "Register live voxel preview", "Expose the preview through the editor package surface after build evidence exists.", true }
    };

    constexpr PackageScriptPlan kPlan{
        "voxel_planetoid_lod",
        "1",
        "cache/packages/voxel_planetoid_lod",
        "https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git",
        "packages/voxel_planetoid_lod",
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
