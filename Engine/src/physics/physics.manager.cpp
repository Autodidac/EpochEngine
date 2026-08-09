/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

module physics.manager;

namespace epochengine::physics
{
    namespace
    {
        [[nodiscard]] bool finite(double value) noexcept
        {
            return std::isfinite(value);
        }

        [[nodiscard]] bool finite(const Vector3& value) noexcept
        {
            return finite(value.x) && finite(value.y) && finite(value.z);
        }

        [[nodiscard]] bool finite(const Quaternion& value) noexcept
        {
            return finite(value.x)
                && finite(value.y)
                && finite(value.z)
                && finite(value.w);
        }

        [[nodiscard]] bool zero(const Vector3& value) noexcept
        {
            return value.x == 0.0 && value.y == 0.0 && value.z == 0.0;
        }

        [[nodiscard]] bool valid_address(const TemporalAddress& address) noexcept
        {
            return address.domain == TemporalDomain::physics;
        }

        [[nodiscard]] bool same_stream(
            const TemporalAddress& left,
            const TemporalAddress& right) noexcept
        {
            return left.timeline == right.timeline
                && left.branch == right.branch
                && left.domain == right.domain;
        }

        [[nodiscard]] bool valid_transform(const Transform& transform) noexcept
        {
            if (!finite(transform.position) || !finite(transform.orientation))
                return false;

            const auto& rotation = transform.orientation;
            const double magnitudeSquared =
                rotation.x * rotation.x
                + rotation.y * rotation.y
                + rotation.z * rotation.z
                + rotation.w * rotation.w;
            return finite(magnitudeSquared)
                && magnitudeSquared > (std::numeric_limits<double>::epsilon)();
        }

        [[nodiscard]] Quaternion normalized(Quaternion value) noexcept
        {
            const double magnitude = std::sqrt(
                value.x * value.x
                + value.y * value.y
                + value.z * value.z
                + value.w * value.w);
            value.x /= magnitude;
            value.y /= magnitude;
            value.z /= magnitude;
            value.w /= magnitude;
            return value;
        }

        [[nodiscard]] Transform canonicalized(Transform value) noexcept
        {
            value.orientation = normalized(value.orientation);
            return value;
        }

        [[nodiscard]] bool valid_descriptor(
            const BodyDescriptor& descriptor) noexcept
        {
            if (!finite(descriptor.mass_kilograms)
                || !finite(descriptor.gravity_scale)
                || !finite(descriptor.linear_damping)
                || !finite(descriptor.angular_damping)
                || descriptor.linear_damping < 0.0
                || descriptor.angular_damping < 0.0)
            {
                return false;
            }

            if (descriptor.motion == BodyMotionType::dynamic_body)
                return descriptor.mass_kilograms > 0.0;

            return descriptor.mass_kilograms == 0.0;
        }

        [[nodiscard]] bool valid_initial_state(
            const BodyDescriptor& descriptor,
            const BodyInitialState& state) noexcept
        {
            if (!valid_transform(state.transform)
                || !finite(state.linear_velocity)
                || !finite(state.angular_velocity))
            {
                return false;
            }

            return descriptor.motion != BodyMotionType::static_body
                || (zero(state.linear_velocity) && zero(state.angular_velocity));
        }

        [[nodiscard]] bool valid_body_state(
            const BodyDescriptor& descriptor,
            const BodyState& state) noexcept
        {
            if (!valid_transform(state.transform)
                || !finite(state.linear_velocity)
                || !finite(state.angular_velocity)
                || !valid_address(state.last_changed_at))
            {
                return false;
            }

            return descriptor.motion != BodyMotionType::static_body
                || (zero(state.linear_velocity) && zero(state.angular_velocity));
        }

        [[nodiscard]] bool valid_snapshot_command(
            const BodyCommand& command,
            const std::vector<BodySlotSnapshot>& slots) noexcept
        {
            if (!command.body || command.body.index >= slots.size())
                return false;

            const BodySlotSnapshot& slot = slots[command.body.index];
            if (!slot.occupied || slot.generation != command.body.generation)
                return false;

            switch (command.kind)
            {
            case BodyCommandKind::destroy_body:
            case BodyCommandKind::set_enabled:
                return true;
            case BodyCommandKind::set_transform:
                return valid_transform(command.transform);
            case BodyCommandKind::set_linear_velocity:
            case BodyCommandKind::set_angular_velocity:
                return finite(command.vector)
                    && (slot.descriptor.motion != BodyMotionType::static_body
                        || zero(command.vector));
            case BodyCommandKind::apply_linear_impulse:
                return finite(command.vector)
                    && slot.descriptor.motion == BodyMotionType::dynamic_body;
            default:
                return false;
            }
        }

        [[nodiscard]] bool command_less(
            const BodyCommand& left,
            const BodyCommand& right) noexcept
        {
            if (left.execute_at.tick != right.execute_at.tick)
                return left.execute_at.tick < right.execute_at.tick;
            return left.sequence < right.sequence;
        }

        [[nodiscard]] std::uint32_t next_generation(
            std::uint32_t generation) noexcept
        {
            ++generation;
            return generation == 0 ? 1 : generation;
        }

        [[nodiscard]] bool checked_add(
            const Vector3& left,
            const Vector3& right,
            Vector3& result) noexcept
        {
            result = {
                left.x + right.x,
                left.y + right.y,
                left.z + right.z
            };
            return finite(result);
        }
    }

    struct PhysicsManager::Impl final
    {
        struct Slot final
        {
            std::uint32_t generation{ 1 };
            bool occupied{};
            BodyDescriptor descriptor{};
            BodyState state{};
        };

        explicit Impl(FixedStepConfiguration value)
            : configuration(std::move(value))
            , current_time(configuration.initial_time)
        {
            slots.reserve(configuration.maximum_bodies);
            pending_commands.reserve(configuration.maximum_queued_commands);
        }

        [[nodiscard]] bool aligned_tick(std::int64_t tick) const noexcept
        {
            if (tick < configuration.initial_time.tick)
                return false;
            return (tick - configuration.initial_time.tick)
                % configuration.fixed_step_ticks == 0;
        }

        [[nodiscard]] ResultCode validate_time(
            const TemporalAddress& address) const noexcept
        {
            if (!valid_address(address))
                return ResultCode::invalid_temporal_input;
            if (!same_stream(address, current_time))
                return ResultCode::temporal_mismatch;
            if (address.tick < current_time.tick)
                return ResultCode::temporal_regression;
            if (!aligned_tick(address.tick))
                return ResultCode::invalid_temporal_input;
            return ResultCode::success;
        }

        [[nodiscard]] Slot* resolve(BodyHandle body) noexcept
        {
            if (!body || body.index >= slots.size())
                return nullptr;

            Slot& slot = slots[body.index];
            return slot.occupied && slot.generation == body.generation
                ? &slot
                : nullptr;
        }

        [[nodiscard]] const Slot* resolve(BodyHandle body) const noexcept
        {
            if (!body || body.index >= slots.size())
                return nullptr;

            const Slot& slot = slots[body.index];
            return slot.occupied && slot.generation == body.generation
                ? &slot
                : nullptr;
        }

        void refresh_gauges() noexcept
        {
            metrics.allocated_slots =
                static_cast<std::uint32_t>(slots.size());
            metrics.queued_commands =
                static_cast<std::uint32_t>(pending_commands.size());
        }

        FixedStepConfiguration configuration{};
        TemporalAddress current_time{};
        std::uint64_t fixed_step_index{};
        std::uint64_t next_command_sequence{ 1 };
        std::vector<Slot> slots{};
        std::vector<BodyCommand> pending_commands{};
        PhysicsMetrics metrics{};
        mutable std::mutex mutex{};
    };

    ResultCode PhysicsManager::validate_configuration(
        const FixedStepConfiguration& configuration) noexcept
    {
        if (!valid_address(configuration.initial_time)
            || configuration.fixed_step_ticks <= 0
            || configuration.maximum_steps_per_advance == 0
            || configuration.maximum_bodies == 0
            || configuration.maximum_queued_commands == 0)
        {
            return ResultCode::invalid_configuration;
        }

        const std::int64_t maximumStepCount =
            static_cast<std::int64_t>(configuration.maximum_steps_per_advance);
        if (configuration.fixed_step_ticks
            > (std::numeric_limits<std::int64_t>::max)() / maximumStepCount)
        {
            return ResultCode::invalid_configuration;
        }

        return ResultCode::success;
    }

    PhysicsManager::PhysicsManager(FixedStepConfiguration configuration)
    {
        if (validate_configuration(configuration)
            != ResultCode::success)
        {
            throw std::invalid_argument(
                "PhysicsManager received an invalid fixed-step configuration.");
        }

        impl_ = std::make_unique<Impl>(std::move(configuration));
    }

    PhysicsManager::~PhysicsManager() = default;
    PhysicsManager::PhysicsManager(PhysicsManager&&) noexcept = default;
    PhysicsManager& PhysicsManager::operator=(PhysicsManager&&) noexcept = default;

    FixedStepConfiguration PhysicsManager::configuration() const
    {
        std::scoped_lock lock{ impl_->mutex };
        return impl_->configuration;
    }

    TemporalAddress PhysicsManager::current_time() const
    {
        std::scoped_lock lock{ impl_->mutex };
        return impl_->current_time;
    }

    PhysicsMetrics PhysicsManager::metrics() const
    {
        std::scoped_lock lock{ impl_->mutex };
        return impl_->metrics;
    }

    CreateBodyResult PhysicsManager::create_body(
        const BodyDescriptor& descriptor,
        const BodyInitialState& initial_state,
        const TemporalAddress& at)
    {
        std::scoped_lock lock{ impl_->mutex };

        const ResultCode timeCode = impl_->validate_time(at);
        if (timeCode != ResultCode::success)
            return { .code = timeCode };
        if (at.tick != impl_->current_time.tick)
            return { .code = ResultCode::invalid_temporal_input };
        if (!valid_descriptor(descriptor)
            || !valid_initial_state(descriptor, initial_state))
        {
            return { .code = ResultCode::invalid_descriptor };
        }

        std::uint32_t slotIndex = BodyHandle::invalid_index;
        for (std::uint32_t index = 0;
            index < static_cast<std::uint32_t>(impl_->slots.size());
            ++index)
        {
            if (!impl_->slots[index].occupied)
            {
                slotIndex = index;
                break;
            }
        }

        if (slotIndex == BodyHandle::invalid_index)
        {
            if (impl_->slots.size() >= impl_->configuration.maximum_bodies)
                return { .code = ResultCode::capacity_exceeded };

            slotIndex = static_cast<std::uint32_t>(impl_->slots.size());
            impl_->slots.emplace_back();
        }

        Impl::Slot& slot = impl_->slots[slotIndex];
        slot.occupied = true;
        slot.descriptor = descriptor;
        slot.state = {
            .transform = canonicalized(initial_state.transform),
            .linear_velocity = initial_state.linear_velocity,
            .angular_velocity = initial_state.angular_velocity,
            .enabled = initial_state.enabled,
            .revision = 1,
            .last_changed_at = at
        };

        ++impl_->metrics.active_bodies;
        ++impl_->metrics.bodies_created_total;
        impl_->metrics.peak_active_bodies = (std::max)(
            impl_->metrics.peak_active_bodies,
            impl_->metrics.active_bodies);
        impl_->refresh_gauges();

        return {
            .code = ResultCode::success,
            .body = {
                .index = slotIndex,
                .generation = slot.generation
            }
        };
    }

    bool PhysicsManager::contains(BodyHandle body) const
    {
        std::scoped_lock lock{ impl_->mutex };
        return impl_->resolve(body) != nullptr;
    }

    std::optional<BodySnapshot> PhysicsManager::body(
        BodyHandle bodyHandle) const
    {
        std::scoped_lock lock{ impl_->mutex };
        const Impl::Slot* slot = impl_->resolve(bodyHandle);
        if (!slot)
            return std::nullopt;

        return BodySnapshot{
            .handle = bodyHandle,
            .descriptor = slot->descriptor,
            .state = slot->state
        };
    }

    std::vector<BodySnapshot> PhysicsManager::bodies() const
    {
        std::scoped_lock lock{ impl_->mutex };
        std::vector<BodySnapshot> result{};
        result.reserve(impl_->metrics.active_bodies);

        for (std::uint32_t index = 0;
            index < static_cast<std::uint32_t>(impl_->slots.size());
            ++index)
        {
            const Impl::Slot& slot = impl_->slots[index];
            if (!slot.occupied)
                continue;

            result.push_back({
                .handle = {
                    .index = index,
                    .generation = slot.generation
                },
                .descriptor = slot.descriptor,
                .state = slot.state
            });
        }

        return result;
    }

    EnqueueResult PhysicsManager::enqueue(BodyCommand command)
    {
        std::scoped_lock lock{ impl_->mutex };

        if (command.sequence != 0)
            return { .code = ResultCode::invalid_command };

        const ResultCode timeCode = impl_->validate_time(command.execute_at);
        if (timeCode != ResultCode::success)
            return { .code = timeCode };

        const Impl::Slot* slot = impl_->resolve(command.body);
        if (!slot)
            return { .code = ResultCode::invalid_handle };

        switch (command.kind)
        {
        case BodyCommandKind::destroy_body:
        case BodyCommandKind::set_enabled:
            break;
        case BodyCommandKind::set_transform:
            if (!valid_transform(command.transform))
                return { .code = ResultCode::invalid_command };
            command.transform = canonicalized(command.transform);
            break;
        case BodyCommandKind::set_linear_velocity:
        case BodyCommandKind::set_angular_velocity:
            if (!finite(command.vector))
                return { .code = ResultCode::invalid_command };
            if (slot->descriptor.motion == BodyMotionType::static_body
                && !zero(command.vector))
            {
                return { .code = ResultCode::unsupported_body_operation };
            }
            break;
        case BodyCommandKind::apply_linear_impulse:
            if (!finite(command.vector))
                return { .code = ResultCode::invalid_command };
            if (slot->descriptor.motion != BodyMotionType::dynamic_body)
                return { .code = ResultCode::unsupported_body_operation };
            break;
        default:
            return { .code = ResultCode::invalid_command };
        }

        if (impl_->pending_commands.size()
            >= impl_->configuration.maximum_queued_commands)
        {
            return { .code = ResultCode::queue_full };
        }
        if (impl_->next_command_sequence
            == (std::numeric_limits<std::uint64_t>::max)())
        {
            return { .code = ResultCode::sequence_exhausted };
        }

        command.sequence = impl_->next_command_sequence++;
        const auto insertionPoint = std::upper_bound(
            impl_->pending_commands.begin(),
            impl_->pending_commands.end(),
            command,
            command_less);
        impl_->pending_commands.insert(insertionPoint, command);

        ++impl_->metrics.commands_queued_total;
        impl_->refresh_gauges();
        impl_->metrics.peak_queued_commands = (std::max)(
            impl_->metrics.peak_queued_commands,
            impl_->metrics.queued_commands);

        return {
            .code = ResultCode::success,
            .sequence = command.sequence
        };
    }

    EnqueueResult PhysicsManager::enqueue_destroy(
        BodyHandle body,
        const TemporalAddress& execute_at)
    {
        return enqueue({
            .kind = BodyCommandKind::destroy_body,
            .body = body,
            .execute_at = execute_at
        });
    }

    EnqueueResult PhysicsManager::enqueue_set_transform(
        BodyHandle body,
        const Transform& transform,
        const TemporalAddress& execute_at)
    {
        return enqueue({
            .kind = BodyCommandKind::set_transform,
            .body = body,
            .execute_at = execute_at,
            .transform = transform
        });
    }

    EnqueueResult PhysicsManager::enqueue_set_linear_velocity(
        BodyHandle body,
        const Vector3& velocity,
        const TemporalAddress& execute_at)
    {
        return enqueue({
            .kind = BodyCommandKind::set_linear_velocity,
            .body = body,
            .execute_at = execute_at,
            .vector = velocity
        });
    }

    EnqueueResult PhysicsManager::enqueue_set_angular_velocity(
        BodyHandle body,
        const Vector3& velocity,
        const TemporalAddress& execute_at)
    {
        return enqueue({
            .kind = BodyCommandKind::set_angular_velocity,
            .body = body,
            .execute_at = execute_at,
            .vector = velocity
        });
    }

    EnqueueResult PhysicsManager::enqueue_set_enabled(
        BodyHandle body,
        bool enabled,
        const TemporalAddress& execute_at)
    {
        return enqueue({
            .kind = BodyCommandKind::set_enabled,
            .body = body,
            .execute_at = execute_at,
            .enabled = enabled
        });
    }

    EnqueueResult PhysicsManager::enqueue_apply_linear_impulse(
        BodyHandle body,
        const Vector3& impulse,
        const TemporalAddress& execute_at)
    {
        return enqueue({
            .kind = BodyCommandKind::apply_linear_impulse,
            .body = body,
            .execute_at = execute_at,
            .vector = impulse
        });
    }

    AdvanceResult PhysicsManager::advance(const FixedStepRequest& request)
    {
        std::scoped_lock lock{ impl_->mutex };
        AdvanceResult result{
            .current_time = impl_->current_time
        };

        if (!valid_address(request.from) || !valid_address(request.to))
        {
            result.code = ResultCode::invalid_temporal_input;
            return result;
        }
        if (!same_stream(request.from, request.to)
            || !same_stream(request.from, impl_->current_time))
        {
            result.code = ResultCode::temporal_mismatch;
            return result;
        }
        if (request.from != impl_->current_time)
        {
            result.code = request.from.tick < impl_->current_time.tick
                ? ResultCode::temporal_regression
                : ResultCode::temporal_mismatch;
            return result;
        }
        if (request.direction != TemporalDirection::forward)
        {
            result.code = ResultCode::unsupported_direction;
            return result;
        }
        if (request.to.tick <= request.from.tick)
        {
            result.code = ResultCode::temporal_regression;
            return result;
        }

        const std::int64_t maximumAdvance =
            impl_->configuration.fixed_step_ticks
            * static_cast<std::int64_t>(
                impl_->configuration.maximum_steps_per_advance);
        if (request.from.tick
            > (std::numeric_limits<std::int64_t>::max)() - maximumAdvance)
        {
            result.code = ResultCode::step_limit_exceeded;
            return result;
        }
        if (request.to.tick > request.from.tick + maximumAdvance)
        {
            result.code = ResultCode::step_limit_exceeded;
            return result;
        }

        const std::int64_t tickDistance =
            request.to.tick - request.from.tick;
        if (tickDistance % impl_->configuration.fixed_step_ticks != 0)
        {
            result.code = ResultCode::invalid_temporal_input;
            return result;
        }

        const auto stepCount = static_cast<std::uint32_t>(
            tickDistance / impl_->configuration.fixed_step_ticks);
        result.command_outcomes.reserve(impl_->pending_commands.size());

        std::size_t commandCursor = 0;
        for (std::uint32_t step = 0; step < stepCount; ++step)
        {
            const std::int64_t boundaryTick =
                impl_->current_time.tick
                + impl_->configuration.fixed_step_ticks;
            TemporalAddress boundary = impl_->current_time;
            boundary.tick = boundaryTick;

            while (commandCursor < impl_->pending_commands.size()
                && impl_->pending_commands[commandCursor].execute_at.tick
                    <= boundaryTick)
            {
                const BodyCommand& command =
                    impl_->pending_commands[commandCursor++];
                CommandOutcome outcome{
                    .sequence = command.sequence,
                    .body = command.body,
                    .kind = command.kind
                };

                Impl::Slot* slot = impl_->resolve(command.body);
                if (!slot)
                {
                    outcome.code = ResultCode::invalid_handle;
                    ++impl_->metrics.commands_rejected_total;
                    result.command_outcomes.push_back(outcome);
                    continue;
                }

                switch (command.kind)
                {
                case BodyCommandKind::destroy_body:
                    slot->occupied = false;
                    slot->descriptor = {};
                    slot->state = {};
                    slot->generation = next_generation(slot->generation);
                    --impl_->metrics.active_bodies;
                    ++impl_->metrics.bodies_destroyed_total;
                    break;
                case BodyCommandKind::set_transform:
                    slot->state.transform = command.transform;
                    break;
                case BodyCommandKind::set_linear_velocity:
                    slot->state.linear_velocity = command.vector;
                    break;
                case BodyCommandKind::set_angular_velocity:
                    slot->state.angular_velocity = command.vector;
                    break;
                case BodyCommandKind::set_enabled:
                    slot->state.enabled = command.enabled;
                    break;
                case BodyCommandKind::apply_linear_impulse:
                {
                    const double inverseMass =
                        1.0 / slot->descriptor.mass_kilograms;
                    const Vector3 velocityDelta{
                        command.vector.x * inverseMass,
                        command.vector.y * inverseMass,
                        command.vector.z * inverseMass
                    };
                    Vector3 newVelocity{};
                    if (!checked_add(
                        slot->state.linear_velocity,
                        velocityDelta,
                        newVelocity))
                    {
                        outcome.code = ResultCode::invalid_command;
                        ++impl_->metrics.commands_rejected_total;
                        result.command_outcomes.push_back(outcome);
                        continue;
                    }
                    slot->state.linear_velocity = newVelocity;
                    break;
                }
                default:
                    outcome.code = ResultCode::invalid_command;
                    ++impl_->metrics.commands_rejected_total;
                    result.command_outcomes.push_back(outcome);
                    continue;
                }

                if (command.kind != BodyCommandKind::destroy_body)
                {
                    ++slot->state.revision;
                    slot->state.last_changed_at = boundary;
                }
                ++impl_->metrics.commands_applied_total;
                result.command_outcomes.push_back(outcome);
            }

            impl_->current_time = boundary;
            ++impl_->fixed_step_index;
            ++impl_->metrics.fixed_steps_committed;
            ++result.steps_committed;
        }

        if (commandCursor != 0)
        {
            impl_->pending_commands.erase(
                impl_->pending_commands.begin(),
                impl_->pending_commands.begin()
                    + static_cast<std::ptrdiff_t>(commandCursor));
        }

        impl_->refresh_gauges();
        result.current_time = impl_->current_time;
        return result;
    }

    PhysicsManagerSnapshot PhysicsManager::snapshot() const
    {
        std::scoped_lock lock{ impl_->mutex };
        PhysicsManagerSnapshot result{
            .configuration = impl_->configuration,
            .current_time = impl_->current_time,
            .fixed_step_index = impl_->fixed_step_index,
            .next_command_sequence = impl_->next_command_sequence,
            .pending_commands = impl_->pending_commands,
            .metrics = impl_->metrics
        };
        result.body_slots.reserve(impl_->slots.size());

        for (const Impl::Slot& slot : impl_->slots)
        {
            result.body_slots.push_back({
                .generation = slot.generation,
                .occupied = slot.occupied,
                .descriptor = slot.descriptor,
                .state = slot.state
            });
        }

        return result;
    }

    ResultCode PhysicsManager::restore(
        const PhysicsManagerSnapshot& snapshotValue)
    {
        std::scoped_lock lock{ impl_->mutex };

        if (snapshotValue.configuration != impl_->configuration
            || !valid_address(snapshotValue.current_time)
            || !same_stream(
                snapshotValue.current_time,
                impl_->configuration.initial_time)
            || snapshotValue.current_time.tick
                < impl_->configuration.initial_time.tick
            || !impl_->aligned_tick(snapshotValue.current_time.tick)
            || snapshotValue.body_slots.size()
                > impl_->configuration.maximum_bodies
            || snapshotValue.pending_commands.size()
                > impl_->configuration.maximum_queued_commands
            || snapshotValue.next_command_sequence == 0)
        {
            return ResultCode::invalid_snapshot;
        }

        const std::int64_t elapsedTicks =
            snapshotValue.current_time.tick
            - impl_->configuration.initial_time.tick;
        const std::uint64_t expectedStepIndex =
            static_cast<std::uint64_t>(
                elapsedTicks / impl_->configuration.fixed_step_ticks);
        if (snapshotValue.fixed_step_index != expectedStepIndex)
            return ResultCode::invalid_snapshot;

        std::uint32_t activeBodyCount = 0;
        for (const BodySlotSnapshot& slot : snapshotValue.body_slots)
        {
            if (slot.generation == 0)
                return ResultCode::invalid_snapshot;
            if (!slot.occupied)
                continue;

            if (!valid_descriptor(slot.descriptor)
                || !valid_body_state(slot.descriptor, slot.state)
                || !same_stream(
                    slot.state.last_changed_at,
                    snapshotValue.current_time)
                || slot.state.last_changed_at.tick
                    > snapshotValue.current_time.tick
                || !impl_->aligned_tick(slot.state.last_changed_at.tick))
            {
                return ResultCode::invalid_snapshot;
            }
            ++activeBodyCount;
        }

        std::uint64_t previousSequence = 0;
        std::int64_t previousTick =
            (std::numeric_limits<std::int64_t>::min)();
        for (const BodyCommand& command : snapshotValue.pending_commands)
        {
            if (!command.body
                || command.body.index >= snapshotValue.body_slots.size()
                || !valid_snapshot_command(command, snapshotValue.body_slots)
                || command.sequence == 0
                || command.sequence >= snapshotValue.next_command_sequence
                || !valid_address(command.execute_at)
                || !same_stream(
                    command.execute_at,
                    snapshotValue.current_time)
                || command.execute_at.tick < snapshotValue.current_time.tick
                || !impl_->aligned_tick(command.execute_at.tick))
            {
                return ResultCode::invalid_snapshot;
            }

            if (command.execute_at.tick < previousTick
                || (command.execute_at.tick == previousTick
                    && command.sequence <= previousSequence))
            {
                return ResultCode::invalid_snapshot;
            }
            previousTick = command.execute_at.tick;
            previousSequence = command.sequence;
        }

        std::vector<Impl::Slot> restoredSlots{};
        restoredSlots.reserve(impl_->configuration.maximum_bodies);
        for (const BodySlotSnapshot& slot : snapshotValue.body_slots)
        {
            restoredSlots.push_back({
                .generation = slot.generation,
                .occupied = slot.occupied,
                .descriptor = slot.descriptor,
                .state = slot.state
            });
        }

        impl_->current_time = snapshotValue.current_time;
        impl_->fixed_step_index = snapshotValue.fixed_step_index;
        impl_->next_command_sequence =
            snapshotValue.next_command_sequence;
        impl_->slots = std::move(restoredSlots);
        impl_->pending_commands = snapshotValue.pending_commands;
        impl_->metrics = snapshotValue.metrics;
        impl_->metrics.active_bodies = activeBodyCount;
        impl_->metrics.peak_active_bodies = (std::max)(
            impl_->metrics.peak_active_bodies,
            activeBodyCount);
        impl_->refresh_gauges();
        impl_->metrics.peak_queued_commands = (std::max)(
            impl_->metrics.peak_queued_commands,
            impl_->metrics.queued_commands);
        return ResultCode::success;
    }
}
