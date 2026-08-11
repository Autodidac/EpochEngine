/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <string_view>
#include <unordered_set>
#include <utility>

module project.actor2d_runtime;

namespace epochengine::project_actor2d
{
    namespace
    {
        constexpr double maximum_parameter = 1'000'000.0;
        constexpr std::uint32_t maximum_fixed_steps = 32u;

        [[nodiscard]] bool finite(double value) noexcept
        {
            return std::isfinite(value);
        }

        [[nodiscard]] bool valid_rect(const Rect& value) noexcept
        {
            return finite(value.x) && finite(value.y)
                && finite(value.width) && finite(value.height)
                && value.width > 0.0 && value.height > 0.0
                && value.width <= maximum_parameter
                && value.height <= maximum_parameter;
        }

        [[nodiscard]] double approach(
            double current,
            double target,
            double maximumDelta) noexcept
        {
            if (current < target)
                return (std::min)(current + maximumDelta, target);
            if (current > target)
                return (std::max)(current - maximumDelta, target);
            return target;
        }

        [[nodiscard]] physics::Solver2DConfiguration solver_configuration(
            const ActorConfiguration& configuration) noexcept
        {
            physics::Solver2DConfiguration result{};
            result.fixed_step.initial_time = {
                .timeline = 1u,
                .branch = 1u,
                .tick = 0,
                .domain = physics::TemporalDomain::physics
            };
            result.fixed_step.fixed_step_ticks = 1;
            result.fixed_step.maximum_steps_per_advance = maximum_fixed_steps;
            result.fixed_step.maximum_bodies = 16u;
            result.fixed_step.maximum_queued_commands = 256u;
            result.initial_gravity = configuration.movement == MovementMode::platformer
                ? physics::Vector2{0.0, configuration.gravity_y}
                : physics::Vector2{};
            result.seconds_per_tick = 1.0
                / static_cast<double>(configuration.fixed_steps_per_second);
            result.maximum_linear_speed = (std::max)(
                128.0,
                (std::max)(configuration.maximum_fall_speed, configuration.move_speed) * 8.0);
            result.maximum_acceleration = (std::max)(
                128.0,
                (std::max)({
                    std::abs(configuration.gravity_y),
                    configuration.ground_acceleration,
                    configuration.air_acceleration,
                    configuration.ground_deceleration}) * 8.0);
            result.maximum_static_primitives = configuration.maximum_collision_surfaces;
            result.maximum_contacts = (std::max)(
                64u,
                configuration.maximum_collision_surfaces * 2u);
            return result;
        }

        [[nodiscard]] physics::Body2DDescriptor actor_descriptor(
            const ActorConfiguration& configuration,
            double x,
            double y,
            double velocityX = 0.0,
            double velocityY = 0.0) noexcept
        {
            physics::Body2DDescriptor descriptor{};
            descriptor.body.motion = physics::BodyMotionType::dynamic_body;
            descriptor.body.mass_kilograms = 1.0;
            descriptor.body.gravity_scale = configuration.movement
                == MovementMode::platformer ? 1.0 : 0.0;
            descriptor.body.stable_user_id = configuration.stable_actor_id;
            descriptor.initial_state.transform.position = {x, y, 0.0};
            descriptor.initial_state.linear_velocity = {
                velocityX, velocityY, 0.0};
            descriptor.shape = physics::Shape2D::box({
                configuration.width * 0.5,
                configuration.height * 0.5});
            descriptor.filter = {
                configuration.actor_layer_bits,
                configuration.actor_mask_bits};
            return descriptor;
        }

        [[nodiscard]] ResultCode map_solver_code(
            physics::Solver2DCode code) noexcept
        {
            switch (code)
            {
            case physics::Solver2DCode::success:
                return ResultCode::ready;
            case physics::Solver2DCode::paused:
                return ResultCode::paused;
            case physics::Solver2DCode::capacity_exceeded:
            case physics::Solver2DCode::contact_capacity_exceeded:
                return ResultCode::capacity_exceeded;
            case physics::Solver2DCode::invalid_static_map:
            case physics::Solver2DCode::duplicate_static_id:
                return ResultCode::invalid_collision;
            case physics::Solver2DCode::invalid_configuration:
                return ResultCode::invalid_configuration;
            default:
                return ResultCode::solver_failure;
            }
        }

        [[nodiscard]] std::vector<physics::StaticAabbPrimitive2D>
        solver_collision(std::span<const StaticCollision> collision)
        {
            std::vector<physics::StaticAabbPrimitive2D> result{};
            result.reserve(collision.size());
            for (const StaticCollision& surface : collision)
            {
                if (surface.sensor)
                    continue;
                result.push_back({
                    .stable_id = surface.stable_id,
                    .bounds = {
                        .minimum = {surface.bounds.x, surface.bounds.y},
                        .maximum = {
                            surface.bounds.x + surface.bounds.width,
                            surface.bounds.y + surface.bounds.height}},
                    .filter = {surface.layer_bits, surface.mask_bits}
                });
            }
            return result;
        }
    }

    struct ActorRuntime::Impl final
    {
        explicit Impl(ActorConfiguration value)
            : configuration(value)
            , configuration_code(ActorRuntime::validate_configuration(value))
            , solver(solver_configuration(
                configuration_code == ResultCode::ready
                    ? value
                    : ActorConfiguration{}))
        {
            if (configuration_code != ResultCode::ready)
                return;

            const auto spawned = solver.spawn(actor_descriptor(
                configuration,
                configuration.spawn_x,
                configuration.spawn_y));
            if (!spawned)
            {
                configuration_code = ResultCode::solver_failure;
                return;
            }
            actor = spawned.body;
            solver.capture_reset_point();
            sync_actor_state(false);
        }

        [[nodiscard]] bool actor_contact_grounded(
            const physics::ContactRecord2D& contact) const noexcept
        {
            if (contact.first.kind == physics::ContactTarget2DKind::body
                && contact.first.body == actor)
            {
                return contact.normal.y > 0.5;
            }
            if (contact.second.kind == physics::ContactTarget2DKind::body
                && contact.second.body == actor)
            {
                return contact.normal.y < -0.5;
            }
            return false;
        }

        void sync_actor_state(bool countRevision) noexcept
        {
            const auto body = solver.body(actor);
            if (!body)
                return;

            ActorState next = state;
            next.x = body->body.state.transform.position.x;
            next.y = body->body.state.transform.position.y;
            next.velocity_x = body->body.state.linear_velocity.x;
            next.velocity_y = body->body.state.linear_velocity.y;
            next.fixed_tick = static_cast<std::uint64_t>(
                (std::max)(std::int64_t{}, solver.current_time().tick));
            next.paused = solver.paused();
            next.grounded = false;
            const auto contacts = solver.contacts();
            for (const auto& contact : contacts)
            {
                if (actor_contact_grounded(contact))
                {
                    next.grounded = true;
                    break;
                }
            }
            const bool changed = next.x != state.x || next.y != state.y
                || next.velocity_x != state.velocity_x
                || next.velocity_y != state.velocity_y
                || next.grounded != state.grounded
                || next.paused != state.paused
                || next.fixed_tick != state.fixed_tick;
            next.revision = countRevision && changed
                ? state.revision + 1u
                : state.revision;
            state = next;
        }

        ActorConfiguration configuration{};
        ResultCode configuration_code{ResultCode::invalid_configuration};
        physics::Solver2D solver;
        physics::BodyHandle actor{};
        std::vector<StaticCollision> collision{};
        ActorState state{};
        RuntimeMetrics metrics{};
        std::uint64_t last_input_sequence{};
        bool pending_jump{};
    };

    ActorRuntime::ActorRuntime(ActorConfiguration configuration)
        : impl_(std::make_unique<Impl>(std::move(configuration)))
    {
    }

    ActorRuntime::~ActorRuntime() = default;
    ActorRuntime::ActorRuntime(ActorRuntime&&) noexcept = default;
    ActorRuntime& ActorRuntime::operator=(ActorRuntime&&) noexcept = default;

    ResultCode ActorRuntime::validate_configuration(
        const ActorConfiguration& configuration) noexcept
    {
        const bool validMovement = configuration.movement
                == MovementMode::platformer
            || configuration.movement == MovementMode::top_down;
        if (!validMovement || configuration.stable_actor_id == 0u
            || !finite(configuration.spawn_x) || !finite(configuration.spawn_y)
            || !finite(configuration.width) || configuration.width <= 0.0
            || configuration.width > maximum_parameter
            || !finite(configuration.height) || configuration.height <= 0.0
            || configuration.height > maximum_parameter
            || !finite(configuration.move_speed) || configuration.move_speed <= 0.0
            || configuration.move_speed > maximum_parameter
            || !finite(configuration.ground_acceleration)
            || configuration.ground_acceleration <= 0.0
            || configuration.ground_acceleration > maximum_parameter
            || !finite(configuration.air_acceleration)
            || configuration.air_acceleration <= 0.0
            || configuration.air_acceleration > maximum_parameter
            || !finite(configuration.ground_deceleration)
            || configuration.ground_deceleration <= 0.0
            || configuration.ground_deceleration > maximum_parameter
            || !finite(configuration.jump_speed) || configuration.jump_speed < 0.0
            || configuration.jump_speed > maximum_parameter
            || !finite(configuration.gravity_y)
            || std::abs(configuration.gravity_y) > maximum_parameter
            || !finite(configuration.maximum_fall_speed)
            || configuration.maximum_fall_speed <= 0.0
            || configuration.maximum_fall_speed > maximum_parameter
            || configuration.fixed_steps_per_second < 15u
            || configuration.fixed_steps_per_second > 480u
            || configuration.maximum_collision_surfaces == 0u
            || configuration.maximum_collision_surfaces > 1'000'000u
            || configuration.actor_layer_bits == 0u
            || configuration.actor_mask_bits == 0u)
        {
            return ResultCode::invalid_configuration;
        }
        const auto solverConfiguration = solver_configuration(configuration);
        const double halfWidth = configuration.width * 0.5;
        const double halfHeight = configuration.height * 0.5;
        if (configuration.spawn_x - halfWidth
                < solverConfiguration.world_bounds.minimum.x
            || configuration.spawn_y - halfHeight
                < solverConfiguration.world_bounds.minimum.y
            || configuration.spawn_x + halfWidth
                > solverConfiguration.world_bounds.maximum.x
            || configuration.spawn_y + halfHeight
                > solverConfiguration.world_bounds.maximum.y)
        {
            return ResultCode::invalid_configuration;
        }
        return physics::Solver2D::validate_configuration(solverConfiguration)
                == physics::Solver2DCode::success
            ? ResultCode::ready
            : ResultCode::invalid_configuration;
    }

    ResultCode ActorRuntime::validate_collision(
        std::span<const StaticCollision> collision,
        std::uint32_t maximumSurfaces)
    {
        if (maximumSurfaces == 0u || collision.size() > maximumSurfaces)
            return ResultCode::capacity_exceeded;
        std::unordered_set<std::uint64_t> identities{};
        identities.reserve(collision.size());
        for (const StaticCollision& surface : collision)
        {
            if (surface.stable_id == 0u || !valid_rect(surface.bounds)
                || surface.layer_bits == 0u || surface.mask_bits == 0u
                || surface.sensor
                || surface.kind != StaticCollisionKind::solid_box
                || !identities.insert(surface.stable_id).second)
            {
                return ResultCode::invalid_collision;
            }
        }
        return ResultCode::ready;
    }

    ResultCode ActorRuntime::replace_collision(
        std::span<const StaticCollision> collision)
    {
        if (impl_->configuration_code != ResultCode::ready)
            return impl_->configuration_code;
        const ResultCode validated = validate_collision(
            collision, impl_->configuration.maximum_collision_surfaces);
        if (validated != ResultCode::ready)
            return validated;

        std::vector<StaticCollision> staged{collision.begin(), collision.end()};
        const auto physical = solver_collision(staged);

        physics::Solver2D resetSolver{
            solver_configuration(impl_->configuration)};
        const auto resetMap = resetSolver.set_static_map(physical);
        if (resetMap != physics::Solver2DCode::success)
            return map_solver_code(resetMap);
        const auto resetActor = resetSolver.spawn(actor_descriptor(
            impl_->configuration,
            impl_->configuration.spawn_x,
            impl_->configuration.spawn_y));
        if (!resetActor)
            return map_solver_code(resetActor.code);
        resetSolver.capture_reset_point();
        const auto canonicalReset = resetSolver.snapshot().state;

        const auto before = impl_->solver.snapshot();
        bool liveMapChanged = false;
        try
        {
            const auto installed = impl_->solver.set_static_map(physical);
            if (installed != physics::Solver2DCode::success)
                return map_solver_code(installed);
            liveMapChanged = true;

            auto live = impl_->solver.snapshot();
            live.reset_point = canonicalReset;
            const auto resetInstalled = impl_->solver.restore(live);
            if (resetInstalled != physics::Solver2DCode::success)
            {
                (void)impl_->solver.restore(before);
                return map_solver_code(resetInstalled);
            }

            impl_->collision = std::move(staged);
            impl_->metrics.collision_surfaces = static_cast<std::uint32_t>(
                physical.size());
            return ResultCode::ready;
        }
        catch (...)
        {
            if (liveMapChanged)
                (void)impl_->solver.restore(before);
            return ResultCode::solver_failure;
        }
    }
    ResultCode ActorRuntime::reset()
    {
        if (impl_->configuration_code != ResultCode::ready)
            return impl_->configuration_code;
        const auto code = impl_->solver.reset();
        if (code != physics::Solver2DCode::success)
            return map_solver_code(code);
        ++impl_->metrics.resets;
        impl_->pending_jump = false;
        impl_->sync_actor_state(true);
        return ResultCode::ready;
    }

    ResultCode ActorRuntime::set_paused(bool paused)
    {
        if (impl_->configuration_code != ResultCode::ready)
            return impl_->configuration_code;
        if (impl_->solver.paused() != paused)
        {
            impl_->solver.set_paused(paused);
            ++impl_->metrics.pause_transitions;
            impl_->sync_actor_state(true);
        }
        return paused ? ResultCode::paused : ResultCode::ready;
    }

    AdvanceResult ActorRuntime::advance(
        const FixedInputFrame& input,
        std::uint32_t fixedSteps)
    {
        AdvanceResult result{};
        result.state = impl_->state;
        if (impl_->configuration_code != ResultCode::ready)
        {
            result.code = impl_->configuration_code;
            return result;
        }
        if (input.sequence == 0u || input.sequence <= impl_->last_input_sequence
            || !finite(input.move_x) || !finite(input.move_y)
            || std::abs(input.move_x) > 1.0 || std::abs(input.move_y) > 1.0
            || fixedSteps > maximum_fixed_steps)
        {
            ++impl_->metrics.rejected_input_frames;
            result.code = ResultCode::invalid_input;
            return result;
        }

        result.events.reserve(static_cast<std::size_t>(fixedSteps) * 3u + 2u);
        const auto beforeSolver = impl_->solver.snapshot();
        const ActorState beforeState = impl_->state;
        const RuntimeMetrics beforeMetrics = impl_->metrics;
        const std::uint64_t beforeSequence = impl_->last_input_sequence;
        const bool beforePendingJump = impl_->pending_jump;
        const auto rollback = [&](ResultCode failure)
        {
            const auto restored = impl_->solver.restore(beforeSolver);
            impl_->state = beforeState;
            impl_->metrics = beforeMetrics;
            impl_->last_input_sequence = beforeSequence;
            impl_->pending_jump = beforePendingJump;
            return AdvanceResult{
                .code = restored == physics::Solver2DCode::success
                    ? failure
                    : ResultCode::solver_failure,
                .state = impl_->state
            };
        };

        impl_->last_input_sequence = input.sequence;
        ++impl_->metrics.accepted_input_frames;
        impl_->pending_jump = impl_->pending_jump || input.jump_pressed;
        std::uint32_t nextEventOrder{1u};
        const auto appendEvent = [&](ActorEventKind kind, bool enabled = false)
        {
            result.events.push_back({
                .kind = kind,
                .stable_actor_id = impl_->configuration.stable_actor_id,
                .fixed_tick = impl_->state.fixed_tick,
                .order = nextEventOrder++,
                .enabled = enabled
            });
        };
        if (input.pause_pressed)
        {
            const bool requestedPause = !impl_->solver.paused();
            const ResultCode pauseCode = set_paused(requestedPause);
            if (pauseCode != ResultCode::ready
                && pauseCode != ResultCode::paused)
            {
                return rollback(pauseCode);
            }
            appendEvent(ActorEventKind::pause_changed, requestedPause);
        }
        if (input.reset_pressed)
        {
            const ResultCode resetCode = reset();
            if (resetCode != ResultCode::ready)
                return rollback(resetCode);
            appendEvent(ActorEventKind::reset);
            result.code = ResultCode::ready;
            result.state = impl_->state;
            return result;
        }
        if (impl_->solver.paused())
        {
            result.code = ResultCode::paused;
            result.state = impl_->state;
            return result;
        }
        if (fixedSteps == 0u)
        {
            result.code = ResultCode::ready;
            result.state = impl_->state;
            return result;
        }

        const double deltaSeconds = 1.0
            / static_cast<double>(impl_->configuration.fixed_steps_per_second);
        for (std::uint32_t step = 0u; step < fixedSteps; ++step)
        {
            const ActorState beforeStep = impl_->state;
            const bool jumpStarted =
                impl_->configuration.movement == MovementMode::platformer
                && step == 0u
                && impl_->pending_jump
                && beforeStep.grounded;
            const auto body = impl_->solver.body(impl_->actor);
            if (!body)
                return rollback(ResultCode::solver_failure);

            physics::Vector2 velocity{
                body->body.state.linear_velocity.x,
                body->body.state.linear_velocity.y};
            double moveX = input.move_x;
            double moveY = input.move_y;
            if (impl_->configuration.movement == MovementMode::top_down)
            {
                const double length = std::sqrt(moveX * moveX + moveY * moveY);
                if (length > 1.0)
                {
                    moveX /= length;
                    moveY /= length;
                }
                const double acceleration = (moveX == 0.0 && moveY == 0.0)
                    ? impl_->configuration.ground_deceleration
                    : impl_->configuration.ground_acceleration;
                velocity.x = approach(
                    velocity.x,
                    moveX * impl_->configuration.move_speed,
                    acceleration * deltaSeconds);
                velocity.y = approach(
                    velocity.y,
                    -moveY * impl_->configuration.move_speed,
                    acceleration * deltaSeconds);
            }
            else
            {
                const double acceleration = moveX == 0.0
                    ? impl_->configuration.ground_deceleration
                    : (impl_->state.grounded
                        ? impl_->configuration.ground_acceleration
                        : impl_->configuration.air_acceleration);
                velocity.x = approach(
                    velocity.x,
                    moveX * impl_->configuration.move_speed,
                    acceleration * deltaSeconds);
                if (jumpStarted)
                    velocity.y = -impl_->configuration.jump_speed;
                velocity.y = (std::min)(
                    velocity.y,
                    impl_->configuration.maximum_fall_speed);
            }

            const auto from = impl_->solver.current_time();
            auto to = from;
            to.tick += impl_->solver.configuration().fixed_step.fixed_step_ticks;
            const auto queued = impl_->solver.enqueue_set_velocity(
                impl_->actor, velocity, to);
            if (!queued)
                return rollback(ResultCode::solver_failure);

            const auto advanced = impl_->solver.advance({
                .from = from,
                .to = to,
                .direction = physics::TemporalDirection::forward});
            if (!advanced)
                return rollback(map_solver_code(advanced.code));

            result.steps_committed += advanced.steps_committed;
            result.contacts = static_cast<std::uint32_t>(advanced.contacts.size());
            impl_->metrics.contact_events += advanced.contacts.size();
            impl_->metrics.peak_contacts_per_step = (std::max)(
                impl_->metrics.peak_contacts_per_step,
                result.contacts);
            impl_->metrics.fixed_steps += advanced.steps_committed;
            impl_->sync_actor_state(true);
            if (impl_->configuration.movement == MovementMode::platformer)
            {
                if (jumpStarted)
                    appendEvent(ActorEventKind::jump_started);
                if (beforeStep.grounded && !impl_->state.grounded)
                    appendEvent(ActorEventKind::left_ground);
                if (!beforeStep.grounded && impl_->state.grounded)
                    appendEvent(ActorEventKind::landed);
            }
            if (step == 0u)
                impl_->pending_jump = false;
        }
        result.code = ResultCode::ready;
        result.state = impl_->state;
        return result;
    }
    ActorConfiguration ActorRuntime::configuration() const noexcept
    {
        return impl_->configuration;
    }

    ActorState ActorRuntime::state() const noexcept
    {
        return impl_->state;
    }

    RuntimeMetrics ActorRuntime::metrics() const noexcept
    {
        return impl_->metrics;
    }

    RuntimeSnapshot ActorRuntime::snapshot() const
    {
        return {
            .configuration = impl_->configuration,
            .collision = impl_->collision,
            .actor = impl_->state,
            .metrics = impl_->metrics,
            .last_input_sequence = impl_->last_input_sequence,
            .pending_jump = impl_->pending_jump,
            .solver = impl_->solver.snapshot()
        };
    }

    ResultCode ActorRuntime::restore(const RuntimeSnapshot& snapshot)
    {
        if (validate_configuration(snapshot.configuration) != ResultCode::ready
            || !finite(snapshot.actor.x)
            || !finite(snapshot.actor.y)
            || !finite(snapshot.actor.velocity_x)
            || !finite(snapshot.actor.velocity_y)
            || snapshot.actor.revision == 0u)
        {
            return ResultCode::invalid_snapshot;
        }

        try
        {
            if (validate_collision(
                    snapshot.collision,
                    snapshot.configuration.maximum_collision_surfaces)
                != ResultCode::ready)
            {
                return ResultCode::invalid_snapshot;
            }

            const auto physical = solver_collision(snapshot.collision);
            if (snapshot.metrics.collision_surfaces != physical.size())
                return ResultCode::invalid_snapshot;

            physics::Solver2D canonicalReset{
                solver_configuration(snapshot.configuration)};
            if (canonicalReset.set_static_map(physical)
                    != physics::Solver2DCode::success)
            {
                return ResultCode::invalid_snapshot;
            }
            const auto resetActor = canonicalReset.spawn(actor_descriptor(
                snapshot.configuration,
                snapshot.configuration.spawn_x,
                snapshot.configuration.spawn_y));
            if (!resetActor)
                return ResultCode::invalid_snapshot;
            canonicalReset.capture_reset_point();
            const auto canonicalResetState = canonicalReset.snapshot().state;
            if (snapshot.solver.reset_point != canonicalResetState)
                return ResultCode::invalid_snapshot;

            physics::Solver2D candidate{
                solver_configuration(snapshot.configuration)};
            if (candidate.restore(snapshot.solver)
                != physics::Solver2DCode::success)
            {
                return ResultCode::invalid_snapshot;
            }
            if (candidate.static_map()
                != canonicalResetState.static_primitives)
            {
                return ResultCode::invalid_snapshot;
            }

            const auto bodies = candidate.bodies();
            if (bodies.size() != 1u)
                return ResultCode::invalid_snapshot;
            const auto& body = bodies.front();
            const auto expectedDescriptor = actor_descriptor(
                snapshot.configuration,
                snapshot.actor.x,
                snapshot.actor.y,
                snapshot.actor.velocity_x,
                snapshot.actor.velocity_y);
            if (body.body.descriptor != expectedDescriptor.body
                || body.shape != expectedDescriptor.shape
                || body.filter != expectedDescriptor.filter)
            {
                return ResultCode::invalid_snapshot;
            }

            Impl candidateState{snapshot.configuration};
            candidateState.solver = std::move(candidate);
            candidateState.actor = body.body.handle;
            candidateState.collision = snapshot.collision;
            candidateState.metrics = snapshot.metrics;
            candidateState.last_input_sequence = snapshot.last_input_sequence;
            candidateState.pending_jump = snapshot.pending_jump;
            candidateState.state = snapshot.actor;
            const ActorState expected = candidateState.state;
            candidateState.sync_actor_state(false);
            if (candidateState.state != expected)
                return ResultCode::invalid_snapshot;

            *impl_ = std::move(candidateState);
            return ResultCode::ready;
        }
        catch (...)
        {
            return ResultCode::invalid_snapshot;
        }
    }
    std::string_view result_code_name(ResultCode code) noexcept
    {
        switch (code)
        {
        case ResultCode::ready: return "ready";
        case ResultCode::paused: return "paused";
        case ResultCode::invalid_configuration: return "invalid_configuration";
        case ResultCode::invalid_input: return "invalid_input";
        case ResultCode::invalid_collision: return "invalid_collision";
        case ResultCode::capacity_exceeded: return "capacity_exceeded";
        case ResultCode::solver_failure: return "solver_failure";
        case ResultCode::invalid_snapshot: return "invalid_snapshot";
        }
        return "unknown";
    }
    std::string_view contract_failure_name(ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "none";
        case ContractFailure::invalid_configuration_accepted: return "invalid_configuration_accepted";
        case ContractFailure::invalid_collision_accepted: return "invalid_collision_accepted";
        case ContractFailure::deterministic_replay_mismatch: return "deterministic_replay_mismatch";
        case ContractFailure::platformer_collision_failed: return "platformer_collision_failed";
        case ContractFailure::top_down_collision_failed: return "top_down_collision_failed";
        case ContractFailure::pause_failed: return "pause_failed";
        case ContractFailure::reset_failed: return "reset_failed";
        case ContractFailure::snapshot_restore_failed: return "snapshot_restore_failed";
        case ContractFailure::contradictory_snapshot_accepted: return "contradictory_snapshot_accepted";
        case ContractFailure::hot_collision_reset_failed: return "hot_collision_reset_failed";
        case ContractFailure::stale_input_accepted: return "stale_input_accepted";
        case ContractFailure::resource_accounting_failed: return "resource_accounting_failed";
        case ContractFailure::actor_event_failed: return "actor_event_failed";
        }
        return "unknown";
    }
}
