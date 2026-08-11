/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <string>
#include <string_view>

export module project.tilemap_pipeline;

import asset.tilemap_artifact;
import project.asset_registry;
import project.tilemap_library;
import render.canvas2d_tilemap;

export namespace epochengine::project_tilemaps
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
        visibility_failure,
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
        case PipelineCode::visibility_failure: return "visibility_failure";
        case PipelineCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct TileMapPipelineLimits final
    {
        project_assets::RegistryLimits registry{};
        TileMapLibraryLimits library{};
        canvas2d::tilemap_runtime::CompileLimits visible{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return registry.valid() && library.valid() && visible.valid()
                && registry.maximum_assets == library.registry.maximum_assets
                && registry.maximum_path_bytes
                    == library.registry.maximum_path_bytes
                && registry.maximum_project_id_bytes
                    == library.registry.maximum_project_id_bytes;
        }
    };

    struct TileMapPipelineMetrics final
    {
        std::uint64_t publication_requests{};
        std::uint64_t publications{};
        std::uint64_t unchanged_publications{};
        std::uint64_t restore_requests{};
        std::uint64_t restorations{};
        std::uint64_t visible_compile_requests{};
        std::uint64_t visible_compilations{};
        std::uint64_t rejected_operations{};
    };

    struct TileMapPipelineResult final
    {
        PipelineCode code{PipelineCode::invalid_pipeline};
        LibraryCode library_code{LibraryCode::invalid_library};
        project_assets::RegistryCode registry_code{
            project_assets::RegistryCode::invalid_registry};
        project_assets::AssetHandle asset{};
        TileMapArtifactLocator locator{};
        std::uint32_t texture_dependencies{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == PipelineCode::ready || code == PipelineCode::unchanged)
                && asset && static_cast<bool>(locator);
        }
    };

    struct VisibleTileMapResult final
    {
        PipelineCode code{PipelineCode::invalid_pipeline};
        TileMapPipelineResult map{};
        canvas2d::tilemap_runtime::VisibleCompilation visible{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == PipelineCode::ready
                && static_cast<bool>(map) && static_cast<bool>(visible);
        }
    };

    struct RestoredTileMapArtifact final
    {
        PipelineCode code{PipelineCode::invalid_pipeline};
        TileMapPipelineResult map{};
        asset::tilemap::CompiledTileMapArtifact artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == PipelineCode::ready
                    || code == PipelineCode::unchanged)
                && static_cast<bool>(map)
                && static_cast<bool>(artifact.identity);
        }
    };

    class ProjectTileMapPipeline final
    {
    public:
        ProjectTileMapPipeline(
            std::string projectId,
            std::string projectRoot,
            TileMapPipelineLimits limits = {}) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] std::string_view project_id() const noexcept;
        [[nodiscard]] std::string_view project_root() const noexcept;
        [[nodiscard]] std::uint64_t project_key() const noexcept;
        [[nodiscard]] const TileMapPipelineLimits& limits() const noexcept;
        [[nodiscard]] TileMapPipelineMetrics metrics() const noexcept;
        [[nodiscard]] const project_assets::AssetRegistry& registry() const noexcept;
        [[nodiscard]] const TileMapArtifactLibrary& library() const noexcept;

        [[nodiscard]] TileMapPipelineResult publish(
            std::string_view logicalPath,
            const asset::tilemap::CompiledTileMapArtifact& artifact) noexcept;
        [[nodiscard]] TileMapPipelineResult restore_exact(
            std::string_view logicalPath,
            const asset::tilemap::ContentHash& artifactKey) noexcept;
        [[nodiscard]] TileMapPipelineResult restore_latest(
            std::string_view logicalPath) noexcept;
        [[nodiscard]] RestoredTileMapArtifact restore_exact_artifact(
            std::string_view logicalPath,
            const asset::tilemap::ContentHash& artifactKey) noexcept;
        [[nodiscard]] RestoredTileMapArtifact restore_latest_artifact(
            std::string_view logicalPath) noexcept;
        [[nodiscard]] VisibleTileMapResult compile_visible_latest(
            std::string_view logicalPath,
            const canvas2d::tilemap_runtime::ViewRequest& view) noexcept;

    private:
        [[nodiscard]] TileMapPipelineResult activate(
            std::string_view logicalPath,
            const asset::tilemap::CompiledTileMapArtifact& artifact,
            TileMapArtifactLocator locator,
            bool libraryChanged) noexcept;
        [[nodiscard]] TileMapPipelineResult reject(
            PipelineCode code,
            LibraryCode libraryCode = LibraryCode::invalid_library,
            project_assets::RegistryCode registryCode =
                project_assets::RegistryCode::invalid_registry) noexcept;

        TileMapPipelineLimits limits_{};
        project_assets::AssetRegistry registry_;
        TileMapArtifactLibrary library_;
        TileMapPipelineMetrics metrics_{};
    };

    enum class TileMapPipelineContractFailure : std::uint8_t
    {
        none,
        temporary_root,
        construction,
        invalid_rejection,
        first_publication,
        idempotent_publication,
        exact_restore,
        exact_library_restore,
        exact_library_not_found,
        exact_library_path_identity,
        exact_library_identify_rejected,
        exact_library_file_missing,
        exact_library_project_identity,
        exact_library_asset_identity,
        exact_library_artifact_identity,
        exact_library_root_identity,
        exact_library_expected_file_missing,
        exact_library_integrity,
        exact_library_invalid,
        latest_restore,
        portable_path_collision,
        visible_compile,
        stale_revision,
        metrics
    };

    [[nodiscard]] constexpr std::string_view
        tilemap_pipeline_contract_failure_name(
            TileMapPipelineContractFailure failure) noexcept
    {
        switch (failure)
        {
        case TileMapPipelineContractFailure::none: return "pass";
        case TileMapPipelineContractFailure::temporary_root:
            return "temporary_root";
        case TileMapPipelineContractFailure::construction: return "construction";
        case TileMapPipelineContractFailure::invalid_rejection:
            return "invalid_rejection";
        case TileMapPipelineContractFailure::first_publication:
            return "first_publication";
        case TileMapPipelineContractFailure::idempotent_publication:
            return "idempotent_publication";
        case TileMapPipelineContractFailure::exact_restore:
            return "exact_restore";
        case TileMapPipelineContractFailure::exact_library_restore:
            return "exact_library_restore";
        case TileMapPipelineContractFailure::exact_library_not_found:
            return "exact_library_not_found";
        case TileMapPipelineContractFailure::exact_library_path_identity:
            return "exact_library_path_identity";
        case TileMapPipelineContractFailure::exact_library_identify_rejected:
            return "exact_library_identify_rejected";
        case TileMapPipelineContractFailure::exact_library_file_missing:
            return "exact_library_file_missing";
        case TileMapPipelineContractFailure::exact_library_project_identity:
            return "exact_library_project_identity";
        case TileMapPipelineContractFailure::exact_library_asset_identity:
            return "exact_library_asset_identity";
        case TileMapPipelineContractFailure::exact_library_artifact_identity:
            return "exact_library_artifact_identity";
        case TileMapPipelineContractFailure::exact_library_root_identity:
            return "exact_library_root_identity";
        case TileMapPipelineContractFailure::exact_library_expected_file_missing:
            return "exact_library_expected_file_missing";
        case TileMapPipelineContractFailure::exact_library_integrity:
            return "exact_library_integrity";
        case TileMapPipelineContractFailure::exact_library_invalid:
            return "exact_library_invalid";
        case TileMapPipelineContractFailure::latest_restore:
            return "latest_restore";
        case TileMapPipelineContractFailure::portable_path_collision:
            return "portable_path_collision";
        case TileMapPipelineContractFailure::visible_compile:
            return "visible_compile";
        case TileMapPipelineContractFailure::stale_revision:
            return "stale_revision";
        case TileMapPipelineContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] TileMapPipelineContractFailure
        project_tilemap_pipeline_runtime_contract_failure() noexcept;
}
