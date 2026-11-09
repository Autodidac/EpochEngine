// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace epoch::extensions::local_ai_llama_cpp_runtime
{
    struct RuntimeLayout
    {
        std::filesystem::path packageRoot{};
        std::filesystem::path sourceRoot{};
        std::filesystem::path buildRoot{};
        std::filesystem::path binaryRoot{};
        std::filesystem::path modelRoot{};
    };

    struct RuntimeBuildPlan
    {
        RuntimeLayout layout{};
        std::string sourceRepository{};
        std::string sourceRevision{};
        std::string sourceLicense{};
        std::vector<std::string> configureArguments{};
        std::vector<std::string> buildArguments{};
        bool humanApproved{};
        bool immutableSourcePinned{};
        bool startsServer{};
        bool valid{};
        std::string diagnostic{};
    };

    struct RuntimeSelection
    {
        std::filesystem::path executable{};
        std::filesystem::path model{};
        bool valid{};
        std::string diagnostic{};
    };

    [[nodiscard]] RuntimeLayout make_runtime_layout(
        const std::filesystem::path& executableLocalCache);
    [[nodiscard]] RuntimeBuildPlan make_build_plan(
        RuntimeLayout layout,
        bool humanApproved,
        std::string immutableRevision = {});
    [[nodiscard]] RuntimeSelection validate_runtime_selection(
        const std::filesystem::path& executable,
        const std::filesystem::path& model);
    [[nodiscard]] std::string render_environment_setup(const RuntimeSelection& selection);
}