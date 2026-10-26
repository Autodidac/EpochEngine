// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once

#include <cstdint>
#include <array>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "../ai.native_multimodel_gpu_budget.hpp"

#include <nlohmann/json.hpp>

namespace epoch_ai::model {

[[nodiscard]] inline bool is_vision_projector_filename(std::string_view name)
{
    std::string lower{name};
    for (auto& ch : lower) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
    return lower.find("mmproj") != std::string::npos;
}

namespace gguf_metadata_detail {

// Shared bounded prefix reader. Neither a prefix nor a file timestamp proves
// tensor integrity or runtime compatibility; the native loader owns those.
class Reader final
{
public:
    explicit Reader(const std::filesystem::path& path)
    {
        std::ifstream input{path, std::ios::binary};
        if (!input) return;
        input.read(reinterpret_cast<char*>(bytes_.data()), bytes_.size());
        size_ = static_cast<std::size_t>(input.gcount());
    }

    [[nodiscard]] std::optional<std::uint64_t> fields()
    {
        if (size_ < 24u || std::string_view{
                reinterpret_cast<const char*>(bytes_.data()), 4u} != "GGUF") return {};
        offset_ = 4u;
        const auto version = number(4u), tensors = number(8u), count = number(8u);
        if (!version || (*version != 2u && *version != 3u) || !tensors
            || !count || *count > 4096u) return {};
        return count;
    }

    [[nodiscard]] std::optional<std::uint64_t> number(std::size_t width)
    {
        if (width > 8u || offset_ > size_ || width > size_ - offset_) return {};
        std::uint64_t value{};
        for (std::size_t i{}; i < width; ++i) value |= std::uint64_t{bytes_[offset_++]} << (8u * i);
        return value;
    }

    [[nodiscard]] std::optional<std::string_view> text()
    {
        const auto length = number(8u);
        if (!length || *length > size_ - offset_) return {};
        const auto value = std::string_view{reinterpret_cast<const char*>(bytes_.data() + offset_), static_cast<std::size_t>(*length)};
        offset_ += static_cast<std::size_t>(*length);
        return value;
    }

    [[nodiscard]] bool skip(std::uint64_t type, unsigned depth = 0u)
    {
        if (depth > 4u) return false;
        constexpr std::array widths{1u, 1u, 2u, 2u, 4u, 4u, 4u, 1u, 0u, 0u, 8u, 8u, 8u};
        if (type >= widths.size()) return false;
        if (widths[type]) return number(widths[type]).has_value();
        if (type == 8u) return text().has_value();
        const auto element = number(4u), count = number(8u);
        if (!element || !count || *count > 4096u) return false;
        for (std::uint64_t i{}; i < *count; ++i) if (!skip(*element, depth + 1u)) return false;
        return true;
    }

private:
    std::array<unsigned char, 65'536u> bytes_{};
    std::size_t size_{};
    std::size_t offset_{};
};
} // namespace gguf_metadata_detail

// Inspect only bounded GGUF metadata, never tensor data. A suggestive filename
// is insufficient; the serving runtime must still confirm actual image input.
[[nodiscard]] inline bool has_vision_projector_metadata(const std::filesystem::path& path)
{
    gguf_metadata_detail::Reader input{path};
    const auto fields = input.fields();
    if (!fields) return false;
    bool clip{}, vision{};
    for (std::uint64_t i{}; i < *fields; ++i) {
        const auto key = input.text(); const auto type = input.number(4u);
        if (!key || !type) return false;
        if (*key == "general.architecture") {
            if (*type != 8u) return false;
            const auto value = input.text(); if (!value || *value != "clip") return false;
            clip = true;
        } else if (*key == "clip.has_vision_encoder") {
            if (*type != 7u) return false;
            const auto value = input.number(1u); if (!value || *value != 1u) return false;
            vision = true;
        } else if (!input.skip(*type)) return false;
        if (clip && vision) return true;
    }
    return false;
}

struct GgufLayerMetadata final
{
    std::string architecture;
    std::uint32_t block_count{};
    std::optional<std::uint32_t> prediction_blocks;

    // Pinned llama runtime: n_layer_all includes prediction blocks; -ngl also
    // counts the output layer. This is not the main forward-pass layer count.
    [[nodiscard]] std::uint32_t offload_capacity() const noexcept
    {
        return block_count + 1u;
    }
};

[[nodiscard]] inline std::optional<GgufLayerMetadata> local_gguf_layer_metadata(
    const std::filesystem::path& path)
{
    gguf_metadata_detail::Reader input{path};
    const auto fields = input.fields();
    if (!fields) return {};
    GgufLayerMetadata result;
    bool blocks{};
    for (std::uint64_t i{}; i < *fields; ++i)
    {
        const auto key = input.text(); const auto type = input.number(4u);
        if (!key || !type) return {};
        if (*key == "general.architecture")
        {
            if (*type != 8u || !result.architecture.empty()) return {};
            const auto value = input.text();
            if (!value || value->empty() || value->size() > 64u) return {};
            for (const char c : *value)
                if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return {};
            result.architecture = *value;
        }
        else if (!result.architecture.empty()
            && (*key == result.architecture + ".block_count"
                || *key == result.architecture + ".nextn_predict_layers"))
        {
            if (*type != 4u) return {};
            const auto value = input.number(4u);
            if (!value) return {};
            if (*key == result.architecture + ".block_count")
            {
                if (blocks || *value == 0u || *value >=
                    epochengine::ai::native_runtime::gpu_budget::maximum_gpu_layers) return {};
                result.block_count = static_cast<std::uint32_t>(*value);
                blocks = true;
            }
            else
            {
                if (result.prediction_blocks) return {};
                result.prediction_blocks = static_cast<std::uint32_t>(*value);
            }
        }
        // Tokenizer arrays can exceed this prefix. Layer-placement fields are
        // conventionally before them; never scan tensor data or trust a guess
        // when the required fields have not been found in this bounded prefix.
        else if (key->starts_with("tokenizer.") && blocks)
            break;
        else if (!input.skip(*type)) return {};
    }
    if (!blocks || result.architecture.empty()
        || result.prediction_blocks.value_or(0u) > result.block_count) return {};
    return result;
}

// One metadata entry owned by the serving runtime. Cached successes and failures
// are invalidated when path, file size or write time changes; no persistent
// profile is rewritten. Runtime loading still validates the actual GGUF.
class GgufLayerMetadataCache final
{
public:
    [[nodiscard]] std::optional<GgufLayerMetadata> get(const std::filesystem::path& path)
    {
        std::scoped_lock lock{mutex_};
        std::error_code error;
        const auto bytes = std::filesystem::file_size(path, error);
        if (error) { entry_.reset(); return {}; }
        const auto stamp = std::filesystem::last_write_time(path, error);
        if (error) { entry_.reset(); return {}; }
        if (entry_ && entry_->path == path && entry_->bytes == bytes && entry_->stamp == stamp)
            return entry_->metadata;
        auto metadata = local_gguf_layer_metadata(path);
        const auto afterBytes = std::filesystem::file_size(path, error);
        if (error || afterBytes != bytes) { entry_.reset(); return {}; }
        const auto afterStamp = std::filesystem::last_write_time(path, error);
        if (error || afterStamp != stamp) { entry_.reset(); return {}; }
        entry_ = Entry{path, bytes, stamp, std::move(metadata)};
        return entry_->metadata;
    }

private:
    struct Entry final
    {
        std::filesystem::path path;
        std::uintmax_t bytes;
        std::filesystem::file_time_type stamp;
        std::optional<GgufLayerMetadata> metadata;
    };
    std::mutex mutex_;
    std::optional<Entry> entry_;
};

// Use only a unique, already-installed sidecar beside this model or inside
// its exact Hugging Face snapshot. Never download or guess from another repo.
[[nodiscard]] inline std::filesystem::path matching_cached_vision_projector(
    const std::filesystem::path& model)
{
    if (model.empty()) return {};
    const auto folder = model.parent_path();
    const auto snapshot = folder.parent_path();
    const std::array folders{folder,
        snapshot.parent_path().filename() == "snapshots" ? snapshot : std::filesystem::path{}};
    std::filesystem::path found;
    for (const auto& directory : folders)
    {
        if (directory.empty()) continue;
        std::error_code ec;
        std::size_t visited{};
        for (std::filesystem::directory_iterator it{directory, ec}, end;
            !ec && it != end && visited++ < 128u; it.increment(ec))
        {
            auto name = it->path().filename().string();
            for (auto& ch : name) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
            if (!is_vision_projector_filename(name) || !name.ends_with(".gguf")) continue;
            const auto marker = name.find("mmproj");
            if (marker != 0u) {
                auto prefix = name.substr(0u, marker);
                while (!prefix.empty() && (prefix.back() == '.' || prefix.back() == '-' || prefix.back() == '_')) prefix.pop_back();
                auto modelName = model.filename().string();
                for (auto& ch : modelName) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
                if (prefix.empty() || !modelName.starts_with(prefix) || modelName.size() <= prefix.size()
                    || (modelName[prefix.size()] != '.' && modelName[prefix.size()] != '-' && modelName[prefix.size()] != '_')) continue;
            }
            if (!it->is_regular_file(ec) || ec) return {};
            const auto bytes = it->file_size(ec);
            if (ec || bytes == 0u) return {};
            if (!has_vision_projector_metadata(it->path())) continue;
            if (!found.empty() && found != it->path()) return {};
            found = it->path();
        }
        if (ec || visited >= 128u) return {};
    }
    return found;
}

// The serving runtime advertises loaded image support independently of a
// model's name/family. Missing or malformed metadata never confirms vision.
[[nodiscard]] inline std::optional<bool> runtime_image_input_capability(const nlohmann::json& properties)
{
    if (!properties.is_object() || !properties.contains("modalities")) return {};
    const auto& modalities = properties.at("modalities");
    if (!modalities.is_object() || !modalities.contains("vision")
        || !modalities.at("vision").is_boolean()) return {};
    return modalities.at("vision").get<bool>();
}

// All native models use their layer-count hint: slider 0..100 maps to
// 0..model layers, with automatic VRAM and cohort limits. This is NOT a
// percentage of VRAM. Multi-model cohorts additionally cap weight budgets.
inline constexpr std::uint32_t gpu_offload_maximum_layers =
    epochengine::ai::native_runtime::gpu_budget::maximum_gpu_layers;
// The former scale belongs only to migration of old manifests; do not use
// it to calculate any new model's layer count.
inline constexpr std::uint32_t gpu_offload_legacy_ceiling_percent = 70U;
inline constexpr std::uint32_t gpu_offload_default_percent = 100U;
inline constexpr std::uint32_t runtime_profile_revision = 15U;

enum class RuntimeKind {
    internal_keymaker,
    qwen_flash,
    external_openai,
};

enum class ModelSourceKind {
    internal_generation,
    hugging_face,
    local_gguf,
    external_provider,
};

// Persisted first-class model/runtime profile. Fields intentionally describe
// model identity, runtime policy, and context policy separately so Epoch AI can
// expose one coherent model library without coupling agent orchestration to a
// particular inference backend.
struct RuntimeModelProfile {
    std::string id;
    std::string display_name;
    RuntimeKind runtime{RuntimeKind::qwen_flash};
    ModelSourceKind source{ModelSourceKind::hugging_face};
    std::string repo;
    std::string quant;
    std::filesystem::path local_path;
    std::uint64_t estimated_bytes{};
    std::uint32_t profile_revision{runtime_profile_revision};

    // Runtime policy. Recommended keeps the proven QwenFlashRuntime
    // scheduling/memory defaults while honoring Epoch AI's GPU layer-offload
    // target. Automatic derives safe memory/batching values from the current
    // machine while using the same GPU target. Manual exposes all controls.
    std::string runtime_policy{"recommended"};
    std::uint32_t gpu_offload_percent{gpu_offload_default_percent};
    std::uint32_t gpu_layer_count_hint{48};
    // Transient per-launch VRAM safeguard; never persisted to manifest JSON.
    bool multimodel_memory_saver{false};

    // Context / concurrency. Qwen3.8-Flash-Next is 262,144 native; Epoch AI's
    // recommended 5080/64 GiB profile starts at 81,920 with one slot so the
    // entire window belongs to the active agent rather than being divided.
    std::uint32_t context_tokens{81'920};
    std::uint32_t parallel_slots{1};
    std::uint32_t context_reserve_tokens{4'096};
    bool auto_compact{true};
    double compact_trigger_ratio{0.97};
    double compact_target_ratio{0.72};

    // Runtime scheduling / memory policy.
    std::uint32_t threads{12};
    std::uint32_t batch_threads{12};
    std::uint32_t batch_size{1024};
    std::uint32_t ubatch_size{256};
    std::string gpu_layers{"auto"};
    bool flash_attention{true};
    std::string kv_type_k{"q4_0"};
    std::string kv_type_v{"q4_0"};
    std::uint32_t kv_gpu_layers{0};
    bool kv_offload{false}; // Manual/automatic override. Recommended preserves the proven QFR backend default.
    bool kv_cpu_pinned{true};
    bool phase_aware_workspace{true};
    std::string load_mode{"mmap"};
    std::string lazy_mode{"on"};
    std::string tensor_override{"per_layer_token_embd.weight=CPU"};
    std::uint32_t prompt_cache_mib{2048};
    bool context_shift{false};
    bool reasoning_preserve{false};

    // Sparse/MoE hot/cold residency policy. `auto` enables the cache only
    // when the selected runtime advertises compatibility with the model
    // layout; `off` is the safe baseline; `on` is an explicit manual force.
    std::string expert_cache_mode{"auto"};
    std::uint32_t expert_cache_mib{6144};
    std::uint32_t pinned_host_mib{12288};
    bool early_router{false};

    // Optional speculative path. Stable default remains off until an MTP/draft
    // sidecar is configured for the selected model.
    bool speculative_enabled{false};
    std::string speculative_type{"draft-mtp"};
    std::string speculative_repo;
    std::filesystem::path speculative_model;
    std::uint32_t speculative_tokens{2};

    bool enabled{true};
};

[[nodiscard]] std::string_view to_string(RuntimeKind value) noexcept;
[[nodiscard]] std::string_view to_string(ModelSourceKind value) noexcept;
[[nodiscard]] std::string runtime_model_uri(const RuntimeModelProfile& profile);
[[nodiscard]] std::uint32_t effective_context_tokens(const RuntimeModelProfile& profile) noexcept;
// Resolve llama.cpp -ngl. The slider uses the model layer-count hint; automatic
// runtime resolution additionally applies weight/VRAM limits.
[[nodiscard]] std::string effective_gpu_layers(const RuntimeModelProfile& profile);
[[nodiscard]] nlohmann::json to_json(const RuntimeModelProfile& profile);
[[nodiscard]] RuntimeModelProfile runtime_model_from_json(const nlohmann::json& value);

} // namespace epoch_ai::model
