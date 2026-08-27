/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

module asset.audio_import;

namespace epochengine::asset::audio
{
    namespace
    {
        struct ContractRoot final
        {
            std::filesystem::path path{};

            ContractRoot()
            {
                std::error_code error{};
                const auto stamp = std::chrono::steady_clock::now()
                    .time_since_epoch().count();
                path = std::filesystem::temp_directory_path(error)
                    / ("epoch_audio_import_contract_"
                        + std::to_string(stamp));
                if (error)
                {
                    path.clear();
                    return;
                }
                std::filesystem::create_directories(path, error);
                if (error)
                    path.clear();
            }

            ~ContractRoot()
            {
                if (path.empty())
                    return;
                std::error_code error{};
                std::filesystem::remove_all(path, error);
            }
        };

        void append_u16(
            std::vector<std::uint8_t>& bytes,
            std::uint16_t value)
        {
            bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
            bytes.push_back(
                static_cast<std::uint8_t>((value >> 8u) & 0xffu));
        }

        void append_u32(
            std::vector<std::uint8_t>& bytes,
            std::uint32_t value)
        {
            bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
            bytes.push_back(
                static_cast<std::uint8_t>((value >> 8u) & 0xffu));
            bytes.push_back(
                static_cast<std::uint8_t>((value >> 16u) & 0xffu));
            bytes.push_back(
                static_cast<std::uint8_t>((value >> 24u) & 0xffu));
        }

        void append_tag(
            std::vector<std::uint8_t>& bytes,
            std::string_view tag)
        {
            for (char value : tag)
                bytes.push_back(static_cast<std::uint8_t>(value));
        }

        [[nodiscard]] std::vector<std::uint8_t> pcm16_wave()
        {
            std::vector<std::uint8_t> bytes{};
            append_tag(bytes, "RIFF");
            append_u32(bytes, 0u);
            append_tag(bytes, "WAVE");

            append_tag(bytes, "fmt ");
            append_u32(bytes, 16u);
            append_u16(bytes, 1u);
            append_u16(bytes, 2u);
            append_u32(bytes, 48'000u);
            append_u32(bytes, 192'000u);
            append_u16(bytes, 4u);
            append_u16(bytes, 16u);

            append_tag(bytes, "data");
            append_u32(bytes, 8u);
            append_u16(bytes, 0x8000u);
            append_u16(bytes, 0x0000u);
            append_u16(bytes, 0x4000u);
            append_u16(bytes, 0x7fffu);

            const std::uint32_t riffSize =
                static_cast<std::uint32_t>(bytes.size() - 8u);
            bytes[4] = static_cast<std::uint8_t>(riffSize & 0xffu);
            bytes[5] =
                static_cast<std::uint8_t>((riffSize >> 8u) & 0xffu);
            bytes[6] =
                static_cast<std::uint8_t>((riffSize >> 16u) & 0xffu);
            bytes[7] =
                static_cast<std::uint8_t>((riffSize >> 24u) & 0xffu);
            return bytes;
        }

        [[nodiscard]] bool write_bytes(
            const std::filesystem::path& path,
            const std::vector<std::uint8_t>& bytes)
        {
            std::ofstream output(path, std::ios::binary);
            return output
                && static_cast<bool>(output.write(
                    reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size())));
        }
    }

    AudioImportContractFailure
        audio_import_contract_failure() noexcept
    {
        ContractRoot root{};
        if (root.path.empty())
            return AudioImportContractFailure::temporary_source;

        const auto source = pcm16_wave();
        const std::filesystem::path wave = root.path / "sample.wav";
        if (!write_bytes(wave, source))
            return AudioImportContractFailure::temporary_source;

        const AudioImportRequest request{
            .source_path = wave,
            .clip_id = {0x4155'4449'4f5f'5445ull, 1u}};
        const AudioImportResult first = import_audio_file(request);
        const AudioImportResult second = import_audio_file(request);
        if (!first
            || first.clip.frame_count != 2u
            || first.clip.interleaved_samples.size() != 4u
            || first.channel_count != 2u
            || first.source_bits_per_sample != 16u
            || first.sample_rate != 48'000u
            || first.decoded_bytes != 4u * sizeof(float)
            || std::abs(first.clip.interleaved_samples[0] + 1.0f)
                > 0.00001f
            || std::abs(first.clip.interleaved_samples[1])
                > 0.00001f
            || std::abs(first.clip.interleaved_samples[2] - 0.5f)
                > 0.00001f
            || first.clip.interleaved_samples[3] < 0.999f)
        {
            return AudioImportContractFailure::pcm16_decode;
        }
        if (!second
            || second.clip.format != first.clip.format
            || second.clip.frame_count != first.clip.frame_count
            || second.clip.interleaved_samples
                != first.clip.interleaved_samples)
        {
            return AudioImportContractFailure::deterministic_decode;
        }

        std::vector<std::uint8_t> malformed = source;
        malformed.resize(30u);
        const std::filesystem::path malformedPath =
            root.path / "malformed.wav";
        if (!write_bytes(malformedPath, malformed)
            || import_audio_file({
                .source_path = malformedPath,
                .clip_id = request.clip_id
            }).status != AudioImportStatus::malformed_source)
        {
            return AudioImportContractFailure::malformed_rejection;
        }

        AudioImportRequest bounded = request;
        bounded.limits.maximum_source_bytes = 44u;
        if (import_audio_file(bounded).status
            != AudioImportStatus::source_too_large)
        {
            return AudioImportContractFailure::oversized_rejection;
        }

        std::vector<std::uint8_t> unsupported = source;
        unsupported[20] = 6u;
        unsupported[21] = 0u;
        const std::filesystem::path unsupportedPath =
            root.path / "unsupported.wav";
        if (!write_bytes(unsupportedPath, unsupported)
            || import_audio_file({
                .source_path = unsupportedPath,
                .clip_id = request.clip_id
            }).status != AudioImportStatus::unsupported_encoding)
        {
            return AudioImportContractFailure::unsupported_rejection;
        }

        return AudioImportContractFailure::none;
    }
}

#if defined(EPOCH_AUDIO_IMPORT_CONTRACT_MAIN)
int main()
{
    return epochengine::asset::audio::audio_import_contract_failure()
            == epochengine::asset::audio::AudioImportContractFailure::none
        ? 0
        : 1;
}
#endif