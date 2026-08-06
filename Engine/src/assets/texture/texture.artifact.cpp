/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

module authoring.texture.artifact;

namespace epochengine::authoring::texture
{
    namespace
    {
        constexpr std::uint64_t kFnvPrime = 1'099'511'628'211ull;

        class StableHash final
        {
        public:
            StableHash() noexcept
                : words_{
                    0xcbf29ce484222325ull,
                    0x9e3779b97f4a7c15ull,
                    0x6a09e667f3bcc909ull,
                    0xbb67ae8584caa73bull}
            {
            }

            void add_byte(std::uint8_t value) noexcept
            {
                words_[0] ^= value;
                words_[0] *= kFnvPrime;
                words_[1] ^= static_cast<std::uint64_t>(value)
                    + 0x9e3779b97f4a7c15ull
                    + (words_[1] << 6)
                    + (words_[1] >> 2);
                words_[1] = std::rotl(words_[1], 17)
                    * 0xbf58476d1ce4e5b9ull;
                words_[2] += static_cast<std::uint64_t>(value)
                    + (words_[0] ^ std::rotl(words_[1], 11));
                words_[2] ^= words_[2] >> 29;
                words_[2] *= 0x94d049bb133111ebull;
                words_[3] ^= static_cast<std::uint64_t>(value)
                    + std::rotl(words_[0], 7)
                    + std::rotl(words_[2], 31);
                words_[3] *= 0x9e3779b185ebca87ull;
            }

            template <typename Value>
            void add_integral(Value value) noexcept
            {
                static_assert(
                    std::is_integral_v<Value> || std::is_enum_v<Value>);
                using Raw = typename std::conditional_t<
                    std::is_enum_v<Value>,
                    std::underlying_type<Value>,
                    std::type_identity<Value>>::type;
                if constexpr (std::is_same_v<std::remove_cv_t<Raw>, bool>)
                {
                    add_byte(value ? 1u : 0u);
                }
                else
                {
                    using Unsigned = std::make_unsigned_t<Raw>;
                    Unsigned bits = static_cast<Unsigned>(value);
                    for (std::size_t index = 0;
                         index < sizeof(Unsigned);
                         ++index)
                    {
                        add_byte(static_cast<std::uint8_t>(
                            bits & static_cast<Unsigned>(0xff)));
                        if constexpr (sizeof(Unsigned) > 1)
                        {
                            if (index + 1u < sizeof(Unsigned))
                                bits = static_cast<Unsigned>(bits >> 8u);
                        }
                    }
                }
            }

            void add_bytes(const std::byte* bytes, std::size_t size) noexcept
            {
                add_integral(size);
                for (std::size_t index = 0; index < size; ++index)
                    add_byte(std::to_integer<std::uint8_t>(bytes[index]));
            }

            void add_string(std::string_view value) noexcept
            {
                add_integral(value.size());
                for (const char character : value)
                    add_byte(static_cast<std::uint8_t>(character));
            }

            void add_hash(const ContentHash& hash) noexcept
            {
                for (const std::uint64_t word : hash.words)
                    add_integral(word);
            }

            [[nodiscard]] ContentHash finish() const noexcept
            {
                ContentHash result{words_};
                for (std::size_t index = 0;
                     index < result.words.size();
                     ++index)
                {
                    std::uint64_t value = result.words[index]
                        ^ std::rotl(
                            result.words[(index + 1) % result.words.size()],
                            static_cast<int>(13 + index * 7));
                    value ^= value >> 30;
                    value *= 0xbf58476d1ce4e5b9ull;
                    value ^= value >> 27;
                    value *= 0x94d049bb133111ebull;
                    value ^= value >> 31;
                    result.words[index] = value;
                }
                if (result.empty())
                    result.words[0] = 1;
                return result;
            }

        private:
            std::array<std::uint64_t, 4> words_{};
        };

        [[nodiscard]] constexpr bool checked_add(
            std::uint64_t left,
            std::uint64_t right,
            std::uint64_t& result) noexcept
        {
            if (left > (std::numeric_limits<std::uint64_t>::max)() - right)
                return false;
            result = left + right;
            return true;
        }

        [[nodiscard]] constexpr bool checked_multiply(
            std::uint64_t left,
            std::uint64_t right,
            std::uint64_t& result) noexcept
        {
            if (left != 0
                && right
                    > (std::numeric_limits<std::uint64_t>::max)() / left)
            {
                return false;
            }
            result = left * right;
            return true;
        }

        [[nodiscard]] constexpr std::uint32_t mip_dimension(
            std::uint32_t base,
            std::uint8_t mip) noexcept
        {
            while (mip != 0 && base > 1)
            {
                base /= 2;
                --mip;
            }
            return (std::max)(1u, base);
        }

        [[nodiscard]] std::uint8_t full_mip_count(
            std::uint32_t width,
            std::uint32_t height) noexcept
        {
            std::uint8_t result{1};
            while (width > 1 || height > 1)
            {
                width = (std::max)(1u, width / 2);
                height = (std::max)(1u, height / 2);
                ++result;
            }
            return result;
        }

        [[nodiscard]] std::uint64_t format_mip_bytes(
            ArtifactFormat format,
            std::uint32_t width,
            std::uint32_t height,
            bool& overflow) noexcept
        {
            std::uint64_t result{};
            const bool compressed = format == ArtifactFormat::bc1_rgb
                || format == ArtifactFormat::bc3_rgba
                || format == ArtifactFormat::bc5_normal
                || format == ArtifactFormat::bc7_rgba
                || format == ArtifactFormat::astc_4x4_rgba;
            if (!compressed)
            {
                std::uint64_t pixels{};
                overflow = !checked_multiply(width, height, pixels)
                    || !checked_multiply(pixels, 4, result);
                return overflow ? 0 : result;
            }

            const std::uint64_t blocksWide = (width + 3ull) / 4ull;
            const std::uint64_t blocksHigh = (height + 3ull) / 4ull;
            const std::uint64_t blockBytes =
                format == ArtifactFormat::bc1_rgb ? 8ull : 16ull;
            std::uint64_t blocks{};
            overflow = !checked_multiply(blocksWide, blocksHigh, blocks)
                || !checked_multiply(blocks, blockBytes, result);
            return overflow ? 0 : result;
        }

        [[nodiscard]] constexpr bool supported_uncompressed_profile(
            const TextureCompileProfile& profile) noexcept
        {
            return (profile.format == ArtifactFormat::rgba8_unorm
                    && profile.color_space == ColorSpace::linear)
                || (profile.format == ArtifactFormat::rgba8_srgb
                    && profile.color_space == ColorSpace::srgb);
        }

        [[nodiscard]] ContentHash compiled_mip_hash(
            const CompiledTextureMip& mip) noexcept
        {
            StableHash hash{};
            hash.add_string("epoch.texture.compiled-mip.v1");
            hash.add_integral(mip.width);
            hash.add_integral(mip.height);
            hash.add_integral(mip.row_pitch_bytes);
            hash.add_bytes(mip.texels.data(), mip.texels.size());
            return hash.finish();
        }

    }

    std::string content_hash_hex(const ContentHash& hash)
    {
        constexpr char digits[] = "0123456789abcdef";
        std::string result(64, '0');
        std::size_t cursor{};
        for (const std::uint64_t word : hash.words)
        {
            for (int shift = 60; shift >= 0; shift -= 4)
                result[cursor++] = digits[(word >> shift) & 0x0full];
        }
        return result;
    }

    CompiledTextureArtifactIdentity
        build_compiled_texture_artifact_identity(
            DocumentRevision source_revision,
            TextureCompileProfile profile,
            std::uint32_t width,
            std::uint32_t height,
            std::uint8_t authored_mip_count) noexcept
    {
        if (source_revision.content.empty()
            || source_revision.sequence == 0
            || profile.compiler_schema_version == 0
            || width == 0
            || height == 0
            || authored_mip_count == 0)
        {
            return {};
        }
        const std::uint8_t mipCount =
            profile.mipmaps == MipmapPolicy::preserve_authored
            ? authored_mip_count
            : full_mip_count(width, height);
        std::uint64_t bytes{};
        for (std::uint8_t mip = 0; mip < mipCount; ++mip)
        {
            bool overflow{};
            const std::uint64_t mipBytes = format_mip_bytes(
                profile.format,
                mip_dimension(width, mip),
                mip_dimension(height, mip),
                overflow);
            if (overflow || !checked_add(bytes, mipBytes, bytes))
                return {};
        }

        StableHash hash{};
        hash.add_string("epoch.texture.artifact.v1");
        hash.add_hash(source_revision.content);
        hash.add_integral(profile.format);
        hash.add_integral(profile.color_space);
        hash.add_integral(profile.mipmaps);
        hash.add_integral(profile.compiler_schema_version);
        hash.add_integral(profile.quality_tier);
        hash.add_integral(width);
        hash.add_integral(height);
        hash.add_integral(mipCount);
        return CompiledTextureArtifactIdentity{
            .key = hash.finish(),
            .source_revision = source_revision,
            .profile = profile,
            .width = width,
            .height = height,
            .mip_count = mipCount,
            .estimated_artifact_bytes = bytes,
            .compilation_required = true};
    }

    ContentHash compiled_texture_mip_content(
        const CompiledTextureMip& mip) noexcept
    {
        return compiled_mip_hash(mip);
    }

    ContentHash compiled_texture_payload_content(
        const CompiledTextureArtifactIdentity& identity,
        std::span<const CompiledTextureMip> mips) noexcept
    {
        StableHash hash{};
        hash.add_string("epoch.texture.compiled-artifact.v1");
        hash.add_hash(identity.key);
        hash.add_integral(mips.size());
        for (const CompiledTextureMip& mip : mips)
        {
            hash.add_integral(mip.width);
            hash.add_integral(mip.height);
            hash.add_integral(mip.row_pitch_bytes);
            hash.add_hash(mip.content);
        }
        return hash.finish();
    }

    bool validate_compiled_artifact(
        const CompiledTextureArtifact& artifact) noexcept
    {
        const CompiledTextureArtifactIdentity& identity = artifact.identity;
        if (artifact.status != ArtifactCompilationStatus::ready
            || !identity
            || identity.compilation_required
            || identity.source_revision.sequence == 0
            || identity.profile.compiler_schema_version == 0
            || !supported_uncompressed_profile(identity.profile)
            || (identity.profile.mipmaps != MipmapPolicy::preserve_authored
                && identity.profile.mipmaps
                    != MipmapPolicy::generate_box_filter)
            || (identity.profile.mipmaps == MipmapPolicy::generate_box_filter
                && identity.profile.color_space == ColorSpace::srgb)
            || artifact.payload_content.empty()
            || artifact.mips.size() != identity.mip_count
            || identity.estimated_artifact_bytes == 0)
        {
            return false;
        }

        const std::uint8_t maximumMipCount =
            full_mip_count(identity.width, identity.height);
        const std::uint8_t expectedMipCount =
            identity.profile.mipmaps == MipmapPolicy::preserve_authored
            ? identity.mip_count
            : maximumMipCount;
        if (identity.mip_count > maximumMipCount
            || expectedMipCount != identity.mip_count)
        {
            return false;
        }

        const CompiledTextureArtifactIdentity expectedIdentity =
            build_compiled_texture_artifact_identity(
                identity.source_revision,
                identity.profile,
                identity.width,
                identity.height,
                identity.mip_count);
        if (!expectedIdentity
            || expectedIdentity.key != identity.key
            || expectedIdentity.estimated_artifact_bytes
                != identity.estimated_artifact_bytes)
        {
            return false;
        }

        std::uint64_t actualBytes{};
        for (std::uint8_t mipIndex = 0;
             mipIndex < identity.mip_count;
             ++mipIndex)
        {
            const CompiledTextureMip& mip = artifact.mips[mipIndex];
            if (!mip.valid()
                || mip.width != mip_dimension(identity.width, mipIndex)
                || mip.height != mip_dimension(identity.height, mipIndex)
                || compiled_texture_mip_content(mip) != mip.content
                || !checked_add(actualBytes, mip.texels.size(), actualBytes))
            {
                return false;
            }
        }
        return actualBytes == identity.estimated_artifact_bytes
            && compiled_texture_payload_content(identity, artifact.mips)
                == artifact.payload_content;
    }

    const char* artifact_compilation_status_name(
        ArtifactCompilationStatus status) noexcept
    {
        switch (status)
        {
        case ArtifactCompilationStatus::ready: return "ready";
        case ArtifactCompilationStatus::invalid_document:
            return "invalid_document";
        case ArtifactCompilationStatus::invalid_profile:
            return "invalid_profile";
        case ArtifactCompilationStatus::unsupported_format:
            return "unsupported_format";
        case ArtifactCompilationStatus::unsupported_color_conversion:
            return "unsupported_color_conversion";
        case ArtifactCompilationStatus::unsupported_mipmap_policy:
            return "unsupported_mipmap_policy";
        case ArtifactCompilationStatus::output_budget_exceeded:
            return "output_budget_exceeded";
        case ArtifactCompilationStatus::malformed_source_state:
            return "malformed_source_state";
        case ArtifactCompilationStatus::arithmetic_overflow:
            return "arithmetic_overflow";
        case ArtifactCompilationStatus::allocation_failed:
            return "allocation_failed";
        }
        return "unknown";
    }
}
