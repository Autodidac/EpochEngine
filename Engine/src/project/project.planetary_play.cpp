// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include "../../include/core.format_text.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <source_location>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

import asset.mesh_artifact;
import core.logger;
import core.timer;
import package.source_install;
import project.planetary;
import project.planetary_runtime;
import render.water_frame_evidence;

#include "project.planetary_play.hpp"

namespace epochengine::project_planetary_play
{

    static void prepare_planetary_wet_transition(project_planetary::StreamResult& stream,
        std::shared_ptr<const project_planetary_snapshot::Snapshot> previous,
        std::shared_ptr<const project_planetary::ShorelineSourceReview> review,
        const project_planetary::Settings& settings,
        std::uint64_t retained_display_bytes,
        std::stop_token stop,const std::shared_ptr<water::wave::Provider>& provider,
        std::shared_ptr<const project_planetary_snapshot::WaterLatticeProvider> lattice_provider,
        std::shared_ptr<const water::wave::Tile> retained_tile);


    static void report_planetary_wet_batch(const project_planetary::StreamResult& stream,
        const project_planetary_snapshot::Snapshot& previous,std::string_view outcome,
        std::optional<project_planetary_snapshot::TileKey> patch,unsigned steps);

    struct ShorelineBatchTiming;
    static void prepare_planetary_wet_batch(project_planetary::StreamResult& stream,
        const project_planetary::Settings& settings,std::uint64_t retained_display_bytes,std::stop_token stop,
        ShorelineBatchTiming& evidence);
    [[nodiscard]] static bool shoreline_inputs_are_current(const std::filesystem::path& root,
        const project_planetary::StreamResult& stream,const project_planetary::ShorelineSourceReview& review,
        std::stop_token stop);

    [[nodiscard]] PlanetaryWaterResult sample_planetary_water(
        const std::filesystem::path& root,const project_planetary::SourceGeneration& identity,
        std::shared_ptr<const project_planetary_snapshot::Snapshot> source,
        const std::shared_ptr<water::wave::Provider>& provider,
        const project_planetary::Settings& settings,double seconds,
        std::string_view water_source_hash,std::uint64_t clock_generation,
        std::uint64_t retained_display_bytes,std::stop_token stop,
        std::shared_ptr<const water::wave::Tile>* retained_tile,
        const project_planetary::PackagedShoreline* packaged,
        const project_planetary::PackagedPlanetaryPose* linked_pose)
    {
        const auto started=timing::Clock::now();
        PlanetaryWaterResult result{.source=std::move(source),.body=settings.body_index,
            .clock_generation=clock_generation,.seconds=seconds};
        if(stop.stop_requested()){result.cancelled=true;return result;}
        const auto current_source=[&] {
            if(packaged)return result.source==packaged->selection.source
                &&settings.body_index==packaged->settings.body_index&&water_source_hash==packaged->fft_source;
            if(linked_pose)return settings==linked_pose->settings&&water_source_hash==linked_pose->fft_source
                &&(!result.source||(result.source->metadata.identity.source_hash==linked_pose->planetary_source
                    &&result.source->metadata.identity.recipe_key==linked_pose->recipe_key));
            if(!project_planetary::source_generation_is_current(root,identity,stop))return false;
            const auto water=package_source::load_project_attachment(root,"fft_ocean",stop);
            return water.source_attached()&&water.content_hash==water_source_hash;
        };
        if((!result.source&&!linked_pose)||!provider||!std::isfinite(seconds)||!current_source())
        {
            result.cancelled=stop.stop_requested();
            return result;
        }
        result.source_current=true;
        // The packaged cut retains the budget it was decoded with.
        auto limits=project_planetary::snapshot_transition_limits(packaged?packaged->settings:settings);
        if(retained_display_bytes>=limits.maximum_retained_bytes)return result;
        limits.maximum_retained_bytes-=retained_display_bytes;
        const auto wave_budget=(std::min<std::uint64_t>)(limits.maximum_transient_bytes,64ull*1024u*1024u);
        const auto wave=provider->evaluate(seconds,wave_budget,stop);
        if(wave&&wave.tile&&wave.tile->seconds()==seconds)
        {
            if(result.source)result.mesh=project_planetary_snapshot::resample_water(
                *result.source,*wave.tile,limits,stop,&result.appearance);
            if(linked_pose&&(!result.source||result.mesh))result.linked_sample=wave.tile;
        }
        result.cancelled=stop.stop_requested();
        result.source_current=!result.cancelled&&current_source();
        if(!result.source_current)
        {
            result.mesh.reset();
            result.appearance.reset();
            result.linked_sample.reset();
        }
        else if(retained_tile&&result.mesh)*retained_tile=wave.tile;
        result.worker_seconds=std::chrono::duration<double>(timing::Clock::now()-started).count();
        return result;
    }
    struct ShorelineBatchTiming final
    {
        bool enabled{};
        double check_before_ms{},advance_ms{},mesh_ms{},rollover_ms{},rejected_mesh_ms{},check_after_ms{};

        [[nodiscard]] static double milliseconds(timing::Clock::duration elapsed) noexcept
        {
            return std::chrono::duration<double,std::milli>(elapsed).count();
        }
        template<class Action>
        auto measure(double& elapsed,Action&& action) const
        {
            if(!enabled)return action();
            const auto started=timing::Clock::now();
            auto result=action();
            elapsed+=milliseconds(timing::Clock::now()-started);
            return result;
        }
    };

    [[nodiscard]] project_planetary::StreamResult prepare_initial_planetary_water_cut(
        const std::filesystem::path& root,const project_planetary::Settings& settings,
        InitialPlanetaryWaterCut cut,std::uint64_t clock_generation,
        std::uint64_t retained_display_bytes,std::stop_token stop)
    {
        auto& stream=cut.stream;
        std::shared_ptr<const water::wave::Tile> retained_tile;
        if(stream.water_edge_sample_seconds)
        {
            const auto seconds=*stream.water_edge_sample_seconds;
            auto sample=sample_planetary_water(root,*stream.verified_source,stream.patch_snapshot,
                cut.provider,settings,seconds,cut.water_source_hash,clock_generation,
                retained_display_bytes,stop,&retained_tile);
            stream.water_edge_sample_seconds.reset();
            if(!sample.source_current||sample.cancelled||!sample.mesh||!sample.appearance
                ||sample.appearance->artifact_key!=sample.mesh->key())
            {
                stream.snapshot_ready=false;
                stream.operation.ok=false;
                stream.operation.cancelled=sample.cancelled;
                stream.operation.message=sample.cancelled
                    ?"Terrain shoreline preparation was cancelled. The previous terrain remains visible."
                    :"Terrain shoreline preparation could not finish. The previous terrain remains visible.";
                return std::move(stream);
            }
            stream.snapshot=std::move(sample.mesh);
            stream.water_appearance=std::move(sample.appearance);
            stream.water_sample_seconds=sample.seconds;
        }
        if(cut.previous&&stream.snapshot&&stream.water_sample_seconds&&!stop.stop_requested())
        {
            std::shared_ptr<const project_planetary::ShorelineSourceReview> review;
            if(stream.verified_source&&project_planetary::stream_recipe_is_current(root,stream.verified_recipe))
                review=project_planetary::review_shoreline_sources(root,*stream.verified_source,cut.water_source_hash,stop);
            if(!review)
            {
                stream.snapshot_ready=false;
                stream.operation.ok=false;
                stream.operation.cancelled=stop.stop_requested();
                stream.operation.message="Terrain or water source changed during shoreline preparation. The previous terrain remains visible.";
                return std::move(stream);
            }
            prepare_planetary_wet_transition(stream,std::move(cut.previous),review,
                settings,retained_display_bytes,stop,cut.provider,std::move(cut.lattice_provider),
                std::move(retained_tile));
            if(!shoreline_inputs_are_current(root,stream,*review,stop))
            {
                stream.snapshot_ready=false;
                stream.operation.ok=false;
                stream.wet_transition.clear();
                stream.wet_cursor.reset();
                stream.operation.cancelled=stop.stop_requested();
                stream.operation.message="Terrain or water source changed during shoreline preparation. The previous terrain remains visible.";
            }
        }
        if(stop.stop_requested())stream.operation.cancelled=true;
        return std::move(stream);
    }
    static void prepare_planetary_wet_transition(project_planetary::StreamResult& stream,
        std::shared_ptr<const project_planetary_snapshot::Snapshot> previous,
        std::shared_ptr<const project_planetary::ShorelineSourceReview> review,
        const project_planetary::Settings& settings,
        std::uint64_t retained_display_bytes,
        std::stop_token stop,const std::shared_ptr<water::wave::Provider>& provider,
        std::shared_ptr<const project_planetary_snapshot::WaterLatticeProvider> lattice_provider,
        std::shared_ptr<const water::wave::Tile> retained_tile)
    {
        namespace patches=project_planetary_snapshot;
        if(!stream.patch_snapshot||stream.patch_snapshot->water_sample_seconds!=stream.water_sample_seconds)
        {
            report_planetary_wet_batch(stream,*previous,"sample_mismatch",{},0u);
            return;
        }
        const auto limits=project_planetary::snapshot_transition_limits(settings);
        patches::Transition transition{limits};
        if(!transition.admit(previous,stop,patches::InitialCut::selected_patches))
        {
            report_planetary_wet_batch(stream,*previous,"previous_cut_rejected",{},0u);
            return;
        }
        if(!transition.admit(stream.patch_snapshot,stop))
        {
            report_planetary_wet_batch(stream,*previous,"target_cut_rejected",{},0u);
            return;
        }
        if(transition.settled())
        {
            report_planetary_wet_batch(stream,*previous,"same_topology",{},0u);
            return;
        }
        if(transition.needs_retained_water_sample())
        {
            const auto old_bytes=patches::retained_snapshot_bytes(*previous);
            const auto target_bytes=patches::retained_snapshot_bytes(*stream.patch_snapshot);
            const auto held_bytes=retained_display_bytes+planetary_cut_mesh_bytes(
                stream.snapshot==stream.patch_snapshot->artifact?nullptr:stream.snapshot,
                stream.water_appearance==stream.patch_snapshot->metadata.water_appearance?nullptr:stream.water_appearance);
            if(!retained_tile&&provider&&held_bytes+target_bytes<limits.maximum_retained_bytes)
            {
                const auto wave_budget=(std::min<std::uint64_t>)({limits.maximum_transient_bytes,64ull*1024u*1024u,
                    limits.maximum_retained_bytes-held_bytes-target_bytes});
                const auto sample=provider->evaluate(*stream.water_sample_seconds,wave_budget,stop);
                if(sample)retained_tile=sample.tile;
            }
            const auto external=held_bytes>=old_bytes?held_bytes-old_bytes:held_bytes;
            if(!transition.prepare_retained_water(settings.cells_per_tile,std::move(retained_tile),
                std::move(lattice_provider),external,stop))
            {
                report_planetary_wet_batch(stream,*previous,
                    stop.stop_requested()?"cancelled":"retained_sample_unavailable",{},0u);
                return;
            }
        }
        stream.wet_cursor.emplace(project_planetary::StreamWetCutCursor{
            std::move(transition),std::move(previous),std::move(review)});
        ShorelineBatchTiming evidence;
        prepare_planetary_wet_batch(stream,settings,retained_display_bytes,stop,evidence);
    }

    [[nodiscard]] std::uint64_t planetary_cut_mesh_bytes(
        const std::shared_ptr<const asset::mesh::Artifact>& mesh,
        const std::shared_ptr<const asset::mesh::SphericalWaterAppearance>& appearance) noexcept
    {
        return (mesh ? std::uint64_t(mesh->vertices().size())*sizeof(asset::mesh::Vertex)
            +std::uint64_t(mesh->indices().size())*sizeof(std::uint32_t)+64u : 0u)
            +(appearance ? sizeof(asset::mesh::SphericalWaterAppearance)
                +appearance->ranges.capacity()*sizeof(asset::mesh::SphericalWaterRange) : 0u);
    }
    static void report_planetary_wet_batch(const project_planetary::StreamResult& stream,
        const project_planetary_snapshot::Snapshot& previous,std::string_view outcome,
        std::optional<project_planetary_snapshot::TileKey> patch,unsigned steps)
    {
        if(!render_water_evidence::enabled())return;
        const auto key=patch.value_or(project_planetary_snapshot::TileKey{});
        logger::infof_loc("Engine.Planet",std::source_location::current(),
            "Shoreline preparation: request={} previous={} outcome={} has_patch={} face={} level={} x={} y={} steps={} frames={}",
            stream.request_id,previous.metadata.identity.request_id,outcome,patch.has_value(),
            unsigned(key.face),unsigned(key.level),key.x,key.y,steps,stream.wet_transition.size());
    }
    static void prepare_planetary_wet_batch(project_planetary::StreamResult& stream,
        const project_planetary::Settings& settings,std::uint64_t retained_display_bytes,std::stop_token stop,
        ShorelineBatchTiming& evidence)
    {
        namespace patches=project_planetary_snapshot;
        constexpr std::size_t maximum_frames=4u;
        constexpr unsigned maximum_steps=128u;
        auto& cursor=*stream.wet_cursor;
        auto& frames=stream.wet_transition;
        frames.clear();
        frames.reserve(maximum_frames);
        auto held_bytes=retained_display_bytes+planetary_cut_mesh_bytes(
            stream.snapshot==stream.patch_snapshot->artifact?nullptr:stream.snapshot,
            stream.water_appearance==stream.patch_snapshot->metadata.water_appearance?nullptr:stream.water_appearance);
        const auto previous_bytes=patches::retained_snapshot_bytes(*cursor.previous);
        std::string_view outcome="staged";
        bool finished{};
        patches::WetTransitionFailure wet_failure;
        while(frames.size()<maximum_frames&&!stop.stop_requested())
        {
            if(cursor.steps>=maximum_steps)
            {
                outcome="step_limit";finished=true;break;
            }
            auto& next=cursor.transition;
            if(!cursor.materialization_pending&&!evidence.measure(evidence.advance_ms,[&] {
                return next.advance_wet(.05,false,stop,&wet_failure);
            }))
            {
                outcome=wet_failure.reason.empty()?"transition_did_not_advance":wet_failure.reason;
                finished=true;break;
            }
            if(next.settled())
            {
                ++cursor.steps;
                outcome="settled";finished=true;break;
            }
            const bool retains_previous=std::ranges::any_of(next.draw(),
                [&](const auto& draw){return draw.snapshot==cursor.previous;});
            const auto external_bytes=retains_previous&&held_bytes>=previous_bytes
                ?held_bytes-previous_bytes:held_bytes;
            project_planetary::StreamCutFrame frame;
            double mesh_ms{};
            frame.mesh=evidence.measure(mesh_ms,[&] {
                return next.materialize_wet(settings.cells_per_tile,*stream.water_sample_seconds,
                    stop,&frame.appearance,external_bytes,&wet_failure);
            });
            if(!frame.mesh)
            {
                if(wet_failure.reason=="output_retained_budget"&&!frames.empty())
                {
                    // Retry this phase once those staged meshes retire.
                    evidence.rollover_ms+=mesh_ms;
                    cursor.materialization_pending=true;
                    outcome="budget_rollover";
                    break;
                }
                evidence.rejected_mesh_ms+=mesh_ms;
                outcome=wet_failure.reason.empty()?"wet_geometry_rejected":wet_failure.reason;
                finished=true;break;
            }
            evidence.mesh_ms+=mesh_ms;
            cursor.materialization_pending=false;
            ++cursor.steps;
            held_bytes+=planetary_cut_mesh_bytes(frame.mesh,frame.appearance);
            frames.push_back(std::move(frame));
        }
        if(stop.stop_requested())
        {
            stream.operation.cancelled=true;
            stream.wet_transition.clear();
            stream.wet_cursor.reset();
            return;
        }
        report_planetary_wet_batch(stream,*cursor.previous,outcome,wet_failure.patch,cursor.steps);
        if(finished&&outcome!="settled")
            stream.operation.message+="\nShoreline transition unavailable for this terrain cut.";
        if(finished)stream.wet_cursor.reset();
    }
    [[nodiscard]] static bool shoreline_inputs_are_current(const std::filesystem::path& root,
        const project_planetary::StreamResult& stream,const project_planetary::ShorelineSourceReview& review,
        std::stop_token stop)
    {
        return !stop.stop_requested()&&stream.verified_source
            &&project_planetary::stream_recipe_is_current(root,stream.verified_recipe)
            &&project_planetary::shoreline_sources_are_current(root,*stream.verified_source,review,stop);
    }
    [[nodiscard]] project_planetary::StreamResult continue_planetary_wet_cut(
        const std::filesystem::path& root,const project_planetary::Settings& settings,
        project_planetary::StreamResult stream,std::uint64_t retained_display_bytes,std::stop_token stop,
        std::optional<timing::Clock::time_point> queued_at)
    {
        ShorelineBatchTiming evidence{.enabled=queued_at.has_value()};
        const auto started=evidence.enabled?timing::Clock::now():timing::Clock::time_point{};
        const auto first_step=stream.wet_cursor->steps;
        // A settled batch releases the cursor. Its final source check still owns the review.
        const auto review=stream.wet_cursor->source_review;
        bool current{};
        if(review&&evidence.measure(evidence.check_before_ms,[&] {
            return shoreline_inputs_are_current(root,stream,*review,stop);
        }))
        {
            prepare_planetary_wet_batch(stream,settings,retained_display_bytes,stop,evidence);
            current=evidence.measure(evidence.check_after_ms,[&] {
                return shoreline_inputs_are_current(root,stream,*review,stop);
            });
        }
        if(queued_at)
            logger::infof_loc("Engine.Planet",std::source_location::current(),
                "Shoreline batch timing: request={} first_step={} frames={} source_current={} worker_start_us={} "
                "queue_ms={:.3f} check_before_ms={:.3f} advance_ms={:.3f} mesh_ms={:.3f} "
                "rollover_ms={:.3f} rejected_mesh_ms={:.3f} check_after_ms={:.3f} worker_ms={:.3f}",
                stream.request_id,first_step,stream.wet_transition.size(),current,
                std::chrono::duration_cast<std::chrono::microseconds>(started.time_since_epoch()).count(),
                evidence.milliseconds(started-*queued_at),evidence.check_before_ms,evidence.advance_ms,
                evidence.mesh_ms,evidence.rollover_ms,evidence.rejected_mesh_ms,evidence.check_after_ms,
                evidence.milliseconds(timing::Clock::now()-started));
        if(current)return stream;
        stream.operation.cancelled=stop.stop_requested();
        stream.operation.ok=false;
        stream.operation.message=stream.operation.cancelled
            ?"Shoreline transition cancelled. The previous terrain remains visible."
            :"Shoreline transition stopped because its approved source changed.";
        stream.wet_transition.clear();
        stream.wet_cursor.reset();
        return stream;
    }
    project_planetary::StreamResult linked_terrain_step(
        project_planetary_runtime::TerrainProvider& provider,
        const project_planetary_runtime::TerrainView& view,std::stop_token stop)
    {
        using Status=project_planetary_runtime::TerrainStatus;
        const auto batch=provider.step(view,stop);
        project_planetary::StreamResult result;
        result.request_id=batch.request_id;
        result.waiting_for_worker=batch.status==Status::busy;
        result.operation.cancelled=batch.status==Status::cancelled||stop.stop_requested();
        if(result.operation.cancelled)
        {result.operation.message="Terrain detail cancelled. The previous mesh is retained.";return result;}
        result.operation.ok=batch.status==Status::ready||batch.status==Status::pending;
        result.snapshot_ready=batch.status==Status::ready&&bool(batch.artifact);
        if(result.snapshot_ready)
        {
            result.snapshot=batch.artifact;
            result.operation.mesh_frame=project_planetary::GeneratedMeshFrame{1.0,batch.source_origin_meters};
            result.patch_snapshot=batch.aquatic_source;
            result.water_appearance=batch.water_appearance;
            if(batch.aquatic_source)result.water_sample_seconds=batch.aquatic_source->water_sample_seconds;
        }
        result.built=static_cast<std::uint32_t>(batch.built);
        result.pending=static_cast<std::uint32_t>(batch.pending);
        result.draw=static_cast<std::uint32_t>(batch.draw_patches);
        result.maximum_level=batch.maximum_level;
        result.operation.message=batch.status==Status::capacity
            ?"Linked terrain fits neither the current residency nor mesh budget. The previous mesh is retained."
            :result.operation.ok||result.waiting_for_worker?"Terrain detail follows the current camera."
            :"Linked terrain could not assemble the requested cut. The previous mesh is retained.";
        if(!batch.failure.empty())
            logger::infof_loc("Engine.Planet",std::source_location::current(),
                "Linked terrain request {} stopped at {}.",view.request_id,batch.failure);
        return result;
    }

    [[nodiscard]] bool planetary_view_changed(const project_planetary::StreamView& current,
        const project_planetary::StreamView& previous) noexcept
    {
        const double radius=std::sqrt(current.camera[0]*current.camera[0]
            +current.camera[1]*current.camera[1]+current.camera[2]*current.camera[2]);
        const double epsilon=std::clamp(std::abs(radius-10.0)*0.05,0.00001,0.025);
        if(current.viewport_height!=previous.viewport_height
            ||std::abs(current.vertical_fov-previous.vertical_fov)>0.001
            ||current.forward.has_value()!=previous.forward.has_value()
            ||current.up.has_value()!=previous.up.has_value()
            ||std::abs(current.aspect_ratio-previous.aspect_ratio)>0.001) return true;
        if(current.forward&&previous.forward)
            for(std::size_t axis=0;axis<3u;++axis)
                if(std::abs((*current.forward)[axis]-(*previous.forward)[axis])>0.01)return true;
        if(current.up&&previous.up)
            for(std::size_t axis=0;axis<3u;++axis)
                if(std::abs((*current.up)[axis]-(*previous.up)[axis])>0.01)return true;
        for(std::size_t axis=0;axis<3u;++axis)
            if(std::abs(current.camera[axis]-previous.camera[axis])>epsilon) return true;
        return false;
    }
}
