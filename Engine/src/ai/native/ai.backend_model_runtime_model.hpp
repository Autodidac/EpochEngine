// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once

#include <cstdint>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "../ai.native_multimodel_gpu_budget.hpp"

#include <nlohmann/json.hpp>

namespace epoch_ai::model {

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
            const auto name = it->path().filename().string();
            if (!name.starts_with("mmproj") || !name.ends_with(".gguf")) continue;
            if (!it->is_regular_file(ec) || ec) return {};
            const auto bytes = it->file_size(ec);
            if (ec || bytes == 0u) return {};
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
