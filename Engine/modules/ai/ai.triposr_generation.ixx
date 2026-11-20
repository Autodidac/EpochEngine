// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <filesystem>
#include <string>
#include <string_view>
export module ai.triposr_generation;
export import ai.model_generation;

export namespace epochengine::ai::triposr_generation
{
    inline constexpr std::string_view model_package = "os_model_triposr";
    inline constexpr std::string_view dino_config_package = "os_model_triposr_dino_v1_config";
    inline constexpr std::string_view source_revision = "107cefdc244c39106fa830359024f6a2f1c78871";
    inline constexpr std::string_view worker_sha256 = "5c953e11be385a45594d802b826078f3f8d7c2673eaa14dd914410b8a25b75a6";
    inline constexpr std::string_view source_manifest_sha256 = "63f535912312705b88b60bf4a266b878a74dbe97d6feba4e34530827f1b646bb";
    struct Configuration final
    {
        std::filesystem::path executable{};
        platform::child_process::ExecutableIdentity expected_executable{};
        std::filesystem::path worker_path{};
        std::string expected_worker_sha256{};
        std::filesystem::path source_root{};
        std::string expected_source_manifest_sha256{};
        std::filesystem::path models_cache_root{};
    };
    // Read-only checks; never loads Python, a model, or a GPU. The optional
    // runtime and its third-party distribution notices remain separate inputs.
    [[nodiscard]] std::string preflight(const model_generation::Request&, const Configuration&);
    // Stages only one exclusive request file below the caller-owned job root.
    // The existing asset-job supervisor owns launch, cancellation and validation.
    [[nodiscard]] model_generation::Invocation prepare(const model_generation::Request&,
        const Configuration&, const std::filesystem::path& owned_work_directory);
}
