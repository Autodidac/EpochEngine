// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "../../_api/epoch_package_script_api.hpp"

namespace
{
    using epoch::packages::script::Operation;
    using epoch::packages::script::OperationKind;
    using epoch::packages::script::PackageScriptPlan;

    constexpr Operation kOperations[] = {
        {
            OperationKind::HumanApprovalGate,
            "Confirm capability gallery activation",
            "Stage the inert renderer-neutral gallery only after explicit Package Manager approval.",
            true
        },
        {
            OperationKind::StageCachePackage,
            "Stage capability gallery source",
            "Place the package under cache/packages/capability_technique_gallery without modifying EpochEngine source.",
            true
        },
        {
            OperationKind::ConfigureBuild,
            "Configure C++23 gallery contracts",
            "Configure the package-local catalog, scene planner, and deterministic self-test.",
            true
        },
        {
            OperationKind::CompileAdapter,
            "Compile capability gallery adapter",
            "Build only renderer-neutral data and scene planning; no backend resource code is imported.",
            true
        },
        {
            OperationKind::RegisterReferenceMaterial,
            "Register technique evidence",
            "Expose labels, tier routes, dependencies, fallbacks, and reference-only evidence to the Package Manager.",
            true
        },
        {
            OperationKind::RegisterLivePreview,
            "Register gallery scene preview",
            "Request a human-visible scene preview while leaving provider execution capability-gated.",
            true
        }
    };

    constexpr PackageScriptPlan kPlan{
        "capability_technique_gallery",
        "1",
        "cache/packages/capability_technique_gallery",
        "https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git",
        "packages/capability_technique_gallery",
        false,
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
