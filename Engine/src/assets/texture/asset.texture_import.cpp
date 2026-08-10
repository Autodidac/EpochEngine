/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

module asset.texture_import;

import asset.texture_artifact;

namespace epochengine::asset::texture
{
    namespace
    {
        namespace fs = std::filesystem;

        struct DecodedImage final
        {
            TextureImportStatus status{TextureImportStatus::malformed_source};
            std::uint32_t width{};
            std::uint32_t height{};
            std::vector<std::byte> rgba{};
        };

        [[nodiscard]] constexpr bool checked_multiply(
            std::uint64_t left,
            std::uint64_t right,
            std::uint64_t& output) noexcept
        {
            if (left != 0u
                && right > (std::numeric_limits<std::uint64_t>::max)() / left)
            {
                return false;
            }
            output = left * right;
            return true;
        }

        [[nodiscard]] constexpr std::uint8_t byte_at(
            std::span<const std::byte> bytes,
            std::size_t offset) noexcept
        {
            return std::to_integer<std::uint8_t>(bytes[offset]);
        }

        [[nodiscard]] bool read_u16(
            std::span<const std::byte> bytes,
            std::size_t offset,
            std::uint16_t& output) noexcept
        {
            if (offset > bytes.size() || bytes.size() - offset < 2u)
                return false;
            output = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(byte_at(bytes, offset))
                | (static_cast<std::uint16_t>(byte_at(bytes, offset + 1u))
                    << 8u));
            return true;
        }

        [[nodiscard]] bool read_u32(
            std::span<const std::byte> bytes,
            std::size_t offset,
            std::uint32_t& output) noexcept
        {
            if (offset > bytes.size() || bytes.size() - offset < 4u)
                return false;
            output = static_cast<std::uint32_t>(byte_at(bytes, offset))
                | (static_cast<std::uint32_t>(byte_at(bytes, offset + 1u))
                    << 8u)
                | (static_cast<std::uint32_t>(byte_at(bytes, offset + 2u))
                    << 16u)
                | (static_cast<std::uint32_t>(byte_at(bytes, offset + 3u))
                    << 24u);
            return true;
        }

        [[nodiscard]] bool read_i32(
            std::span<const std::byte> bytes,
            std::size_t offset,
            std::int32_t& output) noexcept
        {
            std::uint32_t raw{};
            if (!read_u32(bytes, offset, raw))
                return false;
            std::memcpy(&output, &raw, sizeof(output));
            return true;
        }

        [[nodiscard]] bool checked_decoded_size(
            std::uint32_t width,
            std::uint32_t height,
            const TextureImportLimits& limits,
            std::uint64_t& decodedBytes) noexcept
        {
            std::uint64_t pixels{};
            return width != 0u
                && height != 0u
                && width <= limits.maximum_dimension
                && height <= limits.maximum_dimension
                && checked_multiply(width, height, pixels)
                && checked_multiply(pixels, 4u, decodedBytes)
                && decodedBytes <= limits.maximum_decoded_bytes
                && decodedBytes <= (std::numeric_limits<std::size_t>::max)();
        }

        [[nodiscard]] std::string lowercase_extension(const fs::path& path)
        {
            std::string extension = path.extension().string();
            std::transform(
                extension.begin(),
                extension.end(),
                extension.begin(),
                [](unsigned char value)
                {
                    return static_cast<char>(std::tolower(value));
                });
            return extension;
        }

        [[nodiscard]] DecodedImage decode_ppm(
            std::span<const std::byte> source,
            const TextureImportLimits& limits)
        {
            if (source.size() < 3u
                || byte_at(source, 0u) != static_cast<std::uint8_t>('P')
                || byte_at(source, 1u) != static_cast<std::uint8_t>('6'))
            {
                return {};
            }

            std::size_t cursor = 2u;
            const auto skip_space_and_comments = [&]() noexcept
            {
                for (;;)
                {
                    while (cursor < source.size()
                        && std::isspace(byte_at(source, cursor)) != 0)
                    {
                        ++cursor;
                    }
                    if (cursor >= source.size()
                        || byte_at(source, cursor)
                            != static_cast<std::uint8_t>('#'))
                    {
                        return;
                    }
                    while (cursor < source.size()
                        && byte_at(source, cursor)
                            != static_cast<std::uint8_t>('\n'))
                    {
                        ++cursor;
                    }
                }
            };
            const auto read_integer =
                [&](std::uint32_t& output) noexcept -> bool
            {
                skip_space_and_comments();
                if (cursor >= source.size()
                    || !std::isdigit(byte_at(source, cursor)))
                {
                    return false;
                }
                std::uint64_t value{};
                while (cursor < source.size()
                    && std::isdigit(byte_at(source, cursor)))
                {
                    const std::uint64_t digit =
                        byte_at(source, cursor)
                        - static_cast<std::uint8_t>('0');
                    if (value
                        > ((std::numeric_limits<std::uint32_t>::max)()
                            - digit) / 10u)
                    {
                        return false;
                    }
                    value = value * 10u + digit;
                    ++cursor;
                }
                output = static_cast<std::uint32_t>(value);
                return true;
            };

            std::uint32_t width{};
            std::uint32_t height{};
            std::uint32_t maximum{};
            if (!read_integer(width)
                || !read_integer(height)
                || !read_integer(maximum)
                || maximum != 255u
                || cursor >= source.size()
                || std::isspace(byte_at(source, cursor)) == 0)
            {
                return {};
            }

            if (byte_at(source, cursor)
                    == static_cast<std::uint8_t>('\r')
                && cursor + 1u < source.size()
                && byte_at(source, cursor + 1u)
                    == static_cast<std::uint8_t>('\n'))
            {
                cursor += 2u;
            }
            else
            {
                ++cursor;
            }

            std::uint64_t decodedBytes{};
            if (!checked_decoded_size(
                    width,
                    height,
                    limits,
                    decodedBytes))
            {
                return {
                    .status = TextureImportStatus::dimensions_exceeded,
                    .width = width,
                    .height = height
                };
            }
            std::uint64_t rgbBytes{};
            if (!checked_multiply(width, height, rgbBytes)
                || !checked_multiply(rgbBytes, 3u, rgbBytes)
                || cursor > source.size()
                || rgbBytes > source.size() - cursor)
            {
                return {};
            }

            DecodedImage result{
                .status = TextureImportStatus::ready,
                .width = width,
                .height = height,
                .rgba = std::vector<std::byte>(
                    static_cast<std::size_t>(decodedBytes))
            };
            const std::size_t pixelCount =
                static_cast<std::size_t>(width)
                * static_cast<std::size_t>(height);
            for (std::size_t pixel = 0u; pixel < pixelCount; ++pixel)
            {
                const std::size_t sourceOffset = cursor + pixel * 3u;
                const std::size_t destinationOffset = pixel * 4u;
                result.rgba[destinationOffset + 0u] =
                    source[sourceOffset + 0u];
                result.rgba[destinationOffset + 1u] =
                    source[sourceOffset + 1u];
                result.rgba[destinationOffset + 2u] =
                    source[sourceOffset + 2u];
                result.rgba[destinationOffset + 3u] =
                    static_cast<std::byte>(0xffu);
            }
            return result;
        }

        [[nodiscard]] DecodedImage decode_bmp(
            std::span<const std::byte> source,
            const TextureImportLimits& limits)
        {
            if (source.size() < 54u
                || byte_at(source, 0u) != static_cast<std::uint8_t>('B')
                || byte_at(source, 1u) != static_cast<std::uint8_t>('M'))
            {
                return {};
            }

            std::uint32_t dataOffset{};
            std::uint32_t dibSize{};
            std::int32_t signedWidth{};
            std::int32_t signedHeight{};
            std::uint16_t planes{};
            std::uint16_t bitsPerPixel{};
            std::uint32_t compression{};
            if (!read_u32(source, 10u, dataOffset)
                || !read_u32(source, 14u, dibSize)
                || dibSize < 40u
                || !read_i32(source, 18u, signedWidth)
                || !read_i32(source, 22u, signedHeight)
                || !read_u16(source, 26u, planes)
                || !read_u16(source, 28u, bitsPerPixel)
                || !read_u32(source, 30u, compression)
                || signedWidth <= 0
                || signedHeight == 0
                || signedHeight == (std::numeric_limits<std::int32_t>::min)()
                || planes != 1u
                || (bitsPerPixel != 24u && bitsPerPixel != 32u)
                || compression != 0u)
            {
                return {};
            }

            const std::uint32_t width =
                static_cast<std::uint32_t>(signedWidth);
            const std::uint32_t height = static_cast<std::uint32_t>(
                signedHeight < 0 ? -signedHeight : signedHeight);
            std::uint64_t decodedBytes{};
            if (!checked_decoded_size(
                    width,
                    height,
                    limits,
                    decodedBytes))
            {
                return {
                    .status = TextureImportStatus::dimensions_exceeded,
                    .width = width,
                    .height = height
                };
            }

            const std::uint64_t rowBits =
                static_cast<std::uint64_t>(width) * bitsPerPixel;
            const std::uint64_t rowStride = ((rowBits + 31u) / 32u) * 4u;
            std::uint64_t payloadBytes{};
            if (!checked_multiply(rowStride, height, payloadBytes)
                || dataOffset > source.size()
                || payloadBytes > source.size() - dataOffset)
            {
                return {};
            }

            DecodedImage result{
                .status = TextureImportStatus::ready,
                .width = width,
                .height = height,
                .rgba = std::vector<std::byte>(
                    static_cast<std::size_t>(decodedBytes))
            };
            const std::uint32_t channels = bitsPerPixel / 8u;
            const bool sourceTopDown = signedHeight < 0;
            for (std::uint32_t y = 0u; y < height; ++y)
            {
                const std::uint32_t sourceY =
                    sourceTopDown ? y : height - 1u - y;
                const std::size_t sourceRow =
                    static_cast<std::size_t>(dataOffset)
                    + static_cast<std::size_t>(sourceY * rowStride);
                for (std::uint32_t x = 0u; x < width; ++x)
                {
                    const std::size_t sourceOffset =
                        sourceRow + static_cast<std::size_t>(x * channels);
                    const std::size_t destinationOffset =
                        (static_cast<std::size_t>(y) * width + x) * 4u;
                    result.rgba[destinationOffset + 0u] =
                        source[sourceOffset + 2u];
                    result.rgba[destinationOffset + 1u] =
                        source[sourceOffset + 1u];
                    result.rgba[destinationOffset + 2u] =
                        source[sourceOffset + 0u];
                    result.rgba[destinationOffset + 3u] =
                        channels == 4u
                            ? source[sourceOffset + 3u]
                            : static_cast<std::byte>(0xffu);
                }
            }
            return result;
        }

        [[nodiscard]] DecodedImage decode_tga(
            std::span<const std::byte> source,
            const TextureImportLimits& limits)
        {
            if (source.size() < 18u
                || byte_at(source, 1u) != 0u
                || byte_at(source, 2u) != 2u)
            {
                return {};
            }

            std::uint16_t width16{};
            std::uint16_t height16{};
            if (!read_u16(source, 12u, width16)
                || !read_u16(source, 14u, height16))
            {
                return {};
            }
            const std::uint8_t bitsPerPixel = byte_at(source, 16u);
            if ((bitsPerPixel != 24u && bitsPerPixel != 32u)
                || width16 == 0u
                || height16 == 0u)
            {
                return {};
            }

            const std::uint32_t width = width16;
            const std::uint32_t height = height16;
            std::uint64_t decodedBytes{};
            if (!checked_decoded_size(
                    width,
                    height,
                    limits,
                    decodedBytes))
            {
                return {
                    .status = TextureImportStatus::dimensions_exceeded,
                    .width = width,
                    .height = height
                };
            }

            const std::uint32_t channels = bitsPerPixel / 8u;
            const std::size_t dataOffset =
                18u + static_cast<std::size_t>(byte_at(source, 0u));
            std::uint64_t payloadBytes{};
            if (!checked_multiply(width, height, payloadBytes)
                || !checked_multiply(payloadBytes, channels, payloadBytes)
                || dataOffset > source.size()
                || payloadBytes > source.size() - dataOffset)
            {
                return {};
            }

            DecodedImage result{
                .status = TextureImportStatus::ready,
                .width = width,
                .height = height,
                .rgba = std::vector<std::byte>(
                    static_cast<std::size_t>(decodedBytes))
            };
            const bool sourceTopDown = (byte_at(source, 17u) & 0x20u) != 0u;
            for (std::uint32_t y = 0u; y < height; ++y)
            {
                const std::uint32_t sourceY =
                    sourceTopDown ? y : height - 1u - y;
                for (std::uint32_t x = 0u; x < width; ++x)
                {
                    const std::size_t sourceOffset =
                        dataOffset
                        + (static_cast<std::size_t>(sourceY) * width + x)
                            * channels;
                    const std::size_t destinationOffset =
                        (static_cast<std::size_t>(y) * width + x) * 4u;
                    result.rgba[destinationOffset + 0u] =
                        source[sourceOffset + 2u];
                    result.rgba[destinationOffset + 1u] =
                        source[sourceOffset + 1u];
                    result.rgba[destinationOffset + 2u] =
                        source[sourceOffset + 0u];
                    result.rgba[destinationOffset + 3u] =
                        channels == 4u
                            ? source[sourceOffset + 3u]
                            : static_cast<std::byte>(0xffu);
                }
            }
            return result;
        }

        [[nodiscard]] DecodedImage decode(
            std::string_view extension,
            std::span<const std::byte> source,
            const TextureImportLimits& limits)
        {
            if (extension == ".ppm")
                return decode_ppm(source, limits);
            if (extension == ".bmp")
                return decode_bmp(source, limits);
            if (extension == ".tga")
                return decode_tga(source, limits);
            return {
                .status = TextureImportStatus::unsupported_format
            };
        }

        struct ContractRoot final
        {
            fs::path path{};

            ContractRoot() noexcept
            {
                static std::atomic<std::uint64_t> sequence{1u};
                std::error_code error{};
                path = fs::temp_directory_path(error);
                if (error)
                {
                    path.clear();
                    return;
                }
                path /= "epoch_texture_import_contract_"
                    + std::to_string(sequence.fetch_add(1u));
                fs::create_directories(path, error);
                if (error)
                    path.clear();
            }

            ~ContractRoot()
            {
                if (path.empty())
                    return;
                std::error_code error{};
                fs::remove_all(path, error);
            }
        };
    }

    TextureImportResult import_texture_file(
        const TextureImportRequest& request) noexcept
    {
        TextureImportResult result{};
        try
        {
            if (request.source_path.empty()
                || request.source_sequence == 0u
                || !request.limits.valid()
                || request.profile.compiler_schema_version == 0u
                || request.profile.mipmaps != MipmapPolicy::preserve_authored
                || (request.profile.format != ArtifactFormat::rgba8_unorm
                    && request.profile.format != ArtifactFormat::rgba8_srgb)
                || (request.profile.format == ArtifactFormat::rgba8_unorm
                    && request.profile.color_space != ColorSpace::linear)
                || (request.profile.format == ArtifactFormat::rgba8_srgb
                    && request.profile.color_space != ColorSpace::srgb))
            {
                return result;
            }

            std::error_code error{};
            const std::uint64_t sourceBytes =
                fs::file_size(request.source_path, error);
            if (error || !fs::is_regular_file(request.source_path, error))
            {
                result.status = TextureImportStatus::source_not_found;
                return result;
            }
            result.source_bytes = sourceBytes;
            if (sourceBytes == 0u
                || sourceBytes > request.limits.maximum_source_bytes
                || sourceBytes
                    > (std::numeric_limits<std::size_t>::max)())
            {
                result.status = TextureImportStatus::source_too_large;
                return result;
            }

            std::vector<std::byte> source(
                static_cast<std::size_t>(sourceBytes));
            std::ifstream input(request.source_path, std::ios::binary);
            if (!input
                || !input.read(
                    reinterpret_cast<char*>(source.data()),
                    static_cast<std::streamsize>(source.size())))
            {
                result.status = TextureImportStatus::malformed_source;
                return result;
            }

            const std::string extension =
                lowercase_extension(request.source_path);
            DecodedImage decoded = decode(
                extension,
                source,
                request.limits);
            result.status = decoded.status;
            result.width = decoded.width;
            result.height = decoded.height;
            result.decoded_bytes = decoded.rgba.size();
            if (decoded.status != TextureImportStatus::ready)
                return result;

            const ContentHash sourceHash = deterministic_content_hash(
                "epoch.texture.import." + extension,
                source);
            CompiledTextureArtifactIdentity identity =
                build_compiled_texture_artifact_identity(
                    DocumentRevision{
                        .content = sourceHash,
                        .sequence = request.source_sequence
                    },
                    request.profile,
                    decoded.width,
                    decoded.height,
                    1u);
            if (!identity || identity.mip_count != 1u)
            {
                result.status = TextureImportStatus::artifact_failure;
                return result;
            }
            identity.compilation_required = false;

            CompiledTextureMip mip{
                .width = decoded.width,
                .height = decoded.height,
                .row_pitch_bytes = decoded.width * 4u,
                .texels = std::move(decoded.rgba)
            };
            mip.content = compiled_texture_mip_content(mip);
            result.artifact = CompiledTextureArtifact{
                .status = ArtifactCompilationStatus::ready,
                .identity = identity,
                .mips = {std::move(mip)}
            };
            result.artifact.payload_content =
                compiled_texture_payload_content(
                    result.artifact.identity,
                    result.artifact.mips);
            if (!validate_compiled_artifact(result.artifact))
            {
                result.artifact = {};
                result.status = TextureImportStatus::artifact_failure;
                return result;
            }
            result.status = TextureImportStatus::ready;
            return result;
        }
        catch (const std::bad_alloc&)
        {
            result.artifact = {};
            result.status = TextureImportStatus::allocation_failure;
            return result;
        }
        catch (...)
        {
            result.artifact = {};
            result.status = TextureImportStatus::malformed_source;
            return result;
        }
    }

    TextureImportContractFailure
        texture_import_contract_failure() noexcept
    {
        ContractRoot root{};
        if (root.path.empty())
            return TextureImportContractFailure::temporary_source;

        const fs::path ppm = root.path / "sample.ppm";
        const std::array<unsigned char, 17> source{
            'P', '6', '\n', '2', ' ', '1', '\n',
            '2', '5', '5', '\n',
            255, 0, 0,
            0, 255, 0
        };
        {
            std::ofstream output(ppm, std::ios::binary);
            output.write(
                reinterpret_cast<const char*>(source.data()),
                static_cast<std::streamsize>(source.size()));
        }

        const TextureImportRequest request{
            .source_path = ppm,
            .source_sequence = 7u
        };
        const TextureImportResult first = import_texture_file(request);
        const TextureImportResult second = import_texture_file(request);
        if (!first
            || first.width != 2u
            || first.height != 1u
            || first.decoded_bytes != 8u
            || first.artifact.mips.size() != 1u
            || first.artifact.mips.front().texels[0]
                != static_cast<std::byte>(0xffu)
            || first.artifact.mips.front().texels[4]
                != static_cast<std::byte>(0x00u)
            || first.artifact.mips.front().texels[5]
                != static_cast<std::byte>(0xffu))
        {
            return TextureImportContractFailure::ppm_import;
        }
        if (!second
            || second.artifact.identity.key != first.artifact.identity.key
            || second.artifact.payload_content
                != first.artifact.payload_content)
        {
            return TextureImportContractFailure::deterministic_identity;
        }

        const fs::path malformed = root.path / "malformed.ppm";
        {
            std::ofstream output(malformed, std::ios::binary);
            output << "P6\n2 2\n255\nx";
        }
        if (import_texture_file({
                .source_path = malformed
            }).status != TextureImportStatus::malformed_source)
        {
            return TextureImportContractFailure::malformed_rejection;
        }

        TextureImportRequest bounded = request;
        bounded.limits.maximum_source_bytes = 4u;
        if (import_texture_file(bounded).status
            != TextureImportStatus::source_too_large)
        {
            return TextureImportContractFailure::oversized_rejection;
        }
        return TextureImportContractFailure::none;
    }
}
