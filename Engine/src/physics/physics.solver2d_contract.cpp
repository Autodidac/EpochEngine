/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <limits>
#include <vector>

module physics.solver2d;

namespace epochengine::physics::contract
{
    namespace
    {
        [[nodiscard]] TemporalAddress address(std::int64_t tick) noexcept
        {
            return {
                .timeline = 29,
                .branch = 3,
                .tick = tick,
                .domain = TemporalDomain::physics
            };
        }

        [[nodiscard]] Solver2DConfiguration configuration()
        {
            return {
                .fixed_step = {
                    .initial_time = address(0),
                    .fixed_step_ticks = 1,
                    .maximum_steps_per_advance = 16,
                    .maximum_bodies = 32,
                    .maximum_queued_commands = 64
                },
                .initial_gravity = { 0.0, 10.0 },
                .world_bounds = {
                    .minimum = { -100.0, -100.0 },
                    .maximum = { 100.0, 100.0 }
                },
                .seconds_per_tick = 0.1,
                .maximum_linear_speed = 1'000.0,
                .maximum_acceleration = 1'000.0,
                .contact_slop = 1.0e-8,
                .solver_iterations = 8,
                .maximum_static_primitives = 16,
                .maximum_contacts = 64
            };
        }

        [[nodiscard]] Body2DDescriptor body_descriptor(
            BodyMotionType motion,
            Shape2D shape,
            Vector2 position,
            Vector2 velocity = {},
            CollisionFilter2D filter = {})
        {
            return {
                .body = {
                    .motion = motion,
                    .mass_kilograms = motion == BodyMotionType::dynamic_body
                        ? 1.0
                        : 0.0,
                    .stable_user_id = static_cast<std::uint64_t>(
                        position.x * 10.0 + position.y * 100.0 + 1'000.0)
                },
                .initial_state = {
                    .transform = {
                        .position = { position.x, position.y, 0.0 }
                    },
                    .linear_velocity = { velocity.x, velocity.y, 0.0 }
                },
                .shape = shape,
                .filter = filter
            };
        }

        [[nodiscard]] Solver2DAdvanceResult advance_steps(
            Solver2D& solver,
            std::int64_t steps)
        {
            const TemporalAddress from = solver.current_time();
            TemporalAddress to = from;
            to.tick += steps;
            return solver.advance({
                .from = from,
                .to = to,
                .direction = TemporalDirection::forward
            });
        }

        [[nodiscard]] bool has_map_contact(
            const std::vector<ContactRecord2D>& contacts,
            BodyHandle body,
            std::uint64_t primitiveId) noexcept
        {
            for (const ContactRecord2D& contact : contacts)
            {
                if (contact.first.kind == ContactTarget2DKind::body
                    && contact.first.body == body
                    && contact.second.kind == ContactTarget2DKind::static_map
                    && contact.second.static_primitive_id == primitiveId)
                {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] bool has_body_contact(
            const std::vector<ContactRecord2D>& contacts,
            BodyHandle first,
            BodyHandle second) noexcept
        {
            for (const ContactRecord2D& contact : contacts)
            {
                if (contact.first.kind != ContactTarget2DKind::body
                    || contact.second.kind != ContactTarget2DKind::body)
                {
                    continue;
                }
                if ((contact.first.body == first
                        && contact.second.body == second)
                    || (contact.first.body == second
                        && contact.second.body == first))
                {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] int deterministic_map_replay_contract()
        {
            Solver2D solver{ configuration() };
            const StaticAabbPrimitive2D floor{
                .stable_id = 100,
                .bounds = {
                    .minimum = { -10.0, 2.0 },
                    .maximum = { 10.0, 3.0 }
                },
                .filter = { .layer = 2, .mask = 1 }
            };
            if (solver.set_static_map({ &floor, 1 })
                != Solver2DCode::success)
            {
                return 1;
            }

            Body2DDescriptor falling = body_descriptor(
                BodyMotionType::dynamic_body,
                Shape2D::box({ 0.5, 0.5 }),
                { 0.0, 0.5 },
                {},
                { .layer = 1, .mask = 2 });
            falling.body.stable_user_id = 700;
            const SpawnBody2DResult spawned = solver.spawn(falling);
            if (!spawned)
                return 2;

            const Solver2DSnapshot replayPoint = solver.snapshot();
            const Solver2DAdvanceResult firstRun = advance_steps(solver, 5);
            if (!firstRun
                || firstRun.steps_committed != 5
                || !has_map_contact(firstRun.contacts, spawned.body, 100))
            {
                return 3;
            }
            const Solver2DSnapshot expected = solver.snapshot();
            if (expected.state.contacts.size() != 1
                || expected.state.contacts.front().began_step != 5
                || expected.state.contacts.front().last_step != 5)
            {
                return 4;
            }

            if (solver.restore(replayPoint) != Solver2DCode::success)
                return 5;
            const Solver2DAdvanceResult replay = advance_steps(solver, 5);
            if (!replay || solver.snapshot() != expected)
                return 6;

            const ContactRecord2D stableContact = solver.contacts().front();
            if (!advance_steps(solver, 1))
                return 7;
            const std::vector<ContactRecord2D> continued = solver.contacts();
            if (continued.size() != 1
                || continued.front().id != stableContact.id
                || continued.front().began_step != stableContact.began_step
                || continued.front().last_step != 6)
            {
                return 8;
            }
            return 0;
        }

        [[nodiscard]] int mixed_shape_and_filter_contract()
        {
            Solver2DConfiguration config = configuration();
            config.initial_gravity = {};
            Solver2D solver{ config };

            const SpawnBody2DResult box = solver.spawn(body_descriptor(
                BodyMotionType::static_body,
                Shape2D::box({ 0.5, 0.5 }),
                { 2.0, 0.0 },
                {},
                { .layer = 1, .mask = 2 }));
            const SpawnBody2DResult circle = solver.spawn(body_descriptor(
                BodyMotionType::dynamic_body,
                Shape2D::circle(0.5),
                { 0.0, 0.0 },
                { 5.0, 0.0 },
                { .layer = 2, .mask = 1 }));
            if (!box || !circle)
                return 1;
            if (!advance_steps(solver, 3)
                || !has_body_contact(solver.contacts(), box.body, circle.body))
            {
                return 2;
            }
            const auto circleState = solver.body(circle.body);
            if (!circleState
                || circleState->body.state.linear_velocity.x != 0.0
                || circleState->body.state.transform.position.x >= 1.5)
            {
                return 3;
            }

            Solver2D filtered{ config };
            const SpawnBody2DResult first = filtered.spawn(body_descriptor(
                BodyMotionType::static_body,
                Shape2D::box({ 1.0, 1.0 }),
                {},
                {},
                { .layer = 1, .mask = 2 }));
            const SpawnBody2DResult second = filtered.spawn(body_descriptor(
                BodyMotionType::kinematic_body,
                Shape2D::circle(0.5),
                {},
                {},
                { .layer = 4, .mask = 1 }));
            if (!first || !second || !advance_steps(filtered, 1))
                return 4;
            if (!filtered.contacts().empty())
                return 5;
            return 0;
        }

        [[nodiscard]] int pause_reset_modes_and_handles_contract()
        {
            Solver2DConfiguration config = configuration();
            config.initial_gravity = {};
            Solver2D solver{ config };

            const SpawnBody2DResult fixed = solver.spawn(body_descriptor(
                BodyMotionType::static_body,
                Shape2D::box({ 0.5, 0.5 }),
                { -3.0, 0.0 }));
            const SpawnBody2DResult driven = solver.spawn(body_descriptor(
                BodyMotionType::kinematic_body,
                Shape2D::box({ 0.5, 0.5 }),
                { 0.0, 0.0 },
                { 2.0, 0.0 }));
            const SpawnBody2DResult simulated = solver.spawn(body_descriptor(
                BodyMotionType::dynamic_body,
                Shape2D::circle(0.5),
                { 3.0, 0.0 }));
            if (!fixed || !driven || !simulated)
                return 1;

            solver.capture_reset_point();
            const Solver2DSnapshot resetPoint = solver.snapshot();
            solver.set_paused(true);
            const Solver2DAdvanceResult paused = advance_steps(solver, 1);
            if (paused.code != Solver2DCode::paused
                || paused.steps_committed != 0
                || solver.current_time() != address(0))
            {
                return 2;
            }

            solver.set_paused(false);
            if (!advance_steps(solver, 1))
                return 3;
            const auto fixedAfter = solver.body(fixed.body);
            const auto drivenAfter = solver.body(driven.body);
            if (!fixedAfter || !drivenAfter
                || fixedAfter->body.state.transform.position.x != -3.0
                || drivenAfter->body.state.transform.position.x != 0.2)
            {
                return 4;
            }
            if (solver.reset() != Solver2DCode::success
                || solver.snapshot().state != resetPoint.reset_point
                || solver.bodies().size() != 3)
            {
                return 5;
            }

            if (!solver.enqueue_set_velocity(
                    simulated.body,
                    { 1.0, 0.0 },
                    address(1))
                || !solver.enqueue_destroy(simulated.body, address(1))
                || !advance_steps(solver, 1)
                || solver.contains(simulated.body))
            {
                return 6;
            }
            if (solver.enqueue_set_velocity(
                    simulated.body,
                    {},
                    address(2)).code != ResultCode::invalid_handle)
            {
                return 7;
            }

            const SpawnBody2DResult replacement = solver.spawn(body_descriptor(
                BodyMotionType::dynamic_body,
                Shape2D::circle(0.25),
                { 4.0, 0.0 }));
            if (!replacement
                || replacement.body.index != simulated.body.index
                || replacement.body.generation == simulated.body.generation)
            {
                return 8;
            }

            const Solver2DSnapshot beforeTransient = solver.snapshot();
            const std::size_t expectedBodyCount = solver.bodies().size();
            const SpawnBody2DResult transient = solver.spawn(body_descriptor(
                BodyMotionType::dynamic_body,
                Shape2D::box({ 0.25, 0.25 }),
                { 8.0, 0.0 }));
            if (!transient || solver.bodies().size() != expectedBodyCount + 1)
                return 9;
            if (solver.restore(beforeTransient) != Solver2DCode::success
                || solver.bodies().size() != expectedBodyCount
                || solver.contains(transient.body))
            {
                return 10;
            }
            return 0;
        }

        [[nodiscard]] int bounded_validation_contract()
        {
            Solver2DConfiguration invalid = configuration();
            invalid.initial_gravity.x =
                (std::numeric_limits<double>::quiet_NaN)();
            if (Solver2D::validate_configuration(invalid)
                != Solver2DCode::invalid_configuration)
            {
                return 1;
            }

            Solver2D solver{ configuration() };
            const std::vector<StaticAabbPrimitive2D> duplicate{
                {
                    .stable_id = 5,
                    .bounds = {
                        .minimum = { -1.0, 1.0 },
                        .maximum = { 1.0, 2.0 }
                    }
                },
                {
                    .stable_id = 5,
                    .bounds = {
                        .minimum = { 2.0, 1.0 },
                        .maximum = { 3.0, 2.0 }
                    }
                }
            };
            if (solver.set_static_map(duplicate)
                != Solver2DCode::duplicate_static_id)
            {
                return 2;
            }

            Body2DDescriptor outside = body_descriptor(
                BodyMotionType::dynamic_body,
                Shape2D::circle(1.0),
                { 100.0, 0.0 });
            if (solver.spawn(outside).code
                != Solver2DCode::invalid_descriptor)
            {
                return 3;
            }

            const Solver2DSnapshot valid = solver.snapshot();
            Solver2DSnapshot malformed = valid;
            malformed.state.collider_slots.push_back({});
            if (solver.restore(malformed) != Solver2DCode::invalid_snapshot
                || solver.snapshot() != valid)
            {
                return 4;
            }
            return 0;
        }

        [[nodiscard]] int authored_static_surface_contract()
        {
            Solver2DConfiguration config = configuration();
            config.initial_gravity = {};

            const StaticAabbPrimitive2D oneWay{
                .stable_id = 701,
                .bounds = {
                    .minimum = { -2.0, 2.0 },
                    .maximum = { 2.0, 2.25 }
                },
                .kind = StaticPrimitive2DKind::one_way_up
            };
            Solver2D ascending{ config };
            if (ascending.set_static_map({ &oneWay, 1 })
                    != Solver2DCode::success)
            {
                return 1;
            }
            const auto rising = ascending.spawn(body_descriptor(
                BodyMotionType::dynamic_body,
                Shape2D::box({ 0.25, 0.25 }),
                { 0.0, 2.4 },
                { 0.0, -1.0 }));
            if (!rising || !advance_steps(ascending, 1)
                || has_map_contact(ascending.contacts(), rising.body, 701))
            {
                return 2;
            }

            Solver2D falling{ config };
            if (falling.set_static_map({ &oneWay, 1 })
                    != Solver2DCode::success)
            {
                return 3;
            }
            const auto descending = falling.spawn(body_descriptor(
                BodyMotionType::dynamic_body,
                Shape2D::box({ 0.25, 0.25 }),
                { 0.0, 1.6 },
                { 0.0, 3.0 }));
            if (!descending || !advance_steps(falling, 1)
                || !has_map_contact(falling.contacts(), descending.body, 701))
            {
                return 4;
            }
            const auto landed = falling.body(descending.body);
            if (!landed
                || landed->body.state.linear_velocity.y != 0.0
                || landed->body.state.transform.position.y >= 1.76)
            {
                return 5;
            }

            const auto slopeProof = [&](StaticPrimitive2DKind kind,
                                        std::uint64_t stableId,
                                        double expectedNormalX)
            {
                const StaticAabbPrimitive2D slope{
                    .stable_id = stableId,
                    .bounds = {
                        .minimum = { 0.0, 2.0 },
                        .maximum = { 2.0, 4.0 }
                    },
                    .kind = kind
                };
                Solver2D solver{ config };
                if (solver.set_static_map({ &slope, 1 })
                        != Solver2DCode::success)
                {
                    return 1;
                }
                const auto actor = solver.spawn(body_descriptor(
                    BodyMotionType::dynamic_body,
                    Shape2D::box({ 0.25, 0.25 }),
                    { 1.0, 2.55 },
                    { 0.0, 2.0 }));
                if (!actor)
                    return 2;
                if (!advance_steps(solver, 2))
                    return 3;
                const auto contacts = solver.contacts();
                if (!has_map_contact(contacts, actor.body, stableId))
                    return 4;
                const auto& contact = contacts.front();
                if (contact.normal.y < 0.0)
                    return 5;
                if (contact.normal.y == 0.0)
                    return 6;
                if (contact.normal.y <= 0.5)
                    return 7;
                if (contact.normal.x * expectedNormalX < 0.0)
                    return 8;
                if (contact.normal.x * expectedNormalX == 0.0)
                    return 9;
                if (contact.normal.x * expectedNormalX <= 0.25)
                    return 10;
                if (contact.penetration <= 0.0)
                    return 11;
                return 0;
            };
            if (const int slope = slopeProof(
                    StaticPrimitive2DKind::slope_up_right, 702, 1.0))
                return 60 + slope;
            if (const int slope = slopeProof(
                    StaticPrimitive2DKind::slope_down_right, 703, -1.0))
                return 70 + slope;
            return 0;
        }

        [[nodiscard]] bool restore_rejected_transactionally(
            Solver2D& solver,
            const Solver2DSnapshot& malformed,
            const Solver2DSnapshot& expected)
        {
            return solver.restore(malformed) == Solver2DCode::invalid_snapshot
                && solver.snapshot() == expected;
        }

        [[nodiscard]] int adversarial_contact_snapshot_contract()
        {
            Solver2D solver{ configuration() };
            const StaticAabbPrimitive2D floor{
                .stable_id = 900,
                .bounds = {
                    .minimum = { -5.0, 2.0 },
                    .maximum = { 5.0, 3.0 }
                },
                .filter = { .layer = 2, .mask = 1 }
            };
            if (solver.set_static_map({ &floor, 1 })
                != Solver2DCode::success)
            {
                return 1;
            }

            const SpawnBody2DResult spawned = solver.spawn(body_descriptor(
                BodyMotionType::dynamic_body,
                Shape2D::box({ 0.5, 0.5 }),
                { 0.0, 0.5 },
                {},
                { .layer = 1, .mask = 2 }));
            if (!spawned || !advance_steps(solver, 5))
                return 2;

            const Solver2DSnapshot valid = solver.snapshot();
            if (valid.state.contacts.size() != 1)
                return 3;

            Solver2DSnapshot malformed = valid;
            malformed.state.contacts.front().normal.x =
                (std::numeric_limits<double>::quiet_NaN)();
            if (!restore_rejected_transactionally(solver, malformed, valid))
                return 4;

            malformed = valid;
            malformed.state.contacts.front().normal = { 2.0, 0.0 };
            if (!restore_rejected_transactionally(solver, malformed, valid))
                return 5;

            malformed = valid;
            malformed.state.contacts.front().normal.x *= -1.0;
            malformed.state.contacts.front().normal.y *= -1.0;
            if (!restore_rejected_transactionally(solver, malformed, valid))
                return 6;

            malformed = valid;
            malformed.state.manager.body_slots[spawned.body.index]
                .state.transform.position.x = 20.0;
            if (!restore_rejected_transactionally(solver, malformed, valid))
                return 7;

            malformed = valid;
            malformed.state.manager.body_slots[spawned.body.index]
                .state.enabled = false;
            if (!restore_rejected_transactionally(solver, malformed, valid))
                return 8;

            malformed = valid;
            malformed.state.collider_slots[spawned.body.index].filter.mask = 0;
            if (!restore_rejected_transactionally(solver, malformed, valid))
                return 9;

            malformed = valid;
            malformed.reset_point = malformed.state;
            malformed.reset_point.contacts.front().penetration *= 2.0;
            if (!restore_rejected_transactionally(solver, malformed, valid))
                return 10;

            return 0;
        }
    }

    int run_solver2d_contract()
    {
        const int replay = deterministic_map_replay_contract();
        if (replay != 0)
            return 100 + replay;

        const int shapes = mixed_shape_and_filter_contract();
        if (shapes != 0)
            return 200 + shapes;

        const int controls = pause_reset_modes_and_handles_contract();
        if (controls != 0)
            return 300 + controls;

        const int bounds = bounded_validation_contract();
        if (bounds != 0)
            return 400 + bounds;

        const int surfaces = authored_static_surface_contract();
        if (surfaces != 0)
            return 500 + surfaces;

        const int adversarial = adversarial_contact_snapshot_contract();
        if (adversarial != 0)
            return 600 + adversarial;

        return 0;
    }
}

#if defined(EPOCH_PHYSICS_SOLVER2D_CONTRACT_MAIN)
int main()
{
    return epochengine::physics::contract::run_solver2d_contract();
}
#endif
