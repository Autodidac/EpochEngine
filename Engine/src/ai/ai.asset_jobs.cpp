// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <fstream>
#include <memory>
#include <limits>
#include <optional>
#include <span>
#include <set>
#include <sstream>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>
#include <system_error>
#include <nlohmann/json.hpp>
#include "ai.native_runtime.hpp"
#include "ai.epoch_native_storage.hpp"
#include "native/ai.backend_process_process.hpp"
module ai.asset_jobs;
import ai.model_install;
import ai.triposr_generation;
import asset.texture_import;
import asset.texture_artifact;
import asset.model_import;
import asset.mesh_artifact;
import core.logger;
import core.env;
import core.sha256;
import platform.child_process;
import platform.filesystem;
import platform.work_admission;

namespace epochengine::ai::asset_jobs
{
    namespace
    {
        namespace fs = std::filesystem;
        namespace child = platform::child_process;
        namespace admission = platform::work_admission;
        using Json = nlohmann::json;
        constexpr std::uint64_t gib = 1024ull * 1024ull * 1024ull;
        using Clock = std::chrono::steady_clock;
        void log(const std::string& message);
        std::uint64_t elapsed_ms(Clock::time_point start)
        {
            return static_cast<std::uint64_t>(std::chrono::duration_cast<
                std::chrono::milliseconds>(Clock::now() - start).count());
        }
        void phase(Json& evidence, std::string_view name, Clock::time_point start)
        {
            if (!evidence.contains("phases")) evidence["phases"] = Json::array();
            evidence["phases"].push_back({{"name", name}, {"elapsed_ms", elapsed_ms(start)}});
            log("Asset stage: " + std::string{name});
        }

        Json parse_request(const std::string& bytes)
        {
            std::vector<std::unordered_set<std::string>> objects;
            return Json::parse(bytes, [&](int depth, Json::parse_event_t event, Json& value)
            {
                if (depth > 16) throw std::invalid_argument{"Asset request nesting exceeds 16 levels."};
                if (event == Json::parse_event_t::object_start) objects.emplace_back();
                else if (event == Json::parse_event_t::object_end) objects.pop_back();
                else if (event == Json::parse_event_t::key
                    && !objects.back().insert(value.get<std::string>()).second)
                    throw std::invalid_argument{"Duplicate asset request key."};
                return true;
            });
        }
        void fields(const Json& value, std::initializer_list<std::string_view> allowed)
        {
            if (!value.is_object()) throw std::invalid_argument{"Asset request sections must be objects."};
            for (const auto& item : value.items())
                if (std::find(allowed.begin(), allowed.end(), item.key()) == allowed.end())
                    throw std::invalid_argument{"Unknown asset request field: " + item.key()};
        }

        std::uint64_t integer(const Json& object, const char* key,
            std::uint64_t fallback, std::uint64_t maximum)
        {
            const auto it = object.find(key);
            if (it == object.end()) return fallback;
            if (!it->is_number_integer() || (it->is_number_integer()
                && !it->is_number_unsigned() && it->get<std::int64_t>() < 0))
                throw std::invalid_argument{std::string{key} + " must be a nonnegative integer."};
            const auto number = it->get<std::uint64_t>();
            if (number > maximum)
                throw std::invalid_argument{std::string{key} + " is outside the supported range."};
            return number;
        }

        std::string path_text(const fs::path& path)
        {
            const auto bytes = path.generic_u8string();
            return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
        }
        fs::path path_value(const Json& value)
        {
            const auto text = value.get<std::string>();
            if (text.find('\0') != std::string::npos) return {};
            return fs::path{std::u8string{text.begin(), text.end()}};
        }
        bool real_path(const fs::path& path)
        {
            std::error_code error{};
            return path.is_absolute() && path != path.root_path()
                && path.lexically_normal() == path
                && fs::canonical(path, error) == path && !error;
        }
        std::optional<std::string> bounded_file(const fs::path& path,
            std::uint64_t maximum)
        {
            std::error_code error{};
            if (!real_path(path) || !fs::is_regular_file(path, error) || error)
                return {};
            const auto length = fs::file_size(path, error);
            if (error || length == 0u || length > maximum) return {};
            std::ifstream input{path, std::ios::binary};
            std::string bytes(static_cast<std::size_t>(length), '\0');
            input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            if (!input || input.peek() != std::char_traits<char>::eof()) return {};
            return bytes;
        }
        bool write_new(const fs::path& path, const Json& value)
        {
            const auto text = value.dump(2) + "\n";
            std::error_code error{};
            return platform::filesystem::exclusive_create_and_write(path,
                std::as_bytes(std::span{text.data(), text.size()}), error);
        }
        void log(const std::string& message)
        {
            logger::get("AssetGeneration").log(logger::LogLevel::INFO, message);
        }
        std::optional<std::uint64_t> free_nvidia_memory(std::stop_token stop)
        {
            epoch_ai::process::Request query{};
#if defined(_WIN32)
            const auto system_root = core::env::get("SystemRoot");
            if (!system_root) return {};
            query.executable = path_text(fs::path{system_root->c_str()} / "System32/nvidia-smi.exe");
#else
            query.executable = "nvidia-smi";
#endif
            query.arguments = {"--id=0", "--query-gpu=memory.free",
                "--format=csv,noheader,nounits"};
            query.timeout = std::chrono::seconds{2};
            query.max_output_bytes = 1024u;
            const auto result = epoch_ai::process::run(query, stop);
            if (!result || result->exit_code != 0 || result->cancelled || result->timed_out)
                return {};
            std::istringstream input{result->stdout_text};
            std::uint64_t mib{};
            if (!(input >> mib) || mib == 0u || mib > 1024u * 1024u) return {};
            input >> std::ws;
            if (!input.eof()) return {};
            return mib * 1024u * 1024u;
        }
        Json image_request(const image_generation::Request& request)
        {
            return {{"prompt", request.prompt}, {"negative_prompt", request.negative_prompt},
                {"seed", request.seed}, {"width", request.width}, {"height", request.height},
                {"steps", request.steps}, {"cfg_scale", 1.0}, {"sampler", "euler"}};
        }

        bool model_work_guard(const native_runtime::AssetStagePermit* permit,
            std::shared_ptr<native_runtime::WorkLease>& lease)
        {
            if (permit) return permit->valid();
            const auto native = native_runtime::snapshot(false);
            if (native.busy || native.loaded) return false;
            lease = std::make_shared<native_runtime::WorkLease>();
            return lease->admitted();
        }
        bool merge_environment(child::LaunchRequest& launch, const fs::path& work)
        {
            auto environment = child::prepare_workspace_environment(work);
            if (!environment) return false;
            const auto same_name = [](std::string_view a, std::string_view b) {
                return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
                    [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); }); };
            if (launch.environment)
                for (const auto& variable : *launch.environment)
                {
                    const auto found = std::find_if(environment->begin(), environment->end(),
                        [&](const auto& existing) { return same_name(existing.name, variable.name); });
                    if (found != environment->end()) { if (found->value != variable.value) return false; }
                    else environment->push_back(variable);
                }
            launch.environment = std::move(environment);
            return true;
        }
        std::optional<fs::path> prepare_gpu_coordination_cache(const native_storage::Paths& paths)
        {
            try
            {
                // A borrowed child never creates its parent's inputs. Missing
                // Qwen SDKs do not disable independently admitted generators.
                if (paths.shared_model_inputs_bound)
                    return native_shared_inputs::validate_path(paths.model_cache, true);
                const auto expected = native_shared_inputs::validate_path(paths.model_cache, true, true);
                auto current = expected.root_path();
                for (const auto& component : expected.relative_path())
                {
                    current /= component;
                    std::error_code error;
                    if (!fs::exists(current, error))
                    {
                        if (error) return {};
                        (void)native_shared_inputs::validate_path(current, true, true);
                        if (!fs::create_directory(current, error) || error) return {};
                    }
                    (void)native_shared_inputs::validate_path(current, true);
                }
                return native_shared_inputs::validate_path(expected, true);
            }
            catch (...) { return {}; }
        }
        struct WorkerRun final { Code code{Code::backend_failed}; bool succeeded{}; std::string diagnostic{}; };
        // One process owner for every provider. Its guard also settles exceptional
        // exits; it never stops a focused/busy handle owned by a different job.
        WorkerRun run_worker(child::LaunchRequest launch, Result& result, Json& evidence,
            std::shared_ptr<native_runtime::WorkLease> lease, std::stop_token cancellation,
            const native_runtime::AssetStagePermit* permit)
        {
            if (cancellation.stop_requested() || (permit && !permit->valid()))
                return {Code::cancelled, false, "Asset generation cancelled before worker launch."};
            const auto paths = native_storage::defaults();
            const auto shared_cache = prepare_gpu_coordination_cache(paths);
            if (!shared_cache)
                return {Code::resources_unavailable, false, "The shared GPU coordination directory is unavailable or redirected; no worker started."};
            auto gpu = platform::filesystem::ExclusiveFileLease::acquire(shared_cache->parent_path() / "native-gpu.lock");
            if (!gpu)
                return gpu.code() == platform::filesystem::FileLeaseCode::busy
                    ? WorkerRun{Code::model_work_active, false, "Another Epoch application owns local models or generation. Wait for its owned work to retire; no worker started."}
                    : WorkerRun{Code::resources_unavailable, false, "The shared GPU ownership file is invalid or unavailable; no worker started."};
            if (!merge_environment(launch, result.work_directory))
                return {Code::evidence_failed, false, "The owned worker environment could not be prepared."};
            struct WorkerOwnership final
            {
                std::shared_ptr<native_runtime::WorkLease> work;
                platform::filesystem::ExclusiveFileLease gpu;
            };
            // The supervisor retains both leases when child retirement is
            // unknown, including exceptional/cancelled exits. No other app is
            // admitted merely because this blocking function returns.
            launch.retirement_guard = std::make_shared<WorkerOwnership>(
                WorkerOwnership{std::move(lease), std::move(gpu)});
            evidence["gpu_owner_path"] = path_text(shared_cache->parent_path() / "native-gpu.lock");
            const auto launched = child::launch_or_focus(launch);
            if (!launched.owns_new_process()) return {Code::backend_failed, false, launched.message};
            result.worker = launched.handle;
            struct OwnedWorker final
            {
                Result& result;
                bool isolation{};
                void settle(const std::optional<child::ProcessSnapshot>& snapshot)
                {
                    if (snapshot) result.worker_runtime_ns = snapshot->runtime_ns;
                    result.worker_retired = snapshot && !snapshot->active()
                        && (!isolation || snapshot->isolation_retired);
                }
                ~OwnedWorker()
                {
                    if (!result.worker_retired)
                    {
                        (void)child::stop(result.worker, child::StopMode::force);
                        settle(child::wait(result.worker, {}, 5'000'000'000ull).process);
                    }
                    if (result.worker_retired && !child::release(result.worker)) result.worker_retired = false;
                }
            } owner{result, launch.isolation.has_value()};
            evidence["worker_handle"] = {{"slot", launched.handle.slot}, {"generation", launched.handle.generation}};
            const auto waited = child::wait(launched.handle, cancellation, 600'000'000'000ull);
            owner.settle(waited.process);
            evidence["worker_wait_code"] = child::wait_code_name(waited.code);
            if (waited.process && waited.process->exit_code_valid) evidence["worker_exit_code"] = waited.process->exit_code;
            if (waited.code != child::WaitCode::exited || !waited.process
                || !waited.process->exit_code_valid || waited.process->exit_code != 0)
                return {waited.code == child::WaitCode::cancelled ? Code::cancelled
                    : waited.code == child::WaitCode::timed_out ? Code::timed_out : Code::backend_failed,
                    false, "Native asset generation did not complete; inspect generation.log. No output was approved."};
            if (!result.worker_retired)
                return {Code::backend_failed, false, "Worker retirement was not confirmed; no asset can be approved."};
            return {Code::backend_failed, true, {}};
        }
        void timing_evidence(Json& evidence, const Result& result)
        {
            evidence["elapsed_ms"] = result.elapsed_ms;
            evidence["timing"] = {{"verification_and_load_preparation_ms", result.verification_ms},
                {"worker_runtime_ns", result.worker_runtime_ns}, {"independent_validation_ms", result.validation_ms},
                {"total_ms", result.elapsed_ms}, {"total_includes_receipt_commit", false},
                {"worker_runtime_source", "owned process snapshot"}};
            evidence["worker_retired"] = result.worker_retired;
        }
        bool retain_base_color_artifacts(const asset::model_import::BaseColorCompilation& compiled,
            Result& result, Json& evidence, std::stop_token stop,
            const native_runtime::AssetStagePermit* permit)
        {
            // Payloads are immutable job evidence. Only the final manifest
            // admits a complete bundle; cancellation/failure leaves no admitted
            // import and never mutates an ordinary project or an existing file.
            Json bundle{{"schema", "epoch.generated-model.base-color.v1"},
                {"source_glb_sha256", result.output_sha256},
                {"source_content", asset::texture::content_hash_hex(compiled.model.source_content)},
                {"meters_per_unit", compiled.model.meters_per_unit},
                {"model_approved", false}, {"imported", false},
                {"material_scope", "Opaque, single-sided, unit-factor base color. Not PBR rendering."},
                {"textures", Json::array()}, {"primitives", Json::array()}};
            std::error_code error{};
            std::unordered_map<std::string, std::string> written;
            const auto current = [&]() { return !stop.stop_requested()
                && (!permit || permit->valid()) && real_path(result.work_directory); };
            const auto retain = [&](const std::string& name, std::span<const std::byte> bytes)
            {
                if (!current() || bytes.empty()) return false;
                const auto digest = core::sha256::hex(core::sha256::hash(
                    std::span{reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()}));
                if (const auto existing = written.find(name); existing != written.end())
                    return existing->second == digest;
                if (!platform::filesystem::exclusive_create_and_write(result.work_directory / name, bytes, error))
                    return false;
                written.emplace(name, digest);
                return true;
            };
            for (const auto& texture : compiled.textures)
            {
                const auto& artifact = texture.artifact;
                const auto key = asset::texture::content_hash_hex(artifact.identity.key);
                const std::string file = "import-texture-" + key + ".epoch_texture";
                const auto serialized = asset::texture::serialize_compiled_artifact(artifact);
                if (!serialized || !retain(file, serialized.bytes)) return false;
                bundle["textures"].push_back({{"file", file}, {"source_image", texture.source_image},
                    {"artifact_key", key}, {"width", artifact.identity.width},
                    {"height", artifact.identity.height}, {"color_space", "srgb"},
                    {"serialized_bytes", serialized.bytes.size()}});
            }
            for (std::size_t index{}; index < compiled.model.primitives.size(); ++index)
            {
                const auto& primitive = compiled.model.primitives[index];
                const auto& material = compiled.model.materials.at(*primitive.material);
                const auto& binding = *material.base_color_texture;
                const auto key = asset::texture::content_hash_hex(primitive.geometry->key());
                const std::string file = "import-mesh-" + key + ".epoch_mesh";
                const auto bytes = asset::mesh::encode_artifact(*primitive.geometry);
                if (!retain(file, bytes)) return false;
                Json entry{{"file", file}, {"artifact_key", key},
                    {"vertices", primitive.geometry->vertices().size()},
                    {"indices", primitive.geometry->indices().size()},
                    {"source_node", primitive.node}, {"source_mesh", primitive.mesh},
                    {"source_primitive", primitive.primitive}, {"source_material", *primitive.material},
                    {"material_name", material.name}, {"texture", compiled.primitive_textures.at(index)},
                    {"base_color_factor", material.base_color}, {"metallic", material.metallic},
                    {"roughness", material.roughness}, {"emissive", material.emissive},
                    {"alpha_mode", "opaque"}, {"double_sided", material.double_sided},
                    {"texcoord", binding.texcoord}, {"wrap_s", binding.sampler.wrap_s},
                    {"wrap_t", binding.sampler.wrap_t}};
                if (binding.sampler.min_filter) entry["min_filter"] = *binding.sampler.min_filter;
                if (binding.sampler.mag_filter) entry["mag_filter"] = *binding.sampler.mag_filter;
                bundle["primitives"].push_back(std::move(entry));
            }
            if (!current()) return false;
            const auto manifest = result.work_directory / "compiled-base-color.json";
            if (!write_new(manifest, bundle)) return false;
            result.compiled_assets = manifest;
            evidence["project_import_bundle"] = {{"available", true}, {"manifest", path_text(manifest)},
                {"manifest_sha256", core::sha256::hex(core::sha256::hash(bundle.dump(2) + "\n"))},
                {"model_approved", false}, {"imported", false},
                {"scope", "Compiled artifacts only; scene approval/import and native UV/material rendering remain pending."}};
            return true;
        }

        Result failed(Result& result, Json& evidence, Clock::time_point started,
            bool owns_directory, Code code, std::string diagnostic)
        {
            result.code = code; result.diagnostic = std::move(diagnostic);
            result.compiled_assets.clear();
            if (evidence.contains("project_import_bundle"))
            {
                evidence["project_import_bundle"]["available"] = false;
                evidence["project_import_bundle"]["diagnostic"] =
                    "The parent generation stage failed or was cancelled. No project import was admitted.";
            }
            result.elapsed_ms = elapsed_ms(started);
            if (owns_directory)
            {
                evidence["state"] = code == Code::cancelled ? "cancelled" : "failed";
                evidence["diagnostic"] = result.diagnostic;
                evidence["failure_code"] = static_cast<unsigned>(code);
                phase(evidence, "failed_or_cancelled", started);
                timing_evidence(evidence, result);
                const auto path = result.work_directory / "result.json";
                if (write_new(path, evidence)) result.receipt = path;
            }
            log(result.diagnostic);
            return result;
        }
    }

    Result generate_image(const image_generation::Request& request,
        const image_generation::Configuration& configuration,
        const fs::path& work_directory, std::stop_token cancellation,
        const native_runtime::AssetStagePermit* permit)
    {
        Result result{.work_directory = work_directory};
        const auto started = std::chrono::steady_clock::now();
        std::optional<Clock::time_point> validation_started;
        Json evidence{{"schema", "epoch.asset_generation.image.v1"}, {"request", image_request(request)}};
        bool owns_directory{};
        const auto fail = [&](Code code, std::string diagnostic)
        {
            if (validation_started) result.validation_ms = elapsed_ms(*validation_started);
            if (!result.worker.valid()) result.verification_ms = elapsed_ms(started);
            return failed(result, evidence, started, owns_directory, code, std::move(diagnostic));
        };
        try
        {
        if (cancellation.stop_requested()) return fail(Code::cancelled, "Image generation cancelled before admission.");
        if (const auto diagnostic = image_generation::request_diagnostic(request); !diagnostic.empty())
            return fail(Code::invalid_request, diagnostic);
        if (!configuration.expected_executable.valid() || !real_path(configuration.executable)
            || !real_path(configuration.models_cache_root))
            return fail(Code::invalid_request, "The model cache or expected native executable identity is invalid; no hardware query started.");
        if (!work_directory.is_absolute() || work_directory.lexically_normal() != work_directory
            || work_directory.filename().empty() || !real_path(work_directory.parent_path()))
            return fail(Code::invalid_request, "Choose a new job directory below an existing ordinary local folder.");
        // No destructive model swap in this first lane. In-app handoff must
        // wait for the real lifecycle transaction rather than pretend pause frees VRAM.
        std::shared_ptr<native_runtime::WorkLease> work_lease;
        if (!model_work_guard(permit, work_lease))
            return fail(Code::model_work_active, "Coding models are busy or resident. Keep the coding session; queue generation until an explicit safe unload/handoff is available.");
        const auto host = admission::sample_host_resources();
        const auto host_state = admission::assess_memory_budget(host,
            admission::now_milliseconds(), 12u * gib);
        if (host_state != admission::Reason::ready)
            return fail(host_state == admission::Reason::memory_pressure
                ? Code::insufficient_memory : Code::resources_unavailable,
                std::string{admission::reason_message(host_state)});
        const auto free_gpu = free_nvidia_memory(cancellation);
        if (!free_gpu) return fail(Code::resources_unavailable, "Fresh free NVIDIA GPU memory could not be measured; no worker started.");
        // Conservative bring-up allowance, not a measured model peak or reservation.
        if (*free_gpu < 12u * gib)
            return fail(Code::insufficient_memory, "This unbenchmarked native image recipe currently needs 12 GiB free VRAM at admission, including headroom. No models were unloaded.");
        std::error_code error{};
        if (!fs::create_directory(work_directory, error) || error)
            return fail(Code::occupied_workspace, "The job directory already exists or could not be created; existing work was not replaced.");
        owns_directory = true;
        phase(evidence, "backend_preparation", started);
        auto invocation = image_generation::prepare(request, configuration, work_directory);
        if (!invocation.ready) return fail(Code::backend_unavailable, invocation.diagnostic);
        if (cancellation.stop_requested()) return fail(Code::cancelled, "Image generation cancelled during model verification; no worker started.");
        evidence.update(Json{{"schema", "epoch.asset_generation.image.v1"},
            {"request", image_request(request)}, {"backend", "stable-diffusion.cpp"},
            {"backend_revision", "be486498eadc29dc3cd234d944c2180ed42b1415"},
            {"executable_sha256", configuration.expected_executable.sha256},
            {"free_gpu_bytes_before", *free_gpu}, {"available_host_bytes_before", host.available_memory_bytes},
            {"admission_is_reservation", false}, {"models", Json::array()},
            {"imported", false}, {"reference_approved", false}});
        for (auto id : {image_generation::diffusion_package,
                image_generation::text_encoder_package, image_generation::decoder_package})
        {
            const auto plan = model_install::plan_for(id);
            evidence["models"].push_back(Json::parse(model_install::deterministic_receipt(*plan)));
        }
        if (!write_new(work_directory / "request.json", evidence))
            return fail(Code::evidence_failed, "The exact generation request could not be retained; no worker started.");
        result.verification_ms = elapsed_ms(started);
        phase(evidence, "worker", started);
        const auto run = run_worker(invocation.launch, result, evidence, work_lease, cancellation, permit);
        if (!run.succeeded || !result.worker_retired)
            return fail(run.code, run.diagnostic.empty() ? "The owned worker did not retire completely." : run.diagnostic);
        validation_started = Clock::now();
        phase(evidence, "independent_image_validation", started);
        const auto bytes = bounded_file(invocation.output, 64u * 1024u * 1024u);
        if (!bytes) return fail(Code::invalid_output, "The generated image could not be retained and hashed safely.");
        const auto imported = asset::texture::import_texture_memory({
            .source = std::as_bytes(std::span{bytes->data(), bytes->size()}), .extension = ".png"});
        if (!imported || imported.width != request.width || imported.height != request.height)
            return fail(Code::invalid_output, "The native output failed real image decoding or requested dimensions; no reference or import was approved.");
        if (cancellation.stop_requested()) return fail(Code::cancelled, "Image generation cancelled before reference acceptance; no import was approved.");
        core::sha256::Hasher hash{}; hash.update(*bytes);
        result.output = invocation.output;
        result.output_sha256 = core::sha256::hex(hash.finish());
        result.width = imported.width; result.height = imported.height;
        result.receipt = work_directory / "result.json";
        result.elapsed_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<
            std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
        result.validation_ms = elapsed_ms(*validation_started);
        evidence["state"] = "awaiting_reference_approval";
        evidence["output"] = path_text(result.output);
        evidence["output_sha256"] = result.output_sha256;
        evidence["decoded_bytes"] = imported.decoded_bytes;
        evidence["width"] = imported.width; evidence["height"] = imported.height;
        timing_evidence(evidence, result);
        if (cancellation.stop_requested() || (permit && !permit->valid()))
            return fail(Code::cancelled, "Image generation cancelled before reference receipt commit.");
        if (!write_new(result.receipt, evidence))
            return fail(Code::evidence_failed, "Image decoded successfully, but its result receipt could not be saved. Do not import it yet.");
        result.code = Code::awaiting_reference_approval;
        result.diagnostic = "Image generated and decoded. Review the reference before any 3D generation or project import.";
        log(result.diagnostic);
        return result;
        }
        catch (const std::exception& error) { return fail(Code::evidence_failed, std::string{"Image stage failed: "} + error.what()); }
        catch (...) { return fail(Code::evidence_failed, "Image stage failed before acceptance."); }
    }

    Result generate_model(const model_generation::Request& request,
        const model_generation::Configuration& configuration,
        const fs::path& work_directory, std::stop_token cancellation,
        const native_runtime::AssetStagePermit* permit)
    {
        const ModelBackend backend{
            .id = "trellis.cpp", .revision = "c0bed38c1578f7e36e3e50c8ff1e38fa0d47583f",
            .model_package_ids = {std::string{model_generation::model_package}},
            .terms_accepted = configuration.dinov3_terms_accepted,
            .preflight = [configuration](const model_generation::Request& value) -> std::string
            {
                if (!configuration.dinov3_terms_accepted)
                    return "DINOv3 conditioning requires explicit human acceptance of its separate license/access terms.";
                if (const auto reason = model_generation::request_diagnostic(value); !reason.empty()) return reason;
                if (!configuration.expected_executable.valid() || !real_path(configuration.executable)
                    || !real_path(configuration.models_cache_root))
                    return "The native model inputs or executable identity are invalid; no hardware query started.";
                return {};
            },
            .prepare = [configuration](const model_generation::Request& value, const fs::path& work)
            { return model_generation::prepare(value, configuration, work); }};
        return generate_model(request, backend, work_directory, cancellation, permit);
    }

    Result generate_model(const model_generation::Request& request,
        const ModelBackend& backend, const fs::path& work_directory,
        std::stop_token cancellation, const native_runtime::AssetStagePermit* permit)
    {
        Result result{.work_directory = work_directory};
        const auto started = Clock::now();
        std::optional<Clock::time_point> validation_started;
        bool owns_directory{};
        Json evidence{{"schema", "epoch.asset_generation.model.v1"}, {"backend", backend.id},
            {"backend_revision", backend.revision}, {"license_terms_accepted", backend.terms_accepted},
            {"imported", false}, {"reference_approved", true}, {"model_approved", false},
            {"request", {{"reference", path_text(request.reference_path)},
                {"approved_reference_sha256", request.approved_reference_sha256},
                {"seed", request.seed}, {"resolution", request.resolution}, {"steps", request.steps},
                {"face_budget", request.face_budget}, {"atlas", request.atlas}}}};
        const auto fail = [&](Code code, std::string reason)
        {
            if (validation_started) result.validation_ms = elapsed_ms(*validation_started);
            if (!result.worker.valid()) result.verification_ms = elapsed_ms(started);
            return failed(result, evidence, started, owns_directory, code, std::move(reason));
        };
        try
        {
            if (cancellation.stop_requested()) return fail(Code::cancelled, "3D generation cancelled before admission.");
            if (!backend.terms_accepted || !backend.preflight || !backend.prepare || backend.id.empty()
                || backend.id.size() > 256u || backend.revision.empty() || backend.revision.size() > 128u
                || backend.model_package_ids.empty() || backend.model_package_ids.size() > 16u)
                return fail(Code::invalid_request, "Choose an admitted backend with explicit license acceptance and pinned model provenance; no hardware query started.");
            if (const auto reason = backend.preflight(request); !reason.empty()) return fail(Code::invalid_request, reason);
            if (request.face_budget == 0u || request.face_budget > 1'000'000u || request.atlas < 64u
                || request.atlas > 4096u || (request.atlas & (request.atlas - 1u)) != 0u)
                return fail(Code::invalid_request, "The model face/texture budget is invalid; no hardware query started.");
            if (!work_directory.is_absolute() || work_directory.lexically_normal() != work_directory
                || work_directory.filename().empty() || !real_path(work_directory.parent_path()))
                return fail(Code::invalid_request, "Choose a new job directory under an existing ordinary local folder.");
            evidence["models"] = Json::array();
            for (const auto& id : backend.model_package_ids)
            {
                const auto plan = model_install::plan_for(id);
                if (!plan) return fail(Code::backend_unavailable, "A model component has no exact compiled install plan; no hardware query started.");
                evidence["models"].push_back(Json::parse(model_install::deterministic_receipt(*plan)));
            }
            // Snapshot the original once. The hash, PNG decoder and exclusive
            // job copy all consume these same bytes, not three changing files.
            auto reference = bounded_file(request.reference_path, 64u * 1024u * 1024u);
            if (!reference) return fail(Code::invalid_request, "The approved reference is missing, redirected or too large; no hardware query started.");
            core::sha256::Hasher reference_hash; reference_hash.update(*reference);
            const auto approved_hash = core::sha256::hex(reference_hash.finish());
            if (approved_hash != request.approved_reference_sha256)
                return fail(Code::invalid_request, "The reference bytes differ from the approved SHA-256; no hardware query started.");
            auto decoded_reference = asset::texture::import_texture_memory({
                .source = std::as_bytes(std::span{reference->data(), reference->size()}), .extension = ".png"});
            if (!decoded_reference) return fail(Code::invalid_request, "The approved reference must pass full PNG decoding before any hardware query.");
            const auto reference_width = decoded_reference.width, reference_height = decoded_reference.height;
            decoded_reference = {}; // Release compiled reference pixels before the heavy worker.
            if (cancellation.stop_requested()) return fail(Code::cancelled, "3D generation cancelled after reference verification.");
            std::shared_ptr<native_runtime::WorkLease> lease;
            if (!model_work_guard(permit, lease))
                return fail(Code::model_work_active, "Coding work owns model residency. Wait for the existing safe handoff; no coding model was interrupted.");
            const auto host = admission::sample_host_resources();
            const auto host_state = admission::assess_memory_budget(host, admission::now_milliseconds(), 12u * gib);
            if (host_state != admission::Reason::ready)
                return fail(host_state == admission::Reason::memory_pressure ? Code::insufficient_memory : Code::resources_unavailable,
                    std::string{admission::reason_message(host_state)});
            const auto free_gpu = free_nvidia_memory(cancellation);
            if (!free_gpu) return fail(Code::resources_unavailable, "Fresh NVIDIA free-memory telemetry is unavailable; no 3D worker started.");
            if (*free_gpu < 12u * gib)
                return fail(Code::insufficient_memory, "This unbenchmarked 3D lane requires 12 GiB free VRAM at admission. This is headroom policy, not a measured peak or reservation.");
            std::error_code error{};
            if (!fs::create_directory(work_directory, error) || error)
                return fail(Code::occupied_workspace, "The new 3D job directory already exists or is unavailable; prior work was not overwritten.");
            owns_directory = true;
            phase(evidence, "reference_snapshot", started);
            const auto frozen_reference = work_directory / "approved-reference.png";
            if (!platform::filesystem::exclusive_create_and_write(frozen_reference,
                    std::as_bytes(std::span{reference->data(), reference->size()}), error))
                return fail(Code::evidence_failed, "Could not preserve the verified reference in the owned job.");
            reference.reset();
            auto frozen_request = request;
            frozen_request.reference_path = frozen_reference;
            evidence["frozen_reference"] = path_text(frozen_reference);
            evidence["reference_sha256"] = approved_hash;
            evidence["free_gpu_bytes_before"] = *free_gpu;
            evidence["available_host_bytes_before"] = host.available_memory_bytes;
            evidence["admission_is_reservation"] = false;
            evidence["reference_dimensions"] = {reference_width, reference_height};
            evidence["gpu_peak_measured"] = false;
            if (!write_new(work_directory / "request.json", evidence))
                return fail(Code::evidence_failed, "The 3D request and immutable model provenance could not be retained.");
            if (cancellation.stop_requested() || (permit && !permit->valid()))
                return fail(Code::cancelled, "3D generation cancelled before backend preparation.");
            phase(evidence, "backend_preparation", started);
            const auto invocation = backend.prepare(frozen_request, work_directory);
            if (!invocation.ready) return fail(Code::backend_unavailable, invocation.diagnostic);
            if (invocation.reference_sha256 != approved_hash || invocation.required_face_budget != request.face_budget
                || invocation.launch.working_directory != work_directory || invocation.output.parent_path() != work_directory
                || invocation.output.filename().empty() || invocation.output.lexically_normal() != invocation.output
                || invocation.launch.merged_output_path != work_directory / "generation.log"
                || !invocation.launch.expected_executable || !invocation.launch.expected_executable->valid()
                || !invocation.launch.disconnect_standard_input)
                return fail(Code::backend_unavailable, "The backend invocation is not bound to this approved reference, budget, owned output and executable identity.");
            if (fs::exists(invocation.output) || fs::is_symlink(fs::symlink_status(invocation.output)))
                return fail(Code::occupied_workspace, "The 3D backend output already exists; it was not replaced.");
            evidence["executable_sha256"] = invocation.launch.expected_executable->sha256;
            evidence["arguments"] = invocation.launch.arguments;
            evidence["backend_diagnostic"] = invocation.diagnostic;
            if (!write_new(work_directory / "invocation.json", evidence))
                return fail(Code::evidence_failed, "The exact native invocation could not be retained before launch.");
            result.verification_ms = elapsed_ms(started);
            phase(evidence, "worker", started);
            const auto run = run_worker(invocation.launch, result, evidence, lease, cancellation, permit);
            if (!run.succeeded || !result.worker_retired)
                return fail(run.code, run.diagnostic.empty() ? "The 3D worker has not fully retired." : run.diagnostic);
            validation_started = Clock::now();
            phase(evidence, "independent_model_validation", started);
            const auto glb_bytes = bounded_file(invocation.output, 64u * 1024u * 1024u);
            if (!glb_bytes) return fail(Code::invalid_output, "The worker produced no bounded ordinary GLB output.");
            asset::model_import::Limits limits{};
            limits.geometry.maximum_vertices = static_cast<std::uint32_t>((std::min<std::uint64_t>)(1'048'576u, std::uint64_t(request.face_budget) * 3u));
            limits.geometry.maximum_indices = request.face_budget * 3u;
            limits.geometry.maximum_bytes = 64u * 1024u * 1024u;
            const auto parsed = asset::model_import::read_glb(
                std::as_bytes(std::span{glb_bytes->data(), glb_bytes->size()}), limits);
            if (!parsed) return fail(Code::invalid_output, "Independent GLB validation refused the output: " + parsed.diagnostic);
            if (parsed.images.empty() || parsed.materials.empty())
                return fail(Code::invalid_output, "The output has no embedded textures/materials; geometry-only fallback is not accepted as textured generation.");
            std::set<std::pair<std::uint32_t, bool>> texture_roles;
            const auto mark = [&](const std::optional<asset::model_import::TextureBinding>& binding, bool srgb)
            { if (binding) texture_roles.insert({binding->image, srgb}); };
            for (const auto& primitive : parsed.primitives)
            {
                if (!primitive.material || !primitive.texcoords_present)
                    return fail(Code::invalid_output, "Every generated primitive needs valid material coverage and TEXCOORD_0.");
                const auto& material = parsed.materials.at(*primitive.material);
                if (!material.base_color_texture)
                    return fail(Code::invalid_output, "A generated primitive has no base-color texture; untextured fallback was refused.");
                result.vertices += primitive.geometry->vertices().size();
                result.faces += primitive.geometry->indices().size() / 3u;
                mark(material.base_color_texture, true); mark(material.emissive_texture, true);
                mark(material.metallic_roughness_texture, false); mark(material.normal_texture, false); mark(material.occlusion_texture, false);
            }
            if (result.faces > request.face_budget || result.vertices > limits.geometry.maximum_vertices)
                return fail(Code::invalid_output, "Generated geometry exceeds the requested face/vertex budget.");
            evidence["textures"] = Json::array();
            // Decode every embedded image, including unused references. Compile
            // each used color/data role separately so linear data is not sRGB.
            for (std::uint32_t n = 0u; n < parsed.images.size(); ++n)
            {
                const auto& image = parsed.images[n];
                if (image.mime_type != "image/png")
                    return fail(Code::invalid_output, "This lane requires fully decoded embedded PNG textures; JPEG/WebP fallback is not accepted.");
                bool role_present{};
                for (bool srgb : {false, true})
                {
                    if (!texture_roles.contains({n, srgb})) continue;
                    role_present = true;
                    asset::texture::TextureCompileProfile profile{};
                    profile.color_space = srgb ? asset::texture::ColorSpace::srgb : asset::texture::ColorSpace::linear;
                    profile.format = srgb ? asset::texture::ArtifactFormat::rgba8_srgb : asset::texture::ArtifactFormat::rgba8_unorm;
                    const auto texture = asset::texture::import_texture_memory({.source = image.encoded,
                        .extension = ".png", .profile = profile,
                        .limits = {.maximum_source_bytes = 32u * 1024u * 1024u,
                            .maximum_decoded_bytes = 64u * 1024u * 1024u, .maximum_dimension = request.atlas}});
                    if (!texture) return fail(Code::invalid_output, "An embedded PNG failed full decoding, texture size or color/data compilation.");
                    result.decoded_texture_bytes += texture.decoded_bytes;
                    if (result.decoded_texture_bytes > 256u * 1024u * 1024u)
                        return fail(Code::invalid_output, "The generated texture set exceeds the decoded-memory budget.");
                    core::sha256::Hasher image_hash;
                    image_hash.update(std::span{reinterpret_cast<const std::uint8_t*>(image.encoded.data()), image.encoded.size()});
                    evidence["textures"].push_back({{"image", n}, {"sha256", core::sha256::hex(image_hash.finish())},
                        {"width", texture.width}, {"height", texture.height}, {"decoded_bytes", texture.decoded_bytes},
                        {"color_space", srgb ? "srgb" : "linear"},
                        {"compiled_artifact", asset::texture::content_hash_hex(texture.artifact.identity.key)}});
                }
                if (!role_present)
                {
                    const auto texture = asset::texture::import_texture_memory({.source = image.encoded, .extension = ".png",
                        .limits = {.maximum_source_bytes = 32u * 1024u * 1024u,
                            .maximum_decoded_bytes = 64u * 1024u * 1024u, .maximum_dimension = request.atlas}});
                    if (!texture) return fail(Code::invalid_output, "An unused embedded image is malformed or exceeds texture limits.");
                    result.decoded_texture_bytes += texture.decoded_bytes;
                    if (result.decoded_texture_bytes > 256u * 1024u * 1024u)
                        return fail(Code::invalid_output, "The embedded texture set exceeds the decoded-memory budget.");
                }
            }
            core::sha256::Hasher output_hash; output_hash.update(*glb_bytes);
            result.output_sha256 = core::sha256::hex(output_hash.finish());
            result.materials = static_cast<std::uint32_t>(parsed.materials.size());
            result.textures = static_cast<std::uint32_t>(parsed.images.size());
            if (cancellation.stop_requested() || (permit && !permit->valid()))
                return fail(Code::cancelled, "3D generation cancelled before validated output commit.");
            const auto validated = work_directory / "validated.glb";
            if (!platform::filesystem::exclusive_create_and_write(validated,
                    std::as_bytes(std::span{glb_bytes->data(), glb_bytes->size()}), error))
                return fail(Code::evidence_failed, "Could not retain the exact independently validated GLB snapshot.");
            result.output = validated;
            const auto base_color = asset::model_import::compile_base_color_glb(
                std::as_bytes(std::span{glb_bytes->data(), glb_bytes->size()}), limits, request.atlas,
                256u * 1024u * 1024u);
            if (base_color)
            {
                if (!retain_base_color_artifacts(base_color, result, evidence, cancellation, permit))
                    return fail(cancellation.stop_requested() || (permit && !permit->valid()) ? Code::cancelled : Code::evidence_failed,
                        "The base-color artifact bundle was not committed. Project source is unchanged; partial job evidence remains unapproved.");
            }
            else evidence["project_import_bundle"] = {{"available", false},
                {"status", base_color.code == asset::model_import::Code::unsupported_feature
                    ? "unsupported_material_or_input" : "compilation_unavailable"},
                {"compilation_code", static_cast<unsigned>(base_color.code)},
                {"diagnostic", base_color.diagnostic}, {"imported", false}};
            result.validation_ms = elapsed_ms(*validation_started);
            result.elapsed_ms = elapsed_ms(started);
            evidence["state"] = "awaiting_model_approval";
            evidence["source_output"] = path_text(invocation.output);
            evidence["output"] = path_text(validated);
            evidence["output_sha256"] = result.output_sha256;
            evidence["geometry"] = {{"vertices", result.vertices}, {"faces", result.faces},
                {"maximum_faces", request.face_budget}, {"maximum_vertices", limits.geometry.maximum_vertices},
                {"materials", result.materials}, {"embedded_images", result.textures}, {"meters_per_unit", parsed.meters_per_unit}};
            evidence["validation_scope"] = "Static geometry/material coverage and full PNG compilation only; no collision/LOD/rig/animation or game-ready acceptance.";
            timing_evidence(evidence, result);
            if (cancellation.stop_requested() || (permit && !permit->valid()))
                return fail(Code::cancelled, "3D generation cancelled before human-review receipt commit.");
            result.receipt = work_directory / "result.json";
            if (!write_new(result.receipt, evidence)) return fail(Code::evidence_failed, "Validated model receipt could not be saved; do not approve or import it yet.");
            result.code = Code::awaiting_model_approval;
            result.diagnostic = "Static model and embedded PNG materials validated. Human model review is required; nothing was imported or marked game-ready.";
            log(result.diagnostic);
            return result;
        }
        catch (const std::exception& error) { return fail(Code::evidence_failed, std::string{"3D stage failed: "} + error.what()); }
        catch (...) { return fail(Code::evidence_failed, "3D stage failed before acceptance."); }
    }

    namespace
    {
        PreparedJob generation_request(const Json& json)
        {
            const auto action=json.value("action", "");
            if (action == "generate_3d")
            {
                fields(json, {"schema", "action", "model", "backend", "models_cache_root", "work_directory"});
                const auto& spec = json.at("model");
                fields(spec, {"reference_path", "approved_reference_sha256", "seed", "resolution", "steps", "face_budget", "atlas"});
                const auto& backend = json.at("backend");
                model_generation::Request request{
                    .reference_path = path_value(spec.at("reference_path")),
                    .approved_reference_sha256 = spec.at("approved_reference_sha256").get<std::string>(),
                    .seed = static_cast<std::uint32_t>(integer(spec, "seed", 42u, 2'147'483'647u)),
                    .resolution = static_cast<std::uint32_t>(integer(spec, "resolution", 512u, 512u)),
                    .steps = static_cast<std::uint32_t>(integer(spec, "steps", 12u, 24u)),
                    .face_budget = static_cast<std::uint32_t>(integer(spec, "face_budget", 60'000u, 1'000'000u)),
                    .atlas = static_cast<std::uint32_t>(integer(spec, "atlas", 1024u, 4096u))};
                const auto kind = backend.value("kind", "trellis");
                if (kind == "triposr")
                {
                    fields(backend, {"kind", "executable", "bytes", "sha256", "worker_path",
                        "worker_sha256", "source_root", "source_manifest_sha256"});
                    triposr_generation::Configuration config{
                        .executable = path_value(backend.at("executable")),
                        .expected_executable = {.size_bytes = integer(backend, "bytes", 0u, 512u * 1024u * 1024u),
                            .sha256 = backend.at("sha256").get<std::string>()},
                        .worker_path = path_value(backend.at("worker_path")),
                        .expected_worker_sha256 = backend.at("worker_sha256").get<std::string>(),
                        .source_root = path_value(backend.at("source_root")),
                        .expected_source_manifest_sha256 = backend.at("source_manifest_sha256").get<std::string>(),
                        .models_cache_root = path_value(json.at("models_cache_root"))};
                    ModelBackend selected{
                        .id = "triposr-pytorch-offline", .revision = std::string{triposr_generation::source_revision},
                        .model_package_ids = {std::string{triposr_generation::model_package},
                            std::string{triposr_generation::dino_config_package}},
                        .terms_accepted = true,
                        .preflight = [config](const model_generation::Request& value)
                            { return triposr_generation::preflight(value, config); },
                        .prepare = [config](const model_generation::Request& value, const fs::path& work)
                            { return triposr_generation::prepare(value, config, work); }};
                    PreparedJob result;
                    result.action=Action::model;result.model=std::move(request);
                    result.alternative_model_backend=std::move(selected);
                    result.work_directory=path_value(json.at("work_directory"));
                    result.backend_name="TripoSR (local Python/PyTorch)";result.ready=true;return result;
                }
                if (kind != "trellis") throw std::invalid_argument{"Unknown local 3D backend; no worker started."};
                fields(backend, {"kind", "executable", "bytes", "sha256", "dinov3_terms_accepted"});
                model_generation::Configuration config{
                    .executable = path_value(backend.at("executable")),
                    .expected_executable = {.size_bytes = integer(backend, "bytes", 0u, 512u * 1024u * 1024u),
                        .sha256 = backend.at("sha256").get<std::string>()},
                    .models_cache_root = path_value(json.at("models_cache_root")),
                    .dinov3_terms_accepted = backend.value("dinov3_terms_accepted", false)};
                PreparedJob result;result.action=Action::model;result.model=std::move(request);
                result.model_backend=std::move(config);result.work_directory=path_value(json.at("work_directory"));
                result.backend_name="TRELLIS (local native backend)";result.ready=true;return result;
            }
            if (action != "generate_2d") throw std::invalid_argument{"Choose a generate_2d or generate_3d job settings file."};
            fields(json, {"schema", "action", "image", "backend", "models_cache_root", "work_directory"});
            const auto& spec = json.at("image");
            fields(spec, {"prompt", "negative_prompt", "seed", "width", "height", "steps"});
            image_generation::Request request{.prompt = spec.at("prompt").get<std::string>(),
                .negative_prompt = spec.value("negative_prompt", ""),
                .seed = static_cast<std::uint32_t>(integer(spec, "seed", 42u, (std::numeric_limits<std::uint32_t>::max)())),
                .width = static_cast<std::uint32_t>(integer(spec, "width", 1024u, 2048u)),
                .height = static_cast<std::uint32_t>(integer(spec, "height", 1024u, 2048u)),
                .steps = static_cast<std::uint32_t>(integer(spec, "steps", 4u, 8u))};
            const auto& backend = json.at("backend");
            fields(backend, {"executable", "bytes", "sha256"});
            image_generation::Configuration config{
                .executable = path_value(backend.at("executable")),
                .expected_executable = {.size_bytes = integer(backend, "bytes", 0u, 512u * 1024u * 1024u),
                    .sha256 = backend.at("sha256").get<std::string>()},
                .models_cache_root = path_value(json.at("models_cache_root"))};
            PreparedJob result;result.image=std::move(request);result.image_backend=std::move(config);
            result.work_directory=path_value(json.at("work_directory"));
            result.backend_name="FLUX.2 Klein (local native backend)";result.ready=true;return result;
        }
    }
    PreparedJob read_request_file(const fs::path& path) noexcept
    {
        try
        {
            const auto bytes=bounded_file(path,64u*1024u);
            if(!bytes) return {.diagnostic="Choose an ordinary absolute UTF-8 JSON job file up to 64 KiB."};
            const auto json=parse_request(*bytes);
            if(!json.is_object()||json.value("schema", "")!="epoch.asset_job.v1")
                return {.diagnostic="Unknown asset job schema."};
            return generation_request(json);
        }
        catch(const std::exception& error){return {.diagnostic=error.what()};}
        catch(...){return {.diagnostic="Could not read asset job settings."};}
    }
    Result execute(const PreparedJob& job,std::stop_token stop,const native_runtime::AssetStagePermit* permit)
    {
        if(!job) return {.diagnostic=job.diagnostic.empty()?"Asset job settings were not admitted.":job.diagnostic};
        if(job.action==Action::image)return generate_image(job.image,job.image_backend,job.work_directory,stop,permit);
        if(job.alternative_model_backend)return generate_model(job.model,*job.alternative_model_backend,job.work_directory,stop,permit);
        return generate_model(job.model,job.model_backend,job.work_directory,stop,permit);
    }
    int run_file(const fs::path& request_file) noexcept
    {
        try
        {
            const auto bytes=bounded_file(request_file,64u*1024u);
            if(!bytes){log("Asset request must be an ordinary absolute UTF-8 JSON file up to 64 KiB.");return 2;}
            const auto json=parse_request(*bytes);
            if(!json.is_object()||json.value("schema", "")!="epoch.asset_job.v1")
            {log("Unknown asset request schema; no worker started.");return 2;}
            if(json.value("action", "")=="publish_model_snapshot")
            {
                fields(json,{"schema","action","package_id","models_cache_root","staging_root"});
                const auto plan=model_install::plan_for(json.at("package_id").get<std::string>());
                if(!plan){log("Unknown pinned model package.");return 2;}
                const auto result=model_install::publish_verified_snapshot({.plan=*plan,
                    .models_cache_root=path_value(json.at("models_cache_root")),.staging_root=path_value(json.at("staging_root"))});
                log(result.diagnostic);return result.accepted()?0:1;
            }
            return execute(generation_request(json))?0:1;
        }
        catch(const std::exception& error){log(std::string{"Asset request failed before acceptance: "}+error.what());return 2;}
        catch(...){log("Asset request failed before acceptance.");return 2;}
    }
}
