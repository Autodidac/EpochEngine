/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <gui/system_workspace.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module editor.systems_workspace;

import authoring.task_graph;
import systems.registry;
import taskgraph.dotsystem;

export namespace epochengine::editor_systems
{
    enum class Tab : std::uint8_t
    {
        overview,
        systems,
        live_scheduler,
        time,
        learning_graph,
        renderer
    };

    [[nodiscard]] constexpr std::string_view tab_name(Tab tab) noexcept
    {
        switch (tab)
        {
        case Tab::overview: return "Overview";
        case Tab::systems: return "Systems";
        case Tab::live_scheduler: return "Live Scheduler";
        case Tab::time: return "Time";
        case Tab::learning_graph: return "Learning Graph";
        case Tab::renderer: return "Renderer";
        }
        return "Unknown";
    }

    struct ControllerLimits final
    {
        std::uint32_t maximum_registry_systems{1'024u};
        std::uint32_t maximum_live_scheduler_nodes{512u};
        std::uint32_t maximum_time_samples{240u};
        std::uint32_t maximum_learning_nodes{256u};
        std::uint32_t maximum_learning_edges{1'024u};
        std::uint32_t maximum_learning_history{2'048u};
        std::uint32_t maximum_name_bytes{256u};
        std::uint32_t maximum_category_bytes{128u};
        std::uint32_t maximum_filter_bytes{256u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_registry_systems != 0u
                && maximum_live_scheduler_nodes != 0u
                && maximum_live_scheduler_nodes
                    <= taskgraph::TaskGraph::MaxSnapshotNodes
                && maximum_time_samples >= 16u
                && maximum_time_samples <= 4'096u
                && maximum_learning_nodes >= 6u
                && maximum_learning_edges >= 6u
                && maximum_learning_history >= 12u
                && maximum_name_bytes != 0u
                && maximum_category_bytes != 0u
                && maximum_filter_bytes != 0u
                && static_cast<std::uint64_t>(maximum_registry_systems)
                    + maximum_live_scheduler_nodes
                    + maximum_learning_nodes <= 4'096u;
        }
    };

    enum class ControllerCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_controller,
        registry_limit_exceeded,
        scheduler_limit_exceeded,
        workspace_rejected,
        graph_rejected,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view controller_code_name(
        ControllerCode code) noexcept
    {
        switch (code)
        {
        case ControllerCode::ready: return "ready";
        case ControllerCode::unchanged: return "unchanged";
        case ControllerCode::invalid_controller: return "invalid_controller";
        case ControllerCode::registry_limit_exceeded:
            return "registry_limit_exceeded";
        case ControllerCode::scheduler_limit_exceeded:
            return "scheduler_limit_exceeded";
        case ControllerCode::workspace_rejected: return "workspace_rejected";
        case ControllerCode::graph_rejected: return "graph_rejected";
        case ControllerCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct ControllerResult final
    {
        ControllerCode code{ControllerCode::invalid_controller};
        authoring::task_graph::ResultCode graph_code{
            authoring::task_graph::ResultCode::success};
        gui_lib::system_workspace::ErrorCode workspace_code{
            gui_lib::system_workspace::ErrorCode::none};
        bool changed{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ControllerCode::ready
                || code == ControllerCode::unchanged;
        }
    };

    struct RefreshResult final
    {
        ControllerResult result{};
        std::size_t registry_systems{};
        std::size_t committed_rows{};
        std::size_t visible_rows{};
        std::uint64_t registry_revision{};
        std::uint64_t view_revision{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return static_cast<bool>(result);
        }
    };

    enum class LiveDataState : std::uint8_t
    {
        unknown,
        unavailable,
        warming_up,
        available,
        paused
    };

    [[nodiscard]] constexpr std::string_view live_data_state_name(
        LiveDataState state) noexcept
    {
        switch (state)
        {
        case LiveDataState::unknown: return "Unknown";
        case LiveDataState::unavailable: return "Unavailable";
        case LiveDataState::warming_up: return "Warming up";
        case LiveDataState::available: return "Available";
        case LiveDataState::paused: return "Paused";
        }
        return "Unknown";
    }

    struct LiveSchedulerSnapshot final
    {
        LiveDataState state{LiveDataState::unknown};
        std::string owner_label{};
        std::string status{"No live scheduler snapshot has been requested."};
        taskgraph::GraphSnapshot graph{};
        std::uint64_t view_revision{};
    };

    struct SchedulerRefreshResult final
    {
        ControllerResult result{};
        LiveDataState state{LiveDataState::unknown};
        std::size_t visible_nodes{};
        std::size_t total_nodes{};
        std::uint64_t scheduler_revision{};
        std::uint64_t view_revision{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return static_cast<bool>(result);
        }
    };

    struct TimeSample final
    {
        std::uint64_t sequence{};
        std::uint64_t registry_revision{};
        std::uint64_t frame_count{};
        std::uint64_t frame_nanoseconds{};
        std::uint64_t peak_frame_nanoseconds{};
        std::uint64_t systems_nanoseconds{};
        std::uint64_t busiest_system_nanoseconds{};
        std::string busiest_system{};
    };

    struct TimeDiagnosticsSnapshot final
    {
        LiveDataState state{LiveDataState::unknown};
        std::string status{"Timing diagnostics have not sampled a visible frame."};
        std::deque<TimeSample> samples{};
        std::size_t capacity{};
        std::uint64_t dropped_samples{};
        std::uint64_t revision{};
    };

    struct LearningOperationResult final
    {
        ControllerResult result{};
        authoring::task_graph::NodeHandle node{};
        authoring::task_graph::OperationId operation{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return static_cast<bool>(result);
        }
    };

    class SystemsWorkspace final
    {
    public:
        explicit SystemsWorkspace(ControllerLimits limits = {});
        ~SystemsWorkspace();

        SystemsWorkspace(const SystemsWorkspace&) = delete;
        SystemsWorkspace& operator=(const SystemsWorkspace&) = delete;
        SystemsWorkspace(SystemsWorkspace&&) noexcept;
        SystemsWorkspace& operator=(SystemsWorkspace&&) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] const ControllerLimits& limits() const noexcept;
        [[nodiscard]] Tab tab() const noexcept;
        [[nodiscard]] ControllerResult set_tab(Tab tab);

        [[nodiscard]] std::string_view text_filter() const noexcept;
        [[nodiscard]] ControllerResult set_text_filter(std::string_view filter);
        [[nodiscard]] ControllerResult clear_filters();

        [[nodiscard]] std::string_view selected_id() const noexcept;
        [[nodiscard]] ControllerResult select(std::string_view rowId);
        [[nodiscard]] bool clear_selection() noexcept;

        [[nodiscard]] bool diagnostics_sampling_intent() const noexcept;
        [[nodiscard]] ControllerResult set_diagnostics_sampling_intent(
            bool enabled) noexcept;

        [[nodiscard]] RefreshResult refresh_from_registry(
            systems::Registry& registry);
        [[nodiscard]] RefreshResult refresh_from_snapshot(
            systems::RegistryDiagnosticSnapshot snapshot);
        [[nodiscard]] SchedulerRefreshResult refresh_live_scheduler(
            const taskgraph::TaskGraph* scheduler,
            std::string_view owner_label = {});
        [[nodiscard]] SchedulerRefreshResult refresh_live_scheduler_snapshot(
            taskgraph::GraphSnapshot snapshot,
            std::string owner_label);
        [[nodiscard]] SchedulerRefreshResult mark_live_scheduler_unavailable(
            std::string reason);
        [[nodiscard]] ControllerResult reset();
        [[nodiscard]] ControllerResult reset_learning_graph();

        [[nodiscard]] const systems::RegistryDiagnosticSnapshot&
            registry_snapshot() const noexcept;
        [[nodiscard]] const LiveSchedulerSnapshot&
            live_scheduler_snapshot() const noexcept;
        [[nodiscard]] ControllerResult select_live_scheduler_node(
            std::uint64_t node_id) noexcept;
        [[nodiscard]] const taskgraph::NodeDiagnostic*
            selected_live_scheduler_node() const noexcept;
        [[nodiscard]] const TimeDiagnosticsSnapshot&
            time_diagnostics() const noexcept;
        [[nodiscard]] ControllerResult select_time_sample(
            std::uint64_t sequence) noexcept;
        [[nodiscard]] const TimeSample* selected_time_sample() const noexcept;
        [[nodiscard]] const gui_lib::system_workspace::Controller&
            workspace() const noexcept;

        [[nodiscard]] LearningOperationResult add_learning_task(
            std::string name,
            std::string category,
            std::uint64_t estimatedCostUnits);
        [[nodiscard]] LearningOperationResult connect_learning_tasks(
            authoring::task_graph::NodeHandle prerequisite,
            authoring::task_graph::NodeHandle dependent);
        [[nodiscard]] LearningOperationResult disconnect_learning_tasks(
            authoring::task_graph::NodeHandle prerequisite,
            authoring::task_graph::NodeHandle dependent);
        [[nodiscard]] LearningOperationResult rename_learning_task(
            authoring::task_graph::NodeHandle node,
            std::string name,
            std::string category,
            std::uint64_t cost_units);
        [[nodiscard]] LearningOperationResult remove_learning_task(
            authoring::task_graph::NodeHandle node);
        [[nodiscard]] LearningOperationResult undo_learning_graph();
        [[nodiscard]] LearningOperationResult redo_learning_graph();
        [[nodiscard]] bool can_undo_learning_graph() const noexcept;
        [[nodiscard]] bool can_redo_learning_graph() const noexcept;
        [[nodiscard]] std::vector<authoring::task_graph::NodeView>
            learning_tasks() const;
        [[nodiscard]] authoring::task_graph::GraphDiagnostics
            learning_diagnostics() const;
        [[nodiscard]] authoring::task_graph::ParallelWaveSimulation
            simulate_learning_graph() const;

        [[nodiscard]] std::string_view status() const noexcept;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_{};
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        invalid_controller,
        sample_graph,
        registry_refresh,
        system_rows,
        filter_and_selection,
        live_scheduler_unavailable,
        live_scheduler_snapshot,
        live_scheduler_selection,
        time_sampling,
        time_bounds,
        time_selection,
        revision_cache,
        tab_projection,
        graph_add,
        graph_connect,
        cycle_rejection,
        graph_remove,
        undo_redo,
        bounded_registry,
        graph_reset,
        reset
    };

    [[nodiscard]] ContractFailure run_contract();
}
