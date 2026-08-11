/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

export module project.actor2d_runtime;

export import physics.solver2d;

export namespace epochengine::project_actor2d
{
    enum class ResultCode : std::uint8_t
    {
        ready,
        paused,
        invalid_configuration,
        invalid_input,
        invalid_collision,
        capacity_exceeded,
        solver_failure,
        invalid_snapshot
    };

    [[nodiscard]] std::string_view result_code_name(ResultCode code) noexcept;

    enum class MovementMode : std::uint8_t
    {
        platformer,
        top_down
    };

    struct Rect final
    {
        double x{};
        double y{};
        double width{1.0};
        double height{1.0};

        friend constexpr bool operator==(const Rect&, const Rect&) noexcept = default;
    };

    enum class StaticCollisionKind : std::uint8_t
    {
        unsupported,
        solid_box,
        one_way_up,
        slope_up_right,
        slope_down_right
    };

    struct StaticCollision final
    {
        std::uint64_t stable_id{};
        Rect bounds{};
        std::uint32_t layer_bits{1u};
        std::uint32_t mask_bits{0xffff'ffffu};
        bool sensor{};
        StaticCollisionKind kind{StaticCollisionKind::solid_box};

        friend constexpr bool operator==(
            const StaticCollision&,
            const StaticCollision&) noexcept = default;
    };

    struct ActorConfiguration final
    {
        MovementMode movement{MovementMode::platformer};
        std::uint64_t stable_actor_id{1u};
        double spawn_x{1.5};
        double spawn_y{2.0};
        double width{0.75};
        double height{0.9};
        double move_speed{5.5};
        double ground_acceleration{48.0};
        double air_acceleration{24.0};
        double ground_deceleration{56.0};
        double jump_speed{9.5};
        double gravity_y{24.0};
        double maximum_fall_speed{20.0};
        std::uint32_t fixed_steps_per_second{60u};
        std::uint32_t maximum_collision_surfaces{16'384u};
        std::uint32_t actor_layer_bits{2u};
        std::uint32_t actor_mask_bits{0xffff'ffffu};

        friend constexpr bool operator==(
            const ActorConfiguration&,
            const ActorConfiguration&) noexcept = default;
    };

    struct FixedInputFrame final
    {
        std::uint64_t sequence{};
        double move_x{};
        double move_y{};
        bool jump_pressed{};
        bool reset_pressed{};
        bool pause_pressed{};

        friend constexpr bool operator==(
            const FixedInputFrame&,
            const FixedInputFrame&) noexcept = default;
    };

    struct ActorState final
    {
        double x{};
        double y{};
        double velocity_x{};
        double velocity_y{};
        bool grounded{};
        bool paused{};
        std::uint64_t fixed_tick{};
        std::uint64_t revision{1u};

        friend constexpr bool operator==(
            const ActorState&,
            const ActorState&) noexcept = default;
    };

    struct RuntimeMetrics final
    {
        std::uint64_t accepted_input_frames{};
        std::uint64_t rejected_input_frames{};
        std::uint64_t fixed_steps{};
        std::uint64_t contact_events{};
        std::uint64_t resets{};
        std::uint64_t pause_transitions{};
        std::uint32_t collision_surfaces{};
        std::uint32_t peak_contacts_per_step{};

        friend constexpr bool operator==(
            const RuntimeMetrics&,
            const RuntimeMetrics&) noexcept = default;
    };

    struct AdvanceResult final
    {
        ResultCode code{ResultCode::invalid_input};
        ActorState state{};
        std::uint32_t steps_committed{};
        std::uint32_t contacts{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::ready || code == ResultCode::paused;
        }
    };

    struct RuntimeSnapshot final
    {
        ActorConfiguration configuration{};
        std::vector<StaticCollision> collision{};
        ActorState actor{};
        RuntimeMetrics metrics{};
        std::uint64_t last_input_sequence{};
        bool pending_jump{};
        physics::Solver2DSnapshot solver{};

        friend bool operator==(
            const RuntimeSnapshot&,
            const RuntimeSnapshot&) = default;
    };

    class ActorRuntime final
    {
    public:
        explicit ActorRuntime(ActorConfiguration configuration = {});
        ~ActorRuntime();

        ActorRuntime(ActorRuntime&&) noexcept;
        ActorRuntime& operator=(ActorRuntime&&) noexcept;

        ActorRuntime(const ActorRuntime&) = delete;
        ActorRuntime& operator=(const ActorRuntime&) = delete;

        [[nodiscard]] static ResultCode validate_configuration(
            const ActorConfiguration& configuration) noexcept;
        [[nodiscard]] static ResultCode validate_collision(
            std::span<const StaticCollision> collision,
            std::uint32_t maximum_surfaces);

        [[nodiscard]] ResultCode replace_collision(
            std::span<const StaticCollision> collision);
        [[nodiscard]] ResultCode reset();
        [[nodiscard]] ResultCode set_paused(bool paused);
        [[nodiscard]] AdvanceResult advance(
            const FixedInputFrame& input,
            std::uint32_t fixed_steps = 1u);

        [[nodiscard]] ActorConfiguration configuration() const noexcept;
        [[nodiscard]] ActorState state() const noexcept;
        [[nodiscard]] RuntimeMetrics metrics() const noexcept;
        [[nodiscard]] RuntimeSnapshot snapshot() const;
        [[nodiscard]] ResultCode restore(const RuntimeSnapshot& snapshot);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        invalid_configuration_accepted,
        invalid_collision_accepted,
        deterministic_replay_mismatch,
        platformer_collision_failed,
        top_down_collision_failed,
        pause_failed,
        reset_failed,
        snapshot_restore_failed,
        contradictory_snapshot_accepted,
        hot_collision_reset_failed,
        stale_input_accepted,
        resource_accounting_failed
    };

    [[nodiscard]] ContractFailure run_contract() noexcept;
    [[nodiscard]] std::string_view contract_failure_name(
        ContractFailure failure) noexcept;
}
