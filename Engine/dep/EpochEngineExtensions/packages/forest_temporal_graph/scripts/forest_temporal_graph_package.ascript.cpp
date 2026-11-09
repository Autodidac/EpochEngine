// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "../../_api/epoch_package_script_api.hpp"

namespace
{
    using epoch::packages::script::Operation;
    using epoch::packages::script::OperationKind;
    using epoch::packages::script::PackageScriptPlan;

    constexpr Operation kOperations[] = {
        { OperationKind::HumanApprovalGate, "Confirm Forest Factory temporal graph", "Forest Factory is a core Epoch direction; this package stages optional temporal graph source and must stay opt-in.", true },
        { OperationKind::FetchGitSource, "Fetch package source", "Fetch the reviewed extension source into cache/packages/forest_temporal_graph/source/.", true },
        { OperationKind::StageCachePackage, "Stage forest temporal graph adapter", "Stage deterministic grammar and branch segment code for renderer-neutral preview evidence.", true },
        { OperationKind::ConfigureBuild, "Configure forest temporal graph preview", "Configure the C++23 Forest Factory temporal graph self-test target.", true },
        { OperationKind::CompileAdapter, "Compile forest temporal graph preview", "Build the staged Forest Factory temporal graph extension before any editor preview is exposed.", true },
        { OperationKind::RegisterLivePreview, "Register forest temporal graph preview", "Expose the Forest Factory preview only after build evidence exists.", true }
    };

    constexpr PackageScriptPlan kPlan{
        "forest_temporal_graph",
        "1",
        "cache/packages/forest_temporal_graph",
        "https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git",
        "packages/forest_temporal_graph",
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
