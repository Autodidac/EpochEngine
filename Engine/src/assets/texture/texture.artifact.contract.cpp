/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

import authoring.texture.artifact;

namespace epochengine::authoring::texture::contract
{
    [[nodiscard]] CompiledTextureArtifact make_runtime_artifact() noexcept
    {
        TextureCompileProfile profile{};
        profile.format = ArtifactFormat::rgba8_unorm;
        profile.color_space = ColorSpace::linear;
        profile.mipmaps = MipmapPolicy::preserve_authored;
        profile.compiler_schema_version = 7;
        profile.quality_tier = 2;

        CompiledTextureArtifact artifact{};
        artifact.identity = build_compiled_texture_artifact_identity(
            DocumentRevision{
                .content = ContentHash{{
                    0x0123456789abcdefull,
                    0xfedcba9876543210ull,
                    0x1122334455667788ull,
                    0x8877665544332211ull}},
                .sequence = 23},
            profile,
            2,
            2,
            1);
        if (!artifact.identity)
            return artifact;

        CompiledTextureMip mip{};
        mip.width = 2;
        mip.height = 2;
        mip.row_pitch_bytes = 8;
        mip.texels = {
            std::byte{0x11}, std::byte{0x22}, std::byte{0x33}, std::byte{0xff},
            std::byte{0x44}, std::byte{0x55}, std::byte{0x66}, std::byte{0xee},
            std::byte{0x77}, std::byte{0x88}, std::byte{0x99}, std::byte{0xdd},
            std::byte{0xaa}, std::byte{0xbb}, std::byte{0xcc}, std::byte{0xcc}};
        mip.content = compiled_texture_mip_content(mip);
        artifact.mips.push_back(std::move(mip));
        artifact.identity.compilation_required = false;
        artifact.payload_content = compiled_texture_payload_content(
            artifact.identity,
            std::span<const CompiledTextureMip>{artifact.mips});
        artifact.status = ArtifactCompilationStatus::ready;
        return artifact;
    }

    [[nodiscard]] int run_runtime_artifact_contract()
    {
        const CompiledTextureArtifact valid = make_runtime_artifact();
        if (!valid || !validate_compiled_artifact(valid))
            return 1;

        const CompiledTextureArtifact repeated = make_runtime_artifact();
        if (!repeated
            || repeated.identity.key != valid.identity.key
            || repeated.mips.front().content != valid.mips.front().content
            || repeated.payload_content != valid.payload_content)
        {
            return 2;
        }

        CompiledTextureArtifact corrupted = valid;
        corrupted.identity.key.words[0] ^= 1ull;
        if (validate_compiled_artifact(corrupted))
            return 3;

        corrupted = valid;
        corrupted.mips.front().texels.front() ^= std::byte{0x01};
        if (validate_compiled_artifact(corrupted))
            return 4;

        corrupted = valid;
        corrupted.mips.front().content.words[1] ^= 1ull;
        if (validate_compiled_artifact(corrupted))
            return 5;

        corrupted = valid;
        corrupted.payload_content.words[2] ^= 1ull;
        if (validate_compiled_artifact(corrupted))
            return 6;

        corrupted = valid;
        corrupted.identity.compilation_required = true;
        if (validate_compiled_artifact(corrupted))
            return 7;

        corrupted = valid;
        corrupted.mips.front().row_pitch_bytes = 4;
        if (validate_compiled_artifact(corrupted))
            return 8;

        corrupted = valid;
        corrupted.identity.source_revision.sequence = 0;
        if (validate_compiled_artifact(corrupted))
            return 9;

        TextureCompileProfile invalidProfile{};
        invalidProfile.format = ArtifactFormat::rgba8_unorm;
        invalidProfile.color_space = ColorSpace::linear;
        invalidProfile.compiler_schema_version = 0;
        if (build_compiled_texture_artifact_identity(
                valid.identity.source_revision,
                invalidProfile,
                2,
                2,
                1))
        {
            return 10;
        }

        return 0;
    }
}

#if defined(EPOCH_TEXTURE_ARTIFACT_CONTRACT_MAIN)
int main()
{
    return epochengine::authoring::texture::contract::
        run_runtime_artifact_contract();
}
#endif
