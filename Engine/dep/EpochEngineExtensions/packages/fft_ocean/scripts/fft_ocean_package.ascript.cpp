// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "../../_api/epoch_package_script_api.hpp"

namespace
{
    using epoch::packages::script::Operation;
    using epoch::packages::script::OperationKind;
    using epoch::packages::script::PackageScriptPlan;

    constexpr Operation kOperations[] = {
        { OperationKind::HumanApprovalGate, "Confirm FFT ocean package", "FFT ocean is optional and belongs in cache/packages, not the minimal engine clone.", true },
        { OperationKind::FetchGitSource, "Fetch package source", "Fetch the reviewed FFT ocean source into cache/packages/fft_ocean/source/.", true },
        { OperationKind::StageCachePackage, "Stage ocean source", "Stage include/, src/, tests/, and CMakeLists.txt into cache/packages/fft_ocean/source/.", true },
        { OperationKind::ConfigureBuild, "Configure ocean preview", "Configure the package preview without altering core renderer defaults.", true },
        { OperationKind::CompileAdapter, "Compile ocean preview", "Build epoch_extension_fft_ocean_self_test through the Package Manager gate.", true },
        { OperationKind::RegisterLivePreview, "Register ocean preview", "Expose the live ocean preview after visible build evidence exists.", true }
    };

    constexpr PackageScriptPlan kPlan{
        "fft_ocean",
        "1",
        "cache/packages/fft_ocean",
        "https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git",
        "packages/fft_ocean",
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
