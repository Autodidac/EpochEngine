/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

export module project.texture_pipeline;

import asset.texture_artifact;
import project.asset_registry;
import project.texture_library;
import project.texture_resources;
import render.canvas2d;
import render.canvas2d_scene;

export namespace epochengine::project_textures
{
    enum class PipelineCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_pipeline,
        invalid_path,
        invalid_artifact,
        library_failure,
        registry_failure,
        resource_failure,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view pipeline_code_name(
        PipelineCode code) noexcept
    {
        switch (code)
        {
        case PipelineCode::ready: return "ready";
        case PipelineCode::unchanged: return "unchanged";
        case PipelineCode::invalid_pipeline: return "invalid_pipeline";
        case PipelineCode::invalid_path: return "invalid_path";
        case PipelineCode::invalid_artifact: return "invalid_artifact";
        case PipelineCode::library_failure: return "library_failure";
        case PipelineCode::registry_failure: return "registry_failure";
        case PipelineCode::resource_failure: return "resource_failure";
        case PipelineCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct TexturePipelineLimits final
    {
        project_assets::RegistryLimits registry{};
        TextureLibraryLimits library{};
        TextureResourceLimits resources{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return registry.valid() && library.valid() && resources.valid()
                && registry.maximum_assets
                    == library.registry.maximum_assets
                && registry.maximum_path_bytes
                    == library.registry.maximum_path_bytes
                && registry.maximum_project_id_bytes
                    == library.registry.maximum_project_id_bytes;
        }
    };

    struct TexturePipelineMetrics final
    {
        std::uint64_t publication_requests{};
        std::uint64_t publications{};
        std::uint64_t unchanged_publications{};
        std::uint64_t restore_requests{};
        std::uint64_t restorations{};
        std::uint64_t lease_requests{};
        std::uint64_t leases{};
        std::uint64_t rejected_operations{};
    };

    struct TexturePipelineResult final
    {
        PipelineCode code{PipelineCode::invalid_pipeline};
        LibraryCode library_code{LibraryCode::invalid_library};
        project_assets::RegistryCode registry_code{
            project_assets::RegistryCode::invalid_registry};
        ResourceCode resource_code{ResourceCode::invalid_registry};
        project_assets::AssetHandle asset{};
        canvas2d::LogicalTextureReference logical{};
        TextureArtifactLocator locator{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return (code == PipelineCode::ready
                    || code == PipelineCode::unchanged)
                && asset && logical && static_cast<bool>(locator);
        }
    };

    struct Canvas2DLeaseResult final
    {
        PipelineCode code{PipelineCode::invalid_pipeline};
        ResourceCode resource_code{ResourceCode::invalid_registry};
        canvas2d::scene_content::ResourceLease lease{};
        std::uint64_t decoded_bytes{};
        std::uint32_t texture_count{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == PipelineCode::ready && lease.valid();
        }
    };

    class ProjectTexturePipeline final
    {
    public:
        ProjectTexturePipeline(
            std::string projectId,
            std::string projectRoot,
            TexturePipelineLimits limits = {}) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] std::string_view project_id() const noexcept;
        [[nodiscard]] std::string_view project_root() const noexcept;
        [[nodiscard]] std::uint64_t project_key() const noexcept;
        [[nodiscard]] const TexturePipelineLimits& limits() const noexcept;
        [[nodiscard]] TexturePipelineMetrics metrics() const noexcept;
        [[nodiscard]] const project_assets::AssetRegistry& registry() const noexcept;
        [[nodiscard]] const TextureArtifactLibrary& library() const noexcept;
        [[nodiscard]] const TextureResourceService& resources() const noexcept;

        [[nodiscard]] TexturePipelineResult publish(
            std::string_view logicalPath,
            const asset::texture::CompiledTextureArtifact& artifact) noexcept;

        [[nodiscard]] TexturePipelineResult restore_latest(
            std::string_view logicalPath,
            const asset::texture::TextureCompileProfile& profile) noexcept;

        [[nodiscard]] Canvas2DLeaseResult bind_canvas2d(
            std::span<const canvas2d::LogicalTextureReference> logicalTextures) noexcept;

    private:
        [[nodiscard]] TexturePipelineResult activate(
            std::string_view logicalPath,
            const asset::texture::CompiledTextureArtifact& artifact,
            TextureArtifactLocator locator,
            bool libraryChanged) noexcept;
        [[nodiscard]] TexturePipelineResult reject(
            PipelineCode code,
            LibraryCode libraryCode = LibraryCode::invalid_library,
            project_assets::RegistryCode registryCode =
                project_assets::RegistryCode::invalid_registry,
            ResourceCode resourceCode = ResourceCode::invalid_registry) noexcept;

        TexturePipelineLimits limits_{};
        project_assets::AssetRegistry registry_;
        TextureArtifactLibrary library_;
        TextureResourceService resources_;
        TexturePipelineMetrics metrics_{};
    };

    enum class TexturePipelineContractFailure : std::uint8_t
    {
        none,
        temporary_root,
        construction,
        invalid_rejection,
        first_publication,
        idempotent_publication,
        exact_load,
        restoration,
        project_isolation,
        portable_path_collision,
        lease,
        resource_closure,
        scene_publication,
        cpu_raster,
        revision_advance,
        stale_revision,
        metrics
    };

    [[nodiscard]] constexpr std::string_view
        texture_pipeline_contract_failure_name(
            TexturePipelineContractFailure failure) noexcept
    {
        switch (failure)
        {
        case TexturePipelineContractFailure::none: return "pass";
        case TexturePipelineContractFailure::temporary_root: return "temporary_root";
        case TexturePipelineContractFailure::construction: return "construction";
        case TexturePipelineContractFailure::invalid_rejection:
            return "invalid_rejection";
        case TexturePipelineContractFailure::first_publication:
            return "first_publication";
        case TexturePipelineContractFailure::idempotent_publication:
            return "idempotent_publication";
        case TexturePipelineContractFailure::exact_load: return "exact_load";
        case TexturePipelineContractFailure::restoration: return "restoration";
        case TexturePipelineContractFailure::project_isolation:
            return "project_isolation";
        case TexturePipelineContractFailure::portable_path_collision:
            return "portable_path_collision";
        case TexturePipelineContractFailure::lease: return "lease";
        case TexturePipelineContractFailure::resource_closure:
            return "resource_closure";
        case TexturePipelineContractFailure::scene_publication:
            return "scene_publication";
        case TexturePipelineContractFailure::cpu_raster: return "cpu_raster";
        case TexturePipelineContractFailure::revision_advance:
            return "revision_advance";
        case TexturePipelineContractFailure::stale_revision:
            return "stale_revision";
        case TexturePipelineContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] TexturePipelineContractFailure
        project_texture_pipeline_runtime_contract_failure() noexcept;
}
