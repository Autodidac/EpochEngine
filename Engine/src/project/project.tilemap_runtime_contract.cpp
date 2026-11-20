/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include "../../include/engine.config.hpp"
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

module project.tilemap_runtime;

import asset.texture_artifact;
import project.texture_resources;
import render.canvas2d_cpu;

#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
import authoring.tilemap;
import project.tilemap_source;
#if EPOCH_ENABLE_TEXTURE_EDITOR
import authoring.texture;
import project.texture_source;
#endif
#endif

namespace epochengine::project_tilemap_runtime
{
    namespace
    {
        class ContractRoot final
        {
        public:
            ContractRoot() noexcept
            {
                try
                {
                    static std::atomic<std::uint64_t> next{1u};
                    std::error_code error{};
                    const auto base = std::filesystem::temp_directory_path(error);
                    if (error || base.empty())
                        return;
                    for (std::uint32_t attempt = 0u; attempt < 32u; ++attempt)
                    {
                        auto candidate = base
                            / ("epoch_tilemap_runtime_contract_"
                                + std::to_string(next.fetch_add(
                                    1u, std::memory_order_relaxed)));
                        if (std::filesystem::create_directory(candidate, error))
                        {
                            path = std::move(candidate);
                            return;
                        }
                        error.clear();
                    }
                }
                catch (...)
                {
                }
            }

            ~ContractRoot()
            {
                std::error_code error{};
                if (!path.empty())
                    std::filesystem::remove_all(path, error);
            }

            std::filesystem::path path{};
        };

        [[nodiscard]] asset::tilemap::CompiledTileMapArtifact compiled_map(
            std::uint64_t projectKey,
            const project_textures::TexturePipelineResult& texture,
            std::string name,
            std::uint64_t sequence,
            bool includeVisibleCell = true) noexcept
        {
            asset::tilemap::CompiledTileMapArtifact artifact{};
            artifact.identity.source_revision = {
                {{31u, 32u, 33u, sequence}}, sequence};
            artifact.name = std::move(name);
            artifact.extent_tiles = {4u, 4u};
            artifact.default_chunk_extent = {4u, 4u};
            artifact.dependencies.push_back({
                .logical_path = texture.locator.canonical_logical_path,
                .artifact_key = texture.locator.artifact_key.words,
                .project_key = projectKey,
                .asset_key = texture.logical.asset_key,
                .artifact_revision = texture.logical.artifact_revision,
                .stable_material_key = 71u,
                .texture_extent = {2u, 2u}});
            artifact.tile_sets.push_back({
                .id = {1u},
                .dependency_index = 0u,
                .tile_extent = {1u, 1u},
                .grid = {2u, 2u},
                .tile_count = 4u});
            artifact.palette.push_back({
                .id = {1u},
                .tile_set_index = 0u,
                .local_tile = 0u});
            artifact.layers.push_back({
                .id = {1u},
                .name = "World",
                .extent_tiles = {4u, 4u},
                .chunk_extent = {4u, 4u},
                .tile_world_extent = {1.0f, 1.0f}});
            artifact.chunks.push_back({
                .layer_index = 0u,
                .coordinate = {0u, 0u},
                .extent = {4u, 4u},
                .revision = sequence,
                .world_bounds = {0.0f, 0.0f, 4.0f, 4.0f},
                .first_cell = 0u,
                .cell_count = includeVisibleCell ? 1u : 0u});
            if (includeVisibleCell)
            {
                artifact.cells.push_back({
                    .coordinate = {1u, 1u},
                    .palette_index = 0u});
            }
            artifact.identity.key =
                asset::tilemap::compiled_tilemap_payload_content(artifact);
            return artifact;
        }
    }

    ContractFailure run_contract() noexcept
    {
        ContractRoot root{};
        if (root.path.empty())
            return ContractFailure::temporary_root;

        constexpr std::string_view projectId{
            "epoch.project-tilemap-runtime.contract"};
        const std::string projectRoot = root.path.generic_string();
        project_textures::ProjectTexturePipeline texturePipeline{
            std::string{projectId}, projectRoot};
        const auto textureArtifact = project_textures::detail::contract_artifact(
            asset::texture::DocumentRevision{{{11u, 12u, 13u, 14u}}, 2u});
        const auto texture = texturePipeline.publish(
            "Assets/Textures/runtime_tiles.rgba", textureArtifact);
        if (!texture)
            return ContractFailure::texture_publication;

#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
#if EPOCH_ENABLE_TEXTURE_EDITOR
        constexpr std::string_view authored_texture_path{"Assets/Textures/source.epoch_texture"};
        const authoring::texture::CanvasDescriptor textureCanvas{
            .width = 2u, .height = 2u, .tile_extent = 2u, .mip_count = 1u,
            .format = authoring::texture::PixelFormat::rgba8_unorm,
            .color_space = asset::texture::ColorSpace::linear};
        authoring::texture::TextureDocument textureDocument{{2u, 1u}, {3u, 1u}, textureCanvas};
        project_texture_sources::ProjectTextureSourceStore textureSourceStore{
            std::string{projectId}, root.path};
        if (!textureSourceStore.save(authored_texture_path, textureDocument))
            return ContractFailure::texture_publication;
        const auto authoredTexture = textureDocument.compile_artifact(
            project_texture_sources::authored_texture_profile(textureDocument));
        const auto sourceTexture = texturePipeline.publish(authored_texture_path, authoredTexture);
        if (!sourceTexture)
            return ContractFailure::texture_publication;
#else
        const auto& sourceTexture = texture;
#endif
        auto created = authoring::tilemap::Document::create(
            {0u, 1u},
            authoring::tilemap::MapDescriptor{
                .name = "Runtime Source",
                .extent_tiles = {4u, 4u},
                .default_chunk_extent = {4u, 4u}});
        if (!created)
            return ContractFailure::source_creation;

        authoring::tilemap::TileSetDescriptor tileSet{};
        tileSet.name = "Runtime Tiles";
        tileSet.texture = {
            .logical_path = sourceTexture.locator.canonical_logical_path,
            .artifact_key = sourceTexture.locator.artifact_key.words,
            .project_key = texturePipeline.project_key(),
            .asset_key = sourceTexture.logical.asset_key,
            .artifact_revision = sourceTexture.logical.artifact_revision,
            .stable_material_key = 71u,
            .texture_extent = {2u, 2u}};
        tileSet.tile_extent = {1u, 1u};
        tileSet.grid = {2u, 2u};
        tileSet.tile_count = 4u;
        const auto tileSetResult = created.document->create_tile_set(
            std::move(tileSet));
        authoring::tilemap::PaletteEntryDescriptor palette{};
        palette.name = "Ground";
        palette.tile_set = tileSetResult.tile_set;
        palette.collision.kind = asset::tilemap::CollisionKind::full_cell;
        const auto paletteResult = created.document->create_palette_entry(
            std::move(palette));
        authoring::tilemap::LayerDescriptor layer{};
        layer.name = "World";
        layer.extent_tiles = {4u, 4u};
        layer.chunk_extent = {4u, 4u};
        layer.collision_source = true;
        const auto layerResult = created.document->create_layer(
            std::move(layer));
        const std::array edits{
            authoring::tilemap::CellEdit{
                {0u, 0u}, {paletteResult.palette_entry}},
            authoring::tilemap::CellEdit{
                {1u, 0u}, {paletteResult.palette_entry}},
            authoring::tilemap::CellEdit{
                {0u, 1u}, {paletteResult.palette_entry}},
            authoring::tilemap::CellEdit{
                {1u, 1u}, {paletteResult.palette_entry}}};
        if (!tileSetResult || !paletteResult || !layerResult
            || !created.document->paint_cells(layerResult.layer, edits))
        {
            return ContractFailure::source_creation;
        }

        project_tilemap_sources::ProjectTileMapSourceStore sourceStore{
            std::string{projectId}, root.path};
        if (!sourceStore.save(
                "Assets/Maps/source.epochmap", *created.document))
        {
            return ContractFailure::source_save;
        }

#if EPOCH_ENABLE_TEXTURE_EDITOR
        const std::filesystem::path textureCache{sourceTexture.locator.storage_path};
        std::error_code cacheError;
        if (!std::filesystem::remove(textureCache, cacheError) || cacheError)
            return ContractFailure::texture_publication;
#endif
        ProjectTileMapRuntime sourceRuntime{
            std::string{projectId}, projectRoot};
        if (!sourceRuntime.valid())
            return ContractFailure::runtime_construction;
        auto regenerated = sourceRuntime.prepare({
            .logical_path = "Assets/Maps/source.epochmap",
            .source_policy = SourcePolicy::require_source});
        if (!regenerated
            || regenerated.provenance != Provenance::source_compiled
            || !regenerated.library_changed
            || regenerated.scene.sprites.size() != 4u
            || regenerated.collision.size() != 4u)
        {
            return ContractFailure::source_regeneration;
        }
        std::error_code libraryError{};
        if (!std::filesystem::is_regular_file(
                regenerated.map.locator.storage_path, libraryError)
            || libraryError)
        {
            return ContractFailure::library_artifact_missing;
        }
        if (canvas2d::scene_content::validate_resource_closure(
                regenerated.scene.sprites,
                regenerated.scene.resources)
            != canvas2d::scene_content::ResourceClosureCode::ready)
        {
            return ContractFailure::resource_closure;
        }
        const RuntimeMetrics sourceMetrics = sourceRuntime.metrics();
        if (sourceMetrics.prepare_requests != 1u
            || sourceMetrics.prepared_scenes != 1u
            || sourceMetrics.source_compilations != 1u
            || sourceMetrics.texture_restorations != 1u
            || sourceMetrics.rejected_requests != 0u)
        {
            return ContractFailure::metrics;
        }

#if EPOCH_ENABLE_TEXTURE_EDITOR
        const auto restoredTexture = texturePipeline.restore_exact(
            authored_texture_path, sourceTexture.locator.artifact_key);
        const auto retainedSource = textureSourceStore.load(authored_texture_path);
        if (!restoredTexture || restoredTexture.logical != sourceTexture.logical
            || !retainedSource || retainedSource.document->revision() != textureDocument.revision())
            return ContractFailure::texture_source_regeneration;

        {
            std::ofstream corrupt{textureCache, std::ios::binary | std::ios::trunc};
            corrupt << "bad";
            if (!corrupt)
                return ContractFailure::texture_source_regeneration;
        }
        ProjectTileMapRuntime guardedRuntime{std::string{projectId}, projectRoot};
        const auto corrupt = guardedRuntime.prepare({
            .logical_path = "Assets/Maps/source.epochmap",
            .source_policy = SourcePolicy::prefer_source});
        if (corrupt.code != RuntimeCode::texture_restore_failure
            || std::filesystem::file_size(textureCache, cacheError) != 3u || cacheError
            || !std::filesystem::remove(textureCache, cacheError) || cacheError)
            return ContractFailure::texture_source_regeneration;

        const auto compiledOnly = guardedRuntime.prepare({
            .logical_path = "Assets/Maps/source.epochmap",
            .source_policy = SourcePolicy::compiled_only});
        if (compiledOnly.code != RuntimeCode::texture_restore_failure
            || std::filesystem::exists(textureCache, cacheError) || cacheError)
            return ContractFailure::texture_source_regeneration;

        auto changedCanvas = textureCanvas;
        changedCanvas.width = 4u;
        authoring::texture::TextureDocument changedTexture{{2u, 1u}, {3u, 1u}, changedCanvas};
        if (!textureSourceStore.save(authored_texture_path, changedTexture))
            return ContractFailure::texture_source_regeneration;
        const auto changed = guardedRuntime.prepare({
            .logical_path = "Assets/Maps/source.epochmap",
            .source_policy = SourcePolicy::prefer_source});
        if (changed.code != RuntimeCode::texture_identity_mismatch
            || std::filesystem::exists(textureCache, cacheError) || cacheError)
            return ContractFailure::texture_source_regeneration;
#endif

        const auto staleArtifact = compiled_map(
            texturePipeline.project_key(), texture, "Stale Runtime", 8u);
        project_tilemaps::ProjectTileMapPipeline stalePipeline{
            std::string{projectId}, projectRoot};
        if (!stalePipeline.publish(
                "Assets/Maps/malformed.epochmap", staleArtifact))
        {
            return ContractFailure::source_creation;
        }
        const auto malformedPath = root.path
            / "Assets" / "Maps" / "malformed.epochmap";
        {
            std::ofstream malformed{
                malformedPath, std::ios::binary | std::ios::trunc};
            constexpr std::array<char, 3u> badSource{'E', 'P', 'O'};
            malformed.write(
                badSource.data(),
                static_cast<std::streamsize>(badSource.size()));
            if (!malformed)
                return ContractFailure::source_save;
        }
        ProjectTileMapRuntime malformedRuntime{
            std::string{projectId}, projectRoot};
        const auto malformed = malformedRuntime.prepare({
            .logical_path = "Assets/Maps/malformed.epochmap",
            .source_policy = SourcePolicy::prefer_source});
        if (malformed.code != RuntimeCode::source_failure
            || malformedRuntime.metrics().library_restorations != 0u
            || malformedRuntime.metrics().rejected_requests != 1u)
        {
            return ContractFailure::malformed_source_gate;
        }
#endif

        project_tilemaps::ProjectTileMapPipeline mapPipeline{
            std::string{projectId}, projectRoot};
        const auto compiled = compiled_map(
            texturePipeline.project_key(), texture, "Compiled Runtime", 9u);
        if (!mapPipeline.publish("Assets/Maps/compiled.epochmap", compiled))
            return ContractFailure::source_creation;

        ProjectTileMapRuntime compiledRuntime{
            std::string{projectId}, projectRoot};
        const auto restored = compiledRuntime.prepare({
            .logical_path = "Assets/Maps/compiled.epochmap",
            .source_policy = SourcePolicy::compiled_only});
        if (!restored
            || restored.provenance != Provenance::library_restored
            || restored.scene.sprites.size() != 1u)
        {
            return ContractFailure::compiled_only_restore;
        }
        if (canvas2d::scene_content::validate_resource_closure(
                restored.scene.sprites, restored.scene.resources)
            != canvas2d::scene_content::ResourceClosureCode::ready)
        {
            return ContractFailure::resource_closure;
        }
        if (restored.scene.resources.bindings.textures.size() != 1u
            || restored.scene.resources.bindings.textures.front().logical
                != texture.logical
            || restored.scene.sprites.front().material.alpha != canvas2d::SpriteAlphaMode::straight
            || restored.scene.sprites.front().material.color_space
                != restored.scene.resources.bindings.textures.front().color_space)
        {
            return ContractFailure::visible_resource_selection;
        }
        int canvasOwner{};
        const auto publication = canvas2d::scene_content::publish(&canvasOwner, restored.scene);
        const auto acquired = canvas2d::scene_content::acquire(&canvasOwner);
        const auto frame = canvas2d::scene_content::compile(acquired, {1280u, 720u});
        const auto raster = canvas2d::cpu::rasterize(frame, restored.scene.resources.bindings);
        (void)canvas2d::scene_content::retire(&canvasOwner);
        if (!publication || !frame || !raster || raster.metrics.fragments_shaded == 0u)
            return ContractFailure::visible_resource_selection;

        const auto emptyCompiled = compiled_map(
            texturePipeline.project_key(), texture, "Empty Runtime", 10u, false);
        if (!mapPipeline.publish("Assets/Maps/empty.epochmap", emptyCompiled))
            return ContractFailure::source_creation;

        ProjectTileMapRuntime emptyRuntime{
            std::string{projectId}, projectRoot};
        const auto empty = emptyRuntime.prepare({
            .logical_path = "Assets/Maps/empty.epochmap",
            .source_policy = SourcePolicy::compiled_only});
        const RuntimeMetrics emptyMetrics = emptyRuntime.metrics();
        if (!empty
            || !empty.scene.sprites.empty()
            || empty.scene.resources.owner
            || !empty.scene.resources.bindings.textures.empty()
            || !empty.scene.resources.bindings.clips.empty()
            || empty.texture_dependencies.size() != 1u
            || emptyMetrics.texture_restorations != 1u
            || canvas2d::scene_content::validate_resource_closure(
                    empty.scene.sprites, empty.scene.resources)
                != canvas2d::scene_content::ResourceClosureCode::ready)
        {
            return ContractFailure::visible_resource_selection;
        }

        const auto missingSource = compiledRuntime.prepare({
            .logical_path = "Assets/Maps/missing.epochmap",
            .source_policy = SourcePolicy::require_source});
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
        if (missingSource.code != RuntimeCode::source_not_found)
#else
        if (missingSource.code != RuntimeCode::source_unavailable)
#endif
            return ContractFailure::source_required_gate;

        const RuntimeMetrics compiledMetrics = compiledRuntime.metrics();
        if (compiledMetrics.prepare_requests != 2u
            || compiledMetrics.prepared_scenes != 1u
            || compiledMetrics.library_restorations != 1u
            || compiledMetrics.texture_restorations != 1u
            || compiledMetrics.rejected_requests != 1u)
        {
            return ContractFailure::metrics;
        }
        return ContractFailure::none;
    }
}
