// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "../../_api/epoch_package_script_api.hpp"

namespace
{
    using epoch::packages::script::Operation;
    using epoch::packages::script::OperationKind;
    using epoch::packages::script::PackageScriptPlan;

    constexpr Operation kOperations[] = {
        { OperationKind::HumanApprovalGate, "Confirm planetary terrain package", "Planetary terrain is optional and must not bloat minimal software projects.", true },
        { OperationKind::FetchGitSource, "Fetch package source", "Fetch the reviewed package source into cache/packages/planetary_terrain/source/.", true },
        { OperationKind::StageCachePackage, "Stage terrain adapter", "Stage multi-terrain and planet LOD adapters against Epoch core voxel/tracing contracts.", true },
        { OperationKind::ConfigureBuild, "Configure terrain preview", "Configure a live editor preview target without promoting heavy source into core.", true },
        { OperationKind::CompileAdapter, "Compile terrain preview", "Build the terrain adapter through the Package Manager gate.", true },
        { OperationKind::RegisterLivePreview, "Register terrain preview", "Expose NMS-style planetary terrain proof only after build evidence exists.", true }
    };

    constexpr PackageScriptPlan kPlan{
        "planetary_terrain",
        "1",
        "cache/packages/planetary_terrain",
        "https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git",
        "packages/planetary_terrain",
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
