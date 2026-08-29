/*
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

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
                + plan.revision + "/" + artifact.file;
            return artifact.source_url == base
                || artifact.source_url == base + "?download=true";
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
                diagnostic = "The model receipt temporary file could not be written durably.";
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
                (void)std::filesystem::remove(temporary, error);
                diagnostic = "The model receipt could not be published atomically.";
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
    }

    std::optional<InstallPlan> plan_for(const std::string_view package_id)
    {
        if (package_id == qwen_package_id)
            return qwen_plan();
        if (package_id == nemotron_package_id)
            return nemotron_plan();
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
            if (!safe_component(artifact.file) || artifact.bytes == 0u
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
            out << "    {\"file\": \"" << json_escape(artifact.file)
                << "\", \"source_url\": \"" << json_escape(artifact.source_url)
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
                "The verified model snapshot could not be published atomically.");

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
}
