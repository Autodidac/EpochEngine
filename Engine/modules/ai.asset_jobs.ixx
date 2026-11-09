// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
#include "../src/ai/ai.asset_stage_fwd.hpp"
export module ai.asset_jobs;
export import ai.image_generation;
export import ai.model_generation;

export namespace epochengine::ai::asset_jobs
{
    enum class Code : std::uint8_t
    {
        awaiting_reference_approval, invalid_request, occupied_workspace,
        model_work_active, resources_unavailable, insufficient_memory,
        backend_unavailable, cancelled, timed_out, backend_failed,
        invalid_output, evidence_failed, awaiting_model_approval
    };
    struct Result final
    {
        Code code{Code::invalid_request};
        std::filesystem::path work_directory{}, output{}, receipt{};
        std::string diagnostic{}, output_sha256{};
        std::uint32_t width{}, height{};
        std::uint64_t elapsed_ms{};
        bool worker_retired{};
        platform::child_process::ProcessHandle worker{};
        std::uint64_t verification_ms{}, worker_runtime_ns{}, validation_ms{};
        std::uint64_t vertices{}, faces{}, decoded_texture_bytes{};
        std::uint32_t materials{}, textures{};
        // Optional immutable artifact bundle, still awaiting model/project
        // approval. Empty when the material needs an unsupported import path.
        std::filesystem::path compiled_assets{};
        [[nodiscard]] explicit operator bool() const noexcept
        { return code == Code::awaiting_reference_approval || code == Code::awaiting_model_approval; }
    };
    // Blocking stage operation for an existing cancellable task or offscreen
    // command. Creates no scheduler, model manager, renderer, server or import.
    [[nodiscard]] Result generate_image(const image_generation::Request&,
        const image_generation::Configuration&,
        const std::filesystem::path& new_work_directory,
        std::stop_token cancellation = {},
        const native_runtime::AssetStagePermit* permit = nullptr);
    struct ModelBackend final
    {
        std::string id{}, revision{};
        std::vector<std::string> model_package_ids{};
        bool terms_accepted{};
        // No process, model loading or scheduler. Preflight runs before host/GPU
        // queries; prepare may exclusively stage backend inputs in this job.
        std::function<std::string(const model_generation::Request&)> preflight{};
        std::function<model_generation::Invocation(const model_generation::Request&,
            const std::filesystem::path&)> prepare{};
    };
    [[nodiscard]] Result generate_model(const model_generation::Request&,
        const model_generation::Configuration&,
        const std::filesystem::path& new_work_directory,
        std::stop_token cancellation = {},
        const native_runtime::AssetStagePermit* permit = nullptr);
    [[nodiscard]] Result generate_model(const model_generation::Request&,
        const ModelBackend&, const std::filesystem::path& new_work_directory,
        std::stop_token cancellation = {},
        const native_runtime::AssetStagePermit* permit = nullptr);
    enum class Action : std::uint8_t { image, model };
    // One typed admission path for CLI and Editor. Reading never starts a worker,
    // changes model residency, accepts terms, or publishes an asset.
    struct PreparedJob final
    {
        Action action{Action::image};
        image_generation::Request image{};
        image_generation::Configuration image_backend{};
        model_generation::Request model{};
        model_generation::Configuration model_backend{};
        std::optional<ModelBackend> alternative_model_backend{};
        std::filesystem::path work_directory{};
        std::string backend_name{}, diagnostic{};
        bool ready{};
        [[nodiscard]] explicit operator bool() const noexcept { return ready; }
    };
    [[nodiscard]] PreparedJob read_request_file(const std::filesystem::path&) noexcept;
    [[nodiscard]] Result execute(const PreparedJob&, std::stop_token = {},
        const native_runtime::AssetStagePermit* = nullptr);
    // An explicit local request file is the command boundary. No downloads.
    // The command returns before renderer/updater/application initialization.
    [[nodiscard]] int run_file(const std::filesystem::path& request_file) noexcept;
}
