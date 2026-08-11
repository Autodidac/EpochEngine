/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

module authoring.tilemap;

namespace epochengine::authoring::tilemap
{
    ContractFailure run_contract() noexcept
    {
        const CreateResult created = Document::create(
            {0u, 1u},
            {.name = "ContractMap", .extent_tiles = {16u, 8u},
             .default_chunk_extent = {4u, 4u}});
        if (!created)
            return ContractFailure::create_document;
        Document& document = *created.document;

        TextureDependency texture{
            .logical_path = "Assets/Textures/tiles.ppm",
            .artifact_key = {11u, 12u, 13u, 14u},
            .project_key = 21u,
            .asset_key = 22u,
            .artifact_revision = 23u,
            .stable_material_key = 24u,
            .texture_extent = {64u, 16u}};
        const MutationResult tileSet = document.create_tile_set({
            .name = "Terrain",
            .texture = texture,
            .tile_extent = {16u, 16u},
            .grid = {4u, 1u},
            .tile_count = 4u});
        if (!tileSet || !tileSet.tile_set)
            return ContractFailure::create_tile_set;

        const MutationResult palette = document.create_palette_entry({
            .name = "Ground",
            .tile_set = tileSet.tile_set,
            .local_tile = 1u,
            .collision = {
                .kind = asset::tilemap::CollisionKind::full_cell,
                .local_bounds = {0.0f, 0.0f, 1.0f, 1.0f},
                .layer_bits = 2u,
                .mask_bits = 7u}});
        if (!palette || !palette.palette_entry)
            return ContractFailure::create_palette;

        const MutationResult layer = document.create_layer({
            .name = "World",
            .extent_tiles = {16u, 8u},
            .chunk_extent = {4u, 4u},
            .world_origin = {-2.0f, -1.0f},
            .tile_world_extent = {1.0f, 1.0f},
            .collision_source = true});
        if (!layer || !layer.layer)
            return ContractFailure::create_layer;

        const std::array<CellEdit, 3> edits{{
            {{0u, 0u}, {.palette = palette.palette_entry}},
            {{1u, 0u}, {
                .palette = palette.palette_entry,
                .transform = TileTransform::flip_x,
                .tint = {200u, 210u, 220u, 255u}}},
            {{4u, 0u}, {
                .palette = palette.palette_entry,
                .transform = TileTransform::rotate_90}}
        }};
        const MutationResult painted = document.paint_cells(layer.layer, edits);
        if (!painted || painted.affected_cells != edits.size()
            || document.metrics().active_chunks != 2u
            || document.metrics().occupied_cells != 3u)
        {
            return ContractFailure::paint_cells;
        }

        const CompilationResult firstCompile = document.compile();
        const CompilationResult secondCompile = document.compile();
        if (!firstCompile || !asset::tilemap::validate(firstCompile.artifact))
            return ContractFailure::collision_compile;
        if (firstCompile.artifact.collision.size() != 3u
            || firstCompile.artifact.chunks.size() != 2u
            || firstCompile.artifact.cells.size() != 3u)
        {
            return ContractFailure::collision_compile;
        }
        if (!secondCompile
            || firstCompile.artifact.identity != secondCompile.artifact.identity)
        {
            return ContractFailure::deterministic_compile;
        }

        const SnapshotSerializationResult source = document.serialize();
        if (!source)
            return ContractFailure::source_round_trip;
        const RestoreResult restored = Document::deserialize(source.bytes);
        if (!restored || restored.document->revision() != document.revision())
            return ContractFailure::source_round_trip;
        const CompilationResult restoredCompile = restored.document->compile();
        if (!restoredCompile
            || restoredCompile.artifact.identity != firstCompile.artifact.identity)
        {
            return ContractFailure::source_round_trip;
        }

        const asset::tilemap::SerializationResult artifactBytes =
            asset::tilemap::serialize(firstCompile.artifact);
        const asset::tilemap::DeserializationResult artifactRoundTrip =
            artifactBytes
                ? asset::tilemap::deserialize(artifactBytes.bytes)
                : asset::tilemap::DeserializationResult{};
        if (!artifactRoundTrip
            || artifactRoundTrip.artifact.identity != firstCompile.artifact.identity)
        {
            return ContractFailure::artifact_round_trip;
        }

        const MutationResult undone = document.undo();
        if (!undone || document.metrics().occupied_cells != 0u)
            return ContractFailure::undo_redo;
        const MutationResult redone = document.redo();
        if (!redone || document.metrics().occupied_cells != 3u)
            return ContractFailure::undo_redo;

        const std::array<CellEdit, 1> erase{{{{1u, 0u}, {}}}};
        if (!document.paint_cells(layer.layer, erase)
            || document.metrics().occupied_cells != 2u)
        {
            return ContractFailure::erase_cells;
        }
        if (!document.undo() || document.metrics().occupied_cells != 3u)
            return ContractFailure::undo_redo;

        const MutationResult object = document.create_object({
            .name = "PlayerSpawn",
            .type = "spawn",
            .position = {2.0f, 2.0f}});
        if (!object || !document.remove_object(object.object))
            return ContractFailure::stale_handle;
        if (document.set_object_properties(
                object.object,
                {.name = "Moved", .type = "spawn"}).code
            != ResultCode::stale_handle)
        {
            return ContractFailure::stale_handle;
        }

        LayerDescriptor locked = *document.layer(layer.layer);
        locked.locked = true;
        if (!document.set_layer_properties(layer.layer, locked))
            return ContractFailure::locked_layer;
        if (document.paint_cells(layer.layer, edits).code != ResultCode::locked_layer)
            return ContractFailure::locked_layer;
        if (!document.undo())
            return ContractFailure::locked_layer;
        if (document.paint_cells(
                layer.layer,
                std::array<CellEdit, 1>{{{{16u, 0u}, {}}}}).code
            != ResultCode::out_of_bounds)
        {
            return ContractFailure::out_of_bounds;
        }
        if (document.remove_palette_entry(palette.palette_entry).code
            != ResultCode::referenced_resource)
        {
            return ContractFailure::referenced_resource;
        }

        DocumentLimits oneOperation{};
        oneOperation.maximum_operations = 1u;
        const CreateResult bounded = Document::create(
            {1u, 1u},
            {.name = "Bounded", .extent_tiles = {4u, 4u},
             .default_chunk_extent = {4u, 4u}},
            oneOperation);
        if (!bounded || !bounded.document->create_tile_set({
                .name = "Only",
                .texture = texture,
                .tile_extent = {16u, 16u},
                .grid = {4u, 1u},
                .tile_count = 4u}))
        {
            return ContractFailure::history_bound;
        }
        if (bounded.document->create_tile_set({
                .name = "Rejected",
                .texture = TextureDependency{
                    .logical_path = "Assets/Textures/other.ppm",
                    .artifact_key = {31u, 32u, 33u, 34u},
                    .project_key = 21u,
                    .asset_key = 32u,
                    .artifact_revision = 33u,
                    .stable_material_key = 34u,
                    .texture_extent = {64u, 16u}},
                .tile_extent = {16u, 16u},
                .grid = {4u, 1u},
                .tile_count = 4u}).code != ResultCode::history_limit_exceeded)
        {
            return ContractFailure::history_bound;
        }

        std::vector<std::byte> malformed = source.bytes;
        malformed.pop_back();
        if (Document::deserialize(malformed))
            return ContractFailure::malformed_source_accepted;
        return ContractFailure::none;
    }
}

#if defined(EPOCH_AUTHORING_TILEMAP_CONTRACT_MAIN)
int main()
{
    return epochengine::authoring::tilemap::run_contract()
        == epochengine::authoring::tilemap::ContractFailure::none ? 0 : 1;
}
#endif
