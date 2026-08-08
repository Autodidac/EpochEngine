/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module project.texture_resources;

import asset.texture_artifact;
import project.asset_registry;
import render.canvas2d;
import render.canvas2d_cpu;
import render.device;
import render.texture_artifact;
import render.texture_residency;

export namespace epochengine::project_textures
{
    enum class ResourceCode : std::uint8_t
    {
        ready,
        already_published,
        invalid_registry,
        invalid_asset,
        stale_asset,
        wrong_asset_kind,
        source_revision_mismatch,
        invalid_artifact,
        artifact_revision_collision,
        unsupported_format,
        unsupported_color_space,
        unsupported_mip_level,
        publication_limit_exceeded,
        decoded_budget_exceeded,
        binding_limit_exceeded,
        duplicate_binding,
        missing_publication,
        allocation_failure,
        residency_failure
    };

    [[nodiscard]] constexpr std::string_view resource_code_name(
        ResourceCode code) noexcept
    {
        switch (code)
        {
        case ResourceCode::ready: return "ready";
        case ResourceCode::already_published: return "already_published";
        case ResourceCode::invalid_registry: return "invalid_registry";
        case ResourceCode::invalid_asset: return "invalid_asset";
        case ResourceCode::stale_asset: return "stale_asset";
        case ResourceCode::wrong_asset_kind: return "wrong_asset_kind";
        case ResourceCode::source_revision_mismatch:
            return "source_revision_mismatch";
        case ResourceCode::invalid_artifact: return "invalid_artifact";
        case ResourceCode::artifact_revision_collision:
            return "artifact_revision_collision";
        case ResourceCode::unsupported_format: return "unsupported_format";
        case ResourceCode::unsupported_color_space:
            return "unsupported_color_space";
        case ResourceCode::unsupported_mip_level:
            return "unsupported_mip_level";
        case ResourceCode::publication_limit_exceeded:
            return "publication_limit_exceeded";
        case ResourceCode::decoded_budget_exceeded:
            return "decoded_budget_exceeded";
        case ResourceCode::binding_limit_exceeded:
            return "binding_limit_exceeded";
        case ResourceCode::duplicate_binding: return "duplicate_binding";
        case ResourceCode::missing_publication: return "missing_publication";
        case ResourceCode::allocation_failure: return "allocation_failure";
        case ResourceCode::residency_failure: return "residency_failure";
        }
        return "unknown";
    }

    struct TextureResourceLimits final
    {
        std::uint32_t maximum_published_textures{4'096};
        std::uint32_t maximum_textures_per_resource_set{1'024};
        std::uint64_t maximum_decoded_bytes{512ull * 1024ull * 1024ull};
        std::uint64_t maximum_single_texture_bytes{256ull * 1024ull * 1024ull};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_published_textures != 0
                && maximum_textures_per_resource_set != 0
                && maximum_decoded_bytes != 0
                && maximum_single_texture_bytes != 0
                && maximum_single_texture_bytes <= maximum_decoded_bytes;
        }
    };

    struct TextureResourceMetrics final
    {
        std::uint64_t publication_requests{};
        std::uint64_t publications{};
        std::uint64_t replacements{};
        std::uint64_t publication_reuse{};
        std::uint64_t binding_requests{};
        std::uint64_t bound_textures{};
        std::uint64_t residency_requests{};
        std::uint64_t residency_successes{};
        std::uint64_t rejected_operations{};
        std::uint64_t active_publications{};
        std::uint64_t decoded_bytes{};
        std::uint64_t peak_decoded_bytes{};
    };

    struct PublicationResult final
    {
        ResourceCode code{ResourceCode::invalid_artifact};
        canvas2d::LogicalTextureReference logical{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return (code == ResourceCode::ready
                    || code == ResourceCode::already_published)
                && static_cast<bool>(logical);
        }
    };

    struct ResidencyResult final
    {
        ResourceCode code{ResourceCode::residency_failure};
        texture_residency::AcquireResult residency{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResourceCode::ready
                && static_cast<bool>(residency);
        }
    };

    namespace detail
    {
        [[nodiscard]] constexpr project_assets::AssetContentHash asset_hash(
            const asset::texture::ContentHash& content) noexcept
        {
            return {content.words};
        }

        [[nodiscard]] constexpr project_assets::AssetRevision asset_revision(
            const asset::texture::DocumentRevision& revision) noexcept
        {
            return {asset_hash(revision.content), revision.sequence};
        }

        [[nodiscard]] constexpr ResourceCode transfer_code(
            texture_artifact::TransferCode code) noexcept
        {
            using enum texture_artifact::TransferCode;
            switch (code)
            {
            case ready: return ResourceCode::ready;
            case invalid_logical_reference:
            case revision_mismatch: return ResourceCode::source_revision_mismatch;
            case invalid_artifact:
            case compilation_incomplete:
            case seal_allocation_failed: return ResourceCode::invalid_artifact;
            case unsupported_format: return ResourceCode::unsupported_format;
            case unsupported_color_space:
                return ResourceCode::unsupported_color_space;
            case unsupported_mip_level:
                return ResourceCode::unsupported_mip_level;
            case residency_failed: return ResourceCode::residency_failure;
            }
            return ResourceCode::invalid_artifact;
        }

        struct CpuTexturePayload final
        {
            canvas2d::LogicalTextureReference logical{};
            canvas2d::CanvasExtent extent{};
            std::vector<canvas2d::cpu::Rgba8> pixels{};
            canvas2d::SpriteColorSpace color_space{
                canvas2d::SpriteColorSpace::linear};
            canvas2d::cpu::AlphaEncoding alpha_encoding{
                canvas2d::cpu::AlphaEncoding::straight};

            [[nodiscard]] bool valid() const noexcept
            {
                return logical && !extent.empty()
                    && pixels.size() == static_cast<std::uint64_t>(extent.width)
                        * extent.height;
            }
        };
    }

    class Canvas2DResourceSet final
    {
    public:
        Canvas2DResourceSet() = default;
        Canvas2DResourceSet(Canvas2DResourceSet&&) noexcept = default;
        Canvas2DResourceSet& operator=(Canvas2DResourceSet&&) noexcept = default;
        Canvas2DResourceSet(const Canvas2DResourceSet&) = delete;
        Canvas2DResourceSet& operator=(const Canvas2DResourceSet&) = delete;

        [[nodiscard]] bool valid() const noexcept
        {
            return code_ == ResourceCode::ready
                && ownership_.size() == views_.size();
        }

        [[nodiscard]] ResourceCode code() const noexcept
        {
            return code_;
        }

        [[nodiscard]] std::size_t texture_count() const noexcept
        {
            return views_.size();
        }

        [[nodiscard]] std::uint64_t decoded_bytes() const noexcept
        {
            return decoded_bytes_;
        }

        [[nodiscard]] canvas2d::cpu::ResourceBindings bindings() const noexcept
        {
            return {views_, clips_};
        }

        [[nodiscard]] std::span<const texture_residency::ResidencyHandle>
            residency_handles() const noexcept
        {
            return residency_;
        }

    private:
        friend class TextureResourceService;

        ResourceCode code_{ResourceCode::invalid_artifact};
        std::vector<std::shared_ptr<const detail::CpuTexturePayload>> ownership_{};
        std::vector<canvas2d::cpu::TextureView> views_{};
        std::vector<canvas2d::cpu::ClipRect> clips_{};
        std::vector<texture_residency::ResidencyHandle> residency_{};
        std::uint64_t decoded_bytes_{};
    };

    class TextureResourceService final
    {
    public:
        explicit TextureResourceService(
            TextureResourceLimits limits = {}) noexcept
            : limits_(limits)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return limits_.valid();
        }

        [[nodiscard]] const TextureResourceLimits& limits() const noexcept
        {
            return limits_;
        }

        [[nodiscard]] TextureResourceMetrics metrics() const noexcept
        {
            TextureResourceMetrics result = metrics_;
            result.active_publications = entries_.size();
            result.decoded_bytes = decoded_bytes_;
            return result;
        }

        [[nodiscard]] PublicationResult publish(
            const project_assets::AssetRegistry& registry,
            project_assets::AssetHandle asset,
            const asset::texture::CompiledTextureArtifact& artifact) noexcept
        {
            ++metrics_.publication_requests;
            if (!valid() || !registry.valid())
                return reject_publication(ResourceCode::invalid_registry);
            const project_assets::AssetRecord* const record = registry.resolve(asset);
            if (!record)
                return reject_publication(asset ? ResourceCode::stale_asset
                                                : ResourceCode::invalid_asset);
            if (record->identity.kind != project_assets::AssetKind::texture)
                return reject_publication(ResourceCode::wrong_asset_kind);
            if (record->revision
                != detail::asset_revision(artifact.identity.source_revision))
            {
                return reject_publication(ResourceCode::source_revision_mismatch);
            }

            texture_artifact::ArtifactSealResult sealed =
                texture_artifact::seal_compiled_texture(artifact);
            if (!sealed)
                return reject_publication(detail::transfer_code(sealed.code));
            const std::uint64_t artifactRevision =
                texture_artifact::compiled_artifact_revision(artifact.identity);
            const canvas2d::LogicalTextureReference logical{
                record->identity.asset_key,
                artifactRevision};
            const texture_artifact::ArtifactKeyResult key =
                texture_artifact::make_artifact_key(sealed.artifact, logical);
            if (!key)
                return reject_publication(detail::transfer_code(key.code));

            const asset::texture::CompiledTextureMip& mip = artifact.mips[0];
            const std::uint64_t decodedBytes = mip.texels.size();
            if (decodedBytes > limits_.maximum_single_texture_bytes)
                return reject_publication(ResourceCode::decoded_budget_exceeded);

            const auto existing = std::find_if(
                entries_.begin(), entries_.end(),
                [asset](const Entry& candidate) { return candidate.asset == asset; });
            const std::uint64_t replacedBytes = existing == entries_.end()
                ? 0 : existing->decoded_bytes;
            if (decoded_bytes_ - replacedBytes > limits_.maximum_decoded_bytes
                || decodedBytes > limits_.maximum_decoded_bytes
                    - (decoded_bytes_ - replacedBytes))
            {
                return reject_publication(ResourceCode::decoded_budget_exceeded);
            }
            if (existing == entries_.end()
                && entries_.size() >= limits_.maximum_published_textures)
            {
                return reject_publication(ResourceCode::publication_limit_exceeded);
            }

            for (const Entry& candidate : entries_)
            {
                if (candidate.asset != asset && candidate.logical == logical
                    && candidate.artifact_key.words != artifact.identity.key.words)
                {
                    return reject_publication(
                        ResourceCode::artifact_revision_collision);
                }
            }
            if (existing != entries_.end() && existing->logical == logical)
            {
                if (existing->artifact_key.words != artifact.identity.key.words
                    || existing->payload_content.words != artifact.payload_content.words)
                {
                    return reject_publication(
                        ResourceCode::artifact_revision_collision);
                }
                ++metrics_.publication_reuse;
                return {ResourceCode::already_published, logical};
            }

            try
            {
                auto payload = std::make_shared<detail::CpuTexturePayload>();
                payload->logical = logical;
                payload->extent = {mip.width, mip.height};
                payload->pixels.resize(
                    static_cast<std::size_t>(mip.width) * mip.height);
                std::memcpy(
                    payload->pixels.data(),
                    mip.texels.data(),
                    mip.texels.size());
                if (!payload->valid())
                    return reject_publication(ResourceCode::invalid_artifact);

                Entry candidate{};
                candidate.asset = asset;
                candidate.source_revision = record->revision;
                candidate.logical = logical;
                candidate.artifact_key = artifact.identity.key;
                candidate.payload_content = artifact.payload_content;
                candidate.validated = std::move(sealed.artifact);
                candidate.cpu = std::move(payload);
                candidate.decoded_bytes = decodedBytes;

                if (existing == entries_.end())
                {
                    entries_.push_back(std::move(candidate));
                    ++metrics_.publications;
                }
                else
                {
                    *existing = std::move(candidate);
                    ++metrics_.replacements;
                }
                decoded_bytes_ = decoded_bytes_ - replacedBytes + decodedBytes;
                metrics_.peak_decoded_bytes = (std::max)(
                    metrics_.peak_decoded_bytes, decoded_bytes_);
                return {ResourceCode::ready, logical};
            }
            catch (...)
            {
                return reject_publication(ResourceCode::allocation_failure);
            }
        }

        [[nodiscard]] ResourceCode retire(
            project_assets::AssetHandle asset) noexcept
        {
            const auto found = std::find_if(
                entries_.begin(), entries_.end(),
                [asset](const Entry& candidate) { return candidate.asset == asset; });
            if (found == entries_.end())
                return ResourceCode::missing_publication;
            decoded_bytes_ -= found->decoded_bytes;
            entries_.erase(found);
            return ResourceCode::ready;
        }

        [[nodiscard]] Canvas2DResourceSet bind_canvas2d(
            const project_assets::AssetRegistry& registry,
            std::span<const canvas2d::LogicalTextureReference> logicalTextures,
            std::span<const canvas2d::cpu::ClipRect> clips = {}) noexcept
        {
            ++metrics_.binding_requests;
            Canvas2DResourceSet result{};
            if (!valid() || !registry.valid())
            {
                result.code_ = reject_code(ResourceCode::invalid_registry);
                return result;
            }
            if (logicalTextures.size() > limits_.maximum_textures_per_resource_set)
            {
                result.code_ = reject_code(ResourceCode::binding_limit_exceeded);
                return result;
            }

            try
            {
                result.ownership_.reserve(logicalTextures.size());
                result.views_.reserve(logicalTextures.size());
                result.clips_.assign(clips.begin(), clips.end());
                for (std::size_t index = 0; index < logicalTextures.size(); ++index)
                {
                    const canvas2d::LogicalTextureReference logical =
                        logicalTextures[index];
                    if (!logical)
                    {
                        result.code_ = reject_code(ResourceCode::invalid_asset);
                        return result;
                    }
                    if (std::find(
                        logicalTextures.begin(), logicalTextures.begin() + index,
                        logical) != logicalTextures.begin() + index)
                    {
                        result.code_ = reject_code(ResourceCode::duplicate_binding);
                        return result;
                    }
                    const Entry* const entry = resolve_entry(registry, logical);
                    if (!entry)
                    {
                        result.code_ = reject_code(ResourceCode::missing_publication);
                        return result;
                    }
                    result.ownership_.push_back(entry->cpu);
                    const detail::CpuTexturePayload& payload = *entry->cpu;
                    result.views_.push_back(canvas2d::cpu::TextureView{
                        payload.logical,
                        {},
                        payload.extent,
                        payload.extent.width,
                        payload.pixels,
                        payload.color_space,
                        payload.alpha_encoding});
                    result.decoded_bytes_ += entry->decoded_bytes;
                }
                result.code_ = ResourceCode::ready;
                metrics_.bound_textures += logicalTextures.size();
                return result;
            }
            catch (...)
            {
                result = {};
                result.code_ = reject_code(ResourceCode::allocation_failure);
                return result;
            }
        }

        [[nodiscard]] ResidencyResult acquire_resident(
            const project_assets::AssetRegistry& registry,
            canvas2d::LogicalTextureReference logical,
            texture_residency::TextureResidencyCache& cache,
            texture_artifact::ResidencySelection selection = {}) noexcept
        {
            ++metrics_.residency_requests;
            const Entry* const entry = resolve_entry(registry, logical);
            if (!entry)
                return reject_residency(ResourceCode::missing_publication);
            texture_artifact::AcquireArtifactResult acquired =
                texture_artifact::acquire_compiled_texture(
                    cache, entry->validated, logical, selection);
            if (!acquired)
                return reject_residency(detail::transfer_code(acquired.code));
            ++metrics_.residency_successes;
            return {ResourceCode::ready, acquired.residency};
        }

    private:
        struct Entry final
        {
            project_assets::AssetHandle asset{};
            project_assets::AssetRevision source_revision{};
            canvas2d::LogicalTextureReference logical{};
            asset::texture::ContentHash artifact_key{};
            asset::texture::ContentHash payload_content{};
            texture_artifact::ValidatedCompiledTextureArtifact validated{};
            std::shared_ptr<const detail::CpuTexturePayload> cpu{};
            std::uint64_t decoded_bytes{};
        };

        [[nodiscard]] const Entry* resolve_entry(
            const project_assets::AssetRegistry& registry,
            canvas2d::LogicalTextureReference logical) const noexcept
        {
            const project_assets::AssetRecord* const asset =
                registry.find_by_key(logical.asset_key);
            if (!asset || asset->identity.kind != project_assets::AssetKind::texture)
                return nullptr;
            const auto found = std::find_if(
                entries_.begin(), entries_.end(),
                [&](const Entry& candidate)
                {
                    return candidate.asset == asset->handle
                        && candidate.source_revision == asset->revision
                        && candidate.logical == logical;
                });
            return found == entries_.end() ? nullptr : &*found;
        }

        [[nodiscard]] PublicationResult reject_publication(
            ResourceCode code) noexcept
        {
            ++metrics_.rejected_operations;
            return {code, {}};
        }

        [[nodiscard]] ResourceCode reject_code(ResourceCode code) noexcept
        {
            ++metrics_.rejected_operations;
            return code;
        }

        [[nodiscard]] ResidencyResult reject_residency(
            ResourceCode code) noexcept
        {
            ++metrics_.rejected_operations;
            return {code, {}};
        }

        TextureResourceLimits limits_{};
        std::vector<Entry> entries_{};
        TextureResourceMetrics metrics_{};
        std::uint64_t decoded_bytes_{};
    };

    enum class TextureResourceContractFailure : std::uint8_t
    {
        none,
        registry,
        artifact,
        publication,
        content_revision,
        publication_reuse,
        canvas_binding,
        duplicate_binding,
        residency,
        cache_recreation,
        stale_registry_revision,
        retirement,
        metrics
    };

    [[nodiscard]] constexpr std::string_view
        texture_resource_contract_failure_name(
            TextureResourceContractFailure failure) noexcept
    {
        switch (failure)
        {
        case TextureResourceContractFailure::none: return "pass";
        case TextureResourceContractFailure::registry: return "registry";
        case TextureResourceContractFailure::artifact: return "artifact";
        case TextureResourceContractFailure::publication: return "publication";
        case TextureResourceContractFailure::content_revision: return "content_revision";
        case TextureResourceContractFailure::publication_reuse: return "publication_reuse";
        case TextureResourceContractFailure::canvas_binding: return "canvas_binding";
        case TextureResourceContractFailure::duplicate_binding: return "duplicate_binding";
        case TextureResourceContractFailure::residency: return "residency";
        case TextureResourceContractFailure::cache_recreation: return "cache_recreation";
        case TextureResourceContractFailure::stale_registry_revision:
            return "stale_registry_revision";
        case TextureResourceContractFailure::retirement: return "retirement";
        case TextureResourceContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    namespace detail
    {
        class ContractCommandContext final : public ICommandContext
        {
        public:
            void begin(const char*) override {}
            void end() override {}
            void debug_marker(const char*) override {}
            void barrier() override {}
        };

        class ContractDevice final : public IRenderDevice
        {
        public:
            std::string backend_name() const override
            {
                return "project-texture-contract";
            }

            BufferHandle create_buffer(const BufferDesc&) override
            {
                return BufferHandle{1};
            }

            TextureHandle create_texture(const TextureDesc&) override
            {
                ready_ = false;
                return TextureHandle{++next_texture_};
            }

            bool upload_texture(
                TextureHandle texture,
                const TextureUploadDesc& upload) override
            {
                if (!texture || !upload.data || upload.size_bytes == 0)
                    return false;
                uploaded_.assign(
                    static_cast<const std::byte*>(upload.data),
                    static_cast<const std::byte*>(upload.data) + upload.size_bytes);
                ready_ = true;
                return true;
            }

            bool texture_ready(TextureHandle texture) const noexcept override
            {
                return static_cast<bool>(texture) && ready_;
            }

            void destroy(BufferHandle) noexcept override {}
            void destroy(TextureHandle) noexcept override { ready_ = false; }

            ICommandContext& acquire_graphics_context() override
            {
                return commands_;
            }

            void present(ISwapchain&) override {}

            [[nodiscard]] std::span<const std::byte> uploaded() const noexcept
            {
                return uploaded_;
            }

        private:
            ContractCommandContext commands_{};
            std::vector<std::byte> uploaded_{};
            std::uint32_t next_texture_{};
            bool ready_{};
        };

        [[nodiscard]] inline asset::texture::CompiledTextureArtifact
            contract_artifact(
                asset::texture::DocumentRevision revision)
        {
            using namespace asset::texture;
            TextureCompileProfile profile{};
            profile.format = ArtifactFormat::rgba8_unorm;
            profile.color_space = ColorSpace::linear;
            profile.mipmaps = MipmapPolicy::preserve_authored;
            CompiledTextureArtifact artifact{};
            artifact.status = ArtifactCompilationStatus::ready;
            artifact.identity = build_compiled_texture_artifact_identity(
                revision, profile, 2, 2, 1);
            artifact.identity.compilation_required = false;
            CompiledTextureMip mip{};
            mip.width = 2;
            mip.height = 2;
            mip.row_pitch_bytes = 8;
            mip.texels = {
                std::byte{0xff}, std::byte{0x00}, std::byte{0x00}, std::byte{0xff},
                std::byte{0x00}, std::byte{0xff}, std::byte{0x00}, std::byte{0xff},
                std::byte{0x00}, std::byte{0x00}, std::byte{0xff}, std::byte{0xff},
                std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}};
            mip.content = compiled_texture_mip_content(mip);
            artifact.mips.push_back(std::move(mip));
            artifact.payload_content = compiled_texture_payload_content(
                artifact.identity, artifact.mips);
            return artifact;
        }
    }

    [[nodiscard]] TextureResourceContractFailure
        project_texture_resource_runtime_contract_failure();
}
