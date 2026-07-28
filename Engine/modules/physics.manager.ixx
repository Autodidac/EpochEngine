/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

export module physics.manager;

export namespace epochengine::physics
{
    enum class ResultCode : std::uint8_t
    {
        success,
        invalid_configuration,
        invalid_temporal_input,
        temporal_mismatch,
        temporal_regression,
        unsupported_direction,
        step_limit_exceeded,
        capacity_exceeded,
        queue_full,
        sequence_exhausted,
        invalid_handle,
        invalid_descriptor,
        invalid_command,
        unsupported_body_operation,
        invalid_snapshot
    };

    enum class TemporalDomain : std::uint8_t
    {
        real,
        simulation,
        physics,
        presentation,
        animation,
        effects,
        audio,
        network,
        editor,
        replay
    };

    enum class TemporalDirection : std::int8_t
    {
        backward = -1,
        stopped = 0,
        forward = 1
    };

    struct TemporalAddress final
    {
        std::uint64_t timeline{};
        std::uint64_t branch{};
        std::int64_t tick{};
        TemporalDomain domain{ TemporalDomain::physics };

        [[nodiscard]] friend constexpr bool operator==(
            const TemporalAddress&,
            const TemporalAddress&) noexcept = default;
    };

    struct FixedStepConfiguration final
    {
        TemporalAddress initial_time{};
        std::int64_t fixed_step_ticks{ 1 };
        std::uint32_t maximum_steps_per_advance{ 8 };
        std::uint32_t maximum_bodies{ 4096 };
        std::uint32_t maximum_queued_commands{ 16384 };

        [[nodiscard]] friend constexpr bool operator==(
            const FixedStepConfiguration&,
            const FixedStepConfiguration&) noexcept = default;
    };

    struct Vector3 final
    {
        double x{};
        double y{};
        double z{};

        [[nodiscard]] friend constexpr bool operator==(
            const Vector3&,
            const Vector3&) noexcept = default;
    };

    struct Quaternion final
    {
        double x{};
        double y{};
        double z{};
        double w{ 1.0 };

        [[nodiscard]] friend constexpr bool operator==(
            const Quaternion&,
            const Quaternion&) noexcept = default;
    };

    struct Transform final
    {
        Vector3 position{};
        Quaternion orientation{};

        [[nodiscard]] friend constexpr bool operator==(
            const Transform&,
            const Transform&) noexcept = default;
    };

    enum class BodyMotionType : std::uint8_t
    {
        static_body,
        kinematic_body,
        dynamic_body
    };

    struct BodyHandle final
    {
        static constexpr std::uint32_t invalid_index =
            (std::numeric_limits<std::uint32_t>::max)();

        std::uint32_t index{ invalid_index };
        std::uint32_t generation{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return index != invalid_index && generation != 0;
        }

        [[nodiscard]] friend constexpr bool operator==(
            const BodyHandle&,
            const BodyHandle&) noexcept = default;
    };

    struct BodyDescriptor final
    {
        BodyMotionType motion{ BodyMotionType::static_body };
        double mass_kilograms{};
        double gravity_scale{ 1.0 };
        double linear_damping{};
        double angular_damping{};
        std::uint64_t stable_user_id{};

        [[nodiscard]] friend constexpr bool operator==(
            const BodyDescriptor&,
            const BodyDescriptor&) noexcept = default;
    };

    struct BodyInitialState final
    {
        Transform transform{};
        Vector3 linear_velocity{};
        Vector3 angular_velocity{};
        bool enabled{ true };

        [[nodiscard]] friend constexpr bool operator==(
            const BodyInitialState&,
            const BodyInitialState&) noexcept = default;
    };

    struct BodyState final
    {
        Transform transform{};
        Vector3 linear_velocity{};
        Vector3 angular_velocity{};
        bool enabled{ true };
        std::uint64_t revision{};
        TemporalAddress last_changed_at{};

        [[nodiscard]] friend constexpr bool operator==(
            const BodyState&,
            const BodyState&) noexcept = default;
    };

    struct BodySnapshot final
    {
        BodyHandle handle{};
        BodyDescriptor descriptor{};
        BodyState state{};

        [[nodiscard]] friend constexpr bool operator==(
            const BodySnapshot&,
            const BodySnapshot&) noexcept = default;
    };

    enum class BodyCommandKind : std::uint8_t
    {
        destroy_body,
        set_transform,
        set_linear_velocity,
        set_angular_velocity,
        set_enabled,
        apply_linear_impulse
    };

    struct BodyCommand final
    {
        BodyCommandKind kind{ BodyCommandKind::set_transform };
        BodyHandle body{};
        TemporalAddress execute_at{};
        Transform transform{};
        Vector3 vector{};
        bool enabled{ true };
        std::uint64_t sequence{};

        [[nodiscard]] friend constexpr bool operator==(
            const BodyCommand&,
            const BodyCommand&) noexcept = default;
    };

    struct BodySlotSnapshot final
    {
        std::uint32_t generation{ 1 };
        bool occupied{};
        BodyDescriptor descriptor{};
        BodyState state{};

        [[nodiscard]] friend constexpr bool operator==(
            const BodySlotSnapshot&,
            const BodySlotSnapshot&) noexcept = default;
    };

    struct PhysicsMetrics final
    {
        std::uint32_t allocated_slots{};
        std::uint32_t active_bodies{};
        std::uint32_t queued_commands{};
        std::uint32_t peak_active_bodies{};
        std::uint32_t peak_queued_commands{};
        std::uint64_t fixed_steps_committed{};
        std::uint64_t commands_queued_total{};
        std::uint64_t commands_applied_total{};
        std::uint64_t commands_rejected_total{};
        std::uint64_t bodies_created_total{};
        std::uint64_t bodies_destroyed_total{};

        [[nodiscard]] friend constexpr bool operator==(
            const PhysicsMetrics&,
            const PhysicsMetrics&) noexcept = default;
    };

    struct PhysicsManagerSnapshot final
    {
        FixedStepConfiguration configuration{};
        TemporalAddress current_time{};
        std::uint64_t fixed_step_index{};
        std::uint64_t next_command_sequence{ 1 };
        std::vector<BodySlotSnapshot> body_slots{};
        std::vector<BodyCommand> pending_commands{};
        PhysicsMetrics metrics{};

        [[nodiscard]] friend bool operator==(
            const PhysicsManagerSnapshot&,
            const PhysicsManagerSnapshot&) = default;
    };

    struct CreateBodyResult final
    {
        ResultCode code{ ResultCode::success };
        BodyHandle body{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    struct EnqueueResult final
    {
        ResultCode code{ ResultCode::success };
        std::uint64_t sequence{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    struct CommandOutcome final
    {
        std::uint64_t sequence{};
        BodyHandle body{};
        BodyCommandKind kind{ BodyCommandKind::set_transform };
        ResultCode code{ ResultCode::success };
    };

    struct FixedStepRequest final
    {
        TemporalAddress from{};
        TemporalAddress to{};
        TemporalDirection direction{ TemporalDirection::forward };
    };

    struct AdvanceResult final
    {
        ResultCode code{ ResultCode::success };
        TemporalAddress current_time{};
        std::uint32_t steps_committed{};
        std::vector<CommandOutcome> command_outcomes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    class PhysicsManager final
    {
    public:
        /*
         * API invariants:
         * - A body handle resolves only while its slot is occupied and its
         *   generation matches.
         * - Commands are bounded and commit at fixed physics-domain boundaries
         *   in (execute_at.tick, sequence) order.
         * - Advancing commits explicit commands only. No collision solver,
         *   force integration, or hidden wall-clock sampling occurs here.
         * - Snapshots contain canonical manager state, not backend caches or
         *   pointers, and restore is all-or-nothing after validation.
         */
        explicit PhysicsManager(FixedStepConfiguration configuration = {});
        ~PhysicsManager();

        PhysicsManager(PhysicsManager&&) noexcept;
        PhysicsManager& operator=(PhysicsManager&&) noexcept;

        PhysicsManager(const PhysicsManager&) = delete;
        PhysicsManager& operator=(const PhysicsManager&) = delete;

        [[nodiscard]] static ResultCode validate_configuration(
            const FixedStepConfiguration& configuration) noexcept;

        [[nodiscard]] FixedStepConfiguration configuration() const;
        [[nodiscard]] TemporalAddress current_time() const;
        [[nodiscard]] PhysicsMetrics metrics() const;

        [[nodiscard]] CreateBodyResult create_body(
            const BodyDescriptor& descriptor,
            const BodyInitialState& initial_state,
            const TemporalAddress& at);

        [[nodiscard]] bool contains(BodyHandle body) const;
        [[nodiscard]] std::optional<BodySnapshot> body(BodyHandle body) const;
        [[nodiscard]] std::vector<BodySnapshot> bodies() const;

        [[nodiscard]] EnqueueResult enqueue(BodyCommand command);
        [[nodiscard]] EnqueueResult enqueue_destroy(
            BodyHandle body,
            const TemporalAddress& execute_at);
        [[nodiscard]] EnqueueResult enqueue_set_transform(
            BodyHandle body,
            const Transform& transform,
            const TemporalAddress& execute_at);
        [[nodiscard]] EnqueueResult enqueue_set_linear_velocity(
            BodyHandle body,
            const Vector3& velocity,
            const TemporalAddress& execute_at);
        [[nodiscard]] EnqueueResult enqueue_set_angular_velocity(
            BodyHandle body,
            const Vector3& velocity,
            const TemporalAddress& execute_at);
        [[nodiscard]] EnqueueResult enqueue_set_enabled(
            BodyHandle body,
            bool enabled,
            const TemporalAddress& execute_at);
        [[nodiscard]] EnqueueResult enqueue_apply_linear_impulse(
            BodyHandle body,
            const Vector3& impulse,
            const TemporalAddress& execute_at);

        [[nodiscard]] AdvanceResult advance(const FixedStepRequest& request);

        [[nodiscard]] PhysicsManagerSnapshot snapshot() const;
        [[nodiscard]] ResultCode restore(
            const PhysicsManagerSnapshot& snapshot);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
