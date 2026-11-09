/*
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
module;

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include "native/ai.backend_models_candidate_catalog.hpp"

module ai.model_install;

import core.sha256;
import platform.filesystem;

namespace epochengine::ai::model_install
{
    namespace
    {
        constexpr std::size_t maximum_receipt_bytes = 128u * 1024u;
        constexpr std::string_view qwen_package_id = "os_model_qwen_3_8_27b";
        constexpr std::string_view nemotron_package_id =
            "os_model_nemotron_3_nano_4b_bf16";

        [[nodiscard]] bool lower_hex(
            const std::string_view value,
            const std::size_t count) noexcept
        {
            return value.size() == count
                && std::all_of(value.begin(), value.end(), [](const char ch)
                {
                    return (ch >= '0' && ch <= '9')
                        || (ch >= 'a' && ch <= 'f');
                });
        }

        [[nodiscard]] bool safe_component(const std::string_view value) noexcept
        {
            if (value.empty() || value.size() > 192u || value == "."
                || value == "..")
                return false;
            return std::all_of(value.begin(), value.end(), [](const char ch)
            {
                const unsigned char byte = static_cast<unsigned char>(ch);
                return std::isalnum(byte) != 0 || ch == '.' || ch == '_'
                    || ch == '-';
            });
        }

        [[nodiscard]] bool valid_hugging_face_url(
            const InstallPlan& plan,
            const ArtifactSpec& artifact)
        {
            if (!plan.artifact_source.starts_with("https://huggingface.co/")
                || plan.artifact_source.ends_with('/')
                || plan.artifact_source.find('?') != std::string::npos
                || plan.artifact_source.find('#') != std::string::npos)
                return false;
            const std::string base = plan.artifact_source + "/resolve/"
                + plan.revision + "/" + artifact.source_path.value_or(artifact.file);
            return artifact.source_url == base
                || artifact.source_url == base + "?download=true";
        }

        [[nodiscard]] bool safe_source_path(const std::optional<std::string>& source)
        {
            if (!source) return true;
            if (source->empty() || source->size() > 768u) return false;
            std::size_t start{}, components{};
            while (start < source->size())
            {
                const auto end = source->find('/', start);
                const auto component = std::string_view{*source}.substr(start,
                    end == std::string::npos ? std::string::npos : end - start);
                if (++components > 8u || !safe_component(component)) return false;
                if (end == std::string::npos) return true;
                start = end + 1u;
            }
            return false;
        }

        [[nodiscard]] bool safe_root(
            const std::filesystem::path& root)
        {
            if (root.empty() || !root.is_absolute()
                || root.lexically_normal() != root
                || root == root.root_path())
                return false;
            for (const auto& component : root)
            {
                if (component == "..")
                    return false;
            }
            return true;
        }

        [[nodiscard]] std::filesystem::path normalized_absolute(
            const std::filesystem::path& path,
            std::error_code& error)
        {
            error.clear();
            const std::filesystem::path absolute =
                std::filesystem::absolute(path, error);
            return error ? std::filesystem::path{} : absolute.lexically_normal();
        }

        [[nodiscard]] std::string json_escape(const std::string_view value)
        {
            std::string escaped{};
            escaped.reserve(value.size() + 8u);
            for (const char ch : value)
            {
                switch (ch)
                {
                case '\\': escaped += "\\\\"; break;
                case '"': escaped += "\\\""; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped += ch; break;
                }
            }
            return escaped;
        }

        [[nodiscard]] std::string read_bounded(
            const std::filesystem::path& path,
            const std::size_t maximum_bytes)
        {
            std::error_code error{};
            if (!std::filesystem::is_regular_file(path, error) || error
                || std::filesystem::is_symlink(path, error) || error)
                return {};
            const std::uintmax_t size = std::filesystem::file_size(path, error);
            if (error || size == 0u || size > maximum_bytes)
                return {};
            std::ifstream input(path, std::ios::binary);
            if (!input)
                return {};
            std::string bytes(static_cast<std::size_t>(size), '\0');
            input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            return input && input.peek() == std::char_traits<char>::eof()
                ? bytes : std::string{};
        }

        [[nodiscard]] PublishResult failure(
            const Code code,
            const std::filesystem::path& installed_root,
            std::string diagnostic,
            std::string artifact = {})
        {
            return {
                .code = code,
                .installed_root = installed_root,
                .receipt_path = installed_root / receipt_filename,
                .artifact = std::move(artifact),
                .diagnostic = std::move(diagnostic)};
        }

        [[nodiscard]] PublishResult verify_artifacts(
            const InstallPlan& plan,
            const std::filesystem::path& root,
            const bool allow_receipt,
            const bool require_receipt)
        {
            std::error_code error{};
            if (!std::filesystem::is_directory(root, error) || error
                || std::filesystem::is_symlink(root, error) || error)
                return failure(Code::staging_missing, root,
                    "The model snapshot directory does not exist or is not a real directory.");

            std::set<std::string, std::less<>> expected{};
            for (const ArtifactSpec& artifact : plan.artifacts)
                expected.insert(artifact.file);
            if (allow_receipt)
                expected.insert(std::string{receipt_filename});

            std::set<std::string, std::less<>> observed{};
            for (std::filesystem::directory_iterator it{root, error}, end{};
                 !error && it != end; it.increment(error))
            {
                const std::string name = it->path().filename().string();
                if (!expected.contains(name) || !observed.insert(name).second)
                    return failure(Code::unexpected_entry, root,
                        "The model snapshot contains an unexpected entry.", name);
            }
            if (error)
                return failure(Code::filesystem_error, root,
                    "The model snapshot directory could not be enumerated.");

            for (const ArtifactSpec& artifact : plan.artifacts)
            {
                if (!observed.contains(artifact.file))
                    return failure(Code::missing_artifact, root,
                        "A required model artifact is missing.", artifact.file);
                const FileVerification verified = verify_file(
                    root / artifact.file, artifact);
                if (!verified.verified())
                    return failure(verified.code, root,
                        verified.diagnostic, artifact.file);
            }

            const std::filesystem::path receipt = root / receipt_filename;
            if (require_receipt
                && (!observed.contains(std::string{receipt_filename})
                    || !receipt_matches(plan,
                        read_bounded(receipt, maximum_receipt_bytes))))
                return failure(Code::receipt_invalid, root,
                    "The installed model receipt is missing or does not exactly match the snapshot.");

            return {
                .code = Code::published,
                .installed_root = root,
                .receipt_path = receipt,
                .diagnostic = "Every model artifact matched its exact size and SHA-256."};
        }

        [[nodiscard]] bool publish_receipt(
            const InstallPlan& plan,
            const std::filesystem::path& root,
            std::string& diagnostic)
        {
            const std::string receipt = deterministic_receipt(plan);
            if (receipt.empty())
            {
                diagnostic = "The model install plan could not produce a receipt.";
                return false;
            }
            const std::string digest = core::sha256::hex(
                core::sha256::hash(receipt));
            const std::filesystem::path temporary = root
                / (".installed.model." + digest.substr(0u, 16u) + ".tmp");
            std::error_code error{};
            (void)std::filesystem::remove(temporary, error);
            error.clear();
            const auto characters = std::span<const char>{
                receipt.data(), receipt.size()};
            if (!platform::filesystem::exclusive_create_and_write(
                    temporary, std::as_bytes(characters), error))
            {
                diagnostic = "The model receipt temporary file could not be written durably. "
                    "Filesystem error " + std::to_string(error.value()) + ": " + error.message()
                    + "; path characters=" + std::to_string(temporary.native().size()) + ".";
                return false;
            }
            if (!receipt_matches(plan,
                    read_bounded(temporary, maximum_receipt_bytes)))
            {
                (void)std::filesystem::remove(temporary, error);
                diagnostic = "The model receipt temporary file failed exact revalidation.";
                return false;
            }
            if (!platform::filesystem::atomic_replace_same_filesystem(
                    temporary, root / receipt_filename, error))
            {
                std::error_code cleanup_error{};
                (void)std::filesystem::remove(temporary, cleanup_error);
                diagnostic = "The model receipt could not be published atomically. Filesystem error "
                    + std::to_string(error.value()) + ": " + error.message() + ".";
                return false;
            }
            return true;
        }

        [[nodiscard]] InstallPlan qwen_plan()
        {
            constexpr std::string_view artifact_source =
                "https://huggingface.co/unsloth/Qwen3.8-27B-GGUF";
            constexpr std::string_view revision =
                "4ca720788d1e01f1bff70c033e0d0028fd02e502";
            constexpr std::string_view file = "Qwen3.8-27B-UD-Q4_K_M.gguf";
            return {
                .package_id = std::string{qwen_package_id},
                .display_name = "Qwen3.8 27B UD-Q4_K_M",
                .revision = std::string{revision},
                .official_source = "https://huggingface.co/Qwen/Qwen3.8-27B",
                .artifact_source = std::string{artifact_source},
                .artifacts = {{
                    .file = std::string{file},
                    .source_url = std::string{artifact_source} + "/resolve/"
                        + std::string{revision} + "/" + std::string{file}
                        + "?download=true",
                    .bytes = 16'464'440'224ull,
                    .sha256 = "322e194ff79741c7baa497c240f677f54b201b0efab44ca8e50f122b39123482"}}};
        }

        [[nodiscard]] InstallPlan nemotron_plan()
        {
            constexpr std::string_view source =
                "https://huggingface.co/nvidia/NVIDIA-Nemotron-3-Nano-4B-BF16";
            constexpr std::string_view revision =
                "dfaf35de3e30f1867dd8dbc38a7fc9fb52d3914f";
            const auto artifact = [](const std::string_view file,
                                      const std::uint64_t bytes,
                                      const std::string_view sha256)
            {
                constexpr std::string_view base =
                    "https://huggingface.co/nvidia/NVIDIA-Nemotron-3-Nano-4B-BF16";
                constexpr std::string_view commit =
                    "dfaf35de3e30f1867dd8dbc38a7fc9fb52d3914f";
                return ArtifactSpec{
                    .file = std::string{file},
                    .source_url = std::string{base} + "/resolve/"
                        + std::string{commit} + "/" + std::string{file}
                        + "?download=true",
                    .bytes = bytes,
                    .sha256 = std::string{sha256}};
            };
            return {
                .package_id = std::string{nemotron_package_id},
                .display_name = "NVIDIA Nemotron 3 Nano 4B BF16",
                .revision = std::string{revision},
                .official_source = std::string{source},
                .artifact_source = std::string{source},
                .artifacts = {
                    artifact("model.safetensors", 7'947'142'640ull,
                        "55d4e2519456c4a9bddf596b0748d630e3b2ce6ff6f4c2b7ed3e07e2b00dad42"),
                    artifact("tokenizer.json", 17'077'484ull,
                        "623c34567aebb18582765289fbe23d901c62704d6518d71866e0e58db892b5b7"),
                    artifact("config.json", 1'430ull,
                        "fde9241f66cd414458df444a80eb535f53aef2b4b240a91f092e651fa6f27219"),
                    artifact("generation_config.json", 171ull,
                        "4892b7a6418b50ad6854010881854d536c3efff148324455ad6535061aad28c5"),
                    artifact("tokenizer_config.json", 188'034ull,
                        "48de4056b0b17de26e03232fdc1f55b70595c9354ceb2ed061f724f45620aa41"),
                    artifact("special_tokens_map.json", 420ull,
                        "e3a4f63da745f02317a45e00e6476c17fc66ac41faf14bb1b0be1f3211b0ca53"),
                    artifact("chat_template.jinja", 10'504ull,
                        "ab7813c3abdd9cb655905a410728b26c7884eca45ddfab8d9f931553485a7862"),
                    artifact("configuration_nemotron_h.py", 12'119ull,
                        "07fa66e5b3da7e6a71c1a263e3dd68da11c8afa9178b47c49510ba628746fcff"),
                    artifact("modeling_nemotron_h.py", 78'629ull,
                        "ea982af0b805f181573f919ecb001d5bbc0153459923cf4b2f1ccae194e415a4"),
                    artifact("nano_v3_reasoning_parser.py", 798ull,
                        "aafb12208054504f619cbdd01837e1532a482ad937ed987bfe9a13fb812ae2b7")}};
        }

        [[nodiscard]] InstallPlan image_component_plan(
            const std::string_view package,
            const std::string_view display,
            const std::string_view official,
            const std::string_view source,
            const std::string_view revision,
            const std::string_view file,
            const std::uint64_t bytes,
            const std::string_view sha256)
        {
            return {
                .package_id = std::string{package},
                .display_name = std::string{display},
                .revision = std::string{revision},
                .official_source = std::string{official},
                .artifact_source = std::string{source},
                .artifacts = {{
                    .file = std::string{file},
                    .source_url = std::string{source} + "/resolve/"
                        + std::string{revision} + "/" + std::string{file},
                    .bytes = bytes,
                    .sha256 = std::string{sha256}}}};
        }

        [[nodiscard]] InstallPlan trellis_q4_plan()
        {
            constexpr std::string_view source = "https://huggingface.co/ilintar/trellis2-gguf";
            constexpr std::string_view revision = "a57397bd3d351599d9729fc144b3f87c3f87d65b";
            const auto artifact = [&](std::string_view file, std::uint64_t bytes, std::string_view sha)
            {
                const std::string remote = "q4/" + std::string{file};
                return ArtifactSpec{.file = std::string{file},
                    .source_url = std::string{source} + "/resolve/" + std::string{revision} + "/" + remote,
                    .bytes = bytes, .sha256 = std::string{sha}, .source_path = remote};
            };
            // DINOv3 has separate custom terms. Presence of this plan does not
            // authorize downloading/running it; the explicit human gate remains
            // in model_generation and the host package-consent boundary.
            return {.package_id = "os_model_trellis_2_cpp_q4_512",
                .display_name = "TRELLIS.2 native Q4 512 components (DINOv3 custom terms)",
                .revision = std::string{revision},
                .official_source = "https://huggingface.co/microsoft/TRELLIS.2-4B",
                .artifact_source = std::string{source},
                .artifacts = {
                    artifact("birefnet.gguf", 882'749'024ull,
                        "10c5dd4dcac904cf81c9a16180eb66f167dd52ab55867f54a44567b0b2babbc1"),
                    artifact("dinov3.gguf", 172'662'976ull,
                        "6473cf96fd275bf84f5cc0556975a2abaa10b641e4a07101dcad561df1917ef2"),
                    artifact("ss_flow.gguf", 730'496'672ull,
                        "a43c6393ee4a763a03e382de750d4f62752bacf969940442fd856230e62b88c2"),
                    artifact("ss_dec.gguf", 147'379'392ull,
                        "2790b5eecb261cc877d9bf175ce2bd6dd48cd65be8c042c5f5bc023dfca01cf7"),
                    artifact("shape_flow_512.gguf", 730'520'576ull,
                        "de7b87a92280035258c94314258e3b8314de39e5f001eac1ada0b4087785fb63"),
                    artifact("shape_dec.gguf", 845'423'552ull,
                        "79a52ddfed3454f724683940c11cfbfcf76c427ab3b7fdeb4c3cfc6d26647de4"),
                    artifact("tex_flow_512.gguf", 730'548'224ull,
                        "fff7bca6418ad607b8b9ff24034ff63f591d0a9b72f308960bffd6f5cd17390b"),
                    artifact("tex_dec.gguf", 845'414'272ull,
                        "20b208e402db907c6800dd4ad486a0d6e927e4511a6ff87fb51049794988927b")}};
        }

        [[nodiscard]] InstallPlan triposr_plan()
        {
            constexpr std::string_view source = "https://huggingface.co/stabilityai/TripoSR";
            constexpr std::string_view revision = "5b521936b01fbe1890f6f9baed0254ab6351c04a";
            const auto artifact = [&](std::string_view file, std::uint64_t bytes, std::string_view sha)
            {
                return ArtifactSpec{.file = std::string{file},
                    .source_url = std::string{source} + "/resolve/" + std::string{revision} + "/" + std::string{file},
                    .bytes = bytes, .sha256 = std::string{sha}};
            };
            // Model inputs only. The optional isolated Python runtime, offline
            // source and their licenses are separately admitted by the adapter.
            return {.package_id = "os_model_triposr", .display_name = "TripoSR image-to-3D weights and configuration (MIT)",
                .revision = std::string{revision}, .official_source = std::string{source}, .artifact_source = std::string{source},
                .artifacts = {artifact("model.ckpt", 1'677'246'742ull,
                    "429e2c6b22a0923967459de24d67f05962b235f79cde6b032aa7ed2ffcd970ee"),
                    artifact("config.yaml", 987u,
                        "74ca708ce086bf68e97709ea6b3d91f14717921c04691e84043f0eb8fcc68e62")}};
        }
    }

    std::optional<InstallPlan> plan_for(const std::string_view package_id)
    {
        for (const auto& offer : epoch_ai::model::catalog::offers())
        {
            if (offer.package_id != package_id
                || !epoch_ai::model::catalog::download_allowed(offer)) continue;
            const std::string source = "https://huggingface.co/" + std::string{offer.repo};
            return image_component_plan(package_id, offer.label, source, source,
                offer.revision, offer.filename, offer.bytes, offer.sha256);
        }
        if (package_id == qwen_package_id)
            return qwen_plan();
        if (package_id == nemotron_package_id)
            return nemotron_plan();
        if (package_id == "os_model_trellis_2_cpp_q4_512")
            return trellis_q4_plan();
        if (package_id == "os_model_triposr")
            return triposr_plan();
        if (package_id == "os_model_triposr_dino_v1_config")
            return image_component_plan(package_id, "DINOv1 ViT-B/16 local configuration only (Apache-2.0)",
                "https://huggingface.co/facebook/dino-vitb16", "https://huggingface.co/facebook/dino-vitb16",
                "f205d5d8e640a89a2b8ef0369670dfc37cc07fc2", "config.json", 454u,
                "b87c0270b97db085fd82cf114a761fd0f62ae7914fbd407c752a2260646b689c");
        if (package_id == "os_model_flux_2_klein_4b_q4_0")
            return image_component_plan(package_id,
                "FLUX.2 Klein 4B Q4_0 diffusion weights",
                "https://huggingface.co/black-forest-labs/FLUX.2-klein-4B",
                "https://huggingface.co/leejet/FLUX.2-klein-4B-GGUF",
                "3b1f5a9dc3abb32238b053aeb3d823c30afdacbd",
                "flux-2-klein-4b-Q4_0.gguf", 2'460'378'560ull,
                "d1023499ef3f2f82ff7c50e6778495195c1b6cc34835741778868428111f9ff4");
        if (package_id == "os_model_qwen_3_4b_image_encoder_q4_k_m")
            return image_component_plan(package_id,
                "Qwen3 4B Q4_K_M image text encoder",
                "https://huggingface.co/Qwen/Qwen3-4B",
                "https://huggingface.co/unsloth/Qwen3-4B-GGUF",
                "22c9fc8a8c7700b76a1789366280a6a5a1ad1120",
                "Qwen3-4B-Q4_K_M.gguf", 2'497'281'312ull,
                "f6f851777709861056efcdad3af01da38b31223a3ba26e61a4f8bf3a2195813a");
        if (package_id == "os_model_flux_2_small_decoder")
            return image_component_plan(package_id,
                "FLUX.2 complete encoder and small VAE decoder",
                "https://huggingface.co/black-forest-labs/FLUX.2-small-decoder",
                "https://huggingface.co/black-forest-labs/FLUX.2-small-decoder",
                "a3efc24f613ef42d9428af62fdbd6f5fd8856c4a",
                "full_encoder_small_decoder.safetensors", 249'519'092ull,
                "ea4273f02d1fafbf8e1d1c2cf6018ed8748652eb0bf34f2dd91171f16f15ab62");
        return std::nullopt;
    }

    bool valid_plan(const InstallPlan& plan)
    {
        if (!safe_component(plan.package_id) || plan.display_name.empty()
            || !lower_hex(plan.revision, 40u)
            || !plan.official_source.starts_with("https://")
            || plan.artifacts.empty() || plan.artifacts.size() > 64u)
            return false;
        std::set<std::string, std::less<>> files{};
        for (const ArtifactSpec& artifact : plan.artifacts)
        {
            if (!safe_component(artifact.file) || !safe_source_path(artifact.source_path) || artifact.bytes == 0u
                || !lower_hex(artifact.sha256, 64u)
                || !files.insert(artifact.file).second
                || !valid_hugging_face_url(plan, artifact))
                return false;
        }
        return true;
    }

    std::filesystem::path version_root(
        const std::filesystem::path& models_cache_root,
        const InstallPlan& plan)
    {
        if (!safe_root(models_cache_root) || !valid_plan(plan))
            return {};
        return models_cache_root / plan.package_id / "versions" / plan.revision;
    }

    std::filesystem::path staging_root(
        const std::filesystem::path& models_cache_root,
        const InstallPlan& plan,
        const std::string_view operation_id)
    {
        if (!safe_component(operation_id))
            return {};
        const std::filesystem::path installed = version_root(
            models_cache_root, plan);
        return installed.empty() ? std::filesystem::path{}
            : installed.parent_path() / (".stage-" + std::string{operation_id});
    }

    FileVerification verify_file(
        const std::filesystem::path& path,
        const ArtifactSpec& artifact)
    {
        std::error_code error{};
        if (!std::filesystem::exists(path, error) || error)
            return {Code::missing_artifact, 0u, {},
                "The required model artifact is missing."};
        if (!std::filesystem::is_regular_file(path, error) || error
            || std::filesystem::is_symlink(path, error) || error)
            return {Code::invalid_artifact_type, 0u, {},
                "The model artifact is not a real regular file."};
        const std::uintmax_t size = std::filesystem::file_size(path, error);
        if (error || size != artifact.bytes)
            return {Code::size_mismatch, static_cast<std::uint64_t>(size), {},
                "The model artifact size does not match the compiled plan."};

        std::ifstream input(path, std::ios::binary);
        if (!input)
            return {Code::filesystem_error, static_cast<std::uint64_t>(size), {},
                "The model artifact could not be opened for verification."};
        core::sha256::Hasher hasher{};
        std::array<std::uint8_t, 64u * 1024u> buffer{};
        std::uint64_t observed{};
        while (input)
        {
            input.read(reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size()));
            const std::streamsize count = input.gcount();
            if (count > 0)
            {
                observed += static_cast<std::uint64_t>(count);
                hasher.update(std::span<const std::uint8_t>{
                    buffer.data(), static_cast<std::size_t>(count)});
            }
        }
        if (!input.eof() || observed != artifact.bytes)
            return {Code::filesystem_error, observed, {},
                "The model artifact could not be read completely."};
        const std::string sha256 = core::sha256::hex(hasher.finish());
        if (sha256 != artifact.sha256)
            return {Code::sha256_mismatch, observed, sha256,
                "The model artifact SHA-256 does not match the compiled plan."};
        return {Code::published, observed, sha256,
            "The model artifact matches its exact size and SHA-256."};
    }

    std::string deterministic_receipt(const InstallPlan& plan)
    {
        if (!valid_plan(plan))
            return {};
        std::ostringstream out{};
        out << "{\n"
            << "  \"schema\": \"" << receipt_schema << "\",\n"
            << "  \"package_id\": \"" << json_escape(plan.package_id) << "\",\n"
            << "  \"revision\": \"" << plan.revision << "\",\n"
            << "  \"official_source\": \"" << json_escape(plan.official_source) << "\",\n"
            << "  \"artifact_source\": \"" << json_escape(plan.artifact_source) << "\",\n"
            << "  \"automatic_execution\": false,\n"
            << "  \"server_or_listener\": false,\n"
            << "  \"artifacts\": [\n";
        for (std::size_t index = 0u; index < plan.artifacts.size(); ++index)
        {
            const ArtifactSpec& artifact = plan.artifacts[index];
            out << "    {\"file\": \"" << json_escape(artifact.file) << "\"";
            if (artifact.source_path)
                out << ", \"source_path\": \"" << json_escape(*artifact.source_path) << "\"";
            out << ", \"source_url\": \"" << json_escape(artifact.source_url)
                << "\", \"bytes\": " << artifact.bytes
                << ", \"sha256\": \"" << artifact.sha256 << "\"}"
                << (index + 1u == plan.artifacts.size() ? "\n" : ",\n");
        }
        out << "  ]\n}\n";
        return out.str();
    }

    bool receipt_matches(
        const InstallPlan& plan,
        const std::string_view bytes) noexcept
    {
        try
        {
            return bytes == deterministic_receipt(plan);
        }
        catch (...)
        {
            return false;
        }
    }

    namespace detail
    {
    PublishResult verify_snapshot_for_plan(
        const std::filesystem::path& models_cache_root,
        const InstallPlan& plan)
    {
        if (!valid_plan(plan))
            return failure(Code::invalid_plan, {}, "The model install plan is invalid.");
        // A read must not normalize away a caller's '..' or traverse a redirected
        // cache ancestor. Publication and later admission retain separate gates.
        if (!safe_root(models_cache_root))
            return failure(Code::unsafe_cache_root, {},
                "The model cache root is not a normalized absolute directory.");
        const auto root = version_root(models_cache_root, plan);
        std::error_code error{};
        if (std::filesystem::canonical(root, error) != root || error)
            return failure(Code::unsafe_cache_root, root,
                "The installed model path is missing or redirects outside its exact location.");
        return verify_artifacts(plan, root, true, true);
    }

    PublishResult publish_verified_snapshot_for_plan(const PublishRequest& request)
    {
        if (!valid_plan(request.plan))
            return failure(Code::invalid_plan, {},
                "The model install plan is invalid.");
        std::error_code error{};
        const std::filesystem::path cache = normalized_absolute(
            request.models_cache_root, error);
        if (error || !safe_root(cache))
            return failure(Code::unsafe_cache_root, {},
                "The model cache root is not a safe absolute path.");
        const std::filesystem::path installed = version_root(cache, request.plan);

        if (std::filesystem::exists(installed, error) && !error)
        {
            PublishResult existing = verify_artifacts(
                request.plan, installed, true, false);
            if (!existing.accepted())
                return failure(Code::final_conflict, installed,
                    "The immutable model destination already exists but is not the compiled snapshot.",
                    existing.artifact);
            const std::string receipt = read_bounded(
                installed / receipt_filename, maximum_receipt_bytes);
            if (receipt_matches(request.plan, receipt))
            {
                existing.code = Code::already_installed;
                existing.diagnostic =
                    "The exact model snapshot and receipt are already installed.";
                return existing;
            }
            if (!publish_receipt(request.plan, installed, existing.diagnostic))
                return failure(Code::filesystem_error, installed,
                    existing.diagnostic);
            PublishResult recovered = verify_artifacts(
                request.plan, installed, true, true);
            if (!recovered.accepted())
                return recovered;
            recovered.code = Code::recovered_receipt;
            recovered.recovered = true;
            recovered.diagnostic =
                "The exact existing model snapshot received a recovered receipt.";
            return recovered;
        }
        if (error)
            return failure(Code::filesystem_error, installed,
                "The immutable model destination could not be inspected.");

        const std::filesystem::path staged = normalized_absolute(
            request.staging_root, error);
        const std::filesystem::path expected_parent = installed.parent_path();
        const std::string stage_name = staged.filename().string();
        if (error || staged.parent_path() != expected_parent
            || !stage_name.starts_with(".stage-")
            || !safe_component(stage_name.substr(7u)))
            return failure(Code::unsafe_staging_root, installed,
                "The staging root is not an owned sibling of the immutable destination.");

        PublishResult staged_result = verify_artifacts(
            request.plan, staged, false, false);
        if (!staged_result.accepted())
        {
            staged_result.installed_root = installed;
            staged_result.receipt_path = installed / receipt_filename;
            return staged_result;
        }
        if (!publish_receipt(request.plan, staged, staged_result.diagnostic))
            return failure(Code::filesystem_error, installed,
                staged_result.diagnostic);
        staged_result = verify_artifacts(request.plan, staged, true, true);
        if (!staged_result.accepted())
            return staged_result;

        std::filesystem::create_directories(expected_parent, error);
        if (error)
            return failure(Code::filesystem_error, installed,
                "The model versions directory could not be created.");
        std::filesystem::rename(staged, installed, error);
        if (error)
            return failure(Code::filesystem_error, installed,
                "The verified model snapshot could not be published atomically. Filesystem error "
                + std::to_string(error.value()) + " (" + error.category().name()
                + "): " + error.message() + "; source=" + staged.string()
                + "; destination=" + installed.string()
                + "; source characters=" + std::to_string(staged.native().size())
                + "; destination characters=" + std::to_string(installed.native().size()) + ".");

        PublishResult final_result = verify_artifacts(
            request.plan, installed, true, true);
        if (!final_result.accepted())
            return final_result;
        final_result.code = Code::published;
        final_result.published = true;
        final_result.diagnostic =
            "The verified model snapshot and exact receipt were published atomically.";
        return final_result;
    }
    }

    PublishResult publish_verified_snapshot(const PublishRequest& request)
    {
        const std::optional<InstallPlan> compiled = plan_for(
            request.plan.package_id);
        if (!compiled || *compiled != request.plan)
            return failure(Code::unknown_package, {},
                "Only an exact compiled model install plan may be published.");
        return detail::publish_verified_snapshot_for_plan(request);
    }

    PublishResult verify_snapshot(
        const std::filesystem::path& models_cache_root,
        const InstallPlan& plan)
    {
        const auto compiled = plan_for(plan.package_id);
        if (!compiled || *compiled != plan)
            return failure(Code::unknown_package, {},
                "Only an exact compiled model install plan may be admitted.");
        return detail::verify_snapshot_for_plan(models_cache_root, plan);
    }
}
