/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <string_view>

export module project.lifecycle;

export namespace epochengine::project_lifecycle
{
    enum class Request : std::uint8_t
    {
        save,
        materialize,
        build,
        run
    };

    enum class Action : std::uint8_t
    {
        blocked,
        save_scene,
        materialize_shell,
        build_project,
        wait_for_build,
        launch_runtime,
        focus_runtime
    };

    enum class BlockReason : std::uint8_t
    {
        none,
        no_project,
        scene_commit_failed,
        invalid_generation
    };

    struct Evidence final
    {
        bool project_selected{};
        bool scene_committed{};
        bool shell_materialized{};
        bool build_inputs_current{};
        bool output_exists{};
        bool build_active{};
        bool runtime_active{};
    };

    struct Decision final
    {
        Action action{Action::blocked};
        BlockReason reason{BlockReason::none};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return action != Action::blocked;
        }
    };

    [[nodiscard]] constexpr Decision decide(
        Request request,
        const Evidence& evidence) noexcept
    {
        if (!evidence.project_selected)
            return {Action::blocked, BlockReason::no_project};

        if (request == Request::save)
            return {Action::save_scene, BlockReason::none};

        if (!evidence.scene_committed)
            return {Action::save_scene, BlockReason::none};

        if (request == Request::materialize)
        {
            return evidence.shell_materialized
                ? Decision{Action::save_scene, BlockReason::none}
                : Decision{Action::materialize_shell, BlockReason::none};
        }

        if (!evidence.shell_materialized)
            return {Action::materialize_shell, BlockReason::none};

        if (evidence.build_active)
            return {Action::wait_for_build, BlockReason::none};

        if (request == Request::build)
            return {Action::build_project, BlockReason::none};

        if (!evidence.build_inputs_current || !evidence.output_exists)
            return {Action::build_project, BlockReason::none};

        if (evidence.runtime_active)
            return {Action::focus_runtime, BlockReason::none};

        return {Action::launch_runtime, BlockReason::none};
    }

    class AttemptTracker final
    {
    public:
        [[nodiscard]] constexpr std::uint64_t begin() noexcept
        {
            active_generation_ = ++latest_generation_;
            return active_generation_;
        }

        [[nodiscard]] constexpr bool complete(
            std::uint64_t generation) noexcept
        {
            if (generation == 0 || generation != active_generation_)
                return false;
            completed_generation_ = generation;
            active_generation_ = 0;
            return true;
        }

        [[nodiscard]] constexpr bool cancel(
            std::uint64_t generation) noexcept
        {
            if (generation == 0 || generation != active_generation_)
                return false;
            active_generation_ = 0;
            return true;
        }

        [[nodiscard]] constexpr bool active() const noexcept
        {
            return active_generation_ != 0;
        }

        [[nodiscard]] constexpr std::uint64_t active_generation() const noexcept
        {
            return active_generation_;
        }

        [[nodiscard]] constexpr std::uint64_t completed_generation() const noexcept
        {
            return completed_generation_;
        }

    private:
        std::uint64_t latest_generation_{};
        std::uint64_t active_generation_{};
        std::uint64_t completed_generation_{};
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        missing_project,
        save_first,
        materialize_first,
        build_first,
        launch,
        active_build,
        focus_runtime,
        generation_tracking
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::missing_project: return "missing_project";
        case ContractFailure::save_first: return "save_first";
        case ContractFailure::materialize_first: return "materialize_first";
        case ContractFailure::build_first: return "build_first";
        case ContractFailure::launch: return "launch";
        case ContractFailure::active_build: return "active_build";
        case ContractFailure::focus_runtime: return "focus_runtime";
        case ContractFailure::generation_tracking:
            return "generation_tracking";
        }
        return "unknown";
    }

    [[nodiscard]] constexpr ContractFailure runtime_contract_failure() noexcept
    {
        Evidence evidence{};
        if (decide(Request::run, evidence).reason != BlockReason::no_project)
            return ContractFailure::missing_project;

        evidence.project_selected = true;
        if (decide(Request::run, evidence).action != Action::save_scene)
            return ContractFailure::save_first;

        evidence.scene_committed = true;
        if (decide(Request::run, evidence).action
            != Action::materialize_shell)
        {
            return ContractFailure::materialize_first;
        }

        evidence.shell_materialized = true;
        if (decide(Request::run, evidence).action != Action::build_project)
            return ContractFailure::build_first;

        evidence.build_inputs_current = true;
        evidence.output_exists = true;
        if (decide(Request::run, evidence).action != Action::launch_runtime)
            return ContractFailure::launch;

        evidence.build_active = true;
        if (decide(Request::run, evidence).action != Action::wait_for_build)
            return ContractFailure::active_build;

        evidence.build_active = false;
        evidence.runtime_active = true;
        if (decide(Request::run, evidence).action != Action::focus_runtime)
            return ContractFailure::focus_runtime;

        AttemptTracker tracker{};
        const std::uint64_t first = tracker.begin();
        const std::uint64_t second = tracker.begin();
        if (first == second
            || tracker.complete(first)
            || !tracker.active()
            || !tracker.complete(second)
            || tracker.active()
            || tracker.completed_generation() != second)
        {
            return ContractFailure::generation_tracking;
        }

        return ContractFailure::none;
    }
}
