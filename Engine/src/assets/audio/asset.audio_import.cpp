/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

module asset.audio_import;

namespace epochengine::asset::audio
{
    namespace
    {
        struct WaveFormat final
        {
            std::uint16_t encoding{};
            std::uint16_t channels{};
            std::uint32_t sample_rate{};
            std::uint32_t byte_rate{};
            std::uint16_t block_align{};
            std::uint16_t bits_per_sample{};
        };

        struct ByteRange final
        {
            std::size_t offset{};
            std::size_t size{};
        };

        [[nodiscard]] constexpr bool has_bytes(
            std::size_t offset,
            std::size_t count,
            std::size_t size) noexcept
        {
            return offset <= size && count <= size - offset;
        }

        [[nodiscard]] constexpr std::uint16_t read_u16(
            const std::vector<std::uint8_t>& bytes,
            std::size_t offset) noexcept
        {
            return static_cast<std::uint16_t>(bytes[offset])
                | (static_cast<std::uint16_t>(bytes[offset + 1u]) << 8u);
        }

        [[nodiscard]] constexpr std::uint32_t read_u32(
            const std::vector<std::uint8_t>& bytes,
            std::size_t offset) noexcept
        {
            return static_cast<std::uint32_t>(bytes[offset])
                | (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u)
                | (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u)
                | (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
        }

        [[nodiscard]] bool tag_equals(
            const std::vector<std::uint8_t>& bytes,
            std::size_t offset,
            const char (&tag)[5]) noexcept
        {
            return has_bytes(offset, 4u, bytes.size())
                && bytes[offset] == static_cast<std::uint8_t>(tag[0])
                && bytes[offset + 1u] == static_cast<std::uint8_t>(tag[1])
                && bytes[offset + 2u] == static_cast<std::uint8_t>(tag[2])
                && bytes[offset + 3u] == static_cast<std::uint8_t>(tag[3]);
        }

        [[nodiscard]] AudioImportResult rejected(
            AudioImportStatus status,
            std::string diagnostic,
            std::uint64_t sourceBytes = 0u) noexcept
        {
            return {
                .status = status,
                .source_bytes = sourceBytes,
                .diagnostic = std::move(diagnostic)};
        }

        [[nodiscard]] float decode_pcm_sample(
            const std::uint8_t* sample,
            std::uint16_t bits) noexcept
        {
            switch (bits)
            {
            case 8u:
                return (static_cast<float>(*sample) - 128.0f) / 128.0f;
            case 16u:
            {
                const std::uint16_t encoded =
                    static_cast<std::uint16_t>(sample[0])
                    | (static_cast<std::uint16_t>(sample[1]) << 8u);
                const auto value = static_cast<std::int16_t>(encoded);
                return static_cast<float>(value) / 32'768.0f;
            }
            case 24u:
            {
                std::int32_t value =
                    static_cast<std::int32_t>(sample[0])
                    | (static_cast<std::int32_t>(sample[1]) << 8u)
                    | (static_cast<std::int32_t>(sample[2]) << 16u);
                if ((value & 0x0080'0000) != 0)
                    value |= static_cast<std::int32_t>(0xff00'0000u);
                return static_cast<float>(value) / 8'388'608.0f;
            }
            case 32u:
            {
                const std::uint32_t encoded =
                    static_cast<std::uint32_t>(sample[0])
                    | (static_cast<std::uint32_t>(sample[1]) << 8u)
                    | (static_cast<std::uint32_t>(sample[2]) << 16u)
                    | (static_cast<std::uint32_t>(sample[3]) << 24u);
                const auto value = static_cast<std::int32_t>(encoded);
                return static_cast<float>(
                    static_cast<double>(value) / 2'147'483'648.0);
            }
            default:
                return 0.0f;
            }
        }

        [[nodiscard]] float decode_float32_sample(
            const std::uint8_t* sample) noexcept
        {
            const std::uint32_t encoded =
                static_cast<std::uint32_t>(sample[0])
                | (static_cast<std::uint32_t>(sample[1]) << 8u)
                | (static_cast<std::uint32_t>(sample[2]) << 16u)
                | (static_cast<std::uint32_t>(sample[3]) << 24u);
            const float value = std::bit_cast<float>(encoded);
            return std::isfinite(value)
                ? (std::clamp)(value, -1.0f, 1.0f)
                : 0.0f;
        }
    }

    AudioImportResult import_audio_file(
        const AudioImportRequest& request) noexcept
    {
        if (request.source_path.empty()
            || !request.clip_id.valid()
            || !request.limits.valid())
        {
            return rejected(
                AudioImportStatus::invalid_request,
                "audio import request is incomplete");
        }

        try
        {
            std::error_code error{};
            const std::filesystem::path source =
                std::filesystem::absolute(
                    request.source_path, error).lexically_normal();
            if (error)
            {
                return rejected(
                    AudioImportStatus::filesystem_error,
                    "audio source path could not be resolved");
            }
            if (!std::filesystem::is_regular_file(source, error) || error)
            {
                return rejected(
                    AudioImportStatus::source_not_found,
                    "audio source file does not exist");
            }

            const std::uintmax_t sourceSize =
                std::filesystem::file_size(source, error);
            if (error)
            {
                return rejected(
                    AudioImportStatus::filesystem_error,
                    "audio source size could not be inspected");
            }
            if (sourceSize < 12u
                || sourceSize > request.limits.maximum_source_bytes
                || sourceSize
                    > static_cast<std::uintmax_t>(
                        (std::numeric_limits<std::size_t>::max)()))
            {
                return rejected(
                    sourceSize > request.limits.maximum_source_bytes
                        ? AudioImportStatus::source_too_large
                        : AudioImportStatus::malformed_source,
                    "audio source violates the bounded RIFF size policy",
                    static_cast<std::uint64_t>(sourceSize));
            }

            std::vector<std::uint8_t> bytes(
                static_cast<std::size_t>(sourceSize));
            std::ifstream input(source, std::ios::binary);
            if (!input
                || !input.read(
                    reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size())))
            {
                return rejected(
                    AudioImportStatus::filesystem_error,
                    "audio source could not be read completely",
                    static_cast<std::uint64_t>(sourceSize));
            }

            if (!tag_equals(bytes, 0u, "RIFF")
                || !tag_equals(bytes, 8u, "WAVE"))
            {
                return rejected(
                    AudioImportStatus::unsupported_container,
                    "audio source is not a RIFF/WAVE container",
                    static_cast<std::uint64_t>(sourceSize));
            }
            const std::uint32_t declaredRiffSize = read_u32(bytes, 4u);
            if (declaredRiffSize < 4u
                || static_cast<std::uint64_t>(declaredRiffSize) + 8u
                    > bytes.size())
            {
                return rejected(
                    AudioImportStatus::malformed_source,
                    "RIFF length exceeds the available source payload",
                    static_cast<std::uint64_t>(sourceSize));
            }

            WaveFormat format{};
            ByteRange data{};
            bool foundFormat{};
            bool foundData{};
            std::size_t cursor{12u};
            while (has_bytes(cursor, 8u, bytes.size()))
            {
                const std::size_t payload = cursor + 8u;
                const std::uint32_t chunkSize = read_u32(bytes, cursor + 4u);
                if (!has_bytes(payload, chunkSize, bytes.size()))
                {
                    return rejected(
                        AudioImportStatus::malformed_source,
                        "WAVE chunk exceeds the source payload",
                        static_cast<std::uint64_t>(sourceSize));
                }

                if (!foundFormat && tag_equals(bytes, cursor, "fmt "))
                {
                    if (chunkSize < 16u)
                    {
                        return rejected(
                            AudioImportStatus::malformed_source,
                            "WAVE format chunk is truncated",
                            static_cast<std::uint64_t>(sourceSize));
                    }
                    format = {
                        .encoding = read_u16(bytes, payload),
                        .channels = read_u16(bytes, payload + 2u),
                        .sample_rate = read_u32(bytes, payload + 4u),
                        .byte_rate = read_u32(bytes, payload + 8u),
                        .block_align = read_u16(bytes, payload + 12u),
                        .bits_per_sample = read_u16(bytes, payload + 14u)};
                    foundFormat = true;
                }
                else if (!foundData && tag_equals(bytes, cursor, "data"))
                {
                    data = {
                        .offset = payload,
                        .size = static_cast<std::size_t>(chunkSize)};
                    foundData = true;
                }

                const std::uint64_t next =
                    static_cast<std::uint64_t>(payload)
                    + chunkSize + (chunkSize & 1u);
                if (next > bytes.size())
                {
                    return rejected(
                        AudioImportStatus::malformed_source,
                        "WAVE chunk padding exceeds the source payload",
                        static_cast<std::uint64_t>(sourceSize));
                }
                cursor = static_cast<std::size_t>(next);
            }

            if (!foundFormat || !foundData || data.size == 0u)
            {
                return rejected(
                    AudioImportStatus::malformed_source,
                    "WAVE source requires bounded format and data chunks",
                    static_cast<std::uint64_t>(sourceSize));
            }
            if (format.channels == 0u
                || format.channels > request.limits.maximum_channels)
            {
                return rejected(
                    AudioImportStatus::channel_count_exceeded,
                    "WAVE channel count exceeds the project audio policy",
                    static_cast<std::uint64_t>(sourceSize));
            }
            if (format.sample_rate < request.limits.minimum_sample_rate
                || format.sample_rate > request.limits.maximum_sample_rate)
            {
                return rejected(
                    AudioImportStatus::sample_rate_exceeded,
                    "WAVE sample rate exceeds the project audio policy",
                    static_cast<std::uint64_t>(sourceSize));
            }

            const bool integerPcm = format.encoding == 1u
                && (format.bits_per_sample == 8u
                    || format.bits_per_sample == 16u
                    || format.bits_per_sample == 24u
                    || format.bits_per_sample == 32u);
            const bool floatPcm =
                format.encoding == 3u && format.bits_per_sample == 32u;
            if (!integerPcm && !floatPcm)
            {
                return rejected(
                    AudioImportStatus::unsupported_encoding,
                    "WAVE encoding must be PCM8/16/24/32 or IEEE float32",
                    static_cast<std::uint64_t>(sourceSize));
            }

            const std::uint16_t bytesPerSample =
                static_cast<std::uint16_t>(format.bits_per_sample / 8u);
            const std::uint32_t expectedAlign =
                static_cast<std::uint32_t>(format.channels)
                * bytesPerSample;
            if (format.block_align != expectedAlign
                || format.byte_rate
                    != format.sample_rate * expectedAlign
                || data.size % expectedAlign != 0u)
            {
                return rejected(
                    AudioImportStatus::malformed_source,
                    "WAVE block alignment or byte rate is inconsistent",
                    static_cast<std::uint64_t>(sourceSize));
            }

            const std::uint64_t frameCount = data.size / expectedAlign;
            const std::uint64_t sampleCount =
                frameCount * format.channels;
            if (frameCount == 0u
                || frameCount > request.limits.maximum_frames)
            {
                return rejected(
                    AudioImportStatus::frame_budget_exceeded,
                    "decoded WAVE frame count exceeds the project budget",
                    static_cast<std::uint64_t>(sourceSize));
            }
            if (sampleCount
                    > (std::numeric_limits<std::uint64_t>::max)()
                        / sizeof(float)
                || sampleCount * sizeof(float)
                    > request.limits.maximum_decoded_bytes
                || sampleCount
                    > static_cast<std::uint64_t>(
                        (std::numeric_limits<std::size_t>::max)()))
            {
                return rejected(
                    AudioImportStatus::decoded_budget_exceeded,
                    "decoded WAVE samples exceed the project budget",
                    static_cast<std::uint64_t>(sourceSize));
            }

            epochengine::audio::OwnedPcmClip clip{
                .id = request.clip_id,
                .format = {
                    .sample_format = epochengine::audio::
                        PcmSampleFormat::float32_interleaved,
                    .channel_layout = format.channels == 1u
                        ? epochengine::audio::PcmChannelLayout::mono
                        : epochengine::audio::PcmChannelLayout::stereo,
                    .sample_rate = format.sample_rate},
                .frame_count = frameCount};
            clip.interleaved_samples.resize(
                static_cast<std::size_t>(sampleCount));

            const std::uint8_t* samples = bytes.data() + data.offset;
            for (std::size_t index = 0u;
                index < clip.interleaved_samples.size();
                ++index)
            {
                const std::uint8_t* encoded =
                    samples + index * bytesPerSample;
                clip.interleaved_samples[index] = floatPcm
                    ? decode_float32_sample(encoded)
                    : decode_pcm_sample(encoded, format.bits_per_sample);
            }

            return {
                .status = AudioImportStatus::ready,
                .clip = std::move(clip),
                .source_bytes = static_cast<std::uint64_t>(sourceSize),
                .decoded_bytes = sampleCount * sizeof(float),
                .channel_count = format.channels,
                .source_bits_per_sample = format.bits_per_sample,
                .sample_rate = format.sample_rate,
                .diagnostic = "decoded bounded RIFF/WAVE source to float32 PCM"};
        }
        catch (const std::bad_alloc&)
        {
            return rejected(
                AudioImportStatus::allocation_failure,
                "audio import allocation failed");
        }
        catch (const std::filesystem::filesystem_error& error)
        {
            return rejected(
                AudioImportStatus::filesystem_error,
                error.what());
        }
        catch (...)
        {
            return rejected(
                AudioImportStatus::malformed_source,
                "audio import failed with an unknown bounded parser error");
        }
    }
}