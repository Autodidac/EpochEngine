// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>
module ai.triposr_generation;
import ai.model_install;
import platform.filesystem;

namespace epochengine::ai::triposr_generation
{
    namespace
    {
        namespace fs = std::filesystem;
        using Json = nlohmann::json;
        bool exact(const fs::path& path)
        {
            std::error_code error{};
            return path.is_absolute() && path != path.root_path()
                && path.lexically_normal() == path && fs::canonical(path, error) == path && !error;
        }
        bool within(const fs::path& child, const fs::path& parent)
        {
            auto at = child.begin();
            for (const auto& part : parent) if (at == child.end() || *at++ != part) return false;
            return !parent.empty();
        }
        std::string text(const fs::path& path)
        {
            const auto value = path.generic_u8string();
            return {value.begin(), value.end()};
        }
        bool vacant(const fs::path& path)
        {
            std::error_code error{};
            const auto status = fs::symlink_status(path, error);
            return (!error || error == std::errc::no_such_file_or_directory)
                && status.type() == fs::file_type::not_found;
        }
        bool pinned(const fs::path& path, std::string_view sha, std::uint64_t maximum)
        {
            std::error_code error{};
            if (!exact(path) || !fs::is_regular_file(fs::symlink_status(path, error)) || error) return false;
            const auto size = fs::file_size(path, error);
            return !error && size != 0u && size <= maximum && model_install::verify_file(path,
                {.bytes = size, .sha256 = std::string{sha}}).verified();
        }
        bool source_matches(const Configuration& config)
        {
            const auto manifest = config.source_root / "epoch-source.json";
            if (!pinned(manifest, source_manifest_sha256, 64u * 1024u)) return false;
            std::ifstream input{manifest, std::ios::binary};
            const auto json = Json::parse(input);
            if (json.value("upstream_commit", "") != source_revision || json.value("offline_patch", 0) != 1
                || !json.contains("files") || !json["files"].is_object()
                || json["files"].empty() || json["files"].size() > 64u) return false;
            for (const auto& member : json["files"].items())
            {
                const auto relative = fs::path{std::u8string{member.key().begin(), member.key().end()}};
                if (relative.empty() || relative.is_absolute() || relative.lexically_normal() != relative) return false;
                for (const auto& part : relative) if (part == "." || part == "..") return false;
                const auto file = config.source_root / relative;
                if (!within(file, config.source_root) || !pinned(file, member.value().get<std::string>(), 2u * 1024u * 1024u)) return false;
            }
            // The worker repeats this check immediately before importing source.
            for (const auto& entry : fs::recursive_directory_iterator(config.source_root))
            {
                if (entry.is_symlink() || !exact(entry.path())) return false;
                if (entry.is_regular_file() && entry.path() != manifest
                    && !json["files"].contains(text(entry.path().lexically_relative(config.source_root)))) return false;
            }
            return true;
        }
    }

    std::string preflight(const model_generation::Request& request, const Configuration& config)
    {
        try
        {
            if (const auto reason = model_generation::request_diagnostic(request); !reason.empty()) return reason;
            if (request.face_budget > 500'000u || request.atlas < 128u || request.atlas > 2048u)
                return "The optional TripoSR worker supports at most 500000 faces and atlas 128–2048.";
            if (!config.expected_executable.valid() || !exact(config.executable)
                || (config.executable.filename() != "python.exe" && config.executable.filename() != "python3")
                || !model_install::verify_file(config.executable, {.bytes = config.expected_executable.size_bytes,
                    .sha256 = config.expected_executable.sha256}).verified())
                return "Select the exact admitted isolated Python executable; no worker or hardware query started.";
            if (config.expected_worker_sha256 != worker_sha256 || config.expected_source_manifest_sha256 != source_manifest_sha256
                || !pinned(config.worker_path, worker_sha256, 256u * 1024u)
                || !exact(config.source_root) || !source_matches(config))
                return "The offline TripoSR worker or source inventory differs from its admitted revision; repair the optional runtime first.";
            if (!exact(config.models_cache_root)) return "Choose the existing verified TripoSR model cache.";
            return {};
        }
        catch (...) { return "TripoSR input verification failed; no worker or hardware query started."; }
    }

    model_generation::Invocation prepare(const model_generation::Request& request,
        const Configuration& config, const fs::path& work)
    {
        const auto fail = [](std::string diagnostic) { return model_generation::Invocation{.diagnostic = std::move(diagnostic)}; };
        try
        {
            if (auto reason = preflight(request, config); !reason.empty()) return fail(std::move(reason));
            if (!exact(work) || !fs::is_directory(work) || within(work, config.models_cache_root)
                || within(work, config.source_root) || within(work, config.executable.parent_path()))
                return fail("Use a fresh owned job directory outside runtime, source and model inputs.");
            if (!exact(request.reference_path) || request.reference_path != work / "approved-reference.png"
                || !pinned(request.reference_path, request.approved_reference_sha256, 32u * 1024u * 1024u))
                return fail("The frozen approved reference must be this job's exact PNG and SHA-256.");
            for (const auto name : {"triposr-request.json", "model.glb", "generation.log", "worker-result.json", "progress.json", "cancel.flag"})
                if (!vacant(work / name)) return fail("A TripoSR output or request already exists; prior work was not replaced.");
            const auto model_plan = model_install::plan_for(model_package);
            const auto dino_plan = model_install::plan_for(dino_config_package);
            if (!model_plan || !dino_plan) return fail("The exact TripoSR weights/config and DINOv1 config recipes are unavailable.");
            const auto model = model_install::verify_snapshot(config.models_cache_root, *model_plan);
            if (!model.accepted()) return fail("TripoSR snapshot: " + model.diagnostic);
            const auto dino = model_install::verify_snapshot(config.models_cache_root, *dino_plan);
            if (!dino.accepted()) return fail("DINOv1 configuration snapshot: " + dino.diagnostic);
            const Json value{{"schema", "epoch.triposr.worker.v1"}, {"source_root", text(config.source_root)},
                {"source_manifest_sha256", source_manifest_sha256}, {"weights", text(model.installed_root / "model.ckpt")},
                {"config", text(model.installed_root / "config.yaml")}, {"dino_config", text(dino.installed_root / "config.json")},
                {"reference_path", text(request.reference_path)}, {"approved_reference_sha256", request.approved_reference_sha256},
                {"seed", request.seed}, {"resolution", request.resolution}, {"steps", request.steps},
                {"face_budget", request.face_budget}, {"atlas", request.atlas}};
            const auto bytes = value.dump() + "\n";
            const auto request_file = work / "triposr-request.json";
            std::error_code error{};
            if (!platform::filesystem::exclusive_create_and_write(request_file, std::as_bytes(std::span{bytes.data(), bytes.size()}), error))
                return fail("The exact worker request could not be retained; no process was launched.");
            model_generation::Invocation result{.output = work / "model.glb", .ready = true,
                .required_face_budget = request.face_budget, .reference_sha256 = request.approved_reference_sha256};
            result.launch.executable = config.executable;
            result.launch.expected_executable = config.expected_executable;
            result.launch.arguments = {"-I", text(config.worker_path), "--request", text(request_file)};
            result.launch.working_directory = work;
            result.launch.merged_output_path = work / "generation.log";
            result.launch.environment = std::vector<platform::child_process::EnvironmentVariable>{
                {"HF_HUB_OFFLINE", "1"}, {"TRANSFORMERS_OFFLINE", "1"}, {"HF_HUB_DISABLE_TELEMETRY", "1"},
                {"TORCH_FORCE_WEIGHTS_ONLY_LOAD", "1"}};
            result.launch.correlation_key = "triposr-generation:" + text(work);
            result.launch.exclusive_group = "native-model-generation";
            result.launch.display_name = "Local TripoSR image-to-3D (optional Python worker)";
            result.launch.window_mode = platform::child_process::WindowMode::hidden;
            result.launch.disconnect_standard_input = true;
            result.diagnostic = "Pinned offline worker/model inputs verified; shared job admission, real GLB/PNG validation and human model approval remain required. Runtime shipping clearance is separate.";
            return result;
        }
        catch (...) { return fail("TripoSR preparation failed; no process was launched."); }
    }
}
