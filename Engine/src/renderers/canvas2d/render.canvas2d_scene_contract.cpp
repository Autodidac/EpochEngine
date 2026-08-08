/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <utility>

module render.canvas2d_scene;

import render.canvas2d_cpu;

namespace epochengine::canvas2d::scene_content
{
    SceneContractFailure run_scene_content_contract() noexcept
    {
        const SceneMetrics before = metrics();
        int ownerA = 0;
        int ownerB = 0;
        (void)retire(&ownerA);
        (void)retire(&ownerB);

        SceneContent invalid{};
        invalid.source_revision = 0;
        if (publish(nullptr, {}).code != SceneCode::invalid_owner
            || publish(&ownerA, std::move(invalid)).code
                != SceneCode::invalid_content)
        {
            return SceneContractFailure::invalid_rejection;
        }

        SceneContent content{};
        content.project.logical_canvas = {32, 18};
        content.project.pixels_per_world_unit = 1.0f;
        content.camera.pixels_per_world_unit = 1.0f;
        content.camera.y_axis = CanvasYAxis::up;
        content.source_revision = 1;
        SpriteMaterialDeclaration material{};
        material.stable_key = 1;
        material.source = SpriteSourceKind::solid_color;
        material.alpha = SpriteAlphaMode::opaque;
        material.color_space = SpriteColorSpace::linear;
        SpriteSubmission sprite{};
        sprite.sprite = {0, 1};
        sprite.material = material;
        sprite.transform.size = {8.0f, 6.0f};
        sprite.tint = {1.0f, 0.1f, 0.1f, 1.0f};
        sprite.stable_sequence = 1;
        content.sprites.push_back(sprite);

        const PublicationResult first = publish(&ownerA, content);
        if (!first || first.code != SceneCode::ready)
            return SceneContractFailure::first_publication;
        const AcquiredScene oldReader = acquire(&ownerA);
        if (!oldReader || oldReader.content_hash != first.content_hash)
            return SceneContractFailure::acquisition;
        const Canvas2DFramePlan frame = compile(oldReader, {64, 36});
        if (!frame || frame.frame_sequence != first.frame_sequence)
            return SceneContractFailure::frame_compilation;
        const cpu::RasterResult raster = cpu::rasterize(frame);
        if (!raster || raster.canvas_hash == 0
            || raster.metrics.fragments_shaded == 0)
        {
            return SceneContractFailure::cpu_raster;
        }

        const PublicationResult unchanged = publish(&ownerA, content);
        if (!unchanged || unchanged.code != SceneCode::unchanged
            || unchanged.generation != first.generation
            || unchanged.frame_sequence != first.frame_sequence)
        {
            return SceneContractFailure::unchanged_publication;
        }

        SceneContent replacement = content;
        replacement.source_revision = 2;
        replacement.sprites.front().tint = {0.1f, 1.0f, 0.1f, 1.0f};
        const PublicationResult replaced = publish(&ownerA, replacement);
        const AcquiredScene newReader = acquire(&ownerA);
        if (!replaced || replaced.code != SceneCode::ready || !newReader
            || replaced.generation == first.generation
            || replaced.content_hash == first.content_hash)
        {
            return SceneContractFailure::replacement;
        }
        if (!oldReader || oldReader.content->source_revision != 1
            || oldReader.content->sprites.front().tint
                != LinearColor{1.0f, 0.1f, 0.1f, 1.0f})
        {
            return SceneContractFailure::immutable_reader;
        }

        const PublicationResult other = publish(&ownerB, content);
        const AcquiredScene otherReader = acquire(&ownerB);
        if (!other || !otherReader || otherReader.generation == newReader.generation)
            return SceneContractFailure::owner_isolation;
        if (retire(&ownerA) != SceneCode::ready
            || retire(&ownerA) != SceneCode::not_found
            || acquire(&ownerA))
        {
            return SceneContractFailure::retirement;
        }
        (void)retire(&ownerB);

        const SceneMetrics after = metrics();
        if (after.active_slots != before.active_slots
            || after.publications < before.publications + 3
            || after.unchanged_publications < before.unchanged_publications + 1
            || after.acquisitions < before.acquisitions + 3
            || after.retirements < before.retirements + 2)
        {
            return SceneContractFailure::metrics;
        }
        return SceneContractFailure::none;
    }
}
