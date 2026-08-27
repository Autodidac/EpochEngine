/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module project.gameplay2d_runtime;

import perf.tier;
import platform.budgets;

namespace epochengine::project_gameplay2d
{
    namespace
    {
        inline constexpr std::string_view contract_project{
            "epoch.gameplay2d.contract"};
        inline constexpr audio::LogicalResourceId effects_bus{81u, 100u};
        inline constexpr audio::ClipId jump_cue{81u, 1u};
        inline constexpr audio::ClipId land_cue{81u, 2u};
        inline constexpr audio::ClipId ambient_cue{81u, 3u};

        [[nodiscard]] audio::OwnedPcmClip make_clip(
            audio::ClipId id,
            float amplitude)
        {
            constexpr std::uint32_t sampleRate = 48'000u;
            constexpr std::uint64_t frameCount = 4'800u;
            audio::OwnedPcmClip clip{
                .id = id,
                .format = {
                    .sample_format =
                        audio::PcmSampleFormat::float32_interleaved,
                    .channel_layout = audio::PcmChannelLayout::mono,
                    .sample_rate = sampleRate},
                .frame_count = frameCount};
            clip.interleaved_samples.resize(frameCount);
            for (std::uint64_t frame = 0u; frame < frameCount; ++frame)
            {
                const bool positive = (frame / 24u) % 2u == 0u;
                clip.interleaved_samples[frame] =
                    positive ? amplitude : -amplitude;
            }
            return clip;
        }

        [[nodiscard]] AudioProgram make_audio_program()
        {
            AudioProgram result{};
            result.request.stable_session_id = 0x8100'0001u;
            result.request.request_physical_output = false;
            result.request.buses.push_back({
                .id = effects_bus,
                .gain = 1.0f,
                .muted = false});
            result.request.cues.push_back({
                .clip = make_clip(jump_cue, 0.22f),
                .bus = effects_bus,
                .gain = 0.8f,
                .looping = false});
            result.request.cues.push_back({
                .clip = make_clip(land_cue, 0.16f),
                .bus = effects_bus,
                .gain = 0.7f,
                .looping = false});
            result.request.cues.push_back({
                .clip = make_clip(ambient_cue, 0.025f),
                .bus = effects_bus,
                .gain = 0.4f,
                .looping = true});
            result.jump = jump_cue;
            result.land = land_cue;
            result.autoplay.push_back(ambient_cue);
            result.authored = true;
            return result;
        }

        [[nodiscard]] std::optional<
            project_sprite_animation::CompiledSpriteAnimationArtifact>
        make_animation(std::uint64_t expectedProjectKey)
        {
            project_sprite_animation::DefaultActorSheet sheet{};
            sheet.material.logical_texture_path =
                "Assets/Textures/contract_actor.epochtexture";
            sheet.material.texture_artifact_key.words = {
                0x10u, 0x20u, 0x30u, 0x40u};
            sheet.material.texture_artifact_revision = 11u;
            sheet.material.stable_material_key = 0x8101u;
            sheet.material.texture_extent = {64u, 16u};
            sheet.tile_extent = {16u, 16u};
            sheet.grid = {4u, 1u};
            sheet.tile_count = 4u;
            auto source =
                project_sprite_animation::make_default_actor_source(sheet, 7u);
            auto compiled = project_sprite_animation::compile_artifact(
                contract_project, source);
            if (!compiled || compiled.artifact.project_key != expectedProjectKey)
                return std::nullopt;
            return std::move(compiled.artifact);
        }

        [[nodiscard]] std::optional<PreparedProject> make_project()
        {
            auto input = project_input::compile_profile(
                contract_project,
                project_input::make_legacy_default_profile(3u));
            if (!input)
                return std::nullopt;

            PreparedProject result{};
            result.project_key = input.artifact.project_key;
            result.input = std::move(input.artifact);
            result.base_scene.project.logical_canvas = {640u, 360u};
            result.base_scene.project.pixels_per_world_unit = 16.0f;
            result.base_scene.camera.center = {0.0f, 0.0f};
            result.base_scene.camera.pixels_per_world_unit = 16.0f;
            result.base_scene.source_revision = 0x8102u;
            result.actor = {};
            result.collision.push_back({
                .stable_id = 0x8103u,
                .bounds = {-10.0, 3.0, 20.0, 1.0},
                .layer_bits = 1u,
                .mask_bits = 0xffff'ffffu,
                .sensor = false,
                .kind = project_actor2d::StaticCollisionKind::solid_box});
            result.animations = make_animation(result.project_key);
            result.audio_program = make_audio_program();
            if (!result.animations || !result.valid())
                return std::nullopt;
            return result;
        }

        [[nodiscard]] bool has_event(
            const AdvanceResult& result,
            project_actor2d::ActorEventKind kind) noexcept
        {
            for (const auto& event : result.events)
            {
                if (event.kind == kind && event.stable_actor_id != 0u
                    && event.order != 0u)
                {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] bool deterministic_costs_equal(
            const RuntimeCostSnapshot& left,
            const RuntimeCostSnapshot& right) noexcept
        {
            return left.code == right.code
                && left.scene_revision == right.scene_revision
                && left.logical_canvas_width == right.logical_canvas_width
                && left.logical_canvas_height == right.logical_canvas_height
                && left.logical_canvas_pixels == right.logical_canvas_pixels
                && left.source_sprites == right.source_sprites
                && left.source_texture_views == right.source_texture_views
                && left.source_texture_bytes == right.source_texture_bytes
                && left.emitted_sprites == right.emitted_sprites
                && left.emitted_batches == right.emitted_batches
                && left.emitted_vertices == right.emitted_vertices
                && left.emitted_indices == right.emitted_indices
                && left.material_count == right.material_count
                && left.maximum_sprites_per_batch
                    == right.maximum_sprites_per_batch
                && left.maximum_batches == right.maximum_batches
                && left.canvas_rejections == right.canvas_rejections
                && left.collision_surfaces == right.collision_surfaces
                && left.peak_contacts_per_step
                    == right.peak_contacts_per_step
                && left.input_frames == right.input_frames
                && left.rejected_input_frames
                    == right.rejected_input_frames
                && left.fixed_steps == right.fixed_steps
                && left.audio_resident_clips
                    == right.audio_resident_clips
                && left.audio_bound_buses == right.audio_bound_buses
                && left.audio_resident_bytes == right.audio_resident_bytes
                && left.audio_frames_mixed == right.audio_frames_mixed
                && left.audio_frames_submitted
                    == right.audio_frames_submitted
                && left.audio_failures == right.audio_failures
                && left.diagnostic == right.diagnostic;
        }
        struct ReplayEvidence final
        {
            SessionCode opened{SessionCode::closed};
            project_actor2d::ActorState settled{};
            project_actor2d::ActorState moved{};
            project_actor2d::ActorState jumped{};
            project_actor2d::ActorState landed{};
            std::uint64_t settled_scene_hash{};
            std::uint64_t moved_scene_hash{};
            std::uint64_t jumped_scene_hash{};
            std::uint64_t landed_scene_hash{};
            bool saw_idle{};
            bool saw_run{};
            bool saw_rise{};
            bool saw_fall{};
            bool saw_jump_event{};
            bool saw_land_event{};
            bool stale_rejected{};
            bool pause_reset{};
            bool canvas_published{};
            bool repeated_session{};
            bool teardown{};
            SessionMetrics metrics{};
            audio::PlaybackRuntimeMetrics audio_metrics{};
            RuntimeCostSnapshot costs{};

            friend bool deterministic_equal(
                const ReplayEvidence& left,
                const ReplayEvidence& right) noexcept
            {
                return left.opened == right.opened
                    && left.settled == right.settled
                    && left.moved == right.moved
                    && left.jumped == right.jumped
                    && left.landed == right.landed
                    && left.settled_scene_hash == right.settled_scene_hash
                    && left.moved_scene_hash == right.moved_scene_hash
                    && left.jumped_scene_hash == right.jumped_scene_hash
                    && left.landed_scene_hash == right.landed_scene_hash
                    && left.saw_idle == right.saw_idle
                    && left.saw_run == right.saw_run
                    && left.saw_rise == right.saw_rise
                    && left.saw_fall == right.saw_fall
                    && left.saw_jump_event == right.saw_jump_event
                    && left.saw_land_event == right.saw_land_event
                    && left.stale_rejected == right.stale_rejected
                    && left.pause_reset == right.pause_reset
                    && left.canvas_published == right.canvas_published
                    && left.repeated_session == right.repeated_session
                    && left.teardown == right.teardown
                    && left.metrics == right.metrics
                    && left.audio_metrics == right.audio_metrics
                    && deterministic_costs_equal(left.costs, right.costs);
            }
        };

        [[nodiscard]] ReplayEvidence exercise(const PreparedProject& project)
        {
            ReplayEvidence evidence{};
            audio::PlaybackRuntime audioRuntime{};
            Gameplay2DRuntime session{};
            evidence.opened = session.open(project, &audioRuntime);
            if (evidence.opened != SessionCode::ready)
                return evidence;

            std::uint64_t frame{};
            const auto advance = [&session, &frame](
                std::span<const project_input::ActionImpulse> impulses = {})
            {
                return session.advance(
                    project_input::InputSnapshot{.frame_index = ++frame},
                    impulses,
                    1.0 / 60.0);
            };
            const auto sceneHash = [&session]() noexcept
            {
                const auto* scene = session.scene();
                return scene
                    ? canvas2d::scene_content::content_hash(*scene) : 0u;
            };
            const auto poseIs = [&session](
                project_sprite_animation::ActorPose pose) noexcept
            {
                const auto snapshot = session.snapshot();
                return snapshot.animation
                    && snapshot.animation->animation
                        == project_sprite_animation::
                            default_actor_animation_id(pose);
            };

            for (std::uint32_t index = 0u; index < 120u; ++index)
            {
                if (!advance())
                    return evidence;
            }
            evidence.settled = session.actor_state();
            evidence.settled_scene_hash = sceneHash();
            evidence.saw_idle = poseIs(
                project_sprite_animation::ActorPose::idle);

            const std::array move{
                project_input::ActionImpulse{
                    .semantic = project_input::ActionSemantic::move_x,
                    .value_q15 = project_input::normalized_unit}};
            for (std::uint32_t index = 0u; index < 20u; ++index)
            {
                if (!advance(move))
                    return evidence;
            }
            evidence.moved = session.actor_state();
            evidence.moved_scene_hash = sceneHash();
            evidence.saw_run = poseIs(
                project_sprite_animation::ActorPose::run);

            const std::array jump{
                project_input::ActionImpulse{
                    .semantic = project_input::ActionSemantic::jump,
                    .value_q15 = project_input::normalized_unit,
                    .pressed = true}};
            const auto jumpFrame = advance(jump);
            if (!jumpFrame)
                return evidence;
            evidence.jumped = jumpFrame.actor;
            evidence.jumped_scene_hash = sceneHash();
            evidence.saw_jump_event = has_event(
                jumpFrame, project_actor2d::ActorEventKind::jump_started);
            evidence.saw_rise = poseIs(
                project_sprite_animation::ActorPose::rise);

            for (std::uint32_t index = 0u; index < 180u; ++index)
            {
                const auto result = advance();
                if (!result)
                    return evidence;
                evidence.saw_fall = evidence.saw_fall || poseIs(
                    project_sprite_animation::ActorPose::fall);
                if (has_event(result, project_actor2d::ActorEventKind::landed))
                {
                    evidence.saw_land_event = true;
                    break;
                }
            }
            evidence.landed = session.actor_state();
            evidence.landed_scene_hash = sceneHash();

            const auto beforeStale = session.actor_state();
            const auto stale = session.advance({.frame_index = frame}, {},
                1.0 / 60.0);
            evidence.stale_rejected = stale.code == SessionCode::invalid_input
                && session.actor_state() == beforeStale;

            const auto paused = session.set_paused(true);
            const auto resumed = session.set_paused(false);
            const auto reset = session.reset();
            evidence.pause_reset = paused == SessionCode::paused
                && resumed == SessionCode::ready
                && reset == SessionCode::ready
                && session.actor_state().fixed_tick == 0u;

            int owner{};
            if (const auto* scene = session.scene())
            {
                const auto published = canvas2d::scene_content::publish(
                    &owner, *scene);
                const auto acquired = canvas2d::scene_content::acquire(&owner);
                const auto plan = canvas2d::scene_content::compile(
                    acquired, {640u, 360u});
                evidence.canvas_published = published && acquired && plan
                    && acquired.content_hash == published.content_hash
                    && canvas2d::scene_content::retire(&owner)
                        == canvas2d::scene_content::SceneCode::ready;
            }

            evidence.costs = session.cost_snapshot();

            const auto firstClose = session.close();
            evidence.teardown = firstClose == SessionCode::closed
                && !session.active() && session.scene() == nullptr
                && session.cost_snapshot().code == RuntimeCostCode::closed
                && !audioRuntime.snapshot().session_active;
            const auto reopened = session.open(project, &audioRuntime);
            const auto secondClose = session.close();
            evidence.repeated_session = reopened == SessionCode::ready
                && secondClose == SessionCode::closed
                && !audioRuntime.snapshot().session_active;
            evidence.metrics = session.metrics();
            evidence.audio_metrics = audioRuntime.metrics();
            return evidence;
        }

        [[nodiscard]] bool bounded_session_soak(
            const PreparedProject& project) noexcept
        {
            constexpr std::uint32_t sessionCount = 64u;
            constexpr std::uint32_t framesPerSession = 8u;

            audio::PlaybackRuntime audioRuntime{};
            Gameplay2DRuntime session{};
            std::uint64_t initialSceneHash{};
            for (std::uint32_t sessionIndex = 0u;
                sessionIndex < sessionCount;
                ++sessionIndex)
            {
                if (session.open(project, &audioRuntime) != SessionCode::ready
                    || !session.active() || session.input_profile() == nullptr
                    || session.scene() == nullptr)
                {
                    return false;
                }

                const auto openedAudio = audioRuntime.snapshot();
                const std::uint64_t openedSceneHash =
                    canvas2d::scene_content::content_hash(*session.scene());
                if (!openedAudio.session_active || !openedAudio.handle
                    || openedAudio.stable_session_id == 0u
                    || openedAudio.cues.size() != 3u
                    || openedAudio.buses.size() != 1u
                    || openedSceneHash == 0u)
                {
                    return false;
                }
                if (sessionIndex == 0u)
                    initialSceneHash = openedSceneHash;
                else if (openedSceneHash != initialSceneHash)
                    return false;

                const std::int32_t direction = (sessionIndex % 2u == 0u)
                    ? project_input::normalized_unit
                    : -project_input::normalized_unit;
                const std::array move{
                    project_input::ActionImpulse{
                        .semantic = project_input::ActionSemantic::move_x,
                        .value_q15 = direction}};
                for (std::uint32_t frame = 1u;
                    frame <= framesPerSession;
                    ++frame)
                {
                    const auto advanced = session.advance(
                        {.frame_index = frame}, move, 1.0 / 60.0);
                    if (!advanced || advanced.fixed_steps == 0u
                        || session.scene() == nullptr)
                    {
                        return false;
                    }
                }

                if (session.close() != SessionCode::closed)
                    return false;
                const SessionMetrics closedMetrics = session.metrics();
                if (session.close() != SessionCode::closed
                    || session.metrics() != closedMetrics)
                {
                    return false;
                }

                const RuntimeSnapshot closed = session.snapshot();
                const auto closedAudio = audioRuntime.snapshot();
                if (closed.active || closed.code != SessionCode::closed
                    || session.active() || session.scene() != nullptr
                    || session.input_profile() != nullptr
                    || closedAudio.session_active || closedAudio.handle
                    || closedAudio.stable_session_id != 0u
                    || !closedAudio.cues.empty() || !closedAudio.buses.empty())
                {
                    return false;
                }
            }

            const auto sessionMetrics = session.metrics();
            const auto audioMetrics = audioRuntime.metrics();
            return sessionMetrics.sessions_opened == sessionCount
                && sessionMetrics.sessions_closed == sessionCount
                && sessionMetrics.input_frames
                    == sessionCount * framesPerSession
                && sessionMetrics.fixed_steps
                    == sessionCount * framesPerSession
                && sessionMetrics.scene_rebuilds
                    >= sessionCount * (framesPerSession + 1u)
                && sessionMetrics.animation_failures == 0u
                && sessionMetrics.scene_failures == 0u
                && sessionMetrics.audio_failures == 0u
                && audioMetrics.sessions_opened == sessionCount
                && audioMetrics.sessions_closed == sessionCount
                && audioMetrics.cues_registered == sessionCount * 3u
                && audioMetrics.buses_registered == sessionCount;
        }
    }

    ContractFailure run_contract() noexcept
    {
        try
        {
            Gameplay2DRuntime invalid{};
            if (invalid.open({}) != SessionCode::invalid_project)
                return ContractFailure::invalid_project_accepted;

            const auto project = make_project();
            if (!project)
                return ContractFailure::session_open;

            audio::PlaybackRuntime assignmentAudio{};
            Gameplay2DRuntime assignmentTarget{};
            if (assignmentTarget.open(*project, &assignmentAudio)
                    != SessionCode::ready
                || !assignmentAudio.snapshot().session_active)
            {
                return ContractFailure::session_open;
            }
            Gameplay2DRuntime closedReplacement{};
            assignmentTarget = std::move(closedReplacement);
            if (assignmentAudio.snapshot().session_active)
                return ContractFailure::teardown;

            const ReplayEvidence first = exercise(*project);
            const ReplayEvidence second = exercise(*project);
            if (first.opened != SessionCode::ready
                || second.opened != SessionCode::ready)
            {
                return ContractFailure::session_open;
            }
            if (!deterministic_equal(first, second))
                return ContractFailure::deterministic_replay;
            if (!first.settled.grounded
                || first.moved.x <= first.settled.x)
            {
                return ContractFailure::input_mapping;
            }
            if (!first.saw_jump_event || !first.saw_land_event
                || !first.landed.grounded
                || std::abs(first.landed.y - first.settled.y) > 2.0e-3)
            {
                return ContractFailure::collision;
            }
            if (!first.saw_idle || !first.saw_run
                || !first.saw_rise || !first.saw_fall)
            {
                return ContractFailure::animation;
            }
            if (first.metrics.audio_triggers < 2u
                || first.audio_metrics.triggers_accepted < 4u)
            {
                return ContractFailure::audio_cue;
            }
            if (!first.canvas_published
                || first.settled_scene_hash == 0u
                || first.moved_scene_hash == 0u
                || first.jumped_scene_hash == 0u
                || first.landed_scene_hash == 0u)
            {
                return ContractFailure::canvas_publication;
            }
            if (!first.costs)
                return ContractFailure::cost_snapshot_unavailable;
            if (first.costs.logical_canvas_width != 640u
                || first.costs.logical_canvas_height != 360u
                || first.costs.logical_canvas_pixels != 230'400u
                || first.costs.source_sprites != 1u
                || first.costs.emitted_sprites != 1u
                || first.costs.emitted_batches != 1u
                || first.costs.emitted_vertices != 4u
                || first.costs.emitted_indices != 6u
                || first.costs.canvas_rejections != 0u)
            {
                return ContractFailure::cost_snapshot_canvas;
            }
            if (first.costs.collision_surfaces != 1u)
                return ContractFailure::cost_snapshot_physics;
            if (first.costs.audio_resident_clips != 3u)
                return ContractFailure::cost_snapshot_audio_clips;
            if (first.costs.audio_bound_buses != 2u)
                return ContractFailure::cost_snapshot_audio_buses;
            if (first.costs.audio_resident_bytes == 0u)
                return ContractFailure::cost_snapshot_audio_bytes;

            const Budgets portableBudgets =
                platform::recommended_budgets_for_tier(
                    perf::tier::mobile_30);
            const RuntimeBudgetAssessment portable =
                assess_runtime_costs(first.costs, portableBudgets);
            if (!portable || portable.violation_mask != 0u)
                return ContractFailure::cost_budget_portable;

            RuntimeCostSnapshot stressed = first.costs;
            stressed.logical_canvas_pixels =
                portableBudgets.canvas2d.maximum_logical_canvas_pixels + 1u;
            stressed.source_texture_bytes =
                portableBudgets.canvas2d.maximum_logical_texture_bytes + 1u;
            stressed.emitted_sprites =
                portableBudgets.canvas2d.maximum_visible_sprites + 1u;
            stressed.emitted_batches =
                static_cast<std::uint64_t>(
                    portableBudgets.canvas2d.maximum_batches) + 1u;
            stressed.collision_surfaces =
                portableBudgets.canvas2d.maximum_collision_surfaces + 1u;
            stressed.audio_resident_bytes =
                portableBudgets.canvas2d.maximum_resident_audio_bytes + 1u;
            stressed.canvas_rejections = 1u;
            const RuntimeBudgetAssessment constrained =
                assess_runtime_costs(stressed, portableBudgets);
            constexpr std::uint32_t expectedViolations =
                runtime_budget_violation_bit(
                    RuntimeBudgetViolation::canvas_pixels)
                | runtime_budget_violation_bit(
                    RuntimeBudgetViolation::logical_texture_bytes)
                | runtime_budget_violation_bit(
                    RuntimeBudgetViolation::visible_sprites)
                | runtime_budget_violation_bit(
                    RuntimeBudgetViolation::batches)
                | runtime_budget_violation_bit(
                    RuntimeBudgetViolation::collision_surfaces)
                | runtime_budget_violation_bit(
                    RuntimeBudgetViolation::resident_audio_bytes)
                | runtime_budget_violation_bit(
                    RuntimeBudgetViolation::canvas_rejections);
            if (constrained.code != RuntimeBudgetCode::over_budget
                || constrained.violation_mask != expectedViolations)
            {
                return ContractFailure::cost_budget_detection;
            }
            if (!first.stale_rejected)
                return ContractFailure::stale_input;
            if (!first.pause_reset)
                return ContractFailure::pause_reset;
            if (!first.repeated_session)
                return ContractFailure::repeated_session;
            if (!bounded_session_soak(*project))
                return ContractFailure::bounded_session_soak;
            if (!first.teardown)
                return ContractFailure::teardown;
            if (first.metrics.sessions_opened != 2u
                || first.metrics.sessions_closed != 2u
                || first.metrics.fixed_steps == 0u
                || first.metrics.animation_samples == 0u
                || first.metrics.scene_rebuilds == 0u
                || first.metrics.rejected_input_frames != 1u
                || first.audio_metrics.sessions_opened != 2u
                || first.audio_metrics.sessions_closed != 2u)
            {
                return ContractFailure::metrics;
            }
            return ContractFailure::none;
        }
        catch (...)
        {
            return ContractFailure::session_open;
        }
    }
}
