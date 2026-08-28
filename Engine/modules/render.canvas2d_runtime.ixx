/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <string_view>
#include <utility>

export module render.canvas2d_runtime;

import render.canvas2d;
import render.canvas2d_cpu;
import render.canvas2d_scene;

export namespace epochengine::canvas2d::runtime
{
    enum class PrepareCode : std::uint8_t
    {
        ready,
        reused,
        invalid_owner,
        missing_scene,
        invalid_output,
        frame_compile_failed,
        raster_failed,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view prepare_code_name(
        PrepareCode code) noexcept
    {
        switch (code)
        {
        case PrepareCode::ready: return "ready";
        case PrepareCode::reused: return "reused";
        case PrepareCode::invalid_owner: return "invalid_owner";
        case PrepareCode::missing_scene: return "missing_scene";
        case PrepareCode::invalid_output: return "invalid_output";
        case PrepareCode::frame_compile_failed: return "frame_compile_failed";
        case PrepareCode::raster_failed: return "raster_failed";
        case PrepareCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct SessionMetrics final
    {
        std::uint64_t requests{};
        std::uint64_t prepared_frames{};
        std::uint64_t reused_frames{};
        std::uint64_t scene_misses{};
        std::uint64_t compile_failures{};
        std::uint64_t raster_failures{};
        std::uint64_t resets{};
        std::uint64_t last_generation{};
        std::uint64_t last_frame_sequence{};
        std::uint64_t last_content_hash{};
        std::uint64_t last_canvas_hash{};
    };

    struct PreparedSceneView final
    {
        PrepareCode code{PrepareCode::invalid_owner};
        const Canvas2DFramePlan* frame{};
        const cpu::RasterResult* raster{};
        std::uint64_t generation{};
        std::uint64_t content_hash{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return (code == PrepareCode::ready || code == PrepareCode::reused)
                && frame && raster && static_cast<bool>(*frame)
                && static_cast<bool>(*raster)
                && generation != 0 && content_hash != 0;
        }
    };

    class SceneRasterSession final
    {
    public:
        SceneRasterSession() = default;
        SceneRasterSession(const SceneRasterSession&) = delete;
        SceneRasterSession& operator=(const SceneRasterSession&) = delete;
        SceneRasterSession(SceneRasterSession&&) = delete;
        SceneRasterSession& operator=(SceneRasterSession&&) = delete;

        [[nodiscard]] PreparedSceneView prepare(
            const void* owner,
            CanvasExtent output,
            const CanvasLimits& canvasLimits = {},
            const cpu::RasterLimits& rasterLimits = {},
            const cpu::RasterPolicy& rasterPolicy = {}) noexcept
        {
            ++metrics_.requests;
            if (!owner)
                return {PrepareCode::invalid_owner};
            if (output.empty())
                return {PrepareCode::invalid_output};

            const scene_content::AcquiredScene acquired =
                scene_content::acquire(owner);
            if (!acquired)
            {
                ++metrics_.scene_misses;
                reset_cached_state();
                return {PrepareCode::missing_scene};
            }

            if (scene_ && frame_ && raster_
                && scene_.generation == acquired.generation
                && scene_.frame_sequence == acquired.frame_sequence
                && scene_.content_hash == acquired.content_hash
                && output_ == output
                && canvas_limits_ == canvasLimits
                && raster_limits_ == rasterLimits
                && raster_policy_ == rasterPolicy)
            {
                ++metrics_.reused_frames;
                return view(PrepareCode::reused);
            }

            try
            {
                Canvas2DFramePlan nextFrame = scene_content::compile(
                    acquired,
                    output,
                    canvasLimits);
                if (!nextFrame)
                {
                    ++metrics_.compile_failures;
                    return {PrepareCode::frame_compile_failed};
                }

                cpu::RasterResult nextRaster = cpu::rasterize(
                    nextFrame,
                    acquired.content->resources.bindings,
                    rasterLimits,
                    rasterPolicy);
                if (!nextRaster)
                {
                    ++metrics_.raster_failures;
                    return {PrepareCode::raster_failed};
                }

                scene_ = acquired;
                frame_ = std::move(nextFrame);
                raster_ = std::move(nextRaster);
                output_ = output;
                canvas_limits_ = canvasLimits;
                raster_limits_ = rasterLimits;
                raster_policy_ = rasterPolicy;
                ++metrics_.prepared_frames;
                metrics_.last_generation = scene_.generation;
                metrics_.last_frame_sequence = frame_.frame_sequence;
                metrics_.last_content_hash = scene_.content_hash;
                metrics_.last_canvas_hash = raster_.canvas_hash;
                return view(PrepareCode::ready);
            }
            catch (...)
            {
                return {PrepareCode::allocation_failure};
            }
        }

        void reset() noexcept
        {
            ++metrics_.resets;
            reset_cached_state();
        }

        [[nodiscard]] const SessionMetrics& metrics() const noexcept
        {
            return metrics_;
        }

    private:
        [[nodiscard]] PreparedSceneView view(PrepareCode code) const noexcept
        {
            return {
                code,
                &frame_,
                &raster_,
                scene_.generation,
                scene_.content_hash
            };
        }

        void reset_cached_state() noexcept
        {
            scene_ = {};
            frame_ = {};
            raster_ = {};
            output_ = {};
            metrics_.last_generation = 0;
            metrics_.last_frame_sequence = 0;
            metrics_.last_content_hash = 0;
            metrics_.last_canvas_hash = 0;
        }

        scene_content::AcquiredScene scene_{};
        Canvas2DFramePlan frame_{};
        cpu::RasterResult raster_{};
        CanvasExtent output_{};
        CanvasLimits canvas_limits_{};
        cpu::RasterLimits raster_limits_{};
        cpu::RasterPolicy raster_policy_{};
        SessionMetrics metrics_{};
    };

    enum class RuntimeContractFailure : std::uint8_t
    {
        none,
        invalid_owner,
        missing_scene,
        publication,
        first_prepare,
        reuse,
        limit_change,
        resize,
        revision_only_replacement,
        replacement,
        retirement,
        metrics
    };

    [[nodiscard]] constexpr std::string_view runtime_contract_failure_name(
        RuntimeContractFailure failure) noexcept
    {
        switch (failure)
        {
        case RuntimeContractFailure::none: return "pass";
        case RuntimeContractFailure::invalid_owner: return "invalid_owner";
        case RuntimeContractFailure::missing_scene: return "missing_scene";
        case RuntimeContractFailure::publication: return "publication";
        case RuntimeContractFailure::first_prepare: return "first_prepare";
        case RuntimeContractFailure::reuse: return "reuse";
        case RuntimeContractFailure::limit_change: return "limit_change";
        case RuntimeContractFailure::resize: return "resize";
        case RuntimeContractFailure::revision_only_replacement:
            return "revision_only_replacement";
        case RuntimeContractFailure::replacement: return "replacement";
        case RuntimeContractFailure::retirement: return "retirement";
        case RuntimeContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] inline RuntimeContractFailure
        run_scene_raster_session_contract() noexcept
    {
        SceneRasterSession session{};
        int owner{};
        if (session.prepare(nullptr, {16, 16}).code
            != PrepareCode::invalid_owner)
        {
            return RuntimeContractFailure::invalid_owner;
        }
        if (session.prepare(&owner, {16, 16}).code
            != PrepareCode::missing_scene)
        {
            return RuntimeContractFailure::missing_scene;
        }

        scene_content::SceneContent content{};
        content.project.logical_canvas = {8, 8};
        content.project.pixels_per_world_unit = 1.0f;
        content.camera.pixels_per_world_unit = 1.0f;
        content.source_revision = 1;
        SpriteSubmission sprite{};
        sprite.sprite = {0, 1};
        sprite.material.stable_key = 1;
        sprite.material.source = SpriteSourceKind::solid_color;
        sprite.material.alpha = SpriteAlphaMode::opaque;
        sprite.transform = {{0.0f, 0.0f}, {4.0f, 4.0f}};
        sprite.tint = {0.2f, 0.7f, 1.0f, 1.0f};
        sprite.stable_sequence = 1;
        content.sprites.push_back(sprite);
        const auto firstPublication = scene_content::publish(&owner, content);
        if (!firstPublication)
            return RuntimeContractFailure::publication;

        const PreparedSceneView first = session.prepare(&owner, {16, 16});
        if (!first || first.code != PrepareCode::ready
            || first.raster->canvas_hash == 0)
        {
            (void)scene_content::retire(&owner);
            return RuntimeContractFailure::first_prepare;
        }
        const std::uint64_t firstHash = first.raster->canvas_hash;
        const PreparedSceneView reused = session.prepare(&owner, {16, 16});
        if (!reused || reused.code != PrepareCode::reused
            || reused.raster->canvas_hash != firstHash)
        {
            (void)scene_content::retire(&owner);
            return RuntimeContractFailure::reuse;
        }

        cpu::RasterLimits constrained{};
        constrained.maximum_canvas_pixels = 63u;
        const PreparedSceneView constrainedResult = session.prepare(
            &owner,
            {16, 16},
            {},
            constrained);
        const PreparedSceneView reuseAfterRefusal = session.prepare(
            &owner,
            {16, 16});
        if (constrainedResult.code != PrepareCode::raster_failed
            || !reuseAfterRefusal
            || reuseAfterRefusal.code != PrepareCode::reused
            || reuseAfterRefusal.raster->canvas_hash != firstHash)
        {
            (void)scene_content::retire(&owner);
            return RuntimeContractFailure::limit_change;
        }

        const PreparedSceneView resized = session.prepare(&owner, {24, 20});
        if (!resized || resized.code != PrepareCode::ready
            || resized.raster->canvas_hash != firstHash
            || resized.frame->compose.viewport.output_surface
                != CanvasExtent{24, 20})
        {
            (void)scene_content::retire(&owner);
            return RuntimeContractFailure::resize;
        }
        const PreparedSceneView resizedReuse = session.prepare(&owner, {24, 20});
        if (!resizedReuse || resizedReuse.code != PrepareCode::reused
            || resizedReuse.raster->canvas_hash != firstHash)
        {
            (void)scene_content::retire(&owner);
            return RuntimeContractFailure::resize;
        }
        const std::uint64_t resizedGeneration = resizedReuse.generation;
        const std::uint64_t resizedFrameSequence =
            resizedReuse.frame->frame_sequence;
        const std::uint64_t resizedContentHash = resizedReuse.content_hash;

        scene_content::SceneContent revisionOnly = content;
        revisionOnly.source_revision = 2;
        if (!scene_content::publish(&owner, std::move(revisionOnly)))
        {
            (void)scene_content::retire(&owner);
            return RuntimeContractFailure::revision_only_replacement;
        }
        const PreparedSceneView revisionReplaced = session.prepare(
            &owner,
            {24, 20});
        if (!revisionReplaced
            || revisionReplaced.code != PrepareCode::ready
            || revisionReplaced.content_hash != resizedContentHash
            || revisionReplaced.raster->canvas_hash != firstHash
            || revisionReplaced.generation == resizedGeneration
            || revisionReplaced.frame->frame_sequence
                == resizedFrameSequence)
        {
            (void)scene_content::retire(&owner);
            return RuntimeContractFailure::revision_only_replacement;
        }

        content.source_revision = 3;
        content.sprites.front().tint = {1.0f, 0.2f, 0.1f, 1.0f};
        if (!scene_content::publish(&owner, std::move(content)))
        {
            (void)scene_content::retire(&owner);
            return RuntimeContractFailure::replacement;
        }
        const PreparedSceneView replaced = session.prepare(&owner, {24, 20});
        if (!replaced || replaced.code != PrepareCode::ready
            || replaced.raster->canvas_hash == firstHash)
        {
            (void)scene_content::retire(&owner);
            return RuntimeContractFailure::replacement;
        }

        if (scene_content::retire(&owner) != scene_content::SceneCode::ready
            || session.prepare(&owner, {24, 20}).code
                != PrepareCode::missing_scene)
        {
            return RuntimeContractFailure::retirement;
        }
        const SessionMetrics& metrics = session.metrics();
        if (metrics.requests != 11 || metrics.prepared_frames != 4
            || metrics.reused_frames != 3 || metrics.scene_misses != 2
            || metrics.compile_failures != 0 || metrics.raster_failures != 1)
        {
            return RuntimeContractFailure::metrics;
        }
        return RuntimeContractFailure::none;
    }
}
