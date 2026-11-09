// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
module ai.model_generation;
import ai.model_install;

namespace epochengine::ai::model_generation
{
    namespace
    {
        [[nodiscard]] bool digest_valid(std::string_view value) noexcept
        {
            return value.size() == 64u && std::all_of(value.begin(), value.end(),
                [](char ch) { return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'); })
                && value.find_first_not_of('0') != std::string_view::npos;
        }
        [[nodiscard]] bool exact_path(const std::filesystem::path& path)
        {
            if (path.empty() || !path.is_absolute() || path == path.root_path()
                || path.lexically_normal() != path)
                return false;
            std::error_code error{};
            return std::filesystem::canonical(path, error) == path && !error;
        }
        [[nodiscard]] bool within(const std::filesystem::path& child,
            const std::filesystem::path& parent)
        {
            if (parent.empty()) return false;
            auto at = child.begin();
            for (const auto& component : parent)
                if (at == child.end() || *at++ != component) return false;
            return true;
        }
        [[nodiscard]] bool vacant(const std::filesystem::path& path)
        {
            std::error_code error{};
            const auto status = std::filesystem::symlink_status(path, error);
            return (!error || error == std::errc::no_such_file_or_directory)
                && status.type() == std::filesystem::file_type::not_found;
        }
        [[nodiscard]] std::string utf8_path(const std::filesystem::path& path)
        {
            const auto utf8 = path.generic_u8string();
            return {utf8.begin(), utf8.end()};
        }
    }

    std::string request_diagnostic(const Request& request)
    {
        if (!digest_valid(request.approved_reference_sha256) || request.reference_path.empty())
            return "Choose an approved reference image and its exact SHA-256.";
        if (request.resolution != 512u || request.steps == 0u || request.steps > 24u
            || request.face_budget == 0u || request.face_budget > 1'000'000u
            || request.atlas < 64u || request.atlas > 4096u
            || (request.atlas & (request.atlas - 1u)) != 0u
            || request.seed > static_cast<std::uint32_t>((std::numeric_limits<int>::max)()))
            return "This recipe supports resolution 512, 1–24 steps, 1–1000000 faces, power-of-two atlas 64–4096 and seed 0–2147483647.";
        return {};
    }

    Invocation prepare(const Request& request, const Configuration& configuration,
        const std::filesystem::path& owned_work_directory)
    {
        const auto fail = [](std::string reason) { return Invocation{.diagnostic = std::move(reason)}; };
        try
        {
            if (const auto reason = request_diagnostic(request); !reason.empty()) return fail(reason);
            if (!configuration.dinov3_terms_accepted)
                return fail("DINOv3 conditioning uses separate custom terms. Explicit human license/access acceptance is required before this recipe can run.");
            if (!exact_path(configuration.executable) || !configuration.expected_executable.valid()
                || (configuration.executable.filename() != "trellis-cli.exe"
                    && configuration.executable.filename() != "trellis-cli"))
                return fail("Select the pinned native TRELLIS CLI with its verified executable size and SHA-256.");
            const model_install::ArtifactSpec executable{
                .bytes = configuration.expected_executable.size_bytes,
                .sha256 = configuration.expected_executable.sha256};
            if (!model_install::verify_file(configuration.executable, executable).verified())
                return fail("The native TRELLIS executable does not match its verified identity.");
            std::error_code error{};
            if (!exact_path(owned_work_directory) || !std::filesystem::is_directory(owned_work_directory, error) || error
                || within(owned_work_directory, configuration.models_cache_root)
                || within(owned_work_directory, configuration.executable.parent_path()))
                return fail("Use an existing normalized, nonredirected owned job directory outside model and executable inputs.");
            if (!exact_path(request.reference_path) || !within(request.reference_path, owned_work_directory)
                || !std::filesystem::is_regular_file(std::filesystem::symlink_status(request.reference_path)))
                return fail("The approved reference must be a frozen regular file inside this exact job directory.");
            const auto bytes = std::filesystem::file_size(request.reference_path, error);
            if (error || bytes == 0u || bytes > 64ull * 1024ull * 1024ull)
                return fail("The approved reference is missing or exceeds 64 MiB.");
            const model_install::ArtifactSpec reference{
                .bytes = bytes, .sha256 = request.approved_reference_sha256};
            if (!model_install::verify_file(request.reference_path, reference).verified())
                return fail("The reference image no longer matches the approved SHA-256. No model request was prepared.");
            const auto output = owned_work_directory / "generated.glb";
            const auto log = owned_work_directory / "generation.log";
            const auto sidecar = owned_work_directory / "generated_base.png";
            if (!vacant(output) || !vacant(log) || !vacant(sidecar))
                return fail("The job output, sidecar or transcript already exists; use a fresh owned job directory.");
            const auto plan = model_install::plan_for(model_package);
            if (!plan || plan->artifacts.size() != 8u)
                return fail("The complete pinned eight-component TRELLIS recipe is not registered.");
            const auto installed = model_install::verify_snapshot(configuration.models_cache_root, *plan);
            if (!installed.accepted()) return fail("TRELLIS model snapshot: " + installed.diagnostic);

            Invocation result{};
            result.output = output;
            result.required_face_budget = request.face_budget;
            result.reference_sha256 = request.approved_reference_sha256;
            result.launch.executable = configuration.executable;
            result.launch.expected_executable = configuration.expected_executable;
            result.launch.working_directory = owned_work_directory;
            result.launch.merged_output_path = log;
            result.launch.arguments = {
                "--image", utf8_path(request.reference_path), "--output", utf8_path(output),
                "--models", utf8_path(installed.installed_root), "--model", "trellis",
                "--res", "512", "--tex-res", "512", "--seed", std::to_string(request.seed),
                "--steps", std::to_string(request.steps), "--atlas", std::to_string(request.atlas),
                "--bg-removal", "birefnet", "--webp", "off", "--backend", "CUDA", "--gpu", "0",
                "--require-gpu", "--verbose"};
            // Upstream also reads debug dump/output paths from TRELLIS_* and
            // geometry settings from the environment. This exact recipe must
            // not inherit those ambient controls. Adjacent runtime DLLs remain
            // owned inputs; this does not change the host environment or GPU.
            result.launch.environment = std::vector<platform::child_process::EnvironmentVariable>{};
            result.launch.correlation_key = "model-generation:" + utf8_path(owned_work_directory);
            result.launch.exclusive_group = "native-model-generation";
            result.launch.display_name = "Local TRELLIS.2 512 image-to-3D";
            result.launch.window_mode = platform::child_process::WindowMode::hidden;
            result.launch.disconnect_standard_input = true;
            result.ready = true;
            result.diagnostic = "Pinned eight-component model and approved reference verified. Native v0.8.1 targets 150000 faces; the host must independently enforce the requested face budget and validate PNG-bearing GLB before approval.";
            return result;
        }
        catch (...) { return fail("Native 3D request preparation failed; no process or file was created."); }
    }
}
