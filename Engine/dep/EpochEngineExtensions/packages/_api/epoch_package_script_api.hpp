// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#pragma once

#include <cstddef>

#if defined(_WIN32)
#define EPOCH_PACKAGE_SCRIPT_EXPORT extern "C" __declspec(dllexport)
#else
#define EPOCH_PACKAGE_SCRIPT_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace epoch::packages::script
{
    enum class OperationKind
    {
        HumanApprovalGate,
        FetchGitSource,
        VerifyLocalArchive,
        ExtractLocalArchive,
        StageCachePackage,
        ConfigureBuild,
        CompileAdapter,
        RegisterLivePreview,
        RegisterReferenceMaterial
    };

    struct Operation
    {
        OperationKind kind;
        const char* label;
        const char* detail;
        bool requiresHumanApproval;
    };

    struct PackageScriptPlan
    {
        const char* packageId;
        const char* scriptVersion;
        const char* cacheRoot;
        const char* sourceRepo;
        const char* sourceRef;
        bool downloadsSource;
        bool compilesSource;
        bool livePreview;
        bool networkOrServer;
        const Operation* operations;
        std::size_t operationCount;
    };
}
