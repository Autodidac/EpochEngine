// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace epoch_ai::model::catalog
{
    // Publisher metadata is an offer, never a registered profile or benchmark.
    enum class Format : std::uint8_t { gguf, safetensors_bundle };
    enum class Availability : std::uint8_t { pinned_download, blocked };
    struct Offer final
    {
        std::string_view id{};
        std::string_view package_id{};
        std::string_view label{};
        std::string_view repo{};
        std::string_view revision{};
        std::string_view filename{};
        std::string_view sha256{};
        std::string_view license{};
        std::string_view license_url{};
        std::string_view source_url{};
        std::string_view research_date{};
        std::string_view architecture{};
        std::string_view blocked_reason{};
        std::string_view notes{};
        std::uint64_t bytes{}, parameter_count{};
        std::uint32_t file_count{1};
        Format format{Format::gguf};
        Availability availability{Availability::blocked};
    };
    inline constexpr std::string_view evidence_status =
        "Publisher metadata checked 2026-10-29. Weights are not locally verified until installation. "
        "Backend, vision, tools, speed and memory fit have not been tested in Epoch.";
    inline constexpr std::string_view registration_notice =
        "Download uses an exact revision and SHA-256. Verify & Register is a separate action; "
        "it does not load or select the model. Existing profiles and tuning are preserved.";

    inline constexpr std::array<Offer, 6> candidate_offers{{
        {
            .id = "hauhau-qwen3-6-35b-a3b-uncensored-iq3-m",
            .package_id = "os_model_hauhau_qwen3_6_35b_iq3_m",
            .label = "Hauhau Qwen3.6 35B-A3B Uncensored IQ3_M",
            .repo = "HauhauCS/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive",
            .revision = "f12a584fecbeb5f20001130d8ecd66c9327ae685",
            .filename = "Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive-IQ3_M.gguf",
            .sha256 = "4dd9a27c68bd778ede351a553680a1a6e4ec62e5d383b549892860ea337be0c0",
            .license = "Apache-2.0 (publisher model card)",
            .license_url = "https://huggingface.co/HauhauCS/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive/blob/f12a584fecbeb5f20001130d8ecd66c9327ae685/README.md",
            .source_url = "https://huggingface.co/HauhauCS/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive/resolve/f12a584fecbeb5f20001130d8ecd66c9327ae685/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive-IQ3_M.gguf",
            .research_date = "2026-10-29",
            .architecture = "qwen35moe",
            .blocked_reason = "",
            .notes = "Publisher variant; not benchmarked in Epoch. Reserve memory for renderer, KV caches and temporary allocations; GPU/RAM placement must be tested.",
            .bytes = 15440519296ull,
            .parameter_count = 34660610688ull,
            .file_count = 1u,
            .format = Format::gguf,
            .availability = Availability::pinned_download},
        {
            .id = "hauhau-qwen3-6-35b-a3b-uncensored-q4-k-m",
            .package_id = "os_model_hauhau_qwen3_6_35b_q4_k_m",
            .label = "Hauhau Qwen3.6 35B-A3B Uncensored Q4_K_M",
            .repo = "HauhauCS/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive",
            .revision = "f12a584fecbeb5f20001130d8ecd66c9327ae685",
            .filename = "Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive-Q4_K_M.gguf",
            .sha256 = "bbef58c37ce88820be9d98b6437f1cf4bac890c947bd55fc7b68e22098574231",
            .license = "Apache-2.0 (publisher model card)",
            .license_url = "https://huggingface.co/HauhauCS/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive/blob/f12a584fecbeb5f20001130d8ecd66c9327ae685/README.md",
            .source_url = "https://huggingface.co/HauhauCS/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive/resolve/f12a584fecbeb5f20001130d8ecd66c9327ae685/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive-Q4_K_M.gguf",
            .research_date = "2026-10-29",
            .architecture = "qwen35moe",
            .blocked_reason = "",
            .notes = "Publisher variant; not benchmarked in Epoch. Reserve memory for renderer, KV caches and temporary allocations; GPU/RAM placement must be tested.",
            .bytes = 21166758016ull,
            .parameter_count = 34660610688ull,
            .file_count = 1u,
            .format = Format::gguf,
            .availability = Availability::pinned_download},
        {
            .id = "davidau-qwen3-6-27b-fable-fusion-uncensored-iq3-m",
            .package_id = "os_model_davidau_qwen3_6_27b_iq3_m",
            .label = "DavidAU Qwen3.6 27B Fable Fusion Uncensored IQ3_M",
            .repo = "DavidAU/Qwen3.6-27B-Fable-Fusion-711-Uncensored-Heretic-NM-DAU-NEO-MAX-MTP-GGUF",
            .revision = "81e6f49c3faf5b83c57a740521bd2974b6362fd1",
            .filename = "Qwen3.6-27B-Fable-Fus-711-UnHeretic-NM-DAU-NEO-MAX-NEO-IQ3_M.gguf",
            .sha256 = "4408a4815176070afb560ddadcf5883f524e159056e7e830274777a3a9c9b176",
            .license = "Apache-2.0 (publisher model card)",
            .license_url = "https://huggingface.co/DavidAU/Qwen3.6-27B-Fable-Fusion-711-Uncensored-Heretic-NM-DAU-NEO-MAX-MTP-GGUF/blob/81e6f49c3faf5b83c57a740521bd2974b6362fd1/README.md",
            .source_url = "https://huggingface.co/DavidAU/Qwen3.6-27B-Fable-Fusion-711-Uncensored-Heretic-NM-DAU-NEO-MAX-MTP-GGUF/resolve/81e6f49c3faf5b83c57a740521bd2974b6362fd1/Qwen3.6-27B-Fable-Fus-711-UnHeretic-NM-DAU-NEO-MAX-NEO-IQ3_M.gguf",
            .research_date = "2026-10-29",
            .architecture = "qwen35",
            .blocked_reason = "",
            .notes = "Publisher variant; not benchmarked in Epoch. Reserve memory for renderer, KV caches and temporary allocations; GPU/RAM placement must be tested.",
            .bytes = 14080729632ull,
            .parameter_count = 26895998464ull,
            .file_count = 1u,
            .format = Format::gguf,
            .availability = Availability::pinned_download},
        {
            .id = "davidau-qwen3-6-27b-fable-fusion-uncensored-q4-k-m",
            .package_id = "os_model_davidau_qwen3_6_27b_q4_k_m",
            .label = "DavidAU Qwen3.6 27B Fable Fusion Uncensored Q4_K_M",
            .repo = "DavidAU/Qwen3.6-27B-Fable-Fusion-711-Uncensored-Heretic-NM-DAU-NEO-MAX-MTP-GGUF",
            .revision = "81e6f49c3faf5b83c57a740521bd2974b6362fd1",
            .filename = "Qwen3.6-27B-Fable-Fus-711-UnHeretic-NM-DAU-NEO-MAX-NEO-Q4_K_M.gguf",
            .sha256 = "8440f2a076f149f65bf16975f3a08359936ba2658fc1827b51a30b717de49acb",
            .license = "Apache-2.0 (publisher model card)",
            .license_url = "https://huggingface.co/DavidAU/Qwen3.6-27B-Fable-Fusion-711-Uncensored-Heretic-NM-DAU-NEO-MAX-MTP-GGUF/blob/81e6f49c3faf5b83c57a740521bd2974b6362fd1/README.md",
            .source_url = "https://huggingface.co/DavidAU/Qwen3.6-27B-Fable-Fusion-711-Uncensored-Heretic-NM-DAU-NEO-MAX-MTP-GGUF/resolve/81e6f49c3faf5b83c57a740521bd2974b6362fd1/Qwen3.6-27B-Fable-Fus-711-UnHeretic-NM-DAU-NEO-MAX-NEO-Q4_K_M.gguf",
            .research_date = "2026-10-29",
            .architecture = "qwen35",
            .blocked_reason = "",
            .notes = "Publisher variant; not benchmarked in Epoch. Reserve memory for renderer, KV caches and temporary allocations; GPU/RAM placement must be tested.",
            .bytes = 18047255072ull,
            .parameter_count = 26895998464ull,
            .file_count = 1u,
            .format = Format::gguf,
            .availability = Availability::pinned_download},
        {
            .id = "research-qwen3-8-flash-next-abliterated-q4-k-m",
            .package_id = "",
            .label = "Qwen3.8 Flash Next Abliterated Q4_K_M",
            .repo = "windowsxp811203/Qwen3.8-Flash-Next-Abliterated-GGUF",
            .revision = "c3365c410baa29bdd3d7cc8cbc2bf9bee0de2f3a",
            .filename = "Qwen3.8-Flash-Next-Abliterated-Q4_K_M.gguf",
            .sha256 = "324c85132e04654480ac93923f444b760b2950eb8c84a346dd0ec70e680ecde2",
            .license = "Qwen Community License 1.0 (conditions apply)",
            .license_url = "https://huggingface.co/windowsxp811203/Qwen3.8-Flash-Next-Abliterated-GGUF/blob/c3365c410baa29bdd3d7cc8cbc2bf9bee0de2f3a/LICENSE",
            .source_url = "https://huggingface.co/windowsxp811203/Qwen3.8-Flash-Next-Abliterated-GGUF/tree/c3365c410baa29bdd3d7cc8cbc2bf9bee0de2f3a",
            .research_date = "2026-10-29",
            .architecture = "qwen4exp",
            .blocked_reason = "Weights alone exceed the 64 GiB target system RAM. Not offered for download on this target.",
            .notes = "Installed backend support is unverified; upstream support claims are not a local compatibility test. License review is separate.",
            .bytes = 119150722112ull,
            .parameter_count = 176943899520ull,
            .file_count = 1u,
            .format = Format::gguf,
            .availability = Availability::blocked},
        {
            .id = "research-rvn-qwen3-8-flash-next-uncensored-bf16",
            .package_id = "",
            .label = "RVN Qwen3.8 Flash Next Abliterated Uncensored BF16",
            .repo = "0bserverx/RVN-Qwen3.8-Flash-Next-Abliterated-Uncensored",
            .revision = "38b6e951065008472ebfb72d1736c9df64bb0e9a",
            .filename = "",
            .sha256 = "",
            .license = "Qwen Community License 1.0 (conditions apply)",
            .license_url = "https://huggingface.co/0bserverx/RVN-Qwen3.8-Flash-Next-Abliterated-Uncensored/blob/38b6e951065008472ebfb72d1736c9df64bb0e9a/LICENSE",
            .source_url = "https://huggingface.co/0bserverx/RVN-Qwen3.8-Flash-Next-Abliterated-Uncensored/tree/38b6e951065008472ebfb72d1736c9df64bb0e9a",
            .research_date = "2026-10-29",
            .architecture = "qwen4_exp_text",
            .blocked_reason = "No GGUF artifact in the researched revision; this native path cannot load the safetensors bundle. It also exceeds target RAM.",
            .notes = "Bundle size is the sum of 98 BF16 weight shards, not one downloadable GGUF. No single-file hash is claimed.",
            .bytes = 353887987296ull,
            .parameter_count = 176943899520ull,
            .file_count = 98u,
            .format = Format::safetensors_bundle,
            .availability = Availability::blocked},
    }};

    [[nodiscard]] constexpr std::span<const Offer> offers() noexcept { return candidate_offers; }
    [[nodiscard]] constexpr const Offer* find(std::string_view id) noexcept
    {
        for (const auto& offer : offers()) if (offer.id == id) return &offer;
        return nullptr;
    }
    [[nodiscard]] constexpr bool download_allowed(const Offer& offer) noexcept
    {
        return offer.availability == Availability::pinned_download
            && offer.format == Format::gguf && !offer.package_id.empty()
            && !offer.filename.empty() && offer.sha256.size() == 64u
            && offer.revision.size() == 40u && offer.bytes > 0u;
    }
    [[nodiscard]] constexpr bool manual_registration_allowed(const Offer& offer) noexcept
    { return download_allowed(offer); }
}
