// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "../../_api/epoch_package_script_api.hpp"

namespace
{
    using epoch::packages::script::Operation;
    using epoch::packages::script::OperationKind;
    using epoch::packages::script::PackageScriptPlan;

    constexpr Operation kOperations[] = {
        { OperationKind::HumanApprovalGate, "Review local AI runtime plan", "Any future llama.cpp fetch or compilation requires visible Package Manager approval.", true },
        { OperationKind::RegisterReferenceMaterial, "Require immutable llama.cpp revision", "Record a reviewed 40-character upstream git revision before a future executor may fetch source.", true },
        { OperationKind::ConfigureBuild, "Plan offline llama-cli configuration", "Plan C++23 Release with server and curl targets disabled; this script does not execute CMake.", true },
        { OperationKind::RegisterLivePreview, "Register runtime status contract", "Expose existing executable and GGUF readiness without starting a listener, fetch, build, or model process.", true }
    };

    constexpr PackageScriptPlan kPlan{
        "local_ai_llama_cpp_runtime",
        "2",
        "cache/packages/local_ai_llama_cpp_runtime",
        "https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git",
        "packages/local_ai_llama_cpp_runtime",
        false,
        false,
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