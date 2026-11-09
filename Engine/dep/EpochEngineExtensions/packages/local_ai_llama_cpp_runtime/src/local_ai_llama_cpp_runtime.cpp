// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/local_ai_llama_cpp_runtime/local_ai_llama_cpp_runtime.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <utility>

namespace epoch::extensions::local_ai_llama_cpp_runtime
{
    namespace
    {
        [[nodiscard]] bool regular_file(const std::filesystem::path& path)
        {
            std::error_code error;
            return !path.empty() && std::filesystem::is_regular_file(path, error);
        }

        [[nodiscard]] bool immutable_git_revision(std::string_view revision)
        {
            return revision.size() == 40U &&
                std::ranges::all_of(revision, [](unsigned char value) {
                    return std::isxdigit(value) != 0;
                });
        }

        [[nodiscard]] std::string quoted(const std::filesystem::path& path)
        {
            return "\"" + path.generic_string() + "\"";
        }
    }

    RuntimeLayout make_runtime_layout(const std::filesystem::path& executableLocalCache)
    {
        const auto packageRoot = executableLocalCache / "packages" / "local_ai_llama_cpp_runtime";
        return RuntimeLayout{
            .packageRoot = packageRoot,
            .sourceRoot = packageRoot / "source",
            .buildRoot = packageRoot / "build",
            .binaryRoot = packageRoot / "bin",
            .modelRoot = executableLocalCache / "models"
        };
    }

    RuntimeBuildPlan make_build_plan(
        RuntimeLayout layout,
        bool humanApproved,
        std::string immutableRevision)
    {
        RuntimeBuildPlan plan{};
        plan.layout = std::move(layout);
        plan.sourceRepository = "https://github.com/ggml-org/llama.cpp";
        plan.sourceRevision = std::move(immutableRevision);
        plan.sourceLicense = "MIT";
        plan.humanApproved = humanApproved;
        plan.immutableSourcePinned = immutable_git_revision(plan.sourceRevision);
        plan.startsServer = false;
        plan.configureArguments = {
            "cmake", "-S", plan.layout.sourceRoot.generic_string(),
            "-B", plan.layout.buildRoot.generic_string(),
            "-DCMAKE_BUILD_TYPE=Release",
            "-DLLAMA_BUILD_SERVER=OFF",
            "-DLLAMA_CURL=OFF",
            "-DLLAMA_BUILD_TESTS=OFF",
            "-DLLAMA_BUILD_EXAMPLES=ON"
        };
        plan.buildArguments = {
            "cmake", "--build", plan.layout.buildRoot.generic_string(),
            "--config", "Release", "--target", "llama-cli"
        };
        if (!humanApproved)
        {
            plan.diagnostic = "Source fetch and compilation require explicit Package Manager approval.";
            return plan;
        }
        if (!plan.immutableSourcePinned)
        {
            plan.diagnostic = "An immutable 40-character llama.cpp git revision is required before fetch.";
            return plan;
        }
        if (plan.layout.packageRoot.empty() || plan.layout.modelRoot.empty())
        {
            plan.diagnostic = "Executable-local package and model roots are required.";
            return plan;
        }
        plan.valid = true;
        plan.diagnostic =
            "Pinned offline llama-cli build plan is ready; a separate approved executor must fetch and verify the revision.";
        return plan;
    }

    RuntimeSelection validate_runtime_selection(
        const std::filesystem::path& executable,
        const std::filesystem::path& model)
    {
        RuntimeSelection selection{executable, model};
        if (!regular_file(executable))
        {
            selection.diagnostic = "llama-cli executable does not exist.";
            return selection;
        }
        if (!regular_file(model) || model.extension() != ".gguf")
        {
            selection.diagnostic = "An existing GGUF model is required.";
            return selection;
        }
        selection.valid = true;
        selection.diagnostic = "Direct llama.cpp runtime selection is valid.";
        return selection;
    }

    std::string render_environment_setup(const RuntimeSelection& selection)
    {
        if (!selection.valid)
            return {};
#if defined(_WIN32)
        return "$env:EPOCH_LLAMA_CPP_EXECUTABLE=" + quoted(selection.executable)
            + "\n$env:EPOCH_AI_MODEL_PATH=" + quoted(selection.model);
#else
        return "export EPOCH_LLAMA_CPP_EXECUTABLE=" + quoted(selection.executable)
            + "\nexport EPOCH_AI_MODEL_PATH=" + quoted(selection.model);
#endif
    }
}