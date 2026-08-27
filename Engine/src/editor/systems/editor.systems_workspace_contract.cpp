/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <gui/system_workspace.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module editor.systems_workspace;

namespace epochengine::editor_systems
{
    namespace
    {
        [[nodiscard]] authoring::task_graph::NodeHandle find_task(
            const SystemsWorkspace& workspace,
            const std::string& name)
        {
            const std::vector<authoring::task_graph::NodeView> nodes =
                workspace.learning_tasks();
            const auto found = std::find_if(
                nodes.begin(), nodes.end(), [&name](const auto& node)
                {
                    return node.descriptor.name == name;
                });
            return found == nodes.end()
                ? authoring::task_graph::NodeHandle{}
                : found->handle;
        }

        [[nodiscard]] taskgraph::GraphSnapshot scheduler_fixture()
        {
            taskgraph::GraphSnapshot snapshot{};
            snapshot.nodes.push_back(taskgraph::NodeDiagnostic{
                11u, taskgraph::NodeState::completed, "Load Project", 0u, 2u});
            snapshot.nodes.push_back(taskgraph::NodeDiagnostic{
                12u, taskgraph::NodeState::running, "Compile Scripts", 0u, 1u});
            snapshot.nodes.push_back(taskgraph::NodeDiagnostic{
                13u, taskgraph::NodeState::pending, "Build Runtime", 1u, 0u});
            snapshot.nodeCount = snapshot.nodes.size();
            snapshot.workerCount = 4u;
            snapshot.pendingCount = 1u;
            snapshot.runningCount = 1u;
            snapshot.outstandingCount = 2u;
            snapshot.acceptedCount = 3u;
            snapshot.enqueuedCount = 2u;
            snapshot.completedCount = 1u;
            snapshot.revision = 41u;
            snapshot.accepting = true;
            return snapshot;
        }

        [[nodiscard]] systems::RegistryDiagnosticSnapshot registry_fixture()
        {
            systems::RegistryDiagnosticSnapshot snapshot{};
            snapshot.revision = 17u;
            snapshot.frame_count = 120u;
            snapshot.last_frame_nanoseconds = 8'000'000u;
            snapshot.peak_frame_nanoseconds = 12'000'000u;
            snapshot.order_resolved = true;
            snapshot.initialized = true;
            snapshot.diagnostics_sampling = true;
            snapshot.systems.push_back(systems::SystemDiagnostic{
                "Physics",
                {},
                systems::SystemLifecycle::initialized,
                0u,
                120u,
                1'000'000u,
                2'000'000u,
                120'000'000u});
            snapshot.systems.push_back(systems::SystemDiagnostic{
                "Renderer",
                {"Physics"},
                systems::SystemLifecycle::initialized,
                1u,
                120u,
                3'000'000u,
                5'000'000u,
                360'000'000u});
            return snapshot;
        }
    }

    ContractFailure run_contract()
    {
        SystemsWorkspace workspace{};
        if (!workspace.valid())
            return ContractFailure::invalid_controller;

        const authoring::task_graph::GraphDiagnostics initial =
            workspace.learning_diagnostics();
        const authoring::task_graph::ParallelWaveSimulation initialSimulation =
            workspace.simulate_learning_graph();
        if (!initial.healthy()
            || initial.metrics().active_nodes != 6u
            || initial.metrics().edges != 6u
            || !initialSimulation
            || initialSimulation.waves.size() != 5u
            || workspace.status().find("separate") == std::string_view::npos)
        {
            return ContractFailure::sample_graph;
        }

        const RefreshResult refreshed =
            workspace.refresh_from_snapshot(registry_fixture());
        if (!refreshed
            || refreshed.registry_systems != 2u
            || workspace.registry_snapshot().revision != 17u
            || workspace.time_diagnostics().state != LiveDataState::paused
            || !workspace.time_diagnostics().samples.empty())
        {
            return ContractFailure::registry_refresh;
        }
        if (!workspace.set_tab(Tab::systems)
            || workspace.workspace().summary().system_rows != 2u
            || workspace.workspace().summary().task_rows != 0u)
        {
            return ContractFailure::system_rows;
        }

        if (!workspace.select("system:Renderer")
            || workspace.selected_id() != "system:Renderer"
            || !workspace.set_text_filter("render")
            || workspace.workspace().summary().visible_rows != 1u
            || !workspace.clear_filters())
        {
            return ContractFailure::filter_and_selection;
        }

        const SchedulerRefreshResult unavailable =
            workspace.refresh_live_scheduler(nullptr);
        if (!unavailable
            || unavailable.state != LiveDataState::unavailable
            || workspace.live_scheduler_snapshot().status.find("No live scheduler")
                == std::string::npos)
        {
            return ContractFailure::live_scheduler_unavailable;
        }

        const SchedulerRefreshResult scheduler =
            workspace.refresh_live_scheduler_snapshot(
                scheduler_fixture(), "Contract Scheduler");
        if (!scheduler
            || scheduler.state != LiveDataState::available
            || scheduler.visible_nodes != 3u
            || scheduler.total_nodes != 3u
            || workspace.live_scheduler_snapshot().graph.nodes.size() != 3u)
        {
            return ContractFailure::live_scheduler_snapshot;
        }
        if (!workspace.select_live_scheduler_node(12u)
            || !workspace.selected_live_scheduler_node()
            || workspace.selected_live_scheduler_node()->label != "Compile Scripts")
        {
            return ContractFailure::live_scheduler_selection;
        }
        const SchedulerRefreshResult cached =
            workspace.refresh_live_scheduler_snapshot(
                scheduler_fixture(), "Contract Scheduler");
        if (!cached
            || cached.result.code != ControllerCode::unchanged
            || cached.view_revision != scheduler.view_revision)
        {
            return ContractFailure::revision_cache;
        }

        if (!workspace.set_diagnostics_sampling_intent(true))
            return ContractFailure::time_sampling;
        const RefreshResult firstTime =
            workspace.refresh_from_snapshot(registry_fixture());
        const RefreshResult repeatedTime =
            workspace.refresh_from_snapshot(registry_fixture());
        if (!firstTime
            || !repeatedTime
            || repeatedTime.result.code != ControllerCode::unchanged
            || workspace.time_diagnostics().state != LiveDataState::available
            || workspace.time_diagnostics().samples.size() != 1u)
        {
            return ContractFailure::time_sampling;
        }
        const auto& firstSample = workspace.time_diagnostics().samples.front();
        if (firstSample.frame_nanoseconds != 8'000'000u
            || firstSample.systems_nanoseconds != 4'000'000u
            || firstSample.busiest_system != "Renderer")
        {
            return ContractFailure::time_sampling;
        }
        if (!workspace.select_time_sample(firstSample.sequence)
            || !workspace.selected_time_sample()
            || workspace.selected_time_sample()->frame_count != 120u)
        {
            return ContractFailure::time_selection;
        }

        if (!workspace.set_tab(Tab::live_scheduler)
            || workspace.workspace().summary().system_rows != 0u
            || workspace.workspace().summary().task_rows != 3u
            || !workspace.set_tab(Tab::time)
            || workspace.workspace().content_state()
                != gui_lib::system_workspace::ContentState::empty
            || !workspace.set_tab(Tab::learning_graph)
            || workspace.workspace().summary().system_rows != 0u
            || workspace.workspace().summary().task_rows != 6u
            || !workspace.set_tab(Tab::renderer)
            || workspace.workspace().content_state()
                != gui_lib::system_workspace::ContentState::empty
            || !workspace.set_tab(Tab::overview))
        {
            return ContractFailure::tab_projection;
        }

        const LearningOperationResult package = workspace.add_learning_task(
            "Package Runtime", "Build", 2u);
        if (!package || !package.node
            || workspace.learning_diagnostics().metrics().active_nodes != 7u)
        {
            return ContractFailure::graph_add;
        }

        const auto build = find_task(workspace, "Build Runtime");
        const auto run = find_task(workspace, "Run External");
        const auto request = find_task(workspace, "Authoring Request");
        if (!build || !run || !request
            || !workspace.connect_learning_tasks(build, package.node)
            || !workspace.connect_learning_tasks(package.node, run))
        {
            return ContractFailure::graph_connect;
        }

        const LearningOperationResult cycle =
            workspace.connect_learning_tasks(run, request);
        if (cycle
            || cycle.result.graph_code
                != authoring::task_graph::ResultCode::cycle_detected
            || !workspace.learning_diagnostics().healthy())
        {
            return ContractFailure::cycle_rejection;
        }

        if (!workspace.remove_learning_task(package.node)
            || workspace.learning_diagnostics().metrics().active_nodes != 6u)
        {
            return ContractFailure::graph_remove;
        }
        if (!workspace.can_undo_learning_graph()
            || !workspace.undo_learning_graph()
            || workspace.learning_diagnostics().metrics().active_nodes != 7u
            || !workspace.can_redo_learning_graph()
            || !workspace.redo_learning_graph()
            || workspace.learning_diagnostics().metrics().active_nodes != 6u)
        {
            return ContractFailure::undo_redo;
        }

        if (!workspace.reset_learning_graph()
            || workspace.learning_diagnostics().metrics().active_nodes != 6u
            || workspace.learning_diagnostics().metrics().edges != 6u
            || workspace.registry_snapshot().systems.size() != 2u
            || workspace.live_scheduler_snapshot().state
                != LiveDataState::available
            || workspace.time_diagnostics().samples.size() != 1u)
        {
            return ContractFailure::graph_reset;
        }
        ControllerLimits tiny{};
        tiny.maximum_registry_systems = 1u;
        SystemsWorkspace bounded{tiny};
        if (!bounded.valid())
            return ContractFailure::bounded_registry;
        const RefreshResult rejected =
            bounded.refresh_from_snapshot(registry_fixture());
        if (rejected
            || rejected.result.code != ControllerCode::registry_limit_exceeded
            || !bounded.registry_snapshot().systems.empty())
        {
            return ContractFailure::bounded_registry;
        }

        ControllerLimits schedulerLimit{};
        schedulerLimit.maximum_live_scheduler_nodes = 1u;
        SystemsWorkspace schedulerBounded{schedulerLimit};
        const SchedulerRefreshResult schedulerRejected =
            schedulerBounded.refresh_live_scheduler_snapshot(
                scheduler_fixture(), "Too Large");
        if (schedulerRejected
            || schedulerRejected.result.code
                != ControllerCode::scheduler_limit_exceeded
            || schedulerBounded.live_scheduler_snapshot().state
                != LiveDataState::unknown)
        {
            return ContractFailure::live_scheduler_snapshot;
        }

        ControllerLimits timeLimit{};
        timeLimit.maximum_time_samples = 16u;
        SystemsWorkspace timeBounded{timeLimit};
        if (!timeBounded.valid()
            || !timeBounded.set_diagnostics_sampling_intent(true))
        {
            return ContractFailure::time_bounds;
        }
        for (std::uint64_t frame = 1u; frame <= 20u; ++frame)
        {
            systems::RegistryDiagnosticSnapshot sample = registry_fixture();
            sample.frame_count = frame;
            sample.revision = 100u;
            if (!timeBounded.refresh_from_snapshot(std::move(sample)))
                return ContractFailure::time_bounds;
        }
        if (timeBounded.time_diagnostics().samples.size() != 16u
            || timeBounded.time_diagnostics().dropped_samples != 4u
            || timeBounded.time_diagnostics().samples.front().frame_count != 5u)
        {
            return ContractFailure::time_bounds;
        }

        if (!workspace.reset()
            || workspace.tab() != Tab::overview
            || !workspace.registry_snapshot().systems.empty()
            || workspace.live_scheduler_snapshot().state != LiveDataState::unknown
            || !workspace.time_diagnostics().samples.empty()
            || workspace.learning_diagnostics().metrics().active_nodes != 6u)
        {
            return ContractFailure::reset;
        }
        return ContractFailure::none;
    }
}

#if defined(EPOCH_EDITOR_SYSTEMS_WORKSPACE_CONTRACT_MAIN)
int main()
{
    return static_cast<int>(epochengine::editor_systems::run_contract());
}
#endif
