/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <compare>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

export module physics.solver2d;

export import physics.manager;

export namespace epochengine::physics
{
    struct Vector2 final
    {
        double x{};
        double y{};

        [[nodiscard]] friend constexpr bool operator==(
            const Vector2&,
            const Vector2&) noexcept = default;
    };

    struct Aabb2 final
    {
        Vector2 minimum{};
        Vector2 maximum{};

        [[nodiscard]] friend constexpr bool operator==(
            const Aabb2&,
            const Aabb2&) noexcept = default;
    };

    enum class Shape2DKind : std::uint8_t
    {
        aabb,
        circle
    };

    struct Shape2D final
    {
        Shape2DKind kind{ Shape2DKind::aabb };
        Vector2 half_extents{ 0.5, 0.5 };
        double radius{};

        [[nodiscard]] static constexpr Shape2D box(
            Vector2 halfExtents) noexcept
        {
            return {
                .kind = Shape2DKind::aabb,
                .half_extents = halfExtents,
                .radius = 0.0
            };
        }

        [[nodiscard]] static constexpr Shape2D circle(
            double circleRadius) noexcept
        {
            return {
                .kind = Shape2DKind::circle,
                .half_extents = {},
                .radius = circleRadius
            };
        }

        [[nodiscard]] friend constexpr bool operator==(
            const Shape2D&,
            const Shape2D&) noexcept = default;
    };

    struct CollisionFilter2D final
    {
        std::uint32_t layer{ 1 };
        std::uint32_t mask{ 0xFFFFFFFFu };

        [[nodiscard]] friend constexpr bool operator==(
            const CollisionFilter2D&,
            const CollisionFilter2D&) noexcept = default;
    };

    enum class StaticPrimitive2DKind : std::uint8_t
    {
        solid_box,
        one_way_up,
        slope_up_right,
        slope_down_right
    };

    struct StaticAabbPrimitive2D final
    {
        std::uint64_t stable_id{};
        Aabb2 bounds{};
        CollisionFilter2D filter{};
        StaticPrimitive2DKind kind{ StaticPrimitive2DKind::solid_box };

        [[nodiscard]] friend constexpr bool operator==(
            const StaticAabbPrimitive2D&,
            const StaticAabbPrimitive2D&) noexcept = default;
    };

    struct Body2DDescriptor final
    {
        BodyDescriptor body{};
        BodyInitialState initial_state{};
        Shape2D shape{};
        CollisionFilter2D filter{};

        [[nodiscard]] friend constexpr bool operator==(
            const Body2DDescriptor&,
            const Body2DDescriptor&) noexcept = default;
    };

    enum class ContactTarget2DKind : std::uint8_t
    {
        body,
        static_map
    };

    struct ContactTarget2D final
    {
        ContactTarget2DKind kind{ ContactTarget2DKind::body };
        BodyHandle body{};
        std::uint64_t static_primitive_id{};

        [[nodiscard]] friend constexpr bool operator==(
            const ContactTarget2D&,
            const ContactTarget2D&) noexcept = default;
    };

    struct ContactId2D final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value != 0;
        }

        [[nodiscard]] friend constexpr auto operator<=> (
            const ContactId2D&,
            const ContactId2D&) noexcept = default;
    };

    struct ContactRecord2D final
    {
        ContactId2D id{};
        ContactTarget2D first{};
        ContactTarget2D second{};
        Vector2 normal{};
        Vector2 point{};
        double penetration{};
        std::uint64_t began_step{};
        std::uint64_t last_step{};

        [[nodiscard]] friend constexpr bool operator==(
            const ContactRecord2D&,
            const ContactRecord2D&) noexcept = default;
    };

    struct Solver2DConfiguration final
    {
        FixedStepConfiguration fixed_step{};
        Vector2 initial_gravity{ 0.0, 9.81 };
        Aabb2 world_bounds{
            .minimum = { -1'000'000.0, -1'000'000.0 },
            .maximum = { 1'000'000.0, 1'000'000.0 }
        };
        double seconds_per_tick{ 1.0 / 60.0 };
        double maximum_linear_speed{ 100'000.0 };
        double maximum_acceleration{ 100'000.0 };
        double contact_slop{ 1.0e-7 };
        std::uint32_t solver_iterations{ 4 };
        std::uint32_t maximum_static_primitives{ 16'384 };
        std::uint32_t maximum_contacts{ 32'768 };

        [[nodiscard]] friend constexpr bool operator==(
            const Solver2DConfiguration&,
            const Solver2DConfiguration&) noexcept = default;
    };

    enum class Solver2DCode : std::uint8_t
    {
        success,
        paused,
        invalid_configuration,
        invalid_descriptor,
        invalid_shape,
        invalid_filter,
        invalid_static_map,
        duplicate_static_id,
        capacity_exceeded,
        invalid_handle,
        invalid_temporal_input,
        manager_rejected,
        contact_capacity_exceeded,
        numerical_failure,
        invalid_snapshot
    };

    struct SpawnBody2DResult final
    {
        Solver2DCode code{ Solver2DCode::success };
        ResultCode manager_code{ ResultCode::success };
        BodyHandle body{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == Solver2DCode::success;
        }
    };

    struct Solver2DMetrics final
    {
        std::uint32_t active_bodies{};
        std::uint32_t static_primitives{};
        std::uint32_t active_contacts{};
        std::uint32_t peak_contacts{};
        std::uint64_t fixed_steps_committed{};
        std::uint64_t broadphase_pairs_tested{};
        std::uint64_t contact_manifolds_generated{};
        std::uint64_t position_corrections{};
        std::uint64_t velocity_corrections{};
        std::uint64_t world_bounds_clamps{};

        [[nodiscard]] friend constexpr bool operator==(
            const Solver2DMetrics&,
            const Solver2DMetrics&) noexcept = default;
    };

    struct ColliderSlot2DSnapshot final
    {
        std::uint32_t generation{ 1 };
        bool occupied{};
        Shape2D shape{};
        CollisionFilter2D filter{};

        [[nodiscard]] friend constexpr bool operator==(
            const ColliderSlot2DSnapshot&,
            const ColliderSlot2DSnapshot&) noexcept = default;
    };

    struct Solver2DStateSnapshot final
    {
        PhysicsManagerSnapshot manager{};
        std::vector<ColliderSlot2DSnapshot> collider_slots{};
        std::vector<StaticAabbPrimitive2D> static_primitives{};
        std::vector<ContactRecord2D> contacts{};
        Vector2 gravity{};
        bool paused{};
        std::uint64_t next_contact_id{ 1 };
        Solver2DMetrics metrics{};

        [[nodiscard]] friend bool operator==(
            const Solver2DStateSnapshot&,
            const Solver2DStateSnapshot&) = default;
    };

    struct Solver2DSnapshot final
    {
        Solver2DConfiguration configuration{};
        Solver2DStateSnapshot state{};
        Solver2DStateSnapshot reset_point{};

        [[nodiscard]] friend bool operator==(
            const Solver2DSnapshot&,
            const Solver2DSnapshot&) = default;
    };

    struct Solver2DAdvanceResult final
    {
        Solver2DCode code{ Solver2DCode::success };
        ResultCode manager_code{ ResultCode::success };
        TemporalAddress current_time{};
        std::uint32_t steps_committed{};
        std::vector<CommandOutcome> command_outcomes{};
        std::vector<ContactRecord2D> contacts{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Solver2DCode::success
                || code == Solver2DCode::paused;
        }
    };

    struct Body2DSnapshot final
    {
        BodySnapshot body{};
        Shape2D shape{};
        CollisionFilter2D filter{};

        [[nodiscard]] friend constexpr bool operator==(
            const Body2DSnapshot&,
            const Body2DSnapshot&) noexcept = default;
    };

    class Solver2D final
    {
    public:
        /*
         * The manager owns body identity, temporal commands, and canonical
         * body state. This sidecar owns only deterministic 2D shape, map,
         * contact, and integration state indexed by manager handle generation.
         * Every solved step commits after one manager fixed-step boundary.
         */
        explicit Solver2D(Solver2DConfiguration configuration = {});
        ~Solver2D();

        Solver2D(Solver2D&&) noexcept;
        Solver2D& operator=(Solver2D&&) noexcept;

        Solver2D(const Solver2D&) = delete;
        Solver2D& operator=(const Solver2D&) = delete;

        [[nodiscard]] static Solver2DCode validate_configuration(
            const Solver2DConfiguration& configuration) noexcept;

        [[nodiscard]] Solver2DConfiguration configuration() const;
        [[nodiscard]] TemporalAddress current_time() const;
        [[nodiscard]] bool paused() const;
        [[nodiscard]] Vector2 gravity() const;
        [[nodiscard]] Solver2DMetrics metrics() const;

        [[nodiscard]] Solver2DCode set_gravity(Vector2 gravity);
        void set_paused(bool paused) noexcept;

        [[nodiscard]] SpawnBody2DResult spawn(
            const Body2DDescriptor& descriptor);
        [[nodiscard]] bool contains(BodyHandle body) const;
        [[nodiscard]] std::optional<Body2DSnapshot> body(
            BodyHandle body) const;
        [[nodiscard]] std::vector<Body2DSnapshot> bodies() const;

        [[nodiscard]] EnqueueResult enqueue_destroy(
            BodyHandle body,
            const TemporalAddress& execute_at);
        [[nodiscard]] EnqueueResult enqueue_set_transform(
            BodyHandle body,
            const Transform& transform,
            const TemporalAddress& execute_at);
        [[nodiscard]] EnqueueResult enqueue_set_velocity(
            BodyHandle body,
            Vector2 velocity,
            const TemporalAddress& execute_at);
        [[nodiscard]] EnqueueResult enqueue_set_enabled(
            BodyHandle body,
            bool enabled,
            const TemporalAddress& execute_at);
        [[nodiscard]] EnqueueResult enqueue_impulse(
            BodyHandle body,
            Vector2 impulse,
            const TemporalAddress& execute_at);

        [[nodiscard]] Solver2DCode set_static_map(
            std::span<const StaticAabbPrimitive2D> primitives);
        void clear_static_map() noexcept;
        [[nodiscard]] std::vector<StaticAabbPrimitive2D> static_map() const;

        [[nodiscard]] Solver2DAdvanceResult advance(
            const FixedStepRequest& request);
        [[nodiscard]] std::vector<ContactRecord2D> contacts() const;

        void capture_reset_point();
        [[nodiscard]] Solver2DCode reset();

        [[nodiscard]] Solver2DSnapshot snapshot() const;
        [[nodiscard]] Solver2DCode restore(
            const Solver2DSnapshot& snapshot);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}

export namespace epochengine::physics::contract
{
    [[nodiscard]] int run_solver2d_contract();
}
