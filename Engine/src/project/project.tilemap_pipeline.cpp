/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

module project.tilemap_pipeline;

import render.canvas2d;

namespace epochengine::project_tilemaps
{
    namespace
    {
        [[nodiscard]] project_assets::AssetRevision asset_revision(
            const asset::tilemap::DocumentRevision& revision) noexcept
        {
            return {{revision.content.words}, revision.sequence};
        }

        [[nodiscard]] bool accepted(
            project_assets::RegistryCode code) noexcept
        {
            return code == project_assets::RegistryCode::ready
                || code == project_assets::RegistryCode::already_registered
                || code == project_assets::RegistryCode::unchanged;
        }

        class ContractRoot final
        {
        public:
            ContractRoot() noexcept
            {
                try
                {
                    static std::atomic<std::uint64_t> next{1u};
                    std::error_code error{};
                    const std::filesystem::path base =
                        std::filesystem::temp_directory_path(error);
                    if (error || base.empty())
                        return;
                    for (std::uint32_t attempt = 0u; attempt < 32u; ++attempt)
                    {
                        const std::uint64_t identity = next.fetch_add(
                            1u, std::memory_order_relaxed);
                        std::filesystem::path candidate = base
                            / ("epoch_tilemap_pipeline_contract_"
                                + std::to_string(identity));
                        if (std::filesystem::create_directory(candidate, error))
                        {
                            root_ = std::move(candidate);
                            return;
                        }
                        if (error)
                            error.clear();
                    }
                }
                catch (...)
                {
                }
            }

            ~ContractRoot()
            {
                if (root_.empty())
                    return;
                std::error_code error{};
                std::filesystem::remove_all(root_, error);
            }

            [[nodiscard]] bool valid() const noexcept
            {
                return !root_.empty();
            }

            [[nodiscard]] std::string string() const
            {
                return root_.generic_string();
            }

        private:
            std::filesystem::path root_{};
        };

        [[nodiscard]] asset::tilemap::CompiledTileMapArtifact contract_artifact(
            std::uint64_t sequence,
            std::uint64_t seed)
        {
            using namespace asset::tilemap;
            CompiledTileMapArtifact artifact{};
            artifact.identity.source_revision = {
                {{seed + 1u, seed + 2u, seed + 3u, seed + 4u}}, sequence};
            artifact.name = "Contract Map";
            artifact.extent_tiles = {16u, 16u};
            artifact.default_chunk_extent = {8u, 8u};
            artifact.dependencies.push_back({
                .logical_path = "Assets/Textures/contract_tiles.rgba",
                .artifact_key = {10u, 20u, 30u, 40u},
                .project_key = 101u,
                .asset_key = 102u,
                .artifact_revision = 103u,
                .stable_material_key = 104u,
                .texture_extent = {32u, 16u}});
            artifact.tile_sets.push_back({
                .id = {201u},
                .dependency_index = 0u,
                .tile_extent = {16u, 16u},
                .grid = {2u, 1u},
                .tile_count = 2u});
            artifact.palette.push_back({
                .id = {301u},
                .tile_set_index = 0u,
                .local_tile = 1u});
            artifact.layers.push_back({
                .id = {401u},
                .name = "World",
                .extent_tiles = {16u, 16u},
                .chunk_extent = {8u, 8u},
                .tile_world_extent = {1.0f, 1.0f}});
            artifact.chunks.push_back({
                .layer_index = 0u,
                .coordinate = {0u, 0u},
                .extent = {8u, 8u},
                .revision = sequence,
                .world_bounds = {0.0f, 0.0f, 8.0f, 8.0f},
                .first_cell = 0u,
                .cell_count = 1u});
            artifact.cells.push_back({
                .coordinate = {2u, 3u},
                .palette_index = 0u});
            artifact.identity.key = compiled_tilemap_payload_content(artifact);
            return artifact;
        }
    }

    ProjectTileMapPipeline::ProjectTileMapPipeline(
        std::string projectId,
        std::string projectRoot,
        TileMapPipelineLimits limits) noexcept
        : limits_(limits),
          registry_(projectId, limits.registry),
          library_(projectId, std::move(projectRoot), limits.library)
    {
    }

    bool ProjectTileMapPipeline::valid() const noexcept
    {
        return limits_.valid() && registry_.valid() && library_.valid()
            && registry_.project_key() == library_.project_key();
    }

    std::string_view ProjectTileMapPipeline::project_id() const noexcept
    {
        return registry_.project_id();
    }

    std::string_view ProjectTileMapPipeline::project_root() const noexcept
    {
        return library_.project_root();
    }

    std::uint64_t ProjectTileMapPipeline::project_key() const noexcept
    {
        return valid() ? registry_.project_key() : 0u;
    }

    const TileMapPipelineLimits& ProjectTileMapPipeline::limits() const noexcept
    {
        return limits_;
    }

    TileMapPipelineMetrics ProjectTileMapPipeline::metrics() const noexcept
    {
        return metrics_;
    }

    const project_assets::AssetRegistry&
        ProjectTileMapPipeline::registry() const noexcept
    {
        return registry_;
    }

    const TileMapArtifactLibrary&
        ProjectTileMapPipeline::library() const noexcept
    {
        return library_;
    }

    TileMapPipelineResult ProjectTileMapPipeline::publish(
        std::string_view logicalPath,
        const asset::tilemap::CompiledTileMapArtifact& artifact) noexcept
    {
        ++metrics_.publication_requests;
        if (!valid())
            return reject(PipelineCode::invalid_pipeline);
        if (logicalPath.empty())
            return reject(PipelineCode::invalid_path);
        if (!asset::tilemap::validate(artifact, limits_.library.artifact))
            return reject(PipelineCode::invalid_artifact);

        try
        {
            const auto canonical = project_assets::canonical_logical_path(
                logicalPath, limits_.registry);
            if (!canonical)
                return reject(PipelineCode::invalid_path);
            const project_assets::AssetRecord* existing =
                registry_.find_by_path(*canonical);
            if (existing && existing->canonical_path != *canonical)
            {
                return reject(
                    PipelineCode::registry_failure,
                    LibraryCode::invalid_path,
                    project_assets::RegistryCode::path_collision);
            }
            TileMapArtifactLocator locator = library_.persist(
                *canonical, artifact);
            if (!locator)
                return reject(PipelineCode::library_failure, locator.code);
            const bool changed = locator.code == LibraryCode::ready;
            return activate(
                *canonical, artifact, std::move(locator), changed);
        }
        catch (...)
        {
            return reject(PipelineCode::allocation_failure);
        }
    }

    TileMapPipelineResult ProjectTileMapPipeline::restore_exact(
        std::string_view logicalPath,
        const asset::tilemap::ContentHash& artifactKey) noexcept
    {
        return restore_exact_artifact(logicalPath, artifactKey).map;
    }

    RestoredTileMapArtifact ProjectTileMapPipeline::restore_exact_artifact(
        std::string_view logicalPath,
        const asset::tilemap::ContentHash& artifactKey) noexcept
    {
        ++metrics_.restore_requests;
        LoadedTileMapArtifact loaded = library_.load_exact(
            logicalPath, artifactKey);
        if (!loaded)
        {
            TileMapPipelineResult rejected = reject(
                PipelineCode::library_failure, loaded.code);
            return {rejected.code, std::move(rejected), {}};
        }
        const std::string canonicalPath =
            loaded.locator.canonical_logical_path;
        TileMapPipelineResult result = activate(
            canonicalPath,
            loaded.artifact,
            std::move(loaded.locator),
            false);
        if (result)
            ++metrics_.restorations;
        const PipelineCode code = result.code;
        return {
            code,
            std::move(result),
            code == PipelineCode::ready || code == PipelineCode::unchanged
                ? std::move(loaded.artifact)
                : asset::tilemap::CompiledTileMapArtifact{}};
    }

    TileMapPipelineResult ProjectTileMapPipeline::restore_latest(
        std::string_view logicalPath) noexcept
    {
        return restore_latest_artifact(logicalPath).map;
    }

    RestoredTileMapArtifact ProjectTileMapPipeline::restore_latest_artifact(
        std::string_view logicalPath) noexcept
    {
        ++metrics_.restore_requests;
        LoadedTileMapArtifact loaded = library_.load_latest(logicalPath);
        if (!loaded)
        {
            TileMapPipelineResult rejected = reject(
                PipelineCode::library_failure, loaded.code);
            return {rejected.code, std::move(rejected), {}};
        }
        const std::string canonicalPath =
            loaded.locator.canonical_logical_path;
        TileMapPipelineResult result = activate(
            canonicalPath,
            loaded.artifact,
            std::move(loaded.locator),
            false);
        if (result)
            ++metrics_.restorations;
        const PipelineCode code = result.code;
        return {
            code,
            std::move(result),
            code == PipelineCode::ready || code == PipelineCode::unchanged
                ? std::move(loaded.artifact)
                : asset::tilemap::CompiledTileMapArtifact{}};
    }

    VisibleTileMapResult ProjectTileMapPipeline::compile_visible_latest(
        std::string_view logicalPath,
        const canvas2d::tilemap_runtime::ViewRequest& view) noexcept
    {
        ++metrics_.visible_compile_requests;
        ++metrics_.restore_requests;
        LoadedTileMapArtifact loaded = library_.load_latest(logicalPath);
        if (!loaded)
        {
            return {
                PipelineCode::library_failure,
                reject(PipelineCode::library_failure, loaded.code),
                {}};
        }
        const std::string canonicalPath =
            loaded.locator.canonical_logical_path;
        TileMapPipelineResult map = activate(
            canonicalPath,
            loaded.artifact,
            std::move(loaded.locator),
            false);
        if (!map)
            return {map.code, std::move(map), {}};
        ++metrics_.restorations;
        auto visible = canvas2d::tilemap_runtime::compile_visible(
            loaded.artifact, view, limits_.visible);
        if (!visible)
        {
            ++metrics_.rejected_operations;
            return {PipelineCode::visibility_failure, std::move(map), {}};
        }
        ++metrics_.visible_compilations;
        return {
            PipelineCode::ready,
            std::move(map),
            std::move(visible)};
    }

    TileMapPipelineResult ProjectTileMapPipeline::activate(
        std::string_view logicalPath,
        const asset::tilemap::CompiledTileMapArtifact& artifact,
        TileMapArtifactLocator locator,
        bool libraryChanged) noexcept
    {
        const project_assets::AssetRevision revision = asset_revision(
            artifact.identity.source_revision);
        const project_assets::AssetRecord* record =
            registry_.find_by_path(logicalPath);
        project_assets::AssetHandle handle{};
        project_assets::RegistryCode registryCode{
            project_assets::RegistryCode::ready};
        if (!record)
        {
            const auto registered = registry_.register_asset({
                logicalPath,
                project_assets::AssetKind::tilemap,
                revision});
            registryCode = registered.code;
            handle = registered.handle;
            if (!registered)
                return reject(
                    PipelineCode::registry_failure,
                    locator.code,
                    registryCode);
        }
        else
        {
            if (record->identity.kind != project_assets::AssetKind::tilemap)
            {
                return reject(
                    PipelineCode::registry_failure,
                    locator.code,
                    project_assets::RegistryCode::path_collision);
            }
            handle = record->handle;
            registryCode = record->revision == revision
                ? project_assets::RegistryCode::unchanged
                : registry_.update_revision(handle, revision);
            if (!accepted(registryCode))
            {
                return reject(
                    PipelineCode::registry_failure,
                    locator.code,
                    registryCode);
            }
        }

        const bool changed = libraryChanged
            || registryCode == project_assets::RegistryCode::ready;
        if (changed)
            ++metrics_.publications;
        else
            ++metrics_.unchanged_publications;
        return {
            changed ? PipelineCode::ready : PipelineCode::unchanged,
            locator.code,
            registryCode,
            handle,
            std::move(locator),
            static_cast<std::uint32_t>(artifact.dependencies.size())};
    }

    TileMapPipelineResult ProjectTileMapPipeline::reject(
        PipelineCode code,
        LibraryCode libraryCode,
        project_assets::RegistryCode registryCode) noexcept
    {
        ++metrics_.rejected_operations;
        return {code, libraryCode, registryCode};
    }

    TileMapPipelineContractFailure
        project_tilemap_pipeline_runtime_contract_failure() noexcept
    {
        ContractRoot root{};
        if (!root.valid())
            return TileMapPipelineContractFailure::temporary_root;
        const std::string projectRoot = root.string();
        ProjectTileMapPipeline pipeline{
            "epoch.project-tilemap-pipeline.contract", projectRoot};
        if (!pipeline.valid())
            return TileMapPipelineContractFailure::construction;

        const auto first = contract_artifact(7u, 10u);
        auto invalid = first;
        invalid.identity.key.words[0] ^= 1u;
        if (pipeline.publish("Assets/Maps/level01.epochmap", invalid).code
            != PipelineCode::invalid_artifact)
        {
            return TileMapPipelineContractFailure::invalid_rejection;
        }

        const auto published = pipeline.publish(
            "Assets\\Maps//level01.epochmap", first);
        if (!published || published.code != PipelineCode::ready
            || published.locator.canonical_logical_path
                != "Assets/Maps/level01.epochmap"
            || published.texture_dependencies != 1u)
        {
            return TileMapPipelineContractFailure::first_publication;
        }
        const auto repeated = pipeline.publish(
            "Assets/Maps/level01.epochmap", first);
        if (!repeated || repeated.code != PipelineCode::unchanged
            || repeated.asset != published.asset)
        {
            return TileMapPipelineContractFailure::idempotent_publication;
        }
        if (pipeline.publish("assets/maps/level01.epochmap", first).code
            != PipelineCode::registry_failure)
        {
            return TileMapPipelineContractFailure::portable_path_collision;
        }

        TileMapArtifactLibrary verifier{
            "epoch.project-tilemap-pipeline.contract", projectRoot};
        const auto expectedLocator = verifier.identify(
            "Assets/Maps/level01.epochmap", first);
        if (!expectedLocator)
            return TileMapPipelineContractFailure::exact_library_identify_rejected;
        if (expectedLocator.project_key != published.locator.project_key)
        {
            return TileMapPipelineContractFailure::exact_library_project_identity;
        }
        if (expectedLocator.asset_key != published.locator.asset_key)
            return TileMapPipelineContractFailure::exact_library_asset_identity;
        if (expectedLocator.artifact_key.words != published.locator.artifact_key.words)
        {
            return TileMapPipelineContractFailure::
                exact_library_artifact_identity;
        }
        if (verifier.project_root() != pipeline.project_root())
        {
            return TileMapPipelineContractFailure::exact_library_root_identity;
        }
        std::error_code fileError{};
        if (!std::filesystem::is_regular_file(
                published.locator.storage_path, fileError) || fileError)
        {
            return TileMapPipelineContractFailure::exact_library_file_missing;
        }
        fileError.clear();
        if (!std::filesystem::is_regular_file(
                expectedLocator.storage_path, fileError) || fileError)
        {
            return TileMapPipelineContractFailure::
                exact_library_expected_file_missing;
        }
        const auto verified = verifier.load_exact(
            "Assets/Maps/level01.epochmap", first.identity.key);
        if (!verified)
        {
            if (verified.code == LibraryCode::not_found)
                return TileMapPipelineContractFailure::exact_library_not_found;
            if (verified.code == LibraryCode::integrity_failure)
                return TileMapPipelineContractFailure::exact_library_integrity;
            if (verified.code == LibraryCode::invalid_library
                || verified.code == LibraryCode::invalid_path
                || verified.code == LibraryCode::invalid_artifact)
            {
                return TileMapPipelineContractFailure::exact_library_invalid;
            }
            return TileMapPipelineContractFailure::exact_library_restore;
        }

        ProjectTileMapPipeline exact{
            "epoch.project-tilemap-pipeline.contract", projectRoot};
        if (!exact.restore_exact(
                "Assets/Maps/level01.epochmap", first.identity.key))
        {
            return TileMapPipelineContractFailure::exact_restore;
        }
        ProjectTileMapPipeline latest{
            "epoch.project-tilemap-pipeline.contract", projectRoot};
        if (!latest.restore_latest("Assets/Maps/level01.epochmap"))
            return TileMapPipelineContractFailure::latest_restore;

        const auto visible = latest.compile_visible_latest(
            "Assets/Maps/level01.epochmap",
            {.world_bounds = {0.0f, 0.0f, 8.0f, 8.0f}});
        if (!visible || visible.visible.sprites.size() != 1u
            || visible.visible.sprites[0].material.logical_texture
                != canvas2d::LogicalTextureReference{102u, 103u})
        {
            return TileMapPipelineContractFailure::visible_compile;
        }

        const auto second = contract_artifact(8u, 20u);
        if (!pipeline.publish("Assets/Maps/level01.epochmap", second)
            || pipeline.publish("Assets/Maps/level01.epochmap", first).code
                != PipelineCode::registry_failure)
        {
            return TileMapPipelineContractFailure::stale_revision;
        }
        const TileMapPipelineMetrics metrics = pipeline.metrics();
        if (metrics.publication_requests != 6u
            || metrics.publications != 2u
            || metrics.unchanged_publications != 1u
            || metrics.rejected_operations < 3u)
        {
            return TileMapPipelineContractFailure::metrics;
        }
        return TileMapPipelineContractFailure::none;
    }
}
