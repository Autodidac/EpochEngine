// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace epochengine::ai::model_capability
{
    inline constexpr std::string_view edge0_id = "edge0-145b-q3k";
    inline constexpr std::string_view edge0_skeleton = "model-skeleton.gguf";
    inline constexpr std::uint32_t edge0_trial_kv_gpu_layers = 4u;
    inline constexpr std::uint32_t edge0_extended_trial_kv_gpu_layers = 8u;

    // Ordinal capability bands, not parameter counts or benchmark scores.
    // Unclassified is outside the ladder. Assignment never grants tool access.
    enum class Tier { unclassified, micro, low, medium, high, frontier_low, frontier };
    struct Band final {
        Tier tier;
        std::string_view key, label, reference, purpose;
    };
    inline constexpr std::array bands{
        Band{Tier::frontier, "frontier", "Frontier", "QwenNext Flash / paid frontier models",
            "Reference for demanding coding and broad multi-step tasks; verify every result."},
        Band{Tier::frontier_low, "frontier_low", "Frontier Low", "Edge0",
            "Alternate frontier-class reference; compare task correctness, latency and memory separately."},
        Band{Tier::high, "high", "High", "Current 30B reference models",
            "Complex bounded implementation and review, with host verification."},
        Band{Tier::medium, "medium", "Medium", "Current 27B reference models",
            "Grounded implementation, debugging and explanations with relevant source."},
        Band{Tier::low, "low", "Low", "Current 9B reference models",
            "Small edits, straightforward explanations and guided project operations."},
        Band{Tier::micro, "micro", "Micro", "Current 4B reference models",
            "Narrow classification, extraction and short, well-specified tasks."},
        Band{Tier::unclassified, "unclassified", "Unclassified", "Awaiting assessment / task-specific models",
            "No general capability rank yet. Evaluate before choosing automatic responder roles."}
    };
    [[nodiscard]] constexpr const Band& band(Tier tier) noexcept {
        for (const auto& value : bands) if (value.tier == tier) return value;
        return bands.back();
    }
    [[nodiscard]] constexpr std::optional<Tier> parse(std::string_view key) noexcept {
        for (const auto& value : bands) if (value.key == key) return value.tier;
        return {};
    }
    struct Assessment final { Tier tier{Tier::unclassified}; std::string_view basis; };
    // This is an observed restriction on one registered profile, not a size or
    // tier rule. Editing its tier must not re-admit failed coding behavior.
    [[nodiscard]] constexpr std::string_view coding_restriction(std::string_view model) noexcept {
        if (model.starts_with("qwenflash://")) model.remove_prefix(12u);
        return model.compare("qwen3.5-9b-gguf") == 0
            ? "Qwen3.5 9B is restricted to chat and advisory planning after failed coding checks. "
              "Choose a coding-capable loaded model, such as Flash Next, in Model Manager. "
              "No code generation, repair or coding fallback was dispatched."
            : std::string_view{};
    }
    // Dated fixed-fixture observations, independent of routing/tier assessment.
    // Report values describe exact quantizations and offload settings only.
    struct CodingMeasurement final {
        std::string_view id, label, quantization;
        int group{}, passed{}, attempts{}, gpu_layers{};
        bool modified{}, dense{};
        double ram_gib{}, commit_gib{}, vram_gib{}, decode_min{}, decode_median{}, decode_max{};
        double first_token_seconds{}, load_seconds{};
        std::string_view finding;
        int context_tokens{4096}, cpu_threads{4};
    };
    inline constexpr std::array coding_measurements{
        CodingMeasurement{"qwen3.8-flash-next-ud-q2-k-xl", "QwenNext Flash", "UD-Q2_K_XL",
            0, 10, 10, 49, false, false, 37.42, 9.46, 10.62, 8.79, 10.48, 12.45, 9.59, 16.76,
            "Primary coder. Five tasks passed in both runs. Earlier continuous-session RAM growth remains unresolved."},
        CodingMeasurement{"edge0-145b-q3k", "Edge0", "Q3_K",
            0, 8, 10, 49, false, false, 7.98, 10.41, 11.99, 2.98, 3.45, 4.51, 46.25, 26.50,
            "Lower-RAM alternative; explicit loading. Lost partial lines in both streaming-decoder checks."},
        CodingMeasurement{"qwen3.5-9b-gguf", "Qwen3.5 9B", "Q4_K_M",
            1, 2, 5, 49, false, true, 5.34, 6.33, 8.58, 118.15, 119.63, 122.17, 0.12, 2.58,
            "Chat and advisory planning only; not a coder or code drafter. Failed filenames, interval merging and streaming lines; CRLF answer looped until the fixture memory cap."},
        CodingMeasurement{"qwen3.8-27b-i1-iq4_xs-gguf-smaller", "Qwen3.8 27B", "IQ4_XS Smaller",
            1, 5, 5, 40, false, true, 12.81, 9.82, 11.68, 5.05, 5.15, 5.19, 2.61, 5.09,
            "All five fixed tasks passed. Slower partial GPU offload on this machine; five small tasks are not broad coding certification."},
        CodingMeasurement{"hauhau-qwen3-6-35b-a3b-uncensored-iq3-m", "Hauhau 35B-A3B", "IQ3_M",
            2, 3, 5, 49, true, false, 13.07, 6.45, 7.99, 25.55, 26.93, 28.38, 2.41, 2.58,
            "Failed empty-range bounds; streaming decoder did not compile. CPU expert offload; no GPU expert cache was created for this model."},
        CodingMeasurement{"hauhau-qwen3-6-35b-a3b-uncensored-q4-k-m", "Hauhau 35B-A3B", "Q4_K_M",
            2, 3, 5, 49, true, false, 17.88, 6.60, 8.21, 22.81, 23.56, 24.84, 2.88, 2.60,
            "Failed unsigned bounds and partial-line handling. CPU expert offload; no GPU expert cache was created for this model."},
        CodingMeasurement{"davidau-qwen3-6-27b-fable-fusion-uncensored-iq3-m", "Fable Fusion 27B", "IQ3_M",
            2, 4, 5, 40, true, true, 13.62, 11.02, 12.51, 4.74, 4.91, 5.15, 2.49, 5.61,
            "Deduplication did not compile (map used as a set). Other fixed tasks passed with MSVC; partial GPU offload."},
        CodingMeasurement{"davidau-qwen3-6-27b-fable-fusion-uncensored-q4-k-m", "Fable Fusion 27B", "Q4_K_M",
            2, 3, 5, 32, true, true, 17.20, 11.57, 13.12, 5.20, 5.33, 5.56, 2.58, 6.16,
            "Deduplication did not compile; streaming decoder lost partial lines. Partial GPU offload."},
        CodingMeasurement{"qwen3.8-27b-i1-iq4_xs-gguf-smaller", "Qwen3.8 27B / tuned 4K", "IQ4_XS Smaller",
            1, 5, 5, 56, false, true, 14.56, 14.24, 13.68, 16.25, 16.40, 16.65, 0.95, 5.16,
            "56/66 GPU layers, 12 CPU threads; five tasks passed. Smaller context, GPU f16 KV. Previous 40-layer baseline retained above.", 4096, 12},
        CodingMeasurement{"qwen3.8-27b-i1-iq4_xs-gguf-smaller", "Qwen3.8 27B / saved 80K", "IQ4_XS Smaller",
            1, 5, 5, 56, false, true, 15.97, 15.70, 13.69, 9.39, 9.51, 9.58, 1.04, 5.62,
            "Verified installed decoder; five tasks passed. Existing 81,920 capacity retained with CPU q4_0 KV and over 2 GiB free VRAM. Short prompts; full-window performance untested.", 81920, 12}
    };
    [[nodiscard]] constexpr Assessment assess(std::string_view id,
        std::string_view saved = "reference", bool modified = false) noexcept
    {
        if (const auto value = parse(saved)) return {*value, "Operator assessment; not automatic benchmark certification"};
        // Only exact established profile identities inherit an operator reference.
        // Never infer ability from B counts, file size, speed, or a substring.
        if (!modified) {
            if (id.compare("qwen3.8-flash-next-ud-q2-k-xl") == 0)
                return {Tier::frontier, "Operator reference: QwenNext Flash"};
            if (id.compare("edge0-145b-q3k") == 0)
                return {Tier::frontier_low, "Operator reference: Edge0; candidate admission is separate"};
            if (id.compare("qwen3.5-4b-gguf") == 0) return {Tier::micro, "Operator reference: current 4B model"};
            if (id.compare("qwen3.5-9b-gguf") == 0) return {Tier::low, "Operator reference: current 9B model"};
            if (id.compare("qwen3.8-27b-i1-iq4_xs-gguf-smaller") == 0
                || id.compare("qwen3.8-27b") == 0)
                return {Tier::medium, "Operator reference: current 27B model"};
        }
        return {Tier::unclassified, "No assessment for this exact profile; choose a tier after evaluation"};
    }
}
