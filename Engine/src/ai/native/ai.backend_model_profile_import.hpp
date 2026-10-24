#pragma once

#include "ai.backend_model_runtime_model.hpp"
#include <algorithm>
#include <expected>
#include <fstream>
#include <set>
#include <stdexcept>
#include <vector>

namespace epoch_ai::model {

struct ProfileImport final {
    std::vector<nlohmann::json> registrations;
    std::size_t skipped{};
};

// Only inference registration metadata crosses edition boundaries. Never copy a
// runtime directory, conversation, approval, tool command or training state.
[[nodiscard]] inline std::expected<ProfileImport, std::string> read_profile_import(
    const std::filesystem::path& selected, const std::set<std::string>& existing_ids)
{
    using json = nlohmann::json;
    namespace fs = std::filesystem;
    try {
        constexpr std::string_view builtin = "qwen3.8-flash-next-ud-q2-k-xl";
        static const std::set<std::string> allowed{
            "format", "id", "display_name", "runtime", "source", "repo", "quant",
            "local_path", "estimated_bytes", "profile_revision", "runtime_policy",
            "gpu_offload_percent", "gpu_layer_count_hint", "context_tokens", "parallel_slots",
            "effective_context_tokens", "context_reserve_tokens", "auto_compact",
            "compact_trigger_ratio", "compact_target_ratio", "threads", "batch_threads",
            "batch_size", "ubatch_size", "gpu_layers", "flash_attention", "kv_type_k",
            "kv_type_v", "kv_gpu_layers", "kv_offload", "kv_cpu_pinned",
            "phase_aware_workspace", "load_mode", "lazy_mode", "tensor_override",
            "prompt_cache_mib", "context_shift", "reasoning_preserve", "expert_cache_mode",
            "expert_cache_mib", "pinned_host_mib", "early_router", "speculative_enabled",
            "speculative_type", "speculative_repo", "speculative_model", "speculative_tokens",
            "enabled", "provider_model", "kind"};
        const auto safe_file = [](const fs::path& path) {
            const auto status = fs::symlink_status(path);
            return !fs::is_symlink(status) && fs::is_regular_file(status);
        };
        if (selected.empty() || fs::is_symlink(fs::symlink_status(selected)))
            throw std::invalid_argument("Select a profile manifest or its runtime folder.");
        std::vector<fs::path> files;
        if (safe_file(selected)) files.push_back(selected);
        else if (fs::is_directory(selected)) {
            if (safe_file(selected / "manifest.json")) files.push_back(selected / "manifest.json");
            else {
                std::size_t visited{};
                for (const auto& entry : fs::directory_iterator(selected)) {
                    if (++visited > 64u) throw std::invalid_argument("Profile folder contains too many entries.");
                    const auto status = entry.symlink_status();
                    if (!fs::is_symlink(status) && fs::is_directory(status)
                        && safe_file(entry.path() / "manifest.json"))
                        files.push_back(entry.path() / "manifest.json");
                    if (files.size() > 16u) throw std::invalid_argument("Import at most 16 profiles at once.");
                }
            }
        }
        if (files.empty()) throw std::invalid_argument("No runtime profile manifests were found.");
        std::ranges::sort(files);
        ProfileImport result;
        std::set<std::string> seen;
        for (const auto& file : files) {
            const auto bytes = fs::file_size(file);
            if (!bytes || bytes > 65'536u) throw std::invalid_argument("Profile manifest must be 1 to 65536 bytes.");
            std::ifstream input(file, std::ios::binary);
            std::string text(static_cast<std::size_t>(bytes), '\0');
            if (!input.read(text.data(), static_cast<std::streamsize>(bytes)) || input.peek() != EOF)
                throw std::invalid_argument("Profile manifest changed or could not be read.");
            std::set<std::string> keys;
            const auto value = json::parse(text, [&](int depth, json::parse_event_t event, json& parsed) {
                if (event == json::parse_event_t::key) {
                    const auto key = parsed.get<std::string>();
                    if (depth != 1 || !allowed.contains(key) || !keys.insert(key).second)
                        throw std::invalid_argument("Profile contains an unsupported or duplicate field.");
                }
                return true;
            });
            if (!value.is_object() || value.size() != allowed.size())
                throw std::invalid_argument("Import requires a complete current runtime profile.");
            for (const auto& item : value.items())
                if (item.value().is_structured()) throw std::invalid_argument("Profile fields must be scalar values.");
            if (value.at("format") != "engcoder.runtime-model.v4"
                || value.at("runtime") != "qwen-flash" || value.at("kind") != "foundation")
                throw std::invalid_argument("Only current native inference profiles can be imported.");
            const auto id = value.at("id").get<std::string>();
            if (id.empty() || id.size() > 128u
                || !((id.front() >= 'a' && id.front() <= 'z') || (id.front() >= '0' && id.front() <= '9'))
                || id.back() == '.' || id.back() == '-' || id.find("--") != std::string::npos
                || !std::ranges::all_of(id, [](unsigned char c) {
                    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
                }) || !seen.insert(id).second)
                throw std::invalid_argument("Profile id is invalid or repeated.");
            if (id == builtin || existing_ids.contains(id)) { ++result.skipped; continue; }
            if (value.at("source") != "local-gguf" || value.at("enabled") != true
                || value.at("profile_revision") != runtime_profile_revision)
                throw std::invalid_argument("Import supports enabled current local GGUF registrations only.");
            const auto profile = runtime_model_from_json(value);
            const auto normalized = to_json(profile);
            // Refuse old/clamped/defaulted tuning instead of silently retuning.
            if (normalized != value)
                throw std::invalid_argument("Profile requires migration; its saved tuning was not changed.");
            if (!profile.local_path.is_absolute() || !safe_file(profile.local_path)
                || fs::file_size(profile.local_path) == 0u)
                throw std::invalid_argument("The profile's existing local GGUF file is unavailable.");
            auto spec = value;
            for (const auto* key : {"repo", "quant", "speculative_repo", "speculative_model"})
                if (spec.at(key).is_null()) spec.erase(key);
            result.registrations.push_back(std::move(spec));
        }
        return result;
    } catch (const std::exception& error) {
        return std::unexpected(std::string{error.what()});
    }
}

} // namespace epoch_ai::model
