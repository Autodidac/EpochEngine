/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

export module asset.audio_import;

import audio.mixer;

export namespace epochengine::asset::audio
{
    enum class AudioImportStatus : std::uint8_t
    {
        ready,
        invalid_request,
        source_not_found,
        source_too_large,
        unsupported_container,
        unsupported_encoding,
        malformed_source,
        channel_count_exceeded,
        sample_rate_exceeded,
        frame_budget_exceeded,
        decoded_budget_exceeded,
        allocation_failure,
        filesystem_error
    };

    [[nodiscard]] constexpr std::string_view audio_import_status_name(
        AudioImportStatus status) noexcept
    {
        switch (status)
        {
        case AudioImportStatus::ready: return "ready";
        case AudioImportStatus::invalid_request: return "invalid_request";
        case AudioImportStatus::source_not_found: return "source_not_found";
        case AudioImportStatus::source_too_large: return "source_too_large";
        case AudioImportStatus::unsupported_container:
            return "unsupported_container";
        case AudioImportStatus::unsupported_encoding:
            return "unsupported_encoding";
        case AudioImportStatus::malformed_source: return "malformed_source";
        case AudioImportStatus::channel_count_exceeded:
            return "channel_count_exceeded";
        case AudioImportStatus::sample_rate_exceeded:
            return "sample_rate_exceeded";
        case AudioImportStatus::frame_budget_exceeded:
            return "frame_budget_exceeded";
        case AudioImportStatus::decoded_budget_exceeded:
            return "decoded_budget_exceeded";
        case AudioImportStatus::allocation_failure:
            return "allocation_failure";
        case AudioImportStatus::filesystem_error: return "filesystem_error";
        }
        return "unknown";
    }

    struct AudioImportLimits final
    {
        std::uint64_t maximum_source_bytes{128ull * 1024ull * 1024ull};
        std::uint64_t maximum_decoded_bytes{512ull * 1024ull * 1024ull};
        std::uint64_t maximum_frames{48'000ull * 60ull * 30ull};
        std::uint32_t minimum_sample_rate{8'000u};
        std::uint32_t maximum_sample_rate{192'000u};
        std::uint16_t maximum_channels{2u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_source_bytes >= 44u
                && maximum_decoded_bytes >= sizeof(float)
                && maximum_frames != 0u
                && minimum_sample_rate != 0u
                && maximum_sample_rate >= minimum_sample_rate
                && maximum_channels >= 1u
                && maximum_channels <= 2u;
        }
    };

    struct AudioImportRequest final
    {
        std::filesystem::path source_path{};
        epochengine::audio::ClipId clip_id{};
        AudioImportLimits limits{};
    };

    struct AudioImportResult final
    {
        AudioImportStatus status{AudioImportStatus::invalid_request};
        epochengine::audio::OwnedPcmClip clip{};
        std::uint64_t source_bytes{};
        std::uint64_t decoded_bytes{};
        std::uint16_t channel_count{};
        std::uint16_t source_bits_per_sample{};
        std::uint32_t sample_rate{};
        std::string diagnostic{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == AudioImportStatus::ready
                && clip.id.valid()
                && clip.frame_count != 0u
                && !clip.interleaved_samples.empty();
        }
    };

    [[nodiscard]] AudioImportResult import_audio_file(
        const AudioImportRequest& request) noexcept;

    enum class AudioImportContractFailure : std::uint8_t
    {
        none,
        temporary_source,
        pcm16_decode,
        deterministic_decode,
        malformed_rejection,
        oversized_rejection,
        unsupported_rejection
    };

    [[nodiscard]] constexpr std::string_view
        audio_import_contract_failure_name(
            AudioImportContractFailure failure) noexcept
    {
        switch (failure)
        {
        case AudioImportContractFailure::none: return "pass";
        case AudioImportContractFailure::temporary_source:
            return "temporary_source";
        case AudioImportContractFailure::pcm16_decode:
            return "pcm16_decode";
        case AudioImportContractFailure::deterministic_decode:
            return "deterministic_decode";
        case AudioImportContractFailure::malformed_rejection:
            return "malformed_rejection";
        case AudioImportContractFailure::oversized_rejection:
            return "oversized_rejection";
        case AudioImportContractFailure::unsupported_rejection:
            return "unsupported_rejection";
        }
        return "unknown";
    }

    [[nodiscard]] AudioImportContractFailure
        audio_import_contract_failure() noexcept;
}