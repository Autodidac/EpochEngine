/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include "engine.config.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module render.texture_artifact;

import asset.texture_artifact;
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TEXTURE_EDITOR
import authoring.texture;
#endif
import render.canvas2d;
import render.device;
import render.texture_residency;

export namespace epochengine::texture_artifact
{
    enum class TransferCode : std::uint8_t
    {
        ready,
        invalid_logical_reference,
        invalid_artifact,
        compilation_incomplete,
        revision_mismatch,
        unsupported_format,
        unsupported_color_space,
        unsupported_mip_level,
        seal_allocation_failed,
        residency_failed
    };

    [[nodiscard]] constexpr const char* transfer_code_name(
        TransferCode code) noexcept
    {
        switch (code)
        {
        case TransferCode::ready: return "ready";
        case TransferCode::invalid_logical_reference:
            return "invalid_logical_reference";
        case TransferCode::invalid_artifact: return "invalid_artifact";
        case TransferCode::compilation_incomplete:
            return "compilation_incomplete";
        case TransferCode::revision_mismatch: return "revision_mismatch";
        case TransferCode::unsupported_format: return "unsupported_format";
        case TransferCode::unsupported_color_space:
            return "unsupported_color_space";
        case TransferCode::unsupported_mip_level:
            return "unsupported_mip_level";
        case TransferCode::seal_allocation_failed:
            return "seal_allocation_failed";
        case TransferCode::residency_failed: return "residency_failed";
        }
        return "unknown";
    }

    struct ResidencySelection final
    {
        std::uint8_t mip_level{};
        texture_residency::ResidencyPolicy policy{};
        std::uint64_t frame_sequence{};
        std::string_view debug_name{"CompiledTextureArtifact"};
    };

    struct ArtifactKeyResult final
    {
        TransferCode code{TransferCode::invalid_artifact};
        texture_residency::ArtifactKey key{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == TransferCode::ready
                && static_cast<bool>(key);
        }
    };

    struct ArtifactSealResult;

    class ValidatedCompiledTextureArtifact final
    {
    public:
        ValidatedCompiledTextureArtifact() noexcept = default;

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return static_cast<bool>(artifact_);
        }

        [[nodiscard]] const asset::texture::CompiledTextureArtifact*
            artifact() const noexcept
        {
            return artifact_.get();
        }

    private:
        explicit ValidatedCompiledTextureArtifact(
            std::shared_ptr<const asset::texture::CompiledTextureArtifact>
                artifact) noexcept
            : artifact_(std::move(artifact))
        {
        }

        friend ArtifactSealResult seal_compiled_texture(
            const asset::texture::CompiledTextureArtifact& artifact) noexcept;
        friend ArtifactSealResult seal_compiled_texture(
            asset::texture::CompiledTextureArtifact&& artifact) noexcept;

        std::shared_ptr<const asset::texture::CompiledTextureArtifact>
            artifact_{};
    };

    struct ArtifactSealResult final
    {
        TransferCode code{TransferCode::invalid_artifact};
        ValidatedCompiledTextureArtifact artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == TransferCode::ready
                && static_cast<bool>(artifact);
        }
    };

    struct AcquireArtifactResult final
    {
        TransferCode code{TransferCode::invalid_artifact};
        texture_residency::AcquireResult residency{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == TransferCode::ready
                && static_cast<bool>(residency);
        }
    };

    namespace detail
    {
        [[nodiscard]] constexpr texture_residency::ArtifactDigest to_digest(
            const asset::texture::ContentHash& content) noexcept
        {
            return texture_residency::ArtifactDigest{content.words};
        }

        [[nodiscard]] constexpr bool supported_format(
            const asset::texture::CompiledTextureArtifactIdentity& identity)
            noexcept
        {
            return identity.profile.format
                    == asset::texture::ArtifactFormat::rgba8_unorm
                && identity.profile.color_space
                    == asset::texture::ColorSpace::linear;
        }
    }

    [[nodiscard]] constexpr std::uint64_t compiled_artifact_revision(
        const asset::texture::CompiledTextureArtifactIdentity& identity)
        noexcept
    {
        if (identity.key.empty())
            return 0;
        std::uint64_t revision = 14695981039346656037ull;
        for (const std::uint64_t word : identity.key.words)
        {
            for (std::uint32_t byte = 0; byte < 8; ++byte)
            {
                revision ^= static_cast<std::uint8_t>(word >> (byte * 8u));
                revision *= 1099511628211ull;
            }
        }
        return revision == 0 ? 1u : revision;
    }

    [[nodiscard]] ArtifactSealResult seal_compiled_texture(
        const asset::texture::CompiledTextureArtifact& artifact) noexcept
    {
        if (artifact.identity.compilation_required)
            return {TransferCode::compilation_incomplete, {}};
        if (!asset::texture::validate_compiled_artifact(artifact))
            return {TransferCode::invalid_artifact, {}};
        try
        {
            auto shared = std::make_shared<
                const asset::texture::CompiledTextureArtifact>(artifact);
            return {
                TransferCode::ready,
                ValidatedCompiledTextureArtifact{std::move(shared)}};
        }
        catch (...)
        {
            return {TransferCode::seal_allocation_failed, {}};
        }
    }

    [[nodiscard]] ArtifactSealResult seal_compiled_texture(
        asset::texture::CompiledTextureArtifact&& artifact) noexcept
    {
        if (artifact.identity.compilation_required)
            return {TransferCode::compilation_incomplete, {}};
        if (!asset::texture::validate_compiled_artifact(artifact))
            return {TransferCode::invalid_artifact, {}};
        try
        {
            auto shared = std::make_shared<
                const asset::texture::CompiledTextureArtifact>(
                    std::move(artifact));
            return {
                TransferCode::ready,
                ValidatedCompiledTextureArtifact{std::move(shared)}};
        }
        catch (...)
        {
            return {TransferCode::seal_allocation_failed, {}};
        }
    }

    [[nodiscard]] ArtifactKeyResult make_artifact_key(
        const ValidatedCompiledTextureArtifact& validated,
        canvas2d::LogicalTextureReference logical,
        std::uint8_t mip_level = 0) noexcept
    {
        using namespace asset::texture;

        if (!logical)
            return {TransferCode::invalid_logical_reference, {}};
        if (!validated)
            return {TransferCode::invalid_artifact, {}};
        const CompiledTextureArtifact* const artifactPtr = validated.artifact();
        if (!artifactPtr)
            return {TransferCode::invalid_artifact, {}};
        const CompiledTextureArtifact& artifact = *artifactPtr;
        if (logical.artifact_revision
            != compiled_artifact_revision(artifact.identity))
        {
            return {TransferCode::revision_mismatch, {}};
        }
        if (artifact.identity.profile.format != ArtifactFormat::rgba8_unorm)
            return {TransferCode::unsupported_format, {}};
        if (artifact.identity.profile.color_space != ColorSpace::linear)
            return {TransferCode::unsupported_color_space, {}};
        if (!detail::supported_format(artifact.identity))
            return {TransferCode::unsupported_format, {}};
        if (mip_level != 0)
            return {TransferCode::unsupported_mip_level, {}};
        if (mip_level >= artifact.mips.size())
            return {TransferCode::unsupported_mip_level, {}};

        const CompiledTextureMip& mip = artifact.mips[mip_level];

        texture_residency::ArtifactKey key{};
        key.logical = logical;
        key.digest = detail::to_digest(mip.content);
        key.compiler_schema_version =
            artifact.identity.profile.compiler_schema_version;
        key.width = mip.width;
        key.height = mip.height;
        key.mip_levels = 1;
        key.format = TextureFormat::rgba8_unorm;
        return {TransferCode::ready, key};
    }

    [[nodiscard]] AcquireArtifactResult acquire_compiled_texture(
        texture_residency::TextureResidencyCache& cache,
        const ValidatedCompiledTextureArtifact& validated,
        canvas2d::LogicalTextureReference logical,
        ResidencySelection selection = {})
    {
        const ArtifactKeyResult key = make_artifact_key(
            validated,
            logical,
            selection.mip_level);
        if (!key)
            return {key.code, {}};

        const asset::texture::CompiledTextureArtifact* const artifactPtr =
            validated.artifact();
        if (!artifactPtr)
            return {TransferCode::invalid_artifact, {}};
        const asset::texture::CompiledTextureArtifact& artifact = *artifactPtr;
        const asset::texture::CompiledTextureMip& mip = artifact.mips[0];
        texture_residency::ResidencyRequest request{};
        request.artifact = key.key;
        request.payload = texture_residency::TexturePayloadView{
            mip.texels.data(),
            mip.texels.size(),
            mip.row_pitch_bytes};
        request.policy = selection.policy;
        request.frame_sequence = selection.frame_sequence;
        request.debug_name = selection.debug_name;
        texture_residency::AcquireResult result = cache.acquire(request);
        return {
            result ? TransferCode::ready : TransferCode::residency_failed,
            result};
    }

#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TEXTURE_EDITOR
    enum class TransferContractFailure : std::uint8_t
    {
        none,
        artifact_compile,
        request_mapping,
        first_acquire,
        payload_lifetime,
        exact_reuse,
        backend_recreation,
        backend_epoch_reset,
        stale_handle,
        mutation_rejection,
        revision_rejection,
        format_rejection,
        metrics
    };

    [[nodiscard]] constexpr const char* transfer_contract_failure_name(
        TransferContractFailure failure) noexcept
    {
        switch (failure)
        {
        case TransferContractFailure::none: return "pass";
        case TransferContractFailure::artifact_compile: return "artifact_compile";
        case TransferContractFailure::request_mapping: return "request_mapping";
        case TransferContractFailure::first_acquire: return "first_acquire";
        case TransferContractFailure::payload_lifetime: return "payload_lifetime";
        case TransferContractFailure::exact_reuse: return "exact_reuse";
        case TransferContractFailure::backend_recreation:
            return "backend_recreation";
        case TransferContractFailure::backend_epoch_reset:
            return "backend_epoch_reset";
        case TransferContractFailure::stale_handle: return "stale_handle";
        case TransferContractFailure::mutation_rejection:
            return "mutation_rejection";
        case TransferContractFailure::revision_rejection:
            return "revision_rejection";
        case TransferContractFailure::format_rejection: return "format_rejection";
        case TransferContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    namespace detail
    {
        class TransferContractCommandContext final : public ICommandContext
        {
        public:
            void begin(const char*) override {}
            void end() override {}
            void debug_marker(const char*) override {}
            void barrier() override {}
        };

        class TransferContractDevice final : public IRenderDevice
        {
        public:
            struct TextureRecord final
            {
                TextureDesc desc{};
                std::vector<std::byte> uploaded{};
                bool active{};
                bool ready{};
            };

            std::string backend_name() const override
            {
                return "texture-artifact-contract";
            }

            BufferHandle create_buffer(const BufferDesc&) override
            {
                return BufferHandle{++buffer_};
            }

            SamplerHandle create_sampler(const SamplerDesc&) override
            {
                return SamplerHandle{++sampler_};
            }

            ShaderHandle create_shader(const ShaderDesc&) override
            {
                return ShaderHandle{++shader_};
            }

            PipelineHandle create_pipeline(const PipelineDesc&) override
            {
                return PipelineHandle{++pipeline_};
            }

            MaterialHandle create_material(const MaterialDesc&) override
            {
                return MaterialHandle{++material_};
            }

            RenderTargetHandle create_render_target(
                const RenderTargetDesc&) override
            {
                return RenderTargetHandle{++target_};
            }

            BindingSetHandle create_binding_set(
                const CommandResourceBindings&) override
            {
                return BindingSetHandle{++binding_};
            }

            MeshHandle create_mesh(const MeshDesc&) override
            {
                return MeshHandle{++mesh_};
            }

            ModelHandle create_model(const ModelDesc&) override
            {
                return ModelHandle{++model_};
            }

            TextureHandle create_texture(const TextureDesc& desc) override
            {
                for (std::uint32_t index = 0; index < textures_.size(); ++index)
                {
                    if (!textures_[index].active)
                    {
                        textures_[index] = TextureRecord{
                            desc,
                            {},
                            true,
                            false};
                        ++create_count;
                        return TextureHandle{index + 1u};
                    }
                }
                textures_.push_back(TextureRecord{
                    desc,
                    {},
                    true,
                    false});
                ++create_count;
                return TextureHandle{
                    static_cast<std::uint32_t>(textures_.size())};
            }

            bool upload_texture(
                TextureHandle handle,
                const TextureUploadDesc& upload) override
            {
                TextureRecord* const record = resolve(handle);
                if (!record
                    || !epochengine::valid(upload)
                    || upload.mip_level != 0
                    || upload.x != 0
                    || upload.y != 0
                    || upload.width != record->desc.width
                    || upload.height != record->desc.height
                    || upload.format != record->desc.format)
                {
                    return false;
                }
                const auto* const begin =
                    static_cast<const std::byte*>(upload.data);
                record->uploaded.assign(
                    begin,
                    begin + static_cast<std::size_t>(upload.size_bytes));
                record->ready = true;
                ++upload_count;
                return true;
            }

            bool texture_ready(TextureHandle handle) const noexcept override
            {
                const TextureRecord* const record = resolve(handle);
                return record && record->ready;
            }

            void destroy(BufferHandle) noexcept override {}
            void destroy(SamplerHandle) noexcept override {}
            void destroy(ShaderHandle) noexcept override {}
            void destroy(PipelineHandle) noexcept override {}
            void destroy(MaterialHandle) noexcept override {}
            void destroy(RenderTargetHandle) noexcept override {}
            void destroy(BindingSetHandle) noexcept override {}
            void destroy(MeshHandle) noexcept override {}
            void destroy(ModelHandle) noexcept override {}

            void destroy(TextureHandle handle) noexcept override
            {
                TextureRecord* const record = resolve(handle);
                if (!record)
                    return;
                *record = {};
                ++destroy_count;
            }

            ICommandContext& acquire_graphics_context() override
            {
                return context_;
            }

            void present(ISwapchain&) override {}

            [[nodiscard]] const std::vector<std::byte>* uploaded(
                TextureHandle handle) const noexcept
            {
                const TextureRecord* const record = resolve(handle);
                return record ? &record->uploaded : nullptr;
            }

            [[nodiscard]] bool mark_not_ready(TextureHandle handle) noexcept
            {
                TextureRecord* const record = resolve(handle);
                if (!record)
                    return false;
                record->ready = false;
                return true;
            }

            std::uint64_t create_count{};
            std::uint64_t upload_count{};
            std::uint64_t destroy_count{};

        private:
            [[nodiscard]] TextureRecord* resolve(TextureHandle handle) noexcept
            {
                if (!handle || handle.value > textures_.size())
                    return nullptr;
                TextureRecord& record = textures_[handle.value - 1u];
                return record.active ? &record : nullptr;
            }

            [[nodiscard]] const TextureRecord* resolve(
                TextureHandle handle) const noexcept
            {
                if (!handle || handle.value > textures_.size())
                    return nullptr;
                const TextureRecord& record = textures_[handle.value - 1u];
                return record.active ? &record : nullptr;
            }

            TransferContractCommandContext context_{};
            std::vector<TextureRecord> textures_{};
            std::uint32_t buffer_{};
            std::uint32_t sampler_{};
            std::uint32_t shader_{};
            std::uint32_t pipeline_{};
            std::uint32_t material_{};
            std::uint32_t target_{};
            std::uint32_t binding_{};
            std::uint32_t mesh_{};
            std::uint32_t model_{};
        };

        [[nodiscard]] authoring::texture::TextureDocument contract_document(
            authoring::texture::PixelFormat format,
            asset::texture::ColorSpace color_space,
            authoring::texture::DocumentHandle handle)
        {
            using namespace authoring::texture;
            CanvasDescriptor canvas{};
            canvas.width = 8;
            canvas.height = 8;
            canvas.tile_extent = 4;
            canvas.mip_count = 1;
            canvas.format = format;
            canvas.color_space = color_space;
            return TextureDocument{
                handle,
                BranchIdentity{19, 1},
                canvas};
        }
    }

    [[nodiscard]] TransferContractFailure
        texture_artifact_transfer_runtime_contract_failure()
    {
        using namespace authoring::texture;
        using namespace texture_residency;

        TextureDocument document = detail::contract_document(
            PixelFormat::rgba8_unorm,
            ColorSpace::linear,
            DocumentHandle{81, 1});
        const BranchIdentity branch{19, 1};
        const MutationResult layer = document.create_layer(
            LayerDescriptor{.name = "Transfer"},
            0,
            TemporalPoint{branch, 1});
        if (!layer)
            return TransferContractFailure::artifact_compile;

        StrokeDescriptor stroke{};
        stroke.target = layer.layer;
        stroke.color = PixelRgba8{12, 96, 224, 255};
        stroke.radius_subpixels = 256;
        stroke.seed = 9;
        stroke.samples.push_back(StrokeSample{
            .x_subpixels = 3 * 256 + 128,
            .y_subpixels = 3 * 256 + 128});
        if (!document.apply_stroke(stroke, TemporalPoint{branch, 2}))
            return TransferContractFailure::artifact_compile;

        TextureCompileProfile profile{};
        profile.format = ArtifactFormat::rgba8_unorm;
        profile.color_space = ColorSpace::linear;
        profile.mipmaps = MipmapPolicy::preserve_authored;
        profile.compiler_schema_version = 5;
        CompiledTextureArtifact artifact = document.compile_artifact(profile);
        if (!artifact || !validate_compiled_artifact(artifact))
            return TransferContractFailure::artifact_compile;
        const ArtifactSealResult sealed = seal_compiled_texture(artifact);
        if (!sealed)
            return TransferContractFailure::artifact_compile;

        const canvas2d::LogicalTextureReference logical{
            0x45504f4348544558ull,
            compiled_artifact_revision(artifact.identity)};
        ResidencySelection selection{};
        selection.frame_sequence = 1;
        selection.debug_name = "contract.authored.texture";
        const ArtifactKeyResult mappedKey = make_artifact_key(
            sealed.artifact,
            logical);
        if (!mappedKey
            || mappedKey.key.logical != logical
            || mappedKey.key.digest.words != artifact.mips[0].content.words
            || mappedKey.key.compiler_schema_version
                != profile.compiler_schema_version
            || mappedKey.key.width != 8
            || mappedKey.key.height != 8
            || mappedKey.key.mip_levels != 1)
        {
            return TransferContractFailure::request_mapping;
        }

        detail::TransferContractDevice device{};
        ResidencyLimits limits{};
        limits.maximum_entries = 4;
        limits.maximum_resident_bytes = 4'096;
        limits.maximum_single_texture_bytes = 1'024;
        limits.maximum_upload_bytes_per_frame = 1'024;
        limits.maximum_evictions_per_acquire = 4;
        TextureResidencyCache cache{device, 1, limits};
        const AcquireArtifactResult first = acquire_compiled_texture(
            cache,
            sealed.artifact,
            logical,
            selection);
        if (!first
            || first.residency.code != ResidencyCode::resident
            || device.create_count != 1
            || device.upload_count != 1)
        {
            return TransferContractFailure::first_acquire;
        }

        const std::vector<std::byte> expected = artifact.mips[0].texels;
        const std::vector<std::byte>* uploaded =
            device.uploaded(first.residency.physical);
        if (!uploaded || *uploaded != expected)
            return TransferContractFailure::payload_lifetime;
        artifact.mips[0].texels[0] ^= std::byte{0xff};
        uploaded = device.uploaded(first.residency.physical);
        if (!uploaded || *uploaded != expected
            || artifact.mips[0].texels == expected)
            return TransferContractFailure::payload_lifetime;

        const AcquireArtifactResult reused = acquire_compiled_texture(
            cache,
            sealed.artifact,
            logical,
            selection);
        if (!reused
            || reused.residency.code != ResidencyCode::reused
            || reused.residency.handle != first.residency.handle
            || device.create_count != 1
            || device.upload_count != 1)
        {
            return TransferContractFailure::exact_reuse;
        }

        if (!device.mark_not_ready(first.residency.physical))
            return TransferContractFailure::backend_recreation;
        selection.frame_sequence = 2;
        const AcquireArtifactResult recreated = acquire_compiled_texture(
            cache,
            sealed.artifact,
            logical,
            selection);
        if (!recreated
            || recreated.residency.code != ResidencyCode::recreated
            || recreated.residency.handle == first.residency.handle
            || cache.resolve(first.residency.handle)
            || device.create_count != 2
            || device.upload_count != 2
            || device.uploaded(recreated.residency.physical) == nullptr
            || *device.uploaded(recreated.residency.physical) != expected)
        {
            return TransferContractFailure::backend_recreation;
        }

        const ResidencyHandle beforeReset = recreated.residency.handle;
        if (cache.reset_backend_epoch(0)
            || cache.reset_backend_epoch(1)
            || !cache.resolve(beforeReset)
            || device.destroy_count != 1)
        {
            return TransferContractFailure::backend_epoch_reset;
        }
        if (!cache.reset_backend_epoch(3)
            || cache.resolve(beforeReset)
            || cache.backend_epoch() != 3)
        {
            return TransferContractFailure::backend_epoch_reset;
        }
        selection.frame_sequence = 3;
        const AcquireArtifactResult afterReset = acquire_compiled_texture(
            cache,
            sealed.artifact,
            logical,
            selection);
        const std::uint64_t releasesBeforeStale =
            cache.metrics().explicit_releases;
        const std::uint64_t destroysBeforeStale = device.destroy_count;
        if (!afterReset
            || afterReset.residency.code != ResidencyCode::resident
            || cache.resolve(beforeReset)
            || cache.touch(beforeReset, 3) != ResidencyCode::stale_handle
            || cache.set_pinned(beforeReset, true) != ResidencyCode::stale_handle
            || cache.release(beforeReset) != ResidencyCode::stale_handle
            || cache.metrics().explicit_releases != releasesBeforeStale
            || device.destroy_count != destroysBeforeStale)
        {
            return TransferContractFailure::stale_handle;
        }

        CompiledTextureArtifact corrupted = artifact;
        corrupted.mips[0].texels[0] ^= std::byte{0x1};
        const std::uint64_t requestsBeforeMutation =
            cache.metrics().acquire_requests;
        const ArtifactSealResult rejectedMutation =
            seal_compiled_texture(std::move(corrupted));
        if (rejectedMutation.code != TransferCode::invalid_artifact
            || cache.metrics().acquire_requests != requestsBeforeMutation)
        {
            return TransferContractFailure::mutation_rejection;
        }

        const auto wrongRevision = canvas2d::LogicalTextureReference{
            logical.asset_key,
            logical.artifact_revision + 1u};
        if (make_artifact_key(sealed.artifact, wrongRevision).code
            != TransferCode::revision_mismatch)
        {
            return TransferContractFailure::revision_rejection;
        }
        if (make_artifact_key(sealed.artifact, logical, 1).code
            != TransferCode::unsupported_mip_level)
        {
            return TransferContractFailure::revision_rejection;
        }
        if (make_artifact_key(sealed.artifact, {}).code
            != TransferCode::invalid_logical_reference)
        {
            return TransferContractFailure::revision_rejection;
        }
        CompiledTextureArtifact incomplete = artifact;
        incomplete.identity.compilation_required = true;
        if (seal_compiled_texture(std::move(incomplete)).code
            != TransferCode::compilation_incomplete)
        {
            return TransferContractFailure::revision_rejection;
        }

        TextureDocument srgbDocument = detail::contract_document(
            PixelFormat::rgba8_srgb,
            ColorSpace::srgb,
            DocumentHandle{82, 1});
        TextureCompileProfile srgbProfile{};
        srgbProfile.format = ArtifactFormat::rgba8_srgb;
        srgbProfile.color_space = ColorSpace::srgb;
        const CompiledTextureArtifact srgbArtifact =
            srgbDocument.compile_artifact(srgbProfile);
        const canvas2d::LogicalTextureReference srgbLogical{
            logical.asset_key + 1u,
            compiled_artifact_revision(srgbArtifact.identity)};
        const ArtifactSealResult sealedSrgb =
            seal_compiled_texture(srgbArtifact);
        if (!srgbArtifact
            || !sealedSrgb
            || make_artifact_key(sealedSrgb.artifact, srgbLogical).code
                != TransferCode::unsupported_format)
        {
            return TransferContractFailure::format_rejection;
        }

        const ResidencyMetrics& metrics = cache.metrics();
        if (metrics.allocations != 3
            || metrics.recreations != 1
            || metrics.cache_hits != 1
            || metrics.uploads != 3
            || metrics.backend_retirements != 1
            || metrics.active_entries != 1
            || device.create_count != 3
            || device.upload_count != 3
            || device.destroy_count != 2)
        {
            return TransferContractFailure::metrics;
        }

        detail::TransferContractDevice refusedDevice{};
        ResidencyLimits refusedLimits = limits;
        refusedLimits.maximum_resident_bytes = 128;
        refusedLimits.maximum_single_texture_bytes = 128;
        TextureResidencyCache refusedCache{
            refusedDevice,
            1,
            refusedLimits};
        const AcquireArtifactResult refused = acquire_compiled_texture(
            refusedCache,
            sealed.artifact,
            logical,
            ResidencySelection{
                .frame_sequence = 1,
                .debug_name = "contract.refused"});
        if (refused.code != TransferCode::residency_failed
            || refused.residency.code
                != ResidencyCode::resident_budget_exceeded
            || refusedDevice.create_count != 0)
        {
            return TransferContractFailure::metrics;
        }

        detail::TransferContractDevice destructorDevice{};
        {
            TextureResidencyCache destructorCache{
                destructorDevice,
                1,
                limits};
            const AcquireArtifactResult destructorResident =
                acquire_compiled_texture(
                    destructorCache,
                    sealed.artifact,
                    logical,
                    ResidencySelection{
                        .frame_sequence = 1,
                        .debug_name = "contract.destructor"});
            if (!destructorResident || destructorDevice.destroy_count != 0)
                return TransferContractFailure::metrics;
        }
        if (destructorDevice.create_count != 1
            || destructorDevice.upload_count != 1
            || destructorDevice.destroy_count != 1)
        {
            return TransferContractFailure::metrics;
        }

        return TransferContractFailure::none;
    }

    [[nodiscard]] bool texture_artifact_transfer_runtime_contract()
    {
        return texture_artifact_transfer_runtime_contract_failure()
            == TransferContractFailure::none;
    }
#endif
}
