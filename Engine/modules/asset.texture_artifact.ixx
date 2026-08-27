/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <span>
#include <string>
#include <vector>

export module asset.texture_artifact;

export namespace epochengine::asset::texture
{
    struct ContentHash final
    {
        std::array<std::uint64_t, 4> words{};

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return words == std::array<std::uint64_t, 4>{};
        }

        [[nodiscard]] friend constexpr bool operator==(
            const ContentHash& left,
            const ContentHash& right) noexcept
        {
            return left.words == right.words;
        }
    };
    [[nodiscard]] ContentHash deterministic_content_hash(
        std::string_view domain,
        std::span<const std::byte> bytes) noexcept;


    [[nodiscard]] std::string content_hash_hex(const ContentHash& hash);

    struct DocumentRevision final
    {
        ContentHash content{};
        std::uint64_t sequence{};

        [[nodiscard]] friend constexpr bool operator==(
            const DocumentRevision& left,
            const DocumentRevision& right) noexcept
        {
            return left.content.words == right.content.words
                && left.sequence == right.sequence;
        }
    };

    enum class ColorSpace : std::uint8_t
    {
        linear,
        srgb
    };

    enum class ArtifactFormat : std::uint8_t
    {
        rgba8_unorm,
        rgba8_srgb,
        bc1_rgb,
        bc3_rgba,
        bc5_normal,
        bc7_rgba,
        astc_4x4_rgba
    };

    enum class MipmapPolicy : std::uint8_t
    {
        preserve_authored,
        generate_box_filter,
        generate_normal_renormalized
    };

    struct TextureCompileProfile final
    {
        ArtifactFormat format{ArtifactFormat::rgba8_srgb};
        ColorSpace color_space{ColorSpace::srgb};
        MipmapPolicy mipmaps{MipmapPolicy::preserve_authored};
        std::uint32_t compiler_schema_version{1};
        std::uint32_t quality_tier{};

        [[nodiscard]] friend constexpr bool operator==(
            const TextureCompileProfile&,
            const TextureCompileProfile&) noexcept = default;
    };

    struct CompiledTextureArtifactIdentity final
    {
        ContentHash key{};
        DocumentRevision source_revision{};
        TextureCompileProfile profile{};
        std::uint32_t width{};
        std::uint32_t height{};
        std::uint8_t mip_count{};
        std::uint64_t estimated_artifact_bytes{};
        bool compilation_required{true};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return !key.empty()
                && !source_revision.content.empty()
                && width != 0
                && height != 0
                && mip_count != 0;
        }
    };

    enum class ArtifactCompilationStatus : std::uint8_t
    {
        ready,
        invalid_document,
        invalid_profile,
        unsupported_format,
        unsupported_color_conversion,
        unsupported_mipmap_policy,
        output_budget_exceeded,
        malformed_source_state,
        arithmetic_overflow,
        allocation_failed
    };

    struct CompiledTextureMip final
    {
        std::uint32_t width{};
        std::uint32_t height{};
        std::uint32_t row_pitch_bytes{};
        std::vector<std::byte> texels{};
        ContentHash content{};

        [[nodiscard]] bool valid() const noexcept
        {
            if (width == 0 || height == 0 || content.empty())
                return false;
            const std::uint64_t tightRowPitch =
                static_cast<std::uint64_t>(width) * 4ull;
            return tightRowPitch
                    <= (std::numeric_limits<std::uint32_t>::max)()
                && row_pitch_bytes == tightRowPitch
                && texels.size() == tightRowPitch * height;
        }
    };

    struct CompiledTextureArtifact final
    {
        ArtifactCompilationStatus status{
            ArtifactCompilationStatus::invalid_document};
        CompiledTextureArtifactIdentity identity{};
        ContentHash payload_content{};
        std::vector<CompiledTextureMip> mips{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == ArtifactCompilationStatus::ready
                && static_cast<bool>(identity)
                && !identity.compilation_required
                && !payload_content.empty()
                && mips.size() == identity.mip_count;
        }
    };

    enum class ArtifactIoStatus : std::uint8_t
    {
        ready,
        invalid_artifact,
        empty_input,
        invalid_magic,
        unsupported_schema,
        truncated,
        invalid_enum,
        budget_exceeded,
        allocation_failed
    };

    struct ArtifactReadLimits final
    {
        std::uint64_t maximum_serialized_bytes{512ull * 1024ull * 1024ull};
        std::uint64_t maximum_payload_bytes{256ull * 1024ull * 1024ull};
        std::uint32_t maximum_dimension{32'768};
        std::uint8_t maximum_mip_count{32};
    };

    struct SerializedTextureArtifact final
    {
        ArtifactIoStatus status{ArtifactIoStatus::invalid_artifact};
        std::vector<std::byte> bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == ArtifactIoStatus::ready && !bytes.empty();
        }
    };

    struct DeserializedTextureArtifact final
    {
        ArtifactIoStatus status{ArtifactIoStatus::invalid_artifact};
        CompiledTextureArtifact artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == ArtifactIoStatus::ready
                && static_cast<bool>(artifact);
        }
    };
    [[nodiscard]] CompiledTextureArtifactIdentity
        build_compiled_texture_artifact_identity(
            DocumentRevision source_revision,
            TextureCompileProfile profile,
            std::uint32_t width,
            std::uint32_t height,
            std::uint8_t authored_mip_count) noexcept;
    [[nodiscard]] ContentHash compiled_texture_mip_content(
        const CompiledTextureMip& mip) noexcept;
    [[nodiscard]] ContentHash compiled_texture_payload_content(
        const CompiledTextureArtifactIdentity& identity,
        std::span<const CompiledTextureMip> mips) noexcept;

    [[nodiscard]] bool validate_compiled_artifact(
        const CompiledTextureArtifact& artifact) noexcept;
    [[nodiscard]] SerializedTextureArtifact serialize_compiled_artifact(
        const CompiledTextureArtifact& artifact) noexcept;
    [[nodiscard]] DeserializedTextureArtifact deserialize_compiled_artifact(
        std::span<const std::byte> bytes,
        ArtifactReadLimits limits = {}) noexcept;
    [[nodiscard]] const char* artifact_io_status_name(
        ArtifactIoStatus status) noexcept;
    [[nodiscard]] const char* artifact_compilation_status_name(
        ArtifactCompilationStatus status) noexcept;
}
