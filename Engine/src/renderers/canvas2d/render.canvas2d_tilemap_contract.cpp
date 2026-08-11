/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cmath>
#include <algorithm>
#include <limits>
#include <string>

module render.canvas2d_tilemap;

namespace epochengine::canvas2d::tilemap_runtime
{
    ContractFailure run_contract() noexcept
    {
        if (asset::tilemap::run_artifact_contract()
            != asset::tilemap::ArtifactContractFailure::none)
        {
            return ContractFailure::artifact_contract;
        }

        asset::tilemap::CompiledTileMapArtifact artifact{};
        artifact.name = "VisibleMap";
        artifact.extent_tiles = {16u, 8u};
        artifact.default_chunk_extent = {4u, 4u};
        artifact.identity.source_revision = {{{1u, 2u, 3u, 4u}}, 5u};
        artifact.dependencies.push_back({
            .logical_path = "Assets/Textures/tiles.ppm",
            .artifact_key = {11u, 12u, 13u, 14u},
            .project_key = 21u,
            .asset_key = 22u,
            .artifact_revision = 23u,
            .stable_material_key = 24u,
            .texture_extent = {64u, 16u}});
        artifact.tile_sets.push_back({
            .id = {101u},
            .dependency_index = 0u,
            .tile_extent = {16u, 16u},
            .grid = {4u, 1u},
            .tile_count = 4u});
        artifact.palette.push_back({
            .id = {201u},
            .tile_set_index = 0u,
            .local_tile = 1u,
            .collision = {
                .kind = asset::tilemap::CollisionKind::full_cell,
                .local_bounds = {0.0f, 0.0f, 1.0f, 1.0f},
                .layer_bits = 1u,
                .mask_bits = 3u},
            .animation_frame_count = 2u,
            .animation_rate_millihertz = 1'000u});
        artifact.layers.push_back({
            .id = {301u},
            .name = "World",
            .extent_tiles = {16u, 8u},
            .chunk_extent = {4u, 4u},
            .world_origin = {-2.0f, -1.0f},
            .tile_world_extent = {1.0f, 1.0f},
            .collision_source = true});
        artifact.chunks.push_back({
            .layer_index = 0u,
            .coordinate = {0u, 0u},
            .extent = {4u, 4u},
            .revision = 1u,
            .world_bounds = {-2.0f, -1.0f, 4.0f, 4.0f},
            .first_cell = 0u,
            .cell_count = 2u});
        artifact.chunks.push_back({
            .layer_index = 0u,
            .coordinate = {3u, 0u},
            .extent = {4u, 4u},
            .revision = 1u,
            .world_bounds = {10.0f, -1.0f, 4.0f, 4.0f},
            .first_cell = 2u,
            .cell_count = 1u});
        artifact.cells.push_back({
            .coordinate = {0u, 0u},
            .palette_index = 0u});
        artifact.cells.push_back({
            .coordinate = {1u, 0u},
            .palette_index = 0u,
            .transform = asset::tilemap::TileTransform::flip_x});
        artifact.cells.push_back({
            .coordinate = {12u, 0u},
            .palette_index = 0u});
        for (const auto coordinate : {
                 asset::tilemap::UInt2{0u, 0u},
                 asset::tilemap::UInt2{1u, 0u},
                 asset::tilemap::UInt2{12u, 0u}})
        {
            artifact.collision.push_back({
                .layer = {301u},
                .palette_entry = {201u},
                .cell_coordinate = coordinate,
                .kind = asset::tilemap::CollisionKind::full_cell,
                .world_bounds = {
                    -2.0f + static_cast<float>(coordinate.x),
                    -1.0f,
                    1.0f,
                    1.0f},
                .layer_bits = 1u,
                .mask_bits = 3u});
        }
        artifact.objects.push_back({
            .id = {401u},
            .name = "Near",
            .type = "spawn",
            .position = {0.0f, 0.0f}});
        artifact.objects.push_back({
            .id = {402u},
            .name = "Far",
            .type = "marker",
            .position = {12.0f, 0.0f}});
        artifact.identity.key =
            asset::tilemap::compiled_tilemap_payload_content(artifact);
        if (!asset::tilemap::validate(artifact))
            return ContractFailure::artifact_contract;

        const RectF fullBounds = artifact_world_bounds(artifact);
        if (fullBounds != RectF{-2.0f, -1.0f, 16.0f, 8.0f})
            return ContractFailure::world_bounds;
        const ViewRequest view{
            .world_bounds = {-3.0f, -2.0f, 7.0f, 6.0f},
            .simulated_milliseconds = 1'000u};
        const VisibleCompilation visible = compile_visible(artifact, view);
        if (!visible || visible.code != CompileCode::ready)
            return ContractFailure::visible_compile;
        if (visible.metrics.visible_chunks != 1u
            || visible.metrics.culled_chunks != 1u
            || visible.sprites.size() != 2u)
        {
            return ContractFailure::culling;
        }
        if (visible.sprites[0].stable_sequence
                == visible.sprites[1].stable_sequence
            || !less(
                sort_key(visible.sprites[0]),
                sort_key(visible.sprites[1])))
        {
            return ContractFailure::ordering;
        }
        if (visible.sprites[0].material.logical_texture
                != LogicalTextureReference{22u, 23u}
            || visible.sprites[0].material.stable_key != 24u)
        {
            return ContractFailure::material_identity;
        }
        if (visible.sprites[0].source_uv
                != RectF{0.5f, 0.0f, 0.25f, 1.0f})
        {
            return ContractFailure::animation;
        }
        const auto flipped = std::count_if(
            visible.sprites.begin(), visible.sprites.end(),
            [](const SpriteSubmission& sprite)
            {
                return sprite.flip_x
                    && sprite.transform.rotation_radians == 0.0f;
            });
        if (flipped != 1)
        {
            return ContractFailure::transforms;
        }
        if (visible.collision.size() != 2u)
            return ContractFailure::collision_filter;
        if (visible.objects.size() != 1u || visible.objects[0].name != "Near")
            return ContractFailure::object_filter;

        CompileLimits oneSprite{};
        oneSprite.maximum_visible_sprites = 1u;
        const VisibleCompilation bounded = compile_visible(
            artifact, view, oneSprite);
        if (!bounded || bounded.code != CompileCode::partial
            || bounded.sprites.size() != 1u
            || bounded.metrics.capacity_rejections == 0u)
        {
            return ContractFailure::capacity_bound;
        }
        const VisibleCompilation invalid = compile_visible(
            artifact,
            {.world_bounds = {
                0.0f, 0.0f,
                (std::numeric_limits<float>::quiet_NaN)(), 1.0f}});
        if (invalid.code != CompileCode::invalid_view)
            return ContractFailure::invalid_view_accepted;
        return ContractFailure::none;
    }
}
