/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#include <cstdint>
#include <string>
#include <string_view>

import authoring.texture;

namespace epochengine::authoring::texture::contract
{
    namespace
    {
        [[nodiscard]] TextureDocument make_document(
            DocumentHandle handle = { 7, 1 },
            HistoryPolicy history = {},
            DocumentLimits limits = {})
        {
            CanvasDescriptor canvas{};
            canvas.width = 64;
            canvas.height = 64;
            canvas.tile_extent = 16;
            canvas.mip_count = 3;
            return TextureDocument{
                handle,
                BranchIdentity{ 9, 4 },
                canvas,
                history,
                limits
            };
        }

        [[nodiscard]] TemporalPoint at(std::int64_t tick) noexcept
        {
            return TemporalPoint{ BranchIdentity{ 9, 4 }, tick };
        }

        [[nodiscard]] StrokeDescriptor red_stamp(
            LayerHandle layer,
            std::int64_t x,
            std::int64_t y)
        {
            StrokeDescriptor stroke{};
            stroke.target = layer;
            stroke.color = PixelRgba8{ 255, 32, 16, 255 };
            stroke.radius_subpixels = 512;
            stroke.seed = 42;
            stroke.samples.push_back(StrokeSample{
                .x_subpixels = x * 256 + 128,
                .y_subpixels = y * 256 + 128
            });
            return stroke;
        }
    }

    [[nodiscard]] int run_texture_document_contract()
    {
        TextureDocument invalid{
            DocumentHandle{},
            BranchIdentity{ 9, 4 }
        };
        if (invalid.valid()
            || invalid.create_layer({}, 0, at(1)).code
                != ResultCode::invalid_document)
        {
            return 1;
        }

        TextureDocument first = make_document();
        TextureDocument second = make_document();
        if (!first.valid()
            || !second.valid()
            || first.revision().content.empty()
            || content_hash_hex(first.revision().content).size() != 64)
        {
            return 2;
        }

        LayerDescriptor baseDescriptor{};
        baseDescriptor.name = "Base Color";
        const MutationResult firstLayer =
            first.create_layer(baseDescriptor, 0, at(1));
        const MutationResult secondLayer =
            second.create_layer(baseDescriptor, 0, at(1));
        if (!firstLayer
            || !secondLayer
            || !firstLayer.layer
            || firstLayer.layer != secondLayer.layer
            || first.revision().content != second.revision().content)
        {
            return 3;
        }

        const ContentHash clearLayerRevision = first.revision().content;
        const StrokeDescriptor stroke = red_stamp(
            firstLayer.layer,
            15,
            15);
        const MutationResult painted =
            first.apply_stroke(stroke, at(2));
        const MutationResult paintedSecond =
            second.apply_stroke(stroke, at(2));
        if (!painted
            || !paintedSecond
            || painted.affected_tiles != 4
            || first.revision().content != second.revision().content
            || first.pixel(firstLayer.layer, 0, 15, 15).r == 0)
        {
            return 4;
        }

        const DocumentMetrics paintedMetrics = first.metrics();
        if (paintedMetrics.sparse_tile_count != 4
            || paintedMetrics.canonical_tile_bytes
                != 4ull * 16ull * 16ull * 4ull
            || paintedMetrics.operation_count != 2
            || paintedMetrics.applied_operation_count != 2)
        {
            return 5;
        }

        const auto tile = first.tile(
            firstLayer.layer,
            TileCoordinate{ 0, 0, 0 });
        const auto implicitTile = first.tile(
            firstLayer.layer,
            TileCoordinate{ 1, 0, 0 });
        if (!tile
            || tile->implicit_clear()
            || tile->content.empty()
            || !implicitTile
            || !implicitTile->implicit_clear()
            || implicitTile->content.empty())
        {
            return 6;
        }

        const ContentHash paintedRevision = first.revision().content;
        const MutationResult undone = first.undo(at(3));
        if (!undone
            || first.revision().content != clearLayerRevision
            || first.metrics().sparse_tile_count != 0
            || !first.can_redo())
        {
            return 7;
        }
        const MutationResult redone = first.redo(at(4));
        if (!redone
            || first.revision().content != paintedRevision
            || first.metrics().sparse_tile_count != 4)
        {
            return 8;
        }

        const CheckpointResult checkpoint =
            first.capture_checkpoint(at(4));
        if (!checkpoint
            || !checkpoint.checkpoint.handle
            || checkpoint.checkpoint.retained_bytes == 0)
        {
            return 9;
        }

        LayerDescriptor changed = baseDescriptor;
        changed.name = "Paint";
        changed.opacity = 32'768;
        const MutationResult changedProperties =
            first.set_layer_properties(firstLayer.layer, changed, at(5));
        if (!changedProperties
            || first.revision().content == paintedRevision)
        {
            return 10;
        }
        const MutationResult restored = first.restore_checkpoint(
            checkpoint.checkpoint.handle,
            at(6));
        if (!restored
            || first.revision().content != paintedRevision
            || !first.layer(firstLayer.layer)
            || first.layer(firstLayer.layer)->descriptor.name
                != baseDescriptor.name
            || first.layer(firstLayer.layer)->descriptor.opacity
                != baseDescriptor.opacity)
        {
            return 11;
        }

        const auto summaries = first.operation_summaries();
        const auto records = first.operation_records();
        if (summaries.size() != records.size()
            || summaries.size() != 3
            || summaries[0].kind != std::string_view{ "layer_created" }
            || summaries[1].kind
                != std::string_view{ "texture_stroke_applied" }
            || records[1].retained_bytes == 0)
        {
            return 12;
        }

        TextureDocument layerOrder = make_document({ 8, 1 });
        const LayerHandle bottom =
            layerOrder.create_layer(
                LayerDescriptor{ .name = "Bottom" },
                0,
                at(1)).layer;
        const LayerHandle top =
            layerOrder.create_layer(
                LayerDescriptor{ .name = "Top" },
                1,
                at(2)).layer;
        if (!bottom
            || !top
            || !layerOrder.move_layer(top, 0, at(3))
            || layerOrder.layers()[0].handle != top
            || !layerOrder.undo(at(4))
            || layerOrder.layers()[0].handle != bottom)
        {
            return 13;
        }

        const MutationResult removed =
            layerOrder.remove_layer(top, at(5));
        if (!removed
            || layerOrder.layer(top)
            || layerOrder.set_layer_properties(
                    top,
                    LayerDescriptor{ .name = "Stale" },
                    at(6)).code != ResultCode::stale_handle)
        {
            return 14;
        }
        if (!layerOrder.undo(at(6)) || !layerOrder.layer(top))
            return 15;

        HistoryPolicy shortHistory{};
        shortHistory.mode = HistoryMode::semantic_operations;
        shortHistory.maximum_operations = 1;
        TextureDocument boundedHistory =
            make_document({ 9, 1 }, shortHistory);
        const MutationResult onlyOperation =
            boundedHistory.create_layer(
                LayerDescriptor{ .name = "Only" },
                0,
                at(1));
        if (!onlyOperation
            || boundedHistory.set_layer_properties(
                    onlyOperation.layer,
                    LayerDescriptor{ .name = "Too Many" },
                    at(2)).code
                != ResultCode::history_operation_limit_exceeded)
        {
            return 16;
        }

        DocumentLimits oneTileLimits{};
        oneTileLimits.maximum_sparse_tiles = 1;
        oneTileLimits.maximum_tiles_per_stroke = 1;
        TextureDocument boundedTiles = make_document(
            { 10, 1 },
            HistoryPolicy{},
            oneTileLimits);
        const LayerHandle boundedLayer =
            boundedTiles.create_layer(
                LayerDescriptor{ .name = "Bounded" },
                0,
                at(1)).layer;
        StrokeDescriptor crossing = red_stamp(boundedLayer, 15, 15);
        if (boundedTiles.apply_stroke(crossing, at(2)).code
            != ResultCode::stroke_tile_limit_exceeded)
        {
            return 17;
        }
        if (boundedTiles.metrics().sparse_tile_count != 0)
            return 18;

        TextureCompileProfile compileProfile{};
        compileProfile.format = ArtifactFormat::bc7_rgba;
        compileProfile.mipmaps = MipmapPolicy::generate_box_filter;
        compileProfile.compiler_schema_version = 3;
        const DocumentRevision beforeCompile = first.revision();
        const CompiledTextureArtifactIdentity artifact =
            first.compiled_artifact_identity(compileProfile);
        const CompiledTextureArtifactIdentity repeatedArtifact =
            first.compiled_artifact_identity(compileProfile);
        if (!artifact
            || artifact.key != repeatedArtifact.key
            || artifact.mip_count != 7
            || artifact.estimated_artifact_bytes == 0
            || first.revision() != beforeCompile)
        {
            return 19;
        }

        PhysicalResidencyCapabilities atlasCapabilities{};
        atlasCapabilities.atlas_regions = true;
        PhysicalResidencyPolicy atlasPolicy{};
        atlasPolicy.preferred = PhysicalResidencyKind::atlas_region;
        const PhysicalResidencyPlan atlasPlan =
            plan_physical_residency(
                artifact,
                atlasCapabilities,
                atlasPolicy);
        if (!atlasPlan
            || atlasPlan.tiers.size() != 1
            || atlasPlan.tiers[0].kind
                != PhysicalResidencyKind::atlas_region
            || !atlasPlan.disposable
            || atlasPlan.cache_key.empty()
            || first.revision() != beforeCompile)
        {
            return 20;
        }

        PhysicalResidencyCapabilities sparseCapabilities{};
        sparseCapabilities.bindless_images = true;
        sparseCapabilities.sparse_images = true;
        sparseCapabilities.available_bindless_descriptors = 128;
        sparseCapabilities.maximum_resident_bytes = 64 * 1024;
        PhysicalResidencyPolicy sparsePolicy{};
        sparsePolicy.preferred =
            PhysicalResidencyKind::hybrid_sparse_tail;
        sparsePolicy.allow_atlas_for_small_textures = false;
        sparsePolicy.target_resident_bytes = 64 * 1024;
        sparsePolicy.minimum_streamed_mip_count = 2;
        const PhysicalResidencyPlan sparsePlan =
            plan_physical_residency(
                artifact,
                sparseCapabilities,
                sparsePolicy);
        if (!sparsePlan
            || sparsePlan.tiers.size() != 2
            || sparsePlan.tiers[0].kind
                != PhysicalResidencyKind::sparse_pages
            || sparsePlan.tiers[1].kind
                != PhysicalResidencyKind::standalone_image
            || sparsePlan.estimated_resident_bytes
                > sparsePolicy.target_resident_bytes)
        {
            return 21;
        }

        if (std::string_view{
                result_code_name(ResultCode::stale_handle) }
                != "stale_handle"
            || std::string_view{
                residency_kind_name(
                    PhysicalResidencyKind::hybrid_sparse_tail) }
                != "hybrid_sparse_tail"
            || std::string_view{
                residency_plan_status_name(
                    ResidencyPlanStatus::ready_for_allocation) }
                != "ready_for_allocation")
        {
            return 22;
        }

        HistoryPolicy automaticPolicy{};
        automaticPolicy.checkpoint_interval_operations = 1;
        automaticPolicy.maximum_checkpoints = 1;
        TextureDocument automaticCheckpoints = make_document(
            { 11, 1 },
            automaticPolicy);
        const MutationResult automaticLayer =
            automaticCheckpoints.create_layer(
                LayerDescriptor{ .name = "Automatic" },
                0,
                at(1));
        LayerDescriptor automaticChanged{};
        automaticChanged.name = "Automatic Changed";
        const MutationResult skippedAutomatic =
            automaticCheckpoints.set_layer_properties(
                automaticLayer.layer,
                automaticChanged,
                at(2));
        if (!automaticLayer.automatic_checkpoint_captured
            || skippedAutomatic.automatic_checkpoint_captured
            || automaticCheckpoints.metrics().checkpoint_count != 1
            || automaticCheckpoints.metrics().automatic_checkpoints_skipped
                != 1)
        {
            return 23;
        }

        TextureDocument checkpointTime = make_document({ 12, 1 });
        const LayerHandle checkpointLayer =
            checkpointTime.create_layer(
                LayerDescriptor{ .name = "Temporal" },
                0,
                at(1)).layer;
        if (!checkpointTime.capture_checkpoint(at(10))
            || checkpointTime.current_time().tick != 10
            || checkpointTime.set_layer_properties(
                    checkpointLayer,
                    LayerDescriptor{ .name = "Regression" },
                    at(9)).code != ResultCode::temporal_regression)
        {
            return 24;
        }

        HistoryPolicy disabledPolicy{};
        disabledPolicy.mode = HistoryMode::disabled;
        TextureDocument noHistory = make_document(
            { 13, 1 },
            disabledPolicy);
        if (!noHistory.create_layer(
                LayerDescriptor{ .name = "No History" },
                0,
                at(1))
            || noHistory.metrics().operation_count != 0
            || noHistory.can_undo()
            || noHistory.undo(at(2)).code != ResultCode::nothing_to_undo)
        {
            return 25;
        }

        CompiledTextureArtifactIdentity inconsistentArtifact = artifact;
        ++inconsistentArtifact.estimated_artifact_bytes;
        if (plan_physical_residency(
                inconsistentArtifact,
                atlasCapabilities,
                atlasPolicy).status != ResidencyPlanStatus::invalid_artifact)
        {
            return 26;
        }

        return 0;
    }
}

#if defined(EPOCH_AUTHORING_TEXTURE_CONTRACT_MAIN)
int main()
{
    return
        epochengine::authoring::texture::contract::
            run_texture_document_contract();
}
#endif
