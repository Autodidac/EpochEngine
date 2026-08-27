/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

module project.actor2d_runtime;

namespace epochengine::project_actor2d
{
    namespace
    {
        [[nodiscard]] bool near(double left, double right) noexcept
        {
            return std::abs(left - right) <= 2.0e-3;
        }

        [[nodiscard]] StaticCollision floor_surface() noexcept
        {
            return {
                .stable_id = 1001u,
                .bounds = {-10.0, 3.0, 20.0, 1.0},
                .layer_bits = 1u,
                .mask_bits = 0xffff'ffffu
            };
        }

        [[nodiscard]] ActorState settle_platformer(ActorRuntime& runtime)
        {
            for (std::uint64_t sequence = 1u; sequence <= 120u; ++sequence)
            {
                const auto result = runtime.advance({.sequence = sequence});
                if (!result)
                    return {};
            }
            return runtime.state();
        }

        [[nodiscard]] bool has_event(
            const AdvanceResult& result,
            ActorEventKind kind,
            bool enabled = false) noexcept
        {
            for (const ActorEvent& event : result.events)
            {
                if (event.kind == kind
                    && (kind != ActorEventKind::pause_changed
                        || event.enabled == enabled)
                    && event.stable_actor_id != 0u
                    && event.order != 0u)
                {
                    return true;
                }
            }
            return false;
        }
    }

    ContractFailure run_contract() noexcept
    {
        ActorConfiguration invalid{};
        invalid.fixed_steps_per_second = 0u;
        if (ActorRuntime::validate_configuration(invalid) == ResultCode::ready)
            return ContractFailure::invalid_configuration_accepted;
        ActorConfiguration outside{};
        outside.spawn_x = 1'000'000.0;
        if (ActorRuntime::validate_configuration(outside) == ResultCode::ready)
            return ContractFailure::invalid_configuration_accepted;

        const std::vector<StaticCollision> duplicates{
            floor_surface(), floor_surface()};
        if (ActorRuntime::validate_collision(duplicates, 8u)
            == ResultCode::ready)
        {
            return ContractFailure::invalid_collision_accepted;
        }

        StaticCollision unsupported = floor_surface();
        unsupported.kind = StaticCollisionKind::unsupported;
        if (ActorRuntime::validate_collision(
                std::span<const StaticCollision>{&unsupported, 1u}, 8u)
            == ResultCode::ready)
        {
            return ContractFailure::invalid_collision_accepted;
        }
        unsupported = floor_surface();
        unsupported.sensor = true;
        if (ActorRuntime::validate_collision(
                std::span<const StaticCollision>{&unsupported, 1u}, 8u)
            == ResultCode::ready)
        {
            return ContractFailure::invalid_collision_accepted;
        }

        const std::array authoredSurfaceKinds{
            StaticCollisionKind::one_way_up,
            StaticCollisionKind::slope_up_right,
            StaticCollisionKind::slope_down_right};
        for (std::size_t index = 0u; index < authoredSurfaceKinds.size(); ++index)
        {
            StaticCollision surface = floor_surface();
            surface.stable_id += static_cast<std::uint64_t>(index + 1u);
            surface.kind = authoredSurfaceKinds[index];
            ActorRuntime surfaceRuntime{};
            if (surfaceRuntime.replace_collision(
                    std::span<const StaticCollision>{&surface, 1u})
                    != ResultCode::ready)
            {
                return ContractFailure::invalid_collision_accepted;
            }
            const auto snapshot = surfaceRuntime.snapshot();
            if (snapshot.solver.state.static_primitives.size() != 1u
                || snapshot.solver.state.static_primitives.front().stable_id
                    != surface.stable_id)
            {
                return ContractFailure::invalid_collision_accepted;
            }
        }

        const std::vector<StaticCollision> floor{floor_surface()};
        ActorRuntime first{};
        ActorRuntime second{};
        if (first.replace_collision(floor) != ResultCode::ready
            || second.replace_collision(floor) != ResultCode::ready)
        {
            return ContractFailure::invalid_collision_accepted;
        }
        const ActorState firstSettled = settle_platformer(first);
        const ActorState secondSettled = settle_platformer(second);
        if (firstSettled != secondSettled)
            return ContractFailure::deterministic_replay_mismatch;
        if (!firstSettled.grounded)
            return ContractFailure::platformer_collision_failed;
        if (!near(firstSettled.y, 2.55))
            return ContractFailure::platformer_collision_failed;

        const auto paused = first.advance({
            .sequence = 121u,
            .pause_pressed = true});
        if (paused.code != ResultCode::paused || !paused.state.paused
            || paused.steps_committed != 0u
            || !has_event(paused, ActorEventKind::pause_changed, true))
        {
            return ContractFailure::pause_failed;
        }
        const auto stale = first.advance({.sequence = 121u});
        if (stale.code != ResultCode::invalid_input)
            return ContractFailure::stale_input_accepted;
        if (!stale.events.empty())
            return ContractFailure::actor_event_failed;
        const auto resumed = first.advance({
            .sequence = 122u,
            .pause_pressed = true});
        if (!resumed || resumed.state.paused
            || !has_event(resumed, ActorEventKind::pause_changed, false))
            return ContractFailure::pause_failed;
        const auto reset = first.advance({
            .sequence = 123u,
            .reset_pressed = true});
        if (!reset || !near(reset.state.x, 1.5) || !near(reset.state.y, 2.0)
            || reset.state.fixed_tick != 0u
            || !has_event(reset, ActorEventKind::reset))
        {
            return ContractFailure::reset_failed;
        }

        ActorRuntime eventRuntime{};
        if (eventRuntime.replace_collision(floor) != ResultCode::ready
            || !settle_platformer(eventRuntime).grounded)
        {
            return ContractFailure::actor_event_failed;
        }
        const auto jump = eventRuntime.advance({
            .sequence = 121u,
            .jump_pressed = true
        });
        if (!jump
            || !has_event(jump, ActorEventKind::jump_started)
            || (jump.events.size() > 1u
                && jump.events[1].order <= jump.events[0].order))
        {
            return ContractFailure::actor_event_failed;
        }
        ActorConfiguration topDownConfiguration{};
        topDownConfiguration.movement = MovementMode::top_down;
        topDownConfiguration.spawn_x = 0.0;
        topDownConfiguration.spawn_y = 0.0;
        ActorRuntime topDown{topDownConfiguration};
        const std::vector<StaticCollision> wall{{
            .stable_id = 2001u,
            .bounds = {2.0, -4.0, 1.0, 8.0},
            .layer_bits = 1u,
            .mask_bits = 0xffff'ffffu}};
        if (topDown.replace_collision(wall) != ResultCode::ready)
            return ContractFailure::invalid_collision_accepted;
        for (std::uint64_t sequence = 1u; sequence <= 90u; ++sequence)
        {
            if (!topDown.advance({.sequence = sequence, .move_x = 1.0}))
                return ContractFailure::top_down_collision_failed;
        }
        if (topDown.state().x > 1.627)
            return ContractFailure::top_down_collision_failed;

        const RuntimeSnapshot checkpoint = topDown.snapshot();
        if (!topDown.advance({.sequence = 91u, .move_x = -1.0})
            || topDown.restore(checkpoint) != ResultCode::ready
            || topDown.snapshot() != checkpoint)
        {
            return ContractFailure::snapshot_restore_failed;
        }
        if (topDown.metrics().collision_surfaces != wall.size()
            || topDown.snapshot().solver.state.static_primitives.size()
                != wall.size())
        {
            return ContractFailure::resource_accounting_failed;
        }

        const RuntimeSnapshot beforeContradiction = topDown.snapshot();
        RuntimeSnapshot contradictory = beforeContradiction;
        contradictory.collision.front().bounds.x += 1.0;
        if (topDown.restore(contradictory) != ResultCode::invalid_snapshot
            || topDown.snapshot() != beforeContradiction)
        {
            return ContractFailure::contradictory_snapshot_accepted;
        }

        ActorRuntime hotReplacement{};
        if (hotReplacement.replace_collision(floor) != ResultCode::ready
            || !hotReplacement.advance({.sequence = 1u}, 4u))
        {
            return ContractFailure::hot_collision_reset_failed;
        }
        StaticCollision replacement = floor_surface();
        replacement.stable_id = 3001u;
        replacement.bounds.y = 4.0;
        const std::vector<StaticCollision> replacementMap{replacement};
        if (hotReplacement.replace_collision(replacementMap) != ResultCode::ready
            || hotReplacement.reset() != ResultCode::ready)
        {
            return ContractFailure::hot_collision_reset_failed;
        }
        const RuntimeSnapshot replaced = hotReplacement.snapshot();
        if (replaced.solver.state.static_primitives.size() != 1u
            || replaced.solver.reset_point.static_primitives.size() != 1u
            || replaced.solver.state.static_primitives.front().stable_id
                != replacement.stable_id
            || replaced.solver.reset_point.static_primitives.front().stable_id
                != replacement.stable_id
            || !near(replaced.actor.x, 1.5)
            || !near(replaced.actor.y, 2.0))
        {
            return ContractFailure::hot_collision_reset_failed;
        }
        return ContractFailure::none;
    }
}

#if defined(EPOCH_PROJECT_ACTOR2D_CONTRACT_MAIN)
int main()
{
    return static_cast<int>(
        epochengine::project_actor2d::run_contract());
}
#endif
