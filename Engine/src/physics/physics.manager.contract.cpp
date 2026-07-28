/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#include <cstdint>
#include <limits>
#include <optional>

import physics.manager;

namespace epochengine::physics::contract
{
    namespace
    {
        [[nodiscard]] TemporalAddress address(std::int64_t tick) noexcept
        {
            return {
                .timeline = 7,
                .branch = 11,
                .tick = tick,
                .domain = TemporalDomain::physics
            };
        }
    }

    [[nodiscard]] int run_manager_contract()
    {
        PhysicsManager manager{ {
            .initial_time = address(100),
            .fixed_step_ticks = 5,
            .maximum_steps_per_advance = 4,
            .maximum_bodies = 2,
            .maximum_queued_commands = 3
        } };

        const BodyDescriptor dynamicBody{
            .motion = BodyMotionType::dynamic_body,
            .mass_kilograms = 2.0,
            .stable_user_id = 42
        };
        const CreateBodyResult created =
            manager.create_body(dynamicBody, {}, address(100));
        if (!created || !manager.contains(created.body))
            return 1;

        const PhysicsManagerSnapshot before = manager.snapshot();
        if (!manager.enqueue_set_linear_velocity(
                created.body,
                { 1.0, 0.0, 0.0 },
                address(105))
            || !manager.enqueue_apply_linear_impulse(
                created.body,
                { 2.0, 0.0, 0.0 },
                address(105))
            || !manager.enqueue_destroy(created.body, address(110)))
        {
            return 2;
        }

        if (manager.enqueue_set_enabled(
                created.body,
                false,
                address(110)).code != ResultCode::queue_full)
        {
            return 3;
        }

        const AdvanceResult first = manager.advance({
            .from = address(100),
            .to = address(105),
            .direction = TemporalDirection::forward
        });
        if (!first
            || first.steps_committed != 1
            || first.command_outcomes.size() != 2)
        {
            return 4;
        }

        const std::optional<BodySnapshot> state =
            manager.body(created.body);
        if (!state
            || state->state.linear_velocity
                != Vector3{ 2.0, 0.0, 0.0 }
            || state->state.transform.position != Vector3{})
        {
            return 5;
        }

        const AdvanceResult second = manager.advance({
            .from = address(105),
            .to = address(110),
            .direction = TemporalDirection::forward
        });
        if (!second || manager.contains(created.body))
            return 6;

        if (manager.enqueue_set_enabled(
                created.body,
                false,
                address(115)).code != ResultCode::invalid_handle)
        {
            return 7;
        }

        if (manager.restore(before) != ResultCode::success
            || !manager.contains(created.body)
            || manager.current_time() != address(100))
        {
            return 8;
        }

        const PhysicsManagerSnapshot restored = manager.snapshot();
        if (!restored.pending_commands.empty()
            || restored.metrics.active_bodies != 1)
        {
            return 9;
        }

        if (manager.advance({
                .from = address(100),
                .to = address(106),
                .direction = TemporalDirection::forward
            }).code != ResultCode::invalid_temporal_input)
        {
            return 10;
        }

        if (manager.advance({
                .from = address(100),
                .to = address(105),
                .direction = TemporalDirection::backward
            }).code != ResultCode::unsupported_direction)
        {
            return 11;
        }

        const CreateBodyResult secondBody = manager.create_body(
            {
                .motion = BodyMotionType::static_body,
                .mass_kilograms = 0.0
            },
            {},
            address(100));
        if (!secondBody)
            return 12;

        if (manager.create_body(
                dynamicBody,
                {},
                address(100)).code != ResultCode::capacity_exceeded)
        {
            return 13;
        }

        PhysicsManagerSnapshot malformed = before;
        malformed.pending_commands.push_back({
            .kind = BodyCommandKind::set_linear_velocity,
            .body = created.body,
            .execute_at = address(105),
            .vector = {(std::numeric_limits<double>::quiet_NaN)(), 0.0, 0.0},
            .sequence = 1
        });
        malformed.next_command_sequence = 2;
        if (manager.restore(malformed) != ResultCode::invalid_snapshot)
            return 14;

        return 0;
    }
}

#if defined(EPOCH_PHYSICS_MANAGER_CONTRACT_MAIN)
int main()
{
    return epochengine::physics::contract::run_manager_contract();
}
#endif
